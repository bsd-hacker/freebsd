/*
 * Copyright (c) 2012 Andre Oppermann, Internet Business Solutions AG
 * All rights reserved.
 * Copyright (c) 1982, 1986, 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipsec.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/ip_ipsec.h>

#ifdef SCTP
#include <netinet/sctp_crc32.h>
#endif
#include <machine/in_cksum.h>

#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>

extern	struct protosw inetsw[];

/*
 * Implement IPSec as pfil hook for host mode.
 *
 * Theory of operation.
 *
 * IPSec performs a couple of funtions that attach to different parts
 * in the IP[46] network stack:
 *  1. It enforces a packet encyption policy so that non-encrypted
 *     packets to certain destinations are not allow to pass through.
 *     This is firewall like, with deciding factor being the security
 *     policy and the state of the packet.
 *  2. It provides encryption/authentication of packet between two
 *     hosts on their IP addresses.  This is a transformation process
 *     that keeps the source/destination IP adresses intact and
 *     encrypts the payload.
 *     This is called transport mode and can be done directly and
 *     transparently in the IP input and output path.
 *  3. It provides an encrypted tunnel between two hosts like a
 *     virtual interface and encapsulates complete packets in it.
 *     Here routing decisions on which packets to send into a particular
 *     tunnel have to be made.
 *     This should be represented as virtual interfaces in the kernel.
 *
 *
 *      +   +--------------------------------------+ip_enqueue()
 *      |   |                                             ^
 *      v   v                                             |
 *    ip_input()                                          |
 *        +                                               |
 *        |                                               |
 *        v                                               |
 *  pfil_run_hooks()+---+                                 |
 *                      |                                 |
 *                      v                                 |
 *               ipsec_pfil_run()+------>AH|ESP?          |
 *                      +                 +  +            |
 *                      |              no |  | yes        |
 *                      |     policy?<----+  |            |
 *                      |      +  +          |            |
 *                      |   no |  | yes      |            |
 *                      |<-----+  |          v            |
 *                      |         |    verify/decrypt     |
 *                      |         |   no +   +            |
 *                      |         X------+   |            |
 *                      |        drop        v            |
 *                      |                 next_hdr        |
 *                      |                   +  +          |
 *                      |             other |  | ip       +
 *                      |<------------------+  +------>find_if()
 *                      |
 *                      v
 *                next_pfil_hook()
 *        v             +
 *        |             |
 *        |<------------+
 *        |
 *        v
 *
 * Next steps:
 *  - Implement 1 in a pfil hook to block non-encrypted packets.
 *  - Implement 2 in a pfil hook to in-path transform transport mode packets.
 *  - Implement per tunnel virtual ipsec interfaces.
 *  - Implement capturing of AH/ESP protocol type in pfil hook. If it
 *    is transport mode, transform the packet and continue with next
 *    pfil hook. If it is tunnel mode decapsulated the packet and
 *    re-inject it into ip_input() as originating from that virtual
 *    tunnel interface.
 *  - Implement crypto process-to-completion in addition to callback.
 *  - Add better support for NIC based ipsec offloading.
 *  - Clean up the mtags.
 */

