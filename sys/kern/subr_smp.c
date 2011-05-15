/*-
 * Copyright (c) 2001, John Baldwin <jhb@FreeBSD.org>.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * This module holds the global variables and machine independent functions
 * used for the kernel SMP support.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/smp.h>

#include "opt_sched.h"

#ifdef SMP
volatile cpumask_t stopped_cpus;
volatile cpumask_t started_cpus;
volatile cpumask_t stopping_cpus;
volatile cpumask_t hard_stopped_cpus;
volatile cpumask_t hard_started_cpus;
volatile cpumask_t hard_stopping_cpus;
cpumask_t idle_cpus_mask;
cpumask_t hlt_cpus_mask;
cpumask_t logical_cpus_mask;

void (* volatile cpustop_hook)(void);
#endif
/* This is used in modules that need to work in both SMP and UP. */
cpumask_t all_cpus;

int mp_ncpus;
/* export this for libkvm consumers. */
int mp_maxcpus = MAXCPU;

volatile int smp_started;
u_int mp_maxid;

SYSCTL_NODE(_kern, OID_AUTO, smp, CTLFLAG_RD, NULL, "Kernel SMP");

SYSCTL_UINT(_kern_smp, OID_AUTO, maxid, CTLFLAG_RD, &mp_maxid, 0,
    "Max CPU ID.");

SYSCTL_INT(_kern_smp, OID_AUTO, maxcpus, CTLFLAG_RD, &mp_maxcpus, 0,
    "Max number of CPUs that the system was compiled for.");

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_kern_smp, OID_AUTO, active, CTLFLAG_RW, &smp_active, 0,
    "Number of Auxillary Processors (APs) that were successfully started");

int smp_disabled = 0;	/* has smp been disabled? */
SYSCTL_INT(_kern_smp, OID_AUTO, disabled, CTLFLAG_RDTUN, &smp_disabled, 0,
    "SMP has been disabled from the loader");
TUNABLE_INT("kern.smp.disabled", &smp_disabled);

int smp_cpus = 1;	/* how many cpu's running */
SYSCTL_INT(_kern_smp, OID_AUTO, cpus, CTLFLAG_RD, &smp_cpus, 0,
    "Number of CPUs online");

int smp_topology = 0;	/* Which topology we're using. */
SYSCTL_INT(_kern_smp, OID_AUTO, topology, CTLFLAG_RD, &smp_topology, 0,
    "Topology override setting; 0 is default provided by hardware.");
TUNABLE_INT("kern.smp.topology", &smp_topology);

unsigned int coalesced_ipi_count;
SYSCTL_INT(_kern_smp, OID_AUTO, coalesced_ipi_count, CTLFLAG_RD,
    &coalesced_ipi_count, 0, "Count of coalesced SMP rendezvous IPIs");

#ifdef SMP
/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_kern_smp, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0,
	   "Forwarding of a signal to a process on a different CPU");

/* Variables needed for SMP rendezvous. */
struct smp_rendezvous_data {
	void (*smp_rv_action_func)(void *arg);
	void *smp_rv_func_arg;
	int smp_rv_waiters;
	int smp_rv_ncpus;
};

volatile static DPCPU_DEFINE(struct smp_rendezvous_data, smp_rv_data);
static volatile DPCPU_DEFINE(cpumask_t, smp_rv_senders);
static volatile DPCPU_DEFINE(cpumask_t, smp_rv_count);

/*
 * Shared mutex to restrict busywaits between smp_rendezvous() and
 * smp(_targeted)_tlb_shootdown().  A deadlock occurs if both of these
 * functions trigger at once and cause multiple CPUs to busywait with
 * interrupts disabled.
 */
struct mtx smp_ipi_mtx;

/*
 * Let the MD SMP code initialize mp_maxid very early if it can.
 */
static void
mp_setmaxid(void *dummy)
{
	cpu_mp_setmaxid();
}
SYSINIT(cpu_mp_setmaxid, SI_SUB_TUNABLES, SI_ORDER_FIRST, mp_setmaxid, NULL);

/*
 * Call the MD SMP initialization code.
 */
