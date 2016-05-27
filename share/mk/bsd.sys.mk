# $FreeBSD$
#
# This file contains common settings used for building FreeBSD
# sources.

# Enable various levels of compiler warning checks.  These may be
# overridden (e.g. if using a non-gcc compiler) by defining MK_WARNS=no.

# for GCC:   http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Warning-Options.html

.include <bsd.compiler.mk>

# the default is gnu99 for now
CSTD?=		gnu99

.if ${CSTD} == "k&r"
CFLAGS+=	-traditional
.elif ${CSTD} == "c89" || ${CSTD} == "c90"
CFLAGS+=	-std=iso9899:1990
.elif ${CSTD} == "c94" || ${CSTD} == "c95"
CFLAGS+=	-std=iso9899:199409
.elif ${CSTD} == "c99"
CFLAGS+=	-std=iso9899:1999
.else # CSTD
CFLAGS+=	-std=${CSTD}
.endif # CSTD
# -pedantic is problematic because it also imposes namespace restrictions
#CFLAGS+=	-pedantic
.if defined(WARNS)
.if ${WARNS} >= 1
CWARNFLAGS+=	-Wsystem-headers
.if !defined(NO_WERROR) && !defined(NO_WERROR.${COMPILER_TYPE})
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && !NO_WERROR.${COMPILER_TYPE}
.endif # WARNS >= 1
.if ${WARNS} >= 2
CWARNFLAGS+=	-Wall -Wno-format-y2k
.endif # WARNS >= 2
.if ${WARNS} >= 3
CWARNFLAGS+=	-W -Wno-unused-parameter -Wstrict-prototypes\
		-Wmissing-prototypes -Wpointer-arith
.endif # WARNS >= 3
.if ${WARNS} >= 4
CWARNFLAGS+=	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow\
		-Wunused-parameter
.if !defined(NO_WCAST_ALIGN) && !defined(NO_WCAST_ALIGN.${COMPILER_TYPE})
CWARNFLAGS+=	-Wcast-align
.endif # !NO_WCAST_ALIGN !NO_WCAST_ALIGN.${COMPILER_TYPE}
.endif # WARNS >= 4
# BDECFLAGS
.if ${WARNS} >= 6
CWARNFLAGS+=	-Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls\
		-Wold-style-definition
.if !defined(NO_WMISSING_VARIABLE_DECLARATIONS)
CWARNFLAGS.clang+=	-Wmissing-variable-declarations
.endif
.if !defined(NO_WTHREAD_SAFETY)
CWARNFLAGS.clang+=	-Wthread-safety
.endif
.endif # WARNS >= 6
.if ${WARNS} >= 2 && ${WARNS} <= 4
# XXX Delete -Wuninitialized by default for now -- the compiler doesn't
# XXX always get it right.
CWARNFLAGS+=	-Wno-uninitialized
.endif # WARNS >=2 && WARNS <= 4
CWARNFLAGS+=	-Wno-pointer-sign
# Clang has more warnings enabled by default, and when using -Wall, so if WARNS
# is set to low values, these have to be disabled explicitly.
.if ${WARNS} <= 6
CWARNFLAGS.clang+=	-Wno-empty-body -Wno-string-plus-int
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 30400
CWARNFLAGS.clang+= -Wno-unused-const-variable
.endif
.endif # WARNS <= 6
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-unused-value\
		-Wno-parentheses-equality -Wno-unused-function -Wno-enum-conversion
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 30600
CWARNFLAGS.clang+=	-Wno-unused-local-typedef
.endif
.endif # WARNS <= 3
.if ${WARNS} <= 2
CWARNFLAGS.clang+=	-Wno-switch -Wno-switch-enum -Wno-knr-promoted-parameter
.endif # WARNS <= 2
.if ${WARNS} <= 1
CWARNFLAGS.clang+=	-Wno-parentheses
.endif # WARNS <= 1
.if defined(NO_WARRAY_BOUNDS)
CWARNFLAGS.clang+=	-Wno-array-bounds
.endif # NO_WARRAY_BOUNDS
.endif # WARNS

.if defined(FORMAT_AUDIT)
WFORMAT=	1
.endif # FORMAT_AUDIT
.if defined(WFORMAT)
.if ${WFORMAT} > 0
#CWARNFLAGS+=	-Wformat-nonliteral -Wformat-security -Wno-format-extra-args
CWARNFLAGS+=	-Wformat=2 -Wno-format-extra-args
.if ${WARNS} <= 3
CWARNFLAGS.clang+=	-Wno-format-nonliteral
.endif # WARNS <= 3
.if !defined(NO_WERROR) && !defined(NO_WERROR.${COMPILER_TYPE})
CWARNFLAGS+=	-Werror
.endif # !NO_WERROR && !NO_WERROR.${COMPILER_TYPE}
.endif # WFORMAT > 0
.endif # WFORMAT
.if defined(NO_WFORMAT) || defined(NO_WFORMAT.${COMPILER_TYPE})
CWARNFLAGS+=	-Wno-format
.endif # NO_WFORMAT || NO_WFORMAT.${COMPILER_TYPE}

