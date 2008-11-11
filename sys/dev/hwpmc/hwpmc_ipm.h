/*-
 * Copyright (c) 2005, Joseph Koshy
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
 *
 * $FreeBSD$
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_INTEL_APM_H_
#define	_DEV_HWPMC_INTEL_APM_H_

/* 
 * Definitions common to Intel Perfromance Monitoring
 * Version 1 and Version 2 PMCs
 */

/*
 * Information about PMCs can be enumerated using the cpuid.
 * But we'll stick to hardcoded definitions.
 */

/* 
 * IPM v1 facilities has 2 PMCs IA32_PMC0 and IA32_PMC1
 */
#define	IPM_NPMCS_V1	3	/* 1 TSC + 2 PMCs */

/*
 * IPM v2 facilities add 3 dedicated fixed funciton performance counters
 * for counting predefined performance monitoring events 
 */
#define	IPM_NPMCS_V2	6	/* 1 TSC + 2 PMCs + 3 Fixed Function PMCs */

#define	IPM_NPMCS	6	/* Max */

/*
 * Event select registers
 */
#define	IPM_MSR_EVSEL0			0x0186
#define	IPM_MSR_EVSEL1			0x0187

/* 
 * Performance counter registers
 */
#define	IPM_MSR_PERFCTR0		0x00C1
#define	IPM_MSR_PERFCTR1		0x00C2

/*
 * Fixed function performance counter registers
 */
#define	IPM_MSR_PERFFIXEDCTR0		0x0309
#define	IPM_MSR_PERFFIXEDCTR1		0x030A
#define	IPM_MSR_PERFFIXEDCTR2		0x030B

/*
 * Fixed function performance counter configuration register
 */
#define IPM_MSR_PERFFIXEDCTR_CTRL	0x038D

/*
 * PM v2 simplified global counter control facilities register
 */
#define	IPM_MSR_PERFCTR_STATUS		0x038E
#define	IPM_MSR_PERFCTR_CTRL		0x038F
#define	IPM_MSR_PERFCTR_OVF_CTRL	0x0390

/*
 * Common macros
 */
#define	IPM_EVSEL_CMASK_MASK		0xFF000000
#define	IPM_EVSEL_TO_CMASK(C)		(((C) & 0xFF) << 24)
#define	IPM_EVSEL_INV			(1ULL << 23)
#define	IPM_EVSEL_EN			(1ULL << 22)
#define	IPM_EVSEL_INT			(1ULL << 20)
#define	IPM_EVSEL_PC			(1ULL << 19)
#define	IPM_EVSEL_E			(1ULL << 18)
#define	IPM_EVSEL_OS			(1ULL << 17)
#define	IPM_EVSEL_USR			(1ULL << 16)
#define	IPM_EVSEL_UMASK_MASK		(0x0000FF00)
#define	IPM_EVSEL_TO_UMASK(U)		(((U) & 0xFF) << 8)
#define	IPM_EVSEL_EVENT_SELECT(ES)	((ES) & 0xFF)
#define	IPM_EVSEL_RESERVED		(1 << 21)

#define	IPM_FIXEDCTR_PL_OS		0x1
#define	IPM_FIXEDCTR_PL_USR		0x2
#define	IPM_FIXEDCTR_PL_ANY		0x3
#define	IPM_FIXEDCTR_INT		0x8		
#define	IPM_FIXEDCTR_DISABLE(C)		~((uint64_t)((0xF << (4 * (C)))))

#define	IPM_OVF_CLR_ALL_BITS		0xC000000700000003LL

#define	IPM_PERFCTR_READ_MASK		0xFFFFFFFFFFLL	/* 40 bits */
#define	IPM_PERFCTR_WRITE_MASK		0xFFFFFFFFU	/* 32 bits */

#define	IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(R)	(-(R))
#define	IPM_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	(-(P))

#define	IPM_PMC_HAS_OVERFLOWED(P)	((rdpmc(P) & (1LL << 39)) == 0)


