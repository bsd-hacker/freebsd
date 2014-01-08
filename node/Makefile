# $FreeBSD$

ROOT?= /kyua
SHTK?= shtk

BINDIR= ${.CURDIR}
LOCALSTATEDIR= /var
SHELL= /bin/sh
CONFIGS_SUBDIR!= hostname -s
SYSCONFDIR= ${.CURDIR}/configs/${CONFIGS_SUBDIR}

SCRIPTS= iterate loop

.PHONY: all
all: ${SCRIPTS} rc.d/autotest_node

.for script in ${SCRIPTS}
${script}: ${script}.sh
	sed -e 's,__AUTOTEST_BINDIR__,${BINDIR},g' \
	    -e 's,__AUTOTEST_ETCDIR__,${SYSCONFDIR},g' \
	    -e 's,__AUTOTEST_ROOT__,${ROOT},g' \
	    "${script}.sh" | "${SHTK}" build -o "${script}" -s "${SHELL}" -
.endfor

rc.d/autotest_node: rc.d/autotest_node.in
	sed -e "s,__AUTOTEST_BINDIR__,${BINDIR},g" \
	    -e 's,__AUTOTEST_ETCDIR__,${SYSCONFDIR},g' \
	    -e 's,__AUTOTEST_SHELL__,${SHELL},g' \
	    -e "s,__AUTOTEST_VARBASE__,${LOCALSTATEDIR},g" \
	    rc.d/autotest_node.in >rc.d/autotest_node
	chmod +x rc.d/autotest_node

.PHONY: clean
clean:
	rm -f ${SCRIPTS} rc.d/autotest_node
