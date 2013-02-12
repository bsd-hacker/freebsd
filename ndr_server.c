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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <errno.h>
#include <md5.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ndr.h"

struct client {
	pid_t		 pid;		/* handler's PID */

	int		 sd;		/* socket descriptor */
	char		*host;		/* host name */
	char		*port;		/* port number */
	char		*device;	/* device name */

	char		*wpath;		/* working directory */
	char		*dpath;		/* data file */
	int		 dd;		/* data file descriptor */
	char		*mpath;		/* map file */
	int		 md;		/* map file descriptor */
	char		*map;		/* map file pointer */

	uintmax_t	 size;		/* device size */
	uintmax_t	 bsize;		/* device block size */
	uintmax_t	 blocks;	/* # blocks total (size / bsize) */
	uintmax_t	 failed;	/* # blocks failed */
	uintmax_t	 received;	/* # blocks successfully recovered */
	uintmax_t	 retry;		/* # blocks needing retry */

	void		*buf;		/* buffer for receiving data */
	size_t		 bufsz;		/* current size of buffer */
	size_t		 buflen;	/* current length of buffer contents */

	char		*str;		/* buffer for receiving strings */
	size_t		 strsz;		/* current size of string buffer */
};

static volatile sig_atomic_t interrupted;

static void
interrupt(int sig)
{

	(void)sig;
	interrupted = 1;
}

static void
reaper(int sig)
{
	int ret;

	(void)sig;
	if (waitpid(0, &ret, WNOHANG) > 0 && WIFSIGNALED(ret))
		raise(WTERMSIG(ret));
}

static void
init_paths(struct client *cp)
{
	char path[PATH_MAX], rpath[PATH_MAX];
	char *p, *q, t;

	/* XXX overflow */
	snprintf(path, sizeof path, "%s/%s", cp->host, cp->device);

	/* create directory if necessary */
	for (p = q = path; *p != '\0'; p = q) {
		if ((q = strchr(p, '/')) == NULL)
			q = strchr(p, '\0');
		if ((t = *q) != '\0')
			*q = '\0';
		if (mkdir(path, 0755) != 0 && errno != EEXIST)
			err(1, "mkdir(%s)", path);
		if (t == '\0')
			break;
		*q++ = t;
	}

	/* resolve paths */
	realpath(path, rpath);
	if ((cp->wpath = strdup(rpath)) == NULL)
		err(1, "strdup()");
	if ((asprintf(&cp->dpath, "%s/data", cp->wpath)) < 0 ||
	    (asprintf(&cp->mpath, "%s/map", cp->wpath)) < 0)
		err(1, "asprintf()");
}

static void
init_files(struct client *cp)
{
	struct stat st;
	int new = 0;

	/* initialize data and map */
	if ((cp->dd = open(cp->dpath, O_RDWR|O_CREAT, 0600)) < 0)
		err(1, "%s", cp->dpath);
	if (fstat(cp->dd, &st) != 0)
		err(1, "fstat(%s)", cp->dpath);
	if (st.st_size != (off_t)cp->size)
		new = 1;
	if ((cp->md = open(cp->mpath, O_RDWR|O_CREAT, 0600)) < 0)
		err(1, "%s", cp->mpath);
	if (fstat(cp->md, &st) != 0)
		err(1, "fstat(%s)", cp->mpath);
	if (st.st_size != (off_t)cp->blocks)
		new = 1;
	if (new) {
		/* need reinitializing / clearing */
		if (ftruncate(cp->dd, cp->size) != 0)
			err(1, "ftruncate(%s)", cp->dpath);
		if (ftruncate(cp->md, cp->blocks) != 0)
			err(1, "ftruncate(%s)", cp->mpath);
	}
	if ((cp->map = mmap(NULL, cp->blocks, PROT_READ|PROT_WRITE,
	    MAP_SHARED, cp->md, 0)) == NULL)
		err(1, "mmap()");
	if (new) {
		cp->retry = cp->failed = cp->received = 0;
		status("%s:%s initializing block map", cp->host, cp->device);
		memset(cp->map, ' ', cp->blocks);
	} else {
		cp->retry = cp->failed = cp->received = 0;
		for (uintmax_t i = 0; !interrupted && i < cp->blocks; ++i) {
			if (cp->map[i] == '#')
				++cp->received;
			else if (cp->map[i] == '*')
				++cp->failed;
			else if (isdigit((int)cp->map[i]))
				++cp->retry;
			if (i % 0x1000000 == 0xffffff)
				status("%s:%s scanning block map: %.0f%%",
				    cp->host, cp->device,
				    100.0 * i / cp->blocks);
		}
	}
}

