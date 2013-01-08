/*-
 * Copyright (c) 2012 Adrian Chadd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/endian.h>
#include <string.h>	/* for memcpy() */
#include <math.h>

#include <sys/socket.h>
#include <net/if.h>

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "dev/ath/if_athioctl.h"

#include "pkt.h"
#include "ar9280_radar.h"

/* Decode the channel */
#include "chan.h"

/* from _ieee80211.h */
#define	IEEE80211_CHAN_HT40U	0x00020000 /* HT 40 channel w/ ext above */
#define	IEEE80211_CHAN_HT40D	0x00040000 /* HT 40 channel w/ ext below */

/* Relevant on Merlin and later */
#define	CH_SPECTRAL_EVENT		0x10
/* Relevant for Sowl and later */
#define	EXT_CH_RADAR_EARLY_FOUND	0x04
#define	EXT_CH_RADAR_FOUND	0x02
#define	PRI_CH_RADAR_FOUND	0x01

#define	AR9280_SPECTRAL_SAMPLE_SIZE_HT20	60
#define	AR9280_SPECTRAL_SAMPLE_SIZE_HT40	135

#define	NUM_SPECTRAL_ENTRIES_HT20		56

/*
 * GPLed snippet from Zefir on the linux-wireless list; rewrite this
 * soon!
 */
#if 1
/*
 * In my system the post-processed FFT raw data is transferred via a netlink
 * interface to a spectral_proxy, that forwards it to a connected host for real-time
 * inspection and visualization.
 *
 * The interpretation of the data is as follows: the reported values are given as
 * magnitudes, which need to be scaled and converted to absolute power values based
 * on the packets noise floor and RSSI values as follows:
 * bin_sum = 10*log(sum[i=1..56](b(i)^2)
 * power(i) = noise_floor + RSSI + 10*log(b(i)^2) - bin_sum
 *
 * The code fragment to convert magnitude to absolute power values looks like this
 * (assuming you transferred the FFT and magnitude data to user space):
 */
int
convert_data_ht20(struct radar_entry *re, struct radar_fft_entry *fe)
{
	int dc_pwr_idx = NUM_SPECTRAL_ENTRIES_HT20 / 2;
	int pwr_count = NUM_SPECTRAL_ENTRIES_HT20;
	int i;
	int nf0 = -96;	/* XXX populate re with this first! */
	float bsum = 0.0;

	/* DC value is invalid -> interpolate */
	fe->pri.bins[dc_pwr_idx].raw_mag =
	    (fe->pri.bins[dc_pwr_idx - 1].raw_mag + fe->pri.bins[dc_pwr_idx + 1].raw_mag) / 2;
	/* XXX adj mag? */
	fe->pri.bins[dc_pwr_idx].adj_mag =
	    fe->pri.bins[dc_pwr_idx].raw_mag << fe->max_exp;

	/* Calculate the total power - use pre-adjusted magnitudes */
	for (i = 0; i < pwr_count; i++)
		bsum += (float) (fe->pri.bins[i].adj_mag) * (float) (fe->pri.bins[i].adj_mag);
	bsum = log10f(bsum) * 10.0;

	/*
	 * Given the current NF/RSSI value, calculate an absolute dBm, then
	 * break each part up into its consitutent component.
	 */
	for (i = 0; i < pwr_count; i++) {
		float pwr_val;
		int16_t val = fe->pri.bins[i].adj_mag;

		if (val == 0)
			val = 1;

		pwr_val = 20.0 * log10f((float) val);
		pwr_val += (float) nf0 + (float) re->re_rssi - bsum;

		fe->pri.bins[i].dBm = pwr_val;
#if 0
		printf("  [%d] %d -> %d, ", i, fe->pri.bins[i].adj_mag,
		    fe->pri.bins[i].dBm);
#endif
	}
//	printf("\n");
	return (1);
}
#endif

