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

#include <sys/uio.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ndr.h"

static const char	*ndr_default_port = "9110";
static const char	*ndr_client_sig = "ndr client\n";
static const char	*ndr_server_sig = "ndr server\n";

static int
handshake(int sd, const char *mine, const char *theirs)
{
	ssize_t mlen = strlen(mine);
	ssize_t tlen = strlen(theirs);
	char buf[tlen];

	if (write(sd, mine, mlen) != mlen ||
	    read(sd, buf, tlen) != tlen ||
	    strncmp(buf, theirs, tlen) != 0)
		return (-1);
	return (0);
}

int
client_socket(const char *saddr, const char *sport,
    const char *daddr, const char *dport)
{
	struct addrinfo hints, *sres, *dres;
	int one, ret, sd;

	/* resolve server address */
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = 0;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	dres = NULL;
	if (dport == NULL)
		dport = ndr_default_port;
	if ((ret = getaddrinfo(daddr, dport, &hints, &dres)) != 0)
		errx(1, "getaddrinfo(): %s", gai_strerror(ret));

	/* open socket */
	if ((sd = socket(dres->ai_family, dres->ai_socktype, dres->ai_protocol)) < 0)
		err(1, "socket()");

	/* resolve source address if given */
	if (saddr != NULL || sport != NULL) {
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = dres->ai_family;
		hints.ai_socktype = dres->ai_socktype;
		hints.ai_protocol = dres->ai_protocol;
		sres = NULL;
		if ((ret = getaddrinfo(saddr, sport, &hints, &sres)) != 0)
			errx(1, "getaddrinfo(): %s", gai_strerror(ret));
		one = 1;
		if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) != 0)
			err(1, "setsockopt(SO_REUSEADDR)");
		if (bind(sd, sres->ai_addr, sres->ai_addrlen) != 0)
			err(1, "connect()");
		freeaddrinfo(sres);
	}

	/* connect to server */
	if (connect(sd, dres->ai_addr, dres->ai_addrlen) != 0)
		err(1, "connect()");
	freeaddrinfo(dres);

	verbose(1, "connected to server %s:%s\n", daddr, dport);

	/* handshake */
	if (handshake(sd, ndr_client_sig, ndr_server_sig) != 0)
		errx(1, "handshake failed");

	return (sd);
}

int
server_socket(const char *laddr, const char *lport)
{
	struct addrinfo hints, *lres;
	int one, ret, sd;

	/* resolve listening address */
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	lres = NULL;
	if (lport == NULL)
		lport = ndr_default_port;
	if ((ret = getaddrinfo(laddr, lport, &hints, &lres)) != 0)
		errx(1, "getaddrinfo(): %s", gai_strerror(ret));

	/* open listening socket */
	if ((sd = socket(lres->ai_family, lres->ai_socktype, lres->ai_protocol)) < 0)
		err(1, "socket()");
	one = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one) != 0)
		err(1, "setsockopt(SO_REUSEPORT)");
	if (bind(sd, lres->ai_addr, lres->ai_addrlen) != 0)
		err(1, "bind()");
	if (listen(sd, 16) != 0)
		err(1, "listen()");
	freeaddrinfo(lres);

	verbose(1, "server listening on %s:%s\n", laddr ? laddr : "*", lport);

	return (sd);
}

