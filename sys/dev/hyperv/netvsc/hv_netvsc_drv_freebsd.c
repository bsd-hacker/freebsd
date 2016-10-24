/*-
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2004-2006 Kip Macy
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

#include "opt_inet6.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/buf_ring.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/rndis.h>
#include <net/bpf.h>

#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/frame.h>

#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <machine/atomic.h>

#include <machine/intr_machdep.h>

#include <machine/in_cksum.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/include/vmbus_xact.h>

#include <dev/hyperv/netvsc/hv_net_vsc.h>
#include <dev/hyperv/netvsc/hv_rndis_filter.h>
#include <dev/hyperv/netvsc/ndis.h>

#include "vmbus_if.h"

/* Short for Hyper-V network interface */
#define NETVSC_DEVNAME    "hn"

/*
 * It looks like offset 0 of buf is reserved to hold the softc pointer.
 * The sc pointer evidently not needed, and is not presently populated.
 * The packet offset is where the netvsc_packet starts in the buffer.
 */
#define HV_NV_SC_PTR_OFFSET_IN_BUF         0
#define HV_NV_PACKET_OFFSET_IN_BUF         16

/* YYY should get it from the underlying channel */
#define HN_TX_DESC_CNT			512

#define HN_LROENT_CNT_DEF		128

#define HN_RING_CNT_DEF_MAX		8

#define HN_RNDIS_PKT_LEN					\
	(sizeof(struct rndis_packet_msg) +			\
	 HN_RNDIS_PKTINFO_SIZE(HN_NDIS_HASH_VALUE_SIZE) +	\
	 HN_RNDIS_PKTINFO_SIZE(NDIS_VLAN_INFO_SIZE) +		\
	 HN_RNDIS_PKTINFO_SIZE(NDIS_LSO2_INFO_SIZE) +		\
	 HN_RNDIS_PKTINFO_SIZE(NDIS_TXCSUM_INFO_SIZE))
#define HN_RNDIS_PKT_BOUNDARY		PAGE_SIZE
#define HN_RNDIS_PKT_ALIGN		CACHE_LINE_SIZE

#define HN_TX_DATA_BOUNDARY		PAGE_SIZE
#define HN_TX_DATA_MAXSIZE		IP_MAXPACKET
#define HN_TX_DATA_SEGSIZE		PAGE_SIZE
/* -1 for RNDIS packet message */
#define HN_TX_DATA_SEGCNT_MAX		(NETVSC_PACKET_MAXPAGE - 1)

#define HN_DIRECT_TX_SIZE_DEF		128

#define HN_EARLY_TXEOF_THRESH		8

struct hn_txdesc {
#ifndef HN_USE_TXDESC_BUFRING
	SLIST_ENTRY(hn_txdesc) link;
#endif
	struct mbuf	*m;
	struct hn_tx_ring *txr;
	int		refs;
	uint32_t	flags;		/* HN_TXD_FLAG_ */
	struct hn_send_ctx send_ctx;
	uint32_t	chim_index;
	int		chim_size;

	bus_dmamap_t	data_dmap;

	bus_addr_t	rndis_pkt_paddr;
	struct rndis_packet_msg *rndis_pkt;
	bus_dmamap_t	rndis_pkt_dmap;
};

#define HN_TXD_FLAG_ONLIST	0x1
#define HN_TXD_FLAG_DMAMAP	0x2

#define HN_LRO_LENLIM_MULTIRX_DEF	(12 * ETHERMTU)
#define HN_LRO_LENLIM_DEF		(25 * ETHERMTU)
/* YYY 2*MTU is a bit rough, but should be good enough. */
#define HN_LRO_LENLIM_MIN(ifp)		(2 * (ifp)->if_mtu)

#define HN_LRO_ACKCNT_DEF		1

#define HN_LOCK_INIT(sc)		\
	sx_init(&(sc)->hn_lock, device_get_nameunit((sc)->hn_dev))
#define HN_LOCK_ASSERT(sc)		sx_assert(&(sc)->hn_lock, SA_XLOCKED)
#define HN_LOCK_DESTROY(sc)		sx_destroy(&(sc)->hn_lock)
#define HN_LOCK(sc)			sx_xlock(&(sc)->hn_lock)
#define HN_UNLOCK(sc)			sx_xunlock(&(sc)->hn_lock)

#define HN_CSUM_IP_MASK			(CSUM_IP | CSUM_IP_TCP | CSUM_IP_UDP)
#define HN_CSUM_IP6_MASK		(CSUM_IP6_TCP | CSUM_IP6_UDP)
#define HN_CSUM_IP_HWASSIST(sc)		\
	((sc)->hn_tx_ring[0].hn_csum_assist & HN_CSUM_IP_MASK)
#define HN_CSUM_IP6_HWASSIST(sc)	\
	((sc)->hn_tx_ring[0].hn_csum_assist & HN_CSUM_IP6_MASK)

/*
 * Globals
 */

SYSCTL_NODE(_hw, OID_AUTO, hn, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "Hyper-V network interface");

/* Trust tcp segements verification on host side. */
static int hn_trust_hosttcp = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hosttcp, CTLFLAG_RDTUN,
    &hn_trust_hosttcp, 0,
    "Trust tcp segement verification on host side, "
    "when csum info is missing (global setting)");

/* Trust udp datagrams verification on host side. */
static int hn_trust_hostudp = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hostudp, CTLFLAG_RDTUN,
    &hn_trust_hostudp, 0,
    "Trust udp datagram verification on host side, "
    "when csum info is missing (global setting)");

/* Trust ip packets verification on host side. */
static int hn_trust_hostip = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hostip, CTLFLAG_RDTUN,
    &hn_trust_hostip, 0,
    "Trust ip packet verification on host side, "
    "when csum info is missing (global setting)");

/* Limit TSO burst size */
static int hn_tso_maxlen = IP_MAXPACKET;
SYSCTL_INT(_hw_hn, OID_AUTO, tso_maxlen, CTLFLAG_RDTUN,
    &hn_tso_maxlen, 0, "TSO burst limit");

/* Limit chimney send size */
static int hn_tx_chimney_size = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, tx_chimney_size, CTLFLAG_RDTUN,
    &hn_tx_chimney_size, 0, "Chimney send packet size limit");

/* Limit the size of packet for direct transmission */
static int hn_direct_tx_size = HN_DIRECT_TX_SIZE_DEF;
SYSCTL_INT(_hw_hn, OID_AUTO, direct_tx_size, CTLFLAG_RDTUN,
    &hn_direct_tx_size, 0, "Size of the packet for direct transmission");

#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
static int hn_lro_entry_count = HN_LROENT_CNT_DEF;
SYSCTL_INT(_hw_hn, OID_AUTO, lro_entry_count, CTLFLAG_RDTUN,
    &hn_lro_entry_count, 0, "LRO entry count");
#endif
#endif

static int hn_share_tx_taskq = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, share_tx_taskq, CTLFLAG_RDTUN,
    &hn_share_tx_taskq, 0, "Enable shared TX taskqueue");

static struct taskqueue	*hn_tx_taskq;

#ifndef HN_USE_TXDESC_BUFRING
static int hn_use_txdesc_bufring = 0;
#else
static int hn_use_txdesc_bufring = 1;
#endif
SYSCTL_INT(_hw_hn, OID_AUTO, use_txdesc_bufring, CTLFLAG_RD,
    &hn_use_txdesc_bufring, 0, "Use buf_ring for TX descriptors");

static int hn_bind_tx_taskq = -1;
SYSCTL_INT(_hw_hn, OID_AUTO, bind_tx_taskq, CTLFLAG_RDTUN,
    &hn_bind_tx_taskq, 0, "Bind TX taskqueue to the specified cpu");

static int hn_use_if_start = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, use_if_start, CTLFLAG_RDTUN,
    &hn_use_if_start, 0, "Use if_start TX method");

static int hn_chan_cnt = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, chan_cnt, CTLFLAG_RDTUN,
    &hn_chan_cnt, 0,
    "# of channels to use; each channel has one RX ring and one TX ring");

static int hn_tx_ring_cnt = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, tx_ring_cnt, CTLFLAG_RDTUN,
    &hn_tx_ring_cnt, 0, "# of TX rings to use");

static int hn_tx_swq_depth = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, tx_swq_depth, CTLFLAG_RDTUN,
    &hn_tx_swq_depth, 0, "Depth of IFQ or BUFRING");

#if __FreeBSD_version >= 1100095
static u_int hn_lro_mbufq_depth = 0;
SYSCTL_UINT(_hw_hn, OID_AUTO, lro_mbufq_depth, CTLFLAG_RDTUN,
    &hn_lro_mbufq_depth, 0, "Depth of LRO mbuf queue");
#endif

static u_int hn_cpu_index;

/*
 * Forward declarations
 */
static void hn_stop(struct hn_softc *sc);
static void hn_init_locked(struct hn_softc *sc);
static void hn_init(void *xsc);
static int  hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int hn_start_locked(struct hn_tx_ring *txr, int len);
static void hn_start(struct ifnet *ifp);
static void hn_start_txeof(struct hn_tx_ring *);
static int hn_ifmedia_upd(struct ifnet *ifp);
static void hn_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
#if __FreeBSD_version >= 1100099
static int hn_lro_lenlim_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_lro_ackcnt_sysctl(SYSCTL_HANDLER_ARGS);
#endif
static int hn_trust_hcsum_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_chim_size_sysctl(SYSCTL_HANDLER_ARGS);
#if __FreeBSD_version < 1100095
static int hn_rx_stat_int_sysctl(SYSCTL_HANDLER_ARGS);
#else
static int hn_rx_stat_u64_sysctl(SYSCTL_HANDLER_ARGS);
#endif
static int hn_rx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_tx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_tx_conf_int_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_ndis_version_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_caps_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_hwassist_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_rxfilter_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_rss_key_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_rss_ind_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_check_iplen(const struct mbuf *, int);
static int hn_create_tx_ring(struct hn_softc *, int);
static void hn_destroy_tx_ring(struct hn_tx_ring *);
static int hn_create_tx_data(struct hn_softc *, int);
static void hn_fixup_tx_data(struct hn_softc *);
static void hn_destroy_tx_data(struct hn_softc *);
static void hn_start_taskfunc(void *, int);
static void hn_start_txeof_taskfunc(void *, int);
static void hn_link_taskfunc(void *, int);
static void hn_netchg_init_taskfunc(void *, int);
static void hn_netchg_status_taskfunc(void *, int);
static void hn_suspend_mgmt_taskfunc(void *, int);
static int hn_encap(struct hn_tx_ring *, struct hn_txdesc *, struct mbuf **);
static int hn_create_rx_data(struct hn_softc *sc, int);
static void hn_destroy_rx_data(struct hn_softc *sc);
static void hn_set_chim_size(struct hn_softc *, int);
static void hn_set_tso_maxsize(struct hn_softc *, int, int);
static int hn_chan_attach(struct hn_softc *, struct vmbus_channel *);
static void hn_chan_detach(struct hn_softc *, struct vmbus_channel *);
static int hn_attach_subchans(struct hn_softc *);
static void hn_detach_allchans(struct hn_softc *);
static void hn_chan_callback(struct vmbus_channel *chan, void *xrxr);
static void hn_set_ring_inuse(struct hn_softc *, int);
static int hn_synth_attach(struct hn_softc *, int);
static void hn_synth_detach(struct hn_softc *);
static bool hn_tx_ring_pending(struct hn_tx_ring *);
static void hn_suspend(struct hn_softc *);
static void hn_suspend_data(struct hn_softc *);
static void hn_suspend_mgmt(struct hn_softc *);
static void hn_resume(struct hn_softc *);
static void hn_resume_data(struct hn_softc *);
static void hn_resume_mgmt(struct hn_softc *);
static void hn_rx_drain(struct vmbus_channel *);
static void hn_tx_resume(struct hn_softc *, int);
static void hn_tx_ring_qflush(struct hn_tx_ring *);
static int netvsc_detach(device_t dev);
static void hn_link_status(struct hn_softc *);
static int hn_sendpkt_rndis_sglist(struct hn_tx_ring *, struct hn_txdesc *);
static int hn_sendpkt_rndis_chim(struct hn_tx_ring *, struct hn_txdesc *);
static int hn_set_rxfilter(struct hn_softc *);

static void hn_nvs_handle_notify(struct hn_softc *sc,
		const struct vmbus_chanpkt_hdr *pkt);
static void hn_nvs_handle_comp(struct hn_softc *sc, struct vmbus_channel *chan,
		const struct vmbus_chanpkt_hdr *pkt);
static void hn_nvs_handle_rxbuf(struct hn_softc *sc, struct hn_rx_ring *rxr,
		struct vmbus_channel *chan,
		const struct vmbus_chanpkt_hdr *pkthdr);
static void hn_nvs_ack_rxbuf(struct vmbus_channel *chan, uint64_t tid);

static int hn_transmit(struct ifnet *, struct mbuf *);
static void hn_xmit_qflush(struct ifnet *);
static int hn_xmit(struct hn_tx_ring *, int);
static void hn_xmit_txeof(struct hn_tx_ring *);
static void hn_xmit_taskfunc(void *, int);
static void hn_xmit_txeof_taskfunc(void *, int);

static const uint8_t	hn_rss_key_default[NDIS_HASH_KEYSIZE_TOEPLITZ] = {
	0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
	0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
	0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
	0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
	0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
};

#if __FreeBSD_version >= 1100099
static void
hn_set_lro_lenlim(struct hn_softc *sc, int lenlim)
{
	int i;

	for (i = 0; i < sc->hn_rx_ring_inuse; ++i)
		sc->hn_rx_ring[i].hn_lro.lro_length_lim = lenlim;
}
#endif

static __inline int
hn_nvs_send_rndis_sglist1(struct vmbus_channel *chan, uint32_t rndis_mtype,
    struct hn_send_ctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt)
{
	struct hn_nvs_rndis rndis;

	rndis.nvs_type = HN_NVS_TYPE_RNDIS;
	rndis.nvs_rndis_mtype = rndis_mtype;
	rndis.nvs_chim_idx = HN_NVS_CHIM_IDX_INVALID;
	rndis.nvs_chim_sz = 0;

	return (hn_nvs_send_sglist(chan, gpa, gpa_cnt,
	    &rndis, sizeof(rndis), sndc));
}

int
hn_nvs_send_rndis_ctrl(struct vmbus_channel *chan,
    struct hn_send_ctx *sndc, struct vmbus_gpa *gpa, int gpa_cnt)
{

	return hn_nvs_send_rndis_sglist1(chan, HN_NVS_RNDIS_MTYPE_CTRL,
	    sndc, gpa, gpa_cnt);
}

static int
hn_sendpkt_rndis_sglist(struct hn_tx_ring *txr, struct hn_txdesc *txd)
{

	KASSERT(txd->chim_index == HN_NVS_CHIM_IDX_INVALID &&
	    txd->chim_size == 0, ("invalid rndis sglist txd"));
	return (hn_nvs_send_rndis_sglist1(txr->hn_chan, HN_NVS_RNDIS_MTYPE_DATA,
	    &txd->send_ctx, txr->hn_gpa, txr->hn_gpa_cnt));
}

static int
hn_sendpkt_rndis_chim(struct hn_tx_ring *txr, struct hn_txdesc *txd)
{
	struct hn_nvs_rndis rndis;

	KASSERT(txd->chim_index != HN_NVS_CHIM_IDX_INVALID &&
	    txd->chim_size > 0, ("invalid rndis chim txd"));

	rndis.nvs_type = HN_NVS_TYPE_RNDIS;
	rndis.nvs_rndis_mtype = HN_NVS_RNDIS_MTYPE_DATA;
	rndis.nvs_chim_idx = txd->chim_index;
	rndis.nvs_chim_sz = txd->chim_size;

	return (hn_nvs_send(txr->hn_chan, VMBUS_CHANPKT_FLAG_RC,
	    &rndis, sizeof(rndis), &txd->send_ctx));
}

