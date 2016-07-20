/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMBUS_CHANVAR_H_
#define _VMBUS_CHANVAR_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/include/vmbus.h>

typedef struct {
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       write_index;
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       read_index;
	/*
	 * NOTE: The interrupt_mask field is used only for channels, but
	 * vmbus connection also uses this data structure
	 */
	volatile uint32_t       interrupt_mask;
	/* pad it to PAGE_SIZE so that data starts on a page */
	uint8_t                 reserved[4084];

	/*
	 * WARNING: Ring data starts here
	 *  !!! DO NOT place any fields below this !!!
	 */
	uint8_t			buffer[0];	/* doubles as interrupt mask */
} __packed hv_vmbus_ring_buffer;

typedef struct {
	hv_vmbus_ring_buffer*	ring_buffer;
	struct mtx		ring_lock;
	uint32_t		ring_data_size;	/* ring_size */
} hv_vmbus_ring_buffer_info;

typedef struct hv_vmbus_channel {
	device_t			ch_dev;
	struct vmbus_softc		*ch_vmbus;
	uint32_t			ch_flags;	/* VMBUS_CHAN_FLAG_ */
	uint32_t			ch_id;		/* channel id */

	/*
	 * These are based on the offer_msg.monitor_id.
	 * Save it here for easy access.
	 */
	int				ch_montrig_idx;	/* MNF trig index */
	uint32_t			ch_montrig_mask;/* MNF trig mask */

	/*
	 * TX bufring; at the beginning of ch_bufring.
	 */
	hv_vmbus_ring_buffer_info	ch_txbr;
	/*
	 * RX bufring; immediately following ch_txbr.
	 */
	hv_vmbus_ring_buffer_info	ch_rxbr;

	struct taskqueue		*ch_tq;
	struct task			ch_task;
	vmbus_chan_callback_t		ch_cb;
	void				*ch_cbarg;

	struct hyperv_mon_param		*ch_monprm;
	struct hyperv_dma		ch_monprm_dma;

	int				ch_cpuid;	/* owner cpu */
	/*
	 * Virtual cpuid for ch_cpuid; it is used to communicate cpuid
	 * related information w/ Hyper-V.  If MSR_HV_VP_INDEX does not
	 * exist, ch_vcpuid will always be 0 for compatibility.
	 */
	uint32_t			ch_vcpuid;

	/*
	 * If this is a primary channel, ch_subchan* fields
	 * contain sub-channels belonging to this primary
	 * channel.
	 */
	struct mtx			ch_subchan_lock;
	TAILQ_HEAD(, hv_vmbus_channel)	ch_subchans;
	int				ch_subchan_cnt;

	/* If this is a sub-channel */
	TAILQ_ENTRY(hv_vmbus_channel)	ch_sublink;	/* sub-channel link */
	struct hv_vmbus_channel		*ch_prichan;	/* owner primary chan */

	void				*ch_bufring;	/* TX+RX bufrings */
	struct hyperv_dma		ch_bufring_dma;
	uint32_t			ch_bufring_gpadl;

	struct task			ch_detach_task;
	TAILQ_ENTRY(hv_vmbus_channel)	ch_prilink;	/* primary chan link */
	uint32_t			ch_subidx;	/* subchan index */
	volatile uint32_t		ch_stflags;	/* atomic-op */
							/* VMBUS_CHAN_ST_ */
	struct hyperv_guid		ch_guid_type;
	struct hyperv_guid		ch_guid_inst;

	struct sysctl_ctx_list		ch_sysctl_ctx;
} hv_vmbus_channel;

#define VMBUS_CHAN_ISPRIMARY(chan)	((chan)->ch_subidx == 0)

#define VMBUS_CHAN_FLAG_HASMNF		0x0001
/*
 * If this flag is set, this channel's interrupt will be masked in ISR,
 * and the RX bufring will be drained before this channel's interrupt is
 * unmasked.
 *
 * This flag is turned on by default.  Drivers can turn it off according
 * to their own requirement.
 */
#define VMBUS_CHAN_FLAG_BATCHREAD	0x0002

#define VMBUS_CHAN_ST_OPENED_SHIFT	0
#define VMBUS_CHAN_ST_OPENED		(1 << VMBUS_CHAN_ST_OPENED_SHIFT)

#endif	/* !_VMBUS_CHANVAR_H_ */
