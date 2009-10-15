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

#include <stdarg.h>
#include <stdio.h>

#include "distill.h"

static inline void
svnsup_print_where(FILE *f, svnsup_where_t *where)
{
	if (debug) {
		fprintf(f, "svnsup: in %s() on line %d of %s\n",
		    where->func, where->line, where->file);
	}
}

void
svnsup_apr_error(svnsup_where_t *where, apr_status_t status,
    const char *fmt, ...)
{
	char errbuf[1024];
	va_list ap;

	fprintf(stderr, "svnsup: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	apr_strerror(status, errbuf, sizeof(errbuf));
	fprintf(stderr, "\nsvnsup: %s\n", errbuf);
	svnsup_print_where(stderr, where);
	exit(1);
}

void
svnsup_svn_error(svnsup_where_t *where, svn_error_t *error,
    const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "svnsup: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	svn_handle_error2(error, stderr, FALSE, "svnsup: ");
	svnsup_print_where(stderr, where);
	exit(1);
}

void
svnsup_assert(svnsup_where_t *where, const char *cond,
    const char *fmt, ...)
{
	va_list ap;

	if (debug)
		fprintf(stderr, "svnsup: assertion failed: %s\n", cond);
	fprintf(stderr, "svnsup: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	svnsup_print_where(stderr, where);
	exit(1);
}
