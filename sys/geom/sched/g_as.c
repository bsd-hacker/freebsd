/*-
 * Copyright (c) 2007 Fabio Checconi <fabio@FreeBSD.org>
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
 * This code implements a GEOM-based anticipatory disk scheduler.
 * This version does not track process state or behaviour and it is
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

#include <geom/geom.h>
#include <geom/sched/g_gsched.h>

/*
 * Status values for AS.
 */
#define	G_AS_NOWAIT		0	/* Not wating at all. */
#define	G_AS_WAITREQ		1	/* Waiting a request to complete. */
#define	G_AS_WAITING		2	/* Waiting a new request. */

struct g_as_softc {
	struct g_geom		*sc_geom;
	u_long			sc_curkey;
	int			sc_status;
	long			sc_batch;
	int			sc_wait_ticks;
	int			sc_max_batch;

	struct callout		sc_wait;
	struct bio_queue_head	sc_bioq;
};

/*
 * Dispatch the first queued request, and update the status
 * according to the dispatched request.
 * This is called as a result of a start, on a timeout, or on
 * a completion event.
 */
static void
g_as_dispatch(struct g_as_softc *sc)
{
	struct bio *bio;

	/*
	 * Serve the requests at the head of the queue, if any,
	 * and decide whether or not to do anticipatory scheduling
	 * for the next round. We anticipate if this request is from
	 * a new client or the current client has not yet exhausted
	 * its budget. Otherwise, we will serve the next request
	 * immediately.
	 */

	bio = bioq_takefirst(&sc->sc_bioq);
	if (bio == NULL) {
		/* stray call or timeout */
		sc->sc_curkey = 0;
		sc->sc_batch = 0;
		sc->sc_status = G_AS_NOWAIT;
	} else {
		u_long head_key = g_sched_classify(bio);

		/* pass down the current request */
		g_io_request(bio, LIST_FIRST(&sc->sc_geom->consumer));

		/*
		 * Now decide what to do next:
		 * reset budget if client has changed,
		 * store the identity of the current client,
		 * and anticipate if and only if the current
		 * client is below its allowed budget.
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
	}
}

static void
g_as_wait_timeout(void *data)
{
	struct g_as_softc *sc = data;

	g_sched_lock(sc->sc_geom);
	/*
	 * We were waiting for a new request for curthread, it did
	 * not come, just dispatch the next one.
	 */
	if (sc->sc_status == G_AS_WAITING)
		g_as_dispatch(sc);
	g_sched_unlock(sc->sc_geom);
}

/*
 * This function is called when I have a real disk I/O request coming
 * from a client (only for schedulable requests).
 * Queue the request and possibly dispatch it. If not dispatched now,
 * surely there a timeout or a completion event that will keep things
 * running.
 */
static void
g_as_start(void *data, struct bio *bio)
{
	struct g_as_softc *sc = data;

	/*
	 * This is an approximated implementation: we do an immediate
	 * dispatch if the current request is coming from the same client
	 * who was served last (who was the "privileged" one in terms
	 * of the scheduling policy). However, the dispatch may actually
	 * serve a different request if the incoming request is not
	 * sequential (hence it would cause a seek anyways).
	 * For this reason, we do an unconditional disksort, and
	 * then decide to dispatch or wait using the identity
	 * of the client issuing the request.
	 */
	bioq_disksort(&sc->sc_bioq, bio);

	if (sc->sc_status == G_AS_NOWAIT ||
	    g_sched_classify(bio) == sc->sc_curkey) {
		callout_stop(&sc->sc_wait);
		g_as_dispatch(sc);
	}
}

/*
 * callback when a request is complete.
 */
static void
g_as_done(void *data, struct bio *bio)
{
	struct g_as_softc *sc = data;

	if (sc->sc_status == G_AS_WAITREQ) {
		sc->sc_status = G_AS_WAITING; /* have pending timer */
		callout_reset(&sc->sc_wait, sc->sc_wait_ticks,
		    g_as_wait_timeout, sc);
	} else {
		/*
		 * Since we don't have to wait anything just dispatch
		 * the next request.
		 */
		g_as_dispatch(sc);
	}
}

/*
 * Geom glue. When a new geom is attached, allocate and init a
 * descriptor. We use a callout queue for timeouts, and a bioq
 * to store pending requests.
 */
static void *
g_as_init(struct g_geom *geom)
{
	struct g_as_softc *sc;

	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
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
	    ("Still requests pending."));
	callout_drain(&sc->sc_wait);

	g_free(sc);
}

static struct g_gsched g_as = {
	.gs_name = "as",
	.gs_init = g_as_init,
	.gs_fini = g_as_fini,
	.gs_start = g_as_start,
	.gs_done = g_as_done,
};

DECLARE_GSCHED_MODULE(as, &g_as);
