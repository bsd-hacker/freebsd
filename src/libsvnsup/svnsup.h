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

#ifndef SVNSUP_H_INCLUDED
#define SVNSUP_H_INCLUDED

typedef enum svnsup_err {
	SVNSUP_ERR_NONE,
	SVNSUP_ERR_MEMORY,
	SVNSUP_ERR_UNKNOWN,
	SVNSUP_ERR_MAX,
} svnsup_err_t;

/*
 * svnsup_delta.c
 */
typedef struct svnsup_delta *svnsup_delta_t;
typedef struct svnsup_delta_file *svnsup_delta_file_t;

int svnsup_create_delta(svnsup_delta_t *);
int svnsup_close_delta(svnsup_delta_t);

int svnsup_delta_root(svnsup_delta_t, const char *);
int svnsup_delta_uuid(svnsup_delta_t, const char *);
int svnsup_delta_path(svnsup_delta_t, const char *);
int svnsup_delta_comment(svnsup_delta_t, const char *, ...);
int svnsup_delta_meta(svnsup_delta_t, const char *, const char *, ...);
int svnsup_delta_create_directory(svnsup_delta_t, const char *);
int svnsup_delta_remove(svnsup_delta_t, const char *);
int svnsup_delta_text(svnsup_delta_t, const char *, size_t,
    unsigned int *);

int svnsup_delta_create_file(svnsup_delta_t, svnsup_delta_file_t *,
    const char *);
int svnsup_delta_open_file(svnsup_delta_t, svnsup_delta_file_t *,
    const char *);
int svnsup_delta_file_checksum(svnsup_delta_file_t, const char *);
int svnsup_delta_file_text(svnsup_delta_file_t, const char *, size_t,
    unsigned int *);
int svnsup_delta_file_copy(svnsup_delta_file_t, off_t, size_t);
int svnsup_delta_file_repeat(svnsup_delta_file_t, off_t, size_t);
int svnsup_delta_file_insert(svnsup_delta_file_t, unsigned int, off_t, size_t);
int svnsup_delta_close_file(svnsup_delta_file_t, const char *);

/*
 * svnsup_string.c
 */
int svnsup_string_is_safe(const char *);
int svnsup_buf_is_safe(const unsigned char *, size_t);
char *svnsup_string_encode(const char *);
char *svnsup_buf_encode(const unsigned char *, size_t);

#ifdef FOPEN_MAX /* defined by stdio.h, cf. IEEE 1003.1 */
size_t svnsup_string_fencode(FILE *, const char *);
size_t svnsup_buf_fencode(FILE *, const unsigned char *, size_t);
#endif

/*
 * svnsup_base64.c
 */
size_t svnsup_base64_encode(char *, const unsigned char *, size_t);
size_t svnsup_base64_decode(unsigned char *, const char *, size_t);
#ifdef FOPEN_MAX /* defined by stdio.h, cf. IEEE 1003.1 */
size_t svnsup_base64_fencode(FILE *, const unsigned char *, size_t);
/* no fdecode yet */
#endif

#endif
