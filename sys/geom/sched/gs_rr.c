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

/*
 * This code implements a round-robin anticipatory scheduler, with
 * per-client queues.
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
#include <sys/sysctl.h>
#include "gs_scheduler.h"

/* possible states of the scheduler */
enum g_rr_state {
	G_QUEUE_NOWAIT = 0,	/* Ready to dispatch. */
	G_QUEUE_WAITREQ,	/* Waiting for a completion. */
	G_QUEUE_WAITING		/* Waiting for a new request. */
};

struct g_rr_softc;

/*
 * Per client queue structure.  Each client in the system is
 * represented by this structure where we store the client's
 * requests.
 */
struct g_rr_queue {
	int		q_refs;
	enum g_rr_state	q_status;
	u_long		q_key;

	struct g_rr_softc *q_sc;	/* reference to the parent */

	struct bio_queue_head q_bioq;
	unsigned int	q_service;
	unsigned int	q_budget;

	unsigned int	q_wait_ticks;	/* wait time when doing anticipation */
	unsigned int	q_slice_duration; /* slice end in bytes */

	int		q_slice_end;	/* slice end in ticks */

	int		q_expire;	/* when will it expire */
	LIST_ENTRY(g_rr_queue) q_hash;	/* hash table link field */
	TAILQ_ENTRY(g_rr_queue) q_tailq; /* RR list link field */
};

/* List types. */
TAILQ_HEAD(g_rr_tailq, g_rr_queue);
LIST_HEAD(g_hash, g_rr_queue);

/* Size of the per-device hash table storing threads. */
#define	G_RR_HASH_SIZE		32

/* Default slice for RR between queues. */
#define	G_RR_DEFAULT_BUDGET	0x00800000

/*
 * Per device descriptor, holding the Round Robin list of queues
 * accessing the disk, a reference to the geom, the timer
 * and the hash table where we store the existing entries.
 */
struct g_rr_softc {
	struct g_geom	*sc_geom;

	/*
	 * sc_active is the queue we are anticipating for.
	 * it is never in the Round Robin list even if it
	 * has requests queued.
	 */
	struct g_rr_queue *sc_active;
	int		sc_nqueues;	/* number of active queues */
	struct callout	sc_wait;	/* timer for sc_active */
	struct g_rr_tailq sc_rr_tailq;	/* the round-robin list */
	struct g_hash	*sc_hash;
	u_long		sc_hash_mask;

	/*
	 * A queue of pending requests so we can tell the current
	 * position of the disk head. The queue is implemented as a
	 * circular array, dynamically allocated.
	 */
	struct bio	**sc_pending;
	int		sc_pending_max;	/* array size */
	int		sc_pending_first; /* oldest queued request */
	int		sc_in_flight;	/* requests in the driver */

	/* opportunistically flush buckets in g_rr_done */
	int		sc_flush_ticks;	/* next time we want to flush */
	int		sc_flush_bucket;	/* next bucket to flush */
};

/* descriptor for bounded values */
struct x_bound {		
	int	x_min;
	int	x_cur;
	const int	x_max;	/* XXX you are not supposed to change this */
};

/*
 * parameters, config and stats
 */
struct g_rr_params {
	int	expire_secs;		/* expire seconds for queues */
	int	units;			/* how many instances */
	int	queues;			/* total number of queues */
	int	qrefs;			/* total number of refs to queues */

	struct x_bound queue_depth;	/* max nr. of parallel requests */
	struct x_bound wait_ms;		/* wait time in milliseconds */
	struct x_bound slice_ms;	/* slice size in milliseconds */
	struct x_bound slice_kb;	/* slice size in Kb (1024 bytes) */
};

static struct g_rr_params me = {
	.expire_secs =	10,
	.queue_depth =	{ 1,	8,	50},
	.wait_ms =	{ 1, 	5,	30},
	.slice_ms =	{ 1, 	50,	500},
	.slice_kb =	{ 16, 	8192,	65536},
};

