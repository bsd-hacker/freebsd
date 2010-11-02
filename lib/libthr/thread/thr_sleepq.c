/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (C) 2003 Daniel M. Eischen <deischen@freebsd.org>
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

#include <stdlib.h>
#include "thr_private.h"

#define HASHSHIFT	9
#define HASHSIZE	(1 << HASHSHIFT)
#define SC_HASH(wchan, type) ((unsigned)			\
	((((uintptr_t)(wchan) >> 3)				\
	^ ((uintptr_t)(wchan) >> (HASHSHIFT + 3)))		\
	& (HASHSIZE - 1)) + (((type) == MX)? 0 : HASHSIZE))
#define SC_LOOKUP(wc, type)	&sc_table[SC_HASH(wc, type)]

struct sleepqueue_chain {
	LIST_HEAD(, sleepqueue) sc_queues;
	LIST_HEAD(, sleepqueue) sc_freeq;
//	struct umutex		sc_lock;
	umtx_t			sc_lock;
	int			sc_type;
	struct sleepqueue	sc_spare;
};

static struct sleepqueue_chain  sc_table[HASHSIZE * 2];

void
_sleepq_init(void)
{
	int	i;
	struct sleepqueue *sq;

	for (i = 0; i < 2 * HASHSIZE; ++i) {
		LIST_INIT(&sc_table[i].sc_queues);
		LIST_INIT(&sc_table[i].sc_freeq);
		//_thr_umutex_init(&sc_table[i].sc_lock);
		sc_table[i].sc_lock = 0;
		sc_table[i].sc_type = i < HASHSIZE ? MX : CV;
		sq = &sc_table[i].sc_spare;
		TAILQ_INIT(&sq->sq_blocked);
		LIST_INSERT_HEAD(&sc_table[i].sc_freeq, sq, sq_hash);
	}
}

struct sleepqueue *
_sleepq_alloc(void)
{
	struct sleepqueue *sq;

	sq = calloc(1, sizeof(struct sleepqueue));
	TAILQ_INIT(&sq->sq_blocked);
	return (sq);
}

void
_sleepq_free(struct sleepqueue *sq)
{
	if ((char *)sq < (char *)sc_table ||
	    (char *)sq >= (char *)&sc_table[HASHSIZE * 2])
		free(sq);
}

struct sleepqueue *
_sleepq_lock(void *wchan, int type)
{
	struct pthread *curthread = _get_curthread();
	struct sleepqueue_chain *sc;
	struct sleepqueue	*sq;

	sc = SC_LOOKUP(wchan, type);
//	THR_LOCK_ACQUIRE_SPIN(_get_curthread(), &sc->sc_lock);
	THR_CRITICAL_ENTER(curthread);
	_thr_umtx_lock_spin(&sc->sc_lock);	
	LIST_FOREACH(sq, &sc->sc_queues, sq_hash)
		if (sq->sq_wchan == wchan)
			return (sq);
	/*
	 * If not found, pick a free queue header, note that
	 * if a thread locked the chain successfully,
	 * there must have a free sleepqueue, because
	 * we initialized the chain with one extra sleepqueue.
	 */
	sq = LIST_FIRST(&sc->sc_freeq);
	LIST_REMOVE(sq, sq_hash);
	LIST_INSERT_HEAD(&sc->sc_queues, sq, sq_hash);
	sq->sq_wchan = wchan;
	sq->sq_type = type;
	return (sq);
}

void
_sleepq_unlock(struct sleepqueue *sq)
{
	struct pthread *curthread = _get_curthread();
	struct sleepqueue_chain *sc;
                    
	sc = SC_LOOKUP(sq->sq_wchan, sq->sq_type);
	if (TAILQ_EMPTY(&sq->sq_blocked)) {
		LIST_REMOVE(sq, sq_hash);
		LIST_INSERT_HEAD(&sc->sc_freeq, sq, sq_hash);
	}
	//THR_LOCK_RELEASE(_get_curthread(), &sc->sc_lock);
	_thr_umtx_unlock(&sc->sc_lock);
	THR_CRITICAL_LEAVE(curthread);
}

void
_sleepq_add(struct sleepqueue *sq, struct pthread *td)
{
	struct sleepqueue_chain *sc;

	sc = SC_LOOKUP(sq->sq_wchan, sq->sq_type);
	LIST_INSERT_HEAD(&sc->sc_freeq, td->sleepqueue, sq_hash);
	td->sleepqueue = NULL;
	td->wchan = sq->sq_wchan;
	TAILQ_INSERT_TAIL(&sq->sq_blocked, td, wle);
}

void
_sleepq_remove(struct sleepqueue *sq, struct pthread *td)
{
	struct sleepqueue_chain *sc;

	sc = SC_LOOKUP(sq->sq_wchan, sq->sq_type);
	THR_ASSERT((td->wchan == sq->sq_wchan), "wchan is not equal");
	TAILQ_REMOVE(&sq->sq_blocked, td, wle);
	td->wchan = NULL;
	td->sleepqueue = LIST_FIRST(&sc->sc_freeq);
	LIST_REMOVE(td->sleepqueue, sq_hash);
}

void
_sleepq_concat(struct sleepqueue *sq_dst, struct sleepqueue *sq_src)
{
	struct sleepqueue_chain *sc_dst, *sc_src;
	struct sleepqueue *sq_tmp;
	struct pthread *td;

	sc_dst = SC_LOOKUP(sq_dst->sq_wchan, sq_dst->sq_type);
	sc_src = SC_LOOKUP(sq_src->sq_wchan, sq_src->sq_type);
	TAILQ_FOREACH(td, &sq_src->sq_blocked, wle) {
		td->wchan = sq_dst->sq_wchan;
		/*
		 * We should move same number of free sleepqueues to
		 * new channel.
		 */
		sq_tmp = LIST_FIRST(&sc_src->sc_freeq);
		LIST_REMOVE(sq_tmp, sq_hash);
		LIST_INSERT_HEAD(&sc_dst->sc_freeq, sq_tmp, sq_hash);
	}
	TAILQ_CONCAT(&sq_dst->sq_blocked, &sq_src->sq_blocked, wle);
}
