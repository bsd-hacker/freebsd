/*-
 * Copyright (c) 1998 Alex Nash
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "namespace.h"
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

__weak_reference(_pthread_rwlock_destroy, pthread_rwlock_destroy);
__weak_reference(_pthread_rwlock_init, pthread_rwlock_init);
__weak_reference(_pthread_rwlock_rdlock, pthread_rwlock_rdlock);
__weak_reference(_pthread_rwlock_timedrdlock, pthread_rwlock_timedrdlock);
__weak_reference(_pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
__weak_reference(_pthread_rwlock_trywrlock, pthread_rwlock_trywrlock);
__weak_reference(_pthread_rwlock_unlock, pthread_rwlock_unlock);
__weak_reference(_pthread_rwlock_wrlock, pthread_rwlock_wrlock);
__weak_reference(_pthread_rwlock_timedwrlock, pthread_rwlock_timedwrlock);

typedef struct pthread_rwlock *pthread_rwlock_old_t;

int _pthread_rwlock_destroy_1_0(pthread_rwlock_old_t *);
int _pthread_rwlock_init_1_0(pthread_rwlock_old_t *,
	const pthread_rwlockattr_t *);
int _pthread_rwlock_timedrdlock_1_0(pthread_rwlock_old_t *,
	const struct timespec *);
int _pthread_rwlock_tryrdlock_1_0(pthread_rwlock_old_t *);
int _pthread_rwlock_trywrlock_1_0(pthread_rwlock_old_t *);
int _pthread_rwlock_rdlock_1_0(pthread_rwlock_old_t *, const struct timespec *);
int _pthread_rwlock_unlock_1_0(pthread_rwlock_old_t *);

#define RWL_PSHARED(rwp)	((rwp->__flags & USYNC_PROCESS_SHARED) != 0)

/*
 * Prototypes
 */

static int
rwlock_init(struct pthread_rwlock *rwp, const pthread_rwlockattr_t *attr)
{

	memset(rwp, 0, sizeof(*rwp));
	if (attr == NULL || *attr == NULL)
		return (0);
	else {
		if ((*attr)->pshared)
			rwp->__flags |= USYNC_PROCESS_SHARED;
	}

	return (0);
}

static int
rwlock_destroy_common(struct pthread_rwlock *rwp)
{
	if (rwp->__state != 0)
		return (EBUSY);
	return (0);
}

int
_pthread_rwlock_destroy (pthread_rwlock_t *rwp)
{
	return rwlock_destroy_common(rwp);
}

int
_pthread_rwlock_init(pthread_rwlock_t *rwp, const pthread_rwlockattr_t *attr)
{
	return (rwlock_init(rwp, attr));
}

static int
rwlock_rdlock_common(struct pthread_rwlock *rwlp, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	int flags;
	int error;

	if (curthread->rdlock_count) {
		/*
		 * To avoid having to track all the rdlocks held by
		 * a thread or all of the threads that hold a rdlock,
		 * we keep a simple count of all the rdlocks held by
		 * a thread.  If a thread holds any rdlocks it is
		 * possible that it is attempting to take a recursive
		 * rdlock.  If there are blocked writers and precedence
		 * is given to them, then that would result in the thread
		 * deadlocking.  So allowing a thread to take the rdlock
		 * when it already has one or more rdlocks avoids the
		 * deadlock.  I hope the reader can follow that logic ;-)
		 */
		flags = URWLOCK_PREFER_READER;
	} else {
		flags = 0;
	}

	/*
	 * POSIX said the validity of the abstime parameter need
	 * not be checked if the lock can be immediately acquired.
	 */
	error = _thr_rwlock_tryrdlock((struct urwlock *)&rwlp->__state, flags);
	if (error == 0) {
		curthread->rdlock_count++;
		return (error);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	for (;;) {
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			if (ts2.tv_sec < 0 || 
			    (ts2.tv_sec == 0 && ts2.tv_nsec <= 0))
				return (ETIMEDOUT);
			tsp = &ts2;
		} else
			tsp = NULL;

		/* goto kernel and lock it */
		error = __thr_rwlock_rdlock((struct urwlock *)&rwlp->__state, flags, tsp);
		if (error != EINTR)
			break;

		/* if interrupted, try to lock it in userland again. */
		if (_thr_rwlock_tryrdlock((struct urwlock *)&rwlp->__state, flags) == 0) {
			error = 0;
			break;
		}
	}
	if (error == 0)
		curthread->rdlock_count++;
	return (error);
}

int
_pthread_rwlock_rdlock (pthread_rwlock_t *rwlp)
{
	return (rwlock_rdlock_common(rwlp, NULL));
}

