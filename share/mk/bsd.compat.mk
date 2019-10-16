# $FreeBSD$

.if !targets(__<${_this:T}>__)
__<${_this:T}>__:

# -------------------------------------------------------------------
# 32 bit world
.if ${TARGET_ARCH} == "amd64"
HAS_COMPAT=32
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-march=i686 -mmmx -msse -msse2
.else
LIB32CPUFLAGS=	-march=${TARGET_CPUTYPE}
.endif
.if (defined(WANT_COMPILER_TYPE) && ${WANT_COMPILER_TYPE} == gcc) || \
    (defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == gcc)
.else
LIB32CPUFLAGS+=	-target x86_64-unknown-freebsd13.0
.endif
LIB32CPUFLAGS+=	-m32
LIB32WMAKEENV=	MACHINE=i386 MACHINE_ARCH=i386 \
		MACHINE_CPU="i686 mmx sse sse2"
LIB32WMAKEFLAGS=	\
		AS="${XAS} --32" \
		LD="${XLD} -m elf_i386_fbsd -L${LIBCOMPATTMP}/usr/lib32"

.elif ${TARGET_ARCH} == "powerpc64"
HAS_COMPAT=32
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-mcpu=powerpc
.else
LIB32CPUFLAGS=	-mcpu=${TARGET_CPUTYPE}
.endif
LIB32CPUFLAGS+=	-m32
LIB32WMAKEENV=	MACHINE=powerpc MACHINE_ARCH=powerpc
LIB32WMAKEFLAGS=	\
		LD="${XLD} -m elf32ppc_fbsd"

.elif ${TARGET_ARCH:Mmips64*} != ""
HAS_COMPAT=32
.if (defined(WANT_COMPILER_TYPE) && ${WANT_COMPILER_TYPE} == gcc) || \
    (defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == gcc)
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-march=mips3
.else
LIB32CPUFLAGS=	-march=${TARGET_CPUTYPE}
.endif
.else
.if ${TARGET_ARCH:Mmips64el*} != ""
LIB32CPUFLAGS=  -target mipsel-unknown-freebsd13.0
.else
LIB32CPUFLAGS=  -target mips-unknown-freebsd13.0
.endif
.endif
LIB32CPUFLAGS+= -mabi=32
LIB32WMAKEENV=	MACHINE=mips MACHINE_ARCH=mips
.if ${TARGET_ARCH:Mmips64el*} != ""
LIB32WMAKEFLAGS= LD="${XLD} -m elf32ltsmip_fbsd"
.else
LIB32WMAKEFLAGS= LD="${XLD} -m elf32btsmip_fbsd"
.endif
.endif

LIB32WMAKEFLAGS+= NM="${XNM}"
LIB32WMAKEFLAGS+= OBJCOPY="${XOBJCOPY}"

LIB32CFLAGS=	-DCOMPAT_32BIT
LIB32DTRACE=	${DTRACE} -32
LIB32WMAKEFLAGS+=	-DCOMPAT_32BIT

# -------------------------------------------------------------------
# soft-fp world
.if ${TARGET_ARCH:Marmv[67]*} != ""
HAS_COMPAT=SOFT
LIBSOFTCFLAGS=        -DCOMPAT_SOFTFP
LIBSOFTCPUFLAGS= -mfloat-abi=softfp
LIBSOFTWMAKEENV= CPUTYPE=soft MACHINE=arm MACHINE_ARCH=${TARGET_ARCH}
LIBSOFTWMAKEFLAGS=        -DCOMPAT_SOFTFP
.endif

# -------------------------------------------------------------------
# In the program linking case, select LIBCOMPAT
.if defined(NEED_COMPAT)
.ifndef HAS_COMPAT
.error NEED_COMPAT defined, but no LIBCOMPAT is available
.elif !${HAS_COMPAT:M${NEED_COMPAT}} && ${NEED_COMPAT} != "any"
.error NEED_COMPAT (${NEED_COMPAT}) defined, but not in HAS_COMPAT ($HAS_COMPAT)
.elif ${NEED_COMPAT} == "any"
.endif
.ifdef WANT_COMPAT
.error Both WANT_COMPAT and NEED_COMPAT defined
.endif
WANT_COMPAT:=	${NEED_COMPAT}
.endif

.if defined(HAS_COMPAT) && defined(WANT_COMPAT)
.if ${WANT_COMPAT} == "any"
_LIBCOMPAT:=	${HAS_COMPAT:[1]}
.else
_LIBCOMPAT:=	${WANT_COMPAT}
.endif
.endif


# -------------------------------------------------------------------
# Generic code for each type.
# Set defaults based on type.
libcompat=	${_LIBCOMPAT:tl}
_LIBCOMPAT_MAKEVARS=	_OBJTOP TMP CPUFLAGS CFLAGS CXXFLAGS WMAKEENV \
			WMAKEFLAGS WMAKE
.for _var in ${_LIBCOMPAT_MAKEVARS}
.if !empty(LIB${_LIBCOMPAT}${_var})
LIBCOMPAT${_var}?=	${LIB${_LIBCOMPAT}${_var}}
.endif
.endfor

# Shared flags
LIBCOMPAT_OBJTOP?=	${OBJTOP}/obj-lib${libcompat}
LIBCOMPATTMP?=		${LIBCOMPAT_OBJTOP}/tmp

LIBCOMPATCFLAGS+=	${LIBCOMPATCPUFLAGS} \
			-L${LIBCOMPATTMP}/usr/lib${libcompat} \
			--sysroot=${LIBCOMPATTMP} \
			${BFLAGS}

# -B is needed to find /usr/lib32/crti.o for GCC and /usr/libsoft/crti.o for
# Clang/GCC.
LIBCOMPATCFLAGS+=	-B${LIBCOMPATTMP}/usr/lib${libcompat}

.if defined(WANT_COMPAT)
LIBDIR_BASE:=	/usr/lib${libcompat}
_LIB_OBJTOP=	${LIBCOMPAT_OBJTOP}
CFLAGS+=	${LIBCOMPATCFLAGS}
.endif

.endif
