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

#include "libradarpkt/chan.h"

/* from _ieee80211.h */
#define      IEEE80211_CHAN_HT40U    0x00020000 /* HT 40 channel w/ ext above */
#define      IEEE80211_CHAN_HT40D    0x00040000 /* HT 40 channel w/ ext below */

// non-HT
// 0x00200140
// HT, not HT40
// 0x00210140

/*
 * Compile up a rule that's bound to be useful - it only matches on
 * radar errors.
 *
 * tcpdump -ni wlan0 -y IEEE802_11_RADIO -x -X -s0 -v -ve \
 *    'radio[73] == 0x2 && (radio[72] == 5 || radio[72] == 24)
 */

#define	PKTRULE "radio[73] == 0x2 && (radio[72] == 5 || radio[72] == 24)"

static int
pkt_compile(pcap_t *p, struct bpf_program *fp)
{
	if (pcap_compile(p, fp, PKTRULE, 1, 0) != 0)
		return 0;
	return 1;
}

static void
pkt_print(struct radar_entry *re)
{
	printf("ts: %llu, freq=%u, freqsec=%d, chwidth=%d, rssi=%d, dur=%d\n",
	    re->re_timestamp,
	    re->re_freq,
	    re->re_freq_sec,
	    re->re_freqwidth,
	    re->re_rssi,
	    re->re_dur);
}

void
pkt_handle(int chip, const char *pkt, int len)
{
	struct ieee80211_radiotap_header *rh;
	uint8_t rssi, nf;
	struct radar_entry re;
	int r;
	struct xchan x;

	/* XXX assume it's a radiotap frame */
	rh = (struct ieee80211_radiotap_header *) pkt;

	if (rh->it_version != 0) {
		printf("%s: incorrect version (%d)\n", __func__,
		    rh->it_version);
		return;
	}

#if 0
	/* XXX short frames? */
	if (len < 73) {
		printf("%s: short frame (%d bytes)\n", __func__, len);
		return;
	}
#endif

#if 0
	printf("%s: len=%d, present=0x%08x\n",
	    __func__,
	    (rh->it_len),	/* XXX why aren't these endian-converted? */
	    (rh->it_present));
#endif

#if 0
	/*
	 * XXX local hack - enable the radar checking
	 * XXX by assuming byte 72 is the radar status code.
	 */
	if (pkt[72] != 5 && pkt[72] != 24) {
		printf("%s: not a radar error (code %d)?!\n",
		    __func__,
		    pkt[72]);
		return;
	}
#endif

	/*
	 * Do a frequency lookup.
	 */
	/* XXX rh->it_len should be endian checked?! */
	if (pkt_lookup_chan((char *) pkt, len, &x) != 0) {
		printf("%s: channel lookup failed\n", __func__);
		return;
	}

	if (chip == CHIP_AR5212)
		r = ar5212_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);
	else if (chip == CHIP_AR5416)
		r = ar5416_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);
	else if (chip == CHIP_AR9280)
		r = ar9280_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);

	/* Update the channel/frequency information */
	re.re_freq = x.freq;

	if (x.flags & IEEE80211_CHAN_QUARTER) {
		re.re_freq_sec = 0;
		re.re_freqwidth = 5;
	} else if (x.flags & IEEE80211_CHAN_HALF) {
		re.re_freq_sec = 0;
		re.re_freqwidth = 10;
	} else if (x.flags & IEEE80211_CHAN_HT40U) {
		re.re_freq_sec = re.re_freq + 20;
		re.re_freqwidth = 40;
	} else if (x.flags & IEEE80211_CHAN_HT40D) {
		re.re_freq_sec = re.re_freq - 20;
		re.re_freqwidth = 40;
	} else {
		re.re_freq_sec = 0;
		re.re_freqwidth = 20;
	}

	/*
	 * TODO: Print the summary record
	 */
	if (r) {
		pkt_print(&re);
	}
}

static pcap_t *
open_offline(const char *fname)
{
	pcap_t *p;
	char errbuf[PCAP_ERRBUF_SIZE];

	p = pcap_open_offline(fname, errbuf);
	if (p == NULL) {
		printf("pcap_create failed: %s\n", errbuf);
		return (NULL);
	}

	return (p);
}

static pcap_t *
open_online(const char *ifname)
{
	pcap_t *p;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;

	p = pcap_open_live(ifname, 65536, 1, 1000, errbuf);
	if (! p) {
		err(1, "pcap_create: %s\n", errbuf);
		return (NULL);
	}

	if (pcap_set_datalink(p, DLT_IEEE802_11_RADIO) != 0) {
		pcap_perror(p, "pcap_set_datalink");
		return (NULL);
	}

	/* XXX pcap_is_swapped() ? */

	if (! pkt_compile(p, &fp)) {
		pcap_perror(p, "pkg_compile compile error\n");
		return (NULL);
	}

	if (pcap_setfilter(p, &fp) != 0) {
		printf("pcap_setfilter failed\n");
		return (NULL);
	}

	return (p);
}

static void
usage(const char *progname)
{

	printf("Usage: %s <ar5212|ar5416|ar9280> <file|if> <filename|ifname>\n",
	    progname);
}

int
main(int argc, const char *argv[])
{
	char *dev;
	pcap_t * p;
	const char *fname;
	const unsigned char *pkt;
	struct pcap_pkthdr *hdr;
	int len, r;
	int chip = 0;

	if (argc < 4) {
		usage(argv[0]);
		exit(255);
	}

	if (strcmp(argv[1], "ar5212") == 0) {
		chip = CHIP_AR5212;
	} else if (strcmp(argv[1], "ar5416") == 0) {
		chip = CHIP_AR5416;
	} else if (strcmp(argv[1], "ar9280") == 0) {
		chip = CHIP_AR9280;
	} else {
		usage(argv[0]);
		exit(255);
	}

	/* XXX verify */
	fname = argv[3];

	if (strcmp(argv[2], "file") == 0) {
		p = open_offline(fname);
	} else if (strcmp(argv[2], "if") == 0) {
		p = open_online(fname);
	} else {
		usage(argv[0]);
		exit(255);
	}

	if (p == NULL)
		exit(255);

	/*
	 * Iterate over frames, looking for radiotap frames
	 * which have PHY errors.
	 *
	 * XXX We should compile a filter for this, but the
	 * XXX access method is a non-standard hack atm.
	 */
	while ((r = pcap_next_ex(p, &hdr, &pkt)) >= 0) {
#if 0
		printf("capture: len=%d, caplen=%d\n",
		    hdr->len, hdr->caplen);
#endif
		if (r > 0)
			pkt_handle(chip, pkt, hdr->caplen);
	}

	pcap_close(p);
}
