/*-
 * Copyright (c) 2014 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include "opt_inet.h"
#include "opt_inet6.h"

#ifdef DEV_NETMAP
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/selinfo.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <machine/bus.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"

extern int fl_pad;	/* XXXNM */

SYSCTL_NODE(_hw, OID_AUTO, cxgbe, CTLFLAG_RD, 0, "cxgbe netmap parameters");

/*
 * 0 = normal netmap rx
 * 1 = black hole
 * 2 = supermassive black hole (buffer packing enabled)
 */
int black_hole = 0;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_black_hole, CTLFLAG_RDTUN, &black_hole, 0,
    "Sink incoming packets.");

int rx_ndesc = 256;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_rx_ndesc, CTLFLAG_RWTUN,
    &rx_ndesc, 0, "# of rx descriptors after which the hw cidx is updated.");

int holdoff_tmr_idx = 2;
SYSCTL_INT(_hw_cxgbe, OID_AUTO, nm_holdoff_tmr_idx, CTLFLAG_RWTUN,
    &holdoff_tmr_idx, 0, "Holdoff timer index for netmap rx queues.");

/*
 * Congestion drops.
 * -1: no congestion feedback (not recommended).
 *  0: backpressure the channel instead of dropping packets right away.
 *  1: no backpressure, drop packets for the congested queue immediately.
 */
static int nm_cong_drop = 1;
TUNABLE_INT("hw.cxgbe.nm_cong_drop", &nm_cong_drop);

/* netmap ifnet routines */
static void cxgbe_nm_init(void *);
static int cxgbe_nm_ioctl(struct ifnet *, unsigned long, caddr_t);
static int cxgbe_nm_transmit(struct ifnet *, struct mbuf *);
static void cxgbe_nm_qflush(struct ifnet *);

static int cxgbe_nm_init_synchronized(struct vi_info *);
static int cxgbe_nm_uninit_synchronized(struct vi_info *);

/* T4 netmap VI (ncxgbe) interface */
static int ncxgbe_probe(device_t);
static int ncxgbe_attach(device_t);
static int ncxgbe_detach(device_t);
static device_method_t ncxgbe_methods[] = {
	DEVMETHOD(device_probe,		ncxgbe_probe),
	DEVMETHOD(device_attach,	ncxgbe_attach),
	DEVMETHOD(device_detach,	ncxgbe_detach),
	{ 0, 0 }
};
static driver_t ncxgbe_driver = {
	"ncxgbe",
	ncxgbe_methods,
	sizeof(struct vi_info)
};

/* T5 netmap VI (ncxl) interface */
static driver_t ncxl_driver = {
	"ncxl",
	ncxgbe_methods,
	sizeof(struct vi_info)
};

static void
cxgbe_nm_init(void *arg)
{
	struct vi_info *vi = arg;
	struct adapter *sc = vi->pi->adapter;

	if (begin_synchronized_op(sc, vi, SLEEP_OK | INTR_OK, "t4nminit") != 0)
		return;
	cxgbe_nm_init_synchronized(vi);
	end_synchronized_op(sc, 0);

	return;
}

static int
cxgbe_nm_init_synchronized(struct vi_info *vi)
{
	struct adapter *sc = vi->pi->adapter;
	struct ifnet *ifp = vi->ifp;
	int rc = 0;

	ASSERT_SYNCHRONIZED_OP(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return (0);	/* already running */

	if (!(sc->flags & FULL_INIT_DONE) &&
	    ((rc = adapter_full_init(sc)) != 0))
		return (rc);	/* error message displayed already */

	if (!(vi->flags & VI_INIT_DONE) &&
	    ((rc = vi_full_init(vi)) != 0))
		return (rc);	/* error message displayed already */

	rc = update_mac_settings(ifp, XGMAC_ALL);
	if (rc)
		return (rc);	/* error message displayed already */

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	callout_reset(&vi->tick, hz, vi_tick, vi);

	return (rc);
}

static int
cxgbe_nm_uninit_synchronized(struct vi_info *vi)
{
#ifdef INVARIANTS
	struct adapter *sc = vi->pi->adapter;
#endif
	struct ifnet *ifp = vi->ifp;

	ASSERT_SYNCHRONIZED_OP(sc);

	callout_stop(&vi->tick);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	return (0);
}

static int
cxgbe_nm_ioctl(struct ifnet *ifp, unsigned long cmd, caddr_t data)
{
	int rc = 0, mtu, flags;
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->pi->adapter;
	struct ifreq *ifr = (struct ifreq *)data;
	uint32_t mask;

	MPASS(vi->ifp == ifp);

	switch (cmd) {
	case SIOCSIFMTU:
		mtu = ifr->ifr_mtu;
		if ((mtu < ETHERMIN) || (mtu > ETHERMTU_JUMBO))
			return (EINVAL);

		rc = begin_synchronized_op(sc, vi, SLEEP_OK | INTR_OK, "t4nmtu");
		if (rc)
			return (rc);
		ifp->if_mtu = mtu;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			rc = update_mac_settings(ifp, XGMAC_MTU);
		end_synchronized_op(sc, 0);
		break;

	case SIOCSIFFLAGS:
		rc = begin_synchronized_op(sc, vi, SLEEP_OK | INTR_OK, "t4nflg");
		if (rc)
			return (rc);

		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = vi->if_flags;
				if ((ifp->if_flags ^ flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					rc = update_mac_settings(ifp,
					    XGMAC_PROMISC | XGMAC_ALLMULTI);
				}
			} else
				rc = cxgbe_nm_init_synchronized(vi);
			vi->if_flags = ifp->if_flags;
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			rc = cxgbe_nm_uninit_synchronized(vi);
		end_synchronized_op(sc, 0);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI: /* these two are called with a mutex held :-( */
		rc = begin_synchronized_op(sc, vi, HOLD_LOCK, "t4nmulti");
		if (rc)
			return (rc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			rc = update_mac_settings(ifp, XGMAC_MCADDRS);
		end_synchronized_op(sc, LOCK_HELD);
		break;

	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);
		}
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
			ifp->if_hwassist ^= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);
		}
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		ifmedia_ioctl(ifp, ifr, &vi->media, cmd);
		break;

	default:
		rc = ether_ioctl(ifp, cmd, data);
	}

	return (rc);
}

