# $FreeBSD$

PROG	 = ndr
SRCS	 = ndr_main.c ndr_protocol.c ndr_client.c ndr_server.c
CSTD	 = c99
WARNS	?= 6
WFORMAT	?= 1

DPADD	 = ${LIBMD}
LDADD	 = -lmd

.include <bsd.prog.mk>
