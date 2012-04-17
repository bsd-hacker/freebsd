/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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
 * $FreeBSD$
 *
 */

#ifndef _NETINET_TOE_H_
#define _NETINET_TOE_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

struct tcpopt;
struct tcphdr;
struct in_conninfo;

struct toedev {
	TAILQ_ENTRY(toedev) link;	/* glue for toedev_list */
	void *tod_softc;		/* TOE driver private data */

	/* Active open. */
	int (*tod_connect)(struct toedev *, struct socket *, struct rtentry *,
	    struct sockaddr *);

	/* Passive open. */
	int (*tod_listen_start)(struct toedev *, struct tcpcb *);
	int (*tod_listen_stop)(struct toedev *, struct tcpcb *);

	/* Frame received by kernel for an offloaded connection */
	void (*tod_input)(struct toedev *, struct tcpcb *, struct mbuf *, int);

	/* Some data read */
	void (*tod_rcvd)(struct toedev *, struct tcpcb *);

	/* Output data, if any is waiting to be sent out. */
	int (*tod_output)(struct toedev *, struct tcpcb *);

	/* Immediate teardown, send RST to peer */
	int (*tod_send_rst)(struct toedev *, struct tcpcb *);

	/* Orderly disconnect, send FIN to the peer */
	int (*tod_send_fin)(struct toedev *, struct tcpcb *);

	/* Kernel is done with the TCP PCB */
	void (*tod_pcb_detach)(struct toedev *, struct tcpcb *);

	/* Information about an L2 entry is now available. */
	void (*tod_l2_update)(struct toedev *, struct ifnet *,
	    struct sockaddr *, uint8_t *, uint16_t);

	/* XXX.  Route has been redirected. */
	void (*tod_route_redirect)(struct toedev *, struct ifnet *,
	    struct rtentry *, struct rtentry *);

	/* Syncache interaction. */
	void (*tod_syncache_added)(struct toedev *, void *);
	void (*tod_syncache_removed)(struct toedev *, void *);
	int (*tod_syncache_respond)(struct toedev *, void *, struct mbuf *);

	/* TCP socket option */
	void (*tod_ctloutput)(struct toedev *, struct tcpcb *, int, int);
};

#define TOEDEV(ifp) ((ifp)->if_llsoftc)

#include <sys/eventhandler.h>
typedef	void (*tcp_offload_listen_start_fn)(void *, struct tcpcb *);
typedef	void (*tcp_offload_listen_stop_fn)(void *, struct tcpcb *);
EVENTHANDLER_DECLARE(tcp_offload_listen_start, tcp_offload_listen_start_fn);
EVENTHANDLER_DECLARE(tcp_offload_listen_stop, tcp_offload_listen_stop_fn);

void init_toedev(struct toedev *);
int  register_toedev(struct toedev *);
int  unregister_toedev(struct toedev *);

/*
 * General interface for looking up L2 information for an IP or IPv6 address.
 * If an answer is not available right away then the TOE driver's tod_l2_update
 * will be called later.
 */
int toe_l2_resolve(struct toedev *, struct ifnet *, struct sockaddr *,
    uint8_t *, uint16_t *);

void toe_syncache_add(struct in_conninfo *, struct tcpopt *, struct tcphdr *,
    struct inpcb *, void *, void *);
int  toe_syncache_expand(struct in_conninfo *, struct tcpopt *, struct tcphdr *,
    struct socket **);
#endif
