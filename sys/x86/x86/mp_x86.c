/*-
 * Copyright (c) 1996, by Steve Passe
 * Copyright (c) 2003, by Peter Wemm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifdef __i386__
#include "opt_apic.h"
#endif
#include "opt_cpu.h"
#include "opt_kstack_pages.h"
#include "opt_pmap.h"
#include "opt_sched.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>	/* cngetc() */
#include <sys/cpuset.h>
#ifdef GPROF 
#include <sys/gmon.h>
#endif
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <x86/apicreg.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <machine/cpu.h>

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(KERNBASE + 0x0467)
#define WARMBOOT_SEG		(KERNBASE + 0x0469)

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

/* lock region used by kernel profiling */
int	mcount_lock;

int	mp_naps;		/* # of Applications processors */
int	boot_cpu_id = -1;	/* designated BSP */

extern	struct pcpu __pcpu[];

/* AP uses this during bootstrap.  Do not staticize.  */
char *bootSTK;
int bootAP;

/* Free these after use */
void *bootstacks[MAXCPU];
void *dpcpu;

struct pcb stoppcbs[MAXCPU];
struct susppcb **susppcbs;

#ifdef COUNT_IPIS
/* Interrupt counts. */
static u_long *ipi_preempt_counts[MAXCPU];
static u_long *ipi_ast_counts[MAXCPU];
u_long *ipi_invltlb_counts[MAXCPU];
u_long *ipi_invlrng_counts[MAXCPU];
u_long *ipi_invlpg_counts[MAXCPU];
u_long *ipi_invlcache_counts[MAXCPU];
u_long *ipi_rendezvous_counts[MAXCPU];
static u_long *ipi_hardclock_counts[MAXCPU];
#endif

/* Default cpu_ops implementation. */
struct cpu_ops cpu_ops;

/*
 * Local data and functions.
 */

static volatile cpuset_t ipi_stop_nmi_pending;

/* used to hold the AP's until we are ready to release them */
struct mtx ap_boot_mtx;

/* Set to 1 once we're ready to let the APs out of the pen. */
volatile int aps_ready = 0;

/*
 * Store data from cpu_add() until later in the boot when we actually setup
 * the APs.
 */
struct cpu_info cpu_info[MAX_APIC_ID + 1];
int cpu_apic_ids[MAXCPU];
int apic_cpuids[MAX_APIC_ID + 1];

/* Holds pending bitmap based IPIs per CPU */
volatile u_int cpu_ipi_pending[MAXCPU];

int cpu_logical;		/* logical cpus per core */
int cpu_cores;			/* cores per package */

static void	release_aps(void *dummy);

static u_int	hyperthreading_cpus;	/* logical cpus sharing L1 cache */
static int	hyperthreading_allowed = 1;

void
mem_range_AP_init(void)
{

	if (mem_range_softc.mr_op && mem_range_softc.mr_op->initAP)
		mem_range_softc.mr_op->initAP(&mem_range_softc);
}

static void
topo_probe_amd(void)
{
	int core_id_bits;
	int id;

	/* AMD processors do not support HTT. */
	cpu_logical = 1;

	if ((amd_feature2 & AMDID2_CMP) == 0) {
		cpu_cores = 1;
		return;
	}

	core_id_bits = (cpu_procinfo2 & AMDID_COREID_SIZE) >>
	    AMDID_COREID_SIZE_SHIFT;
	if (core_id_bits == 0) {
		cpu_cores = (cpu_procinfo2 & AMDID_CMP_CORES) + 1;
		return;
	}

	/* Fam 10h and newer should get here. */
	for (id = 0; id <= MAX_APIC_ID; id++) {
		/* Check logical CPU availability. */
		if (!cpu_info[id].cpu_present || cpu_info[id].cpu_disabled)
			continue;
		/* Check if logical CPU has the same package ID. */
		if ((id >> core_id_bits) != (boot_cpu_id >> core_id_bits))
			continue;
		cpu_cores++;
	}
}

/*
 * Round up to the next power of two, if necessary, and then
 * take log2.
 * Returns -1 if argument is zero.
 */
static __inline int
mask_width(u_int x)
{

	return (fls(x << (1 - powerof2(x))) - 1);
}

