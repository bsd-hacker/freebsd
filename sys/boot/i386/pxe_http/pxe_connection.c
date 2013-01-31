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
#include "pxe_connection.h"
#include "pxe_core.h"
#include "pxe_filter.h"
#include "pxe_ip.h"
#include "pxe_segment.h"
#include "pxe_sock.h"
#include "pxe_tcp.h"

/* connection structs storage */
static PXE_TCP_CONNECTION       tcp_connections[PXE_MAX_TCP_CONNECTIONS];
/* currently allocated connections */
static int                      all_connections = 0;

/* filter_to_connection() - returns connections,
 *			    associated with provided filter
 * in:
 *	filter - pointer to filter entry structure,
 *		for which connection is searched
 * out:
 *	NULL	- failed to find
 *	not NULL- searched connections
 */
PXE_TCP_CONNECTION *
filter_to_connection(PXE_FILTER_ENTRY *filter)
{
	int con_index = 0;
	
	for ( ; con_index < PXE_MAX_TCP_CONNECTIONS; ++con_index) {

		if (tcp_connections[con_index].filter == filter)
			return (&tcp_connections[con_index]);
	}

	return (NULL);
}

/* alloc_connection() - returns pointer to free connection structure
 * in:
 *	none
 * out:
 *	NULL	- failed to alloc
 *	non NULL- pointer to allocated structure
 */
PXE_TCP_CONNECTION *
alloc_connection()
{

	if (all_connections == PXE_MAX_TCP_CONNECTIONS)
		return (NULL);
		
	uint16_t	index = 0;
	
	for ( ; index < PXE_MAX_TCP_CONNECTIONS; ++index) {
	
		if (tcp_connections[index].state == PXE_TCP_CLOSED) {
			/* if state is closed, it's free structure*/
			all_connections += 1;
			return &tcp_connections[index];
		}
	}
	
	/* NOTE: we must not ever get here */
	return (NULL);
}

/* force_alloc_connection() - returns pointer to free connection structure
 *			forces connection structures in TIME_WAIT state to
 *			be allocated if there are no free connection
 *			structure.
 * in:
 *	none
 * out:
 *	NULL	- failed to alloc
 *	non NULL- pointer to allocated structure
 */
PXE_TCP_CONNECTION *
force_alloc_connection()
{

	if (all_connections < PXE_MAX_TCP_CONNECTIONS)
		return alloc_connection();
		
	uint16_t	index = 0;
	
	for ( ; index < PXE_MAX_TCP_CONNECTIONS; ++index) {
	
		if (tcp_connections[index].state == PXE_TCP_TIME_WAIT) {
			
			tcp_connections[index].state = PXE_TCP_CLOSED;
			
			/* release filter */
			PXE_FILTER_ENTRY *filter = tcp_connections[index].filter;
					
			if (filter != NULL) /* it must always be non NULL */
			        pxe_filter_remove(filter);
#ifdef PXE_TCP_DEBUG
			printf("force_alloc_connection(): forced allocation\n");
#endif
			return &tcp_connections[index];
		}
	}
	
	return (NULL);
}

/* pxe_force_filter_release() - releases filter if used by connections
 *			in TIME_WAIT state. Needed when filters table
 *			is full, but there are no really active connections.
 * in/out:
 *	none
 */
void
pxe_force_filter_release()
{
	uint16_t	index = 0;
	
	for ( ; index < PXE_MAX_TCP_CONNECTIONS; ++index) {
	
		if (tcp_connections[index].state == PXE_TCP_TIME_WAIT) {
			/* free also connection structure */
			tcp_connections[index].state = PXE_TCP_CLOSED;
			
			/* release filter */
			PXE_FILTER_ENTRY *filter = tcp_connections[index].filter;
					
			if (filter != NULL) /* it must always be non NULL */
			        pxe_filter_remove(filter);
				
			all_connections -= 1;
#ifdef PXE_TCP_DEBUG
			printf("pxe_force_filter_release(): filter released.\n");
#endif
			break;
		}
	}
}

