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

#include <limits.h>
#include <mregex.h>
#include <regex.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */

#include "tre-heuristic.h"
#include "tre-internal.h"
#include "tre-mfastmatch.h"
#include "xmalloc.h"

#ifdef TRE_LIBC_BUILD
__weak_reference(tre_mregexec, mregexec);
__weak_reference(tre_mregnexec, mregnexec);
__weak_reference(tre_mregwexec, mregwexec);
__weak_reference(tre_mregwnexec, mregwnexec);
#endif

int
tre_mmatch(const void *str, size_t len, tre_str_type_t type,
	   size_t nmatch, regmatch_t pmatch[], int eflags,
	   const mregex_t *preg)
{
  const tre_char_t *str_wide = str;
  const char *str_byte = str;
  int ret;
  bool need_offsets;

  need_offsets = (((wmsearch_t *)(preg->searchdata))->cflags & REG_NOSUB) &&
		 (nmatch > 0);

#define INPUT(pos) ((type == STR_WIDE) ? (const void *)&str_wide[pos] : \
		   (const void *)&str_byte[pos])

  /*
   * Worst case: at least one pattern does not have a literal
   * prefix so the Wu-Manber algorithm cannot be used to speed
   * up the match.  There is no trivial best solution either,
   * so just try matching for each of the patterns and return
   * the earliest.
   */
  if (preg->type == MHEUR_NONE)
    {
      regmatch_t **pm;
      int i, first = -1;

      /* Alloc one regmatch_t for each pattern */
      if (need_offsets)
	{
	  pm = xmalloc(preg->k * sizeof(regmatch_t *));
	  if (!pm)
	    return REG_ESPACE;
	  for (i = 0; i < preg->k; i++)
	    {
	      pm[i] = xmalloc(nmatch * sizeof(regmatch_t));
	      if (!pm[i])
		goto finish1;
	    }
	}

      /* Run matches for each pattern and save first matching offsets. */
      for (i = 0; i < preg->k; i++)
	{
	  ret = tre_match(&preg->patterns[i], str, len, type,
			  need_offsets ? nmatch : 0, pm[i], eflags);

	  /* Mark if there is no match for the pattern. */
	  if (!need_offsets)
	    {
	      if (ret == REG_OK)
		return REG_OK;
	    }
	  else if (ret == REG_NOMATCH)
	    pm[i][0].rm_so = -1;
	  else if (ret != REG_OK)
	    goto finish1;
	}

      if (!need_offsets)
	return REG_NOMATCH;

      /* Check whether there has been a match at all. */
      for (i = 0; i < preg->k; i++)
	if (pm[i][0].rm_so != -1)
	  {
	    first = i;
	    break;
	  }

      if (first == -1)
	ret = REG_NOMATCH;

      /* If there are matches, find the first one. */
      else
	{
	  for (i = first + 1; i < preg->k; i++)
	    if ((pm[i][0].rm_so < pm[first][0].rm_so) || (pm[i][0].rm_eo > pm[first][0].rm_eo))
	      {
		first = i;
	      }
	}

      /* Fill int the offsets before returning. */
      for (i = 0; need_offsets && (i < 0); i++)
	{
	  pmatch[i].rm_so = pm[first][i].rm_so;
	  pmatch[i].rm_eo = pm[first][i].rm_eo;
	  pmatch[i].p = i;
	}
      ret = REG_OK;

finish1:
      if (pm)
	{
	  for (i = 0; i < preg->k; i++)
	    if (pm[i])
	      xfree(pm[i]);
	  xfree(pm);
	}
      return ret;
    }

  /*
   * REG_NEWLINE: only searching the longest fragment of each
   * pattern and then isolating the line and calling the
   * automaton.
   */
  else if (preg->type == MHEUR_LONGEST)
    {
      regmatch_t *pm, rpm;
      size_t st = 0, bl, el;

      /* Alloc regmatch_t structures for results */
      if (need_offsets)
	{
	  pm = xmalloc(nmatch * sizeof(regmatch_t *));
	  if (!pm)
	    return REG_ESPACE;
        }

      while (st < len)
	{
	  /* Look for a possible match. */
	  ret = tre_wmexec(preg->searchdata, INPUT(st), len, type, 1, &rpm,
			   eflags);
	  if (ret != REG_OK)
	    goto finish2;

	  /* Need to start from here if this fails. */
	  st += rpm.rm_so + 1;

	  /* Look for the beginning of the line. */
	  for (bl = st; bl > 0; bl--)
	    if ((type == STR_WIDE) ? (str_wide[bl] == TRE_CHAR('\n')) :
		(str_byte[bl] == '\n'))
	      break;

	  /* Look for the end of the line. */
	  for (el = st; el < len; el++)
	   if ((type == STR_WIDE) ? (str_wide[el] == TRE_CHAR('\n')) :
	        (str_byte[el] == '\n'))
	      break;

	  /* Try to match the pattern on the line. */
	  ret = tre_match(&preg->patterns[rpm.p], INPUT(bl), el - bl,
			  type, need_offsets ? nmatch : 0, pm, eflags);

	  /* Evaluate result. */
	  if (ret == REG_NOMATCH)
	    continue;
	  else if (ret == REG_OK)
	    {
	      if (!need_offsets)
		return REG_OK;
	      else
		{
		  for (int i = 0; i < nmatch; i++)
		    {
		      pmatch[i].rm_so = pm[i].rm_so;
		      pmatch[i].rm_eo = pm[i].rm_eo;
		      pmatch[i].p = rpm.p;
		      goto finish2;
		    }
		}
	    }
	  else
	    goto finish2;
	}
finish2:
      if (!pm)
	xfree(pm);
      return ret;
    }

  /*
   * Literal case.  It is enough to search with Wu-Manber and immediately
   * return the match.
   */
  else if (preg->type == MHEUR_LITERAL)
    {
      return tre_wmexec(preg->searchdata, str, len, type, nmatch, pmatch,
			eflags);
    }

  /*
   * General case. Look for the beginning of any of the patterns with the
   * Wu-Manber algorithm and try to match from there with the automaton.
   */
  else
    {
      regmatch_t *pm, rpm;
      size_t st = 0;

      /* Alloc regmatch_t structures for results */
      if (need_offsets)
	{
	  pm = xmalloc(nmatch * sizeof(regmatch_t *));
	  if (!pm)
	    return REG_ESPACE;
	}

      while (st < len)
	{
	  ret = tre_wmexec(preg->searchdata, INPUT(st), len, type, nmatch,
			   &rpm, eflags);
	  if (ret != REG_OK)
	    return ret;

	  ret = tre_match(&preg->patterns[rpm.p], INPUT(rpm.rm_so),
			  len - rpm.rm_so, type, need_offsets ? nmatch : 0,
			  pm, eflags);
	  if ((ret == REG_OK) && (pm[0].rm_so == 0))
	    {
	      for (int i = 0; i < nmatch; i++)
		{
		  pm[i].rm_so += st;
		  pm[i].rm_eo += st;
		  pm[i].p = rpm.p;
		}
	      goto finish3;
	    }
	  else if ((ret != REG_NOMATCH) || (ret != REG_OK))
	    goto finish3;
	  st += rpm.rm_so + 1;
	}

finish3:
      if (pm)
	xfree(pm);
      return ret;
    }

}

