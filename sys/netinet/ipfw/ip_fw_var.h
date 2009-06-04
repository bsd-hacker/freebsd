/*-
 * Copyright (c) 2002-2009 Luigi Rizzo, Universita` di Pisa
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
 * $FreeBSD: user/luigi/ipfw_80/sys/netinet/ip_fw.h 191738 2009-05-02 08:16:26Z zec $
 */

#ifndef _IPFW2_VAR_H
#define _IPFW2_VAR_H

/*
 * Kernel side of ipfw data structures.
 */
/*
 * The default rule number.  By the design of ip_fw, the default rule
 * is the last one, so its number can also serve as the highest number
 * allowed for a rule.  The ip_fw code relies on both meanings of this
 * constant. 
 */
#define	IPFW_DEFAULT_RULE	65535

/*
 * The number of ipfw tables.  The maximum allowed table number is the
 * (IPFW_TABLES_MAX - 1).
 */
#define	IPFW_TABLES_MAX		128


#define MTAG_IPFW	1148380143	/* IPFW-tagged cookie */

/* Apply ipv6 mask on ipv6 addr */
#define APPLY_MASK(addr,mask)                          \
    (addr)->__u6_addr.__u6_addr32[0] &= (mask)->__u6_addr.__u6_addr32[0]; \
    (addr)->__u6_addr.__u6_addr32[1] &= (mask)->__u6_addr.__u6_addr32[1]; \
    (addr)->__u6_addr.__u6_addr32[2] &= (mask)->__u6_addr.__u6_addr32[2]; \
    (addr)->__u6_addr.__u6_addr32[3] &= (mask)->__u6_addr.__u6_addr32[3];


/*
 * Main firewall chains definitions and global var's definitions.
 */

/* Return values from ipfw_chk() */
enum {
	IP_FW_PASS = 0,
	IP_FW_DENY,
	IP_FW_DIVERT,
	IP_FW_TEE,
	IP_FW_DUMMYNET,
	IP_FW_NETGRAPH,
	IP_FW_NGTEE,
	IP_FW_NAT,
	IP_FW_REASS,
};

/* flags for divert mtag */
#define	IP_FW_DIVERT_LOOPBACK_FLAG	0x00080000
#define	IP_FW_DIVERT_OUTPUT_FLAG	0x00100000

/*
 * Structure for collecting parameters to dummynet for ip6_output forwarding
 */
struct _ip6dn_args {
       struct ip6_pktopts *opt_or;
       struct route_in6 ro_or;
       int flags_or;
       struct ip6_moptions *im6o_or;
       struct ifnet *origifp_or;
       struct ifnet *ifp_or;
       struct sockaddr_in6 dst_or;
       u_long mtu_or;
       struct route_in6 ro_pmtu_or;
};

/*
 * Arguments for calling ipfw_chk() and dummynet_io(). We put them
 * all into a structure because this way it is easier and more
 * efficient to pass variables around and extend the interface.
 */
struct ip_fw_args {
	struct mbuf	*m;		/* the mbuf chain		*/
	struct ifnet	*oif;		/* output interface		*/
	struct sockaddr_in *next_hop;	/* forward address		*/
	struct ip_fw	*rule;		/* matching rule		*/
	struct ether_header *eh;	/* for bridged packets		*/

	struct ipfw_flow_id f_id;	/* grabbed from IP header	*/
	u_int32_t	cookie;		/* a cookie depending on rule action */
	struct inpcb	*inp;

	struct _ip6dn_args	dummypar; /* dummynet->ip6_output */
	struct sockaddr_in hopstore;	/* store here if cannot use a pointer */
};

/*
 * Function definitions.
 */

/* Firewall hooks */
struct sockopt;
struct dn_flow_set;

int ipfw_check_in(void *, struct mbuf **, struct ifnet *, int, struct inpcb *inp);
int ipfw_check_out(void *, struct mbuf **, struct ifnet *, int, struct inpcb *inp);

int ipfw_chk(struct ip_fw_args *);

int ipfw_init(void);
void ipfw_destroy(void);
#ifdef NOTYET
void ipfw_nat_destroy(void);
#endif

#ifdef VIMAGE_GLOBALS
extern int fw_one_pass;
extern int fw_enable;
#ifdef INET6
extern int fw6_enable;
#endif
#endif /* VIMAGE_GLOBALS */

struct ip_fw_chain {
	struct ip_fw	*rules;		/* list of rules */
	struct ip_fw	*reap;		/* list of rules to reap */
	LIST_HEAD(, cfg_nat) nat;       /* list of nat entries */
	struct radix_node_head *tables[IPFW_TABLES_MAX];
	struct rwlock	rwmtx;
};

#ifdef IPFW_INTERNAL

#define	IPFW_LOCK_INIT(_chain) \
	rw_init(&(_chain)->rwmtx, "IPFW static rules")
#define	IPFW_LOCK_DESTROY(_chain)	rw_destroy(&(_chain)->rwmtx)
#define	IPFW_WLOCK_ASSERT(_chain)	rw_assert(&(_chain)->rwmtx, RA_WLOCKED)

