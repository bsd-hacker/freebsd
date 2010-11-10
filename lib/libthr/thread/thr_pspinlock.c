/*-
 * Copyright (c) 2003 David Xu <davidxu@freebsd.org>
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

#include "namespace.h"
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_pthread_spin_init, pthread_spin_init);
__weak_reference(_pthread_spin_destroy, pthread_spin_destroy);
__weak_reference(_pthread_spin_trylock, pthread_spin_trylock);
__weak_reference(_pthread_spin_lock, pthread_spin_lock);
__weak_reference(_pthread_spin_unlock, pthread_spin_unlock);

typedef pthread_spinlock_t *pthread_spinlock_old_t;
int _pthread_spin_destroy_1_0(pthread_spinlock_old_t *);
int _pthread_spin_init_1_0(pthread_spinlock_old_t *, int);
int _pthread_spin_lock_1_0(pthread_spinlock_old_t *);
int _pthread_spin_trylock_1_0(pthread_spinlock_old_t *);
int _pthread_spin_unlock_1_0(pthread_spinlock_old_t *);

int
_pthread_spin_init(pthread_spinlock_t *lckp, int pshared)
{
	if (pshared != PTHREAD_PROCESS_PRIVATE &&
	    pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);
	lckp->__lock = 0;
	return (0);
}

int
_pthread_spin_destroy(pthread_spinlock_t *lckp)
{
	/* Nothing to do. */
	return (0);
}

int
_pthread_spin_trylock(pthread_spinlock_t *lckp)
{
	if (atomic_cmpset_acq_32(&lckp->__lock, 0, 1))
		return (0);
	return (EBUSY);
}

int
_pthread_spin_lock(pthread_spinlock_t *lckp)
{
	/* 
	 * Nothing has been checked, the lock should be
	 * as fast as possible.
	 */
	if (atomic_cmpset_acq_32(&lckp->__lock, 0, 1))
		return (0);
	for (;;) {
		if (*(volatile int32_t *)&(lckp->__lock) == 0)
			if (atomic_cmpset_acq_32(&lckp->__lock, 0, 1))
				break;
		if (!_thr_is_smp)
			_pthread_yield();
		else
			CPU_SPINWAIT;
	}
	return (0);
}

int
_pthread_spin_unlock(pthread_spinlock_t *lckp)
{
	lckp->__lock = 0;
	wmb();
	return (0);
}

int
_pthread_spin_init_1_0(pthread_spinlock_old_t *lckpp, int pshared)
{
	pthread_spinlock_t *lckp;

	if (pshared != PTHREAD_PROCESS_PRIVATE &&
	    pshared != PTHREAD_PROCESS_SHARED)
		return (EINVAL);
	
	lckp = malloc(sizeof(pthread_spinlock_t));
	if (lckp == NULL)
		return (ENOMEM);
	lckp->__lock = 0;
	*lckpp = lckp;
	return (0);
}

int
_pthread_spin_destroy_1_0(pthread_spinlock_old_t *lckpp)
{
	pthread_spinlock_t *lckp = *lckpp;

	if (lckp != NULL) {
		free(lckp);
		*lckpp = NULL;
		return (0);
	} else
		return (EINVAL);
}

int
_pthread_spin_trylock_1_0(pthread_spinlock_old_t *lckpp)
{
	pthread_spinlock_t *lckp = *lckpp;

	if (lckp == NULL)
		return (EINVAL);
	return _pthread_spin_trylock(lckp);
}

int
_pthread_spin_lock_1_0(pthread_spinlock_old_t *lckpp)
{
	pthread_spinlock_t *lckp = *lckpp;

	if (lckp == NULL)
		return (EINVAL);
	return _pthread_spin_lock(lckp);
}

int
_pthread_spin_unlock_1_0(pthread_spinlock_old_t *lckpp)
{
	pthread_spinlock_t *lckp = *lckpp;

	if (lckp == NULL)
		return (EINVAL);
	return _pthread_spin_unlock(lckp);
}

FB10_COMPAT(_pthread_spin_destroy_1_0, pthread_spin_destroy);
FB10_COMPAT(_pthread_spin_init_1_0, pthread_spin_init);
FB10_COMPAT(_pthread_spin_lock_1_0, pthread_spin_lock);
FB10_COMPAT(_pthread_spin_trylock_1_0, pthread_spin_trylock);
FB10_COMPAT(_pthread_spin_unlock_1_0, pthread_spin_unlock);
