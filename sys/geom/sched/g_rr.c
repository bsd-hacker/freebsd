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
#include <geom/sched/g_gsched.h>

/*
 * Trivial round robin disk scheduler, with per-thread queues, always
 * anticipating requests from the last served thread.
 */

/* Timeout for anticipation. */
#define	G_RR_WAIT_EXPIRE	(hz/200 > 0 ? hz/200 : 2)

#define	G_QUEUE_NOWAIT		0	/* Ready to dispatch. */
#define	G_QUEUE_WAITREQ		1	/* Waiting for a completion. */
#define G_QUEUE_WAITING		2	/* Waiting for a new request. */

/*
 * Per process (thread) queue structure.  Each process (thread) in the
 * system that accesses the disk managed by an instance of this scheduler
 * has an associated queue.
 */
struct g_rr_queue {
	int		q_refs;
	int		q_status;
	u_long		q_key;

	struct bio_queue_head q_bioq;
	unsigned int	q_service;
	unsigned int	q_budget;

	LIST_ENTRY(g_rr_queue) q_hash;
	TAILQ_ENTRY(g_rr_queue) q_tailq;
};

/* List types. */
TAILQ_HEAD(g_rr_tailq, g_rr_queue);
LIST_HEAD(g_hash, g_rr_queue);

/* Size of the per-device hash table storing threads. */
#define	G_RR_HASH_SIZE		32

/* Default slice for RR between queues. */
#define	G_RR_DEFAULT_BUDGET	0x00800000

/*
 * Per device descriptor.  It holds the RR list of queues accessing
 * the disk.
 */
struct g_rr_softc {
	struct g_geom	*sc_geom;

	struct g_rr_queue *sc_active;
	struct g_rr_tailq sc_rr_tailq;

	struct g_hash	*sc_hash;
	u_long		sc_hash_mask;

	struct callout	sc_wait;
};

/* Return the hash chain for the given key. */
static inline struct g_hash *
g_rr_hash(struct g_rr_softc *sc, u_long key)
{

	return (&sc->sc_hash[key & sc->sc_hash_mask]);
}

/*
 * Get a reference to the queue that holds requests for tp, allocating
 * it if necessary.
 */
static struct g_rr_queue *
g_rr_queue_get(struct g_rr_softc *sc, u_long key)
{
	struct g_hash *bucket;
	struct g_rr_queue *qp;

	bucket = g_rr_hash(sc, key);
	LIST_FOREACH(qp, bucket, q_hash) {
		if (qp->q_key == key) {
			qp->q_refs++;
			return (qp);
		}
	}

	qp = g_malloc(sizeof *qp, M_NOWAIT | M_ZERO);

	if (qp != NULL) {
		/* One for the hash table, one for the caller. */
		qp->q_refs = 2;

		qp->q_key = key;
		bioq_init(&qp->q_bioq);
		qp->q_budget = G_RR_DEFAULT_BUDGET;
		LIST_INSERT_HEAD(bucket, qp, q_hash);
	}

	return (qp);
}

/*
 * Release a reference to the queue.
 */
static void
g_rr_queue_put(struct g_rr_queue *qp)
{

	if (--qp->q_refs > 0)
		return;

	LIST_REMOVE(qp, q_hash);
	KASSERT(bioq_first(&qp->q_bioq) == NULL, ("released nonempty queue"));

	g_free(qp);
}

static void *
g_rr_init(struct g_geom *geom)
{
	struct g_rr_softc *sc;

	sc = g_malloc(sizeof *sc, M_WAITOK | M_ZERO);
	sc->sc_geom = geom;
	TAILQ_INIT(&sc->sc_rr_tailq);
	sc->sc_hash = hashinit(G_RR_HASH_SIZE, M_GEOM, &sc->sc_hash_mask);
	callout_init(&sc->sc_wait, CALLOUT_MPSAFE);

	return (sc);
}

