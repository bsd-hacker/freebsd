PROG=	fft_eval

SRCS=	fft_eval.c fft_freebsd.c fft_histogram.c

CFLAGS+=	-I/usr/local/include -L/usr/local/lib -I../../lib/ -L../../lib/libradarpkt -pthread

LDADD+=	-lSDL -lSDL_ttf -lradarpkt -lpcap -pthread

NO_MAN=	1

.include <bsd.prog.mk>
