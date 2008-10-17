#include "opt_mpath.h"

#include <sys/param.h>  
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>  
#include <sys/bitstring.h>
#include <sys/vimage.h>


#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>

#include <net/route.h> 
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
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


struct ip_tuple {
	in_addr_t 	ip_saddr;	/* source address */
	in_addr_t 	ip_daddr;	/* destination address */
	uint16_t 	ip_sport;	/* source port */
	uint16_t 	ip_dport;	/* destination port */
};

union ip_flow {
	struct ip_tuple ipf_ipt;
	uint32_t 	ipf_key[3];
};

struct flentry_v4 {
	uint32_t	fl_fhash;	/* hash flowing forward */
	uint32_t	fl_ticks;	/* last time this flow was accessed */
	uint16_t	fl_flags;	/* flow flags */
	uint8_t		fl_pad;
	uint8_t		fl_proto;	/* protocol */
	union ip_flow	fl_flow;
	struct rtentry *fl_rt;		/* rtentry for flow */
	uint32_t	fl_refcnt;
	uint32_t	fl_hash_next;	/* needed for GC */
	uint32_t	fl_hash_prev;
};

#define	TICKS_PER_MINUTE	(60*hz)
#define	TICKS_PER_HOUR		(60*TICKS_PER_MINUTE)
#define	TICKS_PER_DAY		(24*TICKS_PER_HOUR)


#define SYN_IDLE		(5*TICKS_PER_MINUTE)
#define UDP_IDLE		(5*TICKS_PER_MINUTE)
#define FIN_WAIT_IDLE		(10*TICKS_PER_MINUTE)
#define TCP_IDLE		TICKS_PER_DAY


static struct flentry_v4 *ipv4_flow_table;
static int ipv4_flow_table_size;
static bitstr_t *ipv4_flow_bitstring;
static int ipv4_flow_allocated;
struct mtx *ipv4_flow_locks;
static int ipv4_flow_lock_count;
extern uint32_t hashjitter;
static uint32_t ipv4_flow_route_lookup_fail;
static uint32_t	ipv4_flow_collisions;
struct callout ipv4_flow_callout;
static int ipv4_flow_max_count;


#define FL_ENTRY_INDEX(hash)((hash) % ipv4_flow_table_size)
#define FL_ENTRY(hash) (&ipv4_flow_table[FL_ENTRY_INDEX((hash))])
#define FL_ENTRY_LOCK(hash) mtx_lock(&ipv4_flow_locks[(hash)&(ipv4_flow_lock_count - 1)])
#define FL_ENTRY_UNLOCK(hash) mtx_lock(&ipv4_flow_locks[(hash)&(ipv4_flow_lock_count - 1)])

#define FL_STALE (1<<8)

static uint32_t
ipv4_flow_lookup_hash_internal(struct mbuf *m, struct route *ro,
    uint32_t *key, uint16_t *flags, uint8_t *protop)
{
	uint16_t sport = 0, dport = 0;
	struct ip *ip = mtod(m, struct ip *);
	uint8_t proto = ip->ip_p;
	int iphlen = ip->ip_hl << 2;
	struct sockaddr_in *sin;
	struct tcphdr *th;
	struct udphdr *uh;
	struct sctphdr *sh;

	key[0] = ip->ip_src.s_addr;
	key[1] = ip->ip_dst.s_addr;	

	sin = (struct sockaddr_in *)&ro->ro_dst;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = ip->ip_dst;

	switch (proto) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + iphlen);
		sport = th->th_sport;
		dport = th->th_dport;
		*flags = th->th_flags;
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
		/* no port - hence not a protocol we care about */
		break;;
	
	}
	((uint16_t *)key)[4] = sport;
	((uint16_t *)key)[5] = dport;

	*protop = proto;
	return (hashword(key, 3, hashjitter + proto));
}

uint32_t
ipv4_flow_lookup_hash(struct mbuf *m)
{
	struct route ro;
	uint32_t key[3];
	uint16_t flags;
	uint8_t proto;
	
	bzero(&ro, sizeof(ro));
	return (ipv4_flow_lookup_hash_internal(m, &ro, key, &flags, &proto));
}

static void
ipv4_flow_insert(uint32_t hash, uint32_t *key, uint8_t proto,
    struct rtentry *rt, uint16_t flags)
{
	struct flentry_v4 *fle, *fle2;
	uint32_t *hashkey;
	
	fle = FL_ENTRY(hash);
	hashkey = fle->fl_flow.ipf_key;

	hashkey[0] = key[0];
	hashkey[1] = key[1];
	hashkey[2] = key[2];

