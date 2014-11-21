# $FreeBSD$

CFLAGS+=-I${.CURDIR}/../libvdsk
LDFLAGS+=-L${.OBJDIR}/../libvdsk
LIBVDSK?=${.OBJDIR}/../libvdsk/libvdsk.a
