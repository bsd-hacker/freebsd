# $FreeBSD$

PACKAGE= tests

${PACKAGE}FILES+=		Kyuafile

SUBDIR+= etc
SUBDIR+= sys
SUBDIR_PARALLEL=

TESTSDIR= ${TESTSBASE}
KYUAFILE= yes

${PACKAGE}FILES+=		README

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} ../local/tests ${DESTDIR}${TESTSDIR}/local

.include <bsd.test.mk>
