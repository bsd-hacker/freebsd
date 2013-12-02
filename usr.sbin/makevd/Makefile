# $FreeBSD$

PROG=	makevd
MAN=	makevd.8
SRCS=	makevd.c \
	common.c \
	raw.c \
	vhd.c \
	vmdk.c
WARNS?=	6
CFLAGS+= -I${.CURDIR}

MLINKS=	makevd.8 dumpvd.8

afterinstall:
	ln -f ${BINDIR}/${PROG} ${BINDIR}/dumpvd

.include <bsd.prog.mk>
