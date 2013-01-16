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

#include "pxe_buffer.h"
#include "pxe_core.h"
#include "pxe_filter.h"
#include "pxe_ip.h"
#include "pxe_sock.h"
#include "pxe_udp.h"

#ifdef UDP_DEFAULT_SOCKET
PXE_BUFFER	def_buffer;
#endif

/* pxe_udp_callback() - UDP protocol callback function, executed by pxe_core
 * in:
 *	pack	- packet description
 *	function- function to perform
 * out:
 *	1	- if packet is fragment and code is interested in it
 *	0	- if success or error
 */
int
pxe_udp_callback(PXE_PACKET *pack, uint8_t function)
{
	PXE_UDP_PACKET	*udp_packet = pack->data;
	PXE_IPADDR	from;
	PXE_IPADDR	to; 
	
	from.ip = udp_packet->iphdr.src_ip;
	to.ip = udp_packet->iphdr.dst_ip;

	uint16_t	src_port = ntohs(udp_packet->udphdr.src_port);
	uint16_t	dst_port = ntohs(udp_packet->udphdr.dst_port);	

#ifdef PXE_DEBUG
	printf("pxe_udp_callback(): udp packet from %s:%u to ",
	    inet_ntoa(from.ip), src_port);
	
	printf("%s:%u\n", inet_ntoa(to.ip), dst_port);
#endif

	PXE_SOCKET *sock = pxe_filter_check(&from, src_port, &to, dst_port,
				PXE_UDP_PROTOCOL);

	if (sock == NULL) {	/* nobody is interested in this packet */
#ifndef UDP_DEFAULT_SOCKET
#ifdef PXE_DEBUG
		printf("pxe_udp_callback(): packet filtered out.\n");
#endif
		return (0);	
#endif
	}
	
	/* informm, we are interested in whole packet */
	if (function == PXE_CORE_FRAG)	
		return (1);
		
	/* TODO:  verify checksum  */

	uint16_t data_size = pack->data_size - sizeof(PXE_UDP_PACKET);

#ifdef UDP_DEFAULT_SOCKET
	PXE_BUFFER* recv_buffer =
			(sock != NULL) ? &sock->recv_buffer : &def_buffer;
#else
	PXE_BUFFER* recv_buffer = &sock->recv_buffer;
#endif
	PXE_UDP_DGRAM udp_dgram;
		
	if (pxe_buffer_space(recv_buffer) < data_size + sizeof(PXE_UDP_DGRAM))
		printf("pxe_udp_callback(): socket 0x%x buffer has no space\n",
		    sock);
	else {
		udp_dgram.magic = PXE_MAGIC_DGRAM;
		udp_dgram.src.ip = from.ip;
		udp_dgram.src_port = src_port;
		udp_dgram.size = data_size;
		
		/* NOTE: here is assuming that there is no other writings to
		 * buffer, so, to writes, place data sequentially in bufer.
		 */
		pxe_buffer_write(recv_buffer, &udp_dgram,
		    sizeof(PXE_UDP_DGRAM));
		    
		pxe_buffer_write(recv_buffer,
		    pack->data + sizeof(PXE_UDP_PACKET), data_size);
	}
	
	return (0);
}

/* pxe_udp_init() - initialization of UDP module
 * in/out:
 *	none
 */
void
pxe_udp_init()
{
#ifdef PXE_DEBUG
	printf("pxe_udp_init(): started\n");
#endif
	pxe_core_register(PXE_UDP_PROTOCOL, pxe_udp_callback);
	
#ifdef UDP_DEFAULT_SOCKET
	pxe_buffer_memalloc(&def_buffer, 16384);
#endif
}


/* pxe_udp_shutdown() - cleanup of used memory buffer
 * in/out:
 *	none
 */
void
pxe_udp_shutdown()
{
#ifdef UDP_DEFAULT_SOCKET
	pxe_buffer_memfree(&def_buffer);
#endif
}


/* pxe_udp_send() - send data via UDP protocol
 * in:
 *	data	- buffer of data to send
 *	dst_ip	- destination IP address
 *	dst_port- destination port
 *	src_port- source port
 *	size	- size of data
 *	flags	- 1 if space for UDP & IP headers reserved in buffer,
 *		  0 otherwise
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_udp_send(void *data, const PXE_IPADDR *dst, uint16_t dst_port,
	     uint16_t src_port, uint16_t size)
{
	PXE_UDP_PACKET	*udp_packet = (PXE_UDP_PACKET *)data;

	uint16_t length = size - sizeof(PXE_IP_HDR);
	udp_packet->udphdr.src_port = htons(src_port);
	udp_packet->udphdr.dst_port = htons(dst_port);
	udp_packet->udphdr.length = htons(length);
	udp_packet->udphdr.checksum = 0;

	PXE_IP4_PSEUDO_HDR	pseudo_hdr;
	const PXE_IPADDR	*my = pxe_get_ip(PXE_IP_MY);

#ifdef PXE_DEBUG
        printf("pxe_udp_send(): %s:%u -> ",
	    inet_ntoa(my->ip), src_port);
	    
	printf("%s:%u, size = %u bytes.\n",
	    inet_ntoa(dst->ip), dst_port, size);
#endif

	pseudo_hdr.src_ip = my->ip;
        pseudo_hdr.dst_ip = dst->ip;
	pseudo_hdr.zero = 0;
	pseudo_hdr.proto = PXE_UDP_PROTOCOL;
        pseudo_hdr.length = udp_packet->udphdr.length;

	/* adding pseudo header checksum to checksum of udp header with data
	 * and make it complimentary 
	 */

	uint16_t part1 = pxe_ip_checksum(&pseudo_hdr,
			    sizeof(PXE_IP4_PSEUDO_HDR));
			    
	uint16_t part2 = pxe_ip_checksum(&udp_packet->udphdr, length);
	
	uint32_t tmp_sum = ((uint32_t)part1) + ((uint32_t)part2);

	if (tmp_sum & 0xf0000) /*need carry out */
		tmp_sum -= 0xffff;
	
	udp_packet->udphdr.checksum = ~((uint16_t)(tmp_sum & 0xffff));
	
	/* special case */
	if (udp_packet->udphdr.checksum == 0)
		udp_packet->udphdr.checksum = 0xffff;
	
