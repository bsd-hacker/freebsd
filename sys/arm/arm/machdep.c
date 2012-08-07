/*	$NetBSD: arm32_machdep.c,v 1.44 2004/03/24 15:34:47 atatat Exp $	*/

/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Machine dependant functions for kernel setup
 *
 * Created      : 17/09/94
 * Updated	: 18/04/01 updated for new wscons
 */

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_timer.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/msgbuf.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/early_uart.h>
#include <machine/machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/undefined.h>
#include <machine/vmparam.h>
#include <machine/sysarch.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

#define	ARM_DEVMAP_START	0xf0000000

struct pcpu __pcpu[MAXCPU];
struct pcpu *pcpup = &__pcpu[0];

static struct trapframe proc0_tf;

uint32_t cpu_reset_address = 0;
int cold = 1;
vm_offset_t vector_page;

long realmem = 0;

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

extern vm_offset_t pmap_bootstrap_lastaddr;

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;

extern struct pmap_devmap arm_pmap_devmap[];
struct pv_addr arm_bootstrap_l2pt[128];
vm_offset_t arm_free_va, arm_free_pa, arm_allocated_va;
vm_offset_t arm_start_va, arm_start_pa;
vm_offset_t arm_devmap_size;
vm_offset_t pmap_bootstrap_lastaddr;

vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];

int (*_arm_memcpy)(void *, void *, int, int) = NULL;
int (*_arm_bzero)(void *, int, int) = NULL;
int _min_memcpy_size = 0;
int _min_bzero_size = 0;

extern int *end;
#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

static void arm_valloc_pages(struct pv_addr *, size_t, size_t);
static void arm_process_devmap(struct pmap_devmap *);
static void arm_bootstrap_pagetables(uint32_t, struct pv_addr *,
    struct pv_addr *, int);

