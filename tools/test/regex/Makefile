# $FreeBSD$

SUBDIR=	att
SUBDIR+=regmatch

TESTS=	bre.tests
TESTS+=	ere.tests
TESTS+=	bre.hu_HU.ISO8859-2.tests
TESTS+= bre.es_ES.ISO8859-1.tests

test-all: test-freebsd test-att

test-freebsd: regmatch
	@echo "===== Running FreeBSD regex tests ====="
.for t in ${TESTS}
	@echo "=== Running test ${t} ==="
	@flags=`grep '# *FLAGS' tests/${t} | sed 's|# *FLAGS *||g'`; \
	locale=`grep '# *LOCALE' tests/${t} | sed 's|# *LOCALE *||g'`; \
	for l in `cat tests/${t} | grep -ve '^#'`; do \
		str=`echo $${l} | cut -d \; -s -f 2`; \
		pat=`echo $${l} | cut -d \; -s -f 1`; \
		match=`echo $${l} | cut -d \; -s -f 3`; \
		result=`env LC_ALL=$${locale} REGTEST_FLAGS=$${flags} ./regmatch/regmatch $${pat} $${str}`; \
		if [ "$${match}" != "$${result}" ]; then \
			echo "Failed matching pattern $${pat} to string $${str}"; \
		else \
			echo "PASSED matching pattern $${pat} to string $${str}"; \
		fi \
	done 
.endfor

ATT_TESTS= basic.dat
ATT_TESTS+=categorize.dat
ATT_TESTS+=nullsubexpr.dat
ATT_TESTS+=leftassoc.dat
ATT_TESTS+=rightassoc.dat
ATT_TESTS+=forcedassoc.dat
ATT_TESTS+=repetition.dat

test-att: att
	@echo "===== Running AT&T regex tests ====="
.for t in ${ATT_TESTS}
	@echo "=== Running test ${t} ==="
	${.CURDIR}/att/testregex < ${.CURDIR}/att/tests/${t}
.endfor

.include <bsd.prog.mk>
