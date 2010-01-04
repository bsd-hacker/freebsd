#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpuset.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/unistd.h>

/*
 * Bind an interrupt event to the specified CPU.  Note that not all
 * platforms support binding an interrupt to a CPU.  For those
 * platforms this request will fail.  For supported platforms, any
 * associated ithreads as well as the primary interrupt context will
 * be bound to the specificed CPU.  Using a cpu id of NOCPU unbinds
 * the interrupt event.
 */
int
intr_event_bind(struct intr_event *ie, u_char cpu)
{

	panic("");
	return (0);
		    
}


/*
 * Add a software interrupt handler to a specified event.  If a given event
 * is not specified, then a new event is created.
 */
int
swi_add(struct intr_event **eventp, const char *name, driver_intr_t handler,
	    void *arg, int pri, enum intr_type flags, void **cookiep)
{
	panic("");
	return (0);
}

/*
 * Schedule a software interrupt thread.
 */
void
swi_sched(void *cookie, int flags)
{

	panic("");
}
