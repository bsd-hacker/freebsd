/*      $FreeBSD$    */

/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <err.h>
#include <locale.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
	printf("Usage: %s pattern string\n", getprogname());
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	regex_t pattern;
	regmatch_t pmatch;
	char *env;
	ssize_t len;
	int cflags = 0, ret;
	int eflags = REG_STARTEND;

	setlocale(LC_ALL, "");

	env = getenv("REGTEST_FLAGS");
	if (strchr(env, 'E') != NULL)
		cflags |= REG_EXTENDED;
	if (strchr(env, 'F') != NULL)
		cflags |= REG_NOSPEC;

	if (argc != 3)
		usage();

	ret = regcomp(&pattern, argv[1], cflags);
	if (ret != 0)
		errx(1, NULL);

	len = strlen(argv[2]);
	pmatch.rm_so = 0;
	pmatch.rm_eo = len;
	putchar('(');
	for (bool first = true;;) {
		ret = regexec(&pattern, argv[2], 1, &pmatch, eflags);
		if (ret == REG_NOMATCH)
			break;
		if (!first)
			putchar(',');
		printf("(%lu,%lu)", (unsigned long)pmatch.rm_so,
			(unsigned long)pmatch.rm_eo);
		if (pmatch.rm_eo == len)
			break;
		pmatch.rm_so = (pmatch.rm_so == pmatch.rm_eo) ? pmatch.rm_eo + 1 : pmatch.rm_eo;
		pmatch.rm_eo = len;
		first = false;
	}
	printf(")\n");
	regfree(&pattern);
}
