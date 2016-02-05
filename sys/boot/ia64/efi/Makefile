# $FreeBSD$

MAN=

.include <bsd.own.mk>
MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=

.if ${MK_ZFS} != "no"
# Disable warnings that are currently incompatible with the zfs boot code
CWARNFLAGS.zfs_module.c += -Wno-array-bounds
CWARNFLAGS.zfs_module.c += -Wno-cast-align
CWARNFLAGS.zfs_module.c += -Wno-cast-qual
CWARNFLAGS.zfs_module.c += -Wno-missing-prototypes
CWARNFLAGS.zfs_module.c += -Wno-sign-compare
CWARNFLAGS.zfs_module.c += -Wno-unused-parameter
CWARNFLAGS.zfs_module.c += -Wno-unused-function
.endif

SRCS=		conf.c efimd.c main.c pal.S start.S vers.c
.if ${MK_ZFS} != "no"
SRCS+=		zfs_module.c
.endif

.PATH: ${.CURDIR}/../../../${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

CFLAGS+=	-I${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_CPUARCH}
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

.if ${MK_ZFS} != "no"
CFLAGS+=	-I${.CURDIR}/../../zfs/
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs/
CFLAGS+=	-DEFI_ZFS_BOOT
.endif

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -shared -symbolic

${PROG}: ${LDSCRIPT}

NEWVERSWHAT=	"EFI boot" ${MACHINE_CPUARCH}

vers.c: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

FILES=		loader.efi
FILESMODE_loader.efi= ${BINMODE}

loader.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .data -j .dynamic -j .dynstr -j .dynsym -j .hash \
	    -j .rela.dyn -j .reloc -j .sdata -j .text \
	    --target=efi-app-${MACHINE_CPUARCH} ${.ALLSRC} ${.TARGET}

CLEANFILES=	vers.c loader.efi

LIBIA64=	${.OBJDIR}/../common/libia64.a
LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a
.if ${MK_FORTH} != "no"
LIBFICL=        ${.OBJDIR}/../../ficl/libficl.a
.endif

DPADD=		${LIBIA64} ${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		-Wl,--whole-archive ${LIBIA64} -Wl,--no-whole-archive \
		    ${LIBFICL} ${LIBEFI} -lstand

.include <bsd.prog.mk>
