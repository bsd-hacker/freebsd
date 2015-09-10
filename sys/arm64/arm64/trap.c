/*-
 * Copyright (c) 2014 Andrew Turner
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#ifdef KDB
#include <sys/kdb.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pcpu.h>
#include <machine/vmparam.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#endif

#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef KDB
#include <machine/db_machdep.h>
#endif

#ifdef DDB
#include <ddb/db_output.h>
#endif

extern register_t fsu_intr_fault;

/* Called from exception.S */
void do_el1h_sync(struct trapframe *);
void do_el0_sync(struct trapframe *);
void do_el0_error(struct trapframe *);

int (*dtrace_invop_jump_addr)(struct trapframe *);

static __inline void
call_trapsignal(struct thread *td, int sig, int code, void *addr)
{
	ksiginfo_t ksi;

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = code;
	ksi.ksi_addr = addr;
	trapsignal(td, &ksi);
}

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	register_t *ap;
	int nap;

	nap = 8;
	p = td->td_proc;
	ap = td->td_frame->tf_x;

	sa->code = td->td_frame->tf_x[8];

	if (sa->code == SYS_syscall || sa->code == SYS___syscall) {
		sa->code = *ap++;
		nap--;
	}

	if (p->p_sysent->sv_mask)
		sa->code &= p->p_sysent->sv_mask;
	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &p->p_sysent->sv_table[0];
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	sa->narg = sa->callp->sy_narg;
	memcpy(sa->args, ap, nap * sizeof(register_t));
	if (sa->narg > nap)
		panic("ARM64TODO: Could we have more than 8 args?");

	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

	return (0);
}

#include "../../kern/subr_syscall.c"

static void
svc_handler(struct trapframe *frame)
{
	struct syscall_args sa;
	struct thread *td;
	int error;

	td = curthread;
	td->td_frame = frame;

	error = syscallenter(td, &sa);
	syscallret(td, error, &sa);
}

static void
data_abort(struct trapframe *frame, uint64_t esr, int lower)
{
	struct vm_map *map;
	struct thread *td;
	struct proc *p;
	struct pcb *pcb;
	vm_prot_t ftype;
	vm_offset_t va;
	uint64_t far;
	int error, sig, ucode;

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		return;
	}
#endif

	td = curthread;
	pcb = td->td_pcb;

	/*
	 * Special case for fuswintr and suswintr. These can't sleep so
	 * handle them early on in the trap handler.
	 */
	if (__predict_false(pcb->pcb_onfault == (vm_offset_t)&fsu_intr_fault)) {
		frame->tf_elr = pcb->pcb_onfault;
		return;
	}

	far = READ_SPECIALREG(far_el1);
	p = td->td_proc;

	if (lower)
		map = &td->td_proc->p_vmspace->vm_map;
	else {
		/* The top bit tells us which range to use */
		if ((far >> 63) == 1)
			map = kernel_map;
		else
			map = &td->td_proc->p_vmspace->vm_map;
	}

	va = trunc_page(far);
	ftype = ((esr >> 6) & 1) ? VM_PROT_READ | VM_PROT_WRITE : VM_PROT_READ;

	/* Fault in the page. */
	error = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	if (error != KERN_SUCCESS) {
		if (lower) {
			sig = SIGSEGV;
			if (error == KERN_PROTECTION_FAILURE)
				ucode = SEGV_ACCERR;
			else
				ucode = SEGV_MAPERR;
			call_trapsignal(td, sig, ucode, (void *)far);
		} else {
			if (td->td_intr_nesting_level == 0 &&
			    pcb->pcb_onfault != 0) {
				frame->tf_x[0] = error;
				frame->tf_elr = pcb->pcb_onfault;
				return;
			}
#ifdef KDB
			if (debugger_on_panic || kdb_active)
				if (kdb_trap(ESR_ELx_EXCEPTION(esr), 0, frame))
					return;
#endif
			panic("vm_fault failed: %lx", frame->tf_elr);
		}
	}

	if (lower)
		userret(td, frame);
}

static void
print_registers(struct trapframe *frame)
{
	u_int reg;

	for (reg = 0; reg < 31; reg++) {
		printf(" %sx%d: %16lx\n", (reg < 10) ? " " : "", reg,
		    frame->tf_x[reg]);
	}
	printf("  sp: %16lx\n", frame->tf_sp);
	printf("  lr: %16lx\n", frame->tf_lr);
	printf(" elr: %16lx\n", frame->tf_elr);
	printf("spsr: %16lx\n", frame->tf_spsr);
}

