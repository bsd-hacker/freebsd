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

/**************************************************************************
This module integrates the caching af pages associated with ARC buffers in a
per-SPA vm object. Each SPA also has an associated "zio_state_t" which
tracks mapped bufs allocated for the SPA in a splay tree.

A global lookup table (currently splay tree, subject to change) is used to
map the data KVA to the struct buf. This allows us to provide page cache
integration without churning ZFS' malloc-centric interfaces that only pass
the data address around.

Buffers are malloced if the size is not a multiple of PAGE_SIZE, and thus
the buffer could not be an integer number of pages. 

ZFS does not provide any block information when allocating a buffer.
Thus, even page backed buffers are unmapped at allocation time. Buffers
are mapped to an address in zio_sync_cache (called from zio_create),
where they are either added to or removed from the page cache backing
the vdev. Once the buffer is mapped a second splay tree tracks buffers
by block address whose pages are referenced by the vm object. It is used to
ensure that buffers that belong to an older transaction group don't have their
pages mapped by buffers belonging to a newer transaction group.

Pages in the vm object are marked valid before the initiation of a write
and on completion of a read. 


Logic in sync_cache:
   B_MALLOC:
           evict older txg
       READ:
           copyin from page cache
       WRITE:
           copyout to page cache
   !B_MALLOC:
       READ:
           evict older txg
           if (cached pages all valid)
               free current buffer pages and map in cached pages
           else
               remove all pages from object
               insert current buffer's pages
       WRITE:
           evict older txg + pages
           mark buffer's pages as valid
           insert buffer's pages in the object
       mark buffer B_VMIO
   B_VMIO:
       No work to do 


**************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/refcount.h>
#include <sys/vdev.h>
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/sdt.h>

#include <sys/sf_buf.h>
#include <sys/zfs_bio.h>

#ifdef _KERNEL

SYSCTL_DECL(_vfs_zfs);
int zfs_page_cache_disable = 1;
TUNABLE_INT("vfs.zfs.page_cache_disable", &zfs_page_cache_disable);
SYSCTL_INT(_vfs_zfs, OID_AUTO, page_cache_disable, CTLFLAG_RDTUN,
    &zfs_page_cache_disable, 0, "Disable backing ARC with page cache ");

static eventhandler_tag zfs_bio_event_shutdown;
struct zio_spa_state;
typedef struct zio_spa_state *	zio_spa_state_t;
typedef	struct buf	*	buf_t;

MALLOC_DEFINE(M_ZFS_BIO, "zfs_bio", "zfs buffer cache / vm");

#define	B_DATA		B_00001000

#define	ZB_EVICT_ALL		0x1
#define	ZB_EVICT_BUFFERED	0x2

#define	ZB_COPYIN		0x2
#define	ZB_COPYOUT		0x3

#define	NO_TXG			0x0

#define btos(nbytes)	((nbytes)>>DEV_BSHIFT)
#define stob(nsectors)	((nsectors)<<DEV_BSHIFT) 

#define b_state			b_fsprivate3

struct zio_spa_state {
	struct mtx 	zss_mtx;
	buf_t 		zss_blkno_root;	/* track buf by blkno 		*/

	spa_t		*zss_spa;
	int		zss_generation;
	int		zss_resident_count;
	TAILQ_HEAD(, buf) zss_blkno_memq;	/* list of resident buffers */
};
/*
 * Hash table routines
 */

#define	HT_LOCK_PAD	128

struct ht_lock {
	struct mtx	ht_lock;
#ifdef _KERNEL
	unsigned char	pad[(HT_LOCK_PAD - sizeof (struct mtx))];
#endif
};

typedef struct cluster_list_head *	buf_head_t;

#define	BUF_LOCKS 256
typedef struct buf_hash_table {
	uint64_t	ht_mask;
	buf_head_t 	ht_table;
	struct ht_lock	*ht_locks;
} buf_hash_table_t;

static buf_hash_table_t buf_hash_table;

#define	BUF_HASH_INDEX(va, size)					\
	(buf_hash(va, size) & buf_hash_table.ht_mask)
