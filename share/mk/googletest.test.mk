# $FreeBSD$
#
# You must include bsd.test.mk instead of this file from your Makefile.
#
# Logic to build and install GoogleTest based test programs.
#
# GoogleTest is a C++ test framework, thus, it does not describe/articulate how
# to write tests in other languages, e.g., C or shell, unlike the ATF, plain,
# and TAP raw test interfaces.
#
# For now this is a thin wrapper around the `plain` test interface, but in the
# future this will rely on a newer version of kyua which will integrate in
# GoogleTest support.

.if !target(__<bsd.test.mk>__)
.error googletest.test.mk cannot be included directly.
.endif

# List of GoogleTest test programs to build.
#
# Programs listed here are built according to the semantics of bsd.progs.mk for
# PROGS_CXX.
#
# Test programs registered in this manner are set to be installed into TESTSDIR
# (which should be overridden by the Makefile) and are not required to provide a
# manpage.
GTESTS?=

# Default test interface for googletest
#
# This knob should be used if the version of kyua in use doesn't support the
# `googletest` test interface.
.ifdef GTESTS_USE_PLAIN_TEST_INTERFACE
GTESTS_DEFAULT_TEST_INTERFACE=	plain
.else
GTESTS_DEFAULT_TEST_INTERFACE=	googletest
.endif

.if !empty(GTESTS)
.include <googletest.test.inc.mk>

PROGS_CXX+= ${GTESTS}
_TESTS+= ${GTESTS}
.for _T in ${GTESTS}
BINDIR.${_T}= ${TESTSDIR}
CXXFLAGS.${_T}+= ${GTESTS_CXXFLAGS}
MAN.${_T}?= # empty
SRCS.${_T}?= ${_T}.cc
TEST_INTERFACE.${_T}?= ${GTESTS_DEFAULT_TEST_INTERFACE}
.endfor
.endif
