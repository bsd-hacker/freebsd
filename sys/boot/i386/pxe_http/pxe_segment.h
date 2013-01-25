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
			 
#ifndef PXE_SEGMENT_INCLUDED
#define PXE_SEGMENT_INCLUDED

/*
 * Contains functions to send/resend tcp segments
 */
 
#include <stdint.h>

#include "pxe_connection.h"

/* free status for segment */
#define PXE_SEGMENT_FREE	0x00
/* segment is used, e.g. data is written to it */
#define PXE_SEGMENT_USED	0x01
/* segment is filled with data, sent but not ACKed yet */
#define PXE_SEGMENT_SENT	0x02
/* default resend time if not acked, in seconds */
#define PXE_RESEND_TIME		1
/* default resend trys */
#define PXE_RESEND_TRYS		5

/* how much blocks in buffer */
#define PXE_TCP_BLOCK_COUNT     8
/* how much chunks in one block */
#define PXE_TCP_CHUNK_COUNT     8

/* chunk is unused */
#define PXE_TCP_BLOCK_FREE      0x00
/* 1..(PXE_TCP_CHUNK_COUNT - 1) are dirty flags, partially used block */
#define PXE_TCP_BLOCK_USED      PXE_TCP_CHUNK_COUNT
/* block is entirely owned by one segment */
#define PXE_TCP_BLOCK_EXCLUSIVE 0x80

/* resend queue in this project organized as just a bunch of segments.
 * to update queue it's needed to check all used chunks.
 * buffer is divided in PXE_TCP_BLOCK_COUNT blocks, each block divided
 * in PXE_TCP_CHUNK_COUNT chunks, so by default we have 64 chunks at all
 * with size 64 bytes for each (for default buffer size 4096)
 * packets may be "small" (e.g. system packets ACK, RST and etc) and contain
 * no user data. In such case usualy will be enough one chunk for packet
 * sizeof(iphdr+tcphdr) = 40 bytes + options < 52 = 64 - sizeof(tcp_queue_segment)
 * also packets may be "big", but not bigger than
 * one block size - sizeof(tcp_queue_segment)
 * in such case packet exclusevely uses all chunks in block for own purposes.
 * by default this means 500 bytes packet maximum (or about 460 bytes of
 * user data per packet). For client-side usage this must be enough in
 * this project
 */

typedef struct pxe_tcp_queued_segment {
        uint32_t	seq;		/* sequence number */
        uint32_t	resend_at;	/* time to ressend at */
        uint16_t	size;		/* size of ready to send packet */
        uint8_t		trys;		/* how many resend trys were done */
        uint8_t		status;		/* segment status */
} PXE_TCP_QUEUED_SEGMENT;



/* checks if need to resend some segments of connection */
void pxe_resend_check(PXE_TCP_CONNECTION *connection);

/* updates resend queue, removes ACKed segments */
void pxe_resend_update(PXE_TCP_CONNECTION *connection);

/* removes segments that are dublicates or old versions of new segment */
void pxe_resend_drop_same(PXE_TCP_CONNECTION *connection,
	PXE_TCP_QUEUED_SEGMENT *segment);

/* destroys resend queue */
void pxe_resend_free(PXE_TCP_CONNECTION *connection);

/* inits buffer map of connection  */
void pxe_resend_init(PXE_TCP_CONNECTION *connection);

/* sends chhosed segment to adrressee */
int pxe_tcp_send_segment(PXE_TCP_CONNECTION *connection,
	PXE_TCP_QUEUED_SEGMENT *segment);

#define PXE_SEGMENT_BIG		1
#define PXE_SEGMENT_SMALL	0
/* allocates in buffer space segment */
PXE_TCP_QUEUED_SEGMENT	*tcp_segment_alloc(PXE_TCP_CONNECTION *connection,
			    int allocBig);

/* releases memory used by segment */
void tcp_segment_free(PXE_TCP_CONNECTION *connection, int block_index,
	PXE_TCP_QUEUED_SEGMENT *segment);

#define PXE_SEGMENT_OPTS_DEFAULT	1
#define PXE_SEGMENT_OPTS_NO		0
/* fills most of fields of tcp header of segment */
void tcp_start_segment(PXE_TCP_CONNECTION *connection,
	PXE_TCP_QUEUED_SEGMENT *segment, int add_options);

/* finishes filling of tcp header, adds checksum */
void tcp_finish_segment(PXE_TCP_CONNECTION *connection,
	PXE_TCP_QUEUED_SEGMENT *segment, uint8_t tcp_flags);

/* when resending updates ack and checksum */
void tcp_update_segment(PXE_TCP_CONNECTION *connection,
	PXE_TCP_QUEUED_SEGMENT *segment);

/* when resending updates ack and checksum */
void tcp_resend_stats(PXE_TCP_CONNECTION *connection);

#endif
