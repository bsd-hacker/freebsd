/*-
 * Copyright (c) 2004-2010 Juli Mallett <jmallett@FreeBSD.org>
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/hwfunc.h>
#include <machine/smp.h>

#include <mips/cavium/octeon_pcmap_regs.h>

unsigned octeon_ap_boot = ~0;

void
platform_ipi_send(int cpuid)
{
	panic("%s: not yet implemented.", __func__);
}

void
platform_ipi_clear(void)
{
	panic("%s: not yet implemented.", __func__);
}

int
platform_ipi_intrnum(void)
{
	return (1);
}

void
platform_init_ap(int cpuid)
{
	/*
	 * Set the exception base.
	 *
	 * XXX Low bits seem to be used for cpuid?
	 */
	mips_wr_prid1(0x80000000);
}

int
platform_num_processors(void)
{
	return (fls(octeon_core_mask));
}

int
platform_start_ap(int cpuid)
{
	if (atomic_cmpset_32(&octeon_ap_boot, ~0, cpuid) == 0)
		return (-1);
	for (;;) {
		DELAY(1000);
		if (atomic_cmpset_32(&octeon_ap_boot, 0, ~0) != 0)
			return (0);
		printf("Waiting for cpu%d to start\n", cpuid);
	}
}
