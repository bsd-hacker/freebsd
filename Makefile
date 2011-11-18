PROG=	mptd
BINDIR=	/usr/sbin

SRCS=	mptd.c mpt_cam.c mpt_cmd.c

CFLAGS+= -g -Wall -Wunused

MAN=

LDADD+=	-lcam

.include <bsd.prog.mk>
