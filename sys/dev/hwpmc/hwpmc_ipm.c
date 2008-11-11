/*-
 * Copyright (c) 2003-2005 Joseph Koshy
 * Copyright (c) 2007-2008 Nokia Corporation
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

/*
 * Add Performance Monitoring (PM) support for the processor based on Intel's 
 * Core Solo/Duo architecture and Intel Core microarchitecture.
 * This CPUs support Intel's Architectural PM Version 1/2 functionality.
 * Intel(R) 64 and IA-32 Architectures Software Developer's Manual: 
 * System Programming Guide: Volue 3B: Section 18.12
 *
 * Intel Core Solo/Duo processors introduced architectural PM. 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/pmc_mdep.h>
#include <machine/specialreg.h>

/*
 * I had not had so much trouble coding as giving sensible names.
 * I am sticking to 'ipm' prefix (Intel Performance Monitoring).
 * Where it really makes difference it will further be qualified by
 * 'ipm_v1' or 'ipm_v2'.
 * Bear with me.
 */

/*
 * Globals set from the initialization routine based on the 
 * CPU type
 */
static enum pmc_cputype ipm_cputype;
static const struct ipm_event_descr *ipm_events = NULL;
static const struct ipm_pmc_descr *ipm_pmc_desc;
static int ipm_nevents = 0;
static int ipm_npmcs = 0;

#define	IPM_PMC_CAPS (PMC_CAP_INTERRUPT | PMC_CAP_USER | PMC_CAP_SYSTEM | \
    PMC_CAP_EDGE | PMC_CAP_THRESHOLD | PMC_CAP_READ | PMC_CAP_WRITE |	 \
    PMC_CAP_INVERT | PMC_CAP_QUALIFIER)

#define	IPM_FIXEDPMC_CAPS (PMC_CAP_INTERRUPT | PMC_CAP_USER | \
    PMC_CAP_SYSTEM | PMC_CAP_READ | PMC_CAP_WRITE)

static inline int
ipm_v2_pmc_has_overflowed(uint64_t ovf_status, int ri)
{
	return ((ri < 3) ?
	    (ovf_status & (0x1LL << (ri - 1))) :
	    (ovf_status & (0x100000000LL << (ri - 3))));
}


static inline uint64_t
ipm_v2_pmc_ri_to_msr_en_bit(int ri) 
{
	return ((ri < 3) ?
	    (0x1LL << (ri - 1)) : 
	    (0x100000000LL << (ri - 3)));
}

struct ipm_pmc_descr {
	struct pmc_descr pm_descr; /* common information */
	uint32_t	pm_pmc_msr;
	uint32_t	pm_evsel_msr;
};

