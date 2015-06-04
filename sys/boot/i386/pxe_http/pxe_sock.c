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
#include "pxe_core.h"
#include "pxe_filter.h"
#include "pxe_mem.h"
#include "pxe_sock.h"
#include "pxe_tcp.h"
#include "pxe_udp.h"

/* storage for socket describing structures */
static PXE_SOCKET	pxe_sockets[PXE_DEFAULT_SOCKETS];
/* next available local port to use */
static uint16_t		avail_port = 1025;

/* pxe_socket_init() - initialization of socket module
 * in/out:
 *	none
 */
void
pxe_socket_init()
{

	pxe_memset(pxe_sockets, 0, sizeof(pxe_sockets));
}

/* pxe_socket_alloc() - allocates new socket describing structure
 * in:
 *	none
 * out:
 *	-1		- failed
 *	nonnegative	- success
 */
int
pxe_socket_alloc()
{
	int sock_index = 0;

	/* searching free sockets */
	for (; sock_index < PXE_DEFAULT_SOCKETS; ++sock_index) {

		if (pxe_sockets[sock_index].state == PXE_SOCKET_FREE) {
			/* found free socket */
			pxe_memset(&pxe_sockets[sock_index], 0,
			    sizeof(PXE_SOCKET));
			    
			pxe_sockets[sock_index].state = PXE_SOCKET_USED;
			
			return sock_index;
		}
	}

	return (-1); /* no socket found :( */
}

/* pxe_socket_free() - releases socket describing structure
 * in:
 *	socket		- socket descriptor
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_socket_free(int socket)
{
	PXE_SOCKET *sock = &pxe_sockets[socket];

	sock->state = PXE_SOCKET_FREE;
	pxe_buffer_memfree(&sock->recv_buffer);
	pxe_buffer_memfree(&sock->send_buffer);
	
	return (1);
}

/* pxe_socket() - creates new socket
 * in:
 *	none
 * out:
 *	-1		- failed
 *	nonnegative	- success
 */
int
pxe_socket()
{
	int socket = pxe_socket_alloc();

	/* allocating structure */
	if (socket == -1)
		return (-1);

#ifdef PXE_DEBUG
	printf("pxe_socket(): initing socket %d.\n", socket);
#endif
	/* creating buffers */
	PXE_BUFFER	*rbuf = &pxe_sockets[socket].recv_buffer;
	PXE_BUFFER	*sbuf = &pxe_sockets[socket].send_buffer;
	
	if (!pxe_buffer_memalloc(sbuf, PXE_DEFAULT_SEND_BUFSIZE)) {
	
		pxe_socket_free(socket);
		return (-1);
	}
	
	if (!pxe_buffer_memalloc(rbuf, PXE_DEFAULT_RECV_BUFSIZE)) {
	
		pxe_socket_free(socket);
		return (-1);
	}
#ifdef PXE_DEBUG
	printf("pxe_socket(): socket %d created.\n", socket);
#endif	
	return (socket);		
}

/* pxe_close() - closes socket
 * in:
 *	socket		- socket descriptor number
 * out:
 *	0		- failed
 *	1		- success
 */
int
pxe_close(int socket)
{
#ifdef PXE_DEBUG
	printf("pxe_close(): closing socket %d\n", socket);
#endif	
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_close(): invalid socket %d\n", socket);
		return (0);
	}
#endif	
	PXE_SOCKET	*sock = &pxe_sockets[socket];

	if (sock->state == PXE_SOCKET_FREE) {
#ifdef PXE_DEBUG
		printf("pxe_close(): socket %d already closed.\n", socket);
#endif 	
		return (0);
	}

	PXE_FILTER_ENTRY *filter = sock->filter;
	/* flush data in buffers */
	pxe_flush(socket);

	if (filter == NULL) { /* sanity check */
#ifdef PXE_DEBUG
		printf("pxe_close(): filter is NULL.\n");
#endif
		return (0);
	}
	
	/* UDP socket closing is simple */	
	if (filter->protocol == PXE_UDP_PROTOCOL) {

		if (filter != NULL)	
			pxe_filter_remove(sock->filter);
#ifdef PXE_DEBUG
		else
			printf("pxe_close(): filter for socket already NULL.\n");
#endif
	} else	 /* filter removing is done in check_time_to_die() */
		 pxe_tcp_disconnect(sock);

#ifdef PXE_DEBUG
	printf("pxe_close(): closed\n");
#endif
	return pxe_socket_free(socket);
}

