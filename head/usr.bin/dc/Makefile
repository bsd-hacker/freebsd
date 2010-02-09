#	$FreeBSD: user/eri/pf45/head/usr.bin/dc/Makefile 202843 2010-01-22 23:19:48Z delphij $
#	$OpenBSD: Makefile,v 1.2 2006/11/26 11:31:09 deraadt Exp $

PROG=	dc
SRCS=	dc.c bcode.c inout.c mem.c stack.c
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

.include <bsd.prog.mk>
