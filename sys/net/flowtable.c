#include "opt_mpath.h"

#include <sys/param.h>  
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>  
#include <sys/bitstring.h>
#include <sys/vimage.h>
#include <sys/sysctl.h>


#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_llatbl.h>
#include <net/if_var.h>
#include <net/route.h> 
#include <net/vnet.h>
#include <net/flowtable.h>


#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/sctp.h>

#define calloc(count, size) malloc((count)*(size), M_DEVBUF, M_WAITOK|M_ZERO)
	
/*
 * Taken from http://burtleburtle.net/bob/c/lookup3.c
 */

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose 
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of uint32_t's, and
 -- that the length be the number of uint32_t's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in uint32_ts rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------
*/
static uint32_t hashword(
const uint32_t *k,                   /* the key, an array of uint32_t values */
size_t          length,               /* the length of the key, in uint32_ts */
uint32_t        initval)         /* the previous hash, or an arbitrary value */
{
  uint32_t a,b,c;

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + (((uint32_t)length)<<2) + initval;

  /*------------------------------------------------- handle most of the key */
  while (length > 3)
  {
    a += k[0];
    b += k[1];
    c += k[2];
    mix(a,b,c);
    length -= 3;
    k += 3;
  }

  /*------------------------------------------- handle the last 3 uint32_t's */
  switch(length)                     /* all the case statements fall through */
  { 
  case 3 : c+=k[2];
  case 2 : b+=k[1];
  case 1 : a+=k[0];
    final(a,b,c);
  case 0:     /* case 0: nothing left to add */
    break;
  }
  /*------------------------------------------------------ report the result */
  return c;
}


struct ipv4_tuple {
	uint16_t 	ip_sport;	/* source port */
	uint16_t 	ip_dport;	/* destination port */
	in_addr_t 	ip_saddr;	/* source address */
	in_addr_t 	ip_daddr;	/* destination address */
};

union ipv4_flow {
	struct ipv4_tuple ipf_ipt;
	uint32_t 	ipf_key[3];
};

struct ipv6_tuple {
	uint16_t 	ip_sport;	/* source port */
	uint16_t 	ip_dport;	/* destination port */
	struct in6_addr	ip_saddr;	/* source address */
	struct in6_addr	ip_daddr;	/* destination address */
};

union ipv6_flow {
	struct ipv6_tuple ipf_ipt;
	uint32_t 	ipf_key[9];
};

struct flentry {
	volatile uint32_t	f_fhash;	/* hash flowing forward */
	uint16_t		f_flags;	/* flow flags */
	uint8_t			f_pad;
	uint8_t			f_proto;	/* protocol */
	uint32_t		f_uptime;
	volatile struct rtentry *f_rt;		/* rtentry for flow */
	volatile struct llentry *f_lle;		/* llentry for flow */
};

struct flentry_v4 {
	struct flentry	fl_entry;
	union ipv4_flow	fl_flow;
};

struct flentry_v6 {
	struct flentry	fl_entry;
	union ipv6_flow	fl_flow;
};

#define	fl_fhash	fl_entry.fl_fhash
#define	fl_flags	fl_entry.fl_flags
#define	fl_proto	fl_entry.fl_proto
#define	fl_uptime	fl_entry.fl_uptime
#define	fl_rt		fl_entry.fl_rt
#define	fl_lle		fl_entry.fl_lle

#define	SECS_PER_HOUR		3600
#define	SECS_PER_DAY		(24*SECS_PER_HOUR)

#define	SYN_IDLE		300
#define	UDP_IDLE		300
#define	FIN_WAIT_IDLE		600
#define	TCP_IDLE		SECS_PER_DAY


typedef	void fl_lock_t(struct flowtable *, uint32_t);
typedef void fl_rtalloc_t(struct route *, uint32_t, u_int);

union flentryp {
	struct flentry_v4	*v4;
	struct flentry_v6	*v6;
	struct flentry_v4	*v4_pcpu[MAXCPU];
	struct flentry_v6	*v6_pcpu[MAXCPU];
};

