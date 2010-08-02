/*-
 * Copyright 2010 Nathan Whitehorn
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: user/nwhitehorn/ps3/powerpc/powermac/ps3pic.c 207422 2010-04-30 05:44:54Z nwhitehorn $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/platform.h>

#include "ps3-hvcall.h"
#include "pic_if.h"

static void	ps3pic_identify(driver_t *driver, device_t parent);
static int	ps3pic_probe(device_t);
static int	ps3pic_attach(device_t);

static void	ps3pic_dispatch(device_t, struct trapframe *);
static void	ps3pic_enable(device_t, u_int, u_int);
static void	ps3pic_eoi(device_t, u_int);
static void	ps3pic_ipi(device_t, u_int);
static void	ps3pic_mask(device_t, u_int);
static void	ps3pic_unmask(device_t, u_int);
static uint32_t ps3pic_id(device_t dev);

static device_method_t  ps3pic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ps3pic_identify),
	DEVMETHOD(device_probe,		ps3pic_probe),
	DEVMETHOD(device_attach,	ps3pic_attach),

	/* PIC interface */
	DEVMETHOD(pic_dispatch,		ps3pic_dispatch),
	DEVMETHOD(pic_enable,		ps3pic_enable),
	DEVMETHOD(pic_eoi,		ps3pic_eoi),
	DEVMETHOD(pic_id,		ps3pic_id),
	DEVMETHOD(pic_ipi,		ps3pic_ipi),
	DEVMETHOD(pic_mask,		ps3pic_mask),
	DEVMETHOD(pic_unmask,		ps3pic_unmask),

	{ 0, 0 },
};

static driver_t ps3pic_driver = {
	"ps3pic",
	ps3pic_methods,
	0
};

static devclass_t ps3pic_devclass;

DRIVER_MODULE(ps3pic, nexus, ps3pic_driver, ps3pic_devclass, 0, 0);

static void
ps3pic_identify(driver_t *driver, device_t parent)
{
	if (strcmp(installed_platform(), "ps3") != 0)
		return;

	if (device_find_child(parent, "ps3pic", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "ps3pic", 0);
}

static int
ps3pic_probe(device_t dev)
{
	device_set_desc(dev, "Playstation 3 interrupt controller");
	return (BUS_PROBE_NOWILDCARD);
}

static int
ps3pic_attach(device_t dev)
{
	powerpc_register_pic(dev, 64);
	root_pic = dev; /* PS3s have only one PIC */

	return (0);
}

/*
 * PIC I/F methods.
 */

static void
ps3pic_dispatch(device_t dev, struct trapframe *tf)
{
}

static void
ps3pic_enable(device_t dev, u_int irq, u_int vector)
{
}

static void
ps3pic_eoi(device_t dev __unused, u_int irq __unused)
{
}

static void
ps3pic_ipi(device_t dev, u_int irq)
{
}

static void
ps3pic_mask(device_t dev, u_int irq)
{
}

static void
ps3pic_unmask(device_t dev, u_int irq)
{
}

static uint32_t
ps3pic_id(device_t dev)
{
	return (0);
}

