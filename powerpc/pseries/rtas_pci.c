/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/pseries/powerpc/ofw/ofw_real.c 222059 2011-05-18 15:07:36Z nwhitehorn $");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>
#include <machine/rtas.h>

#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"

/*
 * Device interface.
 */
static int		rtaspci_probe(device_t);
static int		rtaspci_attach(device_t);

/*
 * Bus interface.
 */
static int		rtaspci_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct		resource * rtaspci_alloc_resource(device_t bus,
			    device_t child, int type, int *rid, u_long start,
			    u_long end, u_long count, u_int flags);
static int		rtaspci_release_resource(device_t bus, device_t child,
    			    int type, int rid, struct resource *res);
static int		rtaspci_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);
static int		rtaspci_deactivate_resource(device_t bus,
    			    device_t child, int type, int rid,
    			    struct resource *res);


/*
 * pcib interface.
 */
static int		rtaspci_maxslots(device_t);
static u_int32_t	rtaspci_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		rtaspci_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);
static int		rtaspci_route_interrupt(device_t, device_t, int);

/*
 * ofw_bus interface
 */
static phandle_t rtaspci_get_node(device_t bus, device_t dev);

/*
 * local methods
 */

struct ofw_pci_range {
	uint32_t	pci_hi;
	uint32_t	pci_mid;
	uint32_t	pci_lo;
	uint64_t	host;
	uint32_t	size_hi;
	uint32_t	size_lo;
};

static int rtaspci_fill_ranges(phandle_t node, struct ofw_pci_range **ranges,
    int *nranges);

/*
 * Driver methods.
 */
static device_method_t	rtaspci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rtaspci_probe),
	DEVMETHOD(device_attach,	rtaspci_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	rtaspci_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	rtaspci_alloc_resource),
	DEVMETHOD(bus_release_resource,	rtaspci_release_resource),
	DEVMETHOD(bus_activate_resource,	rtaspci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	rtaspci_deactivate_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	rtaspci_maxslots),
	DEVMETHOD(pcib_read_config,	rtaspci_read_config),
	DEVMETHOD(pcib_write_config,	rtaspci_write_config),
	DEVMETHOD(pcib_route_interrupt,	rtaspci_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     rtaspci_get_node),

	{ 0, 0 }
};

struct rtaspci_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	int			sc_bus;

	cell_t			read_pci_config, write_pci_config;
	cell_t			ex_read_pci_config, ex_write_pci_config;
	int			sc_extended_config;
	struct ofw_pci_register	pcir;

	struct ofw_pci_range	*sc_range;
	int			sc_nrange;

	struct rman		sc_io_rman;
	struct rman		sc_mem_rman;
	bus_space_tag_t		sc_memt;
	bus_dma_tag_t		sc_dmat;
	vm_offset_t		sc_iostart;

	struct ofw_bus_iinfo    sc_pci_iinfo;
};

static driver_t	rtaspci_driver = {
	"pcib",
	rtaspci_methods,
	sizeof(struct rtaspci_softc)
};

static devclass_t	rtaspci_devclass;

DRIVER_MODULE(rtaspci, nexus, rtaspci_driver, rtaspci_devclass, 0, 0);

static int
rtaspci_probe(device_t dev)
{
	const char	*type;

	if (!rtas_exists())
		return (ENXIO);

	type = ofw_bus_get_type(dev);

	if (OF_getproplen(ofw_bus_get_node(dev), "used-by-rtas") < 0)
		return (ENXIO);
	if (type == NULL || strcmp(type, "pci") != 0)
		return (ENXIO);

	device_set_desc(dev, "RTAS Host-PCI bridge");
	return (BUS_PROBE_GENERIC);
}

