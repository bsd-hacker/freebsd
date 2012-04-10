/*-
 * Copyright (c) 2005 Marcel Moolenaar
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
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/rman.h>
#include <sys/tty.h>

#include <dev/vtc/vtc.h>
#include <dev/vtc/vtc_con.h>

static cn_getc_t vtc_cngetc;
static cn_grab_t vtc_cngrab;
static cn_init_t vtc_cninit;
static cn_probe_t vtc_cnprobe;
static cn_putc_t vtc_cnputc;
static cn_term_t vtc_cnterm;
static cn_ungrab_t vtc_cnungrab;

static int vtc_cncol = 0;
static int vtc_cnrow = 0;
static int vtc_cnmute = 0;

CONSOLE_DRIVER(vtc);

static void
vtc_cnprobe(struct consdev *cp)
{
	struct vtc_conout *cur_vc, *vc, **iter;
	int cur_pri, pri;

	cur_pri = -1;
	cur_vc = NULL;
	SET_FOREACH(iter, vtc_conout_set) {
		vc = *iter;
		pri = (vc->vtc_con_probe != NULL) ? vc->vtc_con_probe(vc) : -1;
		if (pri > cur_pri) {
			cur_pri = pri;
			cur_vc = vc;
		}
	}
	strlcpy(cp->cn_name, vtc_device_name, sizeof(cp->cn_name));
	cp->cn_arg = cur_vc;
	cp->cn_pri = (cur_vc != NULL) ? CN_INTERNAL : CN_DEAD;
}

static void
vtc_cninit(struct consdev *cp)
{
	struct vtc_conout *vc = cp->cn_arg;

	vc->vtc_consdev = cp;
	vc->vtc_con_init(vc);
}

static void
vtc_cnterm(struct consdev *cp)
{
}

static void
vtc_cnputc(struct consdev *cp, int c)
{
	struct vtc_conout *vc = cp->cn_arg;
	int ch, width;
	u_char *glyph;

	/* Sanity check. */
	if (c <= 0 || c >= 0x7f)
		return;

	/* Allow low-level console output to be surpressed. */
	if (vtc_cnmute && !kdb_active)
		return;

	width = vc->vtc_con_width;
	switch (c) {
	case 8:		/* BS */
		if (vtc_cncol > 0)
			vtc_cncol--;
		ch = ' ';
		break;
	case 9:		/* HT */
		vtc_cncol = (vtc_cncol + 7) & 7;
		if (vtc_cncol >= 80)
			vtc_cncol = 79;
		ch = 0;
		break;
	case 10:	/* LF */
	case 11:	/* VT (processed as LF) */
	case 12:	/* FF (processed as LF) */
		vtc_cncol = 0;
		vtc_cnrow++;
		ch = 0;
		break;
	case 13:	/* CR */
		vtc_cncol = 0;
		ch = 0;
		break;
	default:
		ch = (c >= ' ') ? c : 0;
		break;
	}

	if (ch != 0) {
		glyph = vtc_font_8x16 + ((ch - ' ') * 16);
		vc->vtc_con_bitblt(vc, BITBLT_H1TOFB, (uintptr_t)glyph,
		    width * vtc_cnrow * 16 + vtc_cncol * 8, 8, 16, 0, 7);
		if (c != 8)
			vtc_cncol++;
	}

	if (vtc_cncol >= 80) {
		vtc_cncol = 0;
		vtc_cnrow++;
	}
	if (vtc_cnrow >= 30) {
		vc->vtc_con_bitblt(vc, BITBLT_FBTOFB, width * 16, 0, width,
		    29 * 16);
		vc->vtc_con_bitblt(vc, BITBLT_CTOFB, 0, width * 29 * 16, width,
		    16);
		vtc_cnrow = 29;
	}
}

static int
vtc_cngetc(struct consdev *cp)
{

	return (-1);
}

static void
vtc_cngrab(struct consdev *cp)
{
}

static void
vtc_cnungrab(struct consdev *cp)
{
}

static void
vtc_cnfinalize(void *data __unused)
{

	/* Shut the low-level console up. */
	vtc_cnmute = 1;
}
SYSINIT(cnfinalize, SI_SUB_VTC, SI_ORDER_ANY, vtc_cnfinalize, NULL);