static int
cxgbe_nm_transmit(struct ifnet *ifp, struct mbuf *m)
{

	m_freem(m);
	return (0);
}

static void
cxgbe_nm_qflush(struct ifnet *ifp)
{

	return;
}

static int
alloc_nm_rxq_hwq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq, int cong)
{
	int rc, cntxt_id, i;
	__be32 v;
	struct adapter *sc = vi->pi->adapter;
	struct sge_params *sp = &sc->params.sge;
	struct netmap_adapter *na = NA(vi->ifp);
	struct fw_iq_cmd c;

	MPASS(na != NULL);
	MPASS(nm_rxq->iq_desc != NULL);
	MPASS(nm_rxq->fl_desc != NULL);

	bzero(nm_rxq->iq_desc, vi->qsize_rxq * IQ_ESIZE);
	bzero(nm_rxq->fl_desc, na->num_rx_desc * EQ_ESIZE + sp->spg_len);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(sc->pf) |
	    V_FW_IQ_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_IQ_CMD_ALLOC | F_FW_IQ_CMD_IQSTART |
	    FW_LEN16(c));
	if (vi->flags & INTR_RXQ) {
		KASSERT(nm_rxq->intr_idx < sc->intr_count,
		    ("%s: invalid direct intr_idx %d", __func__,
		    nm_rxq->intr_idx));
		v = V_FW_IQ_CMD_IQANDSTINDEX(nm_rxq->intr_idx);
	} else {
		CXGBE_UNIMPLEMENTED(__func__);	/* XXXNM: needs review */
		v = V_FW_IQ_CMD_IQANDSTINDEX(nm_rxq->intr_idx) |
		    F_FW_IQ_CMD_IQANDST;
	}
	c.type_to_iqandstindex = htobe32(v |
	    V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
	    V_FW_IQ_CMD_VIID(vi->viid) |
	    V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_INTERRUPT));
	c.iqdroprss_to_iqesize = htobe16(V_FW_IQ_CMD_IQPCIECH(vi->pi->tx_chan) |
	    F_FW_IQ_CMD_IQGTSMODE |
	    V_FW_IQ_CMD_IQINTCNTTHRESH(0) |
	    V_FW_IQ_CMD_IQESIZE(ilog2(IQ_ESIZE) - 4));
	c.iqsize = htobe16(vi->qsize_rxq);
	c.iqaddr = htobe64(nm_rxq->iq_ba);
	if (cong >= 0) {
		c.iqns_to_fl0congen = htobe32(F_FW_IQ_CMD_IQFLINTCONGEN |
		    V_FW_IQ_CMD_FL0CNGCHMAP(cong) | F_FW_IQ_CMD_FL0CONGCIF |
		    F_FW_IQ_CMD_FL0CONGEN);
	}
	c.iqns_to_fl0congen |=
	    htobe32(V_FW_IQ_CMD_FL0HOSTFCMODE(X_HOSTFCMODE_NONE) |
		F_FW_IQ_CMD_FL0FETCHRO | F_FW_IQ_CMD_FL0DATARO |
		(fl_pad ? F_FW_IQ_CMD_FL0PADEN : 0) |
		(black_hole == 2 ? F_FW_IQ_CMD_FL0PACKEN : 0));
	c.fl0dcaen_to_fl0cidxfthresh =
	    htobe16(V_FW_IQ_CMD_FL0FBMIN(X_FETCHBURSTMIN_128B) |
		V_FW_IQ_CMD_FL0FBMAX(X_FETCHBURSTMAX_512B));
	c.fl0size = htobe16(na->num_rx_desc / 8 + sp->spg_len / EQ_ESIZE);
	c.fl0addr = htobe64(nm_rxq->fl_ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create netmap ingress queue: %d\n", rc);
		return (rc);
	}

	nm_rxq->iq_cidx = 0;
	MPASS(nm_rxq->iq_sidx == vi->qsize_rxq - sp->spg_len / IQ_ESIZE);
	nm_rxq->iq_gen = F_RSPD_GEN;
	nm_rxq->iq_cntxt_id = be16toh(c.iqid);
	nm_rxq->iq_abs_id = be16toh(c.physiqid);
	cntxt_id = nm_rxq->iq_cntxt_id - sc->sge.iq_start;
	if (cntxt_id >= sc->sge.niq) {
		panic ("%s: nm_rxq->iq_cntxt_id (%d) more than the max (%d)",
		    __func__, cntxt_id, sc->sge.niq - 1);
	}
	sc->sge.iqmap[cntxt_id] = (void *)nm_rxq;

	nm_rxq->fl_cntxt_id = be16toh(c.fl0id);
	nm_rxq->fl_pidx = nm_rxq->fl_cidx = 0;
	MPASS(nm_rxq->fl_sidx == na->num_rx_desc);
	cntxt_id = nm_rxq->fl_cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.neq) {
		panic("%s: nm_rxq->fl_cntxt_id (%d) more than the max (%d)",
		    __func__, cntxt_id, sc->sge.neq - 1);
	}
	sc->sge.eqmap[cntxt_id] = (void *)nm_rxq;

	nm_rxq->fl_db_val = V_QID(nm_rxq->fl_cntxt_id) |
	    sc->chip_params->sge_fl_db;

	if (is_t5(sc) && cong >= 0) {
		uint32_t param, val;

		param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
		    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
		    V_FW_PARAMS_PARAM_YZ(nm_rxq->iq_cntxt_id);
		param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
		    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
		    V_FW_PARAMS_PARAM_YZ(nm_rxq->iq_cntxt_id);
		if (cong == 0)
			val = 1 << 19;
		else {
			val = 2 << 19;
			for (i = 0; i < 4; i++) {
				if (cong & (1 << i))
					val |= 1 << (i << 2);
			}
		}

		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &param, &val);
		if (rc != 0) {
			/* report error but carry on */
			device_printf(sc->dev,
			    "failed to set congestion manager context for "
			    "ingress queue %d: %d\n", nm_rxq->iq_cntxt_id, rc);
		}
	}

	t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS),
	    V_INGRESSQID(nm_rxq->iq_cntxt_id) |
	    V_SEINTARM(V_QINTR_TIMER_IDX(holdoff_tmr_idx)));

	return (rc);
}

