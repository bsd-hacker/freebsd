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

#include "pxe_await.h"
#include "pxe_buffer.h"
#include "pxe_connection.h"
#include "pxe_core.h"
#include "pxe_filter.h"
#include "pxe_ip.h"
#include "pxe_segment.h"
#include "pxe_tcp.h"

/* used by pxe_tcp_syssend, when connection have no buffers */
static PXE_BUFFER		sysbuf;
/* buffer space for sysbuf */
static uint8_t			bufdata[PXE_TCP_SYSBUF_SIZE];

#ifdef PXE_TCP_DEBUG_HELL
/* states in  human friendly */
static char			strstates[PXE_TCP_ALL_STATES][15] = {
				    "CLOSED",
				    "SYN_SENT",
				    "SYN_RECEIVED",
				    "ESTABLISHED",
				    "CLOSE_WAIT",
				    "LAST_ACK",
				    "FIN_WAIT_1",
				    "CLOSING",
				    "FIN_WAIT_2",
				    "TIME_WAIT"
				};
#endif
		
/* tcp_check_time_to_die() - moves to CLOSED state connections from state
 *	TIME_WAIT if last received packet (ACK for FIN in most cases)
 *	was more than 2*MSL time ago.
 * in:
 *	connection - connection to check
 * out:
 *	none
 */
static void
tcp_check_time_to_die(PXE_TCP_CONNECTION *connection)
{
	/* if connection in other states - do nothing */
	if (connection->state != PXE_TCP_TIME_WAIT)
		return;
		
	time_t cur_time = pxe_get_secs();

	if (cur_time - connection->last_recv > 2 * PXE_TCP_MSL) {
#ifdef PXE_TCP_DEBUG
		printf("tcp_check_time_to_die(): time for 0x%x connection.\n",
		    connection);
#endif
		/* release filter */
		PXE_FILTER_ENTRY *filter = connection->filter;
		
		if (filter != NULL) /* it must always be non NULL */
			pxe_filter_remove(filter);
		
		/* release connection */
		free_connection(connection);
	}
}

/* tcp_send_rst_for() - sends RST in reply to provided packet
 * in:
 *	tcp_packet - packet which caused RST sending
 * out:
 *	0 - failed
 *	1 - success
 */
static int
tcp_send_rst_for(PXE_TCP_PACKET *tcp_packet, uint32_t ack, uint32_t seq,
		 uint8_t flags, uint16_t seglen)
{
	PXE_TCP_CONNECTION	connection;
	pxe_memset(&connection, 0, sizeof(PXE_TCP_CONNECTION));
			
	connection.dst_port = tcp_packet->tcphdr.src_port;
	connection.src_port = tcp_packet->tcphdr.dst_port;
	connection.dst.ip = tcp_packet->iphdr.src_ip;
	
	connection.next_recv = seq + seglen;	/* acking */
	connection.next_send = ack;		/* next send */
	
	connection.chunk_size = PXE_TCP_SYSBUF_SIZE;
	connection.buf_blocks[0] = PXE_TCP_BLOCK_FREE;
	connection.send = &sysbuf;
	
	return pxe_tcp_syssend(&connection, flags);
}

/* tcp_is_acceptable() - first check for SYN_RECEIVED, ESTABLISHED, FIN_WAIT_1,
 *			FIN_WAIT_2, CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 *	seglen	   - segment length
 * out:
 *	0	- not acceptable
 *	1	- acceptable
 */
int
tcp_is_acceptable(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet,
		  uint16_t seglen)
{
	if (connection == NULL)
		return (0);

	if (connection->recv == NULL)
		return (0);
	
	uint16_t window = pxe_buffer_space(connection->recv);
	uint32_t seq = tcp_packet->tcphdr.sequence;
	
	if (seglen == 0) {
	
		if (window == 0) {
			if (seq == connection->next_recv)
				return (1);
		}
		
		if (connection->next_recv > seq)
			return (0);
		
		if (seq < connection->next_recv + window)
			return (1);
		
	} else { /* segment size > 0 */
	
		if (window == 0)
			return (0);
			
		if ((connection->next_recv <= seq) &&
		    (seq < connection->next_recv + window) )
			return (1);

		/* or? */	
		if ((connection->next_recv <= seq + seglen - 1) &&
		    (seq + seglen - 1 < connection->next_recv + window) )
			return (1);
	}
	
	return (0);
}

