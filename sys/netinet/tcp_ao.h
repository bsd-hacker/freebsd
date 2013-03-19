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
 * If a key that is changed that is in active use, packet loss may result.
 *
 * Keys are not shared between sockets.  Adding and removing keys has to be
 * done on each socket where the peer address applies.  This is not much
 * overhead to the application and greatly simplifies the kernel implementation.
 *
 * Since applications tend to pass the key string unmodified it may be better
 * to specify the socket interface to be in base64 instead of an array of
 * uint8_t.  That would allow a human readable string to represent more bit
 * variance per byte.
 *
 * Configured keys on a socket can be retrieved as follows:
 *  getsockopt(so, IPPROTO_TCP, TCP_AO, tcp_ao_sopt, sizeof(*tcp_ao_sopt));
 *
 * All configured peers and key indexs are returned in the supplied vector.
 * If the vector is too small the result is truncated.  The number of keys
 * is returned in tao_keycnt.  No actual keys are returned or exposed.
 *
 * This interface may continue to evolve as the implementation matures and
 * handling experience is gained.  These structs should be moved to tcp.h
 * once stable.
 */

/*
 * TCP-AO key interface struct passed to setsockopt().
 */
struct tcp_ao_sopt {
	int		 tao_flags;		/* flags for this operation */
	int		 tao_keycnt;		/* number of keys in vector */
	struct tcp_ao_key *tao_keyv;		/* pointer to key vector */
};

/*
 * Flags for the tao_flags field.
 */
#define TAO_SOPT_REPLACE	0x00000001	/* replace full set */

/*
 * Per peer structures referenced from tcp_ao_sopt.
 * The commands normally apply to a particular keyidx and peer combination.
 */
struct tcp_ao_key {
	uint8_t		 taok_cmd;		/* command, add, remove key */
	uint8_t		 taok_flags;		/* flags for key */
	uint8_t		 taok_algo;		/* MAC algorithm */
	uint8_t		 taok_keyidx;		/* key index per peer */
	int		 taok_keylen;		/* length of key */
	uint8_t		*taok_key;		/* key string */
	struct sockaddr	*taok_peer;		/* this key applies to ... */
};

/*
 * Commands for the taok_cmd field.
 */
#define TAOK_CMD_ADD			1	/* add or replace key */
#define TAOK_CMD_DELETE			2	/* delete key keyidx|peer */
#define TAOK_CMD_DELETEALL		3	/* delete all idx for peer */

/*
 * Flags for the taok_flags field.
 */
#define	TAOK_FLAGS_ACTIVE		0x01	/* active key index for SYN */

/*
 * MAC and KDF pairs for keys.
 */
#define TAOK_ALGO_MD5SIG		1	/* legacy compatibility */
#define TAOK_ALGO_HMAC-SHA-1-96		2	/* RFC5926, Section 2.2 */
#define TAOK_ALGO_AES-128-CMAC-96	3	/* RFC5926, Section 2.2 */

