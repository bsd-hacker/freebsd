/*-
 * Copyright (c) 2008 Jeffrey Roberson <jeff@FreeBSD.org>
 * Copyright (c) 2009 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufobj.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>

/*
 * XXXKIB TODO
 *
 * 2. VOP_REALLOCBLKS.
 * 3. Unset setuid/setgid bits after write.
 * 4. Filesystem full handling.
 *
 */

static SYSCTL_NODE(_vfs, OID_AUTO, vmio, CTLFLAG_RW, 0, "VFS VMIO leaf");

static int vmio_run = 0;
SYSCTL_INT(_vfs_vmio, OID_AUTO, run, CTLFLAG_RW, &vmio_run, 0,
    "Calculate the max sequential run for vnode_pager_read_cluster");
static int vmio_clrbuf = 1;
SYSCTL_INT(_vfs_vmio, OID_AUTO, clrbuf, CTLFLAG_RW, &vmio_clrbuf, 0,
    ""); /* Intentionally undocumented */
static int vmio_read_pack = 16;
SYSCTL_INT(_vfs_vmio, OID_AUTO, read_pack, CTLFLAG_RW, &vmio_read_pack, 0,
    "Length of the page pack for read");
static int vmio_write_pack = 16;
SYSCTL_INT(_vfs_vmio, OID_AUTO, write_pack, CTLFLAG_RW, &vmio_write_pack,
    0,
    "Length of the page pack for write");
static int vmio_rollbacks1;
SYSCTL_INT(_vfs_vmio, OID_AUTO, rollbacks1, CTLFLAG_RD, &vmio_rollbacks1,
    0,
    "Count of times vnode size has to be rolled back for writes "
    "while collecting pages");
static int vmio_rollbacks2;
SYSCTL_INT(_vfs_vmio, OID_AUTO, rollbacks2, CTLFLAG_RD, &vmio_rollbacks2,
    0,
    "Count of times vnode size has to be rolled back for writes "
    "while reading pages");
static int vmio_getpages_read;
SYSCTL_INT(_vfs_vmio, OID_AUTO, getpages_read, CTLFLAG_RD,
    &vmio_getpages_read, 0,
    "Count of times VOP_GETPAGES called for read");
static int vmio_getpages_write;
SYSCTL_INT(_vfs_vmio, OID_AUTO, getpages_write, CTLFLAG_RD,
    &vmio_getpages_write, 0,
    "Count of times VOP_GETPAGES called for write");
static int vmio_reserv_used;
SYSCTL_INT(_vfs_vmio, OID_AUTO, reserv_used, CTLFLAG_RD,
    &vmio_reserv_used, 0,
    "Count of times reserved page was used by vmio");
static int vmio_alloc_wait;
SYSCTL_INT(_vfs_vmio, OID_AUTO, alloc_wait, CTLFLAG_RD, &vmio_alloc_wait,
    0,
    "Count of times vmio reserved page allocation has to wait");
static long vmio_writedirty;
SYSCTL_LONG(_vfs_vmio, OID_AUTO, writedirty, CTLFLAG_RD, &vmio_writedirty,
    0,
    "Count of pages dirtied by vnode_pager_write");
long vmio_max_writedirty;
SYSCTL_LONG(_vfs_vmio, OID_AUTO, max_writedirty, CTLFLAG_RW,
    &vmio_max_writedirty, 0,
    "Maximum allowed system-wide count of pages dirtied by vnode_pager_write");
static int vmio_writed_wakeups;
SYSCTL_INT(_vfs_vmio, OID_AUTO, writed_wakeups, CTLFLAG_RD,
    &vmio_writed_wakeups, 0,
    "Count of times vmio write daemon was woken up");
static int vmio_writed_inact;
SYSCTL_INT(_vfs_vmio, OID_AUTO, writed_inact, CTLFLAG_RD,
    &vmio_writed_inact, 0,
    "Count of times vmio write daemon cleaned inactive queue");
static int vmio_writed_act;
SYSCTL_INT(_vfs_vmio, OID_AUTO, writed_act, CTLFLAG_RD, &vmio_writed_act,
    0,
    "Count of times vmio write daemon cleaned active queue");

static u_int
io_page_bits(int i, vm_offset_t off, ssize_t size)
{
	int start, chunk;

	if (i == 0) {
		start = off;
		chunk = min(PAGE_SIZE - off, size);
	} else if (i * PAGE_SIZE < off + size) {
		start = 0;
		chunk = PAGE_SIZE;
	} else if ((i - 1) * PAGE_SIZE < off + size) {
		start = 0;
		chunk = (size - off) % PAGE_SIZE;
	} else
		return (0);
	return (vm_page_bits(start, chunk));
}