/* free_connection() - releases connections
 * in:
 *	connection - pointer to connection to release
 *		(assuming it's valid connection)
 * out:
 *	none
 */
void
free_connection(PXE_TCP_CONNECTION *connection)
{

	connection->state = PXE_TCP_CLOSED;
	all_connections -= 1;
#ifdef PXE_TCP_DEBUG_HELL
	printf("free_connection(): %d connections used\n", all_connections);
#endif
}

/* tcp_await() - await function for some TCP protocol functions (handshaking,
 *	breaking connection).
 * NOTE:
 *	main work is done in pxe_tcp_callback()
 */
int
tcp_await(uint8_t function, uint16_t try_number, uint32_t timeout, void *data)
{
	PXE_TCP_WAIT_DATA *wait_data = (PXE_TCP_WAIT_DATA *)data;
	PXE_TCP_CONNECTION *conn = wait_data->connection;
	
        switch(function) {
	        case PXE_AWAIT_NEWPACKETS:
			/* check current state with needed to wait for */

			if (wait_data->state <= conn->state)
				return (PXE_AWAIT_COMPLETED);
			
			/* CLOSED at waiting means connection was breaked */
			if (conn->state == PXE_TCP_CLOSED)
				return (PXE_AWAIT_BREAK);

			break;		
		
	        case PXE_AWAIT_FINISHTRY:
			if (conn->state == PXE_TCP_CLOSED)
				return (PXE_AWAIT_BREAK);
				
			pxe_resend_check(wait_data->connection);
			break;

	        case PXE_AWAIT_STARTTRY: /* nothing to do */
		case PXE_AWAIT_END:
		default:
			break;
	}
	
	return (PXE_AWAIT_OK);
}

/* pxe_tcp_connect() - connects TCP socket (performs handshaking).
 *			Blocks until handshaking is done.
 * in:
 *	socket - socket
 * out:
 *	0	- failed to connect
 *	1	- handshaking successful
 */
int
pxe_tcp_connect(PXE_SOCKET *sock)
{

/*	if (all_connections == PXE_MAX_TCP_CONNECTIONS) {
		printf("pxe_tcp_connect(): too many connections.\n");
		return (0);
	}
*/
	PXE_FILTER_ENTRY	*filter = sock->filter;	
	PXE_TCP_CONNECTION	*connection = force_alloc_connection();

	if (connection == NULL) {
		printf("pxe_tcp_connect(): too many connections.\n");
		return (0);
	}
	
	pxe_memset(connection, 0, sizeof(PXE_TCP_CONNECTION));
			
	connection->dst_port = filter->src_port;
	connection->src_port = filter->dst_port;
	connection->dst.ip = filter->src.ip;
	connection->next_recv = 0;
	
	/* NOTE: need to make more correct initial number */
	connection->iss = (filter->src.ip + filter->dst.ip) +
			  (uint32_t)pxe_get_secs();
			  
	connection->next_send = connection->iss;
	
	connection->filter = filter;
	connection->recv = &sock->recv_buffer;
	connection->send = &sock->send_buffer;
	
	pxe_resend_init(connection);

	if (!pxe_tcp_syssend(connection, PXE_TCP_SYN)) {
		printf("pxe_tcp_connect(): failed to send SYN.\n");
		free_connection(connection);
		return (0);
	}

	connection->state = PXE_TCP_SYN_SENT;
	connection->next_send = connection->iss + 1;
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_connect(): new state - SYN_SENT\n");
#endif		
	PXE_TCP_WAIT_DATA wait_data;
	wait_data.connection = connection;
	
	wait_data.state = PXE_TCP_ESTABLISHED;
	
	/* await ESTABLISHED state.
	 * connection will fell in this state in pxe_tcp_callback(),
	 * after receiving SYN ACK and sending ACK to remote host
	 */
	if (!pxe_await(tcp_await, 5, PXE_TCP_MSL / 5, &wait_data)) {
		/* failed to get SYN/ACK */
	    	free_connection(connection);
		return (0);
	}

#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_connect(): connection established.\n");
#endif	
	return (1);
}

