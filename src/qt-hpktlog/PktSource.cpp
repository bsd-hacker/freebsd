
#include <pcap.h>
#include <sys/endian.h>

#include "net80211/ieee80211_radiotap.h"

#include "PktSource.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5416_radar.h"

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

	return (true);
}

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

	r = pcap_next_ex(PcapHdl, &hdr, &pkt);

	// Error? Delete the timer.
	// TODO: this should handle the "error/EOF" versus "none just for now,
	// but more are coming" errors correctly!
	if (r <= 0) {
		killTimer(timerId);
		timerId = -1;
		printf("%s: final event (r=%d), finish timer!\n",
		    __func__,
		    r);
		this->Close();
		return;
	}

	rt = (struct ieee80211_radiotap_header *) pkt;
	if (rt->it_version != 0) {
		printf("%s: unknown version (%d)\n",
		    __func__,
		    rt->it_version);
		return;
	}

	// TODO: just assume AR5416 for now..
	r = ar5416_radar_decode(rt,
	    (pkt + le16toh(rt->it_len)),
	    hdr->caplen - le16toh(rt->it_len), &re);

	// Error? Just wait for the next one?
	if (r == 0) {
		printf("%s: parse failed\n", __func__);
		return;
	}

#if 0
	printf("%s: parsed: tsf=%llu, rssi=%d, dur=%d\n",
	    __func__,
	    (unsigned long long) re.re_timestamp,
	    re.re_rssi,
	    re.re_dur);
#endif

	// The actual event may be delayed; so i either have
	// to pass a reference (not pointer), _or_ a copy.
	emit emitRadarEntry(re);
}
