/*-
 * Copyright 2003 by Peter Grehan. All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <machine/openpicreg.h>
#include <machine/openpicvar.h>

#include "pic_if.h"

/*
 * MacIO interface
 */
static int	openpic_macio_probe(device_t);
static int	openpic_macio_attach(device_t);
static int	openpic_macio_suspend(device_t);
static int	openpic_macio_resume(device_t);

static device_method_t  openpic_macio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		openpic_macio_probe),
	DEVMETHOD(device_attach,	openpic_macio_attach),
	DEVMETHOD(device_suspend,	openpic_macio_suspend),
	DEVMETHOD(device_resume,	openpic_macio_resume),

	/* PIC interface */
	DEVMETHOD(pic_bind,		openpic_bind),
	DEVMETHOD(pic_config,		openpic_config),
	DEVMETHOD(pic_dispatch,		openpic_dispatch),
	DEVMETHOD(pic_enable,		openpic_enable),
	DEVMETHOD(pic_eoi,		openpic_eoi),
	DEVMETHOD(pic_ipi,		openpic_ipi),
	DEVMETHOD(pic_mask,		openpic_mask),
	DEVMETHOD(pic_unmask,		openpic_unmask),

	{ 0, 0 },
};

static driver_t openpic_macio_driver = {
	"openpic",
	openpic_macio_methods,
	sizeof(struct openpic_softc),
};

DRIVER_MODULE(openpic, macio, openpic_macio_driver, openpic_devclass, 0, 0);

static int
openpic_macio_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "open-pic") != 0)
                return (ENXIO);

	/* On some U4 systems, there is a phantom MPIC in the mac-io cell */
	if (OF_finddevice("/u4") != (phandle_t)-1)
		return (ENXIO);

	device_set_desc(dev, OPENPIC_DEVSTR);
	return (0);
}

static int
openpic_macio_attach(device_t dev)
{
 
	return (openpic_common_attach(dev, ofw_bus_get_node(dev)));
}

static int
openpic_macio_suspend(device_t dev)
{
	struct openpic_softc *sc;
	int i;

	sc = device_get_softc(dev);

	sc->sc_saved_config = bus_read_4(sc->sc_memr, OPENPIC_CONFIG);
	for (i = 0; i < 4; i++) {
		sc->sc_saved_ipis[i] = bus_read_4(sc->sc_memr, OPENPIC_IPI_VECTOR(i));
	}

	for (i = 0; i < 4; i++) {
		sc->sc_saved_prios[i] = bus_read_4(sc->sc_memr, OPENPIC_PCPU_TPR(i));
	}

	for (i = 0; i < OPENPIC_TIMERS; i++) {
		sc->sc_saved_timers[i].tcnt = bus_read_4(sc->sc_memr, OPENPIC_TCNT(i));
		sc->sc_saved_timers[i].tbase = bus_read_4(sc->sc_memr, OPENPIC_TBASE(i));
		sc->sc_saved_timers[i].tvec = bus_read_4(sc->sc_memr, OPENPIC_TVEC(i));
		sc->sc_saved_timers[i].tdst = bus_read_4(sc->sc_memr, OPENPIC_TDST(i));
	}

	for (i = 0; i < OPENPIC_SRC_VECTOR_COUNT; i++)
		sc->sc_saved_vectors[i] =
		    bus_read_4(sc->sc_memr, OPENPIC_SRC_VECTOR(i)) & ~OPENPIC_ACTIVITY;

	return (0);
}

static int
openpic_macio_resume(device_t dev)
{
    	struct openpic_softc *sc;
    	int i;

    	sc = device_get_softc(dev);

	sc->sc_saved_config = bus_read_4(sc->sc_memr, OPENPIC_CONFIG);
	for (i = 0; i < 4; i++) {
		bus_write_4(sc->sc_memr, OPENPIC_IPI_VECTOR(i), sc->sc_saved_ipis[i]);
	}

	for (i = 0; i < 4; i++) {
		bus_write_4(sc->sc_memr, OPENPIC_PCPU_TPR(i), sc->sc_saved_prios[i]);
	}

	for (i = 0; i < OPENPIC_TIMERS; i++) {
		bus_write_4(sc->sc_memr, OPENPIC_TCNT(i), sc->sc_saved_timers[i].tcnt);
		bus_write_4(sc->sc_memr, OPENPIC_TBASE(i), sc->sc_saved_timers[i].tbase);
		bus_write_4(sc->sc_memr, OPENPIC_TVEC(i), sc->sc_saved_timers[i].tvec);
		bus_write_4(sc->sc_memr, OPENPIC_TDST(i), sc->sc_saved_timers[i].tdst);
	}

	for (i = 0; i < OPENPIC_SRC_VECTOR_COUNT; i++)
		bus_write_4(sc->sc_memr, OPENPIC_SRC_VECTOR(i), sc->sc_saved_vectors[i]);

	return (0);
}
