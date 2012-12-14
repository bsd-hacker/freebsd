/*-
 * Copyright (C) 2012 Intel Corporation
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include "ioat.h"
#include "ioat_internal.h"
#include "ioat_logger.h"

static int ioat_probe(device_t device);
static int ioat_attach(device_t device);
static int ioat_detach(device_t device);
static int ioat3_attach(device_t device);
static int ioat_map_pci_bar(struct ioat_softc *ioat);
static void ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
static void ioat_interrupt_setup(struct ioat_softc *ioat);
static void ioat_interrupt_handler(void *arg);
static void ioat_process_events(struct ioat_softc *ioat);
static inline uint32_t ioat_get_active(struct ioat_softc *ioat);
static inline uint32_t ioat_get_ring_space(struct ioat_softc *ioat);
static void ioat_free_ring_entry(struct ioat_softc *ioat,
    struct ioat_descriptor *desc);
static struct ioat_descriptor * ioat_alloc_ring_entry(struct ioat_softc *ioat);
static int ioat_reserve_space_and_lock(struct ioat_softc *ioat, int num_descs);
static struct ioat_descriptor * ioat_get_ring_entry(struct ioat_softc *ioat,
    uint32_t index);
static boolean_t resize_ring(struct ioat_softc *ioat, int order);
static void ioat_timer_callback(void *arg);
static void dump_descriptor(void *hw_desc);
static void ioat_submit_single(struct ioat_softc *ioat);
static void ioat_comp_update_map(void *arg, bus_dma_segment_t *seg, int nseg,
    int error);
static int ioat_reset_hw(struct ioat_softc *ioat);
static void ioat_setup_sysctl(device_t device);

MALLOC_DEFINE(M_IOAT, "ioat", "ioat driver memory allocations");

//#define IOAT_LOGGING
/*
 * OS <-> Driver interface structures
 */
static device_method_t ioat_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ioat_probe),
	DEVMETHOD(device_attach,    ioat_attach),
	DEVMETHOD(device_detach,    ioat_detach),
	{ 0, 0 }
};

static driver_t ioat_pci_driver = {
	"ioat",
	ioat_pci_methods,
	sizeof(struct ioat_softc),
};

static devclass_t ioat_devclass;
DRIVER_MODULE(ioat, pci, ioat_pci_driver, ioat_devclass, 0, 0);

/*
 * Private data structures
 */
struct ioat_softc *ioat_channel[IOAT_MAX_CHANNELS];
int ioat_channel_index = 0;

static struct _pcsid
{
	u_int32_t   type;
	const char  *desc;
} pci_ids[] = {
	{ 0x3c208086,   "SNB IOAT Ch0"  },
	{ 0x3c218086,   "SNB IOAT Ch1"  },
	{ 0x3c228086,   "SNB IOAT Ch2"  },
	{ 0x3c238086,   "SNB IOAT Ch3"  },
	{ 0x3c248086,   "SNB IOAT Ch4"  },
	{ 0x3c258086,   "SNB IOAT Ch5"  },
	{ 0x3c268086,   "SNB IOAT Ch6"  },
	{ 0x3c278086,   "SNB IOAT Ch7"  },
	{ 0x3c2e8086,   "SNB IOAT Ch0"  },
	{ 0x3c2f8086,   "SNB IOAT Ch1"  },
	{ 0x00000000,   NULL            }
};

/*
 * OS <-> Driver linkage functions
 */
static int
ioat_probe(device_t device)
{
	u_int32_t type = pci_get_devid(device);
	struct _pcsid *ep = pci_ids;

	while (ep->type) {
		if (ep->type == type) {
			device_set_desc(device, ep->desc);
			return (0);
		}
		++ep;
	}
	return (ENXIO);
}

static int
ioat_attach(device_t device)
{
	int error = 0;
	struct ioat_softc *ioat = DEVICE2SOFTC(device);

	ioat->device = device;

	/*
	 * TODO: this needs to be moved into a driver init function
	 *  so that it only gets executed once overall, rather than
	 *  once per channel.  But this works fine for now.
	 */
	TUNABLE_INT_FETCH("hw.ioat.debug_level", &g_ioat_debug_level);

	ioat_map_pci_bar(ioat);
	ioat->version = ioat_read_cbver(ioat);
	ioat_interrupt_setup(ioat);

	if (ioat->version >= IOAT_VER_3_0) {
		error = ioat3_attach(device);
	} else {
		ioat_detach(device);
		error = -1;
	}

	if (error == 0)
		ioat_channel[ioat_channel_index++] = ioat;

	return (error);
}

