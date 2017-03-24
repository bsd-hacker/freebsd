#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# sem_timedwait(2) test.

# Test scenario by Eric van Gyzen <vangyzen@FreeBSD.org>
# for changes committed as r315280.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sem_timedwait.c
mycc -o sem_timedwait -Wall -Wextra -O0 -g sem_timedwait.c || exit 1
rm -f sem_timedwait.c
cd $odir

for i in `jot 900`; do
	date -f %s $((`date +%s` + `jot -r 1 -5 5`)) > /dev/null
	sleep .`jot -r 1 1 9`
done &
pid=$!

s=0
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	st=`date +%T`
	/tmp/sem_timedwait > /dev/null || { s=$?; break; }
done
kill $pid > /dev/null 2>&1
wait

rm -rf /tmp/sem_timedwait
exit $s

EOF
#include <err.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int
main(void)
{
    sem_t sem;
    int result;
    struct timespec ts;

    alarm(600);
    result = sem_init(&sem, 0, 0);
    if (result)
        err(1, "sem_init");

    result = clock_gettime(CLOCK_REALTIME, &ts);
    if (result)
        err(1, "clock_gettime");

    ts.tv_sec += 10;

    result = sem_timedwait(&sem, &ts);
    if (result == 0) {
        errx(1, "sem_timedwait succeeded?!");
    } else if (errno != ETIMEDOUT) {
        err(1, "clock_timedwait");
    }

    puts("timeout");

    return (0);
}
