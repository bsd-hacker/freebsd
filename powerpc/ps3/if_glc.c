/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: user/nwhitehorn/ps3/powerpc/ofw/ofw_cpu.c 193156 2009-05-31 09:01:23Z nwhitehorn $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <machine/pio.h>
#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include "ps3bus.h"
#include "ps3-hvcall.h"
#include "if_glcreg.h"

static int	glc_probe(device_t);
static int	glc_attach(device_t);
static void	glc_init(void *xsc);
static void	glc_start(struct ifnet *ifp);
static int	glc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int	glc_add_rxbuf(struct glc_softc *sc, int idx);
static int	glc_add_rxbuf_dma(struct glc_softc *sc, int idx);
static int	glc_encap(struct glc_softc *sc, struct mbuf **m_head,
		    bus_addr_t *pktdesc);
static void	glc_intr(void *xsc);

static MALLOC_DEFINE(M_GLC, "gelic", "PS3 GELIC ethernet");

static device_method_t glc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		glc_probe),
	DEVMETHOD(device_attach,	glc_attach),

	{ 0, 0 }
};

static driver_t glc_driver = {
	"glc",
	glc_methods,
	sizeof(struct glc_softc)
};

static devclass_t glc_devclass;

DRIVER_MODULE(glc, ps3bus, glc_driver, glc_devclass, 0, 0);

static int 
glc_probe(device_t dev) 
{

	if (ps3bus_get_bustype(dev) != PS3_BUSTYPE_SYSBUS ||
	    ps3bus_get_devtype(dev) != PS3_DEVTYPE_GELIC)
		return (ENXIO);

	device_set_desc(dev, "Playstation 3 GELIC Network Controller");
	return (BUS_PROBE_SPECIFIC);
}

static void
glc_getphys(void *xaddr, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;

	*(bus_addr_t *)xaddr = segs[0].ds_addr;
}

