#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/sbp.h>
#include <dev/firewire/fwmem.h>
#include <dev/firewire/fwcsr.h>

/*
 * Initialize callback for busy timeout
 */
void
fwcsr_busy_timeout_init(struct fw_bind *busy_timeout,
		     struct firewire_dev_comm *fd,
		     void *dev_softc,
		     void *callback,
		     struct malloc_type *dev_type,
		     uint32_t send_len,
		     uint32_t recv_len,
		     uint32_t max_lun)
{
	busy_timeout->start = 0xfffff0000000 | BUSY_TIMEOUT;
	busy_timeout->end =   0xfffff0000000 | BUSY_TIMEOUT;

	STAILQ_INIT(&busy_timeout->xferlist);
	fw_xferlist_add(&busy_timeout->xferlist, dev_type,
			/*send*/ send_len, /*recv*/ recv_len,
			max_lun,
			fd->fc, dev_softc, callback);
	fw_bindadd(fd->fc, busy_timeout);
}

/*
 * Terminate busy timeout callback
 */
void
fwcsr_busy_timeout_stop(struct fw_bind *busy_timeout,
		     struct firewire_dev_comm *fd)
{
	mtx_assert(&Giant, MA_OWNED);
	fw_xferlist_remove(&busy_timeout->xferlist);
	fw_bindremove(fd->fc, busy_timeout);
}

/*
 * Initialize the RESET START register
 * handler and process the call back handler
 */
void
fwcsr_reset_start_init(struct fw_bind *reset_start,
		     struct firewire_dev_comm *fd,
		     void *dev_softc,
		     void *callback,
		     struct malloc_type *dev_type,
		     uint32_t send_len,
		     uint32_t recv_len,
		     uint32_t max_lun)
{
	reset_start->start = 0xfffff0000000 | RESET_START;
	reset_start->end =   0xfffff0000000 | RESET_START;

	STAILQ_INIT(&reset_start->xferlist);
	fw_xferlist_add(&reset_start->xferlist, dev_type,
			/*send*/ send_len, /*recv*/ recv_len,
			max_lun,
			fd->fc, dev_softc, callback);
	fw_bindadd(fd->fc, reset_start);
}

/*
 * Terminate processing of RESET_START register
 */
void
fwcsr_reset_start_stop(struct fw_bind *reset_start,
		     struct firewire_dev_comm *fd)
{
	fw_xferlist_remove(&reset_start->xferlist);
	fw_bindremove(fd->fc, reset_start);
}