#define IPFW_RLOCK(p) rw_rlock(&(p)->rwmtx)
#define IPFW_RUNLOCK(p) rw_runlock(&(p)->rwmtx)
#define IPFW_WLOCK(p) rw_wlock(&(p)->rwmtx)
#define IPFW_WUNLOCK(p) rw_wunlock(&(p)->rwmtx)

#define LOOKUP_NAT(l, i, p) do {					\
		LIST_FOREACH((p), &(l.nat), _next) {			\
			if ((p)->id == (i)) {				\
				break;					\
			} 						\
		}							\
	} while (0)

typedef int ipfw_nat_t(struct ip_fw_args *, struct cfg_nat *, struct mbuf *);
typedef int ipfw_nat_cfg_t(struct sockopt *);
#endif

struct eventhandler_entry;
/*
 * Stack virtualization support.
 */
struct vnet_ipfw {
	int			_fw_enable;
	int			_fw6_enable;
	u_int32_t		_set_disable;
	int			_fw_deny_unknown_exthdrs;
	int			_fw_verbose;
	int			_verbose_limit;
	int			_autoinc_step;
	ipfw_dyn_rule **	_ipfw_dyn_v;
	uma_zone_t 		_ipfw_dyn_rule_zone;
	struct ip_fw_chain	_layer3_chain;
	u_int32_t		_dyn_buckets;
	u_int32_t		_curr_dyn_buckets;
	u_int32_t		_dyn_ack_lifetime;
	u_int32_t		_dyn_syn_lifetime;
	u_int32_t		_dyn_fin_lifetime;
	u_int32_t		_dyn_rst_lifetime;
	u_int32_t		_dyn_udp_lifetime;
	u_int32_t		_dyn_short_lifetime;
	u_int32_t		_dyn_keepalive_interval;
	u_int32_t		_dyn_keepalive_period;
	u_int32_t		_dyn_keepalive;
	u_int32_t		_static_count;
	u_int32_t		_static_len;
	u_int32_t		_dyn_count;
	u_int32_t		_dyn_max;
	u_int64_t		_norule_counter;
	struct callout		_ipfw_timeout;
	struct eventhandler_entry *_ifaddr_event_tag;
};

#ifndef VIMAGE
#ifndef VIMAGE_GLOBALS
extern struct vnet_ipfw vnet_ipfw_0;
#endif
#endif

/*
 * Symbol translation macros
 */
#define	INIT_VNET_IPFW(vnet) \
	INIT_FROM_VNET(vnet, VNET_MOD_IPFW, struct vnet_ipfw, vnet_ipfw)
 
#define	VNET_IPFW(sym)		VSYM(vnet_ipfw, sym)
 
#define	V_fw_enable		VNET_IPFW(fw_enable)
#define	V_fw6_enable		VNET_IPFW(fw6_enable)
#define	V_set_disable		VNET_IPFW(set_disable)
#define	V_fw_deny_unknown_exthdrs VNET_IPFW(fw_deny_unknown_exthdrs)
#define	V_fw_verbose		VNET_IPFW(fw_verbose)
#define	V_verbose_limit		VNET_IPFW(verbose_limit)
#define	V_autoinc_step		VNET_IPFW(autoinc_step)
#define	V_ipfw_dyn_v		VNET_IPFW(ipfw_dyn_v)
#define	V_ipfw_dyn_rule_zone	VNET_IPFW(ipfw_dyn_rule_zone)
#define	V_layer3_chain		VNET_IPFW(layer3_chain)
#define	V_dyn_buckets		VNET_IPFW(dyn_buckets)
#define	V_curr_dyn_buckets	VNET_IPFW(curr_dyn_buckets)
#define	V_dyn_ack_lifetime	VNET_IPFW(dyn_ack_lifetime)
#define	V_dyn_syn_lifetime	VNET_IPFW(dyn_syn_lifetime)
#define	V_dyn_fin_lifetime	VNET_IPFW(dyn_fin_lifetime)
#define	V_dyn_rst_lifetime	VNET_IPFW(dyn_rst_lifetime)
#define	V_dyn_udp_lifetime	VNET_IPFW(dyn_udp_lifetime)
#define	V_dyn_short_lifetime	VNET_IPFW(dyn_short_lifetime)
#define	V_dyn_keepalive_interval VNET_IPFW(dyn_keepalive_interval)
#define	V_dyn_keepalive_period	VNET_IPFW(dyn_keepalive_period)
#define	V_dyn_keepalive		VNET_IPFW(dyn_keepalive)
#define	V_static_count		VNET_IPFW(static_count)
#define	V_static_len		VNET_IPFW(static_len)
#define	V_dyn_count		VNET_IPFW(dyn_count)
#define	V_dyn_max		VNET_IPFW(dyn_max)
#define	V_norule_counter	VNET_IPFW(norule_counter)
#define	V_ipfw_timeout		VNET_IPFW(ipfw_timeout)
#define	V_ifaddr_event_tag	VNET_IPFW(ifaddr_event_tag)

#endif /* _IPFW2_VAR_H */
