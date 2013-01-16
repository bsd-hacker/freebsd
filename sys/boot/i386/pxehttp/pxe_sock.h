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
			 
#ifndef PXE_SOCK_H_INCLUDED
#define PXE_SOCK_H_INCLUDED

/*
 * Implements simple sockets API
 */
  
#include <stdint.h>
#include <stddef.h>

#include "pxe_buffer.h"
#include "pxe_filter.h"
#include "pxe_ip.h"

/* default count of sockets used at the same time */
#define PXE_DEFAULT_SOCKETS             4
/* default count of waiting queue */
#define PXE_DEFAULT_WAITCOUNT           3
/* socket timeout when receiving data, in milliseconds */
#define PXE_SOCKET_TIMEOUT		30000
/* timeout, after that force connection checking, in milliseconds */
#define PXE_SOCKET_CHECK_TIMEOUT	100
/* define to add extra socket validating at every function */
 #define PXE_SOCKET_ACCURATE
/* socket states */
/* socket unused and free for allocating  */
#define PXE_SOCKET_FREE                 0x0
/* socket structure used */
#define PXE_SOCKET_USED                 0x1
/* socket binded (set local ip/local port). TODO: check if need */
#define PXE_SOCKET_BINDED		0x2
/* socket connected (set remote ip/remote port). TODO: check if need  */
#define PXE_SOCKET_CONNECTED		0x3
/* connection established. TODO: check if need */
#define PXE_SOCKET_ESTABLISHED		0x4

#define PXE_SOCK_NONBLOCKING		0
#define PXE_SOCK_BLOCKING		1

/* socket */
typedef struct pxe_socket {
	PXE_BUFFER  send_buffer;	/* sending buffer */
	PXE_BUFFER  recv_buffer;	/* receiving buffer */

	/* transmit and status counters*/
	uint32_t    sent;		/* bytes sent to socket */
	uint32_t    recv;		/* bytes received from socket */
	uint8_t     state;		/* current state */
	uint8_t     waiting;		/* number of connections waiting
					 * to be accepted */
	/* for resending usage */
	uint32_t    last_time_sent;
	uint32_t    last_time_recv;
	PXE_FILTER_ENTRY    *filter;	/* filter, that feeds data to socket */
} PXE_SOCKET;

/* inits this module */
void	pxe_sock_init();

/* allocates pxe_socket structure */
/* int	pxe_socket_alloc(); */

/* frees socket structure */
/* int	pxe_socket_free(int socket); (/

/* shows socket usage statistics */
void	pxe_sock_stats();

/* returns current socket state */
int	pxe_sock_state(int socket);

PXE_BUFFER * pxe_sock_recv_buffer(int socket);

/* pxe_listen() - creates "listening" socket 
 *    it's not the same as normal listen() system call.
 * Every pxe_listen() call creates pxe_socket structure
 * and adds filter to filter table.
 * WARN:
 *	-1 - means failed
 *	>= 0 - socket for UDP
 *	== 0 - success for TCP
 */
int	pxe_listen(int socket, uint8_t proto, uint16_t port);

/* accept awaiting connections */
int	pxe_accept(int socket);

/* send to provided ip/port, updating filter for socket */
int	pxe_sendto(int socket, const PXE_IPADDR *dst, uint16_t port,
	    void *data, uint16_t size);

/* moves socket to connected state */
int	pxe_connect(int socket, const PXE_IPADDR *ip, uint16_t port,
	    uint8_t proto);

/* send data to socket, blocking */
int	pxe_send(int socket, void *buf, uint16_t buflen);

/* receive data from socket, blocking  */
int	pxe_recv(int socket, void *buf, uint16_t buflen, int flags);

/* create new socket */
int	pxe_socket();

/* binding */
int	pxe_bind(int socket, const PXE_IPADDR *ip, uint16_t port,
	    uint8_t proto);

/* flushes send buffers */
int	pxe_flush(int socket);

/* close socket */
int	pxe_close(int socket);

/* returns next available local port */
uint16_t pxe_next_port();

#endif // PXE_SOCK_H_INCLUDED
