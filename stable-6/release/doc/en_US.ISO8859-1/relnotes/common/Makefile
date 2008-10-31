# $FreeBSD: user/edwin/releasenotes/stable-6/release/doc/en_US.ISO8859-1/relnotes/Makefile 165865 2007-01-07 18:37:59Z marcel $

RELN_ROOT?= ${.CURDIR}/../../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes
.endif


CLEANFILES+=	contrib.ent
SRCS+=		contrib.ent

all: contrib.ent
contrib.ent: ${RELN_ROOT}/../../../contrib.xml ${RELN_ROOT}/share/sgml/release.ent
	${PERL} -w ${RELN_ROOT}/../../../contrib-xmltoent.pl -x ${RELN_ROOT}/../../../contrib.xml -r ${RELN_ROOT}/share/sgml/release.ent -o contrib.ent

URL_RELPREFIX?=	../../../../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
