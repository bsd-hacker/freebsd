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

/**
 * Implements low-level interactions with Hypver-V/Azure
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/timetc.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/hyperv_machdep.h>
#include <dev/hyperv/vmbus/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#define HYPERV_FREEBSD_BUILD		0ULL
#define HYPERV_FREEBSD_VERSION		((uint64_t)__FreeBSD_version)
#define HYPERV_FREEBSD_OSID		0ULL

#define MSR_HV_GUESTID_BUILD_FREEBSD	\
	(HYPERV_FREEBSD_BUILD & MSR_HV_GUESTID_BUILD_MASK)
#define MSR_HV_GUESTID_VERSION_FREEBSD	\
	((HYPERV_FREEBSD_VERSION << MSR_HV_GUESTID_VERSION_SHIFT) & \
	 MSR_HV_GUESTID_VERSION_MASK)
#define MSR_HV_GUESTID_OSID_FREEBSD	\
	((HYPERV_FREEBSD_OSID << MSR_HV_GUESTID_OSID_SHIFT) & \
	 MSR_HV_GUESTID_OSID_MASK)

#define MSR_HV_GUESTID_FREEBSD		\
	(MSR_HV_GUESTID_BUILD_FREEBSD |	\
	 MSR_HV_GUESTID_VERSION_FREEBSD | \
	 MSR_HV_GUESTID_OSID_FREEBSD |	\
	 MSR_HV_GUESTID_OSTYPE_FREEBSD)

struct hypercall_ctx {
	void			*hc_addr;
	struct hyperv_dma	hc_dma;
};

static u_int	hyperv_get_timecount(struct timecounter *tc);

u_int		hyperv_features;
u_int		hyperv_recommends;

static u_int	hyperv_pm_features;
static u_int	hyperv_features3;

static struct timecounter	hyperv_timecounter = {
	.tc_get_timecount	= hyperv_get_timecount,
	.tc_poll_pps		= NULL,
	.tc_counter_mask	= 0xffffffff,
	.tc_frequency		= HYPERV_TIMER_FREQ,
	.tc_name		= "Hyper-V",
	.tc_quality		= 2000,
	.tc_flags		= 0,
	.tc_priv		= NULL
};

static struct hypercall_ctx	hypercall_context;

static u_int
hyperv_get_timecount(struct timecounter *tc __unused)
{
	return rdmsr(MSR_HV_TIME_REF_COUNT);
}

/**
 * @brief Invoke the specified hypercall
 */
static uint64_t
hv_vmbus_do_hypercall(uint64_t value, void *input, void *output)
{
	uint64_t in_paddr = 0, out_paddr = 0;

	if (input != NULL)
		in_paddr = hv_get_phys_addr(input);
	if (output != NULL)
		out_paddr = hv_get_phys_addr(output);

	return hypercall_md(hypercall_context.hc_addr, value,
	    in_paddr, out_paddr);
}

/**
 * @brief Post a message using the hypervisor message IPC.
 * (This involves a hypercall.)
 */
hv_vmbus_status
hv_vmbus_post_msg_via_msg_ipc(
	hv_vmbus_connection_id	connection_id,
	hv_vmbus_msg_type	message_type,
	void*			payload,
	size_t			payload_size)
{
	struct alignedinput {
	    uint64_t alignment8;
	    hv_vmbus_input_post_message msg;
	};

	hv_vmbus_input_post_message*	aligned_msg;
	hv_vmbus_status 		status;
	size_t				addr;

	if (payload_size > HV_MESSAGE_PAYLOAD_BYTE_COUNT)
	    return (EMSGSIZE);

	addr = (size_t) malloc(sizeof(struct alignedinput), M_DEVBUF,
			    M_ZERO | M_NOWAIT);
	KASSERT(addr != 0,
	    ("Error VMBUS: malloc failed to allocate message buffer!"));
	if (addr == 0)
	    return (ENOMEM);

	aligned_msg = (hv_vmbus_input_post_message*)
	    (HV_ALIGN_UP(addr, HV_HYPERCALL_PARAM_ALIGN));

	aligned_msg->connection_id = connection_id;
	aligned_msg->message_type = message_type;
	aligned_msg->payload_size = payload_size;
	memcpy((void*) aligned_msg->payload, payload, payload_size);

	status = hv_vmbus_do_hypercall(
		    HV_CALL_POST_MESSAGE, aligned_msg, 0) & 0xFFFF;

	free((void *) addr, M_DEVBUF);
	return (status);
}

