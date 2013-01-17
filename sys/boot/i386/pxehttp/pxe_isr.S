/*-
 * Copyright (c) 2007 Alexey Tarasov
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
 
/*  ISR, ISR installer, PXE API calling, etc.
 *  references: Intel PXE 2.1 specification, PXE SDK
 *              pxe.c
 */ 
		.globl  __pxe_isr_occured
		.globl	__pxe_nic_irq
		.globl	__pxe_entry_off, __pxe_entry_seg
		.globl	__pxe_entry_off2, __pxe_entry_seg2		
		.globl  __pxe_call
		.globl  __pxe_isr_install
		.globl  __pxe_isr_remove
		.globl	__pxe_isr
		.globl	__chained_irq_off
		.globl	__chained_irq_seg
		.globl	__mask_irq
		.globl	__mem_copy
	
		.code16
		.p2align 4,0x90
						# s_PXENV_UNDI_ISR start, look pxe.h
		.set UISR_STATUS,0x00           # s_PXENV_UNDI_ISR Status
		.set UISR_FUNC_FLAG,0x02	# s_PXENV_UNDI_ISR FuncFlag
		.set UISR_BUFFER_LEN,0x04	# s_PXENV_UNDI_ISR BufferLength
		.set UISR_FRAME_LEN,0x06	# s_PXENV_UNDI_ISR FrameLength
		.set UISR_HEADER_LEN,0x08	# s_PXENV_UNDI_ISR FrameHeaderLength
		.set UISR_FRAME_OFFSET,0x10	# s_PXENV_UNDI_ISR Frame
		.set UISR_FRAME_SEG,0x12	# s_PXENV_UNDI_ISR Frame
		.set UISR_PROTO_TYPE,0x14	# s_PXENV_UNDI_ISR ProtType
		.set UISR_PACKET_TYPE,0x15	# s_PXENV_UNDI_ISR PktType
						# s_PXENV_UNIDI_ISR end
		.set PXENV_UNDI_ISR_IN_START,0x1

		.set PXENV_UNDI_ISR_OUT_OURS,0x0
		.set PXENV_UNDI_ISR_OUT_NOT_OUTS,0x1

		.set PXENV_UNDI_ISR,0x0014	# PXE_UNDI_ISR function

		/* interrupt registers */
		.set I8259_EOI,0x20    		 # EOI
		.set I8259_EOI_REG,0x20		 # Interrupt Control Register
		.set I8259_AT_INTR_CTRL_REG,0xA0 # AT Interrupt Control Register
		.set I8259_PC_INTR_MASK_REG,0x21 # PC Interrupt Mask Register
		.set I8259_AT_INTR_MASK_REG,0xA1 # AT Interrupt Mask Register

		/* offsets from data_start to needed variables*/
		.set CHAINED_OFF, 0x2		# to __chained_irq_off
		.set ISR_OFF, 0x8		# to s_pxenv_undi_isr
		.set NIC_IRQ_OFF, 0x6		# to __pxe_nic_irq
		.set USER_MEM, 0xa000		# start of user memory
#
data_start:
__pxe_isr_occured:	.word   0x0000		# flagged if isr occured
__chained_irq_off:	.word	0x0000		# original ISR handler offset
__chained_irq_seg:	.word	0x0000		# original ISR handler segment
__pxe_nic_irq:		.word   0x0000		# NIC irq number	
s_pxenv_undi_isr:	.word	0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0	

/*
 * Adopted from libi386/pxetramp.S
 * pxe_call
 * in:
 *	DX - segment of data structure, used by PXE
 *	AX - offset
 *	BX - PXE function number
 * out:
 *	AX - 0, if call was successfull
 *	   - not 0, in other case
 */
__pxe_call:	push %dx			# seg
		push %ax			# off
		push %bx			# uint16_t func no
		.byte   0x9a			# far call
__pxe_entry_off:
		.word   0x0000			# PXE API entry point offset
__pxe_entry_seg:
		.word   0x0000			# PXE API entry point segment
		add $6, %sp			# restore stack
		.byte	0xcb

/*
 * __pxe_isr() - handles interrupt, checks is interrupt ours
 *		and updates __pxe_isr_occured if needed
 * in:
 *	none
 * out:
 *	none
 */
__pxe_isr:	cli				# no interrupts
		pushw	%ds			# saving all affected registers
		pushl	%eax			# 
		pushl	%edi			#

        	movl	$data_start, %edi	# data_start is starting address
		addl	$USER_MEM, %edi		# of all our data
		movl	%edi, %eax
		shrl	$4, %eax		# calc segment
		movw	%ax, %ds		#  our data segment
		andl	$0xf, %edi		# calc offset
						# %ds:%di - address of start_data
		
		pushw	%di			# saving initial offset

