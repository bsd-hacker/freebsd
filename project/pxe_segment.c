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
			 
#include <stand.h>

#include "pxe_buffer.h"
#include "pxe_segment.h"
#include "pxe_connection.h"
#include "pxe_core.h"
#include "pxe_ip.h"
#include "pxe_tcp.h"

/* pxe_resend_free() - releases all used by connection structures
 * in:
 *	connection	- connection to release data
 * out:
 *	none
 */
void
pxe_resend_free(PXE_TCP_CONNECTION *connection)
{
 /* In fact we must not do anything here due current resendeng queue
  * organization(allocating segments in place), memory released in sockets
  * buffer related code. But in case it'll be redone to be more effective
  * or just using other segment allocation algorithm - this function may
  * be needed.
  */
	 
#if defined(PXE_TCP_DEBUG) && defined(PXE_MORE)
	tcp_resend_stats(connection);
#endif
}

/* pxe_resend_init() - initialize buffer map for connection
 * in:
 *	connection	- connection to initialize
 * out:
 *	none
 */
void
pxe_resend_init(PXE_TCP_CONNECTION *connection)
{
	PXE_BUFFER		*buffer = connection->send;
	PXE_TCP_QUEUED_SEGMENT	*segment = NULL;
	void			*data = buffer->data;

	int			all_chunks =
				    PXE_TCP_BLOCK_COUNT * PXE_TCP_CHUNK_COUNT;
				    
	int			chunk_index = 0;
	
	/* NOTE: may be it's better to define macro for all this values
	 * and don't calculate all_chunks and chunk_size in runtime
	 */
	connection->chunk_size = PXE_DEFAULT_SEND_BUFSIZE / all_chunks;
	
	/* marking all chunks in all blocks free */
	for ( ; chunk_index < all_chunks; ++chunk_index) {

		segment = (PXE_TCP_QUEUED_SEGMENT *)data;
		segment->status = PXE_SEGMENT_FREE;
		
		data += connection->chunk_size;
	}
	
	/* zeroing block map */
	pxe_memset(connection->buf_blocks, 0, PXE_TCP_BLOCK_COUNT);
}

/* tcp_segment_alloc() - allocates from send buffer memory for packet,
 *			including segment data, IP & TCP headers
 * in:
 *	connection	- connection, from which send buffer segment is
 *			allocated
 *	allocBig	- 1 if need big segment, 0 otherwise
 * out:
 *	NULL		- failed to allocate memory chunk(s) for segment
 *	not NULL	- pointer to segment structure
 */
PXE_TCP_QUEUED_SEGMENT *
tcp_segment_alloc(PXE_TCP_CONNECTION *connection, int allocBig)
{
#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_segment_alloc(): connection 0x%x, big = %d.\n",
	    connection, allocBig);
#endif
	int			block_index = 0;
	PXE_BUFFER		*buffer = connection->send;
	uint8_t			*buf_blocks = connection->buf_blocks;
	void			*data = NULL;
	PXE_TCP_QUEUED_SEGMENT	*segment = NULL;
	
	if (connection->send == NULL)
		return (NULL);
	
	for ( ; block_index < PXE_TCP_BLOCK_COUNT; ++block_index) {

		/* start of block */
		data = buffer->data +
		       PXE_TCP_CHUNK_COUNT * block_index * connection->chunk_size;
		       
		segment = (PXE_TCP_QUEUED_SEGMENT *)data;
		
		/* alloc small packet (alloc chunk)? */
		if (allocBig == PXE_SEGMENT_SMALL) {
			/* checking if block is not fully used */
			if (buf_blocks[block_index] < PXE_TCP_BLOCK_USED) {
				/* search free chunk in block */
				int chunk_index = 0;
				
				for ( ; chunk_index < PXE_TCP_CHUNK_COUNT; ++chunk_index) {
					
					if (segment->status == PXE_SEGMENT_FREE) {
					
						segment->status = PXE_SEGMENT_USED;
						buf_blocks[block_index] += 1;
						
						return (segment);
					}
					
					/* next chunk in block */
					data += connection->chunk_size;
					segment = (PXE_TCP_QUEUED_SEGMENT *)data;
				}
			}
			
		} else { /* alloc big one (entire block) */

			if (buf_blocks[block_index] == PXE_TCP_BLOCK_FREE) {

				buf_blocks[block_index] = PXE_TCP_BLOCK_EXCLUSIVE;
				segment->status = PXE_SEGMENT_USED;

				return (segment);
			}
		}
	} /* block_index for end */

	return (NULL);
}

