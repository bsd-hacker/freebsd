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

#ifndef NDR_H_INCLUDED
#define NDR_H_INCLUDED

void	 verbose(int level, const char *fmt, ...);
void	 status(const char *fmt, ...);

void	 client(const char *device, const char *saddr, const char *sport,
    const char *daddr, const char *dport);
void	 server(const char *laddr, const char *lport);

int	 client_socket(const char *saddr, const char *sport,
    const char *daddr, const char *dport);
int	 server_socket(const char *laddr, const char *lport);
int	 accept_socket(int sd, char **saddr, char **sport);

void	 sendstr(int sd, const char *str, size_t len);
void	 sendstrf(int sd, const char *fmt, ...);
void	 senddata(int sd, const void *buf, size_t len);
void	 read_full(int sd, void *buf, size_t len);
char	*recvstr(int sd, char **str, size_t *len);
void	*recvdata(int sd, void **buf, size_t *len, size_t *datalen);

#endif