/*
 * initing s_pxenv_undi_isr struct
 */		
		addw	$ISR_OFF, %di		# setting to s_pxenv_undi_isr
		movw	$0x0, %ds:UISR_STATUS(%di)	# status member = 0
						# starting interrupt verification
							
		movw	$PXENV_UNDI_ISR_IN_START, %ds:UISR_FUNC_FLAG(%di)
		
		pushw	%ds			# seg:off to s_pxenv_undi_isr
		pushw	%di
		pushw	$PXENV_UNDI_ISR		# starting handling (_IN_START)

		.byte   0x9a			# far call to PXE entry
__pxe_entry_off2:
		.word   0x0000			# PXE API entry point offset
__pxe_entry_seg2:
		.word   0x0000			# PXE API entry point segment

		add	$6, %sp			# restore stack		

		cmpw	$0, %ax			# is ok?
		jne	pxe_isr.not_our		#  nope
						# getting result func flag
		movw	%ds:UISR_FUNC_FLAG(%di), %ax	
		popw	%di			# restore offset to data_start
/*
 * if interrupt not ours - just return 
 */		
		cmpw	$PXENV_UNDI_ISR_OUT_OURS, %ax	# is interrupt ours?
		jne	pxe_isr.not_our			#  no

#		addw	$0x0, %di		# offset of __pxe_isr_occured
		movw 	$0x1, %ds:(%di)		# flagging it
/*
 * sending EOI to appropriate NIC.
 */
		pushw	%dx
		movw	%ds:NIC_IRQ_OFF(%di), %ax	# getting irq number
 		movb	%al, %dl			# moving it to dl
 		movb	$I8259_EOI, %al			# set EOI control word 
 
 		cmpb	$0x8, %dl			# is IRQ8-IRQ15?
 		jb	pxe_isr.master			#  no, use pic1
 
 		movw	$I8259_AT_INTR_CTRL_REG, %dx	# send EOI to PIC2
 		outb	%al, %dx			#
 
pxe_isr.master: movw	$I8259_EOI_REG, %dx		# send EOI to master
 		outb	%al, %dx			#
 	
		popw	%dx

pxe_isr.exit:	popl	%edi				# restore affected registers
		popl	%eax				#
		popw	%ds				#

		sti
		iret

pxe_isr.not_our:
/*
 * checking, if we have chained handler
 */
		je	pxe_isr.2
		/* ignore chained handler */
		popw	%di			# restore offset to data_start
		addw	$CHAINED_OFF, %di	# set di for __chained_irq_off
		
		movw	%ds:(%di), %ax		# get it value
		cmpw	$0, %ax			# have chained interrupt handler?
		je	pxe_isr.2		#  no, we think - dont

		sti				# enable interrupts
		pushf				# push flags

/*
 *  here is calling of chained interrupt handler.
 */
		pushw	%es			# store es register
		pushw	%bx
		
		movw	%ax, %bx
 		movw	%ds:2(%di), %es		# getting segment
 		call	*%es:(%bx)		# call chained handler
		
		popw	%bx			# restore registers
 		popw	%es			#
 
		popf				# restore flags word


/* from Intel PXE SDK:
 * If say "not my interrupt" by passing control to next in handler list, may
 * end up invoking the BIOS, which will turn off the interrupt at the PIC.
 * In case this happens, on return from next-handler call, see if must restore.
 * This only executes when an interrupt is fielded which is not ours to handle.
 */

/*
 *  NOTE: need to understand, if it is needed
 */

/*		pushw	%dx
		
 		movw    $I8259_PC_INTR_MASK_REG, %dx	# use master
 		movw    __pxe_nic_irq, %bx
		cmpb    $0x8, %bl                       # is IRQ8-IRQ15?
 		jb      pxe_isr.1                       #  no
 		movw    $I8259_AT_INTR_MASK_REG, %dx	# use PIC2
 	
 pxe_isr.1:	movb    save_int_mask, %bl		# get saved mask
 		inb     %dx, %al
 		notb    %bl
 		testb   %bl, %al
 		jz      pxe_isr.2
 
 		notb    %bl
 		andb 	%bl, %al
 		outb    %al, %dx
		
*/
pxe_isr.2:
		jmp	pxe_isr.exit

/* _mem_copy() - copies memory block
 * in:
 *	EAX - seg:off to source
 *	EBX - seg:off to destination
 *	ECX - byte count to copy
 * out:
 *	none
 */
 __mem_copy:	pushw	%es			# store affected registers
		pushw	%di			#
		pushw	%ds			#
		pushw	%si			#
		pushf				# store affected by cld flags
 
		cld				# set index increment flag

		movw	%ax, %si		# setting source seg:off
		shrl	$0x10, %eax		#
		movw	%ax, %ds		#

		movw	%bx, %di		# setting destination  seg:off
		shrl	$0x10, %ebx		#
		movw	%bx, %es		#

		rep	movsb			# copy byte by byte
				
		popf				# restore affected registers
		popw	%si			#
		popw	%ds			#
		popw	%di			#
		popw	%es			#
		
		.byte	0xcb

