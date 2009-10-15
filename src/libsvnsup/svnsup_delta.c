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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "svnsup.h"

struct svnsup_delta {
	FILE *f;
	int started:1;
};

int
svnsup_delta_create(svnsup_delta_t *sdp)
{
	svnsup_delta_t sd;

	if ((sd = calloc(1, sizeof *sd)) == NULL)
		return (SVNSUP_ERR_MEMORY);
	sd->f = stdout;
	*sdp = sd;
	return (SVNSUP_ERR_NONE);
}

int
svnsup_delta_close(svnsup_delta_t sd)
{

	free(sd);
	return (SVNSUP_ERR_NONE);
}

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
		fputs("# ", sd->f);
		while (*p != '\0' && *p != '\n') {
			putc(isprint(*p) ? *p : ' ', sd->f);
			++p;
		}
		putc('\n', sd->f);
		if (*p == '\n')
			++p;
	}
	free(commentbuf);
	return (SVNSUP_ERR_NONE);
}
