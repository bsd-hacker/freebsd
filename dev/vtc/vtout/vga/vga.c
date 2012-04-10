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
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/vtc/vtc.h>
#include <dev/vtc/vtc_vtout.h>

#include <dev/ic/vga.h>
#include <dev/vtc/vtout/vga/vga.h>

/* Convenience macros. */
#define	MEM_READ(sc, ofs)		\
	bus_space_read_1(sc->vga_fb.bst, sc->vga_fb.bsh, ofs)
#define	MEM_WRITE(sc, ofs, val)		\
	bus_space_write_1(sc->vga_fb.bst, sc->vga_fb.bsh, ofs, val)
#define	REG_READ(sc, reg)		\
	bus_space_read_1(sc->vga_reg.bst, sc->vga_reg.bsh, reg)
#define	REG_WRITE(sc, reg, val)		\
	bus_space_write_1(sc->vga_reg.bst, sc->vga_reg.bsh, reg, val)

struct vga_softc vga_console;
devclass_t vga_devclass;
char vga_device_name[] = "vga";

int
vga_probe(struct vga_softc *sc)
{

	return (1);
}

int
vga_init(struct vga_softc *sc)
{
	u_int ofs;
	uint8_t x;

	/* Make sure the VGA adapter is not in monochrome emulation mode. */
	x = REG_READ(sc, VGA_GEN_MISC_OUTPUT_R);
	REG_WRITE(sc, VGA_GEN_MISC_OUTPUT_W, x | VGA_GEN_MO_IOA);

	/* Unprotect CRTC registers 0-7. */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_END);
	x = REG_READ(sc, VGA_CRTC_DATA);
	REG_WRITE(sc, VGA_CRTC_DATA, x & ~VGA_CRTC_VRE_PR);

	/*
	 * Wait for the vertical retrace.
	 * NOTE: this code reads the VGA_GEN_INPUT_STAT_1 register, which has
	 * the side-effect of clearing the internal flip-flip of the attribute
	 * controller's write register. This means that because this code is
	 * here, we know for sure that the first write to the attribute
	 * controller will be a write to the address register. Removing this
	 * code therefore also removes that guarantee and appropriate measures
	 * need to be taken.
	 */
	do {
		x = REG_READ(sc, VGA_GEN_INPUT_STAT_1);
		x &= VGA_GEN_IS1_VR | VGA_GEN_IS1_DE;
	} while (x != (VGA_GEN_IS1_VR | VGA_GEN_IS1_DE));

	/* Now, disable the sync. signals. */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	x = REG_READ(sc, VGA_CRTC_DATA);
	REG_WRITE(sc, VGA_CRTC_DATA, x & ~VGA_CRTC_MC_HR);

	/*
	 * Part 1: Reprogram the overall operating mode.
	 */

	/* Asynchronous sequencer reset. */
	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_RESET);
	REG_WRITE(sc, VGA_SEQ_DATA, VGA_SEQ_RST_SR);
	/* Clock select. */
	REG_WRITE(sc, VGA_GEN_MISC_OUTPUT_W, VGA_GEN_MO_VSP | VGA_GEN_MO_HSP |
	    VGA_GEN_MO_PB | VGA_GEN_MO_ER | VGA_GEN_MO_IOA);
	/* Set sequencer clocking and memory mode. */
	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_CLOCKING_MODE);
	REG_WRITE(sc, VGA_SEQ_DATA, VGA_SEQ_CM_89);
	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_MEMORY_MODE);
	REG_WRITE(sc, VGA_SEQ_DATA, VGA_SEQ_MM_OE | VGA_SEQ_MM_EM);
	/* Set the graphics controller in graphics mode. */
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MISCELLANEOUS);
	REG_WRITE(sc, VGA_GC_DATA, 0x04 + VGA_GC_MISC_GA);
	/* Set the attribute controller in graphics mode. */
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_MODE_CONTROL);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_MC_GA);
	/* Program the CRT controller. */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_HORIZ_TOTAL);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x5f);			/* 760 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_HORIZ_DISP_END);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x4f);			/* 640 - 8 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_HORIZ_BLANK);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x50);			/* 640 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_HORIZ_BLANK);
	REG_WRITE(sc, VGA_CRTC_DATA, VGA_CRTC_EHB_CR + 2);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_HORIZ_RETRACE);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x54);			/* 672 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_HORIZ_RETRACE);
	REG_WRITE(sc, VGA_CRTC_DATA, VGA_CRTC_EHR_EHB + 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_TOTAL);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x0b);			/* 523 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_OVERFLOW);
	REG_WRITE(sc, VGA_CRTC_DATA, VGA_CRTC_OF_VT9 | VGA_CRTC_OF_LC8 |
	    VGA_CRTC_OF_VBS8 | VGA_CRTC_OF_VRS8 | VGA_CRTC_OF_VDE8);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MAX_SCAN_LINE);
	REG_WRITE(sc, VGA_CRTC_DATA, VGA_CRTC_MSL_LC9);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_START);
	REG_WRITE(sc, VGA_CRTC_DATA, 0xea);			/* 480 + 10 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_RETRACE_END);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x0c);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_VERT_DISPLAY_END);
	REG_WRITE(sc, VGA_CRTC_DATA, 0xdf);			/* 480 - 1*/
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_OFFSET);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x28);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_VERT_BLANK);
	REG_WRITE(sc, VGA_CRTC_DATA, 0xe7);			/* 480 + 7 */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_END_VERT_BLANK);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x04);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	REG_WRITE(sc, VGA_CRTC_DATA, VGA_CRTC_MC_WB | VGA_CRTC_MC_AW |
	    VGA_CRTC_MC_SRS | VGA_CRTC_MC_CMS);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_LINE_COMPARE);
	REG_WRITE(sc, VGA_CRTC_DATA, 0xff);			/* 480 + 31 */
	/* Re-enable the sequencer. */
	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_RESET);
	REG_WRITE(sc, VGA_SEQ_DATA, VGA_SEQ_RST_SR | VGA_SEQ_RST_NAR);

	/*
	 * Part 2: Reprogram the remaining registers.
	 */
	REG_WRITE(sc, VGA_GEN_FEATURE_CTRL_W, 0);

	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_MAP_MASK);
	REG_WRITE(sc, VGA_SEQ_DATA, VGA_SEQ_MM_EM3 | VGA_SEQ_MM_EM2 |
	    VGA_SEQ_MM_EM1 | VGA_SEQ_MM_EM0);
	REG_WRITE(sc, VGA_SEQ_ADDRESS, VGA_SEQ_CHAR_MAP_SELECT);
	REG_WRITE(sc, VGA_SEQ_DATA, 0);

	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_PRESET_ROW_SCAN);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_START);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_END);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_ADDR_HIGH);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_START_ADDR_LOW);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_LOC_HIGH);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_CURSOR_LOC_LOW);
	REG_WRITE(sc, VGA_CRTC_DATA, 0x59);
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_UNDERLINE_LOC);
	REG_WRITE(sc, VGA_CRTC_DATA, 0);

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_COLOR_COMPARE);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_DATA_ROTATE);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_READ_MAP_SELECT);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_COLOR_DONT_CARE);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_BIT_MASK);
	REG_WRITE(sc, VGA_GC_DATA, 0xff);

	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(0));
	REG_WRITE(sc, VGA_AC_WRITE, 0);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(1));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(2));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_G);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(3));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(4));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_R);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(5));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_R | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(6));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SG | VGA_AC_PAL_R);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(7));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_R | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(8));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(9));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(10));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_G);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(11));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(12));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(13));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(14));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_G);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PALETTE(15));
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_PAL_SR | VGA_AC_PAL_SG |
	    VGA_AC_PAL_SB | VGA_AC_PAL_R | VGA_AC_PAL_G | VGA_AC_PAL_B);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_OVERSCAN_COLOR);
	REG_WRITE(sc, VGA_AC_WRITE, 0);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_COLOR_PLANE_ENABLE);
	REG_WRITE(sc, VGA_AC_WRITE, 0x0f);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_HORIZ_PIXEL_PANNING);
	REG_WRITE(sc, VGA_AC_WRITE, 0);
	REG_WRITE(sc, VGA_AC_WRITE, VGA_AC_COLOR_SELECT);
	REG_WRITE(sc, VGA_AC_WRITE, 0);

	/*
	 * Done. Clear the frame buffer. All bit planes are enabled, so
	 * a single-paged loop should clear all planes.
	 */
	for (ofs = 0; ofs < 38400; ofs++) {
		MEM_READ(sc, ofs);
		MEM_WRITE(sc, ofs, 0);
	}

	/* Re-enable the sync signals. */
	REG_WRITE(sc, VGA_CRTC_ADDRESS, VGA_CRTC_MODE_CONTROL);
	x = REG_READ(sc, VGA_CRTC_DATA);
	REG_WRITE(sc, VGA_CRTC_DATA, x | VGA_CRTC_MC_HR);

	sc->vga_enable = 1;
	return (0);
}

