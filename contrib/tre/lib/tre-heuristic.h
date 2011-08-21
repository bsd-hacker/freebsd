#ifndef TRE_HEURISTIC_H
#define TRE_HEURISTIC_H 1

#include <fastmatch.h>
#include <stdbool.h>

#include "tre-fastmatch.h"
#include "tre-internal.h"

typedef struct {
  fastmatch_t *start;
  fastmatch_t *end;
  bool prefix;
} heur_t;


extern int tre_compile_heur(heur_t *h, const tre_char_t *regex,
			    size_t len, int cflags);
extern void tre_free_heur(heur_t *h);

#endif	/* TRE_HEURISTIC_H */
