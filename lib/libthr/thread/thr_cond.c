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
static int cond_signal_common(pthread_cond_t *cond);
static int cond_broadcast_common(pthread_cond_t *cond);

#define CV_PSHARED(cv)	(((cv)->c_kerncv.c_flags & USYNC_PROCESS_SHARED) != 0)

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
			pcond->c_clockid = CLOCK_REALTIME;
		} else {
			if ((*cond_attr)->c_pshared)
				pcond->c_kerncv.c_flags |= USYNC_PROCESS_SHARED;
			pcond->c_clockid = (*cond_attr)->c_clockid;
		}
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
		if (cv->c_mutex != NULL)
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

struct cond_cancel_info
{
	pthread_mutex_t	*mutex;
	pthread_cond_t	*cond;
	int		recurse;
};

static void
cond_cancel_handler(void *arg)
{
	struct cond_cancel_info *info = (struct cond_cancel_info *)arg;
  
	_mutex_cv_lock(info->mutex, info->recurse, 1);
}

/*
 * Wait on kernel based condition variable.
 */
static int
cond_wait_kernel(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	struct pthread_mutex *m;
	struct cond_cancel_info info;
	pthread_cond_t  cv;
	int		error, error2;

	cv = *cond;
	m = *mutex;
	error = _mutex_cv_detach(mutex, &info.recurse);
	if (__predict_false(error != 0))
		return (error);

	info.mutex = mutex;
	info.cond  = cond;

	if (abstime != NULL) {
		clock_gettime(cv->c_clockid, &ts);
		TIMESPEC_SUB(&ts2, abstime, &ts);
		tsp = &ts2;
	} else
		tsp = NULL;

	if (cancel) {
		THR_CLEANUP_PUSH(curthread, cond_cancel_handler, &info);
		_thr_cancel_enter2(curthread, 0);
		error = _thr_ucond_wait(&cv->c_kerncv, &m->m_lock, tsp, 1);
		info.cond = NULL;
		_thr_cancel_leave(curthread, (error != 0));
		THR_CLEANUP_POP(curthread, 0);
	} else {
		error = _thr_ucond_wait(&cv->c_kerncv, &m->m_lock, tsp, 0);
	}
	if (error == EINTR)
		error = 0;
	error2 = _mutex_cv_lock(mutex, info.recurse, 1);
	return (error || error2);
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
cond_wait_queue(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread_mutex *m;
	struct sleepqueue *sq;
	pthread_cond_t	cv;
	int		recurse;
	int		error;

	cv = *cond;
	/*
	 * Enqueue thread before unlocking mutex, so we can avoid
	 * sleep lock in pthread_cond_signal whenever possible.
	 */
	if ((error = _mutex_owned(curthread, mutex)) != 0)
		return (error);
	sq = _sleepq_lock(cv, CV);
	if (cv->c_mutex != NULL && cv->c_mutex != mutex) {
		_sleepq_unlock(sq);
		return (EINVAL);
	}
	cv->c_mutex = mutex;
	_sleepq_add(sq, curthread);
	_thr_clear_wake(curthread);
	_sleepq_unlock(sq);
	(void)_mutex_cv_unlock(mutex, &recurse);
	m = *mutex;
	for (;;) {
		if (cancel) {
			_thr_cancel_enter2(curthread, 0);
			error = _thr_sleep(curthread, abstime, cv->c_clockid);
			_thr_cancel_leave(curthread, 0);
		} else {
			error = _thr_sleep(curthread, abstime, cv->c_clockid);
		}
		_thr_clear_wake(curthread);

		sq = _sleepq_lock(cv, CV);
		if (curthread->wchan == NULL) {
			/*
			 * This must be signaled by mutex unlocking,
			 * they remove us from mutex queue.
			 */
			_sleepq_unlock(sq);
			error = 0;
			break;
		} if (curthread->wchan == m) {
			_sleepq_unlock(sq);
			/*
			 * This must be signaled by cond_signal and there
			 * is no owner for the mutex.
			 */
			sq = _sleepq_lock(m, MX);
			if (curthread->wchan == m)
				_sleepq_remove(sq, curthread);
			_sleepq_unlock(sq);
			error = 0;
			break;
		} if (abstime != NULL && error == ETIMEDOUT) {
			_sleepq_remove(sq, curthread);
			if (_sleepq_empty(sq))
				cv->c_mutex = NULL;
			_sleepq_unlock(sq);
			break;
		} else if (SHOULD_CANCEL(curthread)) {
			_sleepq_remove(sq, curthread);
			if (_sleepq_empty(sq))
				cv->c_mutex = NULL;
			_sleepq_unlock(sq);
			(void)_mutex_cv_lock(mutex, recurse, 0);
			_pthread_exit(PTHREAD_CANCELED);
		}
		_sleepq_unlock(sq);
	}
	_mutex_cv_lock(mutex, recurse, 0);
	return (error);
}

static int
cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	pthread_cond_t	cv;
	struct pthread_mutex	*m;

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	CHECK_AND_INIT_COND
	if ((m = *mutex) == NULL || m < THR_MUTEX_DESTROYED)
		return (EINVAL);
	if (IS_SIMPLE_MUTEX(m)) {
		if (!CV_PSHARED(cv))
			return cond_wait_queue(cond, mutex, abstime, cancel);
		else
			return (EINVAL);
	} else {
		return cond_wait_kernel(cond, mutex, abstime, cancel);
	}
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
cond_signal_common(pthread_cond_t *cond)
{
	pthread_mutex_t *mutex;
	struct pthread_mutex *m;
	struct pthread  *td;
	struct pthread_cond *cv;
	struct sleepqueue *cv_sq, *mx_sq;
	unsigned	*waddr = NULL;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	_thr_ucond_signal(&cv->c_kerncv);

	if (CV_PSHARED(cv))
		return (0);

	/* There is no waiter. */
	if (cv->c_mutex == NULL)
		return (0);

	cv_sq = _sleepq_lock(cv, CV);
	if (_sleepq_empty(cv_sq)) {
		_sleepq_unlock(cv_sq);
		return (0);
	} 
	/*
	 * Check if we owned the temporarily binding mutex,
	 * if owned, we can migrate thread to mutex wait
	 * queue without waking up thread.
	 */
	if ((mutex = cv->c_mutex) != NULL)
		m = *mutex;
	else {
		_sleepq_unlock(cv_sq);
		PANIC("mutex == NULL");
	}

	td = _sleepq_first(cv_sq);
	if (m->m_owner == NULL)
		waddr = WAKE_ADDR(td);
	_sleepq_remove(cv_sq, td);
	mx_sq = _sleepq_lock(m, MX);
	_sleepq_add(mx_sq, td);
	_mutex_set_contested(m);
	_sleepq_unlock(mx_sq);
	if (_sleepq_empty(cv_sq))
		cv->c_mutex = NULL;
	_sleepq_unlock(cv_sq);
	if (waddr != NULL) {
		_thr_set_wake(waddr);
		_thr_umtx_wake(waddr, INT_MAX, 0);
	}
	return (0);
}

static int
cond_broadcast_common(pthread_cond_t *cond)
{
	pthread_mutex_t *mutex;
	struct pthread_mutex *m;
	struct pthread  *td;
	struct pthread_cond *cv;
	struct sleepqueue *cv_sq, *mx_sq;
	unsigned	*waddr = NULL;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	_thr_ucond_broadcast(&cv->c_kerncv);

	if (CV_PSHARED(cv))
		return (0);

	/* There is no waiter. */
	if (cv->c_mutex == NULL)
		return (0);

	cv_sq = _sleepq_lock(cv, CV);
	if (_sleepq_empty(cv_sq)) {
		_sleepq_unlock(cv_sq);
		return (0);
	} 
	/*
	 * Check if we owned the temporarily binding mutex,
	 * if owned, we can migrate thread to mutex wait
	 * queue without waking up thread.
	 */
	if ((mutex = cv->c_mutex) != NULL)
		m = *mutex;
	else {
		_sleepq_unlock(cv_sq);
		PANIC("mutex == NULL");
	}

	td = _sleepq_first(cv_sq);
	if (m->m_owner == NULL)
		waddr = WAKE_ADDR(td);
	mx_sq = _sleepq_lock(m, MX);
	_sleepq_concat(mx_sq, cv_sq);
	_mutex_set_contested(m);
	_sleepq_unlock(mx_sq);
	cv->c_mutex = NULL;
	_sleepq_unlock(cv_sq);
	if (waddr != NULL) {
		_thr_set_wake(waddr);
		_thr_umtx_wake(waddr, INT_MAX, 0);
	}
	return (0);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{

	return (cond_signal_common(cond));
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{

	return (cond_broadcast_common(cond));
}