static int
free_nm_rxq_hwq(struct vi_info *vi, struct sge_nm_rxq *nm_rxq)
{
	struct adapter *sc = vi->pi->adapter;
	int rc;

	rc = -t4_iq_free(sc, sc->mbox, sc->pf, 0, FW_IQ_TYPE_FL_INT_CAP,
	    nm_rxq->iq_cntxt_id, nm_rxq->fl_cntxt_id, 0xffff);
	if (rc != 0)
		device_printf(sc->dev, "%s: failed for iq %d, fl %d: %d\n",
		    __func__, nm_rxq->iq_cntxt_id, nm_rxq->fl_cntxt_id, rc);
	return (rc);
}

static int
alloc_nm_txq_hwq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	int rc, cntxt_id;
	size_t len;
	struct adapter *sc = vi->pi->adapter;
	struct netmap_adapter *na = NA(vi->ifp);
	struct fw_eq_eth_cmd c;

	MPASS(na != NULL);
	MPASS(nm_txq->desc != NULL);

	len = na->num_tx_desc * EQ_ESIZE + sc->params.sge.spg_len;
	bzero(nm_txq->desc, len);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_ETH_CMD_PFN(sc->pf) |
	    V_FW_EQ_ETH_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_ETH_CMD_ALLOC |
	    F_FW_EQ_ETH_CMD_EQSTART | FW_LEN16(c));
	c.autoequiqe_to_viid = htobe32(F_FW_EQ_ETH_CMD_AUTOEQUIQE |
	    F_FW_EQ_ETH_CMD_AUTOEQUEQE | V_FW_EQ_ETH_CMD_VIID(vi->viid));
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_ETH_CMD_HOSTFCMODE(X_HOSTFCMODE_NONE) |
		V_FW_EQ_ETH_CMD_PCIECHN(vi->pi->tx_chan) | F_FW_EQ_ETH_CMD_FETCHRO |
		V_FW_EQ_ETH_CMD_IQID(sc->sge.nm_rxq[nm_txq->iqidx].iq_cntxt_id));
	c.dcaen_to_eqsize = htobe32(V_FW_EQ_ETH_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		      V_FW_EQ_ETH_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		      V_FW_EQ_ETH_CMD_EQSIZE(len / EQ_ESIZE));
	c.eqaddr = htobe64(nm_txq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(vi->dev,
		    "failed to create netmap egress queue: %d\n", rc);
		return (rc);
	}

	nm_txq->cntxt_id = G_FW_EQ_ETH_CMD_EQID(be32toh(c.eqid_pkd));
	cntxt_id = nm_txq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.neq)
	    panic("%s: nm_txq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.neq - 1);
	sc->sge.eqmap[cntxt_id] = (void *)nm_txq;

	nm_txq->pidx = nm_txq->cidx = 0;
	MPASS(nm_txq->sidx == na->num_tx_desc);
	nm_txq->equiqidx = nm_txq->equeqidx = nm_txq->dbidx = 0;

	nm_txq->doorbells = sc->doorbells;
	if (isset(&nm_txq->doorbells, DOORBELL_UDB) ||
	    isset(&nm_txq->doorbells, DOORBELL_UDBWC) ||
	    isset(&nm_txq->doorbells, DOORBELL_WCWR)) {
		uint32_t s_qpp = sc->params.sge.eq_s_qpp;
		uint32_t mask = (1 << s_qpp) - 1;
		volatile uint8_t *udb;

		udb = sc->udbs_base + UDBS_DB_OFFSET;
		udb += (nm_txq->cntxt_id >> s_qpp) << PAGE_SHIFT;
		nm_txq->udb_qid = nm_txq->cntxt_id & mask;
		if (nm_txq->udb_qid >= PAGE_SIZE / UDBS_SEG_SIZE)
	    		clrbit(&nm_txq->doorbells, DOORBELL_WCWR);
		else {
			udb += nm_txq->udb_qid << UDBS_SEG_SHIFT;
			nm_txq->udb_qid = 0;
		}
		nm_txq->udb = (volatile void *)udb;
	}

	return (rc);
}

static int
free_nm_txq_hwq(struct vi_info *vi, struct sge_nm_txq *nm_txq)
{
	struct adapter *sc = vi->pi->adapter;
	int rc;

	rc = -t4_eth_eq_free(sc, sc->mbox, sc->pf, 0, nm_txq->cntxt_id);
	if (rc != 0)
		device_printf(sc->dev, "%s: failed for eq %d: %d\n", __func__,
		    nm_txq->cntxt_id, rc);
	return (rc);
}

static int
cxgbe_netmap_on(struct adapter *sc, struct vi_info *vi, struct ifnet *ifp,
    struct netmap_adapter *na)
{
	struct netmap_slot *slot;
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
	int rc, i, j, hwidx;
	struct hw_buf_info *hwb;
	uint16_t *rss;

