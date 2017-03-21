/*-
 * Copyright (c) 2008-2011 Dag-Erling Smørgrav
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
 * $FreeBSD$
 */

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

/*
 * Determine and display the byte order, signedness of char, and size and
 * alignment of various scalar types.  Assumes two's complement and
 * eight-bit bytes.
 */

static void
assumptions(void)
{
	assert(CHAR_BIT == 8);
	/*
	 * (unsigned char)-1 is
	 *   10000001 (0x81) on sign-and-magnitude
	 *   11111110 (0xfe) on one's complement
	 *   11111111 (0xff) on two's complement
	 */
	assert((unsigned char)-1 == 0xff);
}

static void
byte_order(void)
{
	uint32_t e_int = 0x30303030 | 0x04030201;
	const char *e_char = (const char *)&e_int;
	printf("%-16s %.4s\n", "byte order", e_char);
}

static void
signedness(void)
{
	unsigned char uc = 0x80;
	char *c = (char *)&uc;
	printf("%-16s %ssigned\n", "char is", (*c > 0) ? "un" : "");
}

typedef void *void_ptr;
typedef void (*func_ptr)(void);

#define describe(type)							\
	do {								\
		struct s_##t { char bump; type t; };			\
		printf("%-12s %12zd %12zd\n", #type,			\
		    sizeof(type) * 8,					\
		    offsetof(struct s_##t, t) * 8);			\
	} while (0)

static void
sizes(void)
{
	printf("type                 size    alignment\n");
	printf("--------------------------------------\n");
	describe(char);
	describe(wchar_t);
	describe(short);
	describe(int);
	describe(long);
	describe(long long);
	describe(int8_t);
	describe(int16_t);
	describe(int32_t);
	describe(int64_t);
	describe(intmax_t);
	describe(float);
	describe(double);
	describe(long double);
	describe(size_t);
	describe(ptrdiff_t);
	describe(time_t);
	describe(void_ptr);
	describe(func_ptr);
#if __STDC_VERSION__ >= 201112L
	describe(max_align_t);
#endif
	describe(sig_atomic_t);
}

int
main(void)
{
	assumptions();
	byte_order();
	signedness();
	printf("\n");
	sizes();
	return (0);
}