static int
ioat_detach(device_t device)
{
	struct ioat_softc *ioat = DEVICE2SOFTC(device);

	if (ioat->pci_resource != NULL) {
		bus_release_resource(device, SYS_RES_MEMORY,
		    ioat->pci_resource_id, ioat->pci_resource);
	}

	if (ioat->ring) {
		int i;
		for (i = 0; i < (1 << ioat->ring_size_order); i++)
			ioat_free_ring_entry(ioat, ioat->ring[i]);

		free(ioat->ring, M_IOAT);
	}

	if (ioat->comp_update) {
		bus_dmamap_unload(ioat->comp_update_tag, ioat->comp_update_map);
		bus_dmamem_free(ioat->comp_update_tag, ioat->comp_update,
		    ioat->comp_update_map);
		bus_dma_tag_destroy(ioat->comp_update_tag);
	}

	bus_dma_tag_destroy(ioat->hw_desc_tag);

	if(ioat->tag != NULL)
		bus_teardown_intr(device, ioat->res, ioat->tag);

	if(ioat->res != NULL)
		bus_release_resource(device, SYS_RES_IRQ,
		    rman_get_rid(ioat->res), ioat->res);

	pci_release_msi(device);
	callout_drain(&ioat->timer);

	return (0);
}

/*
 * Initialize Hardware
 */
static int
ioat3_attach(device_t device)
{
	struct ioat_softc *ioat = DEVICE2SOFTC(device);
	struct ioat_descriptor **ring;
	int error = 0;
	uint8_t xfercap;
	uint32_t capabilities, chanerr;
	uint64_t status;
	int i, num_descriptors;
	struct ioat_descriptor *next;
	struct ioat_dma_hw_descriptor *dma_hw_desc;

	capabilities = ioat_read_dmacapability(ioat);

	xfercap = ioat_read_xfercap(ioat);

	/* Only bits [4:0] are valid. */
	xfercap &= 0x1f;
	ioat->max_xfer_size = 1 << xfercap;

	/* TODO: need to check DCA here if we ever do XOR/PQ */
	
	mtx_init(&ioat->submit_lock, "ioat_submit", NULL, MTX_DEF);
	mtx_init(&ioat->cleanup_lock, "ioat_process_events", NULL, MTX_DEF);
	callout_init(&ioat->timer, CALLOUT_MPSAFE);

	ioat->is_resize_pending = FALSE;
	ioat->is_completion_pending = FALSE;
	ioat->is_reset_pending = FALSE;
	ioat->is_channel_running = FALSE;
	ioat->is_waiting_for_ack = FALSE;

	bus_dma_tag_create(bus_get_dma_tag(ioat->device), sizeof(uint64_t), 0x0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(uint64_t), 1, sizeof(uint64_t), 0, NULL, NULL,
	    &ioat->comp_update_tag);

	error = bus_dmamem_alloc(ioat->comp_update_tag,
	    (void **)&ioat->comp_update, BUS_DMA_ZERO, &ioat->comp_update_map);

	if (!ioat->comp_update) {
		ioat_detach(ioat->device);
		return (-1);
	}

	error = bus_dmamap_load(ioat->comp_update_tag, ioat->comp_update_map,
				ioat->comp_update, sizeof(uint64_t),
				ioat_comp_update_map, ioat, 0);

	ioat->ring_size_order = IOAT_MIN_ORDER;

	num_descriptors = 1 << ioat->ring_size_order;

	bus_dma_tag_create(bus_get_dma_tag(ioat->device), 0x40, 0x0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct ioat_dma_hw_descriptor), 1,
	    sizeof(struct ioat_dma_hw_descriptor), 0, NULL, NULL,
	    &ioat->hw_desc_tag);

	ioat->ring = malloc(num_descriptors * sizeof(*ring), M_IOAT,
	    M_ZERO | M_NOWAIT);

	if (!ioat->ring) {
		ioat_detach(ioat->device);
		return (-1);
	}

	ring = ioat->ring;

	for (i = 0; i < num_descriptors; i++) {
		ring[i] = ioat_alloc_ring_entry(ioat);

		if (!ring[i]) {
			ioat_detach(ioat->device);
			return (-1);
		}

		ring[i]->id = i;
	}

	for (i = 0; i < num_descriptors-1; i++) {
		next = ring[i+1];
		dma_hw_desc = ring[i]->u.dma;

		dma_hw_desc->next = next->hw_desc_bus_addr;
	}

	ring[i]->u.dma->next = ring[0]->hw_desc_bus_addr;

	ioat->head = 0;
	ioat->tail = 0;
	ioat->last_seen = 0;

	status = ioat_get_chansts(ioat);

	ioat_reset_hw(ioat);

	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);
	ioat_write_chancmp(ioat, ioat->comp_update_bus_addr);
	ioat_write_chainaddr(ioat, ring[0]->hw_desc_bus_addr);

	ioat_acquire(&ioat->dmaengine);
	ioat_null(&ioat->dmaengine, NULL, NULL, 0);
	ioat_release(&ioat->dmaengine);

	i = 100;
	while (i-- > 0) {
		DELAY(1);
		status = ioat_get_chansts(ioat);
		if (is_ioat_idle(status))
			break;
	}

	if (is_ioat_idle(status)) {
		ioat_process_events(ioat);
	} else {
		chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);

		ioat_log_message(0, "could not start channel: "
				 "status = %p\n error = %x\n",
				 (void *)status, chanerr);

		error = -1;
	}

	if (error)
	{
		ioat_detach(device);
		return (error);
	}

	ioat_setup_sysctl(device);
	
	return (0);
}

