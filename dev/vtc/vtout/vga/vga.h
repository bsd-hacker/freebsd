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
 *
 * $FreeBSD$
 */

#ifndef _DEV_VTC_HW_VGA_H_
#define	_DEV_VTC_HW_VGA_H_

#include <machine/stdarg.h>

struct vga_spc
{
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
};

struct vga_softc
{
	device_t	vga_dev;
	dev_t		vga_node;
	struct vtc_conout *vga_conout;

	/* Device flags and state. */
	int		vga_bustype:2;
#define	VGA_BUSTYPE_ISA		1
#define	VGA_BUSTYPE_PCI		2
	int		vga_console:1;
	int		vga_enable:1;

	/* Bus spaces */
	struct vga_spc	vga_spc[2];
#define	VGA_RES_FB		0
#define	VGA_RES_REG		1
};

#define	vga_fb		vga_spc[VGA_RES_FB]
#define	vga_reg		vga_spc[VGA_RES_REG]

struct vga_consdata {
	struct vga_spc	fb;
	struct vga_spc	reg;
};

int vga_get_console(struct vga_consdata*);

extern struct vga_softc	vga_console;
extern devclass_t vga_devclass;
extern char vga_device_name[];

int vga_attach(device_t);
int vga_vbitblt(struct vga_softc *, int, uintptr_t, uintptr_t, int, int,
    va_list);
int vga_init(struct vga_softc *);
int vga_probe(struct vga_softc *);

#endif /* !_DEV_VTC_HW_VGA_H_ */
