/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
 * Copyright (c) 2006 David Xu <davidxu@freebsd.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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

#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sys/mman.h>
#include "un-namespace.h"

#include "thr_private.h"

#define CHECK_AND_INIT_MUTEX						\
	if (__predict_false((m = *mutex) <= THR_MUTEX_DESTROYED)) {	\
		if (m == THR_MUTEX_DESTROYED)				\
			return (EINVAL);				\
		int error;						\
		error = init_static(_get_curthread(), mutex);		\
		if (error)						\
			return (error);					\
		m = *mutex;						\
	}

/*
 * For adaptive mutexes, how many times to spin doing trylock2
 * before entering the kernel to block
 */
#define MUTEX_ADAPTIVE_SPINS	2000

/*
 * Prototypes
 */
int	__pthread_mutex_init(pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutex_attr);
int	__pthread_mutex_trylock(pthread_mutex_t *mutex);
int	__pthread_mutex_lock(pthread_mutex_t *mutex);
int	__pthread_mutex_timedlock(pthread_mutex_t *mutex,
		const struct timespec *abstime);
int	_pthread_mutex_init_calloc_cb(pthread_mutex_t *mutex,
    		void *(calloc_cb)(size_t, size_t));
int	_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count);
int	_pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count);
int	__pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count);
int	__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);

static int	mutex_self_trylock(pthread_mutex_t);
static int	mutex_self_lock(pthread_mutex_t,
				const struct timespec *abstime);
static int	mutex_unlock_common(pthread_mutex_t *);
static int	mutex_lock_sleep(struct pthread *, pthread_mutex_t,
				const struct timespec *);
static void	enqueue_mutex(struct pthread *, struct pthread_mutex *);
static void	dequeue_mutex(struct pthread *, struct pthread_mutex *);

__weak_reference(__pthread_mutex_init, pthread_mutex_init);
__strong_reference(__pthread_mutex_init, _pthread_mutex_init);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);
__strong_reference(__pthread_mutex_lock, _pthread_mutex_lock);
__weak_reference(__pthread_mutex_timedlock, pthread_mutex_timedlock);
__strong_reference(__pthread_mutex_timedlock, _pthread_mutex_timedlock);
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);
__strong_reference(__pthread_mutex_trylock, _pthread_mutex_trylock);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);

__weak_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__weak_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);

__weak_reference(__pthread_mutex_setspinloops_np, pthread_mutex_setspinloops_np);
__strong_reference(__pthread_mutex_setspinloops_np, _pthread_mutex_setspinloops_np);
__weak_reference(_pthread_mutex_getspinloops_np, pthread_mutex_getspinloops_np);

__weak_reference(__pthread_mutex_setyieldloops_np, pthread_mutex_setyieldloops_np);
__strong_reference(__pthread_mutex_setyieldloops_np, _pthread_mutex_setyieldloops_np);
__weak_reference(_pthread_mutex_getyieldloops_np, pthread_mutex_getyieldloops_np);
__weak_reference(_pthread_mutex_isowned_np, pthread_mutex_isowned_np);

