/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "un-namespace.h"

#include "thr_private.h"

/*
 * Prototypes
 */
int	__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int	__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec * abstime);
static int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
static int cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
		    const struct timespec *abstime, int cancel);
static int cond_signal_common(pthread_cond_t *cond, int broadcast);

/*
 * Double underscore versions are cancellation points.  Single underscore
 * versions are not and are provided for libc internal usage (which
 * shouldn't introduce cancellation points).
 */
__weak_reference(__pthread_cond_wait, pthread_cond_wait);
__weak_reference(__pthread_cond_timedwait, pthread_cond_timedwait);

__weak_reference(_pthread_cond_init, pthread_cond_init);
__weak_reference(_pthread_cond_destroy, pthread_cond_destroy);
__weak_reference(_pthread_cond_signal, pthread_cond_signal);
__weak_reference(_pthread_cond_broadcast, pthread_cond_broadcast);

static int
cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	pthread_cond_t	pcond;
	int             rval = 0;

	if ((pcond = (pthread_cond_t)
	    calloc(1, sizeof(struct pthread_cond))) == NULL) {
		rval = ENOMEM;
	} else {
		/*
		 * Initialise the condition variable structure:
		 */
		if (cond_attr == NULL || *cond_attr == NULL) {
			pcond->c_kerncv.c_clockid = CLOCK_REALTIME;
		} else {
			if ((*cond_attr)->c_pshared)
				pcond->c_kerncv.c_flags |= USYNC_PROCESS_SHARED;
			pcond->c_kerncv.c_clockid = (*cond_attr)->c_clockid;
		}
		_thr_umutex_init(&pcond->c_lock);
		pcond->c_kerncv.c_flags |= UCOND_BIND_MUTEX;
		*cond = pcond;
	}
	/* Return the completion status: */
	return (rval);
}

static int
init_static(struct pthread *thread, pthread_cond_t *cond)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_cond_static_lock);

	if (*cond == NULL)
		ret = cond_init(cond, NULL);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_cond_static_lock);

	return (ret);
}

#define CHECK_AND_INIT_COND							\
	if (__predict_false((cv = (*cond)) <= THR_COND_DESTROYED)) {		\
		if (cv == THR_COND_INITIALIZER) {				\
			int ret;						\
			ret = init_static(_get_curthread(), cond);		\
			if (ret)						\
				return (ret);					\
		} else if (cv == THR_COND_DESTROYED) {				\
			return (EINVAL);					\
		}								\
		cv = *cond;							\
	}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{

	*cond = NULL;
	return (cond_init(cond, cond_attr));
}

int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	struct pthread_cond	*cv;
	int			rval = 0;

	if ((cv = *cond) == THR_COND_INITIALIZER)
		rval = 0;
	else if (cv == THR_COND_DESTROYED)
		rval = EINVAL;
	else {
		cv = *cond;
		if (cv->c_waiters != 0)
			return (EBUSY);
		_thr_ucond_broadcast(&cv->c_kerncv);
		*cond = THR_COND_DESTROYED;

		/*
		 * Free the memory allocated for the condition
		 * variable structure:
		 */
		free(cv);
	}
	return (rval);
}

/*
 * Cancellation behaivor:
 *   Thread may be canceled at start, if thread is canceled, it means it
 *   did not get a wakeup from pthread_cond_signal(), otherwise, it is
 *   not canceled.
 *   Thread cancellation never cause wakeup from pthread_cond_signal()
 *   to be lost.
 */
