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
 * CATC USB-EL1210A USB to ethernet driver. Used in the CATC Netmate
 * adapters and others.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The CATC USB-EL1210A provides USB ethernet support at 10Mbps. The
 * RX filter uses a 512-bit multicast hash table, single perfect entry
 * for the station address, and promiscuous mode. Unlike the ADMtek
 * and KLSI chips, the CATC ASIC supports read and write combining
 * mode where multiple packets can be transfered using a single bulk
 * transaction, which helps performance a great deal.
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

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR cue_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_sleepout.h>
#include <dev/usb/net/if_cuereg.h>

/*
 * Various supported device vendors/products.
 */

/* Belkin F5U111 adapter covered by NETMATE entry */

static const struct usb_device_id cue_devs[] = {
#define	CUE_DEV(v,p) { USB_VP(USB_VENDOR_##v, USB_PRODUCT_##v##_##p) }
	CUE_DEV(CATC, NETMATE),
	CUE_DEV(CATC, NETMATE2),
	CUE_DEV(SMARTBRIDGES, SMARTLINK),
#undef CUE_DEV
};

/* prototypes */

static device_probe_t cue_probe;
static device_attach_t cue_attach;
static device_detach_t cue_detach;

static usb_callback_t cue_bulk_read_callback;
static usb_callback_t cue_bulk_write_callback;

static uint8_t	cue_csr_read_1(struct cue_softc *, uint16_t);
static uint16_t	cue_csr_read_2(struct cue_softc *, uint8_t);
static int	cue_csr_write_1(struct cue_softc *, uint16_t, uint16_t);
static int	cue_mem(struct cue_softc *, uint8_t, uint16_t, void *, int);
static int	cue_getmac(struct cue_softc *, void *);
static uint32_t	cue_mchash(const uint8_t *);
static void	cue_reset(struct cue_softc *);
static void	cue_setmulti(void *, int);
static void	cue_setmulti_locked(struct cue_softc *);
static void	cue_init(void *);
static void	cue_init_locked(struct cue_softc *);
static int	cue_ioctl(struct ifnet *, u_long, caddr_t);
static void	cue_start(struct ifnet *);
static void	cue_start_locked(struct ifnet *);
static int	cue_rxbuf(struct cue_softc *, struct usb_page_cache *, 
		    unsigned int, unsigned int);
static void	cue_rxflush(struct cue_softc *);
static void	cue_stop(struct cue_softc *);
static void	cue_watchdog(void *);

#ifdef USB_DEBUG
static int cue_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, cue, CTLFLAG_RW, 0, "USB cue");
SYSCTL_INT(_hw_usb_cue, OID_AUTO, debug, CTLFLAG_RW, &cue_debug, 0,
    "Debug level");
#endif

static const struct usb_config cue_config[CUE_N_TRANSFER] = {
	[CUE_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = (MCLBYTES + 2),
		.flags = {.pipe_bof = 1,},
		.callback = cue_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},
	[CUE_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = (MCLBYTES + 2),
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = cue_bulk_read_callback,
	},
};

static device_method_t cue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, cue_probe),
	DEVMETHOD(device_attach, cue_attach),
	DEVMETHOD(device_detach, cue_detach),

	{0, 0}
};

static driver_t cue_driver = {
	.name = "cue",
	.methods = cue_methods,
	.size = sizeof(struct cue_softc),
};

static devclass_t cue_devclass;

DRIVER_MODULE(cue, uhub, cue_driver, cue_devclass, NULL, 0);
MODULE_DEPEND(cue, usb, 1, 1, 1);
MODULE_DEPEND(cue, ether, 1, 1, 1);
MODULE_VERSION(cue, 1);

#define	CUE_SETBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) | (x))

#define	CUE_CLRBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) & ~(x))

static uint8_t
cue_csr_read_1(struct cue_softc *sc, uint16_t reg)
{
	struct usb_device_request req;
	uint8_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	/* XXX ignore any errors */
	(void)usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, &val, 0,
	    NULL, 1000);
	return (val);
}

static uint16_t
cue_csr_read_2(struct cue_softc *sc, uint8_t reg)
{
	struct usb_device_request req;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	/* XXX ignore any errors */
	(void)usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, &val, 0,
	    NULL, 1000);
	return (le16toh(val));
}

static int
cue_csr_write_1(struct cue_softc *sc, uint16_t reg, uint16_t val)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, NULL, 0,
	    NULL, 1000));
}