static int
hn_set_rxfilter(struct hn_softc *sc)
{
	struct ifnet *ifp = sc->hn_ifp;
	uint32_t filter;
	int error = 0;

	HN_LOCK_ASSERT(sc);

	if (ifp->if_flags & IFF_PROMISC) {
		filter = NDIS_PACKET_TYPE_PROMISCUOUS;
	} else {
		filter = NDIS_PACKET_TYPE_DIRECTED;
		if (ifp->if_flags & IFF_BROADCAST)
			filter |= NDIS_PACKET_TYPE_BROADCAST;
#ifdef notyet
		/*
		 * See the comment in SIOCADDMULTI/SIOCDELMULTI.
		 */
		/* TODO: support multicast list */
		if ((ifp->if_flags & IFF_ALLMULTI) ||
		    !TAILQ_EMPTY(&ifp->if_multiaddrs))
			filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
#else
		/* Always enable ALLMULTI */
		filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
#endif
	}

	if (sc->hn_rx_filter != filter) {
		error = hn_rndis_set_rxfilter(sc, filter);
		if (!error)
			sc->hn_rx_filter = filter;
	}
	return (error);
}

static int
hn_get_txswq_depth(const struct hn_tx_ring *txr)
{

	KASSERT(txr->hn_txdesc_cnt > 0, ("tx ring is not setup yet"));
	if (hn_tx_swq_depth < txr->hn_txdesc_cnt)
		return txr->hn_txdesc_cnt;
	return hn_tx_swq_depth;
}

static int
hn_rss_reconfig(struct hn_softc *sc)
{
	int error;

	HN_LOCK_ASSERT(sc);

	if ((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0)
		return (ENXIO);

	/*
	 * Disable RSS first.
	 *
	 * NOTE:
	 * Direct reconfiguration by setting the UNCHG flags does
	 * _not_ work properly.
	 */
	if (bootverbose)
		if_printf(sc->hn_ifp, "disable RSS\n");
	error = hn_rndis_conf_rss(sc, NDIS_RSS_FLAG_DISABLE);
	if (error) {
		if_printf(sc->hn_ifp, "RSS disable failed\n");
		return (error);
	}

	/*
	 * Reenable the RSS w/ the updated RSS key or indirect
	 * table.
	 */
	if (bootverbose)
		if_printf(sc->hn_ifp, "reconfig RSS\n");
	error = hn_rndis_conf_rss(sc, NDIS_RSS_FLAG_NONE);
	if (error) {
		if_printf(sc->hn_ifp, "RSS reconfig failed\n");
		return (error);
	}
	return (0);
}

static void
hn_rss_ind_fixup(struct hn_softc *sc, int nchan)
{
	struct ndis_rssprm_toeplitz *rss = &sc->hn_rss;
	int i;

	KASSERT(nchan > 1, ("invalid # of channels %d", nchan));

	/*
	 * Check indirect table to make sure that all channels in it
	 * can be used.
	 */
	for (i = 0; i < NDIS_HASH_INDCNT; ++i) {
		if (rss->rss_ind[i] >= nchan) {
			if_printf(sc->hn_ifp,
			    "RSS indirect table %d fixup: %u -> %d\n",
			    i, rss->rss_ind[i], nchan - 1);
			rss->rss_ind[i] = nchan - 1;
		}
	}
}

static int
hn_ifmedia_upd(struct ifnet *ifp __unused)
{

	return EOPNOTSUPP;
}

static void
hn_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct hn_softc *sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if ((sc->hn_link_flags & HN_LINK_FLAG_LINKUP) == 0) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}
	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_10G_T | IFM_FDX;
}

/* {F8615163-DF3E-46c5-913F-F2D2F965ED0E} */
static const struct hyperv_guid g_net_vsc_device_type = {
	.hv_guid = {0x63, 0x51, 0x61, 0xF8, 0x3E, 0xDF, 0xc5, 0x46,
		0x91, 0x3F, 0xF2, 0xD2, 0xF9, 0x65, 0xED, 0x0E}
};

/*
 * Standard probe entry point.
 *
 */
static int
netvsc_probe(device_t dev)
{
	if (VMBUS_PROBE_GUID(device_get_parent(dev), dev,
	    &g_net_vsc_device_type) == 0) {
		device_set_desc(dev, "Hyper-V Network Interface");
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

/*
 * Standard attach entry point.
 *
 * Called when the driver is loaded.  It allocates needed resources,
 * and initializes the "hardware" and software.
 */
static int
netvsc_attach(device_t dev)
{
	struct hn_softc *sc = device_get_softc(dev);
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct ifnet *ifp = NULL;
	int error, ring_cnt, tx_ring_cnt;

	sc->hn_dev = dev;
	sc->hn_prichan = vmbus_get_channel(dev);
	HN_LOCK_INIT(sc);

	/*
	 * Setup taskqueue for transmission.
	 */
	if (hn_tx_taskq == NULL) {
		sc->hn_tx_taskq = taskqueue_create("hn_tx", M_WAITOK,
		    taskqueue_thread_enqueue, &sc->hn_tx_taskq);
		if (hn_bind_tx_taskq >= 0) {
			int cpu = hn_bind_tx_taskq;
			cpuset_t cpu_set;

			if (cpu > mp_ncpus - 1)
				cpu = mp_ncpus - 1;
			CPU_SETOF(cpu, &cpu_set);
			taskqueue_start_threads_cpuset(&sc->hn_tx_taskq, 1,
			    PI_NET, &cpu_set, "%s tx",
			    device_get_nameunit(dev));
		} else {
			taskqueue_start_threads(&sc->hn_tx_taskq, 1, PI_NET,
			    "%s tx", device_get_nameunit(dev));
		}
	} else {
		sc->hn_tx_taskq = hn_tx_taskq;
	}

	/*
	 * Setup taskqueue for mangement tasks, e.g. link status.
	 */
	sc->hn_mgmt_taskq0 = taskqueue_create("hn_mgmt", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->hn_mgmt_taskq0);
	taskqueue_start_threads(&sc->hn_mgmt_taskq0, 1, PI_NET, "%s mgmt",
	    device_get_nameunit(dev));
	TASK_INIT(&sc->hn_link_task, 0, hn_link_taskfunc, sc);
	TASK_INIT(&sc->hn_netchg_init, 0, hn_netchg_init_taskfunc, sc);
	TIMEOUT_TASK_INIT(sc->hn_mgmt_taskq0, &sc->hn_netchg_status, 0,
	    hn_netchg_status_taskfunc, sc);

	/*
	 * Allocate ifnet and setup its name earlier, so that if_printf
	 * can be used by functions, which will be called after
	 * ether_ifattach().
	 */
	ifp = sc->hn_ifp = if_alloc(IFT_ETHER);
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	/*
	 * Initialize ifmedia earlier so that it can be unconditionally
	 * destroyed, if error happened later on.
	 */
	ifmedia_init(&sc->hn_media, 0, hn_ifmedia_upd, hn_ifmedia_sts);

	/*
	 * Figure out the # of RX rings (ring_cnt) and the # of TX rings
	 * to use (tx_ring_cnt).
	 *
	 * NOTE:
	 * The # of RX rings to use is same as the # of channels to use.
	 */
	ring_cnt = hn_chan_cnt;
	if (ring_cnt <= 0) {
		/* Default */
		ring_cnt = mp_ncpus;
		if (ring_cnt > HN_RING_CNT_DEF_MAX)
			ring_cnt = HN_RING_CNT_DEF_MAX;
	} else if (ring_cnt > mp_ncpus) {
		ring_cnt = mp_ncpus;
	}

	tx_ring_cnt = hn_tx_ring_cnt;
	if (tx_ring_cnt <= 0 || tx_ring_cnt > ring_cnt)
		tx_ring_cnt = ring_cnt;
	if (hn_use_if_start) {
		/* ifnet.if_start only needs one TX ring. */
		tx_ring_cnt = 1;
	}

	/*
	 * Set the leader CPU for channels.
	 */
	sc->hn_cpu = atomic_fetchadd_int(&hn_cpu_index, ring_cnt) % mp_ncpus;

	/*
	 * Create enough TX/RX rings, even if only limited number of
	 * channels can be allocated.
	 */
	error = hn_create_tx_data(sc, tx_ring_cnt);
	if (error)
		goto failed;
	error = hn_create_rx_data(sc, ring_cnt);
	if (error)
		goto failed;

	/*
	 * Create transaction context for NVS and RNDIS transactions.
	 */
	sc->hn_xact = vmbus_xact_ctx_create(bus_get_dma_tag(dev),
	    HN_XACT_REQ_SIZE, HN_XACT_RESP_SIZE, 0);
	if (sc->hn_xact == NULL)
		goto failed;

	/*
	 * Attach the synthetic parts, i.e. NVS and RNDIS.
	 */
	error = hn_synth_attach(sc, ETHERMTU);
	if (error)
		goto failed;

	error = hn_rndis_get_eaddr(sc, eaddr);
	if (error)
		goto failed;

#if __FreeBSD_version >= 1100099
	if (sc->hn_rx_ring_inuse > 1) {
		/*
		 * Reduce TCP segment aggregation limit for multiple
		 * RX rings to increase ACK timeliness.
		 */
		hn_set_lro_lenlim(sc, HN_LRO_LENLIM_MULTIRX_DEF);
	}
#endif

	/*
	 * Fixup TX stuffs after synthetic parts are attached.
	 */
	hn_fixup_tx_data(sc);

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "nvs_version", CTLFLAG_RD,
	    &sc->hn_nvs_ver, 0, "NVS version");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "ndis_version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    hn_ndis_version_sysctl, "A", "NDIS version");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "caps",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    hn_caps_sysctl, "A", "capabilities");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "hwassist",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    hn_hwassist_sysctl, "A", "hwassist");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rxfilter",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    hn_rxfilter_sysctl, "A", "rxfilter");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rss_key",
	    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    hn_rss_key_sysctl, "IU", "RSS key");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rss_ind",
	    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    hn_rss_ind_sysctl, "IU", "RSS indirect table");

	/*
	 * Setup the ifmedia, which has been initialized earlier.
	 */
	ifmedia_add(&sc->hn_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->hn_media, IFM_ETHER | IFM_AUTO);
	/* XXX ifmedia_set really should do this for us */
	sc->hn_media.ifm_media = sc->hn_media.ifm_cur->ifm_media;

	/*
	 * Setup the ifnet for this interface.
	 */

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = hn_ioctl;
	ifp->if_init = hn_init;
	if (hn_use_if_start) {
		int qdepth = hn_get_txswq_depth(&sc->hn_tx_ring[0]);

		ifp->if_start = hn_start;
		IFQ_SET_MAXLEN(&ifp->if_snd, qdepth);
		ifp->if_snd.ifq_drv_maxlen = qdepth - 1;
		IFQ_SET_READY(&ifp->if_snd);
	} else {
		ifp->if_transmit = hn_transmit;
		ifp->if_qflush = hn_xmit_qflush;
	}

	ifp->if_capabilities |= IFCAP_RXCSUM | IFCAP_LRO;
#ifdef foo
	/* We can't diff IPv6 packets from IPv4 packets on RX path. */
	ifp->if_capabilities |= IFCAP_RXCSUM_IPV6;
#endif
	if (sc->hn_caps & HN_CAP_VLAN) {
		/* XXX not sure about VLAN_MTU. */
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	}

	ifp->if_hwassist = sc->hn_tx_ring[0].hn_csum_assist;
	if (ifp->if_hwassist & HN_CSUM_IP_MASK)
		ifp->if_capabilities |= IFCAP_TXCSUM;
	if (ifp->if_hwassist & HN_CSUM_IP6_MASK)
		ifp->if_capabilities |= IFCAP_TXCSUM_IPV6;
	if (sc->hn_caps & HN_CAP_TSO4) {
		ifp->if_capabilities |= IFCAP_TSO4;
		ifp->if_hwassist |= CSUM_IP_TSO;
	}
	if (sc->hn_caps & HN_CAP_TSO6) {
		ifp->if_capabilities |= IFCAP_TSO6;
		ifp->if_hwassist |= CSUM_IP6_TSO;
	}

	/* Enable all available capabilities by default. */
	ifp->if_capenable = ifp->if_capabilities;

	if (ifp->if_capabilities & (IFCAP_TSO6 | IFCAP_TSO4)) {
		hn_set_tso_maxsize(sc, hn_tso_maxlen, ETHERMTU);
		ifp->if_hw_tsomaxsegcount = HN_TX_DATA_SEGCNT_MAX;
		ifp->if_hw_tsomaxsegsize = PAGE_SIZE;
	}

	ether_ifattach(ifp, eaddr);

	if ((ifp->if_capabilities & (IFCAP_TSO6 | IFCAP_TSO4)) && bootverbose) {
		if_printf(ifp, "TSO segcnt %u segsz %u\n",
		    ifp->if_hw_tsomaxsegcount, ifp->if_hw_tsomaxsegsize);
	}

	/* Inform the upper layer about the long frame support. */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	/*
	 * Kick off link status check.
	 */
	sc->hn_mgmt_taskq = sc->hn_mgmt_taskq0;
	hn_link_status_update(sc);

	return (0);
failed:
	if (sc->hn_flags & HN_FLAG_SYNTH_ATTACHED)
		hn_synth_detach(sc);
	netvsc_detach(dev);
	return (error);
}

static int
netvsc_detach(device_t dev)
{
	struct hn_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->hn_ifp;

	if (device_is_attached(dev)) {
		HN_LOCK(sc);
		if (sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				hn_stop(sc);
			/*
			 * NOTE:
			 * hn_stop() only suspends data, so managment
			 * stuffs have to be suspended manually here.
			 */
			hn_suspend_mgmt(sc);
			hn_synth_detach(sc);
		}
		HN_UNLOCK(sc);
		ether_ifdetach(ifp);
	}

	ifmedia_removeall(&sc->hn_media);
	hn_destroy_rx_data(sc);
	hn_destroy_tx_data(sc);

	if (sc->hn_tx_taskq != hn_tx_taskq)
		taskqueue_free(sc->hn_tx_taskq);
	taskqueue_free(sc->hn_mgmt_taskq0);

	if (sc->hn_xact != NULL)
		vmbus_xact_ctx_destroy(sc->hn_xact);

	if_free(ifp);

	HN_LOCK_DESTROY(sc);
	return (0);
}

/*
 * Standard shutdown entry point
 */
static int
netvsc_shutdown(device_t dev)
{
	return (0);
}

static void
hn_link_status(struct hn_softc *sc)
{
	uint32_t link_status;
	int error;

	error = hn_rndis_get_linkstatus(sc, &link_status);
	if (error) {
		/* XXX what to do? */
		return;
	}

	if (link_status == NDIS_MEDIA_STATE_CONNECTED)
		sc->hn_link_flags |= HN_LINK_FLAG_LINKUP;
	else
		sc->hn_link_flags &= ~HN_LINK_FLAG_LINKUP;
	if_link_state_change(sc->hn_ifp,
	    (sc->hn_link_flags & HN_LINK_FLAG_LINKUP) ?
	    LINK_STATE_UP : LINK_STATE_DOWN);
}

static void
hn_link_taskfunc(void *xsc, int pending __unused)
{
	struct hn_softc *sc = xsc;

	if (sc->hn_link_flags & HN_LINK_FLAG_NETCHG)
		return;
	hn_link_status(sc);
}

static void
hn_netchg_init_taskfunc(void *xsc, int pending __unused)
{
	struct hn_softc *sc = xsc;

	/* Prevent any link status checks from running. */
	sc->hn_link_flags |= HN_LINK_FLAG_NETCHG;

	/*
	 * Fake up a [link down --> link up] state change; 5 seconds
	 * delay is used, which closely simulates miibus reaction
	 * upon link down event.
	 */
	sc->hn_link_flags &= ~HN_LINK_FLAG_LINKUP;
	if_link_state_change(sc->hn_ifp, LINK_STATE_DOWN);
	taskqueue_enqueue_timeout(sc->hn_mgmt_taskq0,
	    &sc->hn_netchg_status, 5 * hz);
}

static void
hn_netchg_status_taskfunc(void *xsc, int pending __unused)
{
	struct hn_softc *sc = xsc;

	/* Re-allow link status checks. */
	sc->hn_link_flags &= ~HN_LINK_FLAG_NETCHG;
	hn_link_status(sc);
}

void
hn_link_status_update(struct hn_softc *sc)
{

	if (sc->hn_mgmt_taskq != NULL)
		taskqueue_enqueue(sc->hn_mgmt_taskq, &sc->hn_link_task);
}

void
hn_network_change(struct hn_softc *sc)
{

	if (sc->hn_mgmt_taskq != NULL)
		taskqueue_enqueue(sc->hn_mgmt_taskq, &sc->hn_netchg_init);
}

