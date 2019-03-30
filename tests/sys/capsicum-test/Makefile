# $FreeBSD$

.PATH: ${SRCTOP}/contrib/capsicum-test

TESTSDIR=	${TESTSBASE}/sys/capsicum-test

GTESTS+=	capsicum-test

SRCS.capsicum-test+=	\
	capsicum-test-main.cc \
	capsicum-test.cc \
	capability-fd.cc \
	fexecve.cc \
	procdesc.cc \
	capmode.cc \
	fcntl.cc \
	ioctl.cc \
	openat.cc \
	sysctl.cc \
	select.cc \
	mqueue.cc \
	socket.cc \
	sctp.cc \
	capability-fd-pair.cc \
	overhead.cc \
	rename.cc

LIBADD.capsicum-test+=	gtest pthread

.for p in mini-me mini-me.noexec mini-me.setuid
PROGS+=		$p
NO_SHARED.$p=
SRCS.$p=	mini-me.c
.endfor

BINDIR=	${TESTSDIR}

BINMODE.mini-me.noexec=	${NOBINMODE}
BINMODE.mini-me.setuid=	4444

WARNS?=	3

.include <bsd.test.mk>
