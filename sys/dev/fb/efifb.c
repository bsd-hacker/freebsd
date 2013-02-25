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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <machine/bus.h>
#include <machine/efi.h>
#include <machine/metadata.h>
#include <machine/vm.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <sys/rman.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/efifb.h>
#include <dev/syscons/syscons.h>

extern u_char dflt_font_16[];
extern u_char dflt_font_14[];
extern u_char dflt_font_8[];

static int efifb_configure(int flags);

static vi_probe_t		efifb_probe;
static vi_init_t		efifb_init;
static vi_get_info_t		efifb_get_info;
static vi_query_mode_t		efifb_query_mode;
static vi_set_mode_t		efifb_set_mode;
static vi_save_font_t		efifb_save_font;
static vi_load_font_t		efifb_load_font;
static vi_show_font_t		efifb_show_font;
static vi_save_palette_t	efifb_save_palette;
static vi_load_palette_t	efifb_load_palette;
static vi_set_border_t		efifb_set_border;
static vi_save_state_t		efifb_save_state;
static vi_load_state_t		efifb_load_state;
static vi_set_win_org_t		efifb_set_win_org;
static vi_read_hw_cursor_t	efifb_read_hw_cursor;
static vi_set_hw_cursor_t	efifb_set_hw_cursor;
static vi_set_hw_cursor_shape_t	efifb_set_hw_cursor_shape;
static vi_blank_display_t	efifb_blank_display;
static vi_mmap_t		efifb_mmap;
static vi_ioctl_t		efifb_ioctl;
static vi_clear_t		efifb_clear;
static vi_fill_rect_t		efifb_fill_rect;
static vi_bitblt_t		efifb_bitblt;
static vi_diag_t		efifb_diag;
static vi_save_cursor_palette_t	efifb_save_cursor_palette;
static vi_load_cursor_palette_t	efifb_load_cursor_palette;
static vi_copy_t		efifb_copy;
static vi_putp_t		efifb_putp;
static vi_putc_t		efifb_putc;
static vi_puts_t		efifb_puts;
static vi_putm_t		efifb_putm;

static video_switch_t efifbvidsw = {
	.probe			= efifb_probe,
	.init			= efifb_init,
	.get_info		= efifb_get_info,
	.query_mode		= efifb_query_mode,
	.set_mode		= efifb_set_mode,
	.save_font		= efifb_save_font,
	.load_font		= efifb_load_font,
	.show_font		= efifb_show_font,
	.save_palette		= efifb_save_palette,
	.load_palette		= efifb_load_palette,
	.set_border		= efifb_set_border,
	.save_state		= efifb_save_state,
	.load_state		= efifb_load_state,
	.set_win_org		= efifb_set_win_org,
	.read_hw_cursor		= efifb_read_hw_cursor,
	.set_hw_cursor		= efifb_set_hw_cursor,
	.set_hw_cursor_shape	= efifb_set_hw_cursor_shape,
	.blank_display		= efifb_blank_display,
	.mmap			= efifb_mmap,
	.ioctl			= efifb_ioctl,
	.clear			= efifb_clear,
	.fill_rect		= efifb_fill_rect,
	.bitblt			= efifb_bitblt,
	.diag			= efifb_diag,
	.save_cursor_palette	= efifb_save_cursor_palette,
	.load_cursor_palette	= efifb_load_cursor_palette,
	.copy			= efifb_copy,
	.putp			= efifb_putp,
	.putc			= efifb_putc,
	.puts			= efifb_puts,
	.putm			= efifb_putm,
};

VIDEO_DRIVER(efifb, efifbvidsw, efifb_configure);

extern sc_rndr_sw_t txtrndrsw;
RENDERER(efifb, 0, txtrndrsw, gfb_set);

RENDERER_MODULE(efifb, gfb_set);

static vi_putc_t		efifb_putc8;
static vi_putm_t		efifb_putm8;
static vi_set_border_t		efifb_set_border8;

static vi_putc_t		efifb_putc32;
static vi_putm_t		efifb_putm32;
static vi_set_border_t		efifb_set_border32;

/*
 * Define the iso6429-1983 colormap
 */
