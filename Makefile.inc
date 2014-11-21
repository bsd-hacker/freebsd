# $FreeBSD$

CFLAGS +=	-I${.CURDIR}/../libvdsk
LDFLAGS +=	-L${.OBJDIR}/../libvdsk
LIBVDSK ?=	${.OBJDIR}/../libvdsk/libvdsk.a

BINDIR ?=	/usr/sbin
SHLIB_MAJOR ?=	5
WARNS ?=	6
