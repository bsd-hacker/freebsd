.PATH: ${.CURDIR:H:H:H:H}/contrib/pjdfstest/tests

TESTSDIR=	${TESTSBASE}/sys/pjdfstest

SCRIPTSDIR=	${TESTSDIR}
SCRIPTS=	conf
SCRIPTS+=	misc.sh
SCRIPTSNAME_misc.sh=	misc.sh

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
