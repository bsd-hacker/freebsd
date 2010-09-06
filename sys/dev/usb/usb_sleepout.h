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
 *
 * $FreeBSD$
 */

#ifndef _USB_SLEEPOUT_H_
#define	_USB_SLEEPOUT_H_

#include <sys/callout.h>
#include <sys/taskqueue.h>

struct sleepout {
	struct taskqueue	*s_taskqueue;
};

typedef void (*sleepout_func_t)(void *);

struct sleepout_task {
	struct sleepout		*st_sleepout;
	struct callout		st_callout;
	struct task		st_task;
	struct mtx		*st_mtx;
	sleepout_func_t		st_func;
	void			*st_arg;
};

void	sleepout_create(struct sleepout *, const char *);
void	sleepout_free(struct sleepout *);
void	sleepout_init(struct sleepout *, struct sleepout_task *, int);
void	sleepout_init_mtx(struct sleepout *, struct sleepout_task *,
	    struct mtx *, int);
int	sleepout_reset(struct sleepout_task *, int, sleepout_func_t, void *);
int	sleepout_pending(struct sleepout_task *);
int	sleepout_stop(struct sleepout_task *);
int	sleepout_drain(struct sleepout_task *);

#endif
