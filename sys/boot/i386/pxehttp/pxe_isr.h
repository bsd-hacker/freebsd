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
		 
#ifndef PXE_ISR_H_INCLUDED
#define PXE_ISR_H_INCLUDED

/*
 *  Declarations for functions, defined in pxe_isr.s
 */
 
#include <stdint.h>

/* 1 - if interrupt occured, 0 - means nothing to do */
extern uint16_t __pxe_isr_occured;
/* IRQ number of NIC */
extern uint16_t __pxe_nic_irq;	
/* PXE! API entry offset, used in __pxe_call() */
extern uint16_t __pxe_entry_off;
/* PXE! API entry segment, used in __pxe_call() */
extern uint16_t __pxe_entry_seg;
/* PXE! API entry offset, used in __pxe_isr() */
extern uint16_t __pxe_entry_off2;
/* PXE! API entry segment, used in __pxe_isr()  */
extern uint16_t __pxe_entry_seg2;
/* chained ISR segment:offset */
extern uint16_t __chained_irq_seg;
extern uint16_t __chained_irq_off;

extern void __pxe_call(void);	/* PXE API call */
extern void __pxe_isr(void);	/* PXE API call */

extern void	__mask_irq(void);	/* masks irq */
extern void	__pxe_isr_install(void);/* installs handler for interrupt */
extern void	__isr_remove(void);	/* remove handler, ! not working ! now */
extern void	__mem_copy(void);	/* copies memory in vm86 mode */

#endif