static struct {
	uint8_t	red;
	uint8_t	green;
	uint8_t	blue;
} efifb_cmap[16] = {		/*  #     R    G    B   Color */
				/*  -     -    -    -   ----- */
	{ 0x00, 0x00, 0x00 },	/*  0     0    0    0   Black */
	{ 0x00, 0x00, 0xaa },	/*  1     0    0  2/3   Blue  */
	{ 0x00, 0xaa, 0x00 },	/*  2     0  2/3    0   Green */
	{ 0x00, 0xaa, 0xaa },	/*  3     0  2/3  2/3   Cyan  */
	{ 0xaa, 0x00, 0x00 },	/*  4   2/3    0    0   Red   */
	{ 0xaa, 0x00, 0xaa },	/*  5   2/3    0  2/3   Magenta */
	{ 0xaa, 0x55, 0x00 },	/*  6   2/3  1/3    0   Brown */
	{ 0xaa, 0xaa, 0xaa },	/*  7   2/3  2/3  2/3   White */
        { 0x55, 0x55, 0x55 },	/*  8   1/3  1/3  1/3   Gray  */
	{ 0x55, 0x55, 0xff },	/*  9   1/3  1/3    1   Bright Blue  */
	{ 0x55, 0xff, 0x55 },	/* 10   1/3    1  1/3   Bright Green */
	{ 0x55, 0xff, 0xff },	/* 11   1/3    1    1   Bright Cyan  */
	{ 0xff, 0x55, 0x55 },	/* 12     1  1/3  1/3   Bright Red   */
	{ 0xff, 0x55, 0xff },	/* 13     1  1/3    1   Bright Magenta */
	{ 0xff, 0xff, 0x80 },	/* 14     1    1  1/3   Bright Yellow */
	{ 0xff, 0xff, 0xff }	/* 15     1    1    1   Bright White */
};

#define	TODO	printf("%s: unimplemented\n", __func__)

static uint16_t efifb_static_window[ROW*COL];

static struct efifb_softc efifb_softc;

static __inline int
efifb_background(uint8_t attr)
{
	return (attr >> 4);
}

static __inline int
efifb_foreground(uint8_t attr)
{
	return (attr & 0x0f);
}

static uint32_t
efifb_pixel(struct efifb_softc *sc, int attr)
{
	uint32_t	color, pixel;

	color = efifb_cmap[attr].red >> (8 - sc->sc_red_bits);
	pixel = color << sc->sc_red_shift;
	color = efifb_cmap[attr].green >> (8 - sc->sc_green_bits);
	pixel |= color << sc->sc_green_shift;
	color = efifb_cmap[attr].blue >> (8 - sc->sc_blue_bits);
	pixel |= color << sc->sc_blue_shift;

	return (pixel);
}

static int
efifb_configure(int flags)
{
	struct efifb_softc *	sc;
	caddr_t			kmdp;
	struct efi_header *	efihdr;
	struct efi_fb *		efifb;
	int			disable;
	int			depth, d;
	static int		done = 0;

	disable = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disable);
	if (disable != 0)
		return (0);

	if (done != 0)
		return (0);
	done = 1;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
        efihdr = (struct efi_header *)preload_search_info(kmdp,
            MODINFO_METADATA | MODINFOMD_EFI);
        if (!efihdr->fb.fb_present)
        	return (0);
        efifb = &efihdr->fb;

        printf("%s>>> fb_present=%d\n", __func__, efifb->fb_present);
        printf("%s>>> fb_addr=0x%016jx\n", __func__, efifb->fb_addr);
        printf("%s>>> fb_size=%jx\n", __func__, efifb->fb_size);
        printf("%s>>> fb_height=%d\n", __func__, efifb->fb_height);
        printf("%s>>> fb_width=%d\n", __func__, efifb->fb_width);
        printf("%s>>> fb_stride=%d\n", __func__, efifb->fb_stride);
        printf("%s>>> fb_mask_red=%08x\n", __func__, efifb->fb_mask_red);
        printf("%s>>> fb_mask_green=%08x\n", __func__, efifb->fb_mask_green);
        printf("%s>>> fb_mask_blue=%08x\n", __func__, efifb->fb_mask_blue);
        printf("%s>>> fb_mask_reserved=%08x\n", __func__, efifb->fb_mask_reserved);

	sc = &efifb_softc;

	sc->sc_console = 1;
	sc->sc_height = efifb->fb_height;
	sc->sc_width = efifb->fb_width;

	depth = fls(efifb->fb_mask_red);
	d = fls(efifb->fb_mask_green);
	depth = d > depth ? d : depth;
	d = fls(efifb->fb_mask_blue);
	depth = d > depth ? d : depth;
	d = fls(efifb->fb_mask_reserved);
	depth = d > depth ? d : depth;
	sc->sc_depth = depth;

	sc->sc_stride = efifb->fb_stride * (depth / 8);
	printf("%s>>> sc_stride=%d\n", __func__, sc->sc_stride);

	sc->sc_red_bits = fls(efifb->fb_mask_red) - ffs(efifb->fb_mask_red) + 1;
	sc->sc_red_shift = ffs(efifb->fb_mask_red) - 1;
	sc->sc_green_bits = fls(efifb->fb_mask_green) -
	    ffs(efifb->fb_mask_green) + 1;
	sc->sc_green_shift = ffs(efifb->fb_mask_green) - 1;
	sc->sc_blue_bits = fls(efifb->fb_mask_blue) -
	    ffs(efifb->fb_mask_blue) + 1;
	sc->sc_blue_shift = ffs(efifb->fb_mask_blue) - 1;

	switch (depth) {
	case 32:
		sc->sc_putc = efifb_putc32;
		sc->sc_putm = efifb_putm32;
		sc->sc_set_border = efifb_set_border32;
		break;
	case 8:
		sc->sc_putc = efifb_putc8;
		sc->sc_putm = efifb_putm8;
		sc->sc_set_border = efifb_set_border8;
		break;
	default:
		return (0);
		break;
	}

	/*
	 * We could use pmap_mapdev here except that the kernel pmap
	 * hasn't been created yet and hence any attempt to lock it will
	 * fail.
	 */
	sc->sc_addr = (void *)PHYS_TO_DMAP(efifb->fb_addr);

	efifb_init(0, &sc->sc_va, 0);

	return (0);
}

