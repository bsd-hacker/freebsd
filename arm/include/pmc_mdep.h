/*-
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_PMC_MDEP_H_
#define	_MACHINE_PMC_MDEP_H_

union pmc_md_op_pmcallocate {
	uint64_t	__pad[4];
};

/* Logging */
#define	PMCLOG_READADDR		PMCLOG_READ32
#define	PMCLOG_EMITADDR		PMCLOG_EMIT32

#ifdef	_KERNEL
union pmc_md_pmc {
	struct pmc_md_xscale_pmc	pm_xscale;
};

#define	PMC_TRAPFRAME_TO_PC(TF)	((TF)->tf_pc)
#define	PMC_TRAPFRAME_TO_FP(TF)	((TF)->tf_usr_lr)
#define	PMC_TRAPFRAME_TO_SP(TF)	((TF)->tf_usr_sp)

/*
 * Prototypes
 */
struct pmc_mdep *pmc_xscale_initialize(void);
void		pmc_xscale_finalize(struct pmc_mdep *_md);
#endif /* _KERNEL */

#endif /* !_MACHINE_PMC_MDEP_H_ */