static int 
glc_attach(device_t dev) 
{
	struct glc_softc *sc;
	struct glc_txsoft *txs;
	uint64_t mac64, val, junk;
	int i, err;

	sc = device_get_softc(dev);

	sc->sc_bus = ps3bus_get_bus(dev);
	sc->sc_dev = ps3bus_get_device(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	sc->next_txdma_slot = sc->first_used_txdma_slot = 0;

	/*
	 * Shut down existing tasks.
	 */

	lv1_net_stop_tx_dma(sc->sc_bus, sc->sc_dev, 0);
	lv1_net_stop_rx_dma(sc->sc_bus, sc->sc_dev, 0);

	sc->sc_ifp = if_alloc(IFT_ETHER);
	sc->sc_ifp->if_softc = sc;

	/*
	 * Get MAC address and VLAN id
	 */

	lv1_net_control(sc->sc_bus, sc->sc_dev, GELIC_GET_MAC_ADDRESS,
	    0, 0, 0, &mac64, &junk);
	memcpy(sc->sc_enaddr, &((uint8_t *)&mac64)[2], sizeof(sc->sc_enaddr));
	sc->sc_tx_vlan = sc->sc_rx_vlan =  -1;
	err = lv1_net_control(sc->sc_bus, sc->sc_dev, GELIC_GET_VLAN_ID,
	    GELIC_VLAN_TX_ETHERNET, 0, 0, &val, &junk);
	if (err == 0)
		sc->sc_tx_vlan = val;
	err = lv1_net_control(sc->sc_bus, sc->sc_dev, GELIC_GET_VLAN_ID,
	    GELIC_VLAN_RX_ETHERNET, 0, 0, &val, &junk);
	if (err == 0)
		sc->sc_rx_vlan = val;

	/*
	 * Set up interrupt handler
	 */
	sc->sc_irqid = 0;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_irqid,
	    RF_ACTIVE);
	if (sc->sc_irq == NULL) {
		device_printf(dev, "Could not allocate IRQ!\n");
		mtx_destroy(&sc->sc_mtx);
		return (ENXIO);
	}

	bus_setup_intr(dev, sc->sc_irq,
	    INTR_TYPE_MISC | INTR_MPSAFE | INTR_ENTROPY, NULL, glc_intr,
	    sc, &sc->sc_irqctx);
	sc->sc_interrupt_status = (uint64_t *)contigmalloc(8, M_GLC, M_ZERO, 0,
	    BUS_SPACE_MAXADDR_32BIT, 8, PAGE_SIZE);
	lv1_net_set_interrupt_status_indicator(sc->sc_bus, sc->sc_dev,
	    vtophys(sc->sc_interrupt_status), 0);
	lv1_net_set_interrupt_mask(sc->sc_bus, sc->sc_dev,
	    GELIC_INT_RXDONE | GELIC_INT_TXDONE | GELIC_INT_RXFRAME |
	    GELIC_INT_PHY, 0);

	/*
	 * Set up DMA.
	 */

	/* XXX: following should be integrated to busdma */
	err = lv1_allocate_device_dma_region(sc->sc_bus, sc->sc_dev,
	    0x8000000 /* 128 MB */, 24 /* log_2(16 MB) */,
	    0 /* 32-bit transfers */, &sc->sc_dma_base);
	if (err != 0) {
		device_printf(dev, "could not allocate DMA region: %d\n", err);
		goto fail;
	}

	err = lv1_map_device_dma_region(sc->sc_bus, sc->sc_dev, 0 /* physmem */,
	    sc->sc_dma_base, 0x8000000 /* 128 MB */,
	    0xf800000000000000UL /* see Cell IO/MMU docs */);
	if (err != 0) {
		device_printf(dev, "could not map DMA region: %d\n", err);
		goto fail;
	}

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 32, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    129*sizeof(struct glc_dmadesc), 1, 128*sizeof(struct glc_dmadesc),
	    0, NULL,NULL, &sc->sc_dmadesc_tag);

	err = bus_dmamem_alloc(sc->sc_dmadesc_tag, (void **)&sc->sc_txdmadesc,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &sc->sc_txdmadesc_map);
	err = bus_dmamap_load(sc->sc_dmadesc_tag, sc->sc_txdmadesc_map,
	    sc->sc_txdmadesc, 128*sizeof(struct glc_dmadesc), glc_getphys,
	    &sc->sc_txdmadesc_phys, 0);
	err = bus_dmamem_alloc(sc->sc_dmadesc_tag, (void **)&sc->sc_rxdmadesc,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &sc->sc_rxdmadesc_map);
	err = bus_dmamap_load(sc->sc_dmadesc_tag, sc->sc_rxdmadesc_map,
	    sc->sc_rxdmadesc, 128*sizeof(struct glc_dmadesc), glc_getphys,
	    &sc->sc_rxdmadesc_phys, 0);

	err = bus_dma_tag_create(bus_get_dma_tag(dev), 128, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, 0, BUS_SPACE_MAXSIZE_32BIT, 0, NULL,NULL,
	    &sc->sc_rxdma_tag);
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE_32BIT, 16, BUS_SPACE_MAXSIZE_32BIT, 0, NULL,NULL,
	    &sc->sc_txdma_tag);

	/* init transmit descriptors */
	STAILQ_INIT(&sc->sc_txfreeq);
	STAILQ_INIT(&sc->sc_txdirtyq);

	/* create TX DMA maps */
	err = ENOMEM;
	for (i = 0; i < GLC_MAX_TX_PACKETS; i++) {
		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		err = bus_dmamap_create(sc->sc_txdma_tag, 0, &txs->txs_dmamap);
		if (err) {
			device_printf(dev,
			    "unable to create TX DMA map %d, error = %d\n",
			    i, err);
		}
		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/* Create the receive buffer DMA maps. */
	for (i = 0; i < GLC_MAX_RX_PACKETS; i++) {
		err = bus_dmamap_create(sc->sc_rxdma_tag, 0,
		    &sc->sc_rxsoft[i].rxs_dmamap);
		if (err) {
			device_printf(dev,
			    "unable to create RX DMA map %d, error = %d\n",
			    i, err);
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/*
	 * Attach to network stack
	 */

	if_initname(sc->sc_ifp, device_get_name(dev), device_get_unit(dev));
	sc->sc_ifp->if_mtu = ETHERMTU;
	sc->sc_ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	sc->sc_ifp->if_start = glc_start;
	sc->sc_ifp->if_ioctl = glc_ioctl;
	sc->sc_ifp->if_init = glc_init;

	IFQ_SET_MAXLEN(&sc->sc_ifp->if_snd, GLC_MAX_TX_PACKETS);
	sc->sc_ifp->if_snd.ifq_drv_maxlen = GLC_MAX_TX_PACKETS;
	IFQ_SET_READY(&sc->sc_ifp->if_snd);

	ether_ifattach(sc->sc_ifp, sc->sc_enaddr);
	sc->sc_ifp->if_hwassist = 0;

	return (0);

fail:
	mtx_destroy(&sc->sc_mtx);
	if_free(sc->sc_ifp);
	return (ENXIO);
}

static void
glc_init_locked(struct glc_softc *sc)
{
	int i;
	struct glc_rxsoft *rxs;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	lv1_net_stop_tx_dma(sc->sc_bus, sc->sc_dev, 0);
	lv1_net_stop_rx_dma(sc->sc_bus, sc->sc_dev, 0);

	for (i = 0; i < GLC_MAX_RX_PACKETS; i++) {
		rxs = &sc->sc_rxsoft[i];
		rxs->rxs_desc_slot = i;

		if (rxs->rxs_mbuf == NULL) {
			glc_add_rxbuf(sc, i);

			if (rxs->rxs_mbuf == NULL) {
				rxs->rxs_desc_slot = -1;
				break;
			}
		}

		glc_add_rxbuf_dma(sc, i);
		bus_dmamap_sync(sc->sc_dmadesc_tag, sc->sc_rxdmadesc_map,
		    BUS_DMASYNC_PREREAD);
	}

	lv1_net_start_rx_dma(sc->sc_bus, sc->sc_dev,
	    sc->sc_rxsoft[0].rxs_desc, 0);

	sc->sc_ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->sc_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_ifpflags = sc->sc_ifp->if_flags;
}

static void
glc_init(void *xsc)
{
	struct glc_softc *sc = xsc;

	mtx_lock(&sc->sc_mtx);
	glc_init_locked(sc);
	mtx_unlock(&sc->sc_mtx);
}

static void
glc_start_locked(struct ifnet *ifp)
{
	struct glc_softc *sc = ifp->if_softc;
	struct glc_txsoft *txs;
	bus_addr_t first, pktdesc;
	int i, kickstart;
	struct mbuf *mb_head;

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	first = 0;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, mb_head);

		if (mb_head == NULL)
			break;

		BPF_MTAP(ifp, mb_head);

		if (sc->sc_tx_vlan >= 0)
			mb_head = ether_vlanencap(mb_head, sc->sc_tx_vlan);

		if (glc_encap(sc, &mb_head, &pktdesc)) {
			/* Put the packet back and stop */
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, mb_head);
			break;
		}

		if (first == 0)
			first = pktdesc;
	}

	bus_dmamap_sync(sc->sc_dmadesc_tag, sc->sc_txdmadesc_map,
	    BUS_DMASYNC_PREREAD);

	kickstart = 1;
	STAILQ_FOREACH(txs, &sc->sc_txdirtyq, txs_q) {
		for (i = txs->txs_firstdesc;
		     i != (txs->txs_lastdesc+1) % GLC_MAX_TX_PACKETS;
		     i = (i + 1) % GLC_MAX_TX_PACKETS) {
			/*
			 * Check if any segments are currently being processed.
			 * If so, the DMA engine will pick up the bits we
			 * added eventually, otherwise restart DMA
			 */

			if (sc->sc_txdmadesc[i].cmd_stat & GELIC_DESCR_OWNED) {
				//kickstart = 0;
				break;
			}
		}
	}

	if (kickstart && first != 0)
		lv1_net_start_tx_dma(sc->sc_bus, sc->sc_dev, first, 0);
}