static void
topo_probe_0x4(void)
{
	u_int p[4];
	int pkg_id_bits;
	int core_id_bits;
	int max_cores;
	int max_logical;
	int id;

	/* Both zero and one here mean one logical processor per package. */
	max_logical = (cpu_feature & CPUID_HTT) != 0 ?
	    (cpu_procinfo & CPUID_HTT_CORES) >> 16 : 1;
	if (max_logical <= 1)
		return;

	/*
	 * Because of uniformity assumption we examine only
	 * those logical processors that belong to the same
	 * package as BSP.  Further, we count number of
	 * logical processors that belong to the same core
	 * as BSP thus deducing number of threads per core.
	 */
	if (cpu_high >= 0x4) {
		cpuid_count(0x04, 0, p);
		max_cores = ((p[0] >> 26) & 0x3f) + 1;
	} else
		max_cores = 1;
	core_id_bits = mask_width(max_logical/max_cores);
	if (core_id_bits < 0)
		return;
	pkg_id_bits = core_id_bits + mask_width(max_cores);

	for (id = 0; id <= MAX_APIC_ID; id++) {
		/* Check logical CPU availability. */
		if (!cpu_info[id].cpu_present || cpu_info[id].cpu_disabled)
			continue;
		/* Check if logical CPU has the same package ID. */
		if ((id >> pkg_id_bits) != (boot_cpu_id >> pkg_id_bits))
			continue;
		cpu_cores++;
		/* Check if logical CPU has the same package and core IDs. */
		if ((id >> core_id_bits) == (boot_cpu_id >> core_id_bits))
			cpu_logical++;
	}

	KASSERT(cpu_cores >= 1 && cpu_logical >= 1,
	    ("topo_probe_0x4 couldn't find BSP"));

	cpu_cores /= cpu_logical;
	hyperthreading_cpus = cpu_logical;
}

static void
topo_probe_0xb(void)
{
	u_int p[4];
	int bits;
	int cnt;
	int i;
	int logical;
	int type;
	int x;

	/* We only support three levels for now. */
	for (i = 0; i < 3; i++) {
		cpuid_count(0x0b, i, p);

		/* Fall back if CPU leaf 11 doesn't really exist. */
		if (i == 0 && p[1] == 0) {
			topo_probe_0x4();
			return;
		}

		bits = p[0] & 0x1f;
		logical = p[1] &= 0xffff;
		type = (p[2] >> 8) & 0xff;
		if (type == 0 || logical == 0)
			break;
		/*
		 * Because of uniformity assumption we examine only
		 * those logical processors that belong to the same
		 * package as BSP.
		 */
		for (cnt = 0, x = 0; x <= MAX_APIC_ID; x++) {
			if (!cpu_info[x].cpu_present ||
			    cpu_info[x].cpu_disabled)
				continue;
			if (x >> bits == boot_cpu_id >> bits)
				cnt++;
		}
		if (type == CPUID_TYPE_SMT)
			cpu_logical = cnt;
		else if (type == CPUID_TYPE_CORE)
			cpu_cores = cnt;
	}
	if (cpu_logical == 0)
		cpu_logical = 1;
	cpu_cores /= cpu_logical;
}

/*
 * Both topology discovery code and code that consumes topology
 * information assume top-down uniformity of the topology.
 * That is, all physical packages must be identical and each
 * core in a package must have the same number of threads.
 * Topology information is queried only on BSP, on which this
 * code runs and for which it can query CPUID information.
 * Then topology is extrapolated on all packages using the
 * uniformity assumption.
 */
void
topo_probe(void)
{
	static int cpu_topo_probed = 0;

	if (cpu_topo_probed)
		return;

	CPU_ZERO(&logical_cpus_mask);
	if (mp_ncpus <= 1)
		cpu_cores = cpu_logical = 1;
	else if (cpu_vendor_id == CPU_VENDOR_AMD)
		topo_probe_amd();
	else if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		/*
		 * See Intel(R) 64 Architecture Processor
		 * Topology Enumeration article for details.
		 *
		 * Note that 0x1 <= cpu_high < 4 case should be
		 * compatible with topo_probe_0x4() logic when
		 * CPUID.1:EBX[23:16] > 0 (cpu_cores will be 1)
		 * or it should trigger the fallback otherwise.
		 */
		if (cpu_high >= 0xb)
			topo_probe_0xb();
		else if (cpu_high >= 0x1)
			topo_probe_0x4();
	}

	/*
	 * Fallback: assume each logical CPU is in separate
	 * physical package.  That is, no multi-core, no SMT.
	 */
	if (cpu_cores == 0 || cpu_logical == 0)
		cpu_cores = cpu_logical = 1;
	cpu_topo_probed = 1;
}