void
sendsig(catcher, ksi, mask)
	sig_t catcher;
	ksiginfo_t *ksi;
	sigset_t *mask;
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp;
	int onstack;
	int sig;
	int code;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	onstack = sigonstack(tf->tf_usr_sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !(onstack) &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct sigframe *)(td->td_sigstk.ss_sp + 
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		fp = (struct sigframe *)td->td_frame->tf_usr_sp;
		 
	/* make room on the stack */
	fp--;
	
	/* make the stack aligned */
	fp = (struct sigframe *)STACKALIGN(fp);
	/* Populate the siginfo frame. */
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK ) 
	    ? ((onstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	frame.sf_uc.uc_stack = td->td_sigstk;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(td->td_proc);

	/* Copy the sigframe out to the user's stack. */
	if (copyout(&frame, fp, sizeof(*fp)) != 0) {
		/* Process has trashed its stack. Kill it. */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p fp=%p", td, fp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */
	
	tf->tf_r0 = sig;
	tf->tf_r1 = (register_t)&fp->sf_si;
	tf->tf_r2 = (register_t)&fp->sf_uc;

	/* the trampoline uses r5 as the uc address */
	tf->tf_r5 = (register_t)&fp->sf_uc;
	tf->tf_pc = (register_t)catcher;
	tf->tf_usr_sp = (register_t)fp;
	tf->tf_usr_lr = (register_t)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_usr_lr,
	    tf->tf_usr_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

struct kva_md_info kmi;

/*
 * arm32_vector_init:
 *
 *	Initialize the vector page, and select whether or not to
 *	relocate the vectors.
 *
 *	NOTE: We expect the vector page to be mapped at its expected
 *	destination.
 */

extern unsigned int page0[], page0_data[];
void
arm_vector_init(vm_offset_t va, int which)
{
	unsigned int *vectors = (int *) va;
	unsigned int *vectors_data = vectors + (page0_data - page0);
	int vec;

	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < ARM_NVEC; vec++) {
		if ((which & (1 << vec)) == 0) {
			/* Don't want to take over this vector. */
			continue;
		}
		vectors[vec] = page0[vec];
		vectors_data[vec] = page0_data[vec];
	}

	/* Now sync the vectors. */
	cpu_icache_sync_range(va, (ARM_NVEC * 2) * sizeof(u_int));

	vector_page = va;

	if (va == ARM_VECTORS_HIGH) {
		/*
		 * Assume the MD caller knows what it's doing here, and
		 * really does want the vector page relocated.
		 *
		 * Note: This has to be done here (and not just in
		 * cpu_setup()) because the vector page needs to be
		 * accessible *before* cpu_startup() is called.
		 * Think ddb(9) ...
		 *
		 * NOTE: If the CPU control register is not readable,
		 * this will totally fail!  We'll just assume that
		 * any system that has high vector support has a
		 * readable CPU control register, for now.  If we
		 * ever encounter one that does not, we'll have to
		 * rethink this.
		 */
		cpu_control(CPU_CONTROL_VECRELOC, CPU_CONTROL_VECRELOC);
	}
}

static void
cpu_startup(void *dummy)
{
	struct pcb *pcb = thread0.td_pcb;
#ifdef ARM_TP_ADDRESS
#ifndef ARM_CACHE_LOCK_ENABLE
	vm_page_t m;
#endif
#endif

	cpu_setup("");
	identify_arm_cpu();

	printf("real memory  = %ju (%ju MB)\n", (uintmax_t)ptoa(physmem),
	    (uintmax_t)ptoa(physmem) / 1048576);
	realmem = physmem;

	/*
	 * Display the RAM layout.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size;

			size = phys_avail[indx + 1] - phys_avail[indx];
			printf("%#08jx - %#08jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    (uintmax_t)ptoa(cnt.v_free_count),
	    (uintmax_t)ptoa(cnt.v_free_count) / 1048576);

	bufinit();
	vm_pager_bufferinit();
	pcb->un_32.pcb32_und_sp = (u_int)thread0.td_kstack +
	    USPACE_UNDEF_STACK_TOP;
	pcb->un_32.pcb32_sp = (u_int)thread0.td_kstack +
	    USPACE_SVC_STACK_TOP;
	vector_page_setprot(VM_PROT_READ);
	pmap_set_pcb_pagedir(pmap_kernel(), pcb);
	pmap_postinit();
#ifdef ARM_TP_ADDRESS
#ifdef ARM_CACHE_LOCK_ENABLE
	pmap_kenter_user(ARM_TP_ADDRESS, ARM_TP_ADDRESS);
	arm_lock_cache_line(ARM_TP_ADDRESS);
#else
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ | VM_ALLOC_ZERO);
	pmap_kenter_user(ARM_TP_ADDRESS, VM_PAGE_TO_PHYS(m));
#endif
	*(uint32_t *)ARM_RAS_START = 0;
	*(uint32_t *)ARM_RAS_END = 0xffffffff;
#endif
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	cpu_dcache_wb_range((uintptr_t)ptr, len);
	cpu_l2cache_wb_range((uintptr_t)ptr, len);
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	return (ENXIO);
}

void
cpu_idle(int busy)
{
	
#ifndef NO_EVENTTIMERS
	if (!busy) {
		critical_enter();
		cpu_idleclock();
	}
#endif
	cpu_sleep(0);
#ifndef NO_EVENTTIMERS
	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
#endif
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf = td->td_frame;
	bcopy(&tf->tf_r0, regs->r, sizeof(regs->r));
	regs->r_sp = tf->tf_usr_sp;
	regs->r_lr = tf->tf_usr_lr;
	regs->r_pc = tf->tf_pc;
	regs->r_cpsr = tf->tf_spsr;
	return (0);
}
int
fill_fpregs(struct thread *td, struct fpreg *regs)
{
	bzero(regs, sizeof(*regs));
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf = td->td_frame;
	
	bcopy(regs->r, &tf->tf_r0, sizeof(regs->r));
	tf->tf_usr_sp = regs->r_sp;
	tf->tf_usr_lr = regs->r_lr;
	tf->tf_pc = regs->r_pc;
	tf->tf_spsr &=  ~PSR_FLAGS;
	tf->tf_spsr |= regs->r_cpsr & PSR_FLAGS;
	return (0);								
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{
	return (0);
}
int
set_dbregs(struct thread *td, struct dbreg *regs)
{
	return (0);
}


static int
ptrace_read_int(struct thread *td, vm_offset_t addr, u_int32_t *v)
{
	struct iovec iov;
	struct uio uio;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
	iov.iov_base = (caddr_t) v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

static int
ptrace_write_int(struct thread *td, vm_offset_t addr, u_int32_t v)
{
	struct iovec iov;
	struct uio uio;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
	iov.iov_base = (caddr_t) &v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

int
ptrace_single_step(struct thread *td)
{
	struct proc *p;
	int error;
	
	KASSERT(td->td_md.md_ptrace_instr == 0,
	 ("Didn't clear single step"));
	p = td->td_proc;
	PROC_UNLOCK(p);
	error = ptrace_read_int(td, td->td_frame->tf_pc + 4, 
	    &td->td_md.md_ptrace_instr);
	if (error)
		goto out;
	error = ptrace_write_int(td, td->td_frame->tf_pc + 4,
	    PTRACE_BREAKPOINT);
	if (error)
		td->td_md.md_ptrace_instr = 0;
	td->td_md.md_ptrace_addr = td->td_frame->tf_pc + 4;
out:
	PROC_LOCK(p);
	return (error);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct proc *p;

	if (td->td_md.md_ptrace_instr) {
		p = td->td_proc;
		PROC_UNLOCK(p);
		ptrace_write_int(td, td->td_md.md_ptrace_addr,
		    td->td_md.md_ptrace_instr);
		PROC_LOCK(p);
		td->td_md.md_ptrace_instr = 0;
	}
	return (0);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	td->td_frame->tf_pc = addr;
	return (0);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t cspr;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		cspr = disable_interrupts(I32_bit | F32_bit);
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_cspr = cspr;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t cspr;

	td = curthread;
	critical_exit();
	cspr = td->td_md.md_saved_cspr;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		restore_interrupts(cspr);
}

/*
 * Clear registers on exec
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *tf = td->td_frame;

	memset(tf, 0, sizeof(*tf));
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = imgp->entry_addr;
	tf->tf_svc_lr = 0x77777777;
	tf->tf_pc = imgp->entry_addr;
	tf->tf_spsr = PSR_USR32_MODE;
}

/*
 * Get machine context.
 */
int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;
	__greg_t *gr = mcp->__gregs;

	if (clear_ret & GET_MC_CLEAR_RET)
		gr[_REG_R0] = 0;
	else
		gr[_REG_R0]   = tf->tf_r0;
	gr[_REG_R1]   = tf->tf_r1;
	gr[_REG_R2]   = tf->tf_r2;
	gr[_REG_R3]   = tf->tf_r3;
	gr[_REG_R4]   = tf->tf_r4;
	gr[_REG_R5]   = tf->tf_r5;
	gr[_REG_R6]   = tf->tf_r6;
	gr[_REG_R7]   = tf->tf_r7;
	gr[_REG_R8]   = tf->tf_r8;
	gr[_REG_R9]   = tf->tf_r9;
	gr[_REG_R10]  = tf->tf_r10;
	gr[_REG_R11]  = tf->tf_r11;
	gr[_REG_R12]  = tf->tf_r12;
	gr[_REG_SP]   = tf->tf_usr_sp;
	gr[_REG_LR]   = tf->tf_usr_lr;
	gr[_REG_PC]   = tf->tf_pc;
	gr[_REG_CPSR] = tf->tf_spsr;

	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct trapframe *tf = td->td_frame;
	const __greg_t *gr = mcp->__gregs;

	tf->tf_r0 = gr[_REG_R0];
	tf->tf_r1 = gr[_REG_R1];
	tf->tf_r2 = gr[_REG_R2];
	tf->tf_r3 = gr[_REG_R3];
	tf->tf_r4 = gr[_REG_R4];
	tf->tf_r5 = gr[_REG_R5];
	tf->tf_r6 = gr[_REG_R6];
	tf->tf_r7 = gr[_REG_R7];
	tf->tf_r8 = gr[_REG_R8];
	tf->tf_r9 = gr[_REG_R9];
	tf->tf_r10 = gr[_REG_R10];
	tf->tf_r11 = gr[_REG_R11];
	tf->tf_r12 = gr[_REG_R12];
	tf->tf_usr_sp = gr[_REG_SP];
	tf->tf_usr_lr = gr[_REG_LR];
	tf->tf_pc = gr[_REG_PC];
	tf->tf_spsr = gr[_REG_CPSR];

	return (0);
}

/*
 * MPSAFE
 */
int
sys_sigreturn(td, uap)
	struct thread *td;
	struct sigreturn_args /* {
		const struct __ucontext *sigcntxp;
	} */ *uap;
{
	struct sigframe sf;
	struct trapframe *tf;
	int spsr;
	
	if (uap == NULL)
		return (EFAULT);
	if (copyin(uap->sigcntxp, &sf, sizeof(sf)))
		return (EFAULT);
	/*
	 * Make sure the processor mode has not been tampered with and
	 * interrupts have not been disabled.
	 */
	spsr = sf.sf_uc.uc_mcontext.__gregs[_REG_CPSR];
	if ((spsr & PSR_MODE) != PSR_USR32_MODE ||
	    (spsr & (I32_bit | F32_bit)) != 0)
		return (EINVAL);
		/* Restore register context. */
	tf = td->td_frame;
	set_mcontext(td, &sf.sf_uc.uc_mcontext);

	/* Restore signal mask. */
	kern_sigprocmask(td, SIG_SETMASK, &sf.sf_uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}


/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{
	pcb->un_32.pcb32_r8 = tf->tf_r8;
	pcb->un_32.pcb32_r9 = tf->tf_r9;
	pcb->un_32.pcb32_r10 = tf->tf_r10;
	pcb->un_32.pcb32_r11 = tf->tf_r11;
	pcb->un_32.pcb32_r12 = tf->tf_r12;
	pcb->un_32.pcb32_pc = tf->tf_pc;
	pcb->un_32.pcb32_lr = tf->tf_usr_lr;
	pcb->un_32.pcb32_sp = tf->tf_usr_sp;
}

/*
 * Fake up a boot descriptor table
 */
vm_offset_t
fake_preload_metadata(void)
{
#ifdef DDB
	vm_offset_t zstart = 0, zend = 0;
#endif
	vm_offset_t lastaddr;
	int i = 0;
	static uint32_t fake_preload[35];

	fake_preload[i++] = MODINFO_NAME;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i++], "elf kernel");
	i += 2;
	fake_preload[i++] = MODINFO_TYPE;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i++], "elf kernel");
	i += 2;
	fake_preload[i++] = MODINFO_ADDR;
	fake_preload[i++] = sizeof(vm_offset_t);
	fake_preload[i++] = KERNVIRTADDR;
	fake_preload[i++] = MODINFO_SIZE;
	fake_preload[i++] = sizeof(uint32_t);
	fake_preload[i++] = (uint32_t)&end - KERNVIRTADDR;
