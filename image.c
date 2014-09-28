/*-
 * Copyright (c) 2014 Juniper Networks, Inc.
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

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "image.h"
#include "mkimg.h"

struct chunk {
	lba_t	ch_block;		/* Block address in image. */
	off_t	ch_ofs;			/* Offset in backing file. */
	STAILQ_ENTRY(chunk) ch_list;
	size_t	ch_size;		/* Size of chunk in bytes. */
	int	ch_fd;			/* FD of backing file. */
	u_int	ch_flags;
#define	CH_FLAGS_GAP		1	/* Chunk is a gap (no FD). */
#define	CH_FLAGS_DIRTY		2	/* Data modified/only in memory. */
};

static STAILQ_HEAD(chunk_head, chunk) image_chunks;
static u_int image_nchunks;

static char image_swap_file[PATH_MAX];
static int image_swap_fd = -1;
static u_int image_swap_pgsz;
static off_t image_swap_size;

static lba_t image_size;

/*
 * Swap file handlng.
 */

static off_t
image_swap_alloc(size_t size)
{
	off_t ofs;
	size_t unit;

	unit = (secsz > image_swap_pgsz) ? secsz : image_swap_pgsz;
	assert((unit & (unit - 1)) == 0);

	size = (size + unit - 1) & ~(unit - 1);

	ofs = image_swap_size;
	image_swap_size += size;
	if (ftruncate(image_swap_fd, image_swap_size) == -1) {
		image_swap_size = ofs;
		ofs = -1LL;
	}
	fprintf(stderr, "SWAP: off=%jd, size=%zu\n", (intmax_t)ofs, size);
	return (ofs);
}

/*
 * Image chunk handling.
 */

static void
image_chunk_dump(void)
{
	struct chunk *ch;

	fprintf(stderr, "%u chunks:\n", image_nchunks);
	STAILQ_FOREACH(ch, &image_chunks, ch_list) {
		fprintf(stderr, "\tblk=%jd, ofs=%jd, fd=%d, sz=%zu, fl=%u\n",
		    (intmax_t)ch->ch_block, (intmax_t)ch->ch_ofs, ch->ch_fd,
		    ch->ch_size, ch->ch_flags);
	}
}

static size_t
image_chunk_grow(struct chunk *ch, size_t sz)
{
	size_t dsz, newsz;

	newsz = ch->ch_size + sz;
	if (newsz > ch->ch_size) {
		ch->ch_size = newsz;
		return (0);
	}
	/* We would overflow -- create new chunk for remainder. */
	dsz = SIZE_MAX - ch->ch_size;
	assert(dsz < sz);
	ch->ch_size = SIZE_MAX;
	return (sz - dsz);
}

static int
image_chunk_skipto(lba_t to)
{
	struct chunk *ch;
	lba_t from;
	size_t sz;

	ch = STAILQ_LAST(&image_chunks, chunk, ch_list);
	from = (ch != NULL) ? ch->ch_block + (ch->ch_size / secsz) : 0LL;

	assert(from <= to);

	/* Nothing to do? */
	if (from == to)
		return (0);
	/* Avoid bugs due to overflows. */
	if ((uintmax_t)(to - from) > (uintmax_t)(SIZE_MAX / secsz))
		return (EFBIG);
	sz = (to - from) * secsz;
	if (ch != NULL && (ch->ch_flags & CH_FLAGS_GAP)) {
		sz = image_chunk_grow(ch, sz);
		if (sz == 0)
			return (0);
		from = ch->ch_block + (ch->ch_size / secsz);
	}
	ch = malloc(sizeof(*ch));
	if (ch == NULL)
		return (ENOMEM);
	memset(ch, 0, sizeof(*ch));
	ch->ch_block = from;
	ch->ch_size = sz;
	ch->ch_fd = -1;
	ch->ch_flags |= CH_FLAGS_GAP;
	STAILQ_INSERT_TAIL(&image_chunks, ch, ch_list);
	image_nchunks++;
	return (0);
}

