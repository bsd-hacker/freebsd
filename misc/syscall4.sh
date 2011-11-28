#!/bin/sh

#
# Copyright (c) 2011 Peter Holm
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

# Threaded syscall(2) fuzz test inspired by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > syscall4.c
rm -f /tmp/syscall4
cc -o syscall4 -Wall -Wextra -O2 -g syscall4.c -lpthread || exit 1
rm -f syscall4.c

kldstat -v | grep -q sysvmsg  || kldload sysvmsg
kldstat -v | grep -q sysvsem  || kldload sysvsem
kldstat -v | grep -q sysvshm  || kldload sysvshm
kldstat -v | grep -q aio      || kldload aio
kldstat -v | grep -q mqueuefs || kldload mqueuefs

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs -U md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
chmod 777 $mntpoint

for i in `jot 10`; do
	(cd $mntpoint; /tmp/syscall4 $* < /dev/null) &
	sleep 180
	killall -9 syscall4
	ipcs | awk '/^(q|m|s)/ {print " -" $1, $2}' | xargs -L 1 ipcrm
done

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/syscall4
exit
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static int ignore[] = {
	SYS_syscall,
	SYS_exit,
	SYS_fork,
	11,			/* 11 is obsolete execv */
	SYS_unmount,
	SYS_reboot,
	SYS_vfork,
	109,			/* 109 is old sigblock */
	111,			/* 111 is old sigsuspend */
	SYS_shutdown,
	SYS___syscall,
	SYS_rfork,
	SYS_sigsuspend,
	SYS_mac_syscall,
	SYS_sigtimedwait,
	SYS_sigwaitinfo,
};

int fd[900], fds[2], socketpr[2];
#define N (128 * 1024 / (int)sizeof(u_int32_t))
u_int32_t r[N];
int syscallno;

#define		UID_NOBODY	65534
#define		GID_NOBODY	65534

static int
random_int(int mi, int ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

unsigned long
makearg(void)
{
	unsigned int i, val;

	val = arc4random();
	i   = arc4random() % 100;
	if (i < 20)
		val = val & 0xff;
	if (i >= 20 && i < 40)
		val = val & 0xffff;
	if (i >= 40 && i < 60)
		val = (unsigned long)(r) + (val & 0xffff);

	return(val);
}

void *
test(void *arg __unused)
{

	FTS		*fts;
	FTSENT		*p;
	int		ftsoptions;
	char 		*args[5];
	int i;

	ftsoptions = FTS_PHYSICAL;
	ftsoptions = 0;
	args[0] = "/dev";
	args[1] = "/proc";
	args[2] = "/usr/compat/linux/proc";
	args[3] = ".";
	args[4] = 0;

	for (;;) {
		for (i = 0; i < N; i++)
			r[i] = arc4random();
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		i = 0;
		while ((p = fts_read(fts)) != NULL) {
			if (fd[i] > 0)
				close(fd[i]);
			if ((fd[i] = open(p->fts_path, O_RDWR)) == -1)
				if ((fd[i] = open(p->fts_path, O_WRONLY)) == -1)
					if ((fd[i] = open(p->fts_path, O_RDONLY)) == -1)
						continue;
			i++;
			i = i % 900;
		}

		if (errno != 0 && errno != ENOENT)
			warn("fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
		if (pipe(fds) == -1)
			err(1, "pipe()");
		if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, socketpr) == -1)
			err(1, "socketpair()");
		sleep(1);
		close(socketpr[0]);
		close(socketpr[1]);
		close(fds[0]);
		close(fds[1]);
	}
	return(0);
}

void *
calls(void *arg __unused)
{
	int i, j, num;
	unsigned long arg1, arg2, arg3, arg4, arg5, arg6, arg7;

	for (i = 0;; i++) {
		if (i == 0)
			usleep(1000);
		num = syscallno;
		while (num == 0) {
			num = random_int(0, SYS_MAXSYSCALL);
			for (j = 0; j < (int)sizeof(ignore) / (int)sizeof(ignore[0]); j++)
				if (num == ignore[j]) {
					num = 0;
					break;
				}
		}
		arg1 = makearg();
		arg2 = makearg();
		arg3 = makearg();
		arg4 = makearg();
		arg5 = makearg();
		arg6 = makearg();
		arg7 = makearg();

#if 0
		fprintf(stderr, "%2d : syscall(%3d, %lx, %lx, %lx, %lx, %lx, %lx, %lx)\n",
			i, num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		sleep(2);
#endif
		alarm(1);
		syscall(num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		num = 0;
	}

	return (0);
}

int
main(int argc, char **argv)
{
	pthread_t rp, cp[50];
	int i, j;

	if (setgid(GID_NOBODY) == -1)
		err(1, "setgid(%d)", GID_NOBODY);
	if (setuid(UID_NOBODY) == -1)
		err(1, "setuid(%d)", UID_NOBODY);

	signal(SIGALRM, hand);
	signal(SIGILL,  hand);
	signal(SIGFPE,  hand);
	signal(SIGSEGV, hand);
	signal(SIGBUS,  hand);
	signal(SIGURG,  hand);
	signal(SIGSYS,  hand);
	signal(SIGTRAP, hand);

	if (argc == 2)
		syscallno = atoi(argv[1]);

	for (i = 0; i < 100000; i++) {
		if (fork() == 0) {
			arc4random_stir();
			if (pthread_create(&rp, NULL, test, NULL) != 0)
				perror("pthread_create");
			usleep(1000);
			for (j = 0; j < 50; j++) 
				if (pthread_create(&cp[j], NULL, calls, NULL) != 0)
					perror("pthread_create");

			for (j = 0; j < 50; j++) 
				pthread_join(cp[j], NULL);
			exit(0);
		}
		wait(NULL);
	}

	return (0);
}
