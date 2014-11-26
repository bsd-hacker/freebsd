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

static inline int
vdsk_is_dev(struct vdsk *vdsk)
{

	return ((S_ISCHR(vdsk->fsbuf.st_mode)) ? 1 : 0);
}

static struct vdsk *
vdsk_deref(vdskctx ctx)
{
	struct vdsk *vdsk = ctx;

	return (vdsk - 1);
}

static struct vdsk_format *
vdsk_probe(struct vdsk *vdsk)
{
	struct vdsk_format *f, *fmt, **f_iter;
	int error, probe;

	fmt = NULL;
	probe = VDSKFMT_HAS_HEADER | VDSKFMT_HAS_FOOTER;
	probe |= (vdsk_is_dev(vdsk)) ? VDSKFMT_DEVICE_OK : 0;
	probe |= (vdsk->fflags & FWRITE) ? VDSKFMT_CAN_WRITE : 0;
	while (fmt == NULL && probe >= 0) {
		SET_FOREACH(f_iter, libvdsk_formats) {
			f = *f_iter;
			if ((f->flags & probe) != probe)
				continue;
			error = f->probe(vdsk);
			if (!error) {
				fmt = f;
				break;
			}
		}
		probe -= VDSKFMT_HAS_FOOTER;
	}
	if (fmt == NULL)
		errno = EFTYPE;
	return (fmt);
}

vdskctx
vdsk_open(const char *path, int flags, size_t size)
{
	vdskctx ctx;
	struct vdsk *vdsk;
	int lck;

	ctx = NULL;

	do {
		size += sizeof(struct vdsk);
		vdsk = calloc(1, size);
		if (vdsk == NULL)
			break;

		vdsk->fflags = flags + 1;
		if ((vdsk->fflags & ~(O_ACCMODE | O_DIRECT | O_SYNC)) != 0) {
			errno = EINVAL;
			break;
		}

		vdsk->filename = realpath(path, NULL);
		if (vdsk->filename == NULL)
			break;

		flags = (flags & O_ACCMODE) | O_CLOEXEC;
		vdsk->fd = open(vdsk->filename, flags);
		if (vdsk->fd == -1)
			break;

		if (fstat(vdsk->fd, &vdsk->fsbuf) == -1)
			break;

		if (vdsk_is_dev(vdsk)) {
			if (ioctl(vdsk->fd, DIOCGMEDIASIZE,
			    &vdsk->capacity) < 0)
				break;
			if (ioctl(vdsk->fd, DIOCGSECTORSIZE,
			    &vdsk->sectorsize) < 0)
				break;
		} else {
			vdsk->capacity = vdsk->fsbuf.st_size;
			vdsk->sectorsize = DEV_BSIZE;
		}

		vdsk->fmt = vdsk_probe(vdsk);
		if (vdsk->fmt == NULL)
			break;

		lck = (vdsk->fflags & FWRITE) ? LOCK_EX : LOCK_SH;
		if (flock(vdsk->fd, lck | LOCK_NB) == -1)
			break;

		errno = vdsk->fmt->open(vdsk);
		if (errno != 0) {
			flock(vdsk->fd, LOCK_UN);
			break;
		}

		/* Complete... */
		ctx = vdsk + 1;
	} while (0);

	if (ctx == NULL) {
		if (vdsk != NULL) {
			if (vdsk->fd != -1)
				close(vdsk->fd);
			if (vdsk->filename != NULL)
				free(vdsk->filename);
			free(vdsk);
		}
	}

	return (ctx);
}

int
vdsk_close(vdskctx ctx)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	vdsk->fmt->close(vdsk);
	flock(vdsk->fd, LOCK_UN);
	close(vdsk->fd);
	free(vdsk->filename);
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

int
vdsk_read(vdskctx ctx, const struct iovec *iov, int iovcnt, off_t offset)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	return (vdsk->fmt->read(vdsk, iov, iovcnt, offset));
}

int
vdsk_write(vdskctx ctx, const struct iovec *iov, int iovcnt, off_t offset)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	if ((vdsk->fflags & FWRITE) == 0)
		return (EROFS);
	return (vdsk->fmt->write(vdsk, iov, iovcnt, offset));
}

int
vdsk_flush(vdskctx ctx)
{
	struct vdsk *vdsk = vdsk_deref(ctx);

	if ((vdsk->fflags & FWRITE) == 0)
		return (0);
	return (vdsk->fmt->flush(vdsk));
}

