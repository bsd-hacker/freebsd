#ifndef MREGEX_H
#define MREGEX_H 1

#include <sys/types.h>

#include <wchar.h>
#include <regex.h>

#ifndef TRE_LIBC_BUILD
#define tre_mregncomp	mregncomp
#define tre_mregcomp	mregcomp
#define tre_mregnexec	mregnexec
#define tre_mregerror	mregerror
#define tre_mregexec	mregexec
#define tre_mregfree	mregfree
#define tre_mregwncomp	mregwncomp
#define tre_mregwcomp	mregwcomp
#define tre_mregwnexec	mregwnexec
#define tre_mregwexec	mregwexec

#define FUNC_DECL(f)    f

#else

#define mregncomp	tre_mregncomp
#define mregcomp	tre_mregcomp
#define mregnexec	tre_mregnexec
#define mregerror	tre_mregerror
#define mregexec	tre_mregexec
#define mregfree	tre_mregfree
#define mregwncomp	tre_mregwncomp
#define mregwcomp	tre_mregwcomp
#define mregwnexec	tre_mregwnexec
#define mregwexec	tre_mregwexec

#define FUNC_DECL(f)    tre_##f
#endif

typedef struct {
	size_t k;		/* Number of patterns */
	regex_t *patterns;	/* regex_t structure for each pattern */
	size_t mfrag;		/* XXX (private) Number of fragments */
	size_t type;		/* XXX (private) Matching type */
	int err;		/* XXX (private) Which pattern failed */
	void *searchdata;
} mregex_t;

int
FUNC_DECL(mregncomp)(mregex_t *preg, size_t nr, const char **regex,
	      size_t *n, int cflags);
int
FUNC_DECL(mregcomp)(mregex_t *preg, size_t nr, const char **regex, int cflags);

int
FUNC_DECL(mregnexec)(const mregex_t *preg, const char *str, size_t len,
	      size_t nmatch, regmatch_t pmatch[], int eflags);
int
FUNC_DECL(mregexec)(const mregex_t *preg, const char *str,
	    size_t nmatch, regmatch_t pmatch[], int eflags);
void
FUNC_DECL(mregfree)(mregex_t *preg);
size_t
FUNC_DECL(mregerror)(int errcode, const mregex_t *preg,
		     int *errpatn, char *errbuf, size_t errbuf_size);
#ifdef TRE_WCHAR
int
FUNC_DECL(mregwncomp)(mregex_t *preg, size_t nr, const wchar_t **regex,
	       size_t *n, int cflags);
int
FUNC_DECL(mregwcomp)(mregex_t *preg, size_t nr, const wchar_t **regex,
	      int cflags);
int
FUNC_DECL(mregwnexec)(const mregex_t *preg, const wchar_t *str, size_t len,
	       size_t nmatch, regmatch_t pmatch[], int eflags);
int
FUNC_DECL(mregwexec)(const mregex_t *preg, const wchar_t *str,
	      size_t nmatch, regmatch_t pmatch[], int eflags);
#endif /* TRE_WCHAR */

#endif				/* MREGEX_H */

/* EOF */
