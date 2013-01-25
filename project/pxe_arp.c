/*-
 * Copyright (c) 2007 Alexey Tarasov
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
 */
 
#include <stand.h>

#include "pxe_arp.h"
#include "pxe_await.h"
#include "pxe_core.h"
#include "pxe_mem.h"

/* last entry used for PXE client ip/mac */
static PXE_ARP_ENTRY		arp_table[MAX_ARP_ENTRIES + 1];
static PXE_ARP_PACK_DATA	packet_to_send;	
static int			arp_usage = 0;

/*
 *  pxe_arp_init() - initialisation of used by this module structures
 *  in:
 *	none
 *  out:
 *	none
 */
void
pxe_arp_init()
{
#ifdef PXE_ARP_DEBUG
	printf("pxe_arp_init() started.\n");
	
	if (packet_to_send.hdr.hsize != 0) {
		printf("pxe_arp_init() already initialized.\n");
		return;
	}
#endif
	pxe_memset(&packet_to_send, 0, sizeof(PXE_ARP_PACK_DATA) );

	pxe_memset(arp_table, 0, sizeof(arp_table));

	/* filling packet_to_send, it will not change ever */	
	packet_to_send.hdr.hwtype =htons(ETHER_TYPE);
	packet_to_send.hdr.ptype = htons(PXE_PTYPE_IP);
	packet_to_send.hdr.hsize = 6;	/* Ethernet MAC size */
	packet_to_send.hdr.psize = 4;	/* ip4 size */
	packet_to_send.hdr.operation = htons(PXE_ARPOP_REQUEST);

	/* filling source related data: client ip & MAC */	
	pxe_memcpy(pxe_get_mymac(), packet_to_send.body.src_hwaddr, 6);	

	const PXE_IPADDR *addr = pxe_get_ip(PXE_IP_MY);;
	packet_to_send.body.src_paddr = addr->ip;
	
	/* filling entry for own ip/mac*/	
	pxe_memcpy(pxe_get_mymac(), arp_table[MAX_ARP_ENTRIES].mac, 6);	
	arp_table[MAX_ARP_ENTRIES].addr.ip  = addr->ip;
	
	/* setting broadcast target address */
	pxe_memset(packet_to_send.body.target_hwaddr, 0xff, 6); 
}

/*
 *  pxe_arp_table_search() - searches entry in ARP table for given ip
 *  in:
 *	ip     - ip, for which to search MAC
 *  out:
 *	NULL   - not found such entry in arp_table
 *	not NULL - pointer to MAC address
 */
const MAC_ADDR *
pxe_arp_table_search(uint32_t ip)
{
#ifdef PXE_ARP_DEBUG_HELL
	printf("pxe_arp_table_search(): started\n");
#endif		
	int entry = 0;
	
	for (; entry < MAX_ARP_ENTRIES + 1; ++entry) {

		if (arp_table[entry].addr.ip == ip) {
		
			uint8_t *mac = &arp_table[entry].mac[0];
#ifdef PXE_ARP_DEBUG_HELL
			printf("pxe_arp_table_search(): %6D\n", mac, ":");
#endif			
			return (const MAC_ADDR *)mac;
		}
	}

	return (NULL);
}

#ifdef PXE_MORE
/* pxe_arp_stats() - show arp current table state
 * in/out:
 *	none
 */
void
pxe_arp_stats()
{
	int entry = 0;
	int limit = (arp_usage > MAX_ARP_ENTRIES) ? MAX_ARP_ENTRIES : arp_usage;

	printf("ARP updates: %d\n", arp_usage);

	for (; entry < limit; ++entry) {
		
		if ( (arp_table[entry].addr.ip == 0) ||
		     (arp_table[entry].mac == NULL) )
			continue;

		printf("%s\t%6D\n",
		    inet_ntoa(arp_table[entry].addr.ip),
		    arp_table[entry].mac, ":");
	}

}
#endif /* PXE_MORE */

/*
 *  pxe_arp_protocol() - process received arp packet, this function is called in
 *			style of pxe_protocol_call function type, but last
 *			parameter is unused
 *  in:
 *	pack     - rceived packet data
 *	function - protocal function (will be always PXE_CORE_FRAG)
 *  out:
 *	always 0 - we are not interested in storing this packet in pxe_core queue
 */