/* tcp checks has same numbers as in RFC 793, page 65+ */

/* tcp_check_1() - check if packet is acceptable, sends ACK if not
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 *	seglen	   - segment length
 * out:
 *	0	- not acceptable
 *	1	- acceptable
 */
static int inline
tcp_check_1(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet,
	    uint16_t seglen)
{
	if (tcp_is_acceptable(connection, tcp_packet, seglen))
		return (1);

	PXE_BUFFER	*buffer = connection->recv;
	
	if (buffer && (buffer->bufleft == 0))
		connection->winlock = 1;

	pxe_tcp_syssend(connection, PXE_TCP_ACK);

#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_check_1(): failed\n");
#endif
	return (0);
}

/* tcp_check_2() - check if packet has RST flag
 * in:
 *	tcp_packet - received packet
 * out:
 *	0	- not have
 *	1	- have
 */
static int inline
tcp_check_2(PXE_TCP_PACKET *tcp_packet)
{
	if (tcp_packet->tcphdr.flags & PXE_TCP_RST)
		return (1);

#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_check_2(): failed\n");
#endif	
	return (0);
}

/* tcp_check_3() - check precedence
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 * out:
 *	0	- failed
 *	1	- precedence ok
 */
static int inline
tcp_check_3(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet)
{
	/* TODO?: implement */
	return (1);
}

/* tcp_check_4() - check if packet has SYN flag and sends RST
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 *	seglen	   - segment length
 * out:
 *	0	- not have
 *	1	- have
 */
static int inline
tcp_check_4(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet,
	    uint16_t seglen)
{

	if ( (tcp_packet->tcphdr.flags & PXE_TCP_SYN) == 0) {
#ifdef PXE_TCP_DEBUG_HELL
		printf("tcp_check_4(): failed\n");
#endif
		return (0);
	}
	
	tcp_send_rst_for(tcp_packet, 0, connection->next_send, PXE_TCP_RST,
	    seglen);
	
	return (1);
}

/* tcp_check_5() - check if packet has ACK flag
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 * out:
 *	0	- not have
 *	1	- have
 */
static int inline
tcp_check_5(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet)
{
	if ((tcp_packet->tcphdr.flags & PXE_TCP_ACK) == 0) {
#ifdef PXE_TCP_DEBUG_HELL
		printf("tcp_check_5(): failed\n");
#endif	
		return (0);
	}
		
	uint32_t ack = tcp_packet->tcphdr.ack_next;
	
	if (ack > connection->next_send) {
		/* acked something, that was not sent */
#ifdef PXE_TCP_DEBUG_HELL
		printf("tcp_check_5(): failed, acked %d, but next_snd = %d\n",
		    ack - connection->iss,
		    connection->next_send - connection->iss);
#endif
		pxe_tcp_syssend(connection, PXE_TCP_ACK);
		return (0);
	}
	
	if ( connection->una <= ack) {
	        connection->una = ack;
		pxe_resend_update(connection);
	} else  { /* ignore dublicate packet */
#ifdef PXE_TCP_DEBUG
		printf("tcp_check_5(): failed\n");
#endif
		return (0);
	}
	
	connection->remote_window = tcp_packet->tcphdr.window_size;
	
	return (1);
}

/* tcp_check_6() - check if packet has URG flag
 * in:
 *	tcp_packet - received packet
 * out:
 *	0	- not have
 *	1	- have
 */
static int inline
tcp_check_6(PXE_TCP_PACKET *tcp_packet)
{
	if (tcp_packet->tcphdr.flags & PXE_TCP_URG)
		return (1);

#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_check_6(): failed\n");
#endif	
	return (0);
}

/* tcp_process_7() - processes data and sends ACK
 * in:
 *	connection - connection for which packet received
 *	tcp_packet - received packet
 *	seglen	   - segment length
 * out:
 *	0	- not have
 *	1	- have
 */
