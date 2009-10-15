#
# $Id$
#
AC_PREREQ([2.59])
AC_INIT([svnsup], [1.0], [des@des.no])
AC_CONFIG_SRCDIR([src/distill/distill.c])
AM_CONFIG_HEADER([config.h])

AC_LANG(C)
AC_C_CONST
AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_ARG_ENABLE(distill,
	AS_HELP_STRING([--enable-distill],
		[build svnsup-distill (default is YES)]),
	[], [enable_distill=yes]
)
AM_CONDITIONAL([ENABLE_DISTILL], [test x"$enable_distill" = xyes])

AC_ARG_ENABLE(apply,
	AS_HELP_STRING([--enable-apply],
		[build svnsup-apply (default is YES)]),
	[], [enable_apply=yes]
)
AM_CONDITIONAL([ENABLE_APPLY], [test x"$enable_apply" = xyes])

AC_ARG_ENABLE(debugging,
	AS_HELP_STRING([--enable-debugging],
		[enable debugging (default is NO)]),
	[
		CFLAGS="${CFLAGS} -O0 -g"
		CPPFLAGS="${CPPFLAGS} -DDEBUG"
	]
)

AC_ARG_ENABLE(wall,
	AS_HELP_STRING([--enable-wall],
		[gcc only: compile with -Wall (default is NO)]),
	AS_IF([test x"${GCC+set}" = xset], [
		CFLAGS="${CFLAGS} -Wall"
	], [
		AC_MSG_WARN([-Wall is only supported on gcc])
	])
)

AC_ARG_ENABLE(wextra,
	AS_HELP_STRING([--enable-wextra],
		[gcc only: compile with -Wextra (default is NO)]),
	AS_IF([test x"${GCC+set}" = xset], [
		CFLAGS="${CFLAGS} -Wextra"
	], [
		AC_MSG_WARN([-Wextra is only supported on gcc])
	])
)

AC_ARG_ENABLE(werror,
	AS_HELP_STRING([--enable-werror],
		[gcc only: compile with -Werror (default is NO)]),
	AS_IF([test x"${GCC+set}" = xset], [
		CFLAGS="${CFLAGS} -Werror"
	], [
		AC_MSG_WARN([-Werror is only supported on gcc])
	])
)

#
# APR -- needed for svnsup-distill
#
AS_IF([test x"$enable_distill" = x"yes"], [
	PKG_PROG_PKG_CONFIG
	PKG_CHECK_MODULES([APR_1], [apr-1])
])

#
# Helper for Subversion libraries
#
m4_define([check_subversion], [
	saved_CPPFLAGS="${CPPFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${APR_1_CFLAGS}"
	AC_CHECK_HEADER([subversion-1/$2.h], [], [
		AC_MSG_ERROR([cannot proceed without <subversion-1/$2.h>])
	])
	CPPFLAGS="${saved_CPPFLAGS}"
	saved_LIBS="${LIBS}"
	LIBS=""
	AC_CHECK_LIB([$2-1], [$2_version], [], [
		AC_MSG_ERROR([cannot proceed without lib$2-1])
	])
	$1_1_LIBS="${LIBS}"
	LIBS="${saved_LIBS}"
	AC_SUBST([$1_1_LIBS])
])

#
# Subersion remote access library -- needed for svnsup-distill
#
AS_IF([test x"$enable_distill" = x"yes"], [
	check_subversion([SVN_RA], [svn_ra])
])

#
# Subversion delta library -- needed for svnsup-distill
#
AS_IF([test x"$enable_distill" = x"yes"], [
	check_subversion([SVN_DELTA], [svn_delta])
])

#
# Subversion client library -- needed for svnsup-distill
#
AS_IF([test x"$enable_distill" = x"yes"], [
	check_subversion([SVN_CLIENT], [svn_client])
])

#
# Output
#
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/libsvnsup/Makefile
	src/distill/Makefile
	src/apply/Makefile
])
AC_OUTPUT