	ASSERT_SYNCHRONIZED_OP(sc);

	if ((vi->flags & VI_INIT_DONE) == 0 ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return (EAGAIN);

	hwb = &sc->sge.hw_buf_info[0];
	for (i = 0; i < SGE_FLBUF_SIZES; i++, hwb++) {
		if (hwb->size == NETMAP_BUF_SIZE(na))
			break;
	}
	if (i >= SGE_FLBUF_SIZES) {
		if_printf(ifp, "no hwidx for netmap buffer size %d.\n",
		    NETMAP_BUF_SIZE(na));
		return (ENXIO);
	}
	hwidx = i;

	/* Must set caps before calling netmap_reset */
	nm_set_native_flags(na);

	for_each_nm_rxq(vi, i, nm_rxq) {
		alloc_nm_rxq_hwq(vi, nm_rxq, tnl_cong(vi->pi, nm_cong_drop));
		nm_rxq->fl_hwidx = hwidx;
		slot = netmap_reset(na, NR_RX, i, 0);
		MPASS(slot != NULL);	/* XXXNM: error check, not assert */

		/* We deal with 8 bufs at a time */
		MPASS((na->num_rx_desc & 7) == 0);
		MPASS(na->num_rx_desc == nm_rxq->fl_sidx);
		for (j = 0; j < nm_rxq->fl_sidx; j++) {
			uint64_t ba;

			PNMB(na, &slot[j], &ba);
			MPASS(ba != 0);
			nm_rxq->fl_desc[j] = htobe64(ba | hwidx);
		}
		j = nm_rxq->fl_pidx = nm_rxq->fl_sidx - 8;
		MPASS((j & 7) == 0);
		j /= 8;	/* driver pidx to hardware pidx */
		wmb();
		t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
		    nm_rxq->fl_db_val | V_PIDX(j));
	}

	for_each_nm_txq(vi, i, nm_txq) {
		alloc_nm_txq_hwq(vi, nm_txq);
		slot = netmap_reset(na, NR_TX, i, 0);
		MPASS(slot != NULL);	/* XXXNM: error check, not assert */
	}

	rss = malloc(vi->rss_size * sizeof (*rss), M_CXGBE, M_ZERO |
	    M_WAITOK);
	for (i = 0; i < vi->rss_size;) {
		for_each_nm_rxq(vi, j, nm_rxq) {
			rss[i++] = nm_rxq->iq_abs_id;
			if (i == vi->rss_size)
				break;
		}
	}
	rc = -t4_config_rss_range(sc, sc->mbox, vi->viid, 0, vi->rss_size,
	    rss, vi->rss_size);
	if (rc != 0)
		if_printf(ifp, "netmap rss_config failed: %d\n", rc);
	free(rss, M_CXGBE);

	rc = -t4_enable_vi(sc, sc->mbox, vi->viid, true, true);
	if (rc != 0)
		if_printf(ifp, "netmap enable_vi failed: %d\n", rc);

	return (rc);
}

static int
cxgbe_netmap_off(struct adapter *sc, struct vi_info *vi, struct ifnet *ifp,
    struct netmap_adapter *na)
{
	int rc, i;
	struct sge_nm_txq *nm_txq;
	struct sge_nm_rxq *nm_rxq;

	ASSERT_SYNCHRONIZED_OP(sc);

	if ((vi->flags & VI_INIT_DONE) == 0)
		return (0);

	rc = -t4_enable_vi(sc, sc->mbox, vi->viid, false, false);
	if (rc != 0)
		if_printf(ifp, "netmap disable_vi failed: %d\n", rc);
	nm_clear_native_flags(na);

	for_each_nm_txq(vi, i, nm_txq) {
		struct sge_qstat *spg = (void *)&nm_txq->desc[nm_txq->sidx];

		/* Wait for hw pidx to catch up ... */
		while (be16toh(nm_txq->pidx) != spg->pidx)
			pause("nmpidx", 1);

		/* ... and then for the cidx. */
		while (spg->pidx != spg->cidx)
			pause("nmcidx", 1);

		free_nm_txq_hwq(vi, nm_txq);
	}
	for_each_nm_rxq(vi, i, nm_rxq) {
		free_nm_rxq_hwq(vi, nm_rxq);
	}

	return (rc);
}

static int
cxgbe_netmap_reg(struct netmap_adapter *na, int on)
{
	struct ifnet *ifp = na->ifp;
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->pi->adapter;
	int rc;

	rc = begin_synchronized_op(sc, vi, SLEEP_OK | INTR_OK, "t4nmreg");
	if (rc != 0)
		return (rc);
	if (on)
		rc = cxgbe_netmap_on(sc, vi, ifp, na);
	else
		rc = cxgbe_netmap_off(sc, vi, ifp, na);
	end_synchronized_op(sc, 0);

	return (rc);
}

/* How many packets can a single type1 WR carry in n descriptors */
static inline int
ndesc_to_npkt(const int n)
{

	MPASS(n > 0 && n <= SGE_MAX_WR_NDESC);

	return (n * 2 - 1);
}
#define MAX_NPKT_IN_TYPE1_WR	(ndesc_to_npkt(SGE_MAX_WR_NDESC))

/* Space (in descriptors) needed for a type1 WR that carries n packets */
static inline int
npkt_to_ndesc(const int n)
{

	MPASS(n > 0 && n <= MAX_NPKT_IN_TYPE1_WR);

	return ((n + 2) / 2);
}

/* Space (in 16B units) needed for a type1 WR that carries n packets */
static inline int
npkt_to_len16(const int n)
{

	MPASS(n > 0 && n <= MAX_NPKT_IN_TYPE1_WR);

	return (n * 2 + 1);
}

#define NMIDXDIFF(q, idx) IDXDIFF((q)->pidx, (q)->idx, (q)->sidx)

