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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <regex.h>
#include <stdbool.h>
#include <string.h>
#ifdef TRE_WCHAR
#include <wctype.h>
#endif

#include "tre-fastmatch.h"
#include "tre-heuristic.h"
#include "tre-internal.h"
#include "xmalloc.h"

/*
 * A full regex implementation requires a finite state automaton
 * and using an automaton is always about a trade-off. A DFA is
 * fast but complex and requires more memory because of the
 * high number of states. NFA is slower but simpler and uses less
 * memory. Regular expression matching is an underlying common task
 * that is required to be efficient but correctness, clean and
 * maintanable code are also requirements. So what we do is using
 * an NFA implementation and heuristically locate the possible matches
 * with a cheaper algorithm and only apply the heavy one to the
 * possibly matching segments. This allows us to benefit from the
 * advantages of an NFA implementation reducing the effect of the
 * performance impact.
 */

/*
 * Parses bracket expression seeking to the end of the enclosed text.
 * The parameters are the opening (oe) and closing elements (ce).
 * Can handle nested bracket expressions.
 */
#define PARSE_UNIT(oe, ce)						\
  {									\
    int level = 0;							\
									\
    while (i < len)							\
      {									\
	if (regex[i] == TRE_CHAR(oe))					\
	  level++;							\
	else if (regex[i] == TRE_CHAR(ce))				\
	  level--;							\
	if (level == 0)							\
	  break;							\
	i++;								\
      }									\
  }

#define PARSE_BRACKETS							\
  {									\
    i++;								\
    if (regex[i] == TRE_CHAR('^'))					\
      i++;								\
    if (regex[i] == TRE_CHAR(']'))					\
      i++;								\
									\
    for (; i < len; i++)						\
      {									\
	if (regex[i] == TRE_CHAR('['))					\
	  return REG_BADPAT;						\
	if (regex[i] == TRE_CHAR(']'))					\
	  break;							\
      }									\
  }

/*
 * Finishes a segment (fixed-length text fragment).
 */
#define END_SEGMENT(varlen)						\
  do									\
    {									\
      if (varlen)							\
	tlen = -1;							\
      st = i + 1;							\
      escaped = false;							\
      goto end_segment;							\
    } while (0)

#define STORE_CHAR							\
  do									\
    {									\
      heur[pos++] = regex[i];						\
      escaped = false;							\
      tlen = (tlen == -1) ? -1 : tlen + 1;				\
      continue;								\
    } while (0)

#define DEC_POS pos = (pos == 0) ? 0 : pos - 1;

/*
 * Parses a regular expression and constructs a heuristic in heur_t and
 * returns REG_OK if successful or the corresponding error code if a
 * heuristic cannot be constructed.
 */
