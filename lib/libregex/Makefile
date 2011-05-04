#
# $FreeBSD$
#

.PATH: ${.CURDIR}/../../contrib/tre/lib

LIB=	regex
SHLIB_MAJOR=1
NO_MAN=	yes
WARNS?=	6

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}/../../contrib/tre
SRCS=   regcomp.c regerror.c regexec.c tre-ast.c tre-compile.c \
	tre-match-approx.c tre-match-backtrack.c tre-match-parallel.c \
	tre-mem.c tre-parse.c tre-stack.c xmalloc.c

.include <bsd.lib.mk>
