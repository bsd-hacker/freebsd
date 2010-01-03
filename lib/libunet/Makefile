
PREFIX= ${.CURDIR}/../../sys

.PATH: ${PREFIX}/kern
.PATH: ${PREFIX}/libkern
.PATH: ${PREFIX}/net
.PATH: ${PREFIX}/netinet

LIB=	unet

UNET_KERN_COMMON_OBJS +=	\
	kern_mbuf.o		\
	kern_subr.o		\
	subr_eventhandler.o	\
	subr_sbuf.o		\
	uipc_mbuf.o		\
	uipc_mbuf2.o		\
	uipc_domain.o		\
	uipc_sockbuf.o		\
	uipc_socket.o

UNET_NET_COMMON_OBJS +=		\
	if.o			\
	if_clone.o		\
	if_dead.o		\
	if_ether.o		\
	if_loop.o		\
	if_llatbl.o		\
	netisr.o		\
	pfil.o			\
	radix.o			\
	raw_cb.o		\
	raw_usrreq.o		\
	route.o			\
	rtsock.o		

UNET_NETINET_COMMON_OBJS +=	\
	igmp.o			\
	in.o			\
	in_cksum.o		\
	in_mcast.o		\
	in_pcb.o		\
	in_proto.o		\
	in_rmx.o		\
	ip_encap.o		\
	ip_icmp.o		\
	ip_id.o			\
	ip_input.o		\
	ip_options.o		\
	ip_output.o		\
	raw_ip.o		\
	tcp_debug.o		\
	tcp_hostcache.o		\
	tcp_input.o		\
	tcp_lro.o		\
	tcp_offload.o		\
	tcp_output.o		\
	tcp_reass.o		\
	tcp_sack.o		\
	tcp_subr.o		\
	tcp_syncache.o		\
	tcp_timer.o		\
	tcp_timewait.o		\
	tcp_usrreq.o		\
	udp_usrreq.o

UNET_LIBKERN_COMMON_OBJS =	\
	bcmp.o			\
	inet_aton.o		\
	inet_ntoa.o		\
	memcmp.o		\
	strcat.o		\
	strcmp.o		\
	strcpy.o		\
	strlcpy.o		\
	strlen.o		\
	strncmp.o		\
	strtoul.o

UNET_GLUE_COMMON_OBJS =		\
	unet_init.o		\
	unet_uipc_syscalls.o


UNET_COMMON_OBJS =			\
	${UNET_KERN_COMMON_OBJS}	\
	${UNET_LIBKERN_COMMON_OBJS}	\
	${UNET_NET_COMMON_OBJS}		\
	${UNET_NETINET_COMMON_OBJS}	

UNET_COMMON_SRCS= ${UNET_COMMON_OBJS:C/.o$/.c/}

SRCS=	${UNET_COMMON_SRCS}

CFLAGS+=	-I${PREFIX}
CFLAGS+=	-I.
CFLAGS+=	-D_KERNEL
CFLAGS+=	-DUNET
CFLAGS+=	-fno-builtin

.include <bsd.lib.mk>


