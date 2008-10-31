# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..

.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?=	${DOCDIR}/relnotes
.else
DESTDIR?=	${DOCDIR}/en_US.ISO8859-1/relnotes
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml

# Contributed software
SRCS+=	contrib.ent
CLEANFILES+=	contrib.ent

contrib.ent: ${RELN_ROOT}/../../../contrib.xml ${RELN_ROOT}/share/sgml/release.ent
	${PERL} -w ${RELN_ROOT}/../../../contrib-xmltoent.pl -x ${RELN_ROOT}/../../../contrib.xml -r ${RELN_ROOT}/share/sgml/release.ent -o contrib.ent

URL_RELPREFIX?=	../../../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
