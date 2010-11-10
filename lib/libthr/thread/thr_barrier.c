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
#include <string.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_pthread_barrier_init,		pthread_barrier_init);
__weak_reference(_pthread_barrier_wait,		pthread_barrier_wait);
__weak_reference(_pthread_barrier_destroy,	pthread_barrier_destroy);

typedef struct pthread_barrier *pthread_barrier_old_t;
int	_pthread_barrier_destroy_1_0(pthread_barrier_old_t *);
int	_pthread_barrier_wait_1_0(pthread_barrier_old_t *);
int	_pthread_barrier_init_1_0(pthread_barrier_old_t *,
	const pthread_barrierattr_t *, unsigned);

int
_pthread_barrier_destroy(pthread_barrier_t *barp)
{
	(void)_pthread_cond_destroy(&barp->__cond);
	(void)_pthread_mutex_destroy(&barp->__lock);
	memset(barp, -1, sizeof(*barp));
	return (0);
}

int
_pthread_barrier_init(pthread_barrier_t *barp,
		      const pthread_barrierattr_t *attr, unsigned count)
{
	if (count == 0)
		return (EINVAL);

	_pthread_mutex_init(&barp->__lock, NULL);
	_pthread_cond_init(&barp->__cond, NULL);
	if (attr != NULL && *attr != NULL) {
		if ((*attr)->pshared == PTHREAD_PROCESS_SHARED) {
			barp->__lock.__lockflags |= USYNC_PROCESS_SHARED;
			barp->__cond.__flags |= USYNC_PROCESS_SHARED;
		} else if ((*attr)->pshared != PTHREAD_PROCESS_PRIVATE) {
			return (EINVAL);
		}
	}
	barp->__cycle	= 0;
	barp->__waiters	= 0;
	barp->__count	= count;
	return (0);
}

int
_pthread_barrier_wait(pthread_barrier_t *barp)
{
	uint64_t cycle;
	int error;

	_pthread_mutex_lock(&barp->__lock);
	if (++barp->__waiters == barp->__count) {
		/* Current thread is lastest thread. */
		barp->__waiters = 0;
		barp->__cycle++;
		_pthread_cond_broadcast(&barp->__cond);
		_pthread_mutex_unlock(&barp->__lock);
		error = PTHREAD_BARRIER_SERIAL_THREAD;
	} else {
		cycle = barp->__cycle;
		do {
			_pthread_cond_wait(&barp->__cond, &barp->__lock);
			/* test cycle to avoid bogus wakeup */
		} while (cycle == barp->__cycle);
		_pthread_mutex_unlock(&barp->__lock);
		error = 0;
	}
	return (error);
}

int
_pthread_barrier_destroy_1_0(pthread_barrier_old_t *barpp)
{
	struct pthread_barrier *barp;

	if ((barp = *barpp) == NULL)
		return (EINVAL);
	_pthread_barrier_destroy(barp);
	free(barp);
	return (0);
}

int
_pthread_barrier_init_1_0(pthread_barrier_old_t *barpp,
	const pthread_barrierattr_t *attr, unsigned count)
{
	struct pthread_barrier	*barp;
	int error;

	barp = malloc(sizeof(struct pthread_barrier));
	if (barp == NULL)
		return (ENOMEM);
	error = _pthread_barrier_init(barp, attr, count);
	if (error) {
		free(barp);
		return (error);
	}
	*barpp = barp;
	return (0);
}

int
_pthread_barrier_wait_1_0(pthread_barrier_old_t *barpp)
{
	struct pthread_barrier *barp;

	if ((barp = *barpp) == NULL)
		return (EINVAL);
	return _pthread_barrier_wait(barp);
}

FB10_COMPAT(_pthread_barrier_destroy_1_0, pthread_barrier_destroy);
FB10_COMPAT(_pthread_barrier_init_1_0, pthread_barrier_init);
FB10_COMPAT(_pthread_barrier_wait_1_0, pthread_barrier_wait);
