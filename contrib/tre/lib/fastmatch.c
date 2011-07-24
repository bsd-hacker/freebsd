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

static int	fastcmp(const tre_char_t *, const void *, size_t,
			tre_str_type_t);
static void	revstr(tre_char_t *, int);

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
	  startptr = str_byte + n;				\
	  break;						\
	case STR_MBS:						\
	  for (skip = j = 0; j < n; j++)			\
	    {							\
	      siz = mbrlen(str_byte + skip, MB_CUR_MAX, NULL);	\
	      skip += siz;					\
	    }							\
	  startptr = str_byte + skip;				\
	  break;						\
	case STR_WIDE:						\
	  startptr = str_wide + n;				\
	  break;						\
	default:						\
	  /* XXX */						\
	  break;						\
      }								\
  } while (0);							\

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_fastcomp_literal(fastmatch_t *fg, const tre_char_t *pat, size_t n)
{

  /* Initialize. */
  memset(fg, 0, sizeof(*fg));
  fg->len = (n == 0) ? tre_strlen(pat) : n;
  fg->pattern = xmalloc((fg->len + 1) * sizeof(tre_char_t));
  if (fg->pattern == NULL)
    return REG_ESPACE;
  memcpy(fg->pattern, pat, fg->len * sizeof(tre_char_t));
  fg->pattern[fg->len] = TRE_CHAR('\0');

  /* Preprocess pattern. */
#ifdef TRE_WCHAR
  fg->defBc = fg->len;
  fg->qsBc = hashtable_init(fg->len * 3, sizeof(tre_char_t), sizeof(int));
  if (fg->qsBc == NULL)
    return REG_ESPACE;
  for (unsigned int i = 1; i < fg->len; i++)
  {
    int k = fg->len - i;
    hashtable_put(fg->qsBc, &fg->pattern[i], &k);
  }
#else
  for (i = 0; i <= UCHAR_MAX; i++)
    fg->qsBc[i] = fg->len;
  for (i = 1; i < fg->len; i++)
    fg->qsBc[fg->pattern[i]] = fg->len - i;
#endif

  return REG_OK;
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_fastcomp(fastmatch_t *fg, const tre_char_t *pat, size_t n)
{
  int firstHalfDot = -1;
  int firstLastHalfDot = -1;
  int hasDot = 0;
  int lastHalfDot = 0;

  /* Initialize. */
  memset(fg, 0, sizeof(*fg));
  fg->len = (n == 0) ? tre_strlen(pat) : n;

  /* Remove end-of-line character ('$'). */
  if ((fg->len > 0) && (pat[fg->len - 1] == TRE_CHAR('$')))
  {
    fg->eol = true;
    fg->len--;
  }

  /* Remove beginning-of-line character ('^'). */
  if (pat[0] == TRE_CHAR('^'))
  {
    fg->bol = true;
    fg->len--;
    pat++;
  }

  if ((fg->len >= 14) &&
      (memcmp(pat, TRE_CHAR("[[:<:]]"), 7 * sizeof(tre_char_t)) == 0) &&
      (memcmp(pat + fg->len - 7, TRE_CHAR("[[:>:]]"),
	      7 * sizeof(tre_char_t)) == 0))
  {
    fg->len -= 14;
    pat += 7;
    fg->word = true;
  }

  /*
   * pat has been adjusted earlier to not include '^', '$' or
   * the word match character classes at the beginning and ending
   * of the string respectively.
   */
  fg->pattern = xmalloc((fg->len + 1) * sizeof(tre_char_t));
  if (fg->pattern == NULL)
    return REG_ESPACE;
  memcpy(fg->pattern, pat, fg->len * sizeof(tre_char_t));
  fg->pattern[fg->len] = TRE_CHAR('\0');

  /* Look for ways to cheat...er...avoid the full regex engine. */
  for (unsigned int i = 0; i < fg->len; i++) {
    /* Can still cheat? */
    if ((tre_isalnum(fg->pattern[i])) || tre_isspace(fg->pattern[i]) ||
      (fg->pattern[i] == TRE_CHAR('_')) || (fg->pattern[i] == TRE_CHAR(',')) ||
      (fg->pattern[i] == TRE_CHAR('=')) || (fg->pattern[i] == TRE_CHAR('-')) ||
      (fg->pattern[i] == TRE_CHAR(':')) || (fg->pattern[i] == TRE_CHAR('/'))) {
	continue;
    } else if (fg->pattern[i] == TRE_CHAR('.')) {
      hasDot = i;
      if (i < fg->len / 2) {
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
	free(fg->pattern);
	fg->pattern = NULL;
	return REG_BADPAT;
    }
  }

  /*
   * Determine if a reverse search would be faster based on the placement
   * of the dots.
   */
  if ((!(fg->bol || fg->eol)) &&
     (lastHalfDot && ((firstHalfDot < 0) ||
     ((fg->len - (lastHalfDot + 1)) < (size_t)firstHalfDot)))) {
    fg->reversed = true;
    hasDot = fg->len - (firstHalfDot < 0 ?
	     firstLastHalfDot : firstHalfDot) - 1;
    revstr(fg->pattern, fg->len);
  }

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
   * Pattern	Max shift
   * -------	---------
   * this		5
   * .his		4
   * t.is		3
   * th.s		2
   * thi.		1
   */

#ifdef TRE_WCHAR
  /* Adjust the shift based on location of the last dot ('.'). */
  fg->defBc = fg->len - hasDot;

  /* Preprocess pattern. */
  fg->qsBc = hashtable_init(fg->len, sizeof(tre_char_t), sizeof(int));
  for (unsigned int i = hasDot + 1; i < fg->len; i++)
  {
    int k = fg->len - i;
    hashtable_put(fg->qsBc, &fg->pattern[i], &k);
  }
#else
  /* Preprocess pattern. */
  for (unsigned int i = 0; i <= (signed)UCHAR_MAX; i++)
    fg->qsBc[i] = fg->len - hasDot;
  for (unsigned int i = hasDot + 1; i < fg->len; i++) {
    fg->qsBc[fg->pattern[i]] = fg->len - i;
  }
#endif

  /*
   * Put pattern back to normal after pre-processing to allow for easy
   * comparisons later.
   */
  if (fg->reversed)
    revstr(fg->pattern, fg->len);

  return REG_OK;
}

int
tre_fastexec(const fastmatch_t *fg, const void *data, size_t len,
    tre_str_type_t type, int nmatch, regmatch_t pmatch[])
{
  unsigned int j;
  size_t siz, skip;
  int ret = REG_NOMATCH;
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
      if (fastcmp(fg->pattern, startptr, fg->len, type) == REG_OK) {
	pmatch[0].rm_so = j;
	pmatch[0].rm_eo = j + fg->len;
	return REG_OK;
      }
    }
  } else if (fg->reversed) {
    /* Quick Search algorithm. */
    j = len - fg->len;
    do {
      int mismatch;

      SKIP_CHARS(j);
      mismatch = fastcmp(fg->pattern, startptr, fg->len, type);
      if (mismatch == REG_OK) {
	pmatch[0].rm_so = j - fg->len;
	pmatch[0].rm_eo = j;
	return REG_OK;
      } else if (mismatch > 0)
	return mismatch;
      mismatch = -mismatch - 1;

      /* Shift if within bounds, otherwise, we are done. */
      if (((long)len - (long)j) > fg->len)
        break;
#ifdef TRE_WCHAR
      {
	int k, r = -1;
	wint_t wc;

	switch (type)
	  {
	    case STR_BYTE:
	      wc = btowc(((char *)startptr)[mismatch]);
	      r = hashtable_get(fg->qsBc, &wc, &k);
	      break;
	    case STR_MBS:
	      tre_mbrtowc(&wc, &((char *)startptr)[mismatch], MB_CUR_MAX, NULL);
	      r = hashtable_get(fg->qsBc, &wc, &k);
	      break;
	    case STR_WIDE:
	      r = hashtable_get(fg->qsBc, &((char *)startptr)[mismatch], &k);
	      break;
	    default:
	      /* XXX */
	      break;
	  }
	k = (r == 0) ? k : fg->defBc;
	j -= k;
      }
#else
      j -= fg->qsBc[((char *)startptr)[mismatch]];
#endif
    } while (j >= fg->len);
  } else {
    /* Quick Search algorithm. */
    j = 0;
    do {
      int mismatch;

      SKIP_CHARS(j);
      mismatch = fastcmp(fg->pattern, startptr, fg->len, type);
      if (mismatch == REG_OK) {
	pmatch[0].rm_so = j;
	pmatch[0].rm_eo = j + fg->len;
	return REG_OK;
      } else if (mismatch > 0)
        return mismatch;
      mismatch = -mismatch - 1;

      /* Shift if within bounds, otherwise, we are done. */
      if ((j + fg->len) >= len)
	break;
#ifdef TRE_WCHAR
      {
	int k, r = -1;
	wint_t wc;

	switch (type)
	  {
	    case STR_BYTE:
	      wc = btowc(((char *)startptr)[mismatch + 1]);
	      r = hashtable_get(fg->qsBc, &wc, &k);
	      break;
	    case STR_MBS:
	      tre_mbrtowc(&wc, &((char *)startptr)[mismatch + 1], MB_CUR_MAX, NULL);
	      r = hashtable_get(fg->qsBc, &wc, &k);
	      break;
	    case STR_WIDE:
	      r = hashtable_get(fg->qsBc, &((char *)startptr)[mismatch + 1], &k);
	      break;
	    default:
	      /* XXX */
	      break;
	  }
	k = (r == 0) ? k : fg->defBc;
	j += k;
      }
#else
      j += fg->qsBc[((char *)startptr)[mismatch]];
#endif
    } while (j <= (len - fg->len));
  }
  return ret;
}

