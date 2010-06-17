# $FreeBSD$

PROG=	sade
NO_MAN=

LIBSADE=devices.c parts.c util.c fs.c

SRCS=	main.c parted.c menus.c customdlg.c history.c fsed.c
SRCS+=	${LIBSADE}

WARNS?=	1

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBDIALOG} ${LIBNCURSES} ${LIBUFS}
LDADD=	-lutil -lgeom -ldialog -lncurses -lufs

LIBDIALOG_PATH?=	../../../../head/gnu/lib/libdialog
#DEBUG_FLAGS= -g
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/${LIBDIALOG_PATH}

.include <bsd.prog.mk>
