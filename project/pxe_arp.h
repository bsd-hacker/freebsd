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
 
#ifndef PXE_ARP_H
#define PXE_ARP_H

/*
 *  Handles ARP requests and ARP table
 *  Reference: RFC826
 */

#include <stand.h>
#include <string.h>
#include <stdint.h>

#include "../libi386/pxe.h"
#include "pxe_core.h"
#include "pxe_ip.h"

/* max MAC<->ip4 bindings to store */
#define MAX_ARP_ENTRIES	4
/* max try count to send/recieve ARP request/reply */
#define PXE_MAX_ARP_TRY	3
/* max timeout in millyseconds */
#define PXE_TIME_TO_DIE 5000

/* define to enable caching incoming ARP packet sender information */
#define PXE_ARP_SNIFF

typedef struct pxe_arp_entry {
	PXE_IPADDR	addr;
	MAC_ADDR	mac;
} PXE_ARP_ENTRY;


/* initialisation routine */
void pxe_arp_init();

/* find MAC by provided ip */
const MAC_ADDR *pxe_arp_ip4mac(const PXE_IPADDR *addr);

/* protocol handler for received packets */
int pxe_arp_protocol(PXE_PACKET *pack, uint8_t function);

/* ARP table statistics */
void pxe_arp_stats();

/* ARP packet types */
#define PXE_ARPOP_REQUEST	1
#define PXE_ARPOP_REPLY		2

/* protocol types */
#define PXE_PTYPE_IP	0x0800		/* IP4 protocol, used in ARP request */

/* NOTE: here will be realised ARP for Ethernet and IP4 */
typedef struct pxe_arp_packet {
	uint16_t hwtype;	/* hardware type */
	uint16_t ptype;		/* protocol type */
	uint8_t  hsize;		/* size of hardware address */
	uint8_t  psize;		/* size of protocol adress */
	uint16_t operation;
} __packed PXE_ARP_PACKET;

typedef struct pxe_arp_packet_eth4 {
	uint8_t  src_hwaddr[6];		/* source hardware address */
	uint32_t src_paddr;		/* source protocol address */
	uint8_t  target_hwaddr[6];	/* target hardware address if known */
	uint32_t target_paddr;		/* target protocol address if known */
} __packed PXE_ARP_PACKET_ETH4;

typedef struct pxe_arp_pack_data {
	uint8_t			media_hdr[MEDIAHDR_LEN_ETH];
        PXE_ARP_PACKET		hdr;
	PXE_ARP_PACKET_ETH4	body;
} __packed PXE_ARP_PACK_DATA;

typedef struct pxe_arp_wait_data {
	PXE_IPADDR		addr;
	const MAC_ADDR		*mac;
} PXE_ARP_WAIT_DATA;

#endif