static const struct ipm_pmc_descr ipm_pmc_desc_v1[IPM_NPMCS_V1] = {
	/* TSC */
	{
		.pm_descr =
		{
			.pd_name  = "TSC",
			.pd_class = PMC_CLASS_TSC,
			.pd_caps  = PMC_CAP_READ,
			.pd_width = 64
		},
		.pm_pmc_msr   = 0x10,
		.pm_evsel_msr = ~0
	},

	/* PMC 0 */
	{
		.pm_descr =
		{
			.pd_name  = "IPM-PMC-1",
			.pd_class = PMC_CLASS_IAP1,
			.pd_caps  = IPM_PMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr   = IPM_MSR_PERFCTR0,
		.pm_evsel_msr = IPM_MSR_EVSEL0
	},

	/* PMC 1 */
	{
		.pm_descr =
		{
			.pd_name  = "IPM-PMC-2",
			.pd_class = PMC_CLASS_IAP1,
			.pd_caps  = IPM_PMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr   = IPM_MSR_PERFCTR1,
		.pm_evsel_msr = IPM_MSR_EVSEL1
	},
};

static const struct ipm_pmc_descr ipm_pmc_desc_v2[IPM_NPMCS_V2] = {
	/* TSC */
	{
		.pm_descr =
		{
			.pd_name  = "TSC",
			.pd_class = PMC_CLASS_TSC,
			.pd_caps  = PMC_CAP_READ,
			.pd_width = 64
		},
		.pm_pmc_msr   = 0x10,
		.pm_evsel_msr = ~0
	},

	/* PMC 0 */
	{
		.pm_descr =
		{
			.pd_name  = "IPM-PMC-1",
			.pd_caps  = IPM_PMC_CAPS,
			.pd_class = PMC_CLASS_IAP2,
			.pd_width = 40
		},
		.pm_pmc_msr   = IPM_MSR_PERFCTR0,
		.pm_evsel_msr = IPM_MSR_EVSEL0
	},

	/* PMC 1 */
	{
		.pm_descr =
		{
			.pd_name  = "IPM-PMC-2",
			.pd_class = PMC_CLASS_IAP2,
			.pd_caps  = IPM_PMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr   = IPM_MSR_PERFCTR1,
		.pm_evsel_msr = IPM_MSR_EVSEL1
	},

	/*
	 * Following 3 fixed function performance counters are only available
	 * in processors supporting PM version 2 facilities
	 */
	/* Fixed function PMC 1 */
	{
		.pm_descr = 
		{
			.pd_name  = "IPM-FIXED-PMC-1",
			.pd_class = PMC_CLASS_IAF,
			.pd_caps =  IPM_FIXEDPMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr	= IPM_MSR_PERFFIXEDCTR0,
		.pm_evsel_msr	= IPM_MSR_PERFFIXEDCTR_CTRL
	},

	/* Fixed function PMC 2 */
	{
		.pm_descr = 
		{
			.pd_name  = "IPM-FIXED-PMC-2",
			.pd_class = PMC_CLASS_IAF,
			.pd_caps =  IPM_FIXEDPMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr	= IPM_MSR_PERFFIXEDCTR1,
		.pm_evsel_msr	= IPM_MSR_PERFFIXEDCTR_CTRL
	},

	/* Fixed function PMC 3 */
	{
		.pm_descr = 
		{
			.pd_name  = "IPM-FIXED-PMC-3",
			.pd_class = PMC_CLASS_IAF,
			.pd_caps =  IPM_FIXEDPMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr	= IPM_MSR_PERFFIXEDCTR2,
		.pm_evsel_msr	= IPM_MSR_PERFFIXEDCTR_CTRL
	}
};

/*
 * Event descriptors
 */
struct ipm_event_descr {
	const enum pmc_event pm_event;
	uint32_t	     pm_evsel;
	uint32_t	     pm_umask;
	uint32_t	     pm_umask_allowed;
};

/*
 * Fixed function PM events. Processors based on Intel Core Microarchitecture
 * have 3 fixed function PMCs. The following 3 PM events can only be 
 * programmed on fixed function PMCs
 */
static const struct ipm_event_descr ipm_events_fixed[] = {
#define	IPM_EVDESCR(NAME, EVSEL, UMASK)			\
	{						\
		.pm_event = PMC_EV_IAF_##NAME,	\
		.pm_evsel = (EVSEL),			\
		.pm_umask = (UMASK),			\
		.pm_umask_allowed = 0			\
	}

IPM_EVDESCR(INSTRUCTIONS_RETIRED,		0xC0,	0x00),
IPM_EVDESCR(UNHALTED_CORE_CYCLES,		0x3C,	0x00),
IPM_EVDESCR(UNHALTED_REFERENCE_CYCLES,		0x3C,	0x01),

#undef	IPM_EVDESCR
};
#define	IPM_NEVENTS_FIXED	3

/*
 * The following events can be programmed on IA32_PMC0/1 of processors based
 * on Intel Core Solo/Duo processors.
 */
static const struct ipm_event_descr ipm_events_v1[] = {
#define	IPM_EVDESCR(NAME, EVSEL, UMASK, ALLOW)		\
	{						\
		.pm_event = PMC_EV_IAP1_##NAME,	\
		.pm_evsel = (EVSEL),			\
		.pm_umask = (UMASK),			\
		.pm_umask_allowed = (ALLOW)		\
	}
IPM_EVDESCR(INSTRUCTIONS_RETIRED,		0xC0,	0x00,	0),
IPM_EVDESCR(UNHALTED_CORE_CYCLES,		0x3C,	0x00,	0),
IPM_EVDESCR(UNHALTED_REFERENCE_CYCLES,		0x3C,	0x01,	0),
IPM_EVDESCR(LLC_REFERENCE,			0x2E,	0x4F,	0),
IPM_EVDESCR(LLC_MISSES,				0x2E,	0x41,	0),
IPM_EVDESCR(BRANCH_INSTRUCTION_RETIRED,		0xC4,	0x00,	0),
IPM_EVDESCR(BRANCH_MISSES_RETIRED,		0xC5,	0x00,	0),

IPM_EVDESCR(LD_BLOCKS,				0x03,	0x00,	0),
IPM_EVDESCR(SD_DRAINS, 				0x04,	0x00,	0),
IPM_EVDESCR(MISALIGN_MEM_REF, 			0x05,	0x00,	0),
IPM_EVDESCR(SEG_REG_LOADS, 			0x06,	0x00,	0),
IPM_EVDESCR(SSE_PREFNTA_RET, 			0x07,	0x00,	0),
IPM_EVDESCR(SSE_PREFT1_RET, 			0x07,	0x01,	0),	
IPM_EVDESCR(SSE_PREFT2_RET, 			0x07,	0x02,	0),
IPM_EVDESCR(SSE_NTSTORES_RET, 			0x07,	0x03,	0),
IPM_EVDESCR(FP_COMPS_OP_EXE, 			0x10,	0x00,	0),
IPM_EVDESCR(FP_ASSIST, 				0x11,	0x00,	0),
IPM_EVDESCR(MUL, 				0x12,	0x00,	0),
IPM_EVDESCR(DIV, 				0x13,	0x00,	0),
IPM_EVDESCR(CYCLES_DIV_BUSY, 			0x14,	0x00,	0),
IPM_EVDESCR(L2_ADS,				0x21,	0xC0,	1),
IPM_EVDESCR(DBUS_BUSY, 				0x22,	0xC0,	1),
IPM_EVDESCR(DBUS_BUSY_RD, 			0x23,	0xC0,	1),
IPM_EVDESCR(L2_LINES_IN, 			0x24,	0XF0,	1),
IPM_EVDESCR(L2_M_LINES_IN, 			0x25,	0xC0,	1),
IPM_EVDESCR(L2_LINES_OUT, 			0x26,	0xF0,	1),
IPM_EVDESCR(L2_M_LINES_OUT, 			0x27,	0xF0,	1),
IPM_EVDESCR(L2_IFETCH, 				0x28,	0xC0,	1),
IPM_EVDESCR(L2_LD, 				0x29,	0xC0,	1),
IPM_EVDESCR(L2_ST, 				0x2A,	0xC0,	1),
IPM_EVDESCR(L2_RQSTS, 				0x2E,	0xF0,	1),
IPM_EVDESCR(L2_REJECT_CYCLES, 			0x30,	0xF0,	1),
IPM_EVDESCR(L2_NO_REQUEST_CYCLES, 		0x32,	0xF0,	1),
IPM_EVDESCR(EST_TRANS_ALL, 			0x3A,	0x00,	0),
IPM_EVDESCR(EST_TRANS_FREQ, 			0x3A,	0x10,	0),
IPM_EVDESCR(THERMAL_TRIP, 			0x3B,	0xC0,	0),
IPM_EVDESCR(NONHLT_REF_CYCLES, 			0x3C,	0x01,	0),
IPM_EVDESCR(SERIAL_EXECUTION_CYCLES, 		0x3C,	0x02,	0),
IPM_EVDESCR(DCACHE_CACHE_LD, 			0x40,	0x0F,	1),
IPM_EVDESCR(DCACHE_CACHE_ST, 			0x41,	0x0F,	1),
IPM_EVDESCR(DCACHE_CACHE_LOCK, 			0x42,	0x0F,	1),
IPM_EVDESCR(DATA_MEM_REF, 			0x43,	0x01,	0),
IPM_EVDESCR(DATA_MEM_CACHE_REF, 		0x44,	0x02,	0),
IPM_EVDESCR(DCACHE_REPL, 			0x45,	0x0F,	0),
IPM_EVDESCR(DCACHE_M_REPL, 			0x46,	0x00,	0),
IPM_EVDESCR(DCACHE_M_EVICT, 			0x47,	0x00,	0),
IPM_EVDESCR(DCACHE_PEND_MISS, 			0x48,	0x00,	0),
IPM_EVDESCR(DTLB_MISS, 				0x49,	0x00,	0),
IPM_EVDESCR(SSE_PREFNTA_MISS, 			0x4B,	0x00,	0),
IPM_EVDESCR(SSE_PREFT1_MISS, 			0x4B,	0x01,	0),
IPM_EVDESCR(SSE_PREFT2_MISS, 			0x4B,	0x02,	0),
IPM_EVDESCR(SSE_NTSTORES_MISS, 			0x4B,	0x03,	0),
IPM_EVDESCR(L1_PREF_REQ, 			0x4F,	0x00,	0),
IPM_EVDESCR(BUS_REQ_OUTSTANDING, 		0x60,	0xE0,	1),
IPM_EVDESCR(BUS_BNR_CLOCKS, 			0x61,	0x00,	0),
IPM_EVDESCR(BUS_DRDY_CLOCKS, 			0x62,	0x20,	1),
IPM_EVDESCR(BUS_LOCK_CLOCKS, 			0x63,	0xC0,	1),
IPM_EVDESCR(BUS_DATA_RCV, 			0x64,	0x40,	0),
IPM_EVDESCR(BUS_TRANS_BRD, 			0x65,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_RFO, 			0x66,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_IFETCH, 			0x68,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_INVAL, 			0x69,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_PWR, 			0x6A,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_P, 			0x6B,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_IO, 			0x6C,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_DEF, 			0x6D,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_WB, 			0x67,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_BURST, 			0x6E,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_MEM, 			0x6F,	0xE0,	1),
IPM_EVDESCR(BUS_TRANS_ANY, 			0x70,	0xE0,	1),
IPM_EVDESCR(BUS_SNOOPS, 			0x77,	0x2F,	1),
IPM_EVDESCR(DCU_SNOOP_TO_SHARE, 		0x78,	0xC1,	1),
IPM_EVDESCR(BUS_NOT_IN_USE, 			0x7D,	0xC0,	1),
IPM_EVDESCR(BUS_SNOOP_STALL, 			0x7E,	0x00,	0),
IPM_EVDESCR(ICACHE_READS, 			0x80,	0x00,	0),
IPM_EVDESCR(ICACHE_MISSES, 			0x81,	0x00,	0),
IPM_EVDESCR(ITLB_MISSES, 			0x85,	0x00,	0),
IPM_EVDESCR(IFU_MEM_STALL, 			0x86,	0x00,	0),
IPM_EVDESCR(ILD_STALL, 				0x87,	0x00,	0),
IPM_EVDESCR(BR_INST_EXEC, 			0x88,	0x00,	0),
IPM_EVDESCR(BR_MISSP_EXEC, 			0x89,	0x00,	0),
IPM_EVDESCR(BR_BAC_MISSP_EXEC, 			0x8A,	0x00,	0),
IPM_EVDESCR(BR_CND_EXEC, 			0x8B,	0x00,	0),
IPM_EVDESCR(BR_CND_MISSP_EXEC, 			0x8C,	0x00,	0),
IPM_EVDESCR(BR_IND_EXEC, 			0x8D,	0x00,	0),
IPM_EVDESCR(BR_IND_MISSP_EXEC, 			0x8E,	0x00,	0),
IPM_EVDESCR(BR_RET_EXEC, 			0x8F,	0x00,	0),
IPM_EVDESCR(BR_RET_MISSP_EXEC, 			0x90,	0x00,	0),
IPM_EVDESCR(BR_RET_BAC_MISSP_EXEC,	 	0x91,	0x00,	0),
IPM_EVDESCR(BR_CALL_EXEC, 			0x92,	0x00,	0),
IPM_EVDESCR(BR_CALL_MISSP_EXEC, 		0x93,	0x00,	0),
IPM_EVDESCR(BR_IND_CALL_EXEC, 			0x94,	0x00,	0),
IPM_EVDESCR(RESOURCE_STALL, 			0xA2,	0x00,	0),
IPM_EVDESCR(MMX_INSTR_EXEC, 			0xB0,	0x00,	0),
IPM_EVDESCR(SIMD_INT_SAT_EXEC, 			0xB1,	0x00,	0),
IPM_EVDESCR(SIMD_INT_PMUL_EXEC, 		0xB3,	0x01,	0),
IPM_EVDESCR(SIMD_INT_PSFT_EXEC, 		0xB3,	0x02,	0),
IPM_EVDESCR(SIMD_INT_PCK_EXEC, 			0xB3,	0x04,	0),
IPM_EVDESCR(SIMD_INT_UPCK_EXEC, 		0xB3,	0x08,	0),
IPM_EVDESCR(SIMD_INT_PLOG_EXEC, 		0xB3,	0x10,	0),
IPM_EVDESCR(SIMD_INT_PARI_EXEC, 		0xB3,	0x20,	0),
IPM_EVDESCR(INSTR_RET, 				0xC0,	0x00,	0),
IPM_EVDESCR(FP_COMP_INSTR_RET, 			0xC1,	0x00,	0),
IPM_EVDESCR(UOPS_RET, 				0xC2,	0x00,	0),
IPM_EVDESCR(SMC_DETECTED, 			0xC3,	0x00,	0),
IPM_EVDESCR(BR_INSTR_RET, 			0xC4,	0x00,	0),
IPM_EVDESCR(BR_MISPRED_RET, 			0xC5,	0x00,	0),
IPM_EVDESCR(CYCLES_INT_MASKED, 			0xC6,	0x00,	0),
IPM_EVDESCR(CYCLES_INT_PENDING_MASKED, 		0xC7,	0x00,	0),
IPM_EVDESCR(HW_INT_RX, 				0xC8,	0x00,	0),
IPM_EVDESCR(BR_TAKEN_RET, 			0xC9,	0x00,	0),
IPM_EVDESCR(BR_MISPRED_TAKEN_RET, 		0xCA,	0x00,	0),
IPM_EVDESCR(MMX_FP_TRANS, 			0xCC,	0x00,	0),
IPM_EVDESCR(FP_MMX_TRANS, 			0xCC,	0x01,	0),
IPM_EVDESCR(MMX_ASSIST, 			0xCD,	0x00,	0),
IPM_EVDESCR(MMX_INST_RET, 			0xCE,	0x00,	0),
IPM_EVDESCR(INSTR_DECODED, 			0xD0,	0x00,	0),
IPM_EVDESCR(ESP_UOPS, 				0xD7,	0x00,	0),
IPM_EVDESCR(SIMD_FP_SP_RET, 			0xD8,	0x00,	0),
IPM_EVDESCR(SIMD_FP_SP_S_RET, 			0xD8,	0x01,	0),
IPM_EVDESCR(SIMD_FP_DP_P_RET, 			0xD8,	0x02,	0),
IPM_EVDESCR(SIMD_FP_DP_S_RET, 			0xD8,	0x03,	0),
IPM_EVDESCR(SIMD_INT_128_RET, 			0xD8,	0x04,	0),
IPM_EVDESCR(SIMD_FP_SP_P_COMP_RET, 		0xD9,	0x00,	0),
IPM_EVDESCR(SIMD_FP_SP_S_COMP_RET, 		0xD9,	0x01,	0),
IPM_EVDESCR(SIMD_FP_DP_P_COMP_RET,	 	0xD9,	0x02,	0),
IPM_EVDESCR(SIMD_FP_SP_S_COMP_RET,	 	0xD9,	0x03,	0),
IPM_EVDESCR(FUSED_UOPS_RET, 			0xDA,	0x00,	0),
IPM_EVDESCR(FUSED_LD_UOPS_RET, 			0xDA,	0x01,	0),
IPM_EVDESCR(FUSED_ST_UOPS_RET, 			0xDA,	0x02,	0),
IPM_EVDESCR(UNFUSION, 				0xDB,	0x00,	0),
IPM_EVDESCR(BR_INSTR_DECODED, 			0xE0,	0x00,	0),
IPM_EVDESCR(BTB_MISSES, 			0xE2,	0x00,	0),
IPM_EVDESCR(BR_BOGUS, 				0xE4,	0x00,	0),
IPM_EVDESCR(BACLEARS, 				0xE6,	0x00,	0),
IPM_EVDESCR(PREF_RQSTS_UP, 			0xF0,	0x00,	0),
IPM_EVDESCR(PREF_RQSTS_DN,	 		0xF8,	0x00,	0),

#undef IPM_EVDESCR
};
#define	IPM_NEVENTS_V1	(PMC_EV_IAP1_LAST - PMC_EV_IAP1_FIRST + 1)

/*
 * The following events can be programmed on IA32_PMC0/1 of processors based
 * on Intel Core microarchitecture.
 */
static const struct ipm_event_descr ipm_events_v2[] = {
#define	IPM_EVDESCR(NAME, EVSEL, UMASK, ALLOW)		\
	{						\
		.pm_event = PMC_EV_IAP2_##NAME,	\
		.pm_evsel = (EVSEL),			\
		.pm_umask = (UMASK),			\
		.pm_umask_allowed = (ALLOW)		\
	}
IPM_EVDESCR(INSTRUCTIONS_RETIRED,		0xC0,	0x00,	0),
IPM_EVDESCR(UNHALTED_CORE_CYCLES,		0x3C,	0x00,	0),
IPM_EVDESCR(UNHALTED_REFERENCE_CYCLES,		0x3C,	0x01,	0),
IPM_EVDESCR(LLC_REFERENCE,			0x2E,	0x4F,	0),
IPM_EVDESCR(LLC_MISSES,				0x2E,	0x41,	0),
IPM_EVDESCR(BRANCH_INSTRUCTION_RETIRED,		0xC4,	0x00,	0),
IPM_EVDESCR(BRANCH_MISSES_RETIRED,		0xC5,	0x00,	0),

IPM_EVDESCR(LOAD_BLOCK_STA,			0x03, 	0x02,	0), 
IPM_EVDESCR(LOAD_BLOCK_STD, 			0x03, 	0x04, 	0),
IPM_EVDESCR(LOAD_BLOCK_OVERLAP_STORE, 		0x03, 	0x08, 	0),
IPM_EVDESCR(LOAD_BLOCK_UNTIL_RETIRE, 		0x03, 	0x10, 	0),
IPM_EVDESCR(LOAD_BLOCK_L1D, 			0x03, 	0x20, 	0),
IPM_EVDESCR(SB_DRAIN_CYCLES, 			0x04, 	0x01, 	0),
IPM_EVDESCR(STORE_BLOCK_ORDER, 			0x04, 	0x02, 	0),
IPM_EVDESCR(STORE_BLOCK_SNOOP, 			0x04,	0x08,	0),
IPM_EVDESCR(MISALIGN_MEM_REF, 			0x05,	0x00,	0),
IPM_EVDESCR(SEGMENT_REG_LOADS, 			0x06,	0x00,	0),
IPM_EVDESCR(SSE_PRE_EXEC_NTA, 			0x07,	0x00,	0),
IPM_EVDESCR(SSE_PRE_EXEC_L1, 			0x07,	0x01,	0),
IPM_EVDESCR(SSE_PRE_EXEC_L2, 			0x07,	0x02,	0),
IPM_EVDESCR(SSE_PRE_EXEC_STORES, 		0x07,	0x03,	0),
IPM_EVDESCR(DTLB_MISSES_ANY, 			0x08,	0x01,	0),
IPM_EVDESCR(DTLB_MISSES_MISS_LD, 		0x08,	0x02,	0),
IPM_EVDESCR(L0_DTLB_MISSES_MISS_LD, 		0x08,	0x04,	0),
IPM_EVDESCR(DTLB_MISSES_MISS_ST, 		0x08,	0x08,	0),
IPM_EVDESCR(MEMORY_DISAMBIGUATION_RESET, 	0x09,	0x01,	0),
IPM_EVDESCR(MEMORY_DISAMBIGUATION_SUCCESS,	0x09,	0x02,	0),
IPM_EVDESCR(PAGE_WALKS_COUNT, 			0x0C,	0x01,	0),
IPM_EVDESCR(PAGE_WALKS_CYCLES, 			0x0C,	0x02,	0),
IPM_EVDESCR(FP_COMP_OPS_EXE, 			0x10,	0x00,	0),
IPM_EVDESCR(FP_ASSIST, 				0x11,	0x00,	0),
IPM_EVDESCR(MUL, 				0x12,	0x00,	0),
IPM_EVDESCR(DIV, 				0x13,	0x00,	0),
IPM_EVDESCR(CYCLES_DIV_BUSY, 			0x14,	0x00,	0),
IPM_EVDESCR(IDLE_DURING_DIV, 			0x18,	0x00,	0),
IPM_EVDESCR(DELAYED_BYPASS_FP, 			0x19,	0x00,	0),
IPM_EVDESCR(DELAYED_BYPASS_SIMD, 		0x19,	0x01,	0),
IPM_EVDESCR(DELAYED_BYPASS_LOAD, 		0x19,	0x02,	0),
IPM_EVDESCR(L2_ADS, 				0x21,	0xC0,	1),
IPM_EVDESCR(L2_DBUS_BUSY_RD, 			0x23,	0xC0,	1),
IPM_EVDESCR(L2_LINES_IN, 			0x24,	0xF0,	1),
IPM_EVDESCR(L2_M_LINES_IN, 			0x25,	0xC0,	1),
IPM_EVDESCR(L2_LINES_OUT, 			0x26,	0xF0,	1),
IPM_EVDESCR(L2_M_LINES_OUT, 			0x27,	0xF0,	1),
IPM_EVDESCR(L2_IFETCH, 				0x28,	0xCF,	1),
IPM_EVDESCR(L2_LD, 				0x29,	0xFF,	1),
IPM_EVDESCR(L2_ST, 				0x2A,	0xCF,	1),
IPM_EVDESCR(L2_LOCK, 				0x2B,	0xCF,	1),
IPM_EVDESCR(L2_RQSTS, 				0x2E,	0xFF,	1),
IPM_EVDESCR(L2_RQSTS_SELF_DEMAND_I_STATE,	0x2E,	0x41,	0),
IPM_EVDESCR(L2_RQSTS_SELF_DEMAND_MESI,		0x2E,	0x4F,	0),
IPM_EVDESCR(L2_REJECT_BUSQ, 			0x30,	0xFF,	1),
IPM_EVDESCR(L2_NO_REQ, 				0x32,	0xC0,	1),
IPM_EVDESCR(EIST_TRANS, 			0x3A,	0x00,	0),
IPM_EVDESCR(THERMAL_TRIP,			0x3B,	0xC0,	0),
IPM_EVDESCR(CPU_CLK_UNHALTED_CORE, 		0x3C,	0x00,	0),
IPM_EVDESCR(CPU_CLK_UNHALTED_BUS, 		0x3C,	0x01,	0),
IPM_EVDESCR(CPU_CLK_UNHALTED_NO_OTHER, 		0x3C,	0x02,	0),
IPM_EVDESCR(L1D_CACHE_LD, 			0x40,	0x0F,	1),
IPM_EVDESCR(L1D_CACHE_ST, 			0x40,	0x0F,	1),
IPM_EVDESCR(L1D_CACHE_LOCK, 			0x42,	0x0F,	1),
IPM_EVDESCR(L1D_CACHE_LOCK_DURATION, 		0x42,	0x10,	0),
IPM_EVDESCR(L1D_ALL_REF, 			0x43,	0x10,	0),
IPM_EVDESCR(L1D_ALL_CACHE_REF, 			0x43,	0x02,	0),
IPM_EVDESCR(L1D_REPL, 				0x45,	0x0F,	0),
IPM_EVDESCR(L1D_M_REPL, 			0x46,	0x00,	0),
IPM_EVDESCR(L1D_M_EVICT, 			0x47,	0x00,	0),
IPM_EVDESCR(L1D_PEND_MISS, 			0x48,	0x00,	0),
IPM_EVDESCR(L1D_SPLIT_LOADS, 			0x49,	0x01,	0),
IPM_EVDESCR(L1D_SPLIT_STORES, 			0x49,	0x02,	0),
IPM_EVDESCR(SSE_PRE_MISS_NTA, 			0x4B,	0x00,	0),
IPM_EVDESCR(SSE_PRE_MISS_L1, 			0x4B,	0x01,	0),
IPM_EVDESCR(SSE_PRE_MISS_L2, 			0x4B,	0x02,	0),
IPM_EVDESCR(LOAD_HIT_PRE, 			0x4C,	0x00,	0),
IPM_EVDESCR(L1D_PREFETCH_REQUESTS, 		0x4E,	0x10,	0),
IPM_EVDESCR(BUS_REQUEST_OUTSTANDING, 		0x60,	0xE0,	1),
IPM_EVDESCR(BUS_BNR_DRV, 			0x61,	0x20,	1),
IPM_EVDESCR(BUS_DRDY_CLOCKS, 			0x62,	0x20,	1),
IPM_EVDESCR(BUS_LOCK_CLOCKS, 			0x63,	0xE0,	1),
IPM_EVDESCR(BUS_DATA_RCV, 			0x64,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_BRD, 			0x65,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_RFO, 			0x66,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_WB, 			0x67,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_IFETCH, 			0x68,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_INVAL, 			0x69,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_PWR, 			0x6A,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_P,			0x6B,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_IO, 			0x6C,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_DEF, 			0x6D,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_BURST, 			0x6E,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_MEM, 			0x6F,	0xC0,	1),
IPM_EVDESCR(BUS_TRANS_ANY, 			0x70,	0xC0,	1),
IPM_EVDESCR(EXT_SNOOP, 				0x77,	0xCB,	1),
IPM_EVDESCR(CMP_SNOOP, 				0x78,	0xC3,	1),
IPM_EVDESCR(BUS_HIT_DRV, 			0x7A,	0x20,	1),
IPM_EVDESCR(BUS_HITM_DRV, 			0x7B,	0x20,	1),
IPM_EVDESCR(BUSQ_EMPTY, 			0x7D,	0xC0,	1),
IPM_EVDESCR(SNOOP_STALL_DRV, 			0x7E,	0xE0,	1),
IPM_EVDESCR(BUS_IO_WAIT, 			0x7F,	0xC0,	1),
IPM_EVDESCR(L1I_READS, 				0x80,	0x00,	0),
IPM_EVDESCR(L1I_MISSES, 			0x81,	0x00,	0),
IPM_EVDESCR(ITLB_SMALL_MISS, 			0x82,	0x02,	0),
IPM_EVDESCR(ITLB_LARGE_MISS, 			0x82,	0x10,	0),
IPM_EVDESCR(ITLB_FLUSH, 			0x82,	0x40,	0),
IPM_EVDESCR(ITLB_MISSES, 			0x82,	0x12,	0),
IPM_EVDESCR(INST_QUEUE_FULL, 			0x83,	0x02,	0),
IPM_EVDESCR(CYCLES_L1I_MEM_STALLED, 		0x86,	0x00,	0),
IPM_EVDESCR(ILD_STALL, 				0x87,	0x00,	0),
IPM_EVDESCR(BR_INST_EXEC, 			0x88,	0x00,	0),
IPM_EVDESCR(BR_MISSP_EXEC, 			0x89,	0x00,	0),
IPM_EVDESCR(BR_BAC_MISSP_EXEC, 			0x8A,	0x00,	0),
IPM_EVDESCR(BR_CND_EXEC, 			0x8B,	0x00,	0),
IPM_EVDESCR(BR_CND_MISSP_EXEC, 			0x8C,	0x00,	0),
IPM_EVDESCR(BR_IND_EXEC, 			0x8D,	0x00,	0),
IPM_EVDESCR(BR_IND_MISSP_EXEC, 			0x8E,	0x00,	0),
IPM_EVDESCR(BR_RET_EXEC, 			0x8F,	0x00,	0),
IPM_EVDESCR(BR_RET_MISSP_EXEC, 			0x90,	0x00,	0),
IPM_EVDESCR(BR_RET_BAC_MISSP_EXEC, 		0x91,	0x00,	0),
IPM_EVDESCR(BR_CALL_EXEC, 			0x92,	0x00,	0),
IPM_EVDESCR(BR_CALL_MISSP_EXEC, 		0x93,	0x00,	0),
IPM_EVDESCR(BR_IND_CALL_EXEC, 			0x94,	0x00,	0),
IPM_EVDESCR(BR_TKN_BUBBLE_1, 			0x97,	0x00,	0),
IPM_EVDESCR(BR_TKN_BUBBLE_2, 			0x98,	0x00,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED, 		0xA0,	0x00,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT0, 		0xA0,	0x01,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT1, 		0xA0,	0x02,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT2, 		0xA0,	0x04,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT3, 		0xA0,	0x08,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT4, 		0xA0,	0x10,	0),
IPM_EVDESCR(RS_UOPS_DISPATCHED_PORT5, 		0xA0,	0x20,	0),
IPM_EVDESCR(MACRO_INSTS_DECODED, 		0xA1,	0x01,	0),
IPM_EVDESCR(MACRO_INSTS_CISC_DECODED,		0xA1,	0x08,	0),
IPM_EVDESCR(ESP_SYNCH, 				0xAB,	0x01,	0),
IPM_EVDESCR(ESP_ADDITIONS, 			0xAB,	0x02,	0),
IPM_EVDESCR(SIMD_UOPS_EXEC, 			0xB0,	0x00,	0),
IPM_EVDESCR(SIMD_SAT_UOP_EXEC, 			0xB1,	0x00,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_MUL, 		0xB3,	0x01,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_SHIFT,		0xB3,	0x02,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_PACK,		0xB3,	0x04,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_UNPACK,		0xB3,	0x08,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_LOGICAL,		0xB3,	0x10,	0),
IPM_EVDESCR(SIMD_UOP_TYPE_EXEC_ARITHMETIC,	0xB3,	0x20,	0),
IPM_EVDESCR(INST_RETIRED_ANY_P, 		0xC0,	0x00,	0),
IPM_EVDESCR(INST_RETIRED_LOADS, 		0xC0,	0x01,	0),
IPM_EVDESCR(INST_RETIRED_STORES, 		0xC0,	0x02,	0),
IPM_EVDESCR(INST_RETIRED_OTHER, 		0xC0,	0x04,	0),
IPM_EVDESCR(X87_OPS_RETIRED_FXCH, 		0xC1,	0x01,	0),
IPM_EVDESCR(X87_OPS_RETIRED_ANY, 		0xC1,	0xFE,	0),
IPM_EVDESCR(UOPS_RETIRED_L2_IND_BR, 		0xC2,	0x01,	0),
IPM_EVDESCR(UOPS_RETIRED_STD_STA, 		0xC2,	0x02,	0),
IPM_EVDESCR(UOPS_RETIRED_MACRO_FUSION,		0xC2,	0x04,	0),
IPM_EVDESCR(UOPS_RETIRED_FUSED, 		0xC2,	0x07,	0),
IPM_EVDESCR(UOPS_RETIRED_NON_FUSED, 		0xC2,	0x08,	0),
IPM_EVDESCR(UOPS_RETIRED_ANY, 			0xC2,	0x0F,	0),
IPM_EVDESCR(MACHINE_NUKES_SMC, 			0xC3,	0x01,	0),
IPM_EVDESCR(MACHINE_NUKES_MEM_ORDER,		0xC3,	0x04,	0),
IPM_EVDESCR(BR_INST_RETIRED_ANY, 		0xC4,	0x00,	0),
IPM_EVDESCR(BR_INST_RETIRED_PRED_NOT_TAKEN,	0xC4,	0x01,	0),
IPM_EVDESCR(BR_INST_RETIRED_MISPRED_NOT_TAKEN,	0xC4,	0x02,	0),
IPM_EVDESCR(BR_INST_RETIRED_PRED_TAKEN,		0xC4,	0x04,	0),
IPM_EVDESCR(BR_INST_RETIRED_MISPRED_TAKEN,	0xC4,	0x08,	0),
IPM_EVDESCR(BR_INST_RETIRED_TAKEN, 		0xC4,	0x0C,	0),
IPM_EVDESCR(BR_INST_RETIRED_MISPRED,		0xC5,	0x00,	0),
IPM_EVDESCR(CYCLES_INT_MASKED, 			0xC6,	0x01,	0),
IPM_EVDESCR(CYCLES_INT_PENDING_AND_MASKED,	0xC6,	0x02,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_PACKED_SINGLE, 	0xC7,	0x01,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_SCALAR_SINGLE, 	0xC7,	0x02,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_PACKED_DOUBLE, 	0xC7,	0x04,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_SCALAR_DOUBLE, 	0xC7,	0x08,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_VECTOR, 	 	0xC7,	0x10,	0),
IPM_EVDESCR(SIMD_INST_RETIRED_ANY, 		0xC7,	0x1F,	0),
IPM_EVDESCR(HW_INT_RCV, 			0xC8,	0x00,	0),
IPM_EVDESCR(ITLB_MISS_RETIRED, 			0xC9,	0x00,	0),
IPM_EVDESCR(SIMD_COMP_INST_RETIRED_PACKED_SINGLE,	0xCA,	0x01,	0),
IPM_EVDESCR(SIMD_COMP_INST_RETIRED_SCALAR_SINGLE, 	0xCA,	0x02,	0),
IPM_EVDESCR(SIMD_COMP_INST_RETIRED_PACKED_DOUBLE, 	0xCA,	0x04,	0),
IPM_EVDESCR(SIMD_COMP_INST_RETIRED_SCALAR_DOUBLE,	0xCA,	0x08,	0),
IPM_EVDESCR(MEM_LOAD_RETIRED_L1D_MISS,		0xCB,	0x01,	0),
IPM_EVDESCR(MEM_LOAD_RETIRED_L1D_LINE_MISS,	0xCB,	0x02,	0),
IPM_EVDESCR(MEM_LOAD_RETIRED_L2_MISS, 		0xCB,	0x04,	0),
IPM_EVDESCR(MEM_LOAD_RETIRED_L2_LINE_MISS,	0xCB,	0x08,	0),
IPM_EVDESCR(MEM_LOAD_RETIRED_DTLB_MISS, 	0xCB,	0x10,	0),
IPM_EVDESCR(FP_MMX_TRANS_TO_MMX, 		0xCC,	0x01,	0),
IPM_EVDESCR(FP_MMX_TRANS_TO_FP, 		0xCC,	0x02,	0),
IPM_EVDESCR(SIMD_ASSIST, 			0xCD,	0x00,	0),
IPM_EVDESCR(SIMD_INST_RETIRED, 			0xCE,	0x00,	0),
IPM_EVDESCR(SIMD_SAT_INST_RETIRED, 		0xCF,	0x00,	0),
IPM_EVDESCR(RAT_STALLS_ROB_READ_PORT,		0xD2,	0x01,	0),
IPM_EVDESCR(RAT_STALLS_PARTIAL_CYCLES,		0xD2,	0x02,	0),
IPM_EVDESCR(RAT_STALLS_FLAGS, 			0xD2,	0x04,	0),
IPM_EVDESCR(RAT_STALLS_FPSW, 			0xD2,	0x08,	0),
IPM_EVDESCR(RAT_STALLS_ANY, 			0xD2,	0x0F,	0),
IPM_EVDESCR(SEG_RENAME_STALLS_ES, 		0xD4,	0x01,	0),
IPM_EVDESCR(SEG_RENAME_STALLS_DS, 		0xD4,	0x02,	0),
IPM_EVDESCR(SEG_RENAME_STALLS_FS, 		0xD4,	0x04,	0),
IPM_EVDESCR(SEG_RENAME_STALLS_GS, 		0xD4,	0x08,	0),
IPM_EVDESCR(SEG_RENAME_STALLS_ANY, 		0xD4,	0x0F,	0),
IPM_EVDESCR(SEG_REG_RENAME_ES, 			0xD4,	0x01,	0),
IPM_EVDESCR(SEG_REG_RENAME_DS, 			0xD5,	0x02,	0),
IPM_EVDESCR(SEG_REG_RENAME_FS, 			0xD5,	0x04,	0),
IPM_EVDESCR(SEG_REG_RENAME_GS, 			0xD5,	0x08,	0),
IPM_EVDESCR(SEG_REG_RENAME_ANY, 		0xD5,	0x0F,	0),
IPM_EVDESCR(RESOURCE_STALLS_ROB_FULL, 		0xDC,	0x01,	0),
IPM_EVDESCR(RESOURCE_STALLS_RS_FULL, 		0xDC,	0x02,	0),
IPM_EVDESCR(RESOURCE_STALLS_LD_ST, 		0xDC,	0x04,	0),
IPM_EVDESCR(RESOURCE_STALLS_FPCW, 		0xDC,	0x08,	0),
IPM_EVDESCR(RESOURCE_STALLS_BR_MISS_CLEAR,	0xDC,	0x10,	0),
IPM_EVDESCR(RESOURCE_STALLS_ANY, 		0xDC,	0x1F,	0),
IPM_EVDESCR(BR_INST_DECODED, 			0xE0,	0x00,	0),
IPM_EVDESCR(BOGUS_BR, 				0xE4,	0x00,	0),
IPM_EVDESCR(BACLEARS, 				0xE6,	0x00,	0),
IPM_EVDESCR(PREF_RQSTS_UP, 			0xF0,	0x00,	0),
IPM_EVDESCR(PREF_RQSTS_DN, 			0xF8,	0x00,	0)

#undef	IPM_EVDESCR
};
#define	IPM_NEVENTS_V2	(PMC_EV_IAP2_LAST - PMC_EV_IAP2_FIRST + 1)

/*
 * Events defined here can only be programmed on the specified raw index
 */
struct ipm_ev_pmc_mapping {
	const enum pmc_event	pm_event;
	const int 		pm_ri;
};

static const struct ipm_ev_pmc_mapping ipm_ev_pmc_mappings_v1[] = {
#define	IPM_EV_PMC_MAPPING(NAME, RI)			\
	{						\
		.pm_event = PMC_EV_IAP1_##NAME,	\
		.pm_ri	  = (RI),			\
	}
IPM_EV_PMC_MAPPING(FP_ASSIST,			2),
IPM_EV_PMC_MAPPING(MUL,				2),
IPM_EV_PMC_MAPPING(DIV,				2),
IPM_EV_PMC_MAPPING(CYCLES_DIV_BUSY,		1),
IPM_EV_PMC_MAPPING(FP_COMP_INSTR_RET,		1),

#undef	IPM_EV_PMC_MAPPING
};
#define	IAP1_NMAPPINGS		5

static const struct ipm_ev_pmc_mapping ipm_ev_pmc_mappings_v2[] = {
#define	IPM_EV_PMC_MAPPING(NAME, RI)			\
	{						\
		.pm_event = PMC_EV_IAP2_##NAME,	\
		.pm_ri	  = (RI),			\
	}
IPM_EV_PMC_MAPPING(FP_COMP_OPS_EXE,		1),
IPM_EV_PMC_MAPPING(FP_ASSIST,			2),
IPM_EV_PMC_MAPPING(MUL,				2),
IPM_EV_PMC_MAPPING(DIV,				2),
IPM_EV_PMC_MAPPING(CYCLES_DIV_BUSY,		1),
IPM_EV_PMC_MAPPING(IDLE_DURING_DIV,		1),
IPM_EV_PMC_MAPPING(DELAYED_BYPASS_FP,		2),
IPM_EV_PMC_MAPPING(DELAYED_BYPASS_SIMD,		2),
IPM_EV_PMC_MAPPING(DELAYED_BYPASS_LOAD,		2),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT0,	1),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT1,	1),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT2,	1),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT3,	1),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT4,	1),
IPM_EV_PMC_MAPPING(RS_UOPS_DISPATCHED_PORT5,	1),
IPM_EV_PMC_MAPPING(MEM_LOAD_RETIRED_L1D_MISS,	1),
IPM_EV_PMC_MAPPING(MEM_LOAD_RETIRED_L1D_LINE_MISS,	1),
IPM_EV_PMC_MAPPING(MEM_LOAD_RETIRED_L2_MISS,	1),
IPM_EV_PMC_MAPPING(MEM_LOAD_RETIRED_L2_LINE_MISS,	1),
IPM_EV_PMC_MAPPING(MEM_LOAD_RETIRED_DTLB_MISS,	1),

#undef	IPM_EV_PMC_MAPPING
};
#define	IAP2_NMAPPINGS		20

static const struct ipm_ev_pmc_mapping *ipm_ev_pmc_mappings = NULL;
static int ipm_nmappings = 0;

static inline int 
ipm_ev_valid_for_pmc(enum pmc_event ev, int ri)
{
	int valid = 1;
	int i = 0;

	for (; i < ipm_nmappings; i++) {
		if (ev == ipm_ev_pmc_mappings[i].pm_event &&
		    ri != ipm_ev_pmc_mappings[i].pm_ri)
			valid = 0;
	}

	return (valid);
}


static const struct ipm_event_descr *
ipm_find_event(enum pmc_event ev)
{
	int n;

	if (ipm_cputype == PMC_CPU_INTEL_CORE ||
	    ipm_cputype == PMC_CPU_INTEL_CORE2 ||
    	    ipm_cputype == PMC_CPU_INTEL_ATOM) {
		for (n = 0; n < IPM_NEVENTS_FIXED; n++) 
			if (ipm_events_fixed[n].pm_event == ev)
				return (&ipm_events_fixed[n]);
	}

	for (n = 0; n < ipm_nevents; n++) {
		if (ipm_events[n].pm_event == ev)
			return (&ipm_events[n]);
	}

	return (NULL);
}

/*
 * Per-CPU data structure for Intel Core Solo/Duo class CPUs
 *
 * [common stuff]
 * [flags for maintaining PMC start/stop state]
 * [3 struct pmc_hw pointers]
 * [3 struct pmc_hw structures]
 */
struct ipm_cpu {
	struct pmc_cpu	pc_common;
	struct pmc_hw	*pc_hwpmcs[IPM_NPMCS];
	struct pmc_hw	pc_ipm_pmcs[IPM_NPMCS];
	uint32_t	pc_state;
};

#define	IPM_PMC_MARK_STARTED(PC,RI) do {			\
		(PC)->pc_state |= (1 << ((RI)-1));		\
	} while (0)

