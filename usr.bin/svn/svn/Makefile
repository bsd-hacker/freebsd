# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svn

PROG=	svn${SVNLITE}

SRCS=	add-cmd.c blame-cmd.c cat-cmd.c changelist-cmd.c checkout-cmd.c \
	cl-conflicts.c cleanup-cmd.c commit-cmd.c conflict-callbacks.c \
	copy-cmd.c delete-cmd.c deprecated.c diff-cmd.c export-cmd.c \
	file-merge.c help-cmd.c import-cmd.c info-cmd.c list-cmd.c \
	lock-cmd.c log-cmd.c merge-cmd.c mergeinfo-cmd.c mkdir-cmd.c \
	move-cmd.c notify.c patch-cmd.c propdel-cmd.c propedit-cmd.c \
	propget-cmd.c proplist-cmd.c props.c propset-cmd.c relocate-cmd.c \
	resolve-cmd.c resolved-cmd.c revert-cmd.c status-cmd.c status.c \
	svn.c switch-cmd.c unlock-cmd.c update-cmd.c upgrade-cmd.c util.c 

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDFLAGS= -L${LIBSVN_CLIENTDIR} -L${LIBSVN_WCDIR} -L${LIBSVN_RADIR} \
	 -L${LIBSVN_RA_LOCALDIR} -L${LIBSVN_RA_SVNDIR} -L${LIBSVN_RA_SERFDIR} \
	 -L${LIBSVN_REPOSDIR} -L${LIBSVN_FSDIR} -L${LIBSVN_FS_FSDIR} \
	 -L${LIBSVN_FS_UTILDIR} -L${LIBSVN_DELTADIR} -L${LIBSVN_DIFFDIR} \
	 -L${LIBSVN_SUBRDIR} -L${LIBSERFDIR} -L${LIBAPR_UTILDIR} \
	 -L${LIBAPRDIR} -L${LIBSQLITEDIR} \

LDADD=	-lsvn_client -lsvn_wc -lsvn_ra -lsvn_ra_local \
	-lsvn_ra_svn -lsvn_ra_serf -lsvn_repos \
	-lsvn_fs -lsvn_fs_fs -lsvn_fs_util -lsvn_delta \
	-lsvn_diff -lsvn_subr -lserf -lapr-util \
	-lbsdxml -lapr -lsqlite3 -lz -lcrypt -lmagic \
	-lcrypto -lssl -lpthread

DPADD=	${LIBSVN_CLIENT} ${LIBSVN_WC} ${LIBSVN_RA} ${LIBSVN_RA_LOCAL} \
	${LIBSVN_RA_SVN} ${LIBSVN_RA_SERF} ${LIBSVN_REPOS} \
	${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} ${LIBSVN_DELTA} \
	${LIBSVN_DIFF} ${LIBSVN_SUBR} ${LIBSERF} ${LIBAPR_UTIL} \
	${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} ${LIBCRYPT} ${LIBMAGIC} \
	${LIBCRYPTO} ${LIBSSL} ${LIBPTHREAD}

CLEANFILES+=	svnlite.1
.if(defined(ORGANIZATION) && !empty(ORGANIZATION))
DPSRCS+=	freebsd-organization.h
CLEANFILES+=	freebsd-organization.h
CFLAGS+=	-I. -DHAS_ORGANIZATION_NAME
freebsd-organization.h:
	@echo '#define ORGANIZATION_NAME ${ORGANIZATION}' \
	    > freebsd-organization.h
.endif

svnlite.1: svn.1
	# Avoid the website
	sed -E 's,(^| |B|`)svn,\1svnlite,g' ${SVNDIR}/svn/svn.1 > ${.OBJDIR}/svnlite.1

.include <bsd.prog.mk>
