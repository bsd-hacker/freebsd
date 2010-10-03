/*	$NetBSD: if_udav.c,v 1.2 2003/09/04 15:17:38 tsutsui Exp $	*/
/*	$nabe: if_udav.c,v 1.3 2003/08/21 16:57:19 nabe Exp $	*/
/*	$FreeBSD$	*/
/*-
 * Copyright (c) 2003
 *     Shingo WATANABE <nabe@nabechan.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

/*
 * DM9601(DAVICOM USB to Ethernet MAC Controller with Integrated 10/100 PHY)
 * The spec can be found at the following url.
 *   http://www.davicom.com.tw/big5/download/Data%20Sheet/DM9601-DS-P01-930914.pdf
 */

/*
 * TODO:
 *	Interrupt Endpoint support
 *	External PHYs
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#define	USB_DEBUG_VAR udav_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_sleepout.h>

#include <dev/usb/net/if_udavreg.h>

/* prototypes */

static device_probe_t udav_probe;
static device_attach_t udav_attach;
static device_detach_t udav_detach;

static usb_callback_t udav_bulk_write_callback;
static usb_callback_t udav_bulk_read_callback;
static usb_callback_t udav_intr_callback;

static int	udav_csr_read(struct udav_softc *, uint16_t, void *, int);
static int	udav_csr_write(struct udav_softc *, uint16_t, void *, int);
static uint8_t	udav_csr_read1(struct udav_softc *, uint16_t);
static int	udav_csr_write1(struct udav_softc *, uint16_t, uint8_t);
static void	udav_reset(struct udav_softc *);
static int	udav_ifmedia_upd(struct ifnet *);
static void	udav_ifmedia_status(struct ifnet *, struct ifmediareq *);
static void	udav_init(void *);
static void	udav_init_locked(struct udav_softc *);
static int	udav_ioctl(struct ifnet *, u_long, caddr_t);
static void	udav_start(struct ifnet *);
static void	udav_start_locked(struct ifnet *);
static void	udav_setmulti(void *, int);
static void	udav_stop(struct udav_softc *);
static void	udav_setpromisc(struct udav_softc *);
static void	udav_watchdog(void *);

static miibus_readreg_t udav_miibus_readreg;
static miibus_writereg_t udav_miibus_writereg;
static miibus_statchg_t udav_miibus_statchg;

static const struct usb_config udav_config[UDAV_N_TRANSFER] = {
	[UDAV_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = (MCLBYTES + 2),
		.flags = USBD_PIPE_BOF | USBD_FORCE_SHORT_XFER,
		.callback = udav_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},
	[UDAV_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = (MCLBYTES + 3),
		.flags = USBD_PIPE_BOF | USBD_SHORT_XFER_OK,
		.callback = udav_bulk_read_callback,
		.timeout = 0,	/* no timeout */
	},
	[UDAV_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = USBD_PIPE_BOF | USBD_SHORT_XFER_OK,
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = udav_intr_callback,
	},
};

static device_method_t udav_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, udav_probe),
	DEVMETHOD(device_attach, udav_attach),
	DEVMETHOD(device_detach, udav_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, udav_miibus_readreg),
	DEVMETHOD(miibus_writereg, udav_miibus_writereg),
	DEVMETHOD(miibus_statchg, udav_miibus_statchg),

	{0, 0}
};

static driver_t udav_driver = {
	.name = "udav",
	.methods = udav_methods,
	.size = sizeof(struct udav_softc),
};

static devclass_t udav_devclass;

DRIVER_MODULE(udav, uhub, udav_driver, udav_devclass, NULL, 0);
DRIVER_MODULE(miibus, udav, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(udav, usb, 1, 1, 1);
MODULE_DEPEND(udav, ether, 1, 1, 1);
MODULE_DEPEND(udav, miibus, 1, 1, 1);
MODULE_VERSION(udav, 1);

#ifdef USB_DEBUG
static int udav_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, udav, CTLFLAG_RW, 0, "USB udav");
SYSCTL_INT(_hw_usb_udav, OID_AUTO, debug, CTLFLAG_RW, &udav_debug, 0,
    "Debug level");
#endif

#define	UDAV_SETBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) | (x))

#define	UDAV_CLRBIT(sc, reg, x)	\
	udav_csr_write1(sc, reg, udav_csr_read1(sc, reg) & ~(x))

static const struct usb_device_id udav_devs[] = {
	/* ShanTou DM9601 USB NIC */
	{USB_VPI(USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_DM9601, 0)},
	/* ShanTou ST268 USB NIC */
	{USB_VPI(USB_VENDOR_SHANTOU, USB_PRODUCT_SHANTOU_ST268, 0)},
	/* Corega USB-TXC */
	{USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TXC, 0)},
};

