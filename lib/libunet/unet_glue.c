#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/refcount.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/priv.h>
#include <sys/time.h>


SYSCTL_NODE(, 0,	  sysctl, CTLFLAG_RW, 0,
	"Sysctl internal magic");

SYSCTL_NODE(, CTL_KERN,	  kern,   CTLFLAG_RW, 0,
	"High kernel, proc, limits &c");

SYSCTL_NODE(, CTL_NET,	  net,    CTLFLAG_RW, 0,
	"Network, (see socket.h)");

MALLOC_DEFINE(M_IOV, "iov", "large iov's");
MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_TEMP, "temp", "misc temporary data buffers");

/* This is used in modules that need to work in both SMP and UP. */
cpumask_t all_cpus;

int mp_ncpus;
/* export this for libkvm consumers. */
int mp_maxcpus = MAXCPU;

volatile int smp_started;
u_int mp_maxid;

int cold;

static void	timevalfix(struct timeval *);

int
prison_if(struct ucred *cred, struct sockaddr *sa)
{

	return (0);
}

int
prison_check_af(struct ucred *cred, int af)
{

	return (0);
}

int
prison_check_ip4(struct ucred *cred, struct in_addr *ia)
{

	return (0);
}


int
prison_equal_ip4(struct prison *pr1, struct prison *pr2)
{

	return (1);
}

/*
 * See if a prison has the specific flag set.
 */
int
prison_flag(struct ucred *cred, unsigned flag)
{

	/* This is an atomic read, so no locking is necessary. */
	return (flag & PR_HOST);
}

int
prison_get_ip4(struct ucred *cred, struct in_addr *ia)
{

	return (0);
}

int
prison_local_ip4(struct ucred *cred, struct in_addr *ia)
{

	return (0);
}

int
prison_remote_ip4(struct ucred *cred, struct in_addr *ia)
{

	return (0);
}

int
priv_check(struct thread *td, int priv)
{

	return (0);
}

int
priv_check_cred(struct ucred *cred, int priv, int flags)
{

	return (0);
}


int
vslock(void *addr, size_t len)
{

	return (0);
}

int
vsunlock(void *addr, size_t len)
{

	return (0);
}


/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(struct timeval *tv)
{

	if (tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);
	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;
	return (0);
}

/*
 * Decrement an interval timer by a specified number
 * of microseconds, which must be less than a second,
 * i.e. < 1000000.  If the timer expires, then reload
 * it.  In this case, carry over (usec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
int
itimerdecr(struct itimerval *itp, int usec)
{

	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += 1000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timevalisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timevalisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += 1000000;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	return (0);
}

/*
 * Add and subtract routines for timevals.
 * N.B.: subtract routine doesn't deal with
 * results which are before the beginning,
 * it just gets very confused in this case.
 * Caveat emptor.
 */
void
timevaladd(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	timevalfix(t1);
}

void
timevalsub(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static void
timevalfix(struct timeval *t1)
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

/*
 * ratecheck(): simple time-based rate-limit checking.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int rv = 0;

	getmicrouptime(&tv);		/* NB: 10ms precision */
	delta = tv;
	timevalsub(&delta, lasttime);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timevalcmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

void
bintime(struct bintime *bt)
{

	panic("");
}
	
void
getmicrouptime(struct timeval *tvp)
{

	panic("");
}

void
getmicrotime(struct timeval *tvp)
{

	panic("");
}


