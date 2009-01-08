/*-
 * Copyright (c) 2008 Fabio Checconi
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/callout.h>
#include <sys/hash.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <geom/geom.h>
#include <geom/geom_sched.h>

/*
 * Trivial round robin disk scheduler, with per-thread queues, always
 * anticipating requests from the last served thread.
 */

/* Timeout for anticipation. */
#define	G_RR_WAIT_EXPIRE	(hz/200 ? hz/200 : 2)

/* Maximum allowed slice length. */
#define G_RR_SLICE_EXPIRE	(hz/10)

#define	G_QUEUE_NOWAIT		0	/* Ready to dispatch. */
#define	G_QUEUE_WAITREQ		1	/* Waiting for a completion. */
#define G_QUEUE_WAITING		2	/* Waiting for a new request. */

/*
 * Per process (thread) queue structure.  Each process (thread) in the
 * system that accesses the disk managed by an instance of this scheduler
 * has an associated queue.
 */
struct gs_rr_queue {
	int		q_refs;
	int		q_status;
	u_long		q_key;
	struct proc	*q_proc;

	struct bio_queue_head q_bioq;
	unsigned int	q_service;
	unsigned int	q_budget;

	uint64_t	q_slice_end;

	LIST_ENTRY(gs_rr_queue) q_hash;
	TAILQ_ENTRY(gs_rr_queue) q_tailq;
};

/* List types. */
TAILQ_HEAD(gs_rr_tailq, gs_rr_queue);
LIST_HEAD(gs_hash, gs_rr_queue);

/* Size of the per-device hash table storing threads. */
#define	G_RR_HASH_SIZE		32

/* Default slice for RR between queues. */
#define	G_RR_DEFAULT_BUDGET	0x00800000

/*
 * Per device descriptor.  It holds the RR list of queues accessing
 * the disk.
 */
struct gs_rr_softc {
	struct disk	*sc_disk;

	struct gs_rr_queue *sc_active;
	struct gs_rr_tailq sc_rr_tailq;

	struct gs_hash	*sc_hash;
	u_long		sc_hash_mask;

	struct callout	sc_wait;
};

static inline u_long
gs_rr_key(struct thread *tp)
{

	return (tp != NULL ? tp->td_tid : 0);
}

/* Return the hash chain for the given key. */
static inline struct gs_hash *
gs_rr_hash(struct gs_rr_softc *sc, u_long key)
{

	return (&sc->sc_hash[key & sc->sc_hash_mask]);
}

/*
 * Get a reference to the queue that holds requests for tp, allocating
 * it if necessary.
 */
static struct gs_rr_queue *
gs_rr_queue_get(struct gs_rr_softc *sc, struct thread *tp)
{
	struct gs_hash *bucket;
	struct gs_rr_queue *qp, *new_qp;
	u_long key;

	key = gs_rr_key(tp);
	new_qp = NULL;
	bucket = gs_rr_hash(sc, key);
retry:
	LIST_FOREACH(qp, bucket, q_hash) {
		if (qp->q_key == key) {
			qp->q_refs++;
			if (new_qp != NULL) {
				/*
				 * A race occurred, someone else allocated
				 * the queue.  Use it.
				 */
				g_free(new_qp);
			}
			return (qp);
		}
	}

	if (new_qp == NULL) {
		/*
		 * We want allocations that not fail, so release the
		 * d_sched_lock lock and redo the lookup after the
		 * allocation.
		 */
		mtx_unlock(&sc->sc_disk->d_sched_lock);
		new_qp = g_malloc(sizeof *qp, M_WAITOK | M_ZERO);
		mtx_lock(&sc->sc_disk->d_sched_lock);
		goto retry;
	}

	/* One for the hash table, one for the caller. */
	new_qp->q_refs = 2;

	new_qp->q_key = key;
	new_qp->q_proc = tp->td_proc;
	bioq_init(&new_qp->q_bioq);
	new_qp->q_budget = G_RR_DEFAULT_BUDGET;
	LIST_INSERT_HEAD(bucket, new_qp, q_hash);

	return (new_qp);
}

/*
 * Release a reference to the queue.
 */
static void
gs_rr_queue_put(struct gs_rr_queue *qp)
{

	if (--qp->q_refs > 0)
		return;

	LIST_REMOVE(qp, q_hash);
	KASSERT(bioq_first(&qp->q_bioq) == NULL, ("released nonempty queue"));

	g_free(qp);
}

static void *
gs_rr_init(struct disk *dp)
{
	struct gs_rr_softc *sc;

	sc = g_malloc(sizeof *sc, M_WAITOK | M_ZERO);
	sc->sc_disk = dp;
	TAILQ_INIT(&sc->sc_rr_tailq);
	sc->sc_hash = hashinit(G_RR_HASH_SIZE, M_GEOM, &sc->sc_hash_mask);
	callout_init(&sc->sc_wait, CALLOUT_MPSAFE);

	return (sc);
}

static void
gs_rr_fini(void *data)
{
	struct gs_rr_softc *sc;
	struct gs_rr_queue *qp, *qp2;
	int i;

	sc = data;
	callout_drain(&sc->sc_wait);
	KASSERT(sc->sc_active == NULL, ("still a queue under service"));
	KASSERT(TAILQ_EMPTY(&sc->sc_rr_tailq), ("still scheduled queues"));
	for (i = 0; i < G_RR_HASH_SIZE; i++) {
		LIST_FOREACH_SAFE(qp, &sc->sc_hash[i], q_hash, qp2) {
			LIST_REMOVE(qp, q_hash);
			gs_rr_queue_put(qp);
		}
	}
	hashdestroy(sc->sc_hash, M_GEOM, sc->sc_hash_mask);
	g_free(sc);
}