static void
glc_start(struct ifnet *ifp)
{
	struct glc_softc *sc = ifp->if_softc;

	mtx_lock(&sc->sc_mtx);
	glc_start_locked(ifp);
	mtx_unlock(&sc->sc_mtx);
}

static int
glc_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct glc_softc *sc = ifp->if_softc;
#if 0
	struct ifreq *ifr = (struct ifreq *)data;
#endif
	int err = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
                mtx_lock(&sc->sc_mtx);
		if ((ifp->if_flags & IFF_UP) != 0) {
#if 0
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0 &&
			   ((ifp->if_flags ^ sc->sc_ifpflags) &
			    (IFF_ALLMULTI | IFF_PROMISC)) != 0)
				bm_setladrf(sc);
			else
#endif
				glc_init_locked(sc);
		}
#if 0
		else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
			bm_stop(sc);
#endif
		sc->sc_ifpflags = ifp->if_flags;
		mtx_unlock(&sc->sc_mtx);
		break;
	default:
		err = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (err);
}

static int
glc_add_rxbuf(struct glc_softc *sc, int idx)
{
	struct glc_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	bus_dma_segment_t segs[1];
	int error, nsegs;
			
	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;

	if (rxs->rxs_mbuf != NULL) {
		bus_dmamap_sync(sc->sc_rxdma_tag, rxs->rxs_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_rxdma_tag, rxs->rxs_dmamap);
	}

	error = bus_dmamap_load_mbuf_sg(sc->sc_rxdma_tag, rxs->rxs_dmamap, m,
	    segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_self,
		    "cannot load RS DMA map %d, error = %d\n", idx, error);
		m_freem(m);
		return (error);
	}
	/* If nsegs is wrong then the stack is corrupt. */
	KASSERT(nsegs == 1,
	    ("%s: too many DMA segments (%d)", __func__, nsegs));
	rxs->rxs_mbuf = m;
	rxs->segment = segs[0];

	bus_dmamap_sync(sc->sc_rxdma_tag, rxs->rxs_dmamap, BUS_DMASYNC_PREREAD);

	return (0);
}