static int
mutex_init(pthread_mutex_t *mutex,
    const struct pthread_mutex_attr *mutex_attr,
    void *(calloc_cb)(size_t, size_t))
{
	const struct pthread_mutex_attr *attr;
	struct pthread_mutex *pmutex;

	if (mutex_attr == NULL) {
		attr = &_pthread_mutexattr_default;
	} else {
		attr = mutex_attr;
		if (attr->m_type < PTHREAD_MUTEX_ERRORCHECK ||
		    attr->m_type >= PTHREAD_MUTEX_TYPE_MAX)
			return (EINVAL);
		if (attr->m_protocol < PTHREAD_PRIO_NONE ||
		    attr->m_protocol > PTHREAD_PRIO_PROTECT)
			return (EINVAL);
	}
	if ((pmutex = (pthread_mutex_t)
		calloc_cb(1, sizeof(struct pthread_mutex))) == NULL)
		return (ENOMEM);

	pmutex->m_type = attr->m_type;
	pmutex->m_ownertd = NULL;
	pmutex->m_count = 0;
	pmutex->m_refcount = 0;
	pmutex->m_spinloops = 0;
	pmutex->m_yieldloops = 0;
	switch(attr->m_protocol) {
	case PTHREAD_PRIO_NONE:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		if (attr->m_pshared == 0)
			pmutex->m_lock.m_flags |= UMUTEX_SIMPLE;
		break;
	case PTHREAD_PRIO_INHERIT:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_INHERIT;
		break;
	case PTHREAD_PRIO_PROTECT:
		pmutex->m_lock.m_owner = UMUTEX_CONTESTED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_PROTECT;
		if (attr->m_pshared == 0)
			pmutex->m_lock.m_flags |= UMUTEX_SIMPLE;
		pmutex->m_lock.m_ceilings[0] = attr->m_ceiling;
		break;
	}
	if (attr->m_pshared != 0)
		pmutex->m_lock.m_flags |= USYNC_PROCESS_SHARED;
	if (pmutex->m_type == PTHREAD_MUTEX_ADAPTIVE_NP) {
		pmutex->m_spinloops =
		    _thr_spinloops ? _thr_spinloops: MUTEX_ADAPTIVE_SPINS;
		pmutex->m_yieldloops = _thr_yieldloops;
	}

	*mutex = pmutex;
	return (0);
}

static int
init_static(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == THR_MUTEX_INITIALIZER)
		ret = mutex_init(mutex, &_pthread_mutexattr_default, calloc);
	else if (*mutex == THR_ADAPTIVE_MUTEX_INITIALIZER)
		ret = mutex_init(mutex, &_pthread_mutexattr_adaptive_default, calloc);
	else
		ret = 0;
	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}


int
__pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr ? *mutex_attr : NULL, calloc);
}

/* This function is used internally by malloc. */
int
_pthread_mutex_init_calloc_cb(pthread_mutex_t *mutex,
    void *(calloc_cb)(size_t, size_t))
{
	static const struct pthread_mutex_attr attr = {
		.m_type = PTHREAD_MUTEX_NORMAL,
		.m_protocol = PTHREAD_PRIO_NONE,
		.m_ceiling = 0
	};
	int ret;

	ret = mutex_init(mutex, &attr, calloc_cb);
	if (ret == 0)
		(*mutex)->m_private = 1;
	return (ret);
}

void
_mutex_fork(struct pthread *curthread)
{
	struct mutex_link *ml;

	/*
	 * Fix mutex ownership for child process. Only PI mutex need to
	 * be changed, because we still use TID as lock-word.
	 */
	TAILQ_FOREACH(ml, &curthread->pi_mutexq, qe)
		ml->mutexp->m_lock.m_owner = TID(curthread);
}

int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	pthread_mutex_t m;
	int ret;

	m = *mutex;
	if (m < THR_MUTEX_DESTROYED) {
		ret = 0;
	} else if (m == THR_MUTEX_DESTROYED) {
		ret = EINVAL;
	} else {
		if ((m->m_lock.m_owner & UMUTEX_OWNER_MASK) != 0 ||
		     m->m_refcount != 0) {
			ret = EBUSY;
		} else {
			*mutex = THR_MUTEX_DESTROYED;
			free(m);
			ret = 0;
		}
	}

	return (ret);
}

static int
mutex_trylock_common(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m = *mutex;
	uint32_t id;
	int ret;

	if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);
	if (m->m_private)
		THR_CRITICAL_ENTER(curthread);
	ret = _thr_umutex_trylock(&m->m_lock, id);
	if (__predict_true(ret == 0)) {
		enqueue_mutex(curthread, m);
	} else {
		if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0) {
			if (m->m_ownertd == curthread)
				ret = mutex_self_trylock(m);
		} else {
			if ((m->m_lock.m_owner & UMUTEX_OWNER_MASK) == id)
				ret = mutex_self_trylock(m);
		}
	}
	if (ret && m->m_private)
		THR_CRITICAL_LEAVE(curthread);
	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread_mutex *m;

	CHECK_AND_INIT_MUTEX

	return (mutex_trylock_common(mutex));
}