static int
rtaspci_attach(device_t dev)
{
	struct		rtaspci_softc *sc;
	phandle_t	node;
	u_int32_t	busrange[2];
	struct		ofw_pci_range *rp, *io, *mem[5];
	int		nmem, i, error;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	sc->read_pci_config = rtas_token_lookup("read-pci-config");
	sc->write_pci_config = rtas_token_lookup("write-pci-config");
	sc->ex_read_pci_config = rtas_token_lookup("ibm,read-pci-config");
	sc->ex_write_pci_config = rtas_token_lookup("ibm,write-pci-config");
	if (OF_getprop(node, "reg", &sc->pcir, sizeof(sc->pcir)) == -1)
		return (ENXIO);

	sc->sc_extended_config = 0;
	OF_getprop(node, "ibm,pci-config-space-type", &sc->sc_extended_config,
	    sizeof(sc->sc_extended_config));

	if (OF_getprop(node, "bus-range", busrange, sizeof(busrange)) != 8)
		return (ENXIO);

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_bus = busrange[0];

	if (rtaspci_fill_ranges(node, &sc->sc_range, &sc->sc_nrange) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	io = NULL;
	nmem = 0;

	for (rp = sc->sc_range; rp < sc->sc_range + sc->sc_nrange &&
	       rp->pci_hi != 0; rp++) {
		switch (rp->pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_CONFIG:
			break;
		case OFW_PCI_PHYS_HI_SPACE_IO:
			io = rp;
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
			mem[nmem] = rp;
			nmem++;
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM64:
			break;
		}
	}

	if (io == NULL) {
		device_printf(dev, "can't find io range\n");
		return (ENXIO);
	}
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "PCI I/O Ports";
	sc->sc_iostart = io->host;
	if (rman_init(&sc->sc_io_rman) != 0 ||
	    rman_manage_region(&sc->sc_io_rman, io->pci_lo,
	    io->pci_lo + io->size_lo) != 0) {
		panic("rtaspci_attach: failed to set up I/O rman");
	}

	if (nmem == 0) {
		device_printf(dev, "can't find mem ranges\n");
		return (ENXIO);
	}
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "PCI Memory";
	error = rman_init(&sc->sc_mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}
	for (i = 0; i < nmem; i++) {
		error = rman_manage_region(&sc->sc_mem_rman, mem[i]->pci_lo,
		    mem[i]->pci_lo + mem[i]->size_lo);
		if (error) {
			device_printf(dev, 
			    "rman_manage_region() failed. error = %d\n", error);
			return (error);
		}
	}

	ofw_bus_setup_iinfo(node, &sc->sc_pci_iinfo, sizeof(cell_t));

	device_add_child(dev, "pci", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

static int
rtaspci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static uint32_t
rtaspci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct rtaspci_softc *sc;
	uint32_t retval = 0xffffffff;
	uint32_t config_addr;
	int error, pcierror;

	sc = device_get_softc(dev);
	
	config_addr = ((bus & 0xff) << 16) | ((slot & 0x1f) << 11) |
	    ((func & 0x7) << 8) | (reg & 0xff);
	if (sc->sc_extended_config)
		config_addr |= (reg & 0xf00) << 16;
		
	if (sc->ex_read_pci_config != -1)
		error = rtas_call_method(sc->ex_read_pci_config, 4, 2,
		    config_addr, sc->pcir.phys_hi, sc->pcir.phys_mid,
		    width, &pcierror, &retval);
	else
		error = rtas_call_method(sc->read_pci_config, 2, 2,
		    config_addr, width, &pcierror, &retval);

	/* Sign-extend output */
	switch (width) {
	case 1:
		retval = (int32_t)(int8_t)(retval);
		break;
	case 2:
		retval = (int32_t)(int16_t)(retval);
		break;
	}
	
	if (error < 0 || pcierror != 0)
		retval = 0xffffffff;

	return (retval);
}

static void
rtaspci_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int width)
{
	struct rtaspci_softc *sc;
	uint32_t config_addr;
	int pcierror;

	sc = device_get_softc(dev);
	
	config_addr = ((bus & 0xff) << 16) | ((slot & 0x1f) << 11) |
	    ((func & 0x7) << 8) | (reg & 0xff);
	if (sc->sc_extended_config)
		config_addr |= (reg & 0xf00) << 16;
		
	if (sc->ex_write_pci_config != -1)
		rtas_call_method(sc->ex_write_pci_config, 5, 1, config_addr,
		    sc->pcir.phys_hi, sc->pcir.phys_mid, width, val,
		    &pcierror);
	else
		rtas_call_method(sc->write_pci_config, 3, 1, config_addr,
		    width, val, &pcierror);
}

static int
rtaspci_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct rtaspci_softc *sc;
	struct ofw_pci_register reg;
	uint32_t pintr, mintr;
	phandle_t iparent;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bus);
	pintr = pin;
	if (ofw_bus_lookup_imap(ofw_bus_get_node(dev), &sc->sc_pci_iinfo, &reg,
	    sizeof(reg), &pintr, sizeof(pintr), &mintr, sizeof(mintr),
	    &iparent, maskbuf))
		return (MAP_IRQ(iparent, mintr));

	/* Maybe it's a real interrupt, not an intpin */
	if (pin > 4)
		return (pin);

	device_printf(bus, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

static int
rtaspci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	rtaspci_softc *sc;

	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
	}

	return (ENOENT);
}