void
tre_fastfree(fastmatch_t *fg)
{

#ifdef TRE_WCHAR
  hashtable_free(fg->qsBc);
#endif
  free(fg->pattern);
}

/*
 * Returns:	-(i + 1) on failure (position that it failed with minus sign)
 *		error code on error
 *		REG_OK on success
 */
static inline int
fastcmp(const tre_char_t *pat, const void *data, size_t len,
	tre_str_type_t type)
{
  const char *str_byte = data;
  wchar_t *mbs_wide;
  int ret = REG_OK;
#ifdef TRE_WCHAR
  const wchar_t *str_wide = data;
#endif

  if (type == STR_MBS)
    {
#ifdef HAVE_ALLOCA
      mbs_wide = alloca((len + 1) * sizeof(wint_t));
#elif
      mbs_wide = xmalloc((len + 1) * sizeof(wint_t));
      /* XXX */
      if (mbs_wide == NULL)
	return REG_ESPACE;
#endif
      mbstowcs(mbs_wide, str_byte, len);
      type = STR_WIDE;
    }

  for (int i = len - 1; i >= 0; i--) {
    if (pat[i] == TRE_CHAR('.'))
      continue;
    switch (type)
      {
	case STR_BYTE:
	  if (pat[i] == btowc(str_byte[i]))
	    continue;
	  break;
	case STR_WIDE:
	  if (pat[i] == str_wide[i])
	    continue;
	  break;
	default:
	  /* XXX */
	  break;
      }
    ret = -(i + 1);
    break;
  }
#ifndef HAVE_ALLOCA
    if (mbs_wide != NULL)
      free(mbs_wide);
#endif

  return ret;
}

static inline void
revstr(tre_char_t *str, int len)
{
  tre_char_t c;

  for (int i = 0; i < len / 2; i++)
  {
    c = str[i];
    str[i] = str[len - i - 1];
    str[len - i - 1] = c;
  }
}