# GCC 5.2.0
.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 50200
CWARNFLAGS+=	-Wno-error=unused-function -Wno-error=enum-compare -Wno-error=logical-not-parentheses -Wno-error=bool-compare -Wno-error=uninitialized -Wno-error=array-bounds -Wno-error=clobbered -Wno-error=cast-align -Wno-error=extra -Wno-error=attributes -Wno-error=inline -Wno-error=unused-but-set-variable -Wno-error=unused-value -Wno-error=strict-aliasing -Wno-error=address
.endif

# How to handle FreeBSD custom printf format specifiers.
.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 30600
FORMAT_EXTENSIONS=	-D__printf__=__freebsd_kprintf__
.else
FORMAT_EXTENSIONS=	-fformat-extensions
.endif

.if defined(IGNORE_PRAGMA)
CWARNFLAGS+=	-Wno-unknown-pragmas
.endif # IGNORE_PRAGMA

# We need this conditional because many places that use it
# only enable it for some files with CLFAGS.$FILE+=${CLANG_NO_IAS}.
# unconditionally, and can't easily use the CFLAGS.clang=
# mechanism.
.if ${COMPILER_TYPE} == "clang"
CLANG_NO_IAS=	 -no-integrated-as
.endif
CLANG_OPT_SMALL= -mstack-alignment=8 -mllvm -inline-threshold=3\
		 -mllvm -simplifycfg-dup-ret
.if ${COMPILER_VERSION} >= 30500 && ${COMPILER_VERSION} < 30700
CLANG_OPT_SMALL+= -mllvm -enable-gvn=false
.else
CLANG_OPT_SMALL+= -mllvm -enable-load-pre=false
.endif
CFLAGS.clang+=	 -Qunused-arguments
.if ${MACHINE_CPUARCH} == "sparc64"
# Don't emit .cfi directives, since we must use GNU as on sparc64, for now.
CFLAGS.clang+=	 -fno-dwarf2-cfi-asm
.endif # SPARC64
# The libc++ headers use c++11 extensions.  These are normally silenced because
# they are treated as system headers, but we explicitly disable that warning
# suppression when building the base system to catch bugs in our headers.
# Eventually we'll want to start building the base system C++ code as C++11,
# but not yet.
CXXFLAGS.clang+=	 -Wno-c++11-extensions

.if ${MK_SSP} != "no" && \
    ${MACHINE_CPUARCH} != "arm" && ${MACHINE_CPUARCH} != "mips"