/*
 * __pxe_isr_install() - installs interrupt handler for provided IRQ,
 *		saves previous ("chained") handler
 * in:
 * 	AX - interrupt number (not IRQ)
 * 	BX - ISR segment
 * 	DX - ISR offset
 * out:
 *	BX - chained ISR segment
 *      DX - chained ISR offset
 */
__pxe_isr_install:	
		cli				# no interrupts
		pushl   %edi			# saving affected registers
		pushw	%ds			#
						# calc interrupt vector address
		shlw    $2, %ax			# multiply by 4 
		xorw    %di, %di		# clear di
		movw    %di, %ds		# setting 0 to data segment
		movw	%ax, %di                # di offset to interrupt vector
	    
		xchgw	%dx, %ds:(%di)		# installing ISR offset
		xchgw	%bx, %ds:2(%di)		# installing ISR segment

/*
 *	Masking is not our task
 *
 * 		call	mask_int		# masking
 */

    		popw	%ds			# restore affected registers
		popl	%edi			#
		sti				# start interrupts
		.byte	0xcb			# back to vm86


/* __pxe_isr_remove() - uninstalls ISR
 * Adopted from Intel PXE SDK
 * removes interrupt handler for provided IRQ, restores previous handler
 * in:
 * 	AX - interrupt number (not IRQ)
 *	BX - chained ISE offset
 * out:
 *	none
 */
__pxe_isr_remove:	pushf
		pushl	%ebx
		
		shlw    $2, %bx			# calculating offset of
						#  interrupt vector
		xorw    %cx, %cx		# clearing segment register es
		movw    %cx, %es

		movw	%es:(%bx), %ax		# get offset to current handler

pxe_isr_remove.3:	#movl   chained_irq_off, %eax	# getting old chained handler
		#movl   %eax, %es:(%bx)		# restore it

/*
 *  Forget about unmasking for now
 *
 *		call    unmask_int		# unmasking
 **/

pxe_isr_remove.fin:
#		movl    $0x0, chained_irq_off
		popl	%ebx
		popf
		clc
		ret
		
		
/*******************************************************************************/

/* __mask_irq() - masks hardware interrupt
 * Adopted from Intel PXE SDK
 *  
 * in:
 *   BX - IRQ number
 *   CX - original mask
 */
__mask_irq:	pushw	%dx				# saving registers
		pushw	%cx
		pushw	%ax				#  in fact, not useful

		cmpw	$0x7, %bx			# see if this master PIC
		jbe	mask_irq.master			#  yes, it is PIC1

		movw	$I8259_AT_INTR_MASK_REG, %dx	# setting PIC2 register
		jmp	mask_irq.1

mask_irq.master:
		movw	I8259_PC_INTR_MASK_REG, %dx	# setting master PIC

mask_irq.1:	movw	%bx, %cx
		andw	$0x07, %cx			# mask 0..2 bits
		movb	$0x1, %bl			# setting cl-th bit
		shlb	%cl, %bl			# 
		notb	%bl				# 
		
		pushf					# saving FLAGS register
		cli					# stop interrupts
		
		inb	%dx, %al
		andb	%bl, %al

		nop
		nop
			
		outb	%al, %dx
		popw	%ax				# popping FLAGS

		testw	$0x200, %ax			# check interrupt flag
		jz	mask_irq.2			#
		
		sti                                     # starting interrupts
mask_irq.2:
		popw	%ax
		popw	%cx
		popw	%dx
		
		.byte	0xcb				# return

/*
 * Adopted from Intel PXE SDK
 *  unmasks hardware interrupt
 */
#unmask_int:	pushw	%ax				# save all affected
#		pushw	%cx				#  registers
#		pushw	%dx				#
#
#		movw	__pxe_nic_irq, %bx		# getting irq number		
#		cmpw	$0x07, %bx			# is it PIC2?
#		jbe	unmask_int.master		#  no, it is master
#
#		subw	$0x08, %bx
#		mov	$I8259_AT_INTR_MASK_REG, %dx	# slave PIC register
#		jmp	unmask_int.2
#
#unmask_int.master:
#		mov	$I8259_PC_INTR_MASK_REG, %dx	# master PIC register
#
#unmask_int.2:	movw	%bx, %cx
#		andw	$0x07, %cx			# masking first 3 bits
#		movb	$0x01, %bl			# 
#		shlb	%cl, %bl			# shifting 1 bit to left
#		notb	%bl
#
#		movb	%bl, save_int_mask		# saving mask
#
#		pushf                                   # saving FLAGS register
#		cli
#
#		inb	%dx, %al			# sending control word
#		movb	%al, original_mask              # saving mask
#		andb	%bl, %al			# and it with saved
#		outb	%al, %dx			# out result
#
#		popw	%ax				# popping flags
#		testw   $0x200, %ax			# IF==1 ?
#		jz      unmask_int.3			#  yes
#		sti					# set IF=1
#
#unmask_int.3:	popw	%dx                             # restore registers
#		popw	%cx				# 
#		popw	%ax                             #
#		ret
