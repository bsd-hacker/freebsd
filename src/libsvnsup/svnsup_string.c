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

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "svnsup.h"

/*
 * Safe to send as is
 */
int
svnsup_string_is_safe(const char *str)
{

	while (*str != '\0') {
		if (!isprint(*str) || isspace(*str))
			return (0);
		++str;
	}
	return (1);
}

/*
 * Safe to send as is
 */
int
svnsup_buf_is_safe(const char *buf, size_t size)
{

	while (size > 0) {
		if (!isprint(*buf) || isspace(*buf))
			return (0);
		++buf;
		--size;
	}
	return (1);
}

char *
svnsup_string_encode(const char *str)
{

	assert(0);
	(void)str;
	return (NULL);
}

char *
svnsup_buf_encode(const char *buf, size_t size)
{

	assert(0);
	(void)buf;
	(void)size;
	return (NULL);
}

size_t
svnsup_string_fencode(FILE *f, const char *str)
{

	return (svnsup_buf_fencode(f, str, strlen(str)));
}

size_t
svnsup_buf_fencode(FILE *f, const char *buf, size_t size)
{
	int len;

	if (svnsup_buf_is_safe(buf, size))
		return (fprintf(f, "%zu[%.*s]", size, (int)size, buf));
	len = fprintf(f, "%zu{", size);
	len += svnsup_base64_fencode(f, (const unsigned char *)buf, size);
	len += fprintf(f, "}");
	return (len);
}
