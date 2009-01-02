/*-
 * Copyright (c) 2007-2009 Robert N. M. Watson
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
__FBSDID("$FreeBSD$");

/*
 * netisr2 is a work dispatch service, allowing synchronous and asynchronous
 * processing of packets by protocol handlers.  Each protocol registers a
 * handler, and callers pass the protocol identifier and packet to the netisr
 * dispatch routines to cause them to be processed.  Processing may occur
 * synchonously via direct dispatch, or asynchronously via queued dispatch in
 * a worker thread.
 *
 * Maintaining ordering for protocol streams is a critical design concern.
 * Enforcing ordering limits the opportunity for concurrency, but maintains
 * the strong ordering requirements found in some protocols, such as TCP.  Of
 * related concern is CPU affinity--it is desirable to process all data
 * associated with a particular stream on the same CPU over time in order to
 * avoid acquiring locks associated with the connection on different CPUs,
 * keep connection data in one cache, and to generally encourage associated
 * user threads to live on the same CPU as the stream.
 *
 * We handle three cases:
 *
 * - The protocol is unable to determine an a priori ordering based on a
 *   cheap inspection of packet contents, so we either globally order (run in
 *   a single worker) or source order (run in the context of a particular
 *   source).
 *
 * - The protocol exposes ordering information in the form of a generated
 *   flow identifier, and relies on netisr2 to assign this work to a CPU.  We
 *   can execute the handler in the source thread, or we can assign it to a
 *   CPU based on hashing flows to CPUs.
 *
 * - The protocol exposes ordering and affinity information in the form of a
 *   CPU identifier.  We can execute the handler in the source thread, or we
 *   can dispatch it to the worker for that CPU.
 *
 * When CPU and flow affinities are returned by protocols, they also express
 * an affinity strength, which is used by netisr2 to decide whether or not to
 * directly dispatch a packet on a CPU other than the one it has an affinity
 * for.
 *
 * We guarantee that if two packets come from the same source have the same
 * flowid or CPU affinity, and have the same affinity strength, they will
 * remain in order with respect to each other.  We guarantee that if the
 * returned affinity is strong, the packet will only be processed on the
 * requested CPU or CPU associated with the requested flow.
 *
 * Protocols that provide flowids but not affinity should attempt to provide
 * a uniform distribution over the flowid space in order to balance work
 * effectively.
 *
 * Some possible sources of flow identifiers for packets:
 * - Hardware-generated hash from RSS
 * - Software-generated hash from addresses and ports identifying the flow
 * - Interface identifier the packet came from
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/netisr.h>
#include <net/netisr2.h>

#ifdef NETISR_LOCKING
/*-
 * Synchronize use and modification of the registered netisr data structures;
 * acquire a read lock while modifying the set of registered protocols to
 * prevent partially registered or unregistered protocols from being run.
 *
 * We make this optional so that we can measure the performance impact of
 * providing consistency against run-time registration and deregristration,
 * which is a very uncommon event.
 *
 * The following data structures and fields are protected by this lock:
 *
 * - The np array, including all fields of struct netisr_proto.
 * - The nws array, including all fields of struct netisr_worker.
 * - The nws_array array.
 */
static struct rwlock	netisr_rwlock;
#define	NETISR_LOCK_INIT()	rw_init(&netisr_rwlock, "netisr")
#define	NETISR_LOCK_ASSERT()	rw_assert(&netisr_rwlock, RW_LOCKED)
#define	NETISR_RLOCK()		rw_rlock(&netisr_rwlock)
#define	NETISR_RUNLOCK()	rw_runlock(&netisr_rwlock)
#define	NETISR_WLOCK()		rw_wlock(&netisr_rwlock)
#define	NETISR_WUNLOCK()	rw_wunlock(&netisr_rwlock)
#else
#define	NETISR_LOCK_INIT()
#define	NETISR_LOCK_ASSERT()
#define	NETISR_RLOCK()
#define	NETISR_RUNLOCK()
#define	NETISR_WLOCK()
#define	NETISR_WUNLOCK()
#endif

SYSCTL_NODE(_net, OID_AUTO, isr2, CTLFLAG_RW, 0, "netisr2");

static int	netisr_direct = 1;	/* Enable direct dispatch. */
SYSCTL_INT(_net_isr2, OID_AUTO, direct, CTLFLAG_RW, &netisr_direct, 0,
    "Direct dispatch");

