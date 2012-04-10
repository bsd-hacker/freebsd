/*-
 * Copyright (c) 2002-2005 Marcel Moolenaar
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

#include <machine/bus.h>
#include <machine/bus_private.h>

#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>

#include <dev/ic/vga.h>
#include <dev/vtc/vtout/vga/vga.h>

int
vga_get_console(struct vga_consdata *cd)
{
	static struct bus_space_tag bst_store[2];
	char odev[32];
	ihandle_t stdout;
	phandle_t chosen, oh, options;

	/*
	 * Query OFW to see if we have a graphical console and whether
	 * it's actually a VGA.
	 */
	if ((options = OF_finddevice("/options")) == -1)
		return (ENXIO);
	if (OF_getprop(options, "output-device", odev, sizeof(odev)) == -1)
		return (ENXIO);
	if (strcmp(odev, "screen") != 0)
		return (ENODEV);
	if ((oh = OF_finddevice(odev)) == -1)
		return (ENXIO);
	if ((chosen = OF_finddevice("/chosen")) == -1)
		return (ENXIO);
	if (OF_getprop(chosen, "stdout", &stdout, sizeof(stdout)) == -1)
		return (ENXIO);
	if (OF_instance_to_package(stdout) != oh)
		return (ENODEV);

	/* XXX check if it's a VGA. */

	/* Construct the bus tags and handles. */
	cd->fb.bst = &bst_store[0];
	cd->fb.bsh = sparc64_fake_bustag(PCI_MEMORY_BUS_SPACE, 0x1ff000a0000,
	    cd->fb.bst);
	cd->reg.bst = &bst_store[1];
	cd->reg.bsh = sparc64_fake_bustag(PCI_IO_BUS_SPACE, 0x1fe020003c0,
	    cd->reg.bst);
	return (0);
}
