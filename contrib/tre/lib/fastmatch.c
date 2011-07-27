/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2011 Gabor Kovesdan <gabor@FreeBSD.org>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "fastmatch.h"
#include "hashtable.h"
#include "tre.h"
#include "tre-internal.h"
#include "xmalloc.h"

static int	fastcmp(const void *, const void *, size_t,
			tre_str_type_t, bool);
static void	revstr(tre_char_t *, int);
static void	revs(char *str, int len);

#ifdef TRE_WCHAR
#define TRE_CHAR(n)	L##n
#else
#define TRE_CHAR(n)	n
#endif

#define SKIP_CHARS(n)						\
  do {								\
    switch (type)						\
      {								\
	case STR_BYTE:						\
	case STR_MBS:						\
	  startptr = str_byte + n;				\
	  break;						\
	case STR_WIDE:						\
	  startptr = str_wide + n;				\
	  break;						\
	default:						\
	  /* XXX */						\
	  break;						\
      }								\
  } while (0);							\

#define STORE_MBS_PAT						\
  do {								\
    size_t siz;							\
								\
    siz = wcstombs(NULL, fg->wpattern, 0);			\
    if (siz == (size_t)-1)					\
      return REG_BADPAT;					\
    fg->len = siz;						\
    fg->pattern = xmalloc(siz + 1);				\
    if (fg->pattern == NULL)					\
      return REG_ESPACE;					\
    wcstombs(fg->pattern, fg->wpattern, siz);			\
    fg->pattern[siz] = '\0';					\
  } while (0);							\

#define COMPARE								\
  do {									\
    switch (type)							\
      {									\
	case STR_BYTE:							\
	case STR_MBS:							\
	  mismatch = fastcmp(fg->pattern, startptr, fg->len, type,	\
			     fg->icase);				\
	  break;							\
	case STR_WIDE:							\
	  mismatch = fastcmp(fg->wpattern, startptr, fg->wlen, type,	\
			     fg->icase);				\
	default:							\
	  break;							\
      }									\
  } while (0);

#ifdef TRE_WCHAR
#define IS_OUT_OF_BOUNDS						\
  ((type == STR_WIDE) ? ((j + fg->wlen) > len) : ((j + fg->len) > len))
#else
#define IS_OUT_OF_BOUNDS	((j + fg->len) > len)
#endif

#define CHECKBOUNDS							\
  if (IS_OUT_OF_BOUNDS)							\
    break;								\

#ifdef TRE_WCHAR
#define SHIFT								\
      CHECKBOUNDS;							\
      {									\
        int k = 0, r = -1;						\
									\
        switch (type)							\
          {								\
            case STR_BYTE:						\
	    case STR_MBS:						\
	      k = fg->qsBc[(unsigned char)((char *)startptr)		\
		[mismatch + 1]];					\
              break;							\
            case STR_WIDE:						\
              r = hashtable_get(fg->qsBc_table,				\
		&((wchar_t *)startptr)[mismatch + 1], &k);		\
	      k = (r == 0) ? k : fg->defBc;				\
              break;							\
            default:							\
              /* XXX */							\
              break;							\
          }								\
        j += k;								\
      }
#else
#define SHIFT								\
      CHECKBOUNDS;							\
      j += fg->qsBc[(unsigned char)((char *)startptr)[mismatch + 1]];
#endif

/*
 * Normal Quick Search would require a shift based on the position the
 * next character after the comparison is within the pattern.  With
 * wildcards, the position of the last dot effects the maximum shift
 * distance.
 * The closer to the end the wild card is the slower the search.  A
 * reverse version of this algorithm would be useful for wildcards near
 * the end of the string.
 *
 * Examples:
 * Pattern    Max shift
 * -------    ---------
 * this               5
 * .his               4
 * t.is               3
 * th.s               2
 * thi.               1
 */

#ifdef TRE_WCHAR
#define FILL_QSBC							\
  /* Adjust the shift based on location of the last dot ('.'). */	\
  fg->defBc = fg->wlen - hasDot;					\
									\
  /* Preprocess pattern. */						\
  fg->qsBc_table = hashtable_init(fg->wlen, sizeof(tre_char_t),		\
    sizeof(int));							\
  for (unsigned int i = hasDot + 1; i < fg->wlen; i++)			\
  {									\
    int k = fg->wlen - i;						\
    hashtable_put(fg->qsBc_table, &fg->wpattern[i], &k);		\
    if (fg->icase)							\
      {									\
	wint_t wc = iswlower(fg->wpattern[i]) ?				\
	  towupper(fg->wpattern[i]) : towlower(fg->wpattern[i]);	\
	hashtable_put(fg->qsBc_table, &wc, &k);				\
      }									\
  }									\
									\
  for (unsigned int i = 0; i <= UCHAR_MAX; i++)				\
    fg->qsBc[i] = fg->len - hasDot;					\
  for (int i = hasDot + 1; i < fg->len; i++)				\
    {									\
      fg->qsBc[(unsigned)fg->pattern[i]] = fg->len - i;			\
      if (fg->icase)							\
	{								\
	  char c = islower(fg->pattern[i]) ? toupper(fg->pattern[i])	\
	    : tolower(fg->pattern[i]);					\
	  fg->qsBc[(unsigned)c] = fg->len - i;				\
	}								\
    }
