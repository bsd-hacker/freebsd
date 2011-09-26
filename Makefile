# $FreeBSD$

PROG	 = phybs
CSTD	?= c99
WARNS	?= 6

LDADD	 = -lutil
DPADD	 = ${LIBUTIL}

.include <bsd.prog.mk>
