
#	$NetBSD: Makefile,v 1.6 2009/04/20 16:05:30 drochner Exp $

PROG=		unicodename2utf8
SRCS=		unicodename2utf8.c
WARNS?=		6
MAN=		unicodename2utf8.1

LINKS=		${BINDIR}/unicodename2utf8 ${BINDIR}/utf82unicodename
MLINKS=		unicodename2utf8.1 utf82unicodename.1

test:
	./unicodename2utf8 \
	    --cldr=/home/edwin/unicode/cldr/1.7.1/ \
	    --input=nl_NL.unicode \
	    --output=nl_NL.UTF-8.src

.include <bsd.prog.mk>