static __inline int
hn_txdesc_dmamap_load(struct hn_tx_ring *txr, struct hn_txdesc *txd,
    struct mbuf **m_head, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *m = *m_head;
	int error;

	KASSERT(txd->chim_index == HN_NVS_CHIM_IDX_INVALID, ("txd uses chim"));

	error = bus_dmamap_load_mbuf_sg(txr->hn_tx_data_dtag, txd->data_dmap,
	    m, segs, nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		struct mbuf *m_new;

		m_new = m_collapse(m, M_NOWAIT, HN_TX_DATA_SEGCNT_MAX);
		if (m_new == NULL)
			return ENOBUFS;
		else
			*m_head = m = m_new;
		txr->hn_tx_collapsed++;

		error = bus_dmamap_load_mbuf_sg(txr->hn_tx_data_dtag,
		    txd->data_dmap, m, segs, nsegs, BUS_DMA_NOWAIT);
	}
	if (!error) {
		bus_dmamap_sync(txr->hn_tx_data_dtag, txd->data_dmap,
		    BUS_DMASYNC_PREWRITE);
		txd->flags |= HN_TXD_FLAG_DMAMAP;
	}
	return error;
}

static __inline int
hn_txdesc_put(struct hn_tx_ring *txr, struct hn_txdesc *txd)
{

	KASSERT((txd->flags & HN_TXD_FLAG_ONLIST) == 0,
	    ("put an onlist txd %#x", txd->flags));

	KASSERT(txd->refs > 0, ("invalid txd refs %d", txd->refs));
	if (atomic_fetchadd_int(&txd->refs, -1) != 1)
		return 0;

	if (txd->chim_index != HN_NVS_CHIM_IDX_INVALID) {
		KASSERT((txd->flags & HN_TXD_FLAG_DMAMAP) == 0,
		    ("chim txd uses dmamap"));
		hn_chim_free(txr->hn_sc, txd->chim_index);
		txd->chim_index = HN_NVS_CHIM_IDX_INVALID;
	} else if (txd->flags & HN_TXD_FLAG_DMAMAP) {
		bus_dmamap_sync(txr->hn_tx_data_dtag,
		    txd->data_dmap, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(txr->hn_tx_data_dtag,
		    txd->data_dmap);
		txd->flags &= ~HN_TXD_FLAG_DMAMAP;
	}

	if (txd->m != NULL) {
		m_freem(txd->m);
		txd->m = NULL;
	}

	txd->flags |= HN_TXD_FLAG_ONLIST;
#ifndef HN_USE_TXDESC_BUFRING
	mtx_lock_spin(&txr->hn_txlist_spin);
	KASSERT(txr->hn_txdesc_avail >= 0 &&
	    txr->hn_txdesc_avail < txr->hn_txdesc_cnt,
	    ("txdesc_put: invalid txd avail %d", txr->hn_txdesc_avail));
	txr->hn_txdesc_avail++;
	SLIST_INSERT_HEAD(&txr->hn_txlist, txd, link);
	mtx_unlock_spin(&txr->hn_txlist_spin);
#else
	atomic_add_int(&txr->hn_txdesc_avail, 1);
	buf_ring_enqueue(txr->hn_txdesc_br, txd);
#endif

	return 1;
}

static __inline struct hn_txdesc *
hn_txdesc_get(struct hn_tx_ring *txr)
{
	struct hn_txdesc *txd;

#ifndef HN_USE_TXDESC_BUFRING
	mtx_lock_spin(&txr->hn_txlist_spin);
	txd = SLIST_FIRST(&txr->hn_txlist);
	if (txd != NULL) {
		KASSERT(txr->hn_txdesc_avail > 0,
		    ("txdesc_get: invalid txd avail %d", txr->hn_txdesc_avail));
		txr->hn_txdesc_avail--;
		SLIST_REMOVE_HEAD(&txr->hn_txlist, link);
	}
	mtx_unlock_spin(&txr->hn_txlist_spin);
#else
	txd = buf_ring_dequeue_sc(txr->hn_txdesc_br);
#endif

	if (txd != NULL) {
#ifdef HN_USE_TXDESC_BUFRING
		atomic_subtract_int(&txr->hn_txdesc_avail, 1);
#endif
		KASSERT(txd->m == NULL && txd->refs == 0 &&
		    txd->chim_index == HN_NVS_CHIM_IDX_INVALID &&
		    (txd->flags & HN_TXD_FLAG_ONLIST) &&
		    (txd->flags & HN_TXD_FLAG_DMAMAP) == 0, ("invalid txd"));
		txd->flags &= ~HN_TXD_FLAG_ONLIST;
		txd->refs = 1;
	}
	return txd;
}

static __inline void
hn_txdesc_hold(struct hn_txdesc *txd)
{

	/* 0->1 transition will never work */
	KASSERT(txd->refs > 0, ("invalid refs %d", txd->refs));
	atomic_add_int(&txd->refs, 1);
}

static bool
hn_tx_ring_pending(struct hn_tx_ring *txr)
{
	bool pending = false;

#ifndef HN_USE_TXDESC_BUFRING
	mtx_lock_spin(&txr->hn_txlist_spin);
	if (txr->hn_txdesc_avail != txr->hn_txdesc_cnt)
		pending = true;
	mtx_unlock_spin(&txr->hn_txlist_spin);
#else
	if (!buf_ring_full(txr->hn_txdesc_br))
		pending = true;
#endif
	return (pending);
}

static __inline void
hn_txeof(struct hn_tx_ring *txr)
{
	txr->hn_has_txeof = 0;
	txr->hn_txeof(txr);
}

static void
hn_tx_done(struct hn_send_ctx *sndc, struct hn_softc *sc,
    struct vmbus_channel *chan, const void *data __unused, int dlen __unused)
{
	struct hn_txdesc *txd = sndc->hn_cbarg;
	struct hn_tx_ring *txr;

	txr = txd->txr;
	KASSERT(txr->hn_chan == chan,
	    ("channel mismatch, on chan%u, should be chan%u",
	     vmbus_chan_subidx(chan), vmbus_chan_subidx(txr->hn_chan)));

	txr->hn_has_txeof = 1;
	hn_txdesc_put(txr, txd);

	++txr->hn_txdone_cnt;
	if (txr->hn_txdone_cnt >= HN_EARLY_TXEOF_THRESH) {
		txr->hn_txdone_cnt = 0;
		if (txr->hn_oactive)
			hn_txeof(txr);
	}
}

void
hn_chan_rollup(struct hn_rx_ring *rxr, struct hn_tx_ring *txr)
{
#if defined(INET) || defined(INET6)
	tcp_lro_flush_all(&rxr->hn_lro);
#endif

	/*
	 * NOTE:
	 * 'txr' could be NULL, if multiple channels and
	 * ifnet.if_start method are enabled.
	 */
	if (txr == NULL || !txr->hn_has_txeof)
		return;

	txr->hn_txdone_cnt = 0;
	hn_txeof(txr);
}

static __inline uint32_t
hn_rndis_pktmsg_offset(uint32_t ofs)
{

	KASSERT(ofs >= sizeof(struct rndis_packet_msg),
	    ("invalid RNDIS packet msg offset %u", ofs));
	return (ofs - __offsetof(struct rndis_packet_msg, rm_dataoffset));
}

/*
 * NOTE:
 * If this function fails, then both txd and m_head0 will be freed.
 */
static int
hn_encap(struct hn_tx_ring *txr, struct hn_txdesc *txd, struct mbuf **m_head0)
{
	bus_dma_segment_t segs[HN_TX_DATA_SEGCNT_MAX];
	int error, nsegs, i;
	struct mbuf *m_head = *m_head0;
	struct rndis_packet_msg *pkt;
	uint32_t *pi_data;
	int pktlen;

	/*
	 * extension points to the area reserved for the
	 * rndis_filter_packet, which is placed just after
	 * the netvsc_packet (and rppi struct, if present;
	 * length is updated later).
	 */
	pkt = txd->rndis_pkt;
	pkt->rm_type = REMOTE_NDIS_PACKET_MSG;
	pkt->rm_len = sizeof(*pkt) + m_head->m_pkthdr.len;
	pkt->rm_dataoffset = sizeof(*pkt);
	pkt->rm_datalen = m_head->m_pkthdr.len;
	pkt->rm_pktinfooffset = sizeof(*pkt);
	pkt->rm_pktinfolen = 0;

	if (txr->hn_tx_flags & HN_TX_FLAG_HASHVAL) {
		/*
		 * Set the hash value for this packet, so that the host could
		 * dispatch the TX done event for this packet back to this TX
		 * ring's channel.
		 */
		pi_data = hn_rndis_pktinfo_append(pkt, HN_RNDIS_PKT_LEN,
		    HN_NDIS_HASH_VALUE_SIZE, HN_NDIS_PKTINFO_TYPE_HASHVAL);
		*pi_data = txr->hn_tx_idx;
	}

	if (m_head->m_flags & M_VLANTAG) {
		pi_data = hn_rndis_pktinfo_append(pkt, HN_RNDIS_PKT_LEN,
		    NDIS_VLAN_INFO_SIZE, NDIS_PKTINFO_TYPE_VLAN);
		*pi_data = NDIS_VLAN_INFO_MAKE(
		    EVL_VLANOFTAG(m_head->m_pkthdr.ether_vtag),
		    EVL_PRIOFTAG(m_head->m_pkthdr.ether_vtag),
		    EVL_CFIOFTAG(m_head->m_pkthdr.ether_vtag));
	}

	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
#if defined(INET6) || defined(INET)
		struct ether_vlan_header *eh;
		int ether_len;

		/*
		 * XXX need m_pullup and use mtodo
		 */
		eh = mtod(m_head, struct ether_vlan_header*);
		if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN))
			ether_len = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		else
			ether_len = ETHER_HDR_LEN;

		pi_data = hn_rndis_pktinfo_append(pkt, HN_RNDIS_PKT_LEN,
		    NDIS_LSO2_INFO_SIZE, NDIS_PKTINFO_TYPE_LSO);
#ifdef INET
		if (m_head->m_pkthdr.csum_flags & CSUM_IP_TSO) {
			struct ip *ip =
			    (struct ip *)(m_head->m_data + ether_len);
			unsigned long iph_len = ip->ip_hl << 2;
			struct tcphdr *th =
			    (struct tcphdr *)((caddr_t)ip + iph_len);

			ip->ip_len = 0;
			ip->ip_sum = 0;
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
			*pi_data = NDIS_LSO2_INFO_MAKEIPV4(0,
			    m_head->m_pkthdr.tso_segsz);
		}
#endif
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET6
		{
			struct ip6_hdr *ip6 = (struct ip6_hdr *)
			    (m_head->m_data + ether_len);
			struct tcphdr *th = (struct tcphdr *)(ip6 + 1);

			ip6->ip6_plen = 0;
			th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
			*pi_data = NDIS_LSO2_INFO_MAKEIPV6(0,
			    m_head->m_pkthdr.tso_segsz);
		}
#endif
#endif	/* INET6 || INET */
	} else if (m_head->m_pkthdr.csum_flags & txr->hn_csum_assist) {
		pi_data = hn_rndis_pktinfo_append(pkt, HN_RNDIS_PKT_LEN,
		    NDIS_TXCSUM_INFO_SIZE, NDIS_PKTINFO_TYPE_CSUM);
		if (m_head->m_pkthdr.csum_flags &
		    (CSUM_IP6_TCP | CSUM_IP6_UDP)) {
			*pi_data = NDIS_TXCSUM_INFO_IPV6;
		} else {
			*pi_data = NDIS_TXCSUM_INFO_IPV4;
			if (m_head->m_pkthdr.csum_flags & CSUM_IP)
				*pi_data |= NDIS_TXCSUM_INFO_IPCS;
		}

		if (m_head->m_pkthdr.csum_flags & (CSUM_IP_TCP | CSUM_IP6_TCP))
			*pi_data |= NDIS_TXCSUM_INFO_TCPCS;
		else if (m_head->m_pkthdr.csum_flags &
		    (CSUM_IP_UDP | CSUM_IP6_UDP))
			*pi_data |= NDIS_TXCSUM_INFO_UDPCS;
	}

	pktlen = pkt->rm_pktinfooffset + pkt->rm_pktinfolen;
	/* Convert RNDIS packet message offsets */
	pkt->rm_dataoffset = hn_rndis_pktmsg_offset(pkt->rm_dataoffset);
	pkt->rm_pktinfooffset = hn_rndis_pktmsg_offset(pkt->rm_pktinfooffset);

	/*
	 * Chimney send, if the packet could fit into one chimney buffer.
	 */
	if (pkt->rm_len < txr->hn_chim_size) {
		txr->hn_tx_chimney_tried++;
		txd->chim_index = hn_chim_alloc(txr->hn_sc);
		if (txd->chim_index != HN_NVS_CHIM_IDX_INVALID) {
			uint8_t *dest = txr->hn_sc->hn_chim +
			    (txd->chim_index * txr->hn_sc->hn_chim_szmax);

			memcpy(dest, pkt, pktlen);
			dest += pktlen;
			m_copydata(m_head, 0, m_head->m_pkthdr.len, dest);

			txd->chim_size = pkt->rm_len;
			txr->hn_gpa_cnt = 0;
			txr->hn_tx_chimney++;
			txr->hn_sendpkt = hn_sendpkt_rndis_chim;
			goto done;
		}
	}

	error = hn_txdesc_dmamap_load(txr, txd, &m_head, segs, &nsegs);
	if (error) {
		int freed;

		/*
		 * This mbuf is not linked w/ the txd yet, so free it now.
		 */
		m_freem(m_head);
		*m_head0 = NULL;

		freed = hn_txdesc_put(txr, txd);
		KASSERT(freed != 0,
		    ("fail to free txd upon txdma error"));

		txr->hn_txdma_failed++;
		if_inc_counter(txr->hn_sc->hn_ifp, IFCOUNTER_OERRORS, 1);
		return error;
	}
	*m_head0 = m_head;

	/* +1 RNDIS packet message */
	txr->hn_gpa_cnt = nsegs + 1;

	/* send packet with page buffer */
	txr->hn_gpa[0].gpa_page = atop(txd->rndis_pkt_paddr);
	txr->hn_gpa[0].gpa_ofs = txd->rndis_pkt_paddr & PAGE_MASK;
	txr->hn_gpa[0].gpa_len = pktlen;

	/*
	 * Fill the page buffers with mbuf info after the page
	 * buffer for RNDIS packet message.
	 */
	for (i = 0; i < nsegs; ++i) {
		struct vmbus_gpa *gpa = &txr->hn_gpa[i + 1];

		gpa->gpa_page = atop(segs[i].ds_addr);
		gpa->gpa_ofs = segs[i].ds_addr & PAGE_MASK;
		gpa->gpa_len = segs[i].ds_len;
	}

	txd->chim_index = HN_NVS_CHIM_IDX_INVALID;
	txd->chim_size = 0;
	txr->hn_sendpkt = hn_sendpkt_rndis_sglist;
done:
	txd->m = m_head;

	/* Set the completion routine */
	hn_send_ctx_init(&txd->send_ctx, hn_tx_done, txd);

	return 0;
}

/*
 * NOTE:
 * If this function fails, then txd will be freed, but the mbuf
 * associated w/ the txd will _not_ be freed.
 */
static int
hn_send_pkt(struct ifnet *ifp, struct hn_tx_ring *txr, struct hn_txdesc *txd)
{
	int error, send_failed = 0;

again:
	/*
	 * Make sure that txd is not freed before ETHER_BPF_MTAP.
	 */
	hn_txdesc_hold(txd);
	error = txr->hn_sendpkt(txr, txd);
	if (!error) {
		ETHER_BPF_MTAP(ifp, txd->m);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if (!hn_use_if_start) {
			if_inc_counter(ifp, IFCOUNTER_OBYTES,
			    txd->m->m_pkthdr.len);
			if (txd->m->m_flags & M_MCAST)
				if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);
		}
		txr->hn_pkts++;
	}
	hn_txdesc_put(txr, txd);

	if (__predict_false(error)) {
		int freed;

		/*
		 * This should "really rarely" happen.
		 *
		 * XXX Too many RX to be acked or too many sideband
		 * commands to run?  Ask netvsc_channel_rollup()
		 * to kick start later.
		 */
		txr->hn_has_txeof = 1;
		if (!send_failed) {
			txr->hn_send_failed++;
			send_failed = 1;
			/*
			 * Try sending again after set hn_has_txeof;
			 * in case that we missed the last
			 * netvsc_channel_rollup().
			 */
			goto again;
		}
		if_printf(ifp, "send failed\n");

		/*
		 * Caller will perform further processing on the
		 * associated mbuf, so don't free it in hn_txdesc_put();
		 * only unload it from the DMA map in hn_txdesc_put(),
		 * if it was loaded.
		 */
		txd->m = NULL;
		freed = hn_txdesc_put(txr, txd);
		KASSERT(freed != 0,
		    ("fail to free txd upon send error"));

		txr->hn_send_failed++;
	}
	return error;
}