/*
 * Allow the administrator to limit the number of threads (CPUs) to use for
 * netisr2.  Notice that we don't check netisr_maxthreads before creating the
 * thread for CPU 0, so in practice we ignore values <= 1.
 */
static int	netisr_maxthreads = MAXCPU;	/* Bound number of threads. */
TUNABLE_INT("net.isr2.maxthreads", &netisr_maxthreads);
SYSCTL_INT(_net_isr2, OID_AUTO, maxthreads, CTLFLAG_RD, &netisr_maxthreads,
    0, "Use at most this many CPUs for netisr2 processing");

/*
 * Each protocol is described by an instance of netisr_proto, which holds all
 * global per-protocol information.  This data structure is set up by
 * netisr_register().  Currently, no flags are required, as all handlers are
 * MPSAFE in the netisr2 system.  Protocols provide zero or one of the two
 * lookup interfaces, but not both.
 */
struct netisr_proto {
	netisr_t	*np_func;			/* Protocol handler. */
	netisr_lookup_flow_t	*np_lookup_flow;	/* Flow generation. */
	netisr_lookup_cpu_t	*np_lookup_cpu;		/* CPU affinity. */
	const char	*np_name;			/* Protocol name. */
};

#define	NETISR_MAXPROT		32		/* Compile-time limit. */
#define	NETISR_ALLPROT		0xffffffff	/* Run all protocols. */

/*
 * The np array describes all registered protocols, indexed by protocol
 * number.
 */
static struct netisr_proto	np[NETISR_MAXPROT];

/*
 * Protocol-specific work for each workstream is described by struct
 * netisr_work.  Each work descriptor consists of an mbuf queue and
 * statistics.
 *
 * XXXRW: Using a lock-free linked list here might be useful.
 */
struct netisr_work {
	/*
	 * Packet queue, linked by m_nextpkt.
	 */
	struct mbuf	*nw_head;
	struct mbuf	*nw_tail;
	u_int		 nw_len;
	u_int		 nw_max;
	u_int		 nw_watermark;

	/*
	 * Statistics.
	 */
	u_int		 nw_dispatched; /* Number of direct dispatches. */
	u_int		 nw_dropped;	/* Number of drops. */
	u_int		 nw_queued;	/* Number of enqueues. */
	u_int		 nw_handled;	/* Number passed into handler. */
};

/*
 * Workstreams hold a set of ordered work across each protocol, and are
 * described by netisr_workstream.  Each workstream is associated with a
 * worker thread, which in turn is pinned to a CPU.  Work associated with a
 * workstream can be processd in other threads during direct dispatch;
 * concurrent processing is prevented by the NWS_RUNNING flag, which
 * indicates that a thread is already processing the work queue.
 *
 * Currently, #workstreams must equal #CPUs.
 */
struct netisr_workstream {
	struct thread	*nws_thread;		/* Thread serving stream. */
	struct mtx	 nws_mtx;		/* Synchronize work. */
	struct cv	 nws_cv;		/* Wake up worker. */
	u_int		 nws_cpu;		/* CPU pinning. */
	u_int		 nws_flags;		/* Wakeup flags. */

	u_int		 nws_pendingwork;	/* Across all protos. */
	/*
	 * Each protocol has per-workstream data.
	 */
	struct netisr_work	nws_work[NETISR_MAXPROT];
};

/*
 * Kernel process associated with worker threads.
 */
static struct proc			*netisr2_proc;

/*
 * Per-CPU workstream data, indexed by CPU ID.
 */
static struct netisr_workstream		 nws[MAXCPU];

/*
 * Map contiguous values between 0 and nws_count into CPU IDs appropriate for
 * indexing the nws[] array.  This allows constructions of the form
 * nws[nws_array(arbitraryvalue % nws_count)].
 */
static u_int				 nws_array[MAXCPU];

/*
 * Number of registered workstreams.  Should be the number of running CPUs
 * once fully started.
 */
static u_int				 nws_count;

/*
 * Per-workstream flags.
 */
#define	NWS_RUNNING	0x00000001	/* Currently running in a thread. */
#define	NWS_SIGNALED	0x00000002	/* Signal issued. */

