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
# $FreeBSD: projects/stress2/misc/suj.sh 210724 2010-08-01 10:33:03Z pho $
#

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Page fault in softdep_revert_mkdir+0x4d seen and

# fsck updates "clean" FS:
# *** /tmp/dumpfs.1       2010-12-24 19:18:44.000000000 +0100
# --- /tmp/dumpfs.2       2010-12-24 19:18:46.000000000 +0100
# ***************
# *** 5,11 ****
#   frag  8       shift   3       fsbtodb 2
#   minfree       8%      optim   time    symlinklen 120
#   maxbsize 16384        maxbpg  2048    maxcontig 8     contigsumsize 8
# ! nbfree        62794   ndir    2       nifree  141307  nffree  25
#   bpg   11761   fpg     94088   ipg     23552   unrefs  0
#   nindir        2048    inopb   64      maxfilesize     140806241583103
#   sbsize        2048    cgsize  16384   csaddr  3000    cssize  2048
# --- 5,11 ----
#   frag  8       shift   3       fsbtodb 2
#   minfree       8%      optim   time    symlinklen 120
#   maxbsize 16384        maxbpg  2048    maxcontig 8     contigsumsize 8
# ! nbfree        62794   ndir    30      nifree  141307  nffree  25
#   bpg   11761   fpg     94088   ipg     23552   unrefs  0
#   nindir        2048    inopb   64      maxfilesize     140806241583103
#   sbsize        2048    cgsize  16384   csaddr  3000    cssize  2048

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj10.c
cc -o suj10 -Wall -O2 suj10.c
rm -f suj10.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
bsdlabel -w md$mdstart auto
newfs -U md${mdstart}$part > /dev/null
tunefs -j enable /dev/md${mdstart}$part
mount /dev/md${mdstart}$part $mntpoint
chmod 777 $mntpoint

su ${testuser} -c "cd $mntpoint; /tmp/suj10"

while mount | grep "$mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
dumpfs /dev/md${mdstart}$part | grep -v UFS2 > /tmp/dumpfs.1
sleep 1
fsck -t ufs -y -v /dev/md${mdstart}$part > /tmp/fsck.log 2>&1
dumpfs /dev/md${mdstart}$part | grep -v UFS2 > /tmp/dumpfs.2
diff -c /tmp/dumpfs.1 /tmp/dumpfs.2 || cat /tmp/fsck.log
mdconfig -d -u $mdstart
rm -f /tmp/fsck.log /tmp/dumpfs.?
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
/*
static int size = 14100;
Causes:
    Fatal trap 12: page fault while in kernel mode
    Stopped at softdep_revert_mkdir+0x4d: movl 0x28(%ebx),%eax
*/
//static int size = 14000;
static int size = 13000;

void
test(void)
{
	int fd, i, j;
	pid_t pid;
	char file[128];

	for (i = 0; i < 10; i++) {
		if (access("rendezvous", R_OK) == 0)
			break;
		sched_yield();
	}
	pid = getpid();
	sprintf(file,"d%05d", pid);
	if (mkdir(file, 0740) == -1)
		err(1, "mkdir(%s)", file);
	chdir(file);
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = mkdir(file, 0740)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s). %s:%d", file, __FILE__, __LINE__);
				unlink("continue");
				break;
			}
		}

	}
	sleep(3);

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (rmdir(file) == -1)
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
	int fd, i, j;

	umask(0);
	if ((fd = open("continue", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	for (i = 0; i < 1; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0) {
				test();
				exit(0);
			}
		}

		if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
			err(1, "open()");
		close(fd);
		
		for (j = 0; j < PARALLEL; j++)
			wait(NULL);

		unlink("rendezvous");
		if (access("continue", R_OK) == -1) {
			fprintf(stderr, "Loop #%d\n", i + 1); fflush(stderr);
			break;
		}
	}
	unlink("continue");

	return (0);
}