static void inline
tcp_process_7(PXE_TCP_CONNECTION *connection, PXE_TCP_PACKET *tcp_packet,
	      uint16_t seglen)
{
	connection->next_recv += seglen;
	
	if (tcp_packet->tcphdr.flags & (PXE_TCP_SYN | PXE_TCP_FIN) )
		connection->next_recv += 1;

	if ( (seglen > 0) && (connection->state == PXE_TCP_ESTABLISHED)) {
		/* write data to buffer, always enough space,
		 * if packet is acceptable
		 */
		void *data = ((void *)tcp_packet) + sizeof(PXE_IP_HDR) +
			     4 * (tcp_packet->tcphdr.data_off >> 4);
			     
		pxe_buffer_write(connection->recv, data, seglen);
	}

	pxe_tcp_syssend(connection, PXE_TCP_ACK);
	connection->last_recv = pxe_get_secs();	
}

/* tcp_check_8() - check if packet has FIN flag
 * in:
 *	tcp_packet - received packet
 * out:
 *	0	- not have
 *	1	- have
 */
static int inline
tcp_check_8(PXE_TCP_PACKET *tcp_packet)
{
	if (tcp_packet->tcphdr.flags & PXE_TCP_FIN)
		return (1);
		
#ifdef PXE_TCP_DEBUG_HELL
	printf("tcp_check_8(): failed\n");
#endif	
	return (0);
}

/* tcp_syn_sent() - SYN_SENT state handler
 * in:
 *	tcp_packet - incoming packet data
 *	connection - current connection
 * out:
 *	0 - don't interested more in this packet
 *	1 - interested
 */
static int
tcp_syn_sent(PXE_TCP_PACKET *tcp_packet, PXE_TCP_CONNECTION *connection,
	     uint16_t seglen)
{
	uint8_t		flags = tcp_packet->tcphdr.flags;
	uint32_t	ack = tcp_packet->tcphdr.ack_next;
	uint32_t	seq = tcp_packet->tcphdr.sequence;
	int		acceptable = 1;
	
	/* first check */
	if ( flags & PXE_TCP_ACK) {
		
		if ( (ack <= connection->iss) ||
		     (ack > connection->next_send) )
		{
			if ( (flags & PXE_TCP_RST) == 0) {
				/* sending RST, if it was not sent to us */
#ifdef PXE_TCP_DEBUG
				printf("tcp_syn_sent(): resetting, ack = %d, "
				       "iss = %d, nxt = %d\n", ack,
				       connection->iss, connection->next_send);
#endif
				tcp_send_rst_for(tcp_packet, ack, 0,
				    PXE_TCP_RST, 0);
			}
			
			/* drop segment and return */
			return (0);
		}
	
		/* check if ACK acceptable */
		if ( (connection->una > ack) || (ack > connection->next_send) )
			acceptable = 0;
	}
	
	/* second check, check RST */	
	if (flags & PXE_TCP_RST) {
	
		if (acceptable == 0) /* just drop */
			return (0);
		
		/* destroy connection */
#ifdef PXE_TCP_DEBUG
		printf("tcp_syn_sent(): new state - CLOSED\n");
#endif		
		connection->state = PXE_TCP_CLOSED;
		return (0);
	}
	
	/* third check */
	/* TODO?: check security/compartment and precedence */
	
	/* fourth check, check SYN */
	if (flags & PXE_TCP_SYN) {

		if (acceptable == 1) {
			connection->next_recv = seq + 1;
			connection->irs = seq;
			connection->una = ack;
			pxe_resend_update(connection);
		}
		
		if ((connection->una > connection->iss) || (acceptable == 1) ) {
		    /* acking */
			if (pxe_tcp_syssend(connection, PXE_TCP_ACK)) {
#ifdef PXE_TCP_DEBUG
				printf("tcp_syn_sent(): new state - ESTABLISHED\n");
#endif			
				connection->state = PXE_TCP_ESTABLISHED;
				connection->last_recv = pxe_get_secs();
			} else
				printf("tcp_syn_sent(): ack syn reply failed.\n");
		
		} else {
			/* enter SYN_RECEIVED, form SYN+ACK */
		}
	}
	
	return (0);
}

/* pxe_tcp_process() - processes incoming packets in states, other than SYN_SENT
 * in:
 *	tcp_packet - incoming packet data
 *	connection - current connection
 *	
 * out:
 *	0 - don't interested more in this packet
 *	1 - interested
 */
