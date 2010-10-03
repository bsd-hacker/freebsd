/*-
 * Copyright (c) 2001-2003, Shunsuke Akiyama <akiyama@FreeBSD.org>.
 * Copyright (c) 1997, 1998, 1999, 2000 Bill Paul <wpaul@ee.columbia.edu>.
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
/*-
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * RealTek RTL8150 USB to fast ethernet controller driver.
 * Datasheet is available from
 * ftp://ftp.realtek.com.tw/lancard/data_sheet/8150/.
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/ethernet.h>

#include "miibus_if.h"

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR rue_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_sleepout.h>

#include <dev/usb/net/if_ruereg.h>

#ifdef USB_DEBUG
static int rue_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, rue, CTLFLAG_RW, 0, "USB rue");
SYSCTL_INT(_hw_usb_rue, OID_AUTO, debug, CTLFLAG_RW,
    &rue_debug, 0, "Debug level");
#endif

/*
 * Various supported device vendors/products.
 */

static const struct usb_device_id rue_devs[] = {
	{USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUAKTX, 0)},
	{USB_VPI(USB_VENDOR_REALTEK, USB_PRODUCT_REALTEK_USBKR100, 0)},
	{USB_VPI(USB_VENDOR_OQO, USB_PRODUCT_OQO_ETHER01, 0)},
};

/* prototypes */

static device_probe_t rue_probe;
static device_attach_t rue_attach;
static device_detach_t rue_detach;

static miibus_readreg_t rue_miibus_readreg;
static miibus_writereg_t rue_miibus_writereg;
static miibus_statchg_t rue_miibus_statchg;

static usb_callback_t rue_intr_callback;
static usb_callback_t rue_bulk_read_callback;
static usb_callback_t rue_bulk_write_callback;

static int	rue_read_mem(struct rue_softc *, uint16_t, void *, int);
static int	rue_write_mem(struct rue_softc *, uint16_t, void *, int);
static uint8_t	rue_csr_read_1(struct rue_softc *, uint16_t);
static uint16_t	rue_csr_read_2(struct rue_softc *, uint16_t);
static int	rue_csr_write_1(struct rue_softc *, uint16_t, uint8_t);
static int	rue_csr_write_2(struct rue_softc *, uint16_t, uint16_t);
static int	rue_csr_write_4(struct rue_softc *, int, uint32_t);
static void	rue_reset(struct rue_softc *);
static int	rue_ifmedia_upd(struct ifnet *);
static void	rue_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static void	rue_init(void *);
static void	rue_init_locked(struct rue_softc *);
static int	rue_ioctl(struct ifnet *, u_long, caddr_t);
static void	rue_start(struct ifnet *);
static void	rue_start_locked(struct ifnet *);
static void	rue_stop(struct rue_softc *);
static void	rue_watchdog(void *);

static const struct usb_config rue_config[RUE_N_TRANSFER] = {
	[RUE_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MCLBYTES,
		.flags = USBD_PIPE_BOF | USBD_FORCE_SHORT_XFER,
		.callback = rue_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},

	[RUE_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = (MCLBYTES + 4),
		.flags = USBD_PIPE_BOF | USBD_SHORT_XFER_OK,
		.callback = rue_bulk_read_callback,
		.timeout = 0,	/* no timeout */
	},

	[RUE_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = USBD_PIPE_BOF | USBD_SHORT_XFER_OK,
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = rue_intr_callback,
	},
};

static device_method_t rue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, rue_probe),
	DEVMETHOD(device_attach, rue_attach),
	DEVMETHOD(device_detach, rue_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, rue_miibus_readreg),
	DEVMETHOD(miibus_writereg, rue_miibus_writereg),
	DEVMETHOD(miibus_statchg, rue_miibus_statchg),

	{0, 0}
};

static driver_t rue_driver = {
	.name = "rue",
	.methods = rue_methods,
	.size = sizeof(struct rue_softc),
};

static devclass_t rue_devclass;

