/* 
 * Copyright (C) 2012 Simon Wunderlich <siwu@hrz.tu-chemnitz.de>
 * Copyright (C) 2012 Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * 
 */

/*
 * This program has been created to aid open source spectrum
 * analyzer development for Qualcomm/Atheros AR92xx and AR93xx
 * based chipsets.
 */

#include <errno.h>
#include <stdio.h>
#include <pcap.h>
#include <pthread.h>
#include <unistd.h>
#include <err.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "fft_eval.h"

/*
 * This is needed for fft_histogram, which currently
 * manipulates fft_radar_entry structs.
 */
#if 1
#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"
#endif

#include "fft_histogram.h"

#include "fft_display.h"

#define	RMASK 	0x000000ff
#define RBITS	0
#define	GMASK	0x0000ff00
#define GBITS	8
#define	BMASK	0x00ff0000
#define	BBITS	16
#define	AMASK	0xff000000

/* XXX ew globals */

static int
pixel(Uint32 *pixels, int x, int y, Uint32 color)
{
	if (x < 0 || x >= WIDTH)
		return -1;
	if (y < 0 || y >= HEIGHT)
		return -1;

	pixels[x + y * WIDTH] |= color;
	return 0;
}

#define SIZE 2

/* Is this pixel in the viewport? */
static int
is_in_viewport(int x, int y)
{
	if (x - SIZE < 0 || x + SIZE >= WIDTH)
		return 0;
	if (y - SIZE < 0 || y + SIZE >= HEIGHT)
		return 0;
	return (1);
}

/* this function blends a 2*SIZE x 2*SIZE blob at the given position with
 * the defined opacity. */
static int
bigpixel(Uint32 *pixels, int x, int y, Uint32 color, uint8_t opacity)
{
	int x1, y1;

	if (! is_in_viewport(x, y))
		return -1;

	for (x1 = x - SIZE; x1 < x + SIZE; x1++)
	for (y1 = y - SIZE; y1 < y + SIZE; y1++) {
		Uint32 r, g, b;

		r = ((pixels[x1 + y1 * WIDTH] & RMASK) >> RBITS) +
		    ((((color & RMASK) >> RBITS) * opacity) / 255);
		if (r > 255) r = 255;

		g = ((pixels[x1 + y1 * WIDTH] & GMASK) >> GBITS) +
		    ((((color & GMASK) >> GBITS) * opacity) / 255);
		if (g > 255) g = 255;

		b = ((pixels[x1 + y1 * WIDTH] & BMASK) >> BBITS) +
		    ((((color & BMASK) >> BBITS) * opacity) / 255);
		if (b > 255) b = 255;

		pixels[x1 + y1 * WIDTH] = r << RBITS |
		    g << GBITS | b << BBITS | (color & AMASK);
	}
	return 0;
}

/*
 * Like bigpixel, but blending blue (the "data average" colour) to
 * green if the blue is saturated.  A total hack, but a pretty one.
 */
static int
bighotpixel(Uint32 *pixels, int x, int y, Uint32 color, uint8_t opacity)
{
	int x1, y1;

	if (! is_in_viewport(x, y))
		return -1;

	for (x1 = x - SIZE; x1 < x + SIZE; x1++)
	for (y1 = y - SIZE; y1 < y + SIZE; y1++) {
		Uint32 r, g, b;

		r = ((pixels[x1 + y1 * WIDTH] & RMASK) >> RBITS) +
		    ((((color & RMASK) >> RBITS) * opacity) / 255);
		if (r > 255) r = 255;

		g = ((pixels[x1 + y1 * WIDTH] & GMASK) >> GBITS) +
		    ((((color & GMASK) >> GBITS) * opacity) / 255);
		if (g > 255) g = 255;

		/* If we've capped out blue, increment red */
		b = ((pixels[x1 + y1 * WIDTH] & BMASK) >> BBITS);
		if (b > 248) {
			/* green bumped by bluemask */
			g = ((pixels[x1 + y1 * WIDTH] & GMASK) >> GBITS) +
			    ((((color & BMASK) >> BBITS) * (opacity/2)) / 255);
			if (g > 255) g = 255;
		} else {
			b = ((pixels[x1 + y1 * WIDTH] & BMASK) >> BBITS) +
			    ((((color & BMASK) >> BBITS) * opacity) / 255);
			if (b > 255) b = 255;
		}

		pixels[x1 + y1 * WIDTH] = r << RBITS |
		    g << GBITS | b << BBITS | (color & AMASK);
	}
	return 0;
}

