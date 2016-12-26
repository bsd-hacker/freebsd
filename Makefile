# $FreEBSD$

PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c
WARNS=	7
CFLAGS+=	-Wno-incompatible-pointer-types-discards-qualifiers

.include <bsd.prog.mk>
