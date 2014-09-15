# $FreeBSD$

MAN=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svnversion

PROG=	svn${SVNLITE}version

SRCS=	svnversion.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDFLAGS=-L${LIBSVN_WCDIR} -L${LIBSVN_DELTADIR} -L${LIBSVN_DIFFDIR} \
	-L${LIBSVN_SUBRDIR} -L${LIBAPR_UTILDIR} -L${LIBAPRDIR} \
	-L${LIBSQLITEDIR}

LDADD=	-lsvn_wc -lsvn_delta -lsvn_diff -lsvn_subr -lserf -lapr-util -lbsdxml \
	-lapr -lsqlite3 -lz -lcrypt

DPADD=	${LIBSVN_WC} ${LIBSVN_DELTA} ${LIBSVN_DIFF} ${LIBSVN_SUBR} \
	${LIBSERF} ${LIBAPR_UTIL} ${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} \
	${LIBZ} ${LIBCRYPT}

.include <bsd.prog.mk>