static void
ring_nm_txq_db(struct adapter *sc, struct sge_nm_txq *nm_txq)
{
	int n;
	u_int db = nm_txq->doorbells;

	MPASS(nm_txq->pidx != nm_txq->dbidx);

	n = NMIDXDIFF(nm_txq, dbidx);
	if (n > 1)
		clrbit(&db, DOORBELL_WCWR);
	wmb();

	switch (ffs(db) - 1) {
	case DOORBELL_UDB:
		*nm_txq->udb = htole32(V_QID(nm_txq->udb_qid) | V_PIDX(n));
		break;

	case DOORBELL_WCWR: {
		volatile uint64_t *dst, *src;

		/*
		 * Queues whose 128B doorbell segment fits in the page do not
		 * use relative qid (udb_qid is always 0).  Only queues with
		 * doorbell segments can do WCWR.
		 */
		KASSERT(nm_txq->udb_qid == 0 && n == 1,
		    ("%s: inappropriate doorbell (0x%x, %d, %d) for nm_txq %p",
		    __func__, nm_txq->doorbells, n, nm_txq->pidx, nm_txq));

		dst = (volatile void *)((uintptr_t)nm_txq->udb +
		    UDBS_WR_OFFSET - UDBS_DB_OFFSET);
		src = (void *)&nm_txq->desc[nm_txq->dbidx];
		while (src != (void *)&nm_txq->desc[nm_txq->dbidx + 1])
			*dst++ = *src++;
		wmb();
		break;
	}

	case DOORBELL_UDBWC:
		*nm_txq->udb = htole32(V_QID(nm_txq->udb_qid) | V_PIDX(n));
		wmb();
		break;

	case DOORBELL_KDB:
		t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
		    V_QID(nm_txq->cntxt_id) | V_PIDX(n));
		break;
	}
	nm_txq->dbidx = nm_txq->pidx;
}

int lazy_tx_credit_flush = 1;

/*
 * Write work requests to send 'npkt' frames and ring the doorbell to send them
 * on their way.  No need to check for wraparound.
 */
static void
cxgbe_nm_tx(struct adapter *sc, struct sge_nm_txq *nm_txq,
    struct netmap_kring *kring, int npkt, int npkt_remaining, int txcsum)
{
	struct netmap_ring *ring = kring->ring;
	struct netmap_slot *slot;
	const u_int lim = kring->nkr_num_slots - 1;
	struct fw_eth_tx_pkts_wr *wr = (void *)&nm_txq->desc[nm_txq->pidx];
	uint16_t len;
	uint64_t ba;
	struct cpl_tx_pkt_core *cpl;
	struct ulptx_sgl *usgl;
	int i, n;

	while (npkt) {
		n = min(npkt, MAX_NPKT_IN_TYPE1_WR);
		len = 0;

		wr = (void *)&nm_txq->desc[nm_txq->pidx];
		wr->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR));
		wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(npkt_to_len16(n)));
		wr->npkt = n;
		wr->r3 = 0;
		wr->type = 1;
		cpl = (void *)(wr + 1);

		for (i = 0; i < n; i++) {
			slot = &ring->slot[kring->nr_hwcur];
			PNMB(kring->na, slot, &ba);
			MPASS(ba != 0);

			cpl->ctrl0 = nm_txq->cpl_ctrl0;
			cpl->pack = 0;
			cpl->len = htobe16(slot->len);
			/*
			 * netmap(4) says "netmap does not use features such as
			 * checksum offloading, TCP segmentation offloading,
			 * encryption, VLAN encapsulation/decapsulation, etc."
			 *
			 * So the ncxl interfaces have tx hardware checksumming
			 * disabled by default.  But you can override netmap by
			 * enabling IFCAP_TXCSUM on the interface manully.
			 */
			cpl->ctrl1 = txcsum ? 0 :
			    htobe64(F_TXPKT_IPCSUM_DIS | F_TXPKT_L4CSUM_DIS);

			usgl = (void *)(cpl + 1);
			usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
			    V_ULPTX_NSGE(1));
			usgl->len0 = htobe32(slot->len);
			usgl->addr0 = htobe64(ba);

			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);
			cpl = (void *)(usgl + 1);
			MPASS(slot->len + len <= UINT16_MAX);
			len += slot->len;
			kring->nr_hwcur = nm_next(kring->nr_hwcur, lim);
		}
		wr->plen = htobe16(len);

		npkt -= n;
		nm_txq->pidx += npkt_to_ndesc(n);
		MPASS(nm_txq->pidx <= nm_txq->sidx);
		if (__predict_false(nm_txq->pidx == nm_txq->sidx)) {
			/*
			 * This routine doesn't know how to write WRs that wrap
			 * around.  Make sure it wasn't asked to.
			 */
			MPASS(npkt == 0);
			nm_txq->pidx = 0;
		}

		if (npkt == 0 && npkt_remaining == 0) {
			/* All done. */
			if (lazy_tx_credit_flush == 0) {
				wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ |
				    F_FW_WR_EQUIQ);
				nm_txq->equeqidx = nm_txq->pidx;
				nm_txq->equiqidx = nm_txq->pidx;
			}
			ring_nm_txq_db(sc, nm_txq);
			return;
		}

		if (NMIDXDIFF(nm_txq, equiqidx) >= nm_txq->sidx / 2) {
			wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ |
			    F_FW_WR_EQUIQ);
			nm_txq->equeqidx = nm_txq->pidx;
			nm_txq->equiqidx = nm_txq->pidx;
		} else if (NMIDXDIFF(nm_txq, equeqidx) >= 64) {
			wr->equiq_to_len16 |= htobe32(F_FW_WR_EQUEQ);
			nm_txq->equeqidx = nm_txq->pidx;
		}
		if (NMIDXDIFF(nm_txq, dbidx) >= 2 * SGE_MAX_WR_NDESC)
			ring_nm_txq_db(sc, nm_txq);
	}

	/* Will get called again. */
	MPASS(npkt_remaining);
}

