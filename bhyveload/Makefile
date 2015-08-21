# $FreeBSD$

PROG=	bhyveload
SRCS=	bhyveload.c
MAN=	bhyveload.8

DPADD+=	${LIBVDSK} ${LIBVMMAPI}
LDADD+=	-lvdsk -lvmmapi

WARNS?=	3

CFLAGS+=-I/sys/boot/userboot

.include <bsd.prog.mk>
