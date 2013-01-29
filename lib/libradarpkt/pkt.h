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
#ifndef	__PKTHDR_PKT_H__
#define	__PKTHDR_PKT_H__

#ifdef	__cplusplus
extern "C" {
#endif

#define	PKT_NUM_BINS	128

#define	MAX_SPECTRAL_SCAN_SAMPLES_PER_PKT	128

struct radar_fft_bin {
	int32_t	dBm;
	int32_t	adj_mag;
	uint8_t	raw_mag;	/* original magnitude */
	uint8_t	bitmap;	/* whether this is strong/weak */
	uint8_t	pad[2];
};

struct radar_fft_entry {
	int is_ht40;	/* 1=HT40, 0=HT20 */

	struct {
		int nf, rssi;
		uint8_t	max_index;
		uint8_t	bitmap_weight;
		uint16_t	max_magnitude;
	} lower, upper;

	struct radar_fft_bin bins[PKT_NUM_BINS];

	uint8_t	max_exp;
	uint8_t	num_bins;	/* 56 or 128 */
};

struct radar_entry {
	uint64_t	re_timestamp;

	/* Primary frequency */
	uint32_t	re_freq;

	/* Flags */
	uint32_t	re_flags;

	/* Secondary channel frequency, if applicable */
	uint32_t	re_freq_sec;

	/* Channel width */
	uint32_t	re_freqwidth;

	/*
	 * True event frequency centre - for spectral scan and
	 * radar FFTs.
	 */
	uint32_t	re_freq_centre;

	/*
	 * The hardware may give it to us as a negative number;
	 * eg CCK decode which can use self-correlation to decode
	 * a very very weak signal.
	 */
	int32_t		re_rssi;
	uint32_t	re_dur;
	int32_t		re_nf;

	/*
	 * Store the spectral scan primary/extension channel
	 * RSSI here, which will be used by the spectral scan
	 * FFT code to calculate dBm values.
	 */
	int32_t		re_pri_rssi;
	int32_t		re_ext_rssi;

	/* XXX make these optional at some point */
	int 		re_num_spectral_entries;
	struct radar_fft_entry re_spectral_entries[MAX_SPECTRAL_SCAN_SAMPLES_PER_PKT];
};

#ifdef	__cplusplus
}
#endif

#endif	/* __PKTHDR_PKT_H__ */
