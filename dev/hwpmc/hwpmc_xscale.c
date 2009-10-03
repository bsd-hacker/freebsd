/*-
 * Copyright (c) 2009 Rui Paulo <rpaulo@FreeBSD.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Support for the Intel XScale network processors */

#include <sys/param.h>
#include <sys/pmc.h>

#include <machine/pmc_mdep.h>

struct xscale_event_code_map {
	enum pmc_event	pe_ev;
	uint8_t		pe_code;
	uint8_t		pe_mask;
};


static int
xscale_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	return 0;
}

static int
xscale_write_pmc(int cpu, int ri, pmc_value_t v)
{
	return 0;
}

static int
xscale_config_pmc(int cpu, int ri, struct pmc *pm)
{
	return 0;
}

static int
xscale_start_pmc(int cpu, int ri)
{
	return 0;
}

static int
xscale_stop_pmc(int cpu, int ri)
{
	return 0;
}

static int
xscale_intr(int cpu, struct trapframe *tf)
{
	return 0;
}

static int
xscale_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	return 0;
}

static int
xscale_pcpu_init(struct pmc_mdep *md, int cpu)
{
	return 0;
}

static int
xscale_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	return 0;
}

struct pmc_mdep *
pmc_xscale_initialize()
{
	struct pmc_mdep *pmc_mdep;
	struct pmc_classdep *pcd;

	pmc_mdep = malloc(sizeof(struct pmc_mdep) + sizeof(struct pmc_classdep),
	    M_PMC, M_WAITOK|M_ZERO);
	pmc_mdep->pmd_cputype = PMC_CPU_INTEL_XSCALE;
	pmc_mdep->pmd_nclass = 1;
	pmc_mdep->pmd_npmc = XSCALE_NPMCS;

	pcd = &pmc_mdep->pmd_classdep[0];
	pcd->pcd_caps = XSCALE_PMC_CAPS;
	pcd->pcd_num = XSCALE_NPMCS;
	pcd->pcd_ri = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 48; /* XXX */

	//pcd->pcd_allocate_pmc = xscale_allocate_pmc;
	pcd->pcd_config_pmc = xscale_config_pmc;
	pcd->pcd_pcpu_fini = xscale_pcpu_fini;
	pcd->pcd_pcpu_init = xscale_pcpu_init;
	pcd->pcd_describe = xscale_describe;
	//pcd->pcd_get_config = xscale_get_config;
	pcd->pcd_read_pmc = xscale_read_pmc;
	//pcd->pcd_release_pmc = xscale_release_pmc;
	pcd->pcd_start_pmc = xscale_start_pmc;
	pcd->pcd_stop_pmc = xscale_stop_pmc;
	pcd->pcd_write_pmc = xscale_write_pmc;

	pmc_mdep->pmd_intr = xscale_intr;
	//pmc_mdep->pmd_switch_in = xscale_switch_in;
	//pmc_mdep->pmd_switch_out = xscale_switch_out;

	return (pmc_mdep);
}

void
pmc_xscale_finalize(struct pmc_mdep *md)
{
}
