/*-
 * CAM IO Scheduler Interface
 *
 * Copyright (c) 2015 Netflix, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_iosched.h>

static MALLOC_DEFINE(M_CAMSCHED, "CAM I/O Scheduler", "CAM I/O Scheduler buffers");

/*
 * Default I/O scheduler for FreeBSD. This implementation is just a thin-vineer
 * over the bioq_* interface, with notions of separate calls for normal I/O and
 * for trims.
 */

struct cam_iosched_softc
{
	struct bio_queue_head bio_queue;
	struct bio_queue_head trim_queue;
				/* scheduler flags < 16, user flags >= 16 */
	uint32_t	flags;
	int	 sort_io_queue;
};

			/* Trim or similar currently pending completion */
#define CAM_IOSCHED_FLAG_TRIM_ACTIVE	1

			/* Periph drivers set these flags to indicate work */
#define CAM_IOSCHED_FLAG_WORK_FLAGS	((0xffffu) << 16)

static inline int
cam_iosched_has_flagged_work(struct cam_iosched_softc *isc)
{
	return !!(isc->flags & CAM_IOSCHED_FLAG_WORK_FLAGS);
}

static inline int
cam_iosched_has_io(struct cam_iosched_softc *isc)
{
	return bioq_first(&(isc)->bio_queue) != NULL;
}

static inline int
cam_iosched_has_more_trim(struct cam_iosched_softc *isc)
{
	return !(isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) &&
	    bioq_first(&isc->trim_queue);
}

#define cam_iosched_sort_queue(isc)	((isc)->sort_io_queue >= 0 ?	\
    (isc)->sort_io_queue : cam_sort_io_queues)


static inline int
cam_iosched_has_work(struct cam_iosched_softc *isc)
{
	return cam_iosched_has_io(isc) ||
		cam_iosched_has_more_trim(isc) ||
		cam_iosched_has_flagged_work(isc);
}

/*
 * Allocate the iosched structure. This also insulates callers from knowing
 * sizeof struct cam_iosched_softc.
 */
int
cam_iosched_init(struct cam_iosched_softc **iscp)
{

	*iscp = malloc(sizeof(**iscp), M_CAMSCHED, M_NOWAIT | M_ZERO);
	if (*iscp == NULL)
		return ENOMEM;
	(*iscp)->sort_io_queue = -1;
	bioq_init(&(*iscp)->bio_queue);
	bioq_init(&(*iscp)->trim_queue);

	return 0;
}

/*
 * Reclaim all used resources. This assumes that other folks have
 * drained the requests in the hardware. Maybe an unwise assumption.
 */
void
cam_iosched_fini(struct cam_iosched_softc *isc)
{
	cam_iosched_flush(isc, NULL, ENXIO);
	free(isc, M_CAMSCHED);
}

/*
 * After we're sure we're attaching a device, go ahead and add
 * hooks for any sysctl we may wish to honor.
 */
void cam_iosched_sysctl_init(struct cam_iosched_softc *isc,
    struct sysctl_ctx_list *ctx, struct sysctl_oid *node)
{
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
		OID_AUTO, "sort_io_queue", CTLFLAG_RW | CTLFLAG_MPSAFE,
		&isc->sort_io_queue, 0,
		"Sort IO queue to try and optimise disk access patterns");
}

/*
 * Flush outstanding I/O. Consumers of this library don't know all the
 * queues we may keep, so this allows all I/O to be flushed in one
 * convenient call.
 */
void
cam_iosched_flush(struct cam_iosched_softc *isc, struct devstat *stp, int err)
{
	bioq_flush(&isc->bio_queue, stp, err);
	bioq_flush(&isc->trim_queue, stp, err);
}

/*
 * Put back a trim that you weren't able to actually schedule this time.
 */
void
cam_iosched_put_back_trim(struct cam_iosched_softc *isc, struct bio *bp)
{
	bioq_insert_head(&isc->trim_queue, bp);
}

