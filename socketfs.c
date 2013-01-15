/*-
 * Copyright (c) 2008 Alexey Tarasov
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
#include "pxe_ip.h"
#include "pxe_sock.h"
#include "socketfs.h"

static int	socket_open(const char *path, struct open_file *f);
static int	socket_close(struct open_file *f);
static int	socket_read(struct open_file *f, void *buf, size_t size,
		    size_t *resid);
static int	socket_write(struct open_file *f, void *buf, size_t size,
		    size_t *resid);
static off_t	socket_seek(struct open_file *f, off_t offset, int where);
static int	socket_stat(struct open_file *f, struct stat *sb);

struct fs_ops	socket_fsops = {
	"socketfs",
	socket_open,
	socket_close,
	socket_read,
	socket_write,
	socket_seek,
	socket_stat,
	null_readdir
};

void
handle_cleanup(PXE_SOCKET_HANDLE *socketfile)
{
	if (socketfile == NULL)
		return;
		
	if (socketfile->socket != -1)
		pxe_close(socketfile->socket);
	
	free(socketfile);
}

static int 
socket_open(const char *path, struct open_file *f)
{
#ifdef PXE_HTTP_DEBUG
	printf("socket_open(): %s\n", path);
#endif
	PXE_SOCKET_HANDLE *socketfile =
	    (PXE_SOCKET_HANDLE *)malloc(sizeof(PXE_SOCKET_HANDLE));
	
	if (socketfile == NULL)
		return (ENOMEM);

	pxe_memset(socketfile, 0, sizeof(PXE_SOCKET_HANDLE));

	socketfile->offset = 0;
	socketfile->socket = -1;
	
	/* DUMMY: need to get port, protocol and address from string */
	PXE_IPADDR	*addr = pxe_gethostbyname(path[4]);
	int		port = 80;
	
	if (addr == NULL) {
		handle_cleanup(socketfile);
		return (EINVAL);
	}
	
	socketfile->socket = pxe_socket();
	
	if (socketfile->socket == -1) {
		handle_cleanup(socketfile);
		return (EROFS);
	}
	
	if (-1 == pxe_connect(socketfile->socket, addr, port, PXE_TCP_PROTOCOL)) {
		handle_cleanup(socketfile);
		return (EROFS);
	}
	
        f->f_fsdata = (void *) socketfile;

	return (0);
}

static int 
socket_read(struct open_file *f, void *addr, size_t size, size_t *resid)
{
	PXE_SOCKET_HANDLE *socketfile = (PXE_SOCKET_HANDLE *) f->f_fsdata;
	int		result = -1;
		
	if (socketfile == NULL) {
		printf("socket_read(): NULL file descriptor.\n");
		return (EINVAL);
	}
	
	result = pxe_recv(socketfile->socket, addr, size, PXE_SOCK_BLOCKING);
	
	if (result == -1) {
		printf("socket_read(): failed to read\n");
		return (EINVAL);
	}

	socketfile->offset += result;

	if (resid)
		*resid = size - result;

	return (0);
}

static int 
socket_close(struct open_file *f)
{
	PXE_SOCKET_HANDLE *socketfile = (PXE_SOCKET_HANDLE *) f->f_fsdata;
	handle_cleanup(socketfile);
	
	return (0);
}

static int 
socket_write(struct open_file *f, void *start, size_t size, size_t *resid)
{
	PXE_SOCKET_HANDLE *socketfile = (PXE_SOCKET_HANDLE *) f->f_fsdata;

	if (socketfile == NULL) {
		printf("socket_write(): NULL file descriptor.\n");
		return (EINVAL);
	}

	int result = pxe_send(socketfile->socket, start, size);

	if (result == -1) {
		printf("socket_write(): failed to write\n");
		return (EINVAL);
	}
	
	if (resid)
		*resid = size - result;
		
	return (0);
}

static int 
socket_stat(struct open_file *f, struct stat *sb)
{
	PXE_SOCKET_HANDLE *socketfile = (PXE_SOCKET_HANDLE *) f->f_fsdata;
	
	sb->st_mode = 0666 | S_IFREG;
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;

	sb->st_size = -1;
	
	return (0);
}

static off_t 
socket_seek(struct open_file *f, off_t offset, int where)
{
	errno = EOFFSET;

	return (-1);
}
