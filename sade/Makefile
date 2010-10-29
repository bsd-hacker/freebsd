# $FreeBSD$
SRCPATH?=	../../../../head

.PATH:	${.CURDIR}/${SRCPATH}/sbin/mount

PROG=	sade
NO_MAN=

LIBSADE=devices.c parts.c util.c

SRCS=	main.c parted.c menus.c customdlg.c history.c ufsed.c \
	getmntopts.c
SRCS+=	${LIBSADE}

WARNS?=	1

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBDIALOG} ${LIBNCURSES} ${LIBUFS}
LDADD=	-lutil -lgeom -ldialog -lncurses -lufs

DEBUG_FLAGS?=	-g
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/${SRCPATH}/gnu/lib/libdialog \
	 -I${.CURDIR}/${SRCPATH}/sbin/mount

.include <bsd.prog.mk>
