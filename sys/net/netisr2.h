/*-
 * Copyright (c) 2007-2008 Robert N. M. Watson
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
 */

#ifndef _NET_NETISR2_H_
#define	_NET_NETISR2_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*-
 * Protocols may express flow affinities or CPU affinities using lookup
 * functions:
 *
 * netisr_lookup_cpu_t - Given a packet for the protocol handler, return a
 *                       CPU affinity that will be used, and an indication of
 *                       the affinity strength.  All packets with the same
 *                       CPU affinity and strength must be processed in order
 *                       with respect to a single source.
 *
 * netisr_lookup_flow_t - Given a packet for the protocol handler, return a
 *                        flow identifier that can be used to identify
 *                        ordering requirements with respect to other
 *                        packets, and likewise an affinity strength to use
 *                        with the flowid-derived affinity.   All packets
 *                        with the same flowid and affinity strength must be
 *                        processed in order with respect to a single source.
 *
 * Protocols that implement direct CPU assignment of work, rather than
 * returning a flowid, must not return invalid (i.e., absent or otherwise
 * inappropriate) CPU identifiers.  The dispatch routines will panic if that
 * occurs.
 *
 * XXXRW: If we eventually support dynamic reconfiguration, there should be
 * protocol handlers to notify them of CPU configuration changes so that they
 * can rebalance work.
 */
typedef struct mbuf	*netisr_lookup_flow_t(struct mbuf *m, u_int *flowid,
			    u_int *strength);
typedef struct mbuf	*netisr_lookup_cpu_t(struct mbuf *m, u_int *cpuid,
			    u_int *strength);

/*
 * Possibly values for the 'strength' returned by netisr_lookup_cpu_t.
 * Protocols should consistently return the same strength for packets in the
 * same flow, or misordering may occur due to varying dispatch decisions.
 */
#define	NETISR2_AFFINITY_STRONG	1	/* Never direct dispatch off-CPU. */
#define	NETISR2_AFFINITY_WEAK	2	/* Direct dispatch even if off-CPU. */

/*-
 * Register a new netisr2 handler for a given protocol.  No previous
 * registration may exist.  At most one of lookup_cpu and lookup_flow may be
 * defined.  If no lookup routine is defined, the protocol's work will be
 * assigned to a CPU arbitrarily.
 *
 * proto        - Integer protocol identifier.
 * func         - Protocol handler.
 * lookup_cpu   - CPU affinity lookup.
 * lookup_flow  - Flow lookup.
 * max          - Maximum queue depth.
 */
void	netisr2_register(u_int proto, netisr_t func,
	    netisr_lookup_cpu_t lookup_cpu, netisr_lookup_flow_t lookup_flow,
	    const char *name, u_int max);

/*
 * Deregister a protocol handler.
 */
void	netisr2_deregister(u_int proto);

/*
 * Packet processing routines -- each accepts a protocol identifier and a
 * packet.  Some also force the use of a particular CPU affinity or flow
 * identifier rather than having the dispatcher query the protocol.
 *
 * _dispatch variants will attempt do directly dispatch the handler if
 * globally enabled and permitted by the protocol.
 *
 * _queue variants will enqueue the packet for later processing by the
 * handler in a deferred context.
 *
 * Direct dispatch decisions are made based on a combination of global
 * properties (is direct dispatch enabled), caller properties (is direct
 * dispatch allowed in this context) and protocol properties (strong affinity
 * will prevent direct dispatch on the wrong CPU, even if it maintains source
 * ordering).  Callers should use only one of the direct and queued dispatch
 * modes for each source, or ordering constraints with respect to the source
 * cannot be maintained.
 */

/*
 * Process a packet destined for a protocol, looking up the CPU or flowid
 * using the protocol's lookup routines.
 */
int	netisr2_dispatch(u_int proto, struct mbuf *m);
int	netisr2_queue(u_int proto, struct mbuf *m);

/*
 * Process a packet destined for a protocol on a specific CPU, rather than
 * querying the protocol to determine work placement.  The passed CPU ID is
 * considered a strong CPU affinity in making dispatch decisions.
 */
int	netisr2_dispatch_cpu(u_int proto, struct mbuf *m, u_int cpuid);
int	netisr2_queue_cpu(u_int proto, struct mbuf *m, u_int cpuid);

/*
 * Process a packet destined for a protocol with a specific flowid, rather
 * than querying the protocol to determine work placement.  The CPU affinity
 * generated with the flowid is considered a strong affinity in making
 * dispatch decisions.
 */
int	netisr2_dispatch_flow(u_int proto, struct mbuf *m, u_int flowid);
int	netisr2_queue_flow(u_int proto, struct mbuf *m, u_int flowid);

/*
 * In order to allow the same mapping of flow IDs to CPU affinities across
 * layers, expose the netisr2 mapping function.
 *
 * XXXRW: Perhaps we should inline this and instead expose nws_count?
 */
u_int	netisr2_flowid2cpuid(u_int flowid);

#endif /* !_NET_NETISR2_H_ */
