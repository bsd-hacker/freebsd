#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=		xinstall
PROGNAME=	install
MAN=		install.1

.PATH:		${.CURDIR}/../../usr.sbin/mtree
CFLAGS+=	-I${.CURDIR}/../../usr.sbin/mtree
LDADD+=		-lmd
DPADD+=		${LIBMD}

.include <bsd.prog.mk>
