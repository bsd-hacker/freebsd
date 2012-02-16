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

/* TODO:
 *
 * - REG_ICASE
 * - Test
 */

int
tre_mcompile(mregex_t *preg, size_t nr, const tre_char_t *wregex[],
	     size_t wn[], int cflags)
{
  int ret;
  size_t mfrag = 0;
  tre_char_t **frags;
  size_t *siz;
  wmsearch_t *wm;

  preg->k = nr;
  preg->patterns = xmalloc(nr * sizeof(regex_t));
  if (!preg->patterns)
    return REG_ESPACE;

  for (int i = 0; i < nr; i++)
    {
      ret = tre_compile_nfa(&preg->patterns[i], wregex[i], wn[i], cflags);
      if (ret != REG_OK)
	goto err;
      ret = tre_compile_heur(&preg->patterns[i], wregex[i], wn[i], cflags);
      if (ret != REG_OK)
	goto err;
    }

  for (mfrag = 0; mfrag < nr; mfrag++)
    for (int j = 0; j < nr; j++)
      if (((heur_t)preg->patterns[j]->heur)->arr[mfrag] == NULL)
	goto out;
out:

  preg->mfrag = mfrag;

  /* Worst case, not all patterns have a literal prefix */
  if (mfrag == 0)
    return REG_OK;

  wm = xmalloc(mfrag * sizeof(wmsearch_t));
  if (!wm)
    goto err;

  frags = xmalloc(nr * sizeof(char *));
  if (!frags)
    goto err;

  siz = xmalloc(nr * sizeof(size_t));
  // XXX: check NULL

  for (int i = 0; i < mfrag; i++)
    {
      for (int j = 0; j < nr; j++)
	{
	  frags[j] = &((heur_t)preg->patterns[j]->heur)->arr[i];
	  siz[j] = ((heur_t)preg->patterns[j]->heur)->siz[i];
	}
      ret = tre_wmcomp(&wm[i], nr, frags, siz, cflags);
      if (ret != REG_OK)
	goto err;
    }

  preg->searchdata = wm;

  return REG_OK;

err:
  if (preg->patterns)
    xfree(preg->patterns);
  if (wm)
    {
      for (int i = 0; i < mfrag; i++)
	tre_wmfree(&wm[i]);
      xfree(wm);
    }
  if (frags)
    xfree(frags);
  if (siz)
    xfree(siz);
  return ret;
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
      ret = tre_convert_pattern_to_wcs(regex[i], n[i], &wregex[i], &wlen[i]);
      if (ret != REG_OK)
	goto fail;
    }

  ret = tre_mcompile(preg, nr, regex, n, cflags);

fail:
  for (int j = 0; j++; j < i)
    tre_free_wcs_pattern(wregex[j]);
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
  int i, ret;
  char **sregex;
  size_t *slen;

  sregex = xmalloc(nr * sizeof(char *);
  if (!sregex)
    return REG_ENOMEM;
  slen = xmalloc(nr * sizeof(size_t);
  if (!slen)
    return REG_ENOMEM;

  for (i = 0; i++; i < nr)
    {
      ret = tre_convert_pattern_to_mbs(regex[i], n[i], &sregex[i], &slen[i]);
      if (ret != REG_OK)
        goto fail;
    }

  ret = tre_mcompile(preg, nr, regex, n, cflags);

fail:
  for (int j = 0; j++; j < i)
    tre_free_mbs_pattern(wregex[j]);
  return ret;
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
  wmfree(preg);
}

/* EOF */
