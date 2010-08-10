/*-
 * Copyright (c) 2010 Dag-Erling Coïdan Smørgrav
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

#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BSIZE		512
#define MINSIZE		1024
#define MAXSIZE		8192
#define STEP		(MAXSIZE * 4)
#define COUNT		65536

static int opt_r = 0;
static int opt_w = 1;

static void
scan(int fd, size_t size, off_t offset, off_t step, unsigned int count)
{
	struct timeval t0, t1;
	unsigned long usec;
	ssize_t rlen, wlen;
	char *buf;

	printf("%8u%8lu%8lu%8lu", count, (unsigned long)size,
	    (unsigned long)offset, (unsigned long)step);
	fflush(stdout);
	if ((buf = malloc(size)) == NULL)
		err(1, "malloc()");
	memset(buf, 0, size);
	if (gettimeofday(&t0, NULL) == -1)
		err(1, "gettimeofday()");
	for (unsigned int i = 0; i < count; ++i, offset += step) {
		if (opt_r) {
			if (lseek(fd, offset, SEEK_SET) != offset)
				err(1, "lseek(%lu)", (unsigned long)offset);
			if ((rlen = read(fd, buf, size)) == -1)
				err(1, "read(%lu)", (unsigned long)size);
			if (rlen < (ssize_t)size)
				errx(1, "short read: %ld < %lu",
				    (long)rlen, (unsigned long)size);
		}
		if (opt_w) {
			if (lseek(fd, offset, SEEK_SET) != offset)
				err(1, "lseek(%lu)", (unsigned long)offset);
			if ((wlen = write(fd, buf, size)) == -1)
				err(1, "write(%lu)", (unsigned long)size);
			if (wlen < (ssize_t)size)
				errx(1, "short write: %ld < %lu",
				    (long)wlen, (unsigned long)size);
		}
	}
	if (gettimeofday(&t1, NULL) == -1)
		err(1, "gettimeofday()");
	usec = t1.tv_sec * 1000000 + t1.tv_usec;
	usec -= t0.tv_sec * 1000000 + t0.tv_usec;
	printf("%12lu%8lu%8lu\n", usec / 1000,
	    count * 1000000 / usec,
	    count * size * 1000000 / 1024 / usec);
	free(buf);
}

static void
usage(void)
{

	fprintf(stderr, "usage: phybs [-R | -r] [-W | -w] device\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *device;
	int fd, opt;

	while ((opt = getopt(argc, argv, "RrWw")) != -1)
		switch (opt) {
		case 'R':
			opt_r = 0;
			break;
		case 'r':
			opt_r = 1;
			break;
		case 'W':
			opt_w = 0;
			break;
		case 'w':
			opt_w = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (!opt_r && !opt_w)
		opt_r = opt_w = 1;

	device = argv[0];
	if ((fd = open(device, opt_w ? O_RDWR : O_RDONLY)) == -1)
		err(1, "open(%s)", device);
	printf("%8s%8s%8s%8s%12s%8s%8s\n",
	    "count", "size", "offset", "step",
	    "msec", "tps", "kBps");
	for (size_t size = MINSIZE; size <= MAXSIZE; size *= 2) {
		printf("\n");
		scan(fd, size, 0, STEP, COUNT);
		for (off_t offset = BSIZE; offset <= (off_t)size; offset *= 2)
			scan(fd, size, offset, STEP, COUNT);
	}
	close(fd);
	exit(0);
}