	bit_set(ipv4_flow_bitstring, FL_ENTRY_INDEX(hash));
	if (rt->rt_flow_head == 0) {
		rt->rt_flow_head = hash;
		fle->fl_hash_next = fle->fl_hash_prev = 0;
	} else {
		fle->fl_hash_next = rt->rt_flow_head;
		fle2 = FL_ENTRY(rt->rt_flow_head);
		rt->rt_flow_head = hash;
		fle2->fl_hash_prev = hash;
	}
	fle->fl_proto = proto;
	fle->fl_rt = rt;
	fle->fl_fhash = hash;
	fle->fl_ticks = ticks;
	rt->rt_refcnt++;
	ipv4_flow_allocated++;
}

uint32_t
ipv4_flow_alloc(struct mbuf *m, struct route *ro)
{
	uint32_t key[3], hash, *hashkey;
	struct flentry_v4 *fle;
	uint16_t flags = 0;
	uint8_t proto;
	
	/*
	 * Only handle IPv4 for now
	 *
	 */
	hash = ipv4_flow_lookup_hash_internal(m, ro, key, &flags, &proto);

	/*
	 * Ports are zero - thus not a protocol for which 
	 * we need to keep state
	 */
	if (key[3] == 0)
		return (hash);
	
	FL_ENTRY_LOCK(hash);
	fle = FL_ENTRY(hash);

	hashkey = fle->fl_flow.ipf_key;
	
	if (fle->fl_fhash == 0) {
		FL_ENTRY_UNLOCK(hash);
		rtalloc_mpath_fib(ro, hash, M_GETFIB(m));
		if (ro->ro_rt) {
			FL_ENTRY_LOCK(hash);
			ipv4_flow_insert(hash, key, proto, ro->ro_rt, flags);
			RT_UNLOCK(ro->ro_rt);
		} else
			ipv4_flow_route_lookup_fail++;
	} else if (fle->fl_fhash == hash
	    && key[0] == hashkey[0] 
	    && key[1] == hashkey[1]
	    && key[2] == hashkey[2]
	    && proto == fle->fl_proto) {
		fle->fl_ticks = ticks;
		fle->fl_flags |= flags;
		fle->fl_refcnt++;
		ro->ro_rt = fle->fl_rt;
	} else 
		ipv4_flow_collisions++;
		
	FL_ENTRY_UNLOCK(hash);

	return (hash);
}

/*
 * Internal helper routine
 * hash - the hash of the entry to free
 * stale - indicates to only free the entry if it is marked stale
 */

static uint32_t
ipv4_flow_free_internal(uint32_t hash, int staleonly)
{
	struct flentry_v4 *fle, *fleprev, *flenext;
	uint32_t hash_next;

	fle = FL_ENTRY(hash);
	hash_next = fle->fl_hash_next;
	
	if (staleonly && ((fle->fl_flags & FL_STALE) == 0))
	    return (hash_next);
	
	if (fle->fl_hash_next) {
		flenext = FL_ENTRY(fle->fl_hash_next);
		flenext->fl_hash_prev = fle->fl_hash_prev;
	}
	if (fle->fl_hash_prev) {
		fleprev = FL_ENTRY(fle->fl_hash_prev);
		fleprev->fl_hash_next = fle->fl_hash_next;
	}
	fle->fl_hash_next = fle->fl_hash_prev = 0;
	    
	if (fle->fl_refcnt == 0) {
		fle->fl_rt->rt_refcnt--;
		ipv4_flow_allocated--;
		bit_clear(ipv4_flow_bitstring, FL_ENTRY_INDEX(hash));
		bzero(fle, sizeof(struct flentry_v4));
	} else if (!staleonly) 
		fle->fl_flags |= FL_STALE;

	return (hash_next);
}

/*
 * drops the refcount on the flow after alloc was called and 
 * checks if the flow has become stale since alloc was called
 *
 */
void
ipv4_flow_free(uint32_t hash)
{
	struct flentry_v4 *fle;
	struct rtentry *rt;
	int stale;

	fle = FL_ENTRY(hash);
	KASSERT(fle->fl_refcnt > 0,
	    ("route referenced with flow refcount set to zero"));

	stale = ((fle->fl_flags & FL_STALE) &&
	    (fle->fl_refcnt == 1));

	rt = fle->fl_rt;
	if (stale)
		RT_LOCK(rt);
	
	FL_ENTRY_LOCK(hash);
	fle->fl_refcnt--;

	if (stale) {
 		ipv4_flow_free_internal(hash, 0);
		RTFREE_LOCKED(rt);
	} 
	FL_ENTRY_UNLOCK(hash);
}

/*
 *
 * Frees all flows that are linked to this rtentry
 *
 */
