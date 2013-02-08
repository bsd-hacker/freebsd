/*-
 * Copyright (c) 2000 Alfred Perlstein <alfred@freebsd.org>
 * Copyright (c) 2000 Paul Saab <ps@freebsd.org>
 * Copyright (c) 2000 John Baldwin <jhb@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <stdarg.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>

#include <net.h>
#include <netif.h>
#ifdef LOADER_NFS_SUPPORT
#include <nfsv2.h>
#endif
#include <iodesc.h>

#include <bootp.h>
#include <bootstrap.h>
#include "btxv86.h"
#include "pxe.h"

#include "pxe_core.h"
#include "pxe_dhcp.h"
#include "pxe_isr.h"
#include "pxe_ip.h"
#include "pxe_udp.h"


#define	PXE_TFTP_BUFFER_SIZE	512

#ifndef PXEHTTP_UDP_FOR_LIBSTAND
extern uint8_t *scratch_buffer;
extern uint8_t *data_buffer;
#endif

static pxenv_t	*pxenv_p = NULL;        /* PXENV+ */
static pxe_t	*pxe_p   = NULL;	/* !PXE */

static int	pxe_sock = -1;
static int	pxe_opens = 0;

void		pxe_enable(void *pxeinfo);

static int	pxe_init(void);
static int	pxe_strategy(void *devdata, int flag, daddr_t dblk,
			     size_t size, char *buf, size_t *rsize);
static int	pxe_open(struct open_file *f, ...);
static int	pxe_close(struct open_file *f);
static void     pxe_print(int verbose);
static void	pxe_cleanup(void);
#ifdef LOADER_NFS_SUPPORT
static void	pxe_setnfshandle(char *rootpath);
#endif

static void	pxe_perror(int error);
static int	pxe_netif_match(struct netif *nif, void *machdep_hint);
static int	pxe_netif_probe(struct netif *nif, void *machdep_hint);
static void	pxe_netif_init(struct iodesc *desc, void *machdep_hint);
static int	pxe_netif_get(struct iodesc *desc, void *pkt, size_t len,
			      time_t timeout);
static int	pxe_netif_put(struct iodesc *desc, void *pkt, size_t len);
static void	pxe_netif_end(struct netif *nif);

#ifdef LOADER_NFS_SUPPORT
#ifdef OLD_NFSV2
int nfs_getrootfh(struct iodesc*, char*, u_char*);
#else
int nfs_getrootfh(struct iodesc*, char*, uint32_t*, u_char*);
#endif
#endif

extern struct netif_stats	pxe_st[];

struct netif_dif pxe_ifs[] = {
/*      dif_unit        dif_nsel        dif_stats       dif_private     */
	{0,             1,              &pxe_st[0],     0}
};

struct netif_stats pxe_st[NENTS(pxe_ifs)];

struct netif_driver pxenetif = {
	"pxenet",
	pxe_netif_match,
	pxe_netif_probe,
	pxe_netif_init,
	pxe_netif_get,
	pxe_netif_put,
	pxe_netif_end,
	pxe_ifs,
	NENTS(pxe_ifs)
};

struct netif_driver *netif_drivers[] = {
	&pxenetif,
	NULL
};

struct devsw pxedisk = {
	"pxe", 
	DEVT_NET,
	pxe_init,
	pxe_strategy, 
	pxe_open, 
	pxe_close, 
	noioctl,
	pxe_print,
	pxe_cleanup
};

/*
 * This function is called by the loader to enable PXE support if we
 * are booted by PXE.  The passed in pointer is a pointer to the
 * PXENV+ structure.
 */
void
pxe_enable(void *pxeinfo)
{
	pxenv_p  = (pxenv_t *)pxeinfo;
	pxe_p    = (pxe_t *)PTOV(pxenv_p->PXEPtr.segment * 16 +
				 pxenv_p->PXEPtr.offset);
}

/* 
 * return true if pxe structures are found/initialized,
 * also figures out our IP information via the pxe cached info struct 
 */
static int
pxe_init(void)
{
        if (__pxe_nic_irq != 0)
                return (2);
        return pxe_core_init(pxenv_p, pxe_p);
}


static int
pxe_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
		char *buf, size_t *rsize)
{
	return (EIO);
}

