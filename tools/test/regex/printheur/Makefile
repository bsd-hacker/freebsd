# $FreeBSD$
.PATH: ${.CURDIR}/../../../../contrib/tre/lib

PROG=	printheur
NO_MAN= yes
SRCS=	printheur.c tre-heuristic.c tre-fastmatch.c hashtable.c

CFLAGS+= -I${.CURDIR}/../../../../contrib/tre/lib \
	 -I${.CURDIR}/../../../../include

.include <bsd.prog.mk>
