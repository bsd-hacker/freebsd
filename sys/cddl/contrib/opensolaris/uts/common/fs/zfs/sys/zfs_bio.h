/**************************************************************************

Copyright (c) 2009, Kip Macy, BitGravity Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the BitGravity Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/

#ifndef	_SYS_ZFS_BIO_H
#define	_SYS_ZFS_BIO_H

#define	ZBIO_BUF_CLONING	(1 << 30)	/* is being cloned */

void zbio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, void *data, uint64_t size, int bio_op);
void zbio_getblk(arc_buf_t *buf);
void zbio_data_getblk(arc_buf_t *buf);
void zbio_relse(arc_buf_t *buf, size_t size);

typedef struct zbio_buf_hdr zbio_buf_hdr_t;
struct zbio_buf_hdr {
	/* protected by hash lock */
	dva_t			b_dva;
	uint64_t		b_birth;
	uint32_t		b_flags;
	uint32_t		b_datacnt;

	/* immutable */
	arc_buf_contents_t	b_type;
	uint64_t		b_size;
	spa_t			*b_spa;
};

#ifdef _KERNEL
void zbio_init(void);
void zbio_fini(void);
#endif
#endif
