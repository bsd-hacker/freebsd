#ifndef	__PKT_LOG_DATA_H__
#define	__PKT_LOG_DATA_H__

#include <stdint.h>
#include <vector>

#include "libradarpkt/pkt.h"

class PktLogData {
public:
	/* XXX should be private! */
	std::vector<struct radar_entry> RadarEntries;

	void Clear();
	void Load(const char *filename);
	std::vector<double> GetRssi();
	std::vector<double> GetDuration();
	std::vector<uint64_t> GetTimestamp();
	std::vector<uint32_t> GetFreq();
	int Size() { return RadarEntries.size(); }
//private:
	void AddEntry(struct radar_entry re);
};

#endif	/* __PKT_LOG_DATA_H__ */