static void
mp_start(void *dummy)
{

	mtx_init(&smp_ipi_mtx, "smp rendezvous", NULL, MTX_SPIN);

	/* Probe for MP hardware. */
	if (smp_disabled != 0 || cpu_mp_probe() == 0) {
		mp_ncpus = 1;
		all_cpus = PCPU_GET(cpumask);
		return;
	}

	cpu_mp_start();
	printf("FreeBSD/SMP: Multiprocessor System Detected: %d CPUs\n",
	    mp_ncpus);
	cpu_mp_announce();
}
SYSINIT(cpu_mp, SI_SUB_CPU, SI_ORDER_THIRD, mp_start, NULL);

void
forward_signal(struct thread *td)
{
	int id;

	/*
	 * signotify() has already set TDF_ASTPENDING and TDF_NEEDSIGCHECK on
	 * this thread, so all we need to do is poke it if it is currently
	 * executing so that it executes ast().
	 */
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(TD_IS_RUNNING(td),
	    ("forward_signal: thread is not TDS_RUNNING"));

	CTR1(KTR_SMP, "forward_signal(%p)", td->td_proc);

	if (!smp_started || cold || panicstr)
		return;
	if (!forward_signal_enabled)
		return;

	/* No need to IPI ourself. */
	if (td == curthread)
		return;

	id = td->td_oncpu;
	if (id == NOCPU)
		return;
	ipi_cpu(id, IPI_AST);
}

/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to stop.
 *  - Waits for each to stop.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 *
 */
static int
generic_stop_cpus(cpumask_t map, u_int type)
{
	static volatile u_int stopper_cpu = NOCPU;
	cpumask_t mask;
	int cpu;
	int i;

	KASSERT(
#if defined(__amd64__)
	    type == IPI_STOP || type == IPI_SUSPEND,
#else
	    type == IPI_STOP
#endif
	    ("%s: invalid stop type", __func__));

	if (!smp_started)
		return (0);

	CTR2(KTR_SMP, "stop_cpus(%x) with %u type", map, type);

	/* Ensure non-preemtable context, just in case. */
	spinlock_enter();

	mask = PCPU_GET(cpumask);
	cpu = PCPU_GET(cpuid);

	if (cpu != stopper_cpu) {
		while (atomic_cmpset_int(&stopper_cpu, NOCPU, cpu) == 0)
			while (stopper_cpu != NOCPU) {
				if ((mask & stopping_cpus) != 0)
					cpustop_handler();
				else
					cpu_spinwait();
			}
	} else {
		/*
		 * Recursion here is not expected.
		 */
		panic("cpu stop recursion\n");
	}

	/* send the stop IPI to all CPUs in map */
	stopping_cpus = map;
	ipi_selected(map, type);

	i = 0;
	while ((stopped_cpus & map) != map) {
		/* spin */
		cpu_spinwait();
		i++;
		if (i == 100000000) {
			printf("timeout stopping cpus\n");
			break;
		}
	}

	stopper_cpu = NOCPU;
	spinlock_exit();
	return (1);
}

int
stop_cpus(cpumask_t map)
{

	return (generic_stop_cpus(map, IPI_STOP));
}

#if defined(__amd64__)
int
suspend_cpus(cpumask_t map)
{

	return (generic_stop_cpus(map, IPI_SUSPEND));
}
#endif

/*
 * Called by a CPU to restart stopped CPUs.
 *
 * Usually (but not necessarily) called with 'stopped_cpus' as its arg.
 *
 *  - Signals all CPUs in map to restart.
 *  - Waits for each to restart.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 */
int
restart_cpus(cpumask_t map)
{

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	/* signal other cpus to restart */
	atomic_store_rel_int(&started_cpus, map);

	/* wait for each to clear its bit */
	while ((stopped_cpus & map) != 0)
		cpu_spinwait();

	return (1);
}

