/*	$OpenBSD: onewire_bitbang.c,v 1.2 2010/07/02 03:13:42 tedu Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
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
 * 1-Wire bus bit-banging routines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#include "owbb_if.h"
#include "owbus_if.h"

struct owbb_softc {
	device_t owbus;
};

static int	owbb_probe(device_t);
static int	owbb_attach(device_t);
static int	owbb_detach(device_t);

static int	owbb_reset(device_t);
static int	owbb_bit(device_t, int);
static int	owbb_read_byte(device_t);
static void	owbb_write_byte(device_t, int);
static void	owbb_read_block(device_t, void *, int);
static void	owbb_write_block(device_t, const void *, int);
static int	owbb_triplet(device_t, int);
static void	owbb_matchrom(device_t, uint64_t);
static int	owbb_search(device_t, uint64_t *, int, uint64_t);

static int
owbb_probe(device_t dev)
{
	device_set_desc(dev, "1wire bit-banging driver");

	return (0);
}

static int
owbb_attach(device_t dev)
{
	struct owbb_softc *sc = (struct owbb_softc *)device_get_softc(dev);

	sc->owbus = device_add_child(dev, "owbus", -1);
	if (!sc->owbus)
		return (ENXIO);
	bus_generic_attach(dev);

	return (0);
}

static int
owbb_detach(device_t dev)
{
	struct owbb_softc *sc = (struct owbb_softc *)device_get_softc(dev);
	device_t child;

	/*
	 * We need to save child because the detach indirectly causes
	 * sc->owbus to be zeroed.  Since we added the device
	 * unconditionally in owbb_attach, we need to make sure we
	 * delete it here.  See owbb_child_detached.  We need that
	 * callback in case newbus detached our children w/o detaching
	 * us (say owbus is a module and unloaded w/o owbb being
	 * unloaded).
	 */
	child = sc->owbus;
	bus_generic_detach(dev);
	if (child)
		device_delete_child(dev, child);

	return (0);
}

static void
owbb_child_detached( device_t dev, device_t child )
{
	struct owbb_softc *sc = (struct owbb_softc *)device_get_softc(dev);

	if (child == sc->owbus)
		sc->owbus = NULL;
}

static int
owbb_reset(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	int rv, i;

	OWBB_TX(pdev);
	OWBB_SET(pdev, 0);
	DELAY(480);
	OWBB_SET(pdev, 1);
	OWBB_RX(pdev);
	DELAY(30);
	for (i = 0; i < 6; i++) {
		if ((rv = OWBB_GET(pdev)) == 0)
			break;
		DELAY(20);
	}
	DELAY(450);

	return (rv);
}

static int
owbb_bit(device_t dev, int value)
{
	device_t pdev = device_get_parent(dev);
	int rv, i;

	OWBB_TX(pdev);
	OWBB_SET(pdev, 0);
	DELAY(2);
	rv = 0;
	if (value) {
		OWBB_SET(pdev, 1);
		OWBB_RX(pdev);
		for (i = 0; i < 15; i++) {
			if ((rv = OWBB_GET(pdev)) == 0)
				break;
			DELAY(2);
		}
		OWBB_TX(pdev);
	}
	DELAY(60);
	OWBB_SET(pdev, 1);
	DELAY(5);

	return (rv);
}

static int
owbb_read_byte(device_t dev)
{
	uint8_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value |= (owbb_bit(dev, 1) << i);

	return (value);
}

static void
owbb_write_byte(device_t dev, int value)
{
	int i;

	for (i = 0; i < 8; i++)
		owbb_bit(dev, (value >> i) & 0x1);
}

static void
owbb_read_block(device_t dev, void *buf, int len)
{
	uint8_t *p = buf;

	while (len--)
		*p++ = owbb_read_byte(dev);
}

static void
owbb_write_block(device_t dev, const void *buf, int len)
{
	const uint8_t *p = buf;

	while (len--)
		owbb_write_byte(dev, *p++);
}

