#ifndef TRE_HEURISTIC_H
#define TRE_HEURISTIC_H 1

#include <fastmatch.h>
#include <stdbool.h>

#include "tre-fastmatch.h"
#include "tre-internal.h"

#define HEUR_ARRAY		0
#define HEUR_PREFIX_ARRAY	1
#define HEUR_LONGEST		2

typedef struct {
  fastmatch_t *heurs[4];
  int type;
} heur_t;

extern int tre_compile_heur(heur_t *h, const tre_char_t *regex,
			    size_t len, int cflags);
extern void tre_free_heur(heur_t *h);

#endif	/* TRE_HEURISTIC_H */