/*
 * Synchronization for each workstream: a mutex protects all mutable fields
 * in each stream, including per-protocol state (mbuf queues).  The CV will
 * be used to wake up the worker if asynchronous dispatch is required.
 */
#define	NWS_LOCK(s)		mtx_lock(&(s)->nws_mtx)
#define	NWS_LOCK_ASSERT(s)	mtx_assert(&(s)->nws_mtx, MA_OWNED)
#define	NWS_UNLOCK(s)		mtx_unlock(&(s)->nws_mtx)
#define	NWS_SIGNAL(s)		cv_signal(&(s)->nws_cv)
#define	NWS_WAIT(s)		cv_wait(&(s)->nws_cv, &(s)->nws_mtx)

/*
 * Register a new netisr handler, which requires initializing per-protocol
 * fields for each workstream.  All netisr2 work is briefly suspended while
 * the protocol is installed.
 */
void
netisr2_register(u_int proto, netisr_t func, netisr_lookup_cpu_t lookup_cpu,
    netisr_lookup_flow_t lookup_flow, const char *name, u_int max)
{
	struct netisr_work *npwp;
	int i;

	NETISR_WLOCK();
	KASSERT(proto < NETISR_MAXPROT,
	    ("netisr2_register(%d, %s): too many protocols", proto, name));
	KASSERT(np[proto].np_func == NULL,
	    ("netisr2_register(%d, %s): func present", proto, name));
	KASSERT(np[proto].np_lookup_cpu == NULL,
	    ("netisr2_register(%d, %s): lookup_cpu present", proto, name));
	KASSERT(np[proto].np_lookup_flow == NULL,
	    ("netisr2_register(%d, %s): lookup_flow present", proto, name));
	KASSERT(np[proto].np_name == NULL,
	    ("netisr2_register(%d, %s): name present", proto, name));

	KASSERT(func != NULL, ("netisr2_register: func NULL"));
	KASSERT((lookup_flow == NULL && lookup_cpu == NULL) ||
	    (lookup_flow != NULL && lookup_cpu == NULL) ||
	    (lookup_flow == NULL && lookup_cpu != NULL),
	    ("netisr2_register(%d, %s): flow and cpu set", proto, name));

	/*
	 * Initialize global and per-workstream protocol state.
	 */
	np[proto].np_func = func;
	np[proto].np_lookup_cpu = lookup_cpu;
	np[proto].np_lookup_flow = lookup_flow;
	np[proto].np_name = name;
	for (i = 0; i < MAXCPU; i++) {
		npwp = &nws[i].nws_work[proto];
		bzero(npwp, sizeof(*npwp));
		npwp->nw_max = max;
	}
	NETISR_WUNLOCK();
}

/*
 * Drain all packets currently held in a particular protocol work queue.
 */
static void
netisr2_drain_proto(struct netisr_work *npwp)
{
	struct mbuf *m;

	while ((m = npwp->nw_head) != NULL) {
		npwp->nw_head = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (npwp->nw_head == NULL)
			npwp->nw_tail = NULL;
		npwp->nw_len--;
		m_freem(m);
	}
	KASSERT(npwp->nw_tail == NULL, ("netisr_drain_proto: tail"));
	KASSERT(npwp->nw_len == 0, ("netisr_drain_proto: len"));
}

/*
 * Remove the registration of a network protocol, which requires clearing
 * per-protocol fields across all workstreams, including freeing all mbufs in
 * the queues at time of deregister.  All work in netisr2 is briefly
 * suspended while this takes place.
 */
void
netisr2_deregister(u_int proto)
{
	struct netisr_work *npwp;
	int i;

	NETISR_WLOCK();
	KASSERT(proto < NETISR_MAXPROT,
	    ("netisr_deregister(%d): protocol too big", proto));
	KASSERT(np[proto].np_func != NULL,
	    ("netisr_deregister(%d): protocol not registered", proto));

	np[proto].np_func = NULL;
	np[proto].np_name = NULL;
	np[proto].np_lookup_cpu = NULL;
	np[proto].np_lookup_flow = NULL;
	for (i = 0; i < MAXCPU; i++) {
		npwp = &nws[i].nws_work[proto];
		netisr2_drain_proto(npwp);
		bzero(npwp, sizeof(*npwp));
	}
	NETISR_WUNLOCK();
}