static int
ipsec_pfil_run(void *arg, struct mbuf **m, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	struct ip *ip = mtod(*m, struct ip *);
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp = NULL;
	struct in_ifaddr *ia;
	int match = 0;
	int checkif = 0;
	int error = 0;

	switch (dir) {
	case PFIL_IN:
		if (ip->ip_p & (IPPROTO_AH | IPPROTO_ESP)) {
			/*
			 * If the packet is for us do a transform.
			 * We're effectively filtering the traffic.
			 *
			 * If it was transport mode, re-inject into
			 * next pfil hook.
			 *
			 * If it was tunnel mode, re-inject into
			 * ip_input() with new source interface.
			 */
			IN_IFADDR_RLOCK();
			LIST_FOREACH(ia, INADDR_HASH(ip->ip_dst.s_addr), ia_hash) {
				if (IA_SIN(ia)->sin_addr.s_addr == ip->ip_dst.s_addr &&
				    (!checkif || ia->ia_ifp == ifp))
					match = 1;
			}
			IN_IFADDR_RUNLOCK();
			if (!match)
				return (0);	/* Not for us, pass on. */

			error = ipsec4_common_input(*m, (ip->ip_hl << 2),
			    ip->ip_p);
			if (error)
				goto drop;
			*m = NULL;		/* mbuf was consumed. */
			return (0);
		}

		/*
		 * The input path doesn't do a transform.
		 */
		if ((inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0)
			return (0);
		/*
		 * Check if the packet has already had IPsec processing
		 * done.  If so, then just pass it along.  This tag gets
		 * set during AH, ESP, etc. input handling, before the
		 * packet is returned to the ip input queue for delivery.
		 */ 
		mtag = m_tag_find(*m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
		} else
			sp = ipsec_getpolicybyaddr(*m, IPSEC_DIR_INBOUND,
					   IP_FORWARDING, &error);   

		/* Check security policy against packet attributes. */
		if (sp != NULL) {
			error = ipsec_in_reject(sp, *m);
			KEY_FREESP(&sp);
		} else
			error = EINVAL;
		break;

	case PFIL_OUT:
		/*
		 * Check the security policy (SP) for the packet and, if
		 * required, do IPsec-related processing.  There are two
		 * cases here; the first time a packet is sent through
		 * it will be untagged and handled by ipsec4_checkpolicy.
		 * If the packet is resubmitted to ip_output (e.g. after
		 * AH, ESP, etc. processing), there will be a tag to bypass
		 * the lookup and related policy checking.
		 */
		mtag = m_tag_find(*m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			sp = ipsec_getpolicy(tdbi, IPSEC_DIR_OUTBOUND);
			if (sp == NULL) {
				error = -EINVAL;	/* force silent drop */
				goto drop;
			}
			m_tag_delete(*m, mtag);
		} else
			sp = ipsec4_checkpolicy(*m, IPSEC_DIR_OUTBOUND, 0,
						&error, inp);

		if (sp == NULL) {
			if (error != 0) {
				/*
				 * Hack: -EINVAL is used to signal that a packet
				 * should be silently discarded.  This is typically
				 * because we asked key management for an SA and
				 * it was delayed (e.g. kicked up to IKE).
				 */
				if (error == -EINVAL)
					error = 0;
				goto drop;
			}
			return (0);
		}

		/* Loop detection, check if ipsec processing already done */
		KASSERT(sp->req != NULL, ("ip_output: no ipsec request"));

		for (mtag = m_tag_first(*m); mtag != NULL;
		     mtag = m_tag_next(*m, mtag)) {
			if (mtag->m_tag_cookie != MTAG_ABI_COMPAT)
				continue;
			if (mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_DONE &&
			    mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED)
				continue;
			/*
			 * Check if policy has an SA associated with it.
			 * This can happen when an SP has yet to acquire
			 * an SA; e.g. on first reference.  If it occurs,
			 * then we let ipsec4_process_packet do its thing.
			 */
			if (sp->req->sav == NULL)
				break;
			tdbi = (struct tdb_ident *)(mtag + 1);
			if (tdbi->spi == sp->req->sav->spi &&
			    tdbi->proto == sp->req->sav->sah->saidx.proto &&
			    bcmp(&tdbi->dst, &sp->req->sav->sah->saidx.dst,
				 sizeof (union sockaddr_union)) == 0) {
				/*
				 * No IPsec processing is needed, free
				 * reference to SP.
				 *
				 * NB: null pointer to avoid free at
				 *     done: below.
				 */
				KEY_FREESP(&sp);
				return (0);
			}
		}

		/*
		 * Do delayed checksums now because we send before
		 * this is done in the normal processing path.
		 */
		if ((*m)->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			in_delayed_cksum(*m);
			(*m)->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
		}
#ifdef SCTP
		if ((*m)->m_pkthdr.csum_flags & CSUM_SCTP) {
			struct ip *ip = mtod(*m, struct ip *);

			sctp_delayed_cksum(*m, (uint32_t)(ip->ip_hl << 2));
			(*m)->m_pkthdr.csum_flags &= ~CSUM_SCTP;
		}
#endif
		/* NB: callee frees mbuf */
		error = ipsec4_process_packet(*m, sp->req, 0, 0);
		if (error == EJUSTRETURN) {
			/*
			 * We had a SP with a level of 'use' and no SA. We
			 * will just continue to process the packet without
			 * IPsec processing and return without error.
			 */
			error = 0;
			KEY_FREESP(&sp);
			return (0);
		}
		/*
		 * Preserve KAME behaviour: ENOENT can be returned
		 * when an SA acquire is in progress.  Don't propagate
		 * this to user-level; it confuses applications.
		 *
		 * XXX this will go away when the SADB is redone.
		 */
		if (error == ENOENT)
			error = 0;
		goto drop;
		break;

	default:
		break;
	}

drop:
	if (error < 0)
		error = EACCES;
	if (sp != NULL)
		KEY_FREESP(&sp);

	m_freem(*m);
	return (error);
}

static int
ipsec_pfil_hook(int af)
{
	struct pfil_head *pfh;

	pfh = pfil_head_get(PFIL_TYPE_AF, af);
	if (pfh == NULL)
		return ENOENT;

	pfil_add_hook_order(ipsec_pfil_run, NULL, "ipsec",
	    (PFIL_IN | PFIL_OUT), PFIL_ORDER_FIRST, pfh);

	return (0);
}

static int
ipsec_pfil_unhook(int af)
{
	struct pfil_head *pfh;

	pfh = pfil_head_get(PFIL_TYPE_AF, af);
	if (pfh == NULL)
		return ENOENT;

	pfil_remove_hook(ipsec_pfil_run, NULL, (PFIL_IN | PFIL_OUT), pfh);

	return (0);
}

static void
ipsec_pfil_init(void)
{

	(void)ipsec_pfil_hook(AF_INET);
	(void)ipsec_pfil_unhook(AF_INET);
}

SYSINIT(ipsec_pfil_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, ipsec_pfil_init, NULL);