#ifdef DDB
	if (*(uint32_t *)KERNVIRTADDR == MAGIC_TRAMP_NUMBER) {
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_SSYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 4);
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_ESYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 8);
		lastaddr = *(uint32_t *)(KERNVIRTADDR + 8);
		zend = lastaddr;
		zstart = *(uint32_t *)(KERNVIRTADDR + 4);
		ksym_start = zstart;
		ksym_end = zend;
	} else
#endif
		lastaddr = (vm_offset_t)&end;
	fake_preload[i++] = 0;
	fake_preload[i] = 0;
	preload_metadata = (void *)fake_preload;

	return (lastaddr);
}

/*
 * Initialize proc0
 */
void
init_proc0(vm_offset_t kstack)
{
	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kstack;
	thread0.td_pcb = (struct pcb *)
		(thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
}


void
pcpu0_init(void)
{
#if ARM_ARCH_7A || defined(CPU_MV_PJ4B)
	set_pcpu(pcpup);
#endif
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);
#ifdef ARM_VFP_SUPPORT
	PCPU_SET(cpu, 0);
#endif
}

static void
arm_valloc_pages(struct pv_addr *result, size_t npages, size_t boundary)
{
	npages *= PAGE_SIZE;
	boundary *= PAGE_SIZE;

	/* First, round up to specified boundary */
	arm_free_pa = roundup(arm_free_pa, boundary);
	arm_free_va = roundup(arm_free_va, boundary);

	result->pv_pa = arm_free_pa;
	arm_free_pa += npages;
	result->pv_va = arm_free_va;
	arm_free_va += npages;
	arm_allocated_va += npages;
	memset((void *)result->pv_va, 0, npages);

	edebugf("pa=0x%x va=0x%x size=0x%x\n", result->pv_pa, result->pv_va, npages);
}

