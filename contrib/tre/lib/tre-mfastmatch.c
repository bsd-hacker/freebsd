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
#include <mregex.h>
#include <regex.h>
#include "tre-mfastmatch.h"
#include "xmalloc.h"

/* TODO:
 *
 * - REG_ICASE
 * - Store pattern and sizes in pat/wpat/siz/wsiz
 * - Test
 */

#define WM_B 2

#define ALLOC(var, siz)							\
  var = xmalloc(siz);							\
  if (!var)								\
    {									\
      err = REG_ESPACE;							\
      goto fail;							\
    }

#define FAIL								\
  do									\
    {									\
      err = REG_BADPAT;							\
      goto fail;							\
    } while (0)

#define _PROC_WM(pat_arr, siz_arr, char_size, sh_field, m_field)	\
  /* Determine shortest pattern length */				\
  wm->m_field = size_arr[0];						\
  for (int i = 1; i < nr; i++)						\
    wm->m_field = size_arr[i] < wm->m_field ? size_arr[i] : wm->m_field;\
									\
  wm->sh_field = hashtable_init((wm->m_field - 1) * nr * 2, WM_B *	\
				char_size, sizeof(wmentry_t);		\
  if (!wm->sh_field)							\
    {									\
      err = REG_ESPACE;							\
      goto fail;							\
    }									\
									\
  ALLOC(entry, sizeof(wmentry_t));					\
  for (int i = 0; i < nr; i++)						\
    {									\
      int ret, sh;							\
									\
      /* First fragment, treat special because it is a prefix */	\
      ret = hashtable_get(wm->sh_field, pat_arr[i], entry);		\
      sh = size_arr[i] - WM_B;						\
      switch (ret)							\
        {								\
          case HASH_NOTFOUND:						\
            entry->shift = sh;						\
            entry->suff = 0;						\
            entry->pref = 1;						\
            entry->pref_list[0] = i;					\
            ret = hashtable_put(wm->sh_field, pat_arr[i], entry);	\
            if (ret != HASH_OK)						\
              FAIL;							\
            break;							\
          case HASH_OK:							\
            entry->shift = entry->shift < sh ? entry->shift : sh;	\
            entry->pref_list[entry->pref++] = i;			\
            if (ret != HASH_UPDATED)					\
              FAIL;							\
        }								\
      /* Intermediate fragments, only shift calculated */		\
      for (int j = 1; j < size_arr[i] - WB_M; j++)			\
        {								\
          ret = hashtable_get(wm->sh_field, &pat_arr[i][j], entry);	\
          sh = size_arr[i] - WM_B - j;					\
          switch (ret)							\
            {								\
              case HASH_NOTFOUND:					\
                entry->shift = sh;					\
                entry->suff = 0;					\
                entry->pref = 0;					\
                ret = hashtable_put(wm->sh_field, &pat_arr[i][j],	\
				    entry);				\
                if (ret != HASH_OK)					\
                  FAIL;							\
                break;							\
              case HASH_OK:						\
                entry->shift = entry->shift < sh ? entry->shift : sh;	\
                if (ret != HASH_UPDATED)				\
                  FAIL;							\
        }								\
      ret = hashtable_get(wm->sh_field, &pat_arr[i][n[i] - WB_M],	\
			  entry);					\
      switch (ret)							\
        {								\
          case HASH_NOTFOUND:						\
            entry->shift = sh = 0;					\
            entry->suff = 1;						\
            entry->pref = 0;						\
            entry->suff_list[0] = i;					\
            ret = hashtable_put(wm->sh_field, &pat_arr[i][n[i] - WB_M],	\
				entry);					\
            if (ret != HASH_OK)						\
              FAIL;							\
          case HASH_OK:							\
            entry->shift = entry->shift < sh ? entry->shift : sh;	\
            entry->suff_list[entry->suff++] = i;			\
            if (ret != HASH_UPDATED)					\
              FAIL;							\
        }								\
    }									\
  xfree(entry);

#ifdef TRE_WCHAR
#define PROC_WM(par_arr, size_arr)					\
  _PROC_WM(pat_arr, size_arr, 1, shift, m)
#define PROC_WM_WIDE(par_arr, size_arr)					\
  _PROC_WM(pat_arr, size_arr, sizeof(tre_char_t), wshift, wm)
#else
#define PROC_WM(par_arr, size_arr)					\
  _PROC_WM(pat_arr, size_arr, 1, shift, m)
#endif

/*
 * This file implements the Wu-Manber algorithm for pattern matching
 * with multiple patterns.  Even if it is not the best performing one
 * for low number of patterns but it scales well and it is very simple
 * compared to automaton-based multiple pattern algorithms.
 */

int
tre_wmcomp(mregex_t *preg, size_t nr, const char *regex[],
	   size_t n[], int cflags)
{
  wmentry_t *entry = NULL;
  wmsearch_t *wm = NULL;
  int err;
#ifdef TRE_WCHAR
  char **bregex;
  int *bn;
#endif

  ALLOC(wm, sizeof(wmsearch_t));
  preg->n = nr;

#ifdef TRE_WCHAR
  PROC_WM_WIDE(regex, n);

  ALLOC(bregex, sizeof(char *) * nr);
  ALLOC(bn, sizeof(int) * nr);

  for (int i = 0; i < nr; i++)
    {
      bn[i] = wcstombs(NULL, regex[i], 0);
      ALLOC(bregex[i], bn[i] + 1);
      ret = wcstombs(bregex[i], regex[i], bn[i]);

      /* Should never happen */
      if (ret == (size_t)-1)
	{
	  err = REG_BADPAT;
	  goto fail;
	}
    }
  PROC_WM(bregex, bn);
  for (int i = 0; i < nr; i++)
    xfree(bregex[i]);
  xfree(bregex);
#else
  PROC_WM(regex, n);
#endif

  preg->searchdata = &wm;
  return REG_OK;
fail:
#ifdef TRE_WCHAR
  if (wm->wshift)
    hashtable_free(wm->wshift);
  if (bregex)
    {
      for (int i = 0; i < nr; i++)
	if (bregex[i]
	  xfree(bregex[i]);
      xfree(bregex);
    }
#endif
  if (wm->shift)
    hashtable_free(wm->shift);
  if (wm)
    xfree(wm);
  if (entry)
    xfree(entry);
  return err;
}

#define MATCH(beg, end, p)						\
  do									\
    {									\
      match->rm_so = beg;						\
      match->rm_eo = end;						\
      match->p = p;							\
      err = REG_OK;							\
      goto finish;							\
    } while (0);

#define _WMSEARCH(data, pats, sizes, mlen, tbl, dshift)			\
  do									\
    {									\
      ret = hashtable_get(tbl, data[pos - WM_B], s_entry);		\
      shift = (ret == HASH_OK) ? s_entry->shift : dshift;		\
      if (shift == 0)							\
	{								\
	  ret = hashtable_get(tbl, data[pos - mlen, p_entry);		\
	  if (ret == HASH_NOTFOUND)					\
	    {								\
	      pos += 1;							\
	      continue;							\
	    }								\
	  else								\
	    {								\
	      for (int i = 0; i < p_entry->pref; i++)			\
		for (int j = 0; (j < s_entry->suff) &&			\
		    (s_entry->suff_list[j] <= p_entry->pref_list[i]);	\
		     j++)						\
		  if (s_entry->suff_list[j] == p_entry->pref_list[i])	\
                    {							\
		      size_t idx = s_entry->suff_list[j];		\
		      int k;						\
									\
		      if (len - pos > sizes[idx] - mlen)		\
			{						\
			  for (k = WM_B; k < sizes[idx]; k++)		\
			    if (pats[idx][k] != data[pos - mlen + k])	\
			      break;					\
			  if (k == sizes[idx])				\
			    // XXX: match				\
			}						\
		    }							\
		  else							\
		     continue;						\
	      pos += 1;							\
            }								\
	}								\
      else								\
	pos += shift;							\
    } while (0);

#define WMSEARCH							\
  _WMSEARCH(byte_str, wm->pat, wm->siz, wm->m, wm->hash,		\
	    wm->defsh)
#define WMSEARCH_WIDE							\
   _WMSEARCH(wide_str, wm->wpats, wm->wsiz, wm->wm, wm->whash,		\
	     wm->wdefsh)

int
tre_wmexec(const void *str, size_t len, tre_str_type_t type,
	   size_t nmatch, regmatch_t pmatch[], int eflags,
	   const mregex_t *preg, regmatch_t *match)
{
  wmsearch_t *wm = preg->wm;
  wmentry_t *s_entry, *p_entry;
  tre_char_t *wide_str = str;
  char *byte_str = str;
  size_t pos = preg->m;
  size_T shift;
  int ret;
  int err = REG_NOMATCH;

  ALLOC(s_entry, sizeof(wmentry_t));
  ALLOC(p_entry, sizeof(wmentry_t));

  while (pos < len)
    {
      if (type == STR_WIDE)
	WMSEARCH;
      else
	WMSEARCH_WIDE;
    }

fail:
finish:
  if (s_entry)
    xfree(s_entry);
  if (p_entry)
    xfree(p_entry);
  return err;
}

void
wmfree(mregex_t *preg)
{
  wmsearch_t wm = preg->wm;

  if (wm->hash)
    hashtable_free(wm->hash);
  for (int i = 0; i < wm->n; i++)
    if (wm->pat[i])
      xfree(wm->pat[i]);
  if (wm->pat)
    xfree(wm->pat);
  if (wm->siz)
    xfree(wm->siz);
#ifdef TRE_WCHAR
  if (wm->whash)
    hashtable_free(wm->whash);
  for (int i = 0; i < wm->wn; i++)
    if (wm->wpat[i])
      xfree(wm->wpat[i]);
  if (wm->wpat)
    xfree(wm->wpat);
  if (wm->wsiz)
    xfree(wm->wsiz);
#endif
}
