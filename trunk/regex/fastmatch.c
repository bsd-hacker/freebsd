/* $FreeBSD$ */

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

#include "glue.h"

#include <fastmatch.h>
#include <regex.h>
#include <string.h>

#include "tre-fastmatch.h"
#include "xmalloc.h"

/* XXX: avoid duplication */
#define CONV_PAT							\
  int ret;								\
  tre_char_t *wregex;							\
  size_t wlen;								\
									\
  wregex = xmalloc(sizeof(tre_char_t) * (n + 1));			\
  if (wregex == NULL)							\
    return REG_ESPACE;							\
									\
  if (TRE_MB_CUR_MAX == 1)						\
    {									\
      unsigned int i;							\
      const unsigned char *str = (const unsigned char *)regex;		\
      tre_char_t *wstr = wregex;					\
									\
      for (i = 0; i < n; i++)						\
        *(wstr++) = *(str++);						\
      wlen = n;								\
    }									\
  else									\
    {									\
      int consumed;							\
      tre_char_t *wcptr = wregex;					\
      mbstate_t state;							\
      memset(&state, '\0', sizeof(state));				\
      while (n > 0)							\
        {								\
          consumed = tre_mbrtowc(wcptr, regex, n, &state);		\
									\
          switch (consumed)						\
            {								\
            case 0:							\
              if (*regex == '\0')					\
                consumed = 1;						\
              else							\
                {							\
                  xfree(wregex);					\
                  return REG_BADPAT;					\
                }							\
              break;							\
            case -1:							\
              DPRINT(("mbrtowc: error %d: %s.\n", errno,		\
		strerror(errno)));					\
              xfree(wregex);						\
              return REG_BADPAT;					\
            case -2:							\
              consumed = n;						\
              break;							\
            }								\
          regex += consumed;						\
          n -= consumed;						\
          wcptr++;							\
        }								\
      wlen = wcptr - wregex;						\
    }									\
									\
  wregex[wlen] = L'\0';

int
tre_fixncomp(fastmatch_t *preg, const char *regex, size_t n, int cflags)
{
  CONV_PAT;

  ret = tre_compile_literal(preg, wregex, wlen, cflags);
  xfree(wregex);

  return ret;
}

int
tre_fastncomp(fastmatch_t *preg, const char *regex, size_t n, int cflags)
{
  CONV_PAT;

  ret = (cflags & REG_LITERAL) ?
    tre_compile_literal(preg, wregex, wlen, cflags) :
    tre_compile_fast(preg, wregex, wlen, cflags);
  xfree(wregex);

  return ret;
}


int
tre_fixcomp(fastmatch_t *preg, const char *regex, int cflags)
{
  return tre_fixncomp(preg, regex, regex ? strlen(regex) : 0, cflags);
}

int
tre_fastcomp(fastmatch_t *preg, const char *regex, int cflags)
{
  return tre_fastncomp(preg, regex, regex ? strlen(regex) : 0, cflags);
}

int
tre_fixwncomp(fastmatch_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  return tre_compile_literal(preg, regex, n, cflags);
}

int
tre_fastwncomp(fastmatch_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  return (cflags & REG_LITERAL) ?
    tre_compile_literal(preg, regex, n, cflags) :
    tre_compile_fast(preg, regex, n, cflags);
}

int
tre_fixwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags)
{
  return tre_fixwncomp(preg, regex, regex ? tre_strlen(regex) : 0, cflags);
}

int
tre_fastwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags)
{
  return tre_fastwncomp(preg, regex, regex ? tre_strlen(regex) : 0, cflags);
}

void
tre_fastfree(fastmatch_t *preg)
{
  tre_free_fast(preg);
}

/* XXX: avoid duplication */
#define ADJUST_OFFSETS							\
  {									\
    size_t slen = (size_t)(pmatch[0].rm_eo - pmatch[0].rm_so);		\
    size_t offset = pmatch[0].rm_so;					\
    int ret;								\
									\
    if ((len != (unsigned)-1) && (pmatch[0].rm_eo > len))		\
      return REG_NOMATCH;						\
    if ((long long)pmatch[0].rm_eo - pmatch[0].rm_so < 0)		\
      return REG_NOMATCH;						\
    ret = tre_match_fast(preg, &string[offset], slen, type, nmatch,	\
			 pmatch, eflags);				\
    for (unsigned i = 0; (i == 0) || (!(eflags & REG_NOSUB) &&		\
         (i < nmatch)); i++)						\
      {									\
        pmatch[i].rm_so += offset;					\
        pmatch[i].rm_eo += offset;					\
      }									\
    return ret;								\
  }

int
tre_fastnexec(const fastmatch_t *preg, const char *string, size_t len,
         size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = (TRE_MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;

  if (eflags & REG_STARTEND)
    ADJUST_OFFSETS
  else
    return tre_match_fast(preg, string, len, type, nmatch,
      pmatch, eflags);
}

int
tre_fastexec(const fastmatch_t *preg, const char *string, size_t nmatch,
	     regmatch_t pmatch[], int eflags)
{
  return tre_fastnexec(preg, string, (size_t)-1, nmatch, pmatch, eflags);
}

int
tre_fastwnexec(const fastmatch_t *preg, const wchar_t *string, size_t len,
          size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = STR_WIDE;

  if (eflags & REG_STARTEND)
    ADJUST_OFFSETS
  else
    return tre_match_fast(preg, string, len, type, nmatch,
      pmatch, eflags);
}

int
tre_fastwexec(const fastmatch_t *preg, const wchar_t *string,
         size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_fastwnexec(preg, string, (size_t)-1, nmatch, pmatch, eflags);
}

