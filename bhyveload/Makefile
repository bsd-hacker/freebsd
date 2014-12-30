# $FreeBSD$

PROG=	bhyveload
SRCS=	bhyveload.c
MAN=	bhyveload.8

LIBADD=	vdsk vmmapi util

WARNS?=	3

CFLAGS+=-I/sys/boot/userboot

.include <bsd.prog.mk>
