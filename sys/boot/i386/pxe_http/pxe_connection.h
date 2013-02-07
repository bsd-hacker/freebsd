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

#ifndef PXE_CONNECTION_INCLUDED
#define PXE_CONNECTION_INCLUDED

/*
 * Provides TCP connection related routines
 */
 
#include <stdint.h>

#include "pxe_buffer.h"
#include "pxe_filter.h"
#include "pxe_sock.h"

/* maximum existing connections at one time */
#define PXE_MAX_TCP_CONNECTIONS 8

/* connection states */
#define PXE_TCP_STATE_MASK      0x0f

#define PXE_TCP_CLOSED          0x00    /* closed */

#define PXE_TCP_SYN_SENT        0x01    /* active */
#define PXE_TCP_SYN_RECEIVED    0x02    /* sent & received SYN */
#define PXE_TCP_ESTABLISHED     0x03    /* established connection */
#define PXE_TCP_CLOSE_WAIT      0x04    /* got FIN, waiting to close */
#define PXE_TCP_LAST_ACK        0x05    /* got FIN, closing & waiting FIN ACK */

#define PXE_TCP_FIN_WAIT1       0x06    /* CLOSE, sent FIN */
#define PXE_TCP_CLOSING         0x07    /* got FIN, sent ACK, waiting FIN ACK */
#define PXE_TCP_FIN_WAIT2       0x08    /* got FIN ACK */
#define PXE_TCP_TIME_WAIT       0x09    /* closed, waiting 2MSL*/
#define PXE_TCP_ALL_STATES      10

#define PXE_TCP_BLOCK_COUNT	8
#define PXE_TCP_CHUNK_COUNT	8

typedef struct pxe_tcp_connecton {

    uint8_t     state;          /* current TCP conenction state */
    uint8_t     state_out;      /* show latest acked packet flags
				 * (e.g. we sent FIN and it was ACKed,
				 * here will be PXE_TCP_FIN.
				 */
    uint8_t	winlock;	/* flag becomes 1 when recieve window is zero*/
    uint32_t    next_recv;      /* next sequence number to accept */
    uint32_t    next_send;      /* next sequence number to send */
    uint32_t    una;            /* unaccepted sequence number */
		    
    uint32_t    iss;            /* initial send sequence */
    uint32_t    irs;            /* initial recv sequence */
    uint16_t    remote_window;  /* remote host window size */
				
    uint16_t    src_port;       /* source port */
    uint16_t    dst_port;       /* destination port */
    PXE_IPADDR  dst;         /* destination ip */
					    
    PXE_BUFFER  *recv;          /* recieve buffer */
    PXE_BUFFER  *send;          /* send buffer */
						    
    PXE_FILTER_ENTRY* filter;   /* filter, associated with connection */

    /* current segment to fill, NULL - if unknown */
    /* PXE_TCP_QUEUED_SEGMENT	*segment; */
    void	*segment;
					
    /* send buffer usage map */
    uint8_t     buf_blocks[PXE_TCP_BLOCK_COUNT];
    uint16_t    chunk_size;     /* buffer chunk size */

    /* TODO: check if two members below needed */
    time_t      last_sent;      /* timestamp of last sending event */
    time_t      last_recv;      /* timestamp of last received event */
} PXE_TCP_CONNECTION;

/* initialisztion routine */
void pxe_connection_init();

/* statistics */
void pxe_connection_stats();

/* returns associated connection by filter */
PXE_TCP_CONNECTION * filter_to_connection(PXE_FILTER_ENTRY *filter);

/* initates handshaking */
int pxe_tcp_connect(PXE_SOCKET *sock);

/* initates connection break */
int pxe_tcp_disconnect(PXE_SOCKET* sock);

/* sends user data */
int pxe_tcp_write(PXE_SOCKET *sock, void *data, uint16_t size);

/* receives user data */
int pxe_tcp_read(PXE_SOCKET *sock, void *data, uint16_t size);

/* pushes current segment data */
int pxe_tcp_push(PXE_FILTER_ENTRY *entry);

/* checks connection, by sending ACK */
int pxe_tcp_check_connection(PXE_SOCKET *sock);

/* forces release of unused filters */
void pxe_force_filter_release();

#endif
