/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_SCHED_H_
#define	_LINUX_SCHED_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sleepqueue.h>

#define	MAX_SCHEDULE_TIMEOUT	LONG_MAX

#define	TASK_RUNNING		0
#define	TASK_INTERRUPTIBLE	1
#define	TASK_UNINTERRUPTIBLE	2
#define	TASK_DEAD		64
#define	TASK_WAKEKILL		128
#define	TASK_WAKING		256

#define	TASK_SHOULD_STOP	1
#define	TASK_STOPPED		2

/*
 * A task_struct is only provided for threads created by kthread() and
 * file operation callbacks.
 *
 * Using these routines outside the above mentioned contexts will
 * cause panics because no task_struct is assigned and td_retval[1] is
 * overwritten by syscalls.
 */
struct task_struct {
	struct	thread *task_thread;
	int	(*task_fn)(void *data);
	void	*task_data;
	int	task_ret;
	int	state;
	int	should_stop;
	pid_t	pid;
	const char    *comm;
	void	*bsd_ioctl_data;
	unsigned	bsd_ioctl_len;
};

#define	current			task_struct_get(curthread)
#define	task_struct_get(x)	((struct task_struct *)(uintptr_t)(x)->td_retval[1])
#define	task_struct_fill(x, y) do {		\
  	(y)->task_thread = (x);			\
	(y)->comm = (x)->td_name;		\
	(y)->pid = (x)->td_tid;			\
} while (0)
#define	task_struct_set(x, y)	(x)->td_retval[1] = (uintptr_t)(y)

/* ensure the task_struct pointer fits into the td_retval[1] field */
CTASSERT(sizeof(((struct thread *)0)->td_retval[1]) >= sizeof(uintptr_t));

#define	set_current_state(x)						\
	atomic_store_rel_int((volatile int *)&current->state, (x))
#define	__set_current_state(x)	current->state = (x)


#define	schedule()							\
do {									\
	void *c;							\
									\
	if (cold)							\
		break;							\
	c = curthread;							\
	sleepq_lock(c);							\
	if (current->state == TASK_INTERRUPTIBLE ||			\
	    current->state == TASK_UNINTERRUPTIBLE) {			\
		sleepq_add(c, NULL, "task", SLEEPQ_SLEEP, 0);		\
		sleepq_wait(c, 0);					\
	} else {							\
		sleepq_release(c);					\
		sched_relinquish(curthread);				\
	}								\
} while (0)

#define	wake_up_process(x)						\
do {									\
	int wakeup_swapper;						\
	void *c;							\
									\
	c = (x)->task_thread;						\
	sleepq_lock(c);							\
	(x)->state = TASK_RUNNING;					\
	wakeup_swapper = sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);		\
	sleepq_release(c);						\
	if (wakeup_swapper)						\
		kick_proc0();						\
} while (0)

#define	cond_resched()	if (!cold)	sched_relinquish(curthread)

#define	sched_yield()	sched_relinquish(curthread)

static inline long
schedule_timeout(signed long timeout)
{
	if (timeout < 0)
		return 0;

	pause("lstim", timeout);

	return 0;
}

#endif	/* _LINUX_SCHED_H_ */
