/*-
 * Copyright (c) 2007 Alexey Tarasov
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
 *
 */
 
#include <stand.h>
#include <string.h>

#include <netinet/in.h>

#include "btxv86.h"
#include "pxe.h"

#include "pxe_arp.h"
#include "pxe_core.h"
#include "pxe_dhcp.h"
#ifdef PXE_MORE
#include "pxe_icmp.h"
#endif
#include "pxe_ip.h"
#include "pxe_isr.h"
#include "pxe_mem.h"
#include "pxe_udp.h"

/* PXE API calls here will be made in same way as in pxeboot.
 * the only difference - installation of isr, that was not needed in pxe.c.
 */

/* stores data in packet */
static int pxe_core_recieve(PXE_PACKET *pack, void *data, size_t size);
	
uint8_t		*scratch_buffer = NULL;
uint8_t		*data_buffer = NULL;

#ifdef PXE_CORE_STATIC_BUFFERS
static uint8_t		static_scratch_buffer[PXE_BUFFER_SIZE];
static uint8_t		static_data_buffer[PXE_BUFFER_SIZE];
#endif

#ifdef PXE_EXCLUSIVE
static uint8_t		exclusive_protocol = 0;
#endif
static pxenv_t		*pxenv = NULL;	/* PXENV+ */
static pxe_t		*pxe   = NULL;	/* !PXE */

/* pxe core structures*/
/* current processing packet */
static PXE_PACKET		core_packet;
/* protocol's  callback fuctions */
static pxe_protocol_call	core_protocol[256];

/* NIC info */
static MAC_ADDR			nic_mac;
/* frequently used IP's: server's IP, client's ip and etc */
static PXE_IPADDR		core_ips[PXE_IP_MAX];

static int			pxe_state = PXE_DOWN;

/* pxe_core_update_bootp() - updates core_ips and etc, after
 *		receving of rootpath
 * in/out:
 *	none
 */
void
pxe_core_update_bootp()
{
        const PXE_IPADDR	*paddr = pxe_get_ip(PXE_IP_ROOT);
	int			i = 0;
	char			temp[20];
	
        if (paddr->ip == 0)
                pxe_set_ip(PXE_IP_ROOT, pxe_get_ip(PXE_IP_SERVER));

	struct in_addr tmp_in;

        tmp_in.s_addr = pxe_get_ip(PXE_IP_GATEWAY)->ip;
        setenv("boot.netif.gateway", inet_ntoa(tmp_in), 1);

	/* initing route tables, using DHCP reply data */	
	pxe_ip_route_init(pxe_get_ip(PXE_IP_GATEWAY));
	
#ifdef PXE_DEBUG
        printf("pxe_open: gateway ip:  %s\n", inet_ntoa(tmp_in));
#endif

        tmp_in.s_addr = pxe_get_ip(PXE_IP_MY)->ip;
        setenv("boot.netif.ip", inet_ntoa(tmp_in), 1);

        tmp_in.s_addr = pxe_get_ip(PXE_IP_NETMASK)->ip;
        setenv("boot.netif.netmask", inet_ntoa(tmp_in), 1);

        sprintf(temp, "%6D", pxe_get_mymac(), ":");
        setenv("boot.netif.hwaddr", temp, 1);

        if (!rootpath[1])
                strcpy(rootpath, PXENFSROOTPATH);
	
        for (i = 0; rootpath[i] != '\0' && i < FNAME_SIZE; i++)
		if (rootpath[i] == ':')
		        break;

        if (i && i != FNAME_SIZE && rootpath[i] == ':') {

                rootpath[i++] = '\0';

                const PXE_IPADDR *root_addr = pxe_gethostbyname(rootpath);
                pxe_set_ip(PXE_IP_ROOT, root_addr);

                pxe_memcpy(rootpath, servername, i);
                pxe_memcpy(&rootpath[i], &rootpath[0],
		    strlen(&rootpath[i]) + 1);
	}
	
	tmp_in.s_addr = pxe_get_ip(PXE_IP_ROOT)->ip;

#ifdef PXE_DEBUG
        printf("pxe_open: server addr: %s\n", inet_ntoa(tmp_in));
        printf("pxe_open: server path:  %s\n", rootpath);
#endif
#ifdef LOADER_NFS_SUPPORT
        setenv("boot.nfsroot.server", inet_ntoa(tmp_in), 1);
        setenv("boot.nfsroot.path", rootpath, 1);
#endif

	/* removing '/' at tail of rootpath */
	size_t rlen = strlen(rootpath);
	
	if ( (rlen > 0) && (rootpath[rlen - 1] == '/'))
		rootpath[rlen - 1] = '\0';
	
	/* check if Web server option specified,
	 * if not, make it equal to root ip
	 */
	if (pxe_get_ip(PXE_IP_WWW)->ip == 0) {
		pxe_set_ip(PXE_IP_WWW, pxe_get_ip(PXE_IP_ROOT));
	}
}

