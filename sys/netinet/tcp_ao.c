/*
 * Copyright (c) 2013 Juniper Networks
 * All rights reserved.
 *
 * Written by Andre Oppermann <andre@FreeBSD.org>
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

/*
 * TCP-AO protects a TCP session and individual segments with a keyed hash
 * mechanism.  It is specified in RFC5925 and RFC5926.  It is intended to
 * eventually replace TCP-MD5 RFC2385.
 *
 * The implementation consists of 4 parts:
 *  1. the hash implementation to sign and verify segments.
 *  2. changes to the tcp input path to validate incoming segments,
 *     and changes to the tcp output path to sign outgoing segments.
 *  3. key management in the kernel and the exposed userland API.
 *  4. test programs to verify the correct operation.
 *
 * The key management is done through the setsockopt() api because the
 * keys are connection specific.
 *
 * TODO:
 *  sequence number extension
 *  tcp options verification option
 *  key rollover
 *  bring legacy tcp-md5 into the tcp-ao framework.
 *  throughout testing
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/jail.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <sys/md5.h>
#include <crypto/sha1.h>
#include <crypto/cmac/cmac.h>
#include <crypto/hmac/hmac.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_ao.h>

/*
 * The code below is skeleton code and not functional yet.
 */

MALLOC_DEFINE(M_TCPAO, "tcp_ao", "TCP-AO peer and key structures");

static struct tcp_ao_peer
    *tcp_ao_peer_find(struct tcp_ao_cb *, struct sockaddr *);
static struct tcp_ao_peer
    *tcp_ao_peer_add(struct tcp_ao_cb *, struct sockaddr *);
static int      tcp_ao_peer_del(struct tcp_ao_cb *, struct tcp_ao_peer *);
static void     tcp_ao_peer_flush(struct tcp_ao_cb *);

static struct tcp_ao_key
    *tcp_ao_key_find(struct tcp_ao_peer *, uint8_t);
static struct tcp_ao_key
    *tcp_ao_key_add(struct tcp_ao_peer *, struct tcp_ao_sopt *, uint8_t);
static int	tcp_ao_key_del(struct tcp_ao_peer *, uint8_t);
static void     tcp_ao_key_flush(struct tcp_ao_peer *);

static int      tcp_ao_peer_clone(struct tcpcb *, struct tcpcb *,
    struct sockaddr *);
static struct tcp_ao_cb
    *tcp_ao_cb_alloc(void);

/*
 * Perform key management when called from setsockopt.
 */
int
tcp_ao_ctl(struct tcpcb *tp, struct tcp_ao_sopt *tao, int tao_len)
{
	struct tcp_ao_cb *c;
	struct tcp_ao_peer *p;
	struct tcp_ao_key *k;
	struct sockaddr *sa;
	int error = EINVAL;

	sa = (struct sockaddr *)&tao->tao_peer;

	switch (tao->tao_peer.ss_family) {
	case AF_INET:
		if (sa->sa_len != sizeof(struct sockaddr_in))
			error = EINVAL;
		break;
	case AF_INET6:
		if (sa->sa_len != sizeof(struct sockaddr_in6))
			error = EINVAL;
		break;
	default:
		error = EINVAL;
	}
	if (error)
		goto out;

	if (tp->t_ao == NULL) {
		if ((tp->t_ao = tcp_ao_cb_alloc()) == NULL) {
			error = ENOMEM;
			goto out;
		}
		tp->t_flags |= TF_AO;
	}
	c = tp->t_ao;

	switch (tao->tao_cmd) {
	case TAO_CMD_ADD:
		switch (tao->tao_algo) {
		case TAO_ALGO_MD5SIG:
		case TAO_ALGO_HMAC_SHA_1_96:
		case TAO_ALGO_AES_128_CMAC_96:
			break;
		default:
			error = EINVAL;
			goto out;
		}

		/* Find existing peer or allocate new one. */
		if ((p = tcp_ao_peer_add(c, sa)) == NULL) {
			error = EINVAL;
			break;
		}
		/* Can't overwrite active key on an active session. */
		if (tp->t_state > TCPS_LISTEN &&
		    p->tap_activekey == tao->tao_keyidx) {
			error = EINVAL;
			break;
		}
		if ((k = tcp_ao_key_add(p, tao, tao_len - sizeof(*tao))) == NULL) {
			error = EINVAL;
		}
		break;

	case TAO_CMD_DELIDX:
		if ((p = tcp_ao_peer_find(c, sa)) == NULL) {
			error = EINVAL;
			break;
		}
		/* Can't remove active key index. */
		if (tp->t_state > TCPS_LISTEN &&
		    p->tap_activekey == tao->tao_keyidx) {
			error = EINVAL;
			break;
		}
		if (tcp_ao_key_del(p, tao->tao_keyidx) != 0) {
			error = ENOENT;
		}
		break;

	case TAO_CMD_DELPEER:
		/* Can't remove active peer. */
		if (tp->t_state > TCPS_LISTEN)
			break;

		if ((p = tcp_ao_peer_find(c, sa)) == NULL) {
			error = EINVAL;
			break;
		}
		tcp_ao_peer_del(c, p);
		break;

	case TAO_CMD_FLUSH:
		/* Can't remove active peer. */
		if (tp->t_state > TCPS_LISTEN)
			break;

		tcp_ao_peer_flush(c);
		break;

	default:
		error = EINVAL;
		goto out;
	}

out:
	return (error);
}