/*
 * Blocking allocator of the reserve page. Cannot be called with vnode
 * or object lock held.
 */
static void
vnode_alloc_reserv(vm_page_t *reserv)
{

	if (*reserv != NULL)
		return;
	while (*reserv == NULL) {
		*reserv = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
		    VM_ALLOC_NOOBJ);
		if (*reserv == NULL) {
			atomic_add_int(&vmio_alloc_wait, 1);
			VM_WAIT;
		}
	}
}

/*
 * Copied from vm_pageout_scan().
 */
static int
vnode_writedirty_clean_page(vm_page_t m, int queue, int *target,
    vm_page_t *next)
{
	vm_object_t object;
	struct mount *mp;
	struct vnode *vp;
	struct vm_page marker;
	int vfslocked, actcount;

	bzero(&marker, sizeof(marker));
	marker.flags = PG_FICTITIOUS | PG_MARKER;
	marker.oflags = VPO_BUSY;
	marker.queue = queue;
	marker.wire_count = 1;

	if (VM_PAGE_GETQUEUE(m) != queue)
		return (0);
	*next = TAILQ_NEXT(m, pageq);
	object = m->object;

	if (m->flags & PG_MARKER)
		return (1);
	if (m->hold_count) {
		vm_page_requeue(m);
		return (1);
	}
	if (!VM_OBJECT_TRYLOCK(object) &&
	    (!vm_pageout_fallback_object_lock(m, next) ||
	     m->hold_count != 0)) {
		VM_OBJECT_UNLOCK(object);
		return (1);
	}
	if (m->busy || (m->oflags & VPO_BUSY) || !(m->flags & PG_WRITEDIRTY)) {
		VM_OBJECT_UNLOCK(object);
		return (1);
	}
	if (object->ref_count == 0) {
		vm_page_flag_clear(m, PG_REFERENCED);
		KASSERT(!pmap_page_is_mapped(m),
		    ("vm_pageout_clean_writedirty: page %p is mapped", m));
	} else if (((m->flags & PG_REFERENCED) == 0) &&
		   (actcount = pmap_ts_referenced(m))) {
		vm_page_activate(m);
		VM_OBJECT_UNLOCK(object);
		m->act_count += (actcount + ACT_ADVANCE);
		return (1);
	}

	if ((m->flags & PG_REFERENCED) != 0) {
		vm_page_flag_clear(m, PG_REFERENCED);
		actcount = pmap_ts_referenced(m);
		vm_page_activate(m);
		VM_OBJECT_UNLOCK(object);
		m->act_count += (actcount + ACT_ADVANCE + 1);
		return (1);
	}

	if (m->dirty != VM_PAGE_BITS_ALL && (m->flags & PG_WRITEABLE) != 0) {
		if (pmap_is_modified(m))
			vm_page_dirty(m);
		else if (m->dirty == 0)
			pmap_remove_all(m);
	}

	KASSERT(m->valid != 0, ("VPO_WRITEDIRTY and not valid %p", m));
	if (m->dirty == 0) {
		m->flags &= ~PG_WRITEDIRTY;
		vmio_writedirty--;
		VM_OBJECT_UNLOCK(object);
		return (1);
	}
	KASSERT(m->dirty != 0, ("VPO_WRITEDIRTY and not dirty %p", m));
	if (object->flags & OBJ_DEAD) {
		VM_OBJECT_UNLOCK(object);
		vm_page_requeue(m);
		return (1);
	}
	KASSERT(object->type == OBJT_VNODE, ("VPO_WRITEDIRTY and not vnode"));

	TAILQ_INSERT_AFTER(&vm_page_queues[queue].pl, m, &marker, pageq);
	vp = object->handle;
	vfslocked = 0;
	if (vp->v_type == VREG && vn_start_write(vp, &mp, V_NOWAIT) != 0) {
		mp = NULL;
		goto unlock_and_continue;
	}
	KASSERT(mp != NULL, ("vp %p with NULL v_mount", vp));
	vm_page_unlock_queues();
	vm_object_reference_locked(object);
	VM_OBJECT_UNLOCK(object);
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if (vget(vp, LK_EXCLUSIVE | LK_TIMELOCK, curthread)) {
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		vp = NULL;
		goto unlock_and_continue;
	}
	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
	if (VM_PAGE_GETQUEUE(m) != queue || m->object != object ||
	    TAILQ_NEXT(m, pageq) != &marker)
		goto unlock_and_continue;
	if (m->busy || (m->oflags & VPO_BUSY))
		goto unlock_and_continue;
	if (m->hold_count) {
		vm_page_requeue(m);
		goto unlock_and_continue;
	}
	if (vm_pageout_clean(m) != 0)
		(*target)--;
 unlock_and_continue:
	VM_OBJECT_UNLOCK(object);
	if (mp != NULL) {
		vm_page_unlock_queues();
		if (vp != NULL)
			vput(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		vm_object_deallocate(object);
		vn_finished_write(mp);
		vm_page_lock_queues();
	}
	*next = TAILQ_NEXT(&marker, pageq);
	TAILQ_REMOVE(&vm_page_queues[queue].pl, &marker, pageq);
	return (1);
}

static void
vnode_writedirty_clean_queue(int *target, int queue)
{
	vm_page_t m, next;

	vm_page_lock_queues();
 rescan0:
	for (m = TAILQ_FIRST(&vm_page_queues[queue].pl);
	     m != NULL && *target > 0; m = next) {
		if (!vnode_writedirty_clean_page(m, queue, target, &next))
			goto rescan0;
	}
	vm_page_unlock_queues();
}

static struct cv wd_speedup;
static struct cv wd_back;

static void
vnode_writedirty_daemon(void)
{
	int target;

	cv_init(&wd_speedup, "writed");
	cv_init(&wd_back, "vnodeww");

	vm_page_lock_queues();
	for (;;) {
		cv_wait(&wd_speedup, &vm_page_queue_mtx);
		target = vmio_writedirty - vmio_max_writedirty;
		vm_page_unlock_queues();
		atomic_add_int(&vmio_writed_wakeups, 1);
		if (target > 0) {
			if (target > 0) {
				bwillwrite();
				atomic_add_int(&vmio_writed_inact, 1);
				vnode_writedirty_clean_queue(&target,
				    PQ_INACTIVE);
			}
			if (target > 0) {
				bwillwrite();
				atomic_add_int(&vmio_writed_act, 1);
				vnode_writedirty_clean_queue(&target,
				    PQ_ACTIVE);
			}
		}
		vm_page_lock_queues();
		vm_writedirty_cleaned(0);
	}
}

void
vm_writedirty_cleaned(int cnt)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	vmio_writedirty -= cnt;
	cv_broadcast(&wd_back);
}

