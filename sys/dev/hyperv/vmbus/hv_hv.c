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
#include <dev/hyperv/vmbus/hyperv_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

#define HV_NANOSECONDS_PER_SEC		1000000000L

#define	HYPERV_INTERFACE		0x31237648	/* HV#1 */

/*
 * The guest OS needs to register the guest ID with the hypervisor.
 * The guest ID is a 64 bit entity and the structure of this ID is
 * specified in the Hyper-V specification:
 *
 * http://msdn.microsoft.com/en-us/library/windows/
 * hardware/ff542653%28v=vs.85%29.aspx
 *
 * While the current guideline does not specify how FreeBSD guest ID(s)
 * need to be generated, our plan is to publish the guidelines for
 * FreeBSD and other guest operating systems that currently are hosted
 * on Hyper-V. The implementation here conforms to this yet
 * unpublished guidelines.
 *
 * Bit(s)
 * 63    - Indicates if the OS is Open Source or not; 1 is Open Source
 * 62:56 - Os Type: FreeBSD is 0x02
 * 55:48 - Distro specific identification
 * 47:16 - FreeBSD kernel version number
 * 15:0  - Distro specific identification
 */
#define HYPERV_GUESTID_OSS		(0x1ULL << 63)
#define HYPERV_GUESTID_FREEBSD		(0x02ULL << 56)
#define HYPERV_GUESTID(id)				\
	(HYPERV_GUESTID_OSS | HYPERV_GUESTID_FREEBSD |	\
	 (((uint64_t)(((id) & 0xff0000) >> 16)) << 48) |\
	 (((uint64_t)__FreeBSD_version) << 16) |	\
	 ((uint64_t)((id) & 0x00ffff)))

struct hypercall_ctx {
	void			*hc_addr;
	struct hyperv_dma	hc_dma;
};

static struct hypercall_ctx	hypercall_context;

static u_int hv_get_timecount(struct timecounter *tc);

u_int	hyperv_features;
u_int	hyperv_recommends;

static u_int	hyperv_pm_features;
static u_int	hyperv_features3;

/**
 * Globals
 */
hv_vmbus_context hv_vmbus_g_context = {
	.syn_ic_initialized = FALSE,
};

static struct timecounter hv_timecounter = {
	hv_get_timecount, 0, ~0u, HV_NANOSECONDS_PER_SEC/100, "Hyper-V", HV_NANOSECONDS_PER_SEC/100
};

static u_int
hv_get_timecount(struct timecounter *tc)
{
	u_int now = rdmsr(HV_X64_MSR_TIME_REF_COUNT);
	return (now);
}

/**
 * @brief Invoke the specified hypercall
 */
