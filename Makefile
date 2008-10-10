# $FreeBSD$

KMOD=	eeemon
SRCS=	eeemon.c isa_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