.if (${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 30500) || \
    (${COMPILER_TYPE} == "gcc" && \
     (${COMPILER_VERSION} == 40201 || ${COMPILER_VERSION} >= 40900))
# Don't use -Wstack-protector as it breaks world with -Werror.
SSP_CFLAGS?=	-fstack-protector-strong
.else
SSP_CFLAGS?=	-fstack-protector
.endif
CFLAGS+=	${SSP_CFLAGS}
.endif # SSP && !ARM && !MIPS

# Allow user-specified additional warning flags, plus compiler and file
# specific flag overrides, unless we've overriden this...
.if ${MK_WARNS} != "no"
CFLAGS+=	${CWARNFLAGS} ${CWARNFLAGS.${COMPILER_TYPE}}
CFLAGS+=	${CWARNFLAGS.${.IMPSRC:T}}
.endif

CFLAGS+=	 ${CFLAGS.${COMPILER_TYPE}}
CXXFLAGS+=	 ${CXXFLAGS.${COMPILER_TYPE}}

ACFLAGS+=	${ACFLAGS.${.IMPSRC:T}}
CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CXXFLAGS+=	${CXXFLAGS.${.IMPSRC:T}}

# Special handling for external GCC.
.if defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == "gcc"
# GCC's --sysroot support for a cross-compiler without a default
# TARGET_SYSTEM_ROOT does not add sysroot/usr/include in or the C++
# include path of sysroot/usr/include/c++/v1.  They need to be added in
# when not using -nostdinc/-nostdinc++.  This is not a problem with a
# non-cross-compiler external GCC or the in-tree cross-compiler GCC which
# has a default TARGET_SYSTEM_ROOT.
.if ${CC:M--sysroot=*} || ${CFLAGS:M--sysroot=*}
.if ${CFLAGS:M-nostdinc} == ""
CFLAGS+=	-isystem =/usr/include
.endif
# Add in sysroot/usr/lib to ensure that it comes before /usr/local/lib
# from ports compilers.
LDFLAGS+=	-L=/usr/lib
# We want to force building the system with our in-tree libc++.  Note that
# this also requires a symlink in OBJDIR/lib/libc++/libstdc++.so to
# sysroot/usr/lib/libc++.so.
.if ${CXXFLAGS:M-nostdinc++} == "" && ${CXXFLAGS:M-nostdlib} == ""
CXXFLAGS+=	-std=c++11 \
		-nostdinc++ -isystem =/usr/include/c++/v1
LDFLAGS+=	-L${OBJTOP}/lib/libc++
.endif
.endif	# --sysroot
.endif	# X_COMPILER_TYPE == gcc

.if defined(SRCTOP)
# Prevent rebuilding during install to support read-only objdirs.
.if !make(all) && make(install) && empty(.MAKE.MODE:Mmeta)
CFLAGS+=	ERROR-tried-to-rebuild-during-make-install
.endif
.endif

# Tell bmake not to mistake standard targets for things to be searched for
# or expect to ever be up-to-date.
PHONY_NOTMAIN = analyze afterdepend afterinstall all beforedepend beforeinstall \
		beforelinking build build-tools buildconfig buildfiles \
		buildincludes check checkdpadd clean cleandepend cleandir \
		cleanobj configure depend distclean distribute exe \
		files html includes install installconfig installfiles \
		installincludes lint obj objlink objs objwarn \
		realinstall tags whereobj

# we don't want ${PROG} to be PHONY
.PHONY: ${PHONY_NOTMAIN:N${PROG:U}}
.NOTMAIN: ${PHONY_NOTMAIN:Nall}

.if ${MK_STAGING} != "no"
.if defined(_SKIP_BUILD) || (!make(all) && !make(clean*))
_SKIP_STAGING?= yes
.endif
.if ${_SKIP_STAGING:Uno} == "yes"
staging stage_libs stage_files stage_as stage_links stage_symlinks:
.else
# allow targets like beforeinstall to be leveraged
DESTDIR= ${STAGE_OBJTOP}
.export DESTDIR

.if target(beforeinstall)
.if !empty(_LIBS) || (${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG))
staging: beforeinstall
.endif
.endif

# normally only libs and includes are staged
.if ${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG)
STAGE_DIR.prog= ${STAGE_OBJTOP}${BINDIR}

.if !empty(PROG)
.if defined(PROGNAME)
STAGE_AS_SETS+= prog
STAGE_AS_${PROG}= ${PROGNAME}
stage_as.prog: ${PROG}
.else
STAGE_SETS+= prog
stage_files.prog: ${PROG}
STAGE_TARGETS+= stage_files
.endif
.endif
.endif

.if !empty(_LIBS) && !defined(INTERNALLIB)
.if defined(SHLIBDIR) && ${SHLIBDIR} != ${LIBDIR} && ${_LIBS:Uno:M*.so.*} != ""
STAGE_SETS+= shlib
STAGE_DIR.shlib= ${STAGE_OBJTOP}${SHLIBDIR}
STAGE_FILES.shlib+= ${_LIBS:M*.so.*}
stage_files.shlib: ${_LIBS:M*.so.*}
.endif

.if defined(SHLIB_LINK) && commands(${SHLIB_LINK:R}.ld)
STAGE_AS_SETS+= ldscript
STAGE_AS.ldscript+= ${SHLIB_LINK:R}.ld
stage_as.ldscript: ${SHLIB_LINK:R}.ld
STAGE_DIR.ldscript = ${STAGE_LIBDIR}
STAGE_AS_${SHLIB_LINK:R}.ld:= ${SHLIB_LINK}
NO_SHLIB_LINKS=
.endif

.if target(stage_files.shlib)
stage_libs: ${_LIBS}
.if defined(DEBUG_FLAGS) && target(${SHLIB_NAME}.symbols)
stage_files.shlib: ${SHLIB_NAME}.symbols
.endif
.else
stage_libs: ${_LIBS}
.endif
.if defined(SHLIB_NAME) && defined(DEBUG_FLAGS) && target(${SHLIB_NAME}.symbols)
stage_libs: ${SHLIB_NAME}.symbols
.endif

.endif

.if !empty(INCS) || !empty(INCSGROUPS) && target(buildincludes)
.if !defined(NO_BEFOREBUILD_INCLUDES)
stage_includes: buildincludes
beforebuild: stage_includes
.endif
.endif

.for t in stage_libs stage_files stage_as
.if target($t)
STAGE_TARGETS+= $t
.endif
.endfor

.if !empty(STAGE_AS_SETS)
STAGE_TARGETS+= stage_as
.endif

.if !empty(_LIBS) || (${MK_STAGING_PROG} != "no" && !defined(INTERNALPROG))

.if !empty(LINKS)
STAGE_TARGETS+= stage_links
.if ${MAKE_VERSION} < 20131001
stage_links.links: ${_LIBS} ${PROG}
.endif
STAGE_SETS+= links
STAGE_LINKS.links= ${LINKS}
.endif

.if !empty(SYMLINKS)
STAGE_TARGETS+= stage_symlinks
STAGE_SETS+= links
STAGE_SYMLINKS.links= ${SYMLINKS}
.endif

.endif

.include <meta.stage.mk>
.endif
.endif

.if defined(META_TARGETS)
.for _tgt in ${META_TARGETS}
.if target(${_tgt})
${_tgt}: ${META_DEPS}
.endif
.endfor
.endif
