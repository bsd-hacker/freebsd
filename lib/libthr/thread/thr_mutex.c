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

/*
 * For adaptive mutexes, how many times to spin doing trylock2
 * before entering the kernel to block
 */
#define MUTEX_ADAPTIVE_SPINS	2000

/*
 * Prototypes
 */
int	_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count);
int	_pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count);
int	_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);

static int	mutex_self_trylock(struct pthread_mutex *);
static int	mutex_self_lock(struct pthread_mutex *,
				const struct timespec *abstime);
static int	mutex_unlock_common(struct pthread_mutex *);
static int	mutex_lock_sleep(struct pthread_mutex *,
				const struct timespec *);
static void	enqueue_mutex(struct pthread *, struct pthread_mutex *);
static void	dequeue_mutex(struct pthread *, struct pthread_mutex *);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_init, pthread_mutex_init);
__weak_reference(_pthread_mutex_lock, pthread_mutex_lock);
__weak_reference(_pthread_mutex_timedlock, pthread_mutex_timedlock);
__weak_reference(_pthread_mutex_trylock, pthread_mutex_trylock);
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);
__weak_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__weak_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);
__weak_reference(_pthread_mutex_setspinloops_np, pthread_mutex_setspinloops_np);
__weak_reference(_pthread_mutex_getspinloops_np, pthread_mutex_getspinloops_np);
__weak_reference(_pthread_mutex_setyieldloops_np, pthread_mutex_setyieldloops_np);
__weak_reference(_pthread_mutex_getyieldloops_np, pthread_mutex_getyieldloops_np);
__weak_reference(_pthread_mutex_isowned_np, pthread_mutex_isowned_np);
__weak_reference(_pthread_mutex_consistent, pthread_mutex_consistent);

int _pthread_mutex_init_calloc_cb(pthread_mutex_t *mp,
    void *(calloc_cb)(size_t, size_t));

/* Compatible functions */

int _pthread_mutex_destroy_1_0(pthread_mutex_old_t *);
int _pthread_mutex_init_1_0(pthread_mutex_old_t *, const pthread_mutexattr_t *);
int _pthread_mutex_trylock_1_0(pthread_mutex_old_t *);
int _pthread_mutex_lock_1_0(pthread_mutex_old_t *);
int _pthread_mutex_timedlock_1_0(pthread_mutex_old_t *, const struct timespec *);
int _pthread_mutex_unlock_1_0(pthread_mutex_old_t *);
int _pthread_mutex_getprioceiling_1_0(pthread_mutex_old_t *, int *);
int _pthread_mutex_setprioceiling_1_0(pthread_mutex_old_t *, int, int *);
int _pthread_mutex_getspinloops_np_1_0(pthread_mutex_old_t *, int *);
int _pthread_mutex_setspinloops_np_1_0(pthread_mutex_old_t *, int);
int _pthread_mutex_getyieldloops_np_1_0(pthread_mutex_old_t *, int *);
int _pthread_mutex_setyieldloops_np_1_0(pthread_mutex_old_t *, int);
int _pthread_mutex_isowned_np_1_0(pthread_mutex_old_t *);