struct flowtable {
	union flentryp	ft_table;
	int 		ft_size;
	bitstr_t 	*ft_masks[MAXCPU];
	struct mtx	*ft_locks;
	int 		ft_lock_count;
	uint32_t	ft_flags;
	uint32_t	ft_collisions;
	uint32_t	ft_allocated;
	uint64_t	ft_hits;

	uint32_t	ft_udp_idle;
	uint32_t	ft_fin_wait_idle;
	uint32_t	ft_syn_idle;
	uint32_t	ft_tcp_idle;

	fl_lock_t	*ft_lock;
	fl_lock_t 	*ft_unlock;
	fl_rtalloc_t	*ft_rtalloc;

};

static uint32_t hashjitter;

SYSCTL_NODE(_net_inet, OID_AUTO, flowtable, CTLFLAG_RD, NULL, "flowtable");

int	flowtable_enable = 1;
SYSCTL_INT(_net_inet_flowtable, OID_AUTO, enable, CTLFLAG_RW,
    &flowtable_enable, 0, "enable flowtable caching.");


#ifndef RADIX_MPATH
static void
in_rtalloc_ign_wrapper(struct route *ro, uint32_t hash, u_int fib)
{

	in_rtalloc_ign(ro, 0, fib);
}
#endif

static void
flowtable_global_lock(struct flowtable *table, uint32_t hash)
{	
	int lock_index = (hash)&(table->ft_lock_count - 1);

	mtx_lock(&table->ft_locks[lock_index]);
}

static void
flowtable_global_unlock(struct flowtable *table, uint32_t hash)
{	
	int lock_index = (hash)&(table->ft_lock_count - 1);

	mtx_unlock(&table->ft_locks[lock_index]);
}

static void
flowtable_pcpu_lock(struct flowtable *table, uint32_t hash)
{

	critical_enter();
}

static void
flowtable_pcpu_unlock(struct flowtable *table, uint32_t hash)
{

	mb();
	critical_exit();
}

#define FL_ENTRY_INDEX(table, hash)((hash) % (table)->ft_size)
#define FL_ENTRY(table, hash) flowtable_entry((table), (hash))
#define FL_ENTRY_LOCK(table, hash)  (table)->ft_lock((table), (hash))
#define FL_ENTRY_UNLOCK(table, hash) (table)->ft_unlock((table), (hash))

#define FL_STALE (1<<8)

static uint32_t
ipv4_flow_lookup_hash_internal(struct mbuf *m, struct route *ro,
    uint32_t *key, uint16_t *flags, uint8_t *protop)
{
	uint16_t sport = 0, dport = 0;
	struct ip *ip = mtod(m, struct ip *);
	uint8_t proto = ip->ip_p;
	int iphlen = ip->ip_hl << 2;
	uint32_t hash;
	struct sockaddr_in *sin;
	struct tcphdr *th;
	struct udphdr *uh;
	struct sctphdr *sh;

	key[0] = 0;
	key[1] = ip->ip_src.s_addr;
	key[2] = ip->ip_dst.s_addr;	

	sin = (struct sockaddr_in *)&ro->ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = ip->ip_dst;

	if (flowtable_enable == 0)
		return (0);
	
	switch (proto) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + iphlen);
		sport = ntohs(th->th_sport);
		dport = ntohs(th->th_dport);
		*flags |= th->th_flags;
		if (*flags & TH_RST)
			*flags |= FL_STALE;
	break;
	case IPPROTO_UDP:
		uh = (struct udphdr *)((caddr_t)ip + iphlen);
		sport = uh->uh_sport;
		dport = uh->uh_dport;
	break;
	case IPPROTO_SCTP:
		sh = (struct sctphdr *)((caddr_t)ip + iphlen);
		sport = sh->src_port;
		dport = sh->dest_port;
	break;
	default:
		if (*flags & FL_HASH_PORTS)
			goto noop;
		/* no port - hence not a protocol we care about */
		break;;
	
	}
	*protop = proto;

	/*
	 * If this is a transmit route cache then 
	 * hash all flows to a given destination to
	 * the same bucket
	 */
	if ((*flags & FL_HASH_PORTS) == 0)
		proto = sport = dport = 0;

	((uint16_t *)key)[0] = sport;
	((uint16_t *)key)[1] = dport; 

	hash = hashword(key, 3, hashjitter + proto);
	if (m->m_pkthdr.flowid == 0)
		m->m_pkthdr.flowid = hash;
	
	CTR5(KTR_SPARE3, "proto=%d hash=%x key[0]=%x sport=%d dport=%d\n", proto, hash, key[0], sport, dport);
	
	return (hash);
