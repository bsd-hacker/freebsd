#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <pthread.h>

int	hogticks;

typedef struct sleep_entry {
	LIST_ENTRY(sleep_entry) list_entry;
	void 		*chan;
	const char 	*wmesg;
	pthread_cond_t	cond;
	int		waiters;
} *sleep_entry_t;

static void synch_setup(void *dummy);
SYSINIT(synch_setup, SI_SUB_INTR, SI_ORDER_FIRST, synch_setup,
    NULL);

static struct se_head *se_active;
static u_long se_hashmask;
static pthread_mutex_t synch_lock;
#define SE_HASH(chan)	(((uintptr_t)chan) & se_hashmask)
LIST_HEAD(se_head, sleep_entry);

static void
synch_setup(void *arg)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutex_init(&synch_lock, &attr);
	se_active = hashinit(64, M_TEMP, &se_hashmask);
}

sleep_entry_t
se_alloc(void *chan, const char *wmesg)
{
	sleep_entry_t se;
	pthread_condattr_t attr;
	struct se_head *hash_list;	

	se = malloc(sizeof(*se), M_DEVBUF, 0);
	se->chan = chan;
	se->wmesg = wmesg;
	se->waiters = 1;
	pthread_condattr_init(&attr);
	pthread_cond_init(&se->cond, &attr);

	/* insert in hash table */
	hash_list = &se_active[SE_HASH(chan)];
	LIST_INSERT_HEAD(hash_list, se, list_entry);
	
	return (se);
}

sleep_entry_t
se_lookup(void *chan)
{
	struct se_head *hash_list;
	sleep_entry_t se;

	hash_list = &se_active[SE_HASH(chan)];
	LIST_FOREACH(se, hash_list, list_entry) 
		if (se->chan == chan)
			return (se);

	return (NULL);
}

void
se_free(sleep_entry_t se)
{

	if (--se->waiters == 0) {
		LIST_REMOVE(se, list_entry);
		pthread_cond_destroy(&se->cond);
		free(se, M_DEVBUF);
	}
}
		

/*
 * General sleep call.  Suspends the current thread until a wakeup is
 * performed on the specified identifier.  The thread will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If pri includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * The lock argument is unlocked before the caller is suspended, and
 * re-locked before _sleep() returns.  If priority includes the PDROP
 * flag the lock is not re-locked before returning.
 */
int
_sleep(void *ident, struct lock_object *lock, int priority,
    const char *wmesg, int timo)
{
	sleep_entry_t se;
	int rv;
	struct timespec ts;

	pthread_mutex_lock(&synch_lock);
	if ((se = se_lookup(ident)) != NULL)
		se->waiters++;
	else
		se = se_alloc(ident, wmesg);
	pthread_mutex_unlock(&synch_lock);

	if (timo)
		rv = pthread_cond_timedwait(&se->cond, &lock->lo_mutex, &ts);
	else
		rv = pthread_cond_wait(&se->cond, &lock->lo_mutex);

	pthread_mutex_lock(&synch_lock);
	se_free(se);
	pthread_mutex_unlock(&synch_lock);

	return (rv);
}

void
wakeup(void *chan)
{
	sleep_entry_t se;

	pthread_mutex_lock(&synch_lock);
	if ((se = se_lookup(chan)) != NULL)
		pthread_cond_broadcast(&se->cond);
	pthread_mutex_unlock(&synch_lock);
}


void
wakeup_one(void *chan)
{
	sleep_entry_t se;

	pthread_mutex_lock(&synch_lock);
	if ((se = se_lookup(chan)) != NULL)
		pthread_cond_signal(&se->cond);
	pthread_mutex_unlock(&synch_lock);
}