static int
mutex_init(struct pthread_mutex *mp,
    const struct pthread_mutex_attr *mutex_attr)
{
	const struct pthread_mutex_attr *attr;

	/* Must align at integer boundary */
	if (((uintptr_t)mp) & 0x03)
		return (EINVAL);

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
	memset(mp, 0, sizeof(*mp));
	mp->__flags = attr->m_type;
	mp->__ownerdata.__ownertd = NULL;
	mp->__recurse = 0;
	mp->__spinloops = 0;
	switch(attr->m_protocol) {
	case PTHREAD_PRIO_NONE:
		mp->__lockword = UMUTEX_UNOWNED;
		if (attr->m_pshared == 0)
			mp->__lockflags |= UMUTEX_SIMPLE;
		break;
	case PTHREAD_PRIO_INHERIT:
		mp->__lockword = UMUTEX_UNOWNED;
		mp->__lockflags = UMUTEX_PRIO_INHERIT;
		break;
	case PTHREAD_PRIO_PROTECT:
		mp->__lockword = UMUTEX_UNOWNED;
		mp->__lockflags = UMUTEX_PRIO_PROTECT2;
		if (attr->m_pshared == 0)
			mp->__lockflags |= UMUTEX_SIMPLE;
		mp->__ceilings[0] = attr->m_ceiling;
		break;
	}
	if (attr->m_pshared != 0)
		mp->__lockflags |= USYNC_PROCESS_SHARED;
	if (attr->m_robust != PTHREAD_MUTEX_STALLED)
		mp->__lockflags |= UMUTEX_ROBUST;
	if (PMUTEX_TYPE(mp->__flags) == PTHREAD_MUTEX_ADAPTIVE_NP) {
		mp->__spinloops =
		    _thr_spinloops ? _thr_spinloops: MUTEX_ADAPTIVE_SPINS;
	}
	return (0);
}

int
_pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr ? *mutex_attr : NULL);
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
		ml->mutexp->__lockword = TID(curthread);
}

int
_pthread_mutex_destroy(pthread_mutex_t *mp)
{
	memset(mp, 0, sizeof(*mp));
	return (0);
}

static int
mutex_trylock_common(struct pthread_mutex *mp)
{
	struct pthread *curthread = _get_curthread();
	uint32_t id;
	int error;

	if ((mp->__lockflags & (UMUTEX_ROBUST | UMUTEX_PRIO_PROTECT2 |
	     UMUTEX_PRIO_INHERIT)) == 0) {
		if (mp->__lockflags & UMUTEX_SIMPLE)
			id = UMUTEX_SIMPLE_OWNER;
		else
			id = TID(curthread);
		if (atomic_cmpset_acq_32(&mp->__lockword, UMUTEX_UNOWNED,
		     id)) {
			mp->__ownerdata.__ownertd = curthread;
			return (0);
		}
		if (mp->__lockword == UMUTEX_CONTESTED) {
			if (atomic_cmpset_acq_32(&mp->__lockword,
			     UMUTEX_CONTESTED, id|UMUTEX_CONTESTED)) {
				mp->__ownerdata.__ownertd = curthread;
				return (0);
			}
		}
	} else if (mp->__lockflags & (UMUTEX_ROBUST | UMUTEX_PRIO_PROTECT2)) {
		if (mp->__lockflags & UMUTEX_SIMPLE) {
			if (mp->__ownerdata.__ownertd == curthread)
				return mutex_self_trylock(mp);
		} else {
			if ((mp->__lockword & UMUTEX_OWNER_MASK) ==
			      TID(curthread))
				return mutex_self_trylock(mp);
		}
		if ((mp->__lockword & UMUTEX_OWNER_MASK) != 0)
			return (EBUSY);
		error = __thr_umutex_trylock((struct umutex *)&mp->__lockword);
		if (error == 0 || error == EOWNERDEAD)
			enqueue_mutex(curthread, mp);
		return (error);
	} else if (mp->__lockflags & UMUTEX_PRIO_INHERIT) {
		id = TID(curthread);
    		if (atomic_cmpset_acq_32(&mp->__lockword, UMUTEX_UNOWNED, id)){
			enqueue_mutex(curthread, mp);
			return (0);
		}
		if ((mp->__lockflags & UMUTEX_OWNER_MASK) == id)
			return mutex_self_trylock(mp);
		return (EBUSY);
	}
	return (EINVAL);
}

int
_pthread_mutex_trylock(pthread_mutex_t *mp)
{
	struct pthread *curthread = _get_curthread();
	int error;

	if (!(mp->__flags & PMUTEX_FLAG_PRIVATE))
		return mutex_trylock_common(mp);
	THR_CRITICAL_ENTER(curthread);
	error = mutex_trylock_common(mp);
	if (error != 0 && error != EOWNERDEAD)
		THR_CRITICAL_LEAVE(curthread);
	return (error);
}