/*
 * Start a transmit of one or more packets
 */
static int
hn_start_locked(struct hn_tx_ring *txr, int len)
{
	struct hn_softc *sc = txr->hn_sc;
	struct ifnet *ifp = sc->hn_ifp;

	KASSERT(hn_use_if_start,
	    ("hn_start_locked is called, when if_start is disabled"));
	KASSERT(txr == &sc->hn_tx_ring[0], ("not the first TX ring"));
	mtx_assert(&txr->hn_tx_lock, MA_OWNED);

	if (__predict_false(txr->hn_suspended))
		return 0;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return 0;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		struct hn_txdesc *txd;
		struct mbuf *m_head;
		int error;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (len > 0 && m_head->m_pkthdr.len > len) {
			/*
			 * This sending could be time consuming; let callers
			 * dispatch this packet sending (and sending of any
			 * following up packets) to tx taskqueue.
			 */
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			return 1;
		}

		txd = hn_txdesc_get(txr);
		if (txd == NULL) {
			txr->hn_no_txdescs++;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			atomic_set_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
			break;
		}

		error = hn_encap(txr, txd, &m_head);
		if (error) {
			/* Both txd and m_head are freed */
			continue;
		}

		error = hn_send_pkt(ifp, txr, txd);
		if (__predict_false(error)) {
			/* txd is freed, but m_head is not */
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			atomic_set_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
			break;
		}
	}
	return 0;
}

/*
 * Append the specified data to the indicated mbuf chain,
 * Extend the mbuf chain if the new data does not fit in
 * existing space.
 *
 * This is a minor rewrite of m_append() from sys/kern/uipc_mbuf.c.
 * There should be an equivalent in the kernel mbuf code,
 * but there does not appear to be one yet.
 *
 * Differs from m_append() in that additional mbufs are
 * allocated with cluster size MJUMPAGESIZE, and filled
 * accordingly.
 *
 * Return 1 if able to complete the job; otherwise 0.
 */
static int
hv_m_append(struct mbuf *m0, int len, c_caddr_t cp)
{
	struct mbuf *m, *n;
	int remainder, space;

	for (m = m0; m->m_next != NULL; m = m->m_next)
		;
	remainder = len;
	space = M_TRAILINGSPACE(m);
	if (space > 0) {
		/*
		 * Copy into available space.
		 */
		if (space > remainder)
			space = remainder;
		bcopy(cp, mtod(m, caddr_t) + m->m_len, space);
		m->m_len += space;
		cp += space;
		remainder -= space;
	}
	while (remainder > 0) {
		/*
		 * Allocate a new mbuf; could check space
		 * and allocate a cluster instead.
		 */
		n = m_getjcl(M_NOWAIT, m->m_type, 0, MJUMPAGESIZE);
		if (n == NULL)
			break;
		n->m_len = min(MJUMPAGESIZE, remainder);
		bcopy(cp, mtod(n, caddr_t), n->m_len);
		cp += n->m_len;
		remainder -= n->m_len;
		m->m_next = n;
		m = n;
	}
	if (m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len += len - remainder;

	return (remainder == 0);
}

#if defined(INET) || defined(INET6)
static __inline int
hn_lro_rx(struct lro_ctrl *lc, struct mbuf *m)
{
#if __FreeBSD_version >= 1100095
	if (hn_lro_mbufq_depth) {
		tcp_lro_queue_mbuf(lc, m);
		return 0;
	}
#endif
	return tcp_lro_rx(lc, m, 0);
}
#endif

/*
 * Called when we receive a data packet from the "wire" on the
 * specified device
 *
 * Note:  This is no longer used as a callback
 */
int
hn_rxpkt(struct hn_rx_ring *rxr, const void *data, int dlen,
    const struct hn_recvinfo *info)
{
	struct ifnet *ifp = rxr->hn_ifp;
	struct mbuf *m_new;
	int size, do_lro = 0, do_csum = 1;
	int hash_type;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return (0);

	/*
	 * Bail out if packet contains more data than configured MTU.
	 */
	if (dlen > (ifp->if_mtu + ETHER_HDR_LEN)) {
		return (0);
	} else if (dlen <= MHLEN) {
		m_new = m_gethdr(M_NOWAIT, MT_DATA);
		if (m_new == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			return (0);
		}
		memcpy(mtod(m_new, void *), data, dlen);
		m_new->m_pkthdr.len = m_new->m_len = dlen;
		rxr->hn_small_pkts++;
	} else {
		/*
		 * Get an mbuf with a cluster.  For packets 2K or less,
		 * get a standard 2K cluster.  For anything larger, get a
		 * 4K cluster.  Any buffers larger than 4K can cause problems
		 * if looped around to the Hyper-V TX channel, so avoid them.
		 */
		size = MCLBYTES;
		if (dlen > MCLBYTES) {
			/* 4096 */
			size = MJUMPAGESIZE;
		}

		m_new = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, size);
		if (m_new == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			return (0);
		}

		hv_m_append(m_new, dlen, data);
	}
	m_new->m_pkthdr.rcvif = ifp;

	if (__predict_false((ifp->if_capenable & IFCAP_RXCSUM) == 0))
		do_csum = 0;

	/* receive side checksum offload */
	if (info->csum_info != HN_NDIS_RXCSUM_INFO_INVALID) {
		/* IP csum offload */
		if ((info->csum_info & NDIS_RXCSUM_INFO_IPCS_OK) && do_csum) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_IP_CHECKED | CSUM_IP_VALID);
			rxr->hn_csum_ip++;
		}

		/* TCP/UDP csum offload */
		if ((info->csum_info & (NDIS_RXCSUM_INFO_UDPCS_OK |
		     NDIS_RXCSUM_INFO_TCPCS_OK)) && do_csum) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			m_new->m_pkthdr.csum_data = 0xffff;
			if (info->csum_info & NDIS_RXCSUM_INFO_TCPCS_OK)
				rxr->hn_csum_tcp++;
			else
				rxr->hn_csum_udp++;
		}

		if ((info->csum_info &
		     (NDIS_RXCSUM_INFO_TCPCS_OK | NDIS_RXCSUM_INFO_IPCS_OK)) ==
		    (NDIS_RXCSUM_INFO_TCPCS_OK | NDIS_RXCSUM_INFO_IPCS_OK))
			do_lro = 1;
	} else {
		const struct ether_header *eh;
		uint16_t etype;
		int hoff;

		hoff = sizeof(*eh);
		if (m_new->m_len < hoff)
			goto skip;
		eh = mtod(m_new, struct ether_header *);
		etype = ntohs(eh->ether_type);
		if (etype == ETHERTYPE_VLAN) {
			const struct ether_vlan_header *evl;

			hoff = sizeof(*evl);
			if (m_new->m_len < hoff)
				goto skip;
			evl = mtod(m_new, struct ether_vlan_header *);
			etype = ntohs(evl->evl_proto);
		}

		if (etype == ETHERTYPE_IP) {
			int pr;

			pr = hn_check_iplen(m_new, hoff);
			if (pr == IPPROTO_TCP) {
				if (do_csum &&
				    (rxr->hn_trust_hcsum &
				     HN_TRUST_HCSUM_TCP)) {
					rxr->hn_csum_trusted++;
					m_new->m_pkthdr.csum_flags |=
					   (CSUM_IP_CHECKED | CSUM_IP_VALID |
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
					m_new->m_pkthdr.csum_data = 0xffff;
				}
				do_lro = 1;
			} else if (pr == IPPROTO_UDP) {
				if (do_csum &&
				    (rxr->hn_trust_hcsum &
				     HN_TRUST_HCSUM_UDP)) {
					rxr->hn_csum_trusted++;
					m_new->m_pkthdr.csum_flags |=
					   (CSUM_IP_CHECKED | CSUM_IP_VALID |
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
					m_new->m_pkthdr.csum_data = 0xffff;
				}
			} else if (pr != IPPROTO_DONE && do_csum &&
			    (rxr->hn_trust_hcsum & HN_TRUST_HCSUM_IP)) {
				rxr->hn_csum_trusted++;
				m_new->m_pkthdr.csum_flags |=
				    (CSUM_IP_CHECKED | CSUM_IP_VALID);
			}
		}
	}
skip:
	if (info->vlan_info != HN_NDIS_VLAN_INFO_INVALID) {
		m_new->m_pkthdr.ether_vtag = EVL_MAKETAG(
		    NDIS_VLAN_INFO_ID(info->vlan_info),
		    NDIS_VLAN_INFO_PRI(info->vlan_info),
		    NDIS_VLAN_INFO_CFI(info->vlan_info));
		m_new->m_flags |= M_VLANTAG;
	}

	if (info->hash_info != HN_NDIS_HASH_INFO_INVALID) {
		rxr->hn_rss_pkts++;
		m_new->m_pkthdr.flowid = info->hash_value;
		hash_type = M_HASHTYPE_OPAQUE_HASH;
		if ((info->hash_info & NDIS_HASH_FUNCTION_MASK) ==
		    NDIS_HASH_FUNCTION_TOEPLITZ) {
			uint32_t type = (info->hash_info & NDIS_HASH_TYPE_MASK);

			switch (type) {
			case NDIS_HASH_IPV4:
				hash_type = M_HASHTYPE_RSS_IPV4;
				break;

			case NDIS_HASH_TCP_IPV4:
				hash_type = M_HASHTYPE_RSS_TCP_IPV4;
				break;

			case NDIS_HASH_IPV6:
				hash_type = M_HASHTYPE_RSS_IPV6;
				break;

			case NDIS_HASH_IPV6_EX:
				hash_type = M_HASHTYPE_RSS_IPV6_EX;
				break;

			case NDIS_HASH_TCP_IPV6:
				hash_type = M_HASHTYPE_RSS_TCP_IPV6;
				break;

			case NDIS_HASH_TCP_IPV6_EX:
				hash_type = M_HASHTYPE_RSS_TCP_IPV6_EX;
				break;
			}
		}
	} else {
		m_new->m_pkthdr.flowid = rxr->hn_rx_idx;
		hash_type = M_HASHTYPE_OPAQUE;
	}
	M_HASHTYPE_SET(m_new, hash_type);

	/*
	 * Note:  Moved RX completion back to hv_nv_on_receive() so all
	 * messages (not just data messages) will trigger a response.
	 */

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	rxr->hn_pkts++;

	if ((ifp->if_capenable & IFCAP_LRO) && do_lro) {
#if defined(INET) || defined(INET6)
		struct lro_ctrl *lro = &rxr->hn_lro;

		if (lro->lro_cnt) {
			rxr->hn_lro_tried++;
			if (hn_lro_rx(lro, m_new) == 0) {
				/* DONE! */
				return 0;
			}
		}
#endif
	}

	/* We're not holding the lock here, so don't release it */
	(*ifp->if_input)(ifp, m_new);

	return (0);
}

static int
hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct hn_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int mask, error = 0;

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > NETVSC_MAX_CONFIGURABLE_MTU) {
			error = EINVAL;
			break;
		}

		HN_LOCK(sc);

		if ((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0) {
			HN_UNLOCK(sc);
			break;
		}

		if ((sc->hn_caps & HN_CAP_MTU) == 0) {
			/* Can't change MTU */
			HN_UNLOCK(sc);
			error = EOPNOTSUPP;
			break;
		}

		if (ifp->if_mtu == ifr->ifr_mtu) {
			HN_UNLOCK(sc);
			break;
		}

		/*
		 * Suspend this interface before the synthetic parts
		 * are ripped.
		 */
		hn_suspend(sc);

		/*
		 * Detach the synthetics parts, i.e. NVS and RNDIS.
		 */
		hn_synth_detach(sc);

		/*
		 * Reattach the synthetic parts, i.e. NVS and RNDIS,
		 * with the new MTU setting.
		 */
		error = hn_synth_attach(sc, ifr->ifr_mtu);
		if (error) {
			HN_UNLOCK(sc);
			break;
		}

		/*
		 * Commit the requested MTU, after the synthetic parts
		 * have been successfully attached.
		 */
		ifp->if_mtu = ifr->ifr_mtu;

		/*
		 * Make sure that various parameters based on MTU are
		 * still valid, after the MTU change.
		 */
		if (sc->hn_tx_ring[0].hn_chim_size > sc->hn_chim_szmax)
			hn_set_chim_size(sc, sc->hn_chim_szmax);
		hn_set_tso_maxsize(sc, hn_tso_maxlen, ifp->if_mtu);
#if __FreeBSD_version >= 1100099
		if (sc->hn_rx_ring[0].hn_lro.lro_length_lim <
		    HN_LRO_LENLIM_MIN(ifp))
			hn_set_lro_lenlim(sc, HN_LRO_LENLIM_MIN(ifp));
#endif

		/*
		 * All done!  Resume the interface now.
		 */
		hn_resume(sc);

		HN_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS:
		HN_LOCK(sc);

		if ((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0) {
			HN_UNLOCK(sc);
			break;
		}

		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				hn_set_rxfilter(sc);
			else
				hn_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				hn_stop(sc);
		}
		sc->hn_if_flags = ifp->if_flags;

		HN_UNLOCK(sc);
		break;

	case SIOCSIFCAP:
		HN_LOCK(sc);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			if (ifp->if_capenable & IFCAP_TXCSUM)
				ifp->if_hwassist |= HN_CSUM_IP_HWASSIST(sc);
			else
				ifp->if_hwassist &= ~HN_CSUM_IP_HWASSIST(sc);
		}
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
			if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
				ifp->if_hwassist |= HN_CSUM_IP6_HWASSIST(sc);
			else
				ifp->if_hwassist &= ~HN_CSUM_IP6_HWASSIST(sc);
		}

		/* TODO: flip RNDIS offload parameters for RXCSUM. */
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
#ifdef foo
		/* We can't diff IPv6 packets from IPv4 packets on RX path. */
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
#endif

		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;

		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			if (ifp->if_capenable & IFCAP_TSO4)
				ifp->if_hwassist |= CSUM_IP_TSO;
			else
				ifp->if_hwassist &= ~CSUM_IP_TSO;
		}
		if (mask & IFCAP_TSO6) {
			ifp->if_capenable ^= IFCAP_TSO6;
			if (ifp->if_capenable & IFCAP_TSO6)
				ifp->if_hwassist |= CSUM_IP6_TSO;
			else
				ifp->if_hwassist &= ~CSUM_IP6_TSO;
		}

		HN_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef notyet
		/*
		 * XXX
		 * Multicast uses mutex, while RNDIS RX filter setting
		 * sleeps.  We workaround this by always enabling
		 * ALLMULTI.  ALLMULTI would actually always be on, even
		 * if we supported the SIOCADDMULTI/SIOCDELMULTI, since
		 * we don't support multicast address list configuration
		 * for this driver.
		 */
		HN_LOCK(sc);

		if ((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0) {
			HN_UNLOCK(sc);
			break;
		}
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			hn_set_rxfilter(sc);

		HN_UNLOCK(sc);
#endif
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->hn_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);
}

static void
hn_stop(struct hn_softc *sc)
{
	struct ifnet *ifp = sc->hn_ifp;
	int i;

	HN_LOCK_ASSERT(sc);

	KASSERT(sc->hn_flags & HN_FLAG_SYNTH_ATTACHED,
	    ("synthetic parts were not attached"));

	/* Clear RUNNING bit _before_ hn_suspend_data() */
	atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_RUNNING);
	hn_suspend_data(sc);

	/* Clear OACTIVE bit. */
	atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i)
		sc->hn_tx_ring[i].hn_oactive = 0;
}

/*
 * FreeBSD transmit entry point
 */
static void
hn_start(struct ifnet *ifp)
{
	struct hn_softc *sc = ifp->if_softc;
	struct hn_tx_ring *txr = &sc->hn_tx_ring[0];

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		sched = hn_start_locked(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (!sched)
			return;
	}
do_sched:
	taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_tx_task);
}

