# $P4: //depot/user/rpaulo/aird/Makefile#1 $

PROG=	aird
MAN=	aird.1
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