/* pxe_segment_free() - releases used by segment chunks
 * in:
 *	connection	- connection to release chunks
 *	block_index	- index of block to which chunk belongs
 *	segment		- segment to release
 * out:
 *	none
 */
void
tcp_segment_free(PXE_TCP_CONNECTION *connection, int block_index,
		 PXE_TCP_QUEUED_SEGMENT *segment)
{
#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_segment_free(): connection: 0x%x, block: %d, chunk: 0x%x\n",
	    connection, block_index, segment);
#endif
	uint8_t		*block = &connection->buf_blocks[block_index];
	
	if (segment->status == PXE_SEGMENT_FREE) /* already released */
		return;
	
	segment->status = PXE_SEGMENT_FREE;
	
	/* check if block is exlusively used */
	if ( *block == PXE_TCP_BLOCK_EXCLUSIVE)
		*block = PXE_TCP_BLOCK_FREE;
	else /* update used chunk count */
		*block -= 1;
}

/* pxe_resend_check() - checks if need to resend segment
 * in:
 *	connection - connection to check resending needs for
 * out:
 *	none
 */
void
pxe_resend_check(PXE_TCP_CONNECTION *connection)
{
#ifdef PXE_TCP_DEBUG
	printf("pxe_resend_check(): started, state %d.\n", connection->state);
#endif
        PXE_BUFFER              *buffer = connection->send;
        void                    *data = buffer->data;
        int                     block_index = 0;
        PXE_TCP_QUEUED_SEGMENT  *segment = NULL;
	uint8_t			*buf_blocks = connection->buf_blocks;
				
        time_t                  cur_time = pxe_get_secs();
			
        for ( ; block_index < PXE_TCP_BLOCK_COUNT; ++block_index) {
						
                if (buf_blocks[block_index] == PXE_TCP_BLOCK_FREE)
                        continue;       /* block is unused */

		/* pointer to head chunk of block */
		data = buffer->data +
		       block_index * PXE_TCP_CHUNK_COUNT * connection->chunk_size;

                segment = (PXE_TCP_QUEUED_SEGMENT *)data;
		
                /* block is used exclusevely by one "big" packet */
                if (buf_blocks[block_index] == PXE_TCP_BLOCK_EXCLUSIVE) {
		
			if (segment->status != PXE_SEGMENT_SENT)
				continue;	/* it was not ever sent yet */
			
			/* check if it's time to resend */
                        if (cur_time >= segment->resend_at) {
#ifdef PXE_TCP_DEBUG_HELL
				printf("pxe_resend_check(): %lu:%lu "
				       "resending (next try at: %lu)\n",
				    segment->resend_at, cur_time,
				    cur_time +
				    PXE_RESEND_TIME * (segment->trys + 1));
#endif				
				segment->trys += 1;
								
				segment->resend_at = cur_time +
				    PXE_RESEND_TIME * segment->trys;
				
				if (segment->trys == PXE_RESEND_TRYS) {
					/* TODO: need to break connection */
				}
				
				tcp_update_segment(connection, segment);
                                pxe_tcp_send_segment(connection, segment);

            		}
                        continue;
                }

                /* block is dirty, need to check chunks manually  */
                int chunk_index = 0;

                for ( ; chunk_index < PXE_TCP_CHUNK_COUNT; ++chunk_index) {

                        if (segment->status == PXE_SEGMENT_SENT) {
				/* check time to resend */
                                if (cur_time >= segment->resend_at) {
#ifdef PXE_TCP_DEBUG_HELL
					printf("pxe_resend_check(): %lu:%lu "
					       "resending (next try at: %lu)\n",
					    segment->resend_at, cur_time,
					    cur_time +
					    PXE_RESEND_TIME * (segment->trys + 1));
#endif
					/* resend later, with more delay 
					 * with every try */
					segment->trys += 1;
					
					segment->resend_at = cur_time +
					    PXE_RESEND_TIME * segment->trys;

					if (segment->trys == PXE_RESEND_TRYS) {
					/* TODO: need to break connection */
					}
					
					tcp_update_segment(connection, segment);
	                                pxe_tcp_send_segment(connection, segment);
	                        }
	                }

                        /* point segment to next chunk */
			data += connection->chunk_size;
                        segment = (PXE_TCP_QUEUED_SEGMENT *)data;
                }
        } /* block_index for end */
}

