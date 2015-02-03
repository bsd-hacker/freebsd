#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Test scenario for the change of a global SU lock to a per filesystem lock.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pfl.c
cc -o pfl -Wall -Wextra pfl.c || exit 1
rm -f pfl.c
cd $here

mp1=$mntpoint
mp2=${mntpoint}2
[ -d $mp2 ] || mkdir -p $mp2
md1=$mdstart
md2=$((mdstart + 1))

opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-j" || echo "-U")
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
mdconfig -l | grep -q md$md1 &&  mdconfig -d -u $md1
mdconfig -a -t swap -s 2g -u $md1
bsdlabel -w md$md1 auto
newfs $opt md${md1}$part > /dev/null
mount /dev/md${md1}$part $mp1
chmod 777 $mp1

mount | grep "on $mp2 " | grep -q /dev/md && umount -f $mp2
mdconfig -l | grep -q md$md2 &&  mdconfig -d -u $md2
mdconfig -a -t swap -s 2g -u $md2
bsdlabel -w md$md2 auto
newfs $opt md${md2}$part > /dev/null
mount /dev/md${md2}$part $mp2
chmod 777 $mp2

su ${testuser} -c "cd $mp1; /tmp/pfl" &
su ${testuser} -c "cd $mp2; /tmp/pfl" &
wait; wait

while mount | grep "$mp2 " | grep -q /dev/md; do
	umount $mp2 || sleep 1
done
mdconfig -d -u $md2
while mount | grep "$mp1 " | grep -q /dev/md; do
	umount $mp1 || sleep 1
done
mdconfig -d -u $md1
exit

EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 10
static int size = 10000;

void
test(void)
{
	int fd, i, j;
	pid_t pid;
	char file[128];

	pid = getpid();
	sprintf(file,"d%05d", pid);
	if (mkdir(file, 0740) == -1)
		err(1, "mkdir(%s)", file);
	chdir(file);
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1) {
			if (errno != EINTR) {
				warn("mkdir(%s). %s:%d", file, __FILE__, __LINE__);
				unlink("continue");
				break;
			}
		}
		if (arc4random() % 100 < 10)
			if (write(fd, "1", 1) != 1)
				err(1, "write()");
		close(fd);

	}
	sleep(3);

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}
	chdir("..");
	sprintf(file,"d%05d", pid);
	if (rmdir(file) == -1)
		err(3, "unlink(%s)", file);
}

int
main(void)
{
	int fd, i, j, k;

	umask(0);
	if ((fd = open("continue", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0) {
				for (k = 0; k < 50; k++)
					test();
				exit(0);
			}
		}

		for (j = 0; j < PARALLEL; j++)
			wait(NULL);

		if (access("continue", R_OK) == -1) {
			fprintf(stderr, "Loop #%d\n", i + 1); fflush(stderr);
			break;
		}
	}
	unlink("continue");

	return (0);
}