#define	IPM_PMC_MARK_STOPPED(PC,RI) do {			\
		(PC)->pc_state &= ~(1<< ((RI)-1));		\
	} while (0)

#define	IPM_PMC_STOPPED(PC,RI)	(((PC)->pc_state & (1 << ((RI)-1))) == 0)
#define IPM_PMC_STARTED(PC,RI)	(((PC)->pc_state & (1 << ((RI)-1))) == 1)

static int
ipm_init(int cpu)
{
	int n;
	struct ipm_cpu *pcs;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm,%d] bad cpu %d", __LINE__, cpu));

	PMCDBG(MDP,INI,0,"ipm-init cpu=%d", cpu);

	MALLOC(pcs, struct ipm_cpu *, sizeof(struct ipm_cpu), M_PMC,
	    M_WAITOK|M_ZERO);

	phw = pcs->pc_ipm_pmcs;

	for (n = 0; n < ipm_npmcs; n++, phw++) {
		phw->phw_state	  = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n);
		phw->phw_pmc      = NULL;
		pcs->pc_hwpmcs[n] = phw;
	}

	/* Mark the TSC as shareable */
	pcs->pc_hwpmcs[0]->phw_state |= PMC_PHW_FLAG_IS_SHAREABLE;

	pmc_pcpu[cpu] = (struct pmc_cpu *) pcs;

	return (0);
}