/* pxe_core_init() - performs initialization of all PXE related code
 * in:
 *	pxenv_p	- pointer to PXENV+ structure
 *	pxe_p	- pointer to !PXE
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_core_init(pxenv_t *pxenv_p, pxe_t* pxe_p)
{
#ifdef PXE_CORE_DEBUG
	printf("pxe_core_init(): started (pxenv_p = 0x%x, pxe_p = 0x%x).\n",
	    pxenv_p, pxe_p);
#endif
	int     counter = 0;
	uint8_t checksum = 0;
	uint8_t *checkptr = NULL;

#ifdef PXE_CORE_DEBUG
	printf("pxe_core_init(): initing structures....\n");
#endif
	t_PXENV_GET_CACHED_INFO	*gci_p = NULL;

	pxe_memset(&core_packet, 0, sizeof(core_packet));
	pxe_memset(core_protocol, 0, sizeof(core_protocol));
	pxe_memset(core_ips, 0, sizeof(core_ips));
	
	pxenv = pxenv_p;
	pxe = pxe_p;

#ifndef PXE_CORE_STATIC_BUFFERS
	/* 0. initing scratch and data buffers */
	data_buffer = pxe_alloc(PXE_BUFFER_SIZE);

	if (data_buffer == NULL) {
		return (0);
	}
	
	scratch_buffer = pxe_alloc(PXE_BUFFER_SIZE);

	if (scratch_buffer == NULL) {
		pxe_free(data_buffer);
		return (0);
	}
#else
	data_buffer = static_data_buffer;
	scratch_buffer = static_scratch_buffer;