static int
glc_add_rxbuf_dma(struct glc_softc *sc, int idx)
{
	struct glc_rxsoft *rxs = &sc->sc_rxsoft[idx];
	
	bzero(&sc->sc_rxdmadesc[idx], sizeof(sc->sc_rxdmadesc[idx]));
	sc->sc_rxdmadesc[idx].paddr = sc->sc_dma_base + rxs->segment.ds_addr;
	sc->sc_rxdmadesc[idx].len = rxs->segment.ds_len;
	sc->sc_rxdmadesc[idx].next = sc->sc_dma_base + sc->sc_rxdmadesc_phys +
	    ((idx + 1) % GLC_MAX_RX_PACKETS)*sizeof(sc->sc_rxdmadesc[idx]);
	sc->sc_rxdmadesc[idx].cmd_stat = GELIC_DESCR_OWNED;

	rxs->rxs_desc_slot = idx;
	rxs->rxs_desc = sc->sc_dma_base + sc->sc_rxdmadesc_phys +
	    idx*sizeof(struct glc_dmadesc);

        return (0);
}

static int
glc_encap(struct glc_softc *sc, struct mbuf **m_head, bus_addr_t *pktdesc)
{
	bus_dma_segment_t segs[16];
	struct glc_txsoft *txs;
	struct mbuf *m;
	bus_addr_t firstslotphys;
	int i, idx;
	int nsegs = 16;
	int err = 0;

	/* Max number of segments is the number of free DMA slots */
	if (sc->next_txdma_slot >= sc->first_used_txdma_slot)
		nsegs = 128 - sc->next_txdma_slot + sc->first_used_txdma_slot;
	else
		nsegs = sc->first_used_txdma_slot - sc->next_txdma_slot;

	if (nsegs > 16)
		nsegs = 16;

	/* Get a work queue entry. */
	if ((txs = STAILQ_FIRST(&sc->sc_txfreeq)) == NULL) {
		/* Ran out of descriptors. */
		return (ENOBUFS);
	}
	
	err = bus_dmamap_load_mbuf_sg(sc->sc_txdma_tag, txs->txs_dmamap,
	    *m_head, segs, &nsegs, BUS_DMA_NOWAIT);

	if (err == EFBIG) {
		m = m_collapse(*m_head, M_DONTWAIT, nsegs);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;

		err = bus_dmamap_load_mbuf_sg(sc->sc_txdma_tag,
		    txs->txs_dmamap, *m_head, segs, &nsegs, BUS_DMA_NOWAIT);
		if (err != 0) {
			m_freem(*m_head);
			*m_head = NULL;
			return (err);
		}
	} else if (err != 0)
		return (err);

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	txs->txs_ndescs = nsegs;
	txs->txs_firstdesc = sc->next_txdma_slot;

	idx = txs->txs_firstdesc;
	firstslotphys = sc->sc_dma_base + sc->sc_txdmadesc_phys +
	    txs->txs_firstdesc*sizeof(struct glc_dmadesc);

	for (i = 0; i < nsegs; i++) {
		if (i+1 == nsegs)
			txs->txs_lastdesc = sc->next_txdma_slot;

		bzero(&sc->sc_txdmadesc[idx], sizeof(sc->sc_txdmadesc[idx]));
		sc->sc_txdmadesc[idx].paddr = sc->sc_dma_base + segs[i].ds_addr;
		sc->sc_txdmadesc[idx].len = segs[i].ds_len;
		sc->sc_txdmadesc[idx].next = sc->sc_dma_base +
		    sc->sc_txdmadesc_phys +
		    ((idx + 1) % GLC_MAX_TX_PACKETS)*sizeof(struct glc_dmadesc);
		sc->sc_txdmadesc[idx].cmd_stat |= GELIC_CMDSTAT_NOIPSEC;

		if (i+1 == nsegs) {
			txs->txs_lastdesc = sc->next_txdma_slot;
			sc->sc_txdmadesc[idx].next = 0;
			sc->sc_txdmadesc[idx].cmd_stat |= GELIC_CMDSTAT_LAST;
		}

		sc->sc_txdmadesc[idx].cmd_stat |= GELIC_DESCR_OWNED;

		idx = (idx + 1) % GLC_MAX_TX_PACKETS;
	}
	sc->next_txdma_slot = idx;

	bus_dmamap_sync(sc->sc_txdma_tag, txs->txs_dmamap,
	    BUS_DMASYNC_PREWRITE);

	idx = (txs->txs_firstdesc - 1) % GLC_MAX_TX_PACKETS;
	sc->sc_txdmadesc[idx].next = firstslotphys;

	bus_dmamap_sync(sc->sc_txdma_tag, txs->txs_dmamap,
	    BUS_DMASYNC_PREWRITE);

	if (pktdesc != NULL)
		*pktdesc = firstslotphys;

	return (0);
}

