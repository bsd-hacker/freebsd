/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/at91/kb920x_machdep.c, rev 45
 */

#include "opt_ddb.h"
#include "opt_platform.h"
#include "opt_global.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/cons.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kdb.h>
#include <sys/msgbuf.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>

#include <arm/ti/omap4/omap4_reg.h>

#define  DEBUG
#ifdef  DEBUG
#define debugf(fmt, args...) printf(fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

extern unsigned char kernbase[];
extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char _end[];

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

extern int *end;

const struct pmap_devmap *pmap_devmap_bootstrap_table;

static struct mem_region availmem_regions[FDT_MEM_REGIONS];
static int availmem_regions_sz;

static void print_kenv(void);
static void print_kernel_section_addr(void);

void (*ti_cpu_reset)(void);

struct pmap_devmap arm_pmap_devmap[] = {
	DEVMAP_FDT("console-uart", VM_PROT_READ | VM_PROT_WRITE, PTE_DEVICE),
	DEVMAP_END,
};

static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

static void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (uint32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

static void
print_kernel_section_addr(void)
{

	debugf("kernel image addresses:\n");
	debugf(" kernbase       = 0x%08x\n", (uint32_t)kernbase);
	debugf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	debugf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	debugf(" __bss_start    = 0x%08x\n", (uint32_t)__bss_start);
	debugf(" _end           = 0x%08x\n", (uint32_t)_end);
}

void *
initarm(void *mdp, void *unused __unused)
{
	vm_offset_t dtbp, lastaddr;
	uint32_t memsize;
	void *kmdp;
	void *sp;

	kmdp = NULL;
	lastaddr = 0;
	memsize = 0;
	dtbp = (vm_offset_t)NULL;

	set_cpufuncs();

	/*
	 * Mask metadata pointer: it is supposed to be on page boundary. If
	 * the first argument (mdp) doesn't point to a valid address the
	 * bootloader must have passed us something else than the metadata
	 * ptr... In this case we want to fall back to some built-in settings.
	 */
	mdp = (void *)((uint32_t)mdp & ~PAGE_MASK);

	/* Parse metadata and fetch parameters */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
			lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND,
			    vm_offset_t);
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
#endif
		}

		preload_addr_relocate = KERNVIRTADDR - KERNPHYSADDR;
	} else {
		/* Fall back to hardcoded metadata. */
		lastaddr = fake_preload_metadata();
	}

#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == (vm_offset_t)NULL)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#endif

	if (OF_install(OFW_FDT, 0) == FALSE)
		while (1);

	if (OF_init((void *)dtbp) != 0)
		while (1);

	/* Grab physical memory regions information from device tree. */
	if (fdt_get_mem_regions(availmem_regions, &availmem_regions_sz,
	    &memsize) != 0)
		while(1);

	/* Platform-specific initialisation */
	pcpu0_init();

	sp = arm_mmu_init(memsize, lastaddr, true);

	/*
	 * Only after the SOC registers block is mapped we can perform device
	 * tree fixups, as they may attempt to read parameters from hardware.
	 */
	OF_interpret("perform-fixup", 0);

	cninit();

	physmem = memsize / PAGE_SIZE;

	debugf("initarm: console initialized\n");
	debugf(" arg1 mdp = 0x%08x\n", (uint32_t)mdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);
	print_kernel_section_addr();
	print_kenv();

	mutex_init();

	/*
	 * Prepare map of physical memory regions available to vm subsystem.
	 */
	physmap_init(availmem_regions, availmem_regions_sz);

	/* Do basic tuning, hz etc */
	init_param2(physmem);
	kdb_init();

	return (sp);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}

void
cpu_reset()
{
	if (ti_cpu_reset)
		(*ti_cpu_reset)();
	else
		printf("no cpu_reset implementation\n");
	printf("Reset failed!\n");
	while (1);
}