static struct tcp_ao_peer *
tcp_ao_peer_find(struct tcp_ao_cb *tac, struct sockaddr *sa)
{
	struct tcp_ao_peer *p;

	LIST_FOREACH(p, &tac->tac_peers, tap_entry) {
		if (p->tap_peer.sa.sa_family == sa->sa_family &&
		    !bcmp(p->tap_peer.sa.sa_data, sa->sa_data,
			min(p->tap_peer.sa.sa_len, sa->sa_len)))
			return (p);
	}
	return (NULL);
}

static struct tcp_ao_peer *
tcp_ao_peer_add(struct tcp_ao_cb *tac, struct sockaddr *sa)
{
	struct tcp_ao_peer *p;

	if ((p = tcp_ao_peer_find(tac, sa)) == NULL) {
		if ((p = malloc(sizeof(*p), M_TCPAO, M_NOWAIT)) == NULL)
			return (p);
		p->tap_flags = 0;
		bcopy(sa, &p->tap_peer, sa->sa_len);
		SLIST_INIT(&p->tap_keys);
	}

	LIST_INSERT_HEAD(&tac->tac_peers, p, tap_entry);
	return (p);
}

static int
tcp_ao_peer_del(struct tcp_ao_cb *tac, struct tcp_ao_peer *tap)
{

	tcp_ao_key_flush(tap);
	LIST_REMOVE(tap, tap_entry);
	free(tap, M_TCPAO);
	return (0);
}

static void
tcp_ao_peer_flush(struct tcp_ao_cb *tac)
{
	struct tcp_ao_peer *p, *p2;

	LIST_FOREACH_SAFE(p, &tac->tac_peers, tap_entry, p2) {
		tcp_ao_key_flush(p);
		free(p, M_TCPAO);
	}
	LIST_INIT(&tac->tac_peers);
}

static struct tcp_ao_key *
tcp_ao_key_find(struct tcp_ao_peer *tap, uint8_t idx)
{
	struct tcp_ao_key *k;

	SLIST_FOREACH(k, &tap->tap_keys, entry) {
		if (k->keyidx == idx)
			return (k);
	}
	return (NULL);
}

static struct tcp_ao_key *
tcp_ao_key_add(struct tcp_ao_peer *tap, struct tcp_ao_sopt *tao, uint8_t keylen)
{
	struct tcp_ao_key *k;

	if ((k = tcp_ao_key_find(tap, tao->tao_keyidx)) != NULL) {
		SLIST_REMOVE(&tap->tap_keys, k, tcp_ao_key, entry);
		free(k, M_TCPAO);
	}
	if ((k = malloc(sizeof(*k) + keylen, M_TCPAO, M_NOWAIT)) == NULL)
		return (k);

	k->keyidx = tao->tao_keyidx;
	k->keyflags = 0;
	k->keyalgo = tao->tao_algo;
	k->keylen = keylen;
	bcopy(tao->tao_key, k->key, k->keylen);

	SLIST_INSERT_HEAD(&tap->tap_keys, k, entry);
	return (k);
}

