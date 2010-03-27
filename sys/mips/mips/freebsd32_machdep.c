/*-
 * Copyright 2010 Juli Mallett.
 * Copyright 1996-1998 John D. Polstra.
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
 *	from: src/sys/i386/i386/elf_machdep.c,v 1.20 2004/08/11 02:35:05 marcel
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/jmallett/octeon/sys/mips/mips/elf_machdep.c 204031 2010-02-18 05:49:52Z neel $");

#define	__ELF_WORD_SIZE	32
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32_proto.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/cache.h>

/*
 * XXX
 * Add a mechanism to distinguish between n32 and o32.
 */
struct sysentvec elf32_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_mask	= 0,
	.sv_sigsize	= 0,
	.sv_sigtbl	= NULL,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_prepsyscall	= NULL,
	.sv_name	= "FreeBSD ELF32",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= FREEBSD32_USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings = exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_ILP32
};

static Elf32_Brandinfo freebsd32_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_MIPS,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.flags		= 0
};

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_FIRST,
    (sysinit_cfunc_t) elf32_insert_brand_entry,
    &freebsd32_brand_info);

void
elf32_dump_thread(struct thread *td __unused, void *dst __unused,
    size_t *off __unused)
{
}

int
fill_fpregs32(struct thread *td, struct fpreg32 *fpr32)
{
	struct fpreg fpr;
	unsigned i;
	int error;

	error = fill_fpregs(td, &fpr);
	if (error != 0)
		return (error);

	for (i = 0; i < NUMFPREGS; i++) {
		fpr32->r_regs[i] = fpr.r_regs[i];
	}

	return (0);
}

int
fill_regs32(struct thread *td, struct reg32 *r32)
{
	struct reg r;
	unsigned i;
	int error;

	error = fill_regs(td, &r);
	if (error != 0)
		return (error);

	for (i = 0; i < NUMSAVEREGS; i++) {
		r32->r_regs[i] = r.r_regs[i];
	}

	return (0);
}

int
set_fpregs32(struct thread *td, struct fpreg32 *fpr32)
{
	struct fpreg fpr;
	unsigned i;
	int error;

	for (i = 0; i < NUMFPREGS; i++) {
		fpr.r_regs[i] = fpr32->r_regs[i];
	}

	error = set_fpregs(td, &fpr);
	if (error != 0)
		return (error);
	return (0);
}

int
set_regs32(struct thread *td, struct reg32 *r32)
{
	struct reg r;
	unsigned i;
	int error;

	for (i = 0; i < NUMSAVEREGS; i++) {
		r.r_regs[i] = r32->r_regs[i];
	}

	error = set_regs(td, &r);
	if (error != 0)
		return (error);
	return (0);
}

int
fill_dbregs32(struct thread *td, struct dbreg32 *dbr32)
{
	return (ENOSYS);
}

int
set_dbregs32(struct thread *td, struct dbreg32 *dbr32)
{
	return (ENOSYS);
}

int
freebsd32_sigreturn(struct thread *td, struct freebsd32_sigreturn_args *uap)
{
	struct sigreturn_args sa;
	int error;

	sa.sigcntxp = (void *)(intptr_t)(int32_t)(intptr_t)uap->sigcntxp;

	error = sigreturn(td, &sa);
	if (error != 0)
		return (error);
	return (0);
}

int
freebsd32_getcontext(struct thread *td, struct freebsd32_getcontext_args *uap)
{
	struct getcontext_args gca;
	int error;

	gca.ucp = (void *)(intptr_t)(int32_t)(intptr_t)uap->ucp;

	error = getcontext(td, &gca);
	if (error != 0)
		return (error);
	return (0);
}

int
freebsd32_setcontext(struct thread *td, struct freebsd32_setcontext_args *uap)
{
	struct setcontext_args sca;
	int error;

	sca.ucp = (void *)(intptr_t)(int32_t)(intptr_t)uap->ucp;

	error = setcontext(td, &sca);
	if (error != 0)
		return (error);
	return (0);
}

int
freebsd32_swapcontext(struct thread *td, struct freebsd32_swapcontext_args *uap)
{
	struct swapcontext_args sca;
	int error;

	sca.ucp = (void *)(intptr_t)(int32_t)(intptr_t)uap->ucp;

	error = swapcontext(td, &sca);
	if (error != 0)
		return (error);
	return (0);
}

int
freebsd32_sysarch(struct thread *td, struct freebsd32_sysarch_args *uap)
{
	struct sysarch_args saa;
	int error;

	saa.op = uap->op;
	saa.parms = (void *)(intptr_t)(int32_t)(intptr_t)uap->parms;

	error = sysarch(td, &saa);
	if (error != 0)
		return (error);
	return (0);
}