static void
arm_bootstrap_pagetables(uint32_t memsize, struct pv_addr *vectors, struct pv_addr *l1pt, int high_vectors)
{
	struct pv_addr *l2pt = arm_bootstrap_l2pt;
	vm_offset_t vectors_va;
	vm_offset_t l2_start;
	vm_offset_t pagetables_size = 0;
	int l2_needed;
	int l2_devmap;
	int i, j;

	vectors_va = high_vectors ? ARM_VECTORS_HIGH : ARM_VECTORS_LOW;

	/* Allocate L1 pagetable */
	arm_valloc_pages(l1pt, L1_TABLE_SIZE / PAGE_SIZE, L1_TABLE_SIZE / PAGE_SIZE);
	pagetables_size += L1_TABLE_SIZE;

	/* 
	 * Calculate number of needed L2 pagetables: we are starting with 
	 * one needed to map vectors page
	 */
	l2_start = rounddown(arm_free_va, L1_S_SIZE);
	l2_needed = 1; /* vectors */
	l2_devmap = roundup(arm_devmap_size, L1_S_SIZE) >> L1_S_SHIFT;
	/* Add needed number of tables to hold vm_page array */
	l2_needed += roundup((memsize / PAGE_SIZE) * sizeof(struct vm_page), L1_S_SIZE) >> L1_S_SHIFT;
	/* And then to map kernel text and data and associated structures */
	l2_needed += roundup(arm_free_va - l2_start, L1_S_SIZE) >> L1_S_SHIFT;
	/* ...and to map devmap table */
	l2_needed += l2_devmap; 
	/* 
	 * Finally, round up to 4 to not waste space, as we can fit 4
	 * pagetables on one page
	 */
	l2_needed = roundup(l2_needed, 4);

	edebugf("L2 needed=%d devmap=%d\n", l2_needed, l2_devmap);

	/* Allocate L2 page tables */
	arm_valloc_pages(&l2pt[0], (l2_needed * L2_TABLE_SIZE_REAL) / PAGE_SIZE, 1);
	pagetables_size += (l2_needed * L2_TABLE_SIZE_REAL);
	
	for (i = 1; i < l2_needed; i++) {
		/* Fill in L2 page table addresses */
		l2pt[i].pv_pa = l2pt[0].pv_pa + (i * L2_TABLE_SIZE_REAL);
		l2pt[i].pv_va = l2pt[0].pv_va + (i * L2_TABLE_SIZE_REAL);
	}

	for (i = 0; i < l2_needed - l2_devmap - 1; i++) {
		pmap_link_l2pt(l1pt->pv_va, l2_start + (i * L1_S_SIZE), &l2pt[i]);
		edebugf("link L2 page table %d at 0x%x\n", i, l2_start + (i * L1_S_SIZE));
	}

	/* Tell pmap about currently maximum mapped VA address */
	pmap_curmaxkvaddr = roundup(arm_free_va, L1_S_SIZE);

	/* Link devmap tables */
	for (j = 0; j < l2_devmap; j++) {
		pmap_link_l2pt(l1pt->pv_va, ARM_DEVMAP_START + (j * L1_S_SIZE), &l2pt[i + j]);
		edebugf("link L2 page table %d at 0x%x\n", i + j, ARM_DEVMAP_START + (j * L1_S_SIZE));
	}

	/* Link and map vectors page */
	pmap_link_l2pt(l1pt->pv_va, vectors_va, &l2pt[l2_needed - 1]);
	pmap_map_entry(l1pt->pv_va, vectors_va, vectors->pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE, PTE_CACHE);
	
	/* Map kernel and structures */
	pmap_map_chunk(l1pt->pv_va, arm_start_va, arm_start_pa,
	    arm_allocated_va - pagetables_size,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	
	/* Map pagetables itself: L1 */
	pmap_map_chunk(l1pt->pv_va, l1pt->pv_va, l1pt->pv_pa, L1_TABLE_SIZE,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	
	/* and L2 */
	pmap_map_chunk(l1pt->pv_va, l2pt[0].pv_va, l2pt[0].pv_pa, 
	    L2_TABLE_SIZE_REAL * l2_needed,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
}

static void
arm_process_devmap(struct pmap_devmap *devmap)
{
	struct fdt_range ranges[8];
	struct fdt_range *rptr = ranges;
	struct pmap_devmap *entry;
	phandle_t node, parent;
	vm_offset_t totalsize = 0;
	u_long start, size;
	int addr_cells, size_cells, par_addr_cells;
	int nranges, i;

	edebugf("processing devmap entries\n");

	for (i = 0; devmap[i].pd_name != NULL || devmap[i].pd_pa != 0; i++) {
		entry = &devmap[i];
		if (entry->pd_name != NULL) {

			edebugf("fdt %s: ", entry->pd_name);

			node = !strcmp(entry->pd_name, "console-uart")
			    ? fdt_lookup_console_uart()
			    : OF_finddevice(entry->pd_name);
	
			eprintf("node=0x%x ", node);

			if (node == -1)
				goto notfound;
			
			if ((parent = OF_parent(node)) <= 0)
				goto notfound;
			
			if (fdt_addrsize_cells(parent, &addr_cells, &size_cells))
				goto notfound;

			fdt_regsize(node, &start, &size);
			
			if ((par_addr_cells = fdt_parent_addr_cells(parent)) > 2)
				goto noparent;
			
			nranges = fdt_read_ranges(parent, &rptr, addr_cells, 
			    par_addr_cells, size_cells);
			
			if (nranges > 0)
				start += fdt_ranges_lookup(ranges, nranges, start, size);
noparent:
			entry->pd_pa = rounddown(start, PAGE_SIZE);
			entry->pd_size = roundup(size, PAGE_SIZE);

		} else
			edebugf("entry: ");

notfound:
		entry->pd_va = ARM_DEVMAP_START + totalsize;
		totalsize += entry->pd_size;
		eprintf("pa=0x%x va=0x%x size=0x%x\n", entry->pd_pa, entry->pd_va, entry->pd_size);
	}

	edebugf("total mapped size: 0x%x\n", totalsize);
	arm_devmap_size = totalsize;
}


void *
arm_mmu_init(uint32_t memsize, uint32_t lastaddr, int high_vectors)
{
	struct pv_addr pagetable;
	struct pv_addr dpcpu;

	arm_start_va = KERNVIRTADDR;
	arm_start_pa = KERNPHYSADDR;
	arm_free_va = roundup(lastaddr, PAGE_SIZE);
	arm_free_pa = arm_free_va + (KERNPHYSADDR - KERNVIRTADDR);
	arm_allocated_va = arm_free_va - arm_start_va;
	pmap_bootstrap_lastaddr = ARM_DEVMAP_START - ARM_NOCACHE_KVA_SIZE;
	
	edebugf("arm_free_va=0x%x arm_free_pa=0x%x\n", arm_start_va, arm_free_va);
	edebugf("using %s vectors address\n", high_vectors ? "high" : "low");

	/*
	 * Allocate a page for the system page mapped to 0x00000000
	 * or 0xffff0000. This page will just contain the system vectors
	 * and can be shared by all processes.
	 */
	arm_valloc_pages(&systempage, 1, 1);

	/* Allocate dynamic per-cpu area. */
	arm_valloc_pages(&dpcpu, DPCPU_SIZE / PAGE_SIZE, 1);
	dpcpu_init((void *)dpcpu.pv_va, 0);

	/* Allocate stacks for all modes */
	arm_valloc_pages(&irqstack, IRQ_STACK_SIZE * MAXCPU, 1);
	arm_valloc_pages(&abtstack, ABT_STACK_SIZE * MAXCPU, 1);
	arm_valloc_pages(&undstack, UND_STACK_SIZE * MAXCPU, 1);
	arm_valloc_pages(&kernelstack, KSTACK_PAGES * MAXCPU, 1);

	init_param1();

	/* Allocate space for message buffer */
	arm_valloc_pages(&msgbufpv, round_page(msgbufsize) / PAGE_SIZE, 1);
	
	/* Calculate devmap size */
	arm_process_devmap(arm_pmap_devmap);

	/* Construct bootstrap pagetables */
	arm_bootstrap_pagetables(memsize, &systempage, &pagetable, high_vectors);
	pmap_devmap_bootstrap(pagetable.pv_va, arm_pmap_devmap);
	
	edebugf("L1 table pa=0x%x va=0x%x\n", pagetable.pv_pa, pagetable.pv_va);

	/* Launch our bootstrap pagetable */
	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) |
	    DOMAIN_CLIENT);
	setttb(pagetable.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2));

#ifdef ARM_EARLY_DEBUG
	arm_early_uart_base(pmap_devmap_find_name("console-uart")->pd_va);
#endif

	edebugf("bootstrap pagetable launched\n");

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);
	set_stackptrs(0);

	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross relocations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();

	/* Set stack for exception handlers */
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;
	undefined_init();

	init_proc0(kernelstack.pv_va);
	arm_vector_init(high_vectors ? ARM_VECTORS_HIGH : ARM_VECTORS_LOW,
	    ARM_VEC_ALL);

	dump_avail[0] = 0;
	dump_avail[1] = memsize;
	dump_avail[2] = 0;
	dump_avail[3] = 0;

	pmap_bootstrap(arm_free_va, pmap_bootstrap_lastaddr, &pagetable);
	msgbufp = (void *)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);

	edebugf("MMU initialized\n");

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}