static int
pxe_open(struct open_file *f, ...)
{
	va_list	args;
	char	*devname = NULL;
	int	i = 0;
	
        va_start(args, f);
	devname = va_arg(args, char*);
	va_end(args);
	
	if (pxe_opens == 0) {
		/* Find network interface. */
		if (pxe_sock < 0) {
	    		pxe_sock = netif_open(devname);
	    
			if (pxe_sock < 0) {
			        printf("pxe_open: netif_open() failed\n");
				return (ENXIO);
			}
	    
		}

#ifdef 	PXE_BOOTP_USE_LIBSTAND
		const PXE_IPADDR *addr = pxe_get_ip(PXE_IP_ROOT);

		if ( (addr->ip == 0)) {
			pxe_dhcp_query(0);
			pxe_core_update_bootp();

#ifdef PXEHTTP_UDP_FOR_LIBSTAND
			gateip.s_addr = pxe_get_ip(PXE_IP_GATEWAY)->ip;
			rootip.s_addr = pxe_get_ip(PXE_IP_ROOT)->ip;
			netmask = pxe_get_ip(PXE_IP_NETMASK)->ip;
			myip.s_addr = pxe_get_ip(PXE_IP_MY)->ip;
			nameip.s_addr = pxe_get_ip(PXE_IP_NAMESERVER)->ip;
#endif
    		}
#endif	/* PXE_BOOTP_USE_LIBSTAND */
	}
	++pxe_opens;
	f->f_devdata = &pxe_sock;
	
	return (0);
}

static int
pxe_close(struct open_file *f)
{

       	/* On last close, do netif close, etc. */
	f->f_devdata = NULL;
    
	if (pxe_opens)
		--pxe_opens;
	    
	/* Not last close? */
	if (pxe_opens > 0)
		return (0);

#ifdef LOADER_NFS_SUPPORT
    /* get an NFS filehandle for our root filesystem */
    pxe_setnfshandle(rootpath);
#endif

	if (pxe_sock >= 0) {
#ifdef PXE_DEBUG
		printf("pxe_close: calling netif_close()\n");
#endif
		netif_close(pxe_sock);
		pxe_sock = -1;
	}
    
	return (0);
}

static void
pxe_print(int verbose)
{
        printf("    pxenet0:    MAC %6D\n", pxe_get_mymac(), ":");
        printf("        ISR:    at %x:%x (chained at: %x:%x)\n",
                __pxe_entry_seg, __pxe_entry_off,
                __chained_irq_seg, __chained_irq_off);

        return;
}

static void
pxe_cleanup(void)
{
        pxe_core_shutdown();
}

void
pxe_perror(int err)
{
	return;
}

#ifdef LOADER_NFS_SUPPORT
/*
 * Reach inside the libstand NFS code and dig out an NFS handle
 * for the root filesystem.
 */
#ifdef OLD_NFSV2
struct nfs_iodesc {
	struct	iodesc	*iodesc;
	off_t	off;
	u_char	fh[NFS_FHSIZE];
	/* structure truncated here */
};
extern struct	nfs_iodesc nfs_root_node;
extern int      rpc_port;

static void
pxe_rpcmountcall()
{
	struct	iodesc *d;
	int     error;

	if (!(d = socktodesc(pxe_sock)))
		return;
        d->myport = htons(--rpc_port);
        d->destip = rootip;
	if ((error = nfs_getrootfh(d, rootpath, nfs_root_node.fh)) != 0) 
		printf("NFS MOUNT RPC error: %d\n", error);
	nfs_root_node.iodesc = d;
}

static void
pxe_setnfshandle(char *rootpath)
{
	int	i;
	u_char	*fh;
	char	buf[2 * NFS_FHSIZE + 3], *cp;

	/*
	 * If NFS files were never opened, we need to do mount call
	 * ourselves. Use nfs_root_node.iodesc as flag indicating
	 * previous NFS usage.
	 */
	if (nfs_root_node.iodesc == NULL)
		pxe_rpcmountcall();

	fh = &nfs_root_node.fh[0];
	buf[0] = 'X';
	cp = &buf[1];
	for (i = 0; i < NFS_FHSIZE; i++, cp += 2)
		sprintf(cp, "%02x", fh[i]);
	sprintf(cp, "X");
	setenv("boot.nfsroot.nfshandle", buf, 1);
}
#else	/* !OLD_NFSV2 */

