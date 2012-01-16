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
#include <sys/module.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
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
static uint8_t			phyp_outseqno = 0;

enum {
	HVTERM1, HVTERMPROT
};

#define VS_DATA_PACKET_HEADER		0xff
#define VS_CONTROL_PACKET_HEADER	0xfe
#define  VSV_SET_MODEM_CTL		0x01
#define  VSV_MODEM_CTL_UPDATE		0x02
#define  VSV_RENEGOTIATE_CONNECTION	0x03
#define VS_QUERY_PACKET_HEADER		0xfd
#define  VSV_SEND_VERSION_NUMBER	0x01
#define  VSV_SEND_MODEM_CTL_STATUS	0x02
#define VS_QUERY_RESPONSE_PACKET_HEADER	0xfc

/*
 * High-level interface
 */

static int uart_phyp_probe(device_t dev);
static int phyp_uart_bus_probe(struct uart_softc *);
static int phyp_uart_bus_attach(struct uart_softc *);
static int phyp_uart_bus_transmit(struct uart_softc *sc);
static int phyp_uart_bus_receive(struct uart_softc *sc);
static int phyp_uart_bus_ipend(struct uart_softc *sc);
static int phyp_uart_bus_flush(struct uart_softc *, int);
static int phyp_uart_bus_getsig(struct uart_softc *);
static int phyp_uart_bus_ioctl(struct uart_softc *, int, intptr_t);
static int phyp_uart_bus_param(struct uart_softc *, int, int, int, int);
static int phyp_uart_bus_setsig(struct uart_softc *, int);

static device_method_t uart_phyp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_phyp_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),

	DEVMETHOD_END
};

static driver_t uart_phyp_driver = {
	uart_driver_name,
	uart_phyp_methods,
	sizeof(struct uart_softc),
};
 
DRIVER_MODULE(uart, vdevice, uart_phyp_driver, uart_devclass, 0, 0);

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

static kobj_method_t phyp_uart_methods[] = {
	KOBJMETHOD(uart_probe,	phyp_uart_bus_probe),
	KOBJMETHOD(uart_attach,	phyp_uart_bus_attach),
	KOBJMETHOD(uart_transmit,	phyp_uart_bus_transmit),
	KOBJMETHOD(uart_receive,	phyp_uart_bus_receive),
	KOBJMETHOD(uart_ipend,		phyp_uart_bus_ipend),
	KOBJMETHOD(uart_flush,		phyp_uart_bus_flush),
	KOBJMETHOD(uart_getsig,		phyp_uart_bus_getsig),
	KOBJMETHOD(uart_ioctl,		phyp_uart_bus_ioctl),
	KOBJMETHOD(uart_param,		phyp_uart_bus_param),
	KOBJMETHOD(uart_setsig,		phyp_uart_bus_setsig),
	{ 0, 0 }
};

struct uart_class uart_phyp_class = {
	"uart",
	phyp_uart_methods,
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

static int
phyp_uart_bus_probe(struct uart_softc *sc)
{
	return (phyp_uart_probe(&sc->sc_bas));
}

static int
uart_phyp_probe(device_t dev)
{
	const char *name;
	struct uart_softc *sc;
	cell_t reg;

	name = ofw_bus_get_name(dev);
	if (name == NULL || strcmp(name, "vty") != 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_class = &uart_phyp_class;
	OF_getprop(ofw_bus_get_node(dev), "reg", &reg, sizeof(reg));
	sc->sc_bas.bsh = reg;
	sc->sc_bas.bst = NULL;
	sc->sc_bas.chan = ofw_bus_get_node(dev);

	device_set_desc(dev, "POWER Hypervisor Virtual Serial Port");

	return (uart_bus_probe(dev, 0, 0, 0, ofw_bus_get_node(dev)));
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
	uint16_t seqno;
	uint64_t len = 0;
	union {
		uint64_t u64;
		char bytes[8];
	} cbuf;

	switch (bas->regshft) {
	case HVTERM1:
		cbuf.bytes[0] = c;
		len = 1;
		break;
	case HVTERMPROT:
		seqno = phyp_outseqno++;
		cbuf.bytes[0] = VS_DATA_PACKET_HEADER;
		cbuf.bytes[1] = 5; /* total length */
		cbuf.bytes[2] = (seqno >> 8) & 0xff;
		cbuf.bytes[3] = seqno & 0xff;
		cbuf.bytes[4] = c;
		len = 5;
		break;
	}
	phyp_hcall(H_PUT_TERM_CHAR, (uint64_t)bas->bsh, len, cbuf.u64, 0);
}

static int
phyp_uart_rxready(struct uart_bas *bas)
{
	return (1);
}

static int
phyp_uart_bus_attach(struct uart_softc *sc)
{
	return (0);
}

static int
phyp_uart_bus_transmit(struct uart_softc *sc)
{
	int i;

	uart_lock(sc->sc_hwmtx);
	for (i = 0; i < sc->sc_txdatasz; i++)
		phyp_uart_putc(&sc->sc_bas, sc->sc_txbuf[i]);
	uart_unlock(sc->sc_hwmtx);

	return (0);
}

static int
phyp_uart_bus_receive(struct uart_softc *sc)
{
	int c;
	while ((c = phyp_uart_getc(&sc->sc_bas, sc->sc_hwmtx)) != -1)
		uart_rx_put(sc, c);

	return (0);
}

static int
phyp_uart_bus_ipend(struct uart_softc *sc)
{
	return (0);
}

static int
phyp_uart_bus_flush(struct uart_softc *sc, int what)
{
	return (0);
}

static int
phyp_uart_bus_getsig(struct uart_softc *sc)
{
	return (0);
}

static int
phyp_uart_bus_ioctl(struct uart_softc *sc, int req, intptr_t data)
{
	return (EINVAL);
}

static int
phyp_uart_bus_param(struct uart_softc *sc, int baud, int db, int sb, int par)
{
	return (0);
}

static int
phyp_uart_bus_setsig(struct uart_softc *sc, int sig)
{
	return (0);
}