static int
tcp_ao_key_del(struct tcp_ao_peer *tap, uint8_t keyidx)
{
	struct tcp_ao_key *k, *k2;

	SLIST_FOREACH_SAFE(k, &tap->tap_keys, entry, k2) {
		if (k->keyidx == keyidx) {
			SLIST_REMOVE(&tap->tap_keys, k, tcp_ao_key, entry);
			free(k, M_TCPAO);
			return (0);
		}
	}
	return (ENOENT);
}

static void
tcp_ao_key_flush(struct tcp_ao_peer *tap)
{
	struct tcp_ao_key *k, *k2;

	SLIST_FOREACH_SAFE(k, &tap->tap_keys, entry, k2)
		free(k, M_TCPAO);
	SLIST_INIT(&tap->tap_keys);
}

static int
tcp_ao_peer_clone(struct tcpcb *tp1, struct tcpcb *tp2, struct sockaddr *sa)
{
	struct tcp_ao_peer *p1, *p2;
	struct tcp_ao_key *k1, *k2;

	if ((p1 = tcp_ao_peer_find(tp1->t_ao, sa)) == NULL)
		return (0);

	if ((tp2->t_ao = tcp_ao_cb_alloc()) == NULL)
		return (ENOMEM);

	if ((p2 = tcp_ao_peer_add(tp2->t_ao, sa)) == NULL)
		return (ENOMEM);

	bcopy(p1, p2, sizeof(*p2));

	SLIST_INIT(&p2->tap_keys);
	SLIST_FOREACH(k1, &p1->tap_keys, entry) {
		if ((k2 = malloc(sizeof(*k2) + k1->keylen, M_TCPAO, M_NOWAIT)) == NULL)
			return (ENOMEM);
		bcopy(k1, k2, k1->keylen);
		SLIST_INSERT_HEAD(&p2->tap_keys, k2, entry);
	}
	return (0);
}

struct tcp_ao_cb *
tcp_ao_cb_alloc(void)
{
	struct tcp_ao_cb *c;

	if ((c = malloc(sizeof(*c), M_TCPAO, M_ZERO|M_NOWAIT)) == NULL)
		return (NULL);
	LIST_INIT(&c->tac_peers);
	return (c);
}

void
tcp_ao_cb_free(struct tcpcb *tp)
{
	struct tcp_ao_cb *c;

	c = tp->t_ao;
	tp->t_ao = NULL;
	tcp_ao_peer_flush(c);
	free(c, M_TCPAO);
}

/*
 * There two types of key derivation in TCP-AO.
 * One is to create the session key from the imported master key.
 * It involves individual session parameters like the ip addresses,
 * port numbers and inititial sequence numbers.
 * The other is in additional step for certain MAC algorithms when
 * the user supplied key is not exactly the required key MAC size.
 * Here we have to run the key through a special round of key
 * derivation first to get the desired key length.
 */

/*
 * Context for key derivation.
 */
struct tcp_ao_kdf_ctx {
	union {
		struct ip4 {
			struct in_addr src, dst;
			uint16_t sport, dport;
			uint32_t irs, iss;
		} ip4;
		struct ip6 {
			struct in6_addr src, dst;
			uint16_t sport, dport;
			uint32_t irs, iss;
		} ip6;
	} tuple;
	int len;
};
#define ip4_ctx tuple.ip4
#define ip6_ctx tuple.ip6

static int tcp_ao_kdf(struct tcp_ao_key *tak, struct in_conninfo *inc,
    struct tcphdr *th, uint8_t *out, int outlen, int dir);
static int tcp_ao_kdf_hmac(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx);
static int tcp_ao_kdf_cmac(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx);
static int tcp_ao_kdf_md5(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx);

/*
 * Key derivation for sessions and the derived master keys.
 * Return values:
 *  0      = success
 *  EINVAL = invalid input, typically insufficient length
 */
