/*-
 * Copyright (c) 2005 Marcel Moolenaar
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

#include <dev/vtc/vtout/gmch/gmch.h>

#define	PCI_VENDOR_INTEL	0x8086
#define	PCI_DEVICE_I815_DRAM	0x1130
#define	PCI_DEVICE_I815_GUI	0x1132
#define	PCI_DEVICE_I830M	0x3577

static device_t gmch_dram_controller;

static int gmch_pci_attach(device_t dev);
static int gmch_pci_probe(device_t dev);

static device_method_t gmch_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gmch_pci_probe),
	DEVMETHOD(device_attach,	gmch_pci_attach),
	{ 0, 0 }
};

static driver_t gmch_pci_driver = {
	gmch_device_name,
	gmch_pci_methods,
	sizeof(struct gmch_softc)
};

static int
gmch_pci_attach(device_t dev)
{
	struct gmch_softc *sc;
	struct resource *res;
	int rid;

	sc = device_get_softc(dev);

	rid = 0x10;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);
	sc->gmch_fb_bsh = rman_get_bushandle(res);
	sc->gmch_fb_bst = rman_get_bustag(res);

	rid = 0x14;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);
	sc->gmch_reg_bsh = rman_get_bushandle(res);
	sc->gmch_reg_bst = rman_get_bustag(res);

	return (gmch_attach(dev));
}

static int
gmch_pci_probe(device_t dev)
{

	if (pci_get_vendor(dev) != PCI_VENDOR_INTEL)
		return (ENXIO);

	/*
	 * Save the device_t of the DRAM controller of the i815 chipset.
	 * We need it in case we do get to attach to the GUI accel.
	 */
	if (pci_get_device(dev) == PCI_DEVICE_I815_DRAM) {
		gmch_dram_controller = dev;
		return (ENXIO);
	}

	if (pci_get_class(dev) != PCIC_DISPLAY)
		return (ENXIO);

	switch (pci_get_device(dev)) {
	case PCI_DEVICE_I815_GUI:
		device_set_desc(dev, "82815/EM/EP/P Internal GUI Accelerator");
		break;
	case PCI_DEVICE_I830M:
		device_set_desc(dev, "82830M/MG Integrated Graphics Device");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

DRIVER_MODULE(gmch, pci, gmch_pci_driver, gmch_devclass, 0, 0);