static int
render_text(struct fft_display *fdisp, SDL_Surface *surface,
    char *text, int x, int y)
{
	SDL_Surface *text_surface;
	SDL_Color fontcolor = {255, 255, 255, 255};
	SDL_Rect fontdest = {0, 0, 0, 0};

	fontdest.x = x;
	fontdest.y = y;

	text_surface = TTF_RenderText_Solid(fdisp->font, text, fontcolor);
	if (!text_surface)
		return -1;

	SDL_BlitSurface(text_surface, NULL, surface, &fontdest);
	SDL_FreeSurface(text_surface);

	return 0;
}

struct fft_display *
fft_display_create(SDL_Surface *screen, TTF_Font *font)
{
	struct fft_display *fdisp;

	fdisp = calloc(1, sizeof(*fdisp));
	if (fdisp == NULL) {
		warn("%s: malloc", __func__);
		return (NULL);
	}
	fdisp->screen = screen;
	fdisp->font = font;
	return (fdisp);
}

void
fft_display_destroy(struct fft_display *fdisp)
{

	free(fdisp);
}

/*
 * draw_picture - draws the current screen.
 *
 * @highlight: the index of the dataset to be highlighted
 *
 * returns the center frequency of the currently highlighted dataset
 */
int
fft_display_draw_picture(struct fft_display *fdisp, int highlight,
    int startfreq)
{
	Uint32 *pixels, color, opacity;
	int x, y, i, rnum, j;
	int highlight_freq = startfreq + 20;
	char text[1024];
	struct scanresult *result;
	SDL_Surface *surface;

	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, WIDTH, HEIGHT, BPP, RMASK, GMASK, BMASK, AMASK);
	pixels = (Uint32 *) surface->pixels;
	for (y = 0; y < HEIGHT; y++)
		for (x = 0; x < WIDTH; x++)
			pixels[x + y * WIDTH] = AMASK;

	/* vertical lines (frequency) */
	for (i = 2300; i < 6000; i += 20) {
		x = (X_SCALE * (i - startfreq));

		if (x < 0 || x > WIDTH)
			continue;

		for (y = 0; y < HEIGHT - 20; y++)
			pixels[x + y * WIDTH] = 0x40404040 | AMASK;

		snprintf(text, sizeof(text), "%d MHz", i);
		render_text(fdisp, surface, text, x - 30, HEIGHT - 20);
	}

	/* horizontal lines (dBm) */
	for (i = 0; i < 150; i += 10) {
		y = 600 - Y_SCALE * i;
		
		for (x = 0; x < WIDTH; x++)
			pixels[x + y * WIDTH] = 0x40404040 | AMASK;

		snprintf(text, sizeof(text), "-%d dBm", (150 - i));
		render_text(fdisp, surface, text, 5, y - 15);
	}

	/* Render 2300 -> 6000 in 1MHz increments, but using KHz math */
	/* .. as right now the canvas is .. quite large. */
	/* XXX should just do it based on the current viewport! */
	for (i = 2300*1000; i < 6000*1000; i+= 250) {
		float signal;
		int freqKhz = i;
		int16_t *s;

		x = X_SCALE * (freqKhz - (startfreq * 1000)) / 1000;

		if (x < 0 || x > WIDTH)
			continue;

		/* Fetch dBm value at the given frequency in KHz */
		s = fft_fetch_freq_avg(freqKhz);
		if (s == NULL)
			continue;

		for (j = 0; j < FFT_HISTOGRAM_HISTORY_DEPTH; j++) {
			if (s[j] == 0)
				continue;
			signal = (float) s[j];
			color = BMASK | AMASK;
			opacity = 64;
			y = 400 - (400.0 + Y_SCALE * signal);
			if (bighotpixel(pixels, x, y, color, opacity) < 0)
				continue;
		}


		/* .. and the max */
		signal = (float) fft_fetch_freq_max(freqKhz);
		color = RMASK | AMASK;
		opacity = 128;
		y = 400 - (400.0 + Y_SCALE * signal);
		if (bigpixel(pixels, x, y, color, opacity) < 0)
			continue;
	}

	SDL_BlitSurface(surface, NULL, fdisp->screen, NULL);
	SDL_FreeSurface(surface);
	SDL_Flip(fdisp->screen);

	return highlight_freq;
}
