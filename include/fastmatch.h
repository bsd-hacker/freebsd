/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#ifndef FASTMATCH_H
#define FASTMATCH_H 1

#include <hashtable.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <wchar.h>

typedef struct {
  size_t	 wlen;
  size_t	 len;
  wchar_t	*wpattern;
  int		 hasdot;
  int		 qsBc[UCHAR_MAX + 1];
  int		*bmGs;
  char		*pattern;
  int		 defBc;
  hashtable	*qsBc_table;
  int		*sbmGs;

  /* flags */
  bool		 bol;
  bool		 eol;
  bool		 word;
  bool		 icase;
  bool		 newline;
} fastmatch_t;

extern int
tre_fixcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastexec(const fastmatch_t *preg, const char *string, size_t nmatch,
  regmatch_t pmatch[], int eflags);

extern void
tre_fastfree(fastmatch_t *preg);

extern int
tre_fixwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwexec(const fastmatch_t *preg, const wchar_t *string,
         size_t nmatch, regmatch_t pmatch[], int eflags);

/* Versions with a maximum length argument and therefore the capability to
   handle null characters in the middle of the strings. */
extern int
tre_fixncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastnexec(const fastmatch_t *preg, const char *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

extern int
tre_fixwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwnexec(const fastmatch_t *preg, const wchar_t *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

#endif		/* FASTMATCH_H */
