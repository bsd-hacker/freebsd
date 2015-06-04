# pxe_http project
#
.include	<bsd.prog.mk>

LIB=		pxe_http
INTERNALLIB=

SRCS=	pxe_isr.S pxe_mem.c pxe_buffer.c pxe_await.c pxe_arp.c pxe_ip.c \
	pxe_core.c pxe_icmp.c pxe_udp.c pxe_filter.c pxe_dns.c		\
	pxe_dhcp.c pxe_segment.c pxe_tcp.c pxe_sock.c 			\
	pxe_connection.c pxe_http.c httpfs.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../../contrib/dev/acpica \
		-I${.CURDIR}/../../.. -I. -I$(.CURDIR)/.. -I${.CURDIR}/../libi386/

CFLAGS.pxe_isr.S=	${CLANG_NO_IAS}
CFLAGS+=                ${CFLAGS.${.IMPSRC:T}}

# the location of libstand
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

#debug flag
CFLAGS+=	-DPXE_DEBUG
CFLAGS+=	-DPXE_DEBUG_HELL

# core module debug
CFLAGS+=	-DPXE_CORE_DEBUG_HELL
CFLAGS+=	-DPXE_CORE_DEBUG
# TCP module debug 
CFLAGS+=	-DPXE_TCP_DEBUG
CFLAGS+=	-DPXE_TCP_DEBUG_HELL
# IP module debug
CFLAGS+=	-DPXE_IP_DEBUG
CFLAGS+=	-DPXE_IP_DEBUG_HELL
# ARP module debug
CFLAGS+=	-DPXE_ARP_DEBUG
CFLAGS+=	-DPXE_ARP_DEBUG_HELL
# httpfs module
CFLAGS+=	-DPXE_HTTP_DEBUG
CFLAGS+=	-DPXE_HTTP_DEBUG_HELL

# define to get more PXE related code and testing functions
CFLAGS+=	-DPXE_MORE

# define to get some speed up by bigger requests
CFLAGS+=	-DPXE_HTTPFS_CACHING

# define to send packets freqently to speed up connection
#CFLAGS+=	-DPXE_TCP_AGRESSIVE

CFLAGS+=	-DPXE_BOOTP_USE_LIBSTAND

.include <bsd.lib.mk>