void
stop_cpus_hard(void)
{
	static volatile u_int hard_stopper_cpu = NOCPU;
	cpumask_t map;
	cpumask_t mask;
	u_int cpu;
	int i;

	if (!smp_started)
		return;

	/* Ensure non-preemtable context, just in case. */
	spinlock_enter();

	map = PCPU_GET(other_cpus);
	mask = PCPU_GET(cpumask);
	cpu = PCPU_GET(cpuid);

	CTR2(KTR_SMP, "stop_cpus(%x) with %u type", map, IPI_STOP_HARD);

	if (cpu != hard_stopper_cpu) {
		while (atomic_cmpset_int(&hard_stopper_cpu, NOCPU, cpu) == 0)
			while (hard_stopper_cpu != NOCPU) {
				if ((mask & hard_stopping_cpus) != 0)
					cpuhardstop_handler();
				else
					cpu_spinwait();
			}
	} else {
		/*
		 * Recursion here is not expected.
		 */
		atomic_store_rel_int(&hard_stopper_cpu, NOCPU);
		panic("hard stop recursion\n");
	}

	atomic_set_int(&hard_stopping_cpus, map);
	ipi_all_but_self(IPI_STOP_HARD);

	i = 0;
	while ((hard_stopped_cpus & map) != map) {
		cpu_spinwait();
		i++;
		if (i == 10000000) {
			/* Should not happen; other CPU stuck in NMI handler? */
			printf("timeout stopping cpus\n");
			break;
		}
	}

	atomic_store_rel_int(&hard_stopper_cpu, NOCPU);

	spinlock_exit();
	return;
}

void
unstop_cpus_hard(void)
{
	cpumask_t map;

	if (!smp_started)
		return;

	map = PCPU_GET(other_cpus);
	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	/* signal other cpus to restart */
	atomic_store_rel_int(&hard_started_cpus, map);

	/* wait for each to clear its bit */
	while ((hard_stopped_cpus & map) != 0)
		cpu_spinwait();
}

/*
 * All-CPU rendezvous.  CPUs are signalled, all execute the setup function
 * (if specified), rendezvous, execute the action function (if specified),
 * rendezvous again, execute the teardown function (if specified), and then
 * resume.
 *
 * Note that the supplied external functions _must_ be reentrant and aware
 * that they are running in parallel and in an unknown lock context.
 */
static void
smp_rendezvous_action_body(int cpu)
{
	volatile struct smp_rendezvous_data *rv;
	void *func_arg;
	void (*action_func)(void*);

	rv = DPCPU_ID_PTR(cpu, smp_rv_data);
	func_arg = rv->smp_rv_func_arg;
	action_func = rv->smp_rv_action_func;

	if (action_func != NULL)
		action_func(func_arg);

	atomic_add_int(&rv->smp_rv_waiters, 1);
}

static int
smp_rendezvous_action_pass(void)
{
	cpumask_t mask;
	int count;
	int cpu;

	count = 0;
	mask = DPCPU_GET(smp_rv_senders);
	if (mask == 0)
		return (count);

	atomic_clear_acq_int(DPCPU_PTR(smp_rv_senders), mask);
	do {
		count++;
		cpu = ffs(mask) - 1;
		mask &= ~(1 << cpu);
		smp_rendezvous_action_body(cpu);
	} while (mask != 0);

	return (count);
}

void
smp_rendezvous_action(void)
{
	int pending;
	int count;

	pending = DPCPU_GET(smp_rv_count);
	while (pending != 0) {
		KASSERT(pending > 0, ("negative pending rendezvous count"));
		count = smp_rendezvous_action_pass();
		if (count == 0) {
			cpu_spinwait();
			continue;
		}
		pending = atomic_fetchadd_int(DPCPU_PTR(smp_rv_count), -count);
		pending -= count;
	}
}

static void
smp_rendezvous_wait(volatile struct smp_rendezvous_data *rv)
{
	int ncpus;
	int count;

	ncpus = rv->smp_rv_ncpus;

	while (atomic_load_acq_int(&rv->smp_rv_waiters) < ncpus) {
		/* check for incoming events */
		if ((stopping_cpus & (1 << curcpu)) != 0)
			cpustop_handler();

		count = smp_rendezvous_action_pass();
		if (count != 0)
			atomic_add_int(DPCPU_PTR(smp_rv_count), -count);
		else
			cpu_spinwait();
	}
}