/* How many contiguous free descriptors starting at pidx */
static inline int
contiguous_ndesc_available(struct sge_nm_txq *nm_txq)
{

	if (nm_txq->cidx > nm_txq->pidx)
		return (nm_txq->cidx - nm_txq->pidx - 1);
	else if (nm_txq->cidx > 0)
		return (nm_txq->sidx - nm_txq->pidx);
	else
		return (nm_txq->sidx - nm_txq->pidx - 1);
}

static int
reclaim_nm_tx_desc(struct sge_nm_txq *nm_txq)
{
	struct sge_qstat *spg = (void *)&nm_txq->desc[nm_txq->sidx];
	uint16_t hw_cidx = spg->cidx;	/* snapshot */
	struct fw_eth_tx_pkts_wr *wr;
	int n = 0;

	hw_cidx = be16toh(hw_cidx);

	while (nm_txq->cidx != hw_cidx) {
		wr = (void *)&nm_txq->desc[nm_txq->cidx];

		MPASS(wr->op_pkd == htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR)));
		MPASS(wr->type == 1);
		MPASS(wr->npkt > 0 && wr->npkt <= MAX_NPKT_IN_TYPE1_WR);

		n += wr->npkt;
		nm_txq->cidx += npkt_to_ndesc(wr->npkt);

		/*
		 * We never sent a WR that wrapped around so the credits coming
		 * back, WR by WR, should never cause the cidx to wrap around
		 * either.
		 */
		MPASS(nm_txq->cidx <= nm_txq->sidx);
		if (__predict_false(nm_txq->cidx == nm_txq->sidx))
			nm_txq->cidx = 0;
	}

	return (n);
}

static int
cxgbe_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->pi->adapter;
	struct sge_nm_txq *nm_txq = &sc->sge.nm_txq[vi->first_txq + kring->ring_id];
	const u_int head = kring->rhead;
	u_int reclaimed = 0;
	int n, d, npkt_remaining, ndesc_remaining, txcsum;

	/*
	 * Tx was at kring->nr_hwcur last time around and now we need to advance
	 * to kring->rhead.  Note that the driver's pidx moves independent of
	 * netmap's kring->nr_hwcur (pidx counts descriptors and the relation
	 * between descriptors and frames isn't 1:1).
	 */

	npkt_remaining = head >= kring->nr_hwcur ? head - kring->nr_hwcur :
	    kring->nkr_num_slots - kring->nr_hwcur + head;
	txcsum = ifp->if_capenable & (IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6);
	while (npkt_remaining) {
		reclaimed += reclaim_nm_tx_desc(nm_txq);
		ndesc_remaining = contiguous_ndesc_available(nm_txq);
		/* Can't run out of descriptors with packets still remaining */
		MPASS(ndesc_remaining > 0);

		/* # of desc needed to tx all remaining packets */
		d = (npkt_remaining / MAX_NPKT_IN_TYPE1_WR) * SGE_MAX_WR_NDESC;
		if (npkt_remaining % MAX_NPKT_IN_TYPE1_WR)
			d += npkt_to_ndesc(npkt_remaining % MAX_NPKT_IN_TYPE1_WR);

		if (d <= ndesc_remaining)
			n = npkt_remaining;
		else {
			/* Can't send all, calculate how many can be sent */
			n = (ndesc_remaining / SGE_MAX_WR_NDESC) *
			    MAX_NPKT_IN_TYPE1_WR;
			if (ndesc_remaining % SGE_MAX_WR_NDESC)
				n += ndesc_to_npkt(ndesc_remaining % SGE_MAX_WR_NDESC);
		}

		/* Send n packets and update nm_txq->pidx and kring->nr_hwcur */
		npkt_remaining -= n;
		cxgbe_nm_tx(sc, nm_txq, kring, n, npkt_remaining, txcsum);
	}
	MPASS(npkt_remaining == 0);
	MPASS(kring->nr_hwcur == head);
	MPASS(nm_txq->dbidx == nm_txq->pidx);

	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	if (reclaimed || flags & NAF_FORCE_RECLAIM || nm_kr_txempty(kring)) {
		reclaimed += reclaim_nm_tx_desc(nm_txq);
		kring->nr_hwtail += reclaimed;
		if (kring->nr_hwtail >= kring->nkr_num_slots)
			kring->nr_hwtail -= kring->nkr_num_slots;
	}

	return (0);
}

