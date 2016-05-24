/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
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
 */

/*
 * VM Bus Driver Implementation
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/rtprio.h>
#include <sys/interrupt.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <sys/pcpu.h>
#include <x86/apicvar.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#include <contrib/dev/acpica/include/acpi.h>
#include "acpi_if.h"

struct vmbus_softc	*vmbus_sc;

static int vmbus_inited;

static char *vmbus_ids[] = { "VMBUS", NULL };

extern inthand_t IDTVEC(hv_vmbus_callback);

static void
vmbus_msg_task(void *xsc, int pending __unused)
{
	struct vmbus_softc *sc = xsc;
	hv_vmbus_message *msg;

	msg = VMBUS_PCPU_GET(sc, message, curcpu) + HV_VMBUS_MESSAGE_SINT;
	for (;;) {
		const hv_vmbus_channel_msg_table_entry *entry;
		hv_vmbus_channel_msg_header *hdr;
		hv_vmbus_channel_msg_type msg_type;

		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE)
			break; /* no message */

		hdr = (hv_vmbus_channel_msg_header *)msg->u.payload;
		msg_type = hdr->message_type;

		if (msg_type >= HV_CHANNEL_MESSAGE_COUNT) {
			printf("VMBUS: unknown message type = %d\n", msg_type);
			goto handled;
		}

		entry = &g_channel_message_table[msg_type];
		if (entry->messageHandler)
			entry->messageHandler(hdr);
handled:
		msg->header.message_type = HV_MESSAGE_TYPE_NONE;
		/*
		 * Make sure the write to message_type (ie set to
		 * HV_MESSAGE_TYPE_NONE) happens before we read the
		 * message_pending and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages
		 * since there is no empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();
		if (msg->header.message_flags.u.message_pending) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(HV_X64_MSR_EOM, 0);
		}
	}
}

/**
 * @brief Interrupt filter routine for VMBUS.
 *
 * The purpose of this routine is to determine the type of VMBUS protocol
 * message to process - an event or a channel message.
 */
static inline int
hv_vmbus_isr(struct vmbus_softc *sc, struct trapframe *frame, int cpu)
{
	hv_vmbus_message *msg, *msg_base;

	/*
	 * The Windows team has advised that we check for events
	 * before checking for messages. This is the way they do it
	 * in Windows when running as a guest in Hyper-V
	 */
	sc->vmbus_event_proc(sc, cpu);

	/* Check if there are actual msgs to be process */
	msg_base = VMBUS_PCPU_GET(sc, message, cpu);
	msg = msg_base + HV_VMBUS_TIMER_SINT;

	/* we call eventtimer process the message */
	if (msg->header.message_type == HV_MESSAGE_TIMER_EXPIRED) {
		msg->header.message_type = HV_MESSAGE_TYPE_NONE;

		/* call intrrupt handler of event timer */
		hv_et_intr(frame);

		/*
		 * Make sure the write to message_type (ie set to
		 * HV_MESSAGE_TYPE_NONE) happens before we read the
		 * message_pending and EOMing. Otherwise, the EOMing will
		 * not deliver any more messages
		 * since there is no empty slot
		 *
		 * NOTE:
		 * mb() is used here, since atomic_thread_fence_seq_cst()
		 * will become compiler fence on UP kernel.
		 */
		mb();

		if (msg->header.message_flags.u.message_pending) {
			/*
			 * This will cause message queue rescan to possibly
			 * deliver another msg from the hypervisor
			 */
			wrmsr(HV_X64_MSR_EOM, 0);
		}
	}

	msg = msg_base + HV_VMBUS_MESSAGE_SINT;
	if (msg->header.message_type != HV_MESSAGE_TYPE_NONE) {
		taskqueue_enqueue(hv_vmbus_g_context.hv_msg_tq[cpu],
		    &hv_vmbus_g_context.hv_msg_task[cpu]);
	}

	return (FILTER_HANDLED);
}

void
hv_vector_handler(struct trapframe *trap_frame)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	int cpu = curcpu;

	/*
	 * Disable preemption.
	 */
	critical_enter();

	/*
	 * Do a little interrupt counting.
	 */
	(*VMBUS_PCPU_GET(sc, intr_cnt, cpu))++;

	hv_vmbus_isr(sc, trap_frame, cpu);

	/*
	 * Enable preemption.
	 */
	critical_exit();
}