static struct proc *writedproc;
static struct kproc_desc writed_kp = {
	.arg0 = "writed",
	.func = vnode_writedirty_daemon,
	.global_procpp = &writedproc
};
SYSINIT(writed, SI_SUB_KTHREAD_PAGE, SI_ORDER_ANY, kproc_start, &writed_kp);

/*
 * Attempt to put backpressure on writes.
 */
static void
vnode_pager_wwait(void)
{

	if (vmio_writedirty >= vmio_max_writedirty) {
		vm_page_lock_queues();
		while (vmio_writedirty >= vmio_max_writedirty) {
			cv_signal(&wd_speedup);
			cv_wait(&wd_back, &vm_page_queue_mtx);
		}
		vm_page_unlock_queues();
	}
}

#define	VN_GRAB_NO_VMWAIT	0x0001

/*
 * Grab a page, waiting until we are woken up due to the page
 * changing state.  We keep on waiting, if the page continues
 * to be in the object.  If the page doesn't exist allocate it.
 *
 * This routine may block, either waiting for busy vnode page, or for
 * a page allocation. Later may be disabled with VN_GRAB_NO_VMWAIT
 * flag, when vnode lock is held. To ensure progress, reserve page is
 * used for ma[0] when wait is disabled and system cannot provide a
 * page.
 *
 * Returns updated page run length in *wp, and filled in ma page
 * array.
 */
