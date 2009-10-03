
#	$NetBSD: Makefile,v 1.6 2009/04/20 16:05:30 drochner Exp $

PROG=		unicode2utf8
SRCS=		unicode2utf8.c
NO_MAN=		yes
WARNS?=		6

test:
	./unicode2utf8 \
	    --cldr=/home/edwin/unicode/cldr/1.7.1/ \
	    --input=nl_NL.unicode \
	    --output=nl_NL.UTF-8.src

.include <bsd.prog.mk>
