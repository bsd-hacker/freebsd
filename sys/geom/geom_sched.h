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

#ifndef _GEOM_GEOM_SCHED_H_
#define _GEOM_GEOM_SCHED_H_

#ifdef _KERNEL 
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <geom/geom_disk.h>

extern u_int g_sched_debug;

#define	G_SCHED_DEBUG(lvl, ...) do {					\
	if (g_sched_debug >= (lvl)) {					\
		printf("GEOM_SCHED");					\
		if (g_sched_debug > 0)					\
			printf("[%u]", (lvl));				\
		printf(": ");						\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)

/*
 * Types for the callbacks defined by each I/O scheduler.
 *
 * When a scheduler is assigned to a disk its gs_init method is called.
 * It takes the target disk as input and returns the private data the
 * scheduler will use for it.  It can fail and return NULL, thus making
 * the scheduler switch fail.  It is not called under disk d_sched_lock
 * nor under g_sched_mtx.
 */
typedef void *gs_init_t (struct disk *dp);

/*
 * Disk shutdown method, called when the scheduler is detached from a
 * disk.  It is not synchronized WRT other scheduler methods, but it is
 * guaranteed to be executed when no more requests will reach the
 * scheduler.
 */
typedef void gs_fini_t (void *data);

/*
 * Insert a new request into the scheduler.  This method is called only
 * for requests that can be reordered by the scheduler, and is supposed
 * to queue the request.  It is called under the disk d_sched_lock mutex.
 */
typedef void gs_start_t (void *data, struct bio *bp);

/*
 * Get the next request from the scheduler.  This method is called with
 * the disk d_sched_lock mutex held, from the drivers code.  When force
 * is not zero it is assumed to not return NULL unless the scheduler has
 * no more queued requests.
 */
typedef struct bio *gs_next_t (void *data, int force);

/*
 * Completion callback for a previously issued request.  It gets called
 * under the disk d_sched_lock mutex.  It has no specific task, as
 * request completion is handled by the generic code, but can be used
 * by the scheduler to implement anticipation mechanisms.  A nonzero
 * return value means that dispatching has to be restarted.
 */
typedef int gs_done_t (void *data, struct bio *bp);

/*
 * I/O scheduler descriptor.  Each scheduler has to register itself
 * providing a struct g_sched object.  (Must not be const, as the
 * framework needs to modify gs_refs and gs_list.)
 */
struct g_sched {
	const char	*gs_name;

	int		gs_refs;

	gs_init_t	*gs_init;
	gs_fini_t	*gs_fini;
	gs_start_t	*gs_start;
	gs_next_t	*gs_next;
	gs_done_t	*gs_done;

	LIST_ENTRY(g_sched) gs_list;
};

/*
 * Lookup the identity of the issuer of the original request.
 * In the current implementation we use the curthread of the
 * issuer, but different mechanisms may be implemented later
 * so we do not make assumptions on the return value which for
 * us is just an opaque identifier.
 * For the time being we make this inline.
 */
static inline
u_long g_sched_classify(struct bio *bp)
{

	if (bp == NULL) {
		printf("g_sched_classify: NULL bio\n");
		return (0);     /* as good as anything */
	}
	while (bp->bio_parent != NULL)
		bp = bp->bio_parent;
	return ((u_long)(bp->bio_caller1));
}

/*
 * Initialize/cleanup the I/O scheduling subsystem.  Interface towards
 * geom_disk.c, for class init/fini methods.
 */
void g_sched_init(void);
void g_sched_fini(void);

/*
 * Initialize/cleanup the I/O scheduling fields of the given disk.  Interface
 * towards geom_disk.c, used to signal to the scheduler subsystem the main
 * events of the disk lifecycle.
 */
void g_sched_disk_init(struct disk *dp);
void g_sched_disk_fini(struct disk *dp);
void g_sched_disk_gone(struct disk *dp);

/*
 * Try to enqueue a bio into the scheduler of the given disk.  If the
 * request can be reordered (we reorder only READ and WRITE requests),
 * the bio is passed to the scheduler, otherwise it is passed directly
 * to the driver using its d_strategy.
 */
void g_sched_start(struct disk *dp, struct bio *bp);

/*
 * Pick the next request from the scheduler of the given disk.  This
 * function has to be called from the drivers when they want to refill
 * their internal queue (if any) or if they need a new request.
 * If force == 0 the scheduler is allowed to return NULL even if it
 * has requests enqueued, to allow non work-conserving schedulers to
 * be implemented (the kick mechanism will restart the I/O on the
 * device).
 */
struct bio *g_sched_next(struct disk *dp);

/*
 * Notify to the scheduler the completion of a request.
 */
void g_sched_done(struct bio *bp);

/*
 * Try to configure dp to use the scheduler identified by name.  Used
 * by the disk class to handle reconfigurations.  It may sleep waiting
 * for pending requests to complete.
 */
int g_sched_configure(struct disk *dp, const char *name);

/*
 * Register a scheduling module.  Used by the schedulers to register
 * themselves to the system.
 */
int g_sched_modevent(module_t mod, int cmd, void *arg);

#define	DECLARE_GSCHED_MODULE(name, gsched)				\
	static moduledata_t name##_mod = {				\
		#name,							\
		g_sched_modevent,					\
		gsched,							\
	};								\
	DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);	\
	MODULE_DEPEND(name, g_disk, 0, 0, 0);

#endif /* _KERNEL */
#endif /* _GEOM_GEOM_SCHED_H_ */
