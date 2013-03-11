#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <netinet/in.h>	/* for ntohl etc */
#include <sys/endian.h>

#include <sys/socket.h>
#include <net/if.h>

#include <pcap.h>

#include "net80211/ieee80211.h"
#include "net80211/ieee80211_radiotap.h"

#include "dev/ath/if_athioctl.h"

#if 0
#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"
#endif

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
 *    'radio[79] == 0x01'
 */
#define	PKTRULE "radio[79] & 0x01 == 0x01"

static int
pkt_compile(pcap_t *p, struct bpf_program *fp)
{
	if (pcap_compile(p, fp, PKTRULE, 1, 0) != 0)
		return 0;
	return 1;
}

/*
 * Accessor macros to go dig through the DWORD for the relevant
 * EVM bytes.
 */
#define	MS(_v, _f)		( ((_v) & (_f)) >> _f ## _S )

#define	EVM_0		0x000000ff
#define	EVM_0_S		0
#define	EVM_1		0x0000ff00
#define	EVM_1_S		8
#define	EVM_2		0x00ff0000
#define	EVM_2_S		16
#define	EVM_3		0xff000000
#define	EVM_3_S		24

/*
 * The EVM pilot information representation, post-processed.
 */
#define	NUM_EVM_PILOTS		6
#define	NUM_EVM_STREAMS		3
struct evm {
	int8_t	evm_pilots[NUM_EVM_STREAMS][NUM_EVM_PILOTS];
	int num_pilots;
	int num_streams;
};

/* Print the EVM information */
static void
print_evm(struct evm *e)
{
	int s, p;

	for (s = 0; s < e->num_streams; s++) {
		printf(" evm_stream_%d=[", s + 1);
		for (p = 0; p < NUM_EVM_PILOTS; p++) {
			printf(" %d", (int) (e->evm_pilots[s][p]));
		}
		printf(" ]");
	}
}

/*
 * Populate the given EVM information.
 *
 * The EVM pilot offsets depend upon whether the rates are
 * 1, 2 or 3 stream, as well as HT20 or HT40.
 */
static void
populate_evm(struct evm *e, uint32_t evm[4], uint8_t rx_hwrate, int rx_isht40)
{
	/* Initialise everything to 0x80 - invalid */
	memset(e->evm_pilots, 0x80, sizeof(e->evm_pilots));

	/* HT20 pilots - always 4 */
	e->num_pilots = 4;
	if (rx_isht40)
		e->num_pilots += 2;	/* HT40 - 6 pilots */

	/* XXX assume it's MCS */
	if (rx_hwrate < 0x88) {		/* 1 stream */
		e->num_streams = 1;
		e->evm_pilots[0][0] = MS(evm[0], EVM_0);
		e->evm_pilots[0][1] = MS(evm[0], EVM_1);
		e->evm_pilots[0][2] = MS(evm[0], EVM_2);
		e->evm_pilots[0][3] =
		    MS(evm[0], EVM_3);
		if (rx_isht40) {
			e->evm_pilots[0][4] = MS(evm[1], EVM_0),
			e->evm_pilots[0][5] = MS(evm[1], EVM_1);
		}
	} else if (rx_hwrate < 0x90) {	/* 2 stream */
		e->num_streams = 2;
		e->evm_pilots[0][0] = MS(evm[0], EVM_0);
		e->evm_pilots[0][1] = MS(evm[0], EVM_2);
		e->evm_pilots[0][2] = MS(evm[1], EVM_0);
		e->evm_pilots[0][3] = MS(evm[1], EVM_2);
		if (rx_isht40) {
			e->evm_pilots[0][4] = MS(evm[2], EVM_0),
			e->evm_pilots[0][5] = MS(evm[2], EVM_2);
		}
		e->evm_pilots[1][0] = MS(evm[0], EVM_1);
		e->evm_pilots[1][1] = MS(evm[0], EVM_3);
		e->evm_pilots[1][2] = MS(evm[1], EVM_1);
		e->evm_pilots[1][3] = MS(evm[1], EVM_3);
		if (rx_isht40) {
			e->evm_pilots[1][4] = MS(evm[2], EVM_1);
			e->evm_pilots[1][5] = MS(evm[2], EVM_3);
		}
	} else {			/* 3 stream */
		e->num_streams = 3;
		e->evm_pilots[0][0] = MS(evm[0], EVM_0);
		e->evm_pilots[0][1] = MS(evm[0], EVM_3);
		e->evm_pilots[0][2] = MS(evm[1], EVM_2);
		e->evm_pilots[0][3] = MS(evm[2], EVM_1);
		if (rx_isht40) {
			e->evm_pilots[0][4] = MS(evm[3], EVM_0);
			e->evm_pilots[0][5] = MS(evm[3], EVM_3);
		}
		e->evm_pilots[1][0] = MS(evm[0], EVM_1);
		e->evm_pilots[1][1] = MS(evm[1], EVM_0);
		e->evm_pilots[1][2] = MS(evm[1], EVM_3);
		e->evm_pilots[1][3] = MS(evm[2], EVM_2);
		if (rx_isht40) {
			e->evm_pilots[1][4] = MS(evm[3], EVM_1);
			e->evm_pilots[1][5] = MS(evm[4], EVM_0);
		}
		e->evm_pilots[2][0] = MS(evm[0], EVM_2);
		e->evm_pilots[2][1] = MS(evm[1], EVM_1);
		e->evm_pilots[2][2] = MS(evm[2], EVM_0);
		e->evm_pilots[2][3] = MS(evm[2], EVM_3);
		if (rx_isht40) {
			e->evm_pilots[2][4] = MS(evm[3], EVM_2);
			e->evm_pilots[2][5] = MS(evm[4], EVM_1);
		}
	}
}

