#!/bin/sh

# Regression test for
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=217435
# by Tim Newsham <tim newsham nccgroup trust>

# panic: Assertion size > 0 failed at ../../../kern/subr_vmem.c:1082
# cpuid = 2
# time = 1501182301
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe0173117650
# vpanic() at vpanic+0x19c/frame 0xfffffe01731176d0
# kassert_panic() at kassert_panic+0x126/frame 0xfffffe0173117740
# vmem_alloc() at vmem_alloc+0x11b/frame 0xfffffe0173117780
# kmem_malloc() at kmem_malloc+0x33/frame 0xfffffe01731177b0
# uma_large_malloc() at uma_large_malloc+0x48/frame 0xfffffe01731177f0
# malloc() at malloc+0xe3/frame 0xfffffe0173117840
# ktrgenio() at ktrgenio+0x60/frame 0xfffffe0173117880
# sys_kevent() at sys_kevent+0x12f/frame 0xfffffe0173117930
# amd64_syscall() at amd64_syscall+0x7d2/frame 0xfffffe0173117ab0

# Fixed by r315155.

. ../default.cfg

cat > /tmp/kevent10.c <<EOF
#include <sys/param.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	char *fn = "/tmp/trace";
	struct kevent changes;
	struct kevent events;

	if (open(fn, O_RDWR | O_CREAT, 0666) == -1)
		err(1, "%s", fn);
	if (ktrace(fn, KTRFLAG_DESCEND | KTROP_SET, KTRFAC_GENIO, 0) == -1)
		err(1, "ktrace");
	memset(&changes, 0, sizeof(struct kevent));
	memset(&events, 0, sizeof(struct kevent));
	if (kevent(0, &changes, -1, &events, 1, 0) == -1)
		if (errno != EBADF)
			err(1, "kevent");
	return (0);
}
EOF

mycc -o /tmp/kevent10 -Wall -Wextra -O2 /tmp/kevent10.c || exit 1
rm /tmp/kevent10.c

/tmp/kevent10
s=$?

rm /tmp/kevent10
exit $s
