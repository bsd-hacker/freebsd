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
#include <mregex.h>
#include <regex.h>
#include <stdlib.h>

#include "tre-heuristic.h"
#include "tre-internal.h"
#include "tre-mfastmatch.h"
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
tre_mcompile(mregex_t *preg, size_t nr, const wchar_t **wregex,
	     size_t *wn, const char **regex, size_t *n, int cflags)
{
  int ret;
  const wchar_t **frags;
  size_t *siz;
  wmsearch_t *wm;

  preg->k = nr;
  preg->patterns = xmalloc(nr * sizeof(regex_t));
  if (!preg->patterns)
    return REG_ESPACE;

  /* Compile NFA, BM and heuristic for each pattern. */
  for (int i = 0; i < nr; i++)
    {
      ret = tre_compile(&preg->patterns[i], wregex[i], wn[i],
			regex[i], n[i], cflags);
      if (ret != REG_OK)
	{
	  preg->err = i;
	  goto err;
	}
    }

  if (nr == 1)
    {
      preg->type = MHEUR_SINGLE;
      goto finish;
    }

  /* If not literal, check if any of them have fixed-length prefix. */
  if (!(cflags & REG_LITERAL))
    for (int i = 0; i < nr; i++)
      if ((preg->patterns[i].heur == NULL) ||
	 (((heur_t *)(preg->patterns[i].heur))->arr[0] == NULL))
	{
	  preg->type = MHEUR_NONE;
	  goto finish;
	}

  /*
   * Set frag and siz to point to the fragments to compile and
   * their respective sizes.
   */
  if (!(cflags & REG_LITERAL))
    {
      frags = xmalloc(nr * sizeof(char *));
      if (!frags)
        goto err;

      siz = xmalloc(nr * sizeof(size_t));
      if (!siz)
	goto err;

      for (int j = 0; j < nr; j++)
	{
	  frags[j] = ((heur_t *)(preg->patterns[j].heur))->warr[0];
	  siz[j] = ((heur_t *)(preg->patterns[j].heur))->siz[0];
	}
    }
  else
    {
      frags = wregex;
      siz = wn;
    }

  /* Alloc and compile the fragments for Wu-Manber algorithm. */
  wm = xmalloc(sizeof(wmsearch_t));
  if (!wm)
    goto err;
  ret = tre_wmcomp(wm, nr, frags, siz, cflags);
    if (ret != REG_OK)
      goto err;
  wm->cflags = cflags;
  preg->searchdata = wm;

  /* Set the specific type of matching. */
  if (cflags & REG_LITERAL)
    preg->type = MHEUR_LITERAL;
  else if (cflags & REG_NEWLINE)
    preg->type = MHEUR_LONGEST;
  else
    preg->type = MHEUR_PREFIX;

  goto finish;

err:
  if (preg->patterns)
    {
      for (int i = 1; i < nr; i++)
	tre_regfree(&preg->patterns[i]);
      xfree(preg->patterns);
    }
  if (wm)
    xfree(wm);

finish:
  if (!(cflags & REG_LITERAL))
    {
      if (frags)
	xfree(frags);
      if (siz)
	xfree(siz);
    }
  return ret;
}

int
tre_mregncomp(mregex_t *preg, size_t nr, const char **regex,
	      size_t *n, int cflags)
{
  int i, ret;
  const wchar_t **wr;
  wchar_t **wregex;
  size_t *wlen;

  wregex = xmalloc(nr * sizeof(wchar_t *));
  if (!wregex)
    return REG_ESPACE;
  wlen = xmalloc(nr * sizeof(size_t));
  if (!wlen)
    goto err;

  for (i = 0; i < nr; i++)
    {
      ret = tre_convert_pattern_to_wcs(regex[i], n[i], &wregex[i], &wlen[i]);
      if (ret != REG_OK)
	goto err;
    }

  wr = (const wchar_t **)wregex;
  ret = tre_mcompile(preg, nr, wr, wlen, regex, n, cflags);

err:
  if (wregex)
    {
      for (int j = 0; j < i; j++)
	if (wregex[j])
	  tre_free_wcs_pattern(wregex[j]);
    }
  if (wlen)
    xfree(wlen);
  return ret;
}

int
tre_mregcomp(mregex_t *preg, size_t nr, const char **regex, int cflags)
{
  int ret;
  size_t *len;

  len = xmalloc(nr * sizeof(size_t));
  if (!len)
    return REG_ESPACE;

  for (int i = 0; i < nr; i++)
    len[i] = strlen(regex[i]);

  ret = tre_mregncomp(preg, nr, regex, len, cflags);
  xfree(len);
  return ret;
}


#ifdef TRE_WCHAR
int
tre_mregwncomp(mregex_t *preg, size_t nr, const wchar_t **regex,
	       size_t *n, int cflags)
{
  int i, ret;
  const char **sr;
  char **sregex;
  size_t *slen;

  sregex = xmalloc(nr * sizeof(char *));
  if (!sregex)
    return REG_ESPACE;
  slen = xmalloc(nr * sizeof(size_t));
  if (!slen)
    goto err;

  for (i = 0; i < nr; i++)
    {
      ret = tre_convert_pattern_to_mbs(regex[i], n[i], &sregex[i], &slen[i]);
      if (ret != REG_OK)
        goto err;
    }

  sr = (const char **)sregex;
  ret = tre_mcompile(preg, nr, regex, n, sr, slen, cflags);

err:
  if (sregex)
    {
      for (int j = 0; j < i; j++)
	if (sregex[j])
	  tre_free_mbs_pattern(sregex[j]);
    }
  if (slen)
    xfree(slen);
  return ret;
}

int
tre_mregwcomp(mregex_t *preg, size_t nr, const wchar_t **regex,
	      int cflags)
{
  int ret;
  size_t *len;

  len = xmalloc(nr * sizeof(size_t));
  if (!len)
    return REG_ESPACE;

  for (int i = 0; i < nr; i++)
    len[i] = tre_strlen(regex[i]);

  ret = tre_mregwncomp(preg, nr, regex, len, cflags);
  xfree(len);
  return ret;
}
#endif /* TRE_WCHAR */

void
tre_mregfree(mregex_t *preg)
{
  if (!preg)
    {
      tre_wmfree(preg->searchdata);
      xfree(preg);
    }
}

/* EOF */
