/*-
 * Copyright 2009 by Nathan Whitehorn. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: projects/ppc64/sys/powerpc/powermac/celliic.c 183882 2008-10-14 14:54:14Z nwhitehorn $
 */

/*
 * A driver for the Integrated Interrupt Controller found on all Cell
 * processors.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pic_if.h"

static int	celliic_probe(device_t);
static int	celliic_attach(device_t);

static void	celliic_dispatch(device_t, struct trapframe *);
static void	celliic_enable(device_t, u_int, u_int);
static void	celliic_eoi(device_t, u_int);
static void	celliic_ipi(device_t, u_int);
static void	celliic_mask(device_t, u_int);
static void	celliic_unmask(device_t, u_int);

static device_method_t  celliic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         celliic_probe),
	DEVMETHOD(device_attach,        celliic_attach),

	/* PIC interface */
	DEVMETHOD(pic_dispatch,		celliic_dispatch),
	DEVMETHOD(pic_enable,		celliic_enable),
	DEVMETHOD(pic_eoi,		celliic_eoi),
	DEVMETHOD(pic_ipi,		celliic_ipi),
	DEVMETHOD(pic_mask,		celliic_mask),
	DEVMETHOD(pic_unmask,		celliic_unmask),

	{ 0, 0 },
};

static driver_t celliic_driver = {
	"celliic",
	celliic_methods,
	0
};

static devclass_t celliic_devclass;

DRIVER_MODULE(celliic, nexus, celliic_driver, celliic_devclass, 0, 0);

static int
celliic_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (type == NULL || strcmp(type, "stidc-iic") != 0)
		return (ENXIO);

	device_set_desc(dev, "Cell Integrated Interrupt Controller");
	return (0);
}

static int
celliic_attach(device_t dev)
{
	powerpc_register_pic(dev, 64);
	return (0);
}

/*
 * PIC I/F methods.
 */

static void
celliic_dispatch(device_t dev, struct trapframe *tf)
{
}

static void
celliic_enable(device_t dev, u_int irq, u_int vector)
{
}

static void
celliic_eoi(device_t dev __unused, u_int irq __unused)
{
}

static void
celliic_ipi(device_t dev, u_int irq)
{
}

static void
celliic_mask(device_t dev, u_int irq)
{
}

static void
celliic_unmask(device_t dev, u_int irq)
{
}
