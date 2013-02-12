/*-
 * Copyright (c) 2007 Dag-Erling Smørgrav
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ndr.h"

static const char	*a_arg;
static int		 c_flag;
static const char	*d_arg;
static const char	*p_arg;
static int		 s_flag;
static int		 v_count;

static void
usage(void)
{

	fprintf(stderr,
	    "usage: ndr [-v] [-d dir] [-a addr] [-p port] -s\n"
	    "       ndr [-v] -c <device> <server> [<port>]\n");
	exit(1);
}

void
verbose(int level, const char *fmt, ...)
{
	va_list ap;

	if (v_count >= level) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

void
status(const char *fmt, ...)
{
	static struct timeval prev;
	struct timeval now;
	static int olen = 0;
	va_list ap;
	int len;

	if (!isatty(STDERR_FILENO) || v_count > 1)
		return;

	if (fmt[0] == '\0') {
		/* cleanup requested */
		fputc('\n', stderr);
		return;
	}

	gettimeofday(&now, NULL);
	if (now.tv_sec == prev.tv_sec)
		return;

	prev = now;
	fputc('\r', stderr);
	va_start(ap, fmt);
	len = vfprintf(stderr, fmt, ap);
	va_end(ap);

	for (int i = len; i < olen; ++i)
		fputc(' ', stderr);
	olen = len;
}

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "a:cd:p:sv")) != -1)
		switch (opt) {
		case 'a':
			a_arg = optarg;
			break;
		case 'c':
			c_flag = 1;
			break;
		case 'd':
			d_arg = optarg;
			break;
		case 'p':
			p_arg = optarg;
			break;
		case 's':
			s_flag = 1;
			break;
		case 'v':
			v_count++;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (!(c_flag ^ s_flag))
		usage();

	if (c_flag) {
		if (d_arg || argc < 2 || argc > 3)
			usage();
		client(argv[0], a_arg, p_arg, argv[1], argv[2]);
	} else if (s_flag) {
		if (argc > 0)
			usage();
		if (d_arg && chdir(d_arg) != 0)
			err(1, "chdir(%s)", d_arg);
		server(a_arg, p_arg);
	} else {
		usage();
	}
	errx(1, "not reached");
}