int
accept_socket(int sd, char **saddr, char **sport)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int ad, ret;

	memset(&addr, 0, addrlen = sizeof addr);
	if ((ad = accept(sd, (struct sockaddr *)&addr, &addrlen)) < 0)
		return (-1);

	if ((*saddr = malloc(NI_MAXHOST)) == NULL ||
	    (*sport = malloc(NI_MAXSERV)) == NULL)
		err(1, "malloc()");

	/* resolve client address */
	if ((ret = getnameinfo((struct sockaddr *)&addr, addrlen,
	    *saddr, NI_MAXHOST, *sport, NI_MAXSERV,
	    NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
		/* shouldn't happen */
		err(1, "getnameinfo(): %s", gai_strerror(ret));

	verbose(1, "accepted connection from %s:%s\n", *saddr, *sport);

	if (handshake(ad, ndr_server_sig, ndr_client_sig) != 0) {
		verbose(1, "handshake failed");
		close(ad);
		return (-1);
	}

	return (ad);
}

void
sendstr(int sd, const char *str, size_t len)
{
	struct iovec iov[3];
	uint32_t count;
	char s = 's';
	ssize_t res, ret;

	if (len == 0)
		len = strlen(str);
	count = len;
	verbose(2, ">>> s %08x %s\n", count, str);
	count = htonl(count);
	res = 0;
	iov[0].iov_base = &s;
	res += (iov[0].iov_len = 1);
	iov[1].iov_base = &count;
	res += (iov[1].iov_len = sizeof count);
	iov[2].iov_base = (void *)(uintptr_t)str;
	res += (iov[2].iov_len = len);
	if ((ret = writev(sd, iov, 3)) != res) {
		if (ret == -1)
			err(1, "writev()");
		errx(1, "writev(): short write (%zd / %zd)", ret, res);
	}
}

void
sendstrf(int sd, const char *fmt, ...)
{
	va_list ap;
	char *str;
	int len;

	str = NULL;
	va_start(ap, fmt);
	if ((len = vasprintf(&str, fmt, ap)) == -1)
		err(1, "malloc()");
	va_end(ap);

	sendstr(sd, str, len);
	free(str);
}

void
senddata(int sd, const void *buf, size_t len)
{
	struct iovec iov[3];
	uint32_t count;
	char d = 'd';
	ssize_t res, ret;

	if (len == 0)
		len = strlen(buf);
	count = len;
	verbose(3, ">>> d %08x <binary data>\n", count);
	count = htonl(count);
	res = 0;
	iov[0].iov_base = &d;
	res += (iov[0].iov_len = 1);
	iov[1].iov_base = &count;
	res += (iov[1].iov_len = sizeof count);
	iov[2].iov_base = (void *)(uintptr_t)buf;
	res += (iov[2].iov_len = len);
	if ((ret = writev(sd, iov, 3)) != res) {
		if (ret == -1)
			err(1, "writev()");
		errx(1, "writev(): short write (%zd / %zd)", ret, res);
	}
}

void
read_full(int sd, void *buf, size_t len)
{
	ssize_t res, ret;
	int retry;

	for (retry = 0, res = len; res > 0; res -= ret) {
		while ((ret = read(sd, buf, res)) < 0)
			if (errno != EAGAIN && errno != EINTR)
				err(1, "read()");
		if (ret == 0) {
			if (++retry < 5)
				continue;
			errx(1, "read(): short read (%zd / %zu)",
			    len - res, len);
		}
		retry = 0;
		buf = (char *)buf + ret;
	}
}

char *
recvstr(int sd, char **str, size_t *len)
{
	uint32_t count;
	char s;

	/* read marker */
	read_full(sd, &s, sizeof s);
	if (s != 's')
		errx(1, "protocol error");

	/* read count */
	read_full(sd, &count, sizeof count);
	count = ntohl(count);

	/* make sure the buffer is sufficiently large */
	if (*len < count + 1)
		if ((*str = reallocf(*str, count + 1)) == NULL)
			err(1, "realloc()");

	/* read string */
	read_full(sd, *str, count);
	(*str)[count] = '\0';

	/* check for early termination */
	for (unsigned int i = 0; i < count; ++i)
		if ((*str)[i] == '\0')
			err(1, "protocol error");

	verbose(2, "<<< s %08x %s\n", count, *str);
	return (*str);
}

void *
recvdata(int sd, void **buf, size_t *len, size_t *datalen)
{
	uint32_t count;
	char d;

	/* read marker */
	read_full(sd, &d, sizeof d);
	if (d != 'd')
		errx(1, "protocol error");

	/* read count */
	read_full(sd, &count, sizeof count);
	count = ntohl(count);

	/* make sure the buffer is sufficiently large */
	if (*len < count)
		if ((*buf = reallocf(*buf, count)) == NULL)
			err(1, "realloc()");

	/* read data */
	read_full(sd, *buf, count);

	verbose(3, "<<< d %08x <binary data>\n", count);
	*datalen = count;
	return (*buf);
}