/*
 * Naively map a flow ID into a CPU ID.  For now we use a rather poor hash to
 * reduce 32 bits down to a much smaller number of bits.  We should attempt
 * to be much more adaptive to the actual CPU count.
 *
 * XXXRW: This needs to be entirely rewritten.
 */
u_int
netisr2_flowid2cpuid(u_int flowid)
{

	NETISR_LOCK_ASSERT();

	/*
	 * Most systems have less than 256 CPUs, so combine the various bytes
	 * in the flowid so that we get all the entropy down to a single
	 * byte.  We could be doing a much better job here.  On systems with
	 * fewer CPUs, we slide the top nibble into the bottom nibble.
	 */
	flowid = ((flowid & 0xff000000) >> 24) ^
	    ((flowid & 0x00ff0000) >> 16) ^ ((flowid & 0x0000ff00) >> 8) ^
	    (flowid & 0x000000ff);
#if MAXCPU <= 16
	flowid = ((flowid & 0xf0) >> 4) ^ (flowid & 0x0f);
#endif
	return (nws_array[flowid % nws_count]);
}

/*
 * Look up the correct stream for a requested flowid.  There are two cases:
 * one in which the caller has requested execution on the current CPU (i.e.,
 * source ordering is sufficient, perhaps because the underlying hardware has
 * generated multiple input queues with sufficient order), or the case in
 * which we ask the protocol to generate a flowid.  In the latter case, we
 * rely on the protocol generating a reasonable distribution across the
 * flowid space, and hence use a very simple mapping from flowids to workers.
 *
 * Because protocols may need to call m_pullup(), they may rewrite parts of
 * the mbuf chain.  As a result, we must return an mbuf chain that is either
 * the old chain (if there is no update) or the new chain (if there is). NULL
 * is returned if there is a failure in the protocol portion of the lookup
 * (i.e., out of mbufs and a rewrite is required).
 */
static struct mbuf *
netisr2_selectcpu(u_int proto, struct mbuf *m, u_int *cpuidp,
    u_int *strengthp)
{
	u_int flowid;

	NETISR_LOCK_ASSERT();

	KASSERT(nws_count > 0, ("netisr2_workstream_lookup: nws_count"));

	*cpuidp = 0;
	*strengthp = 0;
	if (np[proto].np_lookup_cpu != NULL)
		return (np[proto].np_lookup_cpu(m, cpuidp, strengthp));
	else if (np[proto].np_lookup_flow != NULL) {
		m = np[proto].np_lookup_flow(m, &flowid, strengthp);
		if (m == NULL)
			return (NULL);
		*cpuidp = netisr2_flowid2cpuid(flowid);
		return (m);
	} else {
		/*
		 * XXXRW: Pin protocols without a CPU or flow assignment
		 * preference to an arbitrary CPU.  This needs refinement.
		 */
		*cpuidp = netisr2_flowid2cpuid(proto);
		*strengthp = NETISR2_AFFINITY_WEAK;
		return (m);
	}
}

/*
 * Process packets associated with a workstream and protocol.  For reasons of
 * fairness, we process up to one complete netisr queue at a time, moving the
 * queue to a stack-local queue for processing, but do not loop refreshing
 * from the global queue.  The caller is responsible for deciding whether to
 * loop, and for setting the NWS_RUNNING flag.  The passed workstream will be
 * locked on entry and relocked before return, but will be released while
 * processing.
 */
static void
netisr2_process_workstream_proto(struct netisr_workstream *nwsp, int proto)
{
	struct netisr_work local_npw, *npwp;
	u_int handled;
	struct mbuf *m;

	NWS_LOCK_ASSERT(nwsp);

	KASSERT(nwsp->nws_flags & NWS_RUNNING,
	    ("netisr_process_workstream_proto(%d): not running", proto));
	KASSERT(proto >= 0 && proto < NETISR_MAXPROT,
	    ("netisr_process_workstream_proto(%d): invalid proto\n", proto));

	npwp = &nwsp->nws_work[proto];
	if (npwp->nw_len == 0)
		return;

	/*
	 * Create a local copy of the work queue, and clear the global queue.
	 *
	 * Notice that this means the effective maximum length of the queue
	 * is actually twice that of the maximum queue length specified in
	 * the protocol registration call.
	 */
	handled = npwp->nw_len;
	local_npw = *npwp;
	npwp->nw_head = NULL;
	npwp->nw_tail = NULL;
	npwp->nw_len = 0;
	nwsp->nws_pendingwork -= handled;
	NWS_UNLOCK(nwsp);
	while ((m = local_npw.nw_head) != NULL) {
		local_npw.nw_head = m->m_nextpkt;
		m->m_nextpkt = NULL;
		if (local_npw.nw_head == NULL)
			local_npw.nw_tail = NULL;
		local_npw.nw_len--;
		np[proto].np_func(m);
	}
	KASSERT(local_npw.nw_len == 0,
	    ("netisr_process_proto(%d): len %d", proto, local_npw.nw_len));
	NWS_LOCK(nwsp);
	npwp->nw_handled += handled;
}