int
_pthread_rwlock_timedrdlock (pthread_rwlock_t *rwlp,
	const struct timespec *abstime)
{
	return (rwlock_rdlock_common(rwlp, abstime));
}

int
_pthread_rwlock_tryrdlock (pthread_rwlock_t *rwlp)
{
	struct pthread *curthread = _get_curthread();
	int flags;
	int error;

	if (curthread->rdlock_count) {
		/*
		 * To avoid having to track all the rdlocks held by
		 * a thread or all of the threads that hold a rdlock,
		 * we keep a simple count of all the rdlocks held by
		 * a thread.  If a thread holds any rdlocks it is
		 * possible that it is attempting to take a recursive
		 * rdlock.  If there are blocked writers and precedence
		 * is given to them, then that would result in the thread
		 * deadlocking.  So allowing a thread to take the rdlock
		 * when it already has one or more rdlocks avoids the
		 * deadlock.  I hope the reader can follow that logic ;-)
		 */
		flags = URWLOCK_PREFER_READER;
	} else {
		flags = 0;
	}

	error = _thr_rwlock_tryrdlock((struct urwlock *)&rwlp->__state, flags);
	if (error == 0)
		curthread->rdlock_count++;
	return (error);
}

static void
rwlock_setowner(struct pthread_rwlock *rwlp, struct pthread *td)
{
	if (!RWL_PSHARED(rwlp))
		rwlp->__owner.__ownertd = td;
	else
		rwlp->__owner.__ownertid = TID(td);
}

int
_pthread_rwlock_trywrlock (pthread_rwlock_t *rwlp)
{
	struct pthread *curthread = _get_curthread();
	int error;

	error = _thr_rwlock_trywrlock((struct urwlock *)&rwlp->__state);
	if (error == 0)
		rwlock_setowner(rwlp, curthread);
	return (error);
}

static int
rwlock_wrlock_common(pthread_rwlock_t *rwlp, const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	int error;

	/*
	 * POSIX said the validity of the abstime parameter need
	 * not be checked if the lock can be immediately acquired.
	 */
	error = _thr_rwlock_trywrlock((struct urwlock *)&rwlp->__state);
	if (error == 0) {
		rwlock_setowner(rwlp, curthread);
		return (error);
	}

	if (__predict_false(abstime && 
		(abstime->tv_nsec >= 1000000000 || abstime->tv_nsec < 0)))
		return (EINVAL);

	for (;;) {
		if (abstime != NULL) {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			if (ts2.tv_sec < 0 || 
			    (ts2.tv_sec == 0 && ts2.tv_nsec <= 0))
				return (ETIMEDOUT);
			tsp = &ts2;
		} else
			tsp = NULL;

		/* goto kernel and lock it */
		error = __thr_rwlock_wrlock((struct urwlock *)&rwlp->__state, tsp);
		if (error == 0) {
			rwlock_setowner(rwlp, curthread);
			break;
		}

		if (error != EINTR)
			break;

		/* if interrupted, try to lock it in userland again. */
		if (_thr_rwlock_trywrlock((struct urwlock *)&rwlp->__state) == 0) {
			error = 0;
			rwlock_setowner(rwlp, curthread);
			break;
		}
	}
	return (error);
}

int
_pthread_rwlock_wrlock (pthread_rwlock_t *rwlp)
{
	return (rwlock_wrlock_common(rwlp, NULL));
}

int
_pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlp,
	const struct timespec *abstime)
{
	return (rwlock_wrlock_common(rwlp, abstime));
}

int
_pthread_rwlock_unlock(pthread_rwlock_t *rwlp)
{
	struct pthread *curthread = _get_curthread();
	int error;
	uint32_t state;

	state = rwlp->__state;
	if (state & URWLOCK_WRITE_OWNER) {
		if (RWL_PSHARED(rwlp) &&
		    rwlp->__owner.__ownertid == TID(curthread)) {
			rwlp->__owner.__ownertid = 0;
		} else if (!RWL_PSHARED(rwlp) &&
		         rwlp->__owner.__ownertd == curthread) {
			rwlp->__owner.__ownertd = NULL;
		} else
			return (EPERM);
	}
	error = _thr_rwlock_unlock((struct urwlock *)&rwlp->__state);
	if (error == 0 && (state & URWLOCK_WRITE_OWNER) == 0)
		curthread->rdlock_count--;
	return (error);
}

