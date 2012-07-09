/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */
/* $FreeBSD$ */

#ifndef _MACHDEP_BOOT_MACHDEP_H_
#define _MACHDEP_BOOT_MACHDEP_H_

#include <machine/fdt.h>

/* misc prototypes used by the many arm machdeps */
void arm_lock_cache_line(vm_offset_t);
vm_offset_t fake_preload_metadata(void);
void init_proc0(vm_offset_t kstack);
void *arm_mmu_init(uint32_t, uint32_t, int);
void set_stackptrs(int);
void physmap_init(struct mem_region *, int);
void halt(void);
void data_abort_handler(trapframe_t *);
void prefetch_abort_handler(trapframe_t *);
void undefinedinstruction_bounce(trapframe_t *);

#endif /* !_MACHINE_MACHDEP_H_ */
