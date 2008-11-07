#ifndef _NET_FLOWTABLE_H_
#define	_NET_FLOWTABLE_H_

#ifdef _KERNEL
#include <net/ethernet.h>
#include <netinet/in.h>

#define FL_HASH_PORTS 	(1<<0)	/* hash 4-tuple + protocol */
#define FL_PCPU		(1<<1)	/* pcpu cache */
#define FL_IPV6		(1<<2)	/* IPv6 table */

struct flowtable;
struct flowtable *flowtable_alloc(int nentry, int flags);

struct rtentry_info {
	struct ifnet		*ri_ifp;
	struct ifaddr 		*ri_ifa;
	int			ri_flags;
	int			ri_mtu;
	u_char			ri_desten[ETHER_ADDR_LEN];
	struct sockaddr		ri_dst; /* rt_gateway if RTF_GATEWAY */
};

struct rtentry_info6 {
	struct ifnet		*ri_ifp;
	struct ifaddr 		*ri_ifa;
	int			ri_flags;
	int			ri_mtu;
	u_char			ri_desten[ETHER_ADDR_LEN];
	struct sockaddr_in6	ri_dst; /* rt_gateway if RTF_GATEWAY */
};

/*
 * Given a flow table, look up the L3 and L2 information and
 * return it in ri
 *
 */
int flowtable_lookup(struct flowtable *ft, struct mbuf *m,
    struct rtentry_info *ri);
/*
 * Convert a route and an (optional) L2 address to an 
 * rtentry_info
 *
 */
void route_to_rtentry_info(struct route *ro, u_char *desten,
    struct rtentry_info *ri);

#endif
#endif
