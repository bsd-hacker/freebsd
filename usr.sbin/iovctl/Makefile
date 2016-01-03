# $FreeBSD$

PROG=	iovctl
SRCS=	iovctl.c parse.c validate.c
DPADD+=	${LIBNV} ${LIBUCL} ${LIBM}
LDADD+=	-lnv -lucl -lm

USEPRIVATELIB=

CFLAGS+=-I${.CURDIR}/../../contrib/libucl/include

WARNS?=6

MAN=	\
	iovctl.8 \
	iovctl.conf.5 \

.include <bsd.own.mk>
.include <bsd.prog.mk>