void
do_el1h_sync(struct trapframe *frame)
{
	uint32_t exception;
	uint64_t esr;

	/* Read the esr register to get the exception details */
	esr = READ_SPECIALREG(esr_el1);
	exception = ESR_ELx_EXCEPTION(esr);

#ifdef KDTRACE_HOOKS
	if (dtrace_trap_func != NULL && (*dtrace_trap_func)(frame, exception))
		return;
#endif

	/*
	 * Sanity check we are in an exception er can handle. The IL bit
	 * is used to indicate the instruction length, except in a few
	 * exceptions described in the ARMv8 ARM.
	 *
	 * It is unclear in some cases if the bit is implementation defined.
	 * The Foundation Model and QEMU disagree on if the IL bit should
	 * be set when we are in a data fault from the same EL and the ISV
	 * bit (bit 24) is also set.
	 */
	KASSERT((esr & ESR_ELx_IL) == ESR_ELx_IL ||
	    (exception == EXCP_DATA_ABORT && ((esr & ISS_DATA_ISV) == 0)),
	    ("Invalid instruction length in exception"));

	CTR4(KTR_TRAP,
	    "do_el1_sync: curthread: %p, esr %lx, elr: %lx, frame: %p",
	    curthread, esr, frame->tf_elr, frame);

	switch(exception) {
	case EXCP_FP_SIMD:
	case EXCP_TRAP_FP:
		print_registers(frame);
		panic("VFP exception in the kernel");
	case EXCP_DATA_ABORT:
		data_abort(frame, esr, 0);
		break;
	case EXCP_BRK:
#ifdef KDTRACE_HOOKS
		if ((esr & ESR_ELx_ISS_MASK) == 0x40d && \
		    dtrace_invop_jump_addr != 0) {
			dtrace_invop_jump_addr(frame);
			break;
		}
#endif
		/* FALLTHROUGH */
	case EXCP_WATCHPT_EL1:
	case EXCP_SOFTSTP_EL1:
#ifdef KDB
		kdb_trap(exception, 0, frame);
#else
		panic("No debugger in kernel.\n");
#endif
		break;
	default:
		print_registers(frame);
		panic("Unknown kernel exception %x esr_el1 %lx\n", exception,
		    esr);
	}
}

/*
 * The attempted execution of an instruction bit pattern that has no allocated
 * instruction results in an exception with an unknown reason.
 */
static void
el0_excp_unknown(struct trapframe *frame)
{
	struct thread *td;
	uint64_t far;

	td = curthread;
	far = READ_SPECIALREG(far_el1);
	call_trapsignal(td, SIGILL, ILL_ILLTRP, (void *)far);
	userret(td, frame);
}

void
do_el0_sync(struct trapframe *frame)
{
	struct thread *td;
	uint32_t exception;
	uint64_t esr;

	/* Check we have a sane environment when entering from userland */
	KASSERT((uintptr_t)get_pcpu() >= VM_MIN_KERNEL_ADDRESS,
	    ("Invalid pcpu address from userland: %p (tpidr %lx)",
	     get_pcpu(), READ_SPECIALREG(tpidr_el1)));

	esr = READ_SPECIALREG(esr_el1);
	exception = ESR_ELx_EXCEPTION(esr);

	CTR4(KTR_TRAP,
	    "do_el0_sync: curthread: %p, esr %lx, elr: %lx, frame: %p",
	    curthread, esr, frame->tf_elr, frame);

	switch(exception) {
	case EXCP_FP_SIMD:
	case EXCP_TRAP_FP:
#ifdef VFP
		vfp_restore_state();
#else
		panic("VFP exception in userland");
#endif
		break;
	case EXCP_SVC:
		/*
		 * Ensure the svc_handler is being run with interrupts enabled.
		 * They will be automatically restored when returning from
		 * exception handler.
		 */
		intr_enable();
		svc_handler(frame);
		break;
	case EXCP_INSN_ABORT_L:
	case EXCP_DATA_ABORT_L:
	case EXCP_DATA_ABORT:
		data_abort(frame, esr, 1);
		break;
	case EXCP_UNKNOWN:
		el0_excp_unknown(frame);
		break;
	case EXCP_BRK:
		td = curthread;
		call_trapsignal(td, SIGTRAP, TRAP_BRKPT, (void *)frame->tf_elr);
		userret(td, frame);
		break;
	default:
		print_registers(frame);
		panic("Unknown userland exception %x esr_el1 %lx\n", exception,
		    esr);
	}
}

void
do_el0_error(struct trapframe *frame)
{

	panic("ARM64TODO: do_el0_error");
}

