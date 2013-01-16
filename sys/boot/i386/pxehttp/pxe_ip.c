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

#include "pxe_core.h"
#include "pxe_ip.h"
#ifdef PXE_MORE
#include "pxe_icmp.h"
#endif

/* routing table */
static PXE_IP_ROUTE_ENTRY	route_table[PXE_MAX_ROUTES];
/* total count of routes in table */
static int			all_routes = 0;
/* id of packets */
static uint16_t 		packet_id = 1;

/* pxe_ip_route() - init default routes
 * in:
 *	def_gw	- default gateway ip
 * out:
 *	none
 */
void
pxe_ip_route_init(const PXE_IPADDR *def_gw)
{

        if (all_routes)	/* already inited */
		return;
		
	/* add default gw */
	pxe_ip_route_default(def_gw);
	all_routes += 1;

	/* if using libstand, set network route in pxe_dhcp_query() */
#ifndef PXE_BOOT_USE_LIBSTAND
	const PXE_IPADDR* 	myip = pxe_get_ip(PXE_IP_MY);
	
	/* make route for local network */
	const PXE_IPADDR	*netmask = pxe_get_ip(PXE_IP_NETMASK);
	uint32_t		mask = netmask->ip;
	
	if (mask == 0)	/* mask isn't set via DHCP/BOOTP, setting it manually */
		mask = pxe_ip_get_netmask(myip);
	
	if (mask == 0) {
		printf("pxe_ip_route_init(): my ip is class D or class E,"
		       " don't know how understand this.\n");
		return;
	}
	
	pxe_ip_route_add(myip, mask, NULL);
#endif
}

/* pxe_ip_get_netmask() - returns class based mask for provided ip.
 * in:
 *	ip - ip address, from which network class is extracted 
 * out:
 *	0	- failed
 *	not 0	- network mask
 */
uint32_t
pxe_ip_get_netmask(const PXE_IPADDR *addr)
{
	uint8_t		net_class = ((addr->ip) & 0x000000F0) >> 4;
	
	if ( (net_class & 0x0c) == 0x0c)	/* class C */
		return (0x00ffffff);
	else if ((net_class & 0x08) == 0x08)	/* class B */
	        return (0x0000ffff);
	else if ((net_class & 0x08) == 0x00)	/* class A */
	        return (0x000000ff);
	
	/* D & E classes are not supported yet... */
	return (0);
}

/* pxe_ip_route_add() - adds one more route to routing table
 * in:
 *	net	- network to route to
 *	mask	- network mask
 *	gw	- gateway for this network
 * out:
 *	0	- failed to add route
 *	1	- success
 */
int
pxe_ip_route_add(const PXE_IPADDR *net, uint32_t mask, const PXE_IPADDR *gw)
{

	printf("pxe_ip_route_add(): adding net %s mask %8x gw ",
	    inet_ntoa(net->ip), ntohl(mask));
	
	if (gw && gw->ip)
	    printf("%s\n", inet_ntoa(gw->ip));
	else
	    printf("pxenet0\n");
	
	if (all_routes == PXE_MAX_ROUTES) {
		printf("pxe_ip_route_add(): failed, routing table is full.\n");
		return (0);
	}

#ifdef PXE_MORE
	if ( (gw!=NULL) && (gw->ip) && (pxe_ping(gw, 3, 0) == 0) ) {
		printf("pxe_ip_route_add(): failed, gateway is unreachable.\n");
		return (0);
	}
#endif	
        route_table[all_routes].net.ip = (net->ip & mask);
	route_table[all_routes].mask = mask;
	route_table[all_routes].gw.ip = (gw != NULL) ? gw->ip : 0;
	
	++all_routes;
	
	return (1);
}

#ifdef PXE_MORE
/* pxe_ip_route_del() - removes route from routing table
 * in:
 *	net	- network to route to
 *	mask	- network mask
 *	gw	- gateway for this network
 * out:
 *	0	- failed to remove (e.g. no such route found)
 *	1	- success
 */