static int
cue_mem(struct cue_softc *sc, uint8_t cmd, uint16_t addr, void *buf, int len)
{
	struct usb_device_request req;

	if (cmd == CUE_CMD_READSRAM)
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf, 0,
	    NULL, 1000));
}

static int
cue_getmac(struct cue_softc *sc, void *buf)
{
	struct usb_device_request req;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_GET_MACADDR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, ETHER_ADDR_LEN);

	return (usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, buf, 0,
	    NULL, 1000));
}

#define	CUE_BITS 9

static uint32_t
cue_mchash(const uint8_t *addr)
{
	uint32_t crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);

	return (crc & ((1 << CUE_BITS) - 1));
}

static void
cue_setpromisc(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	/* if we want promiscuous mode, set the allframes bit */
	if (ifp->if_flags & IFF_PROMISC)
		CUE_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
	else
		CUE_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);

	/* write multicast hash-bits */
	cue_setmulti_locked(sc);
}

static void
cue_setmulti(void *arg, int npending)
{
	struct cue_softc *sc = arg;

	CUE_LOCK(sc);
	cue_setmulti_locked(sc);
	CUE_UNLOCK(sc);
}

static void
cue_setmulti_locked(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ifmultiaddr *ifma;
	uint32_t h = 0, i;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < 8; i++)
			hashtbl[i] = 0xff;
		cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
		    &hashtbl, 8);
		return;
	}

	/* now program new ones */
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = cue_mchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		hashtbl[h >> 3] |= 1 << (h & 0x7);
	}
	if_maddr_runlock(ifp);

	/*
	 * Also include the broadcast address in the filter
	 * so we can receive broadcast frames.
 	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		h = cue_mchash(ifp->if_broadcastaddr);
		hashtbl[h >> 3] |= 1 << (h & 0x7);
	}

	cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR, &hashtbl, 8);
}

static void
cue_reset(struct cue_softc *sc)
{
	struct usb_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	/* XXX ignore any errors */
	(void)usbd_do_request_flags(sc->sc_udev, &sc->sc_mtx, &req, NULL, 0,
	    NULL, 1000);
	/*
	 * wait a little while for the chip to get its brains in order:
	 */
	usb_pause_mtx(&sc->sc_mtx, hz / 100);
}

static int
cue_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != CUE_CONFIG_IDX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != CUE_IFACE_IDX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(cue_devs, sizeof(cue_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
cue_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct cue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	uint8_t iface_index;
	int error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	sleepout_create(&sc->sc_sleepout, "cue sleepout");
	sleepout_init_mtx(&sc->sc_sleepout, &sc->sc_watchdog, &sc->sc_mtx, 0);
	TASK_INIT(&sc->sc_setmulti, 0, cue_setmulti, sc);

	iface_index = CUE_IFACE_IDX;
	error = usbd_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, cue_config, CUE_N_TRANSFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed\n");
		goto detach;
	}

	CUE_LOCK(sc);
	cue_getmac(sc, sc->sc_eaddr);
	CUE_UNLOCK(sc);

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
	ifp->if_ioctl = cue_ioctl;
	ifp->if_start = cue_start;
	ifp->if_init = cue_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	if_printf(ifp, "<USB Ethernet> on %s\n",
	    device_get_nameunit(sc->sc_dev));
	ether_ifattach(ifp, sc->sc_eaddr);
	return (0);
detach:
	cue_detach(dev);
	return (ENXIO);			/* failure */
}

static int
cue_detach(device_t dev)
{
	struct cue_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	sleepout_drain(&sc->sc_watchdog);
	taskqueue_drain(sc->sc_sleepout.s_taskqueue, &sc->sc_setmulti);
	usbd_transfer_unsetup(sc->sc_xfer, CUE_N_TRANSFER);
	if (ifp != NULL) {
		CUE_LOCK(sc);
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		CUE_UNLOCK(sc);
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	sleepout_free(&sc->sc_sleepout);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
cue_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct cue_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	uint8_t buf[2];
	int len;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if (actlen <= (2 + sizeof(struct ether_header))) {
			ifp->if_ierrors++;
			goto tr_setup;
		}
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, buf, 2);
		actlen -= 2;
		len = buf[0] | (buf[1] << 8);
		len = min(actlen, len);

		cue_rxbuf(sc, pc, 2, len);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		cue_rxflush(sc);
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
cue_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct cue_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	struct mbuf *m;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer complete\n");
		ifp->if_opackets++;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			return;
		if (m->m_pkthdr.len > MCLBYTES)
			m->m_pkthdr.len = MCLBYTES;
		usbd_xfer_set_frame_len(xfer, 0, (m->m_pkthdr.len + 2));

		/* the first two bytes are the frame length */

		buf[0] = (uint8_t)(m->m_pkthdr.len);
		buf[1] = (uint8_t)(m->m_pkthdr.len >> 8);

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, buf, 2);
		usbd_m_copy_in(pc, 2, m, 0, m->m_pkthdr.len);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
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
cue_tick(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_SINGLECOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_MULTICOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_EXCESSCOLL);

	if (cue_csr_read_2(sc, CUE_RX_FRAMEERR))
		ifp->if_ierrors++;
}