static int
pxe_tcp_process(PXE_TCP_PACKET *tcp_packet, PXE_TCP_CONNECTION * connection,
		uint16_t seglen)
{
	uint8_t	state = connection->state;
	uint8_t state_out = connection->state_out;
	
	/* first check, if acceptable at all */
	if (!tcp_check_1(connection, tcp_packet, seglen))
		return (0);

	/* check establishing of commmunication */
	if (state == PXE_TCP_SYN_SENT) {
	    
		if (tcp_packet->tcphdr.flags & PXE_TCP_SYN) {
		
			uint32_t	ack = tcp_packet->tcphdr.ack_next;
			uint32_t	seq = tcp_packet->tcphdr.sequence;

			connection->next_recv = seq + 1;
			connection->irs = seq;
			connection->una = ack;
			pxe_resend_update(connection);
		}
		
		if (pxe_tcp_syssend(connection, PXE_TCP_ACK)) {
			connection->state = PXE_TCP_ESTABLISHED;
			connection->last_recv = pxe_get_secs();
		} else {
			printf("tcp_syn_sent(): ack syn reply failed.\n");
			return (0);
		}
	}
	
	/* check, if have RST flag, sequentially incorrect or have SYN */
	if (( tcp_check_2(tcp_packet)) ||
	    ( tcp_check_4(connection, tcp_packet, seglen)))
	{
		connection->state = PXE_TCP_CLOSED;
		pxe_resend_free(connection);
		
	/*	if (state == PXE_TCP_TIME_WAIT) */
		free_connection(connection);
			
		return (0);
	}

	/* fifth check, if ACK received */
	if (!tcp_check_5(connection, tcp_packet))
		return (0);
	
	switch (state) {
	case PXE_TCP_FIN_WAIT1:
		connection->next_send = tcp_packet->tcphdr.ack_next;
		
		/* if acked our FIN */
		if (state_out == PXE_TCP_FIN) {
			connection->state = PXE_TCP_FIN_WAIT2;
		/*	return (0); */
		}
		break;
	
	case PXE_TCP_FIN_WAIT2:
		connection->next_send = tcp_packet->tcphdr.ack_next;
		break;
	
	case PXE_TCP_CLOSING:
		if (state_out == PXE_TCP_FIN)
			connection->state = PXE_TCP_TIME_WAIT;
		break;
		
	case PXE_TCP_LAST_ACK:
		if (state_out == PXE_TCP_FIN) {
			connection->state = PXE_TCP_CLOSED;
			pxe_resend_free(connection);
			free_connection(connection);
			return (0);
		}
		break;
	
	case PXE_TCP_TIME_WAIT:
		/* in that state only retransmission of FIN may arrive 
		 * ACKing it
		 */
		pxe_tcp_syssend(connection, PXE_TCP_ACK);
		break;
		
	default:
		break;
	}
	
	/* FIN_WAIT_2
	if (tcp_queue_size(connection) == 0) {
		connection->state = PXE_TCP_CLOSE;
	}
	*/
	
	/* sixth check, if urgent. Ignoring. */
	/* if (tcp_check_6(tcp_packet)) 
		return (0);
	*/

	/* seventh, process segment */
	switch (state) {
	case PXE_TCP_ESTABLISHED:
	case PXE_TCP_FIN_WAIT1:
	case PXE_TCP_FIN_WAIT2:
		tcp_process_7(connection, tcp_packet, seglen);
		break;

	case PXE_TCP_LAST_ACK:
		/* if got here, means we have ACK */
		connection->state = PXE_TCP_CLOSED;
		return (0);
		break;
		
	case PXE_TCP_TIME_WAIT:
		/* check TIME_WAIT time */
		tcp_check_time_to_die(connection);
		/* going to return */

	case PXE_TCP_CLOSE_WAIT:
		/* just return */

	default:
		return (0);
		break;
	}
	
	/* eighth, check FIN */
	if (tcp_check_8(tcp_packet)) {
	
		switch (connection->state) {
		case PXE_TCP_ESTABLISHED:
			/* remote host requested connection break */
			connection->state = PXE_TCP_CLOSE_WAIT;
			break;
			
		case PXE_TCP_FIN_WAIT1:
			if (state_out == PXE_TCP_FIN) {
				connection->state = PXE_TCP_TIME_WAIT;
			}  else {
				connection->state = PXE_TCP_CLOSING;
			}
			break;

		case PXE_TCP_FIN_WAIT2:
			connection->state = PXE_TCP_TIME_WAIT;
			break;
			
		default:
			break;
		}
	}

	return (0);
}