/* pxe_tcp_disconnect() - interrupts TCP connection. Blocks until is done.
 * in:
 *	socket - socket
 * out:
 *	0	- failed to disconnect (timeout)
 *	1	- disconnect successful
 */
int
pxe_tcp_disconnect(PXE_SOCKET *sock)
{
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_disconnect(): started.\n");
#endif
	PXE_FILTER_ENTRY	*filter = sock->filter;	
	
	if (filter == NULL) {
		/* NULL filters means there are no connection for socket */
		printf("pxe_tcp_disconnect(): NULL filter\n");
		return (1);
	}
	
	PXE_TCP_CONNECTION	*connection = filter_to_connection(filter);

	if (connection == NULL) {
		printf("pxe_tcp_disconnect(): NULL connection\n");
		return (0);
	}

	/* process recieved,  queued but not processed packets.
	 * This is useful if server requested breaking of connection
	 * (passive closing) and our disconnect just finishes initiated
	 * by server sequence, no need to send initial FIN (active closing)
	 */
	pxe_core_recv_packets();
	
	if ( connection->state == PXE_TCP_CLOSED) { /* already  closed */
#ifdef PXE_TCP_DEBUG
		printf("pxe_tcp_disconnect(): connection already is closed.\n");
#endif
		return (1);
	}
	
	if (!pxe_tcp_syssend(connection, PXE_TCP_FIN | PXE_TCP_ACK)) { 
		printf("pxe_tcp_disconnect(): failed to send FIN.\n");
		free_connection(connection);
		return (0);
	}
	/* update sequence number */	
	connection->next_send += 1;
	
	PXE_TCP_WAIT_DATA wait_data;
	wait_data.connection = connection;

	if (connection->state == PXE_TCP_ESTABLISHED) {
		/* active closing by our host */
		connection->state = PXE_TCP_FIN_WAIT1;
#ifdef PXE_TCP_DEBUG
		printf("pxe_tcp_disconnect(): new state - FIN_WAIT_1\n");
#endif
		wait_data.state = PXE_TCP_TIME_WAIT;
	
	} else { /* if connection breaked by remote host */
		connection->state = PXE_TCP_LAST_ACK;
		wait_data.state = PXE_TCP_CLOSED;
	}

	connection->state_out = PXE_TCP_FIN;

	/* awaiting expected state to close connection
	 * connection will fell in this state in pxe_tcp_callback()
	 */
	if (!pxe_await(tcp_await, 5, PXE_TCP_MSL / 5, &wait_data)) {
		/* failed to get expected state */
	    	free_connection(connection);
	
		if (connection->state != PXE_TCP_CLOSED) {
#ifdef PXE_TCP_DEBUG		
			printf("pxe_tcp_disconnect(): felt to wrong state.\n");
#endif
			return (0);
		}
	}

	if (connection->state == PXE_TCP_CLOSED) {
		pxe_filter_remove(filter);
		free_connection(connection);
	}
	
	pxe_resend_free(connection);
	
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_disconnect(): connection closed.\n");
#endif
	return (1);
}

/* pxe_connection_init() - inits connections related structures
 * in/out:
 *	none
 */
void
pxe_connection_init()
{

	/* clear connections data */
	pxe_memset(tcp_connections, 0, sizeof(tcp_connections));
}

/* pxe_tcp_write() - transmit data via TCP protocol
 * in:
 *      sock		- TCP socket to write to
 *	data		- pointer to data to send
 *      size_to_send    - data size
 * out:
 *      -1	- failed
 *      >=0     - actual bytes written
 */