static int
tcp_ao_kdf(struct tcp_ao_key *tak, struct in_conninfo *inc, struct tcphdr *th,
    uint8_t *out, int outlen, int dir)
{
	int error = 0;
	struct tcp_ao_kdf_ctx ctx;

	/* Fill in context for traffic keys. */
	switch (inc->inc_flags & INC_ISIPV6) {
	case 0:
		if (dir == TCP_AO_OUT) {
			ctx.ip4_ctx.src = inc->inc_ie.ie_laddr;
			ctx.ip4_ctx.dst = inc->inc_ie.ie_faddr;
			ctx.ip4_ctx.sport = inc->inc_ie.ie_lport;
			ctx.ip4_ctx.dport = inc->inc_ie.ie_fport;
		} else {
			ctx.ip4_ctx.src = inc->inc_ie.ie_faddr;
			ctx.ip4_ctx.dst = inc->inc_ie.ie_laddr;
			ctx.ip4_ctx.sport = inc->inc_ie.ie_fport;
			ctx.ip4_ctx.dport = inc->inc_ie.ie_lport;
		}
		ctx.ip4_ctx.irs = htonl(th->th_ack);
		ctx.ip4_ctx.iss = htonl(th->th_seq);
		ctx.len = sizeof(ctx.ip4_ctx);
		break;
	case INC_ISIPV6:
		if (dir == TCP_AO_OUT) {
			ctx.ip6_ctx.src = inc->inc_ie.ie6_laddr;
			ctx.ip6_ctx.dst = inc->inc_ie.ie6_faddr;
			ctx.ip6_ctx.sport = inc->inc_ie.ie_lport;
			ctx.ip6_ctx.dport = inc->inc_ie.ie_fport;
		} else {
			ctx.ip6_ctx.src = inc->inc_ie.ie6_faddr;
			ctx.ip6_ctx.dst = inc->inc_ie.ie6_laddr;
			ctx.ip6_ctx.sport = inc->inc_ie.ie_fport;
			ctx.ip6_ctx.dport = inc->inc_ie.ie_lport;
		}
		ctx.ip6_ctx.irs = htonl(th->th_ack);
		ctx.ip6_ctx.iss = htonl(th->th_seq);
		ctx.len = sizeof(ctx.ip6_ctx);
		break;
	default:
		error = EINVAL;
		goto out;
	}

	switch (tak->keyalgo) {
	case TAO_ALGO_HMAC_SHA_1_96:
		error = tcp_ao_kdf_hmac(tak, out, outlen, &ctx);
		break;
	case TAO_ALGO_AES_128_CMAC_96:
		error = tcp_ao_kdf_cmac(tak, out, outlen, &ctx);
		break;
	case TAO_ALGO_MD5SIG:
		error = tcp_ao_kdf_md5(tak, out, outlen, &ctx);
		break;
	default:
		error = EINVAL;
	}
	if (error)
		goto out;
out:
	return (error);
}

static int
tcp_ao_kdf_hmac(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx)
{
	HMAC_SHA1_CTX ctx;
	uint8_t res[SHA1_DIGEST_LENGTH];
	char *label = "TCP-AO";
	int error = 0;
	uint8_t i;

	for (i = 1; outlen > 0; outlen -= SHA1_DIGEST_LENGTH, i++) {
		HMAC_SHA1_Init(&ctx, tak->key, tak->keylen);

		HMAC_SHA1_Update(&ctx, &i, sizeof(i));
		HMAC_SHA1_Update(&ctx, label, sizeof(*label));
		if (tctx != NULL)
			HMAC_SHA1_Update(&ctx, (uint8_t *)tctx, tctx->len);
		HMAC_SHA1_Final(res, &ctx);

		bcopy(res, out, min(outlen, SHA1_DIGEST_LENGTH));
		out += SHA1_DIGEST_LENGTH;
	}
	return (error);
}

