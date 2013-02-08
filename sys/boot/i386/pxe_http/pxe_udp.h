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

#ifndef PXE_UDP_H_INCLUDED
#define PXE_UDP_H_INCLUDED

/*
 * UDP related declarations
 * Reference: RFC 768
 */
 
#include <stdint.h>
#include <stddef.h>

#include "../libi386/pxe.h"

/* define to use default UDP socket, for incoming packets if there are no
 * filters, sockets and etc
 */
/* #define UDP_DEFAULT_SOCKET */
/* UDP number in IP stack */
#define PXE_UDP_PROTOCOL	0x11

/* UDP header */
typedef struct pxe_udp_hdr {

        uint16_t    src_port;        /* source port */
	uint16_t    dst_port;        /* destination port */
        uint16_t    length;          /* packet total length,
				      * including this header
				      */
	uint16_t    checksum;        /* header, pseudo header and
				      * data checksum
				      */
} __packed  PXE_UDP_HDR;

typedef struct pxe_udp_packet {

	PXE_IP_HDR	iphdr;
	PXE_UDP_HDR	udphdr;
} __packed  PXE_UDP_PACKET;

#define PXE_MAGIC_DGRAM		0x26101982

/* structure is used to store datagrams in receive buffer of socket */
typedef struct pxe_udp_dgram {

	uint32_t	magic;		/* magic for debug purposes */
	PXE_IPADDR	src;		/* ip of dgram sender */
	uint16_t	src_port;	/* source port */
	uint16_t	size;		/* size of datagram */
		
} PXE_UDP_DGRAM;

/* UDP module init */
void pxe_udp_init();

/* UDP module shutdown routine */
void pxe_udp_shutdown();

/* sends udp packet */
int pxe_udp_send(void *data, const PXE_IPADDR *dst, uint16_t dst_port,
	uint16_t src_port, uint16_t size);

/* writes data to udp socket */
/* int pxe_udp_write(PXE_SOCKET *sock, void *buf, uint16_t buflen); */

/* reads data from udp socket */
/*int pxe_udp_read(PXE_SOCKET *sock, void *tobuf, uint16_t buflen,
	PXE_UDP_DGRAM *dgram_out);*/


#endif // PXE_IP_H_INCLUDED
