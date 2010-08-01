/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _POWERPC_AIM_MMU_OEA64_H
#define _POWERPC_AIM_MMU_OEA64_H

#include <machine/mmuvar.h>

/*
 * Helper routines
 */

/* Allocate physical memory for use in moea64_bootstrap. */
vm_offset_t	moea64_bootstrap_alloc(vm_size_t, u_int);

/* Bootstrap bits before the page table is allocated */
void		moea64_early_bootstrap(mmu_t mmup, vm_offset_t kernelstart,
		    vm_offset_t kernelend);
/* Bootstrap bits after the page table is allocated */
void		moea64_late_bootstrap(mmu_t mmup, vm_offset_t kernelstart,
		    vm_offset_t kernelend);

/*
 * Statistics
 */

extern u_int	moea64_pte_valid;
extern u_int	moea64_pte_overflow;

/*
 * State variables
 */

extern struct pvo_head *moea64_pvo_table;
extern int		moea64_large_page_shift;
extern u_int		moea64_pteg_count;
extern u_int		moea64_pteg_mask;

/*
 * Page table manipulation hooks
 */

extern void	(*moea64_pte_synch_hook)(struct lpte *pt, struct lpte *pvo_pt);
extern void	(*moea64_pte_clear_hook)(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn, u_int64_t ptebit);
extern void	(*moea64_pte_unset_hook)(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn);
extern void	(*moea64_pte_change_hook)(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn);
extern int	(*moea64_pte_insert_hook)(u_int ptegidx, struct lpte *pvo_pt);
extern struct lpte *(*moea64_pvo_to_pte_hook)(const struct pvo_entry *pvo);

/* MMU Interface */

void		moea64_change_wiring(mmu_t, pmap_t, vm_offset_t, boolean_t);
void		moea64_clear_modify(mmu_t, vm_page_t);
void		moea64_clear_reference(mmu_t, vm_page_t);
void		moea64_copy_page(mmu_t, vm_page_t, vm_page_t);
void		moea64_enter(mmu_t, pmap_t, vm_offset_t, vm_page_t, vm_prot_t,
		    boolean_t);
void		moea64_enter_object(mmu_t, pmap_t, vm_offset_t, vm_offset_t,
		    vm_page_t, vm_prot_t);
void		moea64_enter_quick(mmu_t, pmap_t, vm_offset_t, vm_page_t,
		    vm_prot_t);
vm_paddr_t	moea64_extract(mmu_t, pmap_t, vm_offset_t);
vm_page_t	moea64_extract_and_hold(mmu_t, pmap_t, vm_offset_t, vm_prot_t);
void		moea64_init(mmu_t);
boolean_t	moea64_is_modified(mmu_t, vm_page_t);
boolean_t	moea64_is_referenced(mmu_t, vm_page_t);
boolean_t	moea64_ts_referenced(mmu_t, vm_page_t);
vm_offset_t	moea64_map(mmu_t, vm_offset_t *, vm_offset_t, vm_offset_t, int);
boolean_t	moea64_page_exists_quick(mmu_t, pmap_t, vm_page_t);
int		moea64_page_wired_mappings(mmu_t, vm_page_t);
void		moea64_pinit(mmu_t, pmap_t);
void		moea64_pinit0(mmu_t, pmap_t);
void		moea64_protect(mmu_t, pmap_t, vm_offset_t, vm_offset_t,
		    vm_prot_t);
void		moea64_qenter(mmu_t, vm_offset_t, vm_page_t *, int);
void		moea64_qremove(mmu_t, vm_offset_t, int);
void		moea64_release(mmu_t, pmap_t);
void		moea64_remove(mmu_t, pmap_t, vm_offset_t, vm_offset_t);
void		moea64_remove_all(mmu_t, vm_page_t);
void		moea64_remove_write(mmu_t, vm_page_t);
void		moea64_zero_page(mmu_t, vm_page_t);
void		moea64_zero_page_area(mmu_t, vm_page_t, int, int);
void		moea64_zero_page_idle(mmu_t, vm_page_t);
void		moea64_activate(mmu_t, struct thread *);
void		moea64_deactivate(mmu_t, struct thread *);
void		*moea64_mapdev(mmu_t, vm_offset_t, vm_size_t);
void		moea64_unmapdev(mmu_t, vm_offset_t, vm_size_t);
vm_offset_t	moea64_kextract(mmu_t, vm_offset_t);
void		moea64_kenter(mmu_t, vm_offset_t, vm_offset_t);
boolean_t	moea64_dev_direct_mapped(mmu_t, vm_offset_t, vm_size_t);
void		moea64_sync_icache(mmu_t, pmap_t, vm_offset_t, vm_size_t);

#endif /* _POWERPC_AIM_MMU_OEA64_H */

