#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net80211/ieee80211_radiotap.h"

#include "radiotap_iter.h"

#include "chan.h"

int
pkt_lookup_chan(const char *buf, int len, struct xchan *x)
{
	struct ieee80211_radiotap_iterator iter;
	int err;

	bzero(&iter, sizeof(iter));

	err = ieee80211_radiotap_iterator_init(&iter, (void *) buf, len, NULL);
	if (err < 0) {
		printf("%s: ieee80211_radiotap_iterator_init: failed; err=%d\n",
		    __func__,
		    err);
		    return (-1);
	}

	/* Iterate over, looping for the xchannel struct */
	while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
		if (iter.is_radiotap_ns) {
			if (iter.this_arg_index == IEEE80211_RADIOTAP_XCHANNEL) {
				/* XXX endian! */
				memcpy(x, iter.this_arg, 8);
				return (0);
			}
		}
	}
	return (-1);
}
