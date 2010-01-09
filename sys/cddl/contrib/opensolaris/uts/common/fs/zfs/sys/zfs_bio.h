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
#include <sys/vdev_impl.h>	/* vd->vdev_vnode */
#include <sys/spa_impl.h>	/* spa->spa_root_vdev */
#include <sys/zfs_context.h>

extern int zfs_page_cache_disable;

void _zio_cache_valid(void *data, uint64_t size);
int _zio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, void *data,
    uint64_t size, zio_type_t type);

static __inline int
zio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, void *data,
    uint64_t size, zio_type_t type, vdev_t *vd)
{
	int io_bypass = 0;

#ifdef	_KERNEL
	if (!zfs_page_cache_disable && (vd == NULL) &&
	    ((type == ZIO_TYPE_WRITE) || (type == ZIO_TYPE_READ)))
		io_bypass = _zio_sync_cache(spa, bp, txg, data, size, type);
#endif
	return (io_bypass);
}

static __inline void
zio_cache_valid(void *data, uint64_t size, zio_type_t type, vdev_t *vd) 
{

#ifdef	_KERNEL
	if (((vd == NULL) || (vd->vdev_spa->spa_root_vdev == vd)) &&
	    (type == ZIO_TYPE_READ) && (size & PAGE_MASK) == 0)
		_zio_cache_valid(data, size);
#endif	
}


void *zio_getblk(uint64_t size, int flags);
void zio_relse(void *data, size_t size);
void *zio_spa_state_alloc(spa_t *spa);
void zfs_bio_init(void);
void zfs_bio_fini(void);

#ifndef _KERNEL
#define GB_NODUMP	0
#endif
#endif
