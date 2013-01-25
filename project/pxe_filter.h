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
 
#ifndef PXE_FILTER_H_INCLUDED
#define PXE_FILTER_H_INCLUDED

/*
 * Packet filters for sockets.
 */
#include <stdint.h>

#include "pxe_ip.h"

#define PXE_FILTER_ACTIVE	0x01	/* filter is active (entry is used) */
#define PXE_FILTER_PARENT	0x02	/* forks subfilters and creates sockets
					 * for them. */
#define PXE_FILTER_CONSUME	0x04	/* may send recved data to buffer */

#define PXE_FILTER_LISTEN	(PXE_FILTER_PARENT | PXE_FILTER_ACTIVE)
#define PXE_FILTER_ROOT		0x80

typedef struct pxe_filter_entry {

	uint16_t	src_port;	/* source port */
	uint16_t	src_port_mask;	/* source port mask */
	uint16_t	dst_port;	/* destination port */
	uint16_t	dst_port_mask;	/* destination port mask */

	PXE_IPADDR	src;		/* source IP address */
	uint32_t	src_mask;	/* source IP address mask */
	PXE_IPADDR	dst;		/* destination IP address */
	uint32_t	dst_mask;	/* destination IP address mask */

	void		*socket;	/* socket, which receives data,
					 * passed through this filter
					 * NULL - if unknown.
					 */
	uint8_t		protocol;	/* IP based protocol */

	struct pxe_filter_entry *next;	/* next filter */
	struct pxe_filter_entry *prev;	/* previous filter */
	
} PXE_FILTER_ENTRY;

/* number of filter is must be at least equal to  number of sockets */
#define MAX_PXE_FILTERS		8

/* init filter module */
void pxe_filter_init();

/* show active filters */
void pxe_filter_stats();

/* installs new filter*/
PXE_FILTER_ENTRY *pxe_filter_add(const PXE_IPADDR *src_ip, uint16_t src_port,
		    const PXE_IPADDR *dst_ip, uint16_t dst_port, void *socket,
		    uint8_t proto);

/* install filter earlier provided filter */
PXE_FILTER_ENTRY *pxe_filter_before(PXE_FILTER_ENTRY *filter,
		    const PXE_FILTER_ENTRY *def);

/* fills filter masks */
int pxe_filter_mask(PXE_FILTER_ENTRY *filter, uint32_t src_ip_mask,
	uint16_t src_port_mask, uint32_t dst_ip_mask, uint16_t dst_port_mask);

/* removes filter from filter_table*/
int pxe_filter_remove(PXE_FILTER_ENTRY *filter);

/* returns socket, if found trigerred filter */
void *pxe_filter_check(const PXE_IPADDR *src_ip, uint16_t src_port,
	const PXE_IPADDR *dst_ip, uint16_t dst_port, uint8_t proto);

#endif