static int
ipm_cleanup(int cpu)
{
	struct pmc_cpu *pcs;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm,%d] bad cpu %d", __LINE__, cpu));

	PMCDBG(MDP,INI,0,"ipm-cleanup cpu=%d", cpu);

	if ((pcs = pmc_pcpu[cpu]) != NULL)
		FREE(pcs, M_PMC);
	pmc_pcpu[cpu] = NULL;

	return (0);
}

static int
ipm_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;

	PMCDBG(MDP,SWI,1, "pc=%p pp=%p enable-msr=%d", pc, pp,
	    pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS);

	/* allow the RDPMC instruction if needed */
	if (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS)
		load_cr4(rcr4() | CR4_PCE);

	PMCDBG(MDP,SWI,1, "cr4=0x%x", rcr4());

	return (0);
}

static int
ipm_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;
	(void) pp;		/* can be NULL */

	PMCDBG(MDP,SWO,1, "pc=%p pp=%p cr4=0x%x", pc, pp, rcr4());

	/* always turn off the RDPMC instruction */
 	load_cr4(rcr4() & ~CR4_PCE);

	return (0);
}

static int
ipm_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc_hw *phw;
	struct pmc *pm;
	const struct ipm_pmc_descr *pd;
	pmc_value_t tmp;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &ipm_pmc_desc[ri];

	KASSERT(pm,
	    ("[ipm,%d] cpu %d ri %d pmc not configured", 
	    __LINE__, cpu, ri));

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC) {
		*v = rdtsc();
		return (0);
	}

	tmp = rdmsr(pd->pm_pmc_msr) & IPM_PERFCTR_READ_MASK;
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = IPM_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	else
		*v = tmp;

	PMCDBG(MDP,REA,1, "ipm-read cpu=%d ri=%d msr=0x%x -> v=%jx", cpu, ri,
	    pd->pm_pmc_msr, *v);

	return (0);
}

