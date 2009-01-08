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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/module.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>
#include <geom/geom_sched.h>

#define	G_SCHED_FLUSHING	1	/* Disk flush in progress. */
#define	G_SCHED_SWITCHING	2	/* Switching schedulers (debug.) */

/* Debug sysctl stuff. */
SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, sched, CTLFLAG_RW, 0, "I/O scheduler stuff");
u_int g_sched_debug;
SYSCTL_UINT(_kern_geom_sched, OID_AUTO, debug, CTLFLAG_RW, &g_sched_debug, 0,
    "Debug level");

/*
 * Global mutex, protecting the registered schedulers' list and their
 * gs_refs field.
 */
static struct mtx g_sched_mtx;

/* Global list of registered schedulers. */
LIST_HEAD(g_sched_list, g_sched);
static struct g_sched_list g_sched_list;

/* Initialization flag. */
static int g_sched_initialized;

void
g_sched_init(void)
{

	if (g_sched_initialized != 0)
		return;

	g_sched_initialized = 1;

	mtx_init(&g_sched_mtx, "I/O scheduler", NULL, MTX_DEF);
	LIST_INIT(&g_sched_list);
}

void
g_sched_fini(void)
{

	/*
	 * This function is called when the g_disk module is unloaded,
	 * since all the scheduler modules depend on it, they must have
	 * been unregistered.
	 */
	KASSERT(LIST_EMPTY(&g_sched_list), ("still registered schedulers"));
	mtx_destroy(&g_sched_mtx);
}

void
g_sched_disk_init(struct disk *dp)
{

	mtx_init(&dp->d_sched_lock, "disk I/O scheduler", NULL, MTX_DEF);
	dp->d_sched_flags = 0;
	dp->d_nr_sorted = 0;
	dp->d_sched = NULL;
	dp->d_sched_data = NULL;
}

/*
 * Flush the scheduler, assuming that the disk d_sched_lock mutex is
 * held.  This function tries to dispatch all the requests queued in
 * the target scheduler and to wait until they're completed.  Flushing
 * is implemented avoiding queueing for all the requests arriving while
 * the flush is in progres.
 */
static void
g_sched_flush_locked(struct disk *dp)
{
	struct g_sched *gsp;

	gsp = dp->d_sched;
	if (gsp == NULL)
		return;

	dp->d_sched_flags |= G_SCHED_FLUSHING;
	G_SCHED_DEBUG(2, "geom_sched: flushing");
	while (dp->d_nr_sorted > 0) {
		mtx_unlock(&dp->d_sched_lock);
		dp->d_kick(dp);
		G_SCHED_DEBUG(2, "geom_sched: %d to flush", dp->d_nr_sorted);
		tsleep(&dp->d_sched, 0, "I/O sched flush", hz);
		mtx_lock(&dp->d_sched_lock);
	}
	dp->d_sched_flags &= ~G_SCHED_FLUSHING;
}

void
g_sched_disk_gone(struct disk *dp)
{
	struct g_sched *gsp;
	struct bio *bp;

	mtx_lock(&dp->d_sched_lock);
	gsp = dp->d_sched;
	if (gsp != NULL) {
		while ((bp = gsp->gs_next(dp->d_sched_data, 1)) != NULL) {
			mtx_unlock(&dp->d_sched_lock);
			/*
			 * Discard all the requests in the scheduler with
			 * an appropriate error.  Need to release the disk
			 * lock since completion callbacks may reenter the
			 * scheduler.
			 */
			biofinish(bp, NULL, ENXIO);
			mtx_lock(&dp->d_sched_lock);
		}
	}
	mtx_unlock(&dp->d_sched_lock);
}

void
g_sched_disk_fini(struct disk *dp)
{

	g_sched_disk_gone(dp);
	/*
	 * Here we assume that no new requests reach the scheduler, since
	 * the disk is almost already destroyed.
	 */
	g_sched_configure(dp, "none");
	mtx_destroy(&dp->d_sched_lock);
}

