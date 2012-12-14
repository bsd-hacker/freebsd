# ioat Loadable Kernel Module
#
# $FreeBSD$

IOAT_SRC_PATH = ${.CURDIR}/../..

.PATH:  ${IOAT_SRC_PATH}/dev/ioat

KMOD    = ioat
SRCS    = ioat.c ioat_logger.c
#SRCS += ioat_test.c

SRCS += device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

DEBUG_FLAGS = -g
#CFLAGS += -O0
CFLAGS += -I$(IOAT_SRC_PATH)
CFLAGS += -DIOAT_LOGGING
#CFLAGS += -DINVARIANTS
