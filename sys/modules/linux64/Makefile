# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

KMOD=	linux64
SRCS=	linux_fork.c linux_dummy.c linux_emul.c linux_file.c \
	linux_futex.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux_machdep.c linux_mib.c linux_misc.c linux_signal.c \
	linux_socket.c linux_stats.c linux_sysctl.c linux_sysent.c \
	linux_sysvec.c linux_util.c linux_time.c linux_vdso.c\
	opt_inet6.h opt_compat.h opt_kdtrace.h opt_posix.h opt_usb.h \
	vnode_if.h device_if.h bus_if.h assym.s
		
# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_apic.h opt_hwpmc_hooks.h

VDSO=	linux_vdso

OBJS=	linux_support.o ${VDSO}.so

EXPORT_SYMS=
EXPORT_SYMS+=	linux_emul_path
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_ifname
EXPORT_SYMS+=	linux_ioctl_register_handler
EXPORT_SYMS+=	linux_ioctl_unregister_handler

CLEANFILES=	linux_assym.h linux_genassym.o linux_locore.o

linux_assym.h: linux_genassym.o
.if exists(@)
linux_assym.h: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh linux_genassym.o > ${.TARGET}

linux_locore.o: linux_locore.s linux_assym.h
	${CC} -x assembler-with-cpp -DLOCORE -shared -mcmodel=small	\
	-pipe -I. -I@ -Werror -Wall -fno-common -nostdinc -nostdlib	\
	-Wl,-T${.CURDIR}/../../${MACHINE_CPUARCH}/linux/${VDSO}.lds.s	\
	-Wl,-soname=${VDSO}.so.1,-fPIC,-warn-common			\
	    ${.IMPSRC} -o ${.TARGET}


#	-fno-omit-frame-pointer	-foptimize-sibling-calls					\


${VDSO}.so: linux_locore.o
	${OBJCOPY} --input binary --output elf64-x86-64-freebsd		\
	    -S -g --binary-architecture i386:x86-64 linux_locore.o ${.TARGET}

linux_support.o: linux_support.s assym.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux_genassym.o: linux_genassym.c linux.h @ machine
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

.if !defined(KERNBUILDDIR)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}

.if defined(DEBUG)
CFLAGS+=-DDEBUG
.endif
.if defined(KTR)
CFLAGS+=-DKTR
.endif
.endif

.include <bsd.kmod.mk>

CFLAGS.linux_locore.s=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
