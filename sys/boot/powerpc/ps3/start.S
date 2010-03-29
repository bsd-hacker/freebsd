/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * KBoot and simulators will start this program from the _start symbol, with
 * r3 pointing to a flattened device tree (kexec), r4 the physical address
 * at which we were loaded, and r5 0 (kexec) or a pointer to Open Firmware
 * (simulator). If r4 is non-zero, the first order of business is relocating
 * ourselves to 0. In the kboot case, the PPE secondary thread will enter
 * at 0x60.
 *
 * If started directly by the LV1 hypervisor, we are loaded to address 0
 * and execution on both threads begins at 0x100 (EXC_RST).
 */

#define CACHELINE_SIZE			128
#define SPR_CTRL			136
#define TMPSTKSZ			4096

/* KBoot thread 0 entry -- do relocation, then jump to main */
.global _start
_start:
	cmpwi	%r4,0
	bne	relocate_self
relocated_start:
	lis	%r1,(tmpstk+TMPSTKSZ-16)@ha
	addi	%r1,%r1,(tmpstk+TMPSTKSZ-16)@l
	bl	main

. = 0x20
.global secondary_spin_sem
secondary_spin_sem:
	.long	0

. = 0x60
thread1_start_kboot:
	ba	thread1_start	/* kboot copies the first 256 bytes to
				 * address 0, so we are safe to jump
				 * (and stay) there */

thread1_start:
	li	%r0,secondary_spin_sem@l
1:	lwz	%r1,0(%r0)	/* Spin on SECONDARY_SPIN_SEM_ADDRESS */
	cmpwi	%r1,0
	beq	1b
	li	%r0,0x100	/* Invalidate reset vector cache line */
	icbi	0,%r0
	ba	0x100		/* If non-zero, jump to the reset vector */

. = 0x100
exc_rst:
	mfspr	%r0,SPR_CTRL
	/* The first two bits of r0 are 01 (thread 1) or 10 (thread 0) */
	cntlzd	%r0,%r0		/* Now 0 for thread 0, 1 for thread 1 */

	cmpwi	%r0,0
	bne	thread1_start	/* Send thread 1 to wait */

	b	relocated_start /* Main entry point for thread 0 */

relocate_self:
	/* We enter this with r4 the physical offset for our relocation */
	lis	%r8,_end@ha	/* r8: copy length */
	addi	%r8,%r8,_end@l
	li	%r5,0		/* r5: dest address */
1:	add	%r6,%r4,%r5	/* r6: source address */
	ld	%r7,0(%r6)
	std	%r7,0(%r5)
	cmpw	%r5,%r8
	addi	%r5,%r5,8
	blt	1b

	/* Now invalidate the icache */
	li	%r5,0
2:	dcbst	0,%r5
	icbi	0,%r5
	cmpw	%r5,%r8
	addi	%r5,%r5,CACHELINE_SIZE
	blt	2b

	/* All done: absolute jump to relocated entry point */
	ba	relocated_start

.data
.align 4
tmpstk:
        .space  TMPSTKSZ
