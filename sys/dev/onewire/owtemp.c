/*	$OpenBSD: owtemp.c,v 1.15 2010/07/08 07:19:54 jasper Exp $	*/

/*
 * Copyright (c) 2006, 2009 Alexander Yurchenko <grange@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * 1-Wire temperature family type device driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/onewire/onewiredevs.h>
#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#define OWTEMP_DEBUG
#ifdef OWTEMP_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif
/* Commands */

#define DS1920_CMD_CONVERT		0x44
#define DS1920_CMD_READ_SCRATCHPAD	0xbe

/* Scratchpad layout */
#define DS1920_SP_TEMP_LSB		0
#define DS1920_SP_TEMP_MSB		1
#define DS1920_SP_TH			2
#define DS1920_SP_TL			3
#define DS18B20_SP_CONFIG		4
#define DS1920_SP_COUNT_REMAIN		6
#define DS1920_SP_COUNT_PERC		7
#define DS1920_SP_CRC			8

struct owtemp_softc {
	device_t		sc_dev;
	device_t		sc_onewire;
	u_int64_t		sc_rom;
	int			sc_value;
	int			sc_lastupd;
};

static int	owtemp_probe(device_t);
static int	owtemp_attach(device_t);
static int	owtemp_sysctl_temp(SYSCTL_HANDLER_ARGS);
static void	owtemp_update(struct owtemp_softc *);

static const struct onewire_matchfam owtemp_fams[] = {
	{ ONEWIRE_FAMILY_DS1920, "Maxim DS1920" },
	{ ONEWIRE_FAMILY_DS18B20, "Maxim DS18B20" },
	{ ONEWIRE_FAMILY_DS1822, "Maxim DS1822" }
};

#define N(a)	(sizeof(a) / sizeof(a[0]))
static int
owtemp_probe(device_t dev)
{
	struct onewire_attach_args *oaa = device_get_ivars(dev);
	const struct onewire_matchfam *match;

	match = onewire_matchbyfam(oaa, owtemp_fams, N(owtemp_fams));
	if (match) {
		device_set_desc(dev, match->om_desc);
		return (BUS_PROBE_GENERIC);
	}
	return (ENXIO);
}
#undef N

static int
owtemp_attach(device_t dev)
{
	struct owtemp_softc *sc = device_get_softc(dev);
	struct onewire_attach_args *oaa = device_get_ivars(dev);

	sc->sc_dev = dev;
	sc->sc_onewire = device_get_parent(dev);
	sc->sc_rom = oaa->oa_rom;

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "temp",
	    CTLTYPE_INT | CTLFLAG_RD, sc, 0, owtemp_sysctl_temp, "I",
	    "temp");

	return (0);
}

static int
owtemp_sysctl_temp(SYSCTL_HANDLER_ARGS)
{
	struct owtemp_softc *sc = arg1;

	owtemp_update(sc);

	return SYSCTL_OUT(req, &sc->sc_value, sizeof sc->sc_value);
}

static void
owtemp_update(struct owtemp_softc *sc)
{
	uint8_t data[9];
	int16_t temp;
	int count_perc, count_remain, val;

	if (ticks < sc->sc_lastupd + (hz * 3))
		return;

	onewire_lock(sc->sc_onewire);
	if (onewire_reset(sc->sc_onewire) != 0) {
		DPRINTF(("%s: bus reset failed\n", __func__));
		goto done;
	}
	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * Start temperature conversion. The conversion takes up to 750ms.
	 * After sending the command, the data line must be held high for
	 * at least 750ms to provide power during the conversion process.
	 * As such, no other activity may take place on the 1-Wire bus for
	 * at least this period.
	 */
	onewire_write_byte(sc->sc_onewire, DS1920_CMD_CONVERT);
	tsleep(sc, PRIBIO, "owtemp", hz);

	if (onewire_reset(sc->sc_onewire) != 0) {
		DPRINTF(("%s: bus reset after convet failed\n", __func__));
		goto done;
	}
	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * The result of the temperature measurement is placed in the
	 * first two bytes of the scratchpad.
	 */
	onewire_write_byte(sc->sc_onewire, DS1920_CMD_READ_SCRATCHPAD);
	onewire_read_block(sc->sc_onewire, data, 9);
	DPRINTF(("%s: data %*D\n", __func__, 9, (char *)data, ","));
	if (onewire_crc(data, 8) == data[DS1920_SP_CRC]) {
		temp = data[DS1920_SP_TEMP_MSB] << 8 |
		    data[DS1920_SP_TEMP_LSB];
		DPRINTF(("%s: raw temp %d\n", __func__, temp));
		if (ONEWIRE_ROM_FAMILY(sc->sc_rom) == ONEWIRE_FAMILY_DS18B20 ||
		    ONEWIRE_ROM_FAMILY(sc->sc_rom) == ONEWIRE_FAMILY_DS1822) {
			/*
			 * DS18B20 decoding
			 * default 12 bit 0.0625 C resolution
			 */
			val = temp * (1000000 / 16);
		} else {
			/* DS1920 decoding */
			count_perc = data[DS1920_SP_COUNT_PERC];
			count_remain = data[DS1920_SP_COUNT_REMAIN];

			if (count_perc != 0) {
				/* High resolution algorithm */
				temp &= ~0x0001;
				val = temp * 500000 - 250000 +
				    ((count_perc - count_remain) * 1000000) /
				    count_perc;
			} else {
				val = temp * 500000;
			}
		}
		sc->sc_value = val;
		sc->sc_lastupd = ticks;
		DPRINTF(("temp = %d\n", val));
	} else {
		DPRINTF(("%s: crc failed\n", __func__));
	}

done:
	onewire_unlock(sc->sc_onewire);
}

static device_method_t owtemp_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		owtemp_probe),
	DEVMETHOD(device_attach,	owtemp_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	{ 0, 0 }
};

static driver_t owtemp_driver = {
	"owtemp",
	owtemp_methods,
	sizeof(struct owtemp_softc),
};

static devclass_t owtemp_devclass;

DRIVER_MODULE(owtemp, owbus, owtemp_driver, owtemp_devclass, 0, 0);

MODULE_DEPEND(owtemp, owbus, 1, 1, 1);
MODULE_VERSION(owtemp, 1);