#ifdef PXE_MORE
/* pxe_listen() - setups filter for socket and waits new connections
 * in:
 *	socket		- socket descriptor number
 *	proto		- IP stack protocol number
 *	port		- local port to listen connections on
 * out:
 *	-1		- if failed
 *	nonnegative	- number of waiting connections
 */
int
pxe_listen(int socket, uint8_t proto, uint16_t port) 
{
#ifdef PXE_DEBUG
	printf("pxe_listen(): proto 0x%x, port: %d.\n", proto, port);
#endif
	const PXE_IPADDR* to = pxe_get_ip(PXE_IP_MY);
	
	PXE_FILTER_ENTRY *filter = pxe_filter_add(0, 0, to,
					port, &pxe_sockets[socket], proto);
	
	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_listen(): failed to add filter.\n");
#endif
		return (-1);
	}
	
	pxe_filter_mask(filter, 0, 0, 0xffffffff, 0xffff);
	
	pxe_sockets[socket].filter = filter;
	
	if (proto == PXE_UDP_PROTOCOL) {	/* for UDP it's fake listen */
		return (socket);
	}
	
	while (pxe_sockets[socket].waiting == 0) {
#ifdef PXE_DEBUG
		twiddle(1);
#endif		
		if (!pxe_core_recv_packets()) {
			delay(100000);
		}
	}
	
	return (pxe_sockets[socket].waiting);
}

/* pxe_accept() - accepts connections for listening socket and
 *		returns accepted socket
 * in:
 *	socket	- listening socket
 * out:
 *	-1		- failed
 *	nonnegative 	- newly created socket descriptor number
 */
int
pxe_accept(int socket)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		return (-1);
	}
#endif	
	PXE_SOCKET *sock = &pxe_sockets[socket];
	
	if (sock->waiting == 0)
		return (-1);
	
	int	back = sock->waiting;
	
	PXE_FILTER_ENTRY	*entry = sock->filter;
	
	for ( ; back != 0; --back) {

		/* filter childs are earlier */
		entry = entry->prev;

		if (entry == NULL) {
#ifdef PXE_DEBUG
			printf("pxe_accept(): corrupted waiting count.\n");
#endif		
			return (-1);
		}
	}

	int accepted_socket = pxe_socket();
	
	if (accepted_socket == -1)
		return (-1);
		
	/* decreasing waiting queue */
	--sock->waiting;
	
	sock = &pxe_sockets[accepted_socket];
	
	sock->filter = entry;
	entry->socket = sock;

	return (accepted_socket);
}

/* pxe_sock_stats() - show active sockets information
 * in/out:
 *	none
 */
void
pxe_sock_stats()
{
	int		socket = 0;
	PXE_SOCKET*	sock = pxe_sockets;
	
	for ( ; socket < PXE_DEFAULT_SOCKETS; ++socket, ++sock) {
	
		if (sock->state == PXE_SOCKET_FREE)
			continue;
			
		printf("%d: filter 0x%x, recv/sent: %d/%d, waiting: %d.\n", 
		    socket, sock->filter, sock->recv, sock->sent, sock->waiting);
	}
}
#endif /* PXE_MORE */

/* pxe_next_port() - returns local port
 * in:
 *	none
 * out:
 *	0  - no free port
 *	>0 - port number
 */
uint16_t
pxe_next_port()
{	/* dummy, TODO: check filters, if port used */
    
	if (avail_port == 40000)
		avail_port = 1025;

	return (avail_port++);
}

/* NOTE: now assuming that only UDP is implemented */
/* pxe_sendto() - sends data to chosen ip:port, connecting socket if needed
 * in:
 *	socket	- socket descriptor number
 *	dst	- IP address to send to
 *	port	- remote port
 *	data	- data buffer to send
 *	size	- size of data buffer
 * out:
 *	-1		- failed
 *	nonnegative	- actual bytes sended
 */