noop:
	*protop = proto;
	return (0);
}

static bitstr_t *
flowtable_mask(struct flowtable *ft)
{
	bitstr_t *mask;
	
	if (ft->ft_flags & FL_PCPU)
		mask = ft->ft_masks[curcpu];
	else
		mask = ft->ft_masks[0];

	return (mask);
}

static struct flentry *
flowtable_entry(struct flowtable *ft, uint32_t hash)
{
	struct flentry *fle;
	int index = (hash % ft->ft_size);
	
	
	if ((ft->ft_flags & FL_IPV6) == 0) {
		if (ft->ft_flags & FL_PCPU) {
			fle = (struct flentry *)
			    &ft->ft_table.v4_pcpu[curcpu][index];
		} else
			fle = (struct flentry *)&ft->ft_table.v4[index];
	} else {
		if (ft->ft_flags & FL_PCPU)
			fle = (struct flentry *)
			    &ft->ft_table.v6_pcpu[curcpu][index];
		else
			fle = (struct flentry *)&ft->ft_table.v6[index];
	}

	return (fle);
}

static int
flow_stale(struct flowtable *ft, struct flentry *fle)
{
	time_t idle_time;

	if ((fle->f_fhash == 0)
	    || ((fle->f_rt->rt_flags & RTF_HOST) &&
		((fle->f_rt->rt_flags & (RTF_UP))
		    != (RTF_UP)))
	    || (fle->f_rt->rt_ifp == NULL))
		return (1);

	idle_time = time_uptime - fle->f_uptime;

	if ((fle->f_flags & FL_STALE) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK|TH_FIN)) == 0
		&& (idle_time > ft->ft_udp_idle)) ||
	    ((fle->f_flags & TH_FIN)
		&& (idle_time > ft->ft_fin_wait_idle)) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK)) == TH_SYN
		&& (idle_time > ft->ft_syn_idle)) ||
	    ((fle->f_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)
		&& (idle_time > ft->ft_tcp_idle)) ||
	    ((fle->f_rt->rt_flags & RTF_UP) == 0 || 
		(fle->f_rt->rt_ifp == NULL)))
		return (1);

	return (0);
}

static void
flowtable_set_hashkey(struct flowtable *ft, struct flentry *fle, uint32_t *key)
{
	uint32_t *hashkey;
	int i, nwords;

	if (ft->ft_flags & FL_IPV6) {
		nwords = 9;
		hashkey = ((struct flentry_v4 *)fle)->fl_flow.ipf_key;
	} else {
		nwords = 3;
		hashkey = ((struct flentry_v6 *)fle)->fl_flow.ipf_key;
	}
	
	for (i = 0; i < nwords; i++) 
		hashkey[i] = key[i];
}

static int
flowtable_insert(struct flowtable *ft, uint32_t hash, uint32_t *key,
    uint8_t proto, struct route *ro, uint16_t flags)
{
	struct flentry *fle;
	volatile struct rtentry *rt0 = NULL;
	struct rtentry *rt1;
	int stale;
	bitstr_t *mask;
	
retry:	
	FL_ENTRY_LOCK(ft, hash);
	mask = flowtable_mask(ft);
	fle = flowtable_entry(ft, hash);
	if (fle->f_fhash) {
		if ((stale = flow_stale(ft, fle)) != 0) {
			fle->f_fhash = 0;
			rt0 = fle->f_rt;
			fle->f_rt = NULL;
			bit_clear(mask, FL_ENTRY_INDEX(ft, hash));
		}
		FL_ENTRY_UNLOCK(ft, hash);
		if (!stale)
			return (ENOSPC);

		rt1 = __DEVOLATILE(struct rtentry *, rt0);
		RTFREE(rt1);
		/*
		 * We might end up on a different cpu
		 */
		goto retry;
	       
	}
	flowtable_set_hashkey(ft, fle, key);
	bit_set(mask, FL_ENTRY_INDEX(ft, hash));

	fle->f_proto = proto;
	fle->f_rt = ro->ro_rt;
	fle->f_lle = ro->ro_lle;
	fle->f_fhash = hash;
	fle->f_uptime = time_uptime;
	FL_ENTRY_UNLOCK(ft, hash);
	return (0);
}

