#
# $FreeBSD$
#

CLDRDIR!=	grep ^cldr etc/unicode.conf | cut -f 2 -d " "
UNIDATADIR!=	grep ^unidata etc/unicode.conf | cut -f 2 -d " "

ETCDIR=		${.CURDIR}/etc

TYPES?=		monetdef numericdef msgdef timedef

.if defined(LC)
LC:=	--lc=${LC}
.endif

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
	perl -I tools tools/cldr2def.pl \
		--cldr=$$(realpath ${CLDRDIR}) \
		--unidata=$$(realpath ${UNIDATADIR}) \
		--etc=$$(realpath ${ETCDIR}) \
		--type=${t} ${LC}
.endfor

clean:
.for t in ${TYPES}
	-rm ${t}/*
	-rmdir ${t}
.endfor

