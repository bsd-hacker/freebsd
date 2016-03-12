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

# Zero signal number seen.
# Test scenario by Vitaly Magerya <vmagerya gmail com>
# http://people.freebsd.org/~pho/stress/log/kostik646.txt
# Panic fixed in r258497
# Signal number fixed in r258499

# panic: vm_fault: fault on nofault entry, addr: cdbbe000
# https://people.freebsd.org/~pho/stress/log/kostik838.txt
# Fixed by r289661.

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/signal0.c
c99 -o signal0  signal0.c -lpthread || exit 1
rm -f signal0.c
cd $odir

(cd ../testcases/swap; ./swap -t 5m -i 20 -h -v) > /dev/null 2>&1 &
for i in `jot 500`; do
	/tmp/signal0
done
while pkill -9 swap; do
	:
done
wait

rm -f /tmp/signal0
exit

EOF
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void signal_handler(int signum, siginfo_t *si, void *context) {
     if (signum != SIGUSR1) {
         printf("FAIL bad signal, signum=%d\n", signum);
         exit(1);
     }
}

void *thread_func(void *arg) {
     return arg;
}

int main(void) {
     struct sigaction sa = { 0 };
     sa.sa_flags = SA_SIGINFO;
     sa.sa_sigaction = signal_handler;
     if (sigfillset(&sa.sa_mask) != 0) abort();
     if (sigaction(SIGUSR1, &sa, NULL) != 0) abort();
     for (int i = 0; i < 10000; i++) {
         pthread_t t;
         pthread_create(&t, NULL, thread_func, NULL);
         pthread_kill(t, SIGUSR1);
	/*
	 Side note.  pthread_kill(3) call behaviour is undefined if pthread_create(3)
	 in the line before failed.
	 */

     }
     return 0;
}
