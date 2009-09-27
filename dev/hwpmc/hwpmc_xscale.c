/*-
 * Copyright (c) 2009 Rui Paulo <rpaulo@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Support for the Intel XScale network processors */

#include <sys/param.h>
#include <sys/pmc.h>

#include <machine/pmc_mdep.h>

struct xscale_event_code_map {
	enum pmc_event	pe_ev;
	uint8_t		pe_code;
	uint8_t		pe_mask;
};


static int
xscale_read_pmc(int cpu, int ri, pmc_value_t *v)
{
}

static int
xscale_write_pmc(int cpu, int ri, pmc_value_t v)
{
}

static int
xscale_config_pmc(int cpu, int ri, struct pmc *pm)
{
}

static int
xscale_start_pmc(int cpu, int ri)
{
}

static int
xscale_stop_pmc(int cpu, int ri)
{
}

static int
xscale_intr(int cpu, struct trapframe *tf)
{
}

static int
xscale_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
}

struct pmc_mdep *
pmc_xscale_initialize()
{
	return NULL;
}

void
pmc_xscale_finalize(struct pmc_mdep *md)
{
	(void) md;
}
