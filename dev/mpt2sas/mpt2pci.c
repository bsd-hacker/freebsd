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

/*
 * PCI Configuration code for the LSI-Logic 2008 and 2108 SAS PCI-Express host adapters.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <dev/mpt2sas/mpt2sas.h>

struct mtx mpt2sas_global_lock;
struct mpt2sas_tq mpt2sas_tailq;

static int mpt2sas_pci_probe(device_t);
static int mpt2sas_pci_attach(device_t);
static void mpt2sas_pci_intr(void *);
static void mpt2sas_free_bus_resources(mpt2sas_t *mpt);
static int mpt2sas_pci_detach(device_t);
static int mpt2sas_pci_shutdown(device_t);
static int mpt2sas_pci_modevent(module_t, int, void *);


static device_method_t mpt2sas_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mpt2sas_pci_probe),
	DEVMETHOD(device_attach,	mpt2sas_pci_attach),
	DEVMETHOD(device_detach,	mpt2sas_pci_detach),
	DEVMETHOD(device_shutdown,	mpt2sas_pci_shutdown),
	{ 0, 0 }
};

static driver_t mpt2sas_driver = {
	"mpt2sas", mpt2sas_methods, sizeof(mpt2sas_t)
};
static devclass_t mpt2sas_devclass;
DRIVER_MODULE(mpt2sas, pci, mpt2sas_driver, mpt2sas_devclass, mpt2sas_pci_modevent, 0);
MODULE_DEPEND(mpt2sas, pci, 1, 1, 1);
MODULE_VERSION(mpt2sas, 1);

static int
mpt2sas_pci_probe(device_t dev)
{
	char *desc;

	if (pci_get_vendor(dev) != MPI2_MFGPAGE_VENDORID_LSI) {
		return (ENXIO);
	}

	switch ((pci_get_device(dev) & ~1)) {
	case MPI2_MFGPAGE_DEVID_SAS2004:
		desc = "LSILogic SAS2004";
		break;
	case MPI2_MFGPAGE_DEVID_SAS2008:
		desc = "LSILogic SAS2008";
		break;
	case MPI2_MFGPAGE_DEVID_SAS2108_1:
	case MPI2_MFGPAGE_DEVID_SAS2108_2:
	case MPI2_MFGPAGE_DEVID_SAS2108_3:
	case MPI2_MFGPAGE_DEVID_SAS2116_1:
	case MPI2_MFGPAGE_DEVID_SAS2116_2:
	default:
		return (ENXIO);
	}
	device_set_desc(dev, desc);
	return (0);
}

static void
mpt2sas_set_options(mpt2sas_t *mpt)
{
	unsigned int tval;

	tval = 0;
	if (resource_int_value(device_get_name(mpt->dev), device_get_unit(mpt->dev), "disable", &tval) == 0 && tval != 0) {
		mpt->disabled = 1;
	}
	if (resource_int_value(device_get_name(mpt->dev), device_get_unit(mpt->dev), "debug", &tval) == 0 && tval != 0) {
		mpt->prt_mask = tval;
	} else {
		mpt->prt_mask = MP2PRT_WARN|MP2PRT_ERR;
		if (bootverbose) {
			mpt->prt_mask |= MP2PRT_INFO;
		}
	}
}

static int
mpt2sas_pci_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	mpt2sas_t *mpt;
	int iqd, result;

	mpt  = (mpt2sas_t *)device_get_softc(dev);
	if (mpt == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return (ENOMEM);
	}
	result = 0;
	memset(mpt, 0, sizeof(mpt2sas_t));
	mpt->dev = dev;
	mpt2sas_set_options(mpt);

	ctx = device_get_sysctl_ctx(mpt->dev);
	tree = device_get_sysctl_tree(mpt->dev);
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "prt_mask", CTLFLAG_RW, &mpt->prt_mask, 0, "logging mask");
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "config_buf_mask", CTLFLAG_RD, &mpt->config_buf_mask, 0, "logging mask");
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "nactive", CTLFLAG_RD, &mpt->nactive, 0, "number of active commands");
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "nreq_total", CTLFLAG_RD, &mpt->nreq_total, 0, "total number of requests for this HBA");
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "nreq_allocated", CTLFLAG_RD, &mpt->nreq_allocated, 0, "number of requests currently allocated");
	SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "fwversion", CTLFLAG_RD, &mpt->fwversion[0], 0, "F/W version");
	snprintf(mpt->fwversion, sizeof (mpt->fwversion), "0.0.0.0");
	SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "rpintr", CTLFLAG_RD, &mpt->rpintr, "reply interrupts");
	SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "nreply", CTLFLAG_RD, &mpt->nreply, "total replies");
	SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "nreq", CTLFLAG_RD, &mpt->nreq, "total requests");


	/*
	 * Set some defaults
	 */
	mpt->request_frame_size = MPT2_DEFAULT_REQUEST_FRAME_SIZE;

	/*
	 * Set up register access.  PIO mode is required for
	 * certain reset operations (but must be disabled for
	 * some cards otherwise).
	 */
	mpt->pci_pio_rid = PCIR_BAR(0);
	mpt->pci_pio_reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &mpt->pci_pio_rid, 0, ~0, 0, RF_ACTIVE);
	if (mpt->pci_pio_reg == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "unable to map registers in PIO mode\n");
		goto bad;
	}
	mpt->pci_pio_st = rman_get_bustag(mpt->pci_pio_reg);
	mpt->pci_pio_sh = rman_get_bushandle(mpt->pci_pio_reg);

	mpt->pci_mem_rid = PCIR_BAR(1);
	mpt->pci_mem_reg = bus_alloc_resource(dev, SYS_RES_MEMORY, &mpt->pci_mem_rid, 0, ~0, 0, RF_ACTIVE);
	if (mpt->pci_mem_reg == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Unable to memory map registers.\n");
		result = ENXIO;
		goto bad;
	} else {
		mpt->pci_mem_st = rman_get_bustag(mpt->pci_mem_reg);
		mpt->pci_mem_sh = rman_get_bushandle(mpt->pci_mem_reg);
	}

	/* Get a handle to the interrupt */
	iqd = 0;

	/*
	 * First try to alloc an MSI-X message.  If that
	 * fails, then try to alloc an MSI message instead.
	 */
	if (pci_msix_count(dev) == 1) {
		mpt->pci_msi_count = 1;
		if (pci_alloc_msix(dev, &mpt->pci_msi_count) == 0) {
			iqd = 1;
		} else {
			mpt->pci_msi_count = 0;
		}
	}
	if (iqd == 0 && pci_msi_count(dev) == 1) {
		mpt->pci_msi_count = 1;
		if (pci_alloc_msi(dev, &mpt->pci_msi_count) == 0) {
			iqd = 1;
		} else {
			mpt->pci_msi_count = 0;
		}
	}
	mpt->pci_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &iqd, RF_ACTIVE | RF_SHAREABLE);
	if (mpt->pci_irq == NULL) {
		result = ENXIO;
		mpt2sas_prt(mpt, MP2PRT_ERR, "could not allocate interrupt\n");
		goto bad;
	}
	mtx_init(&mpt->lock, "mpt2", NULL, MTX_DEF);
	mpt->locksetup = 1;

	/* Disable interrupts at the part */
	mpt2sas_disable_ints(mpt);

	/* Register the interrupt handler */
	if (bus_setup_intr(dev, mpt->pci_irq, MPT2_IFLAGS, NULL, mpt2sas_pci_intr, mpt, &mpt->ih)) {
		result = ENXIO;
		mpt2sas_prt(mpt, MP2PRT_ERR, "could not setup interrupt\n");
		goto bad;
	}

	/*
	 * Disable PIO until we need it
	 */
	pci_disable_io(dev, SYS_RES_IOPORT);

	/* Initialize the hardware */
	if (mpt->disabled == 0) {
		result = mpt2sas_attach(mpt);
		if (result != 0) {
			goto bad;
		}
	} else {
		mpt2sas_prt(mpt, MP2PRT_INFO, "device disabled at user request\n");
		goto bad;
	}

	return (0);