/* pxe_resend_drop_same() - removes from resend queue older segments with same
 *			sequence number to avoid duplicate resending of same
 *			ACKs and etc.
 * in:
 *	connection	- connection to update segments for
 *	segment		- segment to check with
 * out:
 *	none
 */
void
pxe_resend_drop_same(PXE_TCP_CONNECTION *connection,
		     PXE_TCP_QUEUED_SEGMENT *new_segment)
{
#ifdef PXE_TCP_DEBUG
	printf("pxe_resend_drop_same(): started.\n");
#endif
	if (connection->send == NULL) {
		printf("pxe_resend_drop_same(): send buffer is NULL.\n");
		return;
	}
	
	uint32_t		drop_seq = new_segment->seq;
        PXE_BUFFER              *buffer = connection->send;
        void                    *data = buffer->data;
        int                     block_index = 0;
        PXE_TCP_QUEUED_SEGMENT  *segment = NULL;
	uint8_t			*buf_blocks = connection->buf_blocks;
				
        for ( ; block_index < PXE_TCP_BLOCK_COUNT; ++block_index) {
		
	        /* block is used exclusevely by one "big" packet, skip this */
                if (buf_blocks[block_index] == PXE_TCP_BLOCK_EXCLUSIVE)
			continue;

                if (buf_blocks[block_index] == PXE_TCP_BLOCK_FREE)
                        continue;       /* block is unused */

		/* pointer to head chunk of block */
		data = buffer->data +
		       block_index * PXE_TCP_CHUNK_COUNT * connection->chunk_size;

                segment = (PXE_TCP_QUEUED_SEGMENT *)data;
		
		if (segment == new_segment)
			continue;
		
                /* block is dirty, need to check chunks manually  */
                int chunk_index = 0;

                for ( ; chunk_index < PXE_TCP_CHUNK_COUNT; ++chunk_index) {
			/* skip segment if it's new_segment */
			if (segment == new_segment) 
				continue;
				
                        if ( (segment->status != PXE_SEGMENT_FREE) &&
			     (segment->seq == drop_seq) )
			{ /* this segment is updated by new segment */
#ifdef PXE_TCP_DEBUG_HELL
				printf("pxe_resend_drop_same(): drop chunk %d#%d.\n",
				    chunk_index, block_index);
#endif			
				tcp_segment_free(connection, block_index,
				    segment);
	                }

                        /* point segment to next chunk */
			data += connection->chunk_size;
                        segment = (PXE_TCP_QUEUED_SEGMENT *)data;
                }
        }	
}

/* pxe_resend_update() - update segments that were acked
 * in:
 *	connection	- connection to update segments for
 * out:
 *	none
 */
void
pxe_resend_update(PXE_TCP_CONNECTION *connection)
{
#ifdef PXE_TCP_DEBUG_HELL
	printf("pxe_resend_update(): started.\n");
#endif
        PXE_BUFFER              *buffer = connection->send;
        void                    *data = buffer->data;
        int                     block_index = 0;
        PXE_TCP_QUEUED_SEGMENT  *segment = NULL;
				
        for ( ; block_index < PXE_TCP_BLOCK_COUNT; ++block_index) {
						
                if (connection->buf_blocks[block_index] == PXE_TCP_BLOCK_FREE)
                        continue;       /* block is unused */
		
		/* pointer to head chunk of block */
		data = buffer->data +
		       block_index * PXE_TCP_CHUNK_COUNT * connection->chunk_size;

                segment = (PXE_TCP_QUEUED_SEGMENT *)data;

                /* block is used exclusevely by one "big" packet */
                if (connection->buf_blocks[block_index] == PXE_TCP_BLOCK_EXCLUSIVE) {
		
			if (segment->status != PXE_SEGMENT_SENT)
				continue;	/* it was not ever sent yet */

                        if (connection->una >= segment->seq) {
				/* segment was acked, release it */
#ifdef PXE_TCP_DEBUG_HELL
				printf("pxe_resend_update(): block %d acked.\n",
				    block_index);
#endif
				tcp_segment_free(connection, block_index,
				    segment);
            		}
                        continue;
                }

                /* block is dirty, need to check chunks manually  */
                int chunk_index = 0;

                for ( ; chunk_index < PXE_TCP_CHUNK_COUNT; ++chunk_index) {

                        if (segment->status == PXE_SEGMENT_SENT) {

                            if (connection->una >= segment->seq) {
				/* segment was acked */
#ifdef PXE_TCP_DEBUG_HELL
				printf("pxe_resend_update(): chunk %d@%d acked.\n",
				    chunk_index, block_index);
#endif
	                            tcp_segment_free(connection, block_index,
					segment);
	                    }
	                }

                        /* point segment to next chunk */
			data += connection->chunk_size;
                        segment = (PXE_TCP_QUEUED_SEGMENT *)data;
                }
        }
}