void
set_stackptrs(int cpu)
{

	set_stackptr(PSR_IRQ32_MODE,
	    irqstack.pv_va + ((IRQ_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_ABT32_MODE,
	    abtstack.pv_va + ((ABT_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_UND32_MODE,
	    undstack.pv_va + ((UND_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
}

void
physmap_init(struct mem_region *availmem_regions, int availmem_regions_sz)
{
	int i, j, cnt;
	vm_offset_t phys_kernelend, kernload;
	uint32_t s, e, sz;
	struct mem_region *mp, *mp1;

	phys_kernelend = KERNPHYSADDR + (virtual_avail - KERNVIRTADDR);
	kernload = KERNPHYSADDR;

	/*
	 * Remove kernel physical address range from avail
	 * regions list. Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	sz = 0;
	cnt = availmem_regions_sz;
	eprintf("processing avail regions:\n");
	for (mp = availmem_regions; mp->mr_size; mp++) {
		s = mp->mr_start;
		e = mp->mr_start + mp->mr_size;
		eprintf(" 0x%x-0x%x -> ", s, e);
		/* Check whether this region holds all of the kernel. */
		if (s < kernload && e > phys_kernelend) {
			availmem_regions[cnt].mr_start = phys_kernelend;
			availmem_regions[cnt++].mr_size = e - phys_kernelend;
			e = kernload;
		}
		/* Look whether this regions starts within the kernel. */
		if (s >= kernload && s < phys_kernelend) {
			if (e <= phys_kernelend)
				goto empty;
			s = phys_kernelend;
		}
		/* Now look whether this region ends within the kernel. */
		if (e > kernload && e <= phys_kernelend) {
			if (s >= kernload) {
				goto empty;
			}
			e = kernload;
		}
		/* Now page align the start and size of the region. */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s)
			e = s;
		sz = e - s;
		eprintf("0x%x-0x%x = 0x%x\n", s, e, sz);

		/* Check whether some memory is left here. */
		if (sz == 0) {
		empty:
			eprintf("skipping\n");
			bcopy(mp + 1, mp,
			    (cnt - (mp - availmem_regions)) * sizeof(*mp));
			cnt--;
			mp--;
			continue;
		}

		/* Do an insertion sort. */
		for (mp1 = availmem_regions; mp1 < mp; mp1++)
			if (s < mp1->mr_start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->mr_start = s;
			mp1->mr_size = sz;
		} else {
			mp->mr_start = s;
			mp->mr_size = sz;
		}
	}
	availmem_regions_sz = cnt;

	/* Fill in phys_avail table, based on availmem_regions */
	eprintf("fill in phys_avail:\n");
	for (i = 0, j = 0; i < availmem_regions_sz; i++, j += 2) {

		eprintf(" region: 0x%x - 0x%x (0x%x)\n",
		    availmem_regions[i].mr_start,
		    availmem_regions[i].mr_start + availmem_regions[i].mr_size,
		    availmem_regions[i].mr_size);

		phys_avail[j] = availmem_regions[i].mr_start;
		phys_avail[j + 1] = availmem_regions[i].mr_start +
		    availmem_regions[i].mr_size;
	}
	phys_avail[j] = 0;
	phys_avail[j + 1] = 0;
}

