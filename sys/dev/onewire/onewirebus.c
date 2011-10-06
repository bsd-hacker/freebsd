/*	$OpenBSD: onewire.c,v 1.12 2011/07/03 15:47:16 matthew Exp $	*/

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
 * 1-Wire bus driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#include "owbus_if.h"

#define ONEWIRE_DEBUG
#ifdef ONEWIRE_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define OWBUS_IVAR(d) (struct owbus_ivar *) device_get_ivars(d)
#define OWBUS_SOFTC(d) (struct owbus_softc *) device_get_softc(d)

#define	OWBUS_LOCK(_sc) mtx_lock(&(_sc)->sc_mtx)
#define	OWBUS_UNLOCK(_sc) mtx_unlock(&(_sc)->sc_mtx)
#define	OWBUS_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_busdev), \
	    "owbus", MTX_DEF)
#define	OWBUS_LOCK_DESTROY(_sc) mtx_destroy(&_sc->sc_mtx);
#define	OWBUS_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	OWBUS_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

struct onewire_device
{
	TAILQ_ENTRY(onewire_device)	d_list;
	struct device *			d_dev;	/* may be NULL */
	uint64_t			d_rom;
	int				d_present;
};

struct owbus_softc
{
	struct mtx			sc_mtx;		/* bus mutex */
	device_t			sc_busdev;	/* bus device */
	device_t			sc_owner;	/* bus owner */
	device_t			sc_dev;		/* driver device */
	struct proc			*sc_p;
	uint64_t			sc_rombuf[ONEWIRE_MAXDEVS];
	TAILQ_HEAD(, onewire_device)	sc_devs;
};

static int	owbus_probe(device_t);
static int	owbus_attach(device_t);
static int	owbus_detach(device_t);

static void	owbus_lock_bus(device_t);
static void	owbus_unlock_bus(device_t);
static int	owbus_reset(device_t);
static int	owbus_bit(device_t, int);
static int	owbus_read_byte(device_t);
static void	owbus_write_byte(device_t, int);
static void	owbus_read_block(device_t, void *, int);
static void	owbus_write_block(device_t, const void *, int);
static int	owbus_triplet(device_t, int);
static void	owbus_matchrom(device_t, uint64_t);
static int	owbus_search(device_t, uint64_t *, int, uint64_t);

static void	owbus_thread(void *);
static void	owbus_scan(struct owbus_softc *);

static int
owbus_probe(device_t dev)
{
	device_set_desc(dev, "onewire bus");
	return (0);
}

static int
owbus_attach(device_t dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	sc->sc_busdev = dev;
	sc->sc_dev = device_get_parent(dev);
	OWBUS_LOCK_INIT(sc);
	TAILQ_INIT(&sc->sc_devs);

	kproc_create(&owbus_thread, sc, &sc->sc_p, 0, 0, "%s scan",  device_get_nameunit(dev));

	return (bus_generic_attach(dev));
}

int
owbus_detach(struct device *dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_LOCK_DESTROY(sc);

	return (0);
}

static void
owbus_lock_bus(device_t dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_UNLOCKED(sc);
	OWBUS_LOCK(sc);
}

static void
owbus_unlock_bus(device_t dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	OWBUS_UNLOCK(sc);
}

static int
owbus_reset(device_t dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_RESET(sc->sc_dev));
}

static int
owbus_bit(device_t dev, int value)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_BIT(sc->sc_dev, value));
}

static int
owbus_read_byte(device_t dev)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_READ_BYTE(sc->sc_dev));
}

static void
owbus_write_byte(device_t dev, int val)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_WRITE_BYTE(sc->sc_dev, val));
}

static void
owbus_read_block(device_t dev, void *buf, int len)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_READ_BLOCK(sc->sc_dev, buf, len));
}

static void
owbus_write_block(device_t dev, const void *buf, int len)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_WRITE_BLOCK(sc->sc_dev, buf, len));
}

static int
owbus_triplet(device_t dev, int dir)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_TRIPLET(sc->sc_dev, dir));
}

static void
owbus_matchrom(device_t dev, uint64_t rom)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_LOCKED(sc);
	return (OWBUS_MATCHROM(sc->sc_dev, rom));
}

static int
owbus_search(device_t dev, uint64_t *buf, int size, uint64_t startrom)
{
	struct owbus_softc *sc = OWBUS_SOFTC(dev);

	OWBUS_ASSERT_UNLOCKED(sc);
	return (OWBUS_SEARCH(sc->sc_dev, buf, size, startrom));
}