/* pxe_start_segment() - fills initial data in headers for provided segment
 * in:
 *	connection	- connection to update segments for
 *	segment		- segment to start
 *	add_options	- 1 if add default options (mss), 0 - don't add anything
 * out:
 *	none
 */
void
tcp_start_segment(PXE_TCP_CONNECTION *connection,
		  PXE_TCP_QUEUED_SEGMENT *segment, int add_options)
{
	if (segment == NULL) {
		printf("tcp_start_segment(): segment = NULL.\n");
		return;
	}
	
	PXE_TCP_PACKET  *tcp_packet = (PXE_TCP_PACKET  *)(segment + 1);
		
	/* reserving 8 bytes for options */
	uint16_t length = sizeof(PXE_TCP_PACKET);
	
	tcp_packet->tcphdr.src_port = htons(connection->src_port);
	tcp_packet->tcphdr.dst_port = htons(connection->dst_port);
	tcp_packet->tcphdr.checksum = 0;
	tcp_packet->tcphdr.sequence = htonl(connection->next_send);
	tcp_packet->tcphdr.data_off = sizeof(PXE_TCP_HDR);
	
	if (add_options == PXE_SEGMENT_OPTS_DEFAULT) {
		/* reserving 8 bytes for options */
		length += 8;
		tcp_packet->tcphdr.data_off += 8;
		
		/* pointing to options, leading tcp_header */
		PXE_TCP_DEFAULT_OPTIONS *options =
				    (PXE_TCP_DEFAULT_OPTIONS *)(tcp_packet + 1);
				    
		pxe_memset(options, 0, sizeof(PXE_TCP_DEFAULT_OPTIONS));
		
		options->kind = 2;
		options->size = 4;
		options->mss = htons(PXE_TCP_MSS);
	}
	
	tcp_packet->tcphdr.data_off = (tcp_packet->tcphdr.data_off / 4) << 4;
	tcp_packet->tcphdr.urgent = 0;
	
	segment->trys = 0;
	segment->resend_at = 0;
	segment->size = length;
	segment->seq = connection->next_send;
}

/* pxe_finish_segment() - finishes segmentm calculates checksums and fills
 *			sequence numbers
 * in:
 *	connection	- connection to update segments for
 *	segment		- segment to start
 *	tcp_flags	- flags of header (PXE_TCP_...)
 * out:
 *	none
 */