#endif	
	/* 1. determine PXE API entry point */
	if(pxenv_p == NULL)
		return (0);

	/*  look for "PXENV+" */
	if (bcmp((void *)pxenv_p->Signature, S_SIZE("PXENV+"))) {
		pxenv_p = NULL;
		return (0);
	}

	/* make sure the size is something we can handle */
	if (pxenv_p->Length > sizeof(*pxenv_p)) {
	  	printf("PXENV+ structure too large, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}

	/*
	 * do byte checksum:
	 * add up each byte in the structure, the total should be 0
	 */
	checksum = 0;
	checkptr = (uint8_t *) pxenv_p;

	for (counter = 0; counter < pxenv_p->Length; counter++)
		checksum += *checkptr++;

	if (checksum != 0) {
		printf("PXENV+ structure failed checksum, ignoring\n");
		pxenv_p = NULL;
		return (0);
	}

#ifdef PXE_CORE_DEBUG
	printf("pxe_core_init(): PXENV+ checked.\n");
#endif
    
	/*
	 * PXENV+ passed, so use that if !PXE is not available or
	 * the checksum fails.
	 */
	if (pxenv_p->Version >= 0x0200) {

		for (;;) {
			if (bcmp((void *)pxe_p->Signature, S_SIZE("!PXE"))) {
				pxe_p = NULL;
				break;
			}
			
			checksum = 0;
			checkptr = (uint8_t *)pxe_p;
			
			for (counter = 0; counter < pxe_p->StructLength;
			     counter++)
				checksum += *checkptr++;
				
			if (checksum != 0) {
				pxe_p = NULL;
			}
			
			break;
		}
	}

	/* show version and entry point */
	printf("\nPXE v.%d.%d",
	       (uint8_t) (pxenv_p->Version >> 8),
	       (uint8_t) (pxenv_p->Version & 0xFF));

	printf(" @ %04x:%04x\n",
	       pxe_p->EntryPointSP.segment,
	       pxe_p->EntryPointSP.offset);

	/* setting entry point in tramp code */
	__pxe_entry_seg = pxe->EntryPointSP.segment;
	__pxe_entry_off = pxe->EntryPointSP.offset;

	pxe_state = PXE_INITING;
		
	/* 2. getting cached info */
	gci_p = (t_PXENV_GET_CACHED_INFO *) scratch_buffer;
	pxe_memset(gci_p, 0, sizeof(*gci_p)); 
	
	/* getting Boot Server Discovery reply */
	
	/* pointer to PXE Cached information. */
	BOOTPLAYER*	bootplayer = (BOOTPLAYER *)data_buffer;

	gci_p->PacketType = PXENV_PACKET_TYPE_BINL_REPLY;
	gci_p->BufferSize = sizeof(BOOTPLAYER);
	gci_p->Buffer.segment = VTOPSEG(bootplayer);
	gci_p->Buffer.offset = VTOPOFF(bootplayer);

	if ( (!pxe_core_call(PXENV_GET_CACHED_INFO)) ||
	     (gci_p->Status != 0) )
	{
#ifdef PXE_CORE_DEBUG
	    	printf("pxe_core_init(): error status = 0x%x\n", gci_p->Status);
#endif		
		pxe_p = NULL;
		pxe_state = PXE_DOWN;
		return (0);
	}

#ifdef PXE_CORE_DEBUG
	printf("pxe_core_init(): copied %d (%d)bytes of cached packet.\n",
	    gci_p->BufferSize, gci_p->BufferLimit);
#endif

	/* 3. install isr */
	pxe_core_install_isr();
	pxe_state = PXE_READY;
	
	/* 4. open connection to network */
	t_PXENV_UNDI_OPEN *undi_open = (t_PXENV_UNDI_OPEN *)scratch_buffer;

	pxe_memset(undi_open, 0, sizeof(t_PXENV_UNDI_OPEN));
	undi_open->PktFilter = FLTR_DIRECTED | FLTR_BRDCST;
	undi_open->R_Mcast_Buf.MCastAddrCount = 0;
	
	if (!pxe_core_call(PXENV_UNDI_OPEN)) {
		printf("pxe_core_init(): failed to open network connection.\n");
		return (0);
    	}
	
	/* showing information about NIC */
	PXE_IPADDR addr;
	addr.ip = bootplayer->yip;	/* my ip */
	
	printf("my ip: %s\n", inet_ntoa(addr.ip));

	/* my MAC */
	pxe_memcpy(&bootplayer->CAddr, &nic_mac, MAC_ADDR_LEN);
	printf("my MAC: %6D\n", nic_mac, ":");

	/* setting default ips*/
	pxe_set_ip(PXE_IP_MY, &addr);	/* nic ip */

	addr.ip = bootplayer->sip; /* boot server ip */
	pxe_set_ip(PXE_IP_SERVER, &addr);

	/* setting next to default ip (boot server ip) */
	/* nameserver ip*/
	pxe_set_ip(PXE_IP_NAMESERVER, &addr);
	/* gateway ip */
	pxe_set_ip(PXE_IP_GATEWAY, &addr);

	/* web server */
	addr.ip = 0;
	pxe_set_ip(PXE_IP_WWW, &addr);
	
	addr.ip = 0xffffffff;
	/* netmask, default to 255.255.255.0 */
	pxe_set_ip(PXE_IP_NETMASK, &addr);
	/* broadcast address, default to 255.255.255.255 */
	pxe_set_ip(PXE_IP_BROADCAST, &addr);
	
	/* initing modules */	
	pxe_arp_init();
	pxe_filter_init();
#ifdef PXE_MORE
	pxe_icmp_init();
#endif
#ifdef PXE_POOL_SLOTS
	pxe_buffer_init();
#endif
	pxe_socket_init();
	pxe_udp_init();
	pxe_tcp_init();

#ifndef PXE_BOOTP_USE_LIBSTAND 
	/* trying to get gateway/nameserver info from DHCP server */
	pxe_dhcp_query(bootplayer->ident);
	pxe_core_update_bootp();
#endif

#ifdef PXE_CORE_DEBUG
	printf("pxe_core_init(): ended.\n");
#endif	
	
	return (1);
}

/* pxe_core_install_isr() - installs ISR for NIC
 * in/out:
 *	none
 */
void
pxe_core_install_isr()
{
	t_PXENV_UNDI_GET_INFORMATION *undi_info =
		(t_PXENV_UNDI_GET_INFORMATION *)scratch_buffer;

#ifdef PXE_CORE_DEBUG
	printf("pxe_isr_install() called\n");
#endif
	pxe_memset(undi_info, 0, sizeof(t_PXENV_UNDI_GET_INFORMATION));

	if (!pxe_core_call(PXENV_UNDI_GET_INFORMATION)) {
		printf("pxe_core_install_isr(): failed get NIC information.\n");
		return;
	}

	if (undi_info->Status != 0)
		return;

	__pxe_nic_irq = (uint16_t)(undi_info->IntNumber);

	uint8_t int_num = (__pxe_nic_irq < 8) ?
			   __pxe_nic_irq + 0x08 : __pxe_nic_irq + 0x68;
	 
#ifdef PXE_CORE_DEBUG
	printf("pxe_core_install_isr() info:\n");
	printf("\tIRQ (int): %d (%d)\n", undi_info->IntNumber, int_num);
	printf("\tMTU: %d\n", undi_info->MaxTranUnit);
	printf("\tRX/TX buffer queue: %d/%d\n",
	    undi_info->RxBufCt, undi_info->TxBufCt);
#endif
	__pxe_entry_seg2 = pxe->EntryPointSP.segment;
	__pxe_entry_off2 = pxe->EntryPointSP.offset;

	pxe_memset(&v86, 0, sizeof(v86));

	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.addr = (VTOPSEG(__pxe_isr_install) << 16) | VTOPOFF(__pxe_isr_install);
	v86.eax = int_num;
	v86.ebx = VTOPSEG(__pxe_isr);
	v86.edx = VTOPOFF(__pxe_isr);
	v86int();
	v86.ctl  = V86_FLAGS;
	
	printf("\tchained handler @ 0x%x:0x%x\n", v86.ebx, v86.edx);
	
	__chained_irq_seg = v86.ebx;
	__chained_irq_off = v86.edx;

#ifdef PXE_CORE_DEBUG
	printf("pxe_core_install_isr(): success (isr @ 0x%x:0x%x)\n",
	    VTOPSEG(__pxe_isr), VTOPOFF(__pxe_isr));
#endif
}

#ifdef PXE_MORE
/* pxe_core_copy() -  calls __mem_copy() to copy data in real mode
 *		to data buffer, usefull if data is in addresses inaccessible
 *		from user space. TODO!: Check, if really needed.
 * in:
 *	seg_from - segment of source buffer
 *	off_from - offset of source buffer
 *	seg_to	 - segment of destination buffer
 *	off_to	 - offset of destination buffer
 *	size	 - number of bytes to copy
 * out:
 *	none
 */
void
pxe_core_copy(uint16_t seg_from, uint16_t off_from,
	      uint16_t seg_to, uint16_t off_to, uint16_t size)
{

	pxe_memset(&v86, 0, sizeof(v86));

	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.addr = (VTOPSEG(__mem_copy) << 16) | VTOPOFF(__mem_copy);
	v86.eax = (seg_from << 16 ) | off_from;
	v86.ebx = (seg_to << 16 ) | off_to;
	v86.ecx = size;
	v86int();
	v86.ctl  = V86_FLAGS;
}
#endif /* PXE_MORE */

/* pxe_core_remove_isr() - restores default handler for interrupt
 * in/out:
 *	none
 */
void
pxe_core_remove_isr()
{

	pxe_memset(&v86, 0, sizeof(v86));

	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.addr = (VTOPSEG(__pxe_isr_install) << 16) | VTOPOFF(__pxe_isr_install);

	uint8_t int_num = (__pxe_nic_irq < 8) ?
			   __pxe_nic_irq + 0x08 : __pxe_nic_irq + 0x68;	
	
	v86.eax = int_num;
	v86.ebx = __chained_irq_seg;
	v86.edx = __chained_irq_off;
	v86int();
	v86.ctl  = V86_FLAGS;
}

/* pxe_core_shutdown() - shutdown all modules. TODO: add needed modules shutdown.
 * in:
 *	none
 * out:
 *	0 - failed
 *	1 - success
 */
int
pxe_core_shutdown()
{
#ifdef PXE_CORE_DEBUG
	printf("pxe_core_shutdown(): shutdown started.\n");
#endif
	if (core_packet.data)
		pxe_free(core_packet.data);

	/* 1. uninstall isr */
	pxe_core_remove_isr();
	
	pxe_udp_shutdown();

	/* 2. shutdown PXE */
	t_PXENV_UNLOAD_STACK *unload_stack_p =
	    (t_PXENV_UNLOAD_STACK *)scratch_buffer;

	t_PXENV_UNDI_SHUTDOWN *undi_shutdown_p =
	    (t_PXENV_UNDI_SHUTDOWN *)scratch_buffer;
/*
	pxe_core_call(PXENV_UNDI_SHUTDOWN);
	pxe_core_call(PXENV_UNLOAD_STACK);
*/
#ifndef PXE_CORE_STATIC_BUFFERS
	pxe_free(scratch_buffer);
	pxe_free(data_buffer);
#endif
	/* make pxe_core_call() unavailable */
	pxe_state = PXE_DOWN;
	
	return (1);
}

/*
 *  function code is taken from bangpxe_call(), /sys/boot/libi386/pxe.c
 *  needs pxe_isr.s wrapper and vm86int() support.
 *  in:
 *	func - PXE function number
 *  out:
 *	1 - success
 *	0 - failed
 */
int
pxe_core_call(int func)
{
#ifdef PXE_CORE_DEBUG_HELL
	printf("pxe_core_call(): func = 0x%x...", func);
#endif
	if (pxe_state == PXE_DOWN) {
		printf("pxe_core_call(): internal error, PXE shutdowned.\n");
		return (0);
	}
	
	pxe_memset(&v86, 0, sizeof(v86));

	v86.ctl  = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.edx  = VTOPSEG(scratch_buffer);
	v86.eax  = VTOPOFF(scratch_buffer);
	v86.addr = (VTOPSEG(__pxe_call) << 16) | VTOPOFF(__pxe_call);
	v86.ebx  = func;
	v86int();
	v86.ctl  = V86_FLAGS;
	
	int call_status = v86.eax;
	int status = *((uint16_t *)scratch_buffer);
	
#ifdef PXE_CORE_DEBUG_HELL
	printf("%s (0x%x)\n", (call_status == 0) ? "?OK" : "?NOK", status );
#endif
	return (status == 0) ? 1 : 0;
}

/* pxe_core_transmit() - transmits packet to network
 * in:
 *	pack - packet definition structure.
 * out:
 *	0 - failed
 *	1 - success
 */
int
pxe_core_transmit(PXE_PACKET *pack)
{
	t_PXENV_UNDI_TRANSMIT *undi_send =
		(t_PXENV_UNDI_TRANSMIT *)scratch_buffer;

	pxe_memset(undi_send, 0, sizeof(t_PXENV_UNDI_TRANSMIT));

	t_PXENV_UNDI_TBD	tbd;
	pxe_memset(&tbd, 0, sizeof(t_PXENV_UNDI_TBD));
	
	tbd.ImmedLength = pack->data_size;	/* packet length */
	tbd.Xmit.segment = VTOPSEG(pack->data);	/* immediate transmit buffer */
	tbd.Xmit.offset = VTOPOFF(pack->data);	/*  segment & offset */
	tbd.DataBlkCount = 0 ;			/* only immediate data */

	undi_send->Protocol = pack->protocol;

	undi_send->TBD.segment = VTOPSEG(&tbd);	/* SEGOFF16 to xmit block data */
	undi_send->TBD.offset = VTOPOFF(&tbd);
	
	/* if not broadcast packet, specify destination media address */
	undi_send->XmitFlag =
	    (pack->flags & PXE_BCAST) ? XMT_BROADCAST : XMT_DESTADDR;
	
	if (undi_send->XmitFlag == XMT_DESTADDR) {
    	    undi_send->DestAddr.segment = VTOPSEG(pack->dest_mac);
	    undi_send->DestAddr.offset  = VTOPOFF(pack->dest_mac);
	}

#ifdef PXE_CORE_DEBUG_HELL
	printf("pxe_core_transmit(): %s %6D, proto = %d, %d bytes\n",
	    (undi_send->XmitFlag == XMT_DESTADDR) ? "to" : "bcast",
	    pack->dest_mac, ":", undi_send->Protocol, pack->data_size);
#endif
	
/* NOTE: it is not needed, we use only immediate block */
/*	we've inited undi_info with zero, so two lines below are not needed */
/*	tbd.DataBlk[0].TDRsvdByte = 0;			/* reserved */
/*	tbd.DataBlk[1].TDRsvdByte = 0;			/* reserved */
/*	tbd.DataBlock[0].TDDataLen=tbd.ImmedLength;	/* size of packet*/
/*	tbd.DataBlock[0].TDPtrType = 1;			/* segment:offset type */
/* 	segment and offset to data */
/*	tbd.DataBlock[0].TDDataPtr.segment = VTOPSEG(pack->data);
 *	tbd.DataBlock[0].TDDataPtr.offset = VTOPOFF(pack->data); */
 
	int status = 0;		/* PXE call status */
	int tryCount = 0;	/* tryCount for packet resending */
	
	for (; tryCount < 5; ++tryCount) {
		status = pxe_core_call(PXENV_UNDI_TRANSMIT);

		if (undi_send->Status != 0) {
			printf("%d: pxe_core_transmit(): failed with status 0x%x\n",
			    tryCount, undi_send->Status);
			delay(100);
	    		continue;
		}
		
		if (status != 0)
			break;
	}

	return (status);
}

/* pxe_core_get_packet() - checks, if there are any new packets in receive queue
 * in:
 *	func	- function to fill in FuncFlag of t_PXENV_UND_ISR structure
 *	undi_isr- pointer to t_PXENV_UND_ISR, used to return data (sizes and etc)
 * out:
 *	0 - failed
 *	1 - success
 */
static int
pxe_core_get_packet(int func, t_PXENV_UNDI_ISR *undi_isr )
{
#ifdef PXE_CORE_DEBUG_HELL
	printf("get_packet(): started with func %d\n", func);
#endif
	
	undi_isr->FuncFlag = func;
	int count = 0;

	while(1) {	/* cycle to handle busy flag */

		undi_isr->Status = 0;

		if (!pxe_core_call(PXENV_UNDI_ISR)) {
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): failed.\n");
#endif		
		}

		if (undi_isr->Status != 0) {
			/* something gone wrong */
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): fail status =  0x%x.\n",
			    undi_isr->Status);
#endif					
			return (0);
		}

		if (undi_isr->FuncFlag == PXENV_UNDI_ISR_OUT_DONE) {
			/* nothing to do */
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): all is already done.\n");
#endif					
			return (0);
		}

		if (undi_isr->FuncFlag == PXENV_UNDI_ISR_OUT_BUSY) {
			/* NIC is busy, wait */
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): device is busy.\n");
#endif					
			++count;
			if (count == 10)
				return (0);
			    
			delay(10); /* wait, may be it will be not busy later */

			continue;
		}

		if (undi_isr->FuncFlag == PXENV_UNDI_ISR_OUT_RECIEVE) {
			/* that's what we are waiting for */
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): got packet!\n");
#endif					
			break;
		}

		if (undi_isr->FuncFlag == PXENV_UNDI_ISR_OUT_TRANSMIT) {
			/* transmitted packet */
#ifdef PXE_CORE_DEBUG_HELL
			printf("get_packet(): transmit packet.\n");
#endif					
			return (0);
		}
	}

	return (1);
}

