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
 
#ifndef PXE_IP6_H_INCLUDED
#define PXE_IP6_H_INCLUDED

/*
 * IP6 related declarations
 */
 
#include <stdint.h>
#include <stddef.h>

#include "../libi386/pxe.h"

/* IPv6 header */
typedef struct pxe_ip6_hdr {
	uint32_t   label;		/* version (0-3), traffic class (4-11),
					   and label (12-31) */
	uint16_t   use_size;		/* data size */
	uint8_t	   next_hdr;		/* next header */
	uint8_t	   hops;		/* hop count */
	uint8_t    src_ip[16];		/* source ip6 address */
	uint8_t    dst_ip[16];		/* destination ip6 address */
} __packed  PXE_IP_HDR;

/* IPv6 address */
typedef struct pxe_ipaddr {
	        uint8_t  octet[16];
} __packed PXE_IPADDR;

/* often used here broadcast ip */
#define PXE_IP_BCAST	0xffffffff
/* maximum route table size */
#define	PXE_MAX_ROUTES	4

/* routing related structure */
typedef struct pxe_ip6_route_entry {
	PXE_IPADDR	net;	/* network address */
	uint32_t	mask;	/* network mask in ::123/40 format mask == 40 */
	PXE_IPADDR	gw;	/* gateway to this network */
} PXE_IP_ROUTE_ENTRY;

/* fills ipv6 header data */
void pxe_create_ip6_hdr(void *data, const PXE_IPADDR *dst, uint8_t proto,
    uint16_t size, uint16_t opts);

/* sends ipv6 packet */
int pxe_ip6_send(void *data, const PXE_IPADDR *dst, uint8_t proto, uint16_t size);

#endif // PXE_IP6_H_INCLUDED
