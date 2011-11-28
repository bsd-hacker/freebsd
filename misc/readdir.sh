#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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


# readdir(3) fuzzing inspired by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>

# "panic: kmem_malloc(1328054272): kmem_map too small" seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > readdir.c
cc -o readdir -Wall -Wextra readdir.c
rm -f readdir.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart


mount -t tmpfs tmpfs $mntpoint
echo "Testing tmpfs(5)"
cp -a /usr/include $mntpoint
/tmp/readdir $mntpoint
umount $mntpoint

echo "Testing fdescfs(5)"
/tmp/readdir /dev/fs

echo "Testing procfs(5)"
mount -t procfs procfs $mntpoint
/tmp/readdir $mntpoint
umount $mntpoint

mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs -U md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
cp -a /usr/include $mntpoint
echo "Testing FFS"
/tmp/readdir $mntpoint
umount $mntpoint

mount -t nullfs /bin $mntpoint
echo "Testing nullfs(5)"
/tmp/readdir $mntpoint
umount $mntpoint

rm -f /tmp/readdir
exit 0
EOF
#include <sys/types.h>
#include <strings.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/wait.h>

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

int
test(char *path)
{

	DIR *dirp, fuzz;
	int i;

	signal(SIGSEGV, hand);

	for (i = 0; i < 2000; i++) {
		if ((dirp = opendir(path)) == NULL)
			break;
		bcopy(dirp, &fuzz, sizeof(fuzz));
		fuzz.dd_len = arc4random();
		readdir(&fuzz);
		closedir(dirp);
	}

	exit(0);
}

int
main(int argc __unused, char **argv)
{
	int i;

	for (i = 0; i < 1000; i++) {
		if (fork() == 0)
			test(argv[1]);
		wait(NULL);
	}

	return (0);
}