static void
vmbus_synic_setup(void *arg __unused)
{
	struct vmbus_softc *sc = vmbus_get_softc();
	int			cpu;
	uint64_t		hv_vcpu_index;
	hv_vmbus_synic_simp	simp;
	hv_vmbus_synic_siefp	siefp;
	hv_vmbus_synic_scontrol sctrl;
	hv_vmbus_synic_sint	shared_sint;
	uint64_t		version;

	cpu = PCPU_GET(cpuid);

	/*
	 * TODO: Check the version
	 */
	version = rdmsr(HV_X64_MSR_SVERSION);

	/*
	 * Setup the Synic's message page
	 */
	simp.as_uint64_t = rdmsr(HV_X64_MSR_SIMP);
	simp.u.simp_enabled = 1;
	simp.u.base_simp_gpa =
	    VMBUS_PCPU_GET(sc, message_dma.hv_paddr, cpu) >> PAGE_SHIFT;

	wrmsr(HV_X64_MSR_SIMP, simp.as_uint64_t);

	/*
	 * Setup the Synic's event page
	 */
	siefp.as_uint64_t = rdmsr(HV_X64_MSR_SIEFP);
	siefp.u.siefp_enabled = 1;
	siefp.u.base_siefp_gpa =
	    VMBUS_PCPU_GET(sc, event_flag_dma.hv_paddr, cpu) >> PAGE_SHIFT;

	wrmsr(HV_X64_MSR_SIEFP, siefp.as_uint64_t);

	/*HV_SHARED_SINT_IDT_VECTOR + 0x20; */
	shared_sint.as_uint64_t = 0;
	shared_sint.u.vector = sc->vmbus_idtvec;
	shared_sint.u.masked = FALSE;
	shared_sint.u.auto_eoi = TRUE;

	wrmsr(HV_X64_MSR_SINT0 + HV_VMBUS_MESSAGE_SINT,
	    shared_sint.as_uint64_t);

	wrmsr(HV_X64_MSR_SINT0 + HV_VMBUS_TIMER_SINT,
	    shared_sint.as_uint64_t);

	/* Enable the global synic bit */
	sctrl.as_uint64_t = rdmsr(HV_X64_MSR_SCONTROL);
	sctrl.u.enable = 1;

	wrmsr(HV_X64_MSR_SCONTROL, sctrl.as_uint64_t);

	hv_vmbus_g_context.syn_ic_initialized = TRUE;

	/*
	 * Set up the cpuid mapping from Hyper-V to FreeBSD.
	 * The array is indexed using FreeBSD cpuid.
	 */
	hv_vcpu_index = rdmsr(HV_X64_MSR_VP_INDEX);
	hv_vmbus_g_context.hv_vcpu_index[cpu] = (uint32_t)hv_vcpu_index;
}

static void
vmbus_synic_teardown(void *arg)
{
	hv_vmbus_synic_sint	shared_sint;
	hv_vmbus_synic_simp	simp;
	hv_vmbus_synic_siefp	siefp;

	if (!hv_vmbus_g_context.syn_ic_initialized)
	    return;

	shared_sint.as_uint64_t = rdmsr(
	    HV_X64_MSR_SINT0 + HV_VMBUS_MESSAGE_SINT);

	shared_sint.u.masked = 1;

	/*
	 * Disable the interrupt 0
	 */
	wrmsr(
	    HV_X64_MSR_SINT0 + HV_VMBUS_MESSAGE_SINT,
	    shared_sint.as_uint64_t);

	shared_sint.as_uint64_t = rdmsr(
	    HV_X64_MSR_SINT0 + HV_VMBUS_TIMER_SINT);

	shared_sint.u.masked = 1;

	/*
	 * Disable the interrupt 1
	 */
	wrmsr(
	    HV_X64_MSR_SINT0 + HV_VMBUS_TIMER_SINT,
	    shared_sint.as_uint64_t);
	simp.as_uint64_t = rdmsr(HV_X64_MSR_SIMP);
	simp.u.simp_enabled = 0;
	simp.u.base_simp_gpa = 0;

	wrmsr(HV_X64_MSR_SIMP, simp.as_uint64_t);

	siefp.as_uint64_t = rdmsr(HV_X64_MSR_SIEFP);
	siefp.u.siefp_enabled = 0;
	siefp.u.base_siefp_gpa = 0;

	wrmsr(HV_X64_MSR_SIEFP, siefp.as_uint64_t);
}

