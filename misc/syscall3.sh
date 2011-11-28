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

# Test random syscalls with random arguments.
# Regression test of r209697

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > syscall3.c
cc -o syscall3 -Wall syscall3.c
rm -f syscall3.c

kldstat -v | grep -q sysvmsg  || kldload sysvmsg
kldstat -v | grep -q sysvsem  || kldload sysvsem
kldstat -v | grep -q sysvshm  || kldload sysvshm

kldstat -v | grep -q aio      || kldload aio
kldstat -v | grep -q mqueuefs || kldload mqueuefs

mkdir -p $RUNDIR/syscall3
cd $RUNDIR/syscall3

for i in `jot 4`; do
	su $testuser -c /tmp/syscall3 &
done
for i in `jot 4`; do
	wait
done

chflags -R 0 $RUNDIR/syscall3
rm -rf /tmp/syscall3 $RUNDIR/syscall3
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static unsigned int ignore[] = {
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

void
handler(int i) {
	_exit(0);
}

int
main(int argc, char **argv)
{
	unsigned int i;
	unsigned int arg1, arg2, arg3, arg4, arg5, arg6, arg7, num;

	signal(SIGSYS, SIG_IGN);
	signal(SIGALRM, handler);
	alarm(600);
	for (;;) {
		num = 0;
		while (num == 0) {
			num  = arc4random();
			for (i = 0; i < sizeof(ignore) / sizeof(ignore[0]); i++)
				if (num == ignore[i]) {
					num = 0;
					break;
				}
		}
		arg1 = arc4random();
		arg2 = arc4random();
		arg3 = arc4random();
		arg4 = arc4random();
		arg5 = arc4random();
		arg6 = arc4random();
		arg7 = arc4random();

		if (argc > 1)
			printf("%2d : syscall(%3d, %x, %x, %x, %x, %x, %x, %x)\n",
				i, num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		syscall(num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	}

	return (0);
}