/* pxe_core_recv_packets() - recieves all packets, if there is any waiting
 *			in receiving queue, and gives it to protocol
 *			callback functions.
 * in:
 *    none
 * out:
 *     0 	-  there is no packets in receiving queue,
 *		  or it's not interesting for us.
 *     positive -  there were packets in queue and some
 *		  protocol handler was interested in it.
 */
int
pxe_core_recv_packets()
{
	/*
	 * TODO: make it simplier to understand, too many ifs, many lines.
	 */
	int	buffer_size = 0;  /* total size of packet*/
	int	protocol = 0;     /* protocol */
	int	received = 0;     /* bytes received to buffer */

	int	frame_size = 0;   /* size of frame */
	int	drop_flag = 0;	  /* 1 if current packet must be dropped */
	/* total count of processed packets during call*/
	int	processed_packets = 0; 

	PXE_PACKET *pack=NULL;	/* allocated packet */
	PXE_PACKET  dummy_pack;	/* temporary struct, used to mimic
				 * real packet struct
				 */

/*	if (__pxe_isr_occured == 0) /* there are no packets for us to handle */
/*		return (0);
*/
	__pxe_isr_occured = 0; /* reset flag */

	t_PXENV_UNDI_ISR *undi_isr =
		(t_PXENV_UNDI_ISR *)scratch_buffer;

	/* starting packet receive cycle */

	int func = PXENV_UNDI_ISR_IN_PROCESS;

packet_start:

	drop_flag = 0;
	pxe_memset(undi_isr, 0, sizeof(t_PXENV_UNDI_ISR));
	
	if (0 == pxe_core_get_packet(func, undi_isr))
		return (processed_packets);

	buffer_size = undi_isr->BufferLength;
	protocol = undi_isr->ProtType;
	frame_size = undi_isr->FrameLength;

	/* check if no packet, it seems  'all is done' */
	if ( (frame_size == 0) && (buffer_size == 0))
		return (processed_packets);
	
#ifdef PXE_CORE_DEBUG_HELL
	printf("pxe_core_recv_packets(): size = %d/%d, proto = %d.\n",
	    frame_size, buffer_size, protocol);
#endif		

	/* we are interested in ARP & IP packets */

	if ( (protocol == PXE_PROTOCOL_UNKNOWN) || 
	     (protocol == PXE_PROTOCOL_RARP) )
	{
#ifdef PXE_CORE_DEBUG
		printf("recv_packets(): not interesting protocol.\n");
#endif		
		drop_flag = 1;	/* clear queue, receiving frames of packet */
	}

#ifdef PXE_EXCLUSIVE
	/* experimental: to avoid resendings of ip packets for unknown MAC */
	if (exclusive_protocol && (protocol != exclusive_protocol)) {
#ifdef PXE_CORE_DEBUG
		printf("recv_packets(): not exclusive protocol (%d != %d).\n",
			protocol, exclusive_protocol);
#endif		
		drop_flag = 1;	/* clear queue, receiving frames of packet */
	}
#endif

#ifdef PXE_CORE_DEBUG
	if (frame_size != buffer_size) {
		printf("recv_packets(): fragmented packet %u/%u\n",
		    frame_size, buffer_size);
	}
#endif	
	/* sanity check */
	if (frame_size < PXE_BUFFER_SIZE) {
/*		pxe_core_copy(	undi_isr->Frame.segment, undi_isr->Frame.offset,
			VTOPSEG(data_buffer), VTOPOFF(data_buffer), frame_size);
 */
		pxe_memcpy(PTOV(undi_isr->Frame.segment * 16 +
				undi_isr->Frame.offset),
			   data_buffer, frame_size);
		    
	} else {
		printf("pxe_core_recv_packets(): not enough buffer (%d bytes) "
		       "for frame size %d bytes\n", PXE_BUFFER_SIZE, frame_size);

		drop_flag = 1;		/* drop this packet */
	}
	
	/* checking first fragment, this may help to avoid memory allocation
	 * and memblock copy in main cycle below
	 */

	if (!drop_flag) {

		PXE_IP_HDR *iphdr =
		    (PXE_IP_HDR *)(data_buffer + MEDIAHDR_LEN_ETH);

		dummy_pack.protocol = protocol;
		dummy_pack.state = PXE_PACKET_STATE_USING;
		
		dummy_pack.raw_data = data_buffer;
		dummy_pack.raw_size = frame_size;
		dummy_pack.data = data_buffer + MEDIAHDR_LEN_ETH;
		dummy_pack.data_size = frame_size - MEDIAHDR_LEN_ETH;		
		

		dummy_pack.user_data = NULL;

		if (protocol == PXE_PROTOCOL_ARP) {
	
			pxe_arp_protocol(&dummy_pack, PXE_CORE_HANDLE);
			++processed_packets;

			/* aasume ARP packet always in one fragment */
		
			func = PXENV_UNDI_ISR_IN_GET_NEXT;

			goto packet_start;
		}

    		/* TODO: calc ip checksum */

		if  ( (!core_protocol[iphdr->protocol]) ||
		      (!core_protocol[iphdr->protocol](
		    		&dummy_pack,
				(buffer_size == frame_size) ?
				PXE_CORE_HANDLE : PXE_CORE_FRAG)) )
		{
			drop_flag =  1;
		} else {

			pack = pxe_core_alloc_packet(buffer_size);

			if (pack == NULL)
				drop_flag = 1;
			else {

				/* pointing user_data to beginning of data.
				 * It's used by pxe_core_receive()
				 * during receiving packet.
				 */
				pack->user_data = pack->data;
			}
		}
	}

	received = frame_size;
	
	while (received < buffer_size) {

		if (!pxe_core_get_packet(PXENV_UNDI_ISR_IN_GET_NEXT, undi_isr))
			break;

		frame_size = undi_isr->FrameLength;

		if (frame_size < PXE_BUFFER_SIZE) {
/*			pxe_core_copy(	undi_isr->Frame.segment, undi_isr->Frame.offset,
				VTOPSEG(data_buffer), VTOPOFF(data_buffer), frame_size);
 */
 			pxe_memcpy(PTOV(undi_isr->Frame.segment * 16 +
					undi_isr->Frame.offset),
				   data_buffer, frame_size);
		} else {
			printf("pxe_core_recv_packets(): not enough buffer (%d bytes)"
			       " for frame size %d bytes.",
			       PXE_BUFFER_SIZE, frame_size);

			drop_flag = 1;		/* drop this packet */
		}

		if (!drop_flag)
			pxe_core_recieve(pack, data_buffer, frame_size);

		received += frame_size;
	}

	if (received < buffer_size) { /* pxe_core_get_packet() in cycle failed */

		if (!drop_flag) {
			pack->state = PXE_PACKET_STATE_FREE;
		}

		return (processed_packets); /* it's failed, finish receive cycle */
	}

	if (!drop_flag) {

		pack->user_data = NULL;

		PXE_IP_HDR *iphdr=(PXE_IP_HDR *)pack->data;

		/* TODO: calc ip checksum */
		pack->protocol = protocol;

		if ( (!core_protocol[iphdr->protocol]) ||
		     (!core_protocol[iphdr->protocol](pack, PXE_CORE_HANDLE)))
		{
			/* protocol not interested in it */
			pack->state = PXE_PACKET_STATE_FREE;
		}
	}

	++processed_packets;
	/* received one or more packets, need check if there are any others */
	
	func = PXENV_UNDI_ISR_IN_GET_NEXT;

	goto packet_start;

	/* never getting here */
	return (0);
}