static int
tcp_ao_kdf_cmac(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx)
{
	AES_CMAC_CTX ctx;
	uint8_t res[AES_CMAC_DIGEST_LENGTH];
	uint8_t zero[AES_CMAC_KEY_LENGTH];
	char *label = "TCP-AO";
	int error = 0;
	uint8_t i;

	if (tctx == NULL) {
		bzero(zero, sizeof(*zero));
		AES_CMAC_Init(&ctx);
		AES_CMAC_SetKey(&ctx, zero);
		AES_CMAC_Update(&ctx, tak->key, tak->keylen);
		AES_CMAC_Final(res, &ctx);
		bcopy(res, out, min(outlen, AES_CMAC_DIGEST_LENGTH));
	}

	if (tak->keylen != AES_CMAC_KEY_LENGTH)
		return (EINVAL);

	for (i = 1; outlen > 0; outlen -= AES_CMAC_DIGEST_LENGTH, i++) {
		AES_CMAC_Init(&ctx);
		AES_CMAC_SetKey(&ctx, tak->key);
		AES_CMAC_Update(&ctx, &i, sizeof(i));
		AES_CMAC_Update(&ctx, label, sizeof(*label));
		AES_CMAC_Update(&ctx, (uint8_t *)tctx, tctx->len);
		AES_CMAC_Final(res, &ctx);

		bcopy(res, out, min(outlen, AES_CMAC_DIGEST_LENGTH));
		out += AES_CMAC_DIGEST_LENGTH;
	}
	return (error);
}

static int
tcp_ao_kdf_md5(struct tcp_ao_key *tak, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tctx)
{
	/* XXX: No key derivation is done. */
	return (0);
}


/*
 * The hash over the header has to be stiched together and a couple of
 * fields need manipulation, like zero'ing and byte order conversion.
 * Instead of doing it in-place and calling the hash update function
 * for each we just copy everything into one place in the right order.
 */
struct tcp_ao_thopt {
	struct tcphdr th;
	uint8_t tho[TCP_MAXOLEN];
};
struct tcp_ao_pseudo {
	uint32_t tap_sne;	/* sequence number extension */
	union {
		struct tap_ip {
			struct ippseudo tap_ph4;
			struct tcp_ao_thopt tap_th;
		} tap_ip;
		struct tap_ip6 {
			struct ip6pseudo tap_ph6;
			struct tcp_ao_thopt tap_th;
		} tap_ip6;
	} tap;
	int tap_type;
	int tap_len;
} __packed;
#define tap4	tap.tap_ip
#define tap6	tap.tap_ip6

static int	tcp_ao_mac(struct in_conninfo *inc, struct tcphdr *th,
    struct tcpopt *to, struct mbuf *m, int tlen, uint32_t sne, int algo,
    uint8_t *key, uint8_t *hash, int dir);
static int	tcp_ao_sha1(uint8_t *key, struct tcp_ao_pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t *hash);
static int	tcp_ao_cmac(uint8_t *key, struct tcp_ao_pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t *hash);
static int	tcp_ao_md5(uint8_t *key, struct tcp_ao_pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t *hash);

/*
 * Computation the authentication hash and return the result of the hash
 * comparison.  Return values:
 *  0     = success
 *  EAUTH = authentication failed
 *  other = authentication failed
 */