struct cpu_group *
cpu_topo(void)
{
	int cg_flags;

	/*
	 * Determine whether any threading flags are
	 * necessry.
	 */
	topo_probe();
	if (cpu_logical > 1 && hyperthreading_cpus)
		cg_flags = CG_FLAG_HTT;
	else if (cpu_logical > 1)
		cg_flags = CG_FLAG_SMT;
	else
		cg_flags = 0;
	if (mp_ncpus % (cpu_cores * cpu_logical) != 0) {
		printf("WARNING: Non-uniform processors.\n");
		printf("WARNING: Using suboptimal topology.\n");
		return (smp_topo_none());
	}
	/*
	 * No multi-core or hyper-threaded.
	 */
	if (cpu_logical * cpu_cores == 1)
		return (smp_topo_none());
	/*
	 * Only HTT no multi-core.
	 */
	if (cpu_logical > 1 && cpu_cores == 1)
		return (smp_topo_1level(CG_SHARE_L1, cpu_logical, cg_flags));
	/*
	 * Only multi-core no HTT.
	 */
	if (cpu_cores > 1 && cpu_logical == 1)
		return (smp_topo_1level(CG_SHARE_L2, cpu_cores, cg_flags));
	/*
	 * Both HTT and multi-core.
	 */
	return (smp_topo_2level(CG_SHARE_L2, cpu_cores,
	    CG_SHARE_L1, cpu_logical, cg_flags));
}


void
cpu_add(u_int apic_id, char boot_cpu)
{

	if (apic_id > MAX_APIC_ID) {
		panic("SMP: APIC ID %d too high", apic_id);
		return;
	}
	KASSERT(cpu_info[apic_id].cpu_present == 0, ("CPU %d added twice",
	    apic_id));
	cpu_info[apic_id].cpu_present = 1;
	if (boot_cpu) {
		KASSERT(boot_cpu_id == -1,
		    ("CPU %d claims to be BSP, but CPU %d already is", apic_id,
		    boot_cpu_id));
		boot_cpu_id = apic_id;
		cpu_info[apic_id].cpu_bsp = 1;
	}
	if (mp_ncpus < MAXCPU) {
		mp_ncpus++;
		mp_maxid = mp_ncpus - 1;
	}
	if (bootverbose)
		printf("SMP: Added CPU %d (%s)\n", apic_id, boot_cpu ? "BSP" :
		    "AP");
}

void
cpu_mp_setmaxid(void)
{

	/*
	 * mp_ncpus and mp_maxid should be already set by calls to cpu_add().
	 * If there were no calls to cpu_add() assume this is a UP system.
	 */
	if (mp_ncpus == 0)
		mp_ncpus = 1;
}

int
cpu_mp_probe(void)
{

	/*
	 * Always record BSP in CPU map so that the mbuf init code works
	 * correctly.
	 */
	CPU_SETOF(0, &all_cpus);
	return (mp_ncpus > 1);
}

/*
 * Print various information about the SMP system hardware and setup.
 */
void
cpu_mp_announce(void)
{
	const char *hyperthread;
	int i;

	printf("FreeBSD/SMP: %d package(s) x %d core(s)",
	    mp_ncpus / (cpu_cores * cpu_logical), cpu_cores);
	if (hyperthreading_cpus > 1)
	    printf(" x %d HTT threads", cpu_logical);
	else if (cpu_logical > 1)
	    printf(" x %d SMT threads", cpu_logical);
	printf("\n");

	/* List active CPUs first. */
	printf(" cpu0 (BSP): APIC ID: %2d\n", boot_cpu_id);
	for (i = 1; i < mp_ncpus; i++) {
		if (cpu_info[cpu_apic_ids[i]].cpu_hyperthread)
			hyperthread = "/HT";
		else
			hyperthread = "";
		printf(" cpu%d (AP%s): APIC ID: %2d\n", i, hyperthread,
		    cpu_apic_ids[i]);
	}

	/* List disabled CPUs last. */
	for (i = 0; i <= MAX_APIC_ID; i++) {
		if (!cpu_info[i].cpu_present || !cpu_info[i].cpu_disabled)
			continue;
		if (cpu_info[i].cpu_hyperthread)
			hyperthread = "/HT";
		else
			hyperthread = "";
		printf("  cpu (AP%s): APIC ID: %2d (disabled)\n", hyperthread,
		    i);
	}
}