void
tcp_finish_segment(PXE_TCP_CONNECTION *connection,
		   PXE_TCP_QUEUED_SEGMENT *segment, uint8_t tcp_flags)
{
	if (segment == NULL) {
		printf("tcp_finish_segment(): segment = NULL.\n");
		return;
	}
	
	PXE_TCP_PACKET  *tcp_packet = (PXE_TCP_PACKET  *)(segment + 1);
		
	uint16_t length = segment->size - sizeof(PXE_IP_HDR);
	
	tcp_packet->tcphdr.ack_next = htonl(connection->next_recv);
	tcp_packet->tcphdr.flags = tcp_flags;
												
	PXE_BUFFER *recv_buffer = connection->recv;
	
	/* set window size to free buffer space size,
	 * or to zero if recv_buffer == NULL
	 */
	tcp_packet->tcphdr.window_size = (recv_buffer != NULL) ?
					 htons(recv_buffer->bufleft) : 0;
	tcp_packet->tcphdr.checksum = 0;
	
	PXE_IP4_PSEUDO_HDR      pseudo_hdr;
	const PXE_IPADDR	*my = pxe_get_ip(PXE_IP_MY);

        pseudo_hdr.src_ip = my->ip;
        pseudo_hdr.dst_ip = connection->dst.ip;
        pseudo_hdr.zero = 0;
        pseudo_hdr.proto = PXE_TCP_PROTOCOL;
        pseudo_hdr.length = htons(length);
					
       /* adding pseudo header checksum to checksum of tcp header with data
        * and make it complimentary
        */
	uint16_t part1 = pxe_ip_checksum(&pseudo_hdr,
			    sizeof(PXE_IP4_PSEUDO_HDR));
			    
	uint16_t part2 = pxe_ip_checksum(&tcp_packet->tcphdr, length);
									       
	uint32_t tmp_sum = ((uint32_t)part1) + ((uint32_t)part2);
										       
	if (tmp_sum & 0xf0000)  /* need carry out */
		tmp_sum -= 0xffff;
	
	tcp_packet->tcphdr.checksum = ~((uint16_t)(tmp_sum & 0xffff));
	
	/* special case */
	if (tcp_packet->tcphdr.checksum == 0)
		tcp_packet->tcphdr.checksum = 0xffff;

	/* setting sequence number next to the segment last byte
	 * when connection->una become this value we must remove packet
	 * from resend queue.
	 */
	segment->seq += (length - 4 * (tcp_packet->tcphdr.data_off >> 4));
	
#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_finish_segment(): checksum 0x%4x for %d bytes\n",
	    tcp_packet->tcphdr.checksum, length);
#endif
}

/* pxe_update_segment() - updates segment at resending, recalcs checksum
 *			for updated header
 * in:
 *	connection	- connection to update segments for
 *	segment		- segment to start
 * out:
 *	none
 */
void
tcp_update_segment(PXE_TCP_CONNECTION *connection,
		   PXE_TCP_QUEUED_SEGMENT *segment)
{
	if (segment == NULL) {
		printf("tcp_update_segment(): segment = NULL.\n");
		return;
	}
	
	PXE_TCP_PACKET  *tcp_packet = (PXE_TCP_PACKET  *)(segment + 1);
		
	uint16_t length = segment->size - sizeof(PXE_IP_HDR);
	
	tcp_packet->tcphdr.ack_next = htonl(connection->next_recv);
												
	PXE_BUFFER *recv_buffer = connection->recv;
	
	/* set window size to free buffer space size,
	 * or to zero if recv_buffer == NULL
	 */
	tcp_packet->tcphdr.window_size = (recv_buffer != NULL) ?
					 htons(recv_buffer->bufleft) : 0;
	tcp_packet->tcphdr.checksum = 0;
	
	PXE_IP4_PSEUDO_HDR      pseudo_hdr;
	const PXE_IPADDR	*my = pxe_get_ip(PXE_IP_MY);
	
        pseudo_hdr.src_ip = my->ip;
        pseudo_hdr.dst_ip = connection->dst.ip;
        pseudo_hdr.zero = 0;
        pseudo_hdr.proto = PXE_TCP_PROTOCOL;
        pseudo_hdr.length = htons(length);
					
       /* adding pseudo header checksum to checksum of tcp header with data
        * and make it complimentary
        */
	uint16_t part1 = pxe_ip_checksum(&pseudo_hdr,
			    sizeof(PXE_IP4_PSEUDO_HDR));
			    
	uint16_t part2 = pxe_ip_checksum(&tcp_packet->tcphdr, length);
									       
	uint32_t tmp_sum = ((uint32_t)part1) + ((uint32_t)part2);
										       
	if (tmp_sum & 0xf0000)  /* need carry out */
		tmp_sum -= 0xffff;
	
	tcp_packet->tcphdr.checksum = ~((uint16_t)(tmp_sum & 0xffff));
	
	/* special case */
	if (tcp_packet->tcphdr.checksum == 0)
		tcp_packet->tcphdr.checksum = 0xffff;

#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_update_segment(): checksum 0x%4x for %d bytes\n",
	    tcp_packet->tcphdr.checksum, length);
