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
 
#ifndef PXE_ICMP_H_INCLUDED
#define PXE_ICMP_H_INCLUDED

/* ICMP related code
 * reference: RFC792
 */
 
#include <stdint.h>

#include "pxe_ip.h"

/* ICMP protocol number in IP stack */
#define PXE_ICMP_PROTOCOL	0x01
/* ICMP header */
typedef struct pxe_icmp_hdr {
    uint8_t	type;		/* type of ICMP  packet */
    uint8_t	code;		/* code, used to identify session */
    uint16_t	checksum;	/* ICMP header checksum */
    uint16_t	packet_id;	/* for echo */
    uint16_t	seq_num;	/* for echo */
} __packed PXE_ICMP_HDR;

/* timeout in milliseconds */
#define PXE_ICMP_TIMEOUT	5000

/* pxe_ping - send icmp echo request packets to host
 *    in:
 *       ip      - host ip address
 *       count   - packets to send
 *	 flags	 - 1 echo ping information to screen
 *   out:
 *       successfull recieved echo's count
 */
int pxe_ping(const PXE_IPADDR *ip, int count, int flags);

/* pxe_icmp_init - inits icmp protocol
 *    in:
 *       none
 *   out:
 *       positive    - if successful
 *       0           - failed
 */
int pxe_icmp_init();

#define PXE_ICMP_ECHO_REPLY         0
#define PXE_ICMP_DEST_UNREACHABLE   3
#define PXE_ICMP_REDIRECT_MESSAGE   5
#define PXE_ICMP_ECHO_REQUEST       8

#define PXE_ICMP_ALT_HOST_ADAPTER   6
#define PXE_ICMP_SOURCE_QUENCH      4


/*  other packet types
 *    9 - Router Advertisement
 *   10 - Router Solicitation
 *   11 - Time Exceeded
 *   12 - Parameter Problem
 *   13 - Timestamp
 *   14 - Timestamp Reply
 *   15 - Information Request
 *   16 - Information Reply
 *   17 - Address Mask Request
 *   18 - Address Mask Reply
 *   30 - Traceroute
 *   31 - Datagram Conversion Error
 *   32 - Mobile Host Redirect
 *   35 - Mobile Registration Request
 *   36 - Mobile Registration Reply
 *   38 - Domain Name Reply
 */
#endif // PXE_ICMP_H_INCLUDED