static int
ioat_map_pci_bar(struct ioat_softc *ioat)
{

	ioat->pci_resource_id = PCIR_BAR(0);
	ioat->pci_resource = bus_alloc_resource(ioat->device, SYS_RES_MEMORY,
						&ioat->pci_resource_id, 0, ~0,
						1, RF_ACTIVE);

	if(ioat->pci_resource == NULL)
		ioat_log_message(0, "unable to allocate pci resource\n");
	else {
		ioat->pci_bus_tag = rman_get_bustag(ioat->pci_resource);
		ioat->pci_bus_handle = rman_get_bushandle(ioat->pci_resource);
	}

	return (0);
}

static void
ioat_comp_update_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct ioat_softc *ioat = arg;

	ioat->comp_update_bus_addr = seg[0].ds_addr;
}

static void
ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{

	bus_addr_t *baddr = arg;
	*baddr = segs->ds_addr;
}

/*
 * Interrupt setup and handlers
 */
static void
ioat_interrupt_setup(struct ioat_softc *ioat)
{
	boolean_t use_msix = 0;
	boolean_t force_legacy_interrupts = 0;

	TUNABLE_INT_FETCH("hw.ioat.force_legacy_interrupts",
			  &force_legacy_interrupts);

	if (!force_legacy_interrupts &&
	    pci_msix_count(ioat->device) >= 1) {
		uint32_t num_vectors = 1;

		pci_alloc_msix(ioat->device, &num_vectors);
		if (num_vectors == 1)
			use_msix = TRUE;
	}

	if (use_msix == TRUE) {
		ioat->rid = 1;
		ioat->res = bus_alloc_resource_any(ioat->device, SYS_RES_IRQ,
		    &ioat->rid, RF_ACTIVE);
	}
	else {
		ioat->rid = 0;
		ioat->res = bus_alloc_resource_any(ioat->device, SYS_RES_IRQ,
		    &ioat->rid, RF_SHAREABLE | RF_ACTIVE);
	}

	if (ioat->res == NULL) {
		ioat_log_message(0, "bus_alloc_resource failed\n");
		return;
	}

	ioat->tag = NULL;

	if (bus_setup_intr(ioat->device, ioat->res,
	    INTR_MPSAFE | INTR_TYPE_MISC, NULL,
	    ioat_interrupt_handler, ioat, &ioat->tag)) {
		ioat_log_message(0, "bus_setup_intr failed\n");
		return;
	}

	ioat_write_intrctrl(ioat, IOAT_INTRCTRL_MASTER_INT_EN);
}

static void
ioat_interrupt_handler(void *arg)
{
	struct ioat_softc *ioat = arg;

	ioat_process_events(ioat);
}

