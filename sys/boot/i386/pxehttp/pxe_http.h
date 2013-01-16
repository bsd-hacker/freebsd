/*-
 * Copyright (c) 2007 Alexey Tarasov
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
 *
 */
 
#ifndef PXE_HTTP_INCLUDED
#define PXE_HTTP_INCLUDED

/*
 * http downloading functions.
 */

#include <sys/types.h>
#include <stdint.h>

#ifdef PXE_HTTPFS_CACHING
#include "pxe_buffer.h"
#endif

/* default buffer size for generating/getting http header */
#define PXE_MAX_HTTP_HDRLEN	1024
/* testing function, outputs received data to screen */
int pxe_fetch(char *server, char *filename, off_t from, size_t size);

typedef struct pxe_http_handle {

	char		*filename;	/* filename including path on server */
	char		*servername;	/* server name */
	
	int		socket;		/* opened socket, or -1 */
	
	char		*buf;		/* buffer for creating requests */
	uint16_t	bufsize;	/* size of buffer */
	
	PXE_IPADDR	addr;		/* web server ip */
	off_t		offset;		/* current offset in bytes from
					 * beginning of file */
#ifdef PXE_HTTPFS_CACHING
/*	off_t		cache_start;	/* cached block, to reduce http requests */
	uint16_t	cache_size;	/* size of cached block */
#endif
	size_t		size;		/* file size if known */
	int		isKeepAlive;	/* if connection keep-alive? */
} PXE_HTTP_HANDLE;

/* gets requested data from server */
int pxe_get(PXE_HTTP_HANDLE *hh, size_t size, void *buffer);

/* gets requested data from server, closing connection after */
int pxe_get_close(PXE_HTTP_HANDLE *hh, size_t size, void *buffer);

/* checks if file exists and fills filesize if known */
int pxe_exists(PXE_HTTP_HANDLE *hh);

#define PXE_HTTP_SIZE_UNKNOWN	-1

typedef struct pxe_http_parse_data {
	uint16_t	code;	/* response code */
	size_t		size;	/* size of data if known */
	int		isKeepAlive;	/* positive if server supports
					 * keep-alive connections
					 */
} PXE_HTTP_PARSE_DATA;

#ifdef PXE_HTTPFS_CACHING
/* used in waiting function */
typedef struct pxe_http_wait_data {
	PXE_BUFFER	*buf;		/* buffer, waiting to fill */
	int		socket;		/* socket, which buffer is above */
	uint16_t	wait_size;	/* how much must be filled */
	uint16_t	start_size;	/* how much were in buffer before waiting */
} PXE_HTTP_WAIT_DATA;
#endif

#endif
