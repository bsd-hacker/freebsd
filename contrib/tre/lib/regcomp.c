/*
  tre_regcomp.c - TRE POSIX compatible regex compilation functions.

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>

#include "tre-fastmatch.h"
#include "tre-heuristic.h"
#include "tre-internal.h"
#include "xmalloc.h"

#ifdef TRE_LIBC_BUILD
__weak_reference(tre_regcomp, regcomp);
__weak_reference(tre_regncomp, regncomp);
__weak_reference(tre_regwcomp, regwcomp);
__weak_reference(tre_regwncomp, regwncomp);
__weak_reference(tre_regfree, regfree);
#endif

int
tre_regncomp(regex_t *preg, const char *regex, size_t n, int cflags)
{
  int ret;
  tre_char_t *wregex;
  size_t wlen;

  ret = tre_convert_pattern_to_wcs(regex, n, &wregex, &wlen);
  if (ret != REG_OK)
    return ret;
  else
    ret = tre_compile(preg, wregex, wlen, regex, n, cflags);
  tre_free_wcs_pattern(wregex);
  return ret;
}

int
tre_regcomp(regex_t *preg, const char *regex, int cflags)
{
  if ((cflags & REG_PEND) && (preg->re_endp >= regex))
    return tre_regncomp(preg, regex, preg->re_endp - regex, cflags);
  else
    return tre_regncomp(preg, regex, regex ? strlen(regex) : 0, cflags);
}


#ifdef TRE_WCHAR
int
tre_regwncomp(regex_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  int ret;
  char *sregex;
  size_t slen;

  ret = tre_convert_pattern_to_mbs(regex, n, &sregex, &slen);
  if (ret != REG_OK)
    return ret;
  else
    ret = tre_compile(preg, regex, n, sregex, slen, cflags);
  tre_free_mbs_pattern(sregex);
  return ret;
}

int
tre_regwcomp(regex_t *preg, const wchar_t *regex, int cflags)
{
  if ((cflags & REG_PEND) && (preg->re_wendp >= regex))
    return tre_regwncomp(preg, regex, preg->re_wendp - regex, cflags);
  else
    return tre_regwncomp(preg, regex, regex ? wcslen(regex) : 0, cflags);
}
#endif /* TRE_WCHAR */

void
tre_regfree(regex_t *preg)
{
  if (preg->shortcut != NULL)
    {
      tre_free_fast(preg->shortcut);
      xfree(preg->shortcut);
    }
  if (preg->heur != NULL)
    {
      tre_free_heur(preg->heur);
      xfree(preg->heur);
    }
  tre_free(preg);
}

/* EOF */