static void
vnode_grab_pages(struct vnode *vp, vm_page_t *ma, int *wp, vm_pindex_t pindex,
    int flags, vm_page_t *reserv)
{
	vm_object_t object;
	vm_page_t m;
	vm_pindex_t pi;
	int i;

	KASSERT((flags & VN_GRAB_NO_VMWAIT) || reserv == NULL,
	    ("vnode_grab_pages: NO_VMWAIT and no reserve"));

	object = vp->v_object;
 redo:
	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	m = NULL;
	for (i = 0, pi = pindex; i < *wp; ) {
		if (i > 0) {
			m = TAILQ_NEXT(ma[i - 1], listq);
			if (m && m->pindex != pi)
				m = NULL;
		}
		if (m == NULL)
			m = vm_page_lookup(object, pi);
		if (m != NULL) {
			if (vm_page_sleep_if_busy(m, TRUE, "pgrnbwt"))
				goto redo;
		} else {
			m = vm_page_alloc(object, pi, VM_ALLOC_NORMAL |
			    VM_ALLOC_NOBUSY);
		}
		if (m != NULL) {
			ma[i] = m;
			i++;
			pi++;
			continue;
		}
		if (flags & VN_GRAB_NO_VMWAIT) {
			if (i == 0) {
				m = *reserv;
				*reserv = NULL;
				atomic_add_int(&vmio_reserv_used, 1);
				if (object->memattr != VM_MEMATTR_DEFAULT)
					pmap_page_set_memattr(m,
					    object->memattr);
				vm_page_insert(m, object, pindex);
				vm_page_lock_queues();
				vm_page_flag_clear(m, PG_UNMANAGED);
				vm_page_unlock_queues();
				ma[i] = m;
				i++;
			}
			break;
		}
		VM_OBJECT_UNLOCK(object);
		atomic_add_int(&vmio_alloc_wait, 1);
		VM_WAIT;
		VM_OBJECT_LOCK(object);
		goto redo;
	}
	*wp = i;
}

/*
 * Read a cluster starting at 'ma'. Note that we need to always redo
 * page grab because our caller dropped object lock while not holding
 * vnode lock.
 */
static int
vnode_pager_read_cluster(struct vnode *vp, vm_page_t ma[], vm_pindex_t idx,
    int *maxrun, int flags, vm_page_t *reserv)
{
	vm_object_t obj;
	vm_page_t m;
	daddr_t blkno;
	int bsize;
	int error;
	int run;
	int i;

	obj = vp->v_object;
	bsize = vp->v_mount->mnt_stat.f_iosize;
	error = 0;
	blkno = 0;

	if (vmio_run) {
		VM_OBJECT_UNLOCK(obj);
		error = VOP_BMAP(vp, IDX_TO_OFF(idx)/bsize, NULL, &blkno, &run,
		    NULL);
		VM_OBJECT_LOCK(obj);
		run = MIN(run, *maxrun);
		if (error || run == 0 || blkno == -1) {
/* printf("vnode_pager_read_cluster short\n"); */
			*maxrun = 1;
			vnode_grab_pages(vp, ma, maxrun, idx,
			    VN_GRAB_NO_VMWAIT, reserv);
			error = vm_pager_get_pages(obj, ma, 1, 0);
			if (error != VM_PAGER_OK)
				return (EIO);
			return (0);
		}
		run = (run + 1) * bsize / PAGE_SIZE;
		run = MIN(run, vp->v_mount->mnt_iosize_max / PAGE_SIZE);
	} else {
		if (*maxrun == 0)
			*maxrun = 1;
		run = MIN(*maxrun, vp->v_mount->mnt_iosize_max / PAGE_SIZE);
	}
	if (IDX_TO_OFF(idx) + run * PAGE_SIZE > obj->un_pager.vnp.vnp_size) {
		run = (obj->un_pager.vnp.vnp_size - IDX_TO_OFF(idx)) /
		    PAGE_SIZE;
	}
	if (run == 0)
		run = 1;
	vnode_grab_pages(vp, ma, &run, idx, VN_GRAB_NO_VMWAIT, reserv);
	for (i = 0; i < run; i++) {
		if (i > 0 && ma[i]->valid != 0) {
			run = i;
			break;
		}
		vm_page_busy(ma[i]);
	}

/* printf("vnode_pager_read_cluster %d %p %p\n", run, ma, ma[0]); */
	error = vm_pager_get_pages(obj, ma, run, 0);
	if (error != VM_PAGER_OK) {
		vm_page_lock_queues();
		for (i = 0; i < run; i++)
			vm_page_free(ma[i]);
		vm_page_unlock_queues();
		return (EIO);
	}
	KASSERT(ma[0]->valid == VM_PAGE_BITS_ALL,
	    ("ma[0]->valid %x", ma[0]->valid));
	vm_page_wakeup(ma[0]);
	/* ma[0] cannot be cached */
	for (i = 1; i < run; i++) {
		m = TAILQ_NEXT(ma[i - 1], listq);
		if (m == NULL || m->pindex != ma[0]->pindex + i ||
		    ma[i] != m || m->valid == 0)
			break;
		ma[i] = m;
/* printf("run %d ma[%d]: obj %p %p pindex %jd p+i %jd valid %x\n",
   run, i, obj, ma[i]->object, ma[i]->pindex, ma[0]->pindex + i, ma[i]->valid); */
	}
	*maxrun = i;
	return (0);
}

