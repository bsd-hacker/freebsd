/*	$NetBSD: util.c,v 1.9 2011/02/27 17:33:37 joerg Exp $	*/
/*	$FreeBSD$	*/
/*	$OpenBSD: util.c,v 1.39 2010/07/02 22:18:03 tedu Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2010 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#ifndef WITHOUT_FASTMATCH
#include "fastmatch.h"
#endif
#include "grep.h"

static int	 linesqueued;
static int	 procline(struct str *l, int);

static int	 lasta;
static bool	 ctxover;

bool
file_matching(const char *fname)
{
	char *fname_base, *fname_buf;
	bool ret;

	ret = finclude ? false : true;
	fname_buf = strdup(fname);
	if (fname_buf == NULL)
		err(2, "strdup");
	fname_base = basename(fname_buf);

	for (unsigned int i = 0; i < fpatterns; ++i) {
		if (fnmatch(fpattern[i].pat, fname, 0) == 0 ||
		    fnmatch(fpattern[i].pat, fname_base, 0) == 0) {
			if (fpattern[i].mode == EXCL_PAT) {
				ret = false;
				break;
			} else
				ret = true;
		}
	}
	free(fname_buf);
	return (ret);
}

static inline bool
dir_matching(const char *dname)
{
	bool ret;

	ret = dinclude ? false : true;

	for (unsigned int i = 0; i < dpatterns; ++i) {
		if (dname != NULL &&
		    fnmatch(dpattern[i].pat, dname, 0) == 0) {
			if (dpattern[i].mode == EXCL_PAT)
				return (false);
			else
				ret = true;
		}
	}
	return (ret);
}

/*
 * Processes a directory when a recursive search is performed with
 * the -R option.  Each appropriate file is passed to procfile().
 */
int
grep_tree(char **argv)
{
	FTS *fts;
	FTSENT *p;
	int c, fts_flags;
	bool ok;
	const char *wd[] = { ".", NULL };

	c = fts_flags = 0;

	switch(linkbehave) {
	case LINK_EXPLICIT:
		fts_flags = FTS_COMFOLLOW;
		break;
	case LINK_SKIP:
		fts_flags = FTS_PHYSICAL;
		break;
	default:
		fts_flags = FTS_LOGICAL;
			
	}

	fts_flags |= FTS_NOSTAT | FTS_NOCHDIR;

	fts = fts_open((argv[0] == NULL) ?
	    __DECONST(char * const *, wd) : argv, fts_flags, NULL);
	if (fts == NULL)
		err(2, "fts_open");
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			/* FALLTHROUGH */
		case FTS_ERR:
			file_err = true;
			if(!sflag)
				warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			break;
		case FTS_D:
			/* FALLTHROUGH */
		case FTS_DP:
			if (dexclude || dinclude)
				if (!dir_matching(p->fts_name) ||
				    !dir_matching(p->fts_path))
					fts_set(fts, p, FTS_SKIP);
			break;
		case FTS_DC:
			/* Print a warning for recursive directory loop */
			warnx("warning: %s: recursive directory loop",
				p->fts_path);
			break;
		default:
			/* Check for file exclusion/inclusion */
			ok = true;
			if (fexclude || finclude)
				ok &= file_matching(p->fts_path);

			if (ok)
				c += procfile(p->fts_path);
			break;
		}
	}

	fts_close(fts);
	return (c);
}

/*
 * Opens a file and processes it.  Each file is processed line-by-line
 * passing the lines to procline().
 */
