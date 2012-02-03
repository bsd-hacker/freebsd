#ifndef TRE_MFASTMATCH_H
#define TRE_MFASTMATCH_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <mregex.h>
#include <regex.h>

#define WM_MAXPAT 64

typedef struct {
        size_t m;		/* Shortest pattern length */
	size_t defsh;		/* Default shift */
	void *hash;		/* Wu-Manber shift table */
#ifdef TRE_WCHAR
	size_t wm;		/* Shortest pattern length (wide) */
	size_t wdefsh;		/* Default shift (wide) */
	void *whash;		/* Wu-Manber shift table (wide) */
#endif
} wmsearch_t;

typedef struct {
	size_t shift;			/* Shift value for fragment */
	size_t suff;			/* No of pats ending w/ fragment */
	uint8_t suff_list[WM_MAXPAT];	/* Pats ending w/ fragment */
	size_t pref;			/* No of pats starting w/ fragment */
	uint8_t pref_list[WM_MAXPAT];	/* Pats starting w/ fragment */
} wmentry_t;

#endif				/* TRE_MFASTMATCH_H */
