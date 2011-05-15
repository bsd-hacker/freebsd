/*
 * Copyright (C) 2010 Andreas Tobler
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

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

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
#include "moea64_if.h"

#include "phyp-hvcall.h"

/*
 * Kernel MMU interface
 */

static void	mphyp_bootstrap(mmu_t mmup, vm_offset_t kernelstart,
		    vm_offset_t kernelend);
static void	mphyp_cpu_bootstrap(mmu_t mmup, int ap);
static void	mphyp_pte_synch(mmu_t, uintptr_t pt, struct lpte *pvo_pt);
static void	mphyp_pte_clear(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn, u_int64_t ptebit);
static void	mphyp_pte_unset(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static void	mphyp_pte_change(mmu_t, uintptr_t pt, struct lpte *pvo_pt,
		    uint64_t vpn);
static int	mphyp_pte_insert(mmu_t, u_int ptegidx, struct lpte *pvo_pt);
static uintptr_t mphyp_pvo_to_pte(mmu_t, const struct pvo_entry *pvo);

#define VSID_HASH_MASK		0x0000007fffffffffULL


static mmu_method_t mphyp_methods[] = {
        MMUMETHOD(mmu_bootstrap,        mphyp_bootstrap),
        MMUMETHOD(mmu_cpu_bootstrap,    mphyp_cpu_bootstrap),

	MMUMETHOD(moea64_pte_synch,     mphyp_pte_synch),
        MMUMETHOD(moea64_pte_clear,     mphyp_pte_clear),
        MMUMETHOD(moea64_pte_unset,     mphyp_pte_unset),
        MMUMETHOD(moea64_pte_change,    mphyp_pte_change),
        MMUMETHOD(moea64_pte_insert,    mphyp_pte_insert),
        MMUMETHOD(moea64_pvo_to_pte,    mphyp_pvo_to_pte),

        { 0, 0 }
};

MMU_DEF_INHERIT(pseries_mmu, "mmu_phyp", mphyp_methods, 0, oea64_mmu);

static void
mphyp_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	uint64_t final_pteg_count = 0;
	char buf[8];
	uint32_t prop[2];
        phandle_t dev, node, root;
        int res;

	printf("%s: %d\n", __FILE__, __LINE__);

	moea64_early_bootstrap(mmup, kernelstart, kernelend);
	printf("%s: %d\n", __FILE__, __LINE__);

	root = OF_peer(0);

        dev = OF_child(root);
	while (dev != 0) {
                res = OF_getprop(dev, "name", buf, sizeof(buf));
                if (res > 0 && strcmp(buf, "cpus") == 0)
                        break;
                dev = OF_peer(dev);
        }

	node = OF_child(dev);

	while (node != 0) {
                res = OF_getprop(node, "device_type", buf, sizeof(buf));
                if (res > 0 && strcmp(buf, "cpu") == 0)
                        break;
                node = OF_peer(node);
        }
	
	res = OF_getprop(node, "ibm,pft-size", prop, sizeof(prop));
	if (prop != NULL)
		final_pteg_count = 1 << prop[1];

	printf("final_pteg_count: %#x\n", (u_int)final_pteg_count);

	moea64_pteg_count = final_pteg_count / sizeof(struct lpteg);

	printf("%s: %d\n", __FILE__, __LINE__);
	moea64_mid_bootstrap(mmup, kernelstart, kernelend);
	printf("%s: %d\n", __FILE__, __LINE__);
	moea64_late_bootstrap(mmup, kernelstart, kernelend);
	printf("%s: %d\n", __FILE__, __LINE__);
}

static void
mphyp_cpu_bootstrap(mmu_t mmup, int ap)
{
	struct slb *slb = PCPU_GET(slb);
	register_t seg0;
	int i;

	/*
	 * Install kernel SLB entries
	 */

	printf("%s: %d\n", __FILE__, __LINE__);
        __asm __volatile ("slbia");
        __asm __volatile ("slbmfee %0,%1; slbie %0;" : "=r"(seg0) : "r"(0));
	printf("%s: %d\n", __FILE__, __LINE__);
	for (i = 0; i < 64; i++) {
		if (!(slb[i].slbe & SLBE_VALID))
			continue;

		__asm __volatile ("slbmte %0, %1" ::
		    "r"(slb[i].slbv), "r"(slb[i].slbe));
	}
	printf("%s: %d\n", __FILE__, __LINE__);
}

