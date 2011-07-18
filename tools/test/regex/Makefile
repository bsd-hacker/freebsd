# $FreeBSD$

SUBDIR=	regmatch

TESTS=	bre.tests
TESTS+=	ere.tests

test: regmatch
.for t in ${TESTS}
	@echo "=== Running test ${t} ==="
	@flags=`grep '# *FLAGS' tests/${t} | sed 's|# *FLAGS *||g'`; \
	for l in `cat tests/${t} | grep -ve '^#'`; do \
		str=`echo $${l} | cut -d \; -f 2`; \
		pat=`echo $${l} | cut -d \; -f 1`; \
		match=`echo $${l} | cut -d \; -f 3`; \
		result=`env LC_ALL=C REGTEST_FLAGS=$${flags} ./regmatch/regmatch $${pat} $${str}`; \
		if [ "$${match}" != "$${result}" ]; then \
			echo "Failed matching pattern $${pat} to string $${str}"; \
		else \
			echo "PASSED matching pattern $${pat} to string $${str}"; \
		fi \
	done 
.endfor

.include <bsd.prog.mk>
