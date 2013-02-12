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

#include <err.h>
#include <fcntl.h>
#include <md5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ndr.h"

void
client(const char *device, const char *saddr, const char *sport,
    const char *daddr, const char *dport)
{
	uintmax_t size, bsize;
	struct stat st;
	int dd, sd;

	/* open device */
	if ((dd = open(device, O_RDONLY)) < 0)
		err(1, "%s", device);
	if (fstat(dd, &st) == -1)
		err(1, "fstat()");
	if (S_ISCHR(st.st_mode)) {
		off_t ot;
		unsigned int ui;

		if (ioctl(dd, DIOCGMEDIASIZE, &ot) == -1)
			err(1, "ioctl(DIOCGMEDIASIZE)");
		size = ot;
		if (ioctl(dd, DIOCGSECTORSIZE, &ui) == -1)
			err(1, "ioctl(DIOCGSECTORSIZE)");
		bsize = ui;
	} else if (S_ISREG(st.st_mode)) {
		size = st.st_size;
		bsize = st.st_blksize;
	} else {
		errx(1, "invalid device type");
	}

	/* connect to server */
	sd = client_socket(saddr, sport, daddr, dport);

	/* send device information */
	sendstrf(sd, "device %s", device);
	sendstrf(sd, "size %016jx %08jx", size, bsize);
	sendstrf(sd, "ready");

	/* process server requests */
	void *buf = NULL;
	size_t buflen = 0;
	char *str = NULL;
	size_t len = 0;
	for (;;) {
		uintmax_t rstart, rstop;
		int verify;

		recvstr(sd, &str, &len);
		if (strcmp(str, "done") == 0) {
			break;
		} else if (sscanf(str, "verify %jx %jx", &rstart, &rstop) == 2) {
			verify = 1;
		} else if (sscanf(str, "read %jx %jx", &rstart, &rstop) == 2) {
			verify = 0;
		} else {
			errx(1, "protocol error");
		}

		if (rstop < rstart)
			errx(1, "protocol error");

		off_t off = rstart * bsize;
		size_t rlen = (rstop - rstart + 1) * bsize;
		if ((uintmax_t)(off + rlen) > size ||
		    lseek(dd, off, SEEK_SET) != off) {
			sendstrf(sd, "failed");
			continue;
		}
		if (buflen < rlen)
			if ((buf = reallocf(buf, rlen)) == NULL)
				err(1, "realloc()");
		status("%s blocks %ju - %ju from %s",
		    verify ? "verifying" : "reading",
		    rstart, rstop, device);
		ssize_t ret = read(dd, buf, rlen);
		if (ret < 0) {
			warn("read()");
			sendstrf(sd, "failed");
			continue;
		}
		uintmax_t dlen = ret;
		uintmax_t dstart = rstart;
		uintmax_t dstop = rstop;
		if (dlen < rlen) {
			warnx("read(): short read");
			dstop = dstart + dlen / bsize - 1;
		}
		if (verify) {
			char md5[33];
			MD5Data(buf, dlen, md5);
			sendstrf(sd, "md5 %016jx %016jx", dstart, dstop);
			sendstrf(sd, "%s", md5);
		} else {
			sendstrf(sd, "data %016jx %016jx", dstart, dstop);
			senddata(sd, buf, (dstop - dstart + 1) * bsize);
		}
	}
	status("done");
	status("");
	exit(0);
}