static int
owbb_triplet(device_t dev, int dir)
{
	int rv;

	rv = owbb_bit(dev, 1);
	rv <<= 1;
	rv |= owbb_bit(dev, 1);

	switch (rv) {
	case 0x0:
		owbb_bit(dev, dir);
		break;
	case 0x1:
		owbb_bit(dev, 0);
		break;
	default:
		owbb_bit(dev, 1);
	}

	return (rv);
}

static void
owbb_matchrom(device_t dev, uint64_t rom)
{
	int i;

	owbb_write_byte(dev, ONEWIRE_CMD_MATCH_ROM);
	for (i = 0; i < 8; i++)
		owbb_write_byte(dev, (rom >> (i * 8)) & 0xff);
}

static int
owbb_search(device_t dev, uint64_t *buf, int size, uint64_t startrom)
{
	int search = 1, count = 0, lastd = -1, dir, rv, i, i0;
	uint64_t mask, rom = startrom, lastrom;
	uint8_t data[8];

	while (search && count < size) {
		/* XXX: yield processor */
		tsleep(dev, PWAIT, "owscan", hz / 10);

		/*
		 * Start new search. Go through the previous path to
		 * the point we made a decision last time and make an
		 * opposite decision. If we didn't make any decision
		 * stop searching.
		 */
		lastrom = rom;
		rom = 0;
		OWBUS_LOCK_BUS(dev);
		owbb_reset(dev);
		owbb_write_byte(dev, ONEWIRE_CMD_SEARCH_ROM);
		for (i = 0, i0 = -1; i < 64; i++) {
			dir = (lastrom >> i) & 0x1;
			if (i == lastd)
				dir = 1;
			else if (i > lastd)
				dir = 0;
			rv = owbb_triplet(dev, dir);
			switch (rv) {
			case 0x0:
				if (i != lastd && dir == 0)
					i0 = i;
				mask = dir;
				break;
			case 0x1:
				mask = 0;
				break;
			case 0x2:
				mask = 1;
				break;
			default:
				printf("%s: search triplet error 0x%x, "
				    "step %d\n",
				    device_get_nameunit(dev), rv, i);
				OWBUS_UNLOCK_BUS(dev);
				return (-1);
			}
			rom |= (mask << i);
		}
		OWBUS_UNLOCK_BUS(dev);

		if ((lastd = i0) == -1)
			search = 0;

		if (rom == 0)
			continue;

		/*
		 * The last byte of the ROM code contains a CRC calculated
		 * from the first 7 bytes. Re-calculate it to make sure
		 * we found a valid device.
		 */
		for (i = 0; i < 8; i++)
			data[i] = (rom >> (i * 8)) & 0xff;
		if (onewire_crc(data, 7) != data[7])
			continue;

		buf[count++] = rom;
	}

	return (count);
}
static device_method_t owbb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		owbb_probe),
	DEVMETHOD(device_attach,	owbb_attach),
	DEVMETHOD(device_detach,	owbb_detach),

	/* bus interface */
	DEVMETHOD(bus_child_detached,	owbb_child_detached),

	/* ONEWIRE protocol */
	DEVMETHOD(owbus_bit,		owbb_bit),
	DEVMETHOD(owbus_reset,		owbb_reset),
	DEVMETHOD(owbus_read_byte,	owbb_read_byte),
	DEVMETHOD(owbus_write_byte,	owbb_write_byte),
	DEVMETHOD(owbus_read_block,	owbb_read_block),
	DEVMETHOD(owbus_write_block,	owbb_write_block),
	DEVMETHOD(owbus_triplet,	owbb_triplet),
	DEVMETHOD(owbus_matchrom,	owbb_matchrom),
	DEVMETHOD(owbus_search,		owbb_search),


	{ 0, 0 }
};

driver_t owbb_driver = {
	"owbb",
	owbb_methods,
	sizeof(struct owbb_softc),
};

devclass_t owbb_devclass;

DRIVER_MODULE(owbus, owbb, owbus_driver, owbus_devclass, 0, 0);

MODULE_DEPEND(owbb, owbus, 1, 1, 1);
MODULE_VERSION(owbb, 1);
