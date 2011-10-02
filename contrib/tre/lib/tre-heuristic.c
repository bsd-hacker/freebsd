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
  tre_char_t *heur;
  int st = 0, pos = 0;
  bool escaped = false;
  int errcode, ret;

  /* Temporary space, len will be enough. */
  heur = xmalloc(len);
  if (!heur)
    return REG_ESPACE;

  memset(h, 0, sizeof(*h));

  while (true)
    {

      /*
       * Process the pattern char-by-char.
       *
       * i: position in regex
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
		    goto badpat2;
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

      /* If it is not initialized yet, then we just got the first segment. */
      if (h->start == NULL)
	{

	  /*
	   * An empty or a one-char prefix segment is useless,
	   * better to just fail.
	   */
	  if (pos <= 1)
	    {
	      errcode = REG_BADPAT;
	      DPRINT("tre_compile_heur: pattern does not have a "
		     " fixed-length prefix that is long enough\n");
	      goto badpat1;
	    }

	  h->start = xmalloc(sizeof(fastmatch_t));
	  if (!h->start)
	    {
	      errcode = REG_ESPACE;
	      goto space1;
	    }

	  ret = tre_compile_fast(h->start, heur, pos, 0);
	  if (ret != REG_OK)
	    {
	      errcode = REG_BADPAT;
	      goto badpat2;
	    }
	  DPRINT(("tre_compile_heur: fixed-length prefix is %s\n",
		 h->start->pattern));
	}

      /*
       * If true, this is the last segment. We do not care about the
       * middle ones.
       */
      else if (st == len)
	{

	  /* If empty, we only have a prefix heuristic. */
	  if (pos == 0)
	    {
	      h->prefix = true;
	      errcode = REG_OK;
	      DPRINT("tre-compile_heur: using only a fixed-length prefix; "
		     "no fixed-length suffix is available\n");
	      goto ok;
	    }

	  h->end = xmalloc(sizeof(fastmatch_t));
	  if (!h->end)
	    {
	      errcode = REG_ESPACE;
	      goto space2;
	    }
	    
	  ret = tre_compile_fast(h->end, heur, pos, 0);
	  if (ret != REG_OK)
	    {
	      xfree(h->end);
	      h->prefix = true;
	    }
	  errcode = REG_OK;
	  DPRINT(("tre_compile_heur: fixed-length suffix is %s\n",
		 h->end->pattern));
	  goto ok;
	}

      /* Just drop middle segments by overwriting the temporary space. */
      pos = 0;
    }

badpat2:
space2:
  if (h->start != NULL)
    xfree(h->start);
badpat1:
space1:
  DPRINT("tre_compile_heur: compiling a heuristic failed\n");
ok:
  xfree(heur);
  return errcode;
}

/*
 * Frees a heuristic.
 */
void
tre_free_heur(heur_t *h)
{
  if (h->start != NULL)
    xfree(h->start);
  if (h->end != NULL)
    xfree(h->end);

  DPRINT("tre_free_heur: resources are freed\n");
}
