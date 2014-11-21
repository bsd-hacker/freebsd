/*-
 * Copyright (c) 2014 Marcel Moolenaar
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

#include <sys/disk.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vdsk.h>

#include "vdsk_int.h"

static struct vdsk *
vdsk_deref(vdskctx ctx)
{
	struct vdsk *vdsk = ctx;

	return (vdsk - 1);
}

vdskctx
vdsk_open(const char *path, int flags, size_t size)
{
	struct stat sb;
	struct vdsk *vdsk;

	size += sizeof(struct vdsk);
	vdsk = malloc(size);
	if (vdsk == NULL)
		return (NULL);

	vdsk->fd = open(path, flags);
	if (vdsk->fd == -1) {
		free(vdsk);
		return (NULL);
	}

	if (fstat(vdsk->fd, &sb) == -1) {
		close(vdsk->fd);
		free(vdsk);
		return (NULL);
	}

	if (S_ISCHR(sb.st_mode)) {
		if (ioctl(vdsk->fd, DIOCGMEDIASIZE, &vdsk->capacity) < 0 ||
		    ioctl(vdsk->fd, DIOCGSECTORSIZE, &vdsk->sectorsize) < 0) {
			close(vdsk->fd);
			free(vdsk);
			return (NULL);
		}
	} else {
		vdsk->capacity = sb.st_size;
		vdsk->sectorsize = DEV_BSIZE;
	}
	return (vdsk + 1);
}

int
vdsk_close(vdskctx ctx)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	close(vdsk->fd);
	free(vdsk);
	return (0);
}

off_t
vdsk_capacity(vdskctx ctx)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	return (vdsk->capacity);
}

int
vdsk_sectorsize(vdskctx ctx)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	return (vdsk->sectorsize);
}

ssize_t
vdsk_read(vdskctx ctx, void *buf, size_t nbytes, off_t offset)
{
	struct vdsk *vdsk = vdsk_deref(ctx);
	ssize_t res;

	res = pread(vdsk->fd, buf, nbytes, offset);
	return (res);
}

ssize_t
vdsk_readv(vdskctx ctx, const struct iovec *iov, int iovcnt, off_t offset)
{
	struct vdsk *vdsk = vdsk_deref(ctx);
	ssize_t res;

	res = preadv(vdsk->fd, iov, iovcnt, offset);
	return (res);
}

ssize_t
vdsk_writev(vdskctx ctx, const struct iovec *iov, int iovcnt, off_t offset)
{
	struct vdsk *vdsk = vdsk_deref(ctx);
	ssize_t res;

	res = pwritev(vdsk->fd, iov, iovcnt, offset);
	return (res);
}

int
vdsk_flush(vdskctx ctx __unused)
{

	return (0);
}

