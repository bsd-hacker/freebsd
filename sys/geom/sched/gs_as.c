/*-
 * Copyright (c) 2009 Fabio Checconi <fabio@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This code implements the algorithm for Anticipatory disk Scheduler (AS).
 * This version does not track process state or behaviour, and is
 * just a proof of concept to show how non work-conserving policies
 * can be implemented within this framework.
 */ 

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>

#include "gs_scheduler.h"

/*
 * Status values for AS.
 */
enum g_as_status {
	G_AS_NOWAIT =	0,	/* Not waiting at all. */
	G_AS_WAITREQ,		/* Waiting a request to complete. */
	G_AS_WAITING		/* Waiting a new request. */
};

struct g_as_softc {
	struct g_geom		*sc_geom;
	u_long			sc_curkey;
	enum g_as_status	sc_status;
	long			sc_batch;
	int			sc_wait_ticks;
	int			sc_max_batch;

	struct callout		sc_wait;
	struct bio_queue_head	sc_bioq;
};

/*
 * Return the first queued request, and update the status
 * according to the dispatched request.
 * This is called as a result of a start, on a timeout, or on
 * a completion event, by g_sched_dispatch().
 */
static struct bio *
g_as_next(void *data)
{
	struct g_as_softc *sc = data;
	struct bio *bio;
	u_long head_key;

	if (sc->sc_status != G_AS_NOWAIT)
		return NULL;

	/*
	 * Serve the requests at the head of the queue, if any, and
	 * decide whether or not to do anticipatory scheduling for
	 * the next round.
	 * We do anticipation if this request is from a new client,
	 * or if the current client has not yet exhausted its budget.
	 * Otherwise, we will serve the next request immediately.
	 */

	bio = bioq_takefirst(&sc->sc_bioq);
	if (bio == NULL) {
		/* stray timeout or call, reset parameters */
		sc->sc_curkey = 0;
		sc->sc_batch = 0;
		return NULL;
	}

	head_key = g_sched_classify(bio);

	/*
	 * Update status:
	 * - reset budget if client has changed;
	 * - store the identity of the current client;
	 * - do anticipation if and only if the current
	 *   client is below its allowed budget.
	 */
	if (head_key != sc->sc_curkey)
		sc->sc_batch = 0;
	sc->sc_curkey = head_key;
	if (sc->sc_batch > sc->sc_max_batch) {
		sc->sc_status = G_AS_NOWAIT;
	} else {
		sc->sc_batch += bio->bio_length;
		sc->sc_status = G_AS_WAITREQ;
	}

	return bio;
}

static void
g_as_wait_timeout(void *data)
{
	struct g_as_softc *sc = data;
	struct g_geom *geom = sc->sc_geom;

	g_sched_lock(geom);
	/*
	 * If we timed out waiting for a new request for the current
	 * client, just dispatch whatever we have.
	 * Otherwise ignore the timeout.
	 */
	if (sc->sc_status == G_AS_WAITING) {
		sc->sc_status = G_AS_NOWAIT;
		g_sched_dispatch(geom);
	}
	g_sched_unlock(geom);
}

/*
 * Called when there is a schedulable disk I/O request.
 * Queue the request and possibly dispatch it. If it not
 * dispatched immediately, it is because there is another
 * request waiting for completion, or a pending timeout.
 */
static int
g_as_start(void *data, struct bio *bio)
{
	struct g_as_softc *sc = data;

	/*
	 * This is an approximated implementation of anticipatory
	 * scheduling:
	 * Queue the request sorted by position using disksort,
	 * then do an immediate dispatch if the current request is
	 * from the client we served last (the "privileged" one in
	 * terms of the scheduling policy).
	 *
	 * Note that dispatch will serve the request at the head of the
	 * queue, which may be different from the incoming one if that
	 * is not sequential (hence it would cause a seek anyways).
	 */
	bioq_disksort(&sc->sc_bioq, bio);

	if (sc->sc_status == G_AS_NOWAIT ||
	    g_sched_classify(bio) == sc->sc_curkey)
		callout_stop(&sc->sc_wait);

	return 0;
}

/*
 * Callback from the geom when a request is complete.
 * If we are doing anticipation (sc_status == G_AS_WAITREQ)
 * then start a new timer and record it. Otherwise, dispatch
 * the next request.
 */
static void
g_as_done(void *data, struct bio *bio)
{
	struct g_as_softc *sc = data;

	if (sc->sc_status == G_AS_WAITREQ) {
		sc->sc_status = G_AS_WAITING;
		callout_reset(&sc->sc_wait, sc->sc_wait_ticks,
		    g_as_wait_timeout, sc);
	} else {
		g_sched_dispatch(sc->sc_geom);
	}
}

/*
 * Module glue, called when the module is loaded.
 * Allocate a descriptor and initialize its fields, including the
 * callout queue for timeouts, and a bioq to store pending requests.
 *
 * The fini routine deallocates everything.
 */
static void *
g_as_init(struct g_geom *geom)
{
	struct g_as_softc *sc;

	sc = malloc(sizeof(*sc), M_GEOM_SCHED, M_WAITOK | M_ZERO);
	sc->sc_geom = geom;
	sc->sc_curkey = 0;
	sc->sc_status = G_AS_NOWAIT;
	sc->sc_wait_ticks = (hz >= 400) ? hz/200 : 2;
	sc->sc_max_batch = 0x00800000;	/* 8 MB */

	callout_init(&sc->sc_wait, CALLOUT_MPSAFE);
	bioq_init(&sc->sc_bioq);

	return sc;
}

static void
g_as_fini(void *data)
{
	struct g_as_softc *sc = data;

	/*
	 * geom should guarantee that _fini is only called when there
	 * are no more bio's active (GEOM does not know about the queue,
	 * but it can count existing bio's associated to the geom).
	 */
	KASSERT(bioq_first(&sc->sc_bioq) == NULL,
	    ("Requests still pending."));
	callout_drain(&sc->sc_wait);

	free(sc, M_GEOM_SCHED);
}

static struct g_gsched g_as = {
	.gs_name = "as",
	.gs_init = g_as_init,
	.gs_fini = g_as_fini,
	.gs_start = g_as_start,
	.gs_done = g_as_done,
	.gs_next = g_as_next,
};

DECLARE_GSCHED_MODULE(as, &g_as);
