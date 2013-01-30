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
#include <err.h>
#include <pthread.h>
#include <unistd.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "fft_eval.h"

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"

#include "fft_eval.h"
#include "fft_freebsd.h"
#include "fft_histogram.h"
#include "fft_display.h"

/* 5 a second for now, the rendering is too inefficient otherwise? */
#define	RENDER_PERIOD_MSEC	200

#define	LCL_EVENT_RENDER	69

struct fft_app {
	pthread_mutex_t mtx_histogram;
	SDL_Surface *screen;
	SDL_TimerID rendering_timer_id;
	int g_do_update;
	TTF_Font *font;
	struct fft_display *fdisp;
	struct fft_histogram *fh;
	int highlight_freq;
	int startfreq;
	int accel;
	int scroll;
};

int graphics_init_sdl(struct fft_app *fap)
{
	SDL_VideoInfo *VideoInfo;
	int SDLFlags;

	SDLFlags = SDL_HWPALETTE | SDL_RESIZABLE;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Initializing SDL failed\n");
		return -1;
	}
		
	if ((VideoInfo = (SDL_VideoInfo *) SDL_GetVideoInfo()) == NULL) {
		fprintf(stderr, "Getting SDL Video Info failed\n");
		return -1;
	}

	else {
		if (VideoInfo->hw_available) {
			SDLFlags |= SDL_HWSURFACE;
		} else {
			SDLFlags |= SDL_SWSURFACE;
		}
		if (VideoInfo->blit_hw)
			SDLFlags |= SDL_HWACCEL;
	}

	SDL_WM_SetCaption("FFT eval", "FFT eval");
	fap->screen = SDL_SetVideoMode(WIDTH, HEIGHT, BPP, SDLFlags);

	if (TTF_Init() < 0) {
		fprintf(stderr, "Initializing SDL TTF failed\n");
		return -1;
	}

	fap->font = TTF_OpenFont("font/LiberationSans-Regular.ttf", 14);
	if (! fap->font) {
		fprintf(stderr, "Opening font failed\n");
		return -1;
	}

	return 0;
}

void graphics_quit_sdl(void)
{
	SDL_Quit();
}

static void
graphics_render(struct fft_app *fap)
{

//	printf("render: %d MHz\n", fap->startfreq);

	if (!fap->scroll) {
		/* move to highlighted object */
		if (fap->highlight_freq - 20 < fap->startfreq)
			fap->accel = -10;
		if (fap->highlight_freq > (fap->startfreq + WIDTH/X_SCALE))
			fap->accel = 10;
		
		/* if we are "far off", move a little bit faster */
		if (fap->highlight_freq + 300 < fap->startfreq)
			fap->accel = -100;

		if (fap->highlight_freq - 300 > (fap->startfreq + WIDTH/X_SCALE))
			fap->accel = 100;
	}

	if (fap->accel) {
		fap->startfreq += fap->accel;
		if (fap->accel > 0)
			fap->accel--;
		if (fap->accel < 0)
			fap->accel++;
	}

	/* Cap rendering offset */
	if (fap->startfreq < 2300)
		fap->startfreq = 2300;
	if (fap->startfreq > 6000)
		fap->startfreq = 6000;

	/* .. and now, render */
	fft_display_draw_picture(fap->fdisp, fap->highlight_freq,
	    fap->startfreq);

}

static uint32_t
graphics_draw_event(uint32_t interval, void *cbdata)
{
	struct fft_app *fap = cbdata;
	SDL_Event event;

	event.type = SDL_USEREVENT;
	event.user.code = LCL_EVENT_RENDER;
	event.user.data1 = 0;
	event.user.data2 = 0;

	SDL_PushEvent(&event);

	return (RENDER_PERIOD_MSEC);
}

/*
 * graphics_main - sets up the data and holds the mainloop.
 *
 */