int
pxe_sendto(int socket, const PXE_IPADDR *dst, uint16_t port, void *data,
	   uint16_t size)
{

	if (size + sizeof(PXE_UDP_PACKET) > PXE_DEFAULT_SEND_BUFSIZE) {
		printf("pxe_sendto(): send buffer too small for %u bytes.\n",
		    size);
		    
		return (-1);
	}

	PXE_SOCKET	*sock = &pxe_sockets[socket];	

	if ( sock->state == PXE_SOCKET_BINDED) {
		/* if socket binded filter must not be NULL,
		 * cause pxe_bind() installs filter
		 */
		if (sock->filter == NULL) {
			printf("pxe_sendto(): NULL filter for binded socket %d.\n",
			    socket);
			    
			return (-1);
		}
		
	} else  { /* not binded, connect */
	
		/* NOTE: if it's already connected, return error */
		if (pxe_connect(socket, dst, port, PXE_UDP_PROTOCOL) == -1) {
			printf("pxe_sendto(): failed to connect.\n");
			return (-1);
		}
	}
	
	PXE_FILTER_ENTRY *filter = sock->filter;
		
	/* for UDP socket, send buffer used only for one dgram */
	PXE_UDP_PACKET	*udp_pack = (PXE_UDP_PACKET *)sock->send_buffer.data;
	
	/* copy user data */
	pxe_memcpy(data, udp_pack + 1, size);
	
	/* filters are useful for  incoming packet, so dst_port
	 * is local port. It's always set on this step (both for binded and
	 * connected sockets). for binded sockets pxe_connect() skipped, so
	 * need manually call pxe_next_port() to get local port (don't use
	 * binded local port, it seems correct behaviour)
	 */
	uint16_t lport = filter->dst_port;

#ifdef PXE_DEBUG
	printf("pxe_sendto(): %8x:%u -> %8x:%u, size = %u bytes.\n",
	    (pxe_get_ip(PXE_IP_MY))->ip, lport, dst->ip, port, size);
#endif	

	if (!pxe_udp_send(udp_pack, dst, port, lport,
		size + sizeof(PXE_UDP_PACKET)))
	{
		printf("pxe_sendto(): failed to send data.\n");
		return (-1);
	}

	return (size);
}

/* pxe_connect() - connect to remote ip:port
 * in:
 *	socket	- socket descriptor number
 *	dst	- IP address to connect to
 *	port	- remote port to connect to
 *	proto	- IP stack protocol
 * out:
 *	-1		- failed
 *	0		- success
 */	
int
pxe_connect(int socket, const PXE_IPADDR *dst, uint16_t port, uint8_t proto)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		return (-1);
	}
#endif
#ifdef PXE_DEBUG
	printf("pxe_connect(): started\n");
#endif
	PXE_SOCKET	*sock = &pxe_sockets[socket];
	
	if (sock->state >= PXE_SOCKET_CONNECTED) {
		printf("pxe_connect(): socket %d already connected.\n", socket);
		return (-1);
	}
	
	/* socket was already initalized */
	if (sock->filter != NULL) {
	
		pxe_filter_remove(sock->filter);
		sock->filter = NULL;		/* just to be sure... */
	}
	
	uint16_t lport = pxe_next_port();	/* getting free local port */
	
	if (port == 0) {
		printf("pxe_connect(): failed to allocate local port.\n");
		return (-1);
	}
	
	PXE_FILTER_ENTRY *entry = pxe_filter_add(dst, port,
					pxe_get_ip(PXE_IP_MY),
					lport, sock, proto);
	    
	if (entry == NULL) {
	
		/* try to get free filter from inactive connections */
		if (proto == PXE_TCP_PROTOCOL) {
#ifdef PXE_DEBUG
			printf("pxe_connect(): forcing filter release\n");
#endif
			pxe_force_filter_release();
			entry = pxe_filter_add(dst, port, pxe_get_ip(PXE_IP_MY),
				    lport, sock, proto);
		}
		
		if (entry == NULL)  {
			printf("pxe_connect(): failed to add filter.\n");
			return (-1);
		}
	}
	
	sock->filter = entry;
	sock->state = PXE_SOCKET_CONNECTED;
	
	if (proto == PXE_TCP_PROTOCOL) {
		/* trying handshaking */
		if (pxe_tcp_connect(sock)) {
			sock->state = PXE_SOCKET_ESTABLISHED;
		} else { /* failed, cleanup */
			pxe_filter_remove(entry);
			return (-1);
		}
	}

#ifdef PXE_DEBUG
	printf("pxe_connect(): socket %d connected, 0x%x:%u -> 0x%x:%u\n",
	    socket, (pxe_get_ip(PXE_IP_MY))->ip, lport, dst->ip, port);
#endif	
	/* all is ok */
	return (0);
}

/* pxe_send() - send data to socket
 * in:
 *	socket	- socket descriptor number to send data to
 *	buf	- buffer with data to send
 *	buflen	- size of buffer
 * out:
 *	-1		- failure
 *	nonnegative	- actual count of bytes sent
 */
int
pxe_send(int socket, void *buf, uint16_t buflen)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_send(): invalid socket %d.\n", socket);
		return (-1);
	}
#endif	
	PXE_SOCKET		*sock = &pxe_sockets[socket];
	PXE_FILTER_ENTRY	*filter = sock->filter;
	int			result = -1;
	
	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_send(): NULL filter\n");
