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
 
#include <sys/types.h>
#include <stand.h>

#include "pxe_core.h"
#include "pxe_http.h"
#include "pxe_ip.h"

static int	http_open(const char *path, struct open_file *f);
static int	http_close(struct open_file *f);
static int	http_read(struct open_file *f, void *buf, size_t size,
		    size_t *resid);
static int	http_write(struct open_file *f, void *buf, size_t size,
		    size_t *resid);
static off_t	http_seek(struct open_file *f, off_t offset, int where);
static int	http_stat(struct open_file *f, struct stat *sb);

struct fs_ops	http_fsops = {
	"httpfs",
	http_open,
	http_close,
	http_read,
	http_write,
	http_seek,
	http_stat,
	null_readdir
};

/* http server name. It is set if rootpath option was in DHCP reply */
char		servername[256] = {0};

void
handle_cleanup(PXE_HTTP_HANDLE *httpfile)
{
	if (httpfile == NULL)
		return;
		
	if (httpfile->buf != NULL)
		free(httpfile->buf);

	if (httpfile->filename != NULL)
		free(httpfile->filename);

	if ( (httpfile->servername != NULL) &&
	     (!servername[0]) )
		free(httpfile->servername);

	if (httpfile->socket != -1)
		pxe_close(httpfile->socket);
	
	free(httpfile);
}

static int 
http_open(const char *path, struct open_file *f)
{
#ifdef PXE_HTTP_DEBUG
	printf("http_open(): %s\n", path);
#endif
	PXE_HTTP_HANDLE *httpfile =
	    (PXE_HTTP_HANDLE *)malloc(sizeof(PXE_HTTP_HANDLE));
	
	if (httpfile == NULL)
		return (ENOMEM);

	pxe_memset(httpfile, 0, sizeof(PXE_HTTP_HANDLE));
								

	httpfile->offset = 0;
	httpfile->socket = -1;
	
        httpfile->filename = strdup(path);
	
	if (httpfile->filename == NULL) {
		handle_cleanup(httpfile);
                return (ENOMEM);
        }

	if (servername[0]) {
		httpfile->servername = servername;
	} else {
		httpfile->servername = strdup(inet_ntoa(httpfile->addr.ip));
	}

	pxe_memcpy(pxe_get_ip(PXE_IP_WWW), &httpfile->addr, sizeof(PXE_IPADDR));

#ifdef PXE_DEBUG_HELL	
	printf("servername: %s\n", httpfile->servername);
#endif
	if (httpfile->servername == NULL) {
		handle_cleanup(httpfile);
	}

	httpfile->buf = malloc(PXE_MAX_HTTP_HDRLEN);
	
	if (httpfile->buf == NULL) {
		handle_cleanup(httpfile);
		return (ENOMEM);
	}
	
	httpfile->bufsize = PXE_MAX_HTTP_HDRLEN;
	
        if (!pxe_exists(httpfile)) {
		handle_cleanup(httpfile);
                return (EEXIST);
        }

        f->f_fsdata = (void *) httpfile;

#ifdef PXE_HTTP_DEBUG
	printf("http_open(): %s opened\n", httpfile->filename);
#endif

	return (0);
}

