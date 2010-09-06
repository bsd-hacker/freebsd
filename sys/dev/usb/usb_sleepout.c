/*-
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <dev/usb/usb_sleepout.h>

void
sleepout_create(struct sleepout *s, const char *name)
{

	s->s_taskqueue = taskqueue_create(name, M_WAITOK,
	    taskqueue_thread_enqueue, &s->s_taskqueue);
	/* XXX adjusts the priority. */
	taskqueue_start_threads(&s->s_taskqueue, 1, PI_NET, "%s sleepout",
	    name);
}

void
sleepout_free(struct sleepout *s)
{

	taskqueue_free(s->s_taskqueue);
}

static void
_sleepout_taskqueue_callback(void *arg, int pending)
{
	struct sleepout_task *st = arg;

	(void)pending;

	if (st->st_mtx != NULL)
		mtx_lock(st->st_mtx);

	st->st_func(st->st_arg);

	if (st->st_mtx != NULL)
		mtx_unlock(st->st_mtx);
}

void
sleepout_init(struct sleepout *s, struct sleepout_task *st, int mpsafe)
{

	st->st_sleepout = s;
	callout_init(&st->st_callout, mpsafe);
	TASK_INIT(&st->st_task, 0, _sleepout_taskqueue_callback, st);
	st->st_mtx = NULL;
}

void
sleepout_init_mtx(struct sleepout *s, struct sleepout_task *st, struct mtx *mtx,
    int flags)
{

	st->st_sleepout = s;
	callout_init_mtx(&st->st_callout, mtx, flags);
	TASK_INIT(&st->st_task, 0, _sleepout_taskqueue_callback, st);
	st->st_mtx = mtx;
}

static void
_sleepout_callout_callback(void *arg)
{
	struct sleepout_task *st = arg;
	struct sleepout *s = st->st_sleepout;

	taskqueue_enqueue(s->s_taskqueue, &st->st_task);
}

int
sleepout_reset(struct sleepout_task *st, int to_ticks, sleepout_func_t ftn,
    void *arg)
{

	st->st_func = ftn;
	st->st_arg = arg;
	return (callout_reset(&st->st_callout, to_ticks,
	    _sleepout_callout_callback, st));
}

int
sleepout_pending(struct sleepout_task *st)
{

	return (callout_pending(&st->st_callout));
}

int
sleepout_stop(struct sleepout_task *st)
{

	return (callout_stop(&st->st_callout));
}

int
sleepout_drain(struct sleepout_task *st)
{
	struct sleepout *s = st->st_sleepout;

	taskqueue_drain(s->s_taskqueue, &st->st_task);
	return (callout_drain(&st->st_callout));
	
}