#ifdef PXE_DEBUG_HELL
	printf("pxe_udp_send(): checksum 0x%4x for %u bytes\n",
	    udp_packet->udphdr.checksum, length);
#endif

	if (!pxe_ip_send(udp_packet, dst, PXE_UDP_PROTOCOL,
		length + sizeof(PXE_IP_HDR)))
	{
		printf("pxe_udp_send(): failed to send udp packet to %s\n",
		    inet_ntoa(dst->ip));

		return (0);
	}
		
	return (1);	
}

/* pxe_udp_read() - performs reading from UDP socket
 * in:
 *	sock		- UDP socket to read from
 *	tobuf		- buffer, where to read
 *	buflen		- buffer size
 *	dgram_out	- if not NULL, here placed dgram info
 * out:
 *	-1	- failed
 *	>= 0	- actual bytes were read
 */
int
pxe_udp_read(PXE_SOCKET *sock, void *tobuf, uint16_t buflen,
	     PXE_UDP_DGRAM *dgram_out)
{
        PXE_UDP_DGRAM   udp_dgram;
	
	PXE_BUFFER	*buffer = NULL;

	if (sock == NULL) {
#ifndef UDP_DEFAULT_SOCKET
		return (-1); /* bad socket */
#else
		buffer = &def_buffer;
#endif
	} else 
		buffer = &sock->recv_buffer;

	if (buffer == NULL) {
#ifdef PXE_DEBUG
                printf("pxe_udp_read(): NULL buffer.\n");
#endif
                return (0);
	}
	
	uint16_t	usage = buffer->bufsize - pxe_buffer_space(buffer);
	  
        if (sizeof(PXE_UDP_DGRAM) != pxe_buffer_read(buffer, &udp_dgram,
	    sizeof(PXE_UDP_DGRAM)))
	{
#ifdef PXE_DEBUG_HELL
                printf("pxe_udp_read(): failed to read datagram data.\n");
#endif
                return (0);
        }

        if (udp_dgram.magic != PXE_MAGIC_DGRAM) { /* sanity check failed */
#ifdef PXE_DEBUG
                printf("pxe_udp_sock_recv(): dgram magic failed.\n");
#endif
                return (0);
        }

        uint16_t tocopy = ((uint16_t)buflen < udp_dgram.size) ?
			    (uint16_t)buflen : udp_dgram.size;
			    
        uint16_t result = pxe_buffer_read(buffer, tobuf, tocopy);

        if (result < udp_dgram.size) /* free truncated dgram part */
                pxe_buffer_read(buffer, NULL, udp_dgram.size - result);

	if (dgram_out != NULL) {
		pxe_memcpy(&udp_dgram, dgram_out, sizeof(PXE_UDP_DGRAM));
	}
		
        return ((int)result);
}

/* pxe_udp_write() - performs writing to UDP socket
 * in:
 *	sock	- UDP socket to write to
 *	tobuf	- buffer  with data to write
 *	buflen	- buffer size
 * out:
 *	-1	- failed
 *	>= 0	- actual bytes were written
 */
int
pxe_udp_write(PXE_SOCKET *sock, void *buf, uint16_t buflen)
{

        if (buflen + sizeof(PXE_UDP_PACKET) > PXE_DEFAULT_SEND_BUFSIZE) {
                printf("pxe_udp_write(): send buffer too small for %d bytes.\n",
		    buflen);
		    
                return (-1);
	}
						
        /* for UDP socket, send buffer used only for one dgram */
        PXE_UDP_PACKET		*udp_pack =
				    (PXE_UDP_PACKET *)sock->send_buffer.data;
				    
	PXE_FILTER_ENTRY	*filter = sock->filter;
										
        if (filter == NULL) { /* not connected socket */
                printf("pxe_udp_write(): socket is not connected.\n");
                return (-1);
        }
																
        /* copy user data */
        pxe_memcpy(buf, udp_pack + 1, buflen);
	
	const PXE_IPADDR *my = pxe_get_ip(PXE_IP_MY);

#ifdef PXE_DEBUG
        printf("pxe_udp_write(): %s:%u -> ",
	    inet_ntoa(my->ip), filter->dst_port);
	
	printf("%s:%u, size = %u bytes.\n",
	    inet_ntoa(filter->src.ip), filter->src_port, buflen);
#endif

        if (!pxe_udp_send(udp_pack, &filter->src, filter->src_port,
	    filter->dst_port, buflen + sizeof(PXE_UDP_PACKET)))
	{
		printf("pxe_udp_write(): failed to send data.\n");
                return (-1);
        }

        return (buflen);
}
