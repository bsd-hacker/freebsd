#ifndef	__HEATMAP_H__
#define	__HEATMAP_H__

#include <sys/types.h>

/*
 * This is a hard-coded heatmap for RSSI/duration.
 * It should be made less hardcoded!
 */

/* XXX hard-coded */
#define	MAX_PULSEDUR	255
#define	MAX_RSSI	255

#define	MAX_HEATCNT	255

class HeatMap {
	private:
		uint8_t	heatmap[MAX_PULSEDUR][MAX_RSSI];

	public:
		HeatMap();
		void incr(int x, int y);
		void decr(int x, int y);
		uint8_t get(int x, int y);
};

#endif	/* __HEATMAP_H__ */
