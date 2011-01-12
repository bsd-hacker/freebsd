# $FreeBSD $

BINDIR= /usr/libexec/bsdinstall
PROG=	partedit
LDADD=	-lgeom -lncursesw -lutil -ldialog -lm

PARTEDIT_ARCH= ${MACHINE}
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
PARTEDIT_ARCH= x86
.endif
.if ${MACHINE} == "sun4v"
PARTEDIT_ARCH= sparc64
.endif
.if !exists(partedit_${PARTEDIT_ARCH}.c)
PARTEDIT_ARCH= generic
.endif

SRCS=	diskeditor.c partedit.c gpart_ops.c partedit_${PARTEDIT_ARCH}.c

WARNS?=	3
NO_MAN=	true

.include <bsd.prog.mk>
