/* $FreeBSD$ */
/*-
 * Copyright (c) 2009 by Alacritech Inc.
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Based largely upon the MPT FreeBSD driver under the following copyrights
 */
/*-
 * Generic defines for LSI '909 FC  adapters.
 * FreeBSD Version.
 *
 * Copyright (c)  2000, 2001 by Greg Ansley
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2002, 2006 by Matthew Jacob
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Support from Chris Ellsworth in order to make SAS adapters work
 * is gratefully acknowledged.
 *
 *
 * Support from LSI-Logic has also gone a great deal toward making this a
 * workable subsystem and is gratefully acknowledged.
 */
/*
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MPT2_H_
#define _MPT2_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/bitstring.h>
#include <sys/ata.h>

#include <machine/cpu.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include "opt_ddb.h"


/******************************* MPI Definitions ******************************/
/* force our definitions of basic types */
#define	MPI_TYPE_H	1
typedef uint8_t		U8;
typedef uint16_t	U16;
typedef uint32_t	U32;
typedef uint64_t	U64 __attribute__((aligned(4)));
typedef U8 *		PU8;
typedef U16 *		PU16;
typedef U32 *		PU32;
typedef U64 *		PU64;

#include <dev/mpt2sas/mpilib/mpi2_type.h>
#include <dev/mpt2sas/mpilib/mpi2.h>
#include <dev/mpt2sas/mpilib/mpi2_ioc.h>
#include <dev/mpt2sas/mpilib/mpi2_cnfg.h>
#include <dev/mpt2sas/mpilib/mpi2_init.h>
#include <dev/mpt2sas/mpilib/mpi2_raid.h>
#include <dev/mpt2sas/mpilib/mpi2_tool.h>
#include <dev/mpt2sas/mpilib/mpi2_sas.h>

#include <dev/mpt2sas/mpt2reg.h>
#include <dev/mpt2sas/mpt2cam.h>


/****************************** Misc Definitions ******************************/
#define MPT2_OK		(0)
#define MPT2_FAIL	(0x10000)

#define	MAX_SAS_PORTS	8		/* TEMPORARY */
#define	MPT2_MAX_LUN	0		/* TEMPORARY */
#define	MPT2_SENSE_SIZE		256
#define	MPT2_USABLE_SENSE_SIZE	252

#define	MPT2_MAX_TOPO	256

/* defines for printing */
#define	MP2PRT_ALL	0x00000000
#define	MP2PRT_INFO	0x00000001
#define	MP2PRT_WARN	0x00000002
#define	MP2PRT_ERR	0x00000004
#define	MP2PRT_UNDERRUN	0x00000008
#define	MP2PRT_CONFIG	0x00000010
#define	MP2PRT_CONFIG2	0x00000020
#define	MP2PRT_SPCL	0x00000100
#define	MP2PRT_BLOCKED	0x00000200
#define	MP2PRT_CFLOW0	0x00001000
#define	MP2PRT_DEBUG0	0x00020000

/* definitions for ATA passtrhough */
#define	MPT2_APT_NOWAIT	0x80000000	/* don't wait for results */
#define	MPT2_APT_WRITE	0x20000000	/* data direction is outbound */
#define	MPT2_APT_READ	0x10000000	/* data direction is inbound */
#define	MPT2_APT_PMASK	0x03000000	/* protocol mask */
#define	MPT2_APT_PNONE	0x00000000	/* no protocol */
#define	MPT2_APT_PIO	0x01000000	/* use PIO protocol */
#define	MPT2_APT_DMA	0x02000000	/* use DMA protocol */
#define	MPT2_APT_TMASK	0x000000ff	/* timeout mask */
#define	MPT2_APT_DLMASK	0x00ffff00	/* data length mask */
#define	MPT2_APT_DLSHFT	8		/* shift */

