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
#include <sys/kobj.h>

#include <dev/vtc/vtc_te.h>

#include "vtc_te_if.h"

/* VT-102 emulator states. */
#define	S_NORMAL	0
#define	S_ESCAPE	1	/* ESC received -- escape sequence. */
#define	S_CONTROL	2	/* CSI received -- control sequence. */

struct vt102_softc {
	struct vtc_te_softc base;
	int	state;
	int	count;
	char	cbuf[16];
	int	col, row;
	int	maxcol, maxrow;

	int	autowrap:1;
	int	dh:1;
	int	dw:1;
	int	underline:1;

	int	wrap:1;
};

static int vt102_reset(struct vtc_te_softc *);
static int vt102_write(struct vtc_te_softc *, __wchar_t);

static kobj_method_t vt102_methods[] = {
	KOBJMETHOD(vtc_te_reset,	vt102_reset),
	KOBJMETHOD(vtc_te_write,	vt102_write),
	{ 0, 0 }
};

struct vtc_te_class vt102_class = {
	"vt102 class",
	vt102_methods,
	sizeof(struct vt102_softc),
};

static __inline int
vt102_putc(struct vt102_softc *vt102, __wchar_t wc)
{

	return (vtc_te_putc(&vt102->base, vt102->row << vt102->dh,
		vt102->col << vt102->dw, 1 << vt102->dw, 1 << vt102->dh,
		vt102->underline, wc));
}

static __inline int
vt102_repos(struct vt102_softc *vt102)
{
	int col;

	col = vt102->col;
	if (col == vt102->maxcol)
		col--;
	return (vtc_te_repos(&vt102->base, vt102->row << vt102->dh,
		col << vt102->dw));
}

static __inline int
vt102_newline(struct vt102_softc *vt102)
{

	if (vt102->row == vt102->maxrow - 1)
		return (vtc_te_scroll(&vt102->base, 0, 0,
			vt102->maxrow << vt102->dh, vt102->maxcol << vt102->dw,
			1 << vt102->dh));
	vt102->row++;
	return (vt102_repos(vt102));
}

static int
vt102_answerback(struct vt102_softc *vt102)
{
	return (0);
}

static __inline int
vt102_print(struct vt102_softc *vt102, __wchar_t wc)
{

	if (vt102->autowrap && vt102->wrap) {
		vt102->col = 0;
		vt102->wrap = 0;
		vt102_newline(vt102);
	}
	if (vt102->col < vt102->maxcol) {
		vt102_putc(vt102, wc);
		vt102->col++;
		if (vt102->col == vt102->maxcol && vt102->autowrap)
			vt102->wrap = 1;
		else
			vt102_repos(vt102);
	}
	return (0);
}

static int
vt102_ctlchr(struct vt102_softc *vt102, char cc)
{
	int error = 0;

	switch (cc) {
	case 0x05:	/* ENQ */
		error = vt102_answerback(vt102);
		break;
	case 0x07:	/* BEL */
		error = vtc_te_bell(&vt102->base);
		break;
	case 0x08:	/* BS */
		if (vt102->col > 0) {
			vt102->col--;
			vt102->wrap = 0;
			vt102_putc(vt102, ' ');
			vt102_repos(vt102);
		}
		break;
	case 0x09:	/* HT */
		if (vt102->autowrap && vt102->wrap) {
			vt102->col = 0;
			vt102->wrap = 0;
			vt102_newline(vt102);
		}
		if (vt102->col < vt102->maxcol) {
			vt102->col = (vt102->col + 8) & ~7;
			if (vt102->col > vt102->maxcol)
				vt102->col = vt102->maxcol;
			if (vt102->col == vt102->maxcol && vt102->autowrap)
				vt102->wrap = 1;
			vt102_repos(vt102);
		}
		break;
	case 0x0A:	/* LF */
	case 0x0B:	/* VT (processed as LF) */
	case 0x0C:	/* FF (processed as LF) */
		vt102_newline(vt102);
		break;
	case 0x0D:	/* CR */
		if (vt102->col > 0) {
			vt102->col = 0;
			vt102->wrap = 0;
			vt102_repos(vt102);
		}
		break;
	case 0x0E:	/* SO */
		/*
		 * Select G1 character set. We're Unicode, hence no character
		 * set switching.
		 */
		break;
	case 0x0F:	/* SI */
		/*
		 * Select G0 character set. We're Unicode, hence no character
		 * set switching.
		 */
		break;
	case 0x18:	/* CAN */
	case 0x1A:	/* SUB (processed as CAN) */
		if (vt102->state != S_NORMAL) {
			error = vt102_print(vt102, -1);
			vt102->state = S_NORMAL;
		}
		break;
	case 0x1B:	/* ESC */
		vt102->state = S_ESCAPE;
		vt102->count = 0;
		break;
	}

	return (error);
}

static int
vt102_control(struct vt102_softc *vt102)
{
	return (0);
}

static int
vt102_escape(struct vt102_softc *vt102)
{
	return (0);
}

static int
vt102_reset(struct vtc_te_softc *te)
{
	struct vt102_softc *vt102 = (struct vt102_softc *)te;

	te->te_maxcol = 80;
	te->te_maxrow = 24;

	vt102->maxcol = 80;
	vt102->maxrow = 24;

	vt102->autowrap = 1;
	return (0);
}

static int
vt102_write(struct vtc_te_softc *te, __wchar_t wc)
{
	struct vt102_softc *vt102 = (struct vt102_softc *)te;
	int error;

	KASSERT(wc >= 0, ("Negative UTF-32 characters don't exist!"));

	if (vt102->state == S_NORMAL) {
		if (wc <= 0x1F || wc == 0x7F)
			return (vt102_ctlchr(vt102, (char)wc));
		return (vt102_print(vt102, wc));
	}

	/*
	 * Escape or control sequences.
	 */

	/* Process control characters as normal. */
	if (wc <= 0x1F)
		return (vt102_ctlchr(vt102, (char)wc));

	/*
	 * The VT-102 ignores DEL and since it's a 7-bit terminal it won't
	 * ever see character codes beyond that. We do. Ignore them as well.
	 */
	if (wc >= 0x7F)
		return (0);

	/* Save the character. Is there a maximum for the control string? */
	vt102->cbuf[vt102->count++] = (char)wc;

	/* That's it for intermediate characters. */
	if (wc <= 0x2F)
		return (0);

	/*
	 * What are final characters for escape sequences, are parameter
	 * characters for control sequences.
	 */
	if (vt102->state == S_CONTROL && wc <= 0x3F)
		return (0);

	/* Detect the CSI. */
	if (vt102->count == 1 && wc == 0x5B) {
		vt102->state = S_CONTROL;
		return (0);
	}

	/* It's a final character. */
	if (vt102->state == S_ESCAPE)
		error = vt102_escape(vt102);
	else
		error = vt102_control(vt102);
	vt102->state = S_NORMAL;
	return (error);
}