#define CHECK_AND_INIT_RWLOCK							\
	if (__predict_false((rwlp = (*rwlpp)) <= THR_RWLOCK_DESTROYED)) {	\
		if (rwlp == THR_RWLOCK_INITIALIZER) {				\
			int error;						\
			error = init_static(_get_curthread(), rwlpp);		\
			if (error)						\
				return (error);					\
		} else if (rwlp == THR_RWLOCK_DESTROYED) {			\
			return (EINVAL);					\
		}								\
		*rwlpp = rwlp;							\
	}

static int
rwlock_init_old(pthread_rwlock_old_t *rwlpp, const pthread_rwlockattr_t *attr)
{
	struct pthread_rwlock *rwlp;
	int error;

	rwlp = (struct pthread_rwlock *)malloc(sizeof(struct pthread_rwlock));
	if (rwlp == NULL)
		return (ENOMEM);
	error = rwlock_init(rwlp, attr);
	if (error) {
		free(rwlp);
		return (error);
	}
	*rwlpp = rwlp;
	return (0);
}

static int
init_static(struct pthread *thread, pthread_rwlock_old_t *rwlpp)
{
	int	error;

	THR_LOCK_ACQUIRE(thread, &_rwlock_static_lock);

	if (*rwlpp == THR_RWLOCK_INITIALIZER)
		error = rwlock_init_old(rwlpp, NULL);
	else
		error = 0;

	THR_LOCK_RELEASE(thread, &_rwlock_static_lock);

	return (error);
}

int
_pthread_rwlock_destroy_1_0(pthread_rwlock_old_t *rwlpp)
{
	struct pthread_rwlock	*rwlp;
	int	error;

	rwlp = *rwlpp;
	if (rwlp == THR_RWLOCK_INITIALIZER)
		error = 0;
	else if (rwlp == THR_RWLOCK_DESTROYED)
		error = EINVAL;
	else {
		error = rwlock_destroy_common(rwlp);
		if (error)
			return (error);
		*rwlpp = THR_RWLOCK_DESTROYED;
		free(rwlp);
	}
	return (error);
}

int
_pthread_rwlock_init_1_0(pthread_rwlock_old_t *rwlpp, const pthread_rwlockattr_t *attr)
{
	*rwlpp = NULL;
	return (rwlock_init_old(rwlpp, attr));
}

int
_pthread_rwlock_timedrdlock_1_0(pthread_rwlock_old_t *rwlpp,
	 const struct timespec *abstime)
{
	struct pthread_rwlock *rwlp;

	CHECK_AND_INIT_RWLOCK
	
	return (rwlock_rdlock_common(rwlp, abstime));
}

int
_pthread_rwlock_tryrdlock_1_0(pthread_rwlock_old_t *rwlpp)
{
	struct pthread_rwlock *rwlp;

	CHECK_AND_INIT_RWLOCK
	
	return _pthread_rwlock_tryrdlock(rwlp);
}

int
_pthread_rwlock_trywrlock_1_0(pthread_rwlock_old_t *rwlpp)
{
	struct pthread_rwlock *rwlp;

	CHECK_AND_INIT_RWLOCK
	
	return _pthread_rwlock_trywrlock(rwlp);
}

int
_pthread_rwlock_rdlock_1_0(pthread_rwlock_old_t *rwlpp, const struct timespec *abstime)
{
	struct pthread_rwlock *rwlp;

	CHECK_AND_INIT_RWLOCK
	
	return rwlock_rdlock_common(rwlp, abstime);
}

int
_pthread_rwlock_unlock_1_0(pthread_rwlock_old_t *rwlpp)
{
	struct pthread_rwlock *rwlp;

	rwlp = *rwlpp;
	if (__predict_false(rwlp <= THR_RWLOCK_DESTROYED))
		return (EINVAL);
	return _pthread_rwlock_unlock(rwlp);
}

FB10_COMPAT(_pthread_rwlock_destroy_1_0, pthread_rwlock_destroy);
FB10_COMPAT(_pthread_rwlock_init_1_0, pthread_rwlock_init);
FB10_COMPAT(_pthread_rwlock_rdlock_1_0, pthread_rwlock_rdlock);
FB10_COMPAT(_pthread_rwlock_timedrdlock_1_0, pthread_rwlock_timedrdlock);
FB10_COMPAT(_pthread_rwlock_tryrdlock_1_0, pthread_rwlock_tryrdlock);
FB10_COMPAT(_pthread_rwlock_trywrlock_1_0, pthread_rwlock_trywrlock);
FB10_COMPAT(_pthread_rwlock_unlock_1_0, pthread_rwlock_unlock);
FB10_COMPAT(_pthread_rwlock_wrlock_1_0, pthread_rwlock_wrlock);
FB10_COMPAT(_pthread_rwlock_timedwrlock_1_0, pthread_rwlock_timedwrlock);
