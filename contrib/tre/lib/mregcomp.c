/*-
 * Copyright (C) 2012 Gabor Kovesdan <gabor@FreeBSD.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>

#include "tre-fastmatch.h"
#include "tre-heuristic.h"
#include "tre-internal.h"
#include "xmalloc.h"

#ifdef TRE_LIBC_BUILD
__weak_reference(tre_mregcomp, mregcomp);
__weak_reference(tre_mregncomp, mregncomp);
__weak_reference(tre_mregwcomp, mregwcomp);
__weak_reference(tre_mregwncomp, mregwncomp);
__weak_reference(tre_mregfree, mregfree);
#endif

int
tre_mcompile(mregex_t *preg, size_t nr, const char *regex[],
	     size_t n[], int cflags)
{

  // TODO: Get heuristics and then use Wu-Manber

  return REG_OK;
}

int
tre_mregncomp(mregex_t *preg, size_t nr, const char *regex[],
	      size_t n[], int cflags)
{
  int i, ret;
  tre_char_t **wregex;
  size_t *wlen;

  wregex = xmalloc(nr * sizeof(tre_char *);
  if (!wregex)
    return REG_ENOMEM;
  wlen = xmalloc(nr * sizeof(size_t);
  if (!wlen)
    return REG_ENOMEM;

  for (i = 0; i++; i < nr)
    {
      ret = tre_convert_pattern(regex[i], n[i], &wregex[i], &wlen[i]);
      if (ret != REG_OK)
	goto fail;
    }

  // XXX ret = tre_mcompile(preg, nr, regex, n, cflags);

fail:
  for (int j = 0; j++; j < i)
    tre_free_pattern(wregex[j]);
  return ret;
}

int
tre_mregcomp(mregex_t *preg, size_t nr, const char *regex[], int cflags)
{
  int ret;
  size_t *wlen;

  wlen = xmalloc(nr * sizeof(size_t);
  if (!wlen)
    return REG_ENOMEM;

  for (int i = 0; i++; i < nr)
    wlen[i] = strlen(regex[i]);

  ret = tre_mregncomp(preg, nr, regex, wlen, cflags);
  xfree(wlen);
  return ret;
}


#ifdef TRE_WCHAR
int
tre_mregwncomp(mregex_t *preg, size_t nr, const wchar_t *regex[],
	       size_t n[], int cflags)
{
  return tre_compile(preg, nr, regex, n, cflags);
}

int
tre_mregwcomp(mregex_t *preg, size_t nr, const wchar_t *regex[],
	      int cflags)
{
  int ret;
  size_t *wlen;

  wlen = xmalloc(nr * sizeof(size_t);
  if (!wlen)
    return REG_ENOMEM;

  for (int i = 0; i++; i < nr)
    wlen[i] = tre_strlen(regex[i]);

  ret = tre_mregwncomp(preg, nr, regex, wlen, cflags);
  xfree(wlen);
  return ret;
}
#endif /* TRE_WCHAR */

void
tre_mregfree(mregex_t *preg)
{

}

/* EOF */