static int
tcp_ao_mac(struct in_conninfo *inc, struct tcphdr *th, struct tcpopt *to,
    struct mbuf *m, int tlen, uint32_t sne, int algo, uint8_t *key,
    uint8_t *hash, int dir)
{
	int moff, mlen, thlen;
	struct tcp_ao_pseudo ph;
	struct tcp_ao_thopt *tho;
	int error;

	ph.tap_sne = sne;

	/* Fill in tcpheader including options. */
	thlen = th->th_off << 2;

	/* Fill in pseudo headers. */
	switch(inc->inc_flags & INC_ISIPV6) {
	case 0:
		tho = &ph.tap4.tap_th;
		if (dir == TCP_AO_OUT) {
			ph.tap4.tap_ph4.ippseudo_src = inc->inc_laddr;
			ph.tap4.tap_ph4.ippseudo_dst = inc->inc_faddr;
			tho->th.th_sport = htons(th->th_sport);
			tho->th.th_dport = htons(th->th_dport);
		} else {
			ph.tap4.tap_ph4.ippseudo_src = inc->inc_faddr;
			ph.tap4.tap_ph4.ippseudo_dst = inc->inc_laddr;
			tho->th.th_sport = htons(th->th_dport);
			tho->th.th_dport = htons(th->th_sport);
		}
		ph.tap4.tap_ph4.ippseudo_pad = 0;
		ph.tap4.tap_ph4.ippseudo_p = IPPROTO_TCP;
		ph.tap4.tap_ph4.ippseudo_len = htons(tlen);
		ph.tap_len += sizeof(ph.tap4.tap_ph4);
		break;
	case INC_ISIPV6:
		tho = &ph.tap6.tap_th;
		if (dir == TCP_AO_OUT) {
			ph.tap6.tap_ph6.ip6pseudo_src = inc->inc6_laddr;
			ph.tap6.tap_ph6.ip6pseudo_dst = inc->inc6_faddr;
			tho->th.th_sport = htons(th->th_sport);
			tho->th.th_dport = htons(th->th_dport);
		} else {
			ph.tap6.tap_ph6.ip6pseudo_src = inc->inc6_faddr;
			ph.tap6.tap_ph6.ip6pseudo_dst = inc->inc6_laddr;
			tho->th.th_sport = htons(th->th_dport);
			tho->th.th_dport = htons(th->th_sport);
		}
		ph.tap6.tap_ph6.ip6pseudo_len = htons(tlen);
		ph.tap6.tap_ph6.ip6pseudo_pad = 0;
		ph.tap6.tap_ph6.ip6pseudo_p = IPPROTO_TCP;
		ph.tap_len += sizeof(ph.tap6.tap_ph6);
		break;
	default:
		error = EINVAL;
		goto out;
	}
	ph.tap_len += sizeof(ph.tap_sne);

//	bcopy(th, tho, thlen);
	tho->th.th_sum = 0;	/* Zero out checksum */
	bzero(tho->tho + (to->to_signature - (u_char *)(th + 1)), to->to_siglen);
	tho->th.th_seq = htonl(th->th_seq);
	tho->th.th_ack = htonl(th->th_ack);
	tho->th.th_win = htons(th->th_win);
	tho->th.th_urp = htons(th->th_urp);

	/* Set up the mbuf length fields. */
	moff = thlen;
	mlen = m_length(m, NULL) - thlen;

	switch(algo) {
	case TAO_ALGO_HMAC_SHA_1_96:
		error = tcp_ao_sha1(key, &ph, m, moff, mlen, hash);
		break;
	case TAO_ALGO_AES_128_CMAC_96:
		error = tcp_ao_cmac(key, &ph, m, moff, mlen, hash);
		break;
	case TAO_ALGO_MD5SIG:
		error = tcp_ao_md5(key, &ph, m, moff, mlen, hash);
		break;
	default:
		error = EINVAL;
		goto out;
	}
	if (error)
		goto out;

out:
	return (error);
}

/*
 * We have three cases to verify:
 *  a) LISTEN;
 *	- SYN; create RX and TX traffic keys on the fly;
 *	- ACK; create and store RX and TX traffic keys;
 *  b) SYN SENT; create traffic TX traffic key;
 *	- SYN; create TX traffic key;
 *	- SYN/ACK; create RX traffic key;
 *  c) ESTABLISHED; verify traffic;
 *
 * We have two cases to compute a signature:
 *  a) syncache; on the fly with traffic key derivation;
 *  b) established; directly use traffic key;
 */
int
tcp_ao_sc_findmkey(struct tcpcb *tp, struct in_conninfo *inc, struct tcpopt *to,
    struct tcp_ao_key *tkey)
{
	struct tcp_ao_cb *tac;
	struct tcp_ao_peer *tap;
	struct tcp_ao_key *tak;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr sa;
	uint8_t idx = 0;

	tac = tp->t_ao;

	switch (inc->inc_flags & INC_ISIPV6) {
	case 0:
		sin = (struct sockaddr_in *)&sa;
		sin->sin_len = sizeof(struct sockaddr_in);
		sin->sin_family = AF_INET;
		sin->sin_port = 0;
		sin->sin_addr = inc->inc_faddr;
		break;
	case INC_ISIPV6:
		sin6 = (struct sockaddr_in6 *)&sa;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = 0;
		sin6->sin6_addr = inc->inc6_faddr;
		break;
	default:
		return (EINVAL);
		break;
	}