int
procfile(const char *fn)
{
	struct file *f;
	struct stat sb;
	struct str ln;
	mode_t s;
	int c, t;

	mcount = mlimit;

	if (strcmp(fn, "-") == 0) {
		fn = label != NULL ? label : getstr(1);
		f = grep_open(NULL);
	} else {
		if (!stat(fn, &sb)) {
			/* Check if we need to process the file */
			s = sb.st_mode & S_IFMT;
			if (s == S_IFDIR && dirbehave == DIR_SKIP)
				return (0);
			if ((s == S_IFIFO || s == S_IFCHR || s == S_IFBLK
				|| s == S_IFSOCK) && devbehave == DEV_SKIP)
					return (0);
		}
		f = grep_open(fn);
	}
	if (f == NULL) {
		file_err = true;
		if (!sflag)
			warn("%s", fn);
		return (0);
	}

	ln.file = grep_malloc(strlen(fn) + 1);
	strcpy(ln.file, fn);
	ln.line_no = 0;
	ln.len = 0;
	ctxover = false;
	linesqueued = 0;
	tail = 0;
	lasta = 0;
	ln.off = -1;

	for (c = 0;  c == 0 || !(lflag || qflag); ) {
		ln.off += ln.len + 1;
		if ((ln.dat = grep_fgetln(f, &ln.len)) == NULL || ln.len == 0) {
			if (ln.line_no == 0 && matchall)
				exit(0);
			else
				break;
		}
		if (ln.len > 0 && ln.dat[ln.len - 1] == fileeol)
			--ln.len;
		ln.line_no++;

		/* Return if we need to skip a binary file */
		if (f->binary && binbehave == BINFILE_SKIP) {
			grep_close(f);
			free(ln.file);
			free(f);
			return (0);
		}

		/* Process the file line-by-line, enqueue non-matching lines */
		if ((t = procline(&ln, f->binary)) == 0 && Bflag > 0) {
			/* Except don't enqueue lines that appear in -A ctx */
			if (ln.line_no == 0 || lasta != ln.line_no) {
				/* queue is maxed to Bflag number of lines */
				enqueue(&ln);
				linesqueued++;
				ctxover = false;
			} else {
				/*
				 * Indicate to procline() that we have ctx
				 * overlap and make sure queue is empty.
				 */
				if (!ctxover)
					clearqueue();
				ctxover = true;
			}
		}
		c += t;
		if (mflag && mcount <= 0)
			break;
	}
	if (Bflag > 0)
		clearqueue();
	grep_close(f);

	if (cflag) {
		if (!hflag)
			printf("%s:", ln.file);
		printf("%u\n", c);
	}
	if (lflag && !qflag && c != 0)
		printf("%s%c", fn, nullflag ? 0 : '\n');
	if (Lflag && !qflag && c == 0)
		printf("%s%c", fn, nullflag ? 0 : '\n');
	if (c && !cflag && !lflag && !Lflag &&
	    binbehave == BINFILE_BIN && f->binary && !qflag)
		printf(getstr(8), fn);

	free(ln.file);
	free(f);
	return (c);
}

#define iswword(x)	(iswalnum((x)) || (x) == L'_')

/*
 * Processes a line comparing it with the specified patterns.  Each pattern
 * is looped to be compared along with the full string, saving each and every
 * match, which is necessary to colorize the output and to count the
 * matches.  The matching lines are passed to printline() to display the
 * appropriate output.
 */
static int
procline(struct str *l, int nottext)
{
	regmatch_t matches[MAX_LINE_MATCHES];
	regmatch_t pmatch, lastmatch;
	size_t st = 0, nst = 0;
	unsigned int i;
	int c = 0, m = 0, r = 0, lastmatches = 0, leflags = eflags;
	int startm = 0;

	/* Initialize to avoid a false positive warning from GCC. */
	lastmatch.rm_so = lastmatch.rm_eo = 0;

	/* Loop to process the whole line */
	while (st <= l->len) {
		lastmatches = 0;
		startm = m;
		if (st > 0)
			leflags |= REG_NOTBOL;
		/* Loop to compare with all the patterns */
		for (i = 0; i < patterns; i++) {
			pmatch.rm_so = st;
			pmatch.rm_eo = l->len;
#ifndef WITHOUT_FASTMATCH
			if (fg_pattern[i].pattern)
				r = fastexec(&fg_pattern[i],
				    l->dat, 1, &pmatch, leflags);
			else
#endif
				r = regexec(&r_pattern[i], l->dat, 1,
				    &pmatch, leflags);
			r = (r == 0) ? 0 : REG_NOMATCH;
			if (r == REG_NOMATCH)
				continue;
			/* Check for full match */
			if (r == 0 && xflag)
				if (pmatch.rm_so != 0 ||
				    (size_t)pmatch.rm_eo != l->len)
					r = REG_NOMATCH;
			/* Check for whole word match */
#ifndef WITHOUT_FASTMATCH
			if (r == 0 && (wflag || fg_pattern[i].word)) {
#else
			if (r == 0 && wflag) {
#endif
				wchar_t wbegin, wend;

				wbegin = wend = L' ';
				if (pmatch.rm_so != 0 &&
				    sscanf(&l->dat[pmatch.rm_so - 1],
				    "%lc", &wbegin) != 1)
					r = REG_NOMATCH;
				else if ((size_t)pmatch.rm_eo !=
				    l->len &&
				    sscanf(&l->dat[pmatch.rm_eo],
				    "%lc", &wend) != 1)
					r = REG_NOMATCH;
				else if (iswword(wbegin) ||
				    iswword(wend))
					r = REG_NOMATCH;
			}
			if (r == 0) {
				lastmatches++;
				lastmatch = pmatch;
				if (m == 0)
					c++;

				if (m < MAX_LINE_MATCHES) {
					/* Replace previous match if the new one is earlier and/or longer */
					if (m > startm) {
						if (pmatch.rm_so < matches[m-1].rm_so ||
						    (pmatch.rm_so == matches[m-1].rm_so && (pmatch.rm_eo - pmatch.rm_so) > (matches[m-1].rm_eo - matches[m-1].rm_so))) {
							matches[m-1] = pmatch;
							nst = pmatch.rm_eo;
						}
					} else {
						/* Advance as normal if not */
						matches[m++] = pmatch;
						nst = pmatch.rm_eo;
					}
				}

				/* matches - skip further patterns */
				if ((color == NULL && !oflag) ||
				    qflag || lflag)
					break;
			}
		}

		if (vflag) {
			c = !c;
			break;
		}

		/* One pass if we are not recording matches */
		if (!wflag && ((color == NULL && !oflag) || qflag || lflag || Lflag))
			break;

		/* If we didn't have any matches or REG_NOSUB set */
		if (lastmatches == 0 || (cflags & REG_NOSUB))
			nst = l->len;

		if (lastmatches == 0)
			/* No matches */
			break;
		else if (st == nst && lastmatch.rm_so == lastmatch.rm_eo)
			/* Zero-length match -- advance one more so we don't get stuck */
			nst++;

		/* Advance st based on previous matches */
		st = nst;
	}


	/* Count the matches if we have a match limit */
	if (mflag)
		mcount -= c;

	if (c && binbehave == BINFILE_BIN && nottext)
		return (c); /* Binary file */

	/* Dealing with the context */
	if ((tail || c) && !cflag && !qflag && !lflag && !Lflag) {
		if (c) {
			if (!first && !prev && !tail && (Bflag || Aflag) &&
			    !ctxover)
				printf("--\n");
			tail = Aflag;
			if (Bflag > 0) {
				printqueue();
				ctxover = false;
			}
			linesqueued = 0;
			printline(l, ':', matches, m);
		} else {
			/* Print -A lines following matches */
			lasta = l->line_no;
			printline(l, '-', matches, m);
			tail--;
		}
	}

	if (c) {
		prev = true;
		first = false;
	} else
		prev = false;

	return (c);
}

/*
 * Safe malloc() for internal use.
 */
void *
grep_malloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(2, "malloc");
	return (ptr);
}