static void
vmbus_dma_alloc(struct vmbus_softc *sc)
{
	int cpu;

	CPU_FOREACH(cpu) {
		/*
		 * Per-cpu messages and event flags.
		 */
		VMBUS_PCPU_GET(sc, message, cpu) = hyperv_dmamem_alloc(
		    bus_get_dma_tag(sc->vmbus_dev), PAGE_SIZE, 0, PAGE_SIZE,
		    VMBUS_PCPU_PTR(sc, message_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
		VMBUS_PCPU_GET(sc, event_flag, cpu) = hyperv_dmamem_alloc(
		    bus_get_dma_tag(sc->vmbus_dev), PAGE_SIZE, 0, PAGE_SIZE,
		    VMBUS_PCPU_PTR(sc, event_flag_dma, cpu),
		    BUS_DMA_WAITOK | BUS_DMA_ZERO);
	}
}

static void
vmbus_dma_free(struct vmbus_softc *sc)
{
	int cpu;

	CPU_FOREACH(cpu) {
		if (VMBUS_PCPU_GET(sc, message, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, message_dma, cpu),
			    VMBUS_PCPU_GET(sc, message, cpu));
			VMBUS_PCPU_GET(sc, message, cpu) = NULL;
		}
		if (VMBUS_PCPU_GET(sc, event_flag, cpu) != NULL) {
			hyperv_dmamem_free(
			    VMBUS_PCPU_PTR(sc, event_flag_dma, cpu),
			    VMBUS_PCPU_GET(sc, event_flag, cpu));
			VMBUS_PCPU_GET(sc, event_flag, cpu) = NULL;
		}
	}
}

static int
vmbus_intr_setup(struct vmbus_softc *sc)
{
	int cpu;

	/*
	 * Find a free IDT vector for vmbus messages/events.
	 */
	sc->vmbus_idtvec = lapic_ipi_alloc(IDTVEC(hv_vmbus_callback));
	if (sc->vmbus_idtvec < 0) {
		device_printf(sc->vmbus_dev, "cannot find free IDT vector\n");
		return ENXIO;
	}
	if(bootverbose) {
		device_printf(sc->vmbus_dev, "vmbus IDT vector %d\n",
		    sc->vmbus_idtvec);
	}

	CPU_FOREACH(cpu) {
		char buf[MAXCOMLEN + 1];

		snprintf(buf, sizeof(buf), "cpu%d:hyperv", cpu);
		intrcnt_add(buf, VMBUS_PCPU_PTR(sc, intr_cnt, cpu));
	}

	/*
	 * Per cpu setup.
	 */
	CPU_FOREACH(cpu) {
		cpuset_t cpu_mask;

		/*
		 * Setup taskqueue to handle events
		 */
		hv_vmbus_g_context.hv_event_queue[cpu] =
		    taskqueue_create_fast("hyperv event", M_WAITOK,
		    taskqueue_thread_enqueue,
		    &hv_vmbus_g_context.hv_event_queue[cpu]);
		CPU_SETOF(cpu, &cpu_mask);
		taskqueue_start_threads_cpuset(
		    &hv_vmbus_g_context.hv_event_queue[cpu], 1, PI_NET,
		    &cpu_mask, "hvevent%d", cpu);

		/*
		 * Setup per-cpu tasks and taskqueues to handle msg.
		 */
		hv_vmbus_g_context.hv_msg_tq[cpu] = taskqueue_create_fast(
		    "hyperv msg", M_WAITOK, taskqueue_thread_enqueue,
		    &hv_vmbus_g_context.hv_msg_tq[cpu]);
		CPU_SETOF(cpu, &cpu_mask);
		taskqueue_start_threads_cpuset(
		    &hv_vmbus_g_context.hv_msg_tq[cpu], 1, PI_NET,
		    &cpu_mask, "hvmsg%d", cpu);
		TASK_INIT(&hv_vmbus_g_context.hv_msg_task[cpu], 0,
		    vmbus_msg_task, sc);
	}
	return 0;
}

static void
vmbus_intr_teardown(struct vmbus_softc *sc)
{
	int cpu;

	CPU_FOREACH(cpu) {
		if (hv_vmbus_g_context.hv_event_queue[cpu] != NULL) {
			taskqueue_free(hv_vmbus_g_context.hv_event_queue[cpu]);
			hv_vmbus_g_context.hv_event_queue[cpu] = NULL;
		}
		if (hv_vmbus_g_context.hv_msg_tq[cpu] != NULL) {
			taskqueue_drain(hv_vmbus_g_context.hv_msg_tq[cpu],
			    &hv_vmbus_g_context.hv_msg_task[cpu]);
			taskqueue_free(hv_vmbus_g_context.hv_msg_tq[cpu]);
			hv_vmbus_g_context.hv_msg_tq[cpu] = NULL;
		}
	}
	if (sc->vmbus_idtvec >= 0) {
		lapic_ipi_free(sc->vmbus_idtvec);
		sc->vmbus_idtvec = -1;
	}
}

static int
vmbus_read_ivar(
	device_t	dev,
	device_t	child,
	int		index,
	uintptr_t*	result)
{
	struct hv_device *child_dev_ctx = device_get_ivars(child);

	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
		*result = (uintptr_t) &child_dev_ctx->class_id;
		return (0);
	case HV_VMBUS_IVAR_INSTANCE:
		*result = (uintptr_t) &child_dev_ctx->device_id;
		return (0);
	case HV_VMBUS_IVAR_DEVCTX:
		*result = (uintptr_t) child_dev_ctx;
		return (0);
	case HV_VMBUS_IVAR_NODE:
		*result = (uintptr_t) child_dev_ctx->device;
		return (0);
	}
	return (ENOENT);
}