/**
 * @brief Signal an event on the specified connection using the hypervisor
 * event IPC. (This involves a hypercall.)
 */
hv_vmbus_status
hv_vmbus_signal_event(void *con_id)
{
	hv_vmbus_status status;

	status = hv_vmbus_do_hypercall(
		    HV_CALL_SIGNAL_EVENT,
		    con_id,
		    0) & 0xFFFF;

	return (status);
}

int
hyperv_guid2str(const struct hv_guid *guid, char *buf, size_t sz)
{
	const uint8_t *d = guid->data;

	return snprintf(buf, sz, "%02x%02x%02x%02x-"
	    "%02x%02x-%02x%02x-%02x%02x-"
	    "%02x%02x%02x%02x%02x%02x",
	    d[3], d[2], d[1], d[0],
	    d[5], d[4], d[7], d[6], d[8], d[9],
	    d[10], d[11], d[12], d[13], d[14], d[15]);
}

static bool
hyperv_identify(void)
{
	u_int regs[4];
	unsigned int maxleaf;

	if (vm_guest != VM_GUEST_HV)
		return (false);

	do_cpuid(CPUID_LEAF_HV_MAXLEAF, regs);
	maxleaf = regs[0];
	if (maxleaf < CPUID_LEAF_HV_LIMITS)
		return (false);

	do_cpuid(CPUID_LEAF_HV_INTERFACE, regs);
	if (regs[0] != CPUID_HV_IFACE_HYPERV)
		return (false);

	do_cpuid(CPUID_LEAF_HV_FEATURES, regs);
	if ((regs[0] & CPUID_HV_MSR_HYPERCALL) == 0) {
		/*
		 * Hyper-V w/o Hypercall is impossible; someone
		 * is faking Hyper-V.
		 */
		return (false);
	}
	hyperv_features = regs[0];
	hyperv_pm_features = regs[2];
	hyperv_features3 = regs[3];

	do_cpuid(CPUID_LEAF_HV_IDENTITY, regs);
	printf("Hyper-V Version: %d.%d.%d [SP%d]\n",
	    regs[1] >> 16, regs[1] & 0xffff, regs[0], regs[2]);

	printf("  Features=0x%b\n", hyperv_features,
	    "\020"
	    "\001VPRUNTIME"	/* MSR_HV_VP_RUNTIME */
	    "\002TMREFCNT"	/* MSR_HV_TIME_REF_COUNT */
	    "\003SYNIC"		/* MSRs for SynIC */
	    "\004SYNTM"		/* MSRs for SynTimer */
	    "\005APIC"		/* MSR_HV_{EOI,ICR,TPR} */
	    "\006HYPERCALL"	/* MSR_HV_{GUEST_OS_ID,HYPERCALL} */
	    "\007VPINDEX"	/* MSR_HV_VP_INDEX */
	    "\010RESET"		/* MSR_HV_RESET */
	    "\011STATS"		/* MSR_HV_STATS_ */
	    "\012REFTSC"	/* MSR_HV_REFERENCE_TSC */
	    "\013IDLE"		/* MSR_HV_GUEST_IDLE */
	    "\014TMFREQ"	/* MSR_HV_{TSC,APIC}_FREQUENCY */
	    "\015DEBUG");	/* MSR_HV_SYNTH_DEBUG_ */
	printf("  PM Features=0x%b [C%u]\n",
	    (hyperv_pm_features & ~CPUPM_HV_CSTATE_MASK),
	    "\020"
	    "\005C3HPET",	/* HPET is required for C3 state */
	    CPUPM_HV_CSTATE(hyperv_pm_features));
	printf("  Features3=0x%b\n", hyperv_features3,
	    "\020"
	    "\001MWAIT"		/* MWAIT */
	    "\002DEBUG"		/* guest debug support */
	    "\003PERFMON"	/* performance monitor */
	    "\004PCPUDPE"	/* physical CPU dynamic partition event */
	    "\005XMMHC"		/* hypercall input through XMM regs */
	    "\006IDLE"		/* guest idle support */
	    "\007SLEEP"		/* hypervisor sleep support */
	    "\010NUMA"		/* NUMA distance query support */
	    "\011TMFREQ"	/* timer frequency query (TSC, LAPIC) */
	    "\012SYNCMC"	/* inject synthetic machine checks */
	    "\013CRASH"		/* MSRs for guest crash */
	    "\014DEBUGMSR"	/* MSRs for guest debug */
	    "\015NPIEP"		/* NPIEP */
	    "\016HVDIS");	/* disabling hypervisor */

	do_cpuid(CPUID_LEAF_HV_RECOMMENDS, regs);
	hyperv_recommends = regs[0];
	if (bootverbose)
		printf("  Recommends: %08x %08x\n", regs[0], regs[1]);

	do_cpuid(CPUID_LEAF_HV_LIMITS, regs);
	if (bootverbose) {
		printf("  Limits: Vcpu:%d Lcpu:%d Int:%d\n",
		    regs[0], regs[1], regs[2]);
	}

	if (maxleaf >= CPUID_LEAF_HV_HWFEATURES) {
		do_cpuid(CPUID_LEAF_HV_HWFEATURES, regs);
		if (bootverbose) {
			printf("  HW Features: %08x, AMD: %08x\n",
			    regs[0], regs[3]);
		}
	}

	return (true);
}

