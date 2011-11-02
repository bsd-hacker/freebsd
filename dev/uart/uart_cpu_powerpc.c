/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/ofw_machdep.h>

#include <dev/ofw/openfirm.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

bus_space_tag_t uart_bus_space_io = &bs_le_tag;
bus_space_tag_t uart_bus_space_mem = &bs_le_tag;

extern struct uart_class uart_phyp_class __attribute__((weak));

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	if (b1->bst == NULL && b2->bst == NULL)
		return ((b1->bsh == b2->bsh) ? 1 : 0);
	else if (b1->bst != NULL && b2->bst != NULL)
		return ((pmap_kextract(b1->bsh) == pmap_kextract(b2->bsh)) ?
		    1 : 0);
	else
		return (0);
}

static int
ofw_get_uart_console(phandle_t opts, phandle_t *result, const char *inputdev,
    const char *outputdev)
{
	char buf[64];
	phandle_t input, output;

	*result = -1;
	if (OF_getprop(opts, inputdev, buf, sizeof(buf)) == -1)
		return (ENOENT);
	input = OF_finddevice(buf);
	if (input == -1)
		return (ENOENT);
	if (OF_getprop(opts, outputdev, buf, sizeof(buf)) == -1)
		return (ENOENT);
	output = OF_finddevice(buf);
	if (output == -1)
		return (ENOENT);

	if (input != output) /* UARTs are bidirectional */
		return (ENXIO);

	*result = input;
	return (0);
}

int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	char buf[64];
	struct uart_class *class;
	ihandle_t stdout;
	phandle_t input, opts, chosen;
	cell_t reg;
	int error;

	if ((opts = OF_finddevice("/options")) == -1)
		return (ENXIO);
	if ((chosen = OF_finddevice("/chosen")) == -1)
		return (ENXIO);
	switch (devtype) {
	case UART_DEV_CONSOLE:
		do {
			/* Check if OF has an active stdin/stdout */
			input = -1;
			if (OF_getprop(chosen, "stdout", &stdout,
			    sizeof(stdout)) == sizeof(stdout) && stdout != 0)
				input = OF_instance_to_package(stdout);
			if (input != -1)
				break;

			/* Guess what OF would have done had it had such */
			if (ofw_get_uart_console(opts, &input, "input-device",
			    "output-device") == 0)
				break;

			/*
			 * At least some G5 Xserves require that we
			 * probe input-device-1 as well
			 */
			if (ofw_get_uart_console(opts, &input, "input-device-1",
			    "output-device-1") == 0)
				break;
		} while (0);

		if (input == -1)
			return (ENXIO);
		break;
	case UART_DEV_DBGPORT:
		if (!getenv_string("hw.uart.dbgport", buf, sizeof(buf)))
			return (ENXIO);
		input = OF_finddevice(buf);
		if (input == -1)
			return (ENXIO);
		break;
	default:
		return (EINVAL);
	}

	if (OF_getprop(input, "device_type", buf, sizeof(buf)) == -1)
		return (ENXIO);
	if (strcmp(buf, "serial") != 0)
		return (ENXIO);
	if (OF_getprop(input, "name", buf, sizeof(buf)) == -1)
		return (ENXIO);

	if (strcmp(buf, "ch-a") == 0) {
		class = &uart_z8530_class;
		di->bas.regshft = 4;
		di->bas.chan = 1;
	} else if (strcmp(buf,"serial") == 0) {
		class = &uart_ns8250_class;
		di->bas.regshft = 0;
		di->bas.chan = 0;
	} else if (strcmp(buf,"vty") == 0) {
		class = &uart_phyp_class;
		di->bas.regshft = 0;
		di->bas.chan = input;
	} else
		return (ENXIO);

	if (strcmp(buf,"vty") == 0) {
		if (OF_getproplen(input, "reg") != sizeof(reg))
			return (ENXIO);
		OF_getprop(input, "reg", &reg, sizeof(reg));
		di->bas.bsh = reg;
		di->bas.bst = NULL;
	} else {
		error = OF_decode_addr(input, 0, &di->bas.bst, &di->bas.bsh);
		if (error)
			return (error);
	}

	di->ops = uart_getops(class);

	if (OF_getprop(input, "clock-frequency", &di->bas.rclk, 
	    sizeof(di->bas.rclk)) == -1)
		di->bas.rclk = 230400;
	if (OF_getprop(input, "current-speed", &di->baudrate, 
	    sizeof(di->baudrate)) == -1)
		di->baudrate = 0;

	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;
	return (0);
}
