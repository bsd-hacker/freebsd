/*	$NetBSD: linux_time.c,v 1.14 2006/05/14 03:40:54 christos Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(0, "$NetBSD: linux_time.c,v 1.14 2006/05/14 03:40:54 christos Exp $");
#endif

#include "opt_compat.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/sdt.h>
#include <sys/signal.h>
#include <sys/stdint.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_dtrace.h>

LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);
LIN_SDT_PROBE_DEFINE(time, native_to_linux_timespec, entry);
LIN_SDT_PROBE_ARGTYPE(time, native_to_linux_timespec, entry, 0,
    "struct l_timespec *");
LIN_SDT_PROBE_ARGTYPE(time, native_to_linux_timespec, entry, 1,
    "struct timespec *");
LIN_SDT_PROBE_DEFINE(time, native_to_linux_timespec, return);
LIN_SDT_PROBE_DEFINE(time, linux_to_native_timespec, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_timespec, entry, 0,
    "struct timespec *");
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_timespec, entry, 1,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_to_native_timespec, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_timespec, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(time, linux_to_native_clockid, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_clockid, entry, 0,
    "clockid_t *");
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_clockid, entry, 1,
    "clockid_t");
LIN_SDT_PROBE_DEFINE(time, linux_to_native_clockid, unsupported_clockid);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_clockid, unsupported_clockid, 0,
    "clockid_t");
LIN_SDT_PROBE_DEFINE(time, linux_to_native_clockid, unknown_clockid);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_clockid, unknown_clockid, 0,
    "clockid_t");
LIN_SDT_PROBE_DEFINE(time, linux_to_native_clockid, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_to_native_clockid, return, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_gettime, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, entry, 0, "clockid_t");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, entry, 1,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_clock_gettime, conversion_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, conversion_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_gettime, gettime_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, gettime_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_gettime, copyout_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, copyout_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_gettime, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_gettime, return, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_settime, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, entry, 0, "clockid_t");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, entry, 1,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_clock_settime, conversion_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, conversion_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_settime, settime_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, settime_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_settime, copyin_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, copyin_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_settime, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_settime, return, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, entry, 0, "clockid_t");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, entry, 1,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, nullcall);
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, conversion_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, conversion_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, getres_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, getres_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, copyout_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, copyout_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_getres, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_getres, return, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, entry, 0,
    "const struct l_timespec *");
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, entry, 1,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, conversion_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, conversion_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, nanosleep_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, nanosleep_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, copyout_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, copyout_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, copyin_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, copyin_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_nanosleep, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_nanosleep, return, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, entry);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, entry, 0,
    "clockid_t");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, entry, 1,
    "int");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, entry, 2,
    "struct l_timespec *");
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, entry, 3,
    "struct l_timespec *");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, conversion_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, conversion_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, nanosleep_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, nanosleep_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, copyout_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, copyout_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, copyin_error);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, copyin_error, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, unsupported_flags);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, unsupported_flags, 0, "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, unsupported_clockid);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, unsupported_clockid, 0,
    "int");
LIN_SDT_PROBE_DEFINE(time, linux_clock_nanosleep, return);
LIN_SDT_PROBE_ARGTYPE(time, linux_clock_nanosleep, return, 0, "int");

static void
native_to_linux_timespec(struct l_timespec *ltp, struct timespec *ntp)
{

	LIN_SDT_PROBE(time, native_to_linux_timespec, entry, ltp, ntp,
	    0, 0, 0);

	ltp->tv_sec = ntp->tv_sec;
	ltp->tv_nsec = ntp->tv_nsec;

	LIN_SDT_PROBE(time, native_to_linux_timespec, return, 0, 0,
	    0, 0, 0);
}

static int
linux_to_native_timespec(struct timespec *ntp, struct l_timespec *ltp)
{

	LIN_SDT_PROBE(time, linux_to_native_timespec, entry, ntp, ltp,
	    0, 0, 0);

	if (ltp->tv_sec < 0 || ltp->tv_nsec > (l_long)999999999L) {
		LIN_SDT_PROBE(time, linux_to_native_timespec, return, EINVAL,
		    0, 0, 0, 0);
		return (EINVAL);
	}
	ntp->tv_sec = ltp->tv_sec;
	ntp->tv_nsec = ltp->tv_nsec;

	LIN_SDT_PROBE(time, linux_to_native_timespec, return, 0, 0,
	    0, 0, 0);
	return (0);
}

static int
linux_to_native_clockid(clockid_t *n, clockid_t l)
{

	LIN_SDT_PROBE(time, linux_to_native_clockid, entry, n, l,
	    0, 0, 0);

	switch (l) {
	case LINUX_CLOCK_REALTIME:
		*n = CLOCK_REALTIME;
		break;
	case LINUX_CLOCK_MONOTONIC:
		*n = CLOCK_MONOTONIC;
		break;
	case LINUX_CLOCK_PROCESS_CPUTIME_ID:
	case LINUX_CLOCK_THREAD_CPUTIME_ID:
	case LINUX_CLOCK_REALTIME_HR:
	case LINUX_CLOCK_MONOTONIC_HR:
		LIN_SDT_PROBE(time, linux_to_native_clockid,
		    unsupported_clockid, l, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_to_native_clockid, return, EINVAL,
		    0, 0, 0, 0);
		return (EINVAL);
		break;
	default:
		LIN_SDT_PROBE(time, linux_to_native_clockid,
		    unknown_clockid, l, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_to_native_clockid, return, EINVAL,
		    0, 0, 0, 0);
		return (EINVAL);
		break;
	}

	LIN_SDT_PROBE(time, linux_to_native_clockid, return, 0, 0,
	    0, 0, 0);
	return (0);
}

int
linux_clock_gettime(struct thread *td, struct linux_clock_gettime_args *args)
{
	struct l_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */
	struct timespec tp;

	LIN_SDT_PROBE(time, linux_clock_gettime, entry, args->which, args->tp,
	    0, 0, 0);

	error = linux_to_native_clockid(&nwhich, args->which);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_gettime, conversion_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_gettime, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	error = kern_clock_gettime(td, nwhich, &tp);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_gettime, gettime_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_gettime, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	native_to_linux_timespec(&lts, &tp);

	error = copyout(&lts, args->tp, sizeof lts);
	if (error != 0)
		LIN_SDT_PROBE(time, linux_clock_gettime, copyout_error, error,
		    0, 0, 0, 0);
	LIN_SDT_PROBE(time, linux_clock_gettime, return, error, 0, 0, 0,
	    0);
	return (error);
}

