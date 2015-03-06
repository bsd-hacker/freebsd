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

#include "opt_cam.h"
#include "opt_ddb.h"
#define CAM_NETFLIX_IOSCHED 1 /* hack xxx */

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

#include <ddb/ddb.h>

static MALLOC_DEFINE(M_CAMSCHED, "CAM I/O Scheduler",
    "CAM I/O Scheduler buffers");

/*
 * Default I/O scheduler for FreeBSD. This implementation is just a thin-vineer
 * over the bioq_* interface, with notions of separate calls for normal I/O and
 * for trims.
 */

#ifdef CAM_NETFLIX_IOSCHED
#define IOP_MAX_SKIP 50
#define IOP_MAX_TRAINING 500
#define ALPHA_BITS 14                                   /* ~32k events or about the last minute */

SYSCTL_DECL(_kern_cam);
static int do_netflix_iosched = 1;
TUNABLE_INT("kern.cam.do_netflix_iosched", &do_netflix_iosched);
SYSCTL_INT(_kern_cam, OID_AUTO, do_netflix_iosched, CTLFLAG_RD,
    &do_netflix_iosched, 1,
    "Enable Netflix I/O scheduler optimizations.");

int iosched_debug = 0;

struct iop_stats 
{
	sbintime_t      data[IOP_MAX_TRAINING];	/* Data for training period */
	sbintime_t	worst;		/* estimate of worst case latency */
	int		outliers;	/* Number of outlier latency I/Os */
	int		skipping;	/* Skipping I/Os when < IOP_MAX_SKIP */
	int		training;	/* Training when < IOP_MAX_TRAINING */
		/* Exp Moving Average, alpha = 1 / (1 << alpha_bits) */
	sbintime_t      ema;
	sbintime_t      emss;		/* Exp Moving sum of the squares */
	sbintime_t      sd;		/* Last computed sd */
};
#endif

struct cam_iosched_softc
{
	struct bio_queue_head bio_queue;
	struct bio_queue_head trim_queue;
				/* scheduler flags < 16, user flags >= 16 */
	uint32_t	flags;
	int		sort_io_queue;
#ifdef CAM_NETFLIX_IOSCHED
	/* Number of pending transactions */
	int		pending_reads;
	int		pending_writes;
	/* Have at least this many transactions in progress, if possible */
	int		min_reads;
	int		min_writes;
	/* Maximum number of each type of transaction in progress */
	int		max_reads;
	int		max_writes;
	
	int		trims;
	int		reads;
	int		writes;
	int		queued_reads;
	int		queued_writes;
	int		in_reads;
	int		in_writes;
	int		out_reads;
	int		out_writes;

	int		read_bias;
	int		current_read_bias;

	struct bio_queue_head write_queue;
	struct iop_stats read_stats, write_stats, trim_stats;
#endif
};

			/* Trim or similar currently pending completion */
#define CAM_IOSCHED_FLAG_TRIM_ACTIVE	1

			/* Periph drivers set these flags to indicate work */
#define CAM_IOSCHED_FLAG_WORK_FLAGS	((0xffffu) << 16)

static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, int cmd, size_t size);

static inline int
cam_iosched_has_flagged_work(struct cam_iosched_softc *isc)
{
	return !!(isc->flags & CAM_IOSCHED_FLAG_WORK_FLAGS);
}

static inline int
cam_iosched_has_io(struct cam_iosched_softc *isc)
{
#ifdef CAM_NETFLIX_IOSCHED
	if (do_netflix_iosched) {
		int can_write = bioq_first(&isc->write_queue) != NULL &&
		    (isc->max_writes <= 0 ||
			isc->pending_writes < isc->max_writes);
		int can_read = bioq_first(&isc->bio_queue) != NULL &&
		    (isc->max_reads <= 0 ||
			isc->pending_reads < isc->max_reads);
		if (iosched_debug > 2) {
			printf("can write %d: pending_writes %d max_writes %d\n", can_write, isc->pending_writes, isc->max_writes);
			printf("can read %d: pending_reads %d max_reads %d\n", can_read, isc->pending_reads, isc->max_reads);
			printf("Queued reads %d writes %d\n", isc->queued_reads, isc->queued_writes);
		}
		return can_read || can_write;
	}
#endif
	return bioq_first(&isc->bio_queue) != NULL;
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
	if (iosched_debug > 2)
		printf("has work: %d %d %d\n", cam_iosched_has_io(isc),
		    cam_iosched_has_more_trim(isc),
		    cam_iosched_has_flagged_work(isc));

	return cam_iosched_has_io(isc) ||
		cam_iosched_has_more_trim(isc) ||
		cam_iosched_has_flagged_work(isc);
}

