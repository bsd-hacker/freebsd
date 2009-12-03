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

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/zfs_bio.h>
#include <sys/refcount.h>
#include <sys/vdev.h>
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/sdt.h>

#include <vm/vm_pageout.h>

#ifdef _KERNEL

#define	BUF_EMPTY(buf)						\
	((buf)->b_dva.dva_word[0] == 0 &&			\
	(buf)->b_dva.dva_word[1] == 0 &&			\
	(buf)->b_birth == 0)

SYSCTL_DECL(_vfs_zfs);
static int zfs_page_cache_disable = 1;
TUNABLE_INT("vfs.zfs.page_cache_disable", &zfs_page_cache_disable);
SYSCTL_INT(_vfs_zfs, OID_AUTO, page_cache_disable, CTLFLAG_RDTUN,
    &zfs_page_cache_disable, 0, "Disable backing ARC with page cache ");

static eventhandler_tag zbio_event_shutdown = NULL;

static void
_zbio_getblk(arc_buf_t *buf, int flags)
{
	zbio_buf_hdr_t		*hdr = (zbio_buf_hdr_t *)buf->b_hdr;
	uint64_t		size = hdr->b_size;
	spa_t			*spa = hdr->b_spa;
	uint64_t blkno = hdr->b_dva.dva_word[1] & ~(1ULL<<63);
	void *data;
	struct vnode *vp;
	struct buf *newbp;
	struct bufobj *bo;

	vp = spa_get_vnode(spa);
	bo = &vp->v_bufobj;
	newbp = NULL;
	if ((size < PAGE_SIZE) || (hdr->b_flags & ZBIO_BUF_CLONING) ||
	    zfs_page_cache_disable) {
		data = zio_buf_alloc(size);
		hdr->b_flags &= ~ZBIO_BUF_CLONING;
	} else if (BUF_EMPTY(hdr)) {
		newbp = geteblk(size, flags);
		data = newbp->b_data;
	} else {
		newbp = getblk(vp, blkno, size, 0, 0, flags | GB_LOCK_NOWAIT);
		if (newbp == NULL)
			newbp = geteblk(size, flags);
		else
			brelvp(newbp);
		data = newbp->b_data;
	}

	if (newbp != NULL) {
		BUF_KERNPROC(newbp);
		newbp->b_bufobj = bo;
		CTR4(KTR_SPARE2, "arc_getblk() bp=%p flags %X "
		    "blkno %ld npages %d",
		    newbp, newbp->b_flags, blkno, newbp->b_npages);
	}

	buf->b_bp = newbp;
	buf->b_data = data;
}

void
zbio_getblk(arc_buf_t *buf)
{

	_zbio_getblk(buf, 0);
}

void
zbio_data_getblk(arc_buf_t *buf)
{

	_zbio_getblk(buf, GB_NODUMP);
}

void
zbio_relse(arc_buf_t *buf, size_t size)
{
	struct buf *bp = buf->b_bp;
	void * data = buf->b_data;

	if (bp == NULL) {
		zio_buf_free(data, size);
		return;
	}

	CTR4(KTR_SPARE2, "arc_brelse() bp=%p flags %X"
	    " size %ld blkno=%ld",
	    bp, bp->b_flags, size, bp->b_blkno);

	bp->b_flags |= B_ZFS;
	brelse(bp);
}

void
zbio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, uint64_t size)
{
#ifdef notyet
	uint64_t blkno, blkno_lookup;
	struct vnode *vp;
	struct bufobj *bo;
	struct buf *bp;
	vm_pindex_t start, end;
	vm_object_t object;
	vm_page_t m;
	int i;

	if (zfs_page_cache_disable)
		return;
	blkno_lookup = blkno = dva->dva_word[1] & ~(1ULL<<63);
	vp = spa_get_vnode(spa);
	bo = &vp->v_bufobj;

	if (dva == NULL || spa == NULL || blkno == 0 || size == 0) 
		return;

	start = OFF_TO_IDX((blkno_lookup << 9));
	end = start + OFF_TO_IDX(size + PAGE_MASK);
	object = vp->v_object;

	VM_OBJECT_LOCK(object);
	vm_page_cache_free(object, start, end);
	vm_object_page_remove(object, start, end, FALSE);
#ifdef INVARIANTS
	for (i = 0; i < OFF_TO_IDX(size); i++) {
		KASSERT(vm_page_lookup(object, start + i) == NULL,
		    ("found page at %ld blkno %ld blkno_lookup %ld",
			start + i, blkno, blkno_lookup));
	}
#endif	
	VM_OBJECT_UNLOCK(object);
#endif
}