/*
 * Process either one or all protocols associated with a specific workstream.
 * Handle only existing work for each protocol processed, not new work that
 * may arrive while processing.  Set the running flag so that other threads
 * don't also try to process work in the queue; however, the lock on the
 * workstream will be released by netisr_process_workstream_proto() while
 * entering the protocol so that producers can continue to queue new work.
 *
 * The consumer is responsible for making sure that either all available work
 * is performed until there is no more work to perform, or that the worker is
 * scheduled to pick up where the consumer left off.  They are also
 * responsible for checking the running flag before entering this function.
 */
static void
netisr2_process_workstream(struct netisr_workstream *nwsp, int proto)
{
	u_int i;

	NETISR_LOCK_ASSERT();
	NWS_LOCK_ASSERT(nwsp);

	KASSERT(nwsp->nws_flags & NWS_RUNNING,
	    ("netisr2_process_workstream: not running"));
	if (proto == NETISR_ALLPROT) {
		for (i = 0; i < NETISR_MAXPROT; i++)
			netisr2_process_workstream_proto(nwsp, i);
	} else
		netisr2_process_workstream_proto(nwsp, proto);
}

/*
 * Worker thread that waits for and processes packets in a set of workstreams
 * that it owns.  Each thread has one cv, which is uses for all workstreams
 * it handles.
 */
static void
netisr2_worker(void *arg)
{
	struct netisr_workstream *nwsp;

	nwsp = arg;

	thread_lock(curthread);
	sched_prio(curthread, SWI_NET * RQ_PPQ + PI_SOFT);
	sched_bind(curthread, nwsp->nws_cpu);
	thread_unlock(curthread);

	/*
	 * Main work loop.  In the future we will want to support stopping
	 * workers, as well as re-balancing work, in which case we'll need to
	 * also handle state transitions.
	 *
	 * XXXRW: netisr_rwlock.
	 */
	NWS_LOCK(nwsp);
	while (1) {
		while (nwsp->nws_pendingwork == 0) {
			nwsp->nws_flags &= ~(NWS_SIGNALED | NWS_RUNNING);
			NWS_WAIT(nwsp);
			nwsp->nws_flags |= NWS_RUNNING;
		}
		netisr2_process_workstream(nwsp, NETISR_ALLPROT);
	}
}

/*
 * Internal routines for dispatch and queue.
 */
static void
netisr2_dispatch_internal(u_int proto, struct mbuf *m, u_int cpuid)
{
	struct netisr_workstream *nwsp;
	struct netisr_work *npwp;

	KASSERT(cpuid == curcpu, ("netisr2_dispatch_internal: wrong CPU"));

	NETISR_LOCK_ASSERT();

	nwsp = &nws[cpuid];
	npwp = &nwsp->nws_work[proto];
	NWS_LOCK(nwsp);
	npwp->nw_dispatched++;
	npwp->nw_handled++;
	NWS_UNLOCK(nwsp);
	np[proto].np_func(m);
}

static int
netisr2_queue_internal(u_int proto, struct mbuf *m, u_int cpuid)
{
	struct netisr_workstream *nwsp;
	struct netisr_work *npwp;
	int dosignal, error;

	NETISR_LOCK_ASSERT();

	dosignal = 0;
	nwsp = &nws[cpuid];
	npwp = &nwsp->nws_work[proto];
	NWS_LOCK(nwsp);
	if (npwp->nw_len < npwp->nw_max) {
		error = 0;
		m->m_nextpkt = NULL;
		if (npwp->nw_head == NULL) {
			npwp->nw_head = m;
			npwp->nw_tail = m;
		} else {
			npwp->nw_tail->m_nextpkt = m;
			npwp->nw_tail = m;
		}
		npwp->nw_len++;
		if (npwp->nw_len > npwp->nw_watermark)
			npwp->nw_watermark = npwp->nw_len;
		npwp->nw_queued++;
		nwsp->nws_pendingwork++;
		if (!(nwsp->nws_flags & NWS_SIGNALED)) {
			nwsp->nws_flags |= NWS_SIGNALED;
			dosignal = 1;	/* Defer until unlocked. */
		}
	} else {
		error = ENOBUFS;
		npwp->nw_dropped++;
	}
	NWS_UNLOCK(nwsp);
	if (dosignal)
		NWS_SIGNAL(nwsp);
	return (error);
}

