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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"

static int rawcopy(int, int);
static int writebuf(int, void *, ssize_t);

int
dispatch_bl(int ofd, struct blhead_t *blhead)
{
	struct blist *bl;
	int error;

	TAILQ_FOREACH(bl, blhead, bl_next) {
		printf("processing section: %s\n", bl->bl_name);
		switch (bl->bl_type) {
		case BL_RAWCOPY:
			error = rawcopy(ofd, bl->bl_tf.blf_fd);
			break;
		case BL_RAWDATA:
			error = writebuf(ofd, bl->bl_tr.blr_data,
			    bl->bl_tr.blr_len);
			break;
		default:
			error = 1;
			break;
		}
		if (error)
			return (error);
	}
	return (0);
}

static int
rawcopy(int ofd, int ifd)
{
	ssize_t len0, len = 0;
	char buf[BUFSIZ];

	for (;;) {
		len0 = read(ifd, buf, sizeof(buf));
		if (len0 == 0)
			break;
		if (len0 < 0) {
			warn("read error");
			return (1);
		}
		len = write(ofd, buf, len0);
		if (len < 0) {
			warn("write error");
			return (1);
		}
	}
	return (0);
}

static int
writebuf(int ofd, void *buf, ssize_t len)
{
	ssize_t len0;
	u_char *p;

	p = (u_char *)buf;
	len0 = write(ofd, p, len);
	if (len0 != len) {
		warn("write error");
		return (1);
	}

	return (0);
}