void
g_sched_start(struct disk *dp, struct bio *bp)
{
	struct g_sched *gsp;

	mtx_lock(&dp->d_sched_lock);
	gsp = dp->d_sched;

	/*
	 * Don't try to queue a request if we have no scheduler for
	 * this disk, or if the request is not one of the type we care
	 * about (i.e., it is not a read or write).
	 */
	if (gsp == NULL || (bp->bio_cmd & (BIO_READ | BIO_WRITE)) == 0)
		goto nosched;

	/*
	 * When flushing is in progress we don't want the scheduler
	 * queue to grow, so we dispatch new requests directly to the
	 * driver.
	 */
	if ((dp->d_sched_flags & G_SCHED_FLUSHING) != 0)
		goto nosched;

	dp->d_nr_sorted++;
	gsp->gs_start(dp->d_sched_data, bp);
	mtx_unlock(&dp->d_sched_lock);

	/*
	 * Try to immediately start the queue.  It is up to the scheduler
	 * to freeze it if needed (returning NULL on the next invocation
	 * of gs_next()).  The scheduler will also be responsible of
	 * restarting the dispatches to the driver, invoking d_kick()
	 * directly.
	 */
	dp->d_kick(dp);
	return;

nosched:
	mtx_unlock(&dp->d_sched_lock);

	/*
	 * Mark the request as not sorted by the scheduler.  Schedulers
	 * are supposed to store a non-NULL value in the bio_caller1 field
	 * (they will need it anyway, unless they're really really simple.)
	 */
	bp->bio_caller1 = NULL;
	dp->d_strategy(bp);
}

struct bio *
g_sched_next(struct disk *dp)
{
	struct g_sched *gsp;
	struct bio *bp;

	bp = NULL;

	mtx_lock(&dp->d_sched_lock);
	gsp = dp->d_sched;

	/* If the disk is not using a scheduler, just always return NULL. */
	if (gsp == NULL)
		goto out;

	/* Get the next request from the scheduler. */
	bp = gsp->gs_next(dp->d_sched_data,
	    (dp->d_sched_flags & G_SCHED_FLUSHING) != 0);

	KASSERT(bp == NULL || bp->bio_caller1 != NULL,
	    ("bio_caller1 == NULL"));

out:
	mtx_unlock(&dp->d_sched_lock);

	return (bp);
}

void
g_sched_done(struct bio *bp)
{
	struct disk *dp;
	struct g_sched *gsp;
	int kick;

	dp = bp->bio_disk;

	mtx_lock(&dp->d_sched_lock);

	kick = !!dp->d_nr_sorted;

	gsp = dp->d_sched;
	/*
	 * Don't call the completion callback if we have no scheduler
	 * or if the request that completed was not one we sorted.
	 */
	if (gsp == NULL || bp->bio_caller1 == NULL)
		goto out;

	kick = gsp->gs_done(dp->d_sched_data, bp);

	/*
	 * If flush is in progress and we have no more requests queued,
	 * wake up the flushing process.
	 */
	if (--dp->d_nr_sorted == 0 &&
	    (dp->d_sched_flags & G_SCHED_FLUSHING) != 0) {
		G_SCHED_DEBUG(2, "geom_sched: flush complete");
		wakeup(&dp->d_sched);
	}

out:
	mtx_unlock(&dp->d_sched_lock);

	if (kick)
		dp->d_kick(dp);
}

/*
 * Try to register a new scheduler.  May fail if a scheduler with the
 * same name is already registered.
 */
static int
g_sched_register(struct g_sched *gsp)
{
	struct g_sched *tmp;
	int error;

	error = 0;

	mtx_lock(&g_sched_mtx);
	LIST_FOREACH(tmp, &g_sched_list, gs_list)
		if (strcmp(tmp->gs_name, gsp->gs_name) == 0) {
			G_SCHED_DEBUG(1, "geom_sched: %s already registered",
			    gsp->gs_name);
			error = EEXIST;
			goto out;
		}

	LIST_INSERT_HEAD(&g_sched_list, gsp, gs_list);
	gsp->gs_refs = 1;

out:
	mtx_unlock(&g_sched_mtx);

	return (error);
}

/*
 * Try to unregister a scheduler.  May fail if the scheduler is not
 * registered or if it still in use.
 */
static int
g_sched_unregister(struct g_sched *gsp)
{
	struct g_sched *tmp;
	int error;

	error = 0;

	mtx_lock(&g_sched_mtx);
	LIST_FOREACH(tmp, &g_sched_list, gs_list) {
		if (tmp == gsp) {
			if (gsp->gs_refs != 1) {
				G_SCHED_DEBUG(1, "geom_sched: %s still in use",
				    gsp->gs_name);
				error = EBUSY;
			} else if (gsp->gs_refs == 1)
				/*
				 * The list reference is the last one
				 * that can be removed, so it is safe to
				 * just decrement the counter elsewhere.
				 */
				LIST_REMOVE(gsp, gs_list);
			goto out;
		}
	}

	G_SCHED_DEBUG(1, "geom_sched: %s not registered", gsp->gs_name);

out:
	mtx_unlock(&g_sched_mtx);

	return (error);
}

