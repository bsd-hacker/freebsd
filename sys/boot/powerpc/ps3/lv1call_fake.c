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
#include <machine/pte.h>

#include "bootstrap.h"
#include "lv1call.h"

#define PTEG_COUNT	2048
#define PTEG_MASK	((uint64_t)PTEG_COUNT - 1)

static struct lpteg *pagetable = (struct lpteg *)0x80000;

int mambocall(int, ...);
#define mambo_print(a) mambocall(0,a,strlen(a));

int
lv1_insert_htab_entry(register_t htab_id, register_t ptegidx, 
    uint64_t pte_hi, uint64_t pte_lo, register_t lockflags,
    register_t flags)
{
	struct  lpte *pt;
	int     i;

	/*
	 * First try primary hash.
	 */
	for (pt = pagetable[ptegidx].pt, i = 0; i < 8; i++, pt++) {
		if (!(pt->pte_hi & LPTE_VALID)) {
			pte_hi &= ~LPTE_HID;
			pt->pte_lo = pte_lo;
			pt->pte_hi = pte_hi;
			return (0);
		}
	}

	/*
	 * Now try secondary hash.
	 */
        ptegidx ^= PTEG_MASK;

	for (pt = pagetable[ptegidx].pt, i = 0; i < 8; i++, pt++) {
		if (!(pt->pte_hi & LPTE_VALID)) {
			pte_hi |= LPTE_HID;
			pt->pte_lo = pte_lo;
			pt->pte_hi = pte_hi;
			return (0);
		}
        }

	return (-1);
}

int
lv1_construct_virtual_address_space(int htab_size, int npgsizes,
    uint64_t page_sizes, uint64_t *as_id, uint64_t *ptsize)
{
	*ptsize = PTEG_COUNT * sizeof(struct lpteg);
	*as_id = 0;
	return (0);
}

int
lv1_select_virtual_address_space(uint64_t as)
{
	__asm __volatile("ptesync; mtsdr1 %0; isync" :: "r"((u_int)pagetable | ffs(PTEG_MASK >> 7)));
	return (0);
}

int
lv1_panic(int val)
{
	mambo_print("lv1_panic\n");
}