void
ipv4_flow_free_all(struct rtentry *rt)
{
	uint32_t hash_next = rt->rt_flow_head;

	RT_LOCK_ASSERT(rt);
	while (hash_next) 
		hash_next = ipv4_flow_free_internal(hash_next, 0);
}

/*
 * Frees all flows tied to this rt that 
 * have been marked stale
 *
 */
static int
ipv4_flow_free_stale(struct radix_node *rn, void *unused)
{
	struct rtentry *rt = (struct rtentry *)rn;
	uint32_t hash_next; 

	if (rt->rt_flow_head == 0)
		return (0);

	RT_LOCK(rt);
	hash_next = rt->rt_flow_head;
	while (hash_next)
		hash_next = ipv4_flow_free_internal(hash_next, 1);
	RT_UNLOCK(rt);

	return (0);
}

struct radix_node_head *ipv4_flow_rnh_list[100];
static void
ipv4_flow_check_stale(struct flentry_v4 *fle,
    struct radix_node_head **rnh_list, int *rnh_count)
{
	int count = *rnh_count;
	uint32_t idle_ticks;
	struct radix_node_head *rnh;
	struct rtentry *rt;
	int i, stale = 0, found = 0;
	
	if (ticks > fle->fl_ticks)
		idle_ticks = ticks - fle->fl_ticks;
	else
		idle_ticks = (INT_MAX - fle->fl_ticks) + ticks ;
	
	if ((fle->fl_flags & FL_STALE) ||
	    ((fle->fl_flags & (TH_SYN|TH_ACK|TH_FIN)) == 0
		&& (idle_ticks > UDP_IDLE)) ||
	    ((fle->fl_flags & TH_FIN)
		&& (idle_ticks > FIN_WAIT_IDLE)) ||
	    ((fle->fl_flags & (TH_SYN|TH_ACK)) == TH_SYN
		&& (idle_ticks > SYN_IDLE)) ||
	    ((fle->fl_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)
		&& (idle_ticks > TCP_IDLE)))
		stale = 1;

	if (stale == 0)
		return;

	fle->fl_flags |= FL_STALE;
	rt = fle->fl_rt;
	rnh = V_rt_tables[rt->rt_fibnum][rt_key(rt)->sa_family];

	for (i = 0; i < count; i++) 
		if (rnh_list[i] == rnh) {
			found  = 1;
			break;
		}
	if (found == 0) {
		rnh_list[count] = rnh;
		count++;
		*rnh_count = count;
	}
}


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


static int ipv4_flow_last_index;
static void
ipv4_flow_timeout(void *arg)
{
	int i, idx, rnh_count = 0;
	struct radix_node_head *rnh;
	
	/*
	 * scan 1/4th of the table once a second
	 */
	for (i = 0; i < (ipv4_flow_allocated >> 2); i++) {
		idx = bit_fns(ipv4_flow_bitstring, ipv4_flow_table_size,
		    ipv4_flow_last_index);
		if (idx == -1) {
			ipv4_flow_last_index = 0;
			break;
		}
		
		FL_ENTRY_LOCK(idx);
		ipv4_flow_check_stale(FL_ENTRY(idx), ipv4_flow_rnh_list, &rnh_count);
		FL_ENTRY_UNLOCK(idx);
	}
	for (i = 0; i < rnh_count; i++) {
		rnh = ipv4_flow_rnh_list[i];
		RADIX_NODE_HEAD_LOCK(rnh);
		rnh->rnh_walktree(rnh, ipv4_flow_free_stale, NULL);
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}

	callout_reset(&ipv4_flow_callout, hz, ipv4_flow_timeout, NULL);
}

static void
flowtable_init(void *unused) 
{
	int i, nentry;

	nentry = ipv4_flow_max_count;
	/*
	 * round mp_ncpus up to the next power of 2 and double
	 * to determine the number of locks
	 */
	ipv4_flow_lock_count = (1 << fls(mp_ncpus)) << 1;
	
	ipv4_flow_table_size = nentry;
	ipv4_flow_table = malloc(nentry*sizeof(struct flentry_v4),
	    M_RTABLE, M_WAITOK | M_ZERO);
	ipv4_flow_bitstring = bit_alloc(nentry);
	ipv4_flow_locks = malloc(ipv4_flow_lock_count*sizeof(struct mtx),
	    M_RTABLE, M_WAITOK | M_ZERO);
	for (i = 0; i < ipv4_flow_lock_count; i++)
		mtx_init(&ipv4_flow_locks[i], "ipv4_flow", NULL, MTX_DEF);
	
}
SYSINIT(flowtable, SI_SUB_INIT_IF, SI_ORDER_ANY, flowtable_init, NULL);
