#ifndef	__FFT_DISPLAY_H__
#define	__FFT_DISPLAY_H__

#define	WIDTH	1600
#define	HEIGHT	650
#define	BPP	32

#define	X_SCALE	5
#define	Y_SCALE	4

struct fft_display {
	SDL_Surface *screen;
	TTF_Font *font;
};

extern	struct fft_display * fft_display_create(SDL_Surface *screen,
	    TTF_Font *font);
extern	void fft_display_destroy(struct fft_display *disp);
extern	int fft_display_draw_picture(struct fft_display *fdisp,
	    int highlight, int startfreq);

#endif	/* __FFT_DISPLAY_H__ */