/*
 * Variations on dispatch and queue in which the protocol determines where
 * work is placed.
 *
 * XXXRW: The fact that the strength of affinity is only available by making
 * a call to determine affinity means that we always pay the price of hashing
 * the headers.  If the protocol declared ahead of time the strength of the
 * affinity it required, such as at netisr2 registration time, we could skip
 * the hash generation when we knew we wanted to direct dispatch.
 */
int
netisr2_dispatch(u_int proto, struct mbuf *m)
{
	u_int cpuid, strength;
	int error;

	error = 0;
	sched_pin();
	NETISR_RLOCK();
	m = netisr2_selectcpu(proto, m, &cpuid, &strength);
	if (m == NULL) {
		error = ENOBUFS;
		goto out;
	}
	switch (strength) {
	case NETISR2_AFFINITY_STRONG:
		if (curcpu != cpuid) {
			error = netisr2_queue_internal(proto, m, cpuid);
			break;
		}
		/* FALLSTHROUGH */

	case NETISR2_AFFINITY_WEAK:
		if (netisr_direct) {
			cpuid = curcpu;
			netisr2_dispatch_internal(proto, m, cpuid);
		} else
			error = netisr2_queue_internal(proto, m, cpuid);
		break;
	}
out:
	NETISR_RUNLOCK();
	sched_unpin();
	if (error && m != NULL)
		m_freem(m);
	return (error);
}

int
netisr2_queue(u_int proto, struct mbuf *m)
{
	u_int cpuid, strength;
	int error;

	NETISR_RLOCK();
	m = netisr2_selectcpu(proto, m, &cpuid, &strength);
	if (m == NULL) {
		error = ENOBUFS;
		goto out;
	}
	error = netisr2_queue_internal(proto, m, cpuid);
out:
	NETISR_RUNLOCK();
	if (error && m != NULL)
		m_freem(m);
	return (error);
}

/*
 * Variations on dispatch and queue in which the caller specifies an explicit
 * CPU affinity.
 */
int
netisr2_dispatch_cpu(u_int proto, struct mbuf *m, u_int cpuid)
{
	int error;

	sched_pin();
	NETISR_RLOCK();
	if (cpuid == curcpu) {
		netisr2_dispatch_internal(proto, m, cpuid);
		error = 0;
	} else
		error = netisr2_queue_internal(proto, m, cpuid);
	NETISR_RUNLOCK();
	sched_unpin();
	return (error);
}

int
netisr2_queue_cpu(u_int proto, struct mbuf *m, u_int cpuid)
{
	int error;

	NETISR_RLOCK();
	error = netisr2_queue_internal(proto, m, cpuid);
	NETISR_RUNLOCK();
	return (error);
}

/*
 * Variations on dispatch and queue in which the caller specifies an explicit
 * flow identifier.
 */
int
netisr2_dispatch_flow(u_int proto, struct mbuf *m, u_int flowid)
{
	u_int cpuid;
	int error;

	sched_pin();
	NETISR_RLOCK();
	cpuid = netisr2_flowid2cpuid(flowid);
	if (cpuid == curcpu) {
		netisr2_dispatch_internal(proto, m, cpuid);
		error = 0;
	} else
		error = netisr2_queue_internal(proto, m, cpuid);
	NETISR_RUNLOCK();
	sched_unpin();
	return (error);
}

int
netisr2_queue_flow(u_int proto, struct mbuf *m, u_int flowid)
{
	u_int cpuid;
	int error;

	NETISR_RLOCK();
	cpuid = netisr2_flowid2cpuid(flowid);
	error = netisr2_queue_internal(proto, m, cpuid);
	NETISR_RUNLOCK();
	return (error);
}

