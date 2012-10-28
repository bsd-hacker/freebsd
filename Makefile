# $FreeBSD$

CP?=	/bin/cp
CUT?=	/usr/bin/cut
MKDIR?=	/bin/mkdir
PYTHON?=/usr/bin/env python
RM?=	/bin/rm
SCRIPT=	rclint.py
SED?=	/usr/bin/sed
TAR?=	/usr/bin/tar

VER!=	${PYTHON} ${SCRIPT} --version 2>&1
.for V in ${VER}
VERSION?=	${V:C,^([^-]+).*,\1,}
.endfor
VERSION_MAJOR=	${VERSION:C,([0-9]+).*,\1,}
VERSION_MINOR=	${VERSION:C,[0-9]+\.([0-9]+).*,\1,}
VERSION_MICRO=	${VERSION:C,[0-9]+\.[0-9]+\.([0-9]+).*,\1,}

FILES=	${SCRIPT} errors.en problems.en

.PHONY: majorbump minorbump microbump release

majorbump:
	M=$$(expr ${VERSION_MAJOR} + 1); \
	${SED} -i '' -e "s,^\(MAJOR = \).*,\1$$M," ${SCRIPT}
	${SED} -i '' -e "s,^\(MINOR = \).*,\10," ${SCRIPT}
	${SED} -i '' -e "s,^\(MICRO = \).*,\10," ${SCRIPT}

minorbump:
	M=$$(expr ${VERSION_MINOR} + 1); \
	${SED} -i '' -e "s,^\(MINOR = \).*,\1$$M," ${SCRIPT}
	${SED} -i '' -e "s,^\(MICRO = \).*,\10," ${SCRIPT}

microbump:
	M=$$(expr ${VERSION_MICRO} + 1); \
	${SED} -i '' -e "s,^\(MICRO = \).*,\1$$M," ${SCRIPT}

tarball:
	${MKDIR} rclint-${VERSION} && \
	${CP} ${FILES} rclint-${VERSION} && \
	${TAR} cvfz rclint-${VERSION}.tar.gz rclint-${VERSION} && \
	${RM} -rf rclint-${VERSION}
	if [ "`id -un`@`hostname -s`" = "crees@pegasus" ]; then \
		sudo cp rclint-${VERSION}.tar.gz /usr/local/www/data/dist/rclint/ &&\
		scp rclint-${VERSION}.tar.gz freefall.FreeBSD.org:public_distfiles/rclint/; \
	fi
