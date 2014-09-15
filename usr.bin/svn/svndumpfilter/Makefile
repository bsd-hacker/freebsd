# $FreeBSD$

MAN=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svndumpfilter

PROG=	svn${SVNLITE}dumpfilter

SRCS=	svndumpfilter.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDFLAGS=-L${LIBSVN_REPOSDIR} -L${LIBSVN_FSDIR} -L${LIBSVN_FS_FSDIR} \
	-L${LIBSVN_FS_UTILDIR} -L${LIBSVN_DELTADIR} -L${LIBSVN_SUBRDIR} \
	-L${LIBAPR_UTILDIR} -L${LIBAPRDIR} -L${LIBSQLITEDIR}

LDADD=	-lsvn_repos -lsvn_fs -lsvn_fs_fs -lsvn_fs_util -lsvn_delta -lsvn_subr \
	-lapr-util -lbsdxml -lapr -lsqlite3 -lz -lcrypt

DPADD=	${LIBSVN_REPOS} ${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} \
	${LIBSVN_DELTA} ${LIBSVN_SUBR} ${LIBAPR_UTIL} \
	${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} ${LIBCRYPT}

.include <bsd.prog.mk>