int
vnode_pager_read(struct vnode *vp, struct uio *uio, int ioflags)
{
	vm_object_t obj;
	vm_offset_t off;
	vm_pindex_t idx;
	vm_page_t reserv;
	ssize_t size;
	int error, seqcount, wpmax, wp, i;
	u_int bits;
	struct thread *td;

	if (ioflags & (IO_EXT|IO_DIRECT))
		return (EOPNOTSUPP);

	ASSERT_VOP_LOCKED(vp, "vnode_pager_read");
	if (vp->v_iflag & VI_DOOMED)
		return (EBADF);

	/*
	 * Ignore non-regular files.
	 */
	if (vp->v_type != VREG)
		return (EOPNOTSUPP);
	obj = vp->v_object;
	if (obj == NULL)
		return (EOPNOTSUPP);

	seqcount = (ioflags >> IO_SEQSHIFT) * FRA_BLOCK_SZ / PAGE_SIZE;
	seqcount = min(vfs_read_max, seqcount);
	seqcount = min(vp->v_mount->mnt_iosize_max / PAGE_SIZE, seqcount);
	VOP_UNLOCK(vp, 0);

	wpmax = atomic_load_acq_int(&vmio_read_pack);
	vm_page_t ma[wpmax + 1];

	while (vm_page_count_severe()) {
		atomic_add_int(&vm_pageout_deficit, MIN(wpmax + 1,
		    (uio->uio_resid + PAGE_SIZE - 1) >> PAGE_SHIFT));
		VM_WAIT;
	}

	error = 0;
	reserv = NULL;
	td = uio->uio_td;
	/* XXXKIB This should be disallowed. */
	if (td == NULL)
		td = curthread;

	VM_OBJECT_LOCK(obj);
	while (uio->uio_resid > 0) {
		wp = wpmax;

		size = obj->un_pager.vnp.vnp_size - uio->uio_offset;
		if (size <= 0)
			break;
		idx = OFF_TO_IDX(uio->uio_offset);
		off = uio->uio_offset - IDX_TO_OFF(idx);
		size = MIN(MIN(PAGE_SIZE * wp - off, uio->uio_resid), size);

		wp = (size + off + PAGE_SIZE - 1) / PAGE_SIZE;
		vnode_grab_pages(vp, ma, &wp, idx, 0, NULL);
	find_valid:
		for (i = 0; i < wp; i++) {
			bits = io_page_bits(i, off, size);

			/*
			 * Only do read if first page of array is not
			 * valid for us. We have to drop object lock
			 * to obtain vnode lock, that allows the pages
			 * to change identity or validity bits, and we
			 * can guarantee allocation of only one
			 * (reserved) page.
			 */
			if ((ma[i]->valid & bits) != bits) {
				if (i != 0) {
					wp = i;
					break;
				}
				VM_OBJECT_UNLOCK(obj);
				vnode_alloc_reserv(&reserv);
				error = vn_lock(vp, LK_SHARED);
				VM_OBJECT_LOCK(obj);
				if (error != 0) {
					error = EBADF;
					break;
				}

				/*
				 * Read page, honouring read-ahead settings
				 * for filedescriptor.
				 */
				atomic_add_int(&vmio_getpages_read, 1);
				error = vnode_pager_read_cluster(vp, ma, idx,
				    &wp, VN_GRAB_NO_VMWAIT, &reserv);
				VOP_UNLOCK(vp, 0);
				if (error != 0)
					break;
				/*
				 * No need to redo size calculation.
				 * Despite both vnode and object locks
				 * were dropped, range lock and file
				 * descriptor reference shall keep
				 * file from truncation.
				 */
				goto find_valid;
			}
		}
		if (error != 0)
			break;
		KASSERT(wp > 0, ("wp == 0"));
/* printf("vp %p wp %d size %d\n", vp, wp, size); */

		/*
		 * Prevent object deallocation and pages swap-out.
		 */
		vm_object_pip_add(obj, 1);
		vm_page_lock_queues();
		for (i = 0; i < wp; i++)
			vm_page_hold(ma[i]);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(obj);

		/*
		 * Recalculate i/o size, since vnode_grab_pages()
		 * might shortened the page run.
		 */
		size = MIN(MIN(PAGE_SIZE * wp - off, uio->uio_resid), size);

		/*
		 * Access user map pages, vnode lock is dropped.
		 * Possible page fault is safe at this point.  Vnode
		 * rangelock is held, protecting from parallel
		 * writers.
		 */
/* printf("size %d %d %ju\n", size, uio->uio_resid, (uintmax_t)off); */
		KASSERT((td->td_pflags & TDP_VMIO) == 0,
		    ("Recursed vnode_pager_read"));
		td->td_pflags |= TDP_VMIO;
		error = uiomove_fromphys(ma, off, size, uio);
		td->td_pflags &= ~TDP_VMIO;

		VM_OBJECT_LOCK(obj);
		vm_page_lock_queues();
		for (i = 0; i < wp; i++) {
			vm_page_unhold(ma[i]);
			vm_page_activate(ma[i]);
		}
		vm_page_unlock_queues();
		vm_object_pip_wakeup(obj);
		if (error != 0)
			break;
	}
	VM_OBJECT_UNLOCK(obj);
	if (reserv != NULL)
		vm_page_free(reserv);
	vn_lock(vp, LK_SHARED | LK_RETRY);
	if (error == 0)
		vfs_mark_atime(vp, td->td_ucred);

	return (error);
}