static int
efifb_probe(int unit, video_adapter_t **adp, void *arg, int flags)
{
	TODO;
	return (0);
}

static int
efifb_init(int unit, video_adapter_t *adp, int flags)
{
	struct efifb_softc *	sc;
	video_info_t *		vi;
	int			cborder;
	int			font_height;

	sc = (struct efifb_softc *)adp;
	vi = &adp->va_info;

	vid_init_struct(adp, "efifb", -1, unit);

	/* The default font size can be overridden by loader */
	font_height = 16;
	TUNABLE_INT_FETCH("hw.syscons.fsize", &font_height);
	if (font_height == 8) {
		sc->sc_font = dflt_font_8;
		sc->sc_font_height = 8;
	} else if (font_height == 14) {
		sc->sc_font = dflt_font_14;
		sc->sc_font_height = 14;
	} else {
		/* default is 8x16 */
		sc->sc_font = dflt_font_16;
		sc->sc_font_height = 16;
	}

	/* The user can set a border in chars - default is 1 char width */
	cborder = 1;
	TUNABLE_INT_FETCH("hw.syscons.border", &cborder);

	vi->vi_cheight = sc->sc_font_height;
	vi->vi_width = sc->sc_width/8 - 2*cborder;
	vi->vi_height = sc->sc_height/sc->sc_font_height - 2*cborder;
	vi->vi_cwidth = 8;

	/*
	 * Clamp width/height to syscons maximums
	 */
	if (vi->vi_width > COL)
		vi->vi_width = COL;
	if (vi->vi_height > ROW)
		vi->vi_height = ROW;

	sc->sc_xmargin = (sc->sc_width - (vi->vi_width * vi->vi_cwidth)) / 2;
	sc->sc_ymargin = (sc->sc_height - (vi->vi_height * vi->vi_cheight))/2;

	/*
	 * Avoid huge amounts of conditional code in syscons by
	 * defining a dummy h/w text display buffer.
	 */
	adp->va_window = (vm_offset_t)efifb_static_window;

	/*
	 * Enable future font-loading and flag color support, as well as 
	 * adding V_ADP_MODECHANGE so that we ofwfb_set_mode() gets called
	 * when the X server shuts down. This enables us to get the console
	 * back when X disappears.
	 */
	adp->va_flags |= V_ADP_FONT | V_ADP_COLOR | V_ADP_MODECHANGE;

	efifb_set_mode(&sc->sc_va, 0);

	vid_register(&sc->sc_va);

	return (0);
}

static int
efifb_get_info(video_adapter_t *adp, int mode, video_info_t *info)
{
	bcopy(&adp->va_info, info, sizeof(*info));
	return (0);
}

static int
efifb_query_mode(video_adapter_t *adp, video_info_t *info)
{
	TODO;
	return (0);
}