SYSCTL_DECL(_kern_geom_sched);
SYSCTL_NODE(_kern_geom_sched, OID_AUTO, rr, CTLFLAG_RW, 0,
    "GEOM_SCHED ROUND ROBIN stuff");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, units, CTLFLAG_RD,
    &me.units, 0, "Scheduler instances");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, queues, CTLFLAG_RD,
    &me.queues, 0, "Total rr queues");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, wait_ms, CTLFLAG_RW,
    &me.wait_ms.x_cur, 0, "Wait time milliseconds");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, slice_ms, CTLFLAG_RW,
    &me.slice_ms.x_cur, 0, "Slice size milliseconds");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, slice_kb, CTLFLAG_RW,
    &me.slice_kb.x_cur, 0, "Slice size Kbytes");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, expire_secs, CTLFLAG_RW,
    &me.expire_secs, 0, "Expire time in seconds");
SYSCTL_UINT(_kern_geom_sched_rr, OID_AUTO, queue_depth, CTLFLAG_RW,
    &me.queue_depth.x_cur, 0, "Maximum simultaneous requests");

/* Return the hash chain for the given key. */
static inline struct g_hash *
g_rr_hash(struct g_rr_softc *sc, u_long key)
{

	return (&sc->sc_hash[key & sc->sc_hash_mask]);
}

/*
 * get a bounded value, optionally convert to a min of t_min ticks
 */
static int
get_bounded(struct x_bound *v, int t_min)
{
	int x;

	x = v->x_cur;
	if (x < v->x_min)
		x = v->x_min;
	else if (x > v->x_max)
		x = v->x_max;
	if (t_min) {
		x = x * hz / 1000;	/* convert to ticks */
		if (x < t_min)
			x = t_min;
	}
	return x;
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
			me.qrefs++;
			qp->q_refs++;
			return (qp);
		}
	}

	qp = malloc(sizeof *qp, M_GEOM_SCHED, M_NOWAIT | M_ZERO);

	if (qp != NULL) {
		me.qrefs += 2;
		qp->q_refs = 2; /* One for hash table, one for caller. */

		qp->q_sc = sc;
		qp->q_key = key;
		bioq_init(&qp->q_bioq);

		/* compute the slice size in bytes */
		qp->q_budget = 1024 * get_bounded(&me.slice_kb, 0);

		/* compute the slice size and wait time in ticks */
		qp->q_slice_duration = get_bounded(&me.slice_ms, 2);
		qp->q_wait_ticks = get_bounded(&me.wait_ms, 2);

		LIST_INSERT_HEAD(bucket, qp, q_hash);
		qp->q_sc->sc_nqueues++;
		me.queues++;
	}

	return (qp);
}

/*
 * Release a reference to the queue.
 */
static void
g_rr_queue_put(struct g_rr_queue *qp)
{

	qp->q_expire = ticks + me.expire_secs * hz;

	if (--qp->q_refs > 0)
		return;

	LIST_REMOVE(qp, q_hash);
	KASSERT(bioq_first(&qp->q_bioq) == NULL,
			("released nonempty queue"));
	qp->q_sc->sc_nqueues--;
	me.queues--;

	free(qp, M_GEOM_SCHED);
}

static inline int
g_rr_queue_expired(struct g_rr_queue *qp)
{

	return (qp->q_service >= qp->q_budget ||
	    ticks - qp->q_slice_end >= 0);
}

/*
 * called on a request arrival, timeout or completion.
 * Try to serve a request among those queued.
 */
