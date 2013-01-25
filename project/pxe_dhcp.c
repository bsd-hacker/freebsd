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
#include "pxe_dhcp.h"
#include "pxe_ip.h"

#ifdef PXE_BOOTP_USE_LIBSTAND
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net.h>
#include <bootp.h>
#else
#include "pxe_await.h"
#include "pxe_mem.h"
#include "pxe_sock.h"
#include "pxe_udp.h"
#endif

#ifndef PXE_BOOTP_USE_LIBSTAND
/* adds option to provided buffer 
 * in:
 *	opt	- current pointer in options section of DHCP packet
 *	option	- option id
 *	opt_data- pointer to related to option data
 *	opt_len	- size of data, pointed by opt_data
 * out:
 *	new pointer in options sections
 */
uint8_t *
add_option(uint8_t *opt, uint8_t option, const void *opt_data, uint8_t opt_len)
{
	PXE_DHCP_OPT_HDR *opt_hdr = (PXE_DHCP_OPT_HDR *)opt;
	
	opt_hdr->option = option;
	opt_hdr->len = opt_len;
	
	if ( (opt_data != NULL) && (opt_len != 0) )
		pxe_memcpy(opt_data, opt + sizeof(PXE_DHCP_OPT_HDR), opt_len);
	
	return (opt + sizeof(PXE_DHCP_OPT_HDR) + opt_len);
}

/* parses options section of packet
 * in:
 *	opts 		- pointer to options section
 *	max_size 	- size of option section data
 *	result		- pointer to result return structure
 * out:
 *	result		- result of parsing options
 */
