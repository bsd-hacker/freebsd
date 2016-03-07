/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 * $FreeBSD$
 *
 */

#ifndef	__GPIOBUS_H__
#define	__GPIOBUS_H__

#include "opt_platform.h"

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#ifdef FDT
#include <dev/ofw/ofw_bus_subr.h>
#include <gnu/dts/include/dt-bindings/gpio/gpio.h>
#endif

#include "gpio_if.h"

#ifdef FDT
#define	GPIOBUS_IVAR(d) (struct gpiobus_ivar *)				\
	&((struct ofw_gpiobus_devinfo *)device_get_ivars(d))->opd_dinfo
#else
#define	GPIOBUS_IVAR(d) (struct gpiobus_ivar *) device_get_ivars(d)
#endif
#define	GPIOBUS_SOFTC(d) (struct gpiobus_softc *) device_get_softc(d)
#define	GPIOBUS_LOCK(_sc) mtx_lock(&(_sc)->sc_mtx)
#define	GPIOBUS_UNLOCK(_sc) mtx_unlock(&(_sc)->sc_mtx)
#define	GPIOBUS_LOCK_INIT(_sc) mtx_init(&_sc->sc_mtx,			\
	    device_get_nameunit(_sc->sc_dev), "gpiobus", MTX_DEF)
#define	GPIOBUS_LOCK_DESTROY(_sc) mtx_destroy(&_sc->sc_mtx)
#define	GPIOBUS_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED)
#define	GPIOBUS_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED)

#define	GPIOBUS_WAIT		1
#define	GPIOBUS_DONTWAIT	2

struct gpiobus_pin_data
{
	int		mapped;		/* pin is mapped/reserved. */
	char		*name;		/* pin name. */
};

struct gpiobus_softc
{
	struct mtx	sc_mtx;		/* bus mutex */
	struct rman	sc_intr_rman;	/* isr resources */
	device_t	sc_busdev;	/* bus device */
	device_t	sc_owner;	/* bus owner */
	device_t	sc_dev;		/* driver device */
	int		sc_npins;	/* total pins on bus */
	struct gpiobus_pin_data	*sc_pins; /* pin data */
};

struct gpiobus_pin
{
	device_t	dev;	/* gpio device */
	uint32_t	flags;	/* pin flags */
	uint32_t	pin;	/* pin number */
};
typedef struct gpiobus_pin *gpio_pin_t;

struct gpiobus_ivar
{
	struct resource_list	rl;	/* isr resource list */
	uint32_t	npins;	/* pins total */
	uint32_t	*flags;	/* pins flags */
	uint32_t	*pins;	/* pins map */
};

#ifdef FDT
struct ofw_gpiobus_devinfo {
	struct gpiobus_ivar	opd_dinfo;
	struct ofw_bus_devinfo	opd_obdinfo;
};

static __inline int
gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent, int gcells,
    pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	return (GPIO_MAP_GPIOS(bus, dev, gparent, gcells, gpios, pin, flags));
}

device_t ofw_gpiobus_add_fdt_child(device_t, const char *, phandle_t);
int ofw_gpiobus_parse_gpios(device_t, char *, struct gpiobus_pin **);
void ofw_gpiobus_register_provider(device_t);
void ofw_gpiobus_unregister_provider(device_t);

/* Consumers interface. */
int gpio_pin_get_by_ofw_name(device_t consumer, char *name, gpio_pin_t *gpio);
int gpio_pin_get_by_ofw_idx(device_t consumer, int idx, gpio_pin_t *gpio);
int gpio_pin_get_by_ofw_property(device_t consumer, char *name,
    gpio_pin_t *gpio);
void gpio_pin_release(gpio_pin_t gpio);
int gpio_pin_is_active(gpio_pin_t pin, bool *active);
int gpio_pin_set_active(gpio_pin_t pin, bool active);
int gpio_pin_setflags(gpio_pin_t pin, uint32_t flags);
#endif
int gpio_check_flags(uint32_t, uint32_t);
device_t gpiobus_attach_bus(device_t);
int gpiobus_detach_bus(device_t);
int gpiobus_init_softc(device_t);
int gpiobus_alloc_ivars(struct gpiobus_ivar *);
void gpiobus_free_ivars(struct gpiobus_ivar *);
int gpiobus_map_pin(device_t, uint32_t);

extern driver_t gpiobus_driver;

#endif	/* __GPIOBUS_H__ */
