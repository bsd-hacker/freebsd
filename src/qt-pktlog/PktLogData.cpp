
#include <iostream>
#include <string>
#include <vector>

#include "libradarpkt/pkt.h"
#include "PktLogData.h"

void
PktLogData::Clear()
{

	RadarEntries.clear();
}

void
PktLogData::Load(const char *file)
{

	// XXX TODO
}

void
PktLogData::AddEntry(struct radar_entry re)
{

	// Is this correctly creating a copy of 're' and adding that
	// to the vector?
	RadarEntries.push_back(re);
}

//
// XXX there has to be a clearer way to do this...
//
std::vector<double>
PktLogData::GetRssi()
{
	int i;

	std::vector<double> t;

	t.resize(RadarEntries.size());

	for (i = 0; i < RadarEntries.size(); i++) {
		t[i] = (double) RadarEntries[i].re_rssi;
	}

	return t;
}

std::vector<double>
PktLogData::GetDuration()
{
	int i;

	std::vector<double> t;

	t.resize(RadarEntries.size());

	for (i = 0; i < RadarEntries.size(); i++) {
		t[i] = (double) RadarEntries[i].re_dur;
	}

	return t;

}
