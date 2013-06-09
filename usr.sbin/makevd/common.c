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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

static int rawcopy(int, int, ssize_t, int);
static int writebuf(int, void *, ssize_t, ssize_t, int);

char *
uuid_str2bin(void *dst, const void *src)
{
	const char *p;
	char *q;
	char buf[3];
	ssize_t len;

	p = src;
	len = 0;
#if _BYTE_ORDER == _BIG_ENDIAN
	q = (char *)&dst + 16;
#else
	q = (char *)&dst;
#endif
	while (len < 16 && strlen(p) > 1) {
		long digit;
		char *endptr;

		if (*p == '-') {
			p++;
			continue;
		}
		buf[0] = p[0];
		buf[1] = p[1];
		buf[2] = '\0';
		errno = 0;
		digit = strtol(buf, &endptr, 16);
		if (errno == 0 && *endptr != '\0')
		    errno = EINVAL;
		if (errno) {
			warn("invalid UUID");
			return (NULL);
		}
#if _BYTE_ORDER == _BIG_ENDIAN
		*q-- = digit;
#else
		*q++ = digit;
#endif
		len++;
		p += 2;
	}
#if 0
	{
		int i;

		printf("uuid = ");
		for (i = 0; i < 16; i++)
			printf("%02x", uuid[i]);
		printf("\n");
	}
#endif
	return (dst);
}

char *
uuid_bin2str(void *dst, const void *src)
{
	const char *p;
	char *q;

	p = src;
	q = dst;
	while (p - (char *)src < 16) {
		snprintf(q, 3, "%02x", *p++);
		q += 2;
	}
	q = '\0';

	return (dst);
}

int
dispatch_bl(int ofd, struct blhead_t *blhead)
{
	struct blist *bl;
	int error;

	TAILQ_FOREACH(bl, blhead, bl_next) {
		printf("processing section: %s\n", bl->bl_name);
		switch (bl->bl_type) {
		case BL_RAWCOPY:
			error = rawcopy(ofd, bl->bl_tf.fd,
			    bl->bl_tf.chunksize,
			    bl->bl_tf.padding);
			break;
		case BL_RAWDATA:
			error = writebuf(ofd, bl->bl_tr.data,
			    bl->bl_tr.len,
			    bl->bl_tr.chunksize,
			    bl->bl_tr.padding);
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
rawcopy(int ofd, int ifd, ssize_t chunksize, int padding)
{
	ssize_t rlen, wlen, len;
	size_t remain;
	u_char buf[BUFSIZ];
	u_char zero = 0;

	if (chunksize == 0)
		chunksize = sizeof(buf);
	remain = chunksize;
	for (;;) {
		rlen = (remain < sizeof(buf)) ? remain : sizeof(buf);
		len = read(ifd, buf, rlen);
		if (len == 0)
			break;
		if (len < 0) {
			warn("read error");
			return (1);
		}
		wlen = len;
		len = write(ofd, buf, wlen);
		if (len < 0) {
			warn("write error");
			return (1);
		}
		remain -= rlen;
		if (remain == 0)
			remain = chunksize;
	}
	if (padding) {
		lseek(ofd, remain - 1, SEEK_CUR);
		len = write(ofd, &zero, 1);
		if (len != 1) {
			warn("write error");
			return (1);
		}
	}
	return (0);
}

static int
writebuf(int ofd, void *buf, ssize_t len, ssize_t chunksize, int padding)
{
	ssize_t len0, blks;
	int i;
	u_char *p;
	u_char zero = 0;

	if (chunksize == 0)
		chunksize = len;
	p = (u_char *)buf;

	blks = (len + chunksize - 1)/ chunksize;

	for (i = 0; i < blks; i++) {
		int l;

		fprintf(stderr, "Write %zd\n", chunksize);
		fprintf(stderr, "Data:");
		for (l = 0; l < chunksize; l++) {
			fprintf(stderr, "%02x", *(p+l));
		}
		fprintf(stderr, "\n");
		len0 = write(ofd, p, chunksize);
		if (len0 != chunksize) {
			warn("write error");
			return (1);
		}
		p += chunksize;
	}
	if (0 < len % chunksize) {
		len0 = write(ofd, p, len % chunksize);
		if (len0 != len % chunksize) {
			warn("write error");
			return (1);
		}
		if (padding) {
			lseek(ofd, chunksize - (len % chunksize) - 1,
			      SEEK_CUR);
			len0 = write(ofd, &zero, 1);
			if (len0 != 1) {
				warn("write error");
				return (1);
			}
		}
	}
	return (0);
}