static void
hyperv_init(void *dummy __unused)
{
	if (!hyperv_identify()) {
		/* Not Hyper-V; reset guest id to the generic one. */
		if (vm_guest == VM_GUEST_HV)
			vm_guest = VM_GUEST_VM;
		return;
	}

	/* Set guest id */
	wrmsr(MSR_HV_GUEST_OS_ID, MSR_HV_GUESTID_FREEBSD);

	if (hyperv_features & CPUID_HV_MSR_TIME_REFCNT) {
		/* Register Hyper-V timecounter */
		tc_init(&hyperv_timecounter);
	}
}
SYSINIT(hyperv_initialize, SI_SUB_HYPERVISOR, SI_ORDER_FIRST, hyperv_init,
    NULL);

static void
hypercall_memfree(void)
{
	hyperv_dmamem_free(&hypercall_context.hc_dma,
	    hypercall_context.hc_addr);
	hypercall_context.hc_addr = NULL;
}

static void
hypercall_create(void *arg __unused)
{
	uint64_t hc, hc_orig;

	if (vm_guest != VM_GUEST_HV)
		return;

	hypercall_context.hc_addr = hyperv_dmamem_alloc(NULL, PAGE_SIZE, 0,
	    PAGE_SIZE, &hypercall_context.hc_dma, BUS_DMA_WAITOK);
	if (hypercall_context.hc_addr == NULL) {
		printf("hyperv: Hypercall page allocation failed\n");
		/* Can't perform any Hyper-V specific actions */
		vm_guest = VM_GUEST_VM;
		return;
	}

	/* Get the 'reserved' bits, which requires preservation. */
	hc_orig = rdmsr(MSR_HV_HYPERCALL);

	/*
	 * Setup the Hypercall page.
	 *
	 * NOTE: 'reserved' bits MUST be preserved.
	 */
	hc = ((hypercall_context.hc_dma.hv_paddr >> PAGE_SHIFT) <<
	    MSR_HV_HYPERCALL_PGSHIFT) |
	    (hc_orig & MSR_HV_HYPERCALL_RSVD_MASK) |
	    MSR_HV_HYPERCALL_ENABLE;
	wrmsr(MSR_HV_HYPERCALL, hc);

	/*
	 * Confirm that Hypercall page did get setup.
	 */
	hc = rdmsr(MSR_HV_HYPERCALL);
	if ((hc & MSR_HV_HYPERCALL_ENABLE) == 0) {
		printf("hyperv: Hypercall setup failed\n");
		hypercall_memfree();
		/* Can't perform any Hyper-V specific actions */
		vm_guest = VM_GUEST_VM;
		return;
	}
	if (bootverbose)
		printf("hyperv: Hypercall created\n");
}
SYSINIT(hypercall_ctor, SI_SUB_DRIVERS, SI_ORDER_FIRST, hypercall_create, NULL);

static void
hypercall_destroy(void *arg __unused)
{
	uint64_t hc;

	if (hypercall_context.hc_addr == NULL)
		return;

	/* Disable Hypercall */
	hc = rdmsr(MSR_HV_HYPERCALL);
	wrmsr(MSR_HV_HYPERCALL, (hc & MSR_HV_HYPERCALL_RSVD_MASK));
	hypercall_memfree();

	if (bootverbose)
		printf("hyperv: Hypercall destroyed\n");
}
SYSUNINIT(hypercall_dtor, SI_SUB_DRIVERS, SI_ORDER_FIRST, hypercall_destroy,
    NULL);