#define	BUF_HASH_LOCK_NTRY(idx) 	(buf_hash_table.ht_locks[idx & (BUF_LOCKS-1)])
#define	BUF_HASH_LOCK(idx)		(&(BUF_HASH_LOCK_NTRY(idx).ht_lock))

#define ZIO_SPA_STATE_LOCK(zs)		mtx_lock(&(zs)->zss_mtx)
#define	ZIO_SPA_STATE_UNLOCK(zs)	mtx_unlock(&(zs)->zss_mtx)

#define	spa_get_zio_state(spa)		((zio_spa_state_t)spa_get_vnode((spa))->v_data)
#define	spa_get_vm_object(spa)		spa_get_vnode((spa))->v_object
#define	zio_buf_get_spa(bp)		(((zio_spa_state_t)bp->b_state)->zss_spa)
#define	zio_buf_get_vm_object(bp)	spa_get_vm_object(zio_buf_get_spa((bp)))

#ifndef DEBUG
#define INLINE static __inline
#else
#define INLINE
#endif

static uint64_t
buf_hash(caddr_t va, uint64_t size)
{
	uint64_t crc = -1ULL;
	uint8_t *vav = (uint8_t *)&va;
	int i;

	ASSERT(zfs_crc64_table[128] == ZFS_CRC64_POLY);

	for (i = 0; i < sizeof (caddr_t); i++)
		crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ vav[i]) & 0xFF];

	crc ^= (size>>4);

	return (crc);
}

const char *buf_lock = "ht_lock";


void
buf_init(void)
{
	uint64_t *ct;
	uint64_t hsize = 1ULL << 12;
	int i, j;

	/*
	 * The hash table is big enough to fill all of physical memory
	 * with an average 64K block size.  The table will take up
	 * totalmem*sizeof(void*)/64K (eg. 128KB/GB with 8-byte pointers).
	 */
	while (hsize * 65536 < (uint64_t)physmem * PAGESIZE)
		hsize <<= 1;
retry:
	buf_hash_table.ht_mask = hsize - 1;
	buf_hash_table.ht_table =
	    malloc(hsize * sizeof (struct cluster_list_head), M_ZFS_BIO, M_NOWAIT|M_ZERO);
	if (buf_hash_table.ht_table == NULL) {
		ASSERT(hsize > (1ULL << 8));
		hsize >>= 1;
		goto retry;
	}
	buf_hash_table.ht_locks =
	    malloc(BUF_LOCKS*128, M_ZFS_BIO, M_WAITOK|M_ZERO);	
	
	for (i = 0; i < hsize; i++)
		TAILQ_INIT(&buf_hash_table.ht_table[i]);

	for (i = 0; i < BUF_LOCKS; i++)
		mtx_init(&(buf_hash_table.ht_locks[i].ht_lock),  buf_lock, NULL, MTX_DEF|MTX_DUPOK);
}

void *
zio_spa_state_alloc(spa_t *spa)
{
	struct zio_spa_state *zss;

	zss = malloc(sizeof(struct zio_spa_state), M_ZFS_BIO, M_WAITOK|M_ZERO);

	zss->zss_spa = spa;
	mtx_init(&zss->zss_mtx, "zio_spa_state", NULL, MTX_DEF);
	TAILQ_INIT(&zss->zss_blkno_memq);

	return (zss);
}

/*
 * scan blkno + size range in object to verify that all the pages are
 * resident and valid
 */
static int
vm_pages_valid_locked(vm_object_t object, uint64_t blkno, uint64_t size)
{
	vm_page_t m;
	uint64_t i;

	for (i = stob(blkno); i < stob(blkno) + size; i += PAGE_SIZE)
		if ((m = vm_page_lookup(object, OFF_TO_IDX(i))) == NULL ||
		    (m->valid != VM_PAGE_BITS_ALL))
			return (0);
	return (1);
}

static int
vm_pages_valid(vm_object_t object, uint64_t blkno, uint64_t size)
{
	int valid;

	VM_OBJECT_LOCK(object);
	valid = vm_pages_valid_locked(object, blkno, size);
	VM_OBJECT_UNLOCK(object);

	return (valid);
}


