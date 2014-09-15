# $FreeBSD$

MAN=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svnsync

PROG=	svn${SVNLITE}sync

SRCS=	svnsync.c sync.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDFLAGS=-L${LIBSVN_RADIR} -L${LIBSVN_RA_LOCALDIR} -L${LIBSVN_RA_SVNDIR} \
	-L${LIBSVN_RA_SERFDIR} -L${LIBSVN_REPOSDIR} -L${LIBSVN_FSDIR} \
	-L${LIBSVN_FS_FSDIR} -L${LIBSVN_FS_UTILDIR} -L${LIBSVN_DELTADIR} \
	-L${LIBSVN_SUBRDIR} -L${LIBSERFDIR} -L${LIBAPR_UTILDIR} \
	-L${LIBAPRDIR} -L${LIBSQLITEDIR}

LDADD=	-lsvn_ra -lsvn_ra_local -lsvn_ra_svn -lsvn_ra_serf -lsvn_repos \
	-lsvn_fs -lsvn_fs_fs -lsvn_fs_util -lsvn_delta -lsvn_subr -lserf \
	-lapr-util -lbsdxml -lapr -lsqlite3 -lz -lcrypt -lmagic -lcrypto \
	-lssl -lpthread

DPADD=	${LIBSVN_RA} ${LIBSVN_RA_LOCAL} ${LIBSVN_RA_SVN} ${LIBSVN_RA_SERF} \
	${LIBSVN_REPOS} ${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} \
	${LIBSVN_DELTA} ${LIBSVN_SUBR} ${LIBSERF} ${LIBAPR_UTIL} \
	${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} ${LIBCRYPT} ${LIBMAGIC} \
	${LIBCRYPTO} ${LIBSSL} ${LIBPTHREAD}

.include <bsd.prog.mk>