	if ((tap = tcp_ao_peer_find(tac, &sa)) == NULL)
		return (EINVAL);

	if ((tak = tcp_ao_key_find(tap, idx)) == NULL)
		return (EINVAL);

	bcopy(tak, tkey, sizeof(struct tcp_ao_key) + tak->keylen);

	return (0);
}

int
tcp_ao_sc_verify(struct tcp_ao_key *tak, struct in_conninfo *inc,
    struct tcphdr *th, struct tcpopt *to, struct mbuf *m, int tlen)
{
	int error;
	uint8_t hash[100];

	/* use master key, derive RX traffic key, and compare. */
	error = tcp_ao_kdf(tak, inc, th, hash, 100, 0);
	if (error)
		goto out;

	error = tcp_ao_mac(inc, th, to, m, tlen, tak->keyalgo, 0,
	    (uint8_t *)&hash, hash, 0);
	if (error)
		goto out;

	if (bcmp(hash, to->to_signature, to->to_siglen))
		error = EAUTH;
out:
	return (error);
}

int
tcp_ao_sc_hash(struct tcp_ao_key *tak, struct in_conninfo *inc,
    struct tcphdr *th, struct tcpopt *to, struct mbuf *m, int tlen)
{
	int error;
	uint8_t hash[100];

	/* use master key to derive TX traffic key and put into segment. */
	error = tcp_ao_kdf(tak, inc, th, hash, 100, 1);
	if (error)
		goto out;

	error = tcp_ao_mac(inc, th, to, m, tlen, tak->keyalgo, 0,
	    (uint8_t *)&hash, hash, 1);
	if (error)
		goto out;

	bcopy(hash, to->to_signature, to->to_siglen);
out:
	return (error);
}

int
tcp_ao_sc_copy(struct tcpcb *ltp, struct tcpcb *tp, struct in_conninfo *inc)
{
	int error;

	/* allocate a tcp_ao_cb and copy this peer's structure and keys */
	error = tcp_ao_peer_clone(ltp, tp, NULL);

	return (error);
}

int
tcp_ao_est_verify(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to,
    struct mbuf *m, int tlen)
{
	struct tcp_ao_cb *tac;
	int siglen;
	int error;
	uint8_t hash[100];

	tac = tp->t_ao;

	switch (tac->tac_algo) {
	case TAO_ALGO_HMAC_SHA_1_96:
		siglen = TAO_ALGO_HMAC_SHA_1_96_LEN;
		break;
	case TAO_ALGO_AES_128_CMAC_96:
		siglen = TAO_ALGO_AES_128_CMAC_96_LEN;
		break;
	case TAO_ALGO_MD5SIG:
		siglen = TAO_ALGO_MD5SIG;
		break;
	default:
		error = EAUTH;
		goto out;
	}
	if (to->to_siglen != siglen) {
		error = EAUTH;
		goto out;
	}

	/* use RX traffic key and compare. */
	error = tcp_ao_mac(&tp->t_inpcb->inp_inc, th, to, m, tlen,
	    tac->tac_algo, tac->tac_rcvsne, (uint8_t *)&tac->tac_rkey,
	    hash, 0);
	if (error)
		goto out;

	if (bcmp(hash, to->to_signature, to->to_siglen))
		error = EAUTH;
out:
	return (error);
}

/*
 * Prepare and compute hash for outbound segments.
 */
int
tcp_ao_est_hash(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to,
    struct mbuf *m, int tlen)
{
	struct tcp_ao_cb *tac;
	int error;
	uint8_t hash[100];

	tac = tp->t_ao;

	error = tcp_ao_mac(&tp->t_inpcb->inp_inc, th, to, m, tlen,
	    tac->tac_algo, tac->tac_sndsne, (uint8_t *)&tac->tac_skey,
	    hash, 1);

	if (!error)
		bcopy(hash, to->to_signature, to->to_siglen);

	return (error);
}

