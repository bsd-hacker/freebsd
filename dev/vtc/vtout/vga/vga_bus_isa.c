/*-
 * Copyright (c) 2002-2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <isa/isareg.h>
#include <isa/isavar.h>

#include <dev/vtc/vtc_con.h>

#include <dev/ic/vga.h>
#include <dev/vtc/vtout/vga/vga.h>

static int vga_isa_attach(device_t);
static int vga_isa_probe(device_t);

static device_method_t vga_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vga_isa_probe),
	DEVMETHOD(device_attach,	vga_isa_attach),
	{ 0, 0 }
};

static driver_t vga_isa_driver = {
	vga_device_name,
	vga_isa_methods,
	sizeof(struct vga_softc)
};

static int
vga_isa_attach(device_t dev)
{

	return (ENXIO);
}

static int
vga_isa_probe(device_t dev)
{

	/*
	 * We can get called even when there's a PCI VGA device. It happens
	 * when an accelerated driver for the hardware exists that preempts
	 * the VGA driver. We check for this and bail out right away.
	 */
	if (vga_console.vga_conout->vtc_busdev != NULL)
		return (ENXIO);

	printf("ISA VGA is being probed!\n");
	return (ENXIO);
}

DRIVER_MODULE(vga, isa, vga_isa_driver, vga_devclass, 0, 0);
