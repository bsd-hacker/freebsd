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

struct fft_histogram *
fft_histogram_init(void)
{
	struct fft_histogram *fh;

	fh = calloc(1, sizeof(*fh));
	if (fh == NULL) {
		warn("%s: calloc", __func__);
		return (NULL);
	}

	return (fh);
}

static int
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
fft_add_sample(struct fft_histogram *fh, struct radar_entry *re,
    struct radar_fft_entry *fe)
{
	float ffreq_khz;
	int i;
	int fidx;
	int cur;
	float fwidth_khz;

	if (fe->num_bins == 0) {
		fprintf(stderr, "%s: invalid num_bins (0)\n", __func__);
		return;
	}
	//fprintf(stderr, "%s: yay, bins=%d, centre freq=%d\n", __func__, fe->num_bins, re->re_freq_centre);

	/*
	 * We know the bins are 315KHz (or 312.5KHz for non-fast mode,
	 * but whatever) wide, so let's calculate the true total bin
	 * width.
	 */
	fwidth_khz = (float) (fe->num_bins) * 312.5;

	for (i = 0; i < fe->num_bins; i++) {
		/* Calculate frequency of the given event */
		ffreq_khz = (((float) re->re_freq_centre) * 1000.0) - (fwidth_khz / 2.0) +
		    ((fwidth_khz * i) / (float) fe->num_bins);

		/* If it doesn't fit in the array, toss */
		fidx = freq2fidx((int) ffreq_khz);
		if (fidx < 0) {
			fprintf(stderr, "%s: tossed, ffreq_khz=%d\n", __func__, (int) ffreq_khz);
			continue;
		}

		/* XXX until i figure out what's going on */
		if (fe->bins[i].dBm == 0 || fe->bins[i].dBm < -185) { // || fe->bins[i].dBm > -10) {
//			fprintf(stderr, "%s: tossed; dbm=%d\n", __func__, fe->bins[i].dBm);
			continue;
		}

		/* Rolling/decaying average */
		cur = fh->d.avg_pts_cur[fidx];
		if (fh->d.avg_pts[fidx][cur] == 0) {
			fh->d.avg_pts[fidx][cur] = fe->bins[i].dBm;
		} else {
			fh->d.avg_pts[fidx][cur] = (((fh->d.avg_pts[fidx][cur] * 100) / 90) + fe->bins[i].dBm) / 2;
		}
		fh->d.avg_pts_cur[fidx] = (fh->d.avg_pts_cur[fidx] + 1) % FFT_HISTOGRAM_HISTORY_DEPTH;

		/* Max */
		if (fh->d.max_pts[fidx] == 0 || fe->bins[i].dBm > fh->d.max_pts[fidx]) {
			fh->d.max_pts[fidx] = fe->bins[i].dBm;
		} else {
			fh->d.max_pts[fidx] = ((fh->d.max_pts[fidx] * 990) + (fe->bins[i].dBm * 10)) / 1000;
		}
	}
}

int16_t *
fft_fetch_freq_avg(struct fft_histogram *fh, int freqKhz)
{
	int fidx;

	fidx = freq2fidx(freqKhz);
	if (fidx < 0)
		return NULL;

	return fh->d.avg_pts[fidx];
}

int16_t
fft_fetch_freq_max(struct fft_histogram *fh, int freqKhz)
{
	int fidx;

	fidx = freq2fidx(freqKhz);
	if (fidx < 0)
		return -180; /* XXX */

	return fh->d.max_pts[fidx];
}