/*
 *	zio_buf_insert:		[ internal use only ]
 *
 *	Inserts the given buf into the state splay tree and state list.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
INLINE void
zio_buf_va_insert(buf_t bp)
{
	caddr_t va = bp->b_data;
	long idx, size = bp->b_bcount;
	struct mtx *lock;
	buf_head_t bh;

	idx = BUF_HASH_INDEX(va, size);
	lock = BUF_HASH_LOCK(idx);
	bh = &buf_hash_table.ht_table[idx];

	CTR3(KTR_SPARE3, "va_insert(va=%p size=%ld) idx=%ld", va, size, idx);

	mtx_lock(lock);
	TAILQ_INSERT_HEAD(bh, bp, b_freelist);
	mtx_unlock(lock);
}

/*
 *	zio_buf_va_lookup:
 *
 *	Returns the range associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
INLINE buf_t 
zio_buf_va_lookup(caddr_t va, uint64_t size)
{
	buf_t bp;
	uint64_t idx;
	struct mtx *lock;
	buf_head_t bh;

	idx = BUF_HASH_INDEX(va, size);
	lock = BUF_HASH_LOCK(idx);
	bh = &buf_hash_table.ht_table[idx];
	mtx_lock(lock);
	TAILQ_FOREACH(bp, bh, b_freelist)
		if (bp->b_data == va)
			break;
	mtx_unlock(lock);
	return (bp);
}

/*
 *	zio_buf_va_remove:
 *
 *	Removes the given buf from the spa's state tree
 *	buf list
 *
 *	The state and buf must be locked.
 *	This routine may not block.
 */
INLINE buf_t
zio_buf_va_remove(caddr_t va, uint64_t size)
{
	long idx;
	struct mtx *lock;
	buf_head_t bh;
	buf_t bp;

	idx = BUF_HASH_INDEX(va, size);
	lock = BUF_HASH_LOCK(idx);
	bh = &buf_hash_table.ht_table[idx];

	CTR3(KTR_SPARE3, "va_remove(va=%p size=%ld) idx=%ld", va, (long)size, idx);
	mtx_lock(lock);
	TAILQ_FOREACH(bp, bh, b_freelist)
	    if (bp->b_data == va) {
		    TAILQ_REMOVE(bh, bp, b_freelist);
		    break;
	    }
	mtx_unlock(lock);
	KASSERT(bp != NULL, ("no buffer found for va=%p size=%lld", va, size));
	return (bp);
}

/*
 *	zio_buf_blkno_splay:		[ internal use only ]
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the buf containing the given lblkno.  If, however, that
 *	lblkno is not found in the tree, returns a buf that is
 *	adjacent to the pindex, coming before or after it.
 */
static buf_t 
zio_buf_blkno_splay(daddr_t blkno, buf_t root)
{
	struct buf dummy;
	buf_t lefttreemax, righttreemin, y;
	
	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = (buf_t)&dummy;
	for (;; root = y) {
		if (blkno < root->b_blkno) {
			if ((y = root->b_left) == NULL)
				break;
			if (blkno < y->b_blkno) {
				/* Rotate right. */
				root->b_left = y->b_right;
				y->b_right = root;
				root = y;
				if ((y = root->b_left) == NULL)
					break;
			}
			/* Link into the new root's right tree. */
			righttreemin->b_left = root;
			righttreemin = root;
		} else if (blkno > root->b_blkno) {
			if ((y = root->b_right) == NULL)
				break;
			if (blkno > y->b_blkno) {
				/* Rotate left. */
				root->b_right = y->b_left;
				y->b_left = root;
				root = y;
				if ((y = root->b_right) == NULL)
					break;
			}
			/* Link into the new root's left tree. */
			lefttreemax->b_right = root;
			lefttreemax = root;
		} else
			break;
	}
	/* Assemble the new root. */
	lefttreemax->b_right = root->b_left;
	righttreemin->b_left = root->b_right;
	root->b_left = dummy.b_right;
	root->b_right = dummy.b_left;
	return (root);
}