static void
iop_stats_init(struct iop_stats *ios)
{
	ios->ema = 0;
	ios->emss = 0;
	ios->sd = 0;
	ios->outliers = 0;
	ios->skipping = 0;
	ios->training = 0;
	ios->outliers = 0;
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
	if (iosched_debug)
		printf("CAM IOSCHEDULER Allocating entry at %p\n", *iscp);
	(*iscp)->sort_io_queue = -1;
	bioq_init(&(*iscp)->bio_queue);
	bioq_init(&(*iscp)->trim_queue);
#ifdef CAM_NETFLIX_IOSCHED
	if (do_netflix_iosched) {
		bioq_init(&(*iscp)->write_queue);
		(*iscp)->pending_reads = 0;
		(*iscp)->pending_writes = 0;
		(*iscp)->min_reads = 1;
		(*iscp)->min_writes = 1;
		(*iscp)->max_reads = 0;
		(*iscp)->max_writes = 0;
		(*iscp)->trims = 0;
		(*iscp)->reads = 0;
		(*iscp)->writes = 0;
		(*iscp)->queued_reads = 0;
		(*iscp)->queued_writes = 0;
		(*iscp)->out_reads = 0;
		(*iscp)->out_writes = 0;
		(*iscp)->in_reads = 0;
		(*iscp)->in_writes = 0;
		(*iscp)->read_bias = 100;
		(*iscp)->current_read_bias = 100;
		iop_stats_init(&(*iscp)->read_stats);
		iop_stats_init(&(*iscp)->write_stats);
		iop_stats_init(&(*iscp)->trim_stats);
	}
#endif

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
#ifdef CAM_NETFLIX_IOSCHED
	if (!do_netflix_iosched)
		return;

	/*
	 * Read stats
	 */
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_ema", CTLFLAG_RD,
	    &isc->read_stats.ema,
	    "Fast Exponentially Weighted Moving Average for reads");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_emss", CTLFLAG_RD,
	    &isc->read_stats.emss,
	    "Fast Exponentially Weighted Moving Sum of Squares for reads");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_sd", CTLFLAG_RD,
	    &isc->read_stats.sd,
	    "Estimated SD for fast read ema");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_bad_latency", CTLFLAG_RD,
	    &isc->read_stats.worst,
	    "read bad latency threshold");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_outliers", CTLFLAG_RD,
	    &isc->read_stats.outliers, 0,
	    "read latency outliers");

	/*
	 * Write stats
	 */
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "write_ema", CTLFLAG_RD,
	    &isc->write_stats.ema,
	    "Fast Exponentially Weighted Moving Average for writes");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "write_emss", CTLFLAG_RD,
	    &isc->write_stats.emss,
	    "Fast Exponentially Weighted Moving Sum of Squares for writes");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "write_sd", CTLFLAG_RD,
	    &isc->write_stats.sd,
	    "Estimated SD for fast write ema");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "write_bad_latency", CTLFLAG_RD,
	    &isc->write_stats.worst,
	    "write bad latency threshold");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "write_outliers", CTLFLAG_RD,
	    &isc->write_stats.outliers, 0,
	    "write latency outliers");

	/*
	 * Trim stats
	 */
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trim_ema", CTLFLAG_RD,
	    &isc->trim_stats.ema,
	    "Fast Exponentially Weighted Moving Average for trims");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trim_emss", CTLFLAG_RD,
	    &isc->trim_stats.emss,
	    "Fast Exponentially Weighted Moving Sum of Squares for trims");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trim_sd", CTLFLAG_RD,
	    &isc->trim_stats.sd,
	    "Estimated SD for fast trim ema");
	SYSCTL_ADD_UQUAD(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trim_bad_latency", CTLFLAG_RD,
	    &isc->trim_stats.worst,
	    "trim bad latency threshold");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trim_outliers", CTLFLAG_RD,
	    &isc->trim_stats.outliers, 0,
	    "trim latency outliers");

	/*
	 * Other misc knobs.
	 */
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "pending_reads", CTLFLAG_RD,
	    &isc->pending_reads, 0,
	    "Instantaneous # of pending reads");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "pending_writes", CTLFLAG_RD,
	    &isc->pending_writes, 0,
	    "Instantaneous # of pending writes");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "trims", CTLFLAG_RD,
	    &isc->trims, 0,
	    "# of trims");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "writes", CTLFLAG_RD,
	    &isc->writes, 0,
	    "# of writes submitted to hardware");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "reads", CTLFLAG_RD,
	    &isc->reads, 0,
	    "# of reads submitted to hardware");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "queued_writes", CTLFLAG_RD,
	    &isc->queued_writes, 0,
	    "# of writes in the queue");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "queued_reads", CTLFLAG_RD,
	    &isc->queued_reads, 0,
	    "# of reads in the queue");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "in_writes", CTLFLAG_RD,
	    &isc->in_writes, 0,
	    "# of writes queued");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "in_reads", CTLFLAG_RD,
	    &isc->in_reads, 0,
	    "# of reads queued");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "out_writes", CTLFLAG_RD,
	    &isc->out_writes, 0,
	    "# of writes completed");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "out_reads", CTLFLAG_RD,
	    &isc->in_reads, 0,
	    "# of reads completed");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "read_bias", CTLFLAG_RW,
	    &isc->read_bias, 100,
	    "How biased towards read should we be");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "min_reads", CTLFLAG_RW,
	    &isc->min_reads, 0,
	    "min reads reserved in the queue");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "min_writes", CTLFLAG_RW,
	    &isc->min_writes, 0,
	    "min writes reserved in the queue");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "max_reads", CTLFLAG_RW,
	    &isc->max_reads, 0,
	    "max reads reserved in the queue");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(node),
	    OID_AUTO, "max_writes", CTLFLAG_RW,
	    &isc->max_writes, 0,
	    "max writes reserved in the queue");
