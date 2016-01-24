# $FreeBSD$

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/kernel

TESTSDIR=	${TESTSBASE}/sys/kern

ATF_TESTS_C+=	kern_copyin
ATF_TESTS_C+=	kern_descrip_test
ATF_TESTS_C+=	ptrace_test

LIBADD.ptrace_test+=			pthread

NETBSD_ATF_TESTS_C+=	lockf_test
NETBSD_ATF_TESTS_C+=	mqueue_test

CFLAGS.mqueue_test+=	-I${SRCTOP}/tests
LIBADD.mqueue_test+=	rt

WARNS?=	5

TESTS_SUBDIRS+=	acct
TESTS_SUBDIRS+=	execve
TESTS_SUBDIRS+=	pipe
TESTS_SUBDIRS+=	sockets

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