static void
smp_rendezvous_notify(int cpu, int nhold)
{
	int send_ipi;
	int x;

	if (cpu == curcpu)
		return;

	KASSERT((DPCPU_ID_GET(cpu, smp_rv_senders) & (1 << curcpu)) == 0,
	    ("curcpu bit is set in target cpu's senders map"));

	/*
	 * If this is a first action of a rendezvous invocation
	 * and we are the first to send an event, then send an ipi.
	 */
	send_ipi = 0;
	if (nhold != 0) {
		x = atomic_fetchadd_int(DPCPU_ID_PTR(cpu, smp_rv_count), nhold);
		send_ipi = (x == 0);
		if (!send_ipi)
			coalesced_ipi_count++;
	}

	atomic_set_rel_int(DPCPU_ID_PTR(cpu, smp_rv_senders), 1 << curcpu);
	if (send_ipi)
		ipi_cpu(cpu, IPI_RENDEZVOUS);
}

static void
smp_rendezvous_cpus_oneaction(cpumask_t map,
	volatile struct smp_rendezvous_data *rv,
	int nhold,
	void (*action_func)(void *),
	void *arg)
{
	cpumask_t tmp;
	int ncpus;
	int cpu;

	/*
	 * If nhold != 0, then this is the first call of a more complex
	 * rendezvous invocation, so we need to setup some common data
	 * for all calls and possibly send IPIs to target CPUs.
	 * Otherwise, we just need to set action_func and the incoming
	 * rendezvous bits.
	 */
	if (nhold != 0) {
		ncpus = 0;
		map &= all_cpus;
		tmp = map;
		while (tmp != 0) {
			cpu = ffs(tmp) - 1;
			tmp &= ~(1 << cpu);
			ncpus++;
		}

		rv->smp_rv_ncpus = ncpus;
		rv->smp_rv_func_arg = arg;
	}

	rv->smp_rv_action_func = action_func;
	atomic_store_rel_int(&rv->smp_rv_waiters, 0);

	tmp = map;
	while (tmp != 0) {
		cpu = ffs(tmp) - 1;
		tmp &= ~(1 << cpu);

		smp_rendezvous_notify(cpu, nhold);
	}

	/* Check if the current CPU is in the map */
	if ((map & (1 << curcpu)) != 0)
		smp_rendezvous_action_body(curcpu);
}

/*
 * Execute the action_func on the targeted CPUs.
 *
 * setup_func:
 * - if a function pointer is given, then first execute the function;
 *   only after the function is executed on all targeted can they proceed
 *   to the next step;
 * - if NULL is given, this is equivalent to specifying a pointer to an
 *   empty function; as such there is no actual setup function, but all
 *   targeted CPUs proceed to the next step at about the same time;
 * - smp_no_rendevous_barrier is a special value that signifies that there
 *   is no setup function nor the targeted CPUs should wait for anything
 *   before proceeding to the next step.
 *
 * action_func:
 * - a function to be executed on the targeted CPUs;
 *   NULL is equivalent to specifying a pointer to an empty function.
 *
 * teardown_func:
 * - if a function pointer is given, then first wait for all targeted CPUs
 *   to complete execution of action_func, then execute this function;
 * - if NULL is given, this is equivalent to specifying a pointer to an
 *   empty function; as such there is no actual teardown action, but all
 *   targeted CPUs wait for each other to complete execution of action_func;
 * - smp_no_rendevous_barrier is a special value that signifies that there
 *   is no teardown function nor the targeted CPUs should wait for anything
 *   after completing action_func.
 */