#endif
}

/* pxe_tcp_send_segment() - send data segment via TCP protocol
 * in:
 *	connection	- connection to which segment belongs
 *      segment         - segment to send
 * out:
 *      0       - failed
 *      1       - success
 */
int
pxe_tcp_send_segment(PXE_TCP_CONNECTION *connection,
		     PXE_TCP_QUEUED_SEGMENT *segment)
{
	if (segment == NULL) {
		printf("pxe_tcp_send_segment(): segment = NULL.\n");
		return (0);
	}
	
	PXE_TCP_PACKET *tcp_packet = (PXE_TCP_PACKET *)(segment + 1);
	
	if (!pxe_ip_send(tcp_packet, &connection->dst,
		PXE_TCP_PROTOCOL, segment->size))
	{
		printf("pxe_tcp_send_segment(): failed send tcp packet to %s\n",
		    inet_ntoa(connection->dst.ip));
		    
		return (0);
	}

	/* mark segment to be checked in resend and update calls*/
	segment->status = PXE_SEGMENT_SENT;
	
#ifdef PXE_TCP_DEBUG
	const PXE_IPADDR	*from = pxe_get_ip(PXE_IP_MY);

	printf("pxe_tcp_send_segment(): tcp packet from %s:%u to ",
		inet_ntoa(from->ip), connection->src_port);
		
	printf("%s:%u\n next seq %lu",
	       inet_ntoa(connection->dst.ip), connection->dst_port,
	       connection->next_send - connection->iss);

	uint8_t flags = tcp_packet->tcphdr.flags;

	if (flags & PXE_TCP_FIN)
	        printf(" fin,");

	if (flags & PXE_TCP_SYN)
	        printf(" syn,");

	if (flags & PXE_TCP_RST)
	        printf(" rst,");

	if (flags & PXE_TCP_ACK)
	        printf(" ack %lu,", connection->next_recv - connection->irs);

	if (flags & PXE_TCP_URG)
	        printf(" urg,");

	if (flags & PXE_TCP_URG)
	        printf(" psh,");
	
	uint16_t length = segment->size - sizeof(PXE_IP_HDR) -
			  4 * (tcp_packet->tcphdr.data_off >> 4);
	
	printf(" %lu bytes.\n", length);
#endif
        return (1);
}

#ifdef PXE_MORE
/* tcp_resend_stats() - shows statistics for chunks in resend queue
 * in:
 *	connection - which connection's resend queue to show
 * out:
 *	none
 */
void
tcp_resend_stats(PXE_TCP_CONNECTION *connection)
{
	int	block_index = 0;
	PXE_BUFFER		*buffer = connection->send;
	uint8_t			*buf_blocks = connection->buf_blocks;
	void			*data = NULL;
	PXE_TCP_QUEUED_SEGMENT	*segment = NULL;
	
	printf("pxe_resend_stats(): stats for connection 0x%x\n", connection);
		
	for ( ; block_index < PXE_TCP_BLOCK_COUNT; ++block_index) {

		/* start of block */
		data = buffer->data +
		       PXE_TCP_CHUNK_COUNT * block_index * connection->chunk_size;
		       
		segment = (PXE_TCP_QUEUED_SEGMENT *)data;
		    
		if (buf_blocks[block_index] != PXE_TCP_BLOCK_FREE) {

		    if (buf_blocks[block_index] != PXE_TCP_BLOCK_EXCLUSIVE) {
			/* search free chunk in block */
			int chunk_index = 0;
				
			for ( ; chunk_index < PXE_TCP_CHUNK_COUNT; ++chunk_index) {
					
			    if (segment->status != PXE_SEGMENT_FREE) {

				printf("\tchunk %d@%d awaiting %lu ack.\n",
				    chunk_index, block_index,
				    segment->seq - connection->iss);
			    }
					
			    /* next chunk in block */
			    data += connection->chunk_size;
			    segment = (PXE_TCP_QUEUED_SEGMENT *)data;
			}
			
		    } else {
			printf("pxe_resend_stats(): block %d awaiting %lu ack.\n",
			    block_index, segment->seq - connection->iss);
		    } /* check exclusive end*/
		} /* check free end */
	} /* cycle end */
}
#endif