static void
udav_attach_post(struct udav_softc *sc)
{

	/* reset the adapter */
	udav_reset(sc);

	/* Get Ethernet Address */
	udav_csr_read(sc, UDAV_PAR, sc->sc_eaddr, ETHER_ADDR_LEN);
}

static int
udav_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != UDAV_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != UDAV_IFACE_INDEX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(udav_devs, sizeof(udav_devs), uaa));
}

static int
udav_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct udav_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	uint8_t iface_index;
	int error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	sleepout_create(&sc->sc_sleepout, "axe sleepout");
	sleepout_init_mtx(&sc->sc_sleepout, &sc->sc_watchdog, &sc->sc_mtx, 0);
	TASK_INIT(&sc->sc_setmulti, 0, udav_setmulti, sc);

	iface_index = UDAV_IFACE_INDEX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, udav_config, UDAV_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed\n");
		goto detach;
	}

	UDAV_LOCK(sc);
	udav_attach_post(sc);
	UDAV_UNLOCK(sc);

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
	ifp->if_ioctl = udav_ioctl;
	ifp->if_start = udav_start;
	ifp->if_init = udav_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus,
	    udav_ifmedia_upd, udav_ifmedia_status);
	if (error) {
		device_printf(sc->sc_dev, "MII without any PHY\n");
		goto detach;
	}

	if_printf(ifp, "<USB Ethernet> on %s\n",
	    device_get_nameunit(sc->sc_dev));
	ether_ifattach(ifp, sc->sc_eaddr);
	return (0);
detach:
	udav_detach(dev);
	return (ENXIO);			/* failure */
}

static int
udav_detach(device_t dev)
{
	struct udav_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	sleepout_drain(&sc->sc_watchdog);
	taskqueue_drain(sc->sc_sleepout.s_taskqueue, &sc->sc_setmulti);
	usbd_transfer_unsetup(sc->sc_xfer, UDAV_N_TRANSFER);

	if (sc->sc_miibus != NULL)
		device_delete_child(sc->sc_dev, sc->sc_miibus);
	if (ifp != NULL) {
		UDAV_LOCK(sc);
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		UDAV_UNLOCK(sc);
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	sleepout_free(&sc->sc_sleepout);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

#if 0
static int
udav_mem_read(struct udav_softc *sc, uint16_t offset, void *buf,
    int len)
{
	struct usb_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	return (uether_do_request(&sc->sc_ue, &req, buf, 1000));
}

static int
udav_mem_write(struct udav_softc *sc, uint16_t offset, void *buf,
    int len)
{
	struct usb_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	return (uether_do_request(&sc->sc_ue, &req, buf, 1000));
}

static int
udav_mem_write1(struct udav_softc *sc, uint16_t offset,
    uint8_t ch)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_MEM_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	return (uether_do_request(&sc->sc_ue, &req, NULL, 1000));
}
#endif

static int
udav_csr_read(struct udav_softc *sc, uint16_t offset, void *buf, int len)
{
	struct usb_device_request req;

	len &= 0xff;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_READ;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf, 0,
	    NULL, 1000));
}

static int
udav_csr_write(struct udav_softc *sc, uint16_t offset, void *buf, int len)
{
	struct usb_device_request req;

	offset &= 0xff;
	len &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE;
	USETW(req.wValue, 0x0000);
	USETW(req.wIndex, offset);
	USETW(req.wLength, len);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf,
	    0, NULL, 1000));
}

static uint8_t
udav_csr_read1(struct udav_softc *sc, uint16_t offset)
{
	uint8_t val;

	udav_csr_read(sc, offset, &val, 1);
	return (val);
}

static int
udav_csr_write1(struct udav_softc *sc, uint16_t offset,
    uint8_t ch)
{
	struct usb_device_request req;

	offset &= 0xff;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = UDAV_REQ_REG_WRITE1;
	USETW(req.wValue, ch);
	USETW(req.wIndex, offset);
	USETW(req.wLength, 0x0000);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, NULL,
	    0, NULL, 1000));
}

static void
udav_init(void *arg)
{
	struct udav_softc *sc = arg;

	UDAV_LOCK(sc);
	udav_init_locked(sc);
	UDAV_UNLOCK(sc);
}