DRIVER_MODULE(rue, uhub, rue_driver, rue_devclass, NULL, 0);
DRIVER_MODULE(miibus, rue, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(rue, usb, 1, 1, 1);
MODULE_DEPEND(rue, ether, 1, 1, 1);
MODULE_DEPEND(rue, miibus, 1, 1, 1);
MODULE_VERSION(rue, 1);

#define	RUE_SETBIT(sc, reg, x) \
	rue_csr_write_1(sc, reg, rue_csr_read_1(sc, reg) | (x))

#define	RUE_CLRBIT(sc, reg, x) \
	rue_csr_write_1(sc, reg, rue_csr_read_1(sc, reg) & ~(x))

static int
rue_read_mem(struct rue_softc *sc, uint16_t addr, void *buf, int len)
{
	struct usb_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf,
	    0, NULL, 1000));
}

static int
rue_write_mem(struct rue_softc *sc, uint16_t addr, void *buf, int len)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf,
	    0, NULL, 1000));
}

static uint8_t
rue_csr_read_1(struct rue_softc *sc, uint16_t reg)
{
	uint8_t val;

	rue_read_mem(sc, reg, &val, 1);
	return (val);
}

static uint16_t
rue_csr_read_2(struct rue_softc *sc, uint16_t reg)
{
	uint8_t val[2];

	rue_read_mem(sc, reg, &val, 2);
	return (UGETW(val));
}

static int
rue_csr_write_1(struct rue_softc *sc, uint16_t reg, uint8_t val)
{
	return (rue_write_mem(sc, reg, &val, 1));
}

static int
rue_csr_write_2(struct rue_softc *sc, uint16_t reg, uint16_t val)
{
	uint8_t temp[2];

	USETW(temp, val);
	return (rue_write_mem(sc, reg, &temp, 2));
}

static int
rue_csr_write_4(struct rue_softc *sc, int reg, uint32_t val)
{
	uint8_t temp[4];

	USETDW(temp, val);
	return (rue_write_mem(sc, reg, &temp, 4));
}

static int
rue_miibus_readreg(device_t dev, int phy, int reg)
{
	struct rue_softc *sc = device_get_softc(dev);
	uint16_t rval;
	uint16_t ruereg;
	int locked;

	if (phy != 0)		/* RTL8150 supports PHY == 0, only */
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		RUE_LOCK(sc);

	switch (reg) {
	case MII_BMCR:
		ruereg = RUE_BMCR;
		break;
	case MII_BMSR:
		ruereg = RUE_BMSR;
		break;
	case MII_ANAR:
		ruereg = RUE_ANAR;
		break;
	case MII_ANER:
		ruereg = RUE_AER;
		break;
	case MII_ANLPAR:
		ruereg = RUE_ANLP;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		rval = 0;
		goto done;
	default:
		if (RUE_REG_MIN <= reg && reg <= RUE_REG_MAX) {
			rval = rue_csr_read_1(sc, reg);
			goto done;
		}
		device_printf(sc->sc_dev, "bad phy register\n");
		rval = 0;
		goto done;
	}

	rval = rue_csr_read_2(sc, ruereg);
done:
	if (!locked)
		RUE_UNLOCK(sc);
	return (rval);
}

static int
rue_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct rue_softc *sc = device_get_softc(dev);
	uint16_t ruereg;
	int locked;

	if (phy != 0)		/* RTL8150 supports PHY == 0, only */
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		RUE_LOCK(sc);

	switch (reg) {
	case MII_BMCR:
		ruereg = RUE_BMCR;
		break;
	case MII_BMSR:
		ruereg = RUE_BMSR;
		break;
	case MII_ANAR:
		ruereg = RUE_ANAR;
		break;
	case MII_ANER:
		ruereg = RUE_AER;
		break;
	case MII_ANLPAR:
		ruereg = RUE_ANLP;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		goto done;
	default:
		if (RUE_REG_MIN <= reg && reg <= RUE_REG_MAX) {
			rue_csr_write_1(sc, reg, data);
			goto done;
		}
		device_printf(sc->sc_dev, " bad phy register\n");
		goto done;
	}
	rue_csr_write_2(sc, ruereg, data);
done:
	if (!locked)
		RUE_UNLOCK(sc);
	return (0);
}