static struct bio *
g_rr_next(void *data)
{
	struct g_rr_softc *sc = data;
	struct g_rr_queue *qp;
	struct bio *bp, *next;
	int expired;

	if (sc->sc_in_flight >= get_bounded(&me.queue_depth, 0))
		return NULL;

	/* Try with the queue under service first. */
	qp = sc->sc_active;
	if (qp != NULL && qp->q_status != G_QUEUE_NOWAIT) {
		/* Queue is anticipating, ignore request.
		 * In principle we could check that we are not past
		 * the timeout, but in that case the timeout will
		 * fire immediately afterwards so we don't check.
		 */
		return NULL;
	}

	/* No queue under service, look for the first in RR order. */
	if (qp == NULL)
		qp = TAILQ_FIRST(&sc->sc_rr_tailq);

	/* If no queue at all, just return */
	if (qp == NULL)
		return NULL;

	if (qp != sc->sc_active) {
		/* Select the new queue for service. */
		TAILQ_REMOVE(&sc->sc_rr_tailq, qp, q_tailq);
		sc->sc_active = qp;
	}

	/* set a timeout for the current slice */
	if (qp->q_service == 0)
		qp->q_slice_end = ticks + qp->q_slice_duration;
	bp = bioq_takefirst(&qp->q_bioq);
	qp->q_service += bp->bio_length;
	next = bioq_first(&qp->q_bioq);	/* request remains in the queue */

	/*
	 * Have we have reached our budget in time or bytes ?
	 */
	expired = g_rr_queue_expired(qp);
 	if (!expired && next == NULL && bp->bio_cmd == BIO_READ) {
		/*
		 * There is budget left, but this was the last request,
		 * start anticipating.
		 */
		qp->q_status = G_QUEUE_WAITREQ;
	} else if (next != NULL && expired) {
		/* If it has more requests requeue it. */
		qp->q_status = G_QUEUE_NOWAIT;
		qp->q_service = 0;
		TAILQ_INSERT_TAIL(&sc->sc_rr_tailq, qp, q_tailq);
		sc->sc_active = NULL;
	} else if (next == NULL) {
		/* No more active, release reference. */
		g_rr_queue_put(qp);
		sc->sc_active = NULL;
	}

	sc->sc_in_flight++;

	return bp;
}

/*
 * Called when a real request for disk I/O arrives.
 * Locate the queue associated with the client.
 * If the queue is the one we are anticipating for, reset its timeout;
 * if the queue is not in the round robin list, insert it in the list.
 * On any error, do not queue the request and return -1, the caller
 * will take care of this request.
 */
static int
g_rr_start(void *data, struct bio *bp)
{
	struct g_rr_softc *sc = data;
	struct g_rr_queue *qp;

	/* Get the queue for the thread that issued the request. */
	qp = g_rr_queue_get(sc, g_sched_classify(bp));
	if (qp == NULL)
		return -1; /* allocation failed, tell upstream */

	if (bioq_first(&qp->q_bioq) == NULL) {
		/*
		 * We are inserting into an empty queue; check whether
		 * this is the one for which we are doing anticipation,
		 * in which case stop the timer.
		 * Otherwise insert the queue in the rr list.
		 */
		if (qp == sc->sc_active) {
			callout_stop(&sc->sc_wait);
		} else {
			/*
			 * ... this is the first request, we need to
			 * activate the queue.
			 */
			qp->q_refs++;
			qp->q_service = 0;
			TAILQ_INSERT_TAIL(&sc->sc_rr_tailq, qp, q_tailq);
		}
	}

	qp->q_status = G_QUEUE_NOWAIT;

	/*
	 * Each request holds a reference to the queue containing it:
	 * inherit the "caller" one.
	 */
	bp->bio_caller1 = qp;
	bioq_disksort(&qp->q_bioq, bp);

	return 0;
}

/*
 * Callout executed when a queue times out waiting for a new request.
 */
static void
g_rr_wait_timeout(void *data)
{
	struct g_rr_softc *sc = data;
	struct g_geom *geom = sc->sc_geom;
	struct g_rr_queue *qp;

	g_sched_lock(geom);
	qp = sc->sc_active;
	/*
	 * We can race with a start() or a switch of the active
	 * queue, so check if qp is valid...
	 */
	if (qp != NULL) {
		sc->sc_active = NULL;
		/* release reference to the queue. */
		g_rr_queue_put(qp);
	}
	g_sched_dispatch(geom);
	g_sched_unlock(geom);
}

