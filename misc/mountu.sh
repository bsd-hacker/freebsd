#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

# Change mount point from rw to ro with a file mapped rw
# Currently fails for NFS

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mountu.c
cc -o mountu -Wall -Wextra -O2 mountu.c
rm -f mountu.c

pstat() {
	pid=`ps a | grep -v grep | grep /tmp/mountu | awk '{print $1}'`
	[ -n "$pid" ] && procstat -v $pid
}

mount | grep -q "$mntpoint " && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 100m -u $mdstart
bsdlabel -w md${mdstart} auto
newfs $newfs_flags md${mdstart}${part} > /dev/null
mount /dev/md${mdstart}${part} $mntpoint
chmod 777 $mntpoint

/tmp/mountu $mntpoint/file &

sleep 1
if ! mount -u -o ro $mntpoint 2>&1 | grep -q "Device busy"; then
	echo "UFS FAILED"
	pstat
fi
wait
while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done

mdconfig -d -u $mdstart

mount -t nfs -o tcp -o retrycnt=3 -o intr -o soft -o rw 127.0.0.1:/tmp $mntpoint
rm -f /tmp/file
/tmp/mountu $mntpoint/file &
sleep 1

if ! mount -u -o ro $mntpoint 2>&1 | grep -q "Device busy"; then
	echo "NFS FAILED"
fi
wait
umount $mntpoint

if [ -x /sbin/mount_msdosfs ]; then
	mdconfig -a -t swap -s 100m -u $mdstart
	bsdlabel -w md${mdstart} auto
	newfs_msdos -F 16 -b 8192 /dev/md${mdstart}a > /dev/null 2>&1
	mount_msdosfs -m 777 /dev/md${mdstart}a $mntpoint
	/tmp/mountu $mntpoint/file &

	sleep 1
	if ! mount -u -o ro $mntpoint 2>&1 | grep -q "Device busy"; then
		echo "MSDOS FAILED"
	fi
	wait

	while mount | grep -q "$mntpoint "; do
		umount $mntpoint || sleep 1
	done
fi
rm -f /tmp/mountu /tmp/file
exit 0
EOF
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define STARTADDR 0x0U
#define ADRSPACE  0x0640000U

int
main(int argc __unused, char **argv)
{
	int fd, ps;
	void *p;
	size_t len;
	struct passwd *pw;
	char *c, *path;

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	p = (void *)STARTADDR;
	len = ADRSPACE;

	path = argv[1];
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open(%s)", path);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM) {
			warn("mmap()");
			return (1);
		}
		err(1, "mmap(1)");
	}
//	fprintf(stderr, "%s mapped to %p\n", path, p);

	c = p;
	ps = getpagesize();
	for (c = p; (void *)c < p + len; c += ps) {
		*c = 1;
	}

	close(fd);
	sleep(5);

	return (0);
}