static int
mutex_lock_sleep(struct pthread_mutex *mp,
	const struct timespec *abstime)
{
	struct pthread *curthread  = _get_curthread();
	uint32_t	id, owner;
	int	count;
	int	error;

	/*
	 * For adaptive mutexes, spin for a bit in the expectation
	 * that if the application requests this mutex type then
	 * the lock is likely to be released quickly and it is
	 * faster than entering the kernel.
	 */
	if (__predict_false(
		(mp->__lockflags & 
		 (UMUTEX_PRIO_PROTECT2 | UMUTEX_PRIO_INHERIT |
	          UMUTEX_ROBUST)) != 0))
			goto sleep_in_kernel;

	if ((mp->__lockflags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);

	if (!_thr_is_smp)
		goto sleep_in_kernel;

	count = mp->__spinloops;
	while (count--) {
		owner = mp->__lockword;
		if ((owner & UMUTEX_OWNER_MASK) == 0) {
			if (atomic_cmpset_acq_32(&mp->__lockword, owner,
			    id|owner)) {
				error = 0;
				goto done;
			}
		}
		CPU_SPINWAIT;
	}

sleep_in_kernel:
	if (abstime == NULL) {
		error = __thr_umutex_lock((struct umutex *)&mp->__lockword, id);
	} else {
		error = __thr_umutex_timedlock((struct umutex *)&mp->__lockword, id, abstime);
	}
done:
	if (error == 0 || error == EOWNERDEAD)
		enqueue_mutex(curthread, mp);

	return (error);
}

static inline int
_mutex_lock_common(struct pthread_mutex *mp,
	const struct timespec *abstime)
{
	struct pthread *curthread  = _get_curthread();
	uint32_t id;

	if ((mp->__lockflags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);
	if ((mp->__lockflags & (UMUTEX_ROBUST | UMUTEX_PRIO_PROTECT2 |
	     UMUTEX_PRIO_INHERIT)) == 0) {
		if (atomic_cmpset_acq_32(&mp->__lockword, UMUTEX_UNOWNED,
		     id)) {
			mp->__ownerdata.__ownertd = curthread;
			return (0);
		}
		if (mp->__lockword == UMUTEX_CONTESTED) {
    			if (atomic_cmpset_acq_32(&mp->__lockword,
			     UMUTEX_CONTESTED, id|UMUTEX_CONTESTED)) {
				mp->__ownerdata.__ownertd = curthread;
				return (0);
			}
		}
	} else if ((mp->__lockflags & (UMUTEX_PRIO_INHERIT|UMUTEX_ROBUST)) ==
	            UMUTEX_PRIO_INHERIT) {
		id = TID(curthread);
		if (atomic_cmpset_acq_32(&mp->__lockword, UMUTEX_UNOWNED,
		     id)) {
			enqueue_mutex(curthread, mp);
			return (0);
		}
		if ((mp->__lockword & UMUTEX_OWNER_MASK) == id)
			return mutex_self_trylock(mp);
		return (EBUSY);
	}

	if (abstime != NULL && (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000))
		return (EINVAL);

	if (mp->__lockflags & UMUTEX_SIMPLE) {
		if (mp->__ownerdata.__ownertd == curthread)
			return mutex_self_lock(mp, abstime);
	} else {
		if ((mp->__lockword & UMUTEX_OWNER_MASK) == TID(curthread))
			return mutex_self_lock(mp, abstime);
	}

	return mutex_lock_sleep(mp, abstime);
}

static inline int
mutex_lock_common(struct pthread_mutex *mp,
	const struct timespec *abstime, int cvattach)
{
	struct pthread *curthread = _get_curthread();
	int error;

	if (cvattach || (mp->__flags & PMUTEX_FLAG_PRIVATE) == 0)
		return _mutex_lock_common(mp, abstime);
	if (mp->__flags & PMUTEX_FLAG_PRIVATE)
		THR_CRITICAL_ENTER(curthread);
	error = _mutex_lock_common(mp, abstime);
	if (error && error != EOWNERDEAD)
		THR_CRITICAL_LEAVE(curthread);
	return (error);
}

int
_pthread_mutex_lock(pthread_mutex_t *mp)
{

	_thr_check_init();

	return (mutex_lock_common(mp, NULL, 0));
}

int
_pthread_mutex_timedlock(pthread_mutex_t *mp, const struct timespec *abstime)
{

	_thr_check_init();

	return (mutex_lock_common(mp, abstime, 0));
}

int
_pthread_mutex_unlock(pthread_mutex_t *mp)
{
	return (mutex_unlock_common(mp));
}

static int
mutex_self_trylock(struct pthread_mutex *mp)
{
	int	error;

	switch (PMUTEX_TYPE(mp->__flags)) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
		error = EBUSY; 
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (mp->__recurse + 1 > 0) {
			mp->__recurse++;
			error = 0;
		} else
			error = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		error = EINVAL;
	}

	return (error);
}

static int
mutex_self_lock(struct pthread_mutex *mp, const struct timespec *abstime)
{
	struct timespec	ts1, ts2;
	int	error;

	switch (PMUTEX_TYPE(mp->__flags)) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_ADAPTIVE_NP:
		if (abstime) {
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				error = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				error = ETIMEDOUT;
			}
		} else {
			/*
			 * POSIX specifies that mutexes should return
			 * EDEADLK if a recursive lock is detected.
			 */
			error = EDEADLK; 
		}
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		error = 0;
		if (abstime) {
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				error = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				error = ETIMEDOUT;
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
		if (mp->__recurse + 1 > 0) {
			mp->__recurse++;
			error = 0;
		} else
			error = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		error = EINVAL;
	}

	return (error);
}