/* pxe_tcp_callback() - TCP protocol callback function, executed by pxe_core
 * in:
 *	pack	- packet description
 *	function- function to perform
 * out:
 *	1	- if packet is fragment and code is interested in it
 *	0	- if success or error
 */
int
pxe_tcp_callback(PXE_PACKET *pack, uint8_t function)
{
	PXE_TCP_PACKET	*tcp_packet = pack->data;
	PXE_IPADDR	from;
	PXE_IPADDR	to; 
	
	from.ip = tcp_packet->iphdr.src_ip;
	to.ip = tcp_packet->iphdr.dst_ip;
	
	/* conversion to little endian */
	tcp_packet->tcphdr.sequence = ntohl(tcp_packet->tcphdr.sequence);
	tcp_packet->tcphdr.ack_next = ntohl(tcp_packet->tcphdr.ack_next);
	tcp_packet->tcphdr.src_port = ntohs(tcp_packet->tcphdr.src_port);
	tcp_packet->tcphdr.dst_port = ntohs(tcp_packet->tcphdr.dst_port);

	tcp_packet->tcphdr.window_size =
				    ntohs(tcp_packet->tcphdr.window_size);
	
	uint16_t	src_port = tcp_packet->tcphdr.src_port;
	uint16_t	dst_port = tcp_packet->tcphdr.dst_port;
	
	PXE_IP_HDR	*iphdr = pack->data;

	/* calculating data size from ip length minus headers length */
	uint16_t	data_size = ntohs(iphdr->length) -
				    4 * ((iphdr->ver_ihl & 0x0F) +
				    (tcp_packet->tcphdr.data_off >> 4));
#ifdef PXE_TCP_DEBUG_HELL
	printf("packet: size = %u(%u), ip_hdr = %u(%u), tcp_hdr = %u (%u)\n",
	    data_size, pack->data_size, 4 * (iphdr->ver_ihl & 0x0F),
	    sizeof(PXE_IP_HDR), 4 * (tcp_packet->tcphdr.data_off >> 4),
	    sizeof(PXE_TCP_HDR));
#endif
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_callback(): tcp packet from %s:%u",
	    inet_ntoa(from.ip), src_port);
	    
	printf(" to %s:%u\n",
	    inet_ntoa(to.ip), dst_port);
#endif

	uint8_t		flags = tcp_packet->tcphdr.flags;
	
	PXE_SOCKET	*sock = pxe_filter_check(&from, src_port, &to,
				    dst_port, PXE_TCP_PROTOCOL);

	if (sock == NULL) {	/* nobody is interested in this packet */
#ifdef PXE_TCP_DEBUG
		printf("pxe_tcp_callback(): packet filtered out, sending RST.\n");
#endif
		if (flags & PXE_TCP_ACK)
			tcp_send_rst_for(tcp_packet, 0,
			    tcp_packet->tcphdr.ack_next, PXE_TCP_RST, data_size);
		else
			tcp_send_rst_for(tcp_packet,
			    tcp_packet->tcphdr.ack_next + data_size, 0,
			    PXE_TCP_RST | PXE_TCP_ACK, data_size);
		
		return (0);	
	}
	
	/* inform, we are interested in whole packet */
	if (function == PXE_CORE_FRAG)	
		return (1);

	/* Here filter is not NULL, that means packet is for connected socket or
	 * connection is trying to be established/breaked correctly
	 */
	
	PXE_TCP_CONNECTION *connection = filter_to_connection(sock->filter);

	if (connection == NULL) {
		printf("pxe_tcp_callback(): no connection for filter 0x%x\n",
		    sock->filter);
		    
		pxe_filter_remove(sock->filter);
		return (0);	/* NOTE: this is internal error, if got here */
	}

#ifdef PXE_TCP_DEBUG
	printf(" seq %lu,", tcp_packet->tcphdr.sequence - connection->irs);
	
	if (flags & PXE_TCP_FIN)
	    printf(" fin,");
	
	if (flags & PXE_TCP_SYN)
	    printf(" syn,");
	    
	if (flags & PXE_TCP_RST)
	    printf(" rst,");

	if (flags & PXE_TCP_ACK)
	    printf(" ack %lu,", tcp_packet->tcphdr.ack_next - connection->iss);
	    
	if (flags & PXE_TCP_URG)
	    printf(" urg,");

	if (flags & PXE_TCP_URG)
	    printf(" psh,");
	
	printf(" %u bytes.\n", data_size);