static int
vmbus_write_ivar(
	device_t	dev,
	device_t	child,
	int		index,
	uintptr_t	value)
{
	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
	case HV_VMBUS_IVAR_INSTANCE:
	case HV_VMBUS_IVAR_DEVCTX:
	case HV_VMBUS_IVAR_NODE:
		/* read-only */
		return (EINVAL);
	}
	return (ENOENT);
}

static int
vmbus_child_pnpinfo_str(device_t dev, device_t child, char *buf, size_t buflen)
{
	char guidbuf[40];
	struct hv_device *dev_ctx = device_get_ivars(child);

	if (dev_ctx == NULL)
		return (0);

	strlcat(buf, "classid=", buflen);
	snprintf_hv_guid(guidbuf, sizeof(guidbuf), &dev_ctx->class_id);
	strlcat(buf, guidbuf, buflen);

	strlcat(buf, " deviceid=", buflen);
	snprintf_hv_guid(guidbuf, sizeof(guidbuf), &dev_ctx->device_id);
	strlcat(buf, guidbuf, buflen);

	return (0);
}

struct hv_device*
hv_vmbus_child_device_create(
	hv_guid		type,
	hv_guid		instance,
	hv_vmbus_channel*	channel)
{
	hv_device* child_dev;

	/*
	 * Allocate the new child device
	 */
	child_dev = malloc(sizeof(hv_device), M_DEVBUF,
			M_WAITOK |  M_ZERO);

	child_dev->channel = channel;
	memcpy(&child_dev->class_id, &type, sizeof(hv_guid));
	memcpy(&child_dev->device_id, &instance, sizeof(hv_guid));

	return (child_dev);
}

int
snprintf_hv_guid(char *buf, size_t sz, const hv_guid *guid)
{
	int cnt;
	const unsigned char *d = guid->data;

	cnt = snprintf(buf, sz,
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6],
		d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
	return (cnt);
}

int
hv_vmbus_child_device_register(struct hv_device *child_dev)
{
	device_t child;

	if (bootverbose) {
		char name[40];
		snprintf_hv_guid(name, sizeof(name), &child_dev->class_id);
		printf("VMBUS: Class ID: %s\n", name);
	}

	child = device_add_child(vmbus_get_device(), NULL, -1);
	child_dev->device = child;
	device_set_ivars(child, child_dev);

	return (0);
}

int
hv_vmbus_child_device_unregister(struct hv_device *child_dev)
{
	int ret = 0;
	/*
	 * XXXKYS: Ensure that this is the opposite of
	 * device_add_child()
	 */
	mtx_lock(&Giant);
	ret = device_delete_child(vmbus_get_device(), child_dev->device);
	mtx_unlock(&Giant);
	return(ret);
}

static int
vmbus_probe(device_t dev)
{
	if (ACPI_ID_PROBE(device_get_parent(dev), dev, vmbus_ids) == NULL ||
	    device_get_unit(dev) != 0 || vm_guest != VM_GUEST_HV)
		return (ENXIO);

	device_set_desc(dev, "Hyper-V Vmbus");

	return (BUS_PROBE_DEFAULT);
}