int
tcp_ao_est_opt(struct tcpcb *tp, struct tcpopt *to)
{
	struct tcp_ao_cb *c = tp->t_ao;
	struct tcp_ao_peer *p;
	int siglen;

	switch (c->tac_algo) {
	case TAO_ALGO_HMAC_SHA_1_96:
		siglen = TAO_ALGO_HMAC_SHA_1_96_LEN;
		break;
	case TAO_ALGO_AES_128_CMAC_96:
		siglen = TAO_ALGO_AES_128_CMAC_96_LEN;
		break;
	case TAO_ALGO_MD5SIG:
		siglen = TAO_ALGO_MD5SIG;
		break;
	default:
		siglen = 12;	/* XXX */
		break;
	}
	to->to_siglen = siglen;

	p = LIST_FIRST(&c->tac_peers);

	to->to_ao_keyid = p->tap_activekey;
	to->to_ao_nextkeyid = p->tap_nextkey;

	return (0);
}

/*
 * Note: Can't use cryptodev because of callback based non-inline
 * processing.  Also complexity to set up a crypto session is too high
 * and requires a couple of malloc's.
 */

/*
 * Compute RFC5925+RFC5926 compliant HMAC-SHA1 authentication MAC of
 * a tcp segment.
 */
static int
HMAC_SHA1_Update_x(void *ctx, void *data, u_int len)
{

	HMAC_SHA1_Update(ctx, data, len);
	return (0);
}

static int
tcp_ao_sha1(uint8_t *key, struct tcp_ao_pseudo *ph, struct mbuf *m,
    int moff, int mlen, uint8_t *hash)
{
	HMAC_SHA1_CTX ctx;
	int error = 0;

	HMAC_SHA1_Init(&ctx, key, SHA1_BLOCK_LENGTH);

	/* Pseudo header. */
	HMAC_SHA1_Update(&ctx, (uint8_t *)ph, ph->tap_len);

	error = m_apply(m, moff, mlen, HMAC_SHA1_Update_x, &ctx);
	if (error)
		goto out;

	HMAC_SHA1_Final(hash, &ctx);
out:
	bzero(&ctx, sizeof(ctx));
	return (error);
}

/*
 * Compute RFC5925+RFC5926 compliant AES-128-CMAC authentication MAC of
 * a tcp segment.
 */
static int
AES_CMAC_Update_x(void *ctx, void *data, u_int len)
{

	AES_CMAC_Update_x(ctx, data, len);
	return (0);
}

static int
tcp_ao_cmac(uint8_t *key, struct tcp_ao_pseudo *ph, struct mbuf *m,
    int moff, int mlen, uint8_t *hash)
{
	AES_CMAC_CTX ctx;
	int error = 0;

	AES_CMAC_Init(&ctx);
	AES_CMAC_SetKey(&ctx, key);

	AES_CMAC_Update(&ctx, (uint8_t *)ph, ph->tap_len);

	error = m_apply(m, moff, mlen, AES_CMAC_Update_x, &ctx);
	if (error)
		goto out;

	AES_CMAC_Final(hash, &ctx);
out:
	bzero(&ctx, sizeof(ctx));
	return (error);
}

/*
 * Compute RFC2385 compliant MD5 authentication MAC of a tcp segment.
 * Note that the SNE does not apply, the key comes last and the tcp options
 * are not included.
 */
static int
MD5Update_x(void *ctx, void *data, u_int len)
{

	MD5Update(ctx, data, len);
	return (0);
}

static int
tcp_ao_md5(uint8_t *key, struct tcp_ao_pseudo *ph, struct mbuf *m,
    int moff, int mlen, uint8_t hash[static MD5_DIGEST_LENGTH])
{
	MD5_CTX ctx;
	int error = 0, len;

	MD5Init(&ctx);

	len = ph->tap_len - sizeof(ph->tap_sne) - sizeof(struct tcp_ao_thopt);
	len += sizeof(struct tcphdr);
	MD5Update(&ctx, &ph->tap, len);

	error = m_apply(m, moff, mlen, MD5Update_x, &ctx);
	if (error)
		goto out;

	MD5Update(&ctx, key, MD5_BLOCK_LENGTH);

	MD5Final(hash, &ctx);
out:
	bzero(&ctx, sizeof(ctx));
	return (error);
}