/* other definitions */
#define	MPT2SAS_SYNC_ERR(m, e)											\
	if (e == ETIMEDOUT) {											\
		mpt2sas_prt(m, MP2PRT_ERR, "%s: request timed out (@line %d)\n", __func__, __LINE__);		\
		return (e);											\
 	} else if (e) {												\
		mpt2sas_prt(m, MP2PRT_ERR, "%s: error %d:0x%x (@line %d)\n", __func__, e, m->iocsts, __LINE__);	\
		return (e);											\
	}													\
 	return (0)

#define	MPT2SAS_SYNC_ERR_NORET(m, e)										\
	if (e == ETIMEDOUT) {											\
		mpt2sas_prt(m, MP2PRT_ERR, "%s: request timed out (@line %d)\n", __func__, __LINE__);		\
 	} else if (e) {												\
		mpt2sas_prt(m, MP2PRT_ERR, "%s: error %d:0x%x (@line %d)\n", __func__, e, m->iocsts, __LINE__);	\
	}													\
	do  { ; } while (0)

#define	MPT2SAS_GET_REQUEST(m, r)										\
	if ((r = mpt2sas_get_request(m)) != NULL) {								\
		r->func = __func__;										\
		r->lineno = __LINE__ - 3;									\
	}													\
	do  { ; } while (0)

/****************************** Global Data References ******************************/
extern struct mtx mpt2sas_global_lock;

/**************************** Forward Declarations ****************************/
typedef struct mpt2sas mpt2sas_t;
typedef struct req_entry request_t;
typedef struct mpt2sas_dma_chunk mpt2sas_dma_chunk_t;

/******************************* DMA Support ******************************/

struct mpt2sas_map_info {
	mpt2sas_t *	mpt;
	int		error;
	bus_addr_t	physaddr;
};
void mpt2sas_map_rquest(void *, bus_dma_segment_t *, int, int);
int mpt2sas_mem_alloc(mpt2sas_t *);
void mpt2sas_mem_free(mpt2sas_t *);
static inline void mpt2sas_single_sge(MPI2_SGE_SIMPLE_UNION *, bus_addr_t, bus_size_t, int);

typedef struct {
	bus_dma_tag_t		dmat;	/* DMA tag */
	bus_dmamap_t		dmap;	/* DMA map */
	uint8_t *		vaddr;	/* virtual addr */
	bus_addr_t		paddr;	/* physical addr */
} dbundle_t;

#define	SINGLE_SGE	(MPI2_SGE_FLAGS_LAST_ELEMENT|MPI2_SGE_FLAGS_END_OF_BUFFER|MPI2_SGE_FLAGS_END_OF_LIST|MPI2_SGE_FLAGS_SIMPLE_ELEMENT)
#define	ZERO_LENGTH_SGE	(SINGLE_SGE|MPI2_SGE_FLAGS_32_BIT_ADDRESSING|MPI2_SGE_FLAGS_IOC_TO_HOST << MPI2_SGE_FLAGS_SHIFT)

static inline void
mpt2sas_single_sge(MPI2_SGE_SIMPLE_UNION *up, bus_addr_t addr, bus_size_t len, int flags)
{
	if ((sizeof (bus_addr_t)) > 4) {
		MPI2_SGE_SIMPLE64 *dmap = (MPI2_SGE_SIMPLE64 *)up;
		dmap->FlagsLength = flags;
		dmap->Address = htole64(addr);
		dmap->FlagsLength |= MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
		dmap->FlagsLength <<= MPI2_SGE_FLAGS_SHIFT;
		dmap->FlagsLength |= (len & MPI2_SGE_LENGTH_MASK);
		dmap->FlagsLength = htole32(dmap->FlagsLength);
	} else {
		MPI2_SGE_SIMPLE32 *dmap = (MPI2_SGE_SIMPLE32 *)up;
		dmap->FlagsLength = flags;
		dmap->Address = htole32(addr);
		dmap->FlagsLength |= MPI2_SGE_FLAGS_32_BIT_ADDRESSING;
		dmap->FlagsLength <<= MPI2_SGE_FLAGS_SHIFT;
		dmap->FlagsLength |= (len & MPI2_SGE_LENGTH_MASK);
		dmap->FlagsLength = htole32(dmap->FlagsLength);
	}
}