int
linux_clock_settime(struct thread *td, struct linux_clock_settime_args *args)
{
	struct timespec ts;
	struct l_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */

	LIN_SDT_PROBE(time, linux_clock_settime, entry, args->which, args->tp,
	    0, 0, 0);

	error = linux_to_native_clockid(&nwhich, args->which);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_settime, conversion_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_settime, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	error = copyin(args->tp, &lts, sizeof lts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_settime, copyin_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_settime, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	error = linux_to_native_timespec(&ts, &lts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_settime, conversion_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_settime, return, error, 0, 0, 0,
		    0);
		return (error);
	}

	error = kern_clock_settime(td, nwhich, &ts);
	if (error != 0)
		LIN_SDT_PROBE(time, linux_clock_settime, settime_error, error,
		    0, 0, 0, 0);
	LIN_SDT_PROBE(time, linux_clock_settime, return, error, 0, 0, 0,
	    0);
	return (error);
}

int
linux_clock_getres(struct thread *td, struct linux_clock_getres_args *args)
{
	struct timespec ts;
	struct l_timespec lts;
	int error;
	clockid_t nwhich = 0;	/* XXX: GCC */

	LIN_SDT_PROBE(time, linux_clock_getres, entry, args->which, args->tp,
	    0, 0, 0);

	if (args->tp == NULL) {
		LIN_SDT_PROBE(time, linux_clock_getres, nullcall, 0, 0, 0, 0,
		    0);
		LIN_SDT_PROBE(time, linux_clock_getres, return, 0, 0, 0, 0, 0);
	  	return (0);
	}

	error = linux_to_native_clockid(&nwhich, args->which);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_getres, conversion_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_getres, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	error = kern_clock_getres(td, nwhich, &ts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_getres, getres_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_getres, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	native_to_linux_timespec(&lts, &ts);

	error = copyout(&lts, args->tp, sizeof lts);
	if (error != 0)
		LIN_SDT_PROBE(time, linux_clock_getres, copyout_error, error,
		    0, 0, 0, 0);
	LIN_SDT_PROBE(time, linux_clock_getres, return, error, 0, 0, 0,
	    0);
	return (error);
}

int
linux_nanosleep(struct thread *td, struct linux_nanosleep_args *args)
{
	struct timespec *rmtp;
	struct l_timespec lrqts, lrmts;
	struct timespec rqts, rmts;
	int error;

	LIN_SDT_PROBE(time, linux_nanosleep, entry, args->rqtp, args->rmtp,
	    0, 0, 0);

	error = copyin(args->rqtp, &lrqts, sizeof lrqts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_nanosleep, copyin_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_nanosleep, return, error, 0, 0, 0,
		    0);
		return (error);
	}

	if (args->rmtp != NULL)
	   	rmtp = &rmts;
	else
	   	rmtp = NULL;

	error = linux_to_native_timespec(&rqts, &lrqts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_nanosleep, conversion_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_nanosleep, return, error, 0, 0, 0,
		    0);
		return (error);
	}
	error = kern_nanosleep(td, &rqts, rmtp);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_nanosleep, nanosleep_error, error,
		    0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_nanosleep, return, error, 0, 0, 0,
		    0);
		return (error);
	}

	if (args->rmtp != NULL) {
	   	native_to_linux_timespec(&lrmts, rmtp);
	   	error = copyout(&lrmts, args->rmtp, sizeof(lrmts));
		if (error != 0) {
			LIN_SDT_PROBE(time, linux_nanosleep, copyout_error,
			    error, 0, 0, 0, 0);
			LIN_SDT_PROBE(time, linux_nanosleep, return, error,
			    0, 0, 0, 0);
		   	return (error);
		}
	}

	LIN_SDT_PROBE(time, linux_nanosleep, return, 0, 0, 0, 0, 0);
	return (0);
}

