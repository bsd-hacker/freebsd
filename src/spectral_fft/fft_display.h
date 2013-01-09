#ifndef	__FFT_DISPLAY_H__
#define	__FFT_DISPLAY_H__

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