static int
cxgbe_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct netmap_ring *ring = kring->ring;
	struct ifnet *ifp = na->ifp;
	struct vi_info *vi = ifp->if_softc;
	struct adapter *sc = vi->pi->adapter;
	struct sge_nm_rxq *nm_rxq = &sc->sge.nm_rxq[vi->first_rxq + kring->ring_id];
	u_int const head = kring->rhead;
	u_int n;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;

	if (black_hole)
		return (0);	/* No updates ever. */

	if (netmap_no_pendintr || force_update) {
		kring->nr_hwtail = atomic_load_acq_32(&nm_rxq->fl_cidx);
		kring->nr_kflags &= ~NKR_PENDINTR;
	}

	/* Userspace done with buffers from kring->nr_hwcur to head */
	n = head >= kring->nr_hwcur ? head - kring->nr_hwcur :
	    kring->nkr_num_slots - kring->nr_hwcur + head;
	n &= ~7U;
	if (n > 0) {
		u_int fl_pidx = nm_rxq->fl_pidx;
		struct netmap_slot *slot = &ring->slot[fl_pidx];
		uint64_t ba;
		int i, dbinc = 0, hwidx = nm_rxq->fl_hwidx;

		/*
		 * We always deal with 8 buffers at a time.  We must have
		 * stopped at an 8B boundary (fl_pidx) last time around and we
		 * must have a multiple of 8B buffers to give to the freelist.
		 */
		MPASS((fl_pidx & 7) == 0);
		MPASS((n & 7) == 0);

		IDXINCR(kring->nr_hwcur, n, kring->nkr_num_slots);
		IDXINCR(nm_rxq->fl_pidx, n, nm_rxq->fl_sidx);

		while (n > 0) {
			for (i = 0; i < 8; i++, fl_pidx++, slot++) {
				PNMB(na, slot, &ba);
				MPASS(ba != 0);
				nm_rxq->fl_desc[fl_pidx] = htobe64(ba | hwidx);
				slot->flags &= ~NS_BUF_CHANGED;
				MPASS(fl_pidx <= nm_rxq->fl_sidx);
			}
			n -= 8;
			if (fl_pidx == nm_rxq->fl_sidx) {
				fl_pidx = 0;
				slot = &ring->slot[0];
			}
			if (++dbinc == 8 && n >= 32) {
				wmb();
				t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
				    nm_rxq->fl_db_val | V_PIDX(dbinc));
				dbinc = 0;
			}
		}
		MPASS(nm_rxq->fl_pidx == fl_pidx);

		if (dbinc > 0) {
			wmb();
			t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
			    nm_rxq->fl_db_val | V_PIDX(dbinc));
		}
	}

	return (0);
}

static int
ncxgbe_probe(device_t dev)
{
	char buf[128];
	struct vi_info *vi = device_get_softc(dev);

	snprintf(buf, sizeof(buf), "port %d netmap vi", vi->pi->port_id);
	device_set_desc_copy(dev, buf);

	return (BUS_PROBE_DEFAULT);
}

static int
ncxgbe_attach(device_t dev)
{
	struct vi_info *vi;
	struct port_info *pi;
	struct adapter *sc;
	struct netmap_adapter na;
	struct ifnet *ifp;
	int rc;

	vi = device_get_softc(dev);
	pi = vi->pi;
	sc = pi->adapter;

	/*
	 * Allocate a virtual interface exclusively for netmap use.  Give it the
	 * MAC address normally reserved for use by a TOE interface.  (The TOE
	 * driver on FreeBSD doesn't use it).
	 */
	rc = t4_alloc_vi_func(sc, sc->mbox, pi->tx_chan, sc->pf, 0, 1,
	    vi->hw_addr, &vi->rss_size, FW_VI_FUNC_OFLD, 0);
	if (rc < 0) {
		device_printf(dev, "unable to allocate netmap virtual "
		    "interface for port %d: %d\n", pi->port_id, -rc);
		return (-rc);
	}
	vi->viid = rc;
	vi->xact_addr_filt = -1;
	callout_init(&vi->tick, 1);

	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "Cannot allocate netmap ifnet\n");
		return (ENOMEM);
	}
	vi->ifp = ifp;
	ifp->if_softc = vi;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

	ifp->if_init = cxgbe_nm_init;
	ifp->if_ioctl = cxgbe_nm_ioctl;
	ifp->if_transmit = cxgbe_nm_transmit;
	ifp->if_qflush = cxgbe_nm_qflush;
	ifp->if_get_counter = cxgbe_get_counter;

	/*
	 * netmap(4) says "netmap does not use features such as checksum
	 * offloading, TCP segmentation offloading, encryption, VLAN
	 * encapsulation/decapsulation, etc."
	 *
	 * By default we comply with the statement above.  But we do declare the
	 * ifnet capable of L3/L4 checksumming so that a user can override
	 * netmap and have the hardware do the L3/L4 checksums.
	 */
	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_JUMBO_MTU |
	    IFCAP_HWCSUM_IPV6;
	ifp->if_capenable = 0;
	ifp->if_hwassist = 0;

	/* vi->media has already been setup by the caller */

	ether_ifattach(ifp, vi->hw_addr);

	device_printf(dev, "%d txq, %d rxq (netmap)\n", vi->ntxq, vi->nrxq);

	vi_sysctls(vi);

	/*
	 * Register with netmap in the kernel.
	 */
	bzero(&na, sizeof(na));

	na.ifp = ifp;
	na.na_flags = NAF_BDG_MAYSLEEP;

	/* Netmap doesn't know about the space reserved for the status page. */
	na.num_tx_desc = vi->qsize_txq - sc->params.sge.spg_len / EQ_ESIZE;

	/*
	 * The freelist's cidx/pidx drives netmap's rx cidx/pidx.  So
	 * num_rx_desc is based on the number of buffers that can be held in the
	 * freelist, and not the number of entries in the iq.  (These two are
	 * not exactly the same due to the space taken up by the status page).
	 */
	na.num_rx_desc = (vi->qsize_rxq / 8) * 8;
	na.nm_txsync = cxgbe_netmap_txsync;
	na.nm_rxsync = cxgbe_netmap_rxsync;
	na.nm_register = cxgbe_netmap_reg;
	na.num_tx_rings = vi->ntxq;
	na.num_rx_rings = vi->nrxq;
	netmap_attach(&na);	/* This adds IFCAP_NETMAP to if_capabilities */

	return (0);
}

static int
ncxgbe_detach(device_t dev)
{
	struct vi_info *vi;
	struct adapter *sc;

	vi = device_get_softc(dev);
	sc = vi->pi->adapter;

	doom_vi(sc, vi);

	netmap_detach(vi->ifp);
	ether_ifdetach(vi->ifp);
	cxgbe_nm_uninit_synchronized(vi);
	callout_drain(&vi->tick);
	vi_full_uninit(vi);
	ifmedia_removeall(&vi->media);
	if_free(vi->ifp);
	vi->ifp = NULL;
	t4_free_vi(sc, sc->mbox, sc->pf, 0, vi->viid);

	end_synchronized_op(sc, 0);

	return (0);
}