bad:
	mpt2sas_mem_free(mpt);
	mpt2sas_free_bus_resources(mpt);
	if (mpt->locksetup) {                                           \
		mtx_destroy(&mpt->lock);                                \
		mpt->locksetup = 0;                                     \
	}

	/*
	 * but return zero to preserve unit numbering
	 */
	return (result);
}

static void
mpt2sas_pci_intr(void *arg)
{
	mpt2sas_t *mpt = arg;
	MPT2_LOCK(mpt);
	mpt2sas_intr(mpt);
	MPT2_UNLOCK(mpt);
}


/*
 * Free bus resources
 */
static void
mpt2sas_free_bus_resources(mpt2sas_t *mpt)
{
	if (mpt->ih) {
		bus_teardown_intr(mpt->dev, mpt->pci_irq, mpt->ih);
		mpt->ih = 0;
	}

	if (mpt->pci_irq) {
		bus_release_resource(mpt->dev, SYS_RES_IRQ, mpt->pci_msi_count ? 1 : 0, mpt->pci_irq);
		mpt->pci_irq = 0;
	}

	if (mpt->pci_msi_count) {
		pci_release_msi(mpt->dev);
		mpt->pci_msi_count = 0;
	}
		
	if (mpt->pci_pio_reg) {
		bus_release_resource(mpt->dev, SYS_RES_IOPORT, mpt->pci_pio_rid, mpt->pci_pio_reg);
		mpt->pci_pio_reg = 0;
	}
	if (mpt->pci_mem_reg) {
		bus_release_resource(mpt->dev, SYS_RES_MEMORY, mpt->pci_mem_rid, mpt->pci_mem_reg);
		mpt->pci_mem_reg = 0;
	}
	if (mpt->locksetup) {                                           \
		mtx_destroy(&mpt->lock);                                \
		mpt->locksetup = 0;                                     \
	}
}