/*
 * Module glue -- allocate descriptor, initialize the hash table and
 * the callout structure.
 */
static void *
g_rr_init(struct g_geom *geom)
{
	struct g_rr_softc *sc;

	sc = malloc(sizeof *sc, M_GEOM_SCHED, M_WAITOK | M_ZERO);
	sc->sc_pending_max = me.queue_depth.x_max;
	sc->sc_pending =
		malloc(sc->sc_pending_max * sizeof(struct bio *),
			M_GEOM_SCHED, M_WAITOK | M_ZERO);
	sc->sc_geom = geom;
	sc->sc_flush_ticks = ticks;
	TAILQ_INIT(&sc->sc_rr_tailq);
	sc->sc_hash = hashinit(G_RR_HASH_SIZE, M_GEOM_SCHED,
		&sc->sc_hash_mask);
	callout_init(&sc->sc_wait, CALLOUT_MPSAFE);
	me.units++;

	return (sc);
}

/*
 * Module glue -- drain the callout structure, destroy the
 * hash table and its element, and free the descriptor.
 */
static void
g_rr_fini(void *data)
{
	struct g_rr_softc *sc = data;
	struct g_rr_queue *qp, *qp2;
	int i;

	callout_drain(&sc->sc_wait);
	KASSERT(sc->sc_active == NULL, ("still a queue under service"));
	KASSERT(TAILQ_EMPTY(&sc->sc_rr_tailq), ("still scheduled queues"));
	for (i = 0; i < G_RR_HASH_SIZE; i++) {
		LIST_FOREACH_SAFE(qp, &sc->sc_hash[i], q_hash, qp2) {
			g_rr_queue_put(qp);
		}
	}
	hashdestroy(sc->sc_hash, M_GEOM_SCHED, sc->sc_hash_mask);
	me.units--;
	free(sc->sc_pending, M_GEOM_SCHED);
	free(sc, M_GEOM_SCHED);
}

/*
 * Flush a bucket, come back in some time
 */
static void
g_rr_flush(struct g_rr_softc *sc)
{
	struct g_rr_queue *qp, *qp2;
	int i;

	i = sc->sc_flush_bucket++;
	if (sc->sc_flush_bucket >= G_RR_HASH_SIZE)
		sc->sc_flush_bucket = 0;

	for (i = 0; i < G_RR_HASH_SIZE; i++) {
		LIST_FOREACH_SAFE(qp, &sc->sc_hash[i], q_hash, qp2) {
			if (qp->q_refs == 1 && ticks - qp->q_expire > 0)
				g_rr_queue_put(qp);
		}
	}

	sc->sc_flush_ticks = ticks + me.expire_secs * hz;
}

/*
 * called when the request under service terimnates.
 * Take the chance to opportunistically flush stuff.
 */
static void
g_rr_done(void *data, struct bio *bp)
{
	struct g_rr_softc *sc = data;
	struct g_rr_queue *qp;

	sc->sc_in_flight--;

	qp = bp->bio_caller1;
	if (qp == sc->sc_active && qp->q_status == G_QUEUE_WAITREQ) {
		/* The queue is trying anticipation, start the timer. */
		qp->q_status = G_QUEUE_WAITING;
		callout_reset(&sc->sc_wait, qp->q_wait_ticks,
		    g_rr_wait_timeout, sc);
	} else
		g_sched_dispatch(sc->sc_geom);

	/* Release a reference to the queue. */
	g_rr_queue_put(qp);
        if (ticks - sc->sc_flush_ticks > 0)
		g_rr_flush(sc);
}

static struct g_gsched g_rr = {
	.gs_name = "rr",
	.gs_init = g_rr_init,
	.gs_fini = g_rr_fini,
	.gs_start = g_rr_start,
	.gs_done = g_rr_done,
	.gs_next = g_rr_next,
};

DECLARE_GSCHED_MODULE(rr, &g_rr);
