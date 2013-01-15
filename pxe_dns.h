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

#ifndef PXE_DNS_H_INCLUDED
#define PXE_DNS_H_INCLUDED

/* Implements DNS-client for getting A and CNAME records
 * Reference: RFC 1035
 */
 
#include <stdint.h>

/* max seconds to wait DNS reply in milliseconds */
#define PXE_MAX_DNS_TIMEOUT		10000
/* how many times to try, if there is no reply */
#define	PXE_MAX_DNS_TRYS		3
/* query flags, set only RecursionDesired bit */
#define PXE_DNS_DEFAULT_FLAGS		0x0100
/* query A and CNAME records */
#define	PXE_DNS_QUERY_A			0x0001
#define	PXE_DNS_QUERY_CNAME		0x0005
/* query class */
#define	PXE_DNS_CLASS_IN		0x0001
/* maximum UDP packet size */
#define PXE_DNS_MAX_PACKET_SIZE		512

/* returns ip address by name, or 0 if failed */
const PXE_IPADDR *pxe_gethostbyname(char *name);

/* converts string value of ip to uint32_t value */
uint32_t	pxe_convert_ipstr(char *str);

typedef struct pxe_dns_request_hdr {
	uint16_t	id;	/* query identifier */
	uint16_t	flags;


	uint16_t	qdcount; /* number of entries in the question section */
	uint16_t	ancount; /* number of RRs in the answer section */
	uint16_t	nscount; /* name server resource records in the
				  *   authority records section.
				  */
	uint16_t	arcount; /* number of resource records in the additional
				  *   records section.
				  */
} __packed PXE_DNS_REQUEST_HDR;

/* flags are (copied from RFC 1035):
 *                                1  1  1  1  1  1
 *  0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  QR - 0 for query, 1 for reply
 *  OPCODE -  kind of query
 *              0 -  a standard query (QUERY)
 *              1 -  an inverse query (IQUERY)
 *              2 -  a server status request (STATUS)
 *  AA - set if authorative
 *  TC - set if message truncated
 *  RD - set if recursion desired
 *  RA - set if recursion available
 *  Z  - reserved, must be zeroed
 *  RCODE - return code:
 *              0 - no error
 *              1 - format error
 *              2 - server failed
 *              3 - name error
 *              4 - not implemented
 *              5 - refused
 */

/* RCODE values */
#define PXE_RCODE_NOERROR		0x0
#define PXE_RCODE_FORMAT_ERROR		0x1
#define PXE_RCODE_SERVER_FAILED		0x2
#define PXE_RCODE_NAME_ERROR		0x3
#define PXE_RCODE_NOT_IMPLEMENTED	0x4
#define PXE_RCODE_REFUSED		0x5

typedef struct pxe_dns_request_foot {
	uint16_t	qtype;		/* type of query, e.g. A */
	uint16_t	qclass;		/* class of query, e.g. IN */
} __packed PXE_DNS_REQUEST_FOOT;

typedef struct pxe_dns_request_foot2 {
	uint32_t	ttl;	  /* seconds answer will be valid to cache */
	uint16_t	rdlength; /* length of data, followed by this struct */
} __packed PXE_DNS_REQUEST_FOOT2;

typedef struct pxe_dns_wait_data {
	int		socket;	/* socket, to send/recv data */
	uint16_t	id;	/* id, used to differ packets */
	uint8_t		*data;	/* pointer to buffer */
	uint16_t	size;	/* size of buffer */
	uint8_t		*cname; /* not NULL when resolved to CNAME */
	char		*name;	/* name to resolve */
	PXE_IPADDR	result; /* result of resolving */
} PXE_DNS_WAIT_DATA;
#endif
