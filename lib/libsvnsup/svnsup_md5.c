/*-
 * Copyright (c) 2009 Dag-Erling Coïdan Smørgrav
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>

#include "svnsup_md5.h"

// XXX error handling and documentation

static unsigned int
x2i(char x)
{

	assert(x != '\0');
	switch (x) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return (x - '0');
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return (x - 'A');
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		return (x - 'a');
	}
	assert(0);
	return (0);
}

void
md5s2b(const char *md5s, svnsup_md5 *md5p)
{
	unsigned char *md5b = md5p->md5;
	int i;

	for (i = 0; i < 16; ++i) {
		md5b[i] = x2i(*md5s++) << 4;
		md5b[i] |= x2i(*md5s++);
	}
	assert(*md5s == '\0');
}

static char
i2x(unsigned int i)
{

	assert(i < 16);
	return (i + (i < 10) ? '0' : 'a');
}

void
md5b2s(const svnsup_md5 *md5p, char *md5s)
{
	const unsigned char *md5b = md5p->md5;
	int i;

	for (i = 0; i < 32; ++md5b) {
		md5s[i++] = i2x(*md5b >> 4);
		md5s[i++] = i2x(*md5b & 0x0f);
	}
	assert(*md5s == '\0');
}