#define	NFS_V3MAXFHSIZE		64

struct nfs_iodesc {
	struct iodesc *iodesc;
	off_t off;
	uint32_t fhsize;
	u_char fh[NFS_V3MAXFHSIZE];
	/* structure truncated */
};
extern struct nfs_iodesc nfs_root_node;
extern int rpc_port;

static void
pxe_rpcmountcall()
{
	struct iodesc *d;
	int error;

	if (!(d = socktodesc(pxe_sock)))
		return;
        d->myport = htons(--rpc_port);
        d->destip = rootip;
	if ((error = nfs_getrootfh(d, rootpath, &nfs_root_node.fhsize,
	    nfs_root_node.fh)) != 0) {
		printf("NFS MOUNT RPC error: %d\n", error);
		nfs_root_node.fhsize = 0;
	}
	nfs_root_node.iodesc = d;
}

static void
pxe_setnfshandle(char *rootpath)
{
	int i;
	u_char *fh;
	char buf[2 * NFS_V3MAXFHSIZE + 3], *cp;

	/*
	 * If NFS files were never opened, we need to do mount call
	 * ourselves. Use nfs_root_node.iodesc as flag indicating
	 * previous NFS usage.
	 */
	if (nfs_root_node.iodesc == NULL)
		pxe_rpcmountcall();

	fh = &nfs_root_node.fh[0];
	buf[0] = 'X';
	cp = &buf[1];
	for (i = 0; i < nfs_root_node.fhsize; i++, cp += 2)
		sprintf(cp, "%02x", fh[i]);
	sprintf(cp, "X");
	setenv("boot.nfsroot.nfshandle", buf, 1);
	sprintf(buf, "%d", nfs_root_node.fhsize);
	setenv("boot.nfsroot.nfshandlelen", buf, 1);
}
#endif	/* OLD_NFSV2 */
#endif /* LOADER_NFS_SUPPORT */

static int
pxe_netif_match(struct netif *nif, void *machdep_hint)
{
	return 1;
}


static int
pxe_netif_probe(struct netif *nif, void *machdep_hint)
{
#ifdef PXE_DEBUG
        printf("pxe_netif_probe() called.");
#endif

#ifdef PXEHTTP_UDP_FOR_LIBSTAND
        if (__pxe_nic_irq == 0)
                return (-1);
#else
	t_PXENV_UDP_OPEN *udpopen_p = (t_PXENV_UDP_OPEN *)scratch_buffer;

	bzero(udpopen_p, sizeof(*udpopen_p));

        const PXE_IPADDR *my = pxe_get_ip(PXE_IP_MY);
	udpopen_p->src_ip = my->ip;
	pxe_call(PXENV_UDP_OPEN);

	if (udpopen_p->status != 0) {
		printf("pxe_netif_probe: failed %x\n", udpopen_p->status);
		return -1;
	}
#endif
	return 0;
}

static void
pxe_netif_end(struct netif *nif)
{
#ifdef PXE_DEBUG
        printf("pxe_netif_end() called.");
#endif
#ifndef PXEHTTP_UDP_FOR_LIBSTAND
        t_PXENV_UDP_CLOSE *udpclose_p = (t_PXENV_UDP_CLOSE *)scratch_buffer;

        bzero(udpclose_p, sizeof(*udpclose_p));

        if (udpclose_p->status != 0)
                printf("pxe_end failed %x\n", udpclose_p->status);
#endif
}

static void
pxe_netif_init(struct iodesc *desc, void *machdep_hint)
{
#ifdef PXE_DEBUG
        printf("pxe_netif_init(): called.\n");
#endif
        uint8_t *mac = (uint8_t *)pxe_get_mymac();

	int i;
	for (i = 0; i < 6; ++i)
		desc->myea[i] = mac[i];

        const PXE_IPADDR *my = pxe_get_ip(PXE_IP_MY);
	desc->xid = my->ip;
}

static int
pxe_netif_get(struct iodesc *desc, void *pkt, size_t len, time_t timeout)
{
#ifdef PXE_DEBUG
        printf("pxe_netif_put(): called.\n");
#endif
	return len;
}

static int
pxe_netif_put(struct iodesc *desc, void *pkt, size_t len)
{
#ifdef PXE_DEBUG
        printf("pxe_netif_put(): called.\n");
#endif
	return len;
}

