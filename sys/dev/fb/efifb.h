/*-
 * Copyright (c) 2003 Peter Grehan
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
 *
 * $FreeBSD$
 */

#ifndef _EFIFB_H_
#define _EFIFB_H_

struct efifb_softc {
	video_adapter_t		sc_va;
	struct cdev *		sc_si;
	int	       		sc_console;

	void *			sc_addr;
        int	       		sc_height;
        int	       		sc_width;
	int			sc_depth;
	int	       		sc_stride;
	int			sc_red_bits;
	int			sc_red_shift;
	int			sc_green_bits;
	int			sc_green_shift;
	int			sc_blue_bits;
	int			sc_blue_shift;

        int	       		sc_ncol;
        int	       		sc_nrow;

	int	       		sc_xmargin;
	int	       		sc_ymargin;

	u_char *		sc_font;
	int			sc_font_height;

	vi_putc_t *		sc_putc;
	vi_putm_t *		sc_putm;
	vi_set_border_t *	sc_set_border;
};

/* Color attributes for foreground text */

#define FG_BLACK                0x0
#define FG_BLUE                 0x1
#define FG_GREEN                0x2
#define FG_CYAN                 0x3
#define FG_RED                  0x4
#define FG_MAGENTA              0x5
#define FG_BROWN                0x6
#define FG_LIGHTGREY            0x7     /* aka white */
#define FG_DARKGREY             0x8
#define FG_LIGHTBLUE            0x9
#define FG_LIGHTGREEN           0xa
#define FG_LIGHTCYAN            0xb
#define FG_LIGHTRED             0xc
#define FG_LIGHTMAGENTA         0xd
#define FG_YELLOW               0xe
#define FG_WHITE                0xf     /* aka bright white */
#define FG_BLINK                0x80

/* Color attributes for text background */

#define BG_BLACK                0x00
#define BG_BLUE                 0x10
#define BG_GREEN                0x20
#define BG_CYAN                 0x30
#define BG_RED                  0x40
#define BG_MAGENTA              0x50
#define BG_BROWN                0x60
#define BG_LIGHTGREY            0x70
#define BG_DARKGREY             0x80
#define BG_LIGHTBLUE            0x90
#define BG_LIGHTGREEN           0xa0
#define BG_LIGHTCYAN            0xb0
#define BG_LIGHTRED             0xc0
#define BG_LIGHTMAGENTA         0xd0
#define BG_YELLOW               0xe0
#define BG_WHITE                0xf0

#endif	/* _EFIFB_H_ */