static void
ioat_process_events(struct ioat_softc *ioat)
{
	struct ioat_descriptor *desc;
	uint64_t comp_update, status;
	uint32_t completed;
	struct bus_dmadesc *dmadesc;

	mtx_lock(&ioat->cleanup_lock);

	completed = 0;
	comp_update = *ioat->comp_update;
	status = comp_update & IOAT_CHANSTS_COMPLETED_DESCRIPTOR_MASK;

	ioat_log_message(3, "%s\n", __func__);

	if (status == ioat->last_seen) {
	 	mtx_unlock(&ioat->cleanup_lock);
		return;
	}

	while (1) {
		desc = ioat_get_ring_entry(ioat, ioat->tail);
		dmadesc = &desc->bus_dmadesc;

		ioat_log_message(3, "completing desc %d\n", ioat->tail);

		if (dmadesc->callback_fn) {
			(*dmadesc->callback_fn)(dmadesc->callback_arg);
		}

		ioat->tail++;

		if (desc->hw_desc_bus_addr == status)
			break;

	}

	ioat->last_seen = desc->hw_desc_bus_addr;

	if (ioat->head == ioat->tail) {
		ioat->is_completion_pending = FALSE;
		callout_reset(&ioat->timer, 5000*hz/1000, ioat_timer_callback,
		    ioat);
	}

	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);

	mtx_unlock(&ioat->cleanup_lock);
}

/*
 * User API functions
 */
bus_dmaengine_t
ioat_get_dmaengine(uint32_t index)
{

	if (index < ioat_channel_index)
		return (&ioat_channel[index]->dmaengine);

	return (NULL);
}

void
ioat_acquire(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = (struct ioat_softc *)dmaengine;
	mtx_lock(&ioat->submit_lock);
	ioat_log_message(3, "%s\n", __func__);
}

void
ioat_release(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat_log_message(3, "%s\n", __func__);
	ioat = (struct ioat_softc *)dmaengine;
	ioat_write_2(ioat, IOAT_DMACOUNT_OFFSET, (uint16_t)ioat->head);
	mtx_unlock(&ioat->submit_lock);
}

struct bus_dmadesc *
ioat_null(bus_dmaengine_t dmaengine, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_softc *ioat;
	struct ioat_descriptor *desc;
	struct ioat_dma_hw_descriptor *hw_desc;

	ioat = (struct ioat_softc *)dmaengine;

	if (ioat_reserve_space_and_lock(ioat, 1) != 0)
		return (NULL);

	ioat_log_message(3, "%s\n", __func__);

	desc = ioat_get_ring_entry(ioat, ioat->head);
	hw_desc = desc->u.dma;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control.null = 1;
	hw_desc->u.control.completion_update = 1;

	if (flags & DMA_INT_EN)
		hw_desc->u.control.int_enable = 1;

	hw_desc->size = 8;
	hw_desc->src_addr = 0;
	hw_desc->dest_addr = 0;

	desc->bus_dmadesc.callback_fn = callback_fn;
	desc->bus_dmadesc.callback_arg = callback_arg;

	ioat_submit_single(ioat);

	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_copy(bus_dmaengine_t dmaengine, bus_addr_t dst,
    bus_addr_t src, bus_size_t len, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_descriptor *desc;
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_softc *ioat;

	ioat = (struct ioat_softc *)dmaengine;

	if (len > ioat->max_xfer_size)
		panic("ioat_copy: max_xfer_size = %d, requested = %d\n",
		    ioat->max_xfer_size, (int)len);

	if (ioat_reserve_space_and_lock(ioat, 1) != 0)
		return (NULL);

	ioat_log_message(3, "%s\n", __func__);

	desc = ioat_get_ring_entry(ioat, ioat->head);
	hw_desc = desc->u.dma;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control.completion_update = 1;

	if (flags & DMA_INT_EN)
		hw_desc->u.control.int_enable = 1;

	hw_desc->size = len;
	hw_desc->src_addr = src;
	hw_desc->dest_addr = dst;

#ifdef IOAT_LOGGING
	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);
#endif

	desc->bus_dmadesc.callback_fn = callback_fn;
	desc->bus_dmadesc.callback_arg = callback_arg;

	ioat_submit_single(ioat);

	return (&desc->bus_dmadesc);
}

/*
 * Ring Management
 */
static inline uint32_t
ioat_get_active(struct ioat_softc *ioat)
{

	return ((ioat->head - ioat->tail) & ((1 << ioat->ring_size_order) - 1));
}

static inline uint32_t
ioat_get_ring_space(struct ioat_softc *ioat)
{

	return ((1 << ioat->ring_size_order) - ioat_get_active(ioat) - 1);
}