/*
 * Search a scheduler by name, and return it, adding a reference to it.
 * Return NULL if no scheduler with the given name exists.
 */
static struct g_sched *
g_sched_find(const char *name)
{
	struct g_sched *gsp;

	mtx_lock(&g_sched_mtx);
	LIST_FOREACH(gsp, &g_sched_list, gs_list) {
		if (strcmp(name, gsp->gs_name) == 0) {
			gsp->gs_refs++;
			goto out;
		}
	}

	gsp = NULL;

out:
	mtx_unlock(&g_sched_mtx);

	return (gsp);
}

int
g_sched_configure(struct disk *dp, const char *name)
{
	struct g_sched *gsp, *old_gsp;
	void *data, *old_data;
	int error;

	error = 0;
	old_data = NULL;

	G_SCHED_DEBUG(2, "geom_sched: switching to %s", name);

	/*
	 * A driver that does not provide a d_kick() method cannot
	 * use the scheduler subsystem.  Just ignore the configuration
	 * request.
	 */
	if (dp->d_kick == NULL) {
		printf("d_kick = %p\n", dp->d_kick);
		return (EOPNOTSUPP);
	}

	gsp = g_sched_find(name);
	/*
	 * Admit a NULL gsp to indicate that we are switching to the
	 * default system behavior (no scheduler at all), iff the
	 * provided name is "none."
	 */
	if (gsp == NULL && strcmp("none", name) != 0) {
		printf("scheduler not found\n");
		return (EINVAL);
	}

	mtx_lock(&dp->d_sched_lock);
	old_gsp = dp->d_sched;
	if (old_gsp == gsp) {
		/* Not really a switch, same scheduler, just return. */
		printf("same scheduler\n");
		goto out;
	}

	/*
	 * Reconfiguration events are serialized in the same thread,
	 * so we should not see more than one reconfiguration at time.
	 */
	KASSERT((dp->d_sched_flags & G_SCHED_SWITCHING) == 0,
	    ("multiple reconfiguration requests"));

	dp->d_sched_flags |= G_SCHED_SWITCHING;

	if (old_gsp != NULL) {
		/* We had a previous scheduler, flush it. */
		g_sched_flush_locked(dp);
	}

	if (gsp != NULL) {
		mtx_unlock(&dp->d_sched_lock);
		/* Try to allocate the new private data. */
		data = gsp->gs_init(dp);
		if (data == NULL) {
			error = ENOMEM;
			goto unref;
		}
		mtx_lock(&dp->d_sched_lock);
		/*
		 * Allocation went OK, prepare to release old data and
		 * store the new ones in d_sched_data.
		 */
		old_data = dp->d_sched_data;
		dp->d_sched_data = data;
	}

	/* Commit the switch. */
	dp->d_sched = gsp;
	dp->d_sched_flags &= ~G_SCHED_SWITCHING;

	/* Remember to release the reference to the old scheduler. */
	gsp = old_gsp;
out:
	mtx_unlock(&dp->d_sched_lock);

unref:
	if (gsp != NULL) {
		if (old_data != NULL)
			gsp->gs_fini(old_data);
		mtx_lock(&g_sched_mtx);
		/*
		 * gs_refs > 2 here, as the g_sched_list holds a reference
		 * to it.  The ugly lock/unlock sequence around the decrement
		 * does not increase the number of atomic ops WRT using the
		 * atomic_* functions.  (The ref got in g_sched_find() would
		 * balance the atomic op removed here.)
		 */
		gsp->gs_refs--;
		mtx_unlock(&g_sched_mtx);
	}

	G_SCHED_DEBUG(2, "geom_sched: switch done (%d)", error);

	return (error);
}

/*
 * Helper to load/unload scheduler modules.  Each module should
 * DECLARE_SCHED_MODULE() to declare itself, providing a struct
 * g_sched descriptor.  This function is used from
 * DECLARE_SCHED_MODULE() to register/unregister the scheduler.
 */
int
g_sched_modevent(module_t mod, int cmd, void *arg)
{
	struct g_sched *gsp;
	int error;

	gsp = arg;
	error = EOPNOTSUPP;

	g_sched_init();

	switch (cmd) {
	case MOD_LOAD:
		error = g_sched_register(gsp);
		break;
	case MOD_UNLOAD:
		error = g_sched_unregister(gsp);
		break;
	}

	return (error);
}
