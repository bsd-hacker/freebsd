
#include <pcap.h>
#include <sys/endian.h>
#include <err.h>

#include "net80211/ieee80211_radiotap.h"

#include "PktSource.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5212_radar.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"

//
// This particular class _should_ just be a base class that
// a couple of derivates use for the live versus load stuff.
// So yes, I should do that.

PktSource::~PktSource()
{

	this->Close();
}

bool
PktSource::Load(const char *filename)
{
	char errbuf[PCAP_ERRBUF_SIZE];

	this->Close();

	PcapHdl = pcap_open_offline(filename, errbuf);

	if (PcapHdl == NULL)
		return (false);

	// TODO: turn this into a method
	if (timerId != -1)
		killTimer(timerId);

	//Kick-start the first timer!
	timerId = startTimer(1);

	isLive = false;

	return (true);
}

#define	PKTRULE	"radio[73] == 0x2 && (radio[72] == 5 || radio[72] == 24)"

bool
PktSource::OpenLive(const char *ifname)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;

	this->Close();

	PcapHdl = pcap_open_live(ifname, 65536, 1, 1000, errbuf);
	if (! PcapHdl) {
		err(1, "pcap_create: %s\n", errbuf);
		return (false);
	}

	if (pcap_set_datalink(PcapHdl, DLT_IEEE802_11_RADIO) != 0) {
		pcap_perror(PcapHdl, (char *) "pcap_set_datalink");
		return (false);
	}

	/* XXX pcap_is_swapped() ? */

	if (pcap_compile(PcapHdl, &fp, PKTRULE, 1, 0) != 0) {
		pcap_perror(PcapHdl, (char *) "pkg_compile compile error\n");
		this->Close();
		return (false);
	}

	if (pcap_setfilter(PcapHdl, &fp) != 0) {
		pcap_perror(PcapHdl, (char *) "pcap_setfilter error\n");
		this->Close();
		return (false);
	}

	// Register a timer event _and_ make the socket non-blocking.
	if (pcap_setnonblock(PcapHdl, 1, errbuf) == -1) {
		pcap_perror(PcapHdl, (char *) "pcap_set_nonblock error\n");
		this->Close();
		return (false);
	}

	// For now, we'll just do a 2ms check to see what's going on.
	// Eventually we'll do a 1s timer event to flush the queue
	// _and_ do non-blocking IO via QT.

	// TODO: turn this into a method
	if (timerId != -1)
		killTimer(timerId);

	//Kick-start the first timer!
	timerId = startTimer(2);
	isLive = true;

	return (true);

}

#undef	PKTRULE

void
PktSource::Close()
{

	if (PcapHdl != NULL) {
		pcap_close(PcapHdl);
		PcapHdl = NULL;
	}
}

// Periodically read some more frames and pass them up as events.
// Right now this reads one event.
// Eventually it should pace the events based on their timestamps.
void
PktSource::timerEvent(QTimerEvent *event)
{
	int r;
	struct pcap_pkthdr *hdr;
	unsigned const char *pkt;
	struct ieee80211_radiotap_header *rt;
	struct radar_entry re;

//	printf("%s: timer event!\n", __func__);

	while (1) {
		r = pcap_next_ex(PcapHdl, &hdr, &pkt);

		// Error? Delete the timer.
		if (r < 0) {
			killTimer(timerId);
			timerId = -1;
			printf("%s: final event (r=%d), finish timer!\n",
			    __func__,
			    r);
			this->Close();
			return;
		}

		// Nothing available? Just skip until the next
		// check.
		if (r == 0)
			break;

		rt = (struct ieee80211_radiotap_header *) pkt;
		if (rt->it_version != 0) {
			printf("%s: unknown version (%d)\n",
			    __func__,
			    rt->it_version);
			break;
		}

		// TODO: just assume AR5416 for now..
		switch (chipid) {
		case CHIP_AR5416:
			r = ar5416_radar_decode(rt,
			    (pkt + le16toh(rt->it_len)),
			    hdr->caplen - le16toh(rt->it_len), &re);
			break;
		case CHIP_AR5212:
			r = ar5212_radar_decode(rt,
			    (pkt + le16toh(rt->it_len)),
			    hdr->caplen - le16toh(rt->it_len), &re);
			break;
		case CHIP_AR9280:
			r = ar9280_radar_decode(rt,
			    (pkt + le16toh(rt->it_len)),
			    hdr->caplen - le16toh(rt->it_len), &re);
			break;
		default:
			printf("%s: unknown chip id? (%d)\n",
			    __func__,
			    chipid);
		}

		// Error? Skip to the next one.
		if (r <= 0) {
			printf("%s: parse failed\n", __func__);
		} else {
			// The actual event may be delayed; so i either have
			// to pass a reference (not pointer), _or_ a copy.
			emit emitRadarEntry(re);
		}

		// Break out of the loop if we're not live
		if (! isLive)
			break;
	}
}
