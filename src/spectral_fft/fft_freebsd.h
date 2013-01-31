#ifndef	__FFT_FREEBSD_H__
#define	__FFT_FREEBSD_H__

typedef	void (* scandata_cb)(struct radar_entry *re, void *cbdata);

extern	void set_scandata_callback(scandata_cb cb, void *cbdata);
extern	int read_scandata_freebsd(char *chip, char *mode, char *fname);

#endif