static int
image_chunk_append(lba_t blk, size_t sz, off_t ofs, int fd)
{
	struct chunk *ch;

	ch = STAILQ_LAST(&image_chunks, chunk, ch_list);
	if (ch != NULL && (ch->ch_flags & CH_FLAGS_GAP) == 0) {
		if (fd == ch->ch_fd &&
		    blk == (lba_t)(ch->ch_block + (ch->ch_size / secsz)) &&
		    ofs == (off_t)(ch->ch_ofs + ch->ch_size)) {
			sz = image_chunk_grow(ch, sz);
			if (sz == 0)
				return (0);
			blk = ch->ch_block + (ch->ch_size / secsz);
			ofs = ch->ch_ofs + ch->ch_size;
		}
	}
	ch = malloc(sizeof(*ch));
	if (ch == NULL)
		return (ENOMEM);
	memset(ch, 0, sizeof(*ch));
	ch->ch_block = blk;
	ch->ch_ofs = ofs;
	ch->ch_size = sz;
	ch->ch_fd = fd;
	STAILQ_INSERT_TAIL(&image_chunks, ch, ch_list);
	image_nchunks++;
	return (0);
}

static int
image_chunk_copyin(lba_t blk, void *buf, size_t sz, off_t ofs, int fd)
{
	uint64_t *p = buf;
	size_t n;
	int error;

	assert(((uintptr_t)p & 3) == 0);

	error = 0;
	sz = (sz + secsz - 1) & ~(secsz - 1);
	while (!error && sz > 0) {
		n = 0;
		while (n < (secsz >> 3) && p[n] == 0)
			n++;
		if (n == (secsz >> 3))
			error = image_chunk_skipto(blk + 1);
		else
			error = image_chunk_append(blk, secsz, ofs, fd);
		blk++;
		p += (secsz >> 3);
		sz -= secsz;
		ofs += secsz;
	}
	return (error);
}

/*
 * File mapping support.
 */

static void *
image_file_map(int fd, off_t ofs, size_t sz)
{
	void *ptr;
	size_t unit;
	int flags, prot;

	unit = (secsz > image_swap_pgsz) ? secsz : image_swap_pgsz;
	assert((unit & (unit - 1)) == 0);

	flags = MAP_NOCORE | MAP_NOSYNC | MAP_SHARED;
	/* Allow writing to our swap file only. */
	prot = PROT_READ | ((fd == image_swap_fd) ? PROT_WRITE : 0);
	sz = (sz + unit - 1) & ~(unit - 1);
	ptr = mmap(NULL, sz, prot, flags, fd, ofs);
	return ((ptr == MAP_FAILED) ? NULL : ptr);
}

static int
image_file_unmap(void *buffer, size_t sz)
{
	size_t unit;

	unit = (secsz > image_swap_pgsz) ? secsz : image_swap_pgsz;
	sz = (sz + unit - 1) & ~(unit - 1);
	munmap(buffer, sz);
	return (0);
}

/*
 * Input/source file handling.
 */

static int
image_copyin_stream(lba_t blk, int fd, uint64_t *sizep)
{
	char *buffer;
	uint64_t bytesize;
	off_t swofs;
	size_t iosz;
	ssize_t rdsz;
	int error;

	/*
	 * This makes sure we're doing I/O in multiples of the page
	 * size as well as of the sector size. 2MB is the minimum
	 * by virtue of secsz at least 512 bytes and the page size
	 * at least 4K bytes.
	 */
	iosz = secsz * image_swap_pgsz;

	bytesize = 0;
	do {
		swofs = image_swap_alloc(iosz);
		if (swofs == -1LL)
			return (errno);
		buffer = image_file_map(image_swap_fd, swofs, iosz);
		if (buffer == NULL)
			return (errno);
		rdsz = read(fd, buffer, iosz);
		if (rdsz > 0)
			error = image_chunk_copyin(blk, buffer, rdsz, swofs,
			    image_swap_fd);
		else if (rdsz < 0)
			error = errno;
		else
			error = 0;
		image_file_unmap(buffer, iosz);
		/* XXX should we relinguish unused swap space? */
		if (error)
			return (error);

		bytesize += rdsz;
		blk += (rdsz + secsz - 1) / secsz;
	} while (rdsz > 0);

	if (sizep != NULL)
		*sizep = bytesize;
	return (0);
}

