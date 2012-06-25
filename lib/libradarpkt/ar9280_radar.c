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

#include <sys/socket.h>
#include <net/if.h>

#include "net80211/ieee80211_radiotap.h"

#include "dev/ath/if_athioctl.h"

#include "pkt.h"
#include "ar9280_radar.h"

/* Relevant for Sowl and later */
#define	EXT_CH_RADAR_EARLY_FOUND	0x04
#define	EXT_CH_RADAR_FOUND	0x02
#define	PRI_CH_RADAR_FOUND	0x01

int
ar9280_radar_decode(struct ieee80211_radiotap_header *rh,
    const unsigned char *pkt, int len, struct radar_entry *re)
{
	uint64_t tsf;
	int8_t comb_rssi, pri_rssi, ext_rssi, nf;
	struct ath_rx_radiotap_header *rx =
	    (struct ath_rx_radiotap_header *) rh;

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
	printf(" flags: %s %s %s\n",
	    pkt[len - 1] & PRI_CH_RADAR_FOUND ? "pri" : "",
	    pkt[len - 1] & EXT_CH_RADAR_FOUND ? "ext" : "",
	    pkt[len - 1] & EXT_CH_RADAR_EARLY_FOUND ? "extearly" : ""
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
	re->re_rssi = pri_rssi;	/* XXX extension rssi? */
	re->re_dur = pkt[len - 3];	/* XXX extension duration? */
	re->re_freq = 0;
	/* XXX flags? */

	return(1);
}
