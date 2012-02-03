#ifndef MREGEX_H
#define MREGEX_H 1

#include <sys/types.h>

#include <regex.h>

typedef struct {
	size_t k;		/* Number of patterns */
	regex_t *patterns;	/* regex_t structure for each pattern */
	void *searchdata;
} mregex_t;

#endif				/* REGEX_H */

/* EOF */