void graphics_main(struct fft_app *fap)
{
	SDL_Event event;
	int quit = 0;

	while (!quit) {

		/* Wait for the next event */
		SDL_WaitEvent(&event);

		switch (event.type) {
		case SDL_USEREVENT:
			switch (event.user.code) {
				case LCL_EVENT_RENDER:
					graphics_render(fap);
					break;
				default:
					break;
			}
			break;
		case SDL_QUIT:
			quit = 1;
			printf("quit!\n");
			break;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_LEFT:
				fap->accel-= 2;
				fap->scroll = 1;
				break;
			case SDLK_RIGHT:
				fap->accel+= 2;
				fap->scroll = 1;
				break;
			case SDLK_HOME:
				fap->startfreq = 2300;
				fap->accel = 0;
				break;
			case SDLK_END:
				fap->startfreq = 5100;
				fap->accel = 0;
				break;
			default:
				break;
			}
			break;
		}

		/* Cap acceleration */
		if (fap->accel < -40)
			fap->accel = -40;
		if (fap->accel >  40)
			fap->accel = 40;
	}
}

void usage(int argc, char *argv[])
{
	fprintf(stderr, "Usage: %s [scanfile]\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "scanfile is generated by the spectral analyzer feature\n");
	fprintf(stderr, "of your wifi card. If you have a AR92xx or AR93xx based\n");
	fprintf(stderr, "card, try:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "ifconfig wlan0 up\n");
	fprintf(stderr, "iw dev wlan0 scan spec-scan\n");
	fprintf(stderr, "cat /sys/kernel/debug/ieee80211/phy0/ath9k/spectral_scan > /tmp/fft_results\n");
	fprintf(stderr, "%s /tmp/fft_results\n", argv[0]);
	fprintf(stderr, "\n");
	fprintf(stderr, "(NOTE: maybe debugfs must be mounted first: mount -t debugfs none /sys/kernel/debug/ )\n");
	fprintf(stderr, "\n");

}

static void
fft_eval_cb(struct radar_entry *re, void *cbdata)
{
	struct fft_app *fap = cbdata;
	struct radar_fft_entry *fe;
	int i;

	pthread_mutex_lock(&fap->mtx_histogram);
	for (i = 0; i < re->re_num_spectral_entries; i++) {
		fft_add_sample(fap->fh, re, &re->re_spectral_entries[i]);
	}
	fap->g_do_update = 1;
	pthread_mutex_unlock(&fap->mtx_histogram);

}

int main(int argc, char *argv[])
{
	int ret;
	struct fft_app *fap;

	if (argc < 2) {
		usage(argc, argv);
		return -1;
	}

	fprintf(stderr, "WARNING: Experimental Software! Don't trust anything you see. :)\n");
	fprintf(stderr, "\n");

	fap = calloc(1, sizeof(*fap));
	if (! fap) {
		errx(127, "calloc");
	}

	/* Setup rendering details */
	fap->startfreq = 2350;
	fap->highlight_freq = 2350;

	/* Setup histogram data */
	fap->fh = fft_histogram_init();
	if (! fap->fh)
		exit(127);

	/* Setup radar entry callback */
	pthread_mutex_init(&fap->mtx_histogram, NULL);
	set_scandata_callback(fft_eval_cb, fap);

	/* Setup graphics */
	if (graphics_init_sdl(fap) < 0) {
		fprintf(stderr, "Failed to initialize graphics.\n");
		exit(127);
	}
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	/* Setup fft display */
	fap->fdisp = fft_display_create(fap->screen, fap->font, fap->fh);
	if (fap->fdisp == NULL)
		exit(127);

	/* Fetch data */
	ret = read_scandata_freebsd(argv[1], NULL);
	if (ret < 0) {
		fprintf(stderr, "Couldn't read scanfile ...\n");
		usage(argc, argv);
		return -1;
	}

	/* Begin rendering timer */
	fap->rendering_timer_id = SDL_AddTimer(RENDER_PERIOD_MSEC,
	    graphics_draw_event, fap);

	graphics_main(fap);

	graphics_quit_sdl();

	return 0;
}