static int
mutex_lock_sleep(struct pthread *curthread, struct pthread_mutex *m,
	const struct timespec *abstime)
{
	uint32_t	id, owner;
	int	count;
	int	ret;


	if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0) {
		if (m->m_ownertd == curthread)
			return mutex_self_lock(m, abstime);
		id = UMUTEX_SIMPLE_OWNER;
	} else {
		id = TID(curthread);
		if ((m->m_lock.m_owner & UMUTEX_OWNER_MASK) == id)
			return mutex_self_lock(m, abstime);
	}
	/*
	 * For adaptive mutexes, spin for a bit in the expectation
	 * that if the application requests this mutex type then
	 * the lock is likely to be released quickly and it is
	 * faster than entering the kernel
	 */
	if (__predict_false(
		(m->m_lock.m_flags & 
		 (UMUTEX_PRIO_PROTECT | UMUTEX_PRIO_INHERIT)) != 0))
			goto sleep_in_kernel;

	if (!_thr_is_smp)
		goto yield_loop;

	count = m->m_spinloops;
	while (count--) {
		owner = m->m_lock.m_owner;
		if ((owner & UMUTEX_OWNER_MASK) == 0) {
			if (atomic_cmpset_acq_32(&m->m_lock.m_owner, owner, id|owner)) {
				ret = 0;
				goto done;
			}
		}
		CPU_SPINWAIT;
	}

yield_loop:
	count = m->m_yieldloops;
	while (count--) {
		_sched_yield();
		owner = m->m_lock.m_owner;
		if ((owner & ~UMUTEX_CONTESTED) == 0) {
			if (atomic_cmpset_acq_32(&m->m_lock.m_owner, owner, id|owner)) {
				ret = 0;
				goto done;
			}
		}
	}

sleep_in_kernel:
	if (abstime == NULL) {
		ret = __thr_umutex_lock(&m->m_lock, id);
	} else if (__predict_false(
		   abstime->tv_nsec < 0 ||
		   abstime->tv_nsec >= 1000000000)) {
		ret = EINVAL;
	} else {
		ret = __thr_umutex_timedlock(&m->m_lock, id, abstime);
	}
done:
	if (ret == 0)
		enqueue_mutex(curthread, m);

	return (ret);
}

static inline int
mutex_lock_common(struct pthread_mutex *m,
	const struct timespec *abstime, int cvattach)
{
	struct pthread *curthread  = _get_curthread();
	uint32_t id;
	int ret;

	if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);

	if (m->m_private && !cvattach)
		THR_CRITICAL_ENTER(curthread);
	if (_thr_umutex_trylock2(&m->m_lock, id) == 0) {
		enqueue_mutex(curthread, m);
		ret = 0;
	} else {
		ret = mutex_lock_sleep(curthread, m, abstime);
	}
	if (ret && m->m_private && !cvattach)
		THR_CRITICAL_LEAVE(curthread);
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
	struct pthread_mutex	*m;

	_thr_check_init();

	CHECK_AND_INIT_MUTEX

	return (mutex_lock_common(m, NULL, 0));
}

int
__pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
	struct pthread_mutex	*m;

	_thr_check_init();

	CHECK_AND_INIT_MUTEX

	return (mutex_lock_common(m, abstime, 0));
}

int
_pthread_mutex_unlock(pthread_mutex_t *m)
{
	return (mutex_unlock_common(m));
}