static int
flowtable_key_equal(struct flentry *fle, uint32_t *key, int flags)
{
	uint32_t *hashkey;
	int i, nwords;

	if (flags & FL_IPV6) {
		nwords = 9;
		hashkey = ((struct flentry_v4 *)fle)->fl_flow.ipf_key;
	} else {
		nwords = 3;
		hashkey = ((struct flentry_v6 *)fle)->fl_flow.ipf_key;
	}
	
	for (i = 0; i < nwords; i++) 
		if (hashkey[i] != key[i])
			return (0);

	return (1);
}

int
flowtable_lookup(struct flowtable *ft, struct mbuf *m, struct route *ro)
{
	uint32_t key[9], hash;
	struct flentry *fle;
	uint16_t flags;
	uint8_t proto = 0;
	int cache = 1, error = 0;
	struct rtentry *rt;
	struct llentry *lle;

	flags = ft ? ft->ft_flags : 0;
	ro->ro_rt = NULL;
	ro->ro_lle = NULL;

	/*
	 * The internal hash lookup is the only IPv4 specific bit
	 * remaining
	 */
	hash = ipv4_flow_lookup_hash_internal(m, ro, key,
	    &flags, &proto);

	/*
	 * Ports are zero and this isn't a transmit cache
	 * - thus not a protocol for which we need to keep 
	 * statex
	 * FL_HASH_PORTS => key[0] != 0 for TCP || UDP || SCTP
	 */
	if (hash == 0 || (key[0] == 0 && (ft->ft_flags & FL_HASH_PORTS))) {
		cache = 0;
		goto uncached;
	}
	FL_ENTRY_LOCK(ft, hash);
	fle = FL_ENTRY(ft, hash);
	rt = __DEVOLATILE(struct rtentry *, fle->f_rt);
	lle = __DEVOLATILE(struct llentry *, fle->f_lle);
	if ((rt != NULL)
	    && fle->f_fhash == hash
	    && flowtable_key_equal(fle, key, flags)
	    && (proto == fle->f_proto)
	    && (rt->rt_flags & RTF_UP)
	    && (rt->rt_ifp != NULL)) {
		fle->f_uptime = time_uptime;
		fle->f_flags |= flags;
		ro->ro_rt = rt;
		ro->ro_lle = lle;
		FL_ENTRY_UNLOCK(ft, hash);
		return (0);
	} 
	FL_ENTRY_UNLOCK(ft, hash);

uncached:
	/*
	 * This bit of code ends up locking the
	 * same route 3 times (just like ip_output + ether_output)
	 * - at lookup
	 * - in rt_check when called by arpresolve
	 * - dropping the refcount for the rtentry
	 *
	 * This could be consolidated to one if we wrote a variant
	 * of arpresolve with an rt_check variant that expected to
	 * receive the route locked
	 */
	ft->ft_rtalloc(ro, hash, M_GETFIB(m));
	if (ro->ro_rt == NULL) 
		error = ENETUNREACH;
	else {
		int finsert;
		struct llentry *lle = NULL;
		struct sockaddr *l3addr;
		struct rtentry *rt = ro->ro_rt;
		struct ifnet *ifp = rt->rt_ifp;

		if (rt->rt_flags & RTF_GATEWAY)
			l3addr = rt->rt_gateway;
		else
			l3addr = &ro->ro_dst;
		IF_AFDATA_RLOCK(ifp);	
		lle = lla_lookup(LLTABLE(ifp), LLE_EXCLUSIVE, l3addr);
		IF_AFDATA_RUNLOCK(ifp);
		if ((lle == NULL) && 
		    (ifp->if_flags & (IFF_NOARP | IFF_STATICARP)) == 0) {
			IF_AFDATA_WLOCK(ifp);
			lle = lla_lookup(LLTABLE(ifp),
			    (LLE_CREATE | LLE_EXCLUSIVE), l3addr);
			IF_AFDATA_WUNLOCK(ifp);	
		}
		if (lle != NULL) {
			LLE_ADDREF(lle);
			LLE_WUNLOCK(lle);
		}
		ro->ro_lle = lle;
		finsert = ((lle != NULL) && cache);
		if (finsert) 
			error = flowtable_insert(ft, hash, key, proto,
			    ro, flags);
				
		if (error || !finsert) {
			RTFREE(rt);
			if (lle != NULL)
				LLE_FREE(lle);
		}
		error = 0;
	} 

	return (error);
}