static int
ar9280_radar_spectral_print(struct radar_fft_entry *fe)
{
	int i;
	printf("PRI:  max index=%d, magnitude=%d, bitmap weight=%d, max_exp=%d\n",
	    fe->pri.max_index,
	    fe->pri.max_magnitude,
	    fe->pri.bitmap_weight,
	    fe->max_exp);

	for (i = 0; i < 56; i++) {
		if (i % 8 == 0)
		    printf("PRI: %d:", i);
		printf("%02x ", fe->pri.bins[i].raw_mag);
		if (i % 8 == 7)
		    printf("\n");
	}
	printf("\n");
}

/* XXX why do we need this? */
static int8_t
fix_max_index(uint8_t max_index)
{
       int8_t maxindex = max_index;
       if (max_index > 32)
               maxindex |= 0xe0;
       else
               maxindex &= ~0xe0;
       maxindex += 29;
       return maxindex;
}

static int
ar9280_radar_spectral_decode_ht20(struct ieee80211_radiotap_header *rh,
    const unsigned char *pkt, int len, struct radar_entry *re,
    int cur_sample)
{
	int i;
	struct radar_fft_entry *fe;

	if (len < AR9280_SPECTRAL_SAMPLE_SIZE_HT20) {
		return (-1);
	}

	fe = &re->re_spectral_entries[cur_sample];

	/* Decode the bitmap weight, magnitude, max index */

	fe->pri.max_magnitude =
	    (pkt[57] << 2) |
	    ((pkt[56] & 0xc0) >> 6) |
	    ((pkt[58] & 0x03) << 10);
	fe->pri.bitmap_weight = pkt[56] & 0x3f;
	fe->pri.max_index = (pkt[58] & 0x3f);
	fe->max_exp = pkt[59] & 0x0f;

	/* Decode each bin - the dBm calculation will come later */
	for (i = 0; i < 56; i++) {
		fe->pri.bins[i].raw_mag = pkt[i];
		fe->pri.bins[i].adj_mag = fe->pri.bins[i].raw_mag << fe->max_exp;
	}

	/* Convert to dBm */
	(void) convert_data_ht20(re, fe);

	/* Return OK */
	return (0);
}

/*
 * Decode a spectral scan frame, complete with whatever
 * hilarity / bugs ensue.
 *
 * The known quirks:
 *
 * + The MAC may corrupt a frame - inserting, deleting
 *   and/or realigning things.
 * + The FFT data can terminate at any time; there's no
 *   guarantee that we'll get a complete frame.
 *
 * Let's not handle these for now; we'll just tinker with this
 * in the future.
 */
static int
ar9280_radar_spectral_decode(struct ieee80211_radiotap_header *rh,
    const unsigned char *pkt, int len, struct radar_entry *re)
{
	int i;
	const unsigned char *fr = pkt;
	int fr_len = len;

	
	for (i = 0; i < MAX_SPECTRAL_SCAN_SAMPLES_PER_PKT; i++) {
		/* HT20 or HT40? */
		/* XXX hard-code HT20 */
		if (ar9280_radar_spectral_decode_ht20(rh, fr, fr_len, re, i) != 0) {
			break;
		}
//		ar9280_radar_spectral_print(&re->re_spectral_entries[i]);
		fr_len -= AR9280_SPECTRAL_SAMPLE_SIZE_HT20;
		fr += AR9280_SPECTRAL_SAMPLE_SIZE_HT20;
		if (fr_len < 0)
			break;
	}

//	printf("  Spectral: %d samples\n", i);
	re->re_num_spectral_entries = i;

	return (0);
}

/*
 * Decode a normal radar frame.
 */