static void
client_status(struct client *cp)
{
	uintmax_t pct;

	/* using fixed point gives us a "free" rounding down */
	pct = 100000 * (cp->received + cp->failed) / cp->blocks;
	status("%s:%s [rcvd %10ju fail %5ju retr %5ju] %3ju.%03ju%%",
	    cp->host, cp->device, cp->received, cp->failed, cp->retry,
	    pct / 1000, pct % 1000);
}

static void
request(struct client *cp, uintmax_t rstart, uintmax_t rstop)
{
	uintmax_t dstart, dstop;
	off_t off;

	sendstrf(cp->sd, "read %016jx %016jx", rstart, rstop);
	recvstr(cp->sd, &cp->str, &cp->strsz);
	if (strcmp(cp->str, "failed") == 0)
		return;
	else if (sscanf(cp->str, "data %jx %jx", &dstart, &dstop) != 2 ||
	    dstop < dstart)
		errx(1, "protocol error");
	if (dstart < rstart || dstart > rstop ||
	    dstop < rstart || dstop > rstop)
		errx(1, "didn't get what we asked for");
	recvdata(cp->sd, &cp->buf, &cp->bufsz, &cp->buflen);
	if (cp->buflen != (dstop - dstart + 1) * cp->bsize)
		errx(1, "didn't get what we were told to expect");
	off = dstart * cp->bsize;
	if (lseek(cp->dd, off, SEEK_SET) != off)
		err(1, "lseek()");
	if (write(cp->dd, cp->buf, cp->buflen) != (ssize_t)cp->buflen)
		err(1, "write()");
	fsync(cp->dd);
	for (unsigned int i = dstart; i <= dstop; ++i) {
		cp->map[i] = '#';
		++cp->received;
		--cp->retry;
	}
	fsync(cp->md);
	client_status(cp);
}

static int
verify(struct client *cp, uintmax_t rstart, uintmax_t rstop)
{
	uintmax_t dstart, dstop;
	off_t off;
	size_t len;
	char md5[33];

	sendstrf(cp->sd, "verify %016jx %016jx", rstart, rstop);
	recvstr(cp->sd, &cp->str, &cp->strsz);
	if (strcmp(cp->str, "failed") == 0)
		return (-1);
	else if (sscanf(cp->str, "md5 %jx %jx", &dstart, &dstop) != 2 ||
	    dstop < dstart)
		errx(1, "protocol error");
	if (dstart < rstart || dstart > rstop ||
	    dstop < rstart || dstop > rstop)
		errx(1, "didn't get what we asked for");
	off = dstart * cp->bsize;
	len = (dstop - dstart + 1) * cp->bsize;
	if (cp->bufsz < len)
		if ((cp->buf = reallocf(cp->buf, cp->bufsz = len)) == NULL)
			err(1, "realloc()");
	read_full(cp->dd, cp->buf, len);
	MD5Data(cp->buf, len, md5);
	recvstr(cp->sd, &cp->str, &cp->strsz);
	if (strlen(cp->str) != 32 ||
	    strspn(cp->str, "0123456789ABCDEFabcdef") != 32)
		errx(1, "protocol error");
	if (strcmp(cp->str, md5) != 0)
		return (-1);
	return (0);
}

static void
recover(struct client *cp, int level)
{
	uintmax_t rstart, rstop;
	unsigned int max;
	char ch1, ch2;

	if (level < 0 || level > 9)
		errx(1, "can't happen");
	client_status(cp);
	rstart = rstop = 0;
	max = 1 << (10 - level);
	ch1 = level > 0 ? '0' + level : ' ';
	ch2 = level < 9 ? '1' + level : '*';
	while (!interrupted && rstart < cp->blocks) {
		/* skip to next untried block */
		while (rstart < cp->blocks && cp->map[rstart] != ch1)
			++rstart;

		/* grab up to 2^(10-level) consecutive untried blocks */
		rstop = rstart;
		for (unsigned int i = 0; i < max && rstop < cp->blocks; ++i) {
			if (cp->map[rstop] == ch1) {
				cp->map[rstop] = ch2;
				++cp->retry;
				++rstop;
			}
		}
		if (--rstop < rstart)
			continue;
		fsync(cp->md);
		request(cp, rstart, rstop);
	}
}

