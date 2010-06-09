/*-
 * Copyright (c) 2010 Andrey V. Elsukov <bu7cher@yandex.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "sade.h"

STAILQ_HEAD(history, hentry);
struct hentry {
	STAILQ_ENTRY(hentry)	entry;
	void			*data;
};

history_t
history_init(void)
{
	history_t hist;

	hist = malloc(sizeof(*hist));
	if (hist != NULL)
		STAILQ_INIT(hist);
	return (hist);
}

void
history_free(history_t hist)
{
	free(hist);
}

int
history_add_entry(history_t hist, void *data)
{
	struct hentry *pentry;

	pentry = malloc(sizeof(*pentry));
	if (pentry == NULL)
		return (ENOMEM);
	pentry->data = data;
	STAILQ_INSERT_HEAD(hist, pentry, entry);
	return (0);
}

int
history_rollback(history_t hist, int (*rollback)(void *))
{
	int ret;
	struct hentry *pentry;

	assert(rollback != NULL);
	while((pentry = STAILQ_FIRST(hist)) != NULL) {
		ret = rollback(pentry->data);
		if (ret != 0)
			break;
		STAILQ_REMOVE_HEAD(hist, entry);
		free(pentry);
	}
	return (ret);
}

int
history_play(history_t hist, int (*play)(void *))
{
	int ret;
	struct hentry *pentry;

	assert(play != NULL);
	while((pentry = STAILQ_LAST(hist, hentry, entry)) != NULL) {
		ret = play(pentry->data);
		if (ret != 0)
			break;
		STAILQ_REMOVE(hist, pentry, hentry, entry);
		free(pentry);
	}
	return (ret);
}

int
history_isempty(history_t hist)
{
	return (STAILQ_EMPTY(hist));
}
