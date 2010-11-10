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
static int cond_signal_common(struct pthread_cond *cond);
static int cond_broadcast_common(struct pthread_cond *cond);

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

int _pthread_cond_init_1_0(pthread_cond_old_t *, const pthread_condattr_t *);
int _pthread_cond_signal_1_0(pthread_cond_old_t *);
int _pthread_cond_destroy_1_0(pthread_cond_old_t *);
int _pthread_cond_wait_1_0(pthread_cond_old_t *, pthread_mutex_old_t *);
int _pthread_cond_timedwait_1_0(pthread_cond_old_t *, pthread_mutex_old_t *,
	const struct timespec *);
int _pthread_cond_broadcast_1_0(pthread_cond_old_t *);

#define CV_PSHARED(cvp)	(((cvp)->__flags & USYNC_PROCESS_SHARED) != 0)

static int
cond_init(struct pthread_cond *cvp, const pthread_condattr_t *cond_attr)
{
	int	error = 0;

	/*
	 * Initialise the condition variable structure:
	 */
	memset(cvp, 0, sizeof(*cvp));
	if (cond_attr == NULL || *cond_attr == NULL) {
		cvp->__clock_id = CLOCK_REALTIME;
	} else {
		if ((*cond_attr)->c_pshared)
			cvp->__flags |= USYNC_PROCESS_SHARED;
		cvp->__clock_id = (*cond_attr)->c_clockid;
	}
	return (error);
}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{

	return (cond_init(cond, cond_attr));
}

static int
cond_destroy_common(pthread_cond_t *cvp)
{
	int	error = 0;

	if (cvp->__refcount == 0)
		goto next;
	_thr_umtx_lock_spin(&cvp->__lock);
	if (cvp->__waiters > 0) {
		_thr_umtx_unlock(&cvp->__lock);
		return (EBUSY);
	}
	while (cvp->__refcount != 0) {
		cvp->__destroying = 1;
		_thr_umtx_unlock(&cvp->__lock);
		_thr_umtx_wait_uint((u_int *)&cvp->__destroying,
				1, NULL, CV_PSHARED(cvp));
		_thr_umtx_lock_spin(&cvp->__lock);
	}
	_thr_umtx_unlock(&cvp->__lock);
next:
	_thr_ucond_broadcast((struct ucond *)&cvp->__kern_has_waiters);
	return (error);
}