static struct ioat_descriptor *
ioat_alloc_ring_entry(struct ioat_softc *ioat)
{
	struct ioat_descriptor          *desc;
	struct ioat_dma_hw_descriptor	*hw_desc;

	desc = malloc(sizeof(struct ioat_descriptor), M_IOAT, M_NOWAIT);

	if (desc == NULL)
		return (NULL);

	bus_dmamem_alloc(ioat->hw_desc_tag, (void **)&hw_desc, BUS_DMA_ZERO,
	    &ioat->hw_desc_map);

	if (hw_desc == NULL) {
		free(desc, M_IOAT);
		return (NULL);
	}

	bus_dmamap_load(ioat->hw_desc_tag, ioat->hw_desc_map, hw_desc,
	    sizeof(*hw_desc), ioat_dmamap_cb, &desc->hw_desc_bus_addr, 0);

	desc->u.dma = hw_desc;

	return (desc);
}

static void
ioat_free_ring_entry(struct ioat_softc *ioat, struct ioat_descriptor *desc)
{

	if (desc != NULL) {
		if (desc->u.dma)
			bus_dmamem_free(ioat->hw_desc_tag, desc->u.dma,
			    ioat->hw_desc_map);
		free(desc, M_IOAT);
	}
}

static int
ioat_reserve_space_and_lock(struct ioat_softc *ioat, int num_descs)
{
	int retry;

	do {
		if (ioat_get_ring_space(ioat) >= num_descs) {
			return (0);
		}

		mtx_lock(&ioat->cleanup_lock);
		retry = resize_ring(ioat, ioat->ring_size_order + 1);
		mtx_unlock(&ioat->cleanup_lock);

		if (retry)
			continue;
		else {
			return (ENOMEM);
		}
	} while (1);
}

static struct ioat_descriptor *
ioat_get_ring_entry(struct ioat_softc *ioat, uint32_t index)
{

	return (ioat->ring[index % (1 << ioat->ring_size_order)]);
}

static boolean_t
resize_ring(struct ioat_softc *ioat, int order)
{
	struct ioat_descriptor **ring;
	struct ioat_descriptor *next;
	struct ioat_dma_hw_descriptor *hw;
	struct ioat_descriptor *ent;
	uint32_t current_size, active, new_size, i, new_idx, current_idx;
	uint32_t new_idx2;

	current_size = 1 << ioat->ring_size_order;
	active = (ioat->head - ioat->tail) & (current_size - 1);
	new_size = 1 << order;

	if (order > IOAT_MAX_ORDER)
		return (FALSE);

	/*
	 * when shrinking, verify that we can hold the current active
	 * set in the new ring
	 */
	if (active >= new_size)
		return (FALSE);

	/* allocate the array to hold the software ring */
	ring = malloc(new_size * sizeof(*ring), M_IOAT, M_ZERO | M_NOWAIT);

	if (!ring)
		return (FALSE);

	ioat_log_message(1, "ring resize: new: %d old: %d\n",
	    new_size, current_size);

	/* allocate/trim descriptors as needed */
	if (new_size > current_size) {
		/* copy current descriptors to the new ring */
		for (i = 0; i < current_size; i++) {
			current_idx = (ioat->tail+i) & (current_size-1);
			new_idx = (ioat->tail+i) & (new_size-1);

			ring[new_idx] = ioat->ring[current_idx];
			ring[new_idx]->id = new_idx;
		}

		/* add new descriptors to the ring */
		for (i = current_size; i < new_size; i++) {
			new_idx = (ioat->tail+i) & (new_size-1);

			ring[new_idx] = ioat_alloc_ring_entry(ioat);
			if (!ring[new_idx]) {
				while (i--) {
					new_idx2 = (ioat->tail+i) &
					    (new_size-1);

					ioat_free_ring_entry(ioat, 
					    ring[new_idx2]);
				}
				free(ring, M_IOAT);
				return (FALSE);
			}
			ring[new_idx]->id = new_idx;
		}

		for (i = current_size-1; i < new_size; i++) {
			new_idx = (ioat->tail+i) & (new_size-1);
			next = ring[(new_idx+1) & (new_size-1)];
			hw = ring[new_idx]->u.dma;

			hw->next = next->hw_desc_bus_addr;
		}
	} else {
		/*
		 * copy current descriptors to the new ring, dropping the
		 * removed descriptors
		 */
		for (i = 0; i < new_size; i++) {
			uint32_t curr_idx = (ioat->tail+i) & (current_size-1);
			uint32_t new_idx = (ioat->tail+i) & (new_size-1);

			ring[new_idx] = ioat->ring[curr_idx];
			ring[new_idx]->id = new_idx;
		}

		/* free deleted descriptors */
		for (i = new_size; i < current_size; i++) {
			ent = ioat_get_ring_entry(ioat, ioat->tail+i);
			ioat_free_ring_entry(ioat, ent);
		}

		/* fix up hardware ring */
		hw = ring[(ioat->tail+new_size-1) & (new_size-1)]->u.dma;
		next = ring[(ioat->tail+new_size) & (new_size-1)];
		hw->next = next->hw_desc_bus_addr;
	}

	free(ioat->ring, M_IOAT);
	ioat->ring = ring;
	ioat->ring_size_order = order;

	return (TRUE);
}

