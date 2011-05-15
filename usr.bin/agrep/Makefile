#	$FreeBSD$

.PATH: ${.CURDIR}/../../contrib/tre/src ${.CURDIR}/../../contrib/tre/doc
PROG=	agrep

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}/../../contrib/tre \
	-I${.CURDIR}/../../include
WARNS?=	6

.include <bsd.prog.mk>
