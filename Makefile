# $FreeBSD$

PROG=	aird
MAN=	aird.1
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