static int
efifb_set_mode(video_adapter_t *adp, int mode)
{
	struct efifb_softc *sc;

	sc = (struct efifb_softc *)adp;

	efifb_blank_display(&sc->sc_va, V_DISPLAY_ON);

	return (0);
}

static int
efifb_save_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	TODO;
	return (0);
}

static int
efifb_load_font(video_adapter_t *adp, int page, int size, int width,
    u_char *data, int c, int count)
{
	struct efifb_softc *sc;

	sc = (struct efifb_softc *)adp;

	/*
	 * syscons code has already determined that current width/height
	 * are unchanged for this new font
	 */
	sc->sc_font = data;
	return (0);
}

static int
efifb_show_font(video_adapter_t *adp, int page)
{

	return (0);
}

static int
efifb_save_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
efifb_load_palette(video_adapter_t *adp, u_char *palette)
{
	/* TODO; */
	return (0);
}

static int
efifb_set_border8(video_adapter_t *adp, int border)
{
	struct efifb_softc *sc;
	int i, j;
	uint8_t *addr;
	uint8_t bground;

	sc = (struct efifb_softc *)adp;

	bground = (uint8_t)(efifb_pixel(sc, efifb_background(border)) & 0xff);

	/* Set top margin */
	addr = (uint8_t *) sc->sc_addr;
	for (i = 0; i < sc->sc_ymargin; i++) {
		for (j = 0; j < sc->sc_width; j++) {
			*(addr + j) = bground;
		}
		addr += sc->sc_stride;
	}

	/* bottom margin */
	addr = (uint8_t *) sc->sc_addr + (sc->sc_height - sc->sc_ymargin)*sc->sc_stride;
	for (i = 0; i < sc->sc_ymargin; i++) {
		for (j = 0; j < sc->sc_width; j++) {
			*(addr + j) = bground;
		}
		addr += sc->sc_stride;
	}

	/* remaining left and right borders */
	addr = (uint8_t *) sc->sc_addr + sc->sc_ymargin*sc->sc_stride;
	for (i = 0; i < sc->sc_height - 2*sc->sc_xmargin; i++) {
		for (j = 0; j < sc->sc_xmargin; j++) {
			*(addr + j) = bground;
			*(addr + j + sc->sc_width - sc->sc_xmargin) = bground;
		}
		addr += sc->sc_stride;
	}

	return (0);
}

static int
efifb_set_border32(video_adapter_t *adp, int border)
{
	/* XXX Be lazy for now and blank entire screen */
	return (efifb_blank_display(adp, border));
}

static int
efifb_set_border(video_adapter_t *adp, int border)
{
	struct efifb_softc *sc;

	sc = (struct efifb_softc *)adp;

	return ((*sc->sc_set_border)(adp, border));
}

static int
efifb_save_state(video_adapter_t *adp, void *p, size_t size)
{
	TODO;
	return (0);
}

static int
efifb_load_state(video_adapter_t *adp, void *p)
{
	TODO;
	return (0);
}

static int
efifb_set_win_org(video_adapter_t *adp, off_t offset)
{
	TODO;
	return (0);
}

static int
efifb_read_hw_cursor(video_adapter_t *adp, int *col, int *row)
{
	*col = 0;
	*row = 0;

	return (0);
}

static int
efifb_set_hw_cursor(video_adapter_t *adp, int col, int row)
{

	return (0);
}

static int
efifb_set_hw_cursor_shape(video_adapter_t *adp, int base, int height,
    int celsize, int blink)
{

	return (0);
}

static int
efifb_blank_display(video_adapter_t *adp, int mode)
{
	struct efifb_softc *sc;
	uint32_t	cell;
	uint64_t	addr;
	int		i, count;

	sc = (struct efifb_softc *)adp;

	switch (sc->sc_depth) {
	case 32:
		cell = efifb_pixel(sc, efifb_background(SC_NORM_ATTR));
		break;
	case 8:
		cell = efifb_pixel(sc, efifb_background(SC_NORM_ATTR)) & 0xff;
		cell |= (cell << 8);
		cell |= (cell << 16);
		break;
	default:
		return (0);
	}

	for (i = 0; i < sc->sc_height; i++) {
		count = sc->sc_width;
		addr = (uint64_t)sc->sc_addr + (sc->sc_stride * i);
		for (; count != 0; count--, addr += 4)
			*(volatile uint32_t *)(addr) = cell;
	}

	return (0);
}