static int
ipm_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc_hw *phw;
	struct pmc *pm;
	const struct ipm_pmc_descr *pd;
	uint64_t ctrl_msr;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &ipm_pmc_desc[ri];

	KASSERT(pm,
	    ("[ipm,%d] cpu %d ri %d pmc not configured", 
	    __LINE__, cpu, ri));

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return (0);

	PMCDBG(MDP,WRI,1, "ipm-write cpu=%d ri=%d msr=0x%x v=%jx", cpu, ri,
	    pd->pm_pmc_msr, v);
	
	/*
	 * Stop the PMC first
	 */
	switch (ipm_cputype) {
	case PMC_CPU_INTEL_CORE:
		wrmsr(pd->pm_evsel_msr, 
		    pm->pm_md.pm_ipm.pm_ipm_evsel & (~IPM_EVSEL_EN));
		break;

	case PMC_CPU_INTEL_CORE2:
		ctrl_msr = rdmsr(IPM_MSR_PERFCTR_CTRL);
		ctrl_msr &= ~(ipm_v2_pmc_ri_to_msr_en_bit(ri));
		wrmsr(IPM_MSR_PERFCTR_CTRL, ctrl_msr);
		break;
		
	default:
		break;
	}

	
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(v);
	wrmsr(pd->pm_pmc_msr, v & IPM_PERFCTR_WRITE_MASK);
	
	/*
	 * Start the PMC
	 */
	switch (ipm_cputype) {
	case PMC_CPU_INTEL_CORE:
		wrmsr(pd->pm_evsel_msr,
		    pm->pm_md.pm_ipm.pm_ipm_evsel | IPM_EVSEL_EN);
		break;

	case PMC_CPU_INTEL_CORE2:
		ctrl_msr = rdmsr(IPM_MSR_PERFCTR_CTRL);
		ctrl_msr |= ipm_v2_pmc_ri_to_msr_en_bit(ri);
		wrmsr(IPM_MSR_PERFCTR_CTRL, ctrl_msr);
		break;

	default:
		break;
	}

	return (0);
}

