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
 
#ifndef PXE_DHCP_H_INCLUDED
#define PXE_DHCP_H_INCLUDED

/* Implements simple DHCP client, able to retrieve nameserver
 * and gateway ip data from DHCP-server.
 * Reference: RFC 2131
 */
 
#include <stdint.h>

/* define if want use bootp() instead of functions, provided by
 * pxe_dhcp module 
 * NOTE: bootp() doesn't sets nameserver
 */
/* #define PXE_BOOTP_USE_LIBSTAND */


/* DHCP request/reply packet header */
typedef struct pxe_dhcp_hdr {
	uint8_t		op;	/* request or reply */
	uint8_t		htype;	/* harsware type */
	uint8_t		hlen;	/* hardware address length */
	uint8_t		hops;	/* used by relay agents, zero for client */
	uint32_t	xid;	/* transaction id */
	uint16_t	secs;	/* time elapsed after renewal process */
	uint16_t	flags;
	uint32_t	ciaddr;	/* client ip, filled in BOUND, RENEW,
				 * or REBINDING */
	uint32_t	yiaddr;	/* client ip addr */
	uint32_t	siaddr;	/* next server ip */
	uint32_t	giaddr;	/* relay agent ip */
	uint8_t		chaddr[16];	/* client hardware address */
	uint8_t		sname[64];	/* optional server hostname */
	uint8_t		file[128];	/* boot file name */
	uint32_t	magic;		/* DHCP magic cookie */
} __packed PXE_DHCP_HDR;

/* options  structure */
typedef struct pxe_dhcp_opt_hdr {
	uint8_t	option;			/* option id */
	uint8_t len;			/* size of data,
					 * followed after this member */
} __packed PXE_DHCP_OPT_HDR;

/* dhcp packet types */
#define PXE_DHCP_REQUEST		0x01
#define PXE_DHCP_REPLY			0x02
/* maximal size of buffer for packet */
#define PXE_MAX_DHCPPACK_SIZE		1024
/* broadcast packet flag */
#define PXE_DHCP_BROADCAST		0x8000
/* magic */
#ifdef VM_RFC1048
/* it's defined in pxe.h */
    #define	PXE_MAGIC_DHCP		VM_RFC1048
#else
    #define	PXE_MAGIC_DHCP		0x63825363
#endif

/* DHCP server port*/
#define PXE_DHCP_SERVER_PORT		67
/* local port to listen replies */
#define PXE_DHCP_CLIENT_PORT		68

#define PXE_DHCPDISCOVER 		0x01
#define PXE_DHCPOFFER			0x02
#define PXE_DHCPREQUEST			0x03
#define PXE_DHCPACK       		0x05
#define PXE_DHCPINFORM       		0x08

/* theese are unused in this DHCP client
#define PXE_DHCPDECLINE			0x04
#define PXE_DHCPNAK			0x06
#define PXE_DHCPRELEASE			0x07
*/

/* DHCP options */
#define PXE_DHCP_OPT_NETMASK		1
#define PXE_DHCP_OPT_ROUTER		3
#define PXE_DHCP_OPT_NAMESERVER		6
#define PXE_DHCP_OPT_DOMAIN_NAME	15
#define PXE_DHCP_OPT_ROOTPATH		17
#define PXE_DHCP_OPT_BROADCAST_IP	28
#define PXE_DHCP_OPT_REQUEST_IP		50
#define PXE_DHCP_OPT_LEASE_TIME		51
#define PXE_DHCP_OPT_TYPE		53
#define PXE_DHCP_OPT_ID			54
#define PXE_DHCP_OPT_RENEWAL_TIME	58
#define PXE_DHCP_OPT_REBINDING_TIME	59
#define PXE_DHCP_OPT_WWW_SERVER		72
#define PXE_DHCP_OPT_END		255

/* used in await function */
typedef struct pxe_dhcp_wait_data {
	int		socket;		/* current socket to check replies */
	uint8_t		*data;		/* query packet data */
	uint16_t	size;		/* max size of packet */
	uint32_t	xid;		/* session id */
} PXE_DHCP_WAIT_DATA;

typedef struct pxe_dhcp_parse_result {
	PXE_IPADDR	netmask;
	PXE_IPADDR	bcast_addr;
	PXE_IPADDR	ns;
	PXE_IPADDR	gw;
	PXE_IPADDR	www;
	char		rootpath[256];
	uint8_t		message_type;
} PXE_DHCP_PARSE_RESULT;

/* sends DHCPINFORM packet and updates nameserver
 * and gateway data
 */
void pxe_dhcp_query(uint32_t xid);

/* prints out known DHCP options */
void pxe_dhcp_parse_options(uint8_t *opts, uint16_t max_size,
	PXE_DHCP_PARSE_RESULT *res);

#endif
