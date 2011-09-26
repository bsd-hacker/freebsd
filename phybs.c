/*-
 * Copyright (c) 2010-2011 Dag-Erling Coïdan Smørgrav
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/disk.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static const char *device;

static unsigned int bsize;
static unsigned int minsize;
static unsigned int maxsize;
static unsigned int total;

static int opt_r;
static int opt_s;
static int opt_w;

static int tty = 0;
static char progress[] = " [----------------]"
    "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";

static void
scan(int fd, size_t size, off_t offset, off_t step, unsigned int count)
{
	struct timeval t0, t1;
	unsigned long usec;
	ssize_t rlen, wlen;
	char *buf;

	printf("%8u%8lu%8lu%8lu  ", count, (unsigned long)size,
	    (unsigned long)offset, (unsigned long)step);
	fflush(stdout);
	if ((buf = malloc(size)) == NULL)
		err(EX_OSERR, "malloc()");
	memset(buf, 0, size);
	if (gettimeofday(&t0, NULL) == -1)
		err(EX_OSERR, "gettimeofday()");
	for (unsigned int i = 0; i < count; ++i, offset += step) {
		if (opt_r) {
			if (lseek(fd, offset, SEEK_SET) != offset)
				err(EX_IOERR, "lseek(%lu)", (unsigned long)offset);
			if ((rlen = read(fd, buf, size)) == -1)
				err(EX_IOERR, "read(%lu)", (unsigned long)size);
			if (rlen < (ssize_t)size)
				errx(EX_IOERR, "short read: %ld < %lu",
				    (long)rlen, (unsigned long)size);
		}
		if (opt_w) {
			if (lseek(fd, offset, SEEK_SET) != offset)
				err(EX_IOERR, "lseek(%lu)", (unsigned long)offset);
			if ((wlen = write(fd, buf, size)) == -1)
				err(EX_IOERR, "write(%lu)", (unsigned long)size);
			if (wlen < (ssize_t)size)
				errx(EX_IOERR, "short write: %ld < %lu",
				    (long)wlen, (unsigned long)size);
		}
		if (tty && i % 256 == 0) {
			progress[2 + (i * 16) / count] = '|';
			fputs(progress, stdout);
			progress[2 + (i * 16) / count] = '-';
			fflush(stdout);
		}
	}
	if (gettimeofday(&t1, NULL) == -1)
		err(EX_OSERR, "gettimeofday()");
	usec = t1.tv_sec * 1000000 + t1.tv_usec;
	usec -= t0.tv_sec * 1000000 + t0.tv_usec;
	printf("%10lu%8ju%8ju\n", usec / 1000,
	    (uintmax_t)count * 1000000 / usec,
	    (uintmax_t)count * size * 1000000 / 1024 / usec);
	free(buf);
}

static void
usage(void)
{

	fprintf(stderr, "usage: phybs %s\n",
	    "[-rsw] [-l minsize] [-h maxsize] [-t total] device");
	exit(1);
}

static unsigned int
poweroftwo(char opt, const char *valstr)
{
	uint64_t val;

	if (expand_number(valstr, &val) != 0) {
		fprintf(stderr, "-%c: invalid number\n", opt);
		usage();
	}
	if ((val & (val - 1)) != 0) {
		fprintf(stderr, "-%c: not a power of two\n", opt);
		usage();
	}
	if (val == 0 || val > UINT_MAX) {
		fprintf(stderr, "-%c: out of range\n", opt);
		usage();
	}
	return (val);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	int fd, mode, opt;

	tty = isatty(STDOUT_FILENO);

	while ((opt = getopt(argc, argv, "h:l:rst:w")) != -1)
		switch (opt) {
		case 'h':
			maxsize = poweroftwo(opt, optarg);
			break;
		case 'l':
			minsize = poweroftwo(opt, optarg);
			break;
		case 'r':
			opt_r = 1;
			break;
		case 's':
			opt_s = 1;
			break;
		case 't':
			total = poweroftwo(opt, optarg);
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
	device = argv[0];

	if (!opt_r && !opt_w)
		opt_r = 1;

	mode = opt_w ? O_RDWR : O_RDONLY;
	if (opt_s)
		mode |= O_SYNC;
	if ((fd = open(device, mode)) == -1)
		err(errno == EPERM ? EX_NOPERM : EX_OSERR, "open(%s)", device);

	if (fstat(fd, &st) != 0)
		err(EX_OSERR, "stat(%s)", device);
	bsize = 512;
	if (S_ISCHR(st.st_mode) && ioctl(fd, DIOCGSECTORSIZE, &bsize) == -1)
		err(EX_OSERR, "ioctl(%s, DIOCGSECTORSIZE)", device);

	if (minsize == 0)
		minsize = bsize;
	if (minsize % bsize != 0)
		errx(EX_USAGE, "minsize (%u) is not a multiple of block size (%u)",
		    minsize, bsize);

	if (maxsize == 0)
		maxsize = minsize * 8;
	if (maxsize % minsize != 0)
		errx(EX_USAGE, "maxsize (%u) is not a multiple of minsize (%u)",
		    maxsize, minsize);

	if (total == 0)
		total = 128 * 1024 * 1024;
	if (total % maxsize != 0)
		errx(EX_USAGE, "total (%u) is not a multiple of maxsize (%u)",
		    total, maxsize);

	printf("%8s%8s%8s%8s%12s%8s%8s\n",
	    "count", "size", "offset", "step",
	    "msec", "tps", "kBps");

	for (size_t size = minsize; size <= maxsize; size *= 2) {
		printf("\n");
		scan(fd, size, 0, size * 4, total / size);
		for (off_t offset = bsize; offset < (off_t)size; offset *= 2)
			scan(fd, size, offset, size * 4, total / size);
	}

	close(fd);
	exit(EX_OK);
}
