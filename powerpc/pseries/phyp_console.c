/*-
 * Copyright (C) 2011 by Nathan Whitehorn. All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/pseries/powerpc/phyp/phyp_console.c 214348 2010-10-25 15:41:12Z nwhitehorn $");

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <dev/ofw/openfirm.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_bus.h>

#include "phyp-hvcall.h"
#include "uart_if.h"

static union {
	uint64_t u64[2];
	char str[16];
} phyp_inbuf;
static uint64_t			phyp_inbuflen = 0;

enum {
	HVTERM1, HVTERMPROT
};

/*
 * Low-level UART interface
 */
static int phyp_uart_probe(struct uart_bas *bas);
static void phyp_uart_init(struct uart_bas *bas, int baudrate, int databits,
    int stopbits, int parity);
static void phyp_uart_term(struct uart_bas *bas);
static void phyp_uart_putc(struct uart_bas *bas, int c);
static int phyp_uart_rxready(struct uart_bas *bas);
static int phyp_uart_getc(struct uart_bas *bas, struct mtx *hwmtx);

static struct uart_ops phyp_uart_ops = {
	.probe = phyp_uart_probe,
	.init = phyp_uart_init,
	.term = phyp_uart_term,
	.putc = phyp_uart_putc,
	.rxready = phyp_uart_rxready,
	.getc = phyp_uart_getc,
};

struct uart_class uart_phyp_class = {
	"uart",
	NULL,
	sizeof(struct uart_softc),
	.uc_ops = &phyp_uart_ops,
	.uc_range = 1,
	.uc_rclk = 0x5bbc
};

static int
phyp_uart_probe(struct uart_bas *bas)
{
	phandle_t node = bas->chan;
	char buf[64];

	if (OF_getprop(node, "name", buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if (strcmp(buf, "vty") != 0)
		return (ENXIO);

	if (OF_getprop(node, "device_type", buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if (strcmp(buf, "serial") != 0)
		return (ENXIO);

	if (OF_getprop(node, "compatible", buf, sizeof(buf)) <= 0)
		return (ENXIO);
	if (strcmp(buf, "hvterm1") == 0) {
		bas->regshft = HVTERM1;
		return (0);
	} else if (strcmp(buf, "hvterm-protocol") == 0) {
		bas->regshft = HVTERMPROT;
		return (0);
	}
		
	return (ENXIO);
}

static void
phyp_uart_init(struct uart_bas *bas, int baudrate __unused,
    int databits __unused, int stopbits __unused, int parity __unused)
{
}

static void
phyp_uart_term(struct uart_bas *bas __unused)
{
}

static int
phyp_uart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	int ch, err;

	uart_lock(hwmtx);
	if (phyp_inbuflen == 0) {
		err = phyp_pft_hcall(H_GET_TERM_CHAR, (uint64_t)bas->bsh,
		    0, 0, 0, &phyp_inbuflen, &phyp_inbuf.u64[0],
		    &phyp_inbuf.u64[1]);
		if (err != H_SUCCESS) {
			uart_unlock(hwmtx);
			return (-1);
		}
	}

	if (phyp_inbuflen == 0) {
		uart_unlock(hwmtx);
		return (-1);
	}

	ch = phyp_inbuf.str[0];
	phyp_inbuflen--;
	if (phyp_inbuflen > 0)
		memcpy(&phyp_inbuf.str[0], &phyp_inbuf.str[1], phyp_inbuflen);

	uart_unlock(hwmtx);
	return (ch);
}

static void
phyp_uart_putc(struct uart_bas *bas, int c)
{
	uint64_t cbuf;

	cbuf = (uint64_t)c << 56;
	phyp_hcall(H_PUT_TERM_CHAR, (uint64_t)bas->bsh, 1UL, cbuf, 0);
}

static int
phyp_uart_rxready(struct uart_bas *bas)
{
	return (1);
}
