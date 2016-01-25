# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/kern/sockets

TAP_TESTS_C+=	accept_fd_leak_test
TAP_TESTS_C+=	accf_data_attach_test
PLAIN_TESTS_C+=	fstat_test
TAP_TESTS_C+=	kqueue_test
PLAIN_TESTS_C+=	listen_backlog_test
PLAIN_TESTS_C+=	listen_close_test
PLAIN_TESTS_C+=	pr_atomic_test
PLAIN_TESTS_C+=	reconnect_test
PLAIN_TESTS_C+=	rtsocket_test
PLAIN_TESTS_C+=	sblock_test
PLAIN_TESTS_C+=	sendfile_test
PLAIN_TESTS_C+=	shutdown_test
PLAIN_TESTS_C+=	sigpipe_test
PLAIN_TESTS_C+=	so_setfib_test
PLAIN_TESTS_C+=	socketpair_test
PLAIN_TESTS_C+=	unix_bindconnect_test
PLAIN_TESTS_C+=	unix_close_race_test
TAP_TESTS_SH+=	unix_cmsg_test
PLAIN_TESTS_C+=	unix_gc_test
ATF_TESTS_C+=	unix_passfd_test
PLAIN_TESTS_C+=	unix_sendtorace_test
ATF_TESTS_C+=	unix_seqpacket_test
TEST_METADATA.unix_seqpacket_test+=	timeout="15"
PLAIN_TESTS_C+=	unix_socket_test
PLAIN_TESTS_C+=	unix_sorflush_test
PLAIN_TESTS_C+=	zerosend_test

PROGS+=		unix_cmsg_test_helper

LIBADD.sendfile_test+=		md
LIBADD.unix_seqpacket_test+=		pthread

WARNS.unix_cmsg_test_helper=	2

.include <bsd.test.mk>

WARNS?=	6