static void
hn_start_txeof(struct hn_tx_ring *txr)
{
	struct hn_softc *sc = txr->hn_sc;
	struct ifnet *ifp = sc->hn_ifp;

	KASSERT(txr == &sc->hn_tx_ring[0], ("not the first TX ring"));

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
		sched = hn_start_locked(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (sched) {
			taskqueue_enqueue(txr->hn_tx_taskq,
			    &txr->hn_tx_task);
		}
	} else {
do_sched:
		/*
		 * Release the OACTIVE earlier, with the hope, that
		 * others could catch up.  The task will clear the
		 * flag again with the hn_tx_lock to avoid possible
		 * races.
		 */
		atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
		taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}
}

static void
hn_init_locked(struct hn_softc *sc)
{
	struct ifnet *ifp = sc->hn_ifp;
	int i;

	HN_LOCK_ASSERT(sc);

	if ((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0)
		return;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	/* Configure RX filter */
	hn_set_rxfilter(sc);

	/* Clear OACTIVE bit. */
	atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i)
		sc->hn_tx_ring[i].hn_oactive = 0;

	/* Clear TX 'suspended' bit. */
	hn_tx_resume(sc, sc->hn_tx_ring_inuse);

	/* Everything is ready; unleash! */
	atomic_set_int(&ifp->if_drv_flags, IFF_DRV_RUNNING);
}

static void
hn_init(void *xsc)
{
	struct hn_softc *sc = xsc;

	HN_LOCK(sc);
	hn_init_locked(sc);
	HN_UNLOCK(sc);
}

#ifdef LATER
/*
 *
 */
static void
hn_watchdog(struct ifnet *ifp)
{

	if_printf(ifp, "watchdog timeout -- resetting\n");
	hn_init(ifp->if_softc);    /* XXX */
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
}
#endif

#if __FreeBSD_version >= 1100099

static int
hn_lro_lenlim_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	unsigned int lenlim;
	int error;

	lenlim = sc->hn_rx_ring[0].hn_lro.lro_length_lim;
	error = sysctl_handle_int(oidp, &lenlim, 0, req);
	if (error || req->newptr == NULL)
		return error;

	HN_LOCK(sc);
	if (lenlim < HN_LRO_LENLIM_MIN(sc->hn_ifp) ||
	    lenlim > TCP_LRO_LENGTH_MAX) {
		HN_UNLOCK(sc);
		return EINVAL;
	}
	hn_set_lro_lenlim(sc, lenlim);
	HN_UNLOCK(sc);

	return 0;
}

static int
hn_lro_ackcnt_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ackcnt, error, i;

	/*
	 * lro_ackcnt_lim is append count limit,
	 * +1 to turn it into aggregation limit.
	 */
	ackcnt = sc->hn_rx_ring[0].hn_lro.lro_ackcnt_lim + 1;
	error = sysctl_handle_int(oidp, &ackcnt, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (ackcnt < 2 || ackcnt > (TCP_LRO_ACKCNT_MAX + 1))
		return EINVAL;

	/*
	 * Convert aggregation limit back to append
	 * count limit.
	 */
	--ackcnt;
	HN_LOCK(sc);
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i)
		sc->hn_rx_ring[i].hn_lro.lro_ackcnt_lim = ackcnt;
	HN_UNLOCK(sc);
	return 0;
}

#endif

static int
hn_trust_hcsum_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int hcsum = arg2;
	int on, error, i;

	on = 0;
	if (sc->hn_rx_ring[0].hn_trust_hcsum & hcsum)
		on = 1;

	error = sysctl_handle_int(oidp, &on, 0, req);
	if (error || req->newptr == NULL)
		return error;

	HN_LOCK(sc);
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i) {
		struct hn_rx_ring *rxr = &sc->hn_rx_ring[i];

		if (on)
			rxr->hn_trust_hcsum |= hcsum;
		else
			rxr->hn_trust_hcsum &= ~hcsum;
	}
	HN_UNLOCK(sc);
	return 0;
}

static int
hn_chim_size_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int chim_size, error;

	chim_size = sc->hn_tx_ring[0].hn_chim_size;
	error = sysctl_handle_int(oidp, &chim_size, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (chim_size > sc->hn_chim_szmax || chim_size <= 0)
		return EINVAL;

	HN_LOCK(sc);
	hn_set_chim_size(sc, chim_size);
	HN_UNLOCK(sc);
	return 0;
}

#if __FreeBSD_version < 1100095
static int
hn_rx_stat_int_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_rx_ring *rxr;
	uint64_t stat;

	stat = 0;
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		stat += *((int *)((uint8_t *)rxr + ofs));
	}

	error = sysctl_handle_64(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		*((int *)((uint8_t *)rxr + ofs)) = 0;
	}
	return 0;
}
#else
static int
hn_rx_stat_u64_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_rx_ring *rxr;
	uint64_t stat;

	stat = 0;
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i) {
		rxr = &sc->hn_rx_ring[i];
		stat += *((uint64_t *)((uint8_t *)rxr + ofs));
	}

	error = sysctl_handle_64(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i) {
		rxr = &sc->hn_rx_ring[i];
		*((uint64_t *)((uint8_t *)rxr + ofs)) = 0;
	}
	return 0;
}

#endif

static int
hn_rx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_rx_ring *rxr;
	u_long stat;

	stat = 0;
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i) {
		rxr = &sc->hn_rx_ring[i];
		stat += *((u_long *)((uint8_t *)rxr + ofs));
	}

	error = sysctl_handle_long(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_rx_ring_inuse; ++i) {
		rxr = &sc->hn_rx_ring[i];
		*((u_long *)((uint8_t *)rxr + ofs)) = 0;
	}
	return 0;
}

static int
hn_tx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_tx_ring *txr;
	u_long stat;

	stat = 0;
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i) {
		txr = &sc->hn_tx_ring[i];
		stat += *((u_long *)((uint8_t *)txr + ofs));
	}

	error = sysctl_handle_long(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i) {
		txr = &sc->hn_tx_ring[i];
		*((u_long *)((uint8_t *)txr + ofs)) = 0;
	}
	return 0;
}

static int
hn_tx_conf_int_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error, conf;
	struct hn_tx_ring *txr;

	txr = &sc->hn_tx_ring[0];
	conf = *((int *)((uint8_t *)txr + ofs));

	error = sysctl_handle_int(oidp, &conf, 0, req);
	if (error || req->newptr == NULL)
		return error;

	HN_LOCK(sc);
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i) {
		txr = &sc->hn_tx_ring[i];
		*((int *)((uint8_t *)txr + ofs)) = conf;
	}
	HN_UNLOCK(sc);

	return 0;
}

static int
hn_ndis_version_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	char verstr[16];

	snprintf(verstr, sizeof(verstr), "%u.%u",
	    HN_NDIS_VERSION_MAJOR(sc->hn_ndis_ver),
	    HN_NDIS_VERSION_MINOR(sc->hn_ndis_ver));
	return sysctl_handle_string(oidp, verstr, sizeof(verstr), req);
}

static int
hn_caps_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	char caps_str[128];
	uint32_t caps;

	HN_LOCK(sc);
	caps = sc->hn_caps;
	HN_UNLOCK(sc);
	snprintf(caps_str, sizeof(caps_str), "%b", caps,
	    "\020"
	    "\001VLAN"
	    "\002MTU"
	    "\003IPCS"
	    "\004TCP4CS"
	    "\005TCP6CS"
	    "\006UDP4CS"
	    "\007UDP6CS"
	    "\010TSO4"
	    "\011TSO6"
	    "\012HASHVAL");
	return sysctl_handle_string(oidp, caps_str, sizeof(caps_str), req);
}

static int
hn_hwassist_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	char assist_str[128];
	uint32_t hwassist;

	HN_LOCK(sc);
	hwassist = sc->hn_ifp->if_hwassist;
	HN_UNLOCK(sc);
	snprintf(assist_str, sizeof(assist_str), "%b", hwassist, CSUM_BITS);
	return sysctl_handle_string(oidp, assist_str, sizeof(assist_str), req);
}

static int
hn_rxfilter_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	char filter_str[128];
	uint32_t filter;

	HN_LOCK(sc);
	filter = sc->hn_rx_filter;
	HN_UNLOCK(sc);
	snprintf(filter_str, sizeof(filter_str), "%b", filter,
	    NDIS_PACKET_TYPES);
	return sysctl_handle_string(oidp, filter_str, sizeof(filter_str), req);
}

static int
hn_rss_key_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int error;

	HN_LOCK(sc);

	error = SYSCTL_OUT(req, sc->hn_rss.rss_key, sizeof(sc->hn_rss.rss_key));
	if (error || req->newptr == NULL)
		goto back;

	error = SYSCTL_IN(req, sc->hn_rss.rss_key, sizeof(sc->hn_rss.rss_key));
	if (error)
		goto back;
	sc->hn_flags |= HN_FLAG_HAS_RSSKEY;

	if (sc->hn_rx_ring_inuse > 1) {
		error = hn_rss_reconfig(sc);
	} else {
		/* Not RSS capable, at least for now; just save the RSS key. */
		error = 0;
	}
back:
	HN_UNLOCK(sc);
	return (error);
}

static int
hn_rss_ind_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int error;

	HN_LOCK(sc);

	error = SYSCTL_OUT(req, sc->hn_rss.rss_ind, sizeof(sc->hn_rss.rss_ind));
	if (error || req->newptr == NULL)
		goto back;

	/*
	 * Don't allow RSS indirect table change, if this interface is not
	 * RSS capable currently.
	 */
	if (sc->hn_rx_ring_inuse == 1) {
		error = EOPNOTSUPP;
		goto back;
	}

	error = SYSCTL_IN(req, sc->hn_rss.rss_ind, sizeof(sc->hn_rss.rss_ind));
	if (error)
		goto back;
	sc->hn_flags |= HN_FLAG_HAS_RSSIND;

	hn_rss_ind_fixup(sc, sc->hn_rx_ring_inuse);
	error = hn_rss_reconfig(sc);
back:
	HN_UNLOCK(sc);
	return (error);
}

static int
hn_check_iplen(const struct mbuf *m, int hoff)
{
	const struct ip *ip;
	int len, iphlen, iplen;
	const struct tcphdr *th;
	int thoff;				/* TCP data offset */

	len = hoff + sizeof(struct ip);

	/* The packet must be at least the size of an IP header. */
	if (m->m_pkthdr.len < len)
		return IPPROTO_DONE;

	/* The fixed IP header must reside completely in the first mbuf. */
	if (m->m_len < len)
		return IPPROTO_DONE;

	ip = mtodo(m, hoff);

	/* Bound check the packet's stated IP header length. */
	iphlen = ip->ip_hl << 2;
	if (iphlen < sizeof(struct ip))		/* minimum header length */
		return IPPROTO_DONE;

	/* The full IP header must reside completely in the one mbuf. */
	if (m->m_len < hoff + iphlen)
		return IPPROTO_DONE;

	iplen = ntohs(ip->ip_len);

	/*
	 * Check that the amount of data in the buffers is as
	 * at least much as the IP header would have us expect.
	 */
	if (m->m_pkthdr.len < hoff + iplen)
		return IPPROTO_DONE;

	/*
	 * Ignore IP fragments.
	 */
	if (ntohs(ip->ip_off) & (IP_OFFMASK | IP_MF))
		return IPPROTO_DONE;

	/*
	 * The TCP/IP or UDP/IP header must be entirely contained within
	 * the first fragment of a packet.
	 */
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		if (iplen < iphlen + sizeof(struct tcphdr))
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + sizeof(struct tcphdr))
			return IPPROTO_DONE;
		th = (const struct tcphdr *)((const uint8_t *)ip + iphlen);
		thoff = th->th_off << 2;
		if (thoff < sizeof(struct tcphdr) || thoff + iphlen > iplen)
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + thoff)
			return IPPROTO_DONE;
		break;
	case IPPROTO_UDP:
		if (iplen < iphlen + sizeof(struct udphdr))
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + sizeof(struct udphdr))
			return IPPROTO_DONE;
		break;
	default:
		if (iplen < iphlen)
			return IPPROTO_DONE;
		break;
	}
	return ip->ip_p;
}

static int
hn_create_rx_data(struct hn_softc *sc, int ring_cnt)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	device_t dev = sc->hn_dev;
#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
	int lroent_cnt;
#endif
#endif
	int i;

	/*
	 * Create RXBUF for reception.
	 *
	 * NOTE:
	 * - It is shared by all channels.
	 * - A large enough buffer is allocated, certain version of NVSes
	 *   may further limit the usable space.
	 */
	sc->hn_rxbuf = hyperv_dmamem_alloc(bus_get_dma_tag(dev),
	    PAGE_SIZE, 0, NETVSC_RECEIVE_BUFFER_SIZE, &sc->hn_rxbuf_dma,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->hn_rxbuf == NULL) {
		device_printf(sc->hn_dev, "allocate rxbuf failed\n");
		return (ENOMEM);
	}

	sc->hn_rx_ring_cnt = ring_cnt;
	sc->hn_rx_ring_inuse = sc->hn_rx_ring_cnt;

	sc->hn_rx_ring = malloc(sizeof(struct hn_rx_ring) * sc->hn_rx_ring_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);

#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
	lroent_cnt = hn_lro_entry_count;
	if (lroent_cnt < TCP_LRO_ENTRIES)
		lroent_cnt = TCP_LRO_ENTRIES;
	if (bootverbose)
		device_printf(dev, "LRO: entry count %d\n", lroent_cnt);
#endif
#endif	/* INET || INET6 */

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	/* Create dev.hn.UNIT.rx sysctl tree */
	sc->hn_rx_sysctl_tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "rx",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		struct hn_rx_ring *rxr = &sc->hn_rx_ring[i];

		rxr->hn_br = hyperv_dmamem_alloc(bus_get_dma_tag(dev),
		    PAGE_SIZE, 0,
		    NETVSC_DEVICE_RING_BUFFER_SIZE +
		    NETVSC_DEVICE_RING_BUFFER_SIZE,
		    &rxr->hn_br_dma, BUS_DMA_WAITOK);
		if (rxr->hn_br == NULL) {
			device_printf(dev, "allocate bufring failed\n");
			return (ENOMEM);
		}

		if (hn_trust_hosttcp)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_TCP;
		if (hn_trust_hostudp)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_UDP;
		if (hn_trust_hostip)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_IP;
		rxr->hn_ifp = sc->hn_ifp;
		if (i < sc->hn_tx_ring_cnt)
			rxr->hn_txr = &sc->hn_tx_ring[i];
		rxr->hn_rdbuf = malloc(NETVSC_PACKET_SIZE, M_NETVSC, M_WAITOK);
		rxr->hn_rx_idx = i;
		rxr->hn_rxbuf = sc->hn_rxbuf;

		/*
		 * Initialize LRO.
		 */
#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
		tcp_lro_init_args(&rxr->hn_lro, sc->hn_ifp, lroent_cnt,
		    hn_lro_mbufq_depth);
#else
		tcp_lro_init(&rxr->hn_lro);
		rxr->hn_lro.ifp = sc->hn_ifp;
#endif
#if __FreeBSD_version >= 1100099
		rxr->hn_lro.lro_length_lim = HN_LRO_LENLIM_DEF;
		rxr->hn_lro.lro_ackcnt_lim = HN_LRO_ACKCNT_DEF;
#endif
#endif	/* INET || INET6 */

		if (sc->hn_rx_sysctl_tree != NULL) {
			char name[16];

			/*
			 * Create per RX ring sysctl tree:
			 * dev.hn.UNIT.rx.RINGID
			 */
			snprintf(name, sizeof(name), "%d", i);
			rxr->hn_rx_sysctl_tree = SYSCTL_ADD_NODE(ctx,
			    SYSCTL_CHILDREN(sc->hn_rx_sysctl_tree),
			    OID_AUTO, name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

			if (rxr->hn_rx_sysctl_tree != NULL) {
				SYSCTL_ADD_ULONG(ctx,
				    SYSCTL_CHILDREN(rxr->hn_rx_sysctl_tree),
				    OID_AUTO, "packets", CTLFLAG_RW,
				    &rxr->hn_pkts, "# of packets received");
				SYSCTL_ADD_ULONG(ctx,
				    SYSCTL_CHILDREN(rxr->hn_rx_sysctl_tree),
				    OID_AUTO, "rss_pkts", CTLFLAG_RW,
				    &rxr->hn_rss_pkts,
				    "# of packets w/ RSS info received");
			}
		}
	}

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_queued",
	    CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_lro.lro_queued),
#if __FreeBSD_version < 1100095
	    hn_rx_stat_int_sysctl,
