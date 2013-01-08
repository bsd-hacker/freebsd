#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>	/* for ntohl etc */
#include <pthread.h>

#include <pcap.h>

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"

#include "fft_eval.h"
#include "fft_freebsd.h"

static scandata_cb cb_cb = NULL;
static void * cb_cbdata = NULL;
pthread_t main_pthread;
pcap_t * g_p;
int g_chip = 0;

void
set_scandata_callback(scandata_cb cb, void *cbdata)
{
	cb_cb = cb;
	cb_cbdata = cbdata;
}

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
pkt_handle_single(struct radar_entry *re)
{
	/* Call our callback w/ the radar entry */
	if (cb_cb)
		cb_cb(re, cb_cbdata);

#if 0
	/* Sleep for a bit */
	usleep(100 * 1000);	/* 100ms */
#endif
}

static void
pkt_print(struct radar_entry *re)
{
	printf("ts: %llu, freq=%u, rssi=%d, dur=%d, nsamples=%d\n",
	    re->re_timestamp,
	    re->re_freq,
	    re->re_rssi,
	    re->re_dur,
	    re->re_num_spectral_entries);
}

void
pkt_handle(int chip, const char *pkt, int len)
{
	struct ieee80211_radiotap_header *rh;
	uint8_t rssi, nf;
	struct radar_entry re;
	int r;

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
	if (chip == CHIP_AR5212)
		r = ar5212_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);
	else if (chip == CHIP_AR5416)
		r = ar5416_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);
	else if (chip == CHIP_AR9280)
		r = ar9280_radar_decode(rh, pkt + rh->it_len, len - rh->it_len,
		    &re);

	/* XXX do something about it */
	if (r) {
		//pkt_print(&re);
		pkt_handle_single(&re);
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

static void *
fft_pcap_thread_main(void *arg)
{
	const unsigned char *pkt;
	struct pcap_pkthdr *hdr;
	int len, r;

	/*
	 * Iterate over frames, looking for radiotap frames
	 * which have PHY errors.
	 *
	 * XXX We should compile a filter for this, but the
	 * XXX access method is a non-standard hack atm.
	 */
	while ((r = pcap_next_ex(g_p, &hdr, &pkt)) >= 0) {
#if 0
		printf("capture: len=%d, caplen=%d\n",
		    hdr->len, hdr->caplen);
#endif
		if (r > 0)
			pkt_handle(g_chip, pkt, hdr->caplen);
	}

	return (NULL);
}


int
open_device(const char *dev_str, const char *chip_str, const char *mode)
{
	char *dev;
	const char *fname;
	if (strcmp(chip_str, "ar5212") == 0) {
		g_chip = CHIP_AR5212;
	} else if (strcmp(chip_str, "ar5416") == 0) {
		g_chip = CHIP_AR5416;
	} else if (strcmp(chip_str, "ar9280") == 0) {
		g_chip = CHIP_AR9280;
	} else {
		usage("main");
		exit(255);
	}

	/* XXX verify */
	fname = dev_str;

	if (strcmp(mode, "file") == 0) {
		g_p = open_offline(fname);
	} else if (strcmp(mode, "if") == 0) {
		g_p = open_online(fname);
	} else {
		usage("main");
		exit(255);
	}

	if (g_p == NULL)
		exit(255);

	/* Create data source thread */
	if (pthread_create(&main_pthread, NULL,
	    fft_pcap_thread_main, NULL) != 0) {
		warnx("pthread_create");
		return(-1);
	}

	/* XXX for now */
	return (0);
}

int
read_scandata_freebsd(char *fname, struct scanresult **result)
{

	(void) open_device("wlan0", "ar9280", "if");
	return (0);
}
