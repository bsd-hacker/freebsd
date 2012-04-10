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
#include <sys/kernel.h>
#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/tty.h>

#include <dev/ic/vga.h>
#include <dev/vtc/vtout/vga/vga.h>

#include <dev/vtc/vtc_con.h>

static vtc_con_bitblt_f vga_con_bitblt;
static vtc_con_init_f vga_con_init;
static vtc_con_probe_f vga_con_probe;

VTC_CONOUT(vga, vga_con_probe, vga_con_init, vga_con_bitblt);

static int
vga_con_probe(struct vtc_conout *co)
{
	struct vga_consdata cd;

	bzero(&vga_console, sizeof(vga_console));

	if (vga_get_console(&cd) != 0)
		return (-1);

	vga_console.vga_fb = cd.fb;
	vga_console.vga_reg = cd.reg;

	if (!vga_probe(&vga_console))
		return (-1);

	co->vtc_con_cookie = &vga_console;
	co->vtc_con_width = 640;
	co->vtc_con_height = 480;
	co->vtc_con_depth = 4;
	return (0);
}

static void
vga_con_init(struct vtc_conout *co)
{
	struct vga_softc *sc = co->vtc_con_cookie;

	if (vga_init(sc) == 0) {
		sc->vga_conout = co;
		sc->vga_console = 1;
	}
}

static void
vga_con_bitblt(struct vtc_conout *co, int op, uintptr_t src, uintptr_t dst,
    int width, int height, ...)
{
	struct vga_softc *sc = co->vtc_con_cookie;
	va_list ap;

	va_start(ap, height);
	vga_vbitblt(sc, op, src, dst, width, height, ap);
	va_end(ap);
}