void
init_secondary_tail(void)
{
	u_int cpuid;

	/*
	 * On real hardware, switch to x2apic mode if possible.  Do it
	 * after aps_ready was signalled, to avoid manipulating the
	 * mode while BSP might still want to send some IPI to us
	 * (second startup IPI is ignored on modern hardware etc).
	 */
	lapic_xapic_mode();

	/* Initialize the PAT MSR. */
	pmap_init_pat();

	/* set up CPU registers and state */
	cpu_setregs();

	/* set up SSE/NX */
	initializecpu();

	/* set up FPU state on the AP */
#ifdef __amd64__
	fpuinit();
#else
	npxinit(false);
#endif

	if (cpu_ops.cpu_init)
		cpu_ops.cpu_init();

	/* A quick check from sanity claus */
	cpuid = PCPU_GET(cpuid);
	if (PCPU_GET(apic_id) != lapic_id()) {
		printf("SMP: cpuid = %d\n", cpuid);
		printf("SMP: actual apic_id = %d\n", lapic_id());
		printf("SMP: correct apic_id = %d\n", PCPU_GET(apic_id));
		panic("cpuid mismatch! boom!!");
	}

	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	mca_init();

	mtx_lock_spin(&ap_boot_mtx);

	/* Init local apic for irq's */
	lapic_setup(1);

	/* Set memory range attributes for this CPU to match the BSP */
	mem_range_AP_init();

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", cpuid);
	printf("SMP: AP CPU #%d Launched!\n", cpuid);

	/* Determine if we are a logical CPU. */
	/* XXX Calculation depends on cpu_logical being a power of 2, e.g. 2 */
	if (cpu_logical > 1 && PCPU_GET(apic_id) % cpu_logical != 0)
		CPU_SET(cpuid, &logical_cpus_mask);

	if (bootverbose)
		lapic_dump("AP");

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
	}

#ifdef __amd64__
	/*
	 * Enable global pages TLB extension
	 * This also implicitly flushes the TLB 
	 */
	load_cr4(rcr4() | CR4_PGE);
	if (pmap_pcid_enabled)
		load_cr4(rcr4() | CR4_PCIDE);
	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_ufssel);
#endif

	mtx_unlock_spin(&ap_boot_mtx);

	/* Wait until all the AP's are up. */
	while (atomic_load_acq_int(&smp_started) == 0)
		ia32_pause();

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	sched_throw(NULL);

	panic("scheduler returned us to %s", __func__);
	/* NOTREACHED */
}

/*******************************************************************
 * local functions and data
 */

/*
 * We tell the I/O APIC code about all the CPUs we want to receive
 * interrupts.  If we don't want certain CPUs to receive IRQs we
 * can simply not tell the I/O APIC code about them in this function.
 * We also do not tell it about the BSP since it tells itself about
 * the BSP internally to work with UP kernels and on UP machines.
 */
void
set_interrupt_apic_ids(void)
{
	u_int i, apic_id;

	for (i = 0; i < MAXCPU; i++) {
		apic_id = cpu_apic_ids[i];
		if (apic_id == -1)
			continue;
		if (cpu_info[apic_id].cpu_bsp)
			continue;
		if (cpu_info[apic_id].cpu_disabled)
			continue;

		/* Don't let hyperthreads service interrupts. */
		if (cpu_logical > 1 &&
		    apic_id % cpu_logical != 0)
			continue;

		intr_add_cpu(i);
	}
}

/*
 * Assign logical CPU IDs to local APICs.
 */