static __inline int
vga_bitblt_ctofb(struct vga_softc *sc, u_long c, u_long dst, int width,
    int height)
{
	int w;

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, c & 0x0f);
	dst >>= 3;
	while (height > 0) {
		for (w = 0; w < width; w += 8) {
			MEM_READ(sc, dst);
			MEM_WRITE(sc, dst++, 0);
		}
		dst += (640 - w) >> 3;
		height--;
	}
	return (0);
}

static __inline int
vga_bitblt_fbtofb(struct vga_softc *sc, u_long src, u_long dst, int width,
    int height)
{
	int w;

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE(sc, VGA_GC_DATA, 1);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	dst >>= 3;
	src >>= 3;
	while (height > 0) {
		for (w = 0; w < width; w += 8) {
			MEM_READ(sc, src++);
			MEM_WRITE(sc, dst++, 0);
		}
		src += (640 - w) >> 3;
		dst += (640 - w) >> 3;
		height--;
	}
	return (0);
}

static __inline int
vga_bitblt_h1tofb(struct vga_softc *sc, uint8_t *src, u_long dst, int width,
    int height, int bgclr, int fgclr)
{
	int c, w;
	uint8_t b;

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE(sc, VGA_GC_DATA, 3);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, fgclr);
	c = fgclr;
	dst >>= 3;
	while (height > 0) {
		for (w = 0; w < width; w += 8) {
			b = *src++;
			if (b != 0) {
				if (c != fgclr) {
					REG_WRITE(sc, VGA_GC_ADDRESS,
					    VGA_GC_SET_RESET);
					REG_WRITE(sc, VGA_GC_DATA, fgclr);
					c = fgclr;
				}
				MEM_READ(sc, dst);
				MEM_WRITE(sc, dst, b);
			}
			if (b != 0xff) {
				if (c != bgclr) {
					REG_WRITE(sc, VGA_GC_ADDRESS,
					    VGA_GC_SET_RESET);
					REG_WRITE(sc, VGA_GC_DATA, bgclr);
					c = bgclr;
				}
				MEM_READ(sc, dst);
				MEM_WRITE(sc, dst, ~b);
			}
			dst++;
		}
		dst += (640 - w) >> 3;
		height--;
	}
	return (0);
}

