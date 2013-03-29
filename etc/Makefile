# $FreeBSD$

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= head-update.rc head-build.rc
ETCFILES	+= freebsd8-update.rc freebsd8-build.rc
ETCFILES	+= freebsd9-update.rc freebsd9-build.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
