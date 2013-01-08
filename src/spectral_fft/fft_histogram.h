#ifndef	__FFT_HISTOGRAM_H__
#define	__FFT_HISTOGRAM_H__

#define	FFT_HISTOGRAM_START_FREQ	2300
#define	FFT_HISTOGRAM_END_FREQ		6000

#define	FFT_HISTOGRAM_RESOLUTION	4	/* 250Khz increments */

#define	FFT_HISTOGRAM_SIZE	\
	    ((6000-2300)*FFT_HISTOGRAM_RESOLUTION)

struct fft_histogram_data {
	int	avg_pts[FFT_HISTOGRAM_SIZE];
	int	max_pts[FFT_HISTOGRAM_SIZE];
};

extern	void fft_histogram_init(void);
extern	void fft_add_sample(struct radar_entry *re,
	    struct radar_fft_entry *fe);
extern int fft_fetch_freq_avg(int freqKhz);
extern int fft_fetch_freq_max(int freqKhz);

#endif
