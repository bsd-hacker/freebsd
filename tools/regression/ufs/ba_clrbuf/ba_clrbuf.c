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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const int blks = 2;

static void
flush_buffers(int fd)
{
	struct stat st;
	char *addr;
	int error;

	printf("Flushing buffers\n");
	error = fstat(fd, &st);
	if (error == -1)
		err(2, "stat");
	fsync(fd);
	addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == (char *)-1)
		err(2, "mmap");
	error = msync(addr, st.st_size, MS_SYNC | MS_INVALIDATE);
	if (error == -1)
		err(2, "msync");
	munmap(addr, st.st_size);
}

int
main(int argc, char *argv[])
{
	struct statfs fst;
	char *data, *vrfy;
	size_t sz;
	int fd, i, error, ret;

	if (argc < 2)
		errx(2, "Usage: ba_clrbuf file");

	fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1)
		err(2, "Failed to create %s", argv[1]);

	if (fstatfs(fd, &fst) == -1)
		err(2, "stat");

	sz = fst.f_iosize * blks;
	data = malloc(sz);
	if (data == NULL)
		err(2, "malloc");
	vrfy = malloc(sz);
	if (vrfy == NULL)
		err(2, "malloc");
	for (i = 0; i < (int)sz; i++)
		data[i] = i;
	error = write(fd, data, sz);
	if (error == -1)
		err(2, "write");
	else if (error != (int)sz)
		errx(2, "Short write %d %d", error, sz);

	flush_buffers(fd);

	error = lseek(fd, 0, SEEK_SET);
	if (error == -1)
		err(2, "lseek 0");
	else if (error != 0)
		errx(2, "lseek 0 returned %d", error);
	error = write(fd, NULL, fst.f_iosize);
	printf("faulty write, error %s\n", strerror(errno));

	error = lseek(fd, 0, SEEK_SET);
	if (error == -1)
		err(2, "lseek 0/2");
	else if (error != 0)
		errx(2, "lseek 0/2 returned %d", error);
	error = read(fd, vrfy, sz);
	if (error == -1)
		err(2, "read");
	else if (error != (int)sz)
		errx(2, "short read %d %d", error, sz);

	if (memcmp(data, vrfy, fst.f_iosize) != 0) {
		printf("Zero block corrupted, byte at 0 is %x\n",
		    (unsigned char)vrfy[0]);
		ret = 1;
	} else {
		printf("No corruption\n");
		ret = 0;
	}

	return (ret);
}