static __inline int
vga_bitblt_h4tofb(struct vga_softc *sc, uint8_t *src, u_long dst, int width,
    int height)
{
	u_long dstini;
	int rotini, w;
	uint8_t mask;

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_MODE);
	REG_WRITE(sc, VGA_GC_DATA, 0);
	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_ENABLE_SET_RESET);
	REG_WRITE(sc, VGA_GC_DATA, 0x0f);

	rotini = dst & 7;
	dstini = dst >> 3;

	while (height > 0) {
		dst = dstini;
		mask = 1 << (7 - rotini);
		for (w = 0; w < width; w += 2) {
			REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
			REG_WRITE(sc, VGA_GC_DATA, *src >> 4);
			REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_BIT_MASK);
			REG_WRITE(sc, VGA_GC_DATA, mask);
			MEM_READ(sc, dst);
			MEM_WRITE(sc, dst, 0);
			mask >>= 1;
			if (mask == 0) {
				dst++;
				mask = 0x80;
			}
			REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_SET_RESET);
			REG_WRITE(sc, VGA_GC_DATA, *src & 0x0f);
			REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_BIT_MASK);
			REG_WRITE(sc, VGA_GC_DATA, mask);
			MEM_READ(sc, dst);
			MEM_WRITE(sc, dst, 0);
			mask >>= 1;
			if (mask == 0) {
				dst++;
				mask = 0x80;
			}
			src++;
		}
		dstini += 80;
		height--;
	}

	REG_WRITE(sc, VGA_GC_ADDRESS, VGA_GC_BIT_MASK);
	REG_WRITE(sc, VGA_GC_DATA, 0xff);
	return (0);
}

int
vga_vbitblt(struct vga_softc *sc, int op, uintptr_t src, uintptr_t dst,
    int width, int height, va_list ap)
{
	int bgclr, fgclr;
	int error;

	switch (op) {
	case BITBLT_FBTOFB:
		error = vga_bitblt_fbtofb(sc, src, dst, width, height);
		break;
	case BITBLT_H1TOFB:
		bgclr = va_arg(ap, int);
		fgclr = va_arg(ap, int);
		error = vga_bitblt_h1tofb(sc, (uint8_t *)src, dst, width,
		    height, bgclr, fgclr);
		break;
	case BITBLT_CTOFB:
		error = vga_bitblt_ctofb(sc, src, dst, width, height);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
vga_clear(device_t dev)
{
	struct vga_softc *sc;

	sc = device_get_softc(dev);

	return (vga_bitblt_ctofb(sc, 0, 0, 640, 480));
}

static int
vga_bitblt(device_t dev, int op, uintptr_t src, uintptr_t dst, int width,
    int height, ...)
{
	struct vga_softc *sc;
	va_list ap;
	int error;

	sc = device_get_softc(dev);

	va_start(ap, height);
	error = vga_vbitblt(sc, op, src, dst, width, height, ap);
	va_end(ap);
	return (error);
}

int
vga_attach(device_t dev)
{
	struct vga_softc *sc;
	uintptr_t ofs;
	int error;

	sc = device_get_softc(dev);

	if (!sc->vga_console) {
		error = vga_init(sc);
		if (error)
			return (error);

		ofs = 640 * (480 - vtc_logo4_height) / 2 +
		    (640 - vtc_logo4_width) / 2;
		vga_bitblt_h4tofb(sc, vtc_logo4_image, ofs, vtc_logo4_width,
		    vtc_logo4_height);
	}

	return (vtc_vtout_attach(dev, vga_clear, vga_bitblt, 640, 480));
}
