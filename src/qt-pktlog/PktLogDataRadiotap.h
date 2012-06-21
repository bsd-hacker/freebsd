#ifndef	__PKT_LOG_DATA_RADIOTAP_H__
#define	__PKT_LOG_DATA_RADIOTAP_H__

#include "PktLogData.h"
#include <pcap.h>

class PktLogDataRadiotap : public PktLogData {

private:
	pcap_t *PcapHdl;

public:
	~PktLogDataRadiotap();
	PktLogDataRadiotap() : PcapHdl(NULL) { };
	bool LoadPcapOffline(const char *file, int type);
	bool LoadPcapOnline(const char *ifname, int type);
	void Close();
};


#endif	/* __PKT_LOG_DATA_RADIOTAP_H__ */
