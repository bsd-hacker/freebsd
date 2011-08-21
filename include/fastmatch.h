/* $FreeBSD$ */

#ifndef FASTMATCH_H
#define FASTMATCH_H 1

#include <hashtable.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <wchar.h>

typedef struct {
  size_t	 wlen;
  size_t	 len;
  wchar_t	*wpattern;
  int		 hasdot;
  int		 qsBc[UCHAR_MAX + 1];
  int		*bmGs;
  char		*pattern;
  int		 defBc;
  hashtable	*qsBc_table;
  int		*sbmGs;
  const char	*re_endp;

  /* flags */
  bool		 bol;
  bool		 eol;
  bool		 word;
  bool		 icase;
  bool		 newline;
} fastmatch_t;

extern int
tre_fixcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastcomp(fastmatch_t *preg, const char *regex, int cflags);

extern int
tre_fastexec(const fastmatch_t *preg, const char *string, size_t nmatch,
  regmatch_t pmatch[], int eflags);

extern void
tre_fastfree(fastmatch_t *preg);

extern int
tre_fixwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags);

extern int
tre_fastwexec(const fastmatch_t *preg, const wchar_t *string,
         size_t nmatch, regmatch_t pmatch[], int eflags);

/* Versions with a maximum length argument and therefore the capability to
   handle null characters in the middle of the strings. */
extern int
tre_fixncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastncomp(fastmatch_t *preg, const char *regex, size_t len, int cflags);

extern int
tre_fastnexec(const fastmatch_t *preg, const char *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

extern int
tre_fixwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwncomp(fastmatch_t *preg, const wchar_t *regex, size_t len, int cflags);

extern int
tre_fastwnexec(const fastmatch_t *preg, const wchar_t *string, size_t len,
  size_t nmatch, regmatch_t pmatch[], int eflags);

#endif		/* FASTMATCH_H */
