/*-
 * Copyright (c) 2011
 *	Hiroki Sato <hrs@FreeBSD.org>  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#include "makevd.h"

int
raw_makeim(struct iminfo *imi)
{
	char buf[BUFSIZ], rawfile[PATH_MAX + 10];
	ssize_t len0, len = 0;
	int ifd, ofd;

	ifd = imi->imi_fd;

	if (strcmp(imi->imi_imagename, "-") == 0)
		ofd = STDOUT_FILENO;
	else {
		snprintf(rawfile, sizeof(rawfile), "%s.raw",
		    imi->imi_imagename);
		ofd = open(rawfile, O_WRONLY|O_CREAT|O_TRUNC,
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (ofd < 0)
			err(EX_CANTCREAT, "%s", rawfile);
	}

	for (;;) {
		len0 = read(ifd, buf, sizeof(buf));
		if (len0 == 0)
			break;
		if (len0 < 0) {
			warn("read error");
			return (1);
		}
		len = write(ofd, buf, len0);
		if (len < 0) {
			warn("write error");
			return (1);
		}
	}

	return (0);
}