static int
mutex_self_trylock(struct pthread_mutex *m)
{
	int	ret;

	switch (m->m_type) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
		ret = EBUSY; 
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static int
mutex_self_lock(struct pthread_mutex *m, const struct timespec *abstime)
{
	struct timespec	ts1, ts2;
	int	ret;

	switch (m->m_type) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_ADAPTIVE_NP:
		if (abstime) {
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				ret = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				ret = ETIMEDOUT;
			}
		} else {
			/*
			 * POSIX specifies that mutexes should return
			 * EDEADLK if a recursive lock is detected.
			 */
			ret = EDEADLK; 
		}
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		ret = 0;
		if (abstime) {
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				ret = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				ret = ETIMEDOUT;
			}
		} else {
			ts1.tv_sec = 30;
			ts1.tv_nsec = 0;
			for (;;)
				__sys_nanosleep(&ts1, NULL);
		}
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

int
_mutex_owned(struct pthread *curthread, const pthread_mutex_t *mutex)
{
	struct pthread_mutex	*m;

	m = *mutex;
	if (__predict_false(m <= THR_MUTEX_DESTROYED)) {
		if (m == THR_MUTEX_DESTROYED)
			return (EINVAL);
		return (EPERM);
	}

	/*
	 * Check if the running thread is not the owner of the mutex.
	 */
	if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0) {
		if (__predict_false(m->m_ownertd != curthread))
			return (EPERM);
	} else {
		if ((m->m_lock.m_owner & UMUTEX_OWNER_MASK) != TID(curthread))
			return (EPERM);
	}
	return (0);
}

static int
mutex_unlock_common(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m;
	uint32_t id;
	int err;

	if ((err = _mutex_owned(curthread, mutex)) != 0)
		return (err);

	m = *mutex;

	if (__predict_false(
		m->m_type == PTHREAD_MUTEX_RECURSIVE &&
		m->m_count > 0)) {
		m->m_count--;
	} else {
		if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0)
			id = UMUTEX_SIMPLE_OWNER;
		else
			id = TID(curthread);
		dequeue_mutex(curthread, m);
		_thr_umutex_unlock(&m->m_lock, id);
	}
	if (m->m_private)
		THR_CRITICAL_LEAVE(curthread);
	return (0);
}

int
_mutex_cv_lock(pthread_mutex_t *mutex, int count)
{
	struct pthread_mutex	*m;
	int	ret;

	m = *mutex;
	ret = mutex_lock_common(m, NULL, 1);
	if (ret == 0) {
		m->m_refcount--;
		m->m_count += count;
	}
	return (ret);
}

int
_mutex_cv_unlock(pthread_mutex_t *mutex, int *count)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m;
	uint32_t id;
	int err;

	if ((err = _mutex_owned(curthread, mutex)) != 0)
		return (err);

	m = *mutex;

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*count = m->m_count;
	m->m_refcount++;
	m->m_count = 0;
	dequeue_mutex(curthread, m);
	if ((m->m_lock.m_flags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);
	_thr_umutex_unlock(&m->m_lock, id);

	if (m->m_private)
		THR_CRITICAL_LEAVE(curthread);
	return (0);
}

int
_mutex_cv_attach(pthread_mutex_t *mutex, int count)
{
	struct pthread *	curthread = _get_curthread();
	struct pthread_mutex	*m;
	int	ret;

	m = *mutex;
	enqueue_mutex(curthread, m);
	m->m_refcount--;
	m->m_count += count;
	return (ret);
}