int
_pthread_cond_destroy(pthread_cond_t *cvp)
{
	return cond_destroy_common(cvp);
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
cond_wait_kernel(struct pthread_cond *cvp, struct pthread_mutex *mp,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	int	recurse;
	int	error, error2 = 0;

	error = _mutex_cv_detach(mp, &recurse);
	if (__predict_false(error != 0))
		return (error);

	if (cancel) {
		_thr_cancel_enter2(curthread, 0);
		error = _thr_ucond_wait((struct ucond *)&cvp->__kern_has_waiters,
			(struct umutex *)&mp->__lockword, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
		_thr_cancel_leave(curthread, 0);
	} else {
		error = _thr_ucond_wait((struct ucond *)&cvp->__kern_has_waiters,
			(struct umutex *)&mp->__lockword, abstime,
			CVWAIT_ABSTIME|CVWAIT_CLOCKID);
	}

	/*
	 * Note that PP mutex and ROBUST mutex may return
	 * interesting error codes.
	 */
	if (error == 0) {
		error2 = _mutex_cv_lock(mp, recurse);
	} else if (error == EINTR || error == ETIMEDOUT) {
		error2 = _mutex_cv_lock(mp, recurse);
		if (error2 == 0 && cancel)
			_thr_testcancel(curthread);
		if (error2 == EINTR)
			error = 0;
	} else {
		/* We know that it didn't unlock the mutex. */
		error2 = _mutex_cv_attach(mp, recurse);
		if (error2 == 0 && cancel)
			_thr_testcancel(curthread);
	}
	return (error2 != 0 ? error2 : error);
}

static int
cond_wait_user(struct pthread_cond *cvp, struct pthread_mutex *mp,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	int		recurse;
	int		error;
	uint64_t	seq, bseq;

	_thr_umtx_lock_spin(&cvp->__lock);
	if (cvp->__destroying) {
		_thr_umtx_unlock(&cvp->__lock);
		return (EINVAL);
	}
	cvp->__waiters++;
	error = _mutex_cv_unlock(mp, &recurse);
	if (__predict_false(error != 0)) {
		cvp->__waiters--;
		_thr_umtx_unlock(&cvp->__lock);
		return (error);
	}

	bseq = cvp->__broadcast_seq;
	cvp->__refcount++;
	for(;;) {
		seq = cvp->__seq;
		_thr_umtx_unlock(&cvp->__lock);

		if (abstime != NULL) {
			clock_gettime(cvp->__clock_id, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			tsp = &ts2;
		} else
			tsp = NULL;

		if (cancel) {
			_thr_cancel_enter2(curthread, 0);
			error = _thr_umtx_wait_uint((u_int *)&cvp->__seq,
				(u_int)seq, tsp, CV_PSHARED(cvp));
			_thr_cancel_leave(curthread, 0);
		} else {
			error = _thr_umtx_wait_uint((u_int *)&cvp->__seq,
				(u_int)seq, tsp, CV_PSHARED(cvp));
		}

		_thr_umtx_lock_spin(&cvp->__lock);
		if (cvp->__broadcast_seq != bseq) {
			cvp->__refcount--;
			error = 0;
			break;
		}
		if (cvp->__signals > 0) {
			cvp->__refcount--;
			cvp->__signals--;
			error = 0;
			break;
		} else if (error == ETIMEDOUT) {
			cvp->__refcount--;
			cvp->__waiters--;
			break;
		} else if (cancel && SHOULD_CANCEL(curthread) &&
			   !THR_IN_CRITICAL(curthread)) {
			cvp->__waiters--;
			cvp->__refcount--;
			if (cvp->__destroying && cvp->__refcount == 0) {
				cvp->__destroying = 2;
				_thr_umtx_wake(&cvp->__destroying, INT_MAX, CV_PSHARED(cvp));
			}
			_thr_umtx_unlock(&cvp->__lock);
			_mutex_cv_lock(mp, recurse);
			_pthread_exit(PTHREAD_CANCELED);
		}
	}
	if (cvp->__destroying && cvp->__refcount == 0) {
		cvp->__destroying = 2;
		_thr_umtx_wake(&cvp->__destroying, INT_MAX, CV_PSHARED(cvp));
	}
	_thr_umtx_unlock(&cvp->__lock);
	_mutex_cv_lock(mp, recurse);
	return (error);
}

static int
cond_wait_common(struct pthread_cond *cvp, struct pthread_mutex *mp,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	int error;

	if ((error = _mutex_owned(curthread, mp)) != 0)
		return (error);

	if ((mp->__lockflags & USYNC_PROCESS_SHARED) !=
	    (cvp->__flags & USYNC_PROCESS_SHARED))
		return (EINVAL);

	/*
	 * If the thread is real-time thread or if it holds priority mutex,
	 * it should use kernel based cv, because the cv internal lock
	 * does not protect priority, it can cause priority inversion.
	 * Note that if it is robust type of mutex, we should not use
	 * the internal lock too, because it is not robust.
	 */
	if (curthread->attr.sched_policy != SCHED_OTHER ||
	    curthread->priority_mutex_count != 0  ||
	    (mp->__lockflags & (UMUTEX_PRIO_PROTECT|UMUTEX_PRIO_INHERIT|
		UMUTEX_ROBUST)) != 0)
		return cond_wait_kernel(cvp, mp, abstime, cancel);
	else
		return cond_wait_user(cvp, mp, abstime, cancel);
}

int
_pthread_cond_wait(pthread_cond_t *cvp, pthread_mutex_t *mp)
{

	return (cond_wait_common(cvp, mp, NULL, 0));
}

int
__pthread_cond_wait(pthread_cond_t *cvp, pthread_mutex_t *mp)
{

	return (cond_wait_common(cvp, mp, NULL, 1));
}

int
_pthread_cond_timedwait(pthread_cond_t *cvp, pthread_mutex_t *mp,
		       const struct timespec * abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cvp, mp, abstime, 0));
}

int
__pthread_cond_timedwait(pthread_cond_t *cvp, pthread_mutex_t *mp,
		       const struct timespec * abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cvp, mp, abstime, 1));
}

static int
cond_signal_common(struct pthread_cond *cvp)
{

	_thr_ucond_signal((struct ucond *)&cvp->__kern_has_waiters);

	if (cvp->__waiters == 0)
		return (0);

	_thr_umtx_lock_spin(&cvp->__lock);
	if (cvp->__waiters > 0) {
		cvp->__seq++;
		cvp->__signals++;
		cvp->__waiters--;
		_thr_umtx_wake(&cvp->__seq, 1, CV_PSHARED(cvp));
	}
	_thr_umtx_unlock(&cvp->__lock);
	return (0);
}