/*
 * Safe calloc() for internal use.
 */
void *
grep_calloc(size_t nmemb, size_t size)
{
	void *ptr;

	if ((ptr = calloc(nmemb, size)) == NULL)
		err(2, "calloc");
	return (ptr);
}

/*
 * Safe realloc() for internal use.
 */
void *
grep_realloc(void *ptr, size_t size)
{

	if ((ptr = realloc(ptr, size)) == NULL)
		err(2, "realloc");
	return (ptr);
}

/*
 * Safe strdup() for internal use.
 */
char *
grep_strdup(const char *str)
{
	char *ret;

	if ((ret = strdup(str)) == NULL)
		err(2, "strdup");
	return (ret);
}

/*
 * Prints a matching line according to the command line options.
 */
void
printline(struct str *line, int sep, regmatch_t *matches, int m)
{
	size_t a = 0;
	int i, n = 0;

	/* If matchall, everything matches but don't actually print for -o */
	if (oflag && matchall)
		return;

	if (!hflag) {
		if (!nullflag) {
			fputs(line->file, stdout);
			++n;
		} else {
			printf("%s", line->file);
			putchar(0);
		}
	}
	if (nflag) {
		if (n > 0)
			putchar(sep);
		printf("%d", line->line_no);
		++n;
	}
	if (bflag) {
		if (n > 0)
			putchar(sep);
		printf("%lld", (long long)line->off);
		++n;
	}
	if (n)
		putchar(sep);
	/* --color and -o */
	if ((oflag || color) && m > 0) {
		for (i = 0; i < m; i++) {
			/* Don't output zero length matches */
			if (matches[i].rm_so == matches[i].rm_eo)
				continue;
			if (!oflag)
				fwrite(line->dat + a, matches[i].rm_so - a, 1,
				    stdout);
			if (color) 
				fprintf(stdout, "\33[%sm\33[K", color);

				fwrite(line->dat + matches[i].rm_so, 
				    matches[i].rm_eo - matches[i].rm_so, 1,
				    stdout);
			if (color) 
				fprintf(stdout, "\33[m\33[K");
			a = matches[i].rm_eo;
			if (oflag)
				putchar('\n');
		}
		if (!oflag) {
			if (line->len - a > 0)
				fwrite(line->dat + a, line->len - a, 1, stdout);
			putchar('\n');
		}
	} else {
		fwrite(line->dat, line->len, 1, stdout);
		putchar(fileeol);
	}
}