static void
rue_miibus_statchg(device_t dev)
{

	/*
	 * When the code below is enabled the card starts doing weird
	 * things after link going from UP to DOWN and back UP.
	 *
	 * Looks like some of register writes below messes up PHY
	 * interface.
	 *
	 * No visible regressions were found after commenting this code
	 * out, so that disable it for good.
	 */
#if 0
	struct rue_softc *sc = device_get_softc(dev);
	struct mii_data *mii = GET_MII(sc);
	uint16_t bmcr;
	int locked;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		RUE_LOCK(sc);

	RUE_CLRBIT(sc, RUE_CR, (RUE_CR_RE | RUE_CR_TE));

	bmcr = rue_csr_read_2(sc, RUE_BMCR);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX)
		bmcr |= RUE_BMCR_SPD_SET;
	else
		bmcr &= ~RUE_BMCR_SPD_SET;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		bmcr |= RUE_BMCR_DUPLEX;
	else
		bmcr &= ~RUE_BMCR_DUPLEX;

	rue_csr_write_2(sc, RUE_BMCR, bmcr);

	RUE_SETBIT(sc, RUE_CR, (RUE_CR_RE | RUE_CR_TE));

	if (!locked)
		RUE_UNLOCK(sc);
#endif
}

static void
rue_setpromisc(struct rue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		RUE_SETBIT(sc, RUE_RCR, RUE_RCR_AAP);
	else
		RUE_CLRBIT(sc, RUE_RCR, RUE_RCR_AAP);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void
rue_setmulti(void *arg, int npending)
{
	struct rue_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint16_t rxcfg;
	int h = 0;
	uint32_t hashes[2] = { 0, 0 };
	struct ifmultiaddr *ifma;
	int mcnt = 0;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	rxcfg = rue_csr_read_2(sc, RUE_RCR);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxcfg |= (RUE_RCR_AAM | RUE_RCR_AAP);
		rxcfg &= ~RUE_RCR_AM;
		rue_csr_write_2(sc, RUE_RCR, rxcfg);
		rue_csr_write_4(sc, RUE_MAR0, 0xFFFFFFFF);
		rue_csr_write_4(sc, RUE_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	rue_csr_write_4(sc, RUE_MAR0, 0);
	rue_csr_write_4(sc, RUE_MAR4, 0);

	/* now program new ones */
	if_maddr_rlock(ifp);
	TAILQ_FOREACH (ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}
	if_maddr_runlock(ifp);

	if (mcnt)
		rxcfg |= RUE_RCR_AM;
	else
		rxcfg &= ~RUE_RCR_AM;

	rxcfg &= ~(RUE_RCR_AAM | RUE_RCR_AAP);

	rue_csr_write_2(sc, RUE_RCR, rxcfg);
	rue_csr_write_4(sc, RUE_MAR0, hashes[0]);
	rue_csr_write_4(sc, RUE_MAR4, hashes[1]);
}

static void
rue_reset(struct rue_softc *sc)
{
	int i;

	rue_csr_write_1(sc, RUE_CR, RUE_CR_SOFT_RST);

	for (i = 0; i != RUE_TIMEOUT; i++) {
		if (!(rue_csr_read_1(sc, RUE_CR) & RUE_CR_SOFT_RST))
			break;
		usb_pause_mtx(&sc->sc_mtx, hz / 1000);
	}
	if (i == RUE_TIMEOUT)
		device_printf(sc->sc_dev, "reset never completed\n");

	usb_pause_mtx(&sc->sc_mtx, hz / 100);
}

static void
rue_attach_post(struct rue_softc *sc)
{

	/* reset the adapter */
	rue_reset(sc);

	/* get station address from the EEPROM */
	rue_read_mem(sc, RUE_EEPROM_IDR0, sc->sc_eaddr, ETHER_ADDR_LEN);
}

/*
 * Probe for a RTL8150 chip.
 */
static int
rue_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != RUE_CONFIG_IDX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != RUE_IFACE_IDX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(rue_devs, sizeof(rue_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
rue_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct rue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	uint8_t iface_index;
	int error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	sleepout_create(&sc->sc_sleepout, "rue sleepout");
	sleepout_init_mtx(&sc->sc_sleepout, &sc->sc_watchdog, &sc->sc_mtx, 0);
	TASK_INIT(&sc->sc_setmulti, 0, rue_setmulti, sc);

	iface_index = RUE_IFACE_IDX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, rue_config, RUE_N_TRANSFER,
	    sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed\n");
		goto detach;
	}

	RUE_LOCK(sc);
	rue_attach_post(sc);
	RUE_UNLOCK(sc);

	sc->sc_ifp = ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "could not allocate ifnet\n");
		goto detach;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rue_ioctl;
	ifp->if_start = rue_start;
	ifp->if_init = rue_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
	    rue_ifmedia_upd, rue_ifmedia_sts);
	if (error) {
		device_printf(sc->sc_dev, "MII without any PHY\n");
		goto detach;
	}

	if_printf(ifp, "<USB Ethernet> on %s\n",
	    device_get_nameunit(sc->sc_dev));
	ether_ifattach(ifp, sc->sc_eaddr);
	return (0);
detach:
	rue_detach(dev);
	return (ENXIO);			/* failure */
}