static int
cond_broadcast_common(struct pthread_cond *cvp)
{

	_thr_ucond_broadcast((struct ucond *)&cvp->__kern_has_waiters);

	if (cvp->__waiters == 0)
		return (0);

	_thr_umtx_lock_spin(&cvp->__lock);
	if (cvp->__waiters > 0) {
		cvp->__seq++;
		cvp->__broadcast_seq++;
		cvp->__waiters = 0;
		cvp->__signals = 0;
		_thr_umtx_wake(&cvp->__seq, INT_MAX, CV_PSHARED(cvp));
	}
	_thr_umtx_unlock(&cvp->__lock);
	return (0);
}

int
_pthread_cond_signal(pthread_cond_t *cvp)
{
	return (cond_signal_common(cvp));
}

int
_pthread_cond_broadcast(pthread_cond_t *cvp)
{
	return (cond_broadcast_common(cvp));
}

#define CHECK_AND_INIT_COND							\
	if (__predict_false((cvp = (*cond)) <= THR_COND_DESTROYED)) {		\
		if (cvp == THR_COND_INITIALIZER) {				\
			int error;						\
			error = init_static(_get_curthread(), cond);		\
			if (error)						\
				return (error);					\
		} else if (cvp == THR_COND_DESTROYED) {				\
			return (EINVAL);					\
		}								\
		cvp = *cond;							\
	}

static int
cond_init_old(pthread_cond_old_t *cond, const pthread_condattr_t *cond_attr)
{
	struct pthread_cond	*cvp = NULL;
	int	error = 0;

	if ((cvp = (struct pthread_cond *)
	    calloc(1, sizeof(struct pthread_cond))) == NULL) {
		error = ENOMEM;
	} else {
		error = cond_init(cvp, cond_attr);
		if (error != 0)
			free(cvp);
		else
			*cond = cvp;
	}
	return (error);
}

static int
init_static(struct pthread *thread, pthread_cond_old_t *cond)
{
	int error;

	THR_LOCK_ACQUIRE(thread, &_cond_static_lock);

	if (*cond == NULL)
		error = cond_init_old(cond, NULL);
	else
		error = 0;

	THR_LOCK_RELEASE(thread, &_cond_static_lock);

	return (error);
}

int
_pthread_cond_init_1_0(pthread_cond_old_t *cond, const pthread_condattr_t *cond_attr)
{

	*cond = NULL;
	return (cond_init_old(cond, cond_attr));
}

int
_pthread_cond_destroy_1_0(pthread_cond_old_t *cond)
{
	struct pthread_cond *cvp;
	int error = 0;

	if ((cvp = *cond) == THR_COND_INITIALIZER)
		error = 0;
	else if (cvp == THR_COND_DESTROYED)
		error = EINVAL;
	else {
		cvp = *cond;
		error = cond_destroy_common(cvp);
		if (error)
			return (error);
		*cond = THR_COND_DESTROYED;
		free(cvp);
	}
	return (error);
}

int
_pthread_cond_signal_1_0(pthread_cond_old_t *cond)
{
	pthread_cond_t	*cvp;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	return (cond_signal_common(cvp));
}

int
_pthread_cond_broadcast_1_0(pthread_cond_old_t *cond)
{
	pthread_cond_t	*cvp;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	return (cond_broadcast_common(cvp));
}

int
_pthread_cond_wait_1_0(pthread_cond_old_t *cond, pthread_mutex_old_t *mutex)
{
	pthread_cond_t	*cvp;
	int error;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	if ((error = _mutex_owned_old(_get_curthread(), mutex)) != 0)
		return (error);

	return (cond_wait_common(cvp, *mutex, NULL, 1));
}

int
_pthread_cond_timedwait_1_0(pthread_cond_old_t *cond, pthread_mutex_old_t *mutex,
	       const struct timespec * abstime)
{
	pthread_cond_t	*cvp;
	int error;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	if ((error = _mutex_owned_old(_get_curthread(), mutex)) != 0)
		return (error);

	return (cond_wait_common(cvp, *mutex, abstime, 1));
}

FB10_COMPAT(_pthread_cond_destroy_1_0, pthread_cond_destroy);
FB10_COMPAT(_pthread_cond_init_1_0, pthread_cond_init);
FB10_COMPAT(_pthread_cond_wait_1_0, pthread_cond_wait);
FB10_COMPAT(_pthread_cond_timedwait_1_0, pthread_cond_timedwait);
FB10_COMPAT(_pthread_cond_signal_1_0, pthread_cond_signal);
FB10_COMPAT(_pthread_cond_broadcast_1_0, pthread_cond_broadcast);
