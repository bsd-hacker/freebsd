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
			 
#ifndef PXE_TCP_H_INCLUDED
#define PXE_TCP_H_INCLUDED

/*
 * Contains functions needed to transmit tcp packets, using pxe_segment.h module
 */

#include <stdint.h>
#include <stddef.h>

#include "pxe_buffer.h"
#include "pxe_connection.h"
#include "pxe_ip.h"
#include "pxe_filter.h"

/* TCP IP stack protocol number*/
#define PXE_TCP_PROTOCOL	6
/* maximum segment life time in ms */
#define PXE_TCP_MSL		60000
/* buffer size used for system messages for packets without real connection */
#define PXE_TCP_SYSBUF_SIZE	64

/* tcp packet flags */
#define PXE_TCP_FIN 0x01
#define PXE_TCP_SYN 0x02
#define PXE_TCP_RST 0x04
#define PXE_TCP_PSH 0x08
#define PXE_TCP_ACK 0x10
#define PXE_TCP_URG 0x20

typedef struct pxe_tcp_hdr {

    uint16_t src_port;		/* local port */
    uint16_t dst_port;		/* remote port */
    uint32_t sequence;		/* seqence number */
    uint32_t ack_next;		/* ACK'd number */
    uint8_t  data_off;		/* offset to data */
    uint8_t  flags;		/* TCP flags, see higher TCP_FLAG_ */
    uint16_t window_size;	/* current window size */
    uint16_t checksum;		/* packet checksum*/
    uint16_t urgent;		/* urgent flags */
} __packed PXE_TCP_HDR;

/* #define PXE_TCP_MSS	1460 */
#define PXE_TCP_MSS	1260

/* default TCP options for sending data structure */
typedef struct pxe_tcp_default_options {

	uint8_t		kind;	/* kind = 2, maximum segment size option */
	uint8_t		size;	/* size of option including
				 * sizeof(kind) + sizeof(size)
				 */
	uint16_t	mss;	/* maximum segment size in octets */
	uint8_t		end;	/* kind = 0 */
	uint8_t		pad[3];	/* padding, not nessesary */
} __packed PXE_TCP_DEFAULT_OPTIONS;

typedef struct pxe_tcp_packet {

    PXE_IP_HDR			iphdr;
    PXE_TCP_HDR			tcphdr;
} __packed PXE_TCP_PACKET;

typedef struct pxe_tcp_wait_data {

    uint8_t			state;	     /* what state is waited for */
    PXE_TCP_CONNECTION		*connection; /* which connection is monitored */
} __packed PXE_TCP_WAIT_DATA;

/* state function handler type */
typedef int (*pxe_tcp_state_func)(PXE_TCP_PACKET *tcp_packet,
		    PXE_TCP_CONNECTION *connection, uint16_t seglen);

/* init tcp */
void pxe_tcp_init();

/* sends "system" (no user data) segment */
int pxe_tcp_syssend(PXE_TCP_CONNECTION *connection, uint8_t tcp_flags);

#endif // PXE_TCP_H_INCLUDED
