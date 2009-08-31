# $FreeBSD$

.PATH: ${.CURDIR}/../../libkern/zlib

KMOD=	zlib
SRCS=	adler32.c compress.c deflate.c inflate.c inffast.c inftrees.c
SRCS+=  trees.c uncompr.c zutil.c zl_crc32.c

.include <bsd.kmod.mk>
