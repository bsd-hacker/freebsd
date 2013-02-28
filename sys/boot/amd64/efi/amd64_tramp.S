#include <machine/asmacros.h>

	.text
	.globl	amd64_tramp

/*
 * void amd64_tramp(uint64_t stack, void *copy_finish, uint64_t kernend,
 *		    uint64_t modulep, uint64_t pagetable, uint64_t entry)
 */
amd64_tramp:
	cli			/* Make sure we don't get interrupted. */
	movq	%rdi,%rsp	/* Switch to our temporary stack. */

	movq	%rdx,%r12	/* Stash the kernel values for later. */
	movq	%rcx,%r13
	movq	%r8,%r14
	movq	%r9,%r15

	callq	*%rsi		/* Call copy_finish so we're all ready to go. */

	pushq	%r12		/* Push kernend. */
	salq	$32,%r13	/* Shift modulep and push it. */
	pushq	%r13
	pushq	%r15		/* Push the entry address. */
	movq	%r14,%cr3	/* Switch page tables. */
	ret			/* "Return" to kernel entry. */

	ALIGN_TEXT
amd64_tramp_end:

	.data
	.globl	amd64_tramp_size
amd64_tramp_size:
	.long	amd64_tramp_end-amd64_tramp
