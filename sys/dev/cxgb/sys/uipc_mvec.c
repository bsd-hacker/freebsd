/**************************************************************************
 *
 * Copyright (c) 2007-2008, Kip Macy kmacy@freebsd.org
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. The name of Kip Macy nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 ***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/ktr.h>
#include <sys/sf_buf.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>

#include <cxgb_include.h>
#include <sys/mvec.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#ifdef INVARIANTS
#define M_SANITY m_sanity
#else
#define M_SANITY(a, b)
#endif


int
busdma_map_sg_collapse(struct mbuf **m, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *n = *m;
	struct mbuf *marray[TX_MAX_SEGS];
	int seg_count, defragged = 0, err = 0;
	bus_dma_segment_t *psegs;
	
	KASSERT(n->m_pkthdr.len, ("packet has zero header len"));
	if (n->m_pkthdr.len <= PIO_LEN)
		return (0);
retry:
	psegs = segs;
	seg_count = 0;
	if (n->m_next == NULL) {
		busdma_map_mbuf_fast(n, segs);
		*nsegs = 1;
		return (0);
	}
	while (n && seg_count < TX_MAX_SEGS) {
		/*
		 * firmware doesn't like empty segments
		 */
		if (__predict_true(n->m_len != 0)) {
			seg_count++;
			busdma_map_mbuf_fast(n, psegs);
			psegs++;
		}
		n = n->m_next;
	}
	if (seg_count == 0) {
		if (cxgb_debug)
			printf("empty segment chain\n");
		err = EFBIG;
		goto err_out;
	}  else if (seg_count >= TX_MAX_SEGS) {
		if (cxgb_debug)
			printf("mbuf chain too long: %d max allowed %d\n",
			    seg_count, TX_MAX_SEGS);
		if (!defragged) {
			n = m_defrag(*m, M_DONTWAIT);
			if (n == NULL) {
				err = ENOBUFS;
				goto err_out;
			}
			*m = n;
			defragged = 1;
			goto retry;
		}
		err = EFBIG;
		goto err_out;
	}

	*nsegs = seg_count;
err_out:	
	return (err);
}

int 
busdma_map_sg_vec(struct mbuf **m, bus_dma_segment_t *segs, int pkt_count)
{
	struct mbuf *m0;
	int i;

	for (m0 = *m, i = 0; i < pkt_count; segs++, i++, m0 = m0->m_nextpkt)
		busdma_map_mbuf_fast(m0, segs);

	return (0);
}

