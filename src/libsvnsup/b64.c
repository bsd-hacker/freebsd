/*-
 * Copyright (c) 2010 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *
 * $Id$
 */

#include <stdio.h>

static const char b64enc[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/";

static char b64dec[256];

int
main(void)
{
	int i, j;

	for (i = 0; i < 64; ++i)
		b64dec[(int)b64enc[i]] = i + 1;
	for (i = 0; i < 256; ++i)
		--b64dec[i];

	printf("static const char b64enc[64] = {\n");
	for (i = 0; i < 8; ++i) {
		putchar('\t');
		for (j = 0; j < 8; ++j) {
			if (j > 0)
				putchar(' ');
			printf("'%c',", b64enc[i * 8 + j]);
		}
		putchar('\n');
	}
	printf("};\n");

	printf("static const char b64dec[256] = {\n");
	for (i = 0; i < 16; ++i) {
		putchar('\t');
		for (j = 0; j < 16; ++j) {
			if (j > 0)
				putchar(' ');
			printf("%2d,", b64dec[i * 16 + j]);
		}
		putchar('\n');
	}
	printf("};\n");

	return (0);
}
