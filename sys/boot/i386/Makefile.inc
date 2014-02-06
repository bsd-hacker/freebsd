# Common defines for all of /sys/boot/i386/
#
# $FreeBSD$

BINDIR?=	/boot

LOADER_ADDRESS?=0x200000
.if ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-march=i386
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
.elif ${MACHINE_CPUARCH} == "amd64" && !defined(BUILDING_EFI)
CFLAGS+=	-march=i386
CFLAGS.gcc+=	-mpreferred-stack-boundary=2
.endif
CFLAGS+=	-ffreestanding
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -msoft-float
LDFLAGS+=	-nostdlib

.if ${MACHINE_CPUARCH} == "amd64" && !defined(BUILDING_EFI)
CFLAGS+=	-m32
ACFLAGS+=	-m32
AFLAGS+=	--32
.endif

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

.include "../Makefile.inc"