#if 0
static void
arc_pcache(struct vnode *vp, struct buf *bp, uint64_t blkno)
{
	vm_pindex_t start = OFF_TO_IDX((blkno << 9));
	vm_object_t object = vp->v_object;
	struct bufobj *bo = &vp->v_bufobj;
	vm_page_t m;
	int i;

	CTR3(KTR_SPARE2, "arc_pcache() bp=%p blkno %ld npages %d",
		   bp, blkno, bp->b_npages);
	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		m->valid = VM_PAGE_BITS_ALL;
		vm_page_insert(m, object, start + i);
		m->flags &= ~PG_UNMANAGED;
		vm_page_enqueue(PQ_INACTIVE, m);
		vdrop(vp);
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
	bp->b_bufobj = bo;
	bp->b_flags |= B_VMIO;
}

static void
arc_bcache(arc_buf_t *buf)
{	
	uint64_t blkno = buf->b_hdr->b_dva.dva_word[1] & ~(1ULL<<63);
	struct buf *bp;
	struct vnode *vp = spa_get_vnode(buf->b_hdr->b_spa);
	arc_buf_hdr_t *hdr = buf->b_hdr;
	int cachebuf;

	if (zfs_page_cache_disable)
		return;

	if (blkno == 0 || hdr->b_birth == 0)
		return;

	bp = buf->b_bp;
	bp->b_birth = hdr->b_birth;
	bp->b_blkno = bp->b_lblkno = blkno;
	bp->b_offset = (blkno << 9);
	cachebuf = ((hdr->b_datacnt == 1) &&
	    !(hdr->b_flags & ARC_IO_ERROR) &&
	    ((bp->b_flags & (B_INVAL|B_CACHE)) == B_CACHE) &&
	    (blkno & 0x7) == 0);

	arc_binval(hdr->b_spa, &hdr->b_dva, hdr->b_size);
	if (cachebuf) 
		arc_pcache(vp, bp, blkno);	
}
#endif

static void
zbio_shutdown(void *arg __unused, int howto __unused)
{
	struct mount *mp, *tmpmp;
	int error;

	/*
	 * unmount all ZFS file systems - freeing any buffers
	 * then free all space allocator resources
	 */
	TAILQ_FOREACH_SAFE(mp, &mountlist, mnt_list, tmpmp) {
		if (strcmp(mp->mnt_vfc->vfc_name, "zfs") == 0) {
			error = dounmount(mp, MNT_FORCE, curthread);
			if (error) {
				TAILQ_REMOVE(&mountlist, mp, mnt_list);
				printf("unmount of %s failed (",
				    mp->mnt_stat.f_mntonname);
				if (error == EBUSY)
					printf("BUSY)\n");
				else
					printf("%d)\n", error);
			}
		}
		
	}
	arc_flush(NULL);

#ifdef NOTYET
	/*
	 * need corresponding includes
	 */
	zfsdev_fini();
	zvol_fini();
	zfs_fini();
#endif	
	spa_fini();
}

void
zbio_init(void)
{

	zbio_event_shutdown = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    zbio_shutdown, NULL, EVENTHANDLER_PRI_FIRST);
}

void
zbio_fini(void)
{
	if (zbio_event_shutdown != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, zbio_event_shutdown);
}
#else

void
zbio_getblk(arc_buf_t *buf)
{
	zbio_buf_hdr_t		*hdr = (zbio_buf_hdr_t *)buf->b_hdr;
	uint64_t		size = hdr->b_size;

	buf->b_data = zio_buf_alloc(size);
	hdr->b_flags &= ~ZBIO_BUF_CLONING;
}

void
zbio_data_getblk(arc_buf_t *buf)
{
	zbio_buf_hdr_t		*hdr = (zbio_buf_hdr_t *)buf->b_hdr;
	uint64_t		size = hdr->b_size;

	buf->b_data = zio_data_buf_alloc(size);
	hdr->b_flags &= ~ZBIO_BUF_CLONING;
}

void
zbio_relse(arc_buf_t *buf, size_t size)
{

	zio_buf_free(buf->b_data, size);
}

void
zbio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, uint64_t size)
{
	;
}

#endif