/* pxe_core_recieve() - recieves sequentially fragments data in packet buffer
 * in:
 *	pack		- packet with buffer to receive in
 *	frame_data	- fragment data buffer
 *	frame_size	- frag,ent buffer size
 * out:
 *	0 - failed
 *	1 - success
 */
static int
pxe_core_recieve(PXE_PACKET *pack, void *frame_data, size_t frame_size)
{

	/* check to be sure */
	if (pack->user_data == NULL)
		pack->user_data = pack->data;

	/* sanity check */
	if ( (pack->user_data - pack->data) + frame_size > pack->data_size)
		return (0); /* This must not ever be*/

	pxe_memcpy(pack->user_data, frame_data, frame_size);

	return (1);
}

/* TODO: think if this function is needed
 * allocates packet, creates buffer for data if necessary
 */
PXE_PACKET *
pxe_core_alloc_packet(size_t packet_size)
{

	if (core_packet.state == PXE_PACKET_STATE_FREE) {
		/* packet structure seems to be free */
		/* mark it busy */
		core_packet.state = PXE_PACKET_STATE_USING;

		if (core_packet.data_size < packet_size) {
			/* packet contains less memmory than needed */

			void *data = pxe_alloc(packet_size + MEDIAHDR_LEN_ETH);
			pxe_free(core_packet.data);
			core_packet.raw_data = data;

			/* failed to allocate enough memory for packet */
			if (data == NULL) {
				core_packet.data_size = 0;
				core_packet.raw_size = 0;
				core_packet.data = NULL;
				
				return (NULL);
			}

			core_packet.data_size = packet_size;
			core_packet.raw_size = packet_size + MEDIAHDR_LEN_ETH;
			core_packet.data = data + MEDIAHDR_LEN_ETH;
		}

		return (&core_packet);
	}

	return (NULL);
}