static void
udav_init_locked(struct udav_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Cancel pending I/O
	 */
	udav_stop(sc);

	/* set MAC address */
	udav_csr_write(sc, UDAV_PAR, IF_LLADDR(ifp), ETHER_ADDR_LEN);

	/* initialize network control register */

	/* disable loopback  */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_LBK0 | UDAV_NCR_LBK1);

	/* Initialize RX control register */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_DIS_LONG | UDAV_RCR_DIS_CRC);

	/* load multicast filter and update promiscious mode bit */
	udav_setpromisc(sc);

	/* enable RX */
	UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_RXEN);

	/* clear POWER_DOWN state of internal PHY */
	UDAV_SETBIT(sc, UDAV_GPCR, UDAV_GPCR_GEP_CNTL0);
	UDAV_CLRBIT(sc, UDAV_GPR, UDAV_GPR_GEPIO0);

	usbd_xfer_set_stall(sc->sc_xfer[UDAV_BULK_DT_WR]);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sleepout_reset(&sc->sc_watchdog, hz, udav_watchdog, sc);
	udav_start_locked(sc->sc_ifp);
}

static void
udav_reset(struct udav_softc *sc)
{
	int i;

	/* Select PHY */
#if 1
	/*
	 * XXX: force select internal phy.
	 *	external phy routines are not tested.
	 */
	UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
#else
	if (sc->sc_flags & UDAV_EXT_PHY)
		UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
	else
		UDAV_CLRBIT(sc, UDAV_NCR, UDAV_NCR_EXT_PHY);
#endif

	UDAV_SETBIT(sc, UDAV_NCR, UDAV_NCR_RST);

	for (i = 0; i < UDAV_TX_TIMEOUT; i++) {
		if (!(udav_csr_read1(sc, UDAV_NCR) & UDAV_NCR_RST))
			break;
		usb_pause_mtx(&sc->sc_mtx, hz / 100);
	}

	usb_pause_mtx(&sc->sc_mtx, hz / 100);
}

#define	UDAV_BITS	6
static void
udav_setmulti(void *arg, int npending)
{
	struct udav_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ifmultiaddr *ifma;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int h = 0;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		UDAV_SETBIT(sc, UDAV_RCR, UDAV_RCR_ALL|UDAV_RCR_PRMSC);
		return;
	}

	/* first, zot all the existing hash bits */
	memset(hashtbl, 0x00, sizeof(hashtbl));
	hashtbl[7] |= 0x80;	/* broadcast address */
	udav_csr_write(sc, UDAV_MAR, hashtbl, sizeof(hashtbl));

	/* now program new ones */
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		hashtbl[h / 8] |= 1 << (h % 8);
	}
	if_maddr_runlock(ifp);

	/* disable all multicast */
	UDAV_CLRBIT(sc, UDAV_RCR, UDAV_RCR_ALL);

	/* write hash value to the register */
	udav_csr_write(sc, UDAV_MAR, hashtbl, sizeof(hashtbl));
}

static void
udav_setpromisc(struct udav_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t rxmode;

	rxmode = udav_csr_read1(sc, UDAV_RCR);
	rxmode &= ~(UDAV_RCR_ALL | UDAV_RCR_PRMSC);

	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= UDAV_RCR_ALL | UDAV_RCR_PRMSC;
	else if (ifp->if_flags & IFF_ALLMULTI)
		rxmode |= UDAV_RCR_ALL;

	/* write new mode bits */
	udav_csr_write1(sc, UDAV_RCR, rxmode);
}

static void
udav_start(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;

	UDAV_LOCK(sc);
	udav_start_locked(ifp);
	UDAV_UNLOCK(sc);
}

