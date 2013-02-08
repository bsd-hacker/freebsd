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
 
#ifndef PXE_IP_H_INCLUDED
#define PXE_IP_H_INCLUDED

/*
 * IP related declarations
 * Reference: RFC791
 */
 
#include <stdint.h>
#include <stddef.h>

#include "../libi386/pxe.h"

/* IPv4 header */
typedef struct pxe_ip_hdr {

        uint8_t     ver_ihl;        /* version & IHL (size / 4 octets)*/
	uint8_t     tos;            /* type of service */
        uint16_t    length;         /* packet total length */
	uint16_t    id;             /* packet id */
        uint16_t    data_off;       /* this frame data offset */
	uint8_t     ttl;            /* time to live */
        uint8_t     protocol;       /* protocol */
	uint16_t    checksum;	/* header checksum */
        uint32_t    src_ip;         /* source ip address */
	uint32_t    dst_ip;         /* destination ip address */
} __packed  PXE_IP_HDR;

/* pseudo header, used in checksum calculation for UDP and TCP */
typedef struct pxe_ip4_pseudo_hdr {

        uint32_t    src_ip;     /* source ip */
        uint32_t    dst_ip;     /* destination ip */
        uint8_t     zero;       /* filled by zero */
        uint8_t     proto;      /* protocol */
        uint16_t    length;     /* length (protocol header + data) */
} __packed  PXE_IP4_PSEUDO_HDR;
					
/* IPv4 address */
typedef struct pxe_ipaddr {
        union {
	        uint32_t ip;
	        uint8_t  octet[4];
        };
} __packed PXE_IPADDR;

/* often used here broadcast ip */
#define PXE_IP_BCAST	0xffffffff
/* maximum route table size */
#define	PXE_MAX_ROUTES	4

/* routing related structure */
typedef struct pxe_ip_route_entry {
	PXE_IPADDR	net;	/* network address */
	uint32_t	mask;	/* network mask */
	PXE_IPADDR	gw;	/* gateway to this network */
} PXE_IP_ROUTE_ENTRY;

/* calculates checksum */
uint16_t pxe_ip_checksum(const void *data, size_t size);

/* fills ip header data */
void pxe_create_ip_hdr(void *data, const PXE_IPADDR *dst, uint8_t protocol,
	uint16_t size, uint16_t opts_size);

/* inits routing table */
void pxe_ip_route_init(const PXE_IPADDR *def_gw);

/* adds route to routing table */
int pxe_ip_route_add(const PXE_IPADDR *net, uint32_t mask,
	const PXE_IPADDR *gw);

/* dels route from routing table */
int pxe_ip_route_del(const PXE_IPADDR *net, uint32_t mask,
	const PXE_IPADDR *gw);

/* returns class based netmask for ip */
uint32_t pxe_ip_get_netmask(const PXE_IPADDR *ip);

/* adds default gateway */
int pxe_ip_route_default(const PXE_IPADDR *gw);

/* sends ip packet */
int pxe_ip_send(void *data, const PXE_IPADDR *dst, uint8_t protocol,
	uint16_t size);

/* show route table */
void pxe_ip_route_stat();

#endif // PXE_IP_H_INCLUDED
