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
#include <stdio.h>

#include "svnsup.h"

static const char b64enc[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
};

static const char b64dec[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*
 * Compute the amount of space required to base64-encode a specified
 * amount of data.
 */
size_t
svnsup_base64_encode_length(size_t size)
{

	return (((size + 2) / 3) * 4);
}

/*
 * Encode a buffer into another.  Assumes that str points to a buffer of
 * sufficient length.
 */
size_t
svnsup_base64_encode(char *b64, const unsigned char *data, size_t size)
{
	size_t count = 0;

	while (size > 3) {
		*b64++ = b64enc[data[0] >> 2];
		*b64++ = b64enc[(data[0] << 4 | data[1] >> 4) & 0x3f];
		*b64++ = b64enc[(data[1] << 2 | data[2] >> 6) & 0x3f];
		*b64++ = b64enc[data[2] & 0x3f];
		count += 4;
		data += 3;
		size -= 3;
	}
	if (size > 0) {
		*b64++ = b64enc[data[0] >> 2];
		if (size > 1) {
			*b64++ = b64enc[(data[0] << 4 | data[1] >> 4) & 0x3f];
			*b64++ = b64enc[(data[1] << 2) & 0x3f];
		} else {
			*b64++ = b64enc[(data[0] << 4) & 0x3f];
			*b64++ = '=';
		}
		*b64++ = '=';
		count += 4;
	}
	return (count);
}

/*
 * Encode a buffer and write the result to a file
 */
size_t
svnsup_base64_fencode(FILE *f, const unsigned char *data, size_t size)
{
	size_t count = 0;
#if 0
	int width = 0;
#endif

	while (size >= 3) {
		putc(b64enc[data[0] >> 2], f);
		putc(b64enc[(data[0] << 4 | data[1] >> 4) & 0x3f], f);
		putc(b64enc[(data[1] << 2 | data[2] >> 6) & 0x3f], f);
		putc(b64enc[data[2] & 0x3f], f);
		count += 4;
		data += 3;
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
		putc(b64enc[data[0] >> 2], f);
		if (size > 1) {
			putc(b64enc[(data[0] << 4 | data[1] >> 4) & 0x3f], f);
			putc(b64enc[(data[1] << 2) & 0x3f], f);
		} else {
			putc(b64enc[(data[0] << 4) & 0x3f], f);
			putc('=', f);
		}
		putc('=', f);
		count += 4;
	}
	return (count);
}

/*
 * Compute the amount of space required to decode a base64-encoded string
 * of the specified length.  Note that this number may be a little high
 * due to padding.
 */
size_t
svnsup_base64_decode_length(size_t size)
{

	return ((size + 3 / 4) * 3);
}

/*
 * Decode a bas64-encoded string into a buffer.
 */
size_t
svnsup_base64_decode(unsigned char *data, const char *b64, size_t len)
{
	size_t count = 0;

	assert(len % 4 == 0);
	while (len > 0) {
		assert(b64dec[(int)b64[0]] != -1);
		assert(b64dec[(int)b64[1]] != -1);
		assert(b64dec[(int)b64[2]] != -1 || b64[2] == '=');
		assert(b64dec[(int)b64[3]] != -1 || b64[3] == '=');

		*data = b64dec[(int)b64[0]] << 2 | b64dec[(int)b64[1]] >> 4;
		++count, ++data;
		if (b64[2] != '=') {
			*data = b64dec[(int)b64[1]] << 4 | b64dec[(int)b64[2]] >> 2;
			++count, ++data;
		}
		if (b64[3] != '=') {
			*data = b64dec[(int)b64[2]] << 6 | b64dec[(int)b64[3]];
			++count, ++data;
		}
		b64 += 4;
		len -= 4;
	}
	return (count);
}