int
_mutex_owned(struct pthread *curthread, const struct pthread_mutex *mp)
{
	/*
	 * Check if the running thread is not the owner of the mutex.
	 */
	if ((mp->__lockflags & UMUTEX_SIMPLE) != 0) {
		if (__predict_false(mp->__ownerdata.__ownertd != curthread))
			return (EPERM);
	} else {
		if ((mp->__lockword & UMUTEX_OWNER_MASK) != TID(curthread))
			return (EPERM);
	}
	return (0);
}

static inline int
_mutex_unlock_common(struct pthread_mutex *mp)
{
	struct pthread *curthread;
	uint32_t id;

	if (__predict_false(
		PMUTEX_TYPE(mp->__lockflags) == PTHREAD_MUTEX_RECURSIVE &&
		mp->__recurse > 0)) {
		mp->__recurse--;
		if (mp->__flags & PMUTEX_FLAG_PRIVATE)
			THR_CRITICAL_LEAVE(curthread);
		return (0);
	}

	curthread = _get_curthread();
	dequeue_mutex(curthread, mp);

	if ((mp->__lockflags & UMUTEX_SIMPLE) != 0)
		id = UMUTEX_SIMPLE_OWNER;
	else
		id = TID(curthread);

	if ((mp->__lockflags & (UMUTEX_ROBUST | UMUTEX_PRIO_PROTECT2 |
	     UMUTEX_PRIO_INHERIT)) == 0) {
		if (atomic_cmpset_acq_32(&mp->__lockword, id,
		     UMUTEX_UNOWNED)) {
			goto out;
		}
	} else if ((mp->__lockflags & (UMUTEX_PRIO_INHERIT|UMUTEX_ROBUST)) ==
	            UMUTEX_PRIO_INHERIT) {
		id = TID(curthread);
		if (atomic_cmpset_acq_32(&mp->__lockword, id,
		     UMUTEX_UNOWNED)) {
			goto out;
		}
	}
	__thr_umutex_unlock((struct umutex *)&mp->__lockword, id);
out:
	if (mp->__flags & PMUTEX_FLAG_PRIVATE)
		THR_CRITICAL_LEAVE(curthread);
	return (0);
}

