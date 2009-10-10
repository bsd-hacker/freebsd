/*
 * Copyright (c) 2009 Ermal Luçi. All rights reserved.
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
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <net/if_pflow.h>
#include <net/route.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

void	pflow_status(int);
void	setpflow_sender(const char *, int, int , const struct afswtch *);
void	unsetpflow_sender(const char *, int, int , const struct afswtch *);
void	setpflow_receiver(const char *, int, int , const struct afswtch *);
void	unsetpflow_receiver(const char *, int, int , const struct afswtch *);

static struct cmd pflow_cmds[] = {
	DEF_CMD_ARG("flowsrc",		setpflow_sender),
	DEF_CMD_ARG("-flowsrc",		unsetpflow_sender),
	DEF_CMD_ARG("flowdst",		setpflow_receiver),
	DEF_CMD_ARG("-flowsrc",		unsetpflow_receiver),
};
static struct afswtch af_pflow = {
	.af_name	= "af_pflow",
	.af_af		= AF_UNSPEC,
	.af_other_status = pflow_status,
};

void
pflow_status(int s)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;

	if (ioctl(s, SIOCGETPFLOW, (caddr_t)&ifr) == -1)
		 return;

	printf("\tpflow: sender: %s ", inet_ntoa(preq.sender_ip));
	printf("receiver: %s:%u\n", inet_ntoa(preq.receiver_ip),
	    ntohs(preq.receiver_port));
}

/* ARGSUSED */
void
setpflow_sender(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct pflowreq preq;
	struct addrinfo hints, *sender;
	int ecode;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/

	if ((ecode = getaddrinfo(val, NULL, &hints, &sender)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (sender->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the sender");

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_SRCIP;
	preq.sender_ip.s_addr = ((struct sockaddr_in *)
	    sender->ai_addr)->sin_addr.s_addr;
	

	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");

	freeaddrinfo(sender);
}

void
unsetpflow_sender(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	preq.addrmask |= PFLOW_MASK_SRCIP;
	ifr.ifr_data = (caddr_t)&preq;
	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");
}

/* ARGSUSED */
void
setpflow_receiver(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct pflowreq preq;
	struct addrinfo hints, *receiver;
	int ecode;
	char *ip, *port, buf[MAXHOSTNAMELEN+sizeof (":65535")];

	if (strchr (val, ':') == NULL)
		errx(1, "%s bad value", val);

	if (strlcpy(buf, val, sizeof(buf)) >= sizeof(buf))
		errx(1, "%s bad value", val);
	port = strchr(buf, ':');
	*port++ = '\0';
	ip = buf;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/

	if ((ecode = getaddrinfo(ip, port, &hints, &receiver)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (receiver->ai_addr->sa_family != AF_INET)
		errx(1, "only IPv4 addresses supported for the receiver");

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_DSTIP | PFLOW_MASK_DSTPRT;
	preq.receiver_ip.s_addr = ((struct sockaddr_in *)
	    receiver->ai_addr)->sin_addr.s_addr;
	preq.receiver_port = (u_int16_t) ((struct sockaddr_in *)
	    receiver->ai_addr)->sin_port;

	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");

	freeaddrinfo(receiver);
}

void
unsetpflow_receiver(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct pflowreq preq;

	bzero((char *)&preq, sizeof(struct pflowreq));
	ifr.ifr_data = (caddr_t)&preq;
	preq.addrmask |= PFLOW_MASK_DSTIP | PFLOW_MASK_DSTPRT;
	if (ioctl(s, SIOCSETPFLOW, (caddr_t)&ifr) == -1)
		err(1, "SIOCSETPFLOW");
}

static __constructor void
pflow_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(pflow_cmds);  i++)
		cmd_register(&pflow_cmds[i]);
	af_register(&af_pflow);
#undef N
}
