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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

#include <powerpc/aim/mmu_oea64.h>

#include "mmu_if.h"
#include "ps3-hvcall.h"

#define VSID_HASH_MASK		0x0000007fffffffffUL
#define PTESYNC()		__asm __volatile("ptesync")

extern int ps3fb_remap(void);

static uint64_t mps3_vas_id;

/*
 * Kernel MMU interface
 */

static void	mps3_bootstrap(mmu_t mmup, vm_offset_t kernelstart,
		    vm_offset_t kernelend);
static void	mps3_cpu_bootstrap(mmu_t mmup, int ap);
static void	mps3_pte_synch(struct lpte *pt, struct lpte *pvo_pt);
static void	mps3_pte_clear(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn, u_int64_t ptebit);
static void	mps3_pte_unset(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static void	mps3_pte_change(struct lpte *pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static int	mps3_pte_insert(u_int ptegidx, struct lpte *pvo_pt);
static struct lpte *mps3_pvo_to_pte(const struct pvo_entry *pvo);


static mmu_method_t mps3_methods[] = {
        MMUMETHOD(mmu_change_wiring,    moea64_change_wiring),
        MMUMETHOD(mmu_clear_modify,     moea64_clear_modify),
        MMUMETHOD(mmu_clear_reference,  moea64_clear_reference),
        MMUMETHOD(mmu_copy_page,        moea64_copy_page),
        MMUMETHOD(mmu_enter,            moea64_enter),
        MMUMETHOD(mmu_enter_object,     moea64_enter_object),
        MMUMETHOD(mmu_enter_quick,      moea64_enter_quick),
        MMUMETHOD(mmu_extract,          moea64_extract),
        MMUMETHOD(mmu_extract_and_hold, moea64_extract_and_hold),
        MMUMETHOD(mmu_init,             moea64_init),
        MMUMETHOD(mmu_is_modified,      moea64_is_modified),
        MMUMETHOD(mmu_is_referenced,    moea64_is_referenced),
        MMUMETHOD(mmu_ts_referenced,    moea64_ts_referenced),
        MMUMETHOD(mmu_map,              moea64_map),
        MMUMETHOD(mmu_page_exists_quick,moea64_page_exists_quick),
        MMUMETHOD(mmu_page_wired_mappings,moea64_page_wired_mappings),
        MMUMETHOD(mmu_pinit,            moea64_pinit),
        MMUMETHOD(mmu_pinit0,           moea64_pinit0),
        MMUMETHOD(mmu_protect,          moea64_protect),
        MMUMETHOD(mmu_qenter,           moea64_qenter),
        MMUMETHOD(mmu_qremove,          moea64_qremove),
        MMUMETHOD(mmu_release,          moea64_release),
        MMUMETHOD(mmu_remove,           moea64_remove),
        MMUMETHOD(mmu_remove_all,       moea64_remove_all),
        MMUMETHOD(mmu_remove_write,     moea64_remove_write),
        MMUMETHOD(mmu_sync_icache,      moea64_sync_icache),
        MMUMETHOD(mmu_zero_page,        moea64_zero_page),
        MMUMETHOD(mmu_zero_page_area,   moea64_zero_page_area),
        MMUMETHOD(mmu_zero_page_idle,   moea64_zero_page_idle),
        MMUMETHOD(mmu_activate,         moea64_activate),
        MMUMETHOD(mmu_deactivate,       moea64_deactivate),
 
        /* Internal interfaces */
        MMUMETHOD(mmu_bootstrap,        mps3_bootstrap),
        MMUMETHOD(mmu_cpu_bootstrap,    mps3_cpu_bootstrap),
        MMUMETHOD(mmu_mapdev,           moea64_mapdev),
        MMUMETHOD(mmu_unmapdev,         moea64_unmapdev),
        MMUMETHOD(mmu_kextract,         moea64_kextract),
        MMUMETHOD(mmu_kenter,           moea64_kenter),
        MMUMETHOD(mmu_dev_direct_mapped,moea64_dev_direct_mapped),

        { 0, 0 }
};

static mmu_def_t ps3_mmu = {
        "mmu_ps3",
        mps3_methods,
        0
};
MMU_DEF(ps3_mmu);

static void
mps3_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	uint64_t final_pteg_count;

	/*
	 * Set our page table override functions
	 */
	moea64_pte_synch_hook = mps3_pte_synch;
	moea64_pte_clear_hook = mps3_pte_clear;
	moea64_pte_unset_hook = mps3_pte_unset;
	moea64_pte_change_hook = mps3_pte_change;
	moea64_pte_insert_hook = mps3_pte_insert;
	moea64_pvo_to_pte_hook = mps3_pvo_to_pte;

	moea64_early_bootstrap(mmup, kernelstart, kernelend);

	lv1_construct_virtual_address_space(
	    20 /* log_2(moea64_pteg_count) */, 2 /* n page sizes */,
	    (24UL << 56) | (16UL << 48) /* page sizes 16 MB + 64 KB */,
	    &mps3_vas_id, &final_pteg_count
	);

	moea64_pteg_count = final_pteg_count / sizeof(struct lpteg);

	moea64_late_bootstrap(mmup, kernelstart, kernelend);
}

static void
mps3_cpu_bootstrap(mmu_t mmup, int ap)
{
	struct slb *slb = PCPU_GET(slb);
	register_t seg0;
	int i;

	mtmsr(mfmsr() & ~PSL_DR & ~PSL_IR);

	/*
	 * Destroy the loader's address space if we are coming up for
	 * the first time, and redo the FB mapping so we can continue
	 * having a console.
	 */

	if (!ap)
		lv1_destruct_virtual_address_space(0);

	lv1_select_virtual_address_space(mps3_vas_id);

	if (!ap)
		ps3fb_remap();

	/*
	 * Install kernel SLB entries
	 */

        __asm __volatile ("slbia");
        __asm __volatile ("slbmfee %0,%1; slbie %0;" : "=r"(seg0) : "r"(0));
	for (i = 0; i < 64; i++) {
		if (!(slb[i].slbe & SLBE_VALID))
			continue;

		__asm __volatile ("slbmte %0, %1" ::
		    "r"(slb[i].slbv), "r"(slb[i].slbe));
	}
}

static void
mps3_pte_synch(struct lpte *pt, struct lpte *pvo_pt)
{
	uint64_t halfbucket[4], rcbits;
	uint64_t slot = (uint64_t)(pt)-1;
	
	PTESYNC();
	lv1_read_htab_entries(mps3_vas_id, slot & ~0x3UL, &halfbucket[0],
	    &halfbucket[1], &halfbucket[2], &halfbucket[3], &rcbits);

	/*
	 * rcbits contains the low 12 bits of each PTEs 2nd part,
	 * spaced at 16-bit intervals
	 */

	KASSERT((halfbucket[slot & 0x3] & LPTE_AVPN_MASK) ==
	    (pvo_pt->pte_hi & LPTE_AVPN_MASK),
	    ("PTE upper word %#lx != %#lx\n",
	    halfbucket[slot & 0x3], pvo_pt->pte_hi));

 	pvo_pt->pte_lo |= (rcbits >> ((3 - (slot & 0x3))*16)) &
	    (LPTE_CHG | LPTE_REF);
}

static void
mps3_pte_clear(struct lpte *pt, struct lpte *pvo_pt, uint64_t vpn,
    u_int64_t ptebit)
{
	uint64_t slot = (uint64_t)(pt)-1;

	lv1_write_htab_entry(mps3_vas_id, slot, pvo_pt->pte_hi,
	    pvo_pt->pte_lo & ~ptebit);
}

static void
mps3_pte_unset(struct lpte *pt, struct lpte *pvo_pt, uint64_t vpn)
{
	uint64_t slot = (uint64_t)(pt)-1;

	mps3_pte_synch(pt, pvo_pt);
	pvo_pt->pte_hi &= ~LPTE_VALID;
	lv1_write_htab_entry(mps3_vas_id, slot, 0, 0);
	moea64_pte_valid--;
}

static void
mps3_pte_change(struct lpte *pt, struct lpte *pvo_pt, uint64_t vpn)
{
	uint64_t slot = (uint64_t)(pt)-1;
 
	mps3_pte_synch(pt, pvo_pt);
	lv1_write_htab_entry(mps3_vas_id, slot, pvo_pt->pte_hi,
	    pvo_pt->pte_lo);
}

static int
mps3_pte_insert(u_int ptegidx, struct lpte *pvo_pt)
{
	int result;
	struct lpte evicted;
	struct pvo_entry *pvo;
	uint64_t index;

	pvo_pt->pte_hi |= LPTE_VALID;
	pvo_pt->pte_hi &= ~LPTE_HID;
	evicted.pte_hi = 0;
	result = lv1_insert_htab_entry(mps3_vas_id, ptegidx << 3,
	    pvo_pt->pte_hi, pvo_pt->pte_lo, LPTE_LOCKED | LPTE_WIRED, 0,
	    &index, &evicted.pte_hi, &evicted.pte_lo);

	if (result != 0) {
		/* No freeable slots in either PTEG? We're hosed. */
		panic("mps3_pte_insert: overflow (%d)", result);
		return (-1);
	}

	/*
	 * See where we ended up.
	 */
	if (index >> 3 != ptegidx)
		pvo_pt->pte_hi |= LPTE_HID;

	moea64_pte_valid++;

	if (!evicted.pte_hi)
		return (index & 0x7);

	/*
	 * Synchronize the sacrifice PTE with its PVO, then mark both
	 * invalid. The PVO will be reused when/if the VM system comes
	 * here after a fault.
	 */

	ptegidx = index >> 3; /* Where the sacrifice PTE was found */
	if (evicted.pte_hi & LPTE_HID)
		ptegidx ^= moea64_pteg_mask; /* PTEs indexed by primary */

	result = 0;
	LIST_FOREACH(pvo, &moea64_pvo_table[ptegidx], pvo_olink) {
		if (!PVO_PTEGIDX_ISSET(pvo))
			continue;

		if (pvo->pvo_pte.lpte.pte_hi == (evicted.pte_hi | LPTE_VALID)) {
			KASSERT(pvo->pvo_pte.lpte.pte_hi & LPTE_VALID,
			    ("Invalid PVO for valid PTE!"));
			pvo->pvo_pte.lpte.pte_hi &= ~LPTE_VALID;
			pvo->pvo_pte.lpte.pte_lo |=
			    evicted.pte_lo & (LPTE_REF | LPTE_CHG);
			PVO_PTEGIDX_CLR(pvo);
			moea64_pte_valid--;
			moea64_pte_overflow++;
			result = 1;
			break;
		}
	}

	KASSERT(result == 1, ("PVO for sacrifice PTE not found"));

	return (index & 0x7);
}

static __inline u_int
va_to_pteg(uint64_t vsid, vm_offset_t addr, int large)
{
	uint64_t hash;
	int shift;

	shift = large ? moea64_large_page_shift : ADDR_PIDX_SHFT;
	hash = (vsid & VSID_HASH_MASK) ^ (((uint64_t)addr & ADDR_PIDX) >>
	    shift);
	return (hash & moea64_pteg_mask);
}

static struct lpte *
mps3_pvo_to_pte(const struct pvo_entry *pvo)
{
	uint64_t slot, vsid;
	u_int ptegidx;

	/* If the PTEG index is not set, then there is no page table entry */
	if (!PVO_PTEGIDX_ISSET(pvo))
		return (NULL);

	vsid = PVO_VSID(pvo);
	ptegidx = va_to_pteg(vsid, PVO_VADDR(pvo), pvo->pvo_vaddr & PVO_LARGE);

	/*
	 * We can find the actual pte entry without searching by grabbing
	 * the PTEG index from 3 unused bits in pvo_vaddr and by
	 * noticing the HID bit.
	 */
	if (pvo->pvo_pte.lpte.pte_hi & LPTE_HID)
		ptegidx ^= moea64_pteg_mask;

	slot = (ptegidx << 3) | PVO_PTEGIDX_GET(pvo);

	return ((struct lpte *)(slot + 1));
}