int
_mutex_cv_detach(pthread_mutex_t *mutex, int *count)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m;
	int err;

	if ((err = _mutex_owned(curthread, mutex)) != 0)
		return (err);

	m = *mutex;

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*count = m->m_count;
	m->m_refcount++;
	m->m_count = 0;
	dequeue_mutex(curthread, m);
	return (0);
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t *mutex,
			      int *prioceiling)
{
	struct pthread_mutex *m;
	int ret;

	m = *mutex;
	if ((m <= THR_MUTEX_DESTROYED) ||
	    (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		ret = EINVAL;
	else {
		*prioceiling = m->m_lock.m_ceilings[0];
		ret = 0;
	}

	return (ret);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
			      int ceiling, int *old_ceiling)
{
	struct pthread *curthread = _get_curthread();
	struct mutex_link *ml, *ml1, *ml2;
	struct pthread_mutex *m;
	int ret;

	m = *mutex;
	if ((m <= THR_MUTEX_DESTROYED) ||
	    (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		return (EINVAL);

	ret = __thr_umutex_set_ceiling(&m->m_lock, ceiling, old_ceiling);
	if (ret != 0)
		return (ret);
	if (((m->m_lock.m_flags & UMUTEX_SIMPLE) && (m->m_ownertd == curthread)) || 
           (m->m_lock.m_owner & UMUTEX_OWNER_MASK) == TID(curthread)) {
		TAILQ_FOREACH(ml, &curthread->pp_mutexq, qe) {
			if (ml->mutexp == m)
				break;
		}
		if (ml == NULL) /* howto ? */
			return (0);
		ml1 = TAILQ_PREV(ml, mutex_link_list, qe);
		ml2 = TAILQ_NEXT(ml, qe);
		if ((ml1 != NULL && ml1->mutexp->m_lock.m_ceilings[0] > (u_int)ceiling) ||
		    (ml2 != NULL && ml2->mutexp->m_lock.m_ceilings[0] < (u_int)ceiling)) {
			TAILQ_REMOVE(&curthread->pp_mutexq, ml, qe);
			TAILQ_FOREACH(ml2, &curthread->pp_mutexq, qe) {
				if (ml2->mutexp->m_lock.m_ceilings[0] > (u_int)ceiling) {
					TAILQ_INSERT_BEFORE(ml2, ml, qe);
					return (0);
				}
			}
			TAILQ_INSERT_TAIL(&curthread->pp_mutexq, ml, qe);
		}
	}
	return (0);
}

int
_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count)
{
	struct pthread_mutex	*m;

	CHECK_AND_INIT_MUTEX

	*count = m->m_spinloops;
	return (0);
}

int
__pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread_mutex	*m;

	CHECK_AND_INIT_MUTEX

	m->m_spinloops = count;
	return (0);
}

int
_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count)
{
	struct pthread_mutex	*m;

	CHECK_AND_INIT_MUTEX

	*count = m->m_yieldloops;
	return (0);
}

int
__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread_mutex	*m;

	CHECK_AND_INIT_MUTEX

	m->m_yieldloops = count;
	return (0);
}

int
_pthread_mutex_isowned_np(pthread_mutex_t *mutex)
{
	return (_mutex_owned(_get_curthread(), mutex) == 0);
}

void
_thr_mutex_link_init(struct pthread *td)
{
	TAILQ_INIT(&td->mutex_link_freeq);
	TAILQ_INIT(&td->mutex_link_pages);
	TAILQ_INIT(&td->pi_mutexq);
	TAILQ_INIT(&td->pp_mutexq);
}

struct mutex_link *
_thr_mutex_link_alloc(void)
{
	struct pthread *curthread = _get_curthread();
	struct mutex_link *p;
	unsigned i;

	p = TAILQ_FIRST(&curthread->mutex_link_freeq);
	if (p == NULL) {
		struct mutex_link *pp = (struct mutex_link *)mmap(NULL,
			_thr_page_size, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0);
		for (i = 2; i < _thr_page_size/sizeof(struct mutex_link); ++i)
			TAILQ_INSERT_TAIL(&curthread->mutex_link_freeq, &pp[i], qe);
		pp[0].mutexp = (struct pthread_mutex *)pp; /* the page address */
		TAILQ_INSERT_HEAD(&curthread->mutex_link_pages, &pp[0], qe);
		p = &pp[1];
	}
	return (p);
}

