# $FreeBSD$

# Setup variables for the compiler
#
# COMPILER_TYPE is the major type of compiler. Currently gcc and clang support
# automatic detection. Other compiler types can be shoe-horned in, but require
# explicit setting of the compiler type. The compiler type can also be set
# explicitly if, say, you install gcc as clang...
#
# COMPILER_VERSION is a numeric constant equal to:
#     major * 10000 + minor * 100 + tiny
# It too can be overriden on the command line. When testing it, be sure to
# make sure that you are limiting the test to a specific compiler. Testing
# against 30300 for gcc likely isn't  what you wanted (since versions of gcc
# prior to 4.2 likely have no prayer of working).
#
# COMPILER_FEATURES will contain one or more of the following, based on
# compiler support for that feature:
#
# - c++11 : supports full (or nearly full) C++11 programming environment.
#
# This file may be included multiple times, but only has effect the first time.
#

.if !target(__<bsd.compiler.mk>__)
__<bsd.compiler.mk>__:

.include <bsd.opts.mk>

# Handle ccache after CC is determined, but not if CC/CXX are already
# overridden with a manual setup.
.if ${MK_CCACHE_BUILD:Uno} == "yes" && \
    !make(showconfig) && \
    (${CC:M*ccache/world/*} == "" || ${CXX:M*ccache/world/*} == "")
# CC is always prepended with the ccache wrapper rather than modifying
# PATH since it is more clear that ccache is used and avoids wasting time
# for mkdep/linking/asm builds.
LOCALBASE?=		/usr/local
CCACHE_WRAPPER_PATH?=	${LOCALBASE}/libexec/ccache
CCACHE_BIN?=		${LOCALBASE}/bin/ccache
.if exists(${CCACHE_BIN})
# Export to ensure sub-makes can filter it out for mkdep/linking and
# to chain down into kernel build which won't include this file.
.export CCACHE_BIN
# Expand and export some variables so they may be based on make vars.
# This allows doing something like the following in the environment:
# CCACHE_BASEDIR='${SRCTOP:H}' MAKEOBJDIRPREFIX='${SRCTOP:H}/obj'
.for var in CCACHE_LOGFILE CCACHE_BASEDIR
.if defined(${var})
${var}:=	${${var}}
.export		${var}
.endif
.endfor
# Handle bootstrapped compiler changes properly by hashing their content
# rather than checking mtime.  For external compilers it should be safe
# to use the more optimal mtime check.
# XXX: CCACHE_COMPILERCHECK= string:<compiler_version, compiler_build_rev, compiler_patch_rev, compiler_default_target, compiler_default_sysroot>
.if ${CC:N${CCACHE_BIN}:[1]:M/*} == ""
CCACHE_COMPILERCHECK?=	content
.else
CCACHE_COMPILERCHECK?=	mtime
.endif
.export CCACHE_COMPILERCHECK
# Remove ccache from the PATH to prevent double calls and wasted CPP/LD time.
PATH:=	${PATH:C,:?${CCACHE_WRAPPER_PATH}(/world)?(:$)?,,g}
# Ensure no bogus CCACHE_PATH leaks in which might avoid the in-tree compiler.
.if !empty(CCACHE_PATH)
CCACHE_PATH=
.export CCACHE_PATH
.endif
# Override various toolchain vars.
.for var in CC CXX HOST_CC HOST_CXX
.if defined(${var}) && ${${var}:M${CCACHE_BIN}} == ""
${var}:=	${CCACHE_BIN} ${${var}}
.endif
.endfor
# GCC does not need the CCACHE_CPP2 hack enabled by default in devel/ccache.
# The port enables it due to ccache passing preprocessed C to clang
# which fails with -Wparentheses-equality, -Wtautological-compare, and
# -Wself-assign on macro-expanded lines.
.if defined(COMPILER_TYPE) && ${COMPILER_TYPE} == "gcc"
CCACHE_NOCPP2=	1
.export CCACHE_NOCPP2
.endif
# Canonicalize CCACHE_DIR for meta mode usage.
.if defined(CCACHE_DIR) && empty(.MAKE.META.IGNORE_PATHS:M${CCACHE_DIR})
CCACHE_DIR:=	${CCACHE_DIR:tA}
.MAKE.META.IGNORE_PATHS+= ${CCACHE_DIR}
.export CCACHE_DIR
.endif
ccache-print-options: .PHONY
	@${CCACHE_BIN} -p
.endif	# exists(${CCACHE_BIN})
.endif	# ${MK_CCACHE_BUILD} == "yes"

# Try to import COMPILER_TYPE and COMPILER_VERSION from parent make.
# The value is only used/exported for the same environment that impacts
# CC and COMPILER_* settings here.
_exported_vars=	COMPILER_TYPE COMPILER_VERSION
_cc_hash=	${CC}${MACHINE}${PATH}
_cc_hash:=	${_cc_hash:hash}
# Only import if none of the vars are set somehow else.
_can_export=	yes
.for var in ${_exported_vars}
.if defined(${var})
_can_export=	no
.endif
.endfor
.if ${_can_export} == yes
.for var in ${_exported_vars}
.if defined(${var}.${_cc_hash})
${var}=	${${var}.${_cc_hash}}
.endif
.endfor
.endif

.if ${MACHINE} == "common"
# common is a pseudo machine for architecture independent
# generated files - thus there is no compiler.
COMPILER_TYPE= none
COMPILER_VERSION= 0
.elif !defined(COMPILER_TYPE) || !defined(COMPILER_VERSION)
_v!=	${CC} --version || echo 0.0.0

.if !defined(COMPILER_TYPE)
. if ${CC:T:M*gcc*}
COMPILER_TYPE:=	gcc  
. elif ${CC:T:M*clang*}
COMPILER_TYPE:=	clang
. elif ${_v:Mgcc}
COMPILER_TYPE:=	gcc
. elif ${_v:M\(GCC\)}
COMPILER_TYPE:=	gcc
. elif ${_v:Mclang}
COMPILER_TYPE:=	clang
. else
.error Unable to determine compiler type for ${CC}.  Consider setting COMPILER_TYPE.
. endif
.endif
.if !defined(COMPILER_VERSION)
COMPILER_VERSION!=echo ${_v:M[1-9].[0-9]*} | awk -F. '{print $$1 * 10000 + $$2 * 100 + $$3;}'
.endif
.undef _v
.endif

# Export the values so sub-makes don't have to look them up again, using the
# hash key computed above.
.for var in ${_exported_vars}
${var}.${_cc_hash}:=	${${var}}
.export-env ${var}.${_cc_hash}
.undef ${var}.${_cc_hash}
.endfor

.if ${COMPILER_TYPE} == "clang" || \
	(${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 40800)
COMPILER_FEATURES=	c++11
.else
COMPILER_FEATURES=
.endif

.endif	# !target(__<bsd.compiler.mk>__)