int
vnode_pager_write(struct vnode *vp, struct uio *uio, int ioflags)
{
	vm_object_t obj;
	vm_offset_t off;
	vm_pindex_t idx, clean_start, clean_end;
	vm_page_t reserv;
	struct vattr vattr;
	ssize_t size, size1, osize, osize1, resid, sresid;
	int error, vn_locked, wpmax, wp, i;
	u_int bits;
	boolean_t vnode_locked;
	struct thread *td;

	if (ioflags & (IO_EXT|IO_INVAL|IO_DIRECT))
		return (EOPNOTSUPP);
	ASSERT_VOP_LOCKED(vp, "vnode_pager_write");
	if (vp->v_iflag & VI_DOOMED)
		return (EBADF);
	if (vp->v_type != VREG)
		return (EOPNOTSUPP);
	obj = vp->v_object;
	if (obj == NULL)
		return (EOPNOTSUPP);
	vn_locked = VOP_ISLOCKED(vp);
	vnode_locked = TRUE;
	error = 0;

	wpmax = atomic_load_acq_int(&vmio_write_pack);
	vm_page_t ma[wpmax + 1];

	/*
	 * Try to ensure that enough pages is available in advance.
	 */
	while (vm_page_count_severe()) {
		if (vnode_locked) {
			VOP_UNLOCK(vp, 0);
			vnode_locked = FALSE;
		}
		atomic_add_int(&vm_pageout_deficit, MIN(wpmax + 1,
		    (uio->uio_resid + PAGE_SIZE - 1) >> PAGE_SHIFT));
		VM_WAIT;
	}

	/*
	 * Allocate first reserve page.
	 */
	for (reserv = NULL; reserv == NULL; ) {
		reserv = vm_page_alloc(NULL, 0,
		    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ);
		if (reserv == NULL) {
			if (vnode_locked) {
				VOP_UNLOCK(vp, 0);
				vnode_locked = FALSE;
			}
			atomic_add_int(&vmio_alloc_wait, 1);
			VM_WAIT;
		}
	}
	if (!vnode_locked) {
		/*
		 * Since vnode lock was dropped, we are under low free
		 * pages condition, so more write trottling is due.
		 */
		vnode_pager_wwait();

		vn_lock(vp, vn_locked | LK_RETRY);
		if (vp->v_iflag & VI_DOOMED) {
			if (reserv != NULL)
				vm_page_free(reserv);
			return (EBADF);
		}
		vnode_locked = TRUE;
	}

	if (ioflags & IO_APPEND)
		uio->uio_offset = obj->un_pager.vnp.vnp_size;

	clean_start = OFF_TO_IDX(uio->uio_offset);
	clean_end = OFF_TO_IDX(uio->uio_offset + uio->uio_resid +
	    PAGE_SIZE - 1);

	td = uio->uio_td;
	if (td == NULL)
		td = curthread;

	/*
	 * Enforce the RLIMIT_FSIZE there too.
	 * XXXKIB the check for the file type is kept on purpose.
	 */
	if (vp->v_type == VREG) {
		PROC_LOCK(td->td_proc);
		if (uio->uio_offset + uio->uio_resid >
		    lim_cur(td->td_proc, RLIMIT_FSIZE)) {
			psignal(td->td_proc, SIGXFSZ);
			PROC_UNLOCK(td->td_proc);
			return (EFBIG);
		}
		PROC_UNLOCK(td->td_proc);
	}
	osize = osize1 = obj->un_pager.vnp.vnp_size;
	resid = uio->uio_resid;

 io_loop:
	while (uio->uio_resid > 0) {
		wp = wpmax;
		size = uio->uio_resid;
		idx = OFF_TO_IDX(uio->uio_offset);
		off = uio->uio_offset - IDX_TO_OFF(idx);
		size = MIN(PAGE_SIZE * wp - off, uio->uio_resid);
		if (!vnode_locked) {
			error = vn_lock(vp, LK_EXCLUSIVE);
			if (error != 0) {
				error = EBADF;
				break;
			}
			vnode_locked = TRUE;
		}
		osize1 = obj->un_pager.vnp.vnp_size;

		/*
		 * Extend the file if writing past end.
		 */
		if (osize1 < uio->uio_offset + size) {
			if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE) {
				VOP_UNLOCK(vp, 0);
				vnode_locked = FALSE;
			}
			if (!vnode_locked) {
				error = vn_lock(vp, LK_EXCLUSIVE);
				if (error != 0) {
					error = EBADF;
					break;
				}
				vnode_locked = TRUE;
			}
			vattr.va_size = uio->uio_offset + size;
			error = VOP_EXTEND(vp, td->td_ucred, uio->uio_offset +
			    size, ioflags);
		}
		if (error != 0)
			break;

		wp = (size + off + PAGE_SIZE - 1) / PAGE_SIZE;
		VM_OBJECT_LOCK(obj);

		/*
		 * Use VN_GRAB_NO_VMWAIT since vnode lock is held.
		 */
		vnode_grab_pages(vp, ma, &wp, idx, VN_GRAB_NO_VMWAIT, &reserv);
	find_valid:
		for (i = 0; i < wp; i++) {
			/*
			 * If the page falls into the newly-extended
			 * range, zero it and mark as valid. There is
			 * nothing VOP_GETPAGES can read from file.
			 */
			if (IDX_TO_OFF(ma[i]->pindex) >= osize1) {
				if ((ma[i]->flags & PG_ZERO) == 0)
					pmap_zero_page(ma[i]);
				ma[i]->valid = VM_PAGE_BITS_ALL;
			}

			/*
			 * Pages need to be fully valid, because we
			 * can only hold them during uiomove later.
			 *
			 * The page fault happening in other thread
			 * after uiomove finished but before valid
			 * bits are corrected below would cause lost
			 * of newly written data if page is not fully
			 * valid.
			 */
			if (ma[i]->valid == VM_PAGE_BITS_ALL)
				continue;
			if (!vmio_clrbuf) {
				bits = io_page_bits(i, off, size);
				if ((ma[i]->valid & ~bits) == (~bits &
				    VM_PAGE_BITS_ALL))
					continue;
			}
			if (i != 0) {
				wp = i;
				break;
			}
			if (reserv == NULL)
				reserv = vm_page_alloc(NULL, 0,
				    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ);
			if (reserv == NULL) {
				VM_OBJECT_UNLOCK(obj);

				/*
				 * Truncate the file back to the
				 * original size to prevent mmap from
				 * seeing invalid pages. We are going
				 * to drop vnode lock.
				 */
				if (osize1 < uio->uio_offset + size) {
					atomic_add_int(&vmio_rollbacks1, 1);
					VATTR_NULL(&vattr);
					vattr.va_size = osize1;
					error = VOP_SETATTR(vp, &vattr,
					    td->td_ucred);
					if (error != 0)
						break;
				}
				KASSERT(vnode_locked, ("lost vnode lock 1"));
				VOP_UNLOCK(vp, 0);
				vnode_locked = FALSE;
				vnode_pager_wwait();
				vnode_alloc_reserv(&reserv);
				goto io_loop;
			}

			atomic_add_int(&vmio_getpages_write, 1);
			error = vnode_pager_read_cluster(vp, ma, idx, &wp,
			    VN_GRAB_NO_VMWAIT, &reserv);
			if (error != 0) {
				VM_OBJECT_UNLOCK(obj);
				break;
			}
			goto find_valid;
		}
		/* Loop above is exited with unlocked obj if error != 0. */
		if (error != 0)
			break;
		KASSERT(wp > 0, ("wp == 0"));

		/*
		 * Prevent the object deallocation and hold the pages.
		 * Held page can be removed from object, but cannot be
		 * reused. Range lock taken in vn_truncate() prevents
		 * most typical race.
		 *
		 * XXXKIB Busying the pages there would cause deadlock
		 * with vm_object_page_remove() or self-lock with
		 * vm_fault(), but would allow to not require the
		 * pages to be fully valid before uiomove.
		 *
		 * The mmap could see zeroed pages that are inserted
		 * into extended area after we dropped object lock.
		 * This could be considered an application race.
		 */
		vm_object_pip_add(obj, 1);
		vm_page_lock_queues();
		for (i = 0; i < wp; i++)
			vm_page_hold(ma[i]);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(obj);

		/*
		 * Recalculate i/o size, since vnode_grab_pages()
		 * might have shortened the page run. Save previous
		 * resid to correctly mark written pages regions as
		 * dirty.
		 */
		sresid = uio->uio_resid;
		size1 = MIN(MIN(PAGE_SIZE * wp - off, sresid), size);

		/*
		 * Shrunk file in case we allocated less pages then
		 * the estimation that was used to VOP_EXTEND.
		 */
		KASSERT(vnode_locked, ("lost vnode lock 2"));
		if (size1 < size && osize1 < uio->uio_offset + size) {
			atomic_add_int(&vmio_rollbacks2, 1);
			VATTR_NULL(&vattr);
			vattr.va_size = uio->uio_offset + size1;
			error = VOP_SETATTR(vp, &vattr, td->td_ucred);
			if (error != 0) {
				VM_OBJECT_LOCK(obj);
				vm_page_lock_queues();
				for (i = 0; i < wp; i++) {
					vm_page_unhold(ma[i]);
					vm_page_deactivate(ma[i]);
				}
				vm_page_unlock_queues();
				vm_object_pip_wakeup(obj);
				VM_OBJECT_UNLOCK(obj);
				break;
			}
		}
		size = size1;

		VOP_UNLOCK(vp, 0);
		vnode_locked = FALSE;

		KASSERT((td->td_pflags & TDP_VMIO) == 0,
		    ("Recursed vnode_pager_write"));
/* printf("W: vp %p off %jd %jd size %jd\n",
   vp, (intmax_t)uio->uio_offset, (intmax_t)off, (intmax_t)size); */
		td->td_pflags |= TDP_VMIO;
		error = uiomove_fromphys(ma, off, size, uio);
		td->td_pflags &= ~TDP_VMIO;

		VM_OBJECT_LOCK(obj);
		vm_page_lock_queues();
		for (i = 0; i < wp; i++) {
			/*
			 * Note that the page is dirty regardeless of
			 * the possible error from uiomove. We must
			 * mark the pages that were touched by uiomove
			 * before fault occured. Since we do not
			 * record the progress of the uiomove till
			 * fault, just mark them all.
			 */
			ma[i]->dirty |= io_page_bits(i, off, sresid -
			    uio->uio_resid);
			if ((ma[i]->flags & PG_WRITEDIRTY) == 0) {
				ma[i]->flags |= PG_WRITEDIRTY;
				vmio_writedirty++;
			}
			vm_page_unhold(ma[i]);
			vm_page_activate(ma[i]);
		}
		vm_page_unlock_queues();
		/* See the comment above about page dirtiness. */
		vm_object_set_writeable_dirty(obj);
		vm_object_pip_wakeup(obj);
		VM_OBJECT_UNLOCK(obj);
		if (error != 0)
			break;
		KASSERT(!vnode_locked, ("vnode leak 3"));

		vnode_pager_wwait();

		/*
		 * Re-fill reserv while vnode lock is dropped.
		 */
		if (uio->uio_resid != 0)
			vnode_alloc_reserv(&reserv);
	}

	if (!vnode_locked)
		vn_lock(vp, vn_locked | LK_RETRY);
	if (reserv != NULL)
		vm_page_free(reserv);
	if (vp->v_iflag & VI_DOOMED) {
		if (error == 0)
			error = EBADF;
		return (error);
	}
	if (error == 0) {
		if (((ioflags & IO_SYNC) && (vp->v_vflag & VV_NOSYNC)) ||
		    vm_page_count_severe()) {
			VM_OBJECT_LOCK(obj);
			vm_object_page_clean(obj, clean_start, clean_end,
			    OBJPC_SYNC);
			VM_OBJECT_UNLOCK(obj);
#if 0
			/*
			 * XXXKIB The following call is commented out in
			 * vm_object_page_clean() in the same way.
			 */
			error = VOP_FSYNC(vp, MNT_WAIT, td);
#endif
		}
	} else {
		/*
		 * Roll back on error if atomic write was requested.
		 */
		VATTR_NULL(&vattr);
		vattr.va_size = (ioflags & IO_UNIT) ? osize : osize1;
		VOP_SETATTR(vp, &vattr, td->td_ucred);
		if (ioflags & IO_UNIT) {
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	}

	return (error);
}
