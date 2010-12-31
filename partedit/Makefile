# $FreeBSD $

PROG=	partedit
LDADD=	-lgeom -lncursesw -lutil -L/usr/local/lib -lcdialog
CFLAGS= -I/usr/local/include

PARTEDIT_ARCH= ${MACHINE}
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
PARTEDIT_ARCH= x86
.endif
.if !exists(partedit_${PARTEDIT_ARCH}.c)
PARTEDIT_ARCH= generic
.endif

SRCS=	diskeditor.c partedit.c gpart_ops.c partedit_${PARTEDIT_ARCH}.c

WARNS?=	3
NO_MAN=	true

.include <bsd.prog.mk>