/*
 *	zio_buf_blkno_insert:		[ internal use only ]
 *
 *	Inserts the given buf into the state splay tree and state list.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
static void
zio_buf_blkno_insert(buf_t bp, zio_spa_state_t object)
{
	buf_t root;
	daddr_t root_blkno_end, blkno, blkno_end;

	blkno = bp->b_blkno;
	blkno_end = bp->b_blkno + btos(bp->b_bcount);

	root = object->zss_blkno_root;
	if (root == NULL) {
		bp->b_left = NULL;
		bp->b_right = NULL;
		TAILQ_INSERT_TAIL(&object->zss_blkno_memq, bp, b_freelist);
	} else {
		root = zio_buf_blkno_splay(bp->b_blkno, root);
		root_blkno_end = root->b_blkno + btos(root->b_bcount);

		if (blkno < root->b_blkno) {
			KASSERT(blkno_end <= root->b_blkno, ("buffer overlap!"));
			bp->b_left = root->b_left;
			bp->b_right = root;
			root->b_left = NULL;
			TAILQ_INSERT_BEFORE(root, bp, b_freelist);
		} else if (blkno == root->b_blkno) {
			panic("zio_buf_blkno_insert: blkno already allocated");
		} else {
			KASSERT(root_blkno_end <= blkno, ("buffer overlap!"));

			bp->b_right = root->b_right;
			bp->b_left = root;
			root->b_right = NULL;
			TAILQ_INSERT_AFTER(&object->zss_blkno_memq, root, bp, b_freelist);
		}
	}
	object->zss_blkno_root = bp;
	object->zss_generation++;

	/*
	 * show that the object has one more resident buffer.
	 */
	object->zss_resident_count++;
}

/*
 *	zio_buf_blkno_lookup:
 *
 *	Returns the range associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
static buf_t
zio_buf_blkno_lookup(zio_spa_state_t state, daddr_t blkno)
{
	buf_t bp;

	if ((bp = state->zss_blkno_root) != NULL && bp->b_blkno != blkno) {
		bp = zio_buf_blkno_splay(blkno, bp);
		if ((state->zss_blkno_root = bp)->b_blkno != blkno)
			bp = NULL;
	}
	return (bp);
}

/*
 *	zio_buf_remove:
 *
 *	Removes the given buf from the spa's state tree
 *	buf list
 *
 *	The state and buf must be locked.
 *	This routine may not block.
 */
static void
zio_buf_blkno_remove(buf_t bp)
{
	zio_spa_state_t state;
	buf_t root;
	daddr_t blkno, blkno_end;

	if ((state = bp->b_state) == NULL)
		return;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (bp != state->zss_blkno_root)
		zio_buf_blkno_splay(bp->b_blkno, state->zss_blkno_root);
	if (bp->b_left == NULL)
		root = bp->b_right;
	else {
		root = zio_buf_blkno_splay(bp->b_blkno, bp->b_left);
		root->b_right = bp->b_right;
	}
	state->zss_blkno_root = root;
	TAILQ_REMOVE(&state->zss_blkno_memq, bp, b_freelist);

	/*
	 * And show that the object has one fewer resident page.
	 */
	state->zss_resident_count--;
	state->zss_generation++;
}

static __inline void
zio_buf_vm_object_copy(vm_object_t object, buf_t bp, int direction)
{
	vm_pindex_t start, end;
	vm_offset_t offset;
	uint64_t byte_offset;
	vm_offset_t page_offset;
	int i, size;	
	caddr_t va;	
	vm_page_t m;
	struct sf_buf *sf;

	byte_offset = stob(bp->b_blkno);
	page_offset = byte_offset & PAGE_MASK;
	start = OFF_TO_IDX(byte_offset);
	end = OFF_TO_IDX(byte_offset + bp->b_bcount);

	VM_OBJECT_LOCK(object);	
	if (vm_pages_valid_locked(object, bp->b_blkno, bp->b_bcount) == 0)
		goto done;

	for (i = 0; i < bp->b_npages; i++) {		
		sf = sf_buf_alloc(bp->b_pages[i], 0);
		va = (caddr_t)sf_buf_kva(sf);
		size = PAGE_SIZE;		
				
		if (i == 0) 
			va += page_offset;
		if (i == bp->b_npages - 1)
			size = PAGE_SIZE - page_offset;

		if (direction == ZB_COPYIN)
			memcpy(bp->b_data + PAGE_SIZE*i, va, size);
		else
			memcpy(va, bp->b_data + PAGE_SIZE*i, size);
		sf_buf_free(sf);
	}
	bp->b_flags &= ~B_INVAL;
	bp->b_flags |= B_CACHE;

done:
	bp->b_npages = 0;
	VM_OBJECT_UNLOCK(object);
}