void
assign_cpu_ids(void)
{
	u_int i;

	TUNABLE_INT_FETCH("machdep.hyperthreading_allowed",
	    &hyperthreading_allowed);

	/* Check for explicitly disabled CPUs. */
	for (i = 0; i <= MAX_APIC_ID; i++) {
		if (!cpu_info[i].cpu_present || cpu_info[i].cpu_bsp)
			continue;

		if (hyperthreading_cpus > 1 && i % hyperthreading_cpus != 0) {
			cpu_info[i].cpu_hyperthread = 1;

			/*
			 * Don't use HT CPU if it has been disabled by a
			 * tunable.
			 */
			if (hyperthreading_allowed == 0) {
				cpu_info[i].cpu_disabled = 1;
				continue;
			}
		}

		/* Don't use this CPU if it has been disabled by a tunable. */
		if (resource_disabled("lapic", i)) {
			cpu_info[i].cpu_disabled = 1;
			continue;
		}
	}

	if (hyperthreading_allowed == 0 && hyperthreading_cpus > 1) {
		hyperthreading_cpus = 0;
		cpu_logical = 1;
	}

	/*
	 * Assign CPU IDs to local APIC IDs and disable any CPUs
	 * beyond MAXCPU.  CPU 0 is always assigned to the BSP.
	 *
	 * To minimize confusion for userland, we attempt to number
	 * CPUs such that all threads and cores in a package are
	 * grouped together.  For now we assume that the BSP is always
	 * the first thread in a package and just start adding APs
	 * starting with the BSP's APIC ID.
	 */
	mp_ncpus = 1;
	cpu_apic_ids[0] = boot_cpu_id;
	apic_cpuids[boot_cpu_id] = 0;
	for (i = boot_cpu_id + 1; i != boot_cpu_id;
	     i == MAX_APIC_ID ? i = 0 : i++) {
		if (!cpu_info[i].cpu_present || cpu_info[i].cpu_bsp ||
		    cpu_info[i].cpu_disabled)
			continue;

		if (mp_ncpus < MAXCPU) {
			cpu_apic_ids[mp_ncpus] = i;
			apic_cpuids[i] = mp_ncpus;
			mp_ncpus++;
		} else
			cpu_info[i].cpu_disabled = 1;
	}
	KASSERT(mp_maxid >= mp_ncpus - 1,
	    ("%s: counters out of sync: max %d, count %d", __func__, mp_maxid,
	    mp_ncpus));		
}

#ifdef COUNT_XINVLTLB_HITS
u_int xhits_gbl[MAXCPU];
u_int xhits_pg[MAXCPU];
u_int xhits_rng[MAXCPU];
static SYSCTL_NODE(_debug, OID_AUTO, xhits, CTLFLAG_RW, 0, "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, global, CTLFLAG_RW, &xhits_gbl,
    sizeof(xhits_gbl), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, page, CTLFLAG_RW, &xhits_pg,
    sizeof(xhits_pg), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, range, CTLFLAG_RW, &xhits_rng,
    sizeof(xhits_rng), "IU", "");

u_int ipi_global;
u_int ipi_page;
u_int ipi_range;
u_int ipi_range_size;
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_global, CTLFLAG_RW, &ipi_global, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_page, CTLFLAG_RW, &ipi_page, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range, CTLFLAG_RW, &ipi_range, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range_size, CTLFLAG_RW, &ipi_range_size,
    0, "");
#endif /* COUNT_XINVLTLB_HITS */

/*
 * Init and startup IPI.
 */
void
ipi_startup(int apic_id, int vector)
{

	/*
	 * This attempts to follow the algorithm described in the
	 * Intel Multiprocessor Specification v1.4 in section B.4.
	 * For each IPI, we allow the local APIC ~20us to deliver the
	 * IPI.  If that times out, we panic.
	 */

	/*
	 * first we do an INIT IPI: this INIT IPI might be run, resetting
	 * and running the target CPU. OR this INIT IPI might be latched (P5
	 * bug), CPU waiting for STARTUP IPI. OR this INIT IPI might be
	 * ignored.
	 */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_LEVEL |
	    APIC_LEVEL_ASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT, apic_id);
	lapic_ipi_wait(100);

	/* Explicitly deassert the INIT IPI. */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_LEVEL |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT,
	    apic_id);

	DELAY(10000);		/* wait ~10mS */

	/*
	 * next we do a STARTUP IPI: the previous INIT IPI might still be
	 * latched, (P5 bug) this 1st STARTUP would then terminate
	 * immediately, and the previously started INIT IPI would continue. OR
	 * the previous INIT IPI has already run. and this STARTUP IPI will
	 * run. OR the previous INIT IPI was ignored. and this STARTUP IPI
	 * will run.
	 */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_ASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	if (!lapic_ipi_wait(100))
		panic("Failed to deliver first STARTUP IPI to APIC %d",
		    apic_id);
	DELAY(200);		/* wait ~200uS */

	/*
	 * finally we do a 2nd STARTUP IPI: this 2nd STARTUP IPI should run IF
	 * the previous STARTUP IPI was cancelled by a latched INIT IPI. OR
	 * this STARTUP IPI will be ignored, as only ONE STARTUP IPI is
	 * recognized after hardware RESET or INIT IPI.
	 */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_ASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	if (!lapic_ipi_wait(100))
		panic("Failed to deliver second STARTUP IPI to APIC %d",
		    apic_id);

	DELAY(200);		/* wait ~200uS */
}

