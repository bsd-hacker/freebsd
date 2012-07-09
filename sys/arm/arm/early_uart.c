/*-
 * Copyright (c) 2009 Guillaume Ballet
 * Copyright (c) 2012 Aleksander Dutkowski
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
 */

#include "opt_global.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/early_uart.h>
#include <machine/pcb.h>
#include <machine/machdep.h>
#include <machine/undefined.h>
#include <machine/pte.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <sys/kdb.h>
#include <arm/include/stdarg.h>

static volatile uint32_t *arm_early_uart = (volatile uint32_t *)ARM_EARLY_UART_VA;
static volatile uint32_t *arm_early_uart_lsr = (volatile uint32_t *)(ARM_EARLY_UART_VA + 0x14);

void
arm_early_putc(char c)
{

	while ((*arm_early_uart_lsr & 0x20) == 0);
	*arm_early_uart = c;

	if( c == '\n' )
	{
		while ((*arm_early_uart_lsr & 0x20) == 0);
		*arm_early_uart = '\r';
	}
}

void
arm_early_puts(unsigned char *str)
{
	do {
		arm_early_putc(*str);
	} while (*++str != '\0');
}

void
arm_early_uart_base(uint32_t base_address)
{
	arm_early_uart = (volatile uint32_t *)base_address;
	arm_early_uart_lsr = (volatile uint32_t *)(base_address + 0x14);
}

void
eprintf(const char *fmt,...)
{
	va_list ap;
	const char *hex = "0123456789abcdef";
	char buf[10];
	char *s;
	unsigned u;
	int c;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				arm_early_putc(va_arg(ap, int));
				continue;
			case 's':
				for (s = va_arg(ap, char *); *s; s++)
					arm_early_putc(*s);
				continue;
			case 'd':       /* A lie, always prints unsigned */
			case 'u':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = '0' + u % 10U;
				while (u /= 10U);
				dumpbuf:;
				while (--s >= buf)
					arm_early_putc(*s);
				continue;
			case 'x':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = hex[u & 0xfu];
				while (u >>= 4);
				goto dumpbuf;
			}
		}
		arm_early_putc(c);
	}
	va_end(ap);

	return;
}

void
dump_l2pagetable(uint32_t pta, uint32_t l1)
{
        int i;
        volatile uint32_t *pt = (volatile uint32_t*)pta;

        for (i=0; i<256;i++) {
                switch (pt[i] & 0x3) {
                        case 1:
                                eprintf("0x%x -> 0x%x 64K ",(i<<12) | l1,
                                        pt[i]&0xFFFF0000);
                                eprintf("l2pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("s=%u ",        (pt[i]>>10) &0x1);
                                eprintf("apx=%u ",      (pt[i]>> 9) &0x1);
                                eprintf("tex=%u ",      (pt[i]>>12) &0x7);
                                eprintf("ap=%u ",       (pt[i]>> 4) &0x3);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                        case 2:
                        case 3:
                                eprintf("0x%x -> 0x%x  4K ",(i<<12) | l1,
                                        pt[i]&0xFFFFF000);
                                eprintf("l2pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("s=%u ",        (pt[i]>>10) &0x1);
                                eprintf("apx=%u ",      (pt[i]>> 9) &0x1);
                                eprintf("tex=%u ",      (pt[i]>> 6) &0x7);
                                eprintf("ap=%u ",       (pt[i]>> 4) &0x3);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                }
        }
}

void
dump_l1pagetable(uint32_t pta)
{
        int i;
        eprintf("L1 pagetable starts at 0x%x\n",pta);
        volatile uint32_t *pt = (volatile uint32_t*)pta;
        for (i=0; i<4096;i++) {
                switch (pt[i] & 0x3) {
                        case 1:
                                eprintf("0x%x ->             L2 ",i<<20);
                                eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("l2desc=0x%x ",pt[i] & 0xFFFFFC00);
                                eprintf("p=%u ",(pt[i]>>9) &0x1);
                                eprintf("domain=0x%x\n",(pt[i]>>5) &0xF);
                                dump_l2pagetable(pt[i] & 0xFFFFFC00, i<<20);
                                break;
                        case 2:
                                if (pt[i] &0x40000) {
                                        eprintf("0x%x -> 0x%x 16M ",i<<20, pt[i] & 0xFF000000);
                                        eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                        eprintf("base=0x%x ", ((pt[i]>>24)));
                                } else {
                                        eprintf("0x%x -> 0x%x  1M ",i<<20, pt[i] & 0xFFF00000);
                                        eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                        eprintf("base=0x%x ", (pt[i]>>20));
                                }
                                eprintf("nG=%u ",       (pt[i]>>17) &0x1);
                                eprintf("s=%u ",        (pt[i]>>16) &0x1);
                                eprintf("apx=%u ",      (pt[i]>>15) &0x1);
                                eprintf("tex=%u ",      (pt[i]>>12) &0x7);
                                eprintf("ap=%u ",       (pt[i]>>10) &0x3);
                                eprintf("p=%u ",        (pt[i]>> 9) &0x1);
                                eprintf("domain=0x%x ", (pt[i]>> 5) &0xF);
                                eprintf("xn=%u ",       (pt[i]>> 4) &0x1);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                        case 3:
                                eprintf("pt[0x%x] 0x%x RESV\n",i, pt[i]);
                                break;
                }
        }
}

