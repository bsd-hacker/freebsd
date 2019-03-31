# $FreeBSD$

.include <bsd.init.mk>

.PATH: ${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

GTESTS+=	gmock_stress_test

LIBADD+=	pthread gtest gmock

# This test cannot selectively run a single test, as it verifies results when
# `--gtest_list_tests` is run:
# https://github.com/google/googletest/issues/2204
TEST_INTERFACE.gmock_stress_test=	plain

# The next release will resolve a number of build warnings issues.
NO_WERROR=

.include <bsd.test.mk>
