#ifndef MREGEX_H
#define MREGEX_H 1

#include <sys/types.h>

#include <regex.h>

typedef struct {
	size_t k;		/* Number of patterns */
	regex_t *patterns;	/* regex_t structure for each pattern */
	size_t mfrag;		/* XXX Number of fragments */
	size_t type;		/* XXX Matching type */
	void *searchdata;
} mregex_t;

int
tre_mregncomp(mregex_t *preg, size_t nr, const char *regex[],
	      size_t n[], int cflags);
int
tre_mregcomp(mregex_t *preg, size_t nr, const char *regex[], int cflags);
int
tre_mregnexec(const mregex_t *preg, const char *str, size_t len,
	      size_t nmatch, regmatch_t pmatch[], int eflags);
int
tre_regexec(const mregex_t *preg, const char *str,
	    size_t nmatch, regmatch_t pmatch[], int eflags);
void
tre_mregfree(mregex_t *preg);
#ifdef TRE_WCHAR
int
tre_mregwncomp(mregex_t *preg, size_t nr, const wchar_t *regex[],
	       size_t n[], int cflags);
int
tre_mregwcomp(mregex_t *preg, size_t nr, const wchar_t *regex[],
	      int cflags);
int
tre_mregwnexec(const mregex_t *preg, const wchar_t *str, size_t len,
	       size_t nmatch, regmatch_t pmatch[], int eflags);
int
tre_mregwexec(const mregex_t *preg, const wchar_t *str,
	      size_t nmatch, regmatch_t pmatch[], int eflags);
#endif /* TRE_WCHAR */

#endif				/* MREGEX_H */

/* EOF */