/*
 * Disconnect ourselves from the system.
 */
static int
mpt2sas_pci_detach(device_t dev)
{
	mpt2sas_t *mpt = (mpt2sas_t*)device_get_softc(dev);

	if (mpt && mpt->disabled == 0) {
		int r = mpt2sas_detach(mpt);
		if (r)
			return (r);
		mpt2sas_disable_ints(mpt);
		mpt2sas_mem_free(mpt);
		mpt2sas_free_bus_resources(mpt);
	}
	return(0);
}


/*
 * Disable the hardware
 */
static int
mpt2sas_pci_shutdown(device_t dev)
{
	mpt2sas_t *mpt = (mpt2sas_t *)device_get_softc(dev);
	if (mpt) {
		mpt2sas_disable_ints(mpt);
	}
	return(0);
}

static int
mpt2sas_pci_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		mtx_init(&mpt2sas_global_lock, "mpt2global", NULL, MTX_SPIN);
		TAILQ_INIT(&mpt2sas_tailq);
		break;
	case MOD_UNLOAD:
		mtx_destroy(&mpt2sas_global_lock);
		break;
	default:
		break;
	}
	return (0);
}

#define	MPT2_DMA_SET_STEP(mpt, step)	mpt->dma_setup |= (1 << step)
#define	MPT2_DMA_STEP_SET(mpt, step)	(mpt->dma_setup & (1 << step))