static void
handle_client(struct client *cp)
{
	verbose(1, "handling connection from %s:%s\n", cp->host, cp->port);

	/* receive signature and device name and size */
	for (;;) {
		char *p, *q;

		p = recvstr(cp->sd, &cp->str, &cp->strsz);
		if ((q = strsep(&p, " ")) == NULL)
			errx(1, "protocol error");
		if (strcmp(q, "ready") == 0) {
			break;
		} else if (strcmp(q, "device") == 0 && p != NULL) {
			cp->device = strdup(strsep(&p, " "));
		} else if (strcmp(q, "size") == 0) {
			if (sscanf(p, "%jx %jx", &cp->size, &cp->bsize) != 2)
				errx(1, "protocol error");
		} else {
			errx(1, "protocol error");
		}
	}

	if (cp->device == NULL || cp->size == 0 || cp->bsize == 0)
		errx(1, "protocol error");

	if (cp->size % cp->bsize) {
		fprintf(stderr, "WARNING: device size "
		    "truncated to nearest multiple of block size\n");
		cp->size -= (cp->size % cp->bsize);
	}
	cp->blocks = cp->size / cp->bsize;
	verbose(1, "%s:%s %ju bytes in %ju %ju-byte blocks\n",
	    cp->host, cp->device, cp->size, cp->blocks, cp->bsize);

	init_paths(cp);
	init_files(cp);

	for (int i = 0; !interrupted && i < 10; ++i)
		if (cp->received + cp->failed < cp->blocks)
			recover(cp, i);

	/* verify blocks which were successfully read */
	uintmax_t rstart = 0, rstop = 0;
	while (!interrupted && rstart < cp->blocks) {
		while (rstart < cp->blocks && cp->map[rstart] != '#')
			rstart++;
		rstop = rstart;
		for (uintmax_t i = 0; i < (1 << 10) && rstop < cp->blocks; ++i)
			if (cp->map[rstop] == '#')
				++rstop;
		if (--rstop < rstart)
			continue;
		if (verify(cp, rstart, rstop) != 0) {
			for (uintmax_t i = rstart; i < rstop; ++i) {
				cp->map[i] = '1';
				--cp->received;
			}
			fsync(cp->md);
		}
		uintmax_t pct = 100000 * rstop / cp->blocks;
		status("%s:%s verifying %d.03d%%", cp->host, cp->device,
		    pct / 1000, pct % 1000);
		rstart = rstop + 1;
	}

	if (interrupted) {
		/* triggers a protocol error in the client */
		sendstrf(cp->sd, "interrupted");
	} else {
		sendstrf(cp->sd, "done");
		if (cp->failed)
			status("%s:%s done (%jx blocks failed)",
			    cp->host, cp->device, cp->failed);
		else
			status("%s:%s done", cp->host, cp->device);
		status("");
	}

	close(cp->md);
	close(cp->dd);
	exit(0);
}

void
server(const char *laddr, const char *lport)
{
	struct sigaction sa;
	int sd;

	/* catch interrupts */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = interrupt;
	sa.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* reap children */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = reaper;
	sa.sa_flags = SA_NODEFER;
	sigaction(SIGCHLD, &sa, NULL);

	/* open listening socket */
	sd = server_socket(laddr, lport);

	/* main loop */
	while (!interrupted) {
		struct client *cp;

		if ((cp = calloc(1, sizeof *cp)) == NULL)
			err(1, "calloc()");
		if ((cp->sd = accept_socket(sd, &cp->host, &cp->port)) < 0) {
			free(cp);
			continue;
		}

#if 1
		/* fork off handler */
		if ((cp->pid = fork()) == -1) {
			/* failed */
			err(1, "fork()");
		} else if (cp->pid == 0) {
			/* child */
			close(sd);
			cp->pid = getpid();
#endif
			handle_client(cp);
#if 1
			errx(1, "not reached");
		}
		/* parent */
		verbose(1, "handed off to child %d\n", (int)cp->pid);
#endif
		free(cp->host);
		free(cp->port);
		close(cp->sd);
		free(cp);
	}
	errx(1, "interrupted");
}
