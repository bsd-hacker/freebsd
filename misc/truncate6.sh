#!/bin/sh

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

# The issue, found by Maxim, is that sometimes partial truncate could
# create a UFS inode where the last byte is not populated.
# Fixed by r295950.

# Test scenario by Maxim Sobolev <sobomax@sippysoft.com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
bsdlabel -w md$mdstart auto

if [ $# -eq 0 ]; then
	newfs $newfs_flags md${mdstart}$part > /dev/null
else
	newfs md${mdstart}$part > /dev/null
fi
mount /dev/md${mdstart}$part $mntpoint

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > truncate6.c
mycc -o truncate6 -Wall -Wextra -O2 truncate6.c -lutil
rm -f truncate6.c

cd $mntpoint

/tmp/truncate6
inode=`ls -ail | grep temp | awk '{print $1}'`

cd $here
rm -f /tmp/truncate6

while mount | grep -q md${mdstart}$part; do
	umount $mntpoint || sleep 1
done

full=$(
fsdb -r /dev/md${mdstart}$part <<QUOTE
inode $inode
blocks
quit
QUOTE
)
full=`echo "$full" | sed '/Last Mounted/,+6d'`
r=`echo "$full" | tail -1`
if [ "$r" != "0, 0, 0, 4704" ]; then
	e=1
	echo "FAIL Expected \"0, 0, 0, 4704\", got \"$r\"."
	echo "$full"
else
	e=0
fi

mdconfig -d -u $mdstart
rm /tmp/truncate6
exit $e
EOF
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	off_t hole, data, pos;
	int fd;
	char tempname[] = "temp.XXXXXX";
	char *fname;

	pos = 1024 * 128 + 1;
	fname = tempname;
	if (mkstemp(tempname) == -1)
		err(1, "mkstemp(%s)", tempname);
	if ((fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) ==
	    -1)
		err(1, "open(%s)", fname);
	if (ftruncate(fd, 1024 * 128 + 1) < 0)
		err(1, "ftruncate()");
	hole = lseek(fd, 0, SEEK_HOLE);
	data = lseek(fd, 0, SEEK_DATA);
#if defined(TEST)
	printf("--> hole = %jd, data = %jd, pos = %jd\n",
	    (intmax_t)hole, (intmax_t)data, (intmax_t)pos);
#endif
	if (ftruncate(fd, data) < 0)
		err(1, "ftruncate() 2");
	close(fd);

	return (0);
}