static int
ipm_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG(MDP,CFG,1, "ipm-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	phw->phw_pmc = pm;

	return (0);
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */
static int
ipm_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = pmc_pcpu[cpu]->pc_hwpmcs[ri]->phw_pmc;

	return (0);
}

static int
ipm_allocate_fixed_pmc(int cpu, int ri, uint32_t caps,
    struct pmc *pm, const struct ipm_event_descr *pevent)
{
	uint32_t config, counter;

	counter = ri - 3;
	
	/*
	 * Event and fixed function counter has one to one mapping
	 * and the event is only valid for the counter with same
	 * index
	 */
	if ((pevent->pm_event == PMC_EV_IAF_INSTRUCTIONS_RETIRED &&
	    counter == 0) ||
	    (pevent->pm_event == PMC_EV_IAF_UNHALTED_CORE_CYCLES &&
	    counter == 1) ||
	    (pevent->pm_event == PMC_EV_IAF_UNHALTED_REFERENCE_CYCLES 
	     && counter == 2)) {
		config = 0;
	} else {
		return (EINVAL);
	}

	if (caps & PMC_CAP_SYSTEM)
		config |= IPM_FIXEDCTR_PL_OS;
	if (caps & PMC_CAP_USER) 
		config |= IPM_FIXEDCTR_PL_USR;
	if ((caps & (PMC_CAP_SYSTEM | PMC_CAP_USER)) == 0) 
		config |= IPM_FIXEDCTR_PL_ANY;
	if (caps & PMC_CAP_SYSTEM) 
		config |= IPM_FIXEDCTR_INT;

	pm->pm_md.pm_ipm.pm_ipm_evsel =
	    (uint32_t)(config << (4 * counter));
	
	return (0);
}

static inline int
ipm_check_event_range(enum pmc_event ev)
{
	int valid = 1;

	if (ipm_cputype == PMC_CPU_INTEL_CORE) {
		if (ev < PMC_EV_IAP1_FIRST ||
		    ev > PMC_EV_IAP1_LAST) 
			valid = 0;
	} else if (ipm_cputype == PMC_CPU_INTEL_CORE2) {
		if (ev < PMC_EV_IAF_FIRST ||
		    ev > PMC_EV_IAP2_LAST) 
			valid = 0;
	}
	
	return (valid);
}

/*
 * A pmc may be allocated to a given row index if:
 * - the event is valid for this CPU
 * - the event is valid for this counter index
 */
