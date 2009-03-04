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

/*
 * Given a flow table, look up the L3 and L2 information and
 * return it in the route
 *
 */
int flowtable_lookup(struct flowtable *ft, struct mbuf *m,
    struct route *ro);

#endif
#endif