/********************************** Endianess *********************************/
#define	MPT2_2_HOST64(ptr, tag)	ptr->tag = le64toh(ptr->tag)
#define	MPT2_2_HOST32(ptr, tag)	ptr->tag = le32toh(ptr->tag)
#define	MPT2_2_HOST16(ptr, tag)	ptr->tag = le16toh(ptr->tag)

#define	HOST_2_MPT64(ptr, tag)	ptr->tag = htole64(ptr->tag)
#define	HOST_2_MPT32(ptr, tag)	ptr->tag = htole32(ptr->tag)
#define	HOST_2_MPT16(ptr, tag)	ptr->tag = htole16(ptr->tag)

#if	_BYTE_ORDER == _BIG_ENDIAN
void mpt2host_iocfacts_convert(MPI2_IOC_FACTS_REPLY *);
void mpt2host_portfacts_convert(MPI2_PORT_FACTS_REPLY *);
void mpt2host_phydata_convert(MPI2_SAS_IO_UNIT0_PHY_DATA *);
void mpt2host_sas_dev_page0_convert(MPI2_CONFIG_PAGE_SAS_DEV_0 *);
void mpt2host_sas_discovery_event_convert(MPI2_EVENT_DATA_SAS_DISCOVERY *);
#else
#define	mpt2host_iocfacts_convert(x)		do { ; } while (0)
#define	mpt2host_portfacts_convert(x)		do { ; } while (0)
#define	mpt2host_phydata_convert(x)		do { ; } while (0)
#define	mpt2host_sas_dev_page0_convert(x)	do { ; } while (0)
#define	mpt2host_sas_discovery_event_convert(x)	do { ; } while (0)
#endif


/**************************** MPI Transaction State ***************************/
typedef enum {
	REQ_STATE_NIL		= 0x00,
	REQ_STATE_FREE		= 0x01,
	REQ_STATE_ALLOCATED	= 0x02,
	REQ_STATE_QUEUED	= 0x04,
	REQ_STATE_DONE		= 0x08,
	REQ_STATE_TIMEDOUT	= 0x10,
	REQ_STATE_NEED_WAKEUP	= 0x20,
	REQ_STATE_NEED_CALLBACK	= 0x40,
	REQ_STATE_POLLED	= 0x80,
	REQ_STATE_ABORTED	= 0x100,
	REQ_STATE_LOCKED	= 0x1000,	/* can't be freed */
} req_state_t; 

typedef void req_callback_t(mpt2sas_t *, request_t *, MPI2_DEFAULT_REPLY *);

struct req_entry {
	TAILQ_ENTRY(req_entry) links;		/* Pointer to next in list */
	req_state_t		state;		/* Request State Information */
	uint16_t		IOCStatus;	/* Completion status */
	uint16_t		serno;		/* serial number */
	uint16_t		flags;		/* Flags to be copied into request frame */
	uint16_t		handle;		/* device handle */
	union ccb *		ccb;		/* CAM request */
	mpt2sas_dma_chunk_t *	chain;		/* dma chains */
	bus_dmamap_t		dmap;		/* DMA map for data buffers */
	uint32_t		timeout;	/* timeout in seconds */
	const char *		func;
	uint32_t		lineno;
};

/******************* SAS support structures ***********************************/

/*
 * A SAS Device is a construct we use for mapping between DeviceHandles and
 * (non-persistent) target ids for SSP, SATA or STP devices. This driver
 * does not try and maintain a full SAS topology with SMP (expander)
 * nodes and descendent expanders and devices.
 */
