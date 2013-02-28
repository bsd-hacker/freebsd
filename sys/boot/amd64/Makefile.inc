# Common defines for all of /sys/boot/i386/
#
# $FreeBSD$

BINDIR?=	/boot

CFLAGS+=	-ffreestanding -mno-mmx -mno-3dnow -mno-sse -mno-sse2 \
		    -mno-sse3 -msoft-float
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