static void
g_rr_fini(void *data)
{
	struct g_rr_softc *sc;
	struct g_rr_queue *qp, *qp2;
	int i;

	sc = data;
	callout_drain(&sc->sc_wait);
	KASSERT(sc->sc_active == NULL, ("still a queue under service"));
	KASSERT(TAILQ_EMPTY(&sc->sc_rr_tailq), ("still scheduled queues"));
	for (i = 0; i < G_RR_HASH_SIZE; i++) {
		LIST_FOREACH_SAFE(qp, &sc->sc_hash[i], q_hash, qp2) {
			LIST_REMOVE(qp, q_hash);
			g_rr_queue_put(qp);
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
g_rr_activate(struct g_rr_softc *sc, struct g_rr_queue *qp)
{

	qp->q_service = 0;
	TAILQ_INSERT_TAIL(&sc->sc_rr_tailq, qp, q_tailq);
}

static void
g_rr_dispatch(struct g_rr_softc *sc)
{
	struct g_rr_queue *qp;
	struct bio *bp, *next;

	/* Try with the queue under service first. */
	qp = sc->sc_active;
	if (qp == NULL) {
		/* No queue under service, look for the first in RR order. */
		qp = TAILQ_FIRST(&sc->sc_rr_tailq);
		if (qp == NULL) {
			/* No queue at all, just return. */
			return;
		}
		/* Select the new queue for service. */
		TAILQ_REMOVE(&sc->sc_rr_tailq, qp, q_tailq);
		sc->sc_active = qp;
	} else if (qp->q_status != G_QUEUE_NOWAIT) {
		/* Queue is anticipating, stop dispatching. */
		return;
	}

	bp = bioq_takefirst(&qp->q_bioq);
	qp->q_service += bp->bio_length;
	next = bioq_first(&qp->q_bioq);
 	if (qp->q_service > qp->q_budget) {
		/* Queue exhausted its budget. */
		sc->sc_active = NULL;
		if (next != NULL) {
			/* If it has more requests requeue it. */
			qp->q_status = G_QUEUE_NOWAIT;
			g_rr_activate(sc, qp);
		} else {
			/* No more active. */
			g_rr_queue_put(qp);
		}
	} else if (next == NULL) {
		/*
		 * There is budget left, but this was the last request,
		 * start anticipating.
		 */
		qp->q_status = G_QUEUE_WAITREQ;
	}

	if (bp != NULL)
		g_io_request(bp, LIST_FIRST(&sc->sc_geom->consumer));
}

static void
g_rr_start(void *data, struct bio *bp)
{
	struct g_rr_softc *sc;
	struct g_rr_queue *qp;

	sc = data;
	/* Get the queue for the thread that issued the request. */
	qp = g_rr_queue_get(sc, g_sched_classify(bp));
	if (qp == NULL) {
		g_io_request(bp, LIST_FIRST(&sc->sc_geom->consumer));
		return;
	}

	if (bioq_first(&qp->q_bioq) == NULL) {
		/* We're inserting into an empty queue... */
		if (qp == sc->sc_active) {
			/* ... this is a request we were anticipating. */
			callout_stop(&sc->sc_wait);
		} else {
			/*
			 * ... this is the first request, we need to
			 * activate the queue.
			 */
			qp->q_refs++;
			g_rr_activate(sc, qp);
		}
	}

	qp->q_status = G_QUEUE_NOWAIT;

	/*
	 * Each request holds a reference to the queue containing it:
	 * inherit the "caller" one.
	 */
	bp->bio_caller1 = qp;
	bioq_disksort(&qp->q_bioq, bp);
	g_rr_dispatch(sc);
}

/*
 * Callout executed when a queue times out waiting for a new request.
 */
static void
g_rr_wait_timeout(void *data)
{
	struct g_rr_softc *sc;
	struct g_rr_queue *qp;

	sc = data;

	g_sched_lock(sc->sc_geom);
	qp = sc->sc_active;
	/*
	 * We can race with a start() or a switch of the active
	 * queue, so check if qp is valid...
	 */
	if (qp != NULL) {
		sc->sc_active = NULL;

		/* Put the sc_active ref. */
		g_rr_queue_put(qp);
	}
	g_rr_dispatch(sc);
	g_sched_unlock(sc->sc_geom);
}

static void
g_rr_done(void *data, struct bio *bp)
{
	struct g_rr_softc *sc;
	struct g_rr_queue *qp;

	sc = data;
	qp = bp->bio_caller1;
	if (qp == sc->sc_active && qp->q_status == G_QUEUE_WAITREQ) {
		/* The queue is trying anticipation, start the timer. */
		qp->q_status = G_QUEUE_WAITING;
		callout_reset(&sc->sc_wait, G_RR_WAIT_EXPIRE,
		    g_rr_wait_timeout, sc);
	} else
		g_rr_dispatch(sc);

	/* Put the request ref to the queue. */
	g_rr_queue_put(qp);
}

static struct g_gsched g_rr = {
	.gs_name = "rr",
	.gs_init = g_rr_init,
	.gs_fini = g_rr_fini,
	.gs_start = g_rr_start,
	.gs_done = g_rr_done,
};

DECLARE_GSCHED_MODULE(rr, &g_rr);