int
mpt2sas_mem_alloc(mpt2sas_t *mpt)
{
	int i, nsegs;
	size_t len;
	struct mpt2sas_map_info mi;

	/* Check if we alreay have allocated the reply memory */
	if (mpt->dma_setup) {
		return (0);
	}

	/*
	 * Do some parameter based memory allocations here
	 */
	/*
	 * Allocate device array based upon IOC facts
	 */
	len = sizeof (sas_dev_t) * mpt->ioc_facts.MaxTargets;
	mpt->sas_dev_pool = (sas_dev_t *)malloc(len, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (mpt->sas_dev_pool == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate device pool\n");
		mi.error = ENOMEM;
		goto failure;
	}
	for (i = 0; i < mpt->ioc_facts.MaxTargets; i++) {
		mpt->sas_dev_pool[i].mpt = mpt;
		callout_init(&mpt->sas_dev_pool[i].actions, 1);
	}
	MPT2_DMA_SET_STEP(mpt, 0);

	len = sizeof (request_t) * MPT2_MAX_REQUESTS(mpt);
	mpt->request_pool = (request_t *)malloc(len, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (mpt->request_pool == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate request pool\n");
		mi.error = ENOMEM;
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 1);

	/*
	 * Create a parent dma tag for this device. Align at byte boundaries,
	 */
	mi.error = bus_dma_tag_create(bus_get_dma_tag(mpt->dev),
	    1,					/* alignment */
	    0,					/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    BUS_SPACE_MAXSIZE,			/* maxsize */
	    BUS_SPACE_UNRESTRICTED,		/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    0,					/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->parent_dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create parent dma tag\n");
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 2);

	/*
	 * Create a child tag for requests aligned on a 16 byte boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    16,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_REQ_MEM_SIZE(mpt),		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->requests.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for requests (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 3);

	mi.error = bus_dmamem_alloc(mpt->requests.dmat, (void **)&mpt->requests.vaddr, BUS_DMA_NOWAIT, &mpt->requests.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of request memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 4);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->requests.dmat, mpt->requests.dmap, mpt->requests.vaddr, MPT2_REQ_MEM_SIZE(mpt), mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for DMA request queue\n", mi.error);
		goto failure;
	}
	mpt->requests.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 5);

	/*
	 * Create a child tag for reply buffers aligned on a 4 byte boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    4,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_REPLY_MEM_SIZE(mpt),		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->replies.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for replies (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 6);

	mi.error = bus_dmamem_alloc(mpt->replies.dmat, (void **)&mpt->replies.vaddr, BUS_DMA_NOWAIT, &mpt->replies.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of reply memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 7);


	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->replies.dmat, mpt->replies.dmap, mpt->replies.vaddr, MPT2_REPLY_MEM_SIZE(mpt), mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for DMA reply queue\n", mi.error);
		goto failure;
	}
	mpt->replies.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 8);


	/*
	 * Create a child tag for the Reply Free queue, aligned on a 16 byte boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    16,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_RPF_QDEPTH(mpt) << 2,		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->replyf.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for replyf (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 9);

	mi.error = bus_dmamem_alloc(mpt->replyf.dmat, (void **)&mpt->replyf.vaddr, BUS_DMA_NOWAIT, &mpt->replyf.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of replyf memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 10);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->replyf.dmat, mpt->replyf.dmap, mpt->replyf.vaddr, MPT2_RPF_QDEPTH(mpt) << 2, mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for DMA replyf queue\n", mi.error);
		goto failure;
	}
	mpt->replyf.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 11);

	/*
	 * Create a child tag for the Reply Post queue, aligned on a 16 byte boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    16,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_RPQ_QDEPTH(mpt) << 3,		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->replyq.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for replyq (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 12);

	mi.error = bus_dmamem_alloc(mpt->replyq.dmat, (void **)&mpt->replyq.vaddr, BUS_DMA_NOWAIT, &mpt->replyq.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of replyq memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 13);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->replyq.dmat, mpt->replyq.dmap, mpt->replyq.vaddr, MPT2_RPQ_QDEPTH(mpt) << 3, mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for DMA replyq queue\n", mi.error);
		goto failure;
	}
	mpt->replyq.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 14);

	/*
	 * Create a child tag for the dma chunk area, aligned on a page boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    PAGE_SIZE,				/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_CHUNK_MEM_SIZE(mpt),		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->chunks.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for chunks (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 15);

	mi.error = bus_dmamem_alloc(mpt->chunks.dmat, (void **)&mpt->chunks.vaddr, BUS_DMA_NOWAIT, &mpt->chunks.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of chunks memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 16);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->chunks.dmat, mpt->chunks.dmap, mpt->chunks.vaddr, MPT2_CHUNK_MEM_SIZE(mpt), mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for DMA chunks queue\n", mi.error);
		goto failure;
	}
	mpt->chunks.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 17);

	/*
	 * Create a child tag for the sense buffers, aligned on an 8 byte boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    8,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_SENSE_DATA_SIZE(mpt),		/* buddy paired with requests */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->sense.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for sense (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 18);

	mi.error = bus_dmamem_alloc(mpt->sense.dmat, (void **)&mpt->sense.vaddr, BUS_DMA_NOWAIT, &mpt->sense.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of sense memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 19);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->sense.dmat, mpt->sense.dmap, mpt->sense.vaddr, MPT2_SENSE_DATA_SIZE(mpt), mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for sense\n", mi.error);
		goto failure;
	}
	mpt->sense.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 20);

	/*
	 * Create a child tag for config and sata scratch buffers, a page aligned on a page boundary, allocate memory to back it, and map it in.
	 */
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    PAGE_SIZE,				/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MPT2_CONFIG_DATA_MAX(mpt),		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsz */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->config.dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for config data (%d)\n", mi.error);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 21);

	mi.error = bus_dmamem_alloc(mpt->config.dmat, (void **)&mpt->config.vaddr, BUS_DMA_NOWAIT, &mpt->config.dmap);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot allocate %u bytes of config memory\n", 2 << PAGE_SHIFT);
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 22);

	mi.mpt = mpt;
	mi.error = 0;
	bus_dmamap_load(mpt->config.dmat, mpt->config.dmap, mpt->config.vaddr, MPT2_CONFIG_DATA_MAX(mpt), mpt2sas_map_rquest, &mi, 0);
	if (mi.error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "error %d loading dma map for config\n", mi.error);
		goto failure;
	}
	mpt->config.paddr = mi.physaddr;
	MPT2_DMA_SET_STEP(mpt, 23);

	/* Create a child tag for data buffers */
	nsegs = MPT2_MAX_REQUESTS(mpt) * MPT2_MAX_SEGS_PER_CMD(mpt);
	mi.error = bus_dma_tag_create(mpt->parent_dmat,
	    1,					/* alignment */
	    BUS_SPACE_MAXSIZE_32BIT + 1,	/* boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MAXPHYS,				/* maxsize */
	    nsegs,				/* nsegments */
	    BUS_SPACE_MAXSIZE_24BIT+1,		/* maxsegsz */
	    0,					/* flags */
	    busdma_lock_mutex, &mpt->lock,	/* lockfunc, lockfuncarg */
	    &mpt->buffer_dmat);
	if (mi.error != 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot create a dma tag for data buffers\n");
		goto failure;
	}
	MPT2_DMA_SET_STEP(mpt, 24);

	/* create data buffer dma maps */
	for (i = 0; i < MPT2_MAX_REQUESTS(mpt); i++) {
		request_t *req = &mpt->request_pool[i];
		mi.error = bus_dmamap_create(mpt->buffer_dmat, 0, &req->dmap);
		if (mi.error) {
			request_t *req = &mpt->request_pool[i];
			while (--i >= 0) {
				bus_dmamap_destroy(mpt->buffer_dmat, req->dmap);
			}
			mpt2sas_prt(mpt, MP2PRT_ERR, "error %d creating per-cmd DMA maps\n", mi.error);
			goto failure;
		}
	}
	MPT2_DMA_SET_STEP(mpt, 25);

	for (i = 0; i < MPT2_MAX_TOPO; i++) {
		struct topochg *tp = (struct topochg *)malloc(len, M_DEVBUF, M_WAITOK|M_ZERO);
		TAILQ_INSERT_TAIL(&mpt->topo_free_list, tp, links);
	}
	MPT2_DMA_SET_STEP(mpt, 26);


	return (0);
