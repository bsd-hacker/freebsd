/*-
 * Copyright (c) 2009 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <svnsup/delta.h>
#include <svnsup/error.h>
#include <svnsup/string.h>

// XXX missing I/O error handling

struct svnsup_delta {
	FILE *f;
	const char *root;
	const char *uuid;
	const char *path;
	struct svnsup_delta_file *sdf;
	unsigned int ntxt;
};

struct svnsup_delta_file {
	struct svnsup_delta *sd;
	char *fn;
	int create:1;
	int checksum:1;
};

static svnsup_delta_file_t
svnsup_delta_file_alloc(svnsup_delta_t sd, const char *fn)
{
	svnsup_delta_file_t sdf;

	if ((sdf = calloc(1, sizeof *sdf)) == NULL) {
		return (NULL);
	} else if ((sdf->fn = strdup(fn)) == NULL) {
		free(sdf);
		return (NULL);
	}
	sdf->sd = sd;
	sd->sdf = sdf;
	return (sdf);
}

static void
svnsup_delta_file_free(svnsup_delta_file_t sdf)
{

	sdf->sd->sdf = NULL;
	free(sdf->fn);
	free(sdf);
}

static const char *
svnsup_delta_shorten_path(svnsup_delta_t sd, const char *pn)
{

	assert(strstr(pn, sd->path) == pn);
	pn += strlen(sd->path);
	assert(*pn == '/' || *pn == '\0');
	if (*pn == '/')
		++pn;
	return (pn);
}

/*
 * Create an svnsup delta.
 */
int
svnsup_create_delta(svnsup_delta_t *sdp)
{
	svnsup_delta_t sd;

	if ((sd = calloc(1, sizeof *sd)) == NULL)
		return (SVNSUP_ERR_MEMORY);
	sd->f = stdout;
	*sdp = sd;
	return (SVNSUP_ERR_NONE);
}

/*
 * Close an svnsup delta.
 */
int
svnsup_close_delta(svnsup_delta_t sd)
{

	assert(sd->sdf == NULL);
	free(sd);
	return (SVNSUP_ERR_NONE);
}

/*
 * Comment
 */
int
svnsup_delta_comment(svnsup_delta_t sd, const char *fmt, ...)
{
	va_list ap;
	char *commentbuf, *p;
	int len;

	va_start(ap, fmt);
	len = vasprintf(&commentbuf, fmt, ap);
	va_end(ap);
	if (commentbuf == NULL)
		return (SVNSUP_ERR_MEMORY);
	p = commentbuf;
	while (*p != '\0') {
		fprintf(sd->f, "# ");
		while (*p != '\0' && *p != '\n') {
			fprintf(sd->f, "%c", isprint(*p) ? *p : ' ');
			++p;
		}
		fprintf(sd->f, "\n");
		if (*p == '\n')
			++p;
	}
	free(commentbuf);
	return (SVNSUP_ERR_NONE);
}

/*
 * Metadata
 */