void
smp_rendezvous_cpus(cpumask_t map,
	void (* setup_func)(void *),
	void (* action_func)(void *),
	void (* teardown_func)(void *),
	void *arg)
{
	volatile struct smp_rendezvous_data *rv;
	int nhold;

	if (!smp_started) {
		if (setup_func != NULL)
			setup_func(arg);
		if (action_func != NULL)
			action_func(arg);
		if (teardown_func != NULL)
			teardown_func(arg);
		return;
	}

	nhold = 1;
	if (setup_func != smp_no_rendevous_barrier)
		nhold++;
	if (teardown_func != smp_no_rendevous_barrier)
		nhold++;

	spinlock_enter();
	rv = DPCPU_PTR(smp_rv_data);

	/* Let other CPUs know that we are here, no need to IPI us. */
	atomic_add_int(DPCPU_PTR(smp_rv_count), 1);

	/*
	 * First wait for an event previously posted by us, if any, to complete.
	 * In the future we could have a queue of outgoing events instead
	 * of a single item.
	 */
	smp_rendezvous_wait(rv);

	if (setup_func != smp_no_rendevous_barrier) {
		smp_rendezvous_cpus_oneaction(map, rv, nhold, setup_func, arg);
		smp_rendezvous_wait(rv);
		nhold = 0;
	}

	smp_rendezvous_cpus_oneaction(map, rv, nhold, action_func, arg);

	/*
	 * For now be compatible with historic smp_rendezvous semantics:
	 * if teardown_func is smp_no_rendevous_barrier, then the master
	 * CPU waits for target CPUs to complete main action.
	 * This means that we do not support completely async semantics
	 * (where the master CPU "fires and forgets" for time being..
	 */
#ifdef notyet
	if (teardown_func != smp_no_rendevous_barrier) {
		smp_rendezvous_wait(rv);
		smp_rendezvous_cpus_oneaction(map, rv, 0, teardown_func, arg);
	}
#else
	smp_rendezvous_wait(rv);
	if (teardown_func != smp_no_rendevous_barrier)
		smp_rendezvous_cpus_oneaction(map, rv, 0, teardown_func, arg);
#endif
	/* We are done with out work. */
	atomic_add_int(DPCPU_PTR(smp_rv_count), -1);

	/* Process all pending incoming actions. */
	smp_rendezvous_action();

	spinlock_exit();
}

void
smp_rendezvous(void (* setup_func)(void *),
	       void (* action_func)(void *),
	       void (* teardown_func)(void *),
	       void *arg)
{
	smp_rendezvous_cpus(all_cpus, setup_func, action_func,
	    teardown_func, arg);
}

static struct cpu_group group[MAXCPU];

struct cpu_group *
smp_topo(void)
{
	struct cpu_group *top;

	/*
	 * Check for a fake topology request for debugging purposes.
	 */
	switch (smp_topology) {
	case 1:
		/* Dual core with no sharing.  */
		top = smp_topo_1level(CG_SHARE_NONE, 2, 0);
		break;
	case 2:
		/* No topology, all cpus are equal. */
		top = smp_topo_none();
		break;
	case 3:
		/* Dual core with shared L2.  */
		top = smp_topo_1level(CG_SHARE_L2, 2, 0);
		break;
	case 4:
		/* quad core, shared l3 among each package, private l2.  */
		top = smp_topo_1level(CG_SHARE_L3, 4, 0);
		break;
	case 5:
		/* quad core,  2 dualcore parts on each package share l2.  */
		top = smp_topo_2level(CG_SHARE_NONE, 2, CG_SHARE_L2, 2, 0);
		break;
	case 6:
		/* Single-core 2xHTT */
		top = smp_topo_1level(CG_SHARE_L1, 2, CG_FLAG_HTT);
		break;
	case 7:
		/* quad core with a shared l3, 8 threads sharing L2.  */
		top = smp_topo_2level(CG_SHARE_L3, 4, CG_SHARE_L2, 8,
		    CG_FLAG_SMT);
		break;
	default:
		/* Default, ask the system what it wants. */
		top = cpu_topo();
		break;
	}
	/*
	 * Verify the returned topology.
	 */
	if (top->cg_count != mp_ncpus)
		panic("Built bad topology at %p.  CPU count %d != %d",
		    top, top->cg_count, mp_ncpus);
	if (top->cg_mask != all_cpus)
		panic("Built bad topology at %p.  CPU mask 0x%X != 0x%X",
		    top, top->cg_mask, all_cpus);
	return (top);
}

struct cpu_group *
smp_topo_none(void)
{
	struct cpu_group *top;

	top = &group[0];
	top->cg_parent = NULL;
	top->cg_child = NULL;
	top->cg_mask = all_cpus;
	top->cg_count = mp_ncpus;
	top->cg_children = 0;
	top->cg_level = CG_SHARE_NONE;
	top->cg_flags = 0;
	
	return (top);
}