/*
 * Initialize the netisr subsystem.  We rely on BSS and static initialization
 * of most fields in global data structures.  Start a worker thread for the
 * boot CPU.
 */
static void
netisr2_init(void *arg)
{
	struct netisr_workstream *nwsp;
	int cpuid, error;

	KASSERT(curcpu == 0, ("netisr2_init: not on CPU 0"));

	NETISR_LOCK_INIT();

	KASSERT(PCPU_GET(netisr2) == NULL, ("netisr2_init: pc_netisr2"));

	cpuid = curcpu;
	nwsp = &nws[cpuid];
	mtx_init(&nwsp->nws_mtx, "netisr2_mtx", NULL, MTX_DEF);
	cv_init(&nwsp->nws_cv, "netisr2_cv");
	nwsp->nws_cpu = cpuid;
	error = kproc_kthread_add(netisr2_worker, nwsp, &netisr2_proc,
	    &nwsp->nws_thread, 0, 0, "netisr2", "netisr2: cpu%d", cpuid);
	PCPU_SET(netisr2, nwsp->nws_thread);
	if (error)
		panic("netisr2_init: kproc_kthread_add %d", error);
	nws_array[nws_count] = nwsp->nws_cpu;
	nws_count++;
	if (netisr_maxthreads < 1)
		netisr_maxthreads = 1;
}
SYSINIT(netisr2_init, SI_SUB_SOFTINTR, SI_ORDER_FIRST, netisr2_init, NULL);

/*
 * Start worker threads for additional CPUs.  No attempt to gracefully handle
 * work reassignment, we don't yet support dynamic reconfiguration.
 */
static void
netisr2_start(void *arg)
{
	struct netisr_workstream *nwsp;
	struct pcpu *pc;
	int error;

	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (nws_count >= netisr_maxthreads)
			break;
		/* XXXRW: Is skipping absent CPUs still required here? */
		if (CPU_ABSENT(pc->pc_cpuid))
			continue;
		/* Worker will already be present for boot CPU. */
		if (pc->pc_netisr2 != NULL)
			continue;
		nwsp = &nws[pc->pc_cpuid];
		mtx_init(&nwsp->nws_mtx, "netisr2_mtx", NULL, MTX_DEF);
		cv_init(&nwsp->nws_cv, "netisr2_cv");
		nwsp->nws_cpu = pc->pc_cpuid;
		error = kproc_kthread_add(netisr2_worker, nwsp,
		    &netisr2_proc, &nwsp->nws_thread, 0, 0, "netisr2",
		    "netisr2: cpu%d", pc->pc_cpuid);
		pc->pc_netisr2 = nwsp->nws_thread;
		if (error)
			panic("netisr2_start: kproc_kthread_add %d", error);
		nws_array[nws_count] = pc->pc_cpuid;
		nws_count++;
	}
}
SYSINIT(netisr2_start, SI_SUB_SMP, SI_ORDER_MIDDLE, netisr2_start, NULL);

#ifdef DDB
DB_SHOW_COMMAND(netisr2, db_show_netisr2)
{
	struct netisr_workstream *nwsp;
	struct netisr_work *nwp;
	int cpu, first, proto;

	db_printf("%6s %6s %6s %6s %6s %6s %8s %8s %8s %8s\n", "CPU", "Pend",
	    "Proto", "Len", "WMark", "Max", "Disp", "Drop", "Queue", "Handle");
	for (cpu = 0; cpu < MAXCPU; cpu++) {
		nwsp = &nws[cpu];
		if (nwsp->nws_thread == NULL)
			continue;
		first = 1;
		for (proto = 0; proto < NETISR_MAXPROT; proto++) {
			if (np[proto].np_func == NULL)
				continue;
			nwp = &nwsp->nws_work[proto];
			if (first) {
				db_printf("%6d %6d ", cpu,
				    nwsp->nws_pendingwork);
				first = 0;
			} else
				db_printf("%6s %6s ", "", "");
			db_printf("%6s %6d %6d %6d %8d %8d %8d %8d\n",
			    np[proto].np_name, nwp->nw_len,
			    nwp->nw_watermark, nwp->nw_max,
			    nwp->nw_dispatched, nwp->nw_dropped,
			    nwp->nw_queued, nwp->nw_handled);
		}
	}
}
#endif
