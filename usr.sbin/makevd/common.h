/*-
 * Copyright (c) 2013
 *	Hiroki Sato <hrs@FreeBSD.org>  All rights reserved.
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
 *
 * $FreeBSD$
 *
 */
#ifndef _COMMON_H
#define _COMMON_H

#include <sys/queue.h>

struct blist_raw {
	size_t		blr_len;
	void		*blr_data;
};

struct blist_fd {
	int		blf_fd;
};

struct blist {
	TAILQ_ENTRY(blist)	bl_next;

	int		bl_type;
#define	BL_UNKNOWN	0
#define	BL_RAWDATA	1
#define	BL_RAWCOPY	2
	const char	*bl_name;
	const char	*bl_desc;
	union {
		struct blist_fd		tf;
		struct blist_raw	tr;
	} t;
#define	bl_tf	t.tf
#define	bl_tr	t.tr
};

TAILQ_HEAD(blhead_t, blist);

int dispatch_bl(int, struct blhead_t *);

#endif	/* _COMMON_H */
