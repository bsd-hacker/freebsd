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

#ifdef TRE_USE_ALLOCA
/* AIX requires this to be the first thing in the file.	 */
#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#   endif
#  endif
# endif
#endif
#endif /* TRE_USE_ALLOCA */

#include <assert.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif /* HAVE_WCHAR_H */
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif /* HAVE_WCTYPE_H */
#ifndef TRE_WCHAR
#include <ctype.h>
#endif /* !TRE_WCHAR */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */
#include <limits.h>

#include "tre-fastmatch.h"
#include "tre-heuristic.h"
#include "tre-internal.h"
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

  // XXX: Wu-Manber search with specific cases

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
    return tre_mmatch(str, len, type, nmatch, pmatch, eflags, preg);
}

int
tre_regexec(const mregex_t *preg, const char *str,
	size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_mregnexec(preg, str, strlen(str), nmatch, pmatch, eflags);
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
    return tre_mmatch(str, len, STR_WIDE, nmatch, pmatch, eflags, preg);
}

int
tre_mregwexec(const mregex_t *preg, const wchar_t *str,
	 size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_regwnexec(preg, str, tre_strlen(str), nmatch, pmatch, eflags);
}

#endif /* TRE_WCHAR */

/* EOF */
