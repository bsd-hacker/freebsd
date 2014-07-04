# $FreeBSD$

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= freebsd7-update.rc freebsd7-build.rc
ETCFILES	+= freebsd8-update.rc freebsd8-build.rc
ETCFILES	+= freebsd9-update.rc freebsd9-build.rc
ETCFILES	+= freebsd10-update.rc freebsd10-build.rc
ETCFILES	+= head-update.rc head-build.rc
ETCFILES	+= head-noclang-build.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
