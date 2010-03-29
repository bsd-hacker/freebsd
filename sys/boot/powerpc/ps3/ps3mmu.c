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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/boot/powerpc/ofw/start.c 174722 2007-12-17 22:18:07Z marcel $");

#include <stand.h>
#include <stdint.h>

#define _KERNEL
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/slb.h>

#include "bootstrap.h"
#include "lv1call.h"

#define PS3_LPAR_VAS_ID_CURRENT 0

register_t pteg_count, pteg_mask;

void
ps3mmu_map(uint64_t va, uint64_t pa)
{
	struct lpte pt, expt;
	struct lpteg pteg;
	uint64_t idx, vsid, ptegidx;
	
	if (pa < 0x8000000) { /* Phys mem? */
		pt.pte_hi = LPTE_BIG;
		pt.pte_lo = LPTE_M;
		vsid = 0;
	} else {
		pt.pte_hi = 0;
		pt.pte_lo = LPTE_I | LPTE_G;
		vsid = 1;
	}

	pt.pte_hi |= (vsid << LPTE_VSID_SHIFT) |
            (((uint64_t)(va & ADDR_PIDX) >> ADDR_API_SHFT64) & LPTE_API);
	pt.pte_hi |= LPTE_VALID;

	ptegidx = vsid ^ (((uint64_t)va & ADDR_PIDX) >> ADDR_PIDX_SHFT);
	ptegidx &= pteg_mask;

	lv1_insert_htab_entry(PS3_LPAR_VAS_ID_CURRENT, ptegidx, pt.pte_hi,
	    pt.pte_lo, 0x10, 0, &idx, &expt.pte_hi, &expt.pte_lo);
}

int
ps3mmu_init(int maxmem)
{
	uint64_t as, ptsize;
	int i;

	lv1_construct_virtual_address_space(18 /* log2 256 KB */, 1,
	    24ULL << 56, &as, &ptsize);
	pteg_count = ptsize / sizeof(struct lpteg);
	pteg_mask = pteg_count - 1;

	lv1_select_virtual_address_space(as);
	for (i = 0; i < maxmem; i += 16*1024*1024)
		ps3mmu_map(i,i);
	__asm __volatile ("slbia; slbmte %0, %1; slbmte %2,%3" ::
	    "r"(0 | SLBV_L), "r"(0 | SLBE_VALID),
	    "r"(1 << SLBV_VSID_SHIFT),
	    "r"((0xf << SLBE_ESID_SHIFT) | SLBE_VALID | 1));

	mtmsr(mfmsr() | PSL_IR | PSL_DR);
}

