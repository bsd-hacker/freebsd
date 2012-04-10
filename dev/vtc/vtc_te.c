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

#include <dev/vtc/vtc.h>
#include <dev/vtc/vtc_te.h>
#include <dev/vtc/vtc_vtout.h>

int
vtc_te_bell(struct vtc_te_softc *te)
{

	return (ENXIO);
}

int
vtc_te_putc(struct vtc_te_softc *te, int row, int col, int ws, int hs, int fl,
    __wchar_t wc)
{
	struct vtc_vtout_softc *vo;
	uint8_t *glyph;

	KASSERT(wc >= 0x20, ("Unexpected control character"));

	TAILQ_FOREACH(vo, &te->te_vodevs, vo_tedevs) {
		if (vo->vo_ws != ws || vo->vo_hs != hs) {
			vo->vo_cw = (vo->vo_width / te->te_maxcol) * ws;
			vo->vo_ch = (vo->vo_height / te->te_maxrow) * hs;
			/*
			 * Lookup matching font, with glyphs no wider than
			 * vtout->vo_cw and no higher than vtout->vo_ch.
			 */
			vo->vo_font = vtc_font_8x16;
		}

		glyph = vo->vo_font + (wc - 0x20) * 16;
		vo->vo_bitblt(vo->vo_dev, BITBLT_H1TOFB, (uintptr_t)glyph,
		    vo->vo_width * row * vo->vo_ch + col * vo->vo_cw,
		    8, 16, 0, 7);
	}
	return (0);
}

int
vtc_te_repos(struct vtc_te_softc *te, int row, int col)
{

	return (ENXIO);
}

int
vtc_te_scroll(struct vtc_te_softc *te, int ulr, int ulc, int lrr, int lrc,
    int hs)
{
	struct vtc_vtout_softc *vo;
	int ch, cw;

	TAILQ_FOREACH(vo, &te->te_vodevs, vo_tedevs) {
		ch = vo->vo_height / te->te_maxrow;
		cw = vo->vo_width / te->te_maxcol;

		vo->vo_bitblt(vo->vo_dev, BITBLT_FBTOFB,
		    vo->vo_width * (ulr + hs) * ch + ulc * cw,
		    vo->vo_width * ulr * ch + ulc * cw,
		    (lrc - ulc) * cw, (lrr - ulr - hs) * ch);

		vo->vo_bitblt(vo->vo_dev, BITBLT_CTOFB, 0,
		    vo->vo_width * (lrr - hs) * ch + ulc * cw,
		    (lrc - ulc) * cw, hs * ch);
	}
	return (0);
}