static int
ipm_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	uint32_t caps, config, unitmask, allowed_unitmask;
	const struct ipm_pmc_descr *pd;
	const struct ipm_event_descr *pevent;
	enum pmc_event ev;

	(void) cpu;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ipm_npmcs,
	    ("[ipm,%d] illegal row-index value %d", __LINE__, ri));

	pd = &ipm_pmc_desc[ri];

	PMCDBG(MDP,ALL,1, "ipm-allocate ri=%d class=%d pmccaps=0x%x "
	    "reqcaps=0x%x", ri, pd->pm_descr.pd_class, pd->pm_descr.pd_caps,
	    pm->pm_caps);

	/* check class */
	if ((pd->pm_descr.pd_class & a->pm_class) == 0) 
		return (EINVAL);

	/* check requested capabilities */
	caps = a->pm_caps;
	if ((pd->pm_descr.pd_caps & caps) != caps) 
		return (EPERM);

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC) {
		/* TSC's are always allocated in system-wide counting mode */
		if (a->pm_ev != PMC_EV_TSC_TSC ||
		    a->pm_mode != PMC_MODE_SC)
			return (EINVAL);
		return (0);
	}

	/*
	 * PM events
	 */
	ev = pm->pm_event;
	
	if (!ipm_check_event_range(ev)) 
		return (EINVAL);

	if ((pevent = ipm_find_event(ev)) == NULL) 
		return (ESRCH);

	if (pd->pm_descr.pd_class == PMC_CLASS_IAF) {
		if (ipm_cputype == PMC_CPU_INTEL_CORE2) {
			if (ri >= IPM_NPMCS_V1) 
				return (ipm_allocate_fixed_pmc(cpu, ri, caps, 
			    	    pm, pevent));
			else 
				return (EINVAL);
		} else
			return (EINVAL);
	}

	if (!ipm_ev_valid_for_pmc(ev, ri))
		return (EINVAL);
	
	config = 0;
	if (pevent->pm_umask_allowed) {
		unitmask = a->pm_md.pm_ipm.pm_ipm_config & 
		    IPM_EVSEL_UMASK_MASK;
		allowed_unitmask = IPM_EVSEL_TO_UMASK(pevent->pm_umask);
		if (unitmask & ~allowed_unitmask) /* disallow reserved bits */ 
			return (EINVAL);
		if (unitmask & (caps & PMC_CAP_QUALIFIER))
			config |= unitmask;
	} else {
		unitmask = IPM_EVSEL_TO_UMASK(pevent->pm_umask);
		config |= unitmask;
	}

	config |= IPM_EVSEL_EVENT_SELECT(pevent->pm_evsel);
	
	if (caps & PMC_CAP_THRESHOLD)
		config |= a->pm_md.pm_ipm.pm_ipm_config &
		    IPM_EVSEL_CMASK_MASK;

	/* set at least one of the 'usr' or 'os' caps */
	if (caps & PMC_CAP_USER)
		config |= IPM_EVSEL_USR;
	if (caps & PMC_CAP_SYSTEM)
		config |= IPM_EVSEL_OS;
	if ((caps & (PMC_CAP_USER|PMC_CAP_SYSTEM)) == 0)
		config |= (IPM_EVSEL_USR|IPM_EVSEL_OS);

	if (caps & PMC_CAP_EDGE)
		config |= IPM_EVSEL_E;
	if (caps & PMC_CAP_INVERT)
		config |= IPM_EVSEL_INV;
	if (caps & PMC_CAP_INTERRUPT)
		config |= IPM_EVSEL_INT;

	pm->pm_md.pm_ipm.pm_ipm_evsel = config;

	PMCDBG(MDP,ALL,2, "ipm-v1-allocate config=0x%x", config);

	return (0);
}

static int
ipm_release_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	(void) pm;

	PMCDBG(MDP,REL,1, "ipm-release cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ipm_npmcs,
	    ("[ipm,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];

	KASSERT(phw->phw_pmc == NULL,
	    ("[ipm,%d] PHW pmc %p != pmc %p", __LINE__, phw->phw_pmc, pm));

	return (0);
}

static int
ipm_start_pmc(int cpu, int ri)
{
	uint64_t config = 0, ctrl_msr = 0;
	struct pmc *pm;
	struct ipm_cpu *pc;
	struct pmc_hw *phw;
	const struct ipm_pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ipm_npmcs,
	    ("[ipm,%d] illegal row-index %d", __LINE__, ri));

	pc  = (struct ipm_cpu *) pmc_pcpu[cpu];
	phw = pc->pc_common.pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &ipm_pmc_desc[ri];

	KASSERT(pm,
	    ("[ipm,%d] starting cpu%d,ri%d with no pmc configured",
		__LINE__, cpu, ri));

	PMCDBG(MDP,STA,1, "ipm-v1-start cpu=%d ri=%d", cpu, ri);

	
	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return (0);	/* TSC are always running */

	/*
	 * Do nothing if PMC has already been started
	 */
	if (IPM_PMC_STARTED(pc, ri)) {
		return (0);
	}

	PMCDBG(MDP,STA,2, "ipm-start/2 cpu=%d ri=%d evselmsr=0x%x config=0x%x",
	    cpu, ri, pd->pm_evsel_msr, config);

	IPM_PMC_MARK_STARTED(pc, ri);

	switch (ipm_cputype) {
		case PMC_CPU_INTEL_CORE:
			/*
			 * No fixed function PMCs, PMCs controlled directly
			 * using IA32_PERFEVTSEL MSRs
			 */
			KASSERT(pd->pm_descr.pd_class != PMC_CLASS_IAF,
			    ("[ipm,%d] fixed function PMCs not supported on "
			     "this platform", __LINE__));
			config = pm->pm_md.pm_ipm.pm_ipm_evsel;
			wrmsr(pd->pm_evsel_msr, config | IPM_EVSEL_EN);
			break;

		case PMC_CPU_INTEL_CORE2:
			/*
			 * XXX Intel's doco is a bit ambiguous in the sense
			 * that it is not clear whether we should set the EN
			 * bit or we should just enable the PMC using the
			 * global counter control MSR
			 * Do both...
			 */

			ctrl_msr = rdmsr(IPM_MSR_PERFCTR_CTRL);
			if (pd->pm_descr.pd_class == PMC_CLASS_IAF) {
				config = rdmsr(pd->pm_evsel_msr);
				config |= pm->pm_md.pm_ipm.pm_ipm_evsel;
				wrmsr(pd->pm_evsel_msr, config);
			} else {
				config = pm->pm_md.pm_ipm.pm_ipm_evsel;
				wrmsr(pd->pm_evsel_msr, config | IPM_EVSEL_EN);
			}

			/*
			 * Enable the PMC using the global PMC counter control
			 * MSR
			 */
			ctrl_msr = rdmsr(IPM_MSR_PERFCTR_CTRL);
			ctrl_msr |= ipm_v2_pmc_ri_to_msr_en_bit(ri);
			wrmsr(IPM_MSR_PERFCTR_CTRL, ctrl_msr);
			break;

		default:
			break;
	}
	
	return (0);
}

static int
ipm_stop_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct ipm_cpu *pc;
	struct pmc_hw *phw;
	const struct ipm_pmc_descr *pd;
	uint64_t config, ctrl_msr;
	
	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm-v1,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ipm_npmcs,
	    ("[ipm-v1,%d] illegal row index %d", __LINE__, ri));

	pc  = (struct ipm_cpu *)pmc_pcpu[cpu];

	/* 
	 * Do nothing if PMC has been stopped/not started
	 */
	if (IPM_PMC_STOPPED(pc, ri)) 
		return (0);

	phw = pc->pc_common.pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &ipm_pmc_desc[ri];

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return (0);

	KASSERT(pm,
	    ("[ipm,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_IAP1
	    || pd->pm_descr.pd_class == PMC_CLASS_IAP2 
	    || pd->pm_descr.pd_class == PMC_CLASS_IPM_FIXED,
	    ("[ipm,%d] unknown PMC class %d", __LINE__,
		pd->pm_descr.pd_class));

	PMCDBG(MDP,STO,1, "ipm-stop cpu=%d ri=%d", cpu, ri);

	IPM_PMC_MARK_STOPPED(pc, ri);	/* update software state */

	switch (ipm_cputype) {
		case PMC_CPU_INTEL_CORE:
			wrmsr(pd->pm_evsel_msr, 0);
			break;

		case PMC_CPU_INTEL_CORE2:

			if (pd->pm_descr.pd_class == PMC_CLASS_IAF) {
				config = rdmsr(pd->pm_evsel_msr);
				config &= IPM_FIXEDCTR_DISABLE(ri - 3);
				wrmsr(pd->pm_evsel_msr, config);
			} else {
				wrmsr(pd->pm_evsel_msr, 0);	/* stop hw */
			}

			/* 
			 * Disable the PMC using global PMC counter control
			 * MSR
			 */
			ctrl_msr = rdmsr(IPM_MSR_PERFCTR_CTRL);
			ctrl_msr &= ~(ipm_v2_pmc_ri_to_msr_en_bit(ri));
			wrmsr(IPM_MSR_PERFCTR_CTRL, ctrl_msr);

		default:
			break;
	}

	PMCDBG(MDP,STO,2, "ipm-stop/2 cpu=%d ri=%d", cpu, ri);
	return (0);
}