#else
#define FILL_QSBC							\
  for (unsigned int i = 0; i <= UCHAR_MAX; i++)				\
    fg->qsBc[i] = fg->wlen - hasDot;					\
  for (int i = hasDot + 1; i < fg->wlen; i++)				\
    {									\
      fg->qsBc[(unsigned)fg->wpattern[i]] = fg->wlen - i;		\
      if (fg->icase)							\
	{								\
	  char c = islower(fg->wpattern[i]) ? toupper(fg->wpattern[i])	\
	    : tolower(fg->wpattern[i]);					\
	  fg->qsBc[(unsigned)c] = fg->len - i;				\
	}								\
    }
#endif

#define REVFUNC(name, argtype)						\
static inline void							\
name(argtype *str, int len)						\
{									\
  argtype c;								\
									\
  for (int i = 0; i < len / 2; i++)					\
  {									\
    c = str[i];								\
    str[i] = str[len - i - 1];						\
    str[len - i - 1] = c;						\
  }									\
}

REVFUNC(revstr, tre_char_t)
REVFUNC(revs, char)


/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_fastcomp_literal(fastmatch_t *fg, const tre_char_t *wpat, size_t n,
		     int cflags)
{
  int hasDot = 0;

  /* Initialize. */
  memset(fg, 0, sizeof(*fg));
  fg->icase = (cflags & REG_ICASE);
  /* XXX */
  if (fg->icase && (MB_CUR_MAX > 1))
    return REG_BADPAT;

  fg->wlen = (n == 0) ? tre_strlen(wpat) : n;
  fg->wpattern = xmalloc((fg->wlen + 1) * sizeof(tre_char_t));
  if (fg->wpattern == NULL)
    return REG_ESPACE;
  memcpy(fg->wpattern, wpat, fg->wlen * sizeof(tre_char_t));
  fg->wpattern[fg->wlen] = TRE_CHAR('\0');
#ifdef TRE_WCHAR
  STORE_MBS_PAT;
#endif

  FILL_QSBC;

  return REG_OK;
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_fastcomp(fastmatch_t *fg, const tre_char_t *wpat, size_t n,
	     int cflags)
{
  int firstHalfDot = -1;
  int firstLastHalfDot = -1;
  int hasDot = 0;
  int lastHalfDot = 0;

  /* Initialize. */
  memset(fg, 0, sizeof(*fg));
  fg->icase = (cflags & REG_ICASE);
  /* XXX */
  if (fg->icase && (MB_CUR_MAX > 1))
    return REG_BADPAT;

  fg->wlen = (n == 0) ? tre_strlen(wpat) : n;

  /* Remove end-of-line character ('$'). */
  if ((fg->wlen > 0) && (wpat[fg->wlen - 1] == TRE_CHAR('$')))
  {
    fg->eol = true;
    fg->wlen--;
  }

  /* Remove beginning-of-line character ('^'). */
  if (wpat[0] == TRE_CHAR('^'))
  {
    fg->bol = true;
    fg->wlen--;
    wpat++;
  }

  if ((fg->wlen >= 14) &&
      (memcmp(wpat, TRE_CHAR("[[:<:]]"), 7 * sizeof(tre_char_t)) == 0) &&
      (memcmp(wpat + fg->wlen - 7, TRE_CHAR("[[:>:]]"),
	      7 * sizeof(tre_char_t)) == 0))
  {
    fg->wlen -= 14;
    wpat += 7;
    fg->word = true;
  }

  /*
   * wpat has been adjusted earlier to not include '^', '$' or
   * the word match character classes at the beginning and ending
   * of the string respectively.
   */
  fg->wpattern = xmalloc((fg->wlen + 1) * sizeof(tre_char_t));
  if (fg->wpattern == NULL)
    return REG_ESPACE;
  memcpy(fg->wpattern, wpat, fg->wlen * sizeof(tre_char_t));
  fg->wpattern[fg->wlen] = TRE_CHAR('\0');

  /* Look for ways to cheat...er...avoid the full regex engine. */
  for (unsigned int i = 0; i < fg->wlen; i++) {
    /* Can still cheat? */
    if ((tre_isalnum(fg->wpattern[i])) || tre_isspace(fg->wpattern[i]) ||
      (fg->wpattern[i] == TRE_CHAR('_')) || (fg->wpattern[i] == TRE_CHAR(',')) ||
      (fg->wpattern[i] == TRE_CHAR('=')) || (fg->wpattern[i] == TRE_CHAR('-')) ||
      (fg->wpattern[i] == TRE_CHAR(':')) || (fg->wpattern[i] == TRE_CHAR('/'))) {
	continue;
    } else if (fg->wpattern[i] == TRE_CHAR('.')) {
      hasDot = i;
      if (i < fg->wlen / 2) {
	if (firstHalfDot < 0)
	  /* Closest dot to the beginning */
	  firstHalfDot = i;
      } else {
	  /* Closest dot to the end of the pattern. */
	  lastHalfDot = i;
	  if (firstLastHalfDot < 0)
	    firstLastHalfDot = i;
      }
    } else {
	/* Free memory and let others know this is empty. */
	free(fg->wpattern);
	fg->wpattern = NULL;
	return REG_BADPAT;
    }
  }

#ifdef TRE_WCHAR
  STORE_MBS_PAT;
#endif

  /*
   * Determine if a reverse search would be faster based on the placement
   * of the dots.
   */
  if ((!(fg->bol || fg->eol)) &&
     (lastHalfDot && ((firstHalfDot < 0) ||
     ((fg->wlen - (lastHalfDot + 1)) < (size_t)firstHalfDot)))) {
    fg->reversed = true;
    hasDot = fg->wlen - (firstHalfDot < 0 ?
	     firstLastHalfDot : firstHalfDot) - 1;
    revstr(fg->wpattern, fg->wlen);
#ifdef TRE_WCHAR
    revs(fg->pattern, fg->len);
#endif
  }

  FILL_QSBC;

  /*
   * Put pattern back to normal after pre-processing to allow for easy
   * comparisons later.
   */
  if (fg->reversed)
    {
      revstr(fg->wpattern, fg->wlen);
#ifdef TRE_WCHAR
      revs(fg->pattern, fg->len);
#endif
    }

  return REG_OK;
}

int
tre_fastexec(const fastmatch_t *fg, const void *data, size_t len,
    tre_str_type_t type, int nmatch, regmatch_t pmatch[])
{
  unsigned int j;
  int ret = REG_NOMATCH;
  int mismatch;
  const char *str_byte = data;
  const void *startptr = NULL;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = data;
#endif

  if (len == (unsigned)-1)
    {
      switch (type)
	{
	  case STR_BYTE:
	  case STR_MBS:
	    len = strlen(str_byte);
	    break;
	  case STR_WIDE:
	    len = wcslen(str_wide);
	    break;
	  default:
	    /* XXX */
	    break;
	}
    }

  /* No point in going farther if we do not have enough data. */
  if (len < fg->len)
    return ret;

  /* Only try once at the beginning or ending of the line. */
  if (fg->bol || fg->eol) {
    /* Simple text comparison. */
    if (!((fg->bol && fg->eol) && (len != fg->len))) {
      /* Determine where in data to start search at. */
      j = fg->eol ? len - fg->len : 0;
      SKIP_CHARS(j);
      COMPARE;
      if (mismatch == REG_OK) {
	pmatch[0].rm_so = j;
	pmatch[0].rm_eo = j + fg->len;
	return REG_OK;
      }
    }
  } else if (fg->reversed) {
    /* Quick Search algorithm. */
    j = len - fg->len;
    do {
      SKIP_CHARS(j);
      COMPARE;
      if (mismatch == REG_OK) {
	pmatch[0].rm_so = j - fg->len;
	pmatch[0].rm_eo = j;
	return REG_OK;
      } else if (mismatch > 0)
	return mismatch;
      mismatch = -mismatch - 1;
      SHIFT;
    } while (!IS_OUT_OF_BOUNDS);
  } else {
    /* Quick Search algorithm. */
    j = 0;
    do {
      SKIP_CHARS(j);
      COMPARE;
      if (mismatch == REG_OK) {
	pmatch[0].rm_so = j;
	pmatch[0].rm_eo = j + fg->len;
	return REG_OK;
      } else if (mismatch > 0)
        return mismatch;
      mismatch = -mismatch - 1;
      SHIFT;
    } while (!IS_OUT_OF_BOUNDS);
  }
  return ret;
}

void
tre_fastfree(fastmatch_t *fg)
{

#ifdef TRE_WCHAR
  hashtable_free(fg->qsBc_table);
  free(fg->pattern);
#endif
  free(fg->wpattern);
}

/*
 * Returns:	-(i + 1) on failure (position that it failed with minus sign)
 *		error code on error
 *		REG_OK on success
 */
static inline int
fastcmp(const void *pat, const void *data, size_t len,
	tre_str_type_t type, bool icase)
{
  const char *str_byte = data;
  const char *pat_byte = pat;
  int ret = REG_OK;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = data;
  const wchar_t *pat_wide = pat;
#endif

  for (int i = len - 1; i >= 0; i--) {
    if (pat_wide[i] == TRE_CHAR('.'))
      continue;
    switch (type)
      {
	case STR_BYTE:
	case STR_MBS:
	  if (icase ? (tolower(pat_byte[i]) == tolower(str_byte[i]))
	      : (pat_byte[i] == str_byte[i]))
	    continue;
	  break;
	case STR_WIDE:
	  if (icase ? (towlower(pat_wide[i]) == towlower(str_wide[i]))
	      : (pat_wide[i] == str_wide[i]))
	    continue;
	  break;
	default:
	  /* XXX */
	  break;
      }
    ret = -(i + 1);
    break;
  }

  return ret;
}