static void
cue_start(struct ifnet *ifp)
{
	struct cue_softc *sc = ifp->if_softc;

	CUE_LOCK(sc);
	cue_start_locked(ifp);
	CUE_UNLOCK(sc);
}

static void
cue_start_locked(struct ifnet *ifp)
{
	struct cue_softc *sc = ifp->if_softc;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * start the USB transfers, if not already started:
	 */
	usbd_transfer_start(sc->sc_xfer[CUE_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[CUE_BULK_DT_WR]);
}

static void
cue_init(void *arg)
{
	struct cue_softc *sc = arg;

	CUE_LOCK(sc);
	cue_init_locked(sc);
	CUE_UNLOCK(sc);
}

static void
cue_init_locked(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	int i;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	cue_stop(sc);
#if 0
	cue_reset(sc);
#endif
	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		cue_csr_write_1(sc, CUE_PAR0 - i, IF_LLADDR(ifp)[i]);

	/* Enable RX logic. */
	cue_csr_write_1(sc, CUE_ETHCTL, CUE_ETHCTL_RX_ON | CUE_ETHCTL_MCAST_ON);

	/* Load the multicast filter */
	cue_setpromisc(sc);

	/*
	 * Set the number of RX and TX buffers that we want
	 * to reserve inside the ASIC.
	 */
	cue_csr_write_1(sc, CUE_RX_BUFPKTS, CUE_RX_FRAMES);
	cue_csr_write_1(sc, CUE_TX_BUFPKTS, CUE_TX_FRAMES);

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x01);/* 1 wait state */

	/* Program the LED operation. */
	cue_csr_write_1(sc, CUE_LEDCTL, CUE_LEDCTL_FOLLOW_LINK);

	usbd_xfer_set_stall(sc->sc_xfer[CUE_BULK_DT_WR]);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sleepout_reset(&sc->sc_watchdog, hz, cue_watchdog, sc);
	cue_start_locked(sc->sc_ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
cue_stop(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[CUE_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[CUE_BULK_DT_RD]);

	cue_csr_write_1(sc, CUE_ETHCTL, 0);
	cue_reset(sc);
}

static struct mbuf *
cue_newbuf(void)
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
cue_rxbuf(struct cue_softc *sc, struct usb_page_cache *pc, 
    unsigned int offset, unsigned int len)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	if (len < ETHER_HDR_LEN || len > MCLBYTES - ETHER_ALIGN)
		return (1);

	m = cue_newbuf();
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
cue_rxflush(struct cue_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;

	CUE_LOCK_ASSERT(sc, MA_OWNED);

	for (;;) {
		_IF_DEQUEUE(&sc->sc_rxq, m);
		if (m == NULL)
			break;

		/*
		 * The USB xfer has been resubmitted so its safe to unlock now.
		 */
		CUE_UNLOCK(sc);
		ifp->if_input(ifp, m);
		CUE_LOCK(sc);
	}
}

static int
cue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cue_softc *sc = ifp->if_softc;
	int error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		CUE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				cue_setpromisc(sc);
			else
				cue_init_locked(sc);
		} else
			cue_stop(sc);
		CUE_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_UP &&
		    ifp->if_drv_flags & IFF_DRV_RUNNING)
			taskqueue_enqueue(sc->sc_sleepout.s_taskqueue,
			    &sc->sc_setmulti);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static void
cue_watchdog(void *arg)
{
	struct cue_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	cue_tick(sc);
	sleepout_reset(&sc->sc_watchdog, hz, cue_watchdog, sc);
}