int
svnsup_delta_meta(svnsup_delta_t sd, const char *key, const char *fmt, ...)
{
	va_list ap;
	char *value;
	int ret;

	assert(sd != NULL);
	assert(key != NULL);
	assert(fmt != NULL);
	va_start(ap, fmt);
	ret = vasprintf(&value, fmt, ap);
	va_end(ap);
	if (ret == -1)
		return (SVNSUP_ERR_MEMORY);
	fprintf(sd->f, "@meta ");
	svnsup_string_fencode(sd->f, key);
	fprintf(sd->f, " ");
	svnsup_string_fencode(sd->f, value);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Repository root
 */
int
svnsup_delta_root(svnsup_delta_t sd, const char *root)
{

	assert(sd->root == NULL);
	sd->root = strdup(root);
	if (sd->root == NULL)
		return (SVNSUP_ERR_MEMORY);
	fprintf(sd->f, "@root ");
	svnsup_string_fencode(sd->f, sd->root);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Repository UUID
 */
int
svnsup_delta_uuid(svnsup_delta_t sd, const char *uuid)
{

	assert(sd->uuid == NULL);
	sd->uuid = strdup(uuid);
	if (sd->uuid == NULL)
		return (SVNSUP_ERR_MEMORY);
	fprintf(sd->f, "@uuid ");
	svnsup_string_fencode(sd->f, sd->uuid);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Repository path (branch / subdir)
 */
int
svnsup_delta_path(svnsup_delta_t sd, const char *path)
{

	assert(sd->path == NULL);
	sd->path = strdup(path);
	if (sd->path == NULL)
		return (SVNSUP_ERR_MEMORY);
	fprintf(sd->f, "@path ");
	svnsup_string_fencode(sd->f, sd->path);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Create a directory
 */
int
svnsup_delta_create_directory(svnsup_delta_t sd, const char *dn)
{

	assert(sd != NULL);
	assert(dn != NULL && *dn != '\0');
	assert(sd->sdf == NULL);
	dn = svnsup_delta_shorten_path(sd, dn);
	fprintf(sd->f, "@mkdir ");
	svnsup_string_fencode(sd->f, dn);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Remove a file or directory
 */
int
svnsup_delta_remove(svnsup_delta_t sd, const char *fn)
{

	assert(sd != NULL);
	assert(fn != NULL && *fn != '\0');
	assert(sd->sdf == NULL);
	fn = svnsup_delta_shorten_path(sd, fn);
	fprintf(sd->f, "@remove ");
	svnsup_string_fencode(sd->f, fn);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Text to be used in later edits
 */
int
svnsup_delta_text(svnsup_delta_t sd, const char *src, size_t len,
    unsigned int *txtid)
{

	assert(sd != NULL);
	assert(src != NULL);
	assert(len > 0);
	assert(txtid != NULL);
	*txtid = sd->ntxt++;
	fprintf(sd->f, "@text %u ", *txtid);
	svnsup_buf_fencode(sd->f, (const unsigned char *)src, len);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Create a file and start working on it
 */
int
svnsup_delta_create_file(svnsup_delta_t sd, svnsup_delta_file_t *sdfp,
    const char *fn)
{
	svnsup_delta_file_t sdf;

	assert(sd != NULL);
	assert(sd->sdf == NULL);
	assert(sdfp != NULL);
	assert(fn != NULL && *fn != '\0');
	fn = svnsup_delta_shorten_path(sd, fn);
	if ((sdf = svnsup_delta_file_alloc(sd, fn)) == NULL)
		return (SVNSUP_ERR_MEMORY);
	sdf->create = 1;
	*sdfp = sdf;
	fprintf(sd->f, "@create ");
	svnsup_string_fencode(sd->f, fn);
	fprintf(sd->f, "\n");
	return (SVNSUP_ERR_NONE);
}

/*
 * Start working on the specified file
 */
int
svnsup_delta_open_file(svnsup_delta_t sd, svnsup_delta_file_t *sdfp,
    const char *fn)
{
	svnsup_delta_file_t sdf;

	assert(sd != NULL);
	assert(sd->sdf == NULL);
	assert(sdfp != NULL);
	assert(fn != NULL && *fn != '\0');
	fn = svnsup_delta_shorten_path(sd, fn);
	if ((sdf = svnsup_delta_file_alloc(sd, fn)) == NULL)
		return (SVNSUP_ERR_MEMORY);
	*sdfp = sdf;
	return (SVNSUP_ERR_NONE);
}

/*
 * Checksum of the original file
 */
int
svnsup_delta_file_checksum(svnsup_delta_file_t sdf, const char *md5)
{

	assert(sdf != NULL);
	assert(sdf->sd != NULL);
	assert(sdf->sd->sdf == sdf);
	assert(sdf->fn != NULL);
	assert(!sdf->create);
	assert(*sdf->fn != '\0');
	assert(md5 != NULL && *md5 != '\0');
	fprintf(sdf->sd->f, "@open ");
	svnsup_string_fencode(sdf->sd->f, sdf->fn);
	fprintf(sdf->sd->f, " md5 ");
	svnsup_string_fencode(sdf->sd->f, md5);
	fprintf(sdf->sd->f, "\n");
	sdf->checksum = 1;
	return (SVNSUP_ERR_NONE);
}

/*
 * Shortcut to svnsup_delta_text()
 */
int
svnsup_delta_file_text(svnsup_delta_file_t sdf, const char *src, size_t len,
    unsigned int *txtid)
{

	return (svnsup_delta_text(sdf->sd, src, len, txtid));
}

/*
 * Copy text from the original file to the new file
 */
int
svnsup_delta_file_copy(svnsup_delta_file_t sdf, off_t off, size_t size)
{

	assert(sdf != NULL);
	assert(sdf->sd != NULL);
	assert(sdf->sd->sdf == sdf);
	assert(sdf->create || sdf->checksum);
	assert(size > 0);
	fprintf(sdf->sd->f, "@copy %ju %zu\n", (uintmax_t)off, size);
	return (SVNSUP_ERR_NONE);
}

/*
 * Repeat text in the new file
 */
int
svnsup_delta_file_repeat(svnsup_delta_file_t sdf, off_t off, size_t size)
{

	assert(sdf != NULL);
	assert(sdf->sd != NULL);
	assert(sdf->sd->sdf == sdf);
	assert(sdf->create || sdf->checksum);
	assert(size > 0);
	fprintf(sdf->sd->f, "@repeat %ju %zu\n", (uintmax_t)off, size);
	return (SVNSUP_ERR_NONE);
}

/*
 * Insert text into the new file
 */
int
svnsup_delta_file_insert(svnsup_delta_file_t sdf, unsigned int txtid,
    off_t off, size_t size)
{

	assert(sdf != NULL);
	assert(sdf->sd != NULL);
	assert(sdf->sd->sdf == sdf);
	assert(sdf->create || sdf->checksum);
	assert(txtid < sdf->sd->ntxt);
	assert(size > 0);
	fprintf(sdf->sd->f, "@insert %u %ju %zu\n", txtid, (uintmax_t)off, size);
	return (SVNSUP_ERR_NONE);
}

/*
 * Stop working on the specified file
 */
int
svnsup_delta_close_file(svnsup_delta_file_t sdf, const char *md5)
{

	assert(sdf != NULL);
	assert(sdf->sd != NULL);
	assert(sdf->sd->sdf == sdf);
	assert(sdf->create || sdf->checksum);
	assert(md5 != NULL && *md5 != '\0');
	fprintf(sdf->sd->f, "@close ");
	svnsup_string_fencode(sdf->sd->f, sdf->fn);
	fprintf(sdf->sd->f, " md5 ");
	svnsup_string_fencode(sdf->sd->f, md5);
	fprintf(sdf->sd->f, "\n");
	svnsup_delta_file_free(sdf);
	return (SVNSUP_ERR_NONE);
}