int
pxe_arp_protocol(PXE_PACKET *pack, uint8_t function)
{
#ifdef PXE_ARP_DEBUG_HELL
	printf("pxe_arp_protocol() started.\n");
#endif
	PXE_ARP_PACK_DATA *arp_reply = (PXE_ARP_PACK_DATA *)pack->raw_data;

	if (arp_reply->hdr.operation == htons(PXE_ARPOP_REQUEST) ) {
	
		uint8_t		*mac_src = arp_reply->body.src_hwaddr;
		uint8_t		*mac_dst = arp_reply->body.target_hwaddr;		
		PXE_IPADDR	ip4_src;
		PXE_IPADDR	ip4_dst;
		
		const PXE_IPADDR *addr = pxe_get_ip(PXE_IP_MY);

		ip4_src.ip = arp_reply->body.src_paddr;
		ip4_dst.ip = arp_reply->body.target_paddr;
		
		if (ip4_src.ip == addr->ip) {
			/* got broadcast send by us */
#ifdef PXE_ARP_DEBUG_HELL
			printf("arp request from myself ignored.\n");
#endif
			return (0);
		}
		
#ifdef PXE_ARP_DEBUG		
		printf("arp request from %6D/%s\n\t",
		    mac_src, ":", inet_ntoa(ip4_src.ip));

		printf("to: %6D/%s\n",
		    mac_dst, ":", inet_ntoa(ip4_dst.ip));
#endif		

		/* somebody is looking for us */
		if (ip4_dst.ip == arp_table[MAX_ARP_ENTRIES].addr.ip) {
		
			pxe_memcpy(arp_reply->body.src_hwaddr,
			    packet_to_send.body.target_hwaddr, 6);
			    
			packet_to_send.body.target_paddr =
			    arp_reply->body.src_paddr;
			    
			packet_to_send.hdr.operation = htons(PXE_ARPOP_REPLY);
		
			PXE_PACKET	pack;
	
			pack.raw_size = sizeof(PXE_ARP_PACK_DATA);
			pack.raw_data = &packet_to_send;
			pack.data = &packet_to_send.hdr;
			pack.data_size =
			    sizeof(PXE_ARP_PACK_DATA) - MEDIAHDR_LEN_ETH;
			    
		    	pack.protocol = PXE_PROTOCOL_ARP;
			pack.dest_mac = (const MAC_ADDR *)
			    &packet_to_send.body.target_hwaddr[0];
			    
			pack.flags = PXE_SINGLE;
		
			if (!pxe_core_transmit(&pack)) {
			        printf("pxe_arp_protocol(): reply to arp request failed.\n");
			}

			/* cleaning packet_to_send back to initiakl state */
			pxe_memset(packet_to_send.body.target_hwaddr, 0xff, 6);
			packet_to_send.hdr.operation = htons(PXE_ARPOP_REQUEST);
		}
		
		 /* we may cache information about packet sender */
#ifdef PXE_ARP_SNIFF
		/* just to skip operation filter below */
		arp_reply->hdr.operation = htons(PXE_ARPOP_REPLY);
#else
		return (0);		
#endif
	}
	
	/* we don't need anything except replies on that stage */
	if (arp_reply->hdr.operation != htons(PXE_ARPOP_REPLY) ) 
		return (0);
		
	/* if arp_usage exceeds MAX_ARP_ENTRIES, occurs rewriting of earlier
	 * placed ARP entries. MAC may be lost, so protocol must check this
	 * case when creating packet (cause there used pointer to MAC
	 * in arp_table). May be better way is to panic if arp_table is full.
	 * In fact, we don't need many entries. Only two: one for gateway,
	 * one for DNS-server or for proxy server. Default arp_table size is 8.
	 * It seems more than enough.
	 */	
	 
	const MAC_ADDR *kmac = pxe_arp_table_search(arp_reply->body.src_paddr);
	if (NULL != kmac) {
#ifdef PXE_ARP_DEBUG
	        uint8_t *octet = (uint8_t *)&arp_reply->body.src_paddr;
	        printf("MAC of %d.%d.%d.%d already known: %x:%x:%x:%x:%x:%x\n",
		    octet[0], octet[1], octet[2], octet[3],
		    (*kmac)[0], (*kmac)[1], (*kmac)[2],
		    (*kmac)[3], (*kmac)[4], (*kmac)[5]
		);
#endif
		/* NOTE: theoretically it's possible mac != known mac. Ignore. */
		return (0);
	}
	
	pxe_memcpy(&arp_reply->body.src_hwaddr,
	    &arp_table[arp_usage % MAX_ARP_ENTRIES].mac, 6);
	    
	arp_table[arp_usage % MAX_ARP_ENTRIES].addr.ip =
	    arp_reply->body.src_paddr;
	    
	++arp_usage;

#ifdef PXE_ARP_DEBUG_HELL	
	printf("pxe_arp_protocol(): arp usage = %d\n", arp_usage);
#endif
	
	return (0); /* inform pxe_get_packet() we don't need this packet more. */
}

