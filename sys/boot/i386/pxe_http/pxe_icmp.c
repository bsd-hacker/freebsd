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
#ifdef PXE_MORE 
#include <stand.h>

#include "pxe_arp.h"
#include "pxe_core.h"
#include "pxe_icmp.h"
#include "pxe_ip.h"

/* used in echo replying */
static PXE_IPADDR	pinging;	/* ip to accept replies from */
static uint16_t		seq_number;	/* next sequence number to accept */
/* last sequence number accepted */
static uint16_t		last_accepted = 0xffff;
/* 1 - show messages on screen while ping, 0 - not */
static int		echo_flags = 0;

/* pxe_icmp_callback() - callback function, executed by pxe_core
 * in:
 *	pack	- packet describing data
 *	function- function, which must be performed
 * out:
 *	always 0
 */
int
pxe_icmp_callback(PXE_PACKET *pack, uint8_t function)
{

	if (function == PXE_CORE_FRAG)
		return (1);

	/* icmp header */
	PXE_IP_HDR	*iphdr = (PXE_IP_HDR *)pack->data;
	size_t		iphdr_len = (iphdr->ver_ihl & 0x0f) * 4;
	size_t		data_size = ntohs(iphdr->length) -
				    iphdr_len - sizeof(PXE_ICMP_HDR);
	PXE_ICMP_HDR	*icmphdr = (PXE_ICMP_HDR *)(pack->data + iphdr_len);

#ifdef PXE_DEBUG
	printf("pxe_icmp_callback(): data size %d (of %d) bytes, type: %d\n",
	    data_size, pack->data_size, icmphdr->type);
#endif
	/* TODO: verify checksum */

	/* reply */
	PXE_IP_HDR	*reply_iphdr = NULL;
	PXE_ICMP_HDR	*reply_icmphdr = NULL;
	size_t		reply_size = sizeof(PXE_IP_HDR) + sizeof(PXE_ICMP_HDR) +
				     data_size;

	uint16_t	reply_number = ntohl(icmphdr->seq_num);
	
	/* we are interested only in echo related packets*/
	switch(icmphdr->type) {
	case PXE_ICMP_ECHO_REQUEST:
	case PXE_ICMP_ECHO_REPLY:
 /*	case PXE_ICMP_DEST_UNREACHABLE:
 	case PXE_ICMP_REDIRECT_MESSAGE:
 */
		break;
	default:
		return (0);   /* instruct pxe core to drop packet*/
	};

	if (icmphdr->type == PXE_ICMP_ECHO_REPLY) {

		if ( (reply_number != seq_number) && (icmphdr->code != 0)) {
#ifdef PXE_DEBUG		
			printf("pxe_icmp_callback(): seq %d != %d expected\n",
			    reply_number, seq_number);
#endif
			return (0);	/* ignore this packet */
		}
		
		uint16_t	id = (uint16_t)(seq_number*seq_number);
		if (icmphdr->packet_id != id) {
			if (echo_flags)
				printf("pxe_icmp_callback(): skipping id 0x%x, "
				       "0x%x expected\n", icmphdr->packet_id, id);
				
			return (0);
		}

		if (pinging.ip == iphdr->src_ip) {

			if (echo_flags) {
				printf("pxe_ping(): echo reply from %d.%d.%d.%d,"
				       " seq=%ld ",
				       pinging.octet[0], pinging.octet[1],
				       pinging.octet[2], pinging.octet[3],
				       seq_number);
			}
			
			/* notify pxe_ping() code that we received reply */
			last_accepted = seq_number;
		}
			    
		return (0);
	}

	/* all we need now is echo reply */

	/* using buffer of recieved packet to avoid additional
	 * memory copy operations */

	reply_iphdr = (PXE_IP_HDR *)pack->data;
	reply_icmphdr = (PXE_ICMP_HDR *)(pack->data + iphdr_len);

	reply_icmphdr->type = PXE_ICMP_ECHO_REPLY;
	reply_icmphdr->checksum = 0;
	reply_icmphdr->checksum =
	    ~pxe_ip_checksum(reply_icmphdr, sizeof(PXE_ICMP_HDR) + data_size);
	
	PXE_IPADDR addr;
	addr.ip = iphdr->src_ip;
	
	if (!pxe_ip_send(pack->data, &addr, PXE_ICMP_PROTOCOL,
		pack->data_size) && echo_flags)
	{
		printf("pxe_ping(): failed to send echo reply.\n");
	}

	return (0);	/* drop it, we don't need this packet more.
			 * this is a little bit ugly, may be
			 * using of more return codes will be more flexible
			 */
}

/* pxe_icmp_init() - register ICMP protocol in pxe_core protocols table
 * in:
 *	none
 * out:
 *	always 1. TODO: think about making this function void
 */
int
pxe_icmp_init()
{
	/* register protocol in pxe_core protocols table. */
	pxe_core_register(PXE_ICMP_PROTOCOL, pxe_icmp_callback);

	return (1);
}

/* pxe_ping() - pings choosed ip with 32 bytes of data packets
 * in:
 *	ip	- ip to send echo requests
 *	count	- count of requests
 *	flags	- 0 to hide output, 1 to show
 * out:
 *	number of successfull pings
 */
int
pxe_ping(const PXE_IPADDR *ip, int count, int flags)
{

	seq_number = 0;
	last_accepted = 0xffff;
	echo_flags = flags;
	
	/* creating data storage for packet */
	uint8_t		data[sizeof(PXE_IP_HDR) + sizeof(PXE_ICMP_HDR) + 32];
	
	size_t		pack_size =
			    sizeof(PXE_IP_HDR) + sizeof(PXE_ICMP_HDR) + 32;
	PXE_IP_HDR	*iphdr = NULL;
	PXE_ICMP_HDR	*icmphdr = NULL;
	uint32_t	wait_time = 0;
	int		scount = 0;

	if (flags)
		printf("pxe_ping(): pinging %s, 32 bytes\n", inet_ntoa(ip->ip));
	
	pinging.ip = ip->ip;

	iphdr = (PXE_IP_HDR *)data;
	icmphdr = (PXE_ICMP_HDR *)(data + sizeof(PXE_IP_HDR));

	/* base icmp header side */
	icmphdr->type = PXE_ICMP_ECHO_REQUEST;
	icmphdr->code = 0;

	while (seq_number < count) {

		++seq_number;
		
		icmphdr->seq_num = htons(seq_number);
		/* is this good idea? */
		icmphdr->packet_id = (uint16_t)(seq_number*seq_number); 

		/* recalc for every packet */
		icmphdr->checksum = 0;
		icmphdr->checksum =
		    ~(pxe_ip_checksum(icmphdr, sizeof(PXE_ICMP_HDR) + 32));

	    	if (!pxe_ip_send(data, ip, PXE_ICMP_PROTOCOL, pack_size) &&
		    echo_flags)
		{
			printf("pxe_ping(): failed to send echo reply.\n");
		}	

		/* echo reply waiting */
		wait_time = 0;
		
		while (wait_time < PXE_ICMP_TIMEOUT) {
			
			twiddle(1);
			wait_time += 10;
			
			if (!pxe_core_recv_packets())
				delay(10000);
			
			if (last_accepted == seq_number) {

				if (flags)
					printf("< %d ms\n", wait_time);

				++scount;
				break;
			}
		}

		if ( (last_accepted != seq_number) && flags)
			printf("ping timeout.\n");
		
		/* wait a little, to avoid ICMP flood */
		delay(500000);
	}

	pinging.ip = 0;
	echo_flags = 0;
	
	return (scount);
}
#endif
