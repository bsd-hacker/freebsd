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
 * TODO:
 *  all of the above.
 *
 * Discussion:
 *  the key management can be done in two ways: via the ipsec key interface
 *  or through the setsockopt() api.  Analyse which one is better to handle
 *  in the kernel and for userspace applications.  The setsockopt() API is
 *  the winner and will be used.
 *
 *  legacy tcp-md5 can be brought and integrated into the tcp-ao framework.
 */

/*
 * The code below is skeleton code and not functional yet.
 */

MALLOC_DEFINE(M_TCPAO, "tcp_ao", "TCP-AO peer and key structures");

int
tcp_ao_ctl(struct tcpcb *tp, struct tcp_ao_sopt *tao, int tao_len)
{
	srtuct tcp_ao_cb *c;
	struct tcp_ao_peer *p;
	struct tcp_ao_key *k;
	int error;

	switch (tao->tao_peer.sa_family) {
	case AF_INET:
		if (tao->tao_peer.sa_len != sizeof(struct sockaddr_in))
			error = EINVAL;
		break;
	case AF_INET6:
		if (tao->tao_peer.sa_len != sizeof(struct sockaddr_in6))
			error = EINVAL;
		break;
	default:
		error = EINVAL;
	}
	if (error)
		goto out;

	c = tp->t_ao;

	switch (tao->tao_cmd) {
	case TAO_CMD_ADD:
		switch (tao->tao_algo) {
		case TAO_ALGO_MD5SIG:
		case TAO_ALGO_HMAC-SHA-1-96:
		case TAO_ALGO_AES-128-CMAC-96:
			break;
		default:
			error = EINVAL;
			goto out;
		}

		/* Insert or overwrite */
		if ((p = tcp_ao_peer_add(c, tao)) == NULL) {
			error = EINVAL;
			break;
		}
		if (tp->t_state > TCPS_LISTEN &&
		    p->tap_activekey == tao->tao_keyidx) {
			error = EINVAL;
			break;
		}
		if ((k = tcp_ao_key_add(p, tao, taolen - sizeof(*tao)) == NULL) {
			error = EINVAL;
		}
		break;

	case TAO_CMD_DELIDX:
		/* Can't remove active index */
		if ((p = tcp_ao_peer_find(c, tao)) == NULL) {
			error = EINVAL;
			break;
		}
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
		if (tp->t_state > TCPS_LISTEN)
			break;

		if ((p = tcp_ao_peer_find(c, tao)) == NULL) {
			error = EINVAL;
			break;
		}
		tcp_ao_peer_del(c, p);
		break;

	case TAO_CMD_FLUSH:
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

struct tcp_ao_peer *
tcp_ao_peer_find(struct tcp_ao_cb *tac, struct sockaddr *sa)
{
	struct tcp_ao_peer *p;

	LIST_FOREACH(p, tac->tac_peers, tap_entry) {
		if (p->tap_peer.sa_family == sa->sa_family &&
		    !bcmp(p->tap_peer.sa_data, sa->sa_data,
			min(p->tap_peer.sa.len, sa->sa_len)))
			return (p);
	}
	return (NULL);
}

struct tcp_ao_peer *
tcp_ao_peer_add(struct tcp_ao_cb *tac, struct tcp_ao_sopt *tao)
{
	struct tcp_ao_peer *p;

	if ((p = tcp_ao_peer_find(tac, tao->tao_peer)) == NULL) {
		if ((p = malloc(sizeof(*p), M_TCPAO, M_NOWAIT)) == NULL)
			return (p);
		p->tap_flags = 0;
		bcopy(tao->tao_peer, p->tac_peer, tao->tao_peer.sa_len);
		SLIST_INIT(&p->tak_keys);
	}

	LIST_INSERT_HEAD(&tap->tap_peers, p);
	return (p);
}

int
tcp_ao_peer_del(struct tcp_ao_cb *tac, struct tcp_ao_peer *tap)
{

	tcp_ao_key_flush(tap);
	LIST_REMOVE(tap, tap_list);
	free(tap, M_TCPAO);
	return (0);
}

void
tcp_ao_peer_flush(struct tcp_ao_cb *tac)
{
	struct tcp_ao_peer *p, *p2;

	SLIST_FOREACH_SAFE(p, tac->tac_peers, entries, p2) {
		tcp_ao_key_flush(p);
		free(p, M_TCPAO);
	}
	LIST_INIT(&tac->tac_peers);
}

struct tcp_ao_key *
tcp_ao_key_find(struct tcp_ao_peer *tap, uint8_t idx)
{
	struct tcp_ao_key *k;

	SLIST_FOREACH(k, &tap->tap_keys, entry) {
		if (k->keyidx == idx)
			return (k);
	}
	return (NULL);
}

struct tcp_ao_key *
tcp_ao_key_add(struct tcp_ao_peer *tap, struct tcp_so_sopt *tao, uint8_t keylen)
{
	struct tcp_ao_key *k;

	if ((k = tcp_ao_key_find(tap, tao->tao_keyidx)) != NULL) {
		SLIST_REMOVE(&tap->tap_keys, k, entry, entry);
		free(k, M_TCPAO);
	}
	if ((k = malloc(sizeof(*k) + keylen, M_TCPAO, M_NOWAIT)) == NULL)
		return (k);

	k->keyidx = tao->tao_keyidx;
	k->keyflags = 0;
	k->keyalgo = tao->tao_keyalgo;
	k->keylen = keylen;
	bcopy(tao->tao_key, k->key, k->keylen);

	SLIST_INSERT_HEAD(&tap->tap_keys, k);
	return (k);
}

int
struct tcp_ao_key_del(struct tcp_ao_peer *tap, uint8_t keyidx)
{
	struct tcp_ao_key *k, *k2;

	SLIST_FOREACH_SAFE(k, &tap->tap_keys, entries, k2) {
		if (k->keyidx == keyidx) {
			SLIST_REMOVE(&tap->tap_keys, entries, entries);
			free(k, M_TCPAO);
			return (0);
		}
	}
	return (ENOENT);
}

void
tcp_ao_key_flush(struct tcp_ao_peer *tap)
{
	struct tcp_ao_key *k, *k2;

	SLIST_FOREACH_SAFE(k, &tap->tap_keys, entries, k2)
		free(k, M_TCPAO);
	SLIST_INIT(&tap->tap_keys);
}

int
tcp_ao_peer_clone(struct tcpcb *tp1, struct tcpcb *tp2, struct sockaddr *sa)
{
	struct tcp_ao_peer *p1, *p2;
	struct tcp_ao_key *k1, *k2;

	if ((p1 = tcp_ao_peer_find(tp1->t_ao, sa)) == NULL)
		return (0);

	if (tcp_ao_cb_alloc(tp2) != 0)
		return (ENOMEM);

	bcopy(p1, p2, malloc(sizeof(*p2));
	SLIST_INIT(&p2->tak_keys);
	SLIST_FOREACH(k1, &p1->tap_keys, entry) {
		if ((k2 = malloc(sizeof(*k2) + k1->keylen, M_TCPAO, M_NOWAIT)) == NULL)
			return (ENOMEM);
		bcopy(k1, k2, k1->keylen);
		SLIST_INSERT_HEAD(&p2->tap_keys, k2);
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
union tcp_ao_kdf_ctx {
	struct ip4 {
		struct in_addr src, dst;
		uint16_t sport, dport;
		uint32_t irs, iss;
	} ip4_ctx;
	struct ip6 {
		struct in6_addr src, dst;
		uint16_t sport, dport;
		uint32_t irs, iss;
	} ip6_ctx;
	int len;
};

/*
 * Key derivation for sessions and the derived master keys.
 * Return values:
 *  0      = success
 *  EINVAL = invalid input, typically insufficient length
 *  other  = key derivation failed
 */
static int
tcp_ao_kdf(struct in_conninfo *inc, struct tcphdr *th, uint8_t *out,
    int outlen)
{
	int error = 0;
	struct tcp_ao_kdf_ctx tak;

	/* Fill in context for traffic keys. */
	switch (inc->inc_flags & INC_ISIPV6) {
	case 0:
		tak.ip4_ctx.src = inc->ie_faddr;
		tak.ip4_ctx.dst = inc->ie_laddr;
		tak.ip4_ctx.irs = htonl(th->th_ack);
		tak.ip4_ctx.iss = htonl(th->th_seq);
		tak.len = sizeof(tak.ip4_ctx);
		break;
	case INC_ISIPV6:
		tak.ip6_ctx.src = inc->ie6_faddr;
		tak.ip6_ctx.dst = inc->ie6_laddr;
		tak.ip6_ctx.irs = htonl(th->th_ack);
		tak.ip6_ctx.iss = htonl(th->th_seq);
		tak.len = sizeof(tak.ip6_ctx);
		break;
	default:
		error = EINVAL;
		goto out;
	}

	switch (kdf) {
	case TCP_AO_HMAC_SHA_1_96:
		error = tcp_ao_kdf_hmac(key, keylen, out, outlen, &tak);
		break;
	case TCP_AO_AES_128_CMAC_96:
		error = tcp_ao_kdf_cmac(key, keylen, out, outlen, &tak);
		break;
	case TCP_AO_TCPMD5:
		error = tcp_ao_kdf_cmac(key, keylen, out, outlen, &tak);
		break;
	default:
		error = EINVAL;
	}
	if (error)
		goto out;

	return (error);
}

static int
tcp_ao_kdf_hmac(uint8_t *key, int keylen , uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tak)
{
	HMAC_SHA_CTX ctx;
	uint8_t res[SHA1_DIGEST_LENGTH];
	char *label = "TCP-AO";
	int error = 0;
	uint8_t i;

	for (i = 0; outlen > 0; outlen -= SHA1_DIGEST_LENGTH, i++) {
		HMAC_SHA1_Init(&ctx, key, keylen);

		HMAC_SHA1_Update(&ctx, &i, sizeof(i));
		HMAC_SHA1_Update(&ctx, label, sizeof(*label));
		if (tak != NULL)
			HMAC_SHA1_Update(&ctx, tak, tak->len);
		HMAC_SHA1_Final(res, &ctx);

		bcopy(res, out, min(outlen, SHA1_DIGEST_LENGTH));
		out += SHA1_DIGEST_LENGTH;
	}
	return (error);
}

static int
tcp_ao_kdf_cmac(uint8_t *key, int keylen, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tak)
{
	AES_CMAC_CTX ctx;
	uint8_t res[AES_CMAC_DIGEST_LENGTH];
	uint8_t zero[AES_CMAC_KEY_LENGTH];
	int error = 0;

	if (tak == NULL) {
		bzero(zero, sizeof(*zero));
		AES_CMAC_Init(&ctx);
		AES_CMAC_SetKey(&ctx, zero);
		AES_CMAC_Update(&ctx, key, keylen);
		AES_CMAC_Final(res, &ctx);
		bcopy(res, out, min(outlen, AES_CMAC_DIGEST_LENGTH));
	}

	if (keylen != AES_CMAC_KEY_LENGTH)
		return (EINVAL);

	for (i = 0; outlen > 0; outlen -= AES_CMAC_DIGEST_LENGTH, i++) {
		AES_CMAC_Init(&ctx);
		AES_CMAC_SetKey(&ctx, key);
		AES_CMAC_Update(&ctx, &i, sizeof(i));
		AES_CMAC_Update(&ctx, label, sizeof(*label));
		AES_CMAC_Update(&ctx, tak, tak->len);
		AES_CMAC_Final(res, &ctx);

		bcopy(res, out, min(outlen, AES_CMAC_DIGEST_LENGTH));
		out += AES_CMAC_DIGEST_LENGTH;
	}
	return (error);
}

static int
tcp_ao_kfd_md5(uint8_t *key, int keylen, uint8_t *out, int outlen,
    struct tcp_ao_kdf_ctx *tak)
{
	/* XXX: No key derivation happens. */
	return (0);
}


/*
 * The hash over the header has to be pieced together and a couple of
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
			struct ip6_phdr tap_ph6;
			struct tcp_ao_thopt tap_th;
		} tap_ip6;
	} tap;
	int tap_type;
	int tap_len;
} __packed;
#define tap4	tap.tap_ip
#define tap6	tap.tap_ip6

/* Convenient functions not yet in existence. */
ip_hdr2pseudo(struct ip *ip, struct ippseudo *ipp);
ip6_hdr2pseudo(struct ip6_hdr *ip6, struct ip6pseudo *ipp6);
ip_inc2pseudo(struct in_conninfo *inc, struct ippseudo *ipp);
ip6_inc2pseudo(struct in_conninfo *inc, struct ip6pseudo *ipp6);

/*
 * Computation the authentication hash and return the result of the hash
 * comparison.  Return values:
 *  0     = success
 *  EAUTH = authentication failed
 *  other = authentication failed
 */
static int
tcp_ao_mac(struct tcpcb *tp, struct tcp_ao_key *tk, struct in_conninfo *inc,
    struct tcphdr *th, struct tcpopt *to, struct mbuf *m)
{
	int moff, mlen, thlen;
	struct tcp_ao_pseudo ph;
	struct tcp_ao_thopt *tho;
	uint8_t hash[MAXHASHLEN];

	/*
	 * Set up the virtual sequence number extension that is part of
	 * the authentication hash.
	 */
	if (tp != NULL)
		ph.tap_sne = tp->t_ao->tao_sne;
	else
		ph.tap_sne = 0;

	/* Fill in pseudo headers. */
	switch(inc->inc_flags & INC_ISIPV6) {
	case 0:
		ip_inc2pseudo(inc, &ph.tap4.tap_ph4);
		ph.tap_len += sizeof(ph.tap4.tap_ph4);
		tho = &ph.tap4.tap_th;
		break;
	case INC_ISIPV6:
		ip6_hdr2pseudo(inc, &ph.tap6.tap_ph6);
		ph.tap_len += sizeof(ph.tap6.tap_ph6);
		tho = &ph.tap6.tap_th;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	ph.tap_len += sizeof(ph.tap_sne);

	/* Fill in tcpheader including options. */
	thlen = th->th_off << 2;
	bcopy(th, tho, thlen);
	ph.tap_len += thlen;

	/* Zero out checksum and mac field and swap to network byte order. */
	tho->th.th_sum = 0;
	bzero(&tho->tho + (to->to_signature - (th + 1)), to->to_siglen);
	tcp_fields_to_net(&tho);

	/* Set up the mbuf length fields. */
	moff = thlen;
	mlen = m_length(m, NULL) - thlen;

	switch(tk->algo) {
	case TCP_AO_HMAC_SHA_1_96:
		error = tcp_ao_sha1(tk->key, ph, m, moff, mlen, hash);
		break;
	case TCP_AO_AES_128_CMAC_96:
		error = tcp_ao_cmac(tk->key, ph, m, moff, mlen, hash);
		break;
	case TCP_AO_TCPMD5:
		error = tcp_ao_md5(tk->key, ph, m, moff, mlen, hash);
		break;
	default:
		error = EINVAL;
		goto out;
	}
	if (error)
		goto out;

	/* Compare result to segment signature. */
	if (bcmp(hash, to->to_signature, tk->tk_hashlen));
		error = EAUTH;

out:
	return (error);
}

/*
 * Note: Can't use cryptodev because of callback based non-inline
 * processing.  Also complexity to set up a crypto session is too high
 * and requires a couple of malloc's.
 */

/*
 * Compute RFC5925+RFC5926 compliant HMAC-SHA1 authentication MAC of
 * a tcp segment.
 * XXX: HMAC_SHA1 doesn't exist yet.
 */
static int
tcp_ao_sha1(uint32_t key[static SHA1_BLOCK_LENGTH], struct pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t hash[static SHA1_RESULTLEN])
{
	HMAC_SHA1_CTX ctx;
	int error = 0;

	HMAC_SHA1_Init(&ctx, key, SHA1_BLOCK_LENGTH);

	/* Pseudo header. */
	HMAC_SHA1_Update(&ctx, ph, ph->tap_len);

	error = m_apply(m, moff, mlen, HMAC_SHA1_Update, &ctx);
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
tcp_ao_cmac(uint32_t key[static AES_CMAC_KEY_LENGTH], struct pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t hash[static AES_CMAC_DIGEST_LENGTH])
{
	AES_CMAC_CTX ctx;
	int error = 0;

	AES_CMAC_Init(&ctx);
	AES_CMAC_SetKey(&ctx, key);

	AES_CMAC_Update(&ctx, ph, ph->tap_len);

	error = m_apply(m, moff, mlen, AES_CMAC_Update, &ctx);
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
tcp_ao_md5(uint32_t key[static MD5_BLOCK_LENGTH], struct pseudo *ph,
    struct mbuf *m, int moff, int mlen, uint8_t hash[static MD5_DIGEST_LENGTH])
{
	MD5_CTX ctx;
	int error = 0, len;

	MD5Init(&ctx);

	len = ph->tap_len - sizeof(*ph->tap_sne) - sizeof(struct tcp_ao_thopt);
	len += sizeof(struct tcphdr);
	MD5Update(&ctx, &ph->tap, len;

	error = m_apply(m, moff, mlen, AES_CMAC_Update, &ctx);
	if (error)
		goto out;

	MD5Update(&ctx, key, MD5_BLOCK_LENGTH);

	MD5Final(hash, &ctx);
out:
	bzero(%ctx, sizeof(ctx));
	return (error);
}

