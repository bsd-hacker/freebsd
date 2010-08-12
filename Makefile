# $FreeBSD$

PROG	 = phybs
CSTD	?= c99
WARNS	?= 6
MAN	 = # none

LDADD	 = -lutil
DPADD	 = ${LIBUTIL}

.include <bsd.prog.mk>