int
linux_clock_nanosleep(struct thread *td, struct linux_clock_nanosleep_args *args)
{
	struct timespec *rmtp;
	struct l_timespec lrqts, lrmts;
	struct timespec rqts, rmts;
	int error;

	LIN_SDT_PROBE(time, linux_clock_nanosleep, entry, args->which,
	    args->flags, args->rqtp, args->rmtp, 0);

	if (args->flags != 0) {
		LIN_SDT_PROBE(time, linux_clock_nanosleep, unsupported_flags,
		    args->flags, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_nanosleep, return, EINVAL, 0,
		    0, 0, 0);
		return (EINVAL);	/* XXX deal with TIMER_ABSTIME */
	}

	if (args->which != LINUX_CLOCK_REALTIME) {
		LIN_SDT_PROBE(time, linux_clock_nanosleep, unsupported_clockid,
		    args->which, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_nanosleep, return, EINVAL, 0,
		    0, 0, 0);
		return (EINVAL);
	}

	error = copyin(args->rqtp, &lrqts, sizeof lrqts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_nanosleep, copyin_error,
		    error, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_nanosleep, return, error, 0,
		    0, 0, 0);
		return (error);
	}

	if (args->rmtp != NULL)
	   	rmtp = &rmts;
	else
	   	rmtp = NULL;

	error = linux_to_native_timespec(&rqts, &lrqts);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_nanosleep, conversion_error,
		    error, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_nanosleep, return, error, 0,
		    0, 0, 0);
		return (error);
	}
	error = kern_nanosleep(td, &rqts, rmtp);
	if (error != 0) {
		LIN_SDT_PROBE(time, linux_clock_nanosleep, nanosleep_error,
		    error, 0, 0, 0, 0);
		LIN_SDT_PROBE(time, linux_clock_nanosleep, return, error, 0,
		    0, 0, 0);
		return (error);
	}

	if (args->rmtp != NULL) {
	   	native_to_linux_timespec(&lrmts, rmtp);
	   	error = copyout(&lrmts, args->rmtp, sizeof lrmts );
		if (error != 0) {
			LIN_SDT_PROBE(time, linux_clock_nanosleep,
			    copyout_error, error, 0, 0, 0, 0);
			LIN_SDT_PROBE(time, linux_nanosleep, return, error,
			    0, 0, 0, 0);
		   	return (error);
		}
	}

	LIN_SDT_PROBE(time, linux_clock_nanosleep, return, 0, 0, 0, 0, 0);
	return (0);
}
