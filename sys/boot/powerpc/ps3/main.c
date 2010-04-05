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
#include <sys/param.h>

#define _KERNEL
#include <machine/cpufunc.h>

#include "bootstrap.h"
#include "lv1call.h"
#include "ps3.h"

struct arch_switch	archsw;
extern void *_end;

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

int ps3_getdev(void **vdev, const char *devspec, const char **path);

static uint64_t basetb;

int
main(void)
{
	uint64_t maxmem = 0;
	void *heapbase;
	int i;

	lv1_get_physmem(&maxmem);
	
	ps3mmu_init(maxmem);

	/*
	 * Set up console.
	 */
	cons_probe();

	/*
	 * Set the heap to one page after the end of the loader.
	 */
	heapbase = (void *)((((u_long)&_end) + PAGE_SIZE) & ~PAGE_MASK);
	setheap(heapbase, heapbase + 0x80000);

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	/*
	 * Get timebase at boot.
	 */
	basetb = mftb();

	archsw.arch_getdev = ps3_getdev;

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("Memory: %lldKB\n", maxmem / 1024);

	env_setenv("currdev", EV_VOLATILE, "net", NULL, NULL);
	env_setenv("loaddev", EV_VOLATILE, "net", NULL, NULL);

	interact();			/* doesn't return */

	return (0);
}

const u_int ns_per_tick = 12;

void
exit(int code)
{
}

void
delay(int usecs)
{
	uint64_t tb,ttb;
	tb = mftb();

	ttb = tb + (usecs * 1000 + ns_per_tick - 1) / ns_per_tick;
	while (tb < ttb)
		tb = mftb();
}

int
getsecs()
{
	return ((mftb() - basetb)*ns_per_tick/1000000000);
}

time_t
time(time_t *tloc)
{
	return (0);
}

