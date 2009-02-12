# $FreeBSD$

.include <bsd.own.mk>

KMOD=alqtest
SRCS=alqtest.c alqtest.h
CLEANFILES=alqtest.h

SYSDIR=/devel/freebsd_mirror/user/lstewart/alq_varlen_8.x/sys

alqtest.h:
	@awk -F "\n" '{ if(index($$0, "struct alq {") > 0) p=1; if(p == 1) { print $$0; if($$0 == "};") exit; } }' ${.CURDIR}/../../../sys/kern/kern_alq.c >> ${.TARGET}

.include <bsd.kmod.mk> 

