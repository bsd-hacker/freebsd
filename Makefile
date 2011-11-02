# $FreeBSD$

.POSIX:

PROG	= sizes
CC	= c99
CFLAGS	= # none

all: ${PROG}

clean:
	-rm ${PROG}
