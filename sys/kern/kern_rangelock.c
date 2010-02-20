/*-
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/rangelock.h>
#include <sys/systm.h>
#include <sys/vnode.h>

uma_zone_t rl_entry_zone;

static void
rangelock_sys_init(void)
{

	rl_entry_zone = uma_zcreate("rl_entry", sizeof(struct rl_q_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_ANY, rangelock_sys_init, NULL);

void
rangelock_init(struct rangelock *lock)
{

	TAILQ_INIT(&lock->rl_waiters);
	lock->rl_currdep = NULL;
}

void
rangelock_destroy(struct rangelock *lock)
{

	KASSERT(TAILQ_EMPTY(&lock->rl_waiters), ("Dangling waiters"));
}

static int
rangelock_incompatible(const struct rl_q_entry *e1,
    const struct rl_q_entry *e2)
{

	if ((e1->rl_q_flags & RL_LOCK_TYPE_MASK) == RL_LOCK_READ &&
	    (e2->rl_q_flags & RL_LOCK_TYPE_MASK) == RL_LOCK_READ)
		return (0);
#define	IN_RANGE(a, e) (a >= e->rl_q_start && a < e->rl_q_end)
	if (IN_RANGE(e1->rl_q_start, e2) || IN_RANGE(e2->rl_q_start, e1) ||
	    IN_RANGE(e1->rl_q_end, e2) || IN_RANGE(e2->rl_q_end, e1))
		return (1);
#undef	IN_RANGE
	return (0);
}

static void
rangelock_calc_block(struct rangelock *lock)
{
	struct rl_q_entry *entry, *entry1, *whead;

	if (lock->rl_currdep == TAILQ_FIRST(&lock->rl_waiters) &&
	    lock->rl_currdep != NULL)
		lock->rl_currdep = TAILQ_NEXT(lock->rl_currdep, rl_q_link);
	for (entry = lock->rl_currdep; entry;
	     entry = TAILQ_NEXT(entry, rl_q_link)) {
		TAILQ_FOREACH(entry1, &lock->rl_waiters, rl_q_link) {
			if (rangelock_incompatible(entry, entry1))
				goto out;
			if (entry1 == entry)
				break;
		}
	}
out:
	lock->rl_currdep = entry;
	TAILQ_FOREACH(whead, &lock->rl_waiters, rl_q_link) {
		if (whead == lock->rl_currdep)
			break;
		if (!(whead->rl_q_flags & RL_LOCK_GRANTED)) {
			whead->rl_q_flags |= RL_LOCK_GRANTED;
			wakeup(whead);
		}
	}
}

static void
rangelock_unlock_vp_locked(struct vnode *vp, struct rl_q_entry *entry)
{

	ASSERT_VI_LOCKED(vp, "rangelock");
	KASSERT(entry != vp->v_rl.rl_currdep, ("stuck currdep"));
	TAILQ_REMOVE(&vp->v_rl.rl_waiters, entry, rl_q_link);
	rangelock_calc_block(&vp->v_rl);
	VI_UNLOCK(vp);
	uma_zfree(rl_entry_zone, entry);
}

void
rangelock_unlock(struct vnode *vp, void *cookie)
{
	struct rl_q_entry *entry;

	entry = cookie;
	VI_LOCK(vp);
	rangelock_unlock_vp_locked(vp, entry);
}

void *
rangelock_unlock_range(struct vnode *vp, void *cookie, off_t base, size_t len)
{
	struct rl_q_entry *entry;

	entry = cookie;
	VI_LOCK(vp);
	KASSERT(entry->rl_q_flags & RL_LOCK_GRANTED, ("XXX"));
	KASSERT(entry->rl_q_start == base, ("XXX"));
	KASSERT(entry->rl_q_end >= base + len, ("XXX"));
	if (entry->rl_q_end == base + len) {
		rangelock_unlock_vp_locked(vp, cookie);
		return (NULL);
	}
	entry->rl_q_end = base + len;
	rangelock_calc_block(&vp->v_rl);
	VI_UNLOCK(vp);
	return (cookie);
}

static void *
rangelock_enqueue(struct vnode *vp, struct rl_q_entry *entry)
{

	VI_LOCK(vp);
	TAILQ_INSERT_TAIL(&vp->v_rl.rl_waiters, entry, rl_q_link);
	if (vp->v_rl.rl_currdep == NULL)
		vp->v_rl.rl_currdep = entry;
	rangelock_calc_block(&vp->v_rl);
	while (!(entry->rl_q_flags & RL_LOCK_GRANTED))
		msleep(entry, &vp->v_interlock, 0, "range", 0);
	VI_UNLOCK(vp);
	return (entry);
}

void *
rangelock_rlock(struct vnode *vp, off_t base, size_t len)
{
	struct rl_q_entry *entry;

	entry = uma_zalloc(rl_entry_zone, M_WAITOK);
	entry->rl_q_flags = RL_LOCK_READ;
	entry->rl_q_start = base;
	entry->rl_q_end = base + len;
	return (rangelock_enqueue(vp, entry));
}

void *
rangelock_wlock(struct vnode *vp, off_t base, size_t len)
{
	struct rl_q_entry *entry;

	entry = uma_zalloc(rl_entry_zone, M_WAITOK);
	entry->rl_q_flags = RL_LOCK_WRITE;
	entry->rl_q_start = base;
	entry->rl_q_end = base + len;
	return (rangelock_enqueue(vp, entry));
}
