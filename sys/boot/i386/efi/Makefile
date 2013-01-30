# $FreeBSD$

NO_MAN=
WITHOUT_SSP=
BUILDING_EFI=

.include <bsd.own.mk>

PROG=		loader.sym
INTERNALPROG=

# architecture-specific loader code
SRCS=	main.c exec.c conf.c vers.c reloc.c elf32_freebsd.c
SRCS+=	i386_copy.c bootinfo.c autoload.c devicename.c efimd.c

.PATH:	${.CURDIR}/${MACHINE_CPUARCH}
SRCS+=	start.S

CFLAGS+=	-fPIC
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/i386

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_CPUARCH}
.if ${MACHINE_CPUARCH} == "amd64"
LIBFICL=	${.OBJDIR}/../../ficl64/libficl.a
.else
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif
.endif

# Include bcache code.
HAVE_BCACHE=    yes

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common

FILES=	loader.efi
FILESMODE_loader.efi=	${BINMODE}

LDSCRIPT=	${.CURDIR}/ldscript.${MACHINE_CPUARCH}
LDFLAGS=	-Wl,-T${LDSCRIPT} -Wl,-Bsymbolic -shared -Wl,-znocombreloc

${PROG}:	${LDSCRIPT}

CLEANFILES=	vers.c loader.efi

NEWVERSWHAT=	"EFI loader" ${MACHINE_CPUARCH}

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

.if ${MACHINE_CPUARCH} == "amd64"
EFI_TARGET=	efi-app-x86_64
.else
EFI_TARGET=	efi-app-ia32
.endif

loader.efi: loader.sym
	if [ `${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*' | wc -l` != 0 ]; then \
		${OBJDUMP} -t ${.ALLSRC} | fgrep '*UND*'; \
		exit 1; \
	fi
	${OBJCOPY} -j .text -j .sdata -j .data \
                -j .dynamic -j .dynsym -j .rel.dyn \
                -j .rela.dyn -j .reloc -j .eh_frame -j set_Xcommand_set \
	        --target=${EFI_TARGET} ${.ALLSRC} ${.TARGET}

LIBEFI=		${.OBJDIR}/../../efi/libefi/libefi.a
CFLAGS+=	-I${.CURDIR}/../libi386
CFLAGS+=	-I${.CURDIR}/../btx/lib

DPADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBEFI} ${LIBSTAND}

.include <bsd.prog.mk>
