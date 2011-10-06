/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2010 Luiz Otavio O Souza
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <sys/gpio.h>
#include "gpiobus_if.h"

#include <dev/onewire/onewirevar.h>
#include "owbus_if.h"
#include "owbb_if.h"

#define	ONEWIRE_PIN		0	/* Only one pin */

struct gpio1w_softc
{
	device_t	sc_dev;
	device_t	sc_busdev;
};

static int gpio1w_probe(device_t);
static int gpio1w_attach(device_t);

/* owbb interface */
static void gpio1w_rx(device_t);
static void gpio1w_tx(device_t);
static int gpio1w_get(device_t);
static void gpio1w_set(device_t, int);

static int
gpio1w_probe(device_t dev)
{

	device_set_desc(dev, "GPIO 1wire bit-banging driver");
	return (0);
}

static int
gpio1w_attach(device_t dev)
{
	struct gpio1w_softc	*sc = device_get_softc(dev);
	device_t		bitbang;

	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);

	/* add generic bit-banging code */
	bitbang = device_add_child(dev, "owbb", -1);
	device_probe_and_attach(bitbang);

	return (0);
}

/*
 * Reset bus by setting SDA first and then SCL. 
 * Must always be called with gpio bus locked.
 */
static void
gpio1w_rx(device_t dev)
{
	struct gpio1w_softc *sc = device_get_softc(dev);

	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, ONEWIRE_PIN,
	    GPIO_PIN_INPUT);
}

static void
gpio1w_tx(device_t dev)
{
	struct gpio1w_softc *sc = device_get_softc(dev);

	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, ONEWIRE_PIN,
	    GPIO_PIN_OUTPUT);
}

static int
gpio1w_get(device_t dev)
{
	struct gpio1w_softc *sc = device_get_softc(dev);
	unsigned int val;

	GPIOBUS_LOCK_BUS(sc->sc_busdev);
	GPIOBUS_PIN_GET(sc->sc_busdev, sc->sc_dev, ONEWIRE_PIN, &val);
	GPIOBUS_UNLOCK_BUS(sc->sc_busdev);

	return (val);
}

static void
gpio1w_set(device_t dev, int value)
{
	struct gpio1w_softc *sc = device_get_softc(dev);

	GPIOBUS_LOCK_BUS(sc->sc_busdev);
	GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, ONEWIRE_PIN, value ? 1:0);
	GPIOBUS_UNLOCK_BUS(sc->sc_busdev);
}

static devclass_t gpio1w_devclass;

static device_method_t gpio1w_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpio1w_probe),
	DEVMETHOD(device_attach,	gpio1w_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* owbb interface */
	DEVMETHOD(owbb_rx,		gpio1w_rx),
	DEVMETHOD(owbb_tx,		gpio1w_tx),
	DEVMETHOD(owbb_get,		gpio1w_get),
	DEVMETHOD(owbb_set,		gpio1w_set),

	{ 0, 0 }
};

static driver_t gpio1w_driver = {
	"gpio1w",
	gpio1w_methods,
	sizeof(struct gpio1w_softc),
};

DRIVER_MODULE(gpio1w, gpiobus, gpio1w_driver, gpio1w_devclass, 0, 0);
DRIVER_MODULE(owbb, gpio1w, owbb_driver, owbb_devclass, 0, 0);
MODULE_DEPEND(gpio1w, owbb, 1, 1, 1);
MODULE_DEPEND(gpio1w, gpiobus, 1, 1, 1);