static struct resource *
rtaspci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct			rtaspci_softc *sc;
	struct			resource *rv;
	struct			rman *rm;
	int			needactivate;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;

	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		break;

	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
		    flags));

	default:
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL) {
		device_printf(bus, "failed to reserve resource for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
			device_printf(bus,
			    "failed to activate resource for %s\n",
			    device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
rtaspci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	if (rman_get_flags(res) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, res);
		if (error)
			return error;
	}

	return (rman_release_resource(res));
}

static int
rtaspci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	struct rtaspci_softc *sc;
	void	*p;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ) {
		return (bus_activate_resource(bus, type, rid, res));
	}
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		vm_offset_t start;

		start = (vm_offset_t)rman_get_start(res);

		/*
		 * Some bridges have I/O ports relative to the start of I/O
		 * space, so adjust if we are under that.
		 */
		if (type == SYS_RES_IOPORT && start < sc->sc_iostart)
			start += sc->sc_iostart;

		if (bootverbose)
			printf("rtaspci mapdev: start %zx, len %ld\n", start,
			    rman_get_size(res));

		p = pmap_mapdev(start, (vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);

		rman_set_virtual(res, p);
		rman_set_bustag(res, &bs_le_tag);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}

static int
rtaspci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		u_int32_t psize;

		psize = rman_get_size(res);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(res), psize);
	}

	return (rman_deactivate_resource(res));
}

static phandle_t
rtaspci_get_node(device_t bus, device_t dev)
{
	struct rtaspci_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */

	return (sc->sc_node);
}

static int
rtaspci_fill_ranges(phandle_t node, struct ofw_pci_range **ranges,
    int *nranges)
{
	int address_cells = 1;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int i, j;

	OF_getprop(OF_parent(node), "#address-cells", &address_cells,
	    sizeof(address_cells));
	if (address_cells > 2)
		panic("RTAS PCI: Addresses too wide (%d)", address_cells);

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges <= 0)
		return (-1);

	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getprop(node, "ranges", base_ranges, nbase_ranges);
	*nranges = nbase_ranges / sizeof(cell_t) / (5 + address_cells);
	*ranges = malloc(*nranges * sizeof(struct ofw_pci_range), M_DEVBUF,
	    M_WAITOK);

	for (i = 0, j = 0; i < *nranges; i++) {
		(*ranges)[i].pci_hi = base_ranges[j++];
		(*ranges)[i].pci_mid = base_ranges[j++];
		(*ranges)[i].pci_lo = base_ranges[j++];
		(*ranges)[i].host = base_ranges[j++];
		if (address_cells == 2) {
			(*ranges)[i].host <<= 32;
			(*ranges)[i].host |= base_ranges[j++];
		}
		(*ranges)[i].size_hi = base_ranges[j++];
		(*ranges)[i].size_lo = base_ranges[j++];
	}

	free(base_ranges, M_DEVBUF);
	return (0);
}

