#ifndef _NET_BPFQ_H_
#define _NET_BPFQ_H_

#include <sys/param.h>
#include <sys/bitset.h>
#include <sys/_bitset.h>

#define BPFQ_BITS			256
BITSET_DEFINE(bpf_qmask_bits, BPFQ_BITS);

#define	BPFQ_CLR(n, p)			BIT_CLR(BPFQ_BITS, n, p)
#define	BPFQ_COPY(f, t)			BIT_COPY(BPFQ_BITS, f, t)
#define	BPFQ_ISSET(n, p)		BIT_ISSET(BPFQ_BITS, n, p)
#define	BPFQ_SET(n, p)			BIT_SET(BPFQ_BITS, n, p)
#define	BPFQ_ZERO(p) 			BIT_ZERO(BPFQ_BITS, p)
#define	BPFQ_FILL(p) 			BIT_FILL(BPFQ_BITS, p)
#define	BPFQ_SETOF(n, p)		BIT_SETOF(BPFQ_BITS, n, p)
#define	BPFQ_EMPTY(p)			BIT_EMPTY(BPFQ_BITS, p)
#define	BPFQ_ISFULLSET(p)		BIT_ISFULLSET(BPFQ_BITS, p)
#define	BPFQ_SUBSET(p, c)		BIT_SUBSET(BPFQ_BITS, p, c)
#define	BPFQ_OVERLAP(p, c)		BIT_OVERLAP(BPFQ_BITS, p, c)
#define	BPFQ_CMP(p, c)			BIT_CMP(BPFQ_BITS, p, c)
#define	BPFQ_OR(d, s)			BIT_OR(BPFQ_BITS, d, s)
#define	BPFQ_AND(d, s)			BIT_AND(BPFQ_BITS, d, s)
#define	BPFQ_NAND(d, s)			BIT_NAND(BPFQ_BITS, d, s)
#define	BPFQ_CLR_ATOMIC(n, p)		BIT_CLR_ATOMIC(BPFQ_BITS, n, p)
#define	BPFQ_SET_ATOMIC(n, p)		BIT_SET_ATOMIC(BPFQ_BITS, n, p)
#define	BPFQ_OR_ATOMIC(d, s)		BIT_OR_ATOMIC(BPFQ_BITS, d, s)
#define	BPFQ_COPY_STORE_REL(f, t)	BIT_COPY_STORE_REL(BPFQ_BITS, f, t)
#define	BPFQ_FFS(p)			BIT_FFS(BPFQ_BITS, p)

#endif
