/*-
 * Copyright (c) 2010 Juli Mallett <jmallett@FreeBSD.org>
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
 */

/*
 * Cavium Octeon Ethernet devices.
 *
 * XXX This file should be moved to if_octe.c
 * XXX The driver may have sufficient locking but we need locking to protect
 *     the interfaces presented here, right?
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_types.h>

#include "wrapper-cvmx-includes.h"
#include "cavium-ethernet.h"

#include "ethernet-common.h"

static int		octe_probe(device_t);
static int		octe_attach(device_t);
static int		octe_detach(device_t);
static int		octe_shutdown(device_t);

static void		octe_init(void *);
static void		octe_stop(void *);

static int		octe_medchange(struct ifnet *);
static void		octe_medstat(struct ifnet *, struct ifmediareq *);

static int		octe_ioctl(struct ifnet *, u_long, caddr_t);

static device_method_t octe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		octe_probe),
	DEVMETHOD(device_attach,	octe_attach),
	DEVMETHOD(device_detach,	octe_detach),
	DEVMETHOD(device_shutdown,	octe_shutdown),

	{ 0, 0 }
};

static driver_t octe_driver = {
	"octe",
	octe_methods,
	sizeof (cvm_oct_private_t),
};

static devclass_t octe_devclass;

DRIVER_MODULE(octe, octebus, octe_driver, octe_devclass, 0, 0);

static driver_t pow_driver = {
	"pow",
	octe_methods,
	sizeof (cvm_oct_private_t),
};

static devclass_t pow_devclass;

DRIVER_MODULE(pow, octebus, pow_driver, pow_devclass, 0, 0);

static int
octe_probe(device_t dev)
{
	return (0);
}

static int
octe_attach(device_t dev)
{
	struct ifnet *ifp;
	cvm_oct_private_t *priv;

	priv = device_get_softc(dev);
	ifp = priv->ifp;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	ifmedia_init(&priv->media, 0, octe_medchange, octe_medstat);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&priv->media, IFM_ETHER | IFM_AUTO);

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = octe_init;
	ifp->if_ioctl = octe_ioctl;

	priv->if_flags = ifp->if_flags;

	return (0);
}

static int
octe_detach(device_t dev)
{
	return (0);
}

static int
octe_shutdown(device_t dev)
{
	return (octe_detach(dev));
}

static void
octe_init(void *arg)
{
	struct ifnet *ifp;
	cvm_oct_private_t *priv;

	priv = arg;
	ifp = priv->ifp;

	if (priv->open != NULL)
		priv->open(ifp);
}

static void
octe_stop(void *arg)
{
	struct ifnet *ifp;
	cvm_oct_private_t *priv;

	priv = arg;
	ifp = priv->ifp;

	if (priv->stop != NULL)
		priv->stop(ifp);
}

static int
octe_medchange(struct ifnet *ifp)
{
	return (ENOTSUP);
}

static void
octe_medstat(struct ifnet *ifp, struct ifmediareq *ifm)
{
	cvm_oct_private_t *priv;
	cvmx_helper_link_info_t link_info;

	priv = ifp->if_softc;

	ifm->ifm_status = IFM_AVALID;
	ifm->ifm_active = IFT_ETHER;

	if (priv->poll == NULL)
		return;
	priv->poll(ifp);

	link_info.u64 = priv->link_info;

        if (!link_info.s.link_up)
		return;

	ifm->ifm_status |= IFM_ACTIVE;

	switch (link_info.s.speed) {
	case 10:
		ifm->ifm_active |= IFM_10_T;
		break;
	case 100:
		ifm->ifm_active |= IFM_100_TX;
		break;
	case 1000:
		ifm->ifm_active |= IFM_1000_T;
		break;
	case 10000:
		ifm->ifm_active |= IFM_10G_T;
		break;
	}

	if (link_info.s.full_duplex)
		ifm->ifm_active |= IFM_FDX;
	else
		ifm->ifm_active |= IFM_HDX;
}

static int
octe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	cvm_oct_private_t *priv;
	struct ifreq *ifr;
	int error;

	priv = ifp->if_softc;
	ifr = (struct ifreq *)data;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				octe_init(ifp);
		} else {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
				octe_stop(ifp);
		}
		priv->if_flags = ifp->if_flags;
		return (0);

	case SIOCSIFMTU:
		error = cvm_oct_common_change_mtu(ifp, ifr->ifr_mtu);
		if (error != 0)
			return (EINVAL);
		return (0);

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &priv->media, cmd);
		if (error != 0)
			return (error);
		return (0);
	
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error != 0)
			return (error);
		return (0);
	}
}
