# $FreEBSD$

BINDIR?=	/usr/bin
PROG=	bdiff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c
WARNS=	7

.include <bsd.prog.mk>
