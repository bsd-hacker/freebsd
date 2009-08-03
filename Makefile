#
# $FreeBSD$
#

UNICODEDIR?=	/home/edwin/unicode/
CLDRVERSION?=	1.7.0
CLDRDIR?=	${UNICODEDIR}/cldr/${CLDRVERSION}/
UNIDATAVERSION?=5.1.0
UNIDATADIR?=	${UNICODEDIR}/UNIDATA/${UNIDATAVERSION}/

XMLDIR?=	/home/edwin/svn/edwin/locale/tools/
XMLFILE?=	charmaps.xml

TYPES?=		monetdef numericdef msgdef timedef

all:
.for t in ${TYPES}
	test -d ${t} || mkdir ${t}
	make build-${t}
.endfor
	@echo ""
	@find . -name *failed

install:
.for t in ${TYPES}
	cd ${t} && make
	cd ${t} && sudo DESTDIR=/home/edwin/locale/new make install
.endfor

.for t in ${TYPES}
build-${t}:
	test -d ${t} || mkdir ${t}
	perl -I tools tools/cldr2def.pl ${CLDRDIR} ${UNIDATADIR} ${XMLDIR} ${XMLDIR}/${XMLFILE} ${t} ${LC}
.endfor

clean:
.for t in ${TYPES}
	-rm ${t}/*
	-rmdir ${t}
.endfor

