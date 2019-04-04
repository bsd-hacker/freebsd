# $FreeBSD$

.include <bsd.init.mk>

.PATH:	${GOOGLETEST_SRCROOT}/src ${GOOGLETEST_SRCROOT}/test

GTESTS+=	gtest_environment_test
GTESTS+=	gtest_no_test_unittest
GTESTS+=	googletest-param-test-test
GTESTS+=	gtest_premature_exit_test
GTESTS+=	gtest_repeat_test
GTESTS+=	gtest_stress_test
GTESTS+=	gtest_throw_on_failure_ex_test
GTESTS+=	gtest-unittest-api_test

CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}/include
CXXFLAGS+=	-I${GOOGLETEST_SRCROOT}

.for death_test in gtest-death-test_ex_catch_test gtest-death-test_ex_nocatch_test
# XXX: copying source files in order to tweak CXXFLAGS seems hacky.
${death_test}.cc: googletest-death-test_ex_test.cc
	${CP} ${.ALLSRC} ${.TARGET}
CLEANFILES+=	${death_test}.cc
CXXFLAGS.${death_test}+= -fexception
.endfor

CXXFLAGS.gtest-death-test_ex_catch_test+=	\
	-DGTEST_ENABLE_CATCH_EXCEPTIONS_=1

CXXFLAGS.gtest-death-test_ex_nocatch_test+=     \
	-DGTEST_ENABLE_CATCH_EXCEPTIONS_=0

SRCS.googletest-param-test-test=	\
	googletest-param-test-test.cc	\
	googletest-param-test2-test.cc

LIBADD+=	gtest

# These tests confuse the kyua googletest engine, as they don't conform to the
# googletest spec; they're functional unit tests for the library.
TEST_INTERFACE.gtest_environment_test= plain
TEST_INTERFACE.gtest_no_test_unittest= plain
TEST_INTERFACE.gtest_repeat_test= plain
TEST_INTERFACE.gtest_stress_test= plain
TEST_INTERFACE.gtest_throw_on_failure_ex_test= plain
# This test program cannot selectively run test suites/testcases:
# https://github.com/google/googletest/issues/2205
TEST_INTERFACE.gtest-unittest-api_test= plain

# XXX: explicitly listing -lpthread is incorrect. src.libnames.mk should be
#      handling this.
LIBADD.gtest_stress_test+=	pthread

# The next release will resolve a number of build warnings issues.
NO_WERROR=

.include <bsd.test.mk>
