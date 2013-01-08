#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <netinet/in.h>	/* for ntohl etc */

#include <pcap.h>

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"

#include "fft_eval.h"
#include "fft_freebsd.h"

#include "fft_histogram.h"

struct fft_histogram_data fdata;

/* XXX ew */
#define SPECTRAL_HT20_NUM_BINS          56

void
fft_histogram_init(void)
{
	bzero(&fdata, sizeof(fdata));
}

int
freq2fidx(int freqKhz)
{
	int freqMhz = freqKhz / 1000;
	int fidx;

	if (freqMhz < FFT_HISTOGRAM_START_FREQ ||
	    freqMhz >= FFT_HISTOGRAM_END_FREQ) {
		return (-1);
	}

	/* Calculate index */
	fidx = (freqKhz - FFT_HISTOGRAM_START_FREQ * 1000)
	    / (1000 / FFT_HISTOGRAM_RESOLUTION);

	if (fidx < 0 || fidx >= FFT_HISTOGRAM_SIZE) {
		return (-1);
	}

	return (fidx);
}

void
fft_add_sample(struct radar_entry *re, struct radar_fft_entry *fe)
{
	float ffreq;
	int i;
	int fidx;

	for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
		/* Calculate frequency of the given event */
		ffreq = (float) re->re_freq - 10.0 +
		    ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);

		/* If it doesn't fit in the array, toss */
		fidx = freq2fidx((int) (ffreq * 1000.0));
		if (fidx < 0)
			continue;

		/* XXX until i figure out what's going on */
		if (fe->pri.bins[i].dBm == 0 || fe->pri.bins[i].dBm < -185)
			continue;

		/* Store the current dBm value */
		fdata.pts[fidx] = fe->pri.bins[i].dBm;
	}
}

int
fft_fetch_freq(int freqKhz)
{
	int fidx;

	fidx = freq2fidx(freqKhz);
	if (fidx < 0)
		return -150; /* XXX */

#if 0
	printf("%s: khz=%d, fidx=%d, val=%d\n",
	    __func__,
	    freqKhz,
	    fidx,
	    fdata.pts[fidx]);
#endif

	return fdata.pts[fidx];
}
