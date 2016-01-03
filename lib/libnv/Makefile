# $FreeBSD$

SHLIBDIR?= /lib

.include <bsd.own.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${.CURDIR}/../../sys/contrib/libnv ${.CURDIR}/../../sys/sys
CFLAGS+=-I${.CURDIR}/../../sys -I${.CURDIR}

SRCS=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	nvpair.c

WARNS?=	6

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
