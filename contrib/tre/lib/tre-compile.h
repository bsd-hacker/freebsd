/*
  tre-compile.h: Regex compilation definitions

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/


#ifndef TRE_COMPILE_H
#define TRE_COMPILE_H 1

#include <regex.h>

typedef struct {
  int position;
  int code_min;
  int code_max;
  int *tags;
  int assertions;
  tre_ctype_t class;
  tre_ctype_t *neg_classes;
  int backref;
  int *params;
} tre_pos_and_tags_t;

int tre_compile_bm(regex_t *preg, const tre_char_t *wregex, size_t wn,
		   const char *regex, size_t n, int cflags);
int tre_compile_heur(regex_t *preg, const tre_char_t *regex, size_t n,
		     int cflags);
int tre_compile_nfa(regex_t *preg, const tre_char_t *regex, size_t n,
		    int cflags);

#endif /* TRE_COMPILE_H */

/* EOF */
