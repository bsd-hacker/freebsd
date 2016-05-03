# $FreeBSD$

PACKAGE=	tests

ATF_TESTS_SH=	mkimg

SOURCES!=	cd ${.CURDIR}; echo *.uu
${PACKAGE}FILES+=		${SOURCES:S,.gz.uu,,g}

${TESTS}:
	uudecode -p ${.CURDIR}/${.TARGET}.gz.uu | gunzip -c > ${.TARGET}

CLEANFILES+=	${TESTS}

rebase:
	(cd ${.CURDIR}; atf-sh mkimg.sh rebase)

.include <bsd.test.mk>