static uint64_t
hv_vmbus_do_hypercall(uint64_t control, void* input, void* output)
{
#ifdef __x86_64__
	uint64_t hv_status = 0;
	uint64_t input_address = (input) ? hv_get_phys_addr(input) : 0;
	uint64_t output_address = (output) ? hv_get_phys_addr(output) : 0;
	volatile void *hypercall_page = hypercall_context.hc_addr;

	__asm__ __volatile__ ("mov %0, %%r8" : : "r" (output_address): "r8");
	__asm__ __volatile__ ("call *%3" : "=a"(hv_status):
				"c" (control), "d" (input_address),
				"m" (hypercall_page));
	return (hv_status);
#else
	uint32_t control_high = control >> 32;
	uint32_t control_low = control & 0xFFFFFFFF;
	uint32_t hv_status_high = 1;
	uint32_t hv_status_low = 1;
	uint64_t input_address = (input) ? hv_get_phys_addr(input) : 0;
	uint32_t input_address_high = input_address >> 32;
	uint32_t input_address_low = input_address & 0xFFFFFFFF;
	uint64_t output_address = (output) ? hv_get_phys_addr(output) : 0;
	uint32_t output_address_high = output_address >> 32;
	uint32_t output_address_low = output_address & 0xFFFFFFFF;
	volatile void *hypercall_page = hypercall_context.hc_addr;

	__asm__ __volatile__ ("call *%8" : "=d"(hv_status_high),
				"=a"(hv_status_low) : "d" (control_high),
				"a" (control_low), "b" (input_address_high),
				"c" (input_address_low),
				"D"(output_address_high),
				"S"(output_address_low), "m" (hypercall_page));
	return (hv_status_low | ((uint64_t)hv_status_high << 32));
#endif /* __x86_64__ */
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


static bool
hyperv_identify(void)
{
	u_int regs[4];
	unsigned int maxLeaf;
	unsigned int op;

	if (vm_guest != VM_GUEST_HV)
		return (false);

	op = HV_CPU_ID_FUNCTION_HV_VENDOR_AND_MAX_FUNCTION;
	do_cpuid(op, regs);
	maxLeaf = regs[0];
	if (maxLeaf < HV_CPU_ID_FUNCTION_MS_HV_IMPLEMENTATION_LIMITS)
		return (false);

	op = HV_CPU_ID_FUNCTION_HV_INTERFACE;
	do_cpuid(op, regs);
	if (regs[0] != HYPERV_INTERFACE)
		return (false);

	op = HV_CPU_ID_FUNCTION_MS_HV_FEATURES;
	do_cpuid(op, regs);
	if ((regs[0] & HV_FEATURE_MSR_HYPERCALL) == 0) {
		/*
		 * Hyper-V w/o Hypercall is impossible; someone
		 * is faking Hyper-V.
		 */
		return (false);
	}
	hyperv_features = regs[0];
	hyperv_pm_features = regs[2];
	hyperv_features3 = regs[3];

	op = HV_CPU_ID_FUNCTION_MS_HV_VERSION;
	do_cpuid(op, regs);
	printf("Hyper-V Version: %d.%d.%d [SP%d]\n",
	    regs[1] >> 16, regs[1] & 0xffff, regs[0], regs[2]);

	printf("  Features=0x%b\n", hyperv_features,
	    "\020"
	    "\001VPRUNTIME"	/* MSR_VP_RUNTIME */
	    "\002TMREFCNT"	/* MSR_TIME_REF_COUNT */
	    "\003SYNIC"		/* MSRs for SynIC */
	    "\004SYNTM"		/* MSRs for SynTimer */
	    "\005APIC"		/* MSR_{EOI,ICR,TPR} */
	    "\006HYPERCALL"	/* MSR_{GUEST_OS_ID,HYPERCALL} */
	    "\007VPINDEX"	/* MSR_VP_INDEX */
	    "\010RESET"		/* MSR_RESET */
	    "\011STATS"		/* MSR_STATS_ */
	    "\012REFTSC"	/* MSR_REFERENCE_TSC */
	    "\013IDLE"		/* MSR_GUEST_IDLE */
	    "\014TMFREQ"	/* MSR_{TSC,APIC}_FREQUENCY */
	    "\015DEBUG");	/* MSR_SYNTH_DEBUG_ */
	printf("  PM Features=max C%u, 0x%b\n",
	    HV_PM_FEATURE_CSTATE(hyperv_pm_features),
	    (hyperv_pm_features & ~HV_PM_FEATURE_CSTATE_MASK),
	    "\020"
	    "\005C3HPET");	/* HPET is required for C3 state */
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

	op = HV_CPU_ID_FUNCTION_MS_HV_ENLIGHTENMENT_INFORMATION;
	do_cpuid(op, regs);
	hyperv_recommends = regs[0];
	if (bootverbose)
		printf("  Recommends: %08x %08x\n", regs[0], regs[1]);

	op = HV_CPU_ID_FUNCTION_MS_HV_IMPLEMENTATION_LIMITS;
	do_cpuid(op, regs);
	if (bootverbose) {
		printf("  Limits: Vcpu:%d Lcpu:%d Int:%d\n",
		    regs[0], regs[1], regs[2]);
	}

	if (maxLeaf >= HV_CPU_ID_FUNCTION_MS_HV_HARDWARE_FEATURE) {
		op = HV_CPU_ID_FUNCTION_MS_HV_HARDWARE_FEATURE;
		do_cpuid(op, regs);
		if (bootverbose) {
			printf("  HW Features: %08x AMD: %08x\n",
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

	/* Write guest id */
	wrmsr(HV_X64_MSR_GUEST_OS_ID, HYPERV_GUESTID(0));

	if (hyperv_features & HV_FEATURE_MSR_TIME_REFCNT) {
		/* Register virtual timecount */
		tc_init(&hv_timecounter);
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
	if (hypercall_context.hc_addr == NULL)
		return;

	/* Disable Hypercall */
	wrmsr(MSR_HV_HYPERCALL, 0);
	hypercall_memfree();

	if (bootverbose)
		printf("hyperv: Hypercall destroyed\n");
}
SYSUNINIT(hypercall_dtor, SI_SUB_DRIVERS, SI_ORDER_FIRST, hypercall_destroy,
    NULL);