static int
rue_detach(device_t dev)
{
	struct rue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	sleepout_drain(&sc->sc_watchdog);
	taskqueue_drain(sc->sc_sleepout.s_taskqueue, &sc->sc_setmulti);
	usbd_transfer_unsetup(sc->sc_xfer, RUE_N_TRANSFER);
	if (sc->sc_miibus != NULL)
		device_delete_child(sc->sc_dev, sc->sc_miibus);
	if (ifp != NULL) {
		RUE_LOCK(sc);
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		RUE_UNLOCK(sc);
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	sleepout_free(&sc->sc_sleepout);
	mtx_destroy(&sc->sc_mtx);
	return (0);
}

static void
rue_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct rue_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct rue_intrpkt pkt;
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (ifp && (ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    actlen >= sizeof(pkt)) {
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, &pkt, sizeof(pkt));

			ifp->if_ierrors += pkt.rue_rxlost_cnt;
			ifp->if_ierrors += pkt.rue_crcerr_cnt;
			ifp->if_collisions += pkt.rue_col_cnt;
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		return;
	default:			/* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
rue_rxflush(struct rue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	for (;;) {
		_IF_DEQUEUE(&sc->sc_rxq, m);
		if (m == NULL)
			break;

		/*
		 * The USB xfer has been resubmitted so its safe to unlock now.
		 */
		RUE_UNLOCK(sc);
		ifp->if_input(ifp, m);
		RUE_LOCK(sc);
	}
}

static struct mbuf *
rue_newbuf(void)
{
	struct mbuf *m_new;

	m_new = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m_new == NULL)
		return (NULL);
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	m_adj(m_new, ETHER_ALIGN);
	return (m_new);
}

static int
rue_rxbuf(struct rue_softc *sc, struct usb_page_cache *pc, 
    unsigned int offset, unsigned int len)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	if (len < ETHER_HDR_LEN || len > MCLBYTES - ETHER_ALIGN)
		return (1);

	m = rue_newbuf();
	if (m == NULL) {
		ifp->if_ierrors++;
		return (ENOMEM);
	}

	usbd_copy_out(pc, offset, mtod(m, uint8_t *), len);

	/* finalize mbuf */
	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len;

	/* enqueue for later when the lock can be released */
	_IF_ENQUEUE(&sc->sc_rxq, m);
	return (0);
}

static void
rue_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct rue_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	uint16_t status;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (actlen < 4) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, actlen - 4, &status, sizeof(status));
		actlen -= 4;

		/* check recieve packet was valid or not */
		status = le16toh(status);
		if ((status & RUE_RXSTAT_VALID) == 0) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		rue_rxbuf(sc, pc, 0, actlen);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		rue_rxflush(sc);
		return;
	default:			/* Error */
		DPRINTF("bulk read error, %s\n",
		    usbd_errstr(error));

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
rue_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct rue_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	struct mbuf *m;
	int temp_len;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");
		ifp->if_opackets++;
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if ((sc->sc_flags & RUE_FLAG_LINK) == 0) {
			/*
			 * don't send anything if there is no link !
			 */
			return;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			return;
		if (m->m_pkthdr.len > MCLBYTES)
			m->m_pkthdr.len = MCLBYTES;
		temp_len = m->m_pkthdr.len;

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_m_copy_in(pc, 0, m, 0, m->m_pkthdr.len);

		/*
		 * This is an undocumented behavior.
		 * RTL8150 chip doesn't send frame length smaller than
		 * RUE_MIN_FRAMELEN (60) byte packet.
		 */
		if (temp_len < RUE_MIN_FRAMELEN) {
			usbd_frame_zero(pc, temp_len,
			    RUE_MIN_FRAMELEN - temp_len);
			temp_len = RUE_MIN_FRAMELEN;
		}
		usbd_xfer_set_frame_len(xfer, 0, temp_len);

		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
		 */
		BPF_MTAP(ifp, m);

		m_freem(m);

		usbd_transfer_submit(xfer);
		return;
	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usbd_errstr(error));

		ifp->if_oerrors++;

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		return;
	}
}