/**
 * @brief Main vmbus driver initialization routine.
 *
 * Here, we
 * - initialize the vmbus driver context
 * - setup various driver entry points
 * - invoke the vmbus hv main init routine
 * - get the irq resource
 * - invoke the vmbus to add the vmbus root device
 * - setup the vmbus root device
 * - retrieve the channel offers
 */
static int
vmbus_bus_init(void)
{
	struct vmbus_softc *sc;
	int ret;

	if (vmbus_inited)
		return (0);

	vmbus_inited = 1;
	sc = vmbus_get_softc();

	/*
	 * Setup interrupt.
	 */
	ret = vmbus_intr_setup(sc);
	if (ret != 0)
		goto cleanup;

	/*
	 * Allocate DMA stuffs.
	 */
	vmbus_dma_alloc(sc);

	if (bootverbose)
		printf("VMBUS: Calling smp_rendezvous, smp_started = %d\n",
		    smp_started);
	smp_rendezvous(NULL, vmbus_synic_setup, NULL, NULL);

	/*
	 * Connect to VMBus in the root partition
	 */
	ret = hv_vmbus_connect();

	if (ret != 0)
		goto cleanup;

	if (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008 ||
	    hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)
		sc->vmbus_event_proc = vmbus_event_proc_compat;
	else
		sc->vmbus_event_proc = vmbus_event_proc;

	hv_vmbus_request_channel_offers();

	vmbus_scan();
	bus_generic_attach(sc->vmbus_dev);
	device_printf(sc->vmbus_dev, "device scan, probe and attach done\n");

	return (ret);

cleanup:
	vmbus_dma_free(sc);
	vmbus_intr_teardown(sc);

	return (ret);
}

static void
vmbus_event_proc_dummy(struct vmbus_softc *sc __unused, int cpu __unused)
{
}

static int
vmbus_attach(device_t dev)
{
	vmbus_sc = device_get_softc(dev);
	vmbus_sc->vmbus_dev = dev;
	vmbus_sc->vmbus_idtvec = -1;

	/*
	 * Event processing logic will be configured:
	 * - After the vmbus protocol version negotiation.
	 * - Before we request channel offers.
	 */
	vmbus_sc->vmbus_event_proc = vmbus_event_proc_dummy;

#ifndef EARLY_AP_STARTUP
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold)
#endif
		vmbus_bus_init();

	bus_generic_probe(dev);
	return (0);
}

static void
vmbus_sysinit(void *arg __unused)
{
	if (vm_guest != VM_GUEST_HV || vmbus_get_softc() == NULL)
		return;

#ifndef EARLY_AP_STARTUP
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible, as indicated by the
	 * global cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) 
#endif
		vmbus_bus_init();
}

static int
vmbus_detach(device_t dev)
{
	struct vmbus_softc *sc = device_get_softc(dev);

	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	smp_rendezvous(NULL, vmbus_synic_teardown, NULL, NULL);

	vmbus_dma_free(sc);
	vmbus_intr_teardown(sc);

	return (0);
}

static device_method_t vmbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			vmbus_probe),
	DEVMETHOD(device_attach,		vmbus_attach),
	DEVMETHOD(device_detach,		vmbus_detach),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		bus_generic_add_child),
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		vmbus_read_ivar),
	DEVMETHOD(bus_write_ivar,		vmbus_write_ivar),
	DEVMETHOD(bus_child_pnpinfo_str,	vmbus_child_pnpinfo_str),

	DEVMETHOD_END
};

static driver_t vmbus_driver = {
	"vmbus",
	vmbus_methods,
	sizeof(struct vmbus_softc)
};

static devclass_t vmbus_devclass;

DRIVER_MODULE(vmbus, acpi, vmbus_driver, vmbus_devclass, NULL, NULL);
MODULE_DEPEND(vmbus, acpi, 1, 1, 1);
MODULE_VERSION(vmbus, 1);

#ifndef EARLY_AP_STARTUP
/*
 * NOTE:
 * We have to start as the last step of SI_SUB_SMP, i.e. after SMP is
 * initialized.
 */
SYSINIT(vmbus_initialize, SI_SUB_SMP, SI_ORDER_ANY, vmbus_sysinit, NULL);
#endif