static int
mutex_unlock_common(pthread_mutex_t *mp)
{
	int	error;

	if ((error = _mutex_owned(_get_curthread(), mp)) != 0)
		return (error);
	return _mutex_unlock_common(mp);
}

int
_mutex_cv_lock(pthread_mutex_t *mp, int count)
{
	int	error;

	error = mutex_lock_common(mp, NULL, 1);
	if (error == 0)
		mp->__recurse += count;
	return (error);
}

int
_mutex_cv_unlock(pthread_mutex_t *mp, int *count)
{
	struct pthread *curthread = _get_curthread();
	int	error;

	if ((error = _mutex_owned(curthread, mp)) != 0)
		return (error);

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*count = mp->__recurse;
	mp->__recurse = 0;

	(void)_mutex_unlock_common(mp);
	return (0);
}

int
_mutex_cv_attach(pthread_mutex_t *mp, int count)
{
	struct pthread *curthread = _get_curthread();
	int	error;

	enqueue_mutex(curthread, mp);
	mp->__recurse += count;
	return (error);
}

int
_mutex_cv_detach(pthread_mutex_t *mp, int *recurse)
{
	struct pthread *curthread = _get_curthread();
	int	error;

	if ((error = _mutex_owned(curthread, mp)) != 0)
		return (error);

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*recurse = mp->__recurse;
	mp->__recurse = 0;
	dequeue_mutex(curthread, mp);
	return (0);
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t *mp, int *prioceiling)
{
	int	error;

	if ((mp->__lockflags & UMUTEX_PRIO_PROTECT2) == 0)
		error = EINVAL;
	else {
		*prioceiling = mp->__ceilings[0];
		error = 0;
	}

	return (error);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t *mp,
			      int ceiling, int *old_ceiling)
{
	struct pthread *curthread = _get_curthread();
	struct mutex_link *ml, *ml1, *ml2;
	int	error;

	if ((mp->__lockflags & UMUTEX_PRIO_PROTECT2) == 0)
		return (EINVAL);

	error = __thr_umutex_set_ceiling((struct umutex *)&mp->__lockword,
		ceiling, old_ceiling);
	if (error != 0)
		return (error);
	if (((mp->__lockflags & UMUTEX_SIMPLE) &&
	     (mp->__ownerdata.__ownertd == curthread)) || 
           (mp->__lockword & UMUTEX_OWNER_MASK) == TID(curthread)) {
		TAILQ_FOREACH(ml, &curthread->pp_mutexq, qe) {
			if (ml->mutexp == mp)
				break;
		}
		if (ml == NULL) /* howto ? */
			return (0);
		ml1 = TAILQ_PREV(ml, mutex_link_list, qe);
		ml2 = TAILQ_NEXT(ml, qe);
		if ((ml1 != NULL && ml1->mutexp->__ceilings[0] > (u_int)ceiling) ||
		    (ml2 != NULL && ml2->mutexp->__ceilings[0] < (u_int)ceiling)) {
			TAILQ_REMOVE(&curthread->pp_mutexq, ml, qe);
			TAILQ_FOREACH(ml2, &curthread->pp_mutexq, qe) {
				if (ml2->mutexp->__ceilings[0] > (u_int)ceiling) {
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
_pthread_mutex_getspinloops_np(pthread_mutex_t *mp, int *count)
{

	*count = mp->__spinloops;
	return (0);
}

int
_pthread_mutex_setspinloops_np(pthread_mutex_t *mp, int count)
{

	mp->__spinloops = count;
	return (0);
}

int
_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count)
{
	*count = 0;
	return (0);
}

int
_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count)
{
	return (0);
}

int
_pthread_mutex_isowned_np(pthread_mutex_t *mp)
{
	return (_mutex_owned(_get_curthread(), mp) == 0);
}

int
_pthread_mutex_consistent(pthread_mutex_t *mp)
{

	if (_mutex_owned(_get_curthread(), mp) == 0) {
		if (mp->__lockflags & UMUTEX_ROBUST) {
			atomic_clear_32(&mp->__lockword, UMUTEX_OWNER_DEAD);
			mp->__recurse = 0;
			return (0);
		}
	}
	return (EINVAL);
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
set_inherited_priority(struct pthread *curthread, struct pthread_mutex *mp)
{
	struct mutex_link *ml2;

	ml2 = TAILQ_LAST(&curthread->pp_mutexq, mutex_link_list);
	if (ml2 != NULL)
		mp->__ceilings[1] = ml2->mutexp->__ceilings[0];
	else
		mp->__ceilings[1] = -1;
}

static void
enqueue_mutex(struct pthread *curthread, struct pthread_mutex *mp)
{
	struct mutex_link *ml;

	if ((mp->__lockflags & USYNC_PROCESS_SHARED) == 0)
		mp->__ownerdata.__ownertd = curthread;

	/* 
	 * For PP mutex, we should restore previous priority after a PP
	 * mutex is unlocked, so we should remember every PP mutex.
	 */
	if ((mp->__lockflags & UMUTEX_PRIO_PROTECT2) != 0) {
		curthread->priority_mutex_count++;
		ml = _thr_mutex_link_alloc();
		ml->mutexp = mp;
		TAILQ_INSERT_TAIL(&curthread->pp_mutexq, ml, qe);
	} else if ((mp->__lockflags & UMUTEX_PRIO_INHERIT) != 0) {
		curthread->priority_mutex_count++;
		/*
		 * To make unlocking after fork() work, we need to link it,
		 * because we still use TID as lock-word for PI mutex.
		 * However, processs-shared mutex only has one copy, it should
		 * not be unlockable for child process, so we don't link it,
		 * and _mutex_fork() won't find it.
		 */
		if ((mp->__lockflags & USYNC_PROCESS_SHARED) != 0)
			return;
		ml = _thr_mutex_link_alloc();
		ml->mutexp = mp;
		TAILQ_INSERT_TAIL(&curthread->pi_mutexq, ml, qe);
	}
}

static void
dequeue_mutex(struct pthread *curthread, struct pthread_mutex *mp)
{
	struct mutex_link *ml;

	if ((mp->__lockflags & USYNC_PROCESS_SHARED) == 0)
		mp->__ownerdata.__ownertd = NULL;

	if ((mp->__lockflags & UMUTEX_PRIO_PROTECT2) != 0) {
		curthread->priority_mutex_count--;
		TAILQ_FOREACH(ml, &curthread->pp_mutexq, qe) {
			if (ml->mutexp == mp) {
				TAILQ_REMOVE(&curthread->pp_mutexq, ml, qe);
				set_inherited_priority(curthread, mp);
				_thr_mutex_link_free(ml);
				break;
			}
		}
	} else if ((mp->__lockflags & UMUTEX_PRIO_INHERIT) != 0) {
		curthread->priority_mutex_count--;
		if ((mp->__lockflags & USYNC_PROCESS_SHARED) != 0)
			return;
		TAILQ_FOREACH(ml, &curthread->pi_mutexq, qe) {
			if (ml->mutexp == mp) {
				TAILQ_REMOVE(&curthread->pi_mutexq, ml, qe);
				_thr_mutex_link_free(ml);
				break;
			}
		}
	}
}

int
_mutex_owned_old(struct pthread *curthread, pthread_mutex_old_t *mutex)
{
	struct pthread_mutex *mp;

	mp = *mutex;
        if (__predict_false(mp <= THR_MUTEX_DESTROYED)) {
		if (mp == THR_MUTEX_DESTROYED)
			return (EINVAL);
		return (EPERM);
	}

	return _mutex_owned(curthread, mp);
}

static int
mutex_init_old(pthread_mutex_old_t *mutex,
	const struct pthread_mutex_attr *mutex_attr)
{
	struct pthread_mutex *mp;
	int error;

	if ((mp = (struct pthread_mutex *)
		malloc(sizeof(struct pthread_mutex))) == NULL) {
		return (ENOMEM);
	}
	error = mutex_init(mp, mutex_attr);
	if (error != 0)
		free(mp);
	else
		*mutex = mp;
	return (error);
}

#define CHECK_AND_INIT_MUTEX						\
	if (__predict_false((mp = *mutex) <= THR_MUTEX_DESTROYED)) {	\
		if (mp == THR_MUTEX_DESTROYED)				\
			return (EINVAL);				\
		int error;						\
		error = init_static(_get_curthread(), mutex);		\
		if (error)						\
			return (error);					\
		mp = *mutex;						\
	}

static int
init_static(struct pthread *thread, pthread_mutex_old_t *mutex)
{
	int error;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);
	
	if (*mutex == THR_MUTEX_INITIALIZER) {
		error = mutex_init_old(mutex, &_pthread_mutexattr_default);
	} else if (*mutex == THR_ADAPTIVE_MUTEX_INITIALIZER) {
		error = mutex_init_old(mutex,
			&_pthread_mutexattr_adaptive_default);
	}
	else
		error = 0;
	THR_LOCK_RELEASE(thread, &_mutex_static_lock);
	return (error);
}

int
_pthread_mutex_destroy_1_0(pthread_mutex_old_t *mutex)
{
	pthread_mutex_t *mp;
	int error;

	mp = *mutex;
	if (mp < THR_MUTEX_DESTROYED) {
		error = 0;
	} else if (mp == THR_MUTEX_DESTROYED) {
		error = EINVAL;
	} else {
		*mutex = THR_MUTEX_DESTROYED;
		free(mp);
		error = 0;
	}

	return (error);
}

int
_pthread_mutex_init_1_0(pthread_mutex_old_t *mutex,
	const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init_old(mutex, mutex_attr ? *mutex_attr : NULL);
}

int
_pthread_mutex_trylock_1_0(pthread_mutex_old_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *mp;
	int error;

	CHECK_AND_INIT_MUTEX

	if (!(mp->__flags & PMUTEX_FLAG_PRIVATE))
		return mutex_trylock_common(mp);
	THR_CRITICAL_ENTER(curthread);
	error = mutex_trylock_common(mp);
	if (error != 0 && error != EOWNERDEAD)
		THR_CRITICAL_LEAVE(curthread);
	return (error);
}

int
_pthread_mutex_lock_1_0(pthread_mutex_old_t *mutex)
{
	struct pthread_mutex	*mp;

	_thr_check_init();

	CHECK_AND_INIT_MUTEX

	return (mutex_lock_common(mp, NULL, 0));
}

int
_pthread_mutex_timedlock_1_0(pthread_mutex_old_t *mutex,
	const struct timespec *abstime)
{
	struct pthread_mutex	*mp;

	_thr_check_init();

	CHECK_AND_INIT_MUTEX

	return (mutex_lock_common(mp, abstime, 0));
}

int
_pthread_mutex_unlock_1_0(pthread_mutex_old_t *mutex)
{
	struct pthread_mutex	*mp;

	mp = *mutex;
	if (__predict_false(mp <= THR_MUTEX_DESTROYED)) {
		if (mp == THR_MUTEX_DESTROYED)
			return (EINVAL);
		return (EPERM);
	}
	return _pthread_mutex_unlock(mp);
}

int
_pthread_mutex_getspinloops_np_1_0(pthread_mutex_old_t *mutex, int *count)
{
	struct pthread_mutex	*mp;

	CHECK_AND_INIT_MUTEX

	*count = mp->__spinloops;
	return (0);
}

int
_pthread_mutex_setspinloops_np_1_0(pthread_mutex_old_t *mutex, int count)
{
	struct pthread_mutex	*mp;

	CHECK_AND_INIT_MUTEX

	mp->__spinloops = count;
	return (0);
}

int
_pthread_mutex_getyieldloops_np_1_0(pthread_mutex_old_t *mutex, int *count)
{
#if 0
	struct pthread_mutex	*mp;

	CHECK_AND_INIT_MUTEX

	*count = m->m_yieldloops;
#endif
	*count = 0;
	return (0);
}

int
_pthread_mutex_setyieldloops_np_1_0(pthread_mutex_old_t *mutex, int count)
{
#if 0
	struct pthread_mutex	*mp;

	CHECK_AND_INIT_MUTEX

	mp->m_yieldloops = count;
#endif
	return (0);
}

int
_pthread_mutex_isowned_np_1_0(pthread_mutex_old_t *mutex)
{
	return (_mutex_owned_old(_get_curthread(), mutex) == 0);
}

int
_pthread_mutex_getprioceiling_1_0(pthread_mutex_old_t *mutex,
			      int *prioceiling)
{
	struct pthread_mutex *mp;
	int error;

	mp = *mutex;
	if ((mp <= THR_MUTEX_DESTROYED) ||
	    (mp->__lockflags & UMUTEX_PRIO_PROTECT2) == 0)
		error = EINVAL;
	else {
		*prioceiling = mp->__ceilings[0];
		error = 0;
	}

	return (error);
}

int
_pthread_mutex_setprioceiling_1_0(pthread_mutex_old_t *mutex,
			      int ceiling, int *old_ceiling)
{
	struct pthread_mutex *mp;

	mp = *mutex;
	if ((mp <= THR_MUTEX_DESTROYED) ||
	    (mp->__lockflags & UMUTEX_PRIO_PROTECT2) == 0)
		return (EINVAL);
	return _pthread_mutex_setprioceiling(mp, ceiling, old_ceiling);
}

/* This function is used internally by malloc. */
int
_pthread_mutex_init_calloc_cb(pthread_mutex_t *mp,
    void *(calloc_cb)(size_t, size_t))
{
	static const struct pthread_mutex_attr attr = {
		.m_type = PTHREAD_MUTEX_NORMAL,
		.m_protocol = PTHREAD_PRIO_NONE,
		.m_ceiling = 0,
		.m_pshared = 0,
		.m_robust = PTHREAD_MUTEX_STALLED
	};
	int	error;
	error = mutex_init(mp, &attr);
	if (error == 0)
		mp->__flags |= PMUTEX_FLAG_PRIVATE;
	return (error);
}

FB10_COMPAT(_pthread_mutex_destroy_1_0, pthread_mutex_destroy);
FB10_COMPAT(_pthread_mutex_getprioceiling_1_0, pthread_mutex_getprioceiling);
FB10_COMPAT(_pthread_mutex_getspinloops_np_1_0, pthread_mutex_getspinloops_np);
FB10_COMPAT(_pthread_mutex_getyieldloops_np_1_0, pthread_mutex_getyieldloops_np);
FB10_COMPAT(_pthread_mutex_init_1_0, pthread_mutex_init);
FB10_COMPAT(_pthread_mutex_lock_1_0, pthread_mutex_lock);
FB10_COMPAT(_pthread_mutex_setprioceiling_1_0, pthread_mutex_setprioceiling);
FB10_COMPAT(_pthread_mutex_setspinloops_np_1_0, pthread_mutex_setspinloops_np);
FB10_COMPAT(_pthread_mutex_setyieldloops_np_1_0, pthread_mutex_setyieldloops_np);
FB10_COMPAT(_pthread_mutex_timedlock_1_0, pthread_mutex_timedlock);
FB10_COMPAT(_pthread_mutex_trylock_1_0, pthread_mutex_trylock);
FB10_COMPAT(_pthread_mutex_unlock_1_0, pthread_mutex_unlock);
