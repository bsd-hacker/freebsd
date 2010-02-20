/*-
 * Copyright (c) 2009 Konstantin Belousov
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int chunk_cnt = 1024;
int chunk_size = 1024;

int
main(int argc, char *argv[])
{
	struct iovec *wiov, *riov;
	char **wdata, **rdata;
	int fd, i;
	ssize_t io_error;

	if (argc < 2) {
		fprintf(stderr, "Usage: uio file [chunk count [chunk size]]\n");
		return (2);
	}
	fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		fprintf(stderr, "Failed to create %s: %s\n",
		    argv[1], strerror(errno));
		return (1);
	}

	if (argc > 2)
		chunk_cnt = atoi(argv[2]);
	if (argc > 3)
		chunk_size = atoi(argv[3]);

	wiov = calloc(chunk_cnt, sizeof(*wiov));
	wdata = calloc(chunk_cnt, sizeof(*wdata));

	riov = calloc(chunk_cnt, sizeof(*riov));
	rdata = calloc(chunk_cnt, sizeof(*rdata));

	for (i = 0; i < chunk_cnt; i++) {
		rdata[i] = malloc(chunk_size);
		riov[i].iov_base = rdata[i];
		riov[i].iov_len = chunk_size;

		wdata[i] = malloc(chunk_size);
		memset(wdata[i], i, chunk_size);
		wiov[i].iov_base = wdata[i];
		wiov[i].iov_len = chunk_size;
	}

	io_error = writev(fd, wiov, chunk_cnt);
	if (io_error == -1) {
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		return (1);
	} else if (io_error != chunk_cnt * chunk_size) {
		fprintf(stderr, "truncated write: %d %d\n",
		     io_error, chunk_cnt * chunk_size);
		return (1);
	}

	if (lseek(fd, 0, SEEK_SET) == -1) {
		fprintf(stderr, "lseek failed: %s\n", strerror(errno));
		return (1);
	}

	io_error = readv(fd, riov, chunk_cnt);
	if (io_error == -1) {
		fprintf(stderr, "read failed: %s\n", strerror(errno));
		return (1);
	} else if (io_error != chunk_cnt * chunk_size) {
		fprintf(stderr, "truncated read: %d %d\n",
		     io_error, chunk_cnt * chunk_size);
		return (1);
	}

	for (i = 0; i < chunk_cnt; i++) {
		if (memcmp(rdata[i], wdata[i], chunk_size) != 0) {
			fprintf(stderr, "chunk %d differs\n", i);
			return (1);
		}
	}

	return (0);
}