static void
zio_buf_vm_object_copyout(vm_object_t object, buf_t bp)
{
	
	zio_buf_vm_object_copy(object, bp, ZB_COPYOUT);
}

static void
zio_buf_vm_object_copyin(vm_object_t object, buf_t bp)
{
	
	zio_buf_vm_object_copy(object, bp, ZB_COPYIN);
}

static void
zio_buf_vm_object_evict(buf_t bp)
{
	int i;
	vm_page_t m;

	VM_OBJECT_LOCK_ASSERT(zio_buf_get_vm_object(bp), MA_OWNED);
	vm_page_lock_queues();
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		vm_pageq_remove(m);
	}
	vm_page_unlock_queues();
	/*
	 * remove pages from backing vm_object 
	 */
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		vm_page_remove(m);
		m->valid = 0;
		m->flags |= PG_UNMANAGED;
	}
}

static void
zio_buf_vm_object_insert(buf_t bp, struct vnode *vp,
    vm_object_t object, int valid)
{
	vm_page_t m;
	vm_pindex_t start;
	int i;

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	/*
	 * Insert buffer pages in the object
	 */
	start = OFF_TO_IDX(stob(bp->b_blkno));
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if (valid)
			m->valid = VM_PAGE_BITS_ALL;
		vm_page_insert(m, object, start + i);
		m->flags &= ~PG_UNMANAGED;
	}
	vm_page_lock_queues();
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		vm_page_enqueue(PQ_INACTIVE, m);
	}
	vm_page_unlock_queues();
}

/*
 *	zio_buf_evict_overlap:		[ internal use only ]
 *
 *	Evict the pages of any buffers overlapping with this range
 *
 *	If ZB_EVICT_ALL is passed then evict all the pages in that range
 *	from the vm object
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
static void
zio_buf_evict_overlap(vm_object_t object, daddr_t blkno, int size,
    zio_spa_state_t state, uint64_t txg, int evict_op)
{
	buf_t root, tmpbp;
	daddr_t blkno_end, tmpblkno, tmpblkno_end;
	struct cluster_list_head clh;
	int i, collisions;
	uint64_t tmptxg;
	vm_pindex_t start, end;

	if ((root = state->zss_blkno_root) == NULL)
		goto done;

	collisions = 0;
	root = zio_buf_blkno_splay(blkno, root);
	TAILQ_INIT(&clh);
	if (blkno < root->b_blkno)
		tmpbp = TAILQ_PREV(root, cluster_list_head, b_freelist);

	/*
	 * Find all existing buffers that overlap with this range
	 */
	tmpbp = tmpbp != NULL ? tmpbp : root;
	while (tmpbp != NULL && tmpbp->b_blkno < blkno_end) {
		tmpblkno = tmpbp->b_blkno;
		tmpblkno_end = tmpblkno + btos(tmpbp->b_bcount);
		tmptxg = tmpbp->b_birth;

		if (((tmpblkno >= blkno) && (tmpblkno < blkno_end)) ||
		    (tmpblkno_end > blkno) && (tmpblkno_end <= blkno_end) &&
		    ((txg == NO_TXG) || (tmptxg < txg))) {
			TAILQ_INSERT_TAIL(&clh, tmpbp, b_freelist);
			collisions++;
		}
		tmpbp = TAILQ_NEXT(tmpbp, b_freelist);
	}
	while (!TAILQ_EMPTY(&clh)) {
		tmpbp = TAILQ_FIRST(&clh);
		TAILQ_REMOVE(&clh, tmpbp, b_freelist);
		zio_buf_vm_object_evict(tmpbp);

		tmpbp->b_flags &= ~B_VMIO;
		state->zss_blkno_root = tmpbp;
		/*
		 * move buffer to the unmanaged tree
		 */
		zio_buf_blkno_remove(tmpbp);
	}
