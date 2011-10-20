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

#define MAX_FRAGMENTS 32

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
#define END_SEGMENT							\
  do									\
    {									\
      st = i + 1;							\
      escaped = false;							\
      goto end_segment;							\
    } while (0)

#define STORE_CHAR(esc)							\
  do									\
    {									\
      if (esc)								\
	heur[pos++] = TRE_CHAR('\\');					\
      heur[pos++] = regex[i];						\
      escaped = false;							\
      continue;								\
    } while (0)


/*
 * Parses a regular expression and constructs a heuristic in heur_t and
 * returns REG_OK if successful or the corresponding error code if a
 * heuristic cannot be constructed.
 */
int
tre_compile_heur(heur_t *h, const tre_char_t *regex, size_t len, int cflags)
{
  tre_char_t *arr[MAX_FRAGMENTS], *heur;
  size_t length[MAX_FRAGMENTS];
  int errcode, j = 0, pos = 0, st = 0;
  bool escaped = false;

  heur = xmalloc(len * sizeof(tre_char_t));
  if (!heur)
    return REG_ESPACE;

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
	       * Bracketed expression is substituted with a dot or the
	       * brackets are treated as normal if at least the opening
	       * bracket is escaped.
	       */
	      case TRE_CHAR('['):
		if (escaped)
		  STORE_CHAR(true);
		else
		  {
		    PARSE_BRACKETS;
		    heur[pos++] = TRE_CHAR('.');
		  }
		continue;

	      /*
	       * If a repetition marker, erases the repeting character
	       * and terminates the segment, otherwise treated as a normal
	       * character.
	       */
	      case TRE_CHAR('{'):
		if (escaped && (i == 1))
		  STORE_CHAR(true);
		else if ((i == 0) && !(cflags & REG_EXTENDED))
		  STORE_CHAR(true);
		else if ((i == 0) && (cflags & REG_EXTENDED))
		  continue;

		PARSE_UNIT('{', '}');
		if (escaped ^ (cflags & REG_EXTENDED))
		  {
		    pos--;
		    END_SEGMENT;
		  }
		else
		  STORE_CHAR(cflags & REG_EXTENDED);
		continue;

	      /*
	       * Terminates the current segment when escaped,
	       * otherwise treated as a normal character.
	       */
	      case TRE_CHAR('('):
		if (escaped ^ (cflags & REG_EXTENDED))
		  {
		    PARSE_UNIT('(', ')');
		    END_SEGMENT;
		  }
		else
		  STORE_CHAR(cflags & REG_EXTENDED);
		continue;

	      /*
	       * Sets escaped flag.
	       * Escaped escape is treated as a normal character.
	       * (This is also the GNU behaviour.)
	       */
	      case TRE_CHAR('\\'):
		if (escaped)
		  STORE_CHAR(true);
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
		  STORE_CHAR(true);
		else if ((i != 0))
		  {
		    pos--;
		    END_SEGMENT;
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
		  END_SEGMENT;
		else 
		  STORE_CHAR(cflags & REG_EXTENDED);
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
		    pos--;
		    END_SEGMENT;
		  }
		else
		  STORE_CHAR(true);
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
		  END_SEGMENT;
		else
		  STORE_CHAR(cflags & REG_EXTENDED);
		continue;

	      /*
	       * Cut the segment at an escaped dot because the fast matcher
	       * cannot handle it.
	       */
	      case TRE_CHAR('.'):
		STORE_CHAR(escaped);
		continue;

	      /*
	       * If escaped, terminates segment.
	       * Otherwise adds current character to the current segment
	       * by copying it to the temporary space.
	       */
	      default:
		if (escaped)
		  END_SEGMENT;
		else
		  STORE_CHAR(false);
		continue;
	    }
	}

      /* We are done with the pattern if we got here. */
      st = len;
 
end_segment:

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

      if (j == MAX_FRAGMENTS)
	{
	  errcode = REG_BADPAT;
	  goto err;
	}

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
    }

ok:
  {
    size_t m = 1;
    int ret;

    for (int i = 1; i < j; i++)
      m = (length[i] > length[m]) ? i : m;

    for (int i = 0; i < MIN(3, j + 1); i++)
      {
	h->heurs[i] = xmalloc(sizeof(fastmatch_t));
	if (!h->heurs[i])
	  {
	    errcode = REG_ESPACE;
	    goto err;
	  }
      }

#define CHECK_ERR							\
  if (ret != REG_OK)							\
    {									\
      errcode = REG_BADPAT;						\
      goto err2;							\
    }

    if (cflags & REG_NEWLINE)
      {
	ret = tre_compile_fast(h->heurs[0], arr[m], length[m], 0);
	CHECK_ERR
	h->type = HEUR_LONGEST;
      }
    else
      {
	ret = tre_compile_fast(h->heurs[0], arr[0], length[0], 0);
	CHECK_ERR
	if (j == 1)
	  {
	    free(h->heurs[1]);
	    h->heurs[1] = NULL;
	    errcode = REG_OK;
	    goto finish;
	  }
	else
	  ret = tre_compile_fast(h->heurs[1], arr[m], length[m], 0);
	CHECK_ERR
	if ((h->type == HEUR_PREFIX_ARRAY) || (m == j - 1))
	  {
	    xfree(h->heurs[2]);
	    h->heurs[2] = NULL;
	    errcode = REG_OK;
	    goto finish;
	  }
	else
	  ret = tre_compile_fast(h->heurs[2], arr[j - 1], length[j - 1], 0);
	CHECK_ERR
	h->heurs[3] = NULL;
      }

      errcode = REG_OK;
      goto finish;
  }

err2:
  for (int i = 0; h->heurs[i] != NULL; i++)
    tre_free_fast(h->heurs[i]);
  xfree(h->heurs);
err:
finish:
  for (int i = 0; i < j; i++)
    xfree(arr[i]);
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

  DPRINT("tre_free_heur: resources are freed\n");
}