#ifdef notyet
static __inline int
bit_fns(bitstr_t *name, int nbits, int lastbit)
{
	int lastbit_start = lastbit & ~0x7;
	bitstr_t *bitstr_start = &name[lastbit_start];
	int value = 0;

	while (value <= lastbit && value != 1)
		bit_ffs(bitstr_start, nbits, &value);

	return (value);
}
#endif

struct flowtable *
flowtable_alloc(int nentry, int flags)
{
	struct flowtable *ft;
	int i;

	if (hashjitter == 0)
		hashjitter = arc4random();

	ft = malloc(sizeof(struct flowtable),
	    M_RTABLE, M_WAITOK | M_ZERO);

	ft->ft_flags = flags;
	ft->ft_size = nentry;
#ifdef RADIX_MPATH
	ft->ft_rtalloc = rtalloc_mpath_fib;
#else
	ft->ft_rtalloc = in_rtalloc_ign_wrapper;
#endif
	if (flags & FL_PCPU) {
		ft->ft_lock = flowtable_pcpu_lock;
		ft->ft_unlock = flowtable_pcpu_unlock;

		for (i = 0; i < mp_ncpus; i++) {
			ft->ft_table.v4_pcpu[i] =
			    malloc(nentry*sizeof(struct flentry_v4),
				M_RTABLE, M_WAITOK | M_ZERO);
			ft->ft_masks[i] = bit_alloc(nentry);
		}
	} else {
		ft->ft_lock_count = 2*(powerof2(mp_ncpus) ? mp_ncpus :
		    (fls(mp_ncpus) << 1));
		
		ft->ft_lock = flowtable_global_lock;
		ft->ft_unlock = flowtable_global_unlock;
		ft->ft_table.v4 =
			    malloc(nentry*sizeof(struct flentry_v4),
				M_RTABLE, M_WAITOK | M_ZERO);
		ft->ft_locks = malloc(ft->ft_lock_count*sizeof(struct mtx),
				M_RTABLE, M_WAITOK | M_ZERO);
		for (i = 0; i < ft->ft_lock_count; i++)
			mtx_init(&ft->ft_locks[i], "flow", NULL, MTX_DEF|MTX_DUPOK);

		ft->ft_masks[0] = bit_alloc(nentry);
	}

	/*
	 * In the local transmit case the table truly is 
	 * just a cache - so everything is eligible for
	 * replacement after 5s of non-use
	 */
	if (flags & FL_HASH_PORTS) {
		ft->ft_udp_idle = UDP_IDLE;
		ft->ft_syn_idle = SYN_IDLE;
		ft->ft_fin_wait_idle = FIN_WAIT_IDLE;
		ft->ft_tcp_idle = TCP_IDLE;
	} else {
		ft->ft_udp_idle = ft->ft_fin_wait_idle =
		    ft->ft_syn_idle = ft->ft_tcp_idle = 30;
		
	}
	
	
	return (ft);
}

