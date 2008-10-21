# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..

SUBDIR = common
SUBDIR+= alpha
SUBDIR+= amd64
SUBDIR+= ia64
SUBDIR+= i386
SUBDIR+= pc98
SUBDIR+= sparc64

contrib.ent: ${RELN_ROOT}/../../../contrib.xml
	${PERL} -w ${RELN_ROOT}/../../../contrib-xmltoent.pl

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
