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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/vtc/vtc_con.h>

#include <dev/ic/vga.h>
#include <dev/vtc/vtout/vga/vga.h>

static int vga_pci_attach(device_t dev);
static int vga_pci_probe(device_t dev);

static device_method_t vga_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vga_pci_probe),
	DEVMETHOD(device_attach,	vga_pci_attach),
	{ 0, 0 }
};

static driver_t vga_pci_driver = {
	vga_device_name,
	vga_pci_methods,
	sizeof(struct vga_softc)
};

static int
vga_pci_alloc(device_t dev, struct vga_softc *sc, int type, int rid)
{
	struct resource *res;

	res = bus_alloc_resource(dev, type, &rid, 0, ~0, 1, RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);

	sc->vga_spc[rid].bsh = rman_get_bushandle(res);
	sc->vga_spc[rid].bst = rman_get_bustag(res);
	return (0);
}

static int
vga_pci_attach(device_t dev)
{
	struct vga_softc *sc;
	int error;

	/*
	 * If VGA is the console, this device must be it.  If not, then
	 * we're just using an otherwise unused static softc.
	 */
	sc = &vga_console;
	device_set_softc(dev, sc);

	sc->vga_dev = dev;
	sc->vga_bustype = VGA_BUSTYPE_PCI;

	/* Set the legacy resources */
	bus_set_resource(dev, SYS_RES_MEMORY, VGA_RES_FB, VGA_MEM_BASE,
	    VGA_MEM_SIZE);
	error = vga_pci_alloc(dev, sc, SYS_RES_MEMORY, VGA_RES_FB);
	if (error)
		return (error);
	bus_set_resource(dev, SYS_RES_IOPORT, VGA_RES_REG, VGA_REG_BASE,
	    VGA_REG_SIZE);
	error = vga_pci_alloc(dev, sc, SYS_RES_IOPORT, VGA_RES_REG);
	if (error)
		return (error);

	return (vga_attach(dev));
}

static int
vga_pci_probe(device_t dev)
{
	uint32_t cfg;

	switch (pci_get_class(dev)) {
	case PCIC_OLD:
		if (pci_get_subclass(dev) != PCIS_OLD_VGA)
			return (ENXIO);
		device_set_desc(dev, "Generic (vintage) PCI VGA");
		break;
	case PCIC_DISPLAY:
		if (pci_get_subclass(dev) != PCIS_DISPLAY_VGA)
			return (ENXIO);
		device_set_desc(dev, "Generic PCI VGA");
		break;
	default:
		return (ENXIO);
	}

	/*
	 * Only one VGA device can use the legacy I/O and memory resources.
	 * It's that device that is the default display and thus the device
	 * we can attach to.
	 */
	cfg = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((cfg & PCIM_CMD_PORTEN) == 0)
		return (ENXIO);

	/*
	 * Handle preemption in case we're the VTC console. We typically
	 * don't get to attach when there's an accelerated driver for the
	 * hardware. The problem is that we like to continue to use the
	 * hardware for the low-level console until we're ready to switch
	 * to the virtual terminal, at which time the accelerated driver
	 * can take control. We handshake by letting VTC know what the
	 * device_t for the hardware is so that accelerated drivers can
	 * check whether they're attaching to the low-level console device
	 * or not and take appropriate action.
	 */
	if (vga_console.vga_console)
		vga_console.vga_conout->vtc_busdev = dev;

	return (BUS_PROBE_LOW_PRIORITY);
}

DRIVER_MODULE(vga, pci, vga_pci_driver, vga_devclass, 0, 0);
