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
per-SPA vm object. Each SPA also has an associated "zbio_state_t" which
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
#include <sys/zfs_bio.h>
#include <sys/refcount.h>
#include <sys/vdev.h>
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/sdt.h>

#include <sys/bitstring.h>
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
struct zbio_state;
typedef struct zbio_state	zbio_state_t;
typedef	struct buf		buf_t;
typedef	uint64_t		zbio_pindex_t;

MALLOC_DEFINE(M_ZFS_BIO, "zfs_bio", "zfs buffer cache / vm");

#define	B_EVICTED	B_00000800
#define	B_CLONED	B_00001000
#define	B_ASSIGNED	B_00004000	

#define	ZB_EVICT_ALL	0x1

#define btos(nbytes)	((nbytes)>>DEV_BSHIFT)
#define stob(nsectors)	((nsectors)<<DEV_BSHIFT) 

#define b_arc_buf		b_fsprivate2
#define b_state			b_fsprivate3

struct zbio_state {
	struct mtx 	mtx;
	buf_t 		*blkno_root;		/* track buf by blkno 		*/
	buf_t 		*va_root;		/* track buf by data address 	*/
	spa_t		*spa;
	int		generation;
	int		resident_count;
	TAILQ_HEAD(, buf) blkno_memq;	/* list of resident buffers */
	TAILQ_HEAD(, buf) va_memq;	/* list of resident buffers */	
};

#define ZBIO_STATE_LOCK(zs)	mtx_lock(&(zs)->mtx)
#define	ZBIO_STATE_UNLOCK(zs)	mtx_unlock(&(zs)->mtx)

#define	spa_get_bio_state(spa)	((zbio_state_t *)spa_get_vnode((spa))->v_data)
#define	spa_get_vm_object(spa)	spa_get_vnode((spa))->v_object
#define	zbio_buf_get_spa(bp)	(((zbio_buf_hdr_t *)((arc_buf_t *)(bp->b_arc_buf))->b_hdr)->b_spa)

static void zbio_buf_blkno_remove(buf_t *bp);
static void zbio_buf_va_insert(buf_t *bp, zbio_state_t *object);

/*
 *	zbio_buf_blkno_splay:		[ internal use only ]
 *
 *	Implements Sleator and Tarjan's top-down splay algorithm.  Returns
 *	the buf containing the given lblkno.  If, however, that
 *	lblkno is not found in the tree, returns a buf that is
 *	adjacent to the pindex, coming before or after it.
 */
static buf_t *
zbio_buf_blkno_splay(daddr_t blkno, buf_t *root)
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
zbio_buf_va_splay(caddr_t va, buf_t *root)
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
 *	zbio_buf_blkno_insert:		[ internal use only ]
 *
 *	Inserts the given buf into the state splay tree and state list.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
