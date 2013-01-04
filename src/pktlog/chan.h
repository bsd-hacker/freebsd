#ifndef	__CHAN_H__
#define	__CHAN_H__


struct xchan {
	/* DWORD 0 */
	uint32_t flags;
	/* DWORD 1 */
	uint16_t freq;
	uint8_t chan;
	uint8_t txpow;
};


extern	int pkt_lookup_chan(const char *buf, int len, struct xchan *x);

#endif
