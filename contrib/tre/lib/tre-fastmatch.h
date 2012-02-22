#ifndef TRE_FASTMATCH_H
#define TRE_FASTMATCH_H 1

#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <wchar.h>

#include "hashtable.h"
#include "tre-internal.h"

typedef struct {
  size_t wlen;
  size_t len;
  wchar_t *wpattern;
  bool *wescmap;
  unsigned int qsBc[UCHAR_MAX + 1];
  unsigned int *bmGs;
  char *pattern;
  bool *escmap;
  unsigned int defBc;
  void *qsBc_table;
  unsigned int *sbmGs;
  const char *re_endp;

  /* flags */
  bool hasdot;
  bool bol;
  bool eol;
  bool word;
  bool icase;
  bool newline;
  bool nosub;
  bool matchall;
  bool reversed;
} fastmatch_t;

int
tre_proc_literal(fastmatch_t *, const tre_char_t *, size_t,
		 const char *, size_t, int);
int
tre_proc_fast(fastmatch_t *, const tre_char_t *, size_t,
	      const char *, size_t, int);
int
tre_match_fast(const fastmatch_t *fg, const void *data, size_t len,
	       tre_str_type_t type, int nmatch, regmatch_t pmatch[], int eflags);
void
tre_free_fast(fastmatch_t *preg);

#endif		/* TRE_FASTMATCH_H */