int
pxe_tcp_write(PXE_SOCKET *sock, void *data, uint16_t size_to_send)
{
	PXE_TCP_CONNECTION	*connection = filter_to_connection(sock->filter);

	if (connection == NULL) {
		printf("pxe_tcp_write(): no connection for filter 0x%x "
		       "(socket: 0x%x).\n", sock->filter, sock);
		return (-1);
	}
	
	if ( (connection->state != PXE_TCP_ESTABLISHED) &&
	     (connection->state != PXE_TCP_CLOSE_WAIT) )
	{
		return (-1);	/* cannot write, incorrect state */
	}
	
	/* trying current segment */
	PXE_TCP_QUEUED_SEGMENT *segment = connection->segment;
	
	uint16_t sent_data = 0;
	uint16_t bufleft = 0;
    	uint16_t send_now = 0;
	void	 *segment_data = (void *)(segment + 1);
	
	while (sent_data < size_to_send) {
		
		/* have no allocated segment for writing data, try allocate it */
		if (segment == NULL) {
			/* allocating new segment */
			segment = tcp_segment_alloc(connection, PXE_SEGMENT_BIG);
			
			if (segment == NULL) {
		    		printf("pxe_tcp_write(): failed to allocate segment.\n");
	    			return (sent_data == 0) ? (-1) : sent_data;
			}
		
			connection->segment = segment;
			segment_data = (void *)(segment + 1);
			
			tcp_start_segment(connection, segment,
			    PXE_SEGMENT_OPTS_NO);
		}
		
		/* calculating free space in segment packet */
		bufleft = connection->chunk_size * PXE_TCP_CHUNK_COUNT;
		bufleft -= sizeof(PXE_TCP_QUEUED_SEGMENT) + segment->size;
		/* how much left to send */
		send_now = size_to_send - sent_data;

		if (send_now < bufleft) {
			/* copy data to segment space, actually there is no send,
			 * till segment is fully filled or called pxe_tcp_push()
			 */
			pxe_memcpy(data + sent_data,
			    segment_data + segment->size, send_now);
			    
			segment->size += send_now;
			sent_data += send_now;
			
			return (sent_data);
		}
		
		/* if we got here, then we need to finish current segment
		 * and alloc new segment
		 */
		pxe_memcpy(data + sent_data,
		    segment_data + segment->size, bufleft);
		    
		segment->size += bufleft;
		sent_data += bufleft;
		
		/* finish segment */
		tcp_finish_segment(connection, segment, PXE_TCP_ACK);
		/* updating next_send counter */
		connection->next_send += segment->size - sizeof(PXE_TCP_PACKET);

		segment->resend_at = pxe_get_secs() + PXE_RESEND_TIME;
    
		if (!pxe_tcp_send_segment(connection, segment)) {
			printf("pxe_tcp_write(): failed to send segment.\n");
			/* this segment will be resent later,
			 * so continue normal processing
			 */
		}
		
		pxe_core_recv_packets();
		segment = NULL;
		connection->segment = NULL;
	}

	return (sent_data);
}

/* pxe_tcp_read() - wrapper to read data from TCP socket
 * in:
 *      sock		- TCP socket to read from
 *	data		- buffer to read data
 *      size_to_read    - buffer size
 * out:
 *      -1	- failed
 *      >=0     - actual bytes read
 */
int
pxe_tcp_read(PXE_SOCKET *sock, void *data, uint16_t size_to_read)
{
	PXE_TCP_CONNECTION	*connection = filter_to_connection(sock->filter);

	if (connection == NULL) {
		printf("pxe_tcp_read(): no connection for filter 0x%x "
		       "(socket: 0x%x).\n", sock->filter, sock);
		return (-1);
	}

	PXE_BUFFER	*recv_buffer = connection->recv;
			
	if ( (connection->state != PXE_TCP_ESTABLISHED) &&
	     (recv_buffer->bufleft == recv_buffer->bufsize) )
	{
#ifdef PXE_DEBUG
		printf("pxe_tcp_read(): state %u, no data in buffer\n",
		    connection->state);
#endif
		return (-1);	/* connection closed and no data in buffer */
	}

	int result = pxe_buffer_read(recv_buffer, data, size_to_read);

	if (result != 0) { 

		/* if receive window was zero and now is big enough,
		 * notify remote host
		 */
		if ( (connection->winlock == 1) &&
		     (recv_buffer->bufleft > PXE_DEFAULT_RECV_BUFSIZE / 4))
		{
			if (!pxe_tcp_syssend(connection, PXE_TCP_ACK))
				printf("pxe_tcp_read(): failed to notify "
				       "remote host about window.\n");
			else
				connection->winlock = 0;
		}
	}
	
	/* process new packets if too low data in buffer */
	if (recv_buffer->bufleft > recv_buffer->bufsize / 2)
		pxe_core_recv_packets();
	
	return (result);
}