typedef struct sas_dev sas_dev_t;
struct sas_dev {
	mpt2sas_t *	mpt;
	struct callout	actions;
	U16		AttachedDevHandle;
	U16		Slot;
	U16		EnclosureHandle;
	U8		PhyNum;
	U32		SlotStatus;
	MPI2_SAS_IO_UNIT0_PHY_DATA phy;
	uint32_t
		has_slot_info	: 1,
		internal_tm_bsy	: 1,
		destroy_needed	: 1,
		is_sata		: 1,
		set_qfull 	: 1;
	uint16_t	active;
	uint16_t	qdepth;
	enum { NIL, NEW, ATTACHING, VALIDATING, VALIDATING_DONE, STABLE, FAILED, DRAINING, DETACHING } state;
};
#define	ClearSasDev(x)				\
	(x)->AttachedDevHandle = 0;		\
	(x)->Slot = 0;				\
	(x)->EnclosureHandle = 0;		\
	(x)->PhyNum = 0;			\
	(x)->SlotStatus = 0;			\
	memset(&(x)->phy, 0, sizeof (x->phy));	\
	(x)->has_slot_info = 0;			\
	(x)->internal_tm_bsy = 0;		\
	(x)->destroy_needed = 0;		\
	(x)->is_sata = 0;			\
	(x)->set_qfull = 0;			\
	(x)->active = 0;			\
	(x)->qdepth = 0;			\
	(x)->state = NIL

/*
 * A SAS Port is formed when a link comes up and speed is negotiated and
 * the two sides exchanged Identify Frames. A port may widen or narrow
 * to include multiple physical PHYs. In SAS2, you may have up to 254
 * PHYs, so our representation of a port just keeps track of the bit
 * map of phys for each port.
 *
 * Practically speaking, a Port will be a small integer number less than
 * or equal to the number of actual physical PHYs on an HBA.
 */
typedef struct {
	bitstr_t bitmap[((MAX_SAS_PORTS / sizeof (bitstr_t)) == 0)? 1 : (MAX_SAS_PORTS / sizeof (bitstr_t))];
	uint8_t nset;
	uint8_t lrt;
} sas_port_t;


/*
 * We use this structure to maintain a FIFO list of topology change events
 */
struct topochg {
	TAILQ_ENTRY(topochg) links;		/* Pointer to next in list */
	unsigned int
		create	:	1,
		hdl	:	16;
};

/******************* Per-Controller Instance Data Structures ******************/
TAILQ_HEAD(req_queue, req_entry);
TAILQ_HEAD(mpt2sas_tq, mpt2sas);

/*
 * Controller Soft State Structure
 */
struct mpt2sas {
	TAILQ_ENTRY(mpt2sas)	links;
	/*
	 * FreeBSD device stuff
	 */
	device_t		dev;
	struct mtx		lock;
	struct intr_config_hook	ehook;

	/*
	 * IOC facts
	 */
	MPI2_IOC_FACTS_REPLY	ioc_facts;
	uint16_t		request_frame_size;
	uint16_t		max_requests;
	uint16_t		max_replies;
	uint16_t		reply_free_queue_depth;
	uint16_t		reply_post_queue_depth;
	char			fwversion[32];

	/*
	 * Port Facts- one per port. For this device we only support one port.
	 */
	MPI2_PORT_FACTS_REPLY	port_facts;

	/*
	 * Port and Discovery Information
	 */
	sas_port_t		sas_ports[MAX_SAS_PORTS];
	sas_dev_t *		sas_dev_pool;
	TAILQ_HEAD(, topochg)	topo_free_list;
	TAILQ_HEAD(, topochg)	topo_wait_list;

	/*
	 * Config Header Info for the configuration page being worked on
	 */
	MPI2_CONFIG_PAGE_HEADER	cfg_hdr;
	U16			cfg_ExtPageLength;
	U8                      cfg_ExtPageType;
	U32			iounit_pg1_flags;
	
		
	/*
	 * PCIe Hardware info
	 */
	/* interrupt */
	struct resource *	pci_irq;
	int			pci_msi_count;
	void *			ih;
	uint32_t		intr_mask;

	/* memory mapped registers */
	struct resource *	pci_mem_reg;
	int			pci_mem_rid;
	bus_space_tag_t		pci_mem_st;
	bus_space_handle_t	pci_mem_sh;

