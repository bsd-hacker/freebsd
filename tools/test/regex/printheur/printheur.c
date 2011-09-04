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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "config.h"
#include "tre-heuristic.h"
#include "tre-internal.h"

int
main(int argc, char *argv[])
{
  heur_t h;
  wchar_t *pat;
  size_t siz;
  int cflags, ret;

  siz = strlen(argv[1]);
  pat = malloc(sizeof(wint_t) * (siz + 1));
  if (pat == NULL)
	return (EXIT_FAILURE);
  siz = mbstowcs(pat, argv[1], siz);

  while ((ret = getopt(argc, argv, "E")) != -1)
	switch(ret) {
	case 'E':
		cflags |= REG_EXTENDED;
		break;
	default:
		printf("Usage: %s [-E]\n", getprogname());
		exit(EXIT_FAILURE);
	}

  ret = tre_compile_heur(&h, pat, siz, 0);
  if (ret != 0) {
	printf("No heuristic available.\n");
	exit (EXIT_SUCCESS);
  }

  printf("%s\n", h.prefix ? "Prefix heuristic" : "Normal heuristic");
  printf("Start heuristic: %s\n", h.start->pattern);
  if (!h.prefix)
	printf("End heuristic: %s\n", h.end->pattern);

  return (EXIT_SUCCESS);
}