/* pxe_tcp_push() - flushes send buffer (actually current send segment)
 * in:
 *      filter		- filter of socket, which buffers need to flush
 * out:
 *      0	- failed
 *      1	- success
 */
int
pxe_tcp_push(PXE_FILTER_ENTRY *filter)
{
	PXE_TCP_CONNECTION	*connection = filter_to_connection(filter);

	if (connection == NULL) {
		printf("pxe_tcp_push(): no connection for filter 0x%x.\n",
		    filter);
		    
		return (0);
	}

	if ( (connection->state != PXE_TCP_ESTABLISHED) &&
	     (connection->state != PXE_TCP_CLOSE_WAIT) )
	{
		printf("pxe_tcp_push(): connection 0x%x is in wrong state %d.\n",
		    connection, connection->state);
		/* connection not in established state, ignore available data */
		return (0);	
	}
	
	PXE_TCP_QUEUED_SEGMENT	*segment = connection->segment;
	
	if (segment == NULL)	/* nothing to push */
		return (1);

	/* finish segment */
	tcp_finish_segment(connection, segment, PXE_TCP_ACK | PXE_TCP_PSH);

	segment->resend_at = pxe_get_secs() + PXE_RESEND_TIME;
    	connection->next_send += segment->size - sizeof(PXE_TCP_PACKET);
	
	if (!pxe_tcp_send_segment(connection, segment)) {
		printf("pxe_tcp_push(): failed to send segment.\n");
		/* this segment will be resent later,
		 * so continue normal processing
		 */
	}

	segment = NULL;
	connection->segment = NULL;

	return (1);
}

/* pxe_tcp_check_connection() - checks connections state by sending ACK,
 *				used e.g. to notify remote host about
 *				enough window to recv
 * in:
 *      sock		- TCP socket to check connection for
 * out:
 *      0	- failed
 *      1	- success
 */
int
pxe_tcp_check_connection(PXE_SOCKET *sock)
{
#ifdef PXE_TCP_DEBUG
	printf("pxe_tcp_check_connection(): started.\n");
#endif
	PXE_TCP_CONNECTION	*connection = filter_to_connection(sock->filter);

	if (connection == NULL) {
		printf("pxe_tcp_check_connection(): no connection for filter "
		       "0x%x (socket: 0x%x).\n", sock->filter, sock);
		return (0);
	}

	if (connection->state != PXE_TCP_ESTABLISHED) {
		printf("pxe_tcp_check_connection(): connection 0x%x "
		       "is not in established state(%d).\n",
		       connection, connection->state);
		/* connection not in established state, ignore available data */
		return (0);	
	}
	
	PXE_BUFFER *buffer = connection->recv;
	
	/* send ACK ony if we place enough space */
	if (buffer->bufleft < buffer->bufsize / 3)
		return (0);
	
	if (!pxe_tcp_syssend(connection, PXE_TCP_ACK)) {
		printf("pxe_tcp_check_connection(): failed to send ACK.\n");
		return (0);
	}
	
	return (1);
}

#ifdef PXE_MORE
/* pxe_connection_stats() - shows brief information about connections
 * in/out:
 * 	none
 */
void
pxe_connection_stats()
{
	printf("pxe_connection_stats(): %d connections\n", all_connections);

	int con_index = 0;
	PXE_TCP_CONNECTION *connection = NULL;
	
	for ( ; con_index < PXE_MAX_TCP_CONNECTIONS; ++con_index) {

		connection = &tcp_connections[con_index];
		
		printf("%d: filter: 0x%x, state: %d\n"
		       "  nxt_snd: %lu, nxt_rcv: %lu, iss: %lu, irs: %lu\n",
		       con_index, connection->filter, connection->state,
		       connection->next_send, connection->next_recv,
		       connection->iss, connection->irs);
	}
}
#endif /* PXE_MORE */