	/* i/o space registers */
	struct resource *	pci_pio_reg;
	int			pci_pio_rid;
	bus_space_tag_t		pci_pio_st;
	bus_space_handle_t	pci_pio_sh;

	/*
	 * DMA Mapping support
	 */
	bus_dma_tag_t		parent_dmat;	/* DMA tag for parent bus */
	bus_dma_tag_t		buffer_dmat;	/* DMA tag for vanilla I/O buffers */

	dbundle_t		requests;	/* for request frames */
	dbundle_t		replies;	/* for reply frames */
	dbundle_t		replyq;		/* For the Reply Descriptor Queue */
	dbundle_t		replyf;		/* For the Reply Free Queue */
	dbundle_t		chunks;		/* For Scatter/Gather chunks */
	dbundle_t		sense;		/* For Sense Data */
	dbundle_t		config;		/* For Config Data */

	unsigned int		sge_per_chunk;	/* number of scatter/gather elements per dma sge list chunk */
	mpt2sas_dma_chunk_t *	dma_chunk_free;

	/*
	 * Hardware management
	 */
	u_int			reset_cnt;
	uint16_t		free_host_index;
	uint16_t		post_host_index;

	/*
	 * CAM Related structures
	 */
	request_t *		request_pool;
	TAILQ_HEAD(, req_entry)	request_free_list;
	TAILQ_HEAD(, req_entry)	request_pending_list;
	struct callout		watchdog;
	struct timeval		watchdog_then;

	uint16_t		sequence;

	struct cam_sim *	sim;
	struct cam_path *	path;

	/*
	 * Flags and misc
	 */
	unsigned int
		portenabled	:	1,
		fabchanged	:	1,
		devchanged	:	1,
		outofbeer	:	1,
		acks_needed	:	1,
		ehook_active	:	1,
		getreqwaiter	:	1,
		disabled	:	1,
		locksetup	:	1;
	uint32_t		config_buf_mask;
	uint32_t		prt_mask;
	uint32_t		dma_setup;
	uint32_t		nreq_total;
	uint32_t		nreq_allocated;
	uint32_t		nactive;
	uint64_t		rpintr;
	uint64_t		nreply;
	uint64_t		nreq;

	/*
	 * These values are saved during synchronous mpt2sas_wait_req
	 * calls. No locking is needed in this case.
	 */
	uint16_t		iocsts;
	uint16_t		dhdl;
	/*
	 * synchronous SATA PassThru status
	 */
	uint16_t	SASStatus;
	uint8_t		StatusFIS[20];
};

static __inline void mpt2sas_assign_serno(mpt2sas_t *, request_t *);

static __inline void
mpt2sas_assign_serno(mpt2sas_t *mpt, request_t *req)
{
	if ((req->serno = mpt->sequence++) == 0) {
		req->serno = mpt->sequence++;
	}
}

/***************************** Locking Primitives *****************************/
#define	MPT2_IFLAGS		INTR_TYPE_CAM | INTR_ENTROPY | INTR_MPSAFE
#define	MPT2_LOCK(mpt)		mtx_lock(&(mpt)->lock)
#define	MPT2_UNLOCK(mpt)	mtx_unlock(&(mpt)->lock)
#define	MPT2_OWNED(mpt)		mtx_owned(&(mpt)->lock)
#define	MPT2_LOCK_ASSERT(mpt)	mtx_assert(&(mpt)->lock, MA_OWNED)

/******************************* Register Access ******************************/
static __inline void mpt2sas_write(mpt2sas_t *, size_t, uint32_t);
static __inline void mpt2sas_write_request_descriptor(mpt2sas_t *, uint64_t);
static __inline uint32_t mpt2sas_read(mpt2sas_t *, int);
static __inline void mpt2sas_pio_write(mpt2sas_t *, size_t, uint32_t);
static __inline uint32_t mpt2sas_pio_read(mpt2sas_t *, int);