struct pmc_md_ipm_op_pmcallocate {
	uint32_t	pm_ipm_config;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_ipm_pmc {
	uint32_t	pm_ipm_evsel;
};

/*
 * Prototypes
 */
int	pmc_initialize_ipm(struct pmc_mdep *);

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_INTEL_APM_H_ */
/*-
 * Copyright (c) 2005, Joseph Koshy
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
 *
 * $FreeBSD$
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_INTEL_APM_H_
#define	_DEV_HWPMC_INTEL_APM_H_

/* 
 * Definitions common to Intel Perfromance Monitoring
 * Version 1 and Version 2 PMCs
 */

/*
 * Information about PMCs can be enumerated using the cpuid.
 * But we'll stick to hardcoded definitions.
 */

/* 
 * IPM v1 facilities has 2 PMCs IA32_PMC0 and IA32_PMC1
 */
#define	IPM_NPMCS_V1	3	/* 1 TSC + 2 PMCs */

/*
 * IPM v2 facilities add 3 dedicated fixed funciton performance counters
 * for counting predefined performance monitoring events 
 */
#define	IPM_NPMCS_V2	6	/* 1 TSC + 2 PMCs + 3 Fixed Function PMCs */

#define	IPM_NPMCS	6	/* Max */

/*
 * Event select registers
 */
#define	IPM_MSR_EVSEL0			0x0186
#define	IPM_MSR_EVSEL1			0x0187

/* 
 * Performance counter registers
 */
#define	IPM_MSR_PERFCTR0		0x00C1
#define	IPM_MSR_PERFCTR1		0x00C2

/*
 * Fixed function performance counter registers
 */
#define	IPM_MSR_PERFFIXEDCTR0		0x0309
#define	IPM_MSR_PERFFIXEDCTR1		0x030A
#define	IPM_MSR_PERFFIXEDCTR2		0x030B

/*
 * Fixed function performance counter configuration register
 */
#define IPM_MSR_PERFFIXEDCTR_CTRL	0x038D

/*
 * PM v2 simplified global counter control facilities register
 */
#define	IPM_MSR_PERFCTR_STATUS		0x038E
#define	IPM_MSR_PERFCTR_CTRL		0x038F
#define	IPM_MSR_PERFCTR_OVF_CTRL	0x0390

/*
 * Common macros
 */
#define	IPM_EVSEL_CMASK_MASK		0xFF000000
#define	IPM_EVSEL_TO_CMASK(C)		(((C) & 0xFF) << 24)
#define	IPM_EVSEL_INV			(1ULL << 23)
#define	IPM_EVSEL_EN			(1ULL << 22)
#define	IPM_EVSEL_INT			(1ULL << 20)
#define	IPM_EVSEL_PC			(1ULL << 19)
#define	IPM_EVSEL_E			(1ULL << 18)
#define	IPM_EVSEL_OS			(1ULL << 17)
#define	IPM_EVSEL_USR			(1ULL << 16)
#define	IPM_EVSEL_UMASK_MASK		(0x0000FF00)
#define	IPM_EVSEL_TO_UMASK(U)		(((U) & 0xFF) << 8)
#define	IPM_EVSEL_EVENT_SELECT(ES)	((ES) & 0xFF)
#define	IPM_EVSEL_RESERVED		(1 << 21)

#define	IPM_FIXEDCTR_PL_OS		0x1
#define	IPM_FIXEDCTR_PL_USR		0x2
#define	IPM_FIXEDCTR_PL_ANY		0x3
#define	IPM_FIXEDCTR_INT		0x8		
#define	IPM_FIXEDCTR_DISABLE(C)		~((uint64_t)((0xF << (4 * (C)))))

#define	IPM_OVF_CLR_ALL_BITS		0xC000000700000003LL

#define	IPM_PERFCTR_READ_MASK		0xFFFFFFFFFFLL	/* 40 bits */
#define	IPM_PERFCTR_WRITE_MASK		0xFFFFFFFFU	/* 32 bits */

#define	IPM_RELOAD_COUNT_TO_PERFCTR_VALUE(R)	(-(R))
#define	IPM_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	(-(P))

#define	IPM_PMC_HAS_OVERFLOWED(P)	((rdpmc(P) & (1LL << 39)) == 0)


struct pmc_md_ipm_op_pmcallocate {
	uint32_t	pm_ipm_config;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_ipm_pmc {
	uint32_t	pm_ipm_evsel;
};

/*
 * Prototypes
 */
int	pmc_initialize_ipm(struct pmc_mdep *);

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_INTEL_APM_H_ */