#endif
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
#ifdef CAM_NETFLIX_IOSCHED
	if (do_netflix_iosched)
		bioq_flush(&isc->write_queue, stp, err);
#endif
}

#ifdef CAM_NETFLIX_IOSCHED
static struct bio *
cam_iosched_get_write(struct cam_iosched_softc *isc)
{
	struct bio *bp;

	/*
	 * We control the write rate by controlling how many requests we send
	 * down to the drive at any one time. Fewer requests limits the
	 * effects of both starvation when the requests take a while and write
	 * amplification when each request is causing more than one write to
	 * the NAND media. Limiting the queue depth like this will also limit
	 * the write throughput and give and reads that want to compete to
	 * compete unfairly.
	 */
	if (isc->max_writes > 0 && isc->pending_writes >= isc->max_writes) {
		if (iosched_debug)
			printf("Can't write because pending_writes %d and max_writes %d\n",
			    isc->pending_writes, isc->max_writes);
		return NULL;
	}
	bp = bioq_first(&isc->write_queue);
	if (bp == NULL) {
		if (iosched_debug > 3)
			printf("No writes present in write_queue\n");
		return NULL;
	}
	if (bioq_first(&isc->bio_queue) && isc->current_read_bias) {
		if (iosched_debug)
			printf("Reads present and current_read_bias is %d queued writes %d queued reads %d\n", isc->current_read_bias, isc->queued_writes, isc->queued_reads);
		isc->current_read_bias--;
		return NULL;
	}
	isc->current_read_bias = isc->read_bias;
	bioq_remove(&isc->write_queue, bp);
	if (bp->bio_cmd == BIO_WRITE) {
		isc->queued_writes--;
		isc->writes++;
		isc->pending_writes++;
	}
	if (iosched_debug > 9)
		printf("HWQ : %p %#x\n", bp, bp->bio_cmd);
	return bp;
}
#endif

/*
 * Put back a trim that you weren't able to actually schedule this time.
 */