static __inline void
mpt2sas_write(mpt2sas_t *mpt, size_t offset, uint32_t val)
{
	bus_space_write_4(mpt->pci_mem_st, mpt->pci_mem_sh, offset, val);
}

static __inline void
mpt2sas_write_request_descriptor(mpt2sas_t *mpt, uint64_t val)
{
	bus_space_write_4(mpt->pci_mem_st, mpt->pci_mem_sh, MPI2_REQUEST_DESCRIPTOR_POST_LOW_OFFSET, htole32(val));
	bus_space_write_4(mpt->pci_mem_st, mpt->pci_mem_sh, MPI2_REQUEST_DESCRIPTOR_POST_HIGH_OFFSET, htole32(val >> 32));
}

static __inline uint32_t
mpt2sas_read(mpt2sas_t *mpt, int offset)
{
	return (bus_space_read_4(mpt->pci_mem_st, mpt->pci_mem_sh, offset));
}

/*
 * Some operations (e.g. diagnostic register writes while the ARM proccessor
 * is disabled), must be performed using "PCI pio" operations.  On non-PCI
 * busses, these operations likely map to normal register accesses.
 */
static __inline void
mpt2sas_pio_write(mpt2sas_t *mpt, size_t offset, uint32_t val)
{
	bus_space_write_4(mpt->pci_pio_st, mpt->pci_pio_sh, offset, val);
}

static __inline uint32_t
mpt2sas_pio_read(mpt2sas_t *mpt, int offset)
{
	return (bus_space_read_4(mpt->pci_pio_st, mpt->pci_pio_sh, offset));
}
/*********************** Reply Frame/Request Management ***********************/
#define	DEFAULT_MPT_REQUESTS		4096
#define MPT2_MAX_REQUESTS(mpt)		mpt->max_requests

#define	MPT2_DEFAULT_REQUEST_FRAME_SIZE	128
#define MPT2_REQUEST_SIZE(mpt)		(mpt->request_frame_size)
#define MPT2_REQ_MEM_SIZE(mpt)		(mpt->request_frame_size * (mpt->max_requests+1))

#define	MPT2_MAX_REPLIES(mpt)		(mpt->max_replies)
#define	MPT2_REPLY_SIZE(mpt)		(mpt->ioc_facts.ReplyFrameSize << 2)
#define MPT2_REPLY_MEM_SIZE(mpt)	(MPT2_REPLY_SIZE(mpt) * MPT2_MAX_REPLIES(mpt))

#define	MPT2_MAX_SEGS_PER_CMD(mpt)	((MAXPHYS >> PAGE_SHIFT) + 1)
#define	MPT2_CHUNK_SIZE(mpt)		roundup(((((DFLTPHYS >> PAGE_SHIFT) + 1) * sizeof (MPI2_MPI_SGE_IO_UNION)) + sizeof (void *)), 8)
#define	MPT2_CHUNK_MEM_SIZE(mpt)	(MPT2_MAX_REQUESTS(mpt) * MPT2_MAX_SEGS_PER_CMD(mpt) *  MPT2_CHUNK_SIZE(mpt))

#define	MPT2_RPF_QDEPTH(mpt)		mpt->reply_free_queue_depth
#define	MPT2_RPQ_QDEPTH(mpt)		mpt->reply_post_queue_depth

#define	MPT2_CONFIG_DATA_MAX(mpt)	PAGE_SIZE	/* sum of all config data chunks */
#define	MPT2_CONFIG_DATA_SIZE(mpt)	(PAGE_SIZE >> 9)
#define	MPT2_SENSE_DATA_SIZE(mpt)	(MPT2_MAX_REQUESTS(mpt) * MPT2_SENSE_SIZE)
/*
 * Convert a host request structure pointer into a SMID and vice versa
 */
#define	MPT2_REQ2SMID(mpt, rp)		((rp - mpt->request_pool))
#define	MPT2_SMID2REQ(mpt, idx)		&mpt->request_pool[idx]

/*
 * Convert a message request header pointer into a SMID and vice versa
 */