done:
	if (!(collisions == 1 && tmpbp->b_blkno == blkno &&
		tmpbp->b_bcount == size) && (evict_op == ZB_EVICT_ALL)) {
		start = OFF_TO_IDX(stob(blkno));
		end = start + OFF_TO_IDX(size);
		VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
		vm_page_cache_free(object, start, end);
		vm_object_page_remove(object, start, end, FALSE);
#ifdef INVARIANTS
		for (i = 0; i < OFF_TO_IDX(size); i++) {
			KASSERT(vm_page_lookup(object, start + i) == NULL,
			    ("found page at %ld blkno %ld ",start + i, blkno));
		}
#endif	
	}
}

/*
 * insert pages from object in to bp's b_pages
 * and wire
 */
static void
vm_object_reference_pages(vm_object_t object, buf_t bp)
{
	uint64_t blkno, size;
	vm_pindex_t start;
	vm_page_t m;
	int i;

	start = OFF_TO_IDX(stob(blkno));
	vm_page_lock_queues();
	for (i = 0; i < bp->b_npages; i++) {
		m = vm_page_lookup(object, start + i);
		vm_page_wire(m);
		bp->b_pages[i] = m;
	}
	vm_page_unlock_queues();
}

/*
Cases:

A) B_MALLOC /  address is known
    1) getblk:
          a) page   cached: copyin + mark B_CACHE
	  b) buffer+page cached: copyin + mark B_CACHE
	  c) default: N/A
    2) sync_cache:
          a) page   cached: copy{in, out}
	  b) buffer+page cached: evict overlapping pages
	  c) default: N/A
B) B_MALLOC /  address is !known
    1) getblk: N/A
    2) sync_cache:
          a) page   cached: copy{in, out}
	  b) buffer+page cached: evict overlapping pages
	  c) default: N/A
  
C) !B_MALLOC / address is !known
    2) sync_cache:
          a) page   cached: evict/free old pages + replace
	  b) buffer+page cached: evict overlapping pages from object + replace
	  c) default: add pages to vm object
	  
D) !B_MALLOC / address is known
    1) getblk:
	  a) buffer+page cached: evict pages belonging to older buffer
	  b) default: N/A
    2) sync_cache: N/A - we should only be doing I/O on valid B_VMIO buffers

*/

static buf_t 
_zio_getblk_malloc(uint64_t size, int flags)
{
	buf_t 		newbp;

	newbp = malloc(sizeof(struct buf), M_ZFS_BIO, M_WAITOK|M_ZERO);
	newbp->b_flags = (B_MALLOC|B_INVAL);
	newbp->b_bcount = size;

	if (flags & GB_NODUMP) {
		newbp->b_flags |= B_DATA;
		newbp->b_data = _zio_data_buf_alloc(size);
	} else
		newbp->b_data = _zio_buf_alloc(size);

	return (newbp);
}

static buf_t 
_zio_getblk_vmio(uint64_t size, int flags)
{
	buf_t 		newbp;

	newbp = geteblk(size, flags);
	BUF_KERNPROC(newbp);

	return (newbp);
}

void *
zio_getblk(uint64_t size, int flags)
{
	buf_t 		newbp;

	if (size & PAGE_MASK)
		newbp = _zio_getblk_malloc(size, flags);
	else
		newbp = _zio_getblk_vmio(size, flags);

	zio_buf_va_insert(newbp);
	return (newbp->b_data);
}

void
zio_relse(void *data, size_t size)
{
	buf_t bp;

	bp = zio_buf_va_remove(data, size);

	if (bp->b_flags & B_VMIO)
		zio_buf_blkno_remove(bp);

	if (bp->b_flags & B_MALLOC) {
		if (bp->b_flags & B_DATA)
			_zio_data_buf_free(bp->b_data, size);
		else
			_zio_buf_free(bp->b_data, size);
		free(bp, M_ZFS_BIO);
	} else {
		CTR4(KTR_SPARE2, "arc_brelse() bp=%p flags %X"
		    " size %ld blkno=%ld",
		    bp, bp->b_flags, size, bp->b_blkno);

		bp->b_flags |= B_ZFS;
		brelse(bp);
	}
}

