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

/* Headers required for the ioctl interface */
#include <sys/socket.h>
#include <net/if.h>
#include <sys/queue.h>

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "dev/ath/if_athioctl.h"

#include "pkt.h"

#include "ar5416_radar.h"

int
ar5416_radar_decode(struct ieee80211_radiotap_header *rh,
    const unsigned char *pkt, int len, struct radar_entry *re)
{
	uint64_t tsf;
	int8_t rssi, nf;
	struct ath_rx_radiotap_header *rx =
	    (struct ath_rx_radiotap_header *) rh;

	/* XXX we should really be implementing a real radiotap parser */
	tsf = le64toh(rx->wr_tsf);

	/*
	 * XXX For AR5416, we should use the ctl[0] RSSI for pre Owl-2.2.
	 */
	rssi = rx->wr_v.rssi_ctl[0];
	nf = rx->wr_antnoise;

	/* Last byte is the pulse width */
	if (len < 1) {
		printf("short radar frame\n");
		return (0);
	}

#if 0
	printf("phyerr: %d ", rx->wr_v.vh_phyerr_code);
	printf("ts: %lld", tsf);
	printf("\tit_present: %x", le32toh(rh->it_present));
	printf("\tlen: %d", len);
	printf("\trssi: %d, nf: %d", rssi, nf);
	printf("\tpri: %u", pkt[len - 1] & 0xff);
	printf("\n");
#endif

	/*
	 * If RSSI > 0x80, it's a negative RSSI. We store it signed
	 * so we can at least log that it was negative in order to
	 * plot it. The radar code IIRC just tosses it.
	 */
	re->re_rssi = rssi;

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
	 */

	re->re_timestamp = tsf;
	/* XXX TODO: re->re_freq */
	re->re_dur = pkt[len - 1] & 0xff;
	/* XXX TODO: also store ctl/ext RSSI, and some flags */
	re->re_freq = 0;

	return(1);
}
