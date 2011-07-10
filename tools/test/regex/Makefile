# $FreeBSD$

SUBDIR=	regmatch

TESTS=basic.tests

test: regmatch
.for t in ${TESTS}
	@echo "=== Running test ${t} ==="
	@for l in `cat tests/${t} | grep -ve '^#'`; do \
		str=`echo $${l} | cut -d \; -f 2`; \
		pat=`echo $${l} | cut -d \; -f 1`; \
		match=`echo $${l} | cut -d \; -f 3`; \
		result=`./regmatch/regmatch $${pat} $${str}`; \
		if [ "$${match}" != "$${result}" ]; then \
			echo "Failed matching pattern $${pat} to string $${str}"; \
		else \
			echo "PASSED matching pattern $${pat} to string $${str}"; \
		fi \
	done 
.endfor

.include <bsd.prog.mk>
