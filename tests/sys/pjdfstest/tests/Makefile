PJDFSTEST_SRCDIR=	${.CURDIR:H:H:H:H}/contrib/pjdfstest

TESTSDIR=	${TESTSBASE}/sys/pjdfstest

.PATH: ${.CURDIR} ${PJDFSTEST_SRCDIR}/tests

FILESDIR=	${TESTSDIR}
FILES=		conf
FILES+=		misc.sh

afterinstall: install-tests-symlink
install-tests-symlink: .PHONY
	${INSTALL_SYMLINK} . ${DESTDIR}${TESTSDIR}/tests

KYUAFILE=	no

SUBDIR+=	chflags
SUBDIR+=	chmod
SUBDIR+=	chown
SUBDIR+=	ftruncate
SUBDIR+=	granular
SUBDIR+=	link
SUBDIR+=	mkdir
SUBDIR+=	mkfifo
SUBDIR+=	mknod
SUBDIR+=	open
SUBDIR+=	rename
SUBDIR+=	rmdir
SUBDIR+=	symlink
SUBDIR+=	truncate
SUBDIR+=	unlink

.include <bsd.test.mk>