static void
handle_nm_fw6_msg(struct adapter *sc, struct ifnet *ifp,
    const struct cpl_fw6_msg *cpl)
{
	const struct cpl_sge_egr_update *egr;
	uint32_t oq;
	struct sge_nm_txq *nm_txq;

	if (cpl->type != FW_TYPE_RSSCPL && cpl->type != FW6_TYPE_RSSCPL)
		panic("%s: FW_TYPE 0x%x on nm_rxq.", __func__, cpl->type);

	/* data[0] is RSS header */
	egr = (const void *)&cpl->data[1];
	oq = be32toh(egr->opcode_qid);
	MPASS(G_CPL_OPCODE(oq) == CPL_SGE_EGR_UPDATE);
	nm_txq = (void *)sc->sge.eqmap[G_EGR_QID(oq) - sc->sge.eq_start];

	netmap_tx_irq(ifp, nm_txq->nid);
}

void
t4_nm_intr(void *arg)
{
	struct sge_nm_rxq *nm_rxq = arg;
	struct vi_info *vi = nm_rxq->vi;
	struct adapter *sc = vi->pi->adapter;
	struct ifnet *ifp = vi->ifp;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[nm_rxq->nid];
	struct netmap_ring *ring = kring->ring;
	struct iq_desc *d = &nm_rxq->iq_desc[nm_rxq->iq_cidx];
	uint32_t lq;
	u_int n = 0, work = 0;
	uint8_t opcode;
	uint32_t fl_cidx = atomic_load_acq_32(&nm_rxq->fl_cidx);
	u_int fl_credits = fl_cidx & 7;

	while ((d->rsp.u.type_gen & F_RSPD_GEN) == nm_rxq->iq_gen) {

		rmb();

		lq = be32toh(d->rsp.pldbuflen_qid);
		opcode = d->rss.opcode;

		switch (G_RSPD_TYPE(d->rsp.u.type_gen)) {
		case X_RSPD_TYPE_FLBUF:
			if (black_hole != 2) {
				/* No buffer packing so new buf every time */
				MPASS(lq & F_RSPD_NEWBUF);
			}

			/* fall through */

		case X_RSPD_TYPE_CPL:
			MPASS(opcode < NUM_CPL_CMDS);

			switch (opcode) {
			case CPL_FW4_MSG:
			case CPL_FW6_MSG:
				handle_nm_fw6_msg(sc, ifp,
				    (const void *)&d->cpl[0]);
				break;
			case CPL_RX_PKT:
				ring->slot[fl_cidx].len = G_RSPD_LEN(lq) -
				    sc->params.sge.fl_pktshift;
				ring->slot[fl_cidx].flags = kring->nkr_slot_flags;
				fl_cidx += (lq & F_RSPD_NEWBUF) ? 1 : 0;
				fl_credits += (lq & F_RSPD_NEWBUF) ? 1 : 0;
				if (__predict_false(fl_cidx == nm_rxq->fl_sidx))
					fl_cidx = 0;
				break;
			default:
				panic("%s: unexpected opcode 0x%x on nm_rxq %p",
				    __func__, opcode, nm_rxq);
			}
			break;

		case X_RSPD_TYPE_INTR:
			/* Not equipped to handle forwarded interrupts. */
			panic("%s: netmap queue received interrupt for iq %u\n",
			    __func__, lq);

		default:
			panic("%s: illegal response type %d on nm_rxq %p",
			    __func__, G_RSPD_TYPE(d->rsp.u.type_gen), nm_rxq);
		}

		d++;
		if (__predict_false(++nm_rxq->iq_cidx == nm_rxq->iq_sidx)) {
			nm_rxq->iq_cidx = 0;
			d = &nm_rxq->iq_desc[0];
			nm_rxq->iq_gen ^= F_RSPD_GEN;
		}

		if (__predict_false(++n == rx_ndesc)) {
			atomic_store_rel_32(&nm_rxq->fl_cidx, fl_cidx);
			if (black_hole && fl_credits >= 8) {
				fl_credits /= 8;
				IDXINCR(nm_rxq->fl_pidx, fl_credits * 8,
				    nm_rxq->fl_sidx);
				t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
				    nm_rxq->fl_db_val | V_PIDX(fl_credits));
				fl_credits = fl_cidx & 7;
			} else if (!black_hole) {
				netmap_rx_irq(ifp, nm_rxq->nid, &work);
				MPASS(work != 0);
			}
			t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS),
			    V_CIDXINC(n) | V_INGRESSQID(nm_rxq->iq_cntxt_id) |
			    V_SEINTARM(V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX)));
			n = 0;
		}
	}

	atomic_store_rel_32(&nm_rxq->fl_cidx, fl_cidx);
	if (black_hole) {
		fl_credits /= 8;
		IDXINCR(nm_rxq->fl_pidx, fl_credits * 8, nm_rxq->fl_sidx);
		t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
		    nm_rxq->fl_db_val | V_PIDX(fl_credits));
	} else
		netmap_rx_irq(ifp, nm_rxq->nid, &work);

	t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS), V_CIDXINC(n) |
	    V_INGRESSQID((u32)nm_rxq->iq_cntxt_id) |
	    V_SEINTARM(V_QINTR_TIMER_IDX(holdoff_tmr_idx)));
}

static devclass_t ncxgbe_devclass, ncxl_devclass;

DRIVER_MODULE(ncxgbe, cxgbe, ncxgbe_driver, ncxgbe_devclass, 0, 0);
MODULE_VERSION(ncxgbe, 1);

DRIVER_MODULE(ncxl, cxl, ncxl_driver, ncxl_devclass, 0, 0);
MODULE_VERSION(ncxl, 1);
#endif
