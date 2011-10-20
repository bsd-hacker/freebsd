/*      $FreeBSD$      */

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static regex_t regex;
static size_t fsiz;
static int cflags, eflags;

static void	 procfile(const char *path);

static inline char
*grep_open(const char *path)
{
	struct stat st;
	char *buffer;
	int flags = 0, fd;

	if ((fd = open(path, O_RDONLY)) == -1) {
		printf("Failed opening file %s\n", path);
		return NULL;
	}

	if ((fstat(fd, &st) == -1) || (st.st_size > OFF_MAX) ||
	    (!S_ISREG(st.st_mode))) {
		printf("Failed fstat'ing file %s\n", path);
		return NULL;
	}

#ifdef MAP_PREFAULT_READ
	flags |= MAP_PREFAULT_READ;
#endif
	fsiz = st.st_size;
	buffer = mmap(NULL, fsiz, PROT_READ, flags, fd, (off_t)0);
	if (buffer == MAP_FAILED) {
		printf("Failed mmap'ing file %s\n", path);
		return NULL;
	}
	madvise(buffer, st.st_size, MADV_SEQUENTIAL);
	return (buffer);
}

static inline void
grep_tree(char **argv)
{
	FTS *fts;
	FTSENT *p;
	int fts_flags = 0;

	fts_flags |= FTS_LOGICAL | FTS_NOSTAT | FTS_NOCHDIR;

	if (!(fts = fts_open(argv, fts_flags, NULL))) {
		printf("Failed fts_open\n");
		err(EXIT_FAILURE, "fts_open");
	}

	while ((p = fts_read(fts)) != NULL)
		switch (p->fts_info) {
		case FTS_DNR:
		case FTS_ERR:
		case FTS_D:
		case FTS_DP:
			/* FALLTHROUGH */
		case FTS_DC:
			break;
		default:
			procfile(p->fts_path);
		}

	fts_close(fts);
}

static inline void
procfile(const char *path)
{
	regmatch_t pmatch;
	regoff_t st = 0;
	char *data;

	data = grep_open(path);
	if (data == NULL)
		return;

	printf("Processing file %s\n", path);
	putchar('(');
	while (st != (long long)fsiz) {
		int ret;

		pmatch.rm_so = st;
		pmatch.rm_eo = fsiz;

		ret = regexec(&regex, data, 1, &pmatch, eflags);
		if (ret == REG_NOMATCH)
			break;
		printf("(%ld,%ld)", pmatch.rm_so, pmatch.rm_eo);
		if (pmatch.rm_so == pmatch.rm_eo)
			pmatch.rm_eo++;
		st = pmatch.rm_eo;
	}
	putchar(')');
}

int
main(int argc, char *argv[])
{
	const char *pat = NULL;
	int c, ret;
	bool dir = false;

	setlocale(LC_ALL, "");

	cflags = REG_NEWLINE;
	eflags = REG_STARTEND;

	while (((c = getopt(argc, argv, "e:EFNr")) != -1))
		switch (c) {
		case 'e':
			pat = strdup(optarg);
			if (pat == NULL)
				return (EXIT_FAILURE);
		case 'E':
			cflags |= REG_EXTENDED;
			break;
		case 'F':
			cflags |= REG_NOSPEC;
			break;
		case 'N':
			cflags |= REG_NEWLINE;
			break;
		case 'r':
			dir = true;
		}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		printf("Missing argument\n");
		return (EXIT_FAILURE);
	}

	ret = regcomp(&regex, pat, cflags);
	if (ret != 0) {
		printf("Wrong regex\n");
		return (EXIT_FAILURE);
	}

	if (dir)
		grep_tree(argv);
	else
		procfile(argv[0]);

	return (EXIT_SUCCESS);
}