static int
efifb_mmap(video_adapter_t *adp, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct efifb_softc *	sc;

	sc = (struct efifb_softc *)adp;

	/*
	 * This might be a legacy VGA mem request: if so, just point it at the
	 * framebuffer, since it shouldn't be touched
	 */
	if (offset < sc->sc_stride*sc->sc_height) {
		*paddr = (vm_paddr_t)sc->sc_addr + offset;
		return (0);
	}

	return (EINVAL);
}

static int
efifb_ioctl(video_adapter_t *adp, u_long cmd, caddr_t data)
{

	return (0);
}

static int
efifb_clear(video_adapter_t *adp)
{
	TODO;
	return (0);
}

static int
efifb_fill_rect(video_adapter_t *adp, int val, int x, int y, int cx, int cy)
{
	TODO;
	return (0);
}

static int
efifb_bitblt(video_adapter_t *adp, ...)
{
	TODO;
	return (0);
}

static int
efifb_diag(video_adapter_t *adp, int level)
{
	TODO;
	return (0);
}

static int
efifb_save_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
efifb_load_cursor_palette(video_adapter_t *adp, u_char *palette)
{
	TODO;
	return (0);
}

static int
efifb_copy(video_adapter_t *adp, vm_offset_t src, vm_offset_t dst, int n)
{
	TODO;
	return (0);
}

static int
efifb_putp(video_adapter_t *adp, vm_offset_t off, uint32_t p, uint32_t a,
    int size, int bpp, int bit_ltor, int byte_ltor)
{
	TODO;
	return (0);
}

static int
efifb_putc8(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct efifb_softc *sc;
	int row;
	int col;
	int i;
	uint32_t *addr;
	u_char *p, fg, bg;
	union {
		uint32_t l;
		uint8_t  c[4];
	} ch1, ch2;

	sc = (struct efifb_softc *)adp;
        row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
        col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	p = sc->sc_font + c*sc->sc_font_height;
	addr = (u_int32_t *)((uintptr_t)sc->sc_addr
		+ (row + sc->sc_ymargin)*sc->sc_stride
		+ col + sc->sc_xmargin);

	fg = (u_char)(efifb_pixel(sc, efifb_foreground(a)) & 0xff);
	bg = (u_char)(efifb_pixel(sc, efifb_background(a)) & 0xff);

	for (i = 0; i < sc->sc_font_height; i++) {
		u_char fline = p[i];

		/*
		 * Assume that there is more background than foreground
		 * in characters and init accordingly
		 */
		ch1.l = ch2.l = (bg << 24) | (bg << 16) | (bg << 8) | bg;

		/*
		 * Calculate 2 x 4-chars at a time, and then
		 * write these out.
		 */
		if (fline & 0x80) ch1.c[0] = fg;
		if (fline & 0x40) ch1.c[1] = fg;
		if (fline & 0x20) ch1.c[2] = fg;
		if (fline & 0x10) ch1.c[3] = fg;

		if (fline & 0x08) ch2.c[0] = fg;
		if (fline & 0x04) ch2.c[1] = fg;
		if (fline & 0x02) ch2.c[2] = fg;
		if (fline & 0x01) ch2.c[3] = fg;

		addr[0] = ch1.l;
		addr[1] = ch2.l;
		addr += (sc->sc_stride / sizeof(u_int32_t));
	}

	return (0);
}

static int
efifb_putc32(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct efifb_softc *sc;
	int row;
	int col;
	int i, j, k;
	uint32_t *addr;
	u_char *p;

	sc = (struct efifb_softc *)adp;
        row = (off / adp->va_info.vi_width) * adp->va_info.vi_cheight;
        col = (off % adp->va_info.vi_width) * adp->va_info.vi_cwidth;
	p = sc->sc_font + c*sc->sc_font_height;
	addr = (uint32_t *)sc->sc_addr
		+ (row + sc->sc_ymargin)*(sc->sc_stride/4)
		+ col + sc->sc_xmargin;

	for (i = 0; i < sc->sc_font_height; i++) {
		for (j = 0, k = 7; j < 8; j++, k--) {
			if ((p[i] & (1 << k)) == 0)
				*(addr + j) =
				    efifb_pixel(sc, efifb_background(a));
			else
				*(addr + j) =
				    efifb_pixel(sc, efifb_foreground(a));
		}
		addr += (sc->sc_stride/4);
	}

	return (0);
}

