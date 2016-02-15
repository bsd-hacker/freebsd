/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@bidouilliste.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/twsi/twsi.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/a10_clk.h>

#include "iicbus_if.h"

#define	TWI_ADDR	0x0
#define	TWI_XADDR	0x4
#define	TWI_DATA	0x8
#define	TWI_CNTR	0xC
#define	TWI_STAT	0x10
#define	TWI_CCR		0x14
#define	TWI_SRST	0x18
#define	TWI_EFR		0x1C
#define	TWI_LCR		0x20

static int
a10_twsi_probe(device_t dev)
{
	struct twsi_softc *sc;

	sc = device_get_softc(dev);
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun4i-a10-i2c"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner Integrated I2C Bus Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
a10_twsi_attach(device_t dev)
{
	struct twsi_softc *sc;

	sc = device_get_softc(dev);

	/* Activate clock */
	a10_clk_i2c_activate(device_get_unit(dev));

	sc->reg_data = TWI_DATA;
	sc->reg_slave_addr = TWI_ADDR;
	sc->reg_slave_ext_addr = TWI_XADDR;
	sc->reg_control = TWI_CNTR;
	sc->reg_status = TWI_STAT;
	sc->reg_baud_rate = TWI_CCR;
	sc->reg_soft_reset = TWI_SRST;

	/* Setup baud rate params */
	sc->baud_rate[IIC_SLOW].param = TWSI_BAUD_RATE_PARAM(11, 2);
	sc->baud_rate[IIC_FAST].param = TWSI_BAUD_RATE_PARAM(11, 2);
	sc->baud_rate[IIC_FASTEST].param = TWSI_BAUD_RATE_PARAM(2, 2);

	return (twsi_attach(dev));
}

static phandle_t
a10_twsi_get_node(device_t bus, device_t dev)
{
	return (ofw_bus_get_node(bus));
}

static device_method_t a10_twsi_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		a10_twsi_probe),
	DEVMETHOD(device_attach,	a10_twsi_attach),

	/* OFW methods */
	DEVMETHOD(ofw_bus_get_node,	a10_twsi_get_node),

	{ 0, 0 }
};

DEFINE_CLASS_1(iichb, a10_twsi_driver, a10_twsi_methods,
    sizeof(struct twsi_softc), twsi_driver);

static devclass_t a10_twsi_devclass;

DRIVER_MODULE(a10_twsi, simplebus, a10_twsi_driver, a10_twsi_devclass, 0, 0);
DRIVER_MODULE(iicbus, a10_twsi, iicbus_driver, iicbus_devclass, 0, 0);
MODULE_DEPEND(a10_twsi, iicbus, 1, 1, 1);
