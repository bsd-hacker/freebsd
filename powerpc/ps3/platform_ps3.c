/*-
 * Copyright (c) 2010 Nathan Whitehorn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/nwhitehorn/ps3/powerpc/booke/platform_ps3.c 193492 2009-06-05 09:46:00Z raj $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hid.h>
#include <machine/platform.h>
#include <machine/platformvar.h>
#include <machine/pmap.h>
#include <machine/smp.h>
#include <machine/spr.h>
#include <machine/vmparam.h>

#include <dev/ofw/openfirm.h>

#include "platform_if.h"
#include "ps3-hvcall.h"

#ifdef SMP
extern void *ap_pcpu;
#endif

static int ps3_probe(platform_t);
static int ps3_attach(platform_t);
static void ps3_mem_regions(platform_t, struct mem_region **phys, int *physsz,
    struct mem_region **avail, int *availsz);
static u_long ps3_timebase_freq(platform_t, struct cpuref *cpuref);
static int ps3_smp_first_cpu(platform_t, struct cpuref *cpuref);
static int ps3_smp_next_cpu(platform_t, struct cpuref *cpuref);
static int ps3_smp_get_bsp(platform_t, struct cpuref *cpuref);
static int ps3_smp_start_cpu(platform_t, struct pcpu *cpu);

static platform_method_t ps3_methods[] = {
	PLATFORMMETHOD(platform_probe, 		ps3_probe),
	PLATFORMMETHOD(platform_attach,		ps3_attach),
	PLATFORMMETHOD(platform_mem_regions,	ps3_mem_regions),
	PLATFORMMETHOD(platform_timebase_freq,	ps3_timebase_freq),

	PLATFORMMETHOD(platform_smp_first_cpu,	ps3_smp_first_cpu),
	PLATFORMMETHOD(platform_smp_next_cpu,	ps3_smp_next_cpu),
	PLATFORMMETHOD(platform_smp_get_bsp,	ps3_smp_get_bsp),
	PLATFORMMETHOD(platform_smp_start_cpu,	ps3_smp_start_cpu),

	{ 0, 0 }
};

static platform_def_t ps3_platform = {
	"ps3",
	ps3_methods,
	0
};

PLATFORM_DEF(ps3_platform);

static int
ps3_probe(platform_t plat)
{
#if 0
	phandle_t root;
	char compatible[64];

	root = OF_finddevice("/");

	if (OF_getprop(root, "compatible", compatible, sizeof(compatible)) <= 0)
		return (ENXIO);

	if (strncmp(compatible, "sony,ps3", sizeof(compatible)) == 0)
		return (BUS_PROBE_SPECIFIC);

	return (ENXIO);
#else
	return (BUS_PROBE_NOWILDCARD);
#endif
}

static int
ps3_attach(platform_t plat)
{

	pmap_mmu_install("mmu_ps3", BUS_PROBE_SPECIFIC);

	return (0);
}

#define MEM_REGIONS	8
static struct mem_region avail_regions[MEM_REGIONS];

void
ps3_mem_regions(platform_t plat, struct mem_region **phys, int *physsz,
    struct mem_region **avail, int *availsz)
{
	uint64_t lpar_id, junk, ppe_id;

	avail_regions[0].mr_start = 0;
	lv1_get_logical_partition_id(&lpar_id);
	lv1_get_logical_ppe_id(&ppe_id);
	lv1_get_repository_node_value(lpar_id,
	    lv1_repository_string("bi") >> 32, lv1_repository_string("pu"),
	    ppe_id, lv1_repository_string("rm_size"),
	    &avail_regions[0].mr_size, &junk);

	*phys = *avail = avail_regions;
	*physsz = *availsz = 1;
}

static u_long
ps3_timebase_freq(platform_t plat, struct cpuref *cpuref)
{
	uint64_t ticks, node_id, junk;

	lv1_get_repository_node_value(PS3_LPAR_ID_PME, 
	    lv1_repository_string("be") >> 32, 0, 0, 0, &node_id, &junk);
	lv1_get_repository_node_value(PS3_LPAR_ID_PME,
	    lv1_repository_string("be") >> 32, node_id,
	    lv1_repository_string("clock"), 0, &ticks, &junk);

	return (ticks);
}

static int
ps3_smp_first_cpu(platform_t plat, struct cpuref *cpuref)
{

	cpuref->cr_cpuid = 0;
	cpuref->cr_hwref = cpuref->cr_cpuid;

	return (0);
}

static int
ps3_smp_next_cpu(platform_t plat, struct cpuref *cpuref)
{

	if (cpuref->cr_cpuid >= 1)
		return (ENOENT);

	cpuref->cr_cpuid++;
	cpuref->cr_hwref = cpuref->cr_cpuid;

	return (0);
}

static int
ps3_smp_get_bsp(platform_t plat, struct cpuref *cpuref)
{

	cpuref->cr_cpuid = 0;
	cpuref->cr_hwref = cpuref->cr_cpuid;

	return (0);
}

static int
ps3_smp_start_cpu(platform_t plat, struct pcpu *pc)
{
#ifdef SMP
	/* loader(8) is spinning on 0x40 == 0 right now */
	uint32_t *secondary_spin_sem = (uint32_t *)(0x40);
	int timeout;

	if (pc->pc_hwref != 1)
		return (ENXIO);

	ap_pcpu = pc;
	*secondary_spin_sem = 1;
	powerpc_sync();
	DELAY(1);

	timeout = 10000;
	while (!pc->pc_awake && timeout--)
		DELAY(100);

	return ((pc->pc_awake) ? 0 : EBUSY);
#else
	/* No SMP support */
	return (ENXIO);
#endif
}