#define	MPT2_RQS2SMID(mpt, hp)		(((((uint8_t *)hp) - mpt->requests.vaddr) / MPT2_REQUEST_SIZE(mpt)))
#define	MPT2_SMID2RQS(mpt, smid)	((void *)(&mpt->requests.vaddr[(smid) * MPT2_REQUEST_SIZE(mpt)]))

/*
 * Convert a host request structure into a message request header pointer and vice versa
 */
#define	MPT2_REQ2RQS(mpt, rp)		MPT2_SMID2RQS(mpt, MPT2_REQ2SMID(mpt, rp))
#define	MPT2_RQS2REQ(mpt, hp)		MPT2_SMID2REQ(mpt, MPT2_RQS2SMID(mpt, hp))

/*
 * Index into the Reply Free queue
 */
#define	MPT_REPLYF_QIDX(m, i)		((((uint32_t *)(m)->replyf.vaddr))[i])

/*
 * Index into the Reply Descriptor queue
 */
#define	MPT_REPLYQ_QIDX(m, i)		((((MPI2_REPLY_DESCRIPTORS_UNION *)(m)->replyq.vaddr))[i])

/*
 * Take the low 32 bits of the reply frame address and return a kernel virtual address pointer to that location
 */
#define	MPT2_RFA2PTR(m, a)		((void *)(&m->replies.vaddr[a - (m->replies.paddr & 0xffffffff)]))

/*
 * Get a pointer to Sense Info
 */
#define	MPT2_REQ2SNS(mpt, req)		&mpt->sense.vaddr[MPT2_REQ2SMID(mpt, req) * MPT2_SENSE_SIZE]

/*
 * Get a physical address of the segment portion of a chunk pointer
 */
#define	MPT2_CHUNK2PADDR(mpt, cp)	(mpt->chunks.paddr + (((uint8_t *)cp->segs) - mpt->chunks.vaddr))

/************************** Scatter Gather Management **************************/
struct mpt2sas_dma_chunk {
	mpt2sas_dma_chunk_t *linkage;
	MPI2_MPI_SGE_IO_UNION segs[0];
};

/***************************** IOC Initialization *****************************/
extern struct mpt2sas_tq mpt2sas_tailq;
int mpt2sas_reset(mpt2sas_t *);
void mpt2sas_send_port_enable(mpt2sas_t *);

/**************************** CAM Routines ***************************/
int		mpt2sas_cam_attach(mpt2sas_t *);
int		mpt2sas_cam_detach(mpt2sas_t *);
void		mpt2sas_cam_done(mpt2sas_t *, request_t *, MPI2_SCSI_IO_REPLY *);
int		mpt2sas_run_scsicmd(mpt2sas_t *, U16, U8 *, int, bus_addr_t, bus_size_t, boolean_t);
int		mpt2sas_scsi_abort(mpt2sas_t *, request_t *);
int		mpt2sas_cam_rescan(mpt2sas_t *);
void		mpt2sas_cam_prt(mpt2sas_t *, struct cam_path *, int, const char *, ...) __printflike(4, 5);


