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
tracks bufs allocated for the SPA in two splay trees.

The first splay tree tracks bufs by the data pointer's virtual address.
It is used for malloc'ed buffers, and buffers that are VMIO but do not have
any pages in the SPA's vm object(s).

Buffers are malloced if:
    1) the size is not a multiple of PAGE_SIZE
    2) the buffer is cloned

There are two reasons why a VMIO buf would not have any pages in the vm object:
    1) the buffer has not yet been assigned an address on disk (and thus
       has no offset in the vm object)
    2) the buffer did have pages in the vm object, but they were evicted
       and replaced by a newer 

The second splay tree tracks buffers by block address and is only used
to track buffers whose pages are referenced by the vm object. It is used to
ensure that buffers that belong to an older transaction group don't have their
pages mapped by buffers belonging to a newer transaction group.

zfs_bio assumes that buffers that are cloned and buffers whose pages
are evicted from the vm object are not used for I/O (will not be referenced
from zfs_bio_sync_cache).

Pages in the vm object are marked valid on completion of a read or before the
initiation of a write.



There are two places where we synchronize the ARC with the vm object's
page cache: getblk and sync_cache.

In getblk for a malloced buffer we check if the page at the corresponding offset
is valid, if it is map it in and copy it in to the new buffer. For a VMIO buffer
we need to remove the pages for any existing overlapping buffers and free any
other pages in the vm object.

In sync_cache for a malloced buffer we need to evict pages belonging to overlapping
VMIO buffers, then copy to/from any pages still in the vm object. For an unmapped
VMIO buffer, we need to remove pages belonging to any existing buffers and free
any remaining overlapping pages in the vm object. We then add the VMIO buffers
pages to a VM object. If the buffer is already mapped we mark the pages valid on a
write, on a read we set a flag in the zio and mark the pages valid before calling
the io_done I/O completion function.


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

#define	BUF_EMPTY(buf)						\
	((buf)->b_dva.dva_word[0] == 0 &&			\
	(buf)->b_dva.dva_word[1] == 0 &&			\
	(buf)->b_birth == 0)

SYSCTL_DECL(_vfs_zfs);
int zfs_page_cache_disable = 1;
TUNABLE_INT("vfs.zfs.page_cache_disable", &zfs_page_cache_disable);
SYSCTL_INT(_vfs_zfs, OID_AUTO, page_cache_disable, CTLFLAG_RDTUN,
    &zfs_page_cache_disable, 0, "Disable backing ARC with page cache ");

static eventhandler_tag zfs_bio_event_shutdown = NULL;
struct zio_state;
typedef struct zio_state	zio_state_t;
typedef	struct buf		buf_t;

MALLOC_DEFINE(M_ZFS_BIO, "zfs_bio", "zfs buffer cache / vm");

#define	B_EVICTED	B_00000800
#define	B_DATA		B_00001000
#define	B_ASSIGNED	B_00004000	

#define	ZB_EVICT_ALL		0x1
#define	ZB_EVICT_BUFFERED	0x2

#define	ZB_COPYIN		0x2
#define	ZB_COPYOUT		0x3

#define	NO_TXG			0x0

#define btos(nbytes)	((nbytes)>>DEV_BSHIFT)
#define stob(nsectors)	((nsectors)<<DEV_BSHIFT) 

#define b_state			b_fsprivate3

struct zio_state {
	struct mtx 	mtx;
	buf_t 		*blkno_root;		/* track buf by blkno 		*/
	buf_t 		*va_root;		/* track buf by data address 	*/
	spa_t		*spa;
	int		generation;
	int		resident_count;
	TAILQ_HEAD(, buf) blkno_memq;	/* list of resident buffers */
	TAILQ_HEAD(, buf) va_memq;	/* list of resident buffers */	
};

static zio_state_t global_state;

#define ZIO_STATE_LOCK(zs)	mtx_lock(&(zs)->mtx)
#define	ZIO_STATE_UNLOCK(zs)	mtx_unlock(&(zs)->mtx)

#define	spa_get_zio_state(spa)		((zio_state_t *)spa_get_vnode((spa))->v_data)
#define	spa_get_vm_object(spa)		spa_get_vnode((spa))->v_object
#define	zio_buf_get_spa(bp)		(((zio_state_t *)bp->b_state)->spa)
#define	zio_buf_get_vm_object(bp)	spa_get_vm_object(zio_buf_get_spa((bp)))

static void zio_buf_blkno_remove(buf_t *bp);
static void zio_buf_va_insert(buf_t *bp);

