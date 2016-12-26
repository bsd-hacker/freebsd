# $FreEBSD$

PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c
WARNS=	7

.include <bsd.prog.mk>
