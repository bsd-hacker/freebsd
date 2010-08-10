# $FreeBSD$

PROG	 = phybs
CSTD	?= c99
WARNS	?= 6
MAN	 = # none

.include "../Makefile.inc"
.include <bsd.prog.mk>