static void
udav_start_locked(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * start the USB transfers, if not already started:
	 */
	usbd_transfer_start(sc->sc_xfer[UDAV_INTR_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[UDAV_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[UDAV_BULK_DT_WR]);
}

static void
udav_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct udav_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	struct mbuf *m;
	int extra_len;
	int temp_len;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");
		ifp->if_opackets++;
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if ((sc->sc_flags & UDAV_FLAG_LINK) == 0) {
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
		if (m->m_pkthdr.len < UDAV_MIN_FRAME_LEN)
			extra_len = UDAV_MIN_FRAME_LEN - m->m_pkthdr.len;
		else
			extra_len = 0;

		temp_len = (m->m_pkthdr.len + extra_len);

		/*
		 * the frame length is specified in the first 2 bytes of the
		 * buffer
		 */
		buf[0] = (uint8_t)(temp_len);
		buf[1] = (uint8_t)(temp_len >> 8);

		temp_len += 2;

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, buf, 2);
		usbd_m_copy_in(pc, 2, m, 0, m->m_pkthdr.len);

		if (extra_len)
			usbd_frame_zero(pc, temp_len - extra_len, extra_len);
		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
		 */
		BPF_MTAP(ifp, m);

		m_freem(m);

		usbd_xfer_set_frame_len(xfer, 0, temp_len);
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
udav_rxflush(struct udav_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	for (;;) {
		_IF_DEQUEUE(&sc->sc_rxq, m);
		if (m == NULL)
			break;

		/*
		 * The USB xfer has been resubmitted so its safe to unlock now.
		 */
		UDAV_UNLOCK(sc);
		ifp->if_input(ifp, m);
		UDAV_LOCK(sc);
	}
}

static struct mbuf *
udav_newbuf(void)
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
udav_rxbuf(struct udav_softc *sc, struct usb_page_cache *pc, 
    unsigned int offset, unsigned int len)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	if (len < ETHER_HDR_LEN || len > MCLBYTES - ETHER_ALIGN)
		return (1);

	m = udav_newbuf();
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
udav_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct udav_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	struct udav_rxpkt stat;
	int len;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (actlen < sizeof(stat) + ETHER_CRC_LEN) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, &stat, sizeof(stat));
		actlen -= sizeof(stat);
		len = min(actlen, le16toh(stat.pktlen));
		len -= ETHER_CRC_LEN;

		if (stat.rxstat & UDAV_RSR_LCS) {
			ifp->if_collisions++;
			goto tr_setup;
		}
		if (stat.rxstat & UDAV_RSR_ERR) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		udav_rxbuf(sc, pc, sizeof(stat), len);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		udav_rxflush(sc);
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
udav_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
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
udav_stop(struct udav_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->sc_flags &= ~UDAV_FLAG_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[UDAV_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[UDAV_BULK_DT_RD]);
	usbd_transfer_stop(sc->sc_xfer[UDAV_INTR_DT_RD]);

	udav_reset(sc);
}

static int
udav_ifmedia_upd(struct ifnet *ifp)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

        sc->sc_flags &= ~UDAV_FLAG_LINK;
	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);
	return (0);
}

static void
udav_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct udav_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	UDAV_LOCK(sc);
	mii_pollstat(mii);
	UDAV_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
udav_tick(struct udav_softc *sc)
{
	struct mii_data *mii = GET_MII(sc);

	UDAV_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
	if ((sc->sc_flags & UDAV_FLAG_LINK) == 0
	    && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sc_flags |= UDAV_FLAG_LINK;
		udav_start_locked(sc->sc_ifp);
	}
}

static int
udav_miibus_readreg(device_t dev, int phy, int reg)
{
	struct udav_softc *sc = device_get_softc(dev);
	uint16_t data16;
	uint8_t val[2];
	int locked;

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		UDAV_LOCK(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
	    UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* select PHY operation and start read command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRR);

	/* XXX: should we wait? */

	/* end read command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRR);

	/* retrieve the result from data registers */
	udav_csr_read(sc, UDAV_EPDRL, val, 2);

	data16 = (val[0] | (val[1] << 8));

	DPRINTFN(11, "phy=%d reg=0x%04x => 0x%04x\n",
	    phy, reg, data16);

	if (!locked)
		UDAV_UNLOCK(sc);
	return (data16);
}

static int
udav_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct udav_softc *sc = device_get_softc(dev);
	uint8_t val[2];
	int locked;

	/* XXX: one PHY only for the internal PHY */
	if (phy != 0)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		UDAV_LOCK(sc);

	/* select internal PHY and set PHY register address */
	udav_csr_write1(sc, UDAV_EPAR,
	    UDAV_EPAR_PHY_ADR0 | (reg & UDAV_EPAR_EROA_MASK));

	/* put the value to the data registers */
	val[0] = (data & 0xff);
	val[1] = (data >> 8) & 0xff;
	udav_csr_write(sc, UDAV_EPDRL, val, 2);

	/* select PHY operation and start write command */
	udav_csr_write1(sc, UDAV_EPCR, UDAV_EPCR_EPOS | UDAV_EPCR_ERPRW);

	/* XXX: should we wait? */

	/* end write command */
	UDAV_CLRBIT(sc, UDAV_EPCR, UDAV_EPCR_ERPRW);

	if (!locked)
		UDAV_UNLOCK(sc);
	return (0);
}

static void
udav_miibus_statchg(device_t dev)
{

	/* nothing to do */
}

static int
udav_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct udav_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii = GET_MII(sc);
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		UDAV_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				udav_setpromisc(sc);
			else
				udav_init_locked(sc);
		} else
			udav_stop(sc);
		UDAV_UNLOCK(sc);
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
udav_watchdog(void *arg)
{
	struct udav_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	udav_tick(sc);
	sleepout_reset(&sc->sc_watchdog, hz, udav_watchdog, sc);
}