int
tre_mregnexec(const mregex_t *preg, const char *str, size_t len,
	 size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = (TRE_MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;

  if (eflags & REG_STARTEND)
    CALL_WITH_OFFSET(tre_mmatch(&str[offset], slen, type, nmatch,
                     pmatch, eflags, preg));
  else
    return tre_mmatch(str, len == (unsigned)-1 ? strlen(str) : len,
		      type, nmatch, pmatch, eflags, preg);
}

int
tre_mregexec(const mregex_t *preg, const char *str,
	size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_mregnexec(preg, str, (unsigned)-1, nmatch, pmatch, eflags);
}


#ifdef TRE_WCHAR

int
tre_mregwnexec(const mregex_t *preg, const wchar_t *str, size_t len,
	  size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = STR_WIDE;

  if (eflags & REG_STARTEND)
    CALL_WITH_OFFSET(tre_mmatch(&str[offset], slen, type, nmatch,
		     pmatch, eflags, preg));
  else
    return tre_mmatch(str, len == (unsigned)-1 ? tre_strlen(str) : len,
		      STR_WIDE, nmatch, pmatch, eflags, preg);
}

int
tre_mregwexec(const mregex_t *preg, const wchar_t *str,
	 size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_mregwnexec(preg, str, (unsigned)-1, nmatch, pmatch, eflags);
}

#endif /* TRE_WCHAR */

/* EOF */