int
tre_proc_heur(heur_t *h, const tre_char_t *regex, size_t len, int cflags)
{
  tre_char_t **arr, *heur;
  tre_char_t **farr;
  char **barr;
  size_t *bsiz, *fsiz;
  size_t length[MAX_FRAGMENTS];
  ssize_t tlen = 0;
  int errcode, j = 0, pos = 0, st = 0;
  bool escaped = false;

  heur = xmalloc(len * sizeof(tre_char_t));
  if (!heur)
    return REG_ESPACE;

  arr = xmalloc(MAX_FRAGMENTS * sizeof(tre_char_t *));
  if (!arr)
    {
      errcode = REG_ESPACE;
      goto err;
    }

  h->type = HEUR_ARRAY;

  while (true)
    {

      /*
       * Process the pattern char-by-char.
       *
       * i: position in regex
       * j: number of fragment
       * st: start offset of current segment (fixed-length fragment)
       *     to be processed
       * pos: current position (and length) in the temporary space where
       *      we copy the current segment
       */
      for (int i = st; i < len; i++)
        {
	  switch (regex[i])
	    {

	      /*
	       * Bracketed expression ends the segment or the
	       * brackets are treated as normal if at least the opening
	       * bracket is escaped.
	       */
	      case TRE_CHAR('['):
		if (escaped)
		  STORE_CHAR;
		else
		  {
		    PARSE_BRACKETS;
		    END_SEGMENT(true);
		  }
		continue;

	      /*
	       * If a repetition marker, erases the repeting character
	       * and terminates the segment, otherwise treated as a normal
	       * character.
	       */
	      case TRE_CHAR('{'):
		if (escaped && (i == 1))
		  STORE_CHAR;
		else if (i == 0)
		  STORE_CHAR;

		PARSE_UNIT('{', '}');
		if (escaped ^ (cflags & REG_EXTENDED))
		  {
		    DEC_POS;
		    END_SEGMENT(true);
		  }
		else
		  STORE_CHAR;
		continue;

	      /*
	       * Terminates the current segment when escaped,
	       * otherwise treated as a normal character.
	       */
	      case TRE_CHAR('('):
		if (escaped ^ (cflags & REG_EXTENDED))
		  {
		    PARSE_UNIT('(', ')');
		    END_SEGMENT(true);
		  }
		else
		  STORE_CHAR;
		continue;

	      /*
	       * Sets escaped flag.
	       * Escaped escape is treated as a normal character.
	       * (This is also the GNU behaviour.)
	       */
	      case TRE_CHAR('\\'):
		if (escaped)
		  STORE_CHAR;
		else
		  escaped = true;
		continue;

	      /*
	       * BRE: If not the first character and not escaped, erases the
	       * last character and terminates the segment.
	       * Otherwise treated as a normal character.
	       * ERE: Skipped if first character (GNU), rest is like in BRE.
	       */
	      case TRE_CHAR('*'):
		if (escaped || (!(cflags & REG_EXTENDED) && (i == 0)))
		  STORE_CHAR;
		else if ((i != 0))
		  {
		    DEC_POS;
		    END_SEGMENT(true);
		  }
		continue;

	      /*
	       * In BRE, it is a normal character, behavior is undefined
	       * when escaped.
	       * In ERE, it is special unless escaped. Terminate segment
	       * when not escaped. Last character is not removed because it
	       * must occur at least once. It is skipped when first
	       * character (GNU).
	       */
	      case TRE_CHAR('+'):
		if ((cflags & REG_EXTENDED) && (i == 0))
		  continue;
		else if ((cflags & REG_EXTENDED) ^ escaped)
		  END_SEGMENT(true);
		else
		  STORE_CHAR;
		continue;

	      /*
	       * In BRE, it is a normal character, behavior is undefined
	       * when escaped.
	       * In ERE, it is special unless escaped. Terminate segment
	       * when not escaped. Last character is removed. Skipped when
	       * first character (GNU).
	       */
	      case TRE_CHAR('?'):
		if ((cflags & REG_EXTENDED) && (i == 0))
		  continue;
		if ((cflags & REG_EXTENDED) ^ escaped)
		  {
		    DEC_POS;
		    END_SEGMENT(true);
		  }
		else
		  STORE_CHAR;
		continue;

	      /*
	       * Fail if it is an ERE alternation marker.
	       */
	      case TRE_CHAR('|'):
		if ((cflags & REG_EXTENDED) && !escaped)
		  {
		    errcode = REG_BADPAT;
		    goto err;
		  }
		else if (!(cflags & REG_EXTENDED) && escaped)
		  {
		    errcode = REG_BADPAT;
		    goto err;
		  }
		else
		  STORE_CHAR;
		continue;

	      case TRE_CHAR('.'):
		if (escaped)
		  STORE_CHAR;
		else
		  {
		    tlen = (tlen == -1) ? -1 : tlen + 1;
		    END_SEGMENT(false);
		  }
		continue;

	      /*
	       * If escaped, terminates segment.
	       * Otherwise adds current character to the current segment
	       * by copying it to the temporary space.
	       */
	      default:
		if (escaped)
		  END_SEGMENT(true);
		else
		  STORE_CHAR;
		continue;
	    }
	}

      /* We are done with the pattern if we got here. */
      st = len;
 
end_segment:
      /* Check if pattern is open-ended */
      if (st == len && pos == 0)
	{
	  if (j == 0)
	    {
	      errcode = REG_BADPAT;
	      goto err;
	    }
	  h->type = HEUR_PREFIX_ARRAY;
	  goto ok;
	}
      /* Continue if we got some variable-length part */
      else if (pos == 0)
	continue;

      /* Too many fragments - should never happen but to be safe */
      if (j == MAX_FRAGMENTS)
	{
	  errcode = REG_BADPAT;
	  goto err;
	}

      /* Alloc space for fragment and copy it */
      arr[j] = xmalloc((pos + 1) * sizeof(tre_char_t));
      if (!arr[j])
	{
	  errcode = REG_ESPACE;
	  goto err;
	}
      heur[pos] = TRE_CHAR('\0');
      memcpy(arr[j], heur, (pos + 1) * sizeof(tre_char_t));
      length[j] = pos;
      j++;
      pos = 0;

      /* Check whether there is more input */
      if (st == len)
	goto ok;
    }

ok:
  {
    size_t m = 0;
    int i, ret;

    h->tlen = tlen;

    /* Look up maximum length fragment */
    for (i = 1; i < j; i++)
      m = (length[i] > length[m]) ? i : m;

    /* Will hold the final fragments that we actually use */
    farr = xmalloc(4 * sizeof(tre_char_t *));
    if (!farr)
      {
	errcode = REG_ESPACE;
	goto err;
      }

    /* Sizes for the final fragments */
    fsiz = xmalloc(4 * sizeof(size_t));
    if (!fsiz)
      {
        errcode = REG_ESPACE;
        goto err;
      }

    /*
     * Only save the longest fragment if match is line-based.
     */
    if (cflags & REG_NEWLINE)
      {
	farr[0] = arr[m];
	arr[m] = NULL;
	fsiz[0] = length[0];
	farr[1] = NULL;
      }
    /*
     * Otherwise try to save up to three fragments: beginning, longest
     * intermediate pattern, ending.  If either the beginning or the
     * ending fragment is longer than any intermediate fragments, we will
     * not save any intermediate one.  The point here is to always maximize
     * the possible shifting when searching in the input.  Measurements
     * have shown that the eager approach works best.
     */
    else
      {
	size_t idx = 0;

	/* Always start by saving the beginning */
	farr[idx] = arr[0];
	arr[0] = NULL;
	fsiz[idx++] = length[0];

	/*
	 * If the longest pattern is not the beginning nor the ending,
	 * save it.
	 */
	if ((m != 1) && (m != j - 1))
	  {
	    farr[idx] = arr[m];
	    fsiz[idx++] = length[m];
	    arr[m] = NULL;
	  }

	/*
	 * If we have an ending pattern (even if the pattern is
	 * "open-ended"), save it.
	 */
	if (j > 1)
	  {
	    farr[idx] = arr[j - 1];
	    fsiz[idx++] = length[j - 1];
	    arr[j - 1] = NULL;
	  }

	farr[idx] = NULL;
      }

    /* Once necessary pattern saved, free original array */
    for (i = 0; i < j; i++)
      if (arr[i])
	xfree(arr[i]);
    xfree(arr);

/*
 * Store the array in single-byte and wide char forms in the
 * heur_t struct for later reuse.  When supporting whcar_t
 * convert the fragments to single-byte string because
 * conversion is probably faster than processing the patterns
 * again in single-byte form.
 */
#ifdef TRE_WCHAR
    barr = xmalloc(4 * sizeof(char *));
    if (!barr)
      {
	errcode = REG_ESPACE;
	goto err;
      }

    bsiz = xmalloc(4 * sizeof(size_t));
      if (!bsiz)
	{
	  errcode = REG_ESPACE;
	  goto err;
	}

    for (i = 0; farr[i] != NULL; i++)
      {
	bsiz[i] = wcstombs(NULL, farr[i], 0);
	barr[i] = xmalloc(bsiz[i] + 1);
	if (!barr[i])
	  {
	    errcode = REG_ESPACE;
	    goto err;
	  }
	wcstombs(barr[i], farr[i], bsiz[i]);
	barr[i][bsiz[i]] = '\0';
      }
    barr[i] = NULL;

    h->warr = farr;
    h->wsiz = fsiz;
    h->arr = barr;
    h->siz = bsiz;
#else
    h->arr = farr;
    h->siz = fsiz;
#endif

    /*
     * Compile all the useful fragments for actual matching.
     */
    h->heurs = xmalloc(4 * sizeof(fastmatch_t *));
    if (!h->heurs)
      {
	errcode = REG_ESPACE;
	goto err;
      }
    for (i = 0; farr[i] != NULL; i++)
      {
	h->heurs[i] = xmalloc(sizeof(fastmatch_t));
	if (!h->heurs[i])
	  {
	    errcode = REG_ESPACE;
	    goto err;
	  }
#ifdef TRE_WCHAR
	ret = tre_proc_literal(h->heurs[i], farr[i], fsiz[i],
			       barr[i], bsiz[i], 0);
#else
	ret = tre_proc_literal(h->heurs[i], farr[i], fsiz[i],
			       farr[i], fsiz[i], 0);
#endif
	if (ret != REG_OK)
	  {
	    errcode = REG_BADPAT;
	    goto err;
	  }
      }

    h->heurs[i] = NULL;
    errcode = REG_OK;
    goto finish;
  }

err:
#ifdef TRE_WCHAR
  if (barr)
    {
      for (int i = 0; i < 4; i++)
	if (barr[i])
	  xfree(barr[i]);
      xfree(barr);
    }
  if (bsiz)
    xfree(bsiz);
#endif
  if (farr)
    {
      for (int i = 0; i < j; i++)
	if (farr[i])
	  xfree(farr[i]);
      xfree(farr);
    }
  if (fsiz)
    xfree(fsiz);
  if (h->heurs)
    {
      for (int i = 0; h->heurs[i] != NULL; i++)
	tre_free_fast(h->heurs[i]);
      xfree(h->heurs);
    }
finish:
  if (heur)
    xfree(heur);
  return errcode;
}

/*
 * Frees a heuristic.
 */
void
tre_free_heur(heur_t *h)
{
  for (int i = 0; h->heurs[i] != NULL; i++)
    tre_free_fast(h->heurs[i]);

  if (h->arr)
    {
      for (int i = 0; h->arr[i] != NULL; i++)
	if (h->arr[i])
	  xfree(h->arr[i]);
      xfree(h->arr);
    }

#ifdef TRE_WCHAR
  if (h->warr)
    {
      for (int i = 0; h->warr[i] != NULL; i++)
	if (h->warr[i])
	  xfree(h->warr[i]);
      xfree(h->warr);
    }

#endif

  DPRINT("tre_free_heur: resources are freed\n");
}
