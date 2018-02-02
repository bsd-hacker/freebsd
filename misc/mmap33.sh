#!/bin/sh

# ftruncate+mmap+fsync fails for small maps
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=225586

# Original test scenario by tris_vern@hotmail.com

# Fixed in r328773:
# On pageout, in vnode generic pager, for partially dirty page, only
# clear dirty bits for completely invalid blocks.

. ../default.cfg

cat > /tmp/mmap33.c <<EOF
#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main (int argc, char *argv[])
{
	size_t i, size1, size2;
	int fd;
	char *data;
	char *filename;
	char pattern = 0x01;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s filename size1 size2\n", argv[0]);
		exit(1);
	}

	filename = argv[1];
	size1 = atoi(argv[2]);
	size2 = atoi(argv[3]);

	fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0644);
	for (i = 0; i < size1; i++)
		write(fd, &pattern, 1);
	close(fd);

	fd = open(filename, O_RDWR, 0644);
	if (fd == -1)
		err(1, "open(%s)", filename);
	if (ftruncate(fd, size2) == -1)
		err(1, "ftruncate()");
	data = mmap(NULL, size2, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		err(1, "mmap()");
	memset(data, 0xFF, size2);

	if (munmap(data, size2) == -1)
		err(1, "munmap");
	close(fd);

	return (0);
}
EOF
cc -o /tmp/mmap33 -Wall -Wextra -O2 -g /tmp/mmap33.c || exit 1
rm /tmp/mmap33.c

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
bsdlabel -w md$mdstart auto
newfs $newfs_flags -n md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
set +e

file=file
odir=`pwd`
cd $mntpoint
/tmp/mmap33 $file 1024 511
s=$?
sum1=`md5 < $mntpoint/$file`
[ -f template.core -a $s -eq 0 ] &&
    { ls -l template.core; mv template.core /tmp; s=1; }
cd $odir
umount $mntpoint
mount /dev/md${mdstart}$part $mntpoint
# This fails for truncate size < 512
sum2=`md5 < $mntpoint/$file`
[ $sum1 = $sum2 ] ||
    { s=2; echo "md5 fingerprint differs."; }
umount $mntpoint

mdconfig -d -u $mdstart
rm /tmp/mmap33
exit $s
