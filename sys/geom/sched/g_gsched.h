/*-
 * Copyright (c) 2008 Fabio Checconi <fabio@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_G_GSCHED_H_
#define	_G_GSCHED_H_

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <geom/sched/g_sched.h>

/*
 * This is the interface exported to scheduling modules.
 */
/*
 * Geom I/O scheduler descriptor.
 */
struct g_geom;

typedef void *gs_init_t (struct g_geom *geom);
typedef void gs_fini_t (void *data);
typedef void gs_start_t (void *data, struct bio *bio);
typedef void gs_done_t (void *data, struct bio *bio);

struct g_gsched {
	const char	*gs_name;
	int		gs_refs;

	gs_init_t	*gs_init;
	gs_fini_t	*gs_fini;
	gs_start_t	*gs_start;
	gs_done_t	*gs_done;

	LIST_ENTRY(g_gsched) glist;
};

/*
 * Locking interface.  When each operation registered with the
 * scheduler is invoked, a per-instance lock is taken to protect
 * the data associated with it.  If the scheduler needs something
 * else to access the same data (e.g., a callout) it must use
 * these functions.
 */
void g_sched_lock(struct g_geom *gp);
void g_sched_unlock(struct g_geom *gp);

/*
 * Declaration of a scheduler module.
 */
int g_gsched_modevent(module_t mod, int cmd, void *arg);

#define	DECLARE_GSCHED_MODULE(name, gsched)				\
	static moduledata_t name##_mod = {				\
		#name,							\
		g_gsched_modevent,					\
		gsched,							\
	};								\
	DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);	\
	MODULE_DEPEND(name, g_sched, 0, 0, 0);

#endif	/* _KERNEL */

#endif	/* _G_GSCHED_H_ */