void
pxe_dhcp_parse_options(uint8_t *opts, uint16_t max_size,
		       PXE_DHCP_PARSE_RESULT *result)
{
        uint8_t *p=opts;
        uint8_t code = opts[0];
        uint8_t len = 0;

        printf("DHCP options:\n");
				
        while (code != PXE_DHCP_OPT_END) {
                ++p;
                len = 1 + (*p);

                switch (code) {
	                case 0: /* pad */
	                        len = 0;
	                        break;

	                case PXE_DHCP_OPT_NETMASK:
	                        printf("netmask: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
				result->netmask.ip = *((uint32_t *)(p + 1));
	                        break;

	                case PXE_DHCP_OPT_ROUTER: 
	                        printf("first router: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
				result->gw.ip = *((uint32_t *)(p + 1));
	                        break;

	                case PXE_DHCP_OPT_NAMESERVER:
	                        printf("first nameserver: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
				result->ns.ip = *((uint32_t *)(p + 1));
	                        break;

	                case PXE_DHCP_OPT_TYPE:
				result->message_type = *(p + 1);
#ifdef PXE_DEBUG
	                        printf("message type: 0x%x\n",
				    result->message_type);
#endif				
	                        break;
			
			case PXE_DHCP_OPT_WWW_SERVER:
	                        printf("www server ip: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
				result->www.ip = *((uint32_t *)(p + 1));
	                        break;
			
			case PXE_DHCP_OPT_ROOTPATH:
				pxe_memcpy((p + 1), result->rootpath, len - 1);
				printf("root path: %s\n", result->rootpath);
				break;
#ifdef PXE_MORE				
	                case PXE_DHCP_OPT_LEASE_TIME:
#ifdef PXE_DEBUG
	                        printf("lease time: %d secs\n",
				    ntohl( *((uint32_t *)(p + 1)) ));
#endif				
	                        break;				

	                case PXE_DHCP_OPT_RENEWAL_TIME:
#ifdef PXE_DEBUG
			        printf("renewal in: %d secs\n",
				    ntohl( *((uint32_t *)(p + 1)) ));
#endif				
	                        break;
				
	                case PXE_DHCP_OPT_REBINDING_TIME:
#ifdef PXE_DEBUG
	                        printf("rebinding in: %d secs\n",
				    ntohl( *((uint32_t *)(p + 1)) ));
#endif				
	                        break;
				
	                case PXE_DHCP_OPT_BROADCAST_IP:
	                        printf("broadcast: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
				result->bcast_addr.ip = *((uint32_t *)(p + 1));
	                        break;						

	                case PXE_DHCP_OPT_ID:
#ifdef PXE_DEBUG
	                        printf("server id: %d.%d.%d.%d\n",
				    *(p + 1), *(p + 2), *(p + 3), *(p + 4));
#endif
	                        break;					

			case PXE_DHCP_OPT_DOMAIN_NAME:
#ifdef PXE_DEBUG
	                        printf("domain name: %s\n", (p + 1));
#endif
	                        break;
#endif /* PXE_MORE */
	                default:
#ifdef PXE_DEBUG
				printf("DHCP Option %d (%d bytes) ignored\n",
				    code, len);
#endif
	                        break;
                };
																																																				
                p += len;
                code = *p;
                len = 0;
	    
	        if (p - opts > max_size)
	                break;
        }
}

/* create_dhcp_packet() - fills header of request packet
 * in:
 *	wait_data - pointer to filled PXE_DHCP_WAIT_DATA
 * out:
 *	none
 */
void
create_dhcp_packet(PXE_DHCP_WAIT_DATA *wait_data)
{
	uint8_t		*buf = wait_data->data;
	PXE_DHCP_HDR	*dhcp_hdr = (PXE_DHCP_HDR *)buf;

	dhcp_hdr->op = PXE_DHCP_REQUEST;
	dhcp_hdr->htype = ETHER_TYPE;
	dhcp_hdr->hlen = 6;
	dhcp_hdr->hops = 0;
	dhcp_hdr->secs = 0;
	dhcp_hdr->xid = wait_data->xid;
	
	const PXE_IPADDR *my = pxe_get_ip(PXE_IP_MY);
	dhcp_hdr->ciaddr = my->ip;
	dhcp_hdr->magic = htonl(PXE_MAGIC_DHCP);
	
	pxe_memcpy(pxe_get_mymac(), dhcp_hdr->chaddr, dhcp_hdr->hlen);
}

/* dhcp_send_request() - creates socket, sends DHCP request
 * in:
 *	wait_data - pointer to filled PXE_DHCP_WAIT_DATA
 * out:
 *	0 - failed
 *	1 - success
 */
int
dhcp_send_request(PXE_DHCP_WAIT_DATA *wait_data)
{
	uint8_t	*options = wait_data->data + sizeof(PXE_DHCP_HDR);	
/*	uint8_t	message_type =  PXE_DHCPREQUEST; */
	uint8_t	message_type =  PXE_DHCPDISCOVER;

	/* cleaning up packet data */
	pxe_memset(wait_data->data, 0, wait_data->size);
	
	create_dhcp_packet(wait_data);

	/* setting INFORM type */
	options = add_option(options, PXE_DHCP_OPT_TYPE, &message_type, 1);

	/* requesting for my ip */
	const PXE_IPADDR *client_ip = pxe_get_ip(PXE_IP_MY);
	
	options = add_option(options, PXE_DHCP_OPT_REQUEST_IP,
		    &(client_ip->ip), sizeof(client_ip->ip));

	/* end of options */
	options = add_option(options, PXE_DHCP_OPT_END, NULL, 0);
		
	/* send */
	uint16_t	send_size = options - wait_data->data;	
	int		socket = pxe_socket();

	if (socket == -1) {
		printf("dhcp_send_request(): failed to create socket.\n");
		return (0);
	}
	
	if (pxe_bind(socket, client_ip, PXE_DHCP_CLIENT_PORT,
		PXE_UDP_PROTOCOL) == -1)
	{
		printf("dhcp_send_request(): failed bind client DHCP port.\n");
		pxe_close(socket);
		return (0);	
	}
	
	PXE_IPADDR bcast;
	bcast.ip = PXE_IP_BCAST;
	
	if (send_size != pxe_sendto(socket, &bcast, PXE_DHCP_SERVER_PORT,
			    wait_data->data, send_size))
	{
		printf("dhcp_send_request(): failed to send DHCP request.\n");
		pxe_close(socket);
		return (0);	
	}
	
	wait_data->socket = socket;

	return (1);
}

/* dhcp_parse() - parses received packet, drops if it is invalid
 * in:
 *	data	- pointer to buffer with data
 *	size	- szie of buffer
 *	xid	- client/transaction id
 * out:
 *	0 	- failed
 *	1	- success
 */
int
dhcp_parse(void *data, int size, uint32_t xid)
{
	PXE_DHCP_HDR *dhcp_hdr = (PXE_DHCP_HDR *)data;
	
	if (dhcp_hdr->magic != htonl(PXE_MAGIC_DHCP) ) /* unknown magic */
		return (0);

	if (dhcp_hdr->op != PXE_DHCP_REPLY) {
		printf("dhcp_parse(): got request, not reply.\n");
		return (0);
	}
	
	if (dhcp_hdr->xid != xid) {
		printf("dhcp_parse(): wrong xid 0x%x, need 0x%x.\n",
		    dhcp_hdr->xid, xid);
		return (0);
	}
	
	PXE_DHCP_PARSE_RESULT opts_result;
	pxe_memset(&opts_result, 0, sizeof(PXE_DHCP_PARSE_RESULT));
	
	/* parsing options section */
	pxe_dhcp_parse_options(data + sizeof(PXE_DHCP_HDR),
	    size - sizeof(PXE_DHCP_HDR), &opts_result);
	
	if ( (opts_result.message_type != PXE_DHCPOFFER) &&
	     (opts_result.message_type != PXE_DHCPACK) )
	{
		/* not our packet */
		return (0);
	}
	
	/* if successfuly parsed, setting appropriate ip data */
	if (opts_result.ns.ip)
		pxe_set_ip(PXE_IP_NAMESERVER, &opts_result.ns);
	
	if (opts_result.gw.ip)
		pxe_set_ip(PXE_IP_GATEWAY, &opts_result.gw);
	
	if (opts_result.netmask.ip)
		pxe_set_ip(PXE_IP_NETMASK, &opts_result.netmask);

	if (opts_result.bcast_addr.ip)
		pxe_set_ip(PXE_IP_BROADCAST, &opts_result.bcast_addr);
	
	if (opts_result.www.ip)
		pxe_set_ip(PXE_IP_WWW, &opts_result.www);
	
	if (opts_result.rootpath[0])
		strcpy(PXENFSROOTPATH, opts_result.rootpath);
		
	return (1);
}

/* dhcp_await() - await function for DHCP replies
 * in:
 *      function        - await function
 *      try_number      - number of try
 *      timeout         - timeout from start of try
 *      data            - pointer to PXE_DHCP_WAIT_DATA
 * out:
 *      PXE_AWAIT_ constants
 */
int
dhcp_await(uint8_t function, uint16_t try_number, uint32_t timeout, void *data)
{
	PXE_DHCP_WAIT_DATA	*wait_data = (PXE_DHCP_WAIT_DATA *)data;

	int			size = 0;
	
	switch (function) {
		  
	case PXE_AWAIT_STARTTRY:
	    
		if (!dhcp_send_request(wait_data))
			return (PXE_AWAIT_NEXTTRY);
			
		break;
		
	case PXE_AWAIT_NEWPACKETS:
		/* some packets were received, need checking our socket */
		size = pxe_recv(wait_data->socket, wait_data->data,
			    wait_data->size);
			    
		if ( size > 0) {
			/* something felt to socket */
			if (dhcp_parse(wait_data->data, size, wait_data->xid))
				return (PXE_AWAIT_COMPLETED);
		}
		
		return (PXE_AWAIT_CONTINUE);		
        	break;

        case PXE_AWAIT_FINISHTRY:
		/* close socket if it is valid */
		if (wait_data->socket != -1)
			pxe_close(wait_data->socket);
		
        	break;
		
        case PXE_AWAIT_END:
        default:
        	break;
        }

	return (PXE_AWAIT_OK);
}

/* pxe_dhcp_query() - sends DHCP query, using provided client id
 * in:
 *	xid - client/transaction id, used to differ packets
 * out:
 *	none
 */
void
pxe_dhcp_query(uint32_t xid)
{
	uint8_t			dhcp_pack[PXE_MAX_DHCPPACK_SIZE];
	PXE_DHCP_WAIT_DATA	wait_data;

	printf("pxe_dhcp_query(): getting parameters using DHCP.\n");

	wait_data.data = dhcp_pack;
	wait_data.socket = -1;
	wait_data.size = PXE_MAX_DHCPPACK_SIZE;
	wait_data.xid = xid;
	
	if (!pxe_await(dhcp_await, 3, 60000, &wait_data)) {
		printf("pxe_dhcp_query(): failed to get parameters via DHCP\n");
	} 
}
#else /* defined(PXE_BOOTP_USE_LIBSTAND) */

extern int pxe_sock;

void
pxe_dhcp_query(uint32_t xid)
{

	printf("pxe_dhcp_query(): starting libstand bootp()\n");
	bootp(pxe_sock, BOOTP_PXE);

	/* setting pxe_core variables */
	PXE_IPADDR addr;
	
	addr.ip = nameip.s_addr;
	pxe_set_ip(PXE_IP_NAMESERVER, &addr);
	
	addr.ip = netmask;
	pxe_set_ip(PXE_IP_NETMASK, &addr);
	
	addr.ip = rootip.s_addr;
	pxe_set_ip(PXE_IP_ROOT, &addr);
	
	/* "network route". direct connect for those addresses */
	pxe_ip_route_add(pxe_get_ip(PXE_IP_MY), netmask, NULL);
	
	addr.ip =  gateip.s_addr;
	pxe_set_ip(PXE_IP_GATEWAY, &addr);
	
	/* need update gateway information, cause it's already set to default */
	pxe_ip_route_default(&addr);
}

#endif /* PXE_BOOTP_USE_LIBSTAND */