#endif
		return (-1);
	}
	
	if (filter->protocol == PXE_UDP_PROTOCOL)
		result = pxe_udp_write(sock, buf, buflen);

	else if (filter->protocol == PXE_TCP_PROTOCOL)
		result = pxe_tcp_write(sock, buf, buflen);

	else
		printf("pxe_send(): only TCP and UDP sockets are implemented\n");

	if (result > 0)
		sock->sent += result;

	return (result);
}

/* pxe_recv() - receive data to socket
 * in:
 *	socket	- socket descriptor number to recieve data from
 *	tobuf	- buffer to receive data to
 *	buflen	- size of buffer
 * out:
 *	-1		- failure
 *	nonnegative	- actual count of bytes received
 */
int
pxe_recv(int socket, void *tobuf, uint16_t buflen)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_recv(): invalid socket %d.\n", socket);
		return (-1);
	}
#endif	
	PXE_SOCKET	*sock = &pxe_sockets[socket];
	PXE_FILTER_ENTRY *filter = sock->filter;

	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_recv(): NULL filter\n");
#endif
		return (-1);
	}
	
	if ( (filter->protocol != PXE_UDP_PROTOCOL) &&
	     (filter->protocol != PXE_TCP_PROTOCOL) )
	{
		printf("pxe_recv(): only TCP and UDP sockets are implemented\n");
		return (-1);
	}
	
	uint32_t timer = 0;
#ifdef PXE_TCP_AGRESSIVE
	uint32_t check_timer = 0;
#endif
	int	 result = 0;	
	
	while (1) {
		
		if (filter->protocol == PXE_UDP_PROTOCOL) {
	
		    result =  pxe_udp_read(sock, tobuf, buflen, NULL);
		
		} else {
	
		    result =  pxe_tcp_read(sock, tobuf, buflen);
		} 
		
		if (result != 0)
			break;
		
		if (timer > PXE_SOCKET_TIMEOUT)
			break;
#ifdef PXE_TCP_AGRESSIVE		
		if (filter->protocol == PXE_TCP_PROTOCOL) {

			if (check_timer > PXE_SOCKET_CHECK_TIMEOUT) {
				check_timer = 0;
				pxe_tcp_check_connection(sock);
			}
		}
		
		check_timer += TIME_DELTA;
#endif
		timer += TIME_DELTA;
		/* idle 10 ms */
		delay(TIME_DELTA_MS);
	}
	
	if (result > 0)
		sock->recv += result;
		
	return (result);
}

#ifdef PXE_MORE
/* pxe_recvfrom() - receive data to socket with information about sender
 * in:
 *	socket	- socket descriptor number to recieve data from
 *	tobuf	- buffer to receive data to
 *	buflen	- size of buffer
 *	src_ip	- pointer to memory, where store IP of sender
 * out:
 *	-1		- failure
 *	nonnegative	- actual count of bytes received
 */
int
pxe_recvfrom(int socket, void *tobuf, size_t buflen, uint32_t *src_ip)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_recvfrom(): invalid socket %d.\n", socket);
		return (-1);
	}
#endif	
	PXE_SOCKET	*sock = &pxe_sockets[socket];
	PXE_FILTER_ENTRY *filter = sock->filter;

	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_recvfrom(): NULL filter\n");
#endif
		return (-1);
	}
	
	if ( (filter->protocol != PXE_UDP_PROTOCOL) &&
	     (filter->protocol != PXE_TCP_PROTOCOL) )
	{
		printf("pxe_recvfrom(): only TCP and UDP sockets are implemented\n");
		return (-1);
	}
	
	uint32_t timer = 0;
	uint32_t check_timer = 0;

	int	 result = 0;	

	PXE_UDP_DGRAM	dgram;
	
	while (1) {
		
		if (filter->protocol == PXE_UDP_PROTOCOL) {
	
		    result =  pxe_udp_read(sock, tobuf, buflen, &dgram);
		
		} else {
	
		    result =  pxe_tcp_read(sock, tobuf, buflen);
		} 
		
		if (result != 0)
			break;
		
		if (timer > PXE_SOCKET_TIMEOUT)
			break;
		
		if (filter->protocol == PXE_TCP_PROTOCOL) {

			if (check_timer > PXE_SOCKET_CHECK_TIMEOUT) {
				check_timer = 0;
				pxe_tcp_check_connection(sock);
			}
		}
		
		check_timer += 5;
		timer += 5;
		/* idle 5 ms */
		delay(5000);
	}
	
	if (result >= 0) {
		sock->recv += result;
		
		if (src_ip != NULL) {	/* return source of data, if not NULL */
			if (filter->protocol == PXE_TCP_PROTOCOL)
				*src_ip = filter->src.ip;
			else
				*src_ip = dgram.src.ip;
		}
	}
		
	return (result);
}
#endif /* PXE_MORE */