#else
	    hn_rx_stat_u64_sysctl,
#endif
	    "LU", "LRO queued");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_flushed",
	    CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_lro.lro_flushed),
#if __FreeBSD_version < 1100095
	    hn_rx_stat_int_sysctl,
#else
	    hn_rx_stat_u64_sysctl,
#endif
	    "LU", "LRO flushed");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_tried",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_lro_tried),
	    hn_rx_stat_ulong_sysctl, "LU", "# of LRO tries");
#if __FreeBSD_version >= 1100099
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_length_lim",
	    CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    hn_lro_lenlim_sysctl, "IU",
	    "Max # of data bytes to be aggregated by LRO");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_ackcnt_lim",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    hn_lro_ackcnt_sysctl, "I",
	    "Max # of ACKs to be aggregated by LRO");
#endif
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hosttcp",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, HN_TRUST_HCSUM_TCP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust tcp segement verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hostudp",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, HN_TRUST_HCSUM_UDP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust udp datagram verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hostip",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, HN_TRUST_HCSUM_IP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust ip packet verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_ip",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_ip),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM IP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_tcp",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_tcp),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM TCP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_udp",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_udp),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM UDP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_trusted",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_trusted),
	    hn_rx_stat_ulong_sysctl, "LU",
	    "# of packets that we trust host's csum verification");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "small_pkts",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_rx_ring, hn_small_pkts),
	    hn_rx_stat_ulong_sysctl, "LU", "# of small packets received");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "rx_ring_cnt",
	    CTLFLAG_RD, &sc->hn_rx_ring_cnt, 0, "# created RX rings");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "rx_ring_inuse",
	    CTLFLAG_RD, &sc->hn_rx_ring_inuse, 0, "# used RX rings");

	return (0);
}

static void
hn_destroy_rx_data(struct hn_softc *sc)
{
	int i;

	if (sc->hn_rxbuf != NULL) {
		hyperv_dmamem_free(&sc->hn_rxbuf_dma, sc->hn_rxbuf);
		sc->hn_rxbuf = NULL;
	}

	if (sc->hn_rx_ring_cnt == 0)
		return;

	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		struct hn_rx_ring *rxr = &sc->hn_rx_ring[i];

		if (rxr->hn_br == NULL)
			continue;
		hyperv_dmamem_free(&rxr->hn_br_dma, rxr->hn_br);
		rxr->hn_br = NULL;

#if defined(INET) || defined(INET6)
		tcp_lro_free(&rxr->hn_lro);
#endif
		free(rxr->hn_rdbuf, M_NETVSC);
	}
	free(sc->hn_rx_ring, M_NETVSC);
	sc->hn_rx_ring = NULL;

	sc->hn_rx_ring_cnt = 0;
	sc->hn_rx_ring_inuse = 0;
}

static int
hn_create_tx_ring(struct hn_softc *sc, int id)
{
	struct hn_tx_ring *txr = &sc->hn_tx_ring[id];
	device_t dev = sc->hn_dev;
	bus_dma_tag_t parent_dtag;
	int error, i;

	txr->hn_sc = sc;
	txr->hn_tx_idx = id;

#ifndef HN_USE_TXDESC_BUFRING
	mtx_init(&txr->hn_txlist_spin, "hn txlist", NULL, MTX_SPIN);
#endif
	mtx_init(&txr->hn_tx_lock, "hn tx", NULL, MTX_DEF);

	txr->hn_txdesc_cnt = HN_TX_DESC_CNT;
	txr->hn_txdesc = malloc(sizeof(struct hn_txdesc) * txr->hn_txdesc_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);
#ifndef HN_USE_TXDESC_BUFRING
	SLIST_INIT(&txr->hn_txlist);
#else
	txr->hn_txdesc_br = buf_ring_alloc(txr->hn_txdesc_cnt, M_NETVSC,
	    M_WAITOK, &txr->hn_tx_lock);
#endif

	txr->hn_tx_taskq = sc->hn_tx_taskq;

	if (hn_use_if_start) {
		txr->hn_txeof = hn_start_txeof;
		TASK_INIT(&txr->hn_tx_task, 0, hn_start_taskfunc, txr);
		TASK_INIT(&txr->hn_txeof_task, 0, hn_start_txeof_taskfunc, txr);
	} else {
		int br_depth;

		txr->hn_txeof = hn_xmit_txeof;
		TASK_INIT(&txr->hn_tx_task, 0, hn_xmit_taskfunc, txr);
		TASK_INIT(&txr->hn_txeof_task, 0, hn_xmit_txeof_taskfunc, txr);

		br_depth = hn_get_txswq_depth(txr);
		txr->hn_mbuf_br = buf_ring_alloc(br_depth, M_NETVSC,
		    M_WAITOK, &txr->hn_tx_lock);
	}

	txr->hn_direct_tx_size = hn_direct_tx_size;

	/*
	 * Always schedule transmission instead of trying to do direct
	 * transmission.  This one gives the best performance so far.
	 */
	txr->hn_sched_tx = 1;

	parent_dtag = bus_get_dma_tag(dev);

	/* DMA tag for RNDIS packet messages. */
	error = bus_dma_tag_create(parent_dtag, /* parent */
	    HN_RNDIS_PKT_ALIGN,		/* alignment */
	    HN_RNDIS_PKT_BOUNDARY,	/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    HN_RNDIS_PKT_LEN,		/* maxsize */
	    1,				/* nsegments */
	    HN_RNDIS_PKT_LEN,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockfuncarg */
	    &txr->hn_tx_rndis_dtag);
	if (error) {
		device_printf(dev, "failed to create rndis dmatag\n");
		return error;
	}

	/* DMA tag for data. */
	error = bus_dma_tag_create(parent_dtag, /* parent */
	    1,				/* alignment */
	    HN_TX_DATA_BOUNDARY,	/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    HN_TX_DATA_MAXSIZE,		/* maxsize */
	    HN_TX_DATA_SEGCNT_MAX,	/* nsegments */
	    HN_TX_DATA_SEGSIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockfuncarg */
	    &txr->hn_tx_data_dtag);
	if (error) {
		device_printf(dev, "failed to create data dmatag\n");
		return error;
	}

	for (i = 0; i < txr->hn_txdesc_cnt; ++i) {
		struct hn_txdesc *txd = &txr->hn_txdesc[i];

		txd->txr = txr;
		txd->chim_index = HN_NVS_CHIM_IDX_INVALID;

		/*
		 * Allocate and load RNDIS packet message.
		 */
        	error = bus_dmamem_alloc(txr->hn_tx_rndis_dtag,
		    (void **)&txd->rndis_pkt,
		    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
		    &txd->rndis_pkt_dmap);
		if (error) {
			device_printf(dev,
			    "failed to allocate rndis_packet_msg, %d\n", i);
			return error;
		}

		error = bus_dmamap_load(txr->hn_tx_rndis_dtag,
		    txd->rndis_pkt_dmap,
		    txd->rndis_pkt, HN_RNDIS_PKT_LEN,
		    hyperv_dma_map_paddr, &txd->rndis_pkt_paddr,
		    BUS_DMA_NOWAIT);
		if (error) {
			device_printf(dev,
			    "failed to load rndis_packet_msg, %d\n", i);
			bus_dmamem_free(txr->hn_tx_rndis_dtag,
			    txd->rndis_pkt, txd->rndis_pkt_dmap);
			return error;
		}

		/* DMA map for TX data. */
		error = bus_dmamap_create(txr->hn_tx_data_dtag, 0,
		    &txd->data_dmap);
		if (error) {
			device_printf(dev,
			    "failed to allocate tx data dmamap\n");
			bus_dmamap_unload(txr->hn_tx_rndis_dtag,
			    txd->rndis_pkt_dmap);
			bus_dmamem_free(txr->hn_tx_rndis_dtag,
			    txd->rndis_pkt, txd->rndis_pkt_dmap);
			return error;
		}

		/* All set, put it to list */
		txd->flags |= HN_TXD_FLAG_ONLIST;
#ifndef HN_USE_TXDESC_BUFRING
		SLIST_INSERT_HEAD(&txr->hn_txlist, txd, link);
#else
		buf_ring_enqueue(txr->hn_txdesc_br, txd);
#endif
	}
	txr->hn_txdesc_avail = txr->hn_txdesc_cnt;

	if (sc->hn_tx_sysctl_tree != NULL) {
		struct sysctl_oid_list *child;
		struct sysctl_ctx_list *ctx;
		char name[16];

		/*
		 * Create per TX ring sysctl tree:
		 * dev.hn.UNIT.tx.RINGID
		 */
		ctx = device_get_sysctl_ctx(dev);
		child = SYSCTL_CHILDREN(sc->hn_tx_sysctl_tree);

		snprintf(name, sizeof(name), "%d", id);
		txr->hn_tx_sysctl_tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO,
		    name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

		if (txr->hn_tx_sysctl_tree != NULL) {
			child = SYSCTL_CHILDREN(txr->hn_tx_sysctl_tree);

			SYSCTL_ADD_INT(ctx, child, OID_AUTO, "txdesc_avail",
			    CTLFLAG_RD, &txr->hn_txdesc_avail, 0,
			    "# of available TX descs");
			if (!hn_use_if_start) {
				SYSCTL_ADD_INT(ctx, child, OID_AUTO, "oactive",
				    CTLFLAG_RD, &txr->hn_oactive, 0,
				    "over active");
			}
			SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "packets",
			    CTLFLAG_RW, &txr->hn_pkts,
			    "# of packets transmitted");
		}
	}

	return 0;
}

static void
hn_txdesc_dmamap_destroy(struct hn_txdesc *txd)
{
	struct hn_tx_ring *txr = txd->txr;

	KASSERT(txd->m == NULL, ("still has mbuf installed"));
	KASSERT((txd->flags & HN_TXD_FLAG_DMAMAP) == 0, ("still dma mapped"));

	bus_dmamap_unload(txr->hn_tx_rndis_dtag, txd->rndis_pkt_dmap);
	bus_dmamem_free(txr->hn_tx_rndis_dtag, txd->rndis_pkt,
	    txd->rndis_pkt_dmap);
	bus_dmamap_destroy(txr->hn_tx_data_dtag, txd->data_dmap);
}

static void
hn_destroy_tx_ring(struct hn_tx_ring *txr)
{
	struct hn_txdesc *txd;

	if (txr->hn_txdesc == NULL)
		return;

#ifndef HN_USE_TXDESC_BUFRING
	while ((txd = SLIST_FIRST(&txr->hn_txlist)) != NULL) {
		SLIST_REMOVE_HEAD(&txr->hn_txlist, link);
		hn_txdesc_dmamap_destroy(txd);
	}
#else
	mtx_lock(&txr->hn_tx_lock);
	while ((txd = buf_ring_dequeue_sc(txr->hn_txdesc_br)) != NULL)
		hn_txdesc_dmamap_destroy(txd);
	mtx_unlock(&txr->hn_tx_lock);
#endif

	if (txr->hn_tx_data_dtag != NULL)
		bus_dma_tag_destroy(txr->hn_tx_data_dtag);
	if (txr->hn_tx_rndis_dtag != NULL)
		bus_dma_tag_destroy(txr->hn_tx_rndis_dtag);

#ifdef HN_USE_TXDESC_BUFRING
	buf_ring_free(txr->hn_txdesc_br, M_NETVSC);
#endif

	free(txr->hn_txdesc, M_NETVSC);
	txr->hn_txdesc = NULL;

	if (txr->hn_mbuf_br != NULL)
		buf_ring_free(txr->hn_mbuf_br, M_NETVSC);

#ifndef HN_USE_TXDESC_BUFRING
	mtx_destroy(&txr->hn_txlist_spin);
#endif
	mtx_destroy(&txr->hn_tx_lock);
}

static int
hn_create_tx_data(struct hn_softc *sc, int ring_cnt)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	int i;

	/*
	 * Create TXBUF for chimney sending.
	 *
	 * NOTE: It is shared by all channels.
	 */
	sc->hn_chim = hyperv_dmamem_alloc(bus_get_dma_tag(sc->hn_dev),
	    PAGE_SIZE, 0, NETVSC_SEND_BUFFER_SIZE, &sc->hn_chim_dma,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (sc->hn_chim == NULL) {
		device_printf(sc->hn_dev, "allocate txbuf failed\n");
		return (ENOMEM);
	}

	sc->hn_tx_ring_cnt = ring_cnt;
	sc->hn_tx_ring_inuse = sc->hn_tx_ring_cnt;

	sc->hn_tx_ring = malloc(sizeof(struct hn_tx_ring) * sc->hn_tx_ring_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);

	ctx = device_get_sysctl_ctx(sc->hn_dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->hn_dev));

	/* Create dev.hn.UNIT.tx sysctl tree */
	sc->hn_tx_sysctl_tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "tx",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		int error;

		error = hn_create_tx_ring(sc, i);
		if (error)
			return error;
	}

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "no_txdescs",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_no_txdescs),
	    hn_tx_stat_ulong_sysctl, "LU", "# of times short of TX descs");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "send_failed",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_send_failed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of hyper-v sending failure");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "txdma_failed",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_txdma_failed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of TX DMA failure");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_collapsed",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_tx_collapsed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of TX mbuf collapsed");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_chimney",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_tx_chimney),
	    hn_tx_stat_ulong_sysctl, "LU", "# of chimney send");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_chimney_tried",
	    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_tx_chimney_tried),
	    hn_tx_stat_ulong_sysctl, "LU", "# of chimney send tries");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "txdesc_cnt",
	    CTLFLAG_RD, &sc->hn_tx_ring[0].hn_txdesc_cnt, 0,
	    "# of total TX descs");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "tx_chimney_max",
	    CTLFLAG_RD, &sc->hn_chim_szmax, 0,
	    "Chimney send packet size upper boundary");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_chimney_size",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    hn_chim_size_sysctl, "I", "Chimney send packet size limit");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "direct_tx_size",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_direct_tx_size),
	    hn_tx_conf_int_sysctl, "I",
	    "Size of the packet for direct transmission");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "sched_tx",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc,
	    __offsetof(struct hn_tx_ring, hn_sched_tx),
	    hn_tx_conf_int_sysctl, "I",
	    "Always schedule transmission "
	    "instead of doing direct transmission");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "tx_ring_cnt",
	    CTLFLAG_RD, &sc->hn_tx_ring_cnt, 0, "# created TX rings");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "tx_ring_inuse",
	    CTLFLAG_RD, &sc->hn_tx_ring_inuse, 0, "# used TX rings");

	return 0;
}

static void
hn_set_chim_size(struct hn_softc *sc, int chim_size)
{
	int i;

	for (i = 0; i < sc->hn_tx_ring_inuse; ++i)
		sc->hn_tx_ring[i].hn_chim_size = chim_size;
}

static void
hn_set_tso_maxsize(struct hn_softc *sc, int tso_maxlen, int mtu)
{
	struct ifnet *ifp = sc->hn_ifp;
	int tso_minlen;

	if ((ifp->if_capabilities & (IFCAP_TSO4 | IFCAP_TSO6)) == 0)
		return;

	KASSERT(sc->hn_ndis_tso_sgmin >= 2,
	    ("invalid NDIS tso sgmin %d", sc->hn_ndis_tso_sgmin));
	tso_minlen = sc->hn_ndis_tso_sgmin * mtu;

	KASSERT(sc->hn_ndis_tso_szmax >= tso_minlen &&
	    sc->hn_ndis_tso_szmax <= IP_MAXPACKET,
	    ("invalid NDIS tso szmax %d", sc->hn_ndis_tso_szmax));

	if (tso_maxlen < tso_minlen)
		tso_maxlen = tso_minlen;
	else if (tso_maxlen > IP_MAXPACKET)
		tso_maxlen = IP_MAXPACKET;
	if (tso_maxlen > sc->hn_ndis_tso_szmax)
		tso_maxlen = sc->hn_ndis_tso_szmax;
	ifp->if_hw_tsomax = tso_maxlen - (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
	if (bootverbose)
		if_printf(ifp, "TSO size max %u\n", ifp->if_hw_tsomax);
}

static void
hn_fixup_tx_data(struct hn_softc *sc)
{
	uint64_t csum_assist;
	int i;

	hn_set_chim_size(sc, sc->hn_chim_szmax);
	if (hn_tx_chimney_size > 0 &&
	    hn_tx_chimney_size < sc->hn_chim_szmax)
		hn_set_chim_size(sc, hn_tx_chimney_size);

	csum_assist = 0;
	if (sc->hn_caps & HN_CAP_IPCS)
		csum_assist |= CSUM_IP;
	if (sc->hn_caps & HN_CAP_TCP4CS)
		csum_assist |= CSUM_IP_TCP;
	if (sc->hn_caps & HN_CAP_UDP4CS)
		csum_assist |= CSUM_IP_UDP;
#ifdef notyet
	if (sc->hn_caps & HN_CAP_TCP6CS)
		csum_assist |= CSUM_IP6_TCP;
	if (sc->hn_caps & HN_CAP_UDP6CS)
		csum_assist |= CSUM_IP6_UDP;
#endif
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i)
		sc->hn_tx_ring[i].hn_csum_assist = csum_assist;

	if (sc->hn_caps & HN_CAP_HASHVAL) {
		/*
		 * Support HASHVAL pktinfo on TX path.
		 */
		if (bootverbose)
			if_printf(sc->hn_ifp, "support HASHVAL pktinfo\n");
		for (i = 0; i < sc->hn_tx_ring_cnt; ++i)
			sc->hn_tx_ring[i].hn_tx_flags |= HN_TX_FLAG_HASHVAL;
	}
}

