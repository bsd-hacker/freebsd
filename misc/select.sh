#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# The combination of ualarm() firing before and after the select(2) timeout
# triggers select() to return EINTR a number of times. Not seen on Lunux or
# OS X. Problem only seen on i386.

# Test scenario suggestion by kib@

# "FAIL n = 2389" seen on r302369, no debug build.
# Fixed by: r302573.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/select.c
mycc -o select -Wall -Wextra -O0 -g select.c || exit 1
rm -f select.c
cd $odir

/tmp/select
s=$?

rm -f /tmp/select
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

volatile u_int *share;
volatile int alarms;
int lines;

#define N 2000 /* also seen fail with N = 20.000 */
#define LINES 128000
#define PARALLEL 16 /* Fails seen with 1 - 16 */
#define RUNTIME (10 * 60)
#define SYNC 0

void
handler(int i __unused) {
	alarms++;
}

void
test(void)
{
	struct timeval tv;
	int i, n, r, s;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	signal(SIGALRM, handler);
	s = 0;
	for (i = 0; i < lines; i++) {
		if (arc4random() % 100 < 50)
			ualarm(N / 2, 0);
		else
			ualarm(N * 2, 0);
		tv.tv_sec  = 0;
		tv.tv_usec = N;
		n = 0;
		do {
			r = select(1, NULL, NULL, NULL, &tv);
			n++;
		} while (r == -1 && errno == EINTR);
		if (r == -1)
			err(1, "select");
		ualarm(0, 0);
		if (n > 2) {
			fprintf(stderr, "FAIL n = %d, tv = %ld.%06ld\n",
			    n, (long)tv.tv_sec, tv.tv_usec);
			s = 1;
			break;
		}

	}

	_exit(s);
}

int
main(void)
{
	size_t len;
	time_t start;
	int e, i, j, pids[PARALLEL], status;

	lines = LINES / PARALLEL;
	if (lines == 0)
		lines = 1;
	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++) {
			waitpid(pids[i], &status, 0);
			e += status == 0 ? 0 : 1;
			if (status != 0) {
				for (j = i + 1; j < PARALLEL; j++)
					kill(pids[j], SIGINT);
			}
		}
	}

	return (e);
}