static void
owbus_thread(void *arg)
{
	struct owbus_softc *sc = arg;

	for (;;) {
		owbus_scan(sc);
		tsleep(sc, PWAIT, "owidle", ONEWIRE_SCANTIME * hz);
	}
}

static void
owbus_scan(struct owbus_softc *sc)
{
	struct onewire_device *d, *next, *nd;
	struct onewire_attach_args oa;
	struct device *dev;
	int present;
	uint64_t rom;
	int i, rv;

	/*
	 * Mark all currently present devices as absent before
	 * scanning. This allows to find out later which devices
	 * have been disappeared.
	 */
	TAILQ_FOREACH(d, &sc->sc_devs, d_list)
		d->d_present = 0;

	/*
	 * Reset the bus. If there's no presence pulse don't search
	 * for any devices.
	 */
	owbus_lock_bus(sc->sc_busdev);
	rv = OWBUS_RESET(sc->sc_dev);
	owbus_unlock_bus(sc->sc_busdev);
	if (rv != 0) {
		DPRINTF(("%s: no presence pulse\n",
			    device_get_nameunit(sc->sc_busdev)));
		goto out;
	}

	/* Scan the bus */
	if ((rv = OWBUS_SEARCH(sc->sc_dev, sc->sc_rombuf, ONEWIRE_MAXDEVS, 0)) == -1)
		return;

	for (i = 0; i < rv; i++) {
		rom = sc->sc_rombuf[i];

		printf("owbus_scan found 0x%llx\n", rom);
		/*
		 * Go through the list of attached devices to see if we
		 * found a new one.
		 */
		present = 0;
		TAILQ_FOREACH(d, &sc->sc_devs, d_list) {
			if (d->d_rom == rom) {
				d->d_present = 1;
				present = 1;
				break;
			}
		}
		if (!present) {
			nd = malloc(sizeof(struct onewire_device),
			    M_DEVBUF, M_ZERO|M_NOWAIT);
			if (nd == NULL)
				continue;

			bzero(&oa, sizeof(oa));
			oa.oa_rom = rom;
			dev = device_add_child(sc->sc_busdev, NULL, -1);
			if (dev == NULL) {
				free(d, M_DEVBUF);
				continue;
			}
			device_set_ivars(dev, &oa);
			mtx_lock(&Giant);
			device_probe_and_attach(dev);
			mtx_unlock(&Giant);
			device_set_ivars(dev, NULL);

			nd->d_dev = dev;
			nd->d_rom = rom;
			nd->d_present = 1;
			TAILQ_INSERT_TAIL(&sc->sc_devs, nd, d_list);
		}
	}

out:
	/* Detach disappeared devices */
	TAILQ_FOREACH_SAFE(d, &sc->sc_devs, d_list, next) {
		if (!d->d_present) {
			mtx_lock(&Giant);
			if (d->d_dev != NULL)
				device_delete_child(sc->sc_busdev, d->d_dev);
			mtx_unlock(&Giant);
			TAILQ_REMOVE(&sc->sc_devs, d, d_list);
			free(d, M_DEVBUF);
		}
	}
}

static device_method_t owbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		owbus_probe),
	DEVMETHOD(device_attach,	owbus_attach),
	DEVMETHOD(device_detach,	owbus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* OWBUS interface */
	DEVMETHOD(owbus_lock_bus,	owbus_lock_bus),
	DEVMETHOD(owbus_unlock_bus,	owbus_unlock_bus),
	DEVMETHOD(owbus_bit,		owbus_bit),
	DEVMETHOD(owbus_reset,		owbus_reset),
	DEVMETHOD(owbus_read_byte,	owbus_read_byte),
	DEVMETHOD(owbus_write_byte,	owbus_write_byte),
	DEVMETHOD(owbus_read_block,	owbus_read_block),
	DEVMETHOD(owbus_write_block,	owbus_write_block),
	DEVMETHOD(owbus_triplet,	owbus_triplet),
	DEVMETHOD(owbus_matchrom,	owbus_matchrom),
	DEVMETHOD(owbus_search,		owbus_search),

	{ 0, 0 }
};

driver_t owbus_driver = {
	"owbus",
	owbus_methods,
	sizeof(struct owbus_softc)
};

devclass_t	owbus_devclass;

DRIVER_MODULE(owbus, onewire, owbus_driver, owbus_devclass, 0, 0);
MODULE_VERSION(owbus, 1);