failure:
	mpt2sas_mem_free(mpt);
	return (mi.error);
}

/*
 * Deallocate memory that was allocated by mpt2sas_dma_mem_alloc 
 */
void
mpt2sas_mem_free(mpt2sas_t *mpt)
{
	int i;
	if (MPT2_DMA_STEP_SET(mpt, 26)) {
		struct topochg *tp;
		while ((tp = TAILQ_FIRST(&mpt->topo_free_list)) != NULL) {
			TAILQ_REMOVE(&mpt->topo_free_list, tp, links);
			free(tp, M_DEVBUF);
		}
		while ((tp = TAILQ_FIRST(&mpt->topo_wait_list)) != NULL) {
			TAILQ_REMOVE(&mpt->topo_wait_list, tp, links);
			free(tp, M_DEVBUF);
		}
	}
	if (MPT2_DMA_STEP_SET(mpt, 25)) {
		int i;
		for (i = MPT2_MAX_REQUESTS(mpt) - 1; i >= 0; i--) {
			bus_dmamap_destroy(mpt->buffer_dmat, mpt->request_pool[i].dmap);
		}
	}
	if (MPT2_DMA_STEP_SET(mpt, 24)) {
		bus_dma_tag_destroy(mpt->buffer_dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 23)) {
		bus_dmamap_unload(mpt->config.dmat, mpt->config.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 22)) {
		bus_dmamem_free(mpt->config.dmat, mpt->config.vaddr, mpt->sense.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 21)) {
		bus_dma_tag_destroy(mpt->config.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 20)) {
		bus_dmamap_unload(mpt->sense.dmat, mpt->sense.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 19)) {
		bus_dmamem_free(mpt->sense.dmat, mpt->sense.vaddr, mpt->sense.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 18)) {
		bus_dma_tag_destroy(mpt->sense.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 17)) {
		bus_dmamap_unload(mpt->chunks.dmat, mpt->chunks.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 16)) {
		bus_dmamem_free(mpt->chunks.dmat, mpt->chunks.vaddr, mpt->chunks.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 15)) {
		bus_dma_tag_destroy(mpt->chunks.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 14)) {
		bus_dmamap_unload(mpt->replyq.dmat, mpt->replyq.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 13)) {
		bus_dmamem_free(mpt->replyq.dmat, mpt->replyq.vaddr, mpt->replyq.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 12)) {
		bus_dma_tag_destroy(mpt->replyq.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 11)) {
		bus_dmamap_unload(mpt->replyf.dmat, mpt->replyf.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 10)) {
		bus_dmamem_free(mpt->replyf.dmat, mpt->replyf.vaddr, mpt->replyf.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 9)) {
		bus_dma_tag_destroy(mpt->replyf.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 8)) {
		bus_dmamap_unload(mpt->replies.dmat, mpt->replies.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 7)) {
		bus_dmamem_free(mpt->replies.dmat, mpt->replies.vaddr, mpt->replies.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 6)) {
		bus_dma_tag_destroy(mpt->replies.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 5)) {
		bus_dmamap_unload(mpt->requests.dmat, mpt->requests.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 4)) {
		bus_dmamem_free(mpt->requests.dmat, mpt->requests.vaddr, mpt->requests.dmap);
	}
	if (MPT2_DMA_STEP_SET(mpt, 3)) {
		bus_dma_tag_destroy(mpt->requests.dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 2)) {
		bus_dma_tag_destroy(mpt->parent_dmat);
	}
	if (MPT2_DMA_STEP_SET(mpt, 1)) {
		free(mpt->request_pool, M_DEVBUF);
		mpt->request_pool = NULL;
	}
	if (MPT2_DMA_STEP_SET(mpt, 0)) {
		for (i = 0; i < mpt->ioc_facts.MaxTargets; i++) {
			callout_drain(&mpt->sas_dev_pool[i].actions);
		}
		free(mpt->sas_dev_pool, M_DEVBUF);
		mpt->sas_dev_pool = NULL;
	}
	mpt->dma_setup = 0;
}