static int
efifb_putc(video_adapter_t *adp, vm_offset_t off, uint8_t c, uint8_t a)
{
	struct efifb_softc *sc;

	sc = (struct efifb_softc *)adp;

	return ((*sc->sc_putc)(adp, off, c, a));
}

static int
efifb_puts(video_adapter_t *adp, vm_offset_t off, u_int16_t *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		efifb_putc(adp, off + i, s[i] & 0xff, (s[i] & 0xff00) >> 8);
	}
	return (0);
}

static int
efifb_putm8(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{
	struct efifb_softc *sc;
	int i, j, k;
	uint8_t *addr;
	u_char fg, bg;

	sc = (struct efifb_softc *)adp;
	addr = (u_int8_t *)((uintptr_t)sc->sc_addr
		+ (y + sc->sc_ymargin)*sc->sc_stride
		+ x + sc->sc_xmargin);

	fg = (u_char)(efifb_pixel(sc, efifb_foreground(SC_NORM_ATTR)) & 0xff);
	bg = (u_char)(efifb_pixel(sc, efifb_background(SC_NORM_ATTR)) & 0xff);

	for (i = 0; i < size && i+y < sc->sc_height - 2*sc->sc_ymargin; i++) {
		/*
		 * Calculate 2 x 4-chars at a time, and then
		 * write these out.
		 */
		for (j = 0, k = width; j < 8; j++, k--) {
			if (x + j >= sc->sc_width - 2*sc->sc_xmargin)
				continue;

			if (pixel_image[i] & (1 << k))
				addr[j] = (addr[j] == fg) ? bg : fg;
		}

		addr += (sc->sc_stride / sizeof(u_int8_t));
	}

	return (0);
}

static int
efifb_putm32(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{
	struct efifb_softc *sc;
	int i, j, k;
	uint32_t fg, bg;
	uint32_t *addr;

	sc = (struct efifb_softc *)adp;
	addr = (uint32_t *)sc->sc_addr
		+ (y + sc->sc_ymargin)*(sc->sc_stride/4)
		+ x + sc->sc_xmargin;

	fg = efifb_pixel(sc, efifb_foreground(SC_NORM_ATTR));
	bg = efifb_pixel(sc, efifb_background(SC_NORM_ATTR));

	for (i = 0; i < size && i+y < sc->sc_height - 2*sc->sc_ymargin; i++) {
		for (j = 0, k = width; j < 8; j++, k--) {
			if (x + j >= sc->sc_width - 2*sc->sc_xmargin)
				continue;

			if (pixel_image[i] & (1 << k))
				*(addr + j) = (*(addr + j) == fg) ? bg : fg;
		}
		addr += (sc->sc_stride/4);
	}

	return (0);
}

static int
efifb_putm(video_adapter_t *adp, int x, int y, uint8_t *pixel_image,
    uint32_t pixel_mask, int size, int width)
{
	struct efifb_softc *sc;

	sc = (struct efifb_softc *)adp;

	return ((*sc->sc_putm)(adp, x, y, pixel_image, pixel_mask, size,
	    width));
}

/*
 * Define the syscons nexus device attachment
 */
static void
efifb_scidentify(driver_t *driver, device_t parent)
{
	device_t child;

	child = BUS_ADD_CHILD(parent, INT_MAX, SC_DRIVER_NAME, 0);
}

static int
efifb_scprobe(device_t dev)
{
	int error;

	device_set_desc(dev, "EFI framebuffer console");

	error = sc_probe_unit(device_get_unit(dev), 
	    device_get_flags(dev) | SC_AUTODETECT_KBD);
	if (error != 0)
		return (error);

	/* This is a fake device, so make sure we added it ourselves */
	return (BUS_PROBE_NOWILDCARD);
}

static int
efifb_scattach(device_t dev)
{
	return (sc_attach_unit(device_get_unit(dev),
	    device_get_flags(dev) | SC_AUTODETECT_KBD));
}

static device_method_t efifb_sc_methods[] = {
  	DEVMETHOD(device_identify,	efifb_scidentify),
	DEVMETHOD(device_probe,		efifb_scprobe),
	DEVMETHOD(device_attach,	efifb_scattach),
	{ 0, 0 }
};

static driver_t efifb_sc_driver = {
	SC_DRIVER_NAME,
	efifb_sc_methods,
	sizeof(sc_softc_t),
};

static devclass_t	sc_devclass;

DRIVER_MODULE(efifb, nexus, efifb_sc_driver, sc_devclass, 0, 0);
