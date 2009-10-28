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

#include <stdio.h>

#include "svnsup.h"

static const char b64t[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "+/"
    "=";

int
svnsup_base64_fencode(FILE *f, const unsigned char *buf, size_t size)
{
	int count = 0;
#if 0
	int width = 0;
#endif

	while (size >= 3) {
		putc(b64t[buf[0] >> 2], f);
		putc(b64t[(buf[0] << 4 | buf[1] >> 4) & 0x3f], f);
		putc(b64t[(buf[1] << 2 | buf[2] >> 6) & 0x3f], f);
		putc(b64t[buf[2] & 0x3f], f);
		count += 4;
		buf += 3;
		size -= 3;
#if 0
		if ((width += 3) == 64 && size > 0) {
			putc('\n', f);
			++count;
			width = 0;
		}
#endif
	}
	if (size > 0) {
		putc(b64t[buf[0] >> 2], f);
		if (size > 1) {
			putc(b64t[(buf[0] << 4 | buf[1] >> 4) & 0x3f], f);
			putc(b64t[(buf[1] << 2) & 0x3f], f);
		} else {
			putc(b64t[(buf[0] << 4) & 0x3f], f);
			putc('=', f);
		}
		putc('=', f);
		count += 4;
	}
	return (count);
}