static void
rue_tick(struct rue_softc *sc)
{
	struct mii_data *mii = GET_MII(sc);

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
	if ((sc->sc_flags & RUE_FLAG_LINK) == 0
	    && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sc_flags |= RUE_FLAG_LINK;
		rue_start_locked(sc->sc_ifp);
	}
}

static void
rue_start(struct ifnet *ifp)
{
	struct rue_softc *sc = ifp->if_softc;

	RUE_LOCK(sc);
	rue_start_locked(ifp);
	RUE_UNLOCK(sc);
}

static void
rue_start_locked(struct ifnet *ifp)
{
	struct rue_softc *sc = ifp->if_softc;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * start the USB transfers, if not already started:
	 */
	usbd_transfer_start(sc->sc_xfer[RUE_INTR_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[RUE_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[RUE_BULK_DT_WR]);
}

static void
rue_init(void *arg)
{
	struct rue_softc *sc = arg;

	RUE_LOCK(sc);
	rue_init_locked(sc);
	RUE_UNLOCK(sc);
}

static void
rue_init_locked(struct rue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Cancel pending I/O
	 */
	rue_reset(sc);

	/* Set MAC address */
	rue_write_mem(sc, RUE_IDR0, IF_LLADDR(ifp), ETHER_ADDR_LEN);

	rue_stop(sc);

	/*
	 * Set the initial TX and RX configuration.
	 */
	rue_csr_write_1(sc, RUE_TCR, RUE_TCR_CONFIG);
	rue_csr_write_2(sc, RUE_RCR, RUE_RCR_CONFIG|RUE_RCR_AB);

	/* Load the multicast filter */
	rue_setpromisc(sc);
	/* Load the multicast filter. */
	rue_setmulti(sc, 0);

	/* Enable RX and TX */
	rue_csr_write_1(sc, RUE_CR, (RUE_CR_TE | RUE_CR_RE | RUE_CR_EP3CLREN));

	usbd_xfer_set_stall(sc->sc_xfer[RUE_BULK_DT_WR]);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sleepout_reset(&sc->sc_watchdog, hz, rue_watchdog, sc);
	rue_start_locked(sc->sc_ifp);
}

/*
 * Set media options.
 */
static int
rue_ifmedia_upd(struct ifnet *ifp)
{
	struct rue_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	RUE_LOCK_ASSERT(sc, MA_OWNED);

        sc->sc_flags &= ~RUE_FLAG_LINK;
	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);
	return (0);
}

/*
 * Report current media status.
 */
static void
rue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct rue_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	RUE_LOCK(sc);
	mii_pollstat(mii);
	RUE_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
rue_stop(struct rue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	RUE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->sc_flags &= ~RUE_FLAG_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[RUE_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[RUE_BULK_DT_RD]);
	usbd_transfer_stop(sc->sc_xfer[RUE_INTR_DT_RD]);

	rue_csr_write_1(sc, RUE_CR, 0x00);

	rue_reset(sc);
}

static int
rue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct rue_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii = GET_MII(sc);
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		RUE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				rue_setpromisc(sc);
			else
				rue_init_locked(sc);
		} else
			rue_stop(sc);
		RUE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_UP &&
		    ifp->if_drv_flags & IFF_DRV_RUNNING)
			taskqueue_enqueue(sc->sc_sleepout.s_taskqueue,
			    &sc->sc_setmulti);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
rue_watchdog(void *arg)
{
	struct rue_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	rue_tick(sc);
	sleepout_reset(&sc->sc_watchdog, hz, rue_watchdog, sc);
}