static int
image_copyin_mapped(lba_t blk, int fd, uint64_t *sizep)
{

	return (image_copyin_stream(blk, fd, sizep));
}

int
image_copyin(lba_t blk, int fd, uint64_t *sizep)
{
	struct stat sb;
	int error;

	error = image_chunk_skipto(blk);
	if (!error) {
		if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode))
			error = image_copyin_stream(blk, fd, sizep);
		else
			error = image_copyin_mapped(blk, fd, sizep);
	}
	return (error);
}

int
image_copyout(int fd)
{
	int error;

	error = image_copyout_region(fd, 0, image_size);
	if (!error)
		error = image_copyout_done(fd);
	return (error);
}

int
image_copyout_done(int fd)
{
	off_t ofs;
	int error;

	ofs = lseek(fd, 0L, SEEK_CUR);
	if (ofs == -1)
		return (0);
	error = (ftruncate(fd, ofs) == -1) ? errno : 0;
	return (error);
}

int
image_copyout_region(int fd, lba_t blk, lba_t size)
{
	char *buffer;
	off_t ofs;
	size_t bufsz, sz;
	ssize_t rdsz, wrsz;
	int error;

	bufsz = secsz * image_swap_pgsz;

	ofs = lseek(fd, 0L, SEEK_CUR);

	blk *= secsz;
	if (lseek(image_swap_fd, blk, SEEK_SET) != blk)
		return (errno);
	buffer = malloc(bufsz);
	if (buffer == NULL)
		return (errno);
	error = 0;
	size *= secsz;
	while (size > 0) {
		sz = ((ssize_t)bufsz < size) ? bufsz : (size_t)size;
		rdsz = read(image_swap_fd, buffer, sz);
		if (rdsz <= 0) {
			error = (rdsz < 0) ? errno : 0;
			break;
		}
		wrsz = (ofs == -1) ?
		    write(fd, buffer, rdsz) :
		    sparse_write(fd, buffer, rdsz);
		if (wrsz < 0) {
			error = errno;
			break;
		}
		assert(wrsz == rdsz);
		size -= rdsz;
	}
	free(buffer);
	return (error);
}

int
image_data(lba_t blk, lba_t size)
{
	char *buffer, *p;

	blk *= secsz;
	if (lseek(image_swap_fd, blk, SEEK_SET) != blk)
		return (1);

	size *= secsz;
	buffer = malloc(size);
	if (buffer == NULL)
		return (1);

	if (read(image_swap_fd, buffer, size) != (ssize_t)size) {
		free(buffer);
		return (1);
	}

	p = buffer;
	while (size > 0 && *p == '\0')
		size--, p++;

	free(buffer);
	return ((size == 0) ? 0 : 1);
}

lba_t
image_get_size(void)
{
	static int once = 0;

	if (once == 0) {
		once++;
		image_chunk_dump();
	}
	return (image_size);
}

int
image_set_size(lba_t blk)
{

	image_chunk_skipto(blk);

	image_size = blk;
	if (ftruncate(image_swap_fd, blk * secsz) == -1)
		return (errno);
	return (0);
}

int
image_write(lba_t blk, void *buf, ssize_t len)
{

	blk *= secsz;
	if (lseek(image_swap_fd, blk, SEEK_SET) != blk)
		return (errno);
	len *= secsz;
	if (sparse_write(image_swap_fd, buf, len) != len)
		return (errno);
	return (0);
}

static void
image_cleanup(void)
{

	if (image_swap_fd != -1)
		close(image_swap_fd);
	unlink(image_swap_file);
}

int
image_init(void)
{
	const char *tmpdir;

	STAILQ_INIT(&image_chunks);
	image_nchunks = 0;

	image_swap_size = 0;
	image_swap_pgsz = getpagesize();

	if (atexit(image_cleanup) == -1)
		return (errno);
	if ((tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0')
		tmpdir = _PATH_TMP;
	snprintf(image_swap_file, sizeof(image_swap_file), "%s/mkimg-XXXXXX",
	    tmpdir);
	image_swap_fd = mkstemp(image_swap_file);
	if (image_swap_fd == -1)
		return (errno);
	return (0);
}
