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
 
#ifndef PXE_CORE_H_INCLUDED
#define PXE_CORE_H_INCLUDED

/*
 *  contains wrappers for PXE API functions
 */
 
#include <stand.h>
#include <stdint.h>
#include <stddef.h>

#include "../libi386/pxe.h"
#include "pxe_ip.h"

#define PXE_BUFFER_SIZE		0x0800

/* define to use statically allocated buffers */
#define PXE_CORE_STATIC_BUFFERS

/* packet states for packets, used by pxe_core.
 * Currently (only one packet at any time) - is unused.
 */
#define PXE_PACKET_STATE_FREE		0
#define PXE_PACKET_STATE_USING		1

#define PXE_DOWN			0
#define PXE_INITING			1
#define PXE_READY			2

/* size of media header, used in allocating memmory for packet */
#define	MEDIAHDR_LEN_ETH		14
/* packet type: broadcast and directed */
#define PXE_BCAST			1
#define PXE_SINGLE			0
/*
 *   structure, used to provide information about packet in pxe_core
 */
typedef struct pxe_packet {

    uint8_t     protocol;   /* protocol, used in packet */
    uint8_t     state;      /* state of  packet (PXE_PACKET_STATE_ ... ) */
    uint8_t	flags;      /* flags if it is broadcast packet */
    
    void*	raw_data;   /* pointer to data, including media header */
    size_t	raw_size;   /* real size of packet */
    
    void*       data;       /* pointer to buffer with packet data */
    size_t      data_size;  /* size of packet data */
                            
    const MAC_ADDR *dest_mac;   /* destination media address */

    void*       user_data;  /* pointer to user data.
			     * used by higher level protocols
			     */
} PXE_PACKET;

#define PXE_PROTOCOL_UNKNOWN    0
#define PXE_PROTOCOL_IP         1
#define PXE_PROTOCOL_ARP        2
#define PXE_PROTOCOL_RARP       3
#define PXE_PROTOCOL_OTHER      4

/* init of PXE core structures. */
int pxe_core_init(pxenv_t* pxenv_p, pxe_t* pxe_p);

/* cleanup */
int pxe_core_shutdown();

/* sends packet to a network */
int pxe_core_transmit(PXE_PACKET *pack);

/* allocates buffer for packet */
PXE_PACKET *pxe_core_alloc_packet(size_t packet_size);

/* recieves all packets waiting in queue, and calls protocols if needed */
int pxe_core_recv_packets();

/* copies in real mode from one segment to another. */
void pxe_core_copy(uint16_t seg_from, uint16_t off_from, uint16_t seg_to,
	uint16_t off_to, uint16_t size);

/* installs irq handler*/
void pxe_core_install_isr();

/* removes irq handler*/
void pxe_core_remove_isr();

/* call to PXE API */
int pxe_core_call(int func);

#define PXE_CORE_HANDLE    0x0
#define PXE_CORE_FRAG      0x1
/* protocol callback function type */
typedef int (*pxe_protocol_call)(PXE_PACKET *pack, uint8_t function);

/* registers protocol */
void pxe_core_register(uint8_t ip_proto, pxe_protocol_call proc);

/* set this protocol exclusive, other packets are ignored */
#ifdef PXE_EXCLUSIVE
void pxe_core_exclusive(uint8_t proto);
#endif

/* returns NIC MAC */
const MAC_ADDR *pxe_get_mymac();

#define PXE_IP_MY		0
#define PXE_IP_NET		1
#define PXE_IP_NETMASK		2
#define PXE_IP_NAMESERVER	3
#define PXE_IP_GATEWAY		4
#define PXE_IP_BROADCAST	5
#define PXE_IP_SERVER		6
#define PXE_IP_WWW		7
#define PXE_IP_ROOT		7
#define PXE_IP_MAX		8
const PXE_IPADDR *pxe_get_ip(uint8_t id);
void pxe_set_ip(uint8_t id, const PXE_IPADDR *ip);

/* returns time in seconds */
time_t	pxe_get_secs();

#define pxe_get_secs	getsecs

/* updates IPs after getting them via DHCP/BOOTP */
void pxe_core_update_bootp();

#ifndef FNAME_SIZE
#define FNAME_SIZE      128
#endif
extern char rootpath[FNAME_SIZE];
extern char servername[256];
			     
#endif // PXE_CORE_H_INCLUDED