/* pxe_core_register() -  registers protocol in protocols table
 * in:
 * 	proto	- IP protocol number
 *	proc	- callback
 * out:
 *	none
 */
void
pxe_core_register(uint8_t proto, pxe_protocol_call proc)
{

    core_protocol[proto]=proc;
}

#ifdef PXE_EXCLUSIVE
/* pxe_core_exclusive() -  sets protocol exclusive when receiving packets
 * in:
 * 	proto	- protocol number (PXE_PROTOCOL_...)
 * out:
 *	none
 */
void
pxe_core_exclusive(uint8_t proto)
{
#ifdef PXE_CORE_DEBUG_HELL
	printf("pxe_core_exlusive(): %d protocol.\n", proto);
#endif
	exclusive_protocol = proto;
}
#endif

/* pxe_get_mymac() - returns NIC MAC
 * in:
 *	none
 * out:
 *	non NULL pointer to MAC_ADDR
 */
const MAC_ADDR*
pxe_get_mymac()
{
	return (const MAC_ADDR *)&nic_mac;
}

/* pxe_get_ip() - returns ip related data, specified by id parameter
 * in:
 *	id - id of needed data (PXE_IP_ constants)
 * out:
 *	associated with this id value
 */
const PXE_IPADDR *
pxe_get_ip(uint8_t id)
{
	if (id < PXE_IP_MAX)
		return (&core_ips[id]);
	    
	return (0);
}

/* pxe_set_ip() - sets ip related data, specified by id parameter
 * in:
 *	id	- id of needed data (PXE_IP_ constants)
 *	new_ip	- new uint32_t data
 * out:
 *	none
 */
void
pxe_set_ip(uint8_t id, const PXE_IPADDR *new_ip)
{
	if (id < PXE_IP_MAX) {
		pxe_memcpy(new_ip, &core_ips[id], sizeof(PXE_IPADDR));
	}
}
