# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.PATH:  ${.CURDIR}/../../compat/linux

KMOD=	linuxaio
SRCS=	linux_aio.c linux_aio.h opt_compat.h

.include <bsd.kmod.mk>