int
ar9280_radar_decode(struct ieee80211_radiotap_header *rh,
    const unsigned char *pkt, int len, struct radar_entry *re)
{
	uint64_t tsf;
	int8_t comb_rssi, pri_rssi, ext_rssi, nf;
	struct ath_rx_radiotap_header *rx =
	    (struct ath_rx_radiotap_header *) rh;
	struct xchan x;

	/* XXX we should really be implementing a real radiotap parser */
	tsf = le64toh(rx->wr_tsf);

	/*
	 * XXX which rssi should we use?
	 * XXX ext rssi?
	 */
	comb_rssi = rx->wr_v.vh_rssi;	/* Combined RSSI */
	pri_rssi = rx->wr_v.rssi_ctl[0];
	ext_rssi = rx->wr_v.rssi_ext[0];
	nf = rx->wr_antnoise;

	/* Last three bytes are the radar parameters */
	if (len < 3) {
		printf("short radar frame\n");
		return (0);
	}

	/*
	 * XXX TODO: there's lots of other things that need to be
	 * done with the RSSI and pulse durations.  It's quite likely
	 * that the pkt format should just have all of those
	 * (pri/ext/comb RSSI, flags, pri/ext pulse duration) and then
	 * the "decided" values to match the logic in the current
	 * HAL/DFS code, so they can all be plotted as appropriate.
	 */

#if 0
	printf("tsf: %lld", tsf);
	printf(" len: %d", len);
	printf(" rssi %d/%d", comb_rssi, nf);
	printf(", pri/ext rssi: %d/%d", pri_rssi, ext_rssi);
	printf(" pri: %u", pkt[len - 3] & 0xff);
	printf(" ext: %u", pkt[len - 2] & 0xff);
	printf(" flags: %s %s %s %s\n",
	    pkt[len - 1] & PRI_CH_RADAR_FOUND ? "pri" : "",
	    pkt[len - 1] & EXT_CH_RADAR_FOUND ? "ext" : "",
	    pkt[len - 1] & EXT_CH_RADAR_EARLY_FOUND ? "extearly" : "",
	    pkt[len - 1] & CH_SPECTRAL_EVENT ? "spectral" : ""
	    );
#endif

	/*
	 * XXX TODO:
	 *
	 * The radar event is timestamped by the MAC the end of the event.
	 * To work around this particular issue, a "best guess" of the event
	 * start time involves its duration.
	 *
	 * For the AR5416 we can fake this as we know that in 5GHz mode
	 * the MAC clock is 40MHz, so we can just convert the duration to
	 * a microsecond value and subtract that from the TSF.
	 * This also holds true for the AR9130/AR9160 in 5GHz mode.
	 *
	 * However, for the AR9280, 5GHz operation may be in "fast clock"
	 * mode, where the duration is actually not 0.8uS, but slightly
	 * smaller.
	 *
	 * Since there's currently no way to record this information in
	 * the vendor radiotap header (but there should be, hint hint)
	 * should have a flags field somewhere which includes (among other
	 * things) whether the pulse duration is based on 40MHz or 44MHz.
	 */
	re->re_timestamp = tsf;
	//re->re_rssi = pri_rssi;	/* XXX extension rssi? */
	re->re_rssi = comb_rssi;	/* XXX comb for spectral scan? or not? */
	re->re_dur = pkt[len - 3];	/* XXX extension duration? */
	re->re_num_spectral_entries = 0;
	/* XXX flags? */

	/*
	 * Update the channel frequency information before we decode
	 * the spectral or radar FFT payload.
	 */
	re->re_freq = 0;
	/* XXX endian convert len */
	if (pkt_lookup_chan((char *) rh, rh->it_len, &x) == 0) {
		/* Update the channel/frequency information */
		re->re_freq = x.freq;

		if (x.flags & IEEE80211_CHAN_QUARTER) {
			re->re_freq_sec = 0;
			re->re_freqwidth = 5;
		} else if (x.flags & IEEE80211_CHAN_HALF) {
			re->re_freq_sec = 0;
			re->re_freqwidth = 10;
		} else if (x.flags & IEEE80211_CHAN_HT40U) {
			re->re_freq_sec = re->re_freq + 20;
			re->re_freqwidth = 40;
		} else if (x.flags & IEEE80211_CHAN_HT40D) {
			re->re_freq_sec = re->re_freq - 20;
			re->re_freqwidth = 40;
		} else {
			re->re_freq_sec = 0;
			re->re_freqwidth = 20;
		}
	}

	if (pkt[len - 1] & CH_SPECTRAL_EVENT) {
		(void) ar9280_radar_spectral_decode(rh, pkt, len - 3, re);
	}

	return(1);
}