int
_zio_sync_cache(spa_t *spa, blkptr_t *blkp, uint64_t txg, void *data,
    uint64_t size, zio_type_t zio_op)
{
	buf_t		bp;
	zio_spa_state_t state = spa_get_zio_state(spa);
	dva_t		dva = *BP_IDENTITY(blkp);
	daddr_t		blkno = dva.dva_word[1] & ~(1ULL<<63);
	struct vnode	*vp = spa_get_vnode(spa);
	vm_object_t	object = vp->v_object;
	vm_pindex_t	start;
	vm_page_t	m;	
	int i, io_bypass = FALSE;

	bp = zio_buf_va_lookup(data, size);
	
	if (bp->b_flags & B_MALLOC) {
		zio_buf_evict_overlap(object, blkno, size, state, txg, ZB_EVICT_BUFFERED);

		if (zio_op == ZIO_TYPE_READ) {
			/*
			 * if page resident - copy in
			 * update zio pipeline
			 */
			zio_buf_vm_object_copyin(object, bp);
			if (bp->b_flags & B_CACHE) {
				/* update zio pipeline */
				io_bypass = TRUE;
			}
		} else {
			zio_buf_vm_object_copyout(object, bp);
		}
	} else if (bp->b_flags & B_VMIO) {
		KASSERT(bp == zio_buf_blkno_lookup(state, blkno),
		    ("VMIO buffer not mapped"));
		if (zio_op == ZIO_TYPE_READ && (bp->b_flags & (B_CACHE|B_INVAL)) == B_CACHE)
			io_bypass = TRUE;		
	} else if ((zio_op == ZIO_TYPE_WRITE) || !vm_pages_valid(object, blkno, size)) {
		VM_OBJECT_LOCK(object);
		zio_buf_evict_overlap(object, blkno, size, state, NO_TXG,
		    ZB_EVICT_ALL);
		bp->b_blkno = bp->b_lblkno = blkno;
		bp->b_flags |= B_VMIO;
		bp->b_birth = txg;
		zio_buf_blkno_insert(bp, state);
		zio_buf_vm_object_insert(bp, vp, object, zio_op == ZIO_TYPE_WRITE);
		VM_OBJECT_UNLOCK(object);
	} else {
		KASSERT(zio_op == ZIO_TYPE_READ, ("unexpected op %d", zio_op));
		zio_buf_evict_overlap(object, blkno, size, state, NO_TXG,
		    ZB_EVICT_BUFFERED);
		bp->b_blkno = bp->b_lblkno = blkno;
		bp->b_flags |= B_VMIO;
		bp->b_birth = txg;
		zio_buf_blkno_insert(bp, state);
		VM_OBJECT_LOCK(object);
		if (vm_pages_valid_locked(object, blkno, size)) {
			for (i = 0; i < bp->b_npages; i++)
				vm_page_free(bp->b_pages[i]);
			vm_object_reference_pages(object, bp);
		} else
			zio_buf_vm_object_insert(bp, vp, object, FALSE);
		VM_OBJECT_UNLOCK(object);
	}

	return (io_bypass);
}

void
_zio_cache_valid(void *data, uint64_t size)
{
	buf_t bp;
	int i;

	bp = zio_buf_va_lookup(data, size);
	for (i = 0; i < bp->b_npages; i++) 
		bp->b_pages[i]->valid = VM_PAGE_BITS_ALL;
	bp->b_flags &= ~B_INVAL;
	bp->b_flags |= B_CACHE;
}

static void
zfs_bio_shutdown(void *arg __unused, int howto __unused)
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
zfs_bio_init(void)
{

	if (zfs_page_cache_disable)
		return;

	buf_init();

	zfs_bio_event_shutdown = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    zfs_bio_shutdown, NULL, EVENTHANDLER_PRI_FIRST);
}

void
zfs_bio_fini(void)
{
	if (zfs_bio_event_shutdown != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, zfs_bio_event_shutdown);
}


#else /* !_KERNEL */

void *
zio_getblk(uint64_t size)
{
	return (zio_buf_alloc(size));
}

void
zio_data_getblk(uint64_t size)
{

	return (zio_data_buf_alloc(size));
}

void
zio_relse(void *data, size_t size)
{

	zio_buf_free(data, size);
}

void
zio_sync_cache(spa_t *spa, blkptr_t *bp, uint64_t txg, uint64_t size)
{
	;
}
#endif