/*
 *  pxe_arp_send_whois() - sends ARP request packet for given ip, received
 *			  packets are handled in pxe_arp_protocol()
 *  in:
 *	ip	- target ip, for which to find MAC
 *  out:
 *	0	- failed
 *	1	- success
 */
int
pxe_arp_send_whois(uint32_t ip)
{
        PXE_PACKET	pack;
	
	pack.raw_size = sizeof(PXE_ARP_PACK_DATA);
	pack.raw_data = &packet_to_send;
	pack.data = &packet_to_send.hdr;
	pack.data_size = sizeof(PXE_ARP_PACK_DATA) - MEDIAHDR_LEN_ETH;
	
	pack.protocol = PXE_PROTOCOL_ARP;
	pack.dest_mac = (const MAC_ADDR *)&packet_to_send.body.target_hwaddr[0];
	pack.flags = PXE_BCAST;

	packet_to_send.body.target_paddr = ip;

	if (!pxe_core_transmit(&pack)) {
		printf("pxe_arp_send_whois(): failed to send request.\n");
		return (0);
	}
	
	return (1);
}

/* pxe_arp_await() - await function for ARP replies
 * in:
 *	function	- await function
 *	try_number	- number of try
 *	timeout		- timeout from start of try
 *	data		- pointer to PXE_ARP_WAIT_DATA
 * out:
 *	PXE_AWAIT_ constants
 */
int
pxe_arp_await(uint8_t function, uint16_t try_number, uint32_t timeout,
	      void *data)
{
	PXE_ARP_WAIT_DATA	*wait_data = (PXE_ARP_WAIT_DATA *)data;
	const MAC_ADDR		*res = NULL;
	switch (function) {

	case PXE_AWAIT_STARTTRY:	/* handle start of new try */
		if (!pxe_arp_send_whois(wait_data->addr.ip)) {	
			/* failed to send request, try once more
			 * after waiting a little
			 */
			delay(10000);		
		        return (PXE_AWAIT_NEXTTRY);
		}
		break;

	case PXE_AWAIT_NEWPACKETS:
		/* check if ARP protocol was called and 
		 * arp_table updated 
		 */
		res = pxe_arp_table_search(wait_data->addr.ip);
		if (res != NULL) {
			wait_data->mac = res;
			return (PXE_AWAIT_COMPLETED);
		}
		
		return (PXE_AWAIT_CONTINUE);
		break;		

	case PXE_AWAIT_FINISHTRY:
		if (wait_data->mac == NULL) 	/* nothing got during try */
			printf("\npxe_arp_await(): ARP reply timeout.\n");
		break;	

	case PXE_AWAIT_END:			/* wait ended */
	default:
		break;
	}
	
	return (PXE_AWAIT_OK);
}

/*
 *  pxe_arp_ip4mac() - returns MAC for given ip if it's found in arp_table,
 *                     otherwise - performs request sending
 *  in:
 *	ip     - ip, for which to search MAC
 *  out:
 *	NULL   - not found such entry in arp_table
 *	not NULL - pointer to MAC address
 */
const MAC_ADDR *
pxe_arp_ip4mac(const PXE_IPADDR *addr)
{
	const MAC_ADDR *res = pxe_arp_table_search(addr->ip);

	if (res != NULL)
		return (res);
#ifdef PXE_EXCLUSIVE
	pxe_core_exclusive(PXE_PROTOCOL_ARP);
#endif
	PXE_ARP_WAIT_DATA	wait_data;
	
	wait_data.addr.ip = addr->ip;
	wait_data.mac = NULL;
	
	pxe_await(pxe_arp_await, PXE_MAX_ARP_TRY, PXE_TIME_TO_DIE, &wait_data);
#ifdef PXE_EXCLUSIVE
	pxe_core_exclusive(0);
#endif	
	return (wait_data.mac);
}