static int
ipm_v1_intr(int cpu, struct trapframe *tf)
{
	int i, error, retval, ri;
	struct pmc *pm;
	struct ipm_cpu *pc;
	struct pmc_hw *phw;
	pmc_value_t v;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm-v1,%d] CPU %d out of range", __LINE__, cpu));

	retval = 0;
	pc = (struct ipm_cpu *) pmc_pcpu[cpu];

	for (i = 0; i < IPM_NPMCS_V1 - 1; i++) {
		ri = i + 1;
		phw = pc->pc_common.pc_hwpmcs[ri];

		if ((pm = phw->phw_pmc) == NULL ||
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)) ||
		    IPM_PMC_STOPPED(pc, ri)) {
			continue;
		}
		
		if (!IPM_PMC_HAS_OVERFLOWED(i))
			continue;
		/*
		 * Stop the PMC
		 */
		wrmsr(IPM_MSR_EVSEL0 + i, 
		    pm->pm_md.pm_ipm.pm_ipm_evsel & (~IPM_EVSEL_EN));

		retval = 1;

		error = pmc_process_interrupt(cpu, pm, tf,
		    TRAPF_USERMODE(tf));
		if (error)
			IPM_PMC_MARK_STOPPED(pc,ri);

		/* reload sampling count */
		v = pm->pm_sc.pm_reloadcount;
		wrmsr(IPM_MSR_PERFCTR0 + i,
		    IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(v));
	}

	/*
	 * The LAPIC needs to have its PMC interrupt
	 * unmasked after a PMC interrupt.
	 */
	if (retval)
		pmc_x86_lapic_enable_pmc_interrupt();

	atomic_add_int(retval ? &pmc_stats.pm_intr_processed :
	    &pmc_stats.pm_intr_ignored, 1);

	/* restart counters */
	for (i = 0; i < IPM_NPMCS_V1 - 1; i++) {
		ri = i + 1;
		phw = pc->pc_common.pc_hwpmcs[ri];
		if ((pm = phw->phw_pmc) == NULL || 
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)) ||
		    IPM_PMC_STOPPED(pc, ri)) {
			continue;
		}
		wrmsr(IPM_MSR_EVSEL0 + i, 
		    pm->pm_md.pm_ipm.pm_ipm_evsel | IPM_EVSEL_EN);
	}
	
	return (retval);
}

static int
ipm_v2_intr(int cpu, struct trapframe *tf)
{
	int i, error, retval, ri;
	struct pmc *pm;
	struct ipm_cpu *pc;
	struct pmc_hw *phw;
	pmc_value_t v;
	uint64_t ovf_status, ctrl_msr = 0;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[ipm-v1,%d] CPU %d out of range", __LINE__, cpu));

	retval = 0;
	pc = (struct ipm_cpu *) pmc_pcpu[cpu];

	/*
	 * Stop all the PMCs.
	 */
	wrmsr(IPM_MSR_PERFCTR_CTRL, 0ULL);
	
	/*
	 * Check which PMCs have caused this interrupt
	 * and process the interrupt
	 */
	ovf_status = rdmsr(IPM_MSR_PERFCTR_STATUS);
	for (i = 0; i < IPM_NPMCS_V2 - 1; i++) {
		ri = i + 1;

		phw = pc->pc_common.pc_hwpmcs[ri];

		if ((pm = phw->phw_pmc) == NULL ||
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)) ||
		    IPM_PMC_STOPPED(pc, ri)) {
			continue;
		}

		if (!(ipm_v2_pmc_has_overflowed(ovf_status, ri))) {
			continue;
		}

		retval = 1;

		error = pmc_process_interrupt(cpu, pm, tf,
		    TRAPF_USERMODE(tf));
		if (error)
			IPM_PMC_MARK_STOPPED(pc,ri);

		/* reload sampling count */
		v = pm->pm_sc.pm_reloadcount;
		if (i < IPM_NPMCS_V1 - 1) {
			wrmsr(IPM_MSR_PERFCTR0 + i,
			    IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(v));
		} else {
			wrmsr(IPM_MSR_PERFFIXEDCTR0 + (i - 2), 
			    IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(v));
		}
	}

	/*
	 * The LAPIC needs to have its PMC interrupt
	 * unmasked after a PMC interrupt.
	 */
	if (retval)
		pmc_x86_lapic_enable_pmc_interrupt();

	atomic_add_int(retval ? &pmc_stats.pm_intr_processed :
	    &pmc_stats.pm_intr_ignored, 1);

	/* restart counters */
	for (i = 0; i < IPM_NPMCS_V2 - 1; i++) {
		ri = i + 1;
		phw = pc->pc_common.pc_hwpmcs[ri];
		if ((pm = phw->phw_pmc) == NULL ||
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)) ||
		    IPM_PMC_STOPPED(pc, ri)) {
			continue;
		}
		ctrl_msr |= ipm_v2_pmc_ri_to_msr_en_bit(ri);
	}

	wrmsr(IPM_MSR_PERFCTR_CTRL, ctrl_msr);
	
	return (retval);
}

static int
ipm_describe(int cpu, int ri, struct pmc_info *pi,
    struct pmc **ppmc)
{
	int error;
	size_t copied;
	struct pmc_hw *phw;
	const struct ipm_pmc_descr *pd;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pd  = &ipm_pmc_desc[ri];

	if ((error = copystr(pd->pm_descr.pd_name, pi->pm_name,
		 PMC_NAME_MAX, &copied)) != 0)
		return error;

	pi->pm_class = pd->pm_descr.pd_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return 0;
}

static int
ipm_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < ipm_npmcs,
	    ("[ipm-,%d ri %d out of range", __LINE__, ri));

	*msr = ipm_pmc_desc[ri].pm_pmc_msr - IPM_MSR_PERFCTR0;
	return 0;
}

int
pmc_initialize_ipm(struct pmc_mdep *pmc_mdep)
{
	KASSERT(strcmp(cpu_vendor, "GenuineIntel") == 0,
	    ("[ipm,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,1, "%s", "ipm-initialize");

	ipm_cputype = pmc_mdep->pmd_cputype;
	
	ipm_pmc_desc = ipm_pmc_desc_v1;
	ipm_events = ipm_events_v1;
	ipm_nevents = IPM_NEVENTS_V1;
	ipm_npmcs = IPM_NPMCS_V1;
	ipm_ev_pmc_mappings = ipm_ev_pmc_mappings_v1;
	ipm_nmappings = IAP1_NMAPPINGS;
	
	pmc_mdep->pmd_npmc			= IPM_NPMCS_V1;
	pmc_mdep->pmd_classes[1].pm_class	= PMC_CLASS_IAP1; 
	pmc_mdep->pmd_classes[1].pm_caps  	= IPM_PMC_CAPS;
	pmc_mdep->pmd_classes[1].pm_width	= 40;
	pmc_mdep->pmd_nclasspmcs[1] 		= 2;

	if (ipm_cputype == PMC_CPU_INTEL_CORE2) {
		/* 
		 * Modify the defaults for the V2 PMCs
		 */
		ipm_pmc_desc = ipm_pmc_desc_v2;
		ipm_events = ipm_events_v2;
		ipm_nevents = IPM_NEVENTS_V2;
		ipm_npmcs = IPM_NPMCS_V2;
		ipm_ev_pmc_mappings = ipm_ev_pmc_mappings_v2;
		ipm_nmappings = IAP2_NMAPPINGS;

		pmc_mdep->pmd_nclass 			= 3;
		pmc_mdep->pmd_npmc			= IPM_NPMCS_V2;
		pmc_mdep->pmd_classes[1].pm_class	= PMC_CLASS_IAP2; 
		
		pmc_mdep->pmd_classes[2].pm_class	= PMC_CLASS_IAF;
		pmc_mdep->pmd_classes[2].pm_caps  	= IPM_FIXEDPMC_CAPS;
		pmc_mdep->pmd_classes[2].pm_width	= 40;
		pmc_mdep->pmd_nclasspmcs[2] 		= 3;
	}
	
	pmc_mdep->pmd_init    	    = ipm_init;
	pmc_mdep->pmd_cleanup 	    = ipm_cleanup;
	pmc_mdep->pmd_switch_in     = ipm_switch_in;
	pmc_mdep->pmd_switch_out    = ipm_switch_out;
	pmc_mdep->pmd_read_pmc 	    = ipm_read_pmc;
	pmc_mdep->pmd_write_pmc     = ipm_write_pmc;
	pmc_mdep->pmd_config_pmc    = ipm_config_pmc;
	pmc_mdep->pmd_get_config    = ipm_get_config;
	pmc_mdep->pmd_allocate_pmc  = ipm_allocate_pmc;
	pmc_mdep->pmd_release_pmc   = ipm_release_pmc;
	pmc_mdep->pmd_start_pmc     = ipm_start_pmc;
	pmc_mdep->pmd_stop_pmc      = ipm_stop_pmc;
	pmc_mdep->pmd_describe      = ipm_describe;
	pmc_mdep->pmd_get_msr  	    = ipm_get_msr; /* i386 */	
	if (ipm_cputype == PMC_CPU_INTEL_CORE) {
		pmc_mdep->pmd_intr	= ipm_v1_intr;
	} else {
		pmc_mdep->pmd_intr	= ipm_v2_intr;
	}

	return 0;
}