#endif	
	/* TODO:  verify checksum  */
	
	uint32_t seq = tcp_packet->tcphdr.sequence;	
	
	if (flags & PXE_TCP_RST) {
		/* connection aborted (hard error) by remote host */
		connection->state = PXE_TCP_CLOSED;
#ifdef PXE_TCP_DEBUG
		printf("pxe_tcp_callback(): new state - CLOSED\n");
#endif		
		return (0);
	}

	if (connection->state > PXE_TCP_SYN_SENT) {

		/* if we know sequence number, then check it */
		if (seq != connection->next_recv) {
			/* not next in order, drop it, send ACK */
#ifdef PXE_TCP_DEBUG
			printf("pxe_tcp_callback(): got %d != awaited %d\n",
			    seq - connection->irs,
			    connection->next_recv - connection->irs);
#endif
			pxe_tcp_syssend(connection, PXE_TCP_ACK);
			return (0);
		}
	} else {
	    /* in case of SYN_SENT state we don't know sequence number yet */
	}
	
	int result = 0;
	
	/* calling appropriate state handler, if it's not NULL */
	if (connection->state < PXE_TCP_ALL_STATES) {
	
		while (1) {
#ifdef PXE_TCP_DEBUG_HELL
			printf("pxe_tcp_callback(): connection state = %s\n",
			    strstates[connection->state]);
#endif
				
			if (connection->state == PXE_TCP_SYN_SENT) {
				result = tcp_syn_sent(tcp_packet, connection, data_size);
			} else {

				result = pxe_tcp_process(tcp_packet, connection, data_size);
			}
			    
			if (result == 2)
				continue;
			
			break;
		}
	}
	
	/* check ACKed packets */
	pxe_resend_update(connection);
	
	/* check if need to resend some segments */
	pxe_resend_check(connection);
	
	/* check time to die */
	if (connection->state == PXE_TCP_TIME_WAIT)
		tcp_check_time_to_die(connection);
	
	return (result);
}                                                            

/* pxe_tcp_init() - initialization of TCP module
 * in/out:
 *	none
 */
void
pxe_tcp_init()
{
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_init(): started\n");
#endif
	pxe_connection_init();

	/* registering protocol */
	pxe_core_register(PXE_TCP_PROTOCOL, pxe_tcp_callback);

	/* sysbuf init */
	pxe_memset(&bufdata, 0 , PXE_TCP_SYSBUF_SIZE);
	sysbuf.data = &bufdata;
	/* not really need, cause not using buffer realted functions */
	sysbuf.bufleft = PXE_TCP_SYSBUF_SIZE;
	sysbuf.bufsize = PXE_TCP_SYSBUF_SIZE;
	sysbuf.fstart = 0;
	sysbuf.fend = PXE_TCP_SYSBUF_SIZE;
}

/* pxe_tcp_syssend() - send system packets via TCP protocol
 * in:
 *	connection	- connection to send to
 *	tcp_flags	- one or more PXE_TCP_.. flags
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_tcp_syssend(PXE_TCP_CONNECTION *connection, uint8_t tcp_flags)
{
	/* allocating "small" segment */
	PXE_TCP_QUEUED_SEGMENT *segment = tcp_segment_alloc(connection,
						PXE_SEGMENT_SMALL);
	
	if (segment == NULL) {
		printf("pxe_tcp_syssend(): failed to allocate segment.\n");
		return (0);
	}

	/* add to every system segment default options */
	tcp_start_segment(connection, segment, PXE_SEGMENT_OPTS_DEFAULT);
	
	/* finish segment */
	tcp_finish_segment(connection, segment, tcp_flags);
	
	/* Here is simpliest ever in the world way to calculate resend time.
	 * For more reliable resend time calculation need to implement RTT
	 * calculating and use more accurate timer.
	 */
	segment->resend_at = pxe_get_secs() + PXE_RESEND_TIME;

	/* remove other segments with same sequence number,
	 * so this segment is last
	 */
	pxe_resend_drop_same(connection, segment);
	
	if ( /* (connection->state != PXE_TCP_ESTABLISHED) && */
	     (!pxe_tcp_send_segment(connection, segment)))
	{
		printf("pxe_tcp_syssend(): failed to send segment.\n");
		return (0);
	}

	return (1);
}
