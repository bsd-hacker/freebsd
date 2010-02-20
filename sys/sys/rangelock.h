/*-
 * Copyright (c) 2009 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * $FreeBSD$
 */

#ifndef	_SYS_RANGELOCK_H
#define	_SYS_RANGELOCK_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/sx.h>

#ifdef _KERNEL

struct vnode;

struct rl_q_entry
{
	TAILQ_ENTRY(rl_q_entry) rl_q_link;
	size_t rl_q_start, rl_q_end;
	int rl_q_flags;
};

#define	RL_LOCK_READ		0x0001
#define	RL_LOCK_WRITE		0x0002
#define	RL_LOCK_TYPE_MASK	0x0003
#define	RL_LOCK_GRANTED		0x0004

struct rangelock
{
	TAILQ_HEAD(, rl_q_entry) rl_waiters;
	struct rl_q_entry *rl_currdep;
};

void	rangelock_init(struct rangelock *lock);
void	rangelock_destroy(struct rangelock *lock);
void	rangelock_unlock(struct vnode *vp, void *cookie);
void   *rangelock_unlock_range(struct vnode *vp, void *cookie, off_t base,
    size_t len);
void   *rangelock_rlock(struct vnode *vp, off_t base, size_t len);
void   *rangelock_wlock(struct vnode *vp, off_t base, size_t len);
#endif

#endif
