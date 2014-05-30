# $FreeBSD$

NO_MAN=
NO_OBJ=	noobj
LDADD+= -L../../lib -lstress -lutil
DPADD+= ../../lib/libstress.a
CFLAGS =-g -Wall -Wextra -O2 -I../../include -I../include
NOMAN=	sorry
MK_MAN=no
