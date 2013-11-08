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
 * TCP-AO key interface through socket options.
 *
 * To set one or more keys for one or more peers:
 *  setsockopt(so, IPPROTO_TCP, TCP_AO, tcp_ao_sopt, sizeof(*tcp_ao_sopt));
 *
 * An arbitrary number of keys can be specified on an unconnected or listen
 * socket.  The keys can be added, changed or removed at any time.  Once an
 * application has installed at least one key, TCP-AO is enabled on that
 * socket for the specified peer.
 *
 * A listen socket searches for a matching key when it receives a SYN.
 * After the 3WHS is completed a socket is created for the new connection.
 * This socket inherits only the keys relevant to this peer address.
 * 
 * On a connect all keys except those belonging to that peer are removed.
 *
 * If a key that is in active use is changed, packet loss may result.
 *
 * Keys are not shared between sockets.  Adding and removing keys has to be
 * done on each socket where the peer address applies.  This is not much
 * overhead to the application and greatly simplifies the kernel implementation.
 *
 * Since applications tend to pass the key string unmodified it may be better
 * to specify the socket interface to be in base64 instead of an array of
 * uint8_t.  That would allow a human readable string to represent more bit
 * variance per byte, though the overall entropy doesn't change for a given
 * key length.
 *
 * The active key index on a connected socket can be retrieved as follows:
 *  getsockopt(so, IPPROTO_TCP, TCP_AO, int, sizeof(int));
 *
 * This interface may continue to evolve as the implementation matures and
 * handling experience is gained.  These structs should be moved to tcp.h
 * once stable.
 */

#ifndef _TCP_AO_H_
#define _TCP_AO_H_

MALLOC_DECLARE(M_TCPAO);

#include <sys/md5.h>
#include <crypto/sha1.h>
#include <crypto/cmac/cmac.h>
#include <crypto/hmac/hmac.h>

/*
 * TCP-AO key interface struct passed to setsockopt().
 * Per peer structures referenced from tcp_ao_sopt.
 * The commands normally apply to a particular keyidx and peer combination.
 */
struct tcp_ao_sopt {
	uint16_t	tao_cmd;		/* command, add, remove key */
	uint16_t	tao_flags;		/* flags */
	uint8_t		tao_keyidx;		/* key index per peer */
	uint8_t		tao_algo;		/* MAC algorithm */
	struct sockaddr_storage
			tao_peer;		/* this key applies to ... */
	uint8_t		tao_key[];		/* base64 key string */
};
#define TAO_KEY_MAXLEN			128

/*
 * Commands for the tao_cmd field.
 */
#define TAO_CMD_ADD			1	/* add or replace key */
#define TAO_CMD_DELIDX			2	/* delete keyidx|peer */
#define TAO_CMD_DELPEER			3	/* delete all idx for peer */
#define TAO_CMD_FLUSH			4	/* delete all keys */

/*
 * Flags for the tao_flags field.
 */
#define	TAO_FLAGS_ACTIVE		0x0001	/* active key index for SYN */

/*
 * MAC and KDF pairs for the tao_algo field.
 */
#define TAO_ALGO_MD5SIG			1	/* legacy compatibility */
#define TAO_ALGO_HMAC_SHA_1_96		2	/* RFC5926, Section 2.2 */
#define TAO_ALGO_AES_128_CMAC_96	3	/* RFC5926, Section 2.2 */

#define	TAO_ALGO_MD5SIG_LEN		16
#define	TAO_ALGO_HMAC_SHA_1_96_LEN	12
#define	TAO_ALGO_AES_128_CMAC_96_LEN	12

#ifdef _KERNEL
#define	TCP_AO_MAXHASH			20	/* max raw hash length */

/*
 * In kernel storage of the key information.
 * The active session key is stored in tac_skey and tac_rkey.
 */
struct tcp_ao_cb {
	LIST_HEAD(tac_peer, tcp_ao_peer) tac_peers;
	int tac_algo;
	uint32_t tac_sndsne, tac_rcvsne;
	/* Pre-computed traffic keys for established sessions. */
	union {
		uint8_t md5[MD5_DIGEST_LENGTH];
		uint8_t hmac[SHA1_DIGEST_LENGTH];
		uint8_t cmac[AES_CMAC_DIGEST_LENGTH];
	} tac_skey, tac_rkey;
};

/*
 * Per peer information (remote IP).
 * A control block can have N peers in LISTEN mode.
 * A control block can have 1 peer in SYN_SENT or SYN_RECEIVED mode.
 */
struct tcp_ao_peer {
	LIST_ENTRY(tcp_ao_peer)	tap_entry;
	uint16_t tap_flags;
	uint8_t tap_activekey;
	uint8_t tap_nextkey;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin4;
		struct sockaddr_in6 sin6;
	} tap_peer;
	SLIST_HEAD(tap_key, tcp_ao_key) tap_keys;
};

/*
 * Per key information.  A peer can have N keys, of which one is active.
 */
struct tcp_ao_key {
	SLIST_ENTRY(tcp_ao_key) entry;
	uint8_t keyidx;
	uint8_t keyflags;
	uint8_t keyalgo;
	uint8_t keylen;
	uint8_t key[];
};

#define	TCP_AO_IN	0
#define	TCP_AO_OUT	1

int	tcp_ao_ctl(struct tcpcb *tp, struct tcp_ao_sopt *tao, int tao_len);

int	tcp_ao_sc_findmkey(struct tcpcb *tp, struct in_conninfo *inc,
	    struct tcpopt *to, struct tcp_ao_key *tkey);
int	tcp_ao_sc_verify(struct tcp_ao_key *tak, struct in_conninfo *inc,
	    struct tcphdr *th, struct tcpopt *to, struct mbuf *m, int tlen);
int	tcp_ao_sc_hash(struct tcp_ao_key *tak, struct in_conninfo *inc,
	    struct tcphdr *th, struct tcpopt *to, struct mbuf *m, int tlen);
int	tcp_ao_sc_copy(struct tcpcb *ltp, struct tcpcb *tp,
	    struct in_conninfo *inc);

int	tcp_ao_est_verify(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to,
	    struct mbuf *m, int tlen);
int	tcp_ao_est_hash(struct tcpcb *tp, struct tcphdr *th, struct tcpopt *to,
	    struct mbuf *m, int tlen);
int	tcp_ao_est_opt(struct tcpcb *tp, struct tcpopt *to);

void	tcp_ao_cb_free(struct tcpcb *tp);

#endif /* _KERNEL */
#endif