static void
glc_rxintr(struct glc_softc *sc)
{
	int i, restart_rxdma;
	struct mbuf *m;
	struct ifnet *ifp = sc->sc_ifp;

	bus_dmamap_sync(sc->sc_dmadesc_tag, sc->sc_rxdmadesc_map,
	    BUS_DMASYNC_PREWRITE);

	restart_rxdma = 0;
	while ((sc->sc_rxdmadesc[sc->sc_next_rxdma_slot].cmd_stat &
	   GELIC_DESCR_OWNED) == 0) {
		i = sc->sc_next_rxdma_slot;
		if (sc->sc_rxdmadesc[i].rxerror & GELIC_RXERRORS) {
			ifp->if_ierrors++;
			goto requeue;
		}

		m = sc->sc_rxsoft[i].rxs_mbuf;
		if (glc_add_rxbuf(sc, i)) {
			ifp->if_ierrors++;
			goto requeue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_len = sc->sc_rxdmadesc[i].valid_size;
		m->m_pkthdr.len = m->m_len;
		sc->sc_next_rxdma_slot++;
		if (sc->sc_next_rxdma_slot >= GLC_MAX_RX_PACKETS)
			sc->sc_next_rxdma_slot = 0;

		if (sc->sc_rx_vlan >= 0)
			m_adj(m, 2);

		mtx_unlock(&sc->sc_mtx);
		(*ifp->if_input)(ifp, m);
		mtx_lock(&sc->sc_mtx);

	    requeue:
		if (sc->sc_rxdmadesc[i].cmd_stat & GELIC_CMDSTAT_RX_END)
			restart_rxdma = 1;
		glc_add_rxbuf_dma(sc, i);	
		if (restart_rxdma)
			lv1_net_start_rx_dma(sc->sc_bus, sc->sc_dev,
			    sc->sc_rxsoft[i].rxs_desc, 0);
	}
}

static void
glc_txintr(struct glc_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct glc_txsoft *txs;
	int progress = 0;

	while ((txs = STAILQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		if (sc->sc_txdmadesc[txs->txs_lastdesc].cmd_stat
		    != GELIC_CMDSTAT_DMA_DONE)
			break;

		STAILQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		bus_dmamap_unload(sc->sc_txdma_tag, txs->txs_dmamap);

		if (txs->txs_mbuf != NULL) {
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		ifp->if_opackets++;
		progress = 1;
	}

	if (progress) {
		/*
		 * We freed some descriptors, so reset IFF_DRV_OACTIVE
		 * and restart.
		 */
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#if 0
		sc->sc_wdog_timer = STAILQ_EMPTY(&sc->sc_txdirtyq) ? 0 : 5;
#endif

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			glc_start_locked(ifp);
	}
}

static void
glc_intr(void *xsc)
{
	struct glc_softc *sc = xsc; 

	mtx_lock(&sc->sc_mtx);

	if (*sc->sc_interrupt_status == 0) {
		device_printf(sc->sc_self, "stray interrupt!\n");
		mtx_unlock(&sc->sc_mtx);
		return;
	}

	if (*sc->sc_interrupt_status & (GELIC_INT_RXDONE | GELIC_INT_RXFRAME))
		glc_rxintr(sc);

	if (*sc->sc_interrupt_status & GELIC_INT_TXDONE)
		glc_txintr(sc);

	mtx_unlock(&sc->sc_mtx);
}
