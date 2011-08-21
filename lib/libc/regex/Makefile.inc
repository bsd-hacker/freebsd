#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/tre/lib ${.CURDIR}/regex

CFLAGS+=-DHAVE_CONFIG_H -DTRE_LIBC_BUILD -I${.CURDIR}/../../contrib/tre

SRCS+=	fastmatch.c hashtable.c regcomp.c regerror.c regexec.c tre-ast.c \
	tre-compile.c tre-fastmatch.c tre-heuristic.c tre-match-approx.c \
	tre-match-backtrack.c tre-match-parallel.c tre-mem.c tre-parse.c \
	tre-stack.c xmalloc.c

MAN+=	regex.3 re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3 \
	regexec.3 regfree.3

SYM_MAPS+=${.CURDIR}/regex/Symbol.map
