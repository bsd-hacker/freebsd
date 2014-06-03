# $FreeBSD$

NO_OBJ=
LDADD+= -L../../lib -lstress -lutil
DPADD+= ../../lib/libstress.a
CFLAGS =-g -Wall -Wextra -O2 -I../../include -I../include
MAN=
