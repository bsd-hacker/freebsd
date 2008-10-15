# $FreeBSD$

.PATH: ${.CURDIR}/../../kern

KMOD=	aio
SRCS=	vfs_aio.c opt_vfs_aio.h vnode_if.h

EXPORT_SYMS= aio_init_aioinfo aio_aqueue max_aio_queue_count \
	max_aio_queue_per_proc

.include <bsd.kmod.mk>
