/*-
 * Copyright (c) 2012 Jakub Wojciech Klama <jceel@FreeBSD.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h> 
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

#define	IRQ_PIC_IDX(_irq)	((_irq >> 8) & 0xff)
#define	IRQ_VECTOR_IDX(_irq)	((_irq) & 0xff)
#define	IRQ_GEN(_pic, _irq)	(((_pic) << 8) | ((_irq) & 0xff))
#define	CORE_PIC_IDX		(0)
#define	CORE_PIC_NODE		(0xffffffff)

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

typedef void (*mask_fn)(void *);

struct arm_intr_controller {
	device_t		ic_dev;
	phandle_t		ic_node;
	struct arm_intr_data	ic_id;
};

struct arm_intr_handler {
	device_t		ih_dev;
	int			ih_intrcnt;
	int			ih_irq;
	struct intr_event *	ih_event;
	struct arm_intr_controller *ih_pic;
	struct arm_intr_controller *ih_self;
};

static void arm_mask_irq(void *);
static void arm_unmask_irq(void *);
static void arm_eoi(void *);

static struct arm_intr_handler arm_intrs[NIRQ];
static struct arm_intr_controller arm_pics[NPIC];

void
arm_dispatch_irq(device_t dev, struct trapframe *tf, int irq)
{
	struct arm_intr_handler *ih = NULL;
	void *arg;
	int i;

	debugf("pic %s, tf %p, irq %d\n", device_get_nameunit(dev), tf, irq);

	for (i = 0; arm_intrs[i].ih_dev != NULL; i++) {
		if (arm_intrs[i].ih_pic->ic_dev == dev &&
		    arm_intrs[i].ih_irq == irq) {
			ih = &arm_intrs[i];
			break;
		}
	}

	if (ih == NULL)
		panic("arm_dispatch_irq: unknown irq");

	debugf("requested by %s\n", device_get_nameunit(ih->ih_dev));

	arg = tf;

	/* XXX */
	for (i = 0; arm_pics[i].ic_dev != NULL; i++)
		arm_pics[i].ic_id.tf = tf;

	ih->ih_intrcnt++;
	if (intr_event_handle(ih->ih_event, tf) != 0) {
		/* Stray IRQ */
		arm_mask_irq(ih);
	}
}

void	arm_handler_execute(struct trapframe *, int);

static struct arm_intr_handler *
arm_lookup_intr_handler(device_t pic, int irq)
{
	int i;

	for (i = 0; i < NIRQ; i++) {
		if (arm_intrs[i].ih_pic != NULL &&
		    arm_intrs[i].ih_pic->ic_dev == pic &&
		    arm_intrs[i].ih_irq == irq)
			return (&arm_intrs[i]);

		if (arm_intrs[i].ih_dev == NULL)
			return (&arm_intrs[i]);
	}

	return NULL;
}

int
arm_fdt_map_irq(phandle_t ic, int irq)
{
	int i;

	debugf("ic %08x irq %d\n", ic, irq);

	if (ic == CORE_PIC_NODE)
		return (IRQ_GEN(CORE_PIC_IDX, irq));

	for (i = 0; arm_pics[i].ic_node != 0; i++) {
		if (arm_pics[i].ic_node	== ic)
			return (IRQ_GEN(i, irq));
	}

	/* 
	 * Interrupt controller is not registered yet, so
	 * we map a stub for it. 'i' is pointing to free
	 * first slot in arm_pics table.
	 */
	arm_pics[i].ic_node = ic;
	return (IRQ_GEN(i, irq));
}

const char *
arm_describe_irq(int irq)
{
	struct arm_intr_controller *pic;
	static char buffer[32];

	pic = &arm_pics[IRQ_PIC_IDX(irq)];
	sprintf(buffer, "%s:%d", device_get_nameunit(pic->ic_dev),
	    IRQ_VECTOR_IDX(irq));

	return (buffer);
}

void
arm_register_pic(device_t dev)
{
	struct arm_intr_controller *ic = NULL;
	phandle_t node;
	int i;

	node = ofw_bus_get_node(dev);

	/* Find room for IC */
	for (i = 0; i < NPIC; i++) {
		if (arm_pics[i].ic_dev != NULL)
			continue;

		if (arm_pics[i].ic_node == node) {
			ic = &arm_pics[i];
			break;
		}

		if (arm_pics[i].ic_node == 0) {
			ic = &arm_pics[i];
			break;
		}
	}

	if (ic == NULL)
		panic("not enough room to register interrupt controller");

	ic->ic_dev = dev;
	ic->ic_node = node;

	debugf("device %s node %08x\n", device_get_nameunit(dev), ic->ic_node);

	device_printf(dev, "registered as interrupt controller\n");
}

void
arm_setup_irqhandler(device_t dev, driver_filter_t *filt, 
    void (*hand)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct arm_intr_controller *pic;
	struct arm_intr_handler *ih;
	struct arm_intr_data *id;
	int error;

	if (irq < 0)
		return;

	pic = &arm_pics[IRQ_PIC_IDX(irq)];
	ih = arm_lookup_intr_handler(pic->ic_dev, IRQ_VECTOR_IDX(irq));

	debugf("setup irq %d on %s\n", IRQ_VECTOR_IDX(irq),
	    device_get_nameunit(pic->ic_dev));

	debugf("pic %p, ih %p\n", pic, ih);

	if (ih->ih_event == NULL) {
		error = intr_event_create(&ih->ih_event, (void *)ih, 0, irq,
		    (mask_fn)arm_mask_irq, (mask_fn)arm_unmask_irq,
		    arm_eoi, NULL, "intr%d:", irq);
		
		if (error)
			return;

		ih->ih_dev = dev;
		ih->ih_irq = IRQ_VECTOR_IDX(irq);
		ih->ih_pic = pic;

		arm_unmask_irq(ih);

		debugf("self interrupt controller %p\n", ih->ih_self);
#if 0
		last_printed += 
		    snprintf(intrnames + last_printed,
		    MAXCOMLEN + 1,
		    "irq%d: %s", irq, device_get_nameunit(dev));
		last_printed++;
		intrcnt_tab[irq] = intrcnt_index;
		intrcnt_index++;
		
#endif
	}

	if (flags & INTR_CONTROLLER) {
		struct arm_intr_controller *pic = NULL;
		int i;
		for (i = 0; i < NPIC; i++) {
			if (arm_pics[i].ic_dev == dev)
				pic = &arm_pics[i];
		}

		id = &pic->ic_id;
		id->arg = arg;
		arg = id;
	}

	intr_event_add_handler(ih->ih_event, device_get_nameunit(dev), filt, hand, arg,
	    intr_priority(flags), flags, cookiep);

	debugf("done\n");
}

int
arm_remove_irqhandler(int irq, void *cookie)
{
	/*
	struct intr_event *event;
	int error;

	event = intr_events[irq];
	arm_mask_irq(irq);
	
	error = intr_event_remove_handler(cookie);

	if (!TAILQ_EMPTY(&event->ie_handlers))
		arm_unmask_irq(irq);
	return (error);
	*/
	return (ENXIO);
}

void
arm_mask_irq(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_MASK(ih->ih_pic->ic_dev, ih->ih_irq);
}

void
arm_unmask_irq(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_UNMASK(ih->ih_pic->ic_dev, ih->ih_irq);
}

void
arm_eoi(void *arg)
{
	struct arm_intr_handler *ih = (struct arm_intr_handler *)arg;

	PIC_EOI(ih->ih_pic->ic_dev, ih->ih_irq);
}

void dosoftints(void);
void
dosoftints(void)
{
}