#ifdef PXEHTTP_UDP_FOR_LIBSTAND
/* new versions of udp send/recv functions  */
ssize_t
sendudp(struct iodesc *h, void *pkt, size_t len)
{
#ifdef PXE_DEBUG_HELL
        printf("sendudp(): sending %u bytes  from me:%u -> %s:%u\n",
                len, ntohs(h->myport),
                inet_ntoa(h->destip), ntohs(h->destport));
#endif
        void *ipdata = pkt - sizeof(PXE_UDP_PACKET);
        PXE_IPADDR      dst;
        dst.ip = h->destip.s_addr;
        if (!pxe_udp_send(ipdata, &dst, ntohs(h->destport),
               ntohs(h->myport), len + sizeof(PXE_UDP_PACKET)))
        {
                 printf("sendudp(): failed\n");
                 return (-1);
        }
        return (len);
}

ssize_t
readudp(struct iodesc *h, void *pkt, size_t len, time_t timeout)
{
        PXE_UDP_DGRAM   dgram;
        struct udphdr   *uh = (struct udphdr *) pkt - 1;

        /* process any queued incoming packets */
        pxe_core_recv_packets();

        /* reading from default socket */
        int recv = pxe_udp_read(NULL, pkt, len, &dgram);

        if (recv == -1) {
                printf("readudp(): failed\n");
                return (-1);
        }
#ifdef PXE_DEBUG_HELL
        printf("readudp(): received %d(%u/%u) bytes from %u port\n",
                recv, len, dgram.size, dgram.src_port);
#endif
        uh->uh_sport = htons(dgram.src_port);
        return (recv);
}

#else /* !defined(PXEHTTP_UDP_FOR_LIBSTAND) */
/* old variants of udp send/recv functions */
ssize_t
sendudp(struct iodesc *h, void *pkt, size_t len)
{
	t_PXENV_UDP_WRITE *udpwrite_p = (t_PXENV_UDP_WRITE *)scratch_buffer;
	bzero(udpwrite_p, sizeof(*udpwrite_p));
	
	udpwrite_p->ip             = h->destip.s_addr;
	udpwrite_p->dst_port       = h->destport;
	udpwrite_p->src_port       = h->myport;
	udpwrite_p->buffer_size    = len;
	udpwrite_p->buffer.segment = VTOPSEG(pkt);
	udpwrite_p->buffer.offset  = VTOPOFF(pkt);

	if (netmask == 0 || SAMENET(myip, h->destip, netmask))
		udpwrite_p->gw = 0;
	else
		udpwrite_p->gw = gateip.s_addr;

	pxe_core_call(PXENV_UDP_WRITE);

#if 0
	/* XXX - I dont know why we need this. */
	delay(1000);
#endif
	if (udpwrite_p->status != 0) {
		/* XXX: This happens a lot.  It shouldn't. */
		if (udpwrite_p->status != 1)
			printf("sendudp failed %x\n", udpwrite_p->status);
		return -1;
	}
	return len;
}

ssize_t
readudp(struct iodesc *h, void *pkt, size_t len, time_t timeout)
{
	t_PXENV_UDP_READ *udpread_p = (t_PXENV_UDP_READ *)scratch_buffer;
	struct udphdr *uh = NULL;
	
	uh = (struct udphdr *) pkt - 1;
	bzero(udpread_p, sizeof(*udpread_p));
	
	udpread_p->dest_ip        = h->myip.s_addr;
	udpread_p->d_port         = h->myport;
	udpread_p->buffer_size    = len;
	udpread_p->buffer.segment = VTOPSEG(data_buffer);
	udpread_p->buffer.offset  = VTOPOFF(data_buffer);

	pxe_core_call(PXENV_UDP_READ);

#if 0
	/* XXX - I dont know why we need this. */
	delay(1000);
#endif
	if (udpread_p->status != 0) {
		/* XXX: This happens a lot.  It shouldn't. */
		if (udpread_p->status != 1)
			printf("readudp failed %x\n", udpread_p->status);
		return -1;
	}
	bcopy(data_buffer, pkt, udpread_p->buffer_size);
	uh->uh_sport = udpread_p->s_port;
	return udpread_p->buffer_size;
}
#endif /* PXEHTTP_UDP_FOR_LIBSTAND */
