#
# $FreeBSD$
#

PROG=	bhyve
PACKAGE=	bhyve

DEBUG_FLAGS= -g -O0

MAN=	bhyve.8

SYSDIR?=${.CURDIR}/../..

SRCS=	\
	atkbdc.c		\
	acpi.c			\
	bhyverun.c		\
	block_if.c		\
	bootrom.c		\
	consport.c		\
	dbgport.c		\
	fwctl.c			\
	inout.c			\
	ioapic.c		\
	mem.c			\
	mevent.c		\
	mptbl.c			\
	pci_ahci.c		\
	pci_emul.c		\
	pci_hostbridge.c	\
	pci_irq.c		\
	pci_lpc.c		\
	pci_passthru.c		\
	pci_virtio_block.c	\
	pci_virtio_net.c	\
	pci_virtio_rnd.c	\
	pci_uart.c		\
	pm.c			\
	post.c			\
	rtc.c			\
	smbiostbl.c		\
	task_switch.c		\
	uart_emul.c		\
	virtio.c		\
	xmsr.c			\
	spinup_ap.c

.PATH:  ${SYSDIR}/sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

LIBADD=	vmmapi md pthread

WARNS?=	2

.include <bsd.prog.mk>
