# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests

BINDIR=		${TESTSDIR}

IMPLEMENTATION?=      -DREGEX_SPENCER

CFLAGS.h_regex+=-I${TESTSRC} -I${.CURDIR:H:H}/regex
PROGS+=		h_regex
SRCS.h_regex=	main.c split.c debug.c

NETBSD_ATF_TESTS_SH=	regex_test

${PACKAGE}FILESDIR=	${TESTSDIR}/data
${PACKAGE}FILES+=		README
${PACKAGE}FILES+=		data/anchor.in
${PACKAGE}FILES+=		data/backref.in
${PACKAGE}FILES+=		data/basic.in
${PACKAGE}FILES+=		data/bracket.in
${PACKAGE}FILES+=		data/c_comments.in
${PACKAGE}FILES+=		data/complex.in
${PACKAGE}FILES+=		data/error.in
${PACKAGE}FILES+=		data/meta.in
${PACKAGE}FILES+=		data/nospec.in
${PACKAGE}FILES+=		data/paren.in
${PACKAGE}FILES+=		data/regress.in
${PACKAGE}FILES+=		data/repet_bounded.in
${PACKAGE}FILES+=		data/repet_multi.in
${PACKAGE}FILES+=		data/repet_ordinary.in
${PACKAGE}FILES+=		data/startend.in
${PACKAGE}FILES+=		data/subexp.in
${PACKAGE}FILES+=		data/subtle.in
${PACKAGE}FILES+=		data/word_bound.in
${PACKAGE}FILES+=		data/zero.in
#${PACKAGE}FILES+=		data/att/README
${PACKAGE}FILES+=		data/att/basic.dat
${PACKAGE}FILES+=		data/att/categorization.dat
${PACKAGE}FILES+=		data/att/forcedassoc.dat
${PACKAGE}FILES+=		data/att/leftassoc.dat
${PACKAGE}FILES+=		data/att/nullsubexpr.dat
${PACKAGE}FILES+=		data/att/repetition.dat
${PACKAGE}FILES+=		data/att/rightassoc.dat

NETBSD_ATF_TESTS_C=	exhaust_test
NETBSD_ATF_TESTS_C+=	regex_att_test

.for t in ${NETBSD_ATF_TESTS_C}
CFLAGS.$t+=	-I${TESTSRC} ${IMPLEMENTATION}
.endfor

.include "../Makefile.netbsd-tests"

LIBADD.regex_att_test+=	util

.include <bsd.test.mk>