static void
hn_destroy_tx_data(struct hn_softc *sc)
{
	int i;

	if (sc->hn_chim != NULL) {
		hyperv_dmamem_free(&sc->hn_chim_dma, sc->hn_chim);
		sc->hn_chim = NULL;
	}

	if (sc->hn_tx_ring_cnt == 0)
		return;

	for (i = 0; i < sc->hn_tx_ring_cnt; ++i)
		hn_destroy_tx_ring(&sc->hn_tx_ring[i]);

	free(sc->hn_tx_ring, M_NETVSC);
	sc->hn_tx_ring = NULL;

	sc->hn_tx_ring_cnt = 0;
	sc->hn_tx_ring_inuse = 0;
}

static void
hn_start_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	hn_start_locked(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static void
hn_start_txeof_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	atomic_clear_int(&txr->hn_sc->hn_ifp->if_drv_flags, IFF_DRV_OACTIVE);
	hn_start_locked(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static int
hn_xmit(struct hn_tx_ring *txr, int len)
{
	struct hn_softc *sc = txr->hn_sc;
	struct ifnet *ifp = sc->hn_ifp;
	struct mbuf *m_head;

	mtx_assert(&txr->hn_tx_lock, MA_OWNED);
	KASSERT(hn_use_if_start == 0,
	    ("hn_xmit is called, when if_start is enabled"));

	if (__predict_false(txr->hn_suspended))
		return 0;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || txr->hn_oactive)
		return 0;

	while ((m_head = drbr_peek(ifp, txr->hn_mbuf_br)) != NULL) {
		struct hn_txdesc *txd;
		int error;

		if (len > 0 && m_head->m_pkthdr.len > len) {
			/*
			 * This sending could be time consuming; let callers
			 * dispatch this packet sending (and sending of any
			 * following up packets) to tx taskqueue.
			 */
			drbr_putback(ifp, txr->hn_mbuf_br, m_head);
			return 1;
		}

		txd = hn_txdesc_get(txr);
		if (txd == NULL) {
			txr->hn_no_txdescs++;
			drbr_putback(ifp, txr->hn_mbuf_br, m_head);
			txr->hn_oactive = 1;
			break;
		}

		error = hn_encap(txr, txd, &m_head);
		if (error) {
			/* Both txd and m_head are freed; discard */
			drbr_advance(ifp, txr->hn_mbuf_br);
			continue;
		}

		error = hn_send_pkt(ifp, txr, txd);
		if (__predict_false(error)) {
			/* txd is freed, but m_head is not */
			drbr_putback(ifp, txr->hn_mbuf_br, m_head);
			txr->hn_oactive = 1;
			break;
		}

		/* Sent */
		drbr_advance(ifp, txr->hn_mbuf_br);
	}
	return 0;
}

static int
hn_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct hn_softc *sc = ifp->if_softc;
	struct hn_tx_ring *txr;
	int error, idx = 0;

	/*
	 * Select the TX ring based on flowid
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE)
		idx = m->m_pkthdr.flowid % sc->hn_tx_ring_inuse;
	txr = &sc->hn_tx_ring[idx];

	error = drbr_enqueue(ifp, txr->hn_mbuf_br, m);
	if (error) {
		if_inc_counter(ifp, IFCOUNTER_OQDROPS, 1);
		return error;
	}

	if (txr->hn_oactive)
		return 0;

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		sched = hn_xmit(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (!sched)
			return 0;
	}
do_sched:
	taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_tx_task);
	return 0;
}

static void
hn_tx_ring_qflush(struct hn_tx_ring *txr)
{
	struct mbuf *m;

	mtx_lock(&txr->hn_tx_lock);
	while ((m = buf_ring_dequeue_sc(txr->hn_mbuf_br)) != NULL)
		m_freem(m);
	mtx_unlock(&txr->hn_tx_lock);
}

static void
hn_xmit_qflush(struct ifnet *ifp)
{
	struct hn_softc *sc = ifp->if_softc;
	int i;

	for (i = 0; i < sc->hn_tx_ring_inuse; ++i)
		hn_tx_ring_qflush(&sc->hn_tx_ring[i]);
	if_qflush(ifp);
}

static void
hn_xmit_txeof(struct hn_tx_ring *txr)
{

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		txr->hn_oactive = 0;
		sched = hn_xmit(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (sched) {
			taskqueue_enqueue(txr->hn_tx_taskq,
			    &txr->hn_tx_task);
		}
	} else {
do_sched:
		/*
		 * Release the oactive earlier, with the hope, that
		 * others could catch up.  The task will clear the
		 * oactive again with the hn_tx_lock to avoid possible
		 * races.
		 */
		txr->hn_oactive = 0;
		taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}
}

static void
hn_xmit_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	hn_xmit(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static void
hn_xmit_txeof_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	txr->hn_oactive = 0;
	hn_xmit(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static int
hn_chan_attach(struct hn_softc *sc, struct vmbus_channel *chan)
{
	struct vmbus_chan_br cbr;
	struct hn_rx_ring *rxr;
	struct hn_tx_ring *txr = NULL;
	int idx, error;

	idx = vmbus_chan_subidx(chan);

	/*
	 * Link this channel to RX/TX ring.
	 */
	KASSERT(idx >= 0 && idx < sc->hn_rx_ring_inuse,
	    ("invalid channel index %d, should > 0 && < %d",
	     idx, sc->hn_rx_ring_inuse));
	rxr = &sc->hn_rx_ring[idx];
	KASSERT((rxr->hn_rx_flags & HN_RX_FLAG_ATTACHED) == 0,
	    ("RX ring %d already attached", idx));
	rxr->hn_rx_flags |= HN_RX_FLAG_ATTACHED;

	if (bootverbose) {
		if_printf(sc->hn_ifp, "link RX ring %d to chan%u\n",
		    idx, vmbus_chan_id(chan));
	}

	if (idx < sc->hn_tx_ring_inuse) {
		txr = &sc->hn_tx_ring[idx];
		KASSERT((txr->hn_tx_flags & HN_TX_FLAG_ATTACHED) == 0,
		    ("TX ring %d already attached", idx));
		txr->hn_tx_flags |= HN_TX_FLAG_ATTACHED;

		txr->hn_chan = chan;
		if (bootverbose) {
			if_printf(sc->hn_ifp, "link TX ring %d to chan%u\n",
			    idx, vmbus_chan_id(chan));
		}
	}

	/* Bind this channel to a proper CPU. */
	vmbus_chan_cpu_set(chan, (sc->hn_cpu + idx) % mp_ncpus);

	/*
	 * Open this channel
	 */
	cbr.cbr = rxr->hn_br;
	cbr.cbr_paddr = rxr->hn_br_dma.hv_paddr;
	cbr.cbr_txsz = NETVSC_DEVICE_RING_BUFFER_SIZE;
	cbr.cbr_rxsz = NETVSC_DEVICE_RING_BUFFER_SIZE;
	error = vmbus_chan_open_br(chan, &cbr, NULL, 0, hn_chan_callback, rxr);
	if (error) {
		if_printf(sc->hn_ifp, "open chan%u failed: %d\n",
		    vmbus_chan_id(chan), error);
		rxr->hn_rx_flags &= ~HN_RX_FLAG_ATTACHED;
		if (txr != NULL)
			txr->hn_tx_flags &= ~HN_TX_FLAG_ATTACHED;
	}
	return (error);
}

static void
hn_chan_detach(struct hn_softc *sc, struct vmbus_channel *chan)
{
	struct hn_rx_ring *rxr;
	int idx;

	idx = vmbus_chan_subidx(chan);

	/*
	 * Link this channel to RX/TX ring.
	 */
	KASSERT(idx >= 0 && idx < sc->hn_rx_ring_inuse,
	    ("invalid channel index %d, should > 0 && < %d",
	     idx, sc->hn_rx_ring_inuse));
	rxr = &sc->hn_rx_ring[idx];
	KASSERT((rxr->hn_rx_flags & HN_RX_FLAG_ATTACHED),
	    ("RX ring %d is not attached", idx));
	rxr->hn_rx_flags &= ~HN_RX_FLAG_ATTACHED;

	if (idx < sc->hn_tx_ring_inuse) {
		struct hn_tx_ring *txr = &sc->hn_tx_ring[idx];

		KASSERT((txr->hn_tx_flags & HN_TX_FLAG_ATTACHED),
		    ("TX ring %d is not attached attached", idx));
		txr->hn_tx_flags &= ~HN_TX_FLAG_ATTACHED;
	}

	/*
	 * Close this channel.
	 *
	 * NOTE:
	 * Channel closing does _not_ destroy the target channel.
	 */
	vmbus_chan_close(chan);
}

static int
hn_attach_subchans(struct hn_softc *sc)
{
	struct vmbus_channel **subchans;
	int subchan_cnt = sc->hn_rx_ring_inuse - 1;
	int i, error = 0;

	if (subchan_cnt == 0)
		return (0);

	/* Attach the sub-channels. */
	subchans = vmbus_subchan_get(sc->hn_prichan, subchan_cnt);
	for (i = 0; i < subchan_cnt; ++i) {
		error = hn_chan_attach(sc, subchans[i]);
		if (error)
			break;
	}
	vmbus_subchan_rel(subchans, subchan_cnt);

	if (error) {
		if_printf(sc->hn_ifp, "sub-channels attach failed: %d\n", error);
	} else {
		if (bootverbose) {
			if_printf(sc->hn_ifp, "%d sub-channels attached\n",
			    subchan_cnt);
		}
	}
	return (error);
}

static void
hn_detach_allchans(struct hn_softc *sc)
{
	struct vmbus_channel **subchans;
	int subchan_cnt = sc->hn_rx_ring_inuse - 1;
	int i;

	if (subchan_cnt == 0)
		goto back;

	/* Detach the sub-channels. */
	subchans = vmbus_subchan_get(sc->hn_prichan, subchan_cnt);
	for (i = 0; i < subchan_cnt; ++i)
		hn_chan_detach(sc, subchans[i]);
	vmbus_subchan_rel(subchans, subchan_cnt);

back:
	/*
	 * Detach the primary channel, _after_ all sub-channels
	 * are detached.
	 */
	hn_chan_detach(sc, sc->hn_prichan);

	/* Wait for sub-channels to be destroyed, if any. */
	vmbus_subchan_drain(sc->hn_prichan);

#ifdef INVARIANTS
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		KASSERT((sc->hn_rx_ring[i].hn_rx_flags &
		    HN_RX_FLAG_ATTACHED) == 0,
		    ("%dth RX ring is still attached", i));
	}
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		KASSERT((sc->hn_tx_ring[i].hn_tx_flags &
		    HN_TX_FLAG_ATTACHED) == 0,
		    ("%dth TX ring is still attached", i));
	}
#endif
}

static int
hn_synth_alloc_subchans(struct hn_softc *sc, int *nsubch)
{
	struct vmbus_channel **subchans;
	int nchan, rxr_cnt, error;

	nchan = *nsubch + 1;
	if (nchan == 1) {
		/*
		 * Multiple RX/TX rings are not requested.
		 */
		*nsubch = 0;
		return (0);
	}

	/*
	 * Query RSS capabilities, e.g. # of RX rings, and # of indirect
	 * table entries.
	 */
	error = hn_rndis_query_rsscaps(sc, &rxr_cnt);
	if (error) {
		/* No RSS; this is benign. */
		*nsubch = 0;
		return (0);
	}
	if (bootverbose) {
		if_printf(sc->hn_ifp, "RX rings offered %u, requested %d\n",
		    rxr_cnt, nchan);
	}

	if (nchan > rxr_cnt)
		nchan = rxr_cnt;
	if (nchan == 1) {
		if_printf(sc->hn_ifp, "only 1 channel is supported, no vRSS\n");
		*nsubch = 0;
		return (0);
	}

	/*
	 * Allocate sub-channels from NVS.
	 */
	*nsubch = nchan - 1;
	error = hn_nvs_alloc_subchans(sc, nsubch);
	if (error || *nsubch == 0) {
		/* Failed to allocate sub-channels. */
		*nsubch = 0;
		return (0);
	}

	/*
	 * Wait for all sub-channels to become ready before moving on.
	 */
	subchans = vmbus_subchan_get(sc->hn_prichan, *nsubch);
	vmbus_subchan_rel(subchans, *nsubch);
	return (0);
}

static int
hn_synth_attach(struct hn_softc *sc, int mtu)
{
	struct ndis_rssprm_toeplitz *rss = &sc->hn_rss;
	int error, nsubch, nchan, i;
	uint32_t old_caps;

	KASSERT((sc->hn_flags & HN_FLAG_SYNTH_ATTACHED) == 0,
	    ("synthetic parts were attached"));

	/* Save capabilities for later verification. */
	old_caps = sc->hn_caps;
	sc->hn_caps = 0;

	/*
	 * Attach the primary channel _before_ attaching NVS and RNDIS.
	 */
	error = hn_chan_attach(sc, sc->hn_prichan);
	if (error)
		return (error);

	/*
	 * Attach NVS.
	 */
	error = hn_nvs_attach(sc, mtu);
	if (error)
		return (error);

	/*
	 * Attach RNDIS _after_ NVS is attached.
	 */
	error = hn_rndis_attach(sc, mtu);
	if (error)
		return (error);

	/*
	 * Make sure capabilities are not changed.
	 */
	if (device_is_attached(sc->hn_dev) && old_caps != sc->hn_caps) {
		if_printf(sc->hn_ifp, "caps mismatch old 0x%08x, new 0x%08x\n",
		    old_caps, sc->hn_caps);
		/* Restore old capabilities and abort. */
		sc->hn_caps = old_caps;
		return ENXIO;
	}

	/*
	 * Allocate sub-channels for multi-TX/RX rings.
	 *
	 * NOTE:
	 * The # of RX rings that can be used is equivalent to the # of
	 * channels to be requested.
	 */
	nsubch = sc->hn_rx_ring_cnt - 1;
	error = hn_synth_alloc_subchans(sc, &nsubch);
	if (error)
		return (error);

	nchan = nsubch + 1;
	if (nchan == 1) {
		/* Only the primary channel can be used; done */
		goto back;
	}

	/*
	 * Configure RSS key and indirect table _after_ all sub-channels
	 * are allocated.
	 */

	if ((sc->hn_flags & HN_FLAG_HAS_RSSKEY) == 0) {
		/*
		 * RSS key is not set yet; set it to the default RSS key.
		 */
		if (bootverbose)
			if_printf(sc->hn_ifp, "setup default RSS key\n");
		memcpy(rss->rss_key, hn_rss_key_default, sizeof(rss->rss_key));
		sc->hn_flags |= HN_FLAG_HAS_RSSKEY;
	}

	if ((sc->hn_flags & HN_FLAG_HAS_RSSIND) == 0) {
		/*
		 * RSS indirect table is not set yet; set it up in round-
		 * robin fashion.
		 */
		if (bootverbose) {
			if_printf(sc->hn_ifp, "setup default RSS indirect "
			    "table\n");
		}
		/* TODO: Take ndis_rss_caps.ndis_nind into account. */
		for (i = 0; i < NDIS_HASH_INDCNT; ++i)
			rss->rss_ind[i] = i % nchan;
		sc->hn_flags |= HN_FLAG_HAS_RSSIND;
	} else {
		/*
		 * # of usable channels may be changed, so we have to
		 * make sure that all entries in RSS indirect table
		 * are valid.
		 */
		hn_rss_ind_fixup(sc, nchan);
	}

	error = hn_rndis_conf_rss(sc, NDIS_RSS_FLAG_NONE);
	if (error) {
		/*
		 * Failed to configure RSS key or indirect table; only
		 * the primary channel can be used.
		 */
		nchan = 1;
	}
back:
	/*
	 * Set the # of TX/RX rings that could be used according to
	 * the # of channels that NVS offered.
	 */
	hn_set_ring_inuse(sc, nchan);

	/*
	 * Attach the sub-channels, if any.
	 */
	error = hn_attach_subchans(sc);
	if (error)
		return (error);

	sc->hn_flags |= HN_FLAG_SYNTH_ATTACHED;
	return (0);
}

