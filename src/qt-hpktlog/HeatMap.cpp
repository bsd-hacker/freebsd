
#include "HeatMap.h"

HeatMap::HeatMap()
{

	// Blank the heat map
	for (int i = 0; i < MAX_RSSI; i++)
		for (int j = 0; j < MAX_PULSEDUR; j++)
		heatmap[i][j] = 0;
}

void
HeatMap::incr(int x, int y)
{

	if (heatmap[x % MAX_RSSI][y % MAX_PULSEDUR] < (MAX_HEATCNT - 1))
		heatmap[x % MAX_RSSI][y % MAX_PULSEDUR]++;
}

void
HeatMap::decr(int x, int y)
{

	if (heatmap[x % MAX_RSSI][y % MAX_PULSEDUR] > 0)
		heatmap[x % MAX_RSSI][y % MAX_PULSEDUR]--;
}

uint8_t
HeatMap::get(int x, int y)
{
	return heatmap[x % MAX_RSSI][y % MAX_PULSEDUR];
}