static int
smp_topo_addleaf(struct cpu_group *parent, struct cpu_group *child, int share,
    int count, int flags, int start)
{
	cpumask_t mask;
	int i;

	for (mask = 0, i = 0; i < count; i++, start++)
		mask |= (1 << start);
	child->cg_parent = parent;
	child->cg_child = NULL;
	child->cg_children = 0;
	child->cg_level = share;
	child->cg_count = count;
	child->cg_flags = flags;
	child->cg_mask = mask;
	parent->cg_children++;
	for (; parent != NULL; parent = parent->cg_parent) {
		if ((parent->cg_mask & child->cg_mask) != 0)
			panic("Duplicate children in %p.  mask 0x%X child 0x%X",
			    parent, parent->cg_mask, child->cg_mask);
		parent->cg_mask |= child->cg_mask;
		parent->cg_count += child->cg_count;
	}

	return (start);
}

struct cpu_group *
smp_topo_1level(int share, int count, int flags)
{
	struct cpu_group *child;
	struct cpu_group *top;
	int packages;
	int cpu;
	int i;

	cpu = 0;
	top = &group[0];
	packages = mp_ncpus / count;
	top->cg_child = child = &group[1];
	top->cg_level = CG_SHARE_NONE;
	for (i = 0; i < packages; i++, child++)
		cpu = smp_topo_addleaf(top, child, share, count, flags, cpu);
	return (top);
}

struct cpu_group *
smp_topo_2level(int l2share, int l2count, int l1share, int l1count,
    int l1flags)
{
	struct cpu_group *top;
	struct cpu_group *l1g;
	struct cpu_group *l2g;
	int cpu;
	int i;
	int j;

	cpu = 0;
	top = &group[0];
	l2g = &group[1];
	top->cg_child = l2g;
	top->cg_level = CG_SHARE_NONE;
	top->cg_children = mp_ncpus / (l2count * l1count);
	l1g = l2g + top->cg_children;
	for (i = 0; i < top->cg_children; i++, l2g++) {
		l2g->cg_parent = top;
		l2g->cg_child = l1g;
		l2g->cg_level = l2share;
		for (j = 0; j < l2count; j++, l1g++)
			cpu = smp_topo_addleaf(l2g, l1g, l1share, l1count,
			    l1flags, cpu);
	}
	return (top);
}


struct cpu_group *
smp_topo_find(struct cpu_group *top, int cpu)
{
	struct cpu_group *cg;
	cpumask_t mask;
	int children;
	int i;

	mask = (1 << cpu);
	cg = top;
	for (;;) {
		if ((cg->cg_mask & mask) == 0)
			return (NULL);
		if (cg->cg_children == 0)
			return (cg);
		children = cg->cg_children;
		for (i = 0, cg = cg->cg_child; i < children; cg++, i++)
			if ((cg->cg_mask & mask) != 0)
				break;
	}
	return (NULL);
}
#else /* !SMP */

void
smp_rendezvous_cpus(cpumask_t map,
	void (*setup_func)(void *), 
	void (*action_func)(void *),
	void (*teardown_func)(void *),
	void *arg)
{
	if (setup_func != NULL)
		setup_func(arg);
	if (action_func != NULL)
		action_func(arg);
	if (teardown_func != NULL)
		teardown_func(arg);
}

void
smp_rendezvous(void (*setup_func)(void *), 
	       void (*action_func)(void *),
	       void (*teardown_func)(void *),
	       void *arg)
{

	if (setup_func != NULL)
		setup_func(arg);
	if (action_func != NULL)
		action_func(arg);
	if (teardown_func != NULL)
		teardown_func(arg);
}

/*
 * Provide dummy SMP support for UP kernels.  Modules that need to use SMP
 * APIs will still work using this dummy support.
 */
static void
mp_setvariables_for_up(void *dummy)
{
	mp_ncpus = 1;
	mp_maxid = PCPU_GET(cpuid);
	all_cpus = PCPU_GET(cpumask);
	KASSERT(PCPU_GET(cpuid) == 0, ("UP must have a CPU ID of zero"));
}
SYSINIT(cpu_mp_setvariables, SI_SUB_TUNABLES, SI_ORDER_FIRST,
    mp_setvariables_for_up, NULL);
#endif /* SMP */

void
smp_no_rendevous_barrier(void *dummy)
{
#ifdef SMP
	KASSERT((!smp_started),("smp_no_rendevous called and smp is started"));
#endif
}