/* pxe_bind() - bind socket to local ip:port
 * in:
 *      socket  - socket descriptor number
 *      to      - local ip to bind to
 *      lport   - local port to bind to
 *      proto   - IP stack protocol number
 * out:
 *      -1      - failed
 *      0       - success
 */
int
pxe_bind(int socket, const PXE_IPADDR *to, uint16_t lport, uint8_t proto)
{
#ifdef PXE_SOCKET_ACCURATE
        if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
                printf("pxe_bind(): invalid socket %d.\n", socket);
                return (-1);
        }
#endif
        PXE_SOCKET      *sock = &pxe_sockets[socket];

        if (sock->state == PXE_SOCKET_CONNECTED) {
                printf("pxe_bind(): cannot bind connected socket %d.\n",
                    socket);

                return (-1);
	}
	/* socket was already initalized */
	if (sock->filter != NULL) {
	        pxe_filter_remove(sock->filter);
	        sock->filter = NULL;
	}
	
	PXE_FILTER_ENTRY *entry =
		            pxe_filter_add( NULL, 0, to, lport, sock, proto);

        if ( entry == NULL ) {
                printf("pxe_bind(): failed to add filter.\n");
                return (0);
        }

        /* allow any src_ip:port to our ip:lport */
        pxe_filter_mask(entry, 0, 0, 0xffffffff, 0xffff);
        sock->filter = entry;

        /* all is ok */

        sock->state = PXE_SOCKET_BINDED;
        return (0);
}
																																					
/* pxe_flush() - flushes send buffers
 * in:
 *	socket	- socket descriptor number
 * out:
 *	-1	- failed
 *	0	- success
 */
int
pxe_flush(int socket)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_flush(): invalid socket %d.\n", socket);
		return (-1);
	}
#endif	
	PXE_SOCKET	*sock = &pxe_sockets[socket];
	PXE_FILTER_ENTRY *filter = sock->filter;
	
	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_flush(): NULL filter\n");
#endif
		return (-1);	
	}
	
	if (filter->protocol == PXE_UDP_PROTOCOL) /* it's always flushed */
		return (0);
	else if (filter->protocol == PXE_TCP_PROTOCOL)
		return (pxe_tcp_push(sock->filter) == 0) ? (-1) : 0;
	
	printf("pxe_flush(): only TCP and UDP sockets are implemented.\n");

	return (-1);
}

/* pxe_sock_state() - returns current state of socket
 * in:
 *	socket	- socket descriptor number
 * out:
 *	-1	- if failed
 *	one of PXE_SOCKET_ state flags otherwise
 */
int
pxe_sock_state(int socket)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_sock_state(): invalid socket %d.\n", socket);
		return (-1);
	}
#endif	
	PXE_SOCKET	*sock = &pxe_sockets[socket];
	PXE_FILTER_ENTRY *filter = sock->filter;
	
	if (filter == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_sock_state(): NULL filter\n");
#endif
		return (PXE_SOCKET_USED);
	}
	
	if (filter->protocol == PXE_UDP_PROTOCOL) /* it's always 'established' */
		return (PXE_SOCKET_ESTABLISHED);
		
	else if (filter->protocol == PXE_TCP_PROTOCOL) {
		/* for TCP connections need to check state */
		PXE_TCP_CONNECTION *connection = filter_to_connection(filter);
		
		if (connection && (connection->state == PXE_TCP_ESTABLISHED) )
			return (PXE_SOCKET_ESTABLISHED);
	}
	
	return (PXE_SOCKET_CONNECTED);
}

/* pxe_sock_recv_buffer() - returns recv buffer of socket
 * in:
 *	socket	- socket descriptor number
 * out:
 *	NULL	 - if failed
 *	non NULL - pointer to buffer
 */
PXE_BUFFER *
pxe_sock_recv_buffer(int socket)
{
#ifdef PXE_SOCKET_ACCURATE
	if ( (socket >= PXE_DEFAULT_SOCKETS) || (socket == -1)) {
		printf("pxe_sock_recv_buffer(): invalid socket %d.\n", socket);
		return (NULL);
	}
#endif
	return (&pxe_sockets[socket].recv_buffer);
}

