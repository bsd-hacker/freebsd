PROG=	fft_eval

SRCS=	fft_eval.c fft_linux.c fft_freebsd.c

CFLAGS+=	-I/usr/local/include -L/usr/local/lib -I../../lib/ -L../../lib/libradarpkt

LDADD+=	-lSDL -lSDL_ttf -lradarpkt -lpcap

NO_MAN=	1

.include <bsd.prog.mk>