/*
 * Activate a queue, inserting it into the RR list and preparing it
 * to be served.
 */
static inline void
gs_rr_activate(struct gs_rr_softc *sc, struct gs_rr_queue *qp)
{

	qp->q_service = 0;
	TAILQ_INSERT_TAIL(&sc->sc_rr_tailq, qp, q_tailq);
}

static void
gs_rr_start(void *data, struct bio *bp)
{
	struct gs_rr_softc *sc;
	struct gs_rr_queue *qp;

	sc = data;
	/* Get the queue for the thread that issued the request. */
	qp = gs_rr_queue_get(sc, bp->bio_thread);
	if (bioq_first(&qp->q_bioq) == NULL) {
		/* We're inserting into an empty queue... */
		if (qp == sc->sc_active) {
			/* ... this is a request we were anticipating. */
			callout_stop(&sc->sc_wait);
		} else {
			/*
			 * ... this is the first request, we need to
			 * activate the queue.  Take a reference for
			 * being on the active list.
			 */
			qp->q_refs++;
			gs_rr_activate(sc, qp);
		}
	}

	qp->q_status = G_QUEUE_NOWAIT;

	/*
	 * Each request holds a reference to the queue containing it:
	 * inherit the "caller" one here.
	 */
	bp->bio_caller1 = qp;
	bioq_disksort(&qp->q_bioq, bp);
}

static struct bio *
gs_rr_next(void *data, int force)
{
	struct gs_rr_softc *sc;
	struct gs_rr_queue *qp;
	struct bio *bp, *next;

	sc = data;
	/* Try with the queue under service first. */
	qp = sc->sc_active;
	if (qp != NULL && force != 0 && bioq_first(&qp->q_bioq) == NULL) {
		/*
		 * The current queue is being anticipated, but we're asked
		 * a forced dispatch...
		 */
		callout_stop(&sc->sc_wait);
		gs_rr_queue_put(qp);
		qp = NULL;
		sc->sc_active = NULL;
	}

	if (qp == NULL) {
		/* No queue under service, look for the first in RR order. */
		qp = TAILQ_FIRST(&sc->sc_rr_tailq);
		if (qp == NULL) {
			/* No queue at all, just return. */
			return (NULL);
		}

		/*
		 * Select the new queue for service.  The active list
		 * reference is kept until the queue is either sc_active
		 * or on the list.
		 */
		TAILQ_REMOVE(&sc->sc_rr_tailq, qp, q_tailq);
		sc->sc_active = qp;
	} else if (force == 0 && qp->q_status != G_QUEUE_NOWAIT) {
		/* Queue is anticipating, stop dispatching. */
		return (NULL);
	}

	if (qp->q_service == 0) {
		/*
		 * Doing this we charge the first seek to the queue;
		 * even if this is not correct, it should approximate
		 * the correct behavior.
		 */
		qp->q_slice_end = ticks + G_RR_SLICE_EXPIRE;
	}

	bp = bioq_takefirst(&qp->q_bioq);
	qp->q_service += bp->bio_length;
	next = bioq_first(&qp->q_bioq);
 	if (qp->q_service > qp->q_budget ||
	    (int64_t)(ticks - qp->q_slice_end) >= 0) {
		/* Queue exhausted its budget or timed out. */
		sc->sc_active = NULL;
		if (next != NULL) {
			/* If it has more requests requeue it. */
			qp->q_status = G_QUEUE_NOWAIT;
			gs_rr_activate(sc, qp);
		} else {
			/* No more active. */
			gs_rr_queue_put(qp);
		}
	} else if (next == NULL) {
		/*
		 * There is budget left, but this was the last request,
		 * start anticipating.
		 */
		qp->q_status = G_QUEUE_WAITREQ;
	}

	return (bp);
}

/*
 * Callout executed when a queue times out waiting for a new request.
 */
static void
gs_rr_wait_timeout(void *data)
{
	struct gs_rr_softc *sc;
	struct gs_rr_queue *qp;
	struct disk *dp;

	sc = data;
	dp = sc->sc_disk;

	mtx_lock(&dp->d_sched_lock);
	qp = sc->sc_active;
	/*
	 * We can race with a start() or a switch of the active
	 * queue, so check if qp is valid...
	 */
	if (qp != NULL) {
		sc->sc_active = NULL;

		/* Put the sc_active ref. */
		gs_rr_queue_put(qp);
	}
	mtx_unlock(&dp->d_sched_lock);

	/* Restart queueing from the driver. */
	dp->d_kick(dp);
}

static int
gs_rr_done(void *data, struct bio *bp)
{
	struct gs_rr_softc *sc;
	struct gs_rr_queue *qp;
	int ret;

	ret = 0;
	sc = data;
	qp = bp->bio_caller1;
	if (qp == sc->sc_active && qp->q_status == G_QUEUE_WAITREQ) {
		/* The queue is trying anticipation, start the timer. */
		qp->q_status = G_QUEUE_WAITING;
		callout_reset(&sc->sc_wait, G_RR_WAIT_EXPIRE,
		    gs_rr_wait_timeout, sc);
	} else
		ret = 1;

	/* Put the request ref to the queue. */
	gs_rr_queue_put(qp);

	return (ret);
}

static struct g_sched gs_rr = {
	.gs_name = "gs_rr",
	.gs_init = gs_rr_init,
	.gs_fini = gs_rr_fini,
	.gs_start = gs_rr_start,
	.gs_next = gs_rr_next,
	.gs_done = gs_rr_done,
};

DECLARE_GSCHED_MODULE(rr, &gs_rr);
