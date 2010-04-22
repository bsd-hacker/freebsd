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
#include <linux/mii.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <net/dst.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

extern struct net_device *cvm_oct_device[];


static unsigned long long cvm_oct_stats_read_switch(struct net_device *dev, int phy_id, int offset)
{
	cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
	priv->mii_info.mdio_write(dev, phy_id, 0x1d, 0xcc00 | offset);
	return ((uint64_t)priv->mii_info.mdio_read(dev, phy_id, 0x1e)<<16) | (uint64_t)priv->mii_info.mdio_read(dev, phy_id, 0x1f);
}

static int cvm_oct_stats_switch_show(struct seq_file *m, void *v)
{
	static const int ports[] = {0, 1, 2, 3, 9, -1};
	struct net_device *dev = cvm_oct_device[0];
	int index = 0;

	while (ports[index] != -1) {

		/* Latch port */
		cvm_oct_private_t *priv = (cvm_oct_private_t *)netdev_priv(dev);
		priv->mii_info.mdio_write(dev, 0x1b, 0x1d, 0xdc00 | ports[index]);
		seq_printf(m, "\nSwitch Port %d\n", ports[index]);
		seq_printf(m, "InGoodOctets:   %12llu\t"
			   "OutOctets:      %12llu\t"
			   "64 Octets:      %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x00) | (cvm_oct_stats_read_switch(dev, 0x1b, 0x01)<<32),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x0E) | (cvm_oct_stats_read_switch(dev, 0x1b, 0x0F)<<32),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x08));

		seq_printf(m, "InBadOctets:    %12llu\t"
			   "OutUnicast:     %12llu\t"
			   "65-127 Octets:  %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x02),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x10),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x09));

		seq_printf(m, "InUnicast:      %12llu\t"
			   "OutBroadcasts:  %12llu\t"
			   "128-255 Octets: %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x04),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x13),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x0A));

		seq_printf(m, "InBroadcasts:   %12llu\t"
			   "OutMulticasts:  %12llu\t"
			   "256-511 Octets: %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x06),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x12),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x0B));

		seq_printf(m, "InMulticasts:   %12llu\t"
			   "OutPause:       %12llu\t"
			   "512-1023 Octets:%12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x07),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x15),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x0C));

		seq_printf(m, "InPause:        %12llu\t"
			   "Excessive:      %12llu\t"
			   "1024-Max Octets:%12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x16),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x11),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x0D));

		seq_printf(m, "InUndersize:    %12llu\t"
			   "Collisions:     %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x18),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1E));

		seq_printf(m, "InFragments:    %12llu\t"
			   "Deferred:       %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x19),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x05));

		seq_printf(m, "InOversize:     %12llu\t"
			   "Single:         %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1A),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x14));

		seq_printf(m, "InJabber:       %12llu\t"
			   "Multiple:       %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1B),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x17));

		seq_printf(m, "In RxErr:       %12llu\t"
			   "OutFCSErr:      %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1C),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x03));

		seq_printf(m, "InFCSErr:       %12llu\t"
			   "Late:           %12llu\n",
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1D),
			   cvm_oct_stats_read_switch(dev, 0x1b, 0x1F));
		index++;
	}
	return 0;
}

/**
 * User is reading /proc/octeon_ethernet_stats
 *
 * @param m
 * @param v
 * @return
 */
static int cvm_oct_stats_show(struct seq_file *m, void *v)
{
	cvm_oct_private_t *priv;
	int port;

	for (port = 0; port < TOTAL_NUMBER_OF_PORTS; port++) {

		if (cvm_oct_device[port]) {

			priv = (cvm_oct_private_t *)netdev_priv(cvm_oct_device[port]);
			seq_printf(m, "\nOcteon Port %d (%s)\n", port, cvm_oct_device[port]->name);
			seq_printf(m,
				   "rx_packets:             %12lu\t"
				   "tx_packets:             %12lu\n",
				   priv->stats.rx_packets,
				   priv->stats.tx_packets);
			seq_printf(m,
				   "rx_bytes:               %12lu\t"
				   "tx_bytes:               %12lu\n",
				   priv->stats.rx_bytes,
				   priv->stats.tx_bytes);
			seq_printf(m,
				   "rx_errors:              %12lu\t"
				   "tx_errors:              %12lu\n",
				   priv->stats.rx_errors,
				   priv->stats.tx_errors);
			seq_printf(m,
				   "rx_dropped:             %12lu\t"
				   "tx_dropped:             %12lu\n",
				   priv->stats.rx_dropped,
				   priv->stats.tx_dropped);
			seq_printf(m,
				   "rx_length_errors:       %12lu\t"
				   "tx_aborted_errors:      %12lu\n",
				   priv->stats.rx_length_errors,
				   priv->stats.tx_aborted_errors);
			seq_printf(m,
				   "rx_over_errors:         %12lu\t"
				   "tx_carrier_errors:      %12lu\n",
				   priv->stats.rx_over_errors,
				   priv->stats.tx_carrier_errors);
			seq_printf(m,
				   "rx_crc_errors:          %12lu\t"
				   "tx_fifo_errors:         %12lu\n",
				   priv->stats.rx_crc_errors,
				   priv->stats.tx_fifo_errors);
			seq_printf(m,
				   "rx_frame_errors:        %12lu\t"
				   "tx_heartbeat_errors:    %12lu\n",
				   priv->stats.rx_frame_errors,
				   priv->stats.tx_heartbeat_errors);
			seq_printf(m,
				   "rx_fifo_errors:         %12lu\t"
				   "tx_window_errors:       %12lu\n",
				   priv->stats.rx_fifo_errors,
				   priv->stats.tx_window_errors);
			seq_printf(m,
				   "rx_missed_errors:       %12lu\t"
				   "multicast:              %12lu\n",
				   priv->stats.rx_missed_errors,
				   priv->stats.multicast);
		}
	}

	if (cvm_oct_device[0]) {
		priv = (cvm_oct_private_t *)netdev_priv(cvm_oct_device[0]);
		if (priv->imode == CVMX_HELPER_INTERFACE_MODE_GMII)
			cvm_oct_stats_switch_show(m, v);
	}
	return 0;
}


/**
 * /proc/octeon_ethernet_stats was openned. Use the single_open iterator
 *
 * @param inode
 * @param file
 * @return
 */
static int cvm_oct_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, cvm_oct_stats_show, NULL);
}


static struct file_operations cvm_oct_stats_operations = {
	.open		= cvm_oct_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


void cvm_oct_proc_initialize(void)
{
	struct proc_dir_entry *entry = create_proc_entry("octeon_ethernet_stats", 0, NULL);
	if (entry)
		entry->proc_fops = &cvm_oct_stats_operations;
}

void cvm_oct_proc_shutdown(void)
{
	remove_proc_entry("octeon_ethernet_stats", NULL);
}