void
cam_iosched_put_back_trim(struct cam_iosched_softc *isc, struct bio *bp)
{
	bioq_insert_head(&isc->trim_queue, bp);
	isc->trims--;
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
	isc->trims++;
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
	 * See if we have a trim that can be scheduled. We can only send one
	 * at a time down, so this takes that into account.
	 *
	 * XXX newer TRIM commands are queueable. Revisit this when we
	 * implement them.
	 */
	if ((bp = cam_iosched_get_trim(isc)) != NULL)
		return bp;

#ifdef CAM_NETFLIX_IOSCHED
	/*
	 * See if we have any pending writes, and room in the queue for them,
	 * and if so, those are next.
	 */
	if (do_netflix_iosched) {
		if ((bp = cam_iosched_get_write(isc)) != NULL)
			return bp;
	}
#endif

	/*
	 * next, see if there's other, normal I/O waiting. If so return that.
	 */
#ifdef CAM_NETFLIX_IOSCHED
	/*
	 * For the netflix scheduler, bio_queue is only for reads, so enforce
	 * the limits here.
	 */
	if (do_netflix_iosched) {
		if (isc->max_reads > 0 && isc->pending_reads >= isc->max_reads)
			return NULL;
	}
#endif
	if ((bp = bioq_first(&isc->bio_queue)) != NULL) {
		bioq_remove(&isc->bio_queue, bp);
#ifdef CAM_NETFLIX_IOSCHED
		if (do_netflix_iosched) {
			if (bp->bio_cmd == BIO_READ) {
				isc->queued_reads--;
				isc->reads++;
				isc->pending_reads++;
			} else
				printf("Found bio_cmd = %#x\n", bp->bio_cmd);
		}
#endif
		if (iosched_debug > 9)
			printf("HWQ : %p %#x\n", bp, bp->bio_cmd);
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
	if (bp->bio_cmd == BIO_DELETE) {
		bioq_disksort(&isc->trim_queue, bp);
	}
#ifdef CAM_NETFLIX_IOSCHED
	else if (do_netflix_iosched &&
	    (bp->bio_cmd == BIO_WRITE || bp->bio_cmd == BIO_FLUSH)) {
		if (cam_iosched_sort_queue(isc))
			bioq_disksort(&isc->write_queue, bp);
		else
			bioq_insert_tail(&isc->write_queue, bp);
		if (iosched_debug > 9)
			printf("Qw  : %p %#x\n", bp, bp->bio_cmd);
		if (bp->bio_cmd == BIO_WRITE) {
			isc->in_writes++;
			isc->queued_writes++;
		}
	}
#endif
	else {
		if (cam_iosched_sort_queue(isc))
			bioq_disksort(&isc->bio_queue, bp);
		else
			bioq_insert_tail(&isc->bio_queue, bp);
		if (iosched_debug > 9)
			printf("Qr  : %p %#x\n", bp, bp->bio_cmd);
		if (bp->bio_cmd == BIO_READ) {
			isc->in_reads++;
			isc->queued_reads++;
		} else if (bp->bio_cmd == BIO_WRITE) {
			isc->in_writes++;
			isc->queued_writes++;
		}
	}
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
	int retval = 0;
#ifdef CAM_NETFLIX_IOSCHED
	if (!do_netflix_iosched)
		return retval;

	if (iosched_debug > 10)
		printf("done: %p %#x\n", bp, bp->bio_cmd);
	if (bp->bio_cmd == BIO_WRITE) {
		retval = isc->max_writes > 0 && isc->pending_writes == isc->max_writes;
		if (isc->max_writes > 0 && retval && iosched_debug)
			printf("NEEDS SCHED\n");
		isc->out_writes++;
		isc->pending_writes--;
	} else if (bp->bio_cmd == BIO_READ) {
		retval = isc->max_reads > 0 && isc->pending_reads == isc->max_reads;
		isc->out_reads++;
		isc->pending_reads--;
	} else if (bp->bio_cmd != BIO_FLUSH && bp->bio_cmd != BIO_DELETE) {
		if (iosched_debug)
			printf("Completing command with bio_cmd == %#x\n", bp->bio_cmd);
	}

	if (!(bp->bio_flags & BIO_ERROR))
		cam_iosched_io_metric_update(isc, done_ccb->ccb_h.qos.sim_data,
		    bp->bio_cmd, bp->bio_bcount);
#endif
	return retval;
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

#ifdef CAM_NETFLIX_IOSCHED
/*
 * After the method presented in Jack Crenshaw's 1998 article "Integer
 * Suqare Roots," reprinted at
 * http://www.embedded.com/electronics-blogs/programmer-s-toolbox/4219659/Integer-Square-Roots
 * and well worth the read. Briefly, we find the power of 4 that's the
 * largest smaller than val. We then check each smaller power of 4 to
 * see if val is still bigger. The right shifts at each step divide
 * the result by 2 which after successive application winds up
 * accumulating the right answer. It could also have been accumulated
 * using a separate root counter, but this code is smaller and faster
 * than that method. This method is also integer size invariant.
 * It returns floor(sqrt((float)val)), or the larget integer less than
 * or equal to the square root.
 */
static uint64_t
isqrt64(uint64_t val)
{
	uint64_t res = 0;
	uint64_t bit = 1ULL << (sizeof(uint64_t) * NBBY - 2);

	/*
	 * Find the largest power of 4 smaller than val.
	 */
	while (bit > val)
		bit >>= 2;

	/*
	 * Accumulate the answer, one bit at a time (we keep moving
	 * them over since 2 is the square root of 4 and we test
	 * powers of 4). We accumulate where we find the bit, but
	 * the successive shifts land the bit in the right place
	 * by the end.
	 */
	while (bit != 0) {
		if (val >= res + bit) {
			val -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;
		bit >>= 2;
	}
	
	return res;
}

/*
 * a and b are 32.32 fixed point stored in a 64-bit word.
 * Let al and bl be the .32 part of a and b.
 * Let ah and bh be the 32 part of a and b.
 * R is the radix and is 1 << 32
 *
 * a * b
 * (ah + al / R) * (bh + bl / R)
 * ah * bh + (al * bh + ah * bl) / R + al * bl / R^2
 *
 * After multiplicaiton, we have to renormalize by multiply by
 * R, so we wind up with
 *	ah * bh * R + al * bh + ah * bl + al * bl / R
 * which turns out to be a very nice way to compute this value
 * so long as ah and bh are < 65536 there's no loss of high bits
 * and the low order bits are below the threshold of caring for
 * this application.
 */
static uint64_t
mul(uint64_t a, uint64_t b)
{
	uint64_t al, ah, bl, bh;
	al = a & 0xffffffff;
	ah = a >> 32;
	bl = b & 0xffffffff;
	bh = b >> 32;
	return ((ah * bh) << 32) + al * bh + ah * bl + ((al * bl) >> 32);
}

static int
cmp(const void *a, const void *b)
{
	return (int)(*(sbintime_t *)a - *(sbintime_t *)b);
}

static void
cam_iosched_update(struct iop_stats *iop, sbintime_t sim_latency)
{
	sbintime_t y, yy;
	uint64_t var;

	/*
	 * Skip the first ~50 I/Os since experience has shown the first few are
	 * atypical.
	 */
	if (iop->skipping < IOP_MAX_SKIP) {
		iop->skipping++;
		return;
	}

	/*
	 * After the first few, the next few hundred are typical of what we'd
	 * expect.
	 */
	if (iop->training < IOP_MAX_TRAINING) {
		iop->data[iop->training++] = sim_latency;
		if (iop->training == IOP_MAX_TRAINING) {
			qsort(iop->data, nitems(iop->data),
			    sizeof(iop->data[0]), cmp);
			/* 95th percentile */
			iop->worst = iop->data[IOP_MAX_TRAINING * 95 / 100];
			/*
			 * We expect about 5% of the I/Os to be above the 95th
			 * percentile. However, the training happens early in
			 * boot where things are somewhat ideal compared to
			 * the fully loaded system, so allow for a wide margin
			 * of error before declaring things outliers by
			 * multiplying that by 4 to allow for expected
			 * degredation.
			 */
			iop->worst *= 4;
		}
		return;
	}

	if (sim_latency > iop->worst)
		iop->outliers++;

	/*
	 * We determine this new measurement is an outlier before we update
	 * our estimators. If sd is 0, then we have no reliable estimate for
	 * SD yet so don't bother seeing if this is an outlier.
	 */
	if (iop->sd != 0 && sim_latency > iop->ema + 5 * iop->sd) {
//		printf("Outlier: EMA: %ju SD: %ju lat %ju\n", (intmax_t)iop->ema,
//		       (intmax_t)iop->sd, (intmax_t)sim_latency);
		iop->outliers++;
	}

	/* 
	 * Classic expoentially decaying average with a tiny alpha
	 * (2 ^ -alpha_bits). For more info see the NIST statistical
	 * handbook.
	 *
	 * ema_t = y_t * alpha + ema_t-1 * (1 - alpha)
	 * alpha = 1 / (1 << alpha_bits)
	 *
	 * Since alpha is a power of two, we can compute this w/o any mult or
	 * division.
	 */
	y = sim_latency;
	iop->ema = (y + (iop->ema << ALPHA_BITS) - iop->ema) >> ALPHA_BITS;

	yy = mul(y, y);
	iop->emss = (yy + (iop->emss << ALPHA_BITS) - iop->emss) >> ALPHA_BITS;

	/*
         * s_1 = sum of data
	 * s_2 = sum of data * data
	 * ema ~ mean (or s_1 / N)
	 * emss ~ s_2 / N
	 *
	 * sd = sqrt((N * s_2 - s_1 ^ 2) / (N * (N - 1)))
	 * sd = sqrt((N * s_2 / N * (N - 1)) - (s_1 ^ 2 / (N * (N - 1))))
	 *
	 * N ~ 2 / alpha - 1
	 * alpha < 1 / 16 (typically much less)
	 * N > 31 --> N large so N * (N - 1) is approx N * N
	 *
	 * substituting and rearranging:
	 * sd ~ sqrt(s_2 / N - (s_1 / N) ^ 2)
	 *    ~ sqrt(emss - ema ^ 2);
	 * which is the formula used here to get a decent estimate of sd which
	 * we use to detect outliers. Note that when first starting up, it
	 * takes a while for emss sum of squares estimator to converge on a
	 * good value.  during this time, it can be less than ema^2. We
	 * compute a sd of 0 in that case, and ignore outliers.
	 */
	var = iop->emss - mul(iop->ema, iop->ema);
	iop->sd = (int64_t)var < 0 ? 0 : isqrt64(var);
}

static void
cam_iosched_io_metric_update(struct cam_iosched_softc *isc,
    sbintime_t sim_latency, int cmd, size_t size)
{
	/* xxx Do we need to scale based on the size of the I/O ? */
	switch (cmd) {
	case BIO_READ:
		cam_iosched_update(&isc->read_stats, sim_latency);
		break;
	case BIO_WRITE:
		cam_iosched_update(&isc->write_stats, sim_latency);
		break;
	case BIO_DELETE:
		cam_iosched_update(&isc->trim_stats, sim_latency);
		break;
	default:
		break;
	}
}

#ifdef DDB
static int biolen(struct bio_queue_head *bq)
{
	int i = 0;
	struct bio *bp;

	TAILQ_FOREACH(bp, &bq->queue, bio_queue) {
		i++;
	}
	return i;
}

/*
 * Show the internal state of the I/O scheduler.
 */
DB_SHOW_COMMAND(iosched, cam_iosched_db_show)
{
	struct cam_iosched_softc *isc;

	if (!have_addr) {
		db_printf("Need addr\n");
		return;
	}
	isc = (struct cam_iosched_softc *)addr;
	db_printf("pending_reads:     %d\n", isc->pending_reads);
	db_printf("min_reads:         %d\n", isc->min_reads);
	db_printf("max_reads:         %d\n", isc->max_reads);
	db_printf("reads:             %d\n", isc->reads);
	db_printf("in_reads:          %d\n", isc->in_reads);
	db_printf("out_reads:         %d\n", isc->out_reads);
	db_printf("queued_reads:      %d\n", isc->queued_reads);
	db_printf("Current Q len      %d\n", biolen(&isc->bio_queue));
	db_printf("pending_writes:    %d\n", isc->pending_writes);
	db_printf("min_writes:        %d\n", isc->min_writes);
	db_printf("max_writes:        %d\n", isc->max_writes);
	db_printf("writes:            %d\n", isc->writes);
	db_printf("in_writes:         %d\n", isc->in_writes);
	db_printf("out_writes:        %d\n", isc->out_writes);
	db_printf("queued_writes:     %d\n", isc->queued_writes);
	db_printf("Current Q len      %d\n", biolen(&isc->write_queue));
	db_printf("read_bias:         %d\n", isc->read_bias);
	db_printf("current_read_bias: %d\n", isc->current_read_bias);
	db_printf("Trim active?       %s\n", 
	    (isc->flags & CAM_IOSCHED_FLAG_TRIM_ACTIVE) ? "yes" : "no");
}
#endif
#endif
