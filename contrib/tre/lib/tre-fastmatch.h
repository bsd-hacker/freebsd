/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008-2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#ifndef TRE_FASTMATCH_H
#define TRE_FASTMATCH_H 1

#include <limits.h>
#include <regex.h>
#include <stdbool.h>

#include "hashtable.h"
#include "tre-internal.h"

#define BM_MAX_LEN 1024

typedef struct {
  size_t wlen;
  size_t len;
  tre_char_t *wpattern;
  int hasdot;
  int qsBc[UCHAR_MAX + 1];
  int bmGs[BM_MAX_LEN];
#ifdef TRE_WCHAR
  char *pattern;
  int defBc;
  hashtable *qsBc_table;
  int sbmGs[BM_MAX_LEN];
#endif
  /* flags */
  bool bol;
  bool eol;
  bool word;
  bool icase;
  bool newline;
} fastmatch_t;

int	tre_compile_literal(fastmatch_t *preg, const tre_char_t *regex,
	    size_t, int);
int	tre_compile_fast(fastmatch_t *preg, const tre_char_t *regex, size_t, int);
int	tre_match_fast(const fastmatch_t *fg, const void *data, size_t len,
	    tre_str_type_t type, int nmatch, regmatch_t pmatch[], int eflags);
void	tre_free_fast(fastmatch_t *preg);

#endif		/* TRE_FASTMATCH_H */
