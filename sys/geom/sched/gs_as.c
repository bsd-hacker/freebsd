/*-
 * Copyright (c) 2008 Fabio Checconi <fabio@FreeBSD.org>
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
#include <geom/geom_sched.h>

/*
 * Status values for AS.
 */
#define	G_AS_NOWAIT		0	/* Not wating at all. */
#define	G_AS_WAITREQ		1	/* Waiting a request to complete. */
#define	G_AS_WAITING		2	/* Waiting a new request. */

struct gs_as_softc {
	struct disk		*sc_disk;

	u_long			sc_curkey;
	int			sc_status;
	long			sc_batch;

	struct callout		sc_wait;
	struct bio_queue_head	sc_bioq;
};

#define	G_AS_WAIT_EXPIRE	(hz/200 > 0 ? hz/200 : 2) 
#define	G_AS_MAX_BATCH		0x00800000

/*
 * Dispatch the first queued request.  Here we also update the status
 * according to the dispatched request.
 */
static struct bio *
gs_as_next(void *data, int force)
{
	struct gs_as_softc *sc;
	struct bio *bio;

	sc = data;

	if (sc->sc_status != G_AS_NOWAIT && force == 0)
		return (NULL);

	/*
	 * Batching means just don't serve too many requests waiting
	 * for sequential ones, it is not really coupled with the
	 * threads being served.  Its only purpose is to let not the
	 * scheduler starve other threads while an aggressive one
	 * is making continuously new requests.
	 */
	sc->sc_curkey = NULL;

	bio = bioq_takefirst(&sc->sc_bioq);
	if (bio != NULL) {
		sc->sc_batch += bio->bio_length;
		if (sc->sc_batch > G_AS_MAX_BATCH) {
			/*
			 * Too many requests served here, don't wait
			 * for the next.
			 */
			sc->sc_batch = 0;
			sc->sc_status = G_AS_NOWAIT;
		} else if (force == 0) {
			/*
			 * When this request will be served we'll wait
			 * for a new one from the same thread.
			 * Of course we are anticipating everything
			 * here, even writes, asynchronous requests and
			 * seeky processes, but this is only a prototype.
			 */
			sc->sc_status = G_AS_WAITREQ;
		}
	}

	if (bio == NULL || force != 0)
		sc->sc_status = G_AS_NOWAIT;

	G_SCHED_DEBUG(2, "gs_as: dispatch %p", bio);

	return (bio);
}

static void
gs_as_wait_timeout(void *data)
{
	struct gs_as_softc *sc = data;
	struct disk *dp = sc->sc_disk;

	mtx_lock(&dp->d_sched_lock);
	/*
	 * We were waiting for a new request for curkey, it did
	 * not come, just dispatch the next one.
	 */
	if (sc->sc_status == G_AS_WAITING)
		sc->sc_status = G_AS_NOWAIT;
	mtx_unlock(&dp->d_sched_lock);

	G_SCHED_DEBUG(2, "gs_as: timed out, kicking the driver");

	dp->d_kick(dp);
}

static void
gs_as_start(void *data, struct bio *bio)
{
	struct gs_as_softc *sc = data;

	bio->bio_caller1 = bio;
	bioq_disksort(&sc->sc_bioq, bio);

	/*
	 * If the request being submitted is the one we were waiting for
	 * stop the timer and dispatch it, otherwise do nothing.
	 */
	if (sc->sc_status == G_AS_NOWAIT ||
	    g_sched_classify(bio) == sc->sc_curkey) {
		callout_stop(&sc->sc_wait);
		sc->sc_status = G_AS_NOWAIT;
	}
}

static int
gs_as_done(void *data, struct bio *bio)
{
	struct gs_as_softc *sc = data;
	struct bio *next;

	G_SCHED_DEBUG(2, "gs_as: done %p", bio);

	if (sc->sc_status == G_AS_WAITREQ) {
		next = bioq_first(&sc->sc_bioq);
		if (next != NULL &&
		    g_sched_classify(next) == g_sched_classify(bio)) {
			/*
			 * Don't wait if the current thread already
			 * has pending requests.  This is not complete,
			 * since its next bio may not be sequential,
			 * BTW all this thing is not complete...
			 */
			sc->sc_status = G_AS_NOWAIT;

			return (1);
		}

		/* Start waiting for a new request from curthread. */
		sc->sc_curkey = g_sched_classify(bio);
		sc->sc_status = G_AS_WAITING;
		callout_reset(&sc->sc_wait, G_AS_WAIT_EXPIRE,
		    gs_as_wait_timeout, sc);
		G_SCHED_DEBUG(2, "gs_as: waiting for %lu", sc->sc_curkey);

		return (0);
	}

	return (1);
}

static void *
gs_as_init(struct disk *dp)
{
	struct gs_as_softc *sc;

	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
	sc->sc_disk = dp;
	sc->sc_curkey = 0;
	sc->sc_status = G_AS_NOWAIT;

	callout_init(&sc->sc_wait, CALLOUT_MPSAFE);
	bioq_init(&sc->sc_bioq);

	return sc;
}

static void
gs_as_fini(void *data)
{
	struct gs_as_softc *sc = data;

	KASSERT(bioq_first(&sc->sc_bioq) == NULL,
	    ("Still requests pending."));
	callout_drain(&sc->sc_wait);

	g_free(sc);
}

static struct g_sched gs_as = {
	.gs_name = "gs_as",
	.gs_init = gs_as_init,
	.gs_fini = gs_as_fini,
	.gs_start = gs_as_start,
	.gs_next = gs_as_next,
	.gs_done = gs_as_done,
};

DECLARE_GSCHED_MODULE(as, &gs_as);
