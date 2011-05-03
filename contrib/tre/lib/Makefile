#
# $FreeBSD$
#

LIB=	regex
SHLIB_MAJOR= 1
NO_MAN=	yes

CFLAGS+= -DHAVE_MBRTOWC -DHAVE_MBSTATE_T -DHAVE_WCTYPE_H \
		-DTRE_REGEX_T_FIELD=value

SRCS=	regcomp.c regerror.c regexec.c tre-ast.c tre-compile.c \
	tre-match-approx.c tre-match-backtrack.c tre-match-parallel.c \
	tre-mem.c tre-parse.c tre-stack.c xmalloc.c

.include <bsd.lib.mk>