void
pkt_handle(int chip, const char *pkt, int len)
{
	struct ieee80211_radiotap_header *rh;
	struct ath_rx_radiotap_header *rx;
	uint8_t rssi, nf;
	int r;
	struct xchan x;
	uint32_t evm[5];	/* XXX ATH_RADIOTAP_MAX_CHAINS */
	uint8_t rx_chainmask;
	uint8_t rx_hwrate;
	int rx_isht40;
	int rx_isht;
	int rx_isaggr;
	int rx_lastaggr;
	struct evm e;

	/* XXX assume it's a radiotap frame */
	rh = (struct ieee80211_radiotap_header *) pkt;
	rx = (struct ath_rx_radiotap_header *) pkt;

	if (rh->it_version != 0) {
		printf("%s: incorrect version (%d)\n", __func__,
		    rh->it_version);
		return;
	}

#if 0
	printf("%s: len=%d, present=0x%08x\n",
	    __func__,
	    (rh->it_len),	/* XXX why aren't these endian-converted? */
	    (rh->it_present));
#endif

	/* XXX TODO: check vh_flags to ensure this is an RX frame */

	/*
	 * Do a frequency lookup.
	 */
	/* XXX rh->it_len should be endian checked?! */
	if (pkt_lookup_chan((char *) pkt, len, &x) != 0) {
		printf("%s: channel lookup failed\n", __func__);
		return;
	}

	/*
	 * Copy out the EVM data, receive rate, RX chainmask from the
	 * header.
	 *
	 * XXX TODO: methodize this; endianness!
	 */
	memcpy(evm, pkt + 48, 4 * 4);
	rx_chainmask = rx->wr_v.vh_rx_chainmask;
	rx_hwrate = rx->wr_v.vh_rx_hwrate;
	rx_isht40 = !! (rx->wr_chan_flags & (IEEE80211_CHAN_HT40U | IEEE80211_CHAN_HT40D));
	rx_isht = !! (rx_hwrate & 0x80);

	/*
	 * If aggr=1, then we only care about lastaggr.
	 * If aggr=0, then the stack will only pass us up a
	 * completed frame, with the final descriptors' status.
	 */

	rx_isaggr = !! (rx->wr_v.vh_flags & ATH_VENDOR_PKT_ISAGGR);
	rx_lastaggr = 0;
	if ((rx->wr_v.vh_flags & ATH_VENDOR_PKT_ISAGGR) &&
	    ! (rx->wr_v.vh_flags & ATH_VENDOR_PKT_MOREAGGR)) {
		rx_lastaggr = 1;
	}

	if (rx_isht && (! rx_isaggr || rx_lastaggr)) {
		populate_evm(&e, evm, rx_hwrate, rx_isht40);

		printf("ts=%llu: rs_status=0x%x, chainmask=0x%x, "
		    "hwrate=0x%02x, isht=%d, is40=%d, "
		    "rssi_comb=%d, rssi_ctl=[%d %d %d], "
		    "rssi_ext=[%d %d %d]",
		    le64toh(rx->wr_tsf),
		    (int) rx->wr_v.vh_rs_status,
		    (int) rx->wr_v.vh_rx_chainmask,
		    (int) rx->wr_v.vh_rx_hwrate,
		    (int) rx_isht,
		    (int) rx_isht40,
		    (int) (int8_t) ((rx->wr_v.vh_rssi) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ctl[0]) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ctl[1]) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ctl[2]) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ext[0]) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ext[1]) & 0xff),
		    (int) (int8_t) ((rx->wr_v.rssi_ext[2]) & 0xff)
		    );
		print_evm(&e);

		printf("\n");
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

#if 1
	if (pcap_setfilter(p, &fp) != 0) {
		printf("pcap_setfilter failed\n");
		return (NULL);
	}
#endif

	return (p);
}

static void
usage(const char *progname)
{

	printf("Usage: %s <file|if> <filename|ifname>\n",
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

	if (argc < 3) {
		usage(argv[0]);
		exit(255);
	}

	/* XXX verify */
	fname = argv[2];

	if (strcmp(argv[1], "file") == 0) {
		p = open_offline(fname);
	} else if (strcmp(argv[1], "if") == 0) {
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