static int 
http_read(struct open_file *f, void *addr, size_t size, size_t *resid)
{
	PXE_HTTP_HANDLE *httpfile = (PXE_HTTP_HANDLE *) f->f_fsdata;
	int		result = -1;
		
	if (httpfile == NULL) {
		printf("http_read(): NULL file descriptor.\n");
		return (EINVAL);
	}
	
#ifdef PXE_HTTP_DEBUG_HELL
	printf("http_read(): %s:%llu+%lu\n",
	    httpfile->filename, httpfile->offset, size);
#endif
	if (((httpfile->size != PXE_HTTP_SIZE_UNKNOWN) &&
	     (httpfile->offset >= httpfile->size)) ||
	     (size == 0))
	{
	    if (resid)
		*resid = size;
#ifdef PXE_HTTP_DEBUG_HELL
	    printf("http_read(): EOF\n");
#endif
	    return (0);
	}
	
	size_t to_read = (httpfile->offset + size < httpfile->size) ?
			    size: httpfile->size - (size_t)httpfile->offset;

#ifndef PXE_HTTPFS_CACHING
	result = pxe_get(httpfile, to_read, addr);
#else
	void	*addr2 = addr;
	int	part1 = -1;
	
	if (httpfile->cache_size < to_read) {

		/* read all we have in buffer */
		if (httpfile->cache_size != 0) {
			part1 = pxe_recv(httpfile->socket, addr2,
				    httpfile->cache_size);
#ifdef PXE_HTTP_DEBUG_HELL			
			printf("http_read(): cache > %ld/%lu/%lu/%u bytes\n",
			    part1, to_read, size, httpfile->cache_size);
#endif
		}

		if (part1 != -1) {
			to_read -= part1;
			addr2 += part1;
			httpfile->cache_size -= part1;
		}
		
		/* update cache */
		if (httpfile->socket != -1) {
			PXE_BUFFER *buf =
				    pxe_sock_recv_buffer(httpfile->socket);
			
			size_t to_get = httpfile->size - httpfile->offset -
					((part1 != -1) ? part1 : 0 );
			
			if (to_get > buf->bufsize / 2)
				 to_get = buf->bufsize / 2;
#ifdef PXE_HTTP_DEBUG_HELL
			printf("http_read(): cache < %lu bytes\n", to_get);
#endif				
			pxe_get(httpfile, to_get, NULL);
		}
	}
	
	/* try reading of cache */
	if (httpfile->cache_size < to_read) {
		printf("http_read(): read of cache failed\n");
		return (EINVAL);
	}
	
	result = pxe_recv(httpfile->socket, addr2, to_read);
#ifdef PXE_HTTP_DEBUG_HELL
	printf("http_read(): cache > %ld/%lu/%lu/%u bytes\n",
	    result, to_read, size, httpfile->cache_size);
#endif	
	if (result != -1) {
		httpfile->cache_size -= to_read;
		result += (part1 != -1) ? part1 : 0;
	} else 
		result = part1;

#endif
	if (result == -1) {
		printf("http_read(): failed to read\n");
		return (EINVAL);
	}

	httpfile->offset += result;

/* #ifdef PXE_HTTP_DEBUG */
	if (httpfile->size != PXE_HTTP_SIZE_UNKNOWN)
		printf("%3llu%%\b\b\b\b",
		    100LL * httpfile->offset / httpfile->size);
	else
		printf("http_read(): %llu byte(s) read\n", httpfile->offset);
/* #endif */
	if (resid)
		*resid = size - result;

	return (0);
}

static int 
http_close(struct open_file *f)
{
	PXE_HTTP_HANDLE *httpfile = (PXE_HTTP_HANDLE *) f->f_fsdata;

#ifdef PXE_HTTP_DEBUG
	printf("http_close(): closing file %s\n", httpfile->filename);
#endif
	handle_cleanup(httpfile);
	
	return (0);
}

static int 
http_write(struct open_file *f,	void *start, size_t size, size_t *resid)
{
	/* cannot write */
	return (EROFS);
}

static int 
http_stat(struct open_file *f, struct stat *sb)
{
	PXE_HTTP_HANDLE *httpfile = (PXE_HTTP_HANDLE *) f->f_fsdata;
	
#ifdef PXE_HTTP_DEBUG
	printf("http_stat(): stat for file %s\n", httpfile->filename);
#endif
	sb->st_mode = 0444 | S_IFREG;
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;

	sb->st_size = (httpfile != NULL) ? httpfile->size : -1;
	
	return (0);
}

static off_t 
http_seek(struct open_file *f, off_t offset, int where)
{
	PXE_HTTP_HANDLE *httpfile = (PXE_HTTP_HANDLE *) f->f_fsdata;
	
#ifdef PXE_HTTP_DEBUG
	printf("http_seek(): file 0x%x\n", httpfile);
#endif

	if (httpfile == NULL) {	/* to be sure */
		errno = EINVAL;
		return (-1);
	}
	
	switch (where) {
	case SEEK_SET:
		httpfile->offset = offset;
		break;
		
	case SEEK_CUR:
		httpfile->offset += offset;
		break;
		
	default:
		errno = EOFFSET;
		return (-1);
	}
#ifdef PXE_HTTPFS_CACHING
	/* if we seeked somewhere, cache failed, need clean it */
	pxe_recv(httpfile->socket, httpfile->cache_size, NULL);
	httpfile->cache_size = 0;
#endif
	return (httpfile->offset);
}
