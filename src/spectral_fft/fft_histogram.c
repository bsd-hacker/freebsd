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
	int cur;

	for (i = 0; i < SPECTRAL_HT20_NUM_BINS; i++) {
		/* Calculate frequency of the given event */
		ffreq = (float) re->re_freq - 10.0 +
		    ((20.0 * i) / SPECTRAL_HT20_NUM_BINS);

		/* If it doesn't fit in the array, toss */
		fidx = freq2fidx((int) (ffreq * 1000.0));
		if (fidx < 0)
			continue;

		/* XXX until i figure out what's going on */
		if (fe->pri.bins[i].dBm == 0 || fe->pri.bins[i].dBm < -185) // || fe->pri.bins[i].dBm > -10)
			continue;

		/* Rolling/decaying average */
		cur = fdata.avg_pts_cur[fidx];
		if (fdata.avg_pts[fidx][cur] == 0) {
			fdata.avg_pts[fidx][cur] = fe->pri.bins[i].dBm;
		} else {
			fdata.avg_pts[fidx][cur] = (((fdata.avg_pts[fidx][cur] * 100) / 90) + fe->pri.bins[i].dBm) / 2;
		}
		fdata.avg_pts_cur[fidx] = (fdata.avg_pts_cur[fidx] + 1) % FFT_HISTOGRAM_HISTORY_DEPTH;

		/* Max */
		if (fdata.max_pts[fidx] == 0 || fe->pri.bins[i].dBm > fdata.max_pts[fidx]) {
			fdata.max_pts[fidx] = fe->pri.bins[i].dBm;
		} else {
			fdata.max_pts[fidx] = ((fdata.max_pts[fidx] * 975) + (fe->pri.bins[i].dBm * 25)) / 1000;
		}
	}
}

int16_t *
fft_fetch_freq_avg(int freqKhz)
{
	int fidx;

	fidx = freq2fidx(freqKhz);
	if (fidx < 0)
		return NULL;

	return fdata.avg_pts[fidx];
}

int16_t
fft_fetch_freq_max(int freqKhz)
{
	int fidx;

	fidx = freq2fidx(freqKhz);
	if (fidx < 0)
		return -180; /* XXX */

	return fdata.max_pts[fidx];
}