int
pxe_ip_route_del(const PXE_IPADDR *net, uint32_t mask, const PXE_IPADDR *gw)
{
	int route_index = 1;
	
	printf("pxe_ip_route_add(): deleting net %s mask %8x gw",
	    inet_ntoa(net->ip), htonl(mask));
	    
	printf("%s\n", inet_ntoa(gw->ip));
	
	for ( ; route_index < all_routes; ++route_index) {
	        
		if ((route_table[route_index].net.ip == net->ip) &&
	    	    (route_table[route_index].gw.ip == gw->ip) &&
		    (route_table[route_index].mask == mask)) 
		{
			--all_routes;    		
			
			if (route_index == all_routes)
				return (1);
			
			/* shift routes */
			pxe_memcpy(&route_table[route_index + 1],
				    &route_table[route_index],
				    sizeof(PXE_IP_ROUTE_ENTRY) *
				    (all_routes - route_index));
			
			return (1);
		}

	}
	
	printf("pxe_ip_route_del(): there is no such route.\n");
	return (0);
}
#endif /* PXE_MORE */

/* pxe_ip_route_default() - sets default gateway
 * in:
 *	gw - ip address of default gateway
 * out:
 *	0	- failed (e.g. gateway is unreachable for pings)
 *	1	- success
 */
int
pxe_ip_route_default(const PXE_IPADDR *gw)
{

	printf("pxe_ip_route_default(): setting default gateway %s\n",
	    inet_ntoa(gw->ip));
	
#ifdef PXE_MORE
	/* don't check if there are no any entries */
	if (all_routes && (pxe_ping(gw, 3, 0) == 0) ) {
		printf("pxe_ip_route_add(): failed, gateway is unreachable.\n");
		return (0);
	}
#endif	
        route_table[0].net.ip = 0;
	route_table[0].mask = 0;
	route_table[0].gw.ip = gw->ip;
	
	return (1);
}

/* pxe_ip_checksum() -  calculates 16bit checksum of 16bit words
 * in:
 *	data - pointer to buffer to calc checksum for
 *	size - size of buffer
 * out:
 *	checksum
 */
uint16_t
pxe_ip_checksum(const void *data, size_t size)
{
	const uint8_t   *cur = data;
	uint32_t        sum = 0;
	uint32_t        ind = 0;

	for (; ind < size; ++ind, ++cur) {
		uint32_t byte = (*cur);

		if (ind & 1) /* odd */
			byte <<= 8;

		sum += byte;

		if (sum & 0xF0000) /* need carry out */
			sum -= 0xFFFF;
	}

	return (uint16_t)( (sum) & 0x0000FFFF);
}

#ifdef PXE_MORE
/* pxe_ip_route_stat() - shows current routes
 * in:
 *	none
 * out:
 *	none
 */
void
pxe_ip_route_stat()
{
	int index = 0;
	
	printf("Destination\t\tGateway\t\tMAC\n");
	for ( ; index < all_routes; ++index) {

        	printf("%s/%x\t\t",
		    inet_ntoa(route_table[index].net.ip),
		    ntohl(route_table[index].mask));
	    
		if (route_table[index].gw.ip == 0)
		        printf("pxenet0");
		else {
		        printf("%s\t\t",
			    inet_ntoa(route_table[index].gw.ip));
			
			uint8_t *mac =
			    (uint8_t *)pxe_arp_ip4mac(&route_table[index].gw);

			if (mac != NULL)
				printf("%6D", mac, ":");
		}
		printf("\n");
	}
}
#endif /* PXE_MORE */

/* pxe_create_ip_hdr() - creates IP header for data,
 *			placing header in this buffer
 * in:
 *	data	- buffer with data in which header will be created
 *	dst_ip	- destination IP address
 *	protocol- IP protocol (e.g. UDP)
 *	size	- size of buffer
 *	opts_size - size of IP header options. Currently unused, set 0.
 * out:
 *	none
 */