/*
 * gets the next trim from the trim queue.
 *
 * Assumes we're called with the periph lock held.  It removes this
 * trim from the queue and the device must explicitly reinstert it
 * should the need arise.
 */
struct bio *
cam_iosched_next_trim(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	bp  = bioq_first(&isc->trim_queue);
	if (bp == NULL)
		return NULL;
	bioq_remove(&isc->trim_queue, bp);
	return bp;
}

/*
 * gets the an available trim from the trim queue, if there's no trim
 * already pending. It removes this trim from the queue and the device
 * must explicitly reinstert it should the need arise.
 *
 * Assumes we're called with the periph lock held.
 */
struct bio *
cam_iosched_get_trim(struct cam_iosched_softc *isc)
{

	if (!cam_iosched_has_more_trim(isc))
		return NULL;

	return cam_iosched_next_trim(isc);
}

/*
 * Determine what the next bit of work to do is for the periph. The
 * default implementation looks to see if we have trims to do, but no
 * trims outstanding. If so, we do that. Otherwise we see if we have
 * other work. If we do, then we do that. Otherwise why were we called?
 */
struct bio *
cam_iosched_next_bio(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	/*
	 * First, see if we have a trim that can be scheduled. We can only send one
	 * at a time down, so this takes that into account.
	 */
	if ((bp = cam_iosched_get_trim(isc)) != NULL)
		return bp;

	/*
	 * next, see if there's a normal I/O waiting. If so return that.
	 */
	if ((bp = bioq_first(&isc->bio_queue)) != NULL) {
		bioq_remove(&isc->bio_queue, bp);
		return bp;
	}

	/*
	 * Otherwise, we got nada, so return that
	 */
	return NULL;
}
	
/*
 * Driver has been given some work to do by the block layer. Tell the
 * scheduler about it and have it queue the work up. The scheduler module
 * will then return the currently most useful bit of work later, possibly
 * deferring work for various reasons.
 */
void
cam_iosched_queue_work(struct cam_iosched_softc *isc, struct bio *bp)
{

	/*
	 * Put all trims on the trim queue sorted, since we know
	 * that the collapsing code requires this. Otherwise put
	 * the work on the bio queue.
	 */
	if (bp->bio_cmd == BIO_DELETE)
		bioq_disksort(&isc->trim_queue, bp);
	else if (cam_iosched_sort_queue(isc))
		bioq_disksort(&isc->bio_queue, bp);
	else
		bioq_insert_tail(&isc->bio_queue, bp);
}

/* 
 * If we have work, get it scheduled. Called with the periph lock held.
 */
void
cam_iosched_schedule(struct cam_iosched_softc *isc, struct cam_periph *periph)
{

	if (cam_iosched_has_work(isc))
		xpt_schedule(periph, CAM_PRIORITY_NORMAL);
}

/*
 * Complete a trim request
 */
void
cam_iosched_trim_done(struct cam_iosched_softc *isc)
{

	isc->flags &= ~CAM_IOSCHED_FLAG_TRIM_ACTIVE;
}

/*
 * Complete a bio. Called before we release the ccb with xpt_release_ccb so we
 * might use notes in the ccb for statistics.
 */
int
cam_iosched_bio_complete(struct cam_iosched_softc *isc, struct bio *bp, union ccb *done_ccb)
{
	return 0;
}

/*
 * Tell the io scheduler that you've pushed a trim down into the sim.
 * xxx better place for this?
 */
void
cam_iosched_submit_trim(struct cam_iosched_softc *isc)
{

	isc->flags |= CAM_IOSCHED_FLAG_TRIM_ACTIVE;
}

/*
 * Change the sorting policy hint for I/O transactions for this device.
 */
void
cam_iosched_set_sort_queue(struct cam_iosched_softc *isc, int val)
{

	isc->sort_io_queue = val;
}

int
cam_iosched_has_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	return isc->flags & flags;
}

void
cam_iosched_set_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	isc->flags |= flags;
}

void
cam_iosched_clr_work_flags(struct cam_iosched_softc *isc, uint32_t flags)
{
	isc->flags &= ~flags;
}