/*
 * NOTE:
 * The interface must have been suspended though hn_suspend(), before
 * this function get called.
 */
static void
hn_synth_detach(struct hn_softc *sc)
{
	HN_LOCK_ASSERT(sc);

	KASSERT(sc->hn_flags & HN_FLAG_SYNTH_ATTACHED,
	    ("synthetic parts were not attached"));

	/* Detach the RNDIS first. */
	hn_rndis_detach(sc);

	/* Detach NVS. */
	hn_nvs_detach(sc);

	/* Detach all of the channels. */
	hn_detach_allchans(sc);

	sc->hn_flags &= ~HN_FLAG_SYNTH_ATTACHED;
}

static void
hn_set_ring_inuse(struct hn_softc *sc, int ring_cnt)
{
	KASSERT(ring_cnt > 0 && ring_cnt <= sc->hn_rx_ring_cnt,
	    ("invalid ring count %d", ring_cnt));

	if (sc->hn_tx_ring_cnt > ring_cnt)
		sc->hn_tx_ring_inuse = ring_cnt;
	else
		sc->hn_tx_ring_inuse = sc->hn_tx_ring_cnt;
	sc->hn_rx_ring_inuse = ring_cnt;

	if (bootverbose) {
		if_printf(sc->hn_ifp, "%d TX ring, %d RX ring\n",
		    sc->hn_tx_ring_inuse, sc->hn_rx_ring_inuse);
	}
}

static void
hn_rx_drain(struct vmbus_channel *chan)
{

	while (!vmbus_chan_rx_empty(chan) || !vmbus_chan_tx_empty(chan))
		pause("waitch", 1);
	vmbus_chan_intr_drain(chan);
}

static void
hn_suspend_data(struct hn_softc *sc)
{
	struct vmbus_channel **subch = NULL;
	int i, nsubch;

	HN_LOCK_ASSERT(sc);

	/*
	 * Suspend TX.
	 */
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i) {
		struct hn_tx_ring *txr = &sc->hn_tx_ring[i];

		mtx_lock(&txr->hn_tx_lock);
		txr->hn_suspended = 1;
		mtx_unlock(&txr->hn_tx_lock);
		/* No one is able send more packets now. */

		/* Wait for all pending sends to finish. */
		while (hn_tx_ring_pending(txr))
			pause("hnwtx", 1 /* 1 tick */);

		taskqueue_drain(txr->hn_tx_taskq, &txr->hn_tx_task);
		taskqueue_drain(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}

	/*
	 * Disable RX by clearing RX filter.
	 */
	hn_rndis_set_rxfilter(sc, 0);
	sc->hn_rx_filter = 0;

	/*
	 * Give RNDIS enough time to flush all pending data packets.
	 */
	pause("waitrx", (200 * hz) / 1000);

	/*
	 * Drain RX/TX bufrings and interrupts.
	 */
	nsubch = sc->hn_rx_ring_inuse - 1;
	if (nsubch > 0)
		subch = vmbus_subchan_get(sc->hn_prichan, nsubch);

	if (subch != NULL) {
		for (i = 0; i < nsubch; ++i)
			hn_rx_drain(subch[i]);
	}
	hn_rx_drain(sc->hn_prichan);

	if (subch != NULL)
		vmbus_subchan_rel(subch, nsubch);
}

static void
hn_suspend_mgmt_taskfunc(void *xsc, int pending __unused)
{

	((struct hn_softc *)xsc)->hn_mgmt_taskq = NULL;
}

static void
hn_suspend_mgmt(struct hn_softc *sc)
{
	struct task task;

	HN_LOCK_ASSERT(sc);

	/*
	 * Make sure that hn_mgmt_taskq0 can nolonger be accessed
	 * through hn_mgmt_taskq.
	 */
	TASK_INIT(&task, 0, hn_suspend_mgmt_taskfunc, sc);
	vmbus_chan_run_task(sc->hn_prichan, &task);

	/*
	 * Make sure that all pending management tasks are completed.
	 */
	taskqueue_drain(sc->hn_mgmt_taskq0, &sc->hn_netchg_init);
	taskqueue_drain_timeout(sc->hn_mgmt_taskq0, &sc->hn_netchg_status);
	taskqueue_drain_all(sc->hn_mgmt_taskq0);
}

static void
hn_suspend(struct hn_softc *sc)
{

	if (sc->hn_ifp->if_drv_flags & IFF_DRV_RUNNING)
		hn_suspend_data(sc);
	hn_suspend_mgmt(sc);
}

static void
hn_tx_resume(struct hn_softc *sc, int tx_ring_cnt)
{
	int i;

	KASSERT(tx_ring_cnt <= sc->hn_tx_ring_cnt,
	    ("invalid TX ring count %d", tx_ring_cnt));

	for (i = 0; i < tx_ring_cnt; ++i) {
		struct hn_tx_ring *txr = &sc->hn_tx_ring[i];

		mtx_lock(&txr->hn_tx_lock);
		txr->hn_suspended = 0;
		mtx_unlock(&txr->hn_tx_lock);
	}
}

static void
hn_resume_data(struct hn_softc *sc)
{
	int i;

	HN_LOCK_ASSERT(sc);

	/*
	 * Re-enable RX.
	 */
	hn_set_rxfilter(sc);

	/*
	 * Make sure to clear suspend status on "all" TX rings,
	 * since hn_tx_ring_inuse can be changed after
	 * hn_suspend_data().
	 */
	hn_tx_resume(sc, sc->hn_tx_ring_cnt);

	if (!hn_use_if_start) {
		/*
		 * Flush unused drbrs, since hn_tx_ring_inuse may be
		 * reduced.
		 */
		for (i = sc->hn_tx_ring_inuse; i < sc->hn_tx_ring_cnt; ++i)
			hn_tx_ring_qflush(&sc->hn_tx_ring[i]);
	}

	/*
	 * Kick start TX.
	 */
	for (i = 0; i < sc->hn_tx_ring_inuse; ++i) {
		struct hn_tx_ring *txr = &sc->hn_tx_ring[i];

		/*
		 * Use txeof task, so that any pending oactive can be
		 * cleared properly.
		 */
		taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}
}

static void
hn_resume_mgmt(struct hn_softc *sc)
{

	sc->hn_mgmt_taskq = sc->hn_mgmt_taskq0;

	/*
	 * Kick off network change detection, if it was pending.
	 * If no network change was pending, start link status
	 * checks, which is more lightweight than network change
	 * detection.
	 */
	if (sc->hn_link_flags & HN_LINK_FLAG_NETCHG)
		hn_network_change(sc);
	else
		hn_link_status_update(sc);
}

static void
hn_resume(struct hn_softc *sc)
{

	if (sc->hn_ifp->if_drv_flags & IFF_DRV_RUNNING)
		hn_resume_data(sc);
	hn_resume_mgmt(sc);
}

static void
hn_nvs_handle_notify(struct hn_softc *sc, const struct vmbus_chanpkt_hdr *pkt)
{
	const struct hn_nvs_hdr *hdr;

	if (VMBUS_CHANPKT_DATALEN(pkt) < sizeof(*hdr)) {
		if_printf(sc->hn_ifp, "invalid nvs notify\n");
		return;
	}
	hdr = VMBUS_CHANPKT_CONST_DATA(pkt);

	if (hdr->nvs_type == HN_NVS_TYPE_TXTBL_NOTE) {
		/* Useless; ignore */
		return;
	}
	if_printf(sc->hn_ifp, "got notify, nvs type %u\n", hdr->nvs_type);
}

static void
hn_nvs_handle_comp(struct hn_softc *sc, struct vmbus_channel *chan,
    const struct vmbus_chanpkt_hdr *pkt)
{
	struct hn_send_ctx *sndc;

	sndc = (struct hn_send_ctx *)(uintptr_t)pkt->cph_xactid;
	sndc->hn_cb(sndc, sc, chan, VMBUS_CHANPKT_CONST_DATA(pkt),
	    VMBUS_CHANPKT_DATALEN(pkt));
	/*
	 * NOTE:
	 * 'sndc' CAN NOT be accessed anymore, since it can be freed by
	 * its callback.
	 */
}

static void
hn_nvs_handle_rxbuf(struct hn_softc *sc, struct hn_rx_ring *rxr,
    struct vmbus_channel *chan, const struct vmbus_chanpkt_hdr *pkthdr)
{
	const struct vmbus_chanpkt_rxbuf *pkt;
	const struct hn_nvs_hdr *nvs_hdr;
	int count, i, hlen;

	if (__predict_false(VMBUS_CHANPKT_DATALEN(pkthdr) < sizeof(*nvs_hdr))) {
		if_printf(rxr->hn_ifp, "invalid nvs RNDIS\n");
		return;
	}
	nvs_hdr = VMBUS_CHANPKT_CONST_DATA(pkthdr);

	/* Make sure that this is a RNDIS message. */
	if (__predict_false(nvs_hdr->nvs_type != HN_NVS_TYPE_RNDIS)) {
		if_printf(rxr->hn_ifp, "nvs type %u, not RNDIS\n",
		    nvs_hdr->nvs_type);
		return;
	}

	hlen = VMBUS_CHANPKT_GETLEN(pkthdr->cph_hlen);
	if (__predict_false(hlen < sizeof(*pkt))) {
		if_printf(rxr->hn_ifp, "invalid rxbuf chanpkt\n");
		return;
	}
	pkt = (const struct vmbus_chanpkt_rxbuf *)pkthdr;

	if (__predict_false(pkt->cp_rxbuf_id != HN_NVS_RXBUF_SIG)) {
		if_printf(rxr->hn_ifp, "invalid rxbuf_id 0x%08x\n",
		    pkt->cp_rxbuf_id);
		return;
	}

	count = pkt->cp_rxbuf_cnt;
	if (__predict_false(hlen <
	    __offsetof(struct vmbus_chanpkt_rxbuf, cp_rxbuf[count]))) {
		if_printf(rxr->hn_ifp, "invalid rxbuf_cnt %d\n", count);
		return;
	}

	/* Each range represents 1 RNDIS pkt that contains 1 Ethernet frame */
	for (i = 0; i < count; ++i) {
		int ofs, len;

		ofs = pkt->cp_rxbuf[i].rb_ofs;
		len = pkt->cp_rxbuf[i].rb_len;
		if (__predict_false(ofs + len > NETVSC_RECEIVE_BUFFER_SIZE)) {
			if_printf(rxr->hn_ifp, "%dth RNDIS msg overflow rxbuf, "
			    "ofs %d, len %d\n", i, ofs, len);
			continue;
		}
		hv_rf_on_receive(sc, rxr, rxr->hn_rxbuf + ofs, len);
	}
	
	/*
	 * Moved completion call back here so that all received 
	 * messages (not just data messages) will trigger a response
	 * message back to the host.
	 */
	hn_nvs_ack_rxbuf(chan, pkt->cp_hdr.cph_xactid);
}

/*
 * Net VSC on receive completion
 *
 * Send a receive completion packet to RNDIS device (ie NetVsp)
 */
static void
hn_nvs_ack_rxbuf(struct vmbus_channel *chan, uint64_t tid)
{
	struct hn_nvs_rndis_ack ack;
	int retries = 0;
	int ret = 0;
	
	ack.nvs_type = HN_NVS_TYPE_RNDIS_ACK;
	ack.nvs_status = HN_NVS_STATUS_OK;

retry_send_cmplt:
	/* Send the completion */
	ret = vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_COMP,
	    VMBUS_CHANPKT_FLAG_NONE, &ack, sizeof(ack), tid);
	if (ret == 0) {
		/* success */
		/* no-op */
	} else if (ret == EAGAIN) {
		/* no more room... wait a bit and attempt to retry 3 times */
		retries++;

		if (retries < 4) {
			DELAY(100);
			goto retry_send_cmplt;
		}
	}
}

static void
hn_chan_callback(struct vmbus_channel *chan, void *xrxr)
{
	struct hn_rx_ring *rxr = xrxr;
	struct hn_softc *sc = rxr->hn_ifp->if_softc;
	void *buffer;
	int bufferlen = NETVSC_PACKET_SIZE;

	buffer = rxr->hn_rdbuf;
	do {
		struct vmbus_chanpkt_hdr *pkt = buffer;
		uint32_t bytes_rxed;
		int ret;

		bytes_rxed = bufferlen;
		ret = vmbus_chan_recv_pkt(chan, pkt, &bytes_rxed);
		if (ret == 0) {
			switch (pkt->cph_type) {
			case VMBUS_CHANPKT_TYPE_COMP:
				hn_nvs_handle_comp(sc, chan, pkt);
				break;
			case VMBUS_CHANPKT_TYPE_RXBUF:
				hn_nvs_handle_rxbuf(sc, rxr, chan, pkt);
				break;
			case VMBUS_CHANPKT_TYPE_INBAND:
				hn_nvs_handle_notify(sc, pkt);
				break;
			default:
				if_printf(rxr->hn_ifp,
				    "unknown chan pkt %u\n",
				    pkt->cph_type);
				break;
			}
		} else if (ret == ENOBUFS) {
			/* Handle large packet */
			if (bufferlen > NETVSC_PACKET_SIZE) {
				free(buffer, M_NETVSC);
				buffer = NULL;
			}

			/* alloc new buffer */
			buffer = malloc(bytes_rxed, M_NETVSC, M_NOWAIT);
			if (buffer == NULL) {
				if_printf(rxr->hn_ifp,
				    "hv_cb malloc buffer failed, len=%u\n",
				    bytes_rxed);
				bufferlen = 0;
				break;
			}
			bufferlen = bytes_rxed;
		} else {
			/* No more packets */
			break;
		}
	} while (1);

	if (bufferlen > NETVSC_PACKET_SIZE)
		free(buffer, M_NETVSC);

	hv_rf_channel_rollup(rxr, rxr->hn_txr);
}

static void
hn_tx_taskq_create(void *arg __unused)
{
	if (!hn_share_tx_taskq)
		return;

	hn_tx_taskq = taskqueue_create("hn_tx", M_WAITOK,
	    taskqueue_thread_enqueue, &hn_tx_taskq);
	if (hn_bind_tx_taskq >= 0) {
		int cpu = hn_bind_tx_taskq;
		cpuset_t cpu_set;

		if (cpu > mp_ncpus - 1)
			cpu = mp_ncpus - 1;
		CPU_SETOF(cpu, &cpu_set);
		taskqueue_start_threads_cpuset(&hn_tx_taskq, 1, PI_NET,
		    &cpu_set, "hn tx");
	} else {
		taskqueue_start_threads(&hn_tx_taskq, 1, PI_NET, "hn tx");
	}
}
SYSINIT(hn_txtq_create, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    hn_tx_taskq_create, NULL);

static void
hn_tx_taskq_destroy(void *arg __unused)
{
	if (hn_tx_taskq != NULL)
		taskqueue_free(hn_tx_taskq);
}
SYSUNINIT(hn_txtq_destroy, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    hn_tx_taskq_destroy, NULL);

static device_method_t netvsc_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         netvsc_probe),
        DEVMETHOD(device_attach,        netvsc_attach),
        DEVMETHOD(device_detach,        netvsc_detach),
        DEVMETHOD(device_shutdown,      netvsc_shutdown),

        { 0, 0 }
};

static driver_t netvsc_driver = {
        NETVSC_DEVNAME,
        netvsc_methods,
        sizeof(struct hn_softc)
};

static devclass_t netvsc_devclass;

DRIVER_MODULE(hn, vmbus, netvsc_driver, netvsc_devclass, 0, 0);
MODULE_VERSION(hn, 1);
MODULE_DEPEND(hn, vmbus, 1, 1, 1);
