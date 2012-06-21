#include <iostream>
#include <string>
#include <vector>

#include <pcap.h>
#include <sys/endian.h>

#include "net80211/ieee80211_radiotap.h"

#include "libradarpkt/pkt.h"
#include "libradarpkt/ar5416_radar.h"
#include "libradarpkt/ar9280_radar.h"

#include "PktLogData.h"
#include "PktLogDataRadiotap.h"

PktLogDataRadiotap::~PktLogDataRadiotap()
{

	this->Close();
}

bool
PktLogDataRadiotap::LoadPcapOffline(const char *file, int type)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	unsigned const char *pkt;
	struct pcap_pkthdr *hdr;
	int r;
	struct ieee80211_radiotap_header *rt;
	struct radar_entry re;

	this->Close();
	PcapHdl = pcap_open_offline(file, errbuf);
	if (PcapHdl == NULL) {
		printf("pcap_open_offline failed: %s\n", errbuf);
		return false;
	}

	// Grab data, assume AR5416 for now
	while ((r = pcap_next_ex(PcapHdl, &hdr, &pkt)) >= 0) {
		//printf("read: %d byte frame, r=%d\n", hdr->caplen, r);
		if (r <= 0)
			continue;

		rt = (struct ieee80211_radiotap_header *) pkt;
		//printf("read: %d byte frame, r=%d, version=%d\n", hdr->caplen, r, rt->it_version);
		if (rt->it_version != 0)
			continue;

		/* XXX length checks, phyerr checks, etc */
		switch (type) {
			case CHIP_AR5416:
				r = ar5416_radar_decode(rt,
				    (pkt + le16toh(rt->it_len)),
				    hdr->caplen - le16toh(rt->it_len), &re);
				break;
			case CHIP_AR9280:
				r = ar9280_radar_decode(rt,
				    (pkt + le16toh(rt->it_len)),
				    hdr->caplen - le16toh(rt->it_len), &re);
				break;
			default:
				fprintf(stderr, "%s: unknown chip! (%d) \n",
				    __func__,
				    type);
				return (false);
		}
		if (r == 0)
			continue;
		AddEntry(re);
	}

	this->Close();

	return true;
}

void
PktLogDataRadiotap::Close()
{

	if (PcapHdl != NULL) {
		pcap_close(PcapHdl);
		PcapHdl = NULL;
	}
}