/*
 *	zio_buf_blkno_splay:		[ internal use only ]
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the buf containing the given lblkno.  If, however, that
 *	lblkno is not found in the tree, returns a buf that is
 *	adjacent to the pindex, coming before or after it.
 */
static buf_t *
zio_buf_blkno_splay(daddr_t blkno, buf_t *root)
{
	buf_t dummy;
	buf_t *lefttreemax, *righttreemin, *y;
	
	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = &dummy;
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

static buf_t *
zio_buf_va_splay(caddr_t va, buf_t *root)
{
	buf_t dummy;
	buf_t *lefttreemax, *righttreemin, *y;
	
	if (root == NULL)
		return (root);
	lefttreemax = righttreemin = &dummy;
	for (;; root = y) {
		if (va < root->b_data) {
			if ((y = root->b_left) == NULL)
				break;
			if (va < y->b_data) {
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
		} else if (va > root->b_data) {
			if ((y = root->b_right) == NULL)
				break;
			if (va > y->b_data) {
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
zio_buf_blkno_insert(buf_t *bp, zio_state_t *object)
{
	buf_t *root;
	daddr_t root_blkno_end, blkno, blkno_end;

	blkno = bp->b_blkno;
	blkno_end = bp->b_blkno + btos(bp->b_bcount);

	root = object->blkno_root;
	if (root == NULL) {
		bp->b_left = NULL;
		bp->b_right = NULL;
		TAILQ_INSERT_TAIL(&object->blkno_memq, bp, b_bobufs);
	} else {
		root = zio_buf_blkno_splay(bp->b_blkno, root);
		root_blkno_end = root->b_blkno + btos(root->b_bcount);

		if (blkno < root->b_blkno) {
			KASSERT(blkno_end <= root->b_blkno, ("buffer overlap!"));
			bp->b_left = root->b_left;
			bp->b_right = root;
			root->b_left = NULL;
			TAILQ_INSERT_BEFORE(root, bp, b_bobufs);
		} else if (blkno == root->b_blkno) {
			panic("zio_buf_blkno_insert: blkno already allocated");
		} else {
			KASSERT(root_blkno_end <= blkno, ("buffer overlap!"));

			bp->b_right = root->b_right;
			bp->b_left = root;
			root->b_right = NULL;
			TAILQ_INSERT_AFTER(&object->blkno_memq, root, bp, b_bobufs);
		}
	}
	object->blkno_root = bp;
	object->generation++;

	/*
	 * show that the object has one more resident buffer.
	 */
	object->resident_count++;
}

/*
 *	zio_buf_insert:		[ internal use only ]
 *
 *	Inserts the given buf into the state splay tree and state list.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
static void
zio_buf_va_insert(buf_t *bp)
{
	buf_t *root;
	caddr_t va = bp->b_data;
	zio_state_t *object = &global_state;

	root = object->va_root;
	if (root == NULL) {
		bp->b_left = NULL;
		bp->b_right = NULL;
		TAILQ_INSERT_TAIL(&object->va_memq, bp, b_bobufs);
	} else {
		root = zio_buf_va_splay(bp->b_data, root);
		if (va < root->b_data) {
			bp->b_left = root->b_left;
			bp->b_right = root;
			root->b_left = NULL;
			TAILQ_INSERT_BEFORE(root, bp, b_bobufs);
		} else if (va == root->b_data) {
			panic("zio_buf_va_insert: address already allocated");
		} else {
			bp->b_right = root->b_right;
			bp->b_left = root;
			root->b_right = NULL;
			TAILQ_INSERT_AFTER(&object->va_memq, root, bp, b_bobufs);
		}
	}
	object->va_root = bp;
	object->generation++;

	/*
	 * show that the object has one more resident buffer.
	 */
	object->resident_count++;
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
zio_buf_blkno_remove(buf_t *bp)
{
	zio_state_t *state;
	buf_t *root;
	daddr_t blkno, blkno_end;

	if ((state = bp->b_state) == NULL)
		return;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (bp != state->blkno_root)
		zio_buf_blkno_splay(bp->b_blkno, state->blkno_root);
	if (bp->b_left == NULL)
		root = bp->b_right;
	else {
		root = zio_buf_blkno_splay(bp->b_blkno, bp->b_left);
		root->b_right = bp->b_right;
	}
	state->blkno_root = root;
	TAILQ_REMOVE(&state->blkno_memq, bp, b_bobufs);

	/*
	 * And show that the object has one fewer resident page.
	 */
	state->resident_count--;
	state->generation++;
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
static void
zio_buf_va_remove(buf_t *bp)
{
	zio_state_t *state;
	buf_t *root;
	vm_offset_t va;

	if ((state = bp->b_state) == NULL)
		return;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (bp != state->va_root)
		zio_buf_va_splay(bp->b_data, state->va_root);
	if (bp->b_left == NULL)
		root = bp->b_right;
	else {
		root = zio_buf_va_splay(bp->b_data, bp->b_left);
		root->b_right = bp->b_right;
	}
	state->va_root = root;
	TAILQ_REMOVE(&state->va_memq, bp, b_bobufs);

	/*
	 * And show that the object has one fewer resident page.
	 */
	state->resident_count--;
	state->generation++;
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
static buf_t *
zio_buf_va_lookup(caddr_t va)
{
	buf_t *bp;

	if ((bp = global_state.va_root) != NULL && bp->b_data != va) {
		bp = zio_buf_va_splay(va, bp);
		if ((global_state.va_root = bp)->b_data != va)
			bp = NULL;
	}
	return (bp);
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
static buf_t *
zio_buf_blkno_lookup(zio_state_t *state, daddr_t blkno)
{
	buf_t *bp;

	if ((bp = state->blkno_root) != NULL && bp->b_blkno != blkno) {
		bp = zio_buf_blkno_splay(blkno, bp);
		if ((state->blkno_root = bp)->b_blkno != blkno)
			bp = NULL;
	}
	return (bp);
}

static void
zio_buf_vm_object_copy(buf_t *bp, int direction)
{
	vm_object_t object;
	vm_pindex_t start, end;
	vm_offset_t offset;
	uint64_t byte_offset;
	vm_offset_t page_offset;
	int i, size;	
	caddr_t va;	
	vm_page_t m;
	struct sf_buf *sf;

	object = zio_buf_get_vm_object(bp);
	byte_offset = stob(bp->b_blkno);
	page_offset = byte_offset & PAGE_MASK;
	start = OFF_TO_IDX(byte_offset);
	end = OFF_TO_IDX(byte_offset + bp->b_bcount);

	VM_OBJECT_LOCK(object);	
	for (bp->b_npages = i = 0; start + i < end; i++) {
		m = vm_page_lookup(object, start + i);

		if ((m == NULL) || (m->valid != VM_PAGE_BITS_ALL))
			goto done;

		bp->b_pages[i] = m;		
		bp->b_npages++;
	}
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
zio_buf_vm_object_copyout(buf_t *bp)
{
	
	zio_buf_vm_object_copy(bp, ZB_COPYOUT);
}

static void
zio_buf_vm_object_copyin(buf_t *bp)
{
	
	zio_buf_vm_object_copy(bp, ZB_COPYIN);
}

static void
zio_buf_vm_object_evict(buf_t *bp)
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
zio_buf_vm_object_insert_locked(buf_t *bp, struct vnode *vp,
    vm_object_t object, int valid)
{
	vm_page_t m;
	vm_pindex_t start = OFF_TO_IDX(stob(bp->b_blkno));
	int i;

	/*
	 * Insert buffer pages in the object
	 */
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

static void
zio_buf_vm_object_insert(buf_t *bp, int valid)
{
	spa_t *spa = zio_buf_get_spa(bp);
	struct vnode *vp = spa_get_vnode(spa);
	vm_object_t object = vp->v_object;

	VM_OBJECT_LOCK(object);
	zio_buf_vm_object_insert_locked(bp, vp, object, valid);
	VM_OBJECT_UNLOCK(object);
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
zio_buf_evict_overlap_locked(daddr_t blkno, int size, zio_state_t *state,
    uint64_t txg, int evict_op, vm_object_t object)
{
	buf_t *root, *tmpbp;
	daddr_t blkno_end, tmpblkno, tmpblkno_end;
	struct cluster_list_head clh;
	int i, collisions;
	uint64_t tmptxg;
	vm_pindex_t start, end;

	if ((root = state->blkno_root) == NULL)
		goto done;

	collisions = 0;
	root = zio_buf_blkno_splay(blkno, root);
	TAILQ_INIT(&clh);
	if (blkno < root->b_blkno)
		tmpbp = TAILQ_PREV(root, cluster_list_head, b_bobufs);

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
		tmpbp = TAILQ_NEXT(tmpbp, b_bobufs);
	}
	while (!TAILQ_EMPTY(&clh)) {
		tmpbp = TAILQ_FIRST(&clh);
		TAILQ_REMOVE(&clh, tmpbp, b_freelist);
		zio_buf_vm_object_evict(tmpbp);

		KASSERT(tmpbp->b_flags & B_EVICTED == 0,
		    ("buffer has already been evicted"));
		tmpbp->b_flags |= B_EVICTED;
		state->blkno_root = tmpbp;
		/*
		 * move buffer to the unmanaged tree
		 */
		zio_buf_blkno_remove(tmpbp);
		zio_buf_va_insert(tmpbp);
	}
done:
	if (!(collisions == 1 && tmpbp->b_blkno == blkno && tmpbp->b_bcount == size)
	    && (evict_op == ZB_EVICT_ALL)) {
		start = OFF_TO_IDX(stob(blkno));
		end = start + OFF_TO_IDX(size);
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

static void
zio_buf_evict_overlap(daddr_t blkno, int size, zio_state_t *state,
    uint64_t txg, int evict_op)
{
	vm_object_t	object = spa_get_vm_object(state->spa);

	VM_OBJECT_LOCK(object);
	zio_buf_evict_overlap_locked(blkno, size, state, txg, evict_op, object);
	VM_OBJECT_UNLOCK(object);
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

static buf_t *
_zio_getblk_malloc(uint64_t size, int flags)
{
	buf_t 		*newbp;
	void 		*data;

	if (flags & GB_NODUMP) 
		data = _zio_data_buf_alloc(size);
	else
		data = _zio_buf_alloc(size);
	newbp = malloc(sizeof(struct buf), M_ZFS_BIO, M_WAITOK|M_ZERO);
	newbp->b_data = data;
	newbp->b_flags = (B_MALLOC|B_INVAL);
	newbp->b_bcount = size;
}

static buf_t *
_zio_getblk_vmio(uint64_t size, int flags)
{
	buf_t 		*newbp;

	newbp = geteblk(size, flags);
	BUF_KERNPROC(newbp);

	return (newbp);
}

void *
zio_getblk(uint64_t size, int flags)
{
	buf_t 		*newbp;

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
	buf_t *bp;

	bp = zio_buf_va_lookup(data);
	zio_buf_va_remove(bp);

	if (bp->b_flags & B_ASSIGNED)
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
    uint64_t size, zio_type_t bio_op)
{
	buf_t		*bp;
	zio_state_t 	*state = spa_get_zio_state(spa);
	dva_t		dva = *BP_IDENTITY(blkp);
	daddr_t		blkno = dva.dva_word[1] & ~(1ULL<<63);
	struct vnode	*vp = spa_get_vnode(spa);
	vm_object_t	object = vp->v_object;
	vm_pindex_t	start;
	vm_page_t	m;	
	int i, io_bypass = FALSE;

	/*
	 * XXX incomplete
	 */

	
	if ((bp = zio_buf_va_lookup(data)) != NULL) {
		KASSERT(bp->b_flags & B_EVICTED == 0,
		    ("doing I/O with cloned or evicted buffer 0x%x", bp->b_flags));

		if (bp->b_flags & B_MALLOC) {
			zio_buf_evict_overlap(blkno, size, state, txg, ZB_EVICT_BUFFERED);

			if (bio_op == BIO_READ) {
				/*
				 * if page resident - copy in
				 * update zio pipeline
				 */
				zio_buf_vm_object_copyin(bp);
				if (bp->b_flags & B_CACHE) {
					/* update zio pipeline */
					io_bypass = TRUE;
				}
			} else {
				zio_buf_vm_object_copyout(bp);
			}
		} else {
			zio_buf_va_remove(bp);
			VM_OBJECT_LOCK(object);
			zio_buf_evict_overlap_locked(blkno, size, state, NO_TXG,
			    ZB_EVICT_ALL, object);
			bp->b_blkno = bp->b_lblkno = blkno;
			bp->b_flags |= (B_VMIO|B_ASSIGNED);
			zio_buf_blkno_insert(bp, state);
			zio_buf_vm_object_insert_locked(bp, vp, object, bio_op == BIO_WRITE);
			VM_OBJECT_UNLOCK(object);
		}
	} else {
		bp = zio_buf_blkno_lookup(state, blkno);
		if (bio_op == BIO_READ && (bp->b_flags & (B_CACHE|B_INVAL)) == B_CACHE)
			io_bypass = TRUE;
		KASSERT(bp != NULL, ("blkno=%ld data=%p unmanaged", blkno, bp->b_data));
	}

	return (io_bypass);
}

void
_zio_cache_valid(void *data, uint64_t size)
{

	
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

