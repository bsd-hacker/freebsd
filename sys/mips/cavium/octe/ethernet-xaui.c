/*************************************************************************
Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Cavium Networks nor the names of
      its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

This Software, including technical data, may be subject to U.S. export  control laws, including the U.S. Export Administration Act and its  associated regulations, and may be subject to export or import  regulations in other countries.

TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.

*************************************************************************/
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <net/dst.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

extern int octeon_is_simulation(void);

static int cvm_oct_xaui_open(struct net_device *dev)
{
	cvmx_gmxx_prtx_cfg_t gmx_cfg;
	cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
	int interface = INTERFACE(priv->port);
	int index = INDEX(priv->port);
	cvmx_helper_link_info_t link_info;

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	gmx_cfg.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);

	if (!octeon_is_simulation()) {
	     link_info = cvmx_helper_link_get(priv->port);
	     if (!link_info.s.link_up)	
		netif_carrier_off(dev);	
	}
	return 0;
}

static int cvm_oct_xaui_stop(struct net_device *dev)
{
	cvmx_gmxx_prtx_cfg_t gmx_cfg;
	cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
	int interface = INTERFACE(priv->port);
	int index = INDEX(priv->port);

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	gmx_cfg.s.en = 0;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmx_cfg.u64);
	return 0;
}

static void cvm_oct_xaui_poll(struct net_device *dev)
{
	cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
	cvmx_helper_link_info_t link_info;

	link_info = cvmx_helper_link_get(priv->port);
	if (link_info.u64 == priv->link_info)
		return;

	link_info = cvmx_helper_link_autoconf(priv->port);
	priv->link_info = link_info.u64;

	/* Tell Linux */
	if (link_info.s.link_up) {

		if (!netif_carrier_ok(dev))
			netif_carrier_on(dev);
		if (priv->queue != -1)
			DEBUGPRINT("%s: %u Mbps %s duplex, port %2d, queue %2d\n",
				   dev->name, link_info.s.speed,
				   (link_info.s.full_duplex) ? "Full" : "Half",
				   priv->port, priv->queue);
		else
			DEBUGPRINT("%s: %u Mbps %s duplex, port %2d, POW\n",
				   dev->name, link_info.s.speed,
				   (link_info.s.full_duplex) ? "Full" : "Half",
				   priv->port);
	} else {
		if (netif_carrier_ok(dev))
			netif_carrier_off(dev);
		DEBUGPRINT("%s: Link down\n", dev->name);
	}
}


int cvm_oct_xaui_init(struct net_device *dev)
{
	cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
	cvm_oct_common_init(dev);
	dev->open = cvm_oct_xaui_open;
	dev->stop = cvm_oct_xaui_stop;
	dev->stop(dev);
	if (!octeon_is_simulation())
		priv->poll = cvm_oct_xaui_poll;

    return 0;
}

void cvm_oct_xaui_uninit(struct net_device *dev)
{
	cvm_oct_common_uninit(dev);
}

