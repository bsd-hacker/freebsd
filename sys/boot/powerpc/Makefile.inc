# $FreeBSD$

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc
LDFLAGS+=	-m elf32-powerpc
AFLAGS+=	--32
.endif

.include "../Makefile.inc"