static int
cond_wait_kernel(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	pthread_cond_t  cv;
	struct pthread_mutex *m;
	int	recurse;
	int	ret;

	cv = *cond;
	ret = _mutex_cv_detach(mutex, &recurse);
	if (__predict_false(ret != 0))
		return (ret);
	m = *mutex;

	if (cancel) {
		_thr_cancel_enter2(curthread, 0);
		ret = _thr_ucond_wait(&cv->c_kerncv, &m->m_lock, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
		_thr_cancel_leave(curthread, 0);
	} else {
		ret = _thr_ucond_wait(&cv->c_kerncv, &m->m_lock, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
	}

	if (ret == 0) {
		_mutex_cv_lock(mutex, recurse);
	} else if (ret == EINTR || ret == ETIMEDOUT) {
		_mutex_cv_lock(mutex, recurse);
		if (cancel)
			_thr_testcancel(curthread);
		if (ret == EINTR)
			ret = 0;
	} else {
		/* We know that it didn't unlock the mutex. */
		_mutex_cv_attach(mutex, recurse);
		if (cancel)
			_thr_testcancel(curthread);
	}
	return (ret);
}

static int
cond_wait_user(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	int		recurse;
	pthread_cond_t  cv;
	int		ret;
	uint64_t	seq, bseq;

	cv = *cond;
	THR_UMUTEX_LOCK(curthread, &cv->c_lock);
	cv->c_waiters++;
	ret = _mutex_cv_unlock(mutex, &recurse);
	if (__predict_false(ret != 0)) {
		cv->c_waiters--;
		THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
		return (ret);
	}

	if (abstime != NULL) {
		clock_gettime(cv->c_kerncv.c_clockid, &ts);
		TIMESPEC_SUB(&ts2, abstime, &ts);
		tsp = &ts2;
	} else
		tsp = NULL;

	bseq = cv->c_broadcast_seq;
	for(;;) {
		seq = cv->c_seq;
		THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);

		if (cancel) {
			_thr_cancel_enter2(curthread, 0);
			ret = _thr_umtx_wait_uint((u_int *)&cv->c_seq,
				(u_int)seq, tsp, 0);
			_thr_cancel_leave(curthread, 0);
		} else {
			ret = _thr_umtx_wait_uint((u_int *)&cv->c_seq,
				(u_int)seq, tsp, 0);
		}

		THR_UMUTEX_LOCK(curthread, &cv->c_lock);
		if (cv->c_broadcast_seq != bseq) {
			ret = 0;
			break;
		}
		if (cv->c_signaled > 0) {
			cv->c_signaled--;
			ret = 0;
			break;
		} else if (cancel && SHOULD_CANCEL(curthread) &&
			   !THR_IN_CRITICAL(curthread)) {
				THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
				_pthread_exit(PTHREAD_CANCELED);
		}
	}
	THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);

	if (ret == EINTR)
		ret = 0;
	_mutex_cv_lock(mutex, recurse);
	return (ret);
}


static int
cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread_mutex *m;
	pthread_cond_t  cv;
	int err;

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	CHECK_AND_INIT_COND

	if ((err = _mutex_owned(curthread, mutex)) != 0)
		return (err);

	m = *mutex;
	if ((m->m_lock.m_flags & USYNC_PROCESS_SHARED) !=
	    (cv->c_kerncv.c_flags & USYNC_PROCESS_SHARED))
		return (EINVAL);

	if (m->m_lock.m_flags & (UMUTEX_PRIO_PROTECT|UMUTEX_PRIO_INHERIT)) 
		return cond_wait_kernel(cond, mutex, abstime, cancel);
	else
		return cond_wait_user(cond, mutex, abstime, cancel);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 0));
}

int
__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 1));
}

int
_pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 0));
}

int
__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 1));
}

static int
cond_signal_common(pthread_cond_t *cond, int broadcast)
{
	struct pthread	*curthread = _get_curthread();
	pthread_cond_t	cv;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	if (!broadcast)
		_thr_ucond_signal(&cv->c_kerncv);
	else
		_thr_ucond_broadcast(&cv->c_kerncv);

	if (cv->c_waiters == 0)
		return (0);

	THR_UMUTEX_LOCK(curthread, &cv->c_lock);
	if (cv->c_waiters > 0) {
		if (!broadcast) {
			cv->c_seq++;
			cv->c_signaled++;
			cv->c_waiters--;
			_thr_umtx_wake(&cv->c_seq, 1, 0);
		} else {
			cv->c_seq++;
			cv->c_broadcast_seq++;
			cv->c_waiters = 0;
			cv->c_signaled = 0;
			_thr_umtx_wake(&cv->c_seq, INT_MAX, 0);
		}
	}
	THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
	return (0);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{

	return (cond_signal_common(cond, 0));
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{

	return (cond_signal_common(cond, 1));
}