/*
 * Send an IPI to specified CPU handling the bitmap logic.
 */
void
ipi_send_cpu(int cpu, u_int ipi)
{
	u_int bitmap, old_pending, new_pending;

	KASSERT(cpu_apic_ids[cpu] != -1, ("IPI to non-existent CPU %d", cpu));

	if (IPI_IS_BITMAPED(ipi)) {
		bitmap = 1 << ipi;
		ipi = IPI_BITMAP_VECTOR;
		do {
			old_pending = cpu_ipi_pending[cpu];
			new_pending = old_pending | bitmap;
		} while  (!atomic_cmpset_int(&cpu_ipi_pending[cpu],
		    old_pending, new_pending));	
		if (old_pending)
			return;
	}
	lapic_ipi_vectored(ipi, cpu_apic_ids[cpu]);
}

void
ipi_bitmap_handler(struct trapframe frame)
{
	struct trapframe *oldframe;
	struct thread *td;
	int cpu = PCPU_GET(cpuid);
	u_int ipi_bitmap;

	critical_enter();
	td = curthread;
	td->td_intr_nesting_level++;
	oldframe = td->td_intr_frame;
	td->td_intr_frame = &frame;
	ipi_bitmap = atomic_readandclear_int(&cpu_ipi_pending[cpu]);
	if (ipi_bitmap & (1 << IPI_PREEMPT)) {
#ifdef COUNT_IPIS
		(*ipi_preempt_counts[cpu])++;
#endif
		sched_preempt(td);
	}
	if (ipi_bitmap & (1 << IPI_AST)) {
#ifdef COUNT_IPIS
		(*ipi_ast_counts[cpu])++;
#endif
		/* Nothing to do for AST */
	}
	if (ipi_bitmap & (1 << IPI_HARDCLOCK)) {
#ifdef COUNT_IPIS
		(*ipi_hardclock_counts[cpu])++;
#endif
		hardclockintr();
	}
	td->td_intr_frame = oldframe;
	td->td_intr_nesting_level--;
	critical_exit();
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(cpuset_t cpus, u_int ipi)
{
	int cpu;

	/*
	 * IPI_STOP_HARD maps to a NMI and the trap handler needs a bit
	 * of help in order to understand what is the source.
	 * Set the mask of receiving CPUs for this purpose.
	 */
	if (ipi == IPI_STOP_HARD)
		CPU_OR_ATOMIC(&ipi_stop_nmi_pending, &cpus);

	while ((cpu = CPU_FFS(&cpus)) != 0) {
		cpu--;
		CPU_CLR(cpu, &cpus);
		CTR3(KTR_SMP, "%s: cpu: %d ipi: %x", __func__, cpu, ipi);
		ipi_send_cpu(cpu, ipi);
	}
}

/*
 * send an IPI to a specific CPU.
 */
void
ipi_cpu(int cpu, u_int ipi)
{

	/*
	 * IPI_STOP_HARD maps to a NMI and the trap handler needs a bit
	 * of help in order to understand what is the source.
	 * Set the mask of receiving CPUs for this purpose.
	 */
	if (ipi == IPI_STOP_HARD)
		CPU_SET_ATOMIC(cpu, &ipi_stop_nmi_pending);

	CTR3(KTR_SMP, "%s: cpu: %d ipi: %x", __func__, cpu, ipi);
	ipi_send_cpu(cpu, ipi);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int ipi)
{
	cpuset_t other_cpus;

	other_cpus = all_cpus;
	CPU_CLR(PCPU_GET(cpuid), &other_cpus);
	if (IPI_IS_BITMAPED(ipi)) {
		ipi_selected(other_cpus, ipi);
		return;
	}

	/*
	 * IPI_STOP_HARD maps to a NMI and the trap handler needs a bit
	 * of help in order to understand what is the source.
	 * Set the mask of receiving CPUs for this purpose.
	 */
	if (ipi == IPI_STOP_HARD)
		CPU_OR_ATOMIC(&ipi_stop_nmi_pending, &other_cpus);

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	lapic_ipi_vectored(ipi, APIC_IPI_DEST_OTHERS);
}

int
ipi_nmi_handler()
{
	u_int cpuid;

	/*
	 * As long as there is not a simple way to know about a NMI's
	 * source, if the bitmask for the current CPU is present in
	 * the global pending bitword an IPI_STOP_HARD has been issued
	 * and should be handled.
	 */
	cpuid = PCPU_GET(cpuid);
	if (!CPU_ISSET(cpuid, &ipi_stop_nmi_pending))
		return (1);

	CPU_CLR_ATOMIC(cpuid, &ipi_stop_nmi_pending);
	cpustop_handler();
	return (0);
}

/*
 * Handle an IPI_STOP by saving our current context and spinning until we
 * are resumed.
 */
void
cpustop_handler(void)
{
	u_int cpu;

	cpu = PCPU_GET(cpuid);

	savectx(&stoppcbs[cpu]);

	/* Indicate that we are stopped */
	CPU_SET_ATOMIC(cpu, &stopped_cpus);

	/* Wait for restart */
	while (!CPU_ISSET(cpu, &started_cpus))
	    ia32_pause();

	CPU_CLR_ATOMIC(cpu, &started_cpus);
	CPU_CLR_ATOMIC(cpu, &stopped_cpus);

#if defined(__amd64__) && defined(DDB)
	amd64_db_resume_dbreg();
#endif

	if (cpu == 0 && cpustop_restartfunc != NULL) {
		cpustop_restartfunc();
		cpustop_restartfunc = NULL;
	}
}

/*
 * Handle an IPI_SUSPEND by saving our current context and spinning until we
 * are resumed.
 */
void
cpususpend_handler(void)
{
	u_int cpu;

	mtx_assert(&smp_ipi_mtx, MA_NOTOWNED);

	cpu = PCPU_GET(cpuid);
	if (savectx(&susppcbs[cpu]->sp_pcb)) {
#ifdef __amd64__
		fpususpend(susppcbs[cpu]->sp_fpususpend);
#else
		npxsuspend(susppcbs[cpu]->sp_fpususpend);
#endif
		wbinvd();
		CPU_SET_ATOMIC(cpu, &suspended_cpus);
	} else {
#ifdef __amd64__
		fpuresume(susppcbs[cpu]->sp_fpususpend);
#else
		npxresume(susppcbs[cpu]->sp_fpususpend);
#endif
		pmap_init_pat();
		initializecpu();
		PCPU_SET(switchtime, 0);
		PCPU_SET(switchticks, ticks);

		/* Indicate that we are resumed */
		CPU_CLR_ATOMIC(cpu, &suspended_cpus);
	}

	/* Wait for resume */
	while (!CPU_ISSET(cpu, &started_cpus))
		ia32_pause();

	if (cpu_ops.cpu_resume)
		cpu_ops.cpu_resume();
#ifdef __amd64__
	if (vmm_resume_p)
		vmm_resume_p();
#endif

	/* Resume MCA and local APIC */
	lapic_xapic_mode();
	mca_resume();
	lapic_setup(0);

	/* Indicate that we are resumed */
	CPU_CLR_ATOMIC(cpu, &suspended_cpus);
	CPU_CLR_ATOMIC(cpu, &started_cpus);
}


void
invlcache_handler(void)
{
#ifdef COUNT_IPIS
	(*ipi_invlcache_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	wbinvd();
	atomic_add_int(&smp_tlb_wait, 1);
}

/*
 * This is called once the rest of the system is up and running and we're
 * ready to let the AP's out of the pen.
 */
static void
release_aps(void *dummy __unused)
{

	if (mp_ncpus == 1) 
		return;
	atomic_store_rel_int(&aps_ready, 1);
	while (smp_started == 0)
		ia32_pause();
}
SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);

#ifdef COUNT_IPIS
/*
 * Setup interrupt counters for IPI handlers.
 */
static void
mp_ipi_intrcnt(void *dummy)
{
	char buf[64];
	int i;

	CPU_FOREACH(i) {
		snprintf(buf, sizeof(buf), "cpu%d:invltlb", i);
		intrcnt_add(buf, &ipi_invltlb_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:invlrng", i);
		intrcnt_add(buf, &ipi_invlrng_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:invlpg", i);
		intrcnt_add(buf, &ipi_invlpg_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:invlcache", i);
		intrcnt_add(buf, &ipi_invlcache_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:preempt", i);
		intrcnt_add(buf, &ipi_preempt_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:ast", i);
		intrcnt_add(buf, &ipi_ast_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:rendezvous", i);
		intrcnt_add(buf, &ipi_rendezvous_counts[i]);
		snprintf(buf, sizeof(buf), "cpu%d:hardclock", i);
		intrcnt_add(buf, &ipi_hardclock_counts[i]);
	}		
}
SYSINIT(mp_ipi_intrcnt, SI_SUB_INTR, SI_ORDER_MIDDLE, mp_ipi_intrcnt, NULL);
#endif

/*
 * Flush the TLB on other CPU's
 */

/* Variables needed for SMP tlb shootdown. */
static vm_offset_t smp_tlb_addr1, smp_tlb_addr2;
pmap_t smp_tlb_pmap;
volatile int smp_tlb_wait;

#ifdef __amd64__
#define	read_eflags() read_rflags()
#endif

static void
smp_targeted_tlb_shootdown(cpuset_t mask, u_int vector, pmap_t pmap,
    vm_offset_t addr1, vm_offset_t addr2)
{
	int cpu, ncpu, othercpus;

	othercpus = mp_ncpus - 1;	/* does not shootdown self */

	/*
	 * Check for other cpus.  Return if none.
	 */
	if (CPU_ISFULLSET(&mask)) {
		if (othercpus < 1)
			return;
	} else {
		CPU_CLR(PCPU_GET(cpuid), &mask);
		if (CPU_EMPTY(&mask))
			return;
	}

	if (!(read_eflags() & PSL_I))
		panic("%s: interrupts disabled", __func__);
	mtx_lock_spin(&smp_ipi_mtx);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	smp_tlb_pmap = pmap;
	smp_tlb_wait =  0;
	if (CPU_ISFULLSET(&mask)) {
		ncpu = othercpus;
		ipi_all_but_self(vector);
	} else {
		ncpu = 0;
		while ((cpu = CPU_FFS(&mask)) != 0) {
			cpu--;
			CPU_CLR(cpu, &mask);
			CTR3(KTR_SMP, "%s: cpu: %d ipi: %x", __func__,
			    cpu, vector);
			ipi_send_cpu(cpu, vector);
			ncpu++;
		}
	}
	while (smp_tlb_wait < ncpu)
		ia32_pause();
	mtx_unlock_spin(&smp_ipi_mtx);
}

void
smp_masked_invltlb(cpuset_t mask, pmap_t pmap)
{

	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLTLB, pmap, 0, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_global++;
#endif
	}
}

void
smp_masked_invlpg(cpuset_t mask, vm_offset_t addr)
{

	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLPG, NULL, addr, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_page++;
#endif
	}
}

void
smp_masked_invlpg_range(cpuset_t mask, vm_offset_t addr1, vm_offset_t addr2)
{

	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLRNG, NULL,
		    addr1, addr2);
#ifdef COUNT_XINVLTLB_HITS
		ipi_range++;
		ipi_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
	}
}

void
smp_cache_flush(void)
{

	if (smp_started) {
		smp_targeted_tlb_shootdown(all_cpus, IPI_INVLCACHE, NULL,
		    0, 0);
	}
}

/*
 * Handlers for TLB related IPIs
 */
void
invltlb_handler(void)
{
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	if (smp_tlb_pmap == kernel_pmap)
		invltlb_glob();
	else
		invltlb();
	atomic_add_int(&smp_tlb_wait, 1);
}

void
invlpg_handler(void)
{
#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	invlpg(smp_tlb_addr1);
	atomic_add_int(&smp_tlb_wait, 1);
}

void
invlrng_handler(void)
{
	vm_offset_t addr;

#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	addr = smp_tlb_addr1;
	do {
		invlpg(addr);
		addr += PAGE_SIZE;
	} while (addr < smp_tlb_addr2);

	atomic_add_int(&smp_tlb_wait, 1);
}