/**************************** Unclassified Routines ***************************/
void		mpt2sas_send_cmd(mpt2sas_t *, request_t *);
int		mpt2sas_wait_req(mpt2sas_t *, request_t *, req_state_t, req_state_t, int);
void		mpt2sas_enable_ints(mpt2sas_t *);
void		mpt2sas_disable_ints(mpt2sas_t *);
int		mpt2sas_shutdown(mpt2sas_t *);
int		mpt2sas_handshake_cmd(mpt2sas_t *, size_t, void *, size_t, void *);
char *		mpt2sas_decode_request(mpt2sas_t *, request_t *, char *, size_t);
request_t *	mpt2sas_get_request(mpt2sas_t *);
void		mpt2sas_free_request(mpt2sas_t *, request_t *);
void		mpt2sas_intr(void *);
int		mpt2sas_create_dev(mpt2sas_t *, U16);
void		mpt2sas_build_ata_passthru(mpt2sas_t *, sas_dev_t *, U8 *, request_t *, bus_addr_t, uint32_t);
int		mpt2sas_check_sata_passthru_failure(mpt2sas_t *, MPI2_SATA_PASSTHROUGH_REPLY *);
int		mpt2sas_destroy_dev(mpt2sas_t *, U16);
void		mpt2sas_destroy_dev_part2(sas_dev_t *);
void		mpt2sas_prt(mpt2sas_t *, int, const char *, ...) __printflike(3, 4);
void		mpt2sas_prt_cont(mpt2sas_t *, int, const char *, ...) __printflike(3, 4);
int		mpt2sas_read_sep(mpt2sas_t *, sas_dev_t *);
int		mpt2sas_write_sep(mpt2sas_t *, sas_dev_t *, U32);
int		mpt2sas_attach(mpt2sas_t *);
int		mpt2sas_detach(mpt2sas_t *);

/************************** Inlines **************************/
static __inline int mpt2sas_get_cfgbuf(mpt2sas_t *, int *);
static __inline void mpt2sas_free_cfgbuf(mpt2sas_t *, int);
static __inline sas_dev_t *mpt2_hdl2dev(mpt2sas_t *, uint16_t);
static __inline sas_dev_t * mpt2_tgt2dev(mpt2sas_t *, target_id_t);
static __inline target_id_t mpt2_dev2tgt(mpt2sas_t *, sas_dev_t *);

static __inline int
mpt2sas_get_cfgbuf(mpt2sas_t *mpt, int *off)
{
	int i;
	for (i = 0; i < (PAGE_SIZE >> 9); i++) {
		if ((mpt->config_buf_mask & (1 << i)) == 0) {
			mpt->config_buf_mask |= (1 << i);
			*off = (i << 9);
			mpt2sas_prt(mpt, MP2PRT_SPCL, "%s: alloc off %d (mask now 0x%08x)\n", __func__, i << 9, mpt->config_buf_mask);
			return (0);
		}
	}
	return (ENOMEM);
}

static __inline void
mpt2sas_free_cfgbuf(mpt2sas_t *mpt, int off)
{
	if (off >= 0 && off < PAGE_SIZE && (off & ((1 << 9) - 1)) == 0) {
		int mask = 1 << (off >> 9);
		if (mpt->config_buf_mask & mask) {
			mpt->config_buf_mask ^= mask;
			mpt2sas_prt(mpt, MP2PRT_SPCL, "%s: free off %d (mask now 0x%08x)\n", __func__, off, mpt->config_buf_mask);
		} else {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: free off %d twice! (mask is 0x%08x)\n", __func__, off, mpt->config_buf_mask);
		}
	} else {
		mpt2sas_prt(mpt, MP2PRT_ERR,  "%s: bad off %d (mask is 0x%08x)\n", __func__, off, mpt->config_buf_mask);
	}
}

static __inline sas_dev_t *
mpt2_hdl2dev(mpt2sas_t *mpt, U16 hdl)
{
	sas_dev_t *dp;
	KASSERT(hdl < mpt->ioc_facts.MaxTargets, ("%s: oops", __func__));
	dp = &mpt->sas_dev_pool[hdl];
	if (dp->AttachedDevHandle) {
		return (dp);
	} else {
		return (NULL);
	}
}

static __inline sas_dev_t *
mpt2_tgt2dev(mpt2sas_t *mpt, target_id_t id)
{
	sas_dev_t *dp;
	if (id >= mpt->ioc_facts.MaxTargets) {
		return (NULL);
	}
	dp = &mpt->sas_dev_pool[id];
	if (dp->AttachedDevHandle) {
		return (dp);
	} else {
		return (NULL);
	}
}

static __inline target_id_t
mpt2_dev2tgt(mpt2sas_t *mpt, sas_dev_t *dp)
{
	return dp - mpt->sas_dev_pool;
}

#endif /* _MPT2_H_ */