static void
mphyp_pte_synch(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt)
{
	struct lpte pte;
	uint64_t junk;

	phyp_pft_hcall(H_READ, 0, slot, 0, 0, &pte.pte_hi, &pte.pte_lo,
	    &junk);

	pvo_pt->pte_lo |= pte.pte_lo & (LPTE_CHG | LPTE_REF);
}

static void
mphyp_pte_clear(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn,
    u_int64_t ptebit)
{

	if (ptebit & LPTE_CHG)
		phyp_hcall(H_CLEAR_MOD, 0, slot);
	if (ptebit & LPTE_REF)
		phyp_hcall(H_CLEAR_REF, 0, slot);
}

static void
mphyp_pte_unset(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn)
{

	/* XXX: last argument can check the VPN -- set flag to enable */
	phyp_hcall(H_REMOVE, 0, slot, vpn);
}

static void
mphyp_pte_change(mmu_t mmu, uintptr_t slot, struct lpte *pvo_pt, uint64_t vpn)
{
	struct lpte evicted;
	uint64_t index, junk;
	int64_t result;

	/*
	 * NB: this is protected by the global table lock, so this two-step
	 * is safe, except for the scratch-page case. No CPUs on which we run
	 * this code should be using scratch pages.
	 */

	/* XXX: optimization using H_PROTECT for common case? */
	result = phyp_hcall(H_REMOVE, 0, slot, vpn);
	if (result != H_SUCCESS)
		panic("mphyp_pte_change() invalidation failure: %ld\n", result);
	phyp_pft_hcall(H_ENTER, H_EXACT, slot, pvo_pt->pte_hi,
	    pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result != H_SUCCESS)
		panic("mphyp_pte_change() insertion failure: %ld\n", result);
}

static int
mphyp_pte_insert(mmu_t mmu, u_int ptegidx, struct lpte *pvo_pt)
{
	int64_t result;
	struct lpte evicted;
	uint64_t index, junk;
	u_int pteg_bktidx;

	pvo_pt->pte_hi |= LPTE_VALID;
	pvo_pt->pte_hi &= ~LPTE_HID;
	evicted.pte_hi = 0;

	/*
	 * First try primary hash.
	 */
	pteg_bktidx = ptegidx;
	result = phyp_pft_hcall(H_ENTER, 0, pteg_bktidx << 3, pvo_pt->pte_hi,
	    pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS)
		return (index & 0x07);

	/*
	 * First try primary hash.
	 */
	pteg_bktidx ^= moea64_pteg_mask;
	pvo_pt->pte_hi |= LPTE_HID;
	result = phyp_pft_hcall(H_ENTER, 0, pteg_bktidx << 3,
	    pvo_pt->pte_hi, pvo_pt->pte_lo, &index, &evicted.pte_lo, &junk);
	if (result == H_SUCCESS)
		return (index & 0x07);

	panic("OVERFLOW (%ld)", result);
	return (-1);
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

static uintptr_t
mphyp_pvo_to_pte(mmu_t mmu, const struct pvo_entry *pvo)
{
	uint64_t vsid;
	u_int ptegidx;

	/* If the PTEG index is not set, then there is no page table entry */
	if (!PVO_PTEGIDX_ISSET(pvo))
		return (-1);

	vsid = PVO_VSID(pvo);
	ptegidx = va_to_pteg(vsid, PVO_VADDR(pvo), pvo->pvo_vaddr & PVO_LARGE);

	/*
	 * We can find the actual pte entry without searching by grabbing
	 * the PTEG index from 3 unused bits in pvo_vaddr and by
	 * noticing the HID bit.
	 */
	if (pvo->pvo_pte.lpte.pte_hi & LPTE_HID)
		ptegidx ^= moea64_pteg_mask;

	return ((ptegidx << 3) | PVO_PTEGIDX_GET(pvo));
}

