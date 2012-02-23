/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#ifdef __FreeBSD__
#include "mips/nlm/hal/ucore.h"
#else
#include "ucore.h"
#endif

#define TCP_ACK_BIT (1 << 4)

#define __jhash_mix(a, b, c)		\
{					\
        a -= b; a -= c; a ^= (c>>13);	\
        b -= c; b -= a; b ^= (a<<8);	\
        c -= a; c -= b; c ^= (b>>13);	\
        a -= b; a -= c; a ^= (c>>12);	\
        b -= c; b -= a; b ^= (a<<16);	\
        c -= a; c -= b; c ^= (b>>5);	\
        a -= b; a -= c; a ^= (c>>3);	\
        b -= c; b -= a; b ^= (a<<10);	\
        c -= a; c -= b; c ^= (b>>15);	\
}

#define JHASH_GOLDEN_RATIO      0x9e3779b9

/* this enum need to match the one defined in board.h */
enum driver_mode {
	XLPNAE_SPRAY,
	XLPNAE_JHASH,
};

static __inline__ unsigned int
jhash_3words(unsigned int a, unsigned int b,
    unsigned int c, unsigned int initval)
{
        a += JHASH_GOLDEN_RATIO;
        b += JHASH_GOLDEN_RATIO;
        c += initval;

        __jhash_mix(a, b, c);

        return c;
}

int main(void)
{
	volatile unsigned int *prepad = 
	    (volatile unsigned int *) (PACKET_MEMORY);
	volatile unsigned int *pkt = 
	    (volatile unsigned int *) (PACKET_MEMORY + PACKET_DATA_OFFSET);
	volatile unsigned int *shared_mem =
	    (volatile unsigned int *)SHARED_SCRATCH_MEM;
	unsigned int pktrdy, msgring_mask, ucore_mode;
	unsigned int eth_type, vlan_outer_tag, vlan_tag;
	unsigned int ip_proto, src_ip, dst_ip, src_port, dst_port;
	unsigned int ip_hdrlen, tot_len, tcp_hdrlen, flags, port;
	unsigned int hash, dest_vc;
	unsigned int vc_index[32];
	unsigned int num_cpus = 0;
	int i;
	/*
	 * max ethermtu possible = 1518
	 * XLP L3 cacheline size = 64
	 * with this setup, the whole pkt will be in XLP L3
	 */
	int num_cachelines = 1518 >> 6; /* or 1518/64 */

	/* spray the packets till SMP comes up and
	 * msgring_mask is sent to us
	 */
	while ((msgring_mask = *(shared_mem)) == 0) {
		pktrdy = nlm_read_ucore_rxpktrdy();
		nlm_ucore_setup_poepktdistr(FWD_DIST_VEC,
		    0, 0, 0, 0);
		nlm_ucore_pkt_done(num_cachelines,
		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	ucore_mode = *(shared_mem + 2);

	/* Now we decide how we send the packets depending on
	 * ucore mode. ucore_mode is not checked in fast_path
	 * since accessing shared memory seem to slow down 
	 * ucore processing.
	 */

	if (ucore_mode == XLPNAE_SPRAY) {
		while(1) {
			pktrdy = nlm_read_ucore_rxpktrdy();
			nlm_ucore_setup_poepktdistr(FWD_DIST_VEC,
			    0, 0, 0, 0);
			nlm_ucore_pkt_done(num_cachelines,
			    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		}
	}

	for (i = 0; i < 32; i++) {
		if (msgring_mask & (0x1 << i))
			num_cpus++;
	}

	for (i = 0; i < num_cpus; i++)
		vc_index[i] = (i * 4); /* send to vc 0 */

	if (ucore_mode == XLPNAE_JHASH) {
		while (1) {
			pktrdy = nlm_read_ucore_rxpktrdy();
			vlan_tag = (pkt[3] >> 16) & 0xffff;
			/* check Q-in-Q packet or 802.1ad packet */
			if ((vlan_tag == 0x9100) ||
			    (vlan_tag == 0x88a8)) {
				vlan_outer_tag = vlan_tag;
				vlan_tag = (pkt[4] >> 16) & 0xffff;
				eth_type = (pkt[5] >> 16) & 0xffff;
				tot_len = (pkt[6] >> 16) & 0xffff;
				ip_hdrlen = (pkt[5] >> 8) & 0xf;
			} else if (vlan_tag == 0x8100) {
				/* 802.1q vlan tagged pkt */
				eth_type = (pkt[4] >> 16) & 0xffff;
				tot_len = (pkt[5] >> 16) & 0xffff;
				ip_hdrlen = (pkt[4] >> 8) & 0xf;
			}
			else {
				/* normal ethernet packet */
				eth_type = vlan_tag;
				vlan_tag = 0;
				tot_len = (pkt[4] >> 16) & 0xffff;
				ip_hdrlen = (pkt[3] >> 8) & 0xf;
			}

			/* we do jhash only on IPv4 pkts */
			if (eth_type == 0x800) {
				ip_proto = (prepad[0] >> 24) & 0xff;
				src_ip = ((prepad[0] & 0xffffff) << 8) |
				    ((prepad[1] >> 24) & 0xff);
				dst_ip = ((prepad[1] & 0xffffff) << 8) |
				    ((prepad[2] >> 24) & 0xff);
				/* if tcp or udp, take src and dest ports */
				if ((ip_proto == 0x06) || (ip_proto == 0x11)) {
					src_port = (prepad[2] >> 8) & 0xffff;
					dst_port = ((prepad[2] & 0xff) << 8) |
					    ((prepad[3] >> 24) & 0xff);
				} else
					src_port = dst_port = 0;

				if (ip_proto == 0x6) {
					tcp_hdrlen = (prepad[5] >> 20) & 0xf;
					flags = (prepad[5] >> 16) & 0x3f;
				} else
					tcp_hdrlen = flags = 0;

				/* handle better distribution of TCP ack pkts */
				if((ip_proto == 0x6) && (flags & TCP_ACK_BIT) &&
				    (tot_len == 
					(tcp_hdrlen * 4 + ip_hdrlen * 4))) {
					dest_vc = vc_index[dst_port & num_cpus];
				} else {
					port = (src_port << 16) | dst_port;
					hash = jhash_3words(src_ip, dst_ip, port, 0);
					dest_vc = vc_index[hash & num_cpus];
				}

				nlm_ucore_setup_poepktdistr(0x1,
				    0, 0, dest_vc, 0);

			} else {
				/* spray the packets */
				nlm_ucore_setup_poepktdistr(FWD_DIST_VEC,
				    0, 0, 0, 0);
			}

			nlm_ucore_pkt_done(num_cachelines,
			    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		}
	}
	return (0);
}
