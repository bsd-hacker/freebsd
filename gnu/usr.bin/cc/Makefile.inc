# $FreeBSD$

.include "../Makefile.inc"

# Sometimes this is .include'd several times...
.if !defined(__CC_MAKEFILE_INC__)
__CC_MAKEFILE_INC__= ${MFILE}

GCCDIR=	${.CURDIR}/../../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../../contrib/gcclibs

.include "Makefile.tgt"

# Machine description.
MD_FILE=	${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}.md
GCC_TARGET=	${TARGET_ARCH}-undermydesk-freebsd

CFLAGS+=	-DIN_GCC -DHAVE_CONFIG_H
CFLAGS+=	-DPREFIX=\"${TOOLS_PREFIX}/usr\"
#CFLAGS+=	-DWANT_COMPILER_INVARIANTS
CSTD?=	gnu89

# If building 64-bit longs for the i386, "_LARGE_LONG" should also be defined
# to get the proper sizes in limits.h
.if defined(LONG_TYPE_SIZE)
CFLAGS+=	-DLONG_TYPE_SIZE=${LONG_TYPE_SIZE}
.endif

.if ${TARGET_ARCH} != ${MACHINE_ARCH}
CFLAGS+=	-DCROSS_COMPILE
.endif

.if ${TARGET_ARCH} == "mips"
# XXX This is backwards, MIPS should default to BE.
.if !defined(TARGET_BIG_ENDIAN)
CFLAGS += -DTARGET_ENDIAN_DEFAULT=0
.endif

.if defined(TARGET_ABI) && ${TARGET_ABI} != "o32"
.if ${TARGET_ABI} == "n32"
MIPS_ABI_DEFAULT=ABI_N32
.elif ${TARGET_ABI} == "n64"
MIPS_ABI_DEFAULT=ABI_64
.endif
.endif

# GCC by default takes the ISA from the ABI's requirements.  If world is built
# with a superior ISA, since we lack multilib, we have to set the right
# default ISA to be able to link against what's in /usr/lib.  Terrible stuff.
.if defined(TARGET_CPUTYPE)
.if ${TARGET_CPUTYPE} == "mips1"
MIPS_ISA_DEFAULT=1
.elif ${TARGET_CPUTYPE} == "mips2"
MIPS_ISA_DEFAULT=2
.elif ${TARGET_CPUTYPE} == "mips3"
MIPS_ISA_DEFAULT=3
.elif ${TARGET_CPUTYPE} == "mips4"
MIPS_ISA_DEFAULT=4
.elif ${TARGET_CPUTYPE} == "mips32"
MIPS_ISA_DEFAULT=32
.elif ${TARGET_CPUTYPE} == "mips32r2"
MIPS_ISA_DEFAULT=32r2
.elif ${TARGET_CPUTYPE} == "mips64"
MIPS_ISA_DEFAULT=64
.else
# Let's hope from-abi is good enough.
.endif
CFLAGS += -DMIPS_ISA_DEFAULT=${MIPS_ISA_DEFAULT}
.endif

MIPS_ABI_DEFAULT?=ABI_32
CFLAGS += -DMIPS_ABI_DEFAULT=${MIPS_ABI_DEFAULT}
.endif

.if defined(WANT_FORCE_OPTIMIZATION_DOWNGRADE)
CFLAGS+= -DFORCE_OPTIMIZATION_DOWNGRADE=${WANT_FORCE_OPTIMIZATION_DOWNGRADE}
.endif

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.endif
CFLAGS+=	-I${.CURDIR}/../cc_tools
# This must go after the -I for cc_tools to resolve ambiguities for hash.h
# correctly.
CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config

CFLAGS+=	-I${GCCLIB}/include
CFLAGS+=	-I${GCCLIB}/libcpp/include
CFLAGS+=	-I${GCCLIB}/libdecnumber

.if exists(${.OBJDIR}/../cc_int)
LIBBACKEND=	${.OBJDIR}/../cc_int/libbackend.a
.else
LIBBACKEND=	${.CURDIR}/../cc_int/libbackend.a
.endif

.if exists(${.OBJDIR}/../libiberty)
LIBIBERTY=	${.OBJDIR}/../libiberty/libiberty.a
.else
LIBIBERTY=	${.CURDIR}/../libiberty/libiberty.a
.endif

.if exists(${.OBJDIR}/../libcpp)
LIBCPP=		${.OBJDIR}/../libcpp/libcpp.a
.else
LIBCPP=		${.CURDIR}/../libcpp/libcpp.a
.endif

.if exists(${.OBJDIR}/../libdecnumber)
LIBDECNUMBER=	${.OBJDIR}/../libdecnumber/libdecnumber.a
.else
LIBDECNUMBER=	${.CURDIR}/../libdecnumber/libdecnumber.a
.endif

.endif # !__CC_MAKEFILE_INC__