void
pxe_create_ip_hdr(void* data, const PXE_IPADDR *dst, uint8_t protocol,
		  uint16_t size, uint16_t opts_size)
{
	PXE_IP_HDR *iphdr = (PXE_IP_HDR *)data;
	const PXE_IPADDR *addr = pxe_get_ip(PXE_IP_MY);

	iphdr->checksum = 0;
	iphdr->length = htons(size);
	iphdr->protocol = protocol;
	iphdr->checksum = 0;

	/* data_off - offset of fragment, need to think about renaming. */
	iphdr->data_off = 0;
	iphdr->dst_ip = dst->ip;
	iphdr->id = htons(++packet_id);
	iphdr->tos = 0;
	iphdr->src_ip = addr->ip;
	iphdr->ttl = 64;

	/* 0x45 [ver_ihl] = 0x4 << 4 [ip version] |
         *                  0x5 [header length = 20 bytes, no opts]
         */
	iphdr->ver_ihl = 0x45;
	iphdr->ver_ihl += (opts_size >> 2);

	iphdr->checksum =
	    ~pxe_ip_checksum(data, sizeof(PXE_IP_HDR) + opts_size);
}

/* pxe_ip_route_find() - searches gateway to destination 
 * in:
 *	dst	- destination IP address
 * out:
 *	dst	- if there is no gateway, or dst_ip directly accessible
 *	otherwise  - gateway IP address
 */
const PXE_IPADDR *
pxe_ip_route_find(const PXE_IPADDR *dst)
{
        int index = 1;	/* route 0 - default gateway */
	
	/* if there are no routes, try to send directly */
	if (all_routes == 0)
		return (dst);

	PXE_IP_ROUTE_ENTRY	*route = &route_table[1];
    
	for ( ; index < all_routes; ++index) {
    
		if ( (dst->ip & route->mask) == route->net.ip ) {
			/* found route */
#ifdef PXE_IP_DEBUG_HELL
			printf("pxe_ip_route_find(): route 0x%x\n",
			    (route->gw.ip == 0) ? dst : &route->gw);
#endif		    
			/* gateway == 0 only for local network */
			return (route->gw.ip == 0) ? dst : &route->gw;
		}
	}
	
	/* return default gateway */
	return &route_table[0].gw;
}

/* pxe_ip_send() - transmits packet provided destination address,
 *		 using routing table
 * in:
 *	data	- buffer to transmit
 *	dst_ip	- destination IP address
 *	protocol- IP stack protocol (e.g. UDP)
 *	size	- size of data buffer
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_ip_send(void *data, const PXE_IPADDR *dst, uint8_t protocol, uint16_t size)
{
	PXE_PACKET	pack_out;
	int		status = 0;
	
	pack_out.data = data;
	pack_out.data_size = size;
	
	/* creating ip header */
	pxe_create_ip_hdr(pack_out.data, dst, protocol, size, 0);
	 
	/* setting pxe_core packet parameters */
        pack_out.flags = (dst->ip != PXE_IP_BCAST) ? PXE_SINGLE : PXE_BCAST;
        pack_out.protocol = PXE_PROTOCOL_IP;
	
	/* find gateway or direct MAC */
	const PXE_IPADDR *ip_to_send = dst;

	if (pack_out.flags != PXE_BCAST) {
		ip_to_send = pxe_ip_route_find(dst);
	        pack_out.dest_mac = pxe_arp_ip4mac(ip_to_send);
	} else {
		pack_out.dest_mac = NULL;
	}
	
#ifdef PXE_IP_DEBUG_HELL
	printf("pxe_ip_send(): %d proto, %s (%6D), %s.\n", protocol,
	    inet_ntoa(ip_to_send->ip),
	    pack_out.dest_mac != NULL ? pack_out.dest_mac : "\0\0\0\0\0\0", ":",
	    pack_out.flags == PXE_SINGLE ? "single" : "bcast");
#endif	

        if ( (pack_out.flags != PXE_BCAST) && (pack_out.dest_mac == NULL) ) {
		/* MAC is not found for destination ip or gateway */
#ifdef PXE_IP_DEBUG
		printf("pxe_ip_send(): cannot send ip packet to %s, ",
			inet_ntoa(dst->ip));
			
		printf("MAC is unknown for %s\n", inet_ntoa(ip_to_send->ip));
#endif
        } else  {
                if (!pxe_core_transmit(&pack_out)) {
#ifdef PXE_IP_DEBUG
                        printf("pxe_ip_send(): failed to send packet.\n");
#endif			
	        } else
			status = 1;
        }

	return (status);
}