void
_thr_mutex_link_free(struct mutex_link *ml)
{
	struct pthread *curthread = _get_curthread();

	TAILQ_INSERT_TAIL(&curthread->mutex_link_freeq, ml, qe);
}

void
_thr_mutex_link_exit(struct pthread *curthread)
{
	struct mutex_link *ml, *ml2;

	TAILQ_FOREACH_SAFE(ml, &curthread->mutex_link_pages, qe, ml2) {
		TAILQ_REMOVE(&curthread->mutex_link_pages, ml, qe);
		munmap(ml->mutexp, _thr_page_size);
	}
	TAILQ_INIT(&curthread->mutex_link_freeq);
}

static void
set_inherited_priority(struct pthread *curthread, struct pthread_mutex *m)
{
	struct mutex_link *ml2;

	ml2 = TAILQ_LAST(&curthread->pp_mutexq, mutex_link_list);
	if (ml2 != NULL)
		m->m_lock.m_ceilings[1] = ml2->mutexp->m_lock.m_ceilings[0];
	else
		m->m_lock.m_ceilings[1] = -1;
}

static void
enqueue_mutex(struct pthread *curthread, struct pthread_mutex *m)
{
	struct mutex_link *ml;

	if ((m->m_lock.m_flags & USYNC_PROCESS_SHARED) == 0)
		m->m_ownertd = curthread;

	/* 
	 * For PP mutex, we should restore previous priority after a PP
	 * mutex is unlocked, so we should remember every PP mutex.
	 */
	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) != 0) {
		ml = _thr_mutex_link_alloc();
		ml->mutexp = m;
		TAILQ_INSERT_TAIL(&curthread->pp_mutexq, ml, qe);
	} else if ((m->m_lock.m_flags & (UMUTEX_PRIO_INHERIT |
			USYNC_PROCESS_SHARED)) == UMUTEX_PRIO_INHERIT) {
		/*
		 * To make unlocking after fork() work, we need to link it,
		 * because we still use TID as lock-word for PI mutex.
		 * However, processs-shared mutex only has one copy, it should
		 * not be unlockable for child process, so we don't link it.
		 */
		if ((m->m_lock.m_flags & USYNC_PROCESS_SHARED) == 0) {
			ml = _thr_mutex_link_alloc();
			ml->mutexp = m;
			TAILQ_INSERT_TAIL(&curthread->pi_mutexq, ml, qe);
		}
	}
	if ((m->m_lock.m_flags &
	    (UMUTEX_PRIO_PROTECT|UMUTEX_PRIO_PROTECT)) != 0) {
		curthread->priority_mutex_count++;
	}
}

static void
dequeue_mutex(struct pthread *curthread, struct pthread_mutex *m)
{
	struct mutex_link *ml;

	if ((m->m_lock.m_flags & USYNC_PROCESS_SHARED) == 0)
		m->m_ownertd = NULL;

	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) != 0) {
		TAILQ_FOREACH(ml, &curthread->pp_mutexq, qe) {
			if (ml->mutexp == m) {
				TAILQ_REMOVE(&curthread->pp_mutexq, ml, qe);
				set_inherited_priority(curthread, m);
				_thr_mutex_link_free(ml);
				goto out;
			}
		}
	} else if ((m->m_lock.m_flags & (UMUTEX_PRIO_INHERIT | 
			USYNC_PROCESS_SHARED)) == UMUTEX_PRIO_INHERIT) {
		TAILQ_FOREACH(ml, &curthread->pi_mutexq, qe) {
			if (ml->mutexp == m) {
				TAILQ_REMOVE(&curthread->pi_mutexq, ml, qe);
				_thr_mutex_link_free(ml);
				goto out;
			}
		}
	}
	return;

out:
	if ((m->m_lock.m_flags &
	    (UMUTEX_PRIO_PROTECT|UMUTEX_PRIO_PROTECT)) != 0) {
		curthread->priority_mutex_count--;
	}
}