static void
ioat_timer_callback(void *arg)
{
	struct ioat_softc *ioat = arg;
	uint64_t status;
	struct ioat_descriptor *desc;
	uint32_t chanerr;

	ioat_log_message(2, "%s\n", __func__);

	if (ioat->is_completion_pending) {
		status = ioat_get_chansts(ioat);

		/*
		 * when halted due to errors check for channel
		 * programming errors before advancing the completion state
		 */
		if (is_ioat_halted(status)) {
			chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
			ioat_log_message(0, "Channel halted (%x)\n", chanerr);

			desc = ioat_get_ring_entry(ioat, ioat->tail+0);
			dump_descriptor(desc->u.raw);

			desc = ioat_get_ring_entry(ioat, ioat->tail+1);
			dump_descriptor(desc->u.raw);
		}
		ioat_process_events(ioat);
	} else {
		mtx_lock(&ioat->submit_lock);
		mtx_lock(&ioat->cleanup_lock);

		if (ioat_get_active(ioat) == 0 &&
		    ioat->ring_size_order > IOAT_MIN_ORDER)
			resize_ring(ioat, ioat->ring_size_order-1);

		mtx_unlock(&ioat->cleanup_lock);
		mtx_unlock(&ioat->submit_lock);

		if (ioat->ring_size_order > IOAT_MIN_ORDER) {
			callout_reset(&ioat->timer, 5000*hz/1000,
			    ioat_timer_callback, ioat);
		}
	}
}
/*
 * Support Functions
 */
static void
ioat_submit_single(struct ioat_softc *ioat)
{

	atomic_add_rel_int(&ioat->head, 1);

	if (!ioat->is_completion_pending) {
		ioat->is_completion_pending = TRUE;
		callout_reset(&ioat->timer, 10000*hz/1000, ioat_timer_callback,
		    ioat);
	}
}

static int ioat_reset_hw(struct ioat_softc *ioat)
{
	int timeout = 20; /* in milliseconds */
	uint64_t status;
	uint32_t chanerr;

	status = ioat_get_chansts(ioat);
	if (is_ioat_active(status) || is_ioat_idle(status))
		ioat_suspend(ioat);

	while (is_ioat_active(status) || is_ioat_idle(status)) {
		DELAY(1000);
		timeout--;
		if (timeout == 0)
			return (-1);
		status = ioat_get_chansts(ioat);
	}

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	ioat_write_4(ioat, IOAT_CHANERR_OFFSET, chanerr);

	/*
	 * IOAT v3 workaround - CHANERRMSK_INT with 3E07h to masks out errors
	 *  that can cause stability issues for IOAT v3.
	 */
	pci_write_config(ioat->device, IOAT_CFG_CHANERRMASK_INT_OFFSET, 0x3e07,
	    4);
	chanerr = pci_read_config(ioat->device, IOAT_CFG_CHANERR_INT_OFFSET, 4);
	pci_write_config(ioat->device, IOAT_CFG_CHANERR_INT_OFFSET, chanerr, 4);

	ioat_reset(ioat);

	timeout = 20;

	while (ioat_reset_pending(ioat)) {
		DELAY(1000);
		timeout--;
		if (timeout == 0)
			return (-1);
	}

	return (0);
}

static void
dump_descriptor(void *hw_desc)
{
	int i, j;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 8; j++)
			printf("%08x ", ((uint32_t *)hw_desc)[i*8+j]);
		printf("\n");
	}
}

static void
ioat_setup_sysctl(device_t device)
{
	struct ioat_softc *ioat = DEVICE2SOFTC(device);
	struct sysctl_ctx_list *sysctl_ctx = device_get_sysctl_ctx(device);
	struct sysctl_oid *sysctl_tree = device_get_sysctl_tree(device);

	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "ring_size_order", CTLFLAG_RD, &ioat->ring_size_order,
	    0, "HW descriptor ring size order");

	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "head", CTLFLAG_RD, &ioat->head,
	    0, "HW descriptor head pointer index");

	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "tail", CTLFLAG_RD, &ioat->tail,
	    0, "HW descriptor tail pointer index");
}