static void
zbio_buf_blkno_insert(buf_t *bp, zbio_state_t *object)
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
		root = zbio_buf_blkno_splay(bp->b_blkno, root);
		root_blkno_end = root->b_blkno + btos(root->b_bcount);

		if (blkno < root->b_blkno) {
			KASSERT(blkno_end <= root->b_blkno, ("buffer overlap!"));
			bp->b_left = root->b_left;
			bp->b_right = root;
			root->b_left = NULL;
			TAILQ_INSERT_BEFORE(root, bp, b_bobufs);
		} else if (blkno == root->b_blkno) {
			panic("zbio_buf_blkno_insert: blkno already allocated");
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
 *	zbio_buf_insert:		[ internal use only ]
 *
 *	Inserts the given buf into the state splay tree and state list.
 *
 *	The object and page must be locked.
 *	This routine may not block.
 */
static void
zbio_buf_va_insert(buf_t *bp, zbio_state_t *object)
{
	buf_t *root;
	caddr_t va = bp->b_data;

	bp->b_state = object;
	root = object->va_root;
	if (root == NULL) {
		bp->b_left = NULL;
		bp->b_right = NULL;
		TAILQ_INSERT_TAIL(&object->va_memq, bp, b_bobufs);
	} else {
		root = zbio_buf_va_splay(bp->b_data, root);
		if (va < root->b_data) {
			bp->b_left = root->b_left;
			bp->b_right = root;
			root->b_left = NULL;
			TAILQ_INSERT_BEFORE(root, bp, b_bobufs);
		} else if (va == root->b_data) {
			panic("zbio_buf_va_insert: address already allocated");
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
 *	zbio_buf_remove:
 *
 *	Removes the given buf from the spa's state tree
 *	buf list
 *
 *	The state and buf must be locked.
 *	This routine may not block.
 */
static void
zbio_buf_blkno_remove(buf_t *bp)
{
	zbio_state_t *state;
	buf_t *root;
	daddr_t blkno, blkno_end;

	if ((state = bp->b_state) == NULL)
		return;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (bp != state->blkno_root)
		zbio_buf_blkno_splay(bp->b_blkno, state->blkno_root);
	if (bp->b_left == NULL)
		root = bp->b_right;
	else {
		root = zbio_buf_blkno_splay(bp->b_blkno, bp->b_left);
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
 *	zbio_buf_va_remove:
 *
 *	Removes the given buf from the spa's state tree
 *	buf list
 *
 *	The state and buf must be locked.
 *	This routine may not block.
 */
static void
zbio_buf_va_remove(buf_t *bp)
{
	zbio_state_t *state;
	buf_t *root;
	vm_offset_t va;

	if ((state = bp->b_state) == NULL)
		return;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	if (bp != state->va_root)
		zbio_buf_va_splay(bp->b_data, state->va_root);
	if (bp->b_left == NULL)
		root = bp->b_right;
	else {
		root = zbio_buf_va_splay(bp->b_data, bp->b_left);
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
 *	zbio_buf_va_lookup:
 *
 *	Returns the range associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
static buf_t *
zbio_buf_va_lookup(zbio_state_t *state, caddr_t va)
{
	buf_t *bp;

	if ((bp = state->va_root) != NULL && bp->b_data != va) {
		bp = zbio_buf_va_splay(va, bp);
		if ((state->va_root = bp)->b_data != va)
			bp = NULL;
	}
	return (bp);
}


/*
 *	zbio_buf_blkno_lookup:
 *
 *	Returns the range associated with the object/offset
 *	pair specified; if none is found, NULL is returned.
 *
 *	The object must be locked.
 *	This routine may not block.
 *	This is a critical path routine
 */
static buf_t *
zbio_buf_blkno_lookup(zbio_state_t *state, daddr_t blkno)
{
	buf_t *bp;

	if ((bp = state->blkno_root) != NULL && bp->b_blkno != blkno) {
		bp = zbio_buf_blkno_splay(blkno, bp);
		if ((state->blkno_root = bp)->b_blkno != blkno)
			bp = NULL;
	}
	return (bp);
}

static void
zbio_buf_vm_object_copyin(buf_t *bp)
{

	
}

static void
zbio_buf_vm_object_copyout(buf_t *bp)
{

	
}

static void
zbio_buf_vm_object_evict(buf_t *bp)
{
	int i;

	/*
	 * remove pages from backing vm_object 
	 */
	for (i = 0; i < bp->b_npages; i++) 
		vm_page_remove(bp->b_pages[i]);
}

static void
zbio_buf_vm_object_insert(buf_t *bp, int valid)
{
	vm_page_t m;
	vm_pindex_t start = OFF_TO_IDX(stob(bp->b_blkno));
	spa_t *spa = zbio_buf_get_spa(bp);
	struct vnode *vp = spa_get_vnode(spa);
	struct vm_object *object = vp->v_object;
	int i;

	VM_OBJECT_LOCK(object);
	/*
	 * Insert buffer pages in the object
	 */
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if (valid)
			m->valid = VM_PAGE_BITS_ALL;
		vm_page_insert(m, object, start + i);
		m->flags &= ~PG_UNMANAGED;
		vdrop(vp);
	}
	vm_page_lock_queues();
	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		vm_page_enqueue(PQ_INACTIVE, m);
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
	
}

/*
 *	zbio_buf_evict_overlap:		[ internal use only ]
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
zbio_buf_blkno_evict_overlap(daddr_t blkno, int size, zbio_state_t *state,
    uint64_t txg, int evict_op, int locked)
{
	buf_t *root, *tmpbp;
	daddr_t blkno_end, tmpblkno, tmpblkno_end;
	struct cluster_list_head clh;
	int i, collisions;
	uint64_t tmptxg;
	vm_pindex_t start, end;
	vm_object_t 	object = spa_get_vm_object(state->spa);

	if (!locked)
		VM_OBJECT_LOCK(object);
	if ((root = state->blkno_root) == NULL)
		goto done;

	collisions = 0;
	root = zbio_buf_blkno_splay(blkno, root);
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
		tmptxg = ((zbio_buf_hdr_t *)((arc_buf_t *)tmpbp->b_arc_buf)->b_hdr)->b_birth;
		
		if (((tmpblkno >= blkno) && (tmpblkno < blkno_end)) ||
		    (tmpblkno_end > blkno) && (tmpblkno_end <= blkno_end) &&
		    ((txg == 0) || (tmptxg < txg))) {
			TAILQ_INSERT_TAIL(&clh, tmpbp, b_freelist);
			collisions++;
		}
		tmpbp = TAILQ_NEXT(tmpbp, b_bobufs);
	}
	while (!TAILQ_EMPTY(&clh)) {
		tmpbp = TAILQ_FIRST(&clh);
		TAILQ_REMOVE(&clh, tmpbp, b_freelist);
		zbio_buf_vm_object_evict(tmpbp);

		KASSERT(tmpbp->b_flags & B_EVICTED == 0,
		    ("buffer has already been evicted"));
		tmpbp->b_flags |= B_EVICTED;
		state->blkno_root = tmpbp;
		/*
		 * move buffer to the unmanaged tree
		 */
		zbio_buf_blkno_remove(tmpbp);
		zbio_buf_va_insert(tmpbp, state);
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
	if (!locked)
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
_zbio_getblk_malloc(zbio_buf_hdr_t *hdr, int flags)
{
	buf_t 		*newbp, *tmpbp;
	void 		*data;
	daddr_t 	blkno;
	uint64_t	size = hdr->b_size;
	uint64_t	txg = hdr->b_birth;
	zbio_state_t	*state = spa_get_bio_state(hdr->b_spa);

	if (flags & GB_NODUMP) 
		data = zio_data_buf_alloc(size);
	else
		data = zio_buf_alloc(size);
	newbp = malloc(sizeof(struct buf), M_ZFS_BIO, M_WAITOK|M_ZERO);
	newbp->b_data = data;
	newbp->b_flags = (B_MALLOC|B_INVAL);
	newbp->b_bcount = size;
	if (!BUF_EMPTY(hdr) && !(hdr->b_flags & ZBIO_BUF_CLONING)) {
		blkno = hdr->b_dva.dva_word[1] & ~(1ULL<<63);
		zbio_buf_blkno_evict_overlap(blkno, size, state, txg, 0, FALSE);
		newbp->b_blkno = blkno;
		/*
		 * Copy in from the page cache if found & valid
		 * and mark B_CACHE
		 */
		zbio_buf_vm_object_copyin(newbp);
	}

	if (hdr->b_flags & ZBIO_BUF_CLONING) {
		newbp->b_flags |= B_CLONED;
		hdr->b_flags &= ~ZBIO_BUF_CLONING;
	}
	zbio_buf_va_insert(newbp, state);
}

static buf_t *
_zbio_getblk_vmio(zbio_buf_hdr_t *hdr, int flags)
{
	buf_t 		*newbp;
	daddr_t 	blkno;
	uint64_t	size = hdr->b_size;
	spa_t		*spa = hdr->b_spa;
	zbio_state_t	*state = spa_get_bio_state(spa);
	struct vnode 	*vp = spa_get_vnode(spa);
	struct bufobj	*bo = &vp->v_bufobj;

	if (BUF_EMPTY(hdr)) {
		newbp = geteblk(size, flags);
		zbio_buf_va_insert(newbp, state);
	} else {
		blkno = hdr->b_dva.dva_word[1] & ~(1ULL<<63);
		zbio_buf_blkno_evict_overlap(blkno, size, state, 0, 0, FALSE);

		while (newbp == NULL)
			newbp = getblk(vp, blkno, size, 0, 0, flags | GB_LOCK_NOWAIT);
		brelvp(newbp);
		newbp->b_flags |= B_ASSIGNED;
		zbio_buf_blkno_insert(newbp, state);
	}
	newbp->b_bufobj = bo;
	BUF_KERNPROC(newbp);
	CTR4(KTR_SPARE2, "arc_getblk() bp=%p flags %X "
	    "blkno %ld npages %d",
	    newbp, newbp->b_flags, blkno, newbp->b_npages);

	return (newbp);
}

static void
_zbio_getblk(arc_buf_t *buf, int flags)
{
	zbio_buf_hdr_t		*hdr = (zbio_buf_hdr_t *)buf->b_hdr;
	uint64_t		size = hdr->b_size;	
	buf_t 			*newbp;

	if (zfs_page_cache_disable) {		
		buf->b_data = zio_buf_alloc(size);
		hdr->b_flags &= ~ZBIO_BUF_CLONING;
		return;
	}

	if ((size & PAGE_MASK) || (hdr->b_flags & ZBIO_BUF_CLONING))
		newbp = _zbio_getblk_malloc(hdr, flags);
	else
		newbp = _zbio_getblk_vmio(hdr, flags);

	buf->b_bp = newbp;
	buf->b_data = newbp->b_data;
	newbp->b_arc_buf = buf;
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

	if (zfs_page_cache_disable) {
		zio_buf_free(buf->b_data, size);
		return;
	}

	if (bp->b_flags & B_ASSIGNED)
		zbio_buf_blkno_remove(bp);
	else
		zbio_buf_va_remove(bp);

	if (bp->b_flags & B_MALLOC) {
		zio_buf_free(bp->b_data, size);
		free(bp, M_ZFS_BIO);
	} else {
		CTR4(KTR_SPARE2, "arc_brelse() bp=%p flags %X"
		    " size %ld blkno=%ld",
		    bp, bp->b_flags, size, bp->b_blkno);

		bp->b_flags |= B_ZFS;
		brelse(bp);
	}
}

void
zbio_sync_cache(spa_t *spa, blkptr_t *blkp, uint64_t txg, void *data, uint64_t size, int bio_op)
{
	buf_t		*bp;
	zbio_state_t 	*state = spa_get_bio_state(spa);
	dva_t		dva = *BP_IDENTITY(blkp);
	daddr_t		blkno = dva.dva_word[1] & ~(1ULL<<63);
	struct vnode	*vp = spa_get_vnode(spa);
	vm_object_t	object = vp->v_object;
	vm_pindex_t	start;
	vm_page_t	m;	
	int i;

	if (zfs_page_cache_disable)
		return;
	/*
	 * XXX incomplete
	 */

	
	if ((bp = zbio_buf_va_lookup(state, data)) != NULL) {
		KASSERT(bp->b_flags & (B_CLONED|B_EVICTED) == 0,
		    ("doing I/O with cloned or evicted buffer 0x%x", bp->b_flags));

		if (bp->b_flags & B_MALLOC) {
			zbio_buf_blkno_evict_overlap(blkno, size, state, txg, 0, FALSE);

			if (bio_op == BIO_READ) {
				/*
				 * if page resident - copy in
				 * update zio pipeline
				 */
				zbio_buf_vm_object_copyin(bp);
				if (bp->b_flags & B_CACHE) {
					/* update zio pipeline */
				}
			} else
				zbio_buf_vm_object_copyout(bp);
		} else {
			zbio_buf_blkno_evict_overlap(blkno, size, state, 0, ZB_EVICT_ALL, TRUE);
			bp->b_blkno = bp->b_lblkno = blkno;
			bp->b_flags |= (B_VMIO|B_ASSIGNED);
			zbio_buf_vm_object_insert(bp, bio_op == BIO_WRITE);
		}
	} else {
		bp = zbio_buf_blkno_lookup(state, blkno);
		KASSERT(bp != NULL, ("blkno=%ld data=%p unmanaged", blkno, bp->b_data));
	}
}

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

	if (zfs_page_cache_disable)
		return;

	zbio_event_shutdown = EVENTHANDLER_REGISTER(shutdown_pre_sync,
	    zbio_shutdown, NULL, EVENTHANDLER_PRI_FIRST);
}

void
zbio_fini(void)
{
	if (zbio_event_shutdown != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_pre_sync, zbio_event_shutdown);
}


#else /* !_KERNEL */

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

