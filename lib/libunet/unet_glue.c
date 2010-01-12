#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/event.h>
#include <sys/jail.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include <sys/time.h>
#include <sys/ucred.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

SYSCTL_NODE(, 0,	  sysctl, CTLFLAG_RW, 0,
	"Sysctl internal magic");

SYSCTL_NODE(, CTL_KERN,	  kern,   CTLFLAG_RW, 0,
	"High kernel, proc, limits &c");

SYSCTL_NODE(, CTL_NET,	  net,    CTLFLAG_RW, 0,
	"Network, (see socket.h)");

SYSCTL_NODE(, CTL_VM,	  vm,    CTLFLAG_RW, 0,
	"Virtual memory");

SYSCTL_NODE(, CTL_DEBUG,  debug,  CTLFLAG_RW, 0,
	"Debugging");

MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_TEMP, "temp", "misc temporary data buffers");


extern void abort(void);

int	ticks;

time_t time_second = 1;
time_t time_uptime = 1;

/* This is used in modules that need to work in both SMP and UP. */
cpumask_t all_cpus;

int mp_ncpus;
/* export this for libkvm consumers. */
int mp_maxcpus = MAXCPU;

volatile int smp_started;
u_int mp_maxid;

long first_page = 0;

struct vmmeter cnt;
vm_map_t kernel_map=0;
vm_map_t kmem_map=0;

struct vm_object kernel_object_store;
struct vm_object kmem_object_store;

struct filterops fs_filtops;
struct filterops sig_filtops;

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


/*
 * Return 1 if the passed credential is in a jail and that jail does not
 * have its own virtual network stack, otherwise 0.
 */
int
jailed_without_vnet(struct ucred *cred)
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

void
vsunlock(void *addr, size_t len)
{

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

/*
 * ppsratecheck(): packets (or events) per second limitation.
 *
 * Return 0 if the limit is to be enforced (e.g. the caller
 * should drop a packet because of the rate limitation).
 *
 * maxpps of 0 always causes zero to be returned.  maxpps of -1
 * always causes 1 to be returned; this effectively defeats rate
 * limiting.
 *
 * Note that we maintain the struct timeval for compatibility
 * with other bsd systems.  We reuse the storage and just monitor
 * clock ticks for minimal overhead.  
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	int now;

	/*
	 * Reset the last time and counter if this is the first call
	 * or more than a second has passed since the last update of
	 * lasttime.
	 */
	now = ticks;
	if (lasttime->tv_sec == 0 || (u_int)(now - lasttime->tv_sec) >= hz) {
		lasttime->tv_sec = now;
		*curpps = 1;
		return (maxpps != 0);
	} else {
		(*curpps)++;		/* NB: ignore potential overflow */
		return (maxpps < 0 || *curpps < maxpps);
	}
}

/*
 * Compute number of ticks in the specified amount of time.
 */
int
tvtohz(tv)
	struct timeval *tv;
{
	register unsigned long ticks;
	register long sec, usec;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * difference fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time difference fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time difference down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	sec = tv->tv_sec;
	usec = tv->tv_usec;
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}
	if (sec < 0) {
#ifdef DIAGNOSTIC
		if (usec > 0) {
			sec++;
			usec -= 1000000;
		}
		printf("tvotohz: negative time difference %ld sec %ld usec\n",
		       sec, usec);
#endif
		ticks = 1;
	} else if (sec <= LONG_MAX / 1000000)
		ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
			/ tick + 1;
	else if (sec <= LONG_MAX / hz)
		ticks = sec * hz
			+ ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		ticks = LONG_MAX;
	if (ticks > INT_MAX)
		ticks = INT_MAX;
	return ((int)ticks);
}

int
copyin(const void *uaddr, void *kaddr, size_t len)
{

	memcpy(kaddr, uaddr, len);

	return (0);
}

int
copyout(const void *kaddr, void *uaddr, size_t len)
{
	
	memcpy(uaddr, kaddr, len);

	return (0);
}


int
copystr(const void *kfaddr, void *kdaddr, size_t len, size_t *done)
{
	size_t bytes;
	
	bytes = strlcpy(kdaddr, kfaddr, len);
	if (done != NULL)
		*done = bytes;

	return (0);
}



int
copyinstr(const void *uaddr, void *kaddr, size_t len, size_t *done)
{	
	size_t bytes;
	
	bytes = strlcpy(kaddr, uaddr, len);
	if (done != NULL)
		*done = bytes;

	return (0);
}


int
subyte(void *base, int byte)
{

	*(char *)base = (uint8_t)byte;
	return (0);
}



/*
 * Change the total socket buffer size a user has used.
 */
int
chgsbsize(uip, hiwat, to, max)
	struct	uidinfo	*uip;
	u_int  *hiwat;
	u_int	to;
	rlim_t	max;
{
	int diff;

	diff = to - *hiwat;
	if (diff > 0) {
		if (atomic_fetchadd_long(&uip->ui_sbsize, (long)diff) + diff > max) {
			atomic_subtract_long(&uip->ui_sbsize, (long)diff);
			return (0);
		}
	} else {
		atomic_add_long(&uip->ui_sbsize, (long)diff);
		if (uip->ui_sbsize < 0)
			printf("negative sbsize for uid = %d\n", uip->ui_uid);
	}
	*hiwat = to;
	return (1);
}


/*
 * Return the current (soft) limit for a particular system resource.
 * The which parameter which specifies the index into the rlimit array
 */
rlim_t
lim_cur(struct proc *p, int which)
{
	struct rlimit rl;

	lim_rlimit(p, which, &rl);
	return (rl.rlim_cur);
}

/*
 * Return a copy of the entire rlimit structure for the system limit
 * specified by 'which' in the rlimit structure pointed to by 'rlp'.
 */
void
lim_rlimit(struct proc *p, int which, struct rlimit *rlp)
{

#if 0
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(which >= 0 && which < RLIM_NLIMITS,
	    ("request for invalid resource limit"));
	*rlp = p->p_limit->pl_rlimit[which];
	if (p->p_sysent->sv_fixlimit != NULL)
		p->p_sysent->sv_fixlimit(rlp, which);
#endif
}

int
useracc(void *addr, int len, int rw)
{
	return (1);
}


       
struct proc *
zpfind(pid_t pid)
{

	return (NULL);
}

int
p_cansee(struct thread *td, struct proc *p)
{

	return (0);
}

struct proc *
pfind(pid_t pid)
{

	return (NULL);
}

/*
 * Fill in a struct xucred based on a struct ucred.
 */
void
cru2x(struct ucred *cr, struct xucred *xcr)
{
#if 0
	int ngroups;

	bzero(xcr, sizeof(*xcr));
	xcr->cr_version = XUCRED_VERSION;
	xcr->cr_uid = cr->cr_uid;

	ngroups = MIN(cr->cr_ngroups, XU_NGROUPS);
	xcr->cr_ngroups = ngroups;
	bcopy(cr->cr_groups, xcr->cr_groups,
	    ngroups * sizeof(*cr->cr_groups));
#endif
}

int
cr_cansee(struct ucred *u1, struct ucred *u2)
{

	return (0);
}

int
cr_canseeinpcb(struct ucred *cred, struct inpcb *inp)
{

	return (0);
}

int
securelevel_gt(struct ucred *cr, int level)
{

	return (0);
}


/**
 * @brief Send a 'notification' to userland, using standard ways
 */
void
devctl_notify(const char *system, const char *subsystem, const char *type,
    const char *data)
{
	;	
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
	;	
}


/*
 * Send a SIGIO or SIGURG signal to a process or process group using stored
 * credentials rather than those of the current process.
 */
void
pgsigio(sigiop, sig, checkctty)
	struct sigio **sigiop;
	int sig, checkctty;
{
	printf("SIGIO not supported yet\n");
	abort();
#ifdef notyet
	ksiginfo_t ksi;
	struct sigio *sigio;

	ksiginfo_init(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = SI_KERNEL;

	SIGIO_LOCK();
	sigio = *sigiop;
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}
	if (sigio->sio_pgid > 0) {
		PROC_LOCK(sigio->sio_proc);
		if (CANSIGIO(sigio->sio_ucred, sigio->sio_proc->p_ucred))
			psignal(sigio->sio_proc, sig);
		PROC_UNLOCK(sigio->sio_proc);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		PGRP_LOCK(sigio->sio_pgrp);
		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (CANSIGIO(sigio->sio_ucred, p->p_ucred) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(sigio->sio_pgrp);
	}
	SIGIO_UNLOCK();
#endif
}

void
kproc_exit(int ecode)
{
	panic("kproc_exit unsupported");
}

vm_offset_t
kmem_alloc_contig(vm_map_t map, vm_size_t size, int flags, vm_paddr_t low,
    vm_paddr_t high, unsigned long alignment, unsigned long boundary,
    vm_memattr_t memattr)
{
	return (kmem_malloc(map, size, flags));
}

void
malloc_init(void *data)
{
#ifdef notyet
	struct malloc_type_internal *mtip;
	struct malloc_type *mtp;

	KASSERT(cnt.v_page_count != 0, ("malloc_register before vm_init"));

	mtp = data;
	if (mtp->ks_magic != M_MAGIC)
		panic("malloc_init: bad malloc type magic");

	mtip = uma_zalloc(mt_zone, M_WAITOK | M_ZERO);
	mtp->ks_handle = mtip;

	mtx_lock(&malloc_mtx);
	mtp->ks_next = kmemstatistics;
	kmemstatistics = mtp;
	kmemcount++;
	mtx_unlock(&malloc_mtx);
#endif
}

void
malloc_uninit(void *data)
{
#ifdef notyet
	struct malloc_type_internal *mtip;
	struct malloc_type_stats *mtsp;
	struct malloc_type *mtp, *temp;
	uma_slab_t slab;
	long temp_allocs, temp_bytes;
	int i;

	mtp = data;
	KASSERT(mtp->ks_magic == M_MAGIC,
	    ("malloc_uninit: bad malloc type magic"));
	KASSERT(mtp->ks_handle != NULL, ("malloc_deregister: cookie NULL"));

	mtx_lock(&malloc_mtx);
	mtip = mtp->ks_handle;
	mtp->ks_handle = NULL;
	if (mtp != kmemstatistics) {
		for (temp = kmemstatistics; temp != NULL;
		    temp = temp->ks_next) {
			if (temp->ks_next == mtp) {
				temp->ks_next = mtp->ks_next;
				break;
			}
		}
		KASSERT(temp,
		    ("malloc_uninit: type '%s' not found", mtp->ks_shortdesc));
	} else
		kmemstatistics = mtp->ks_next;
	kmemcount--;
	mtx_unlock(&malloc_mtx);

	/*
	 * Look for memory leaks.
	 */
	temp_allocs = temp_bytes = 0;
	for (i = 0; i < MAXCPU; i++) {
		mtsp = &mtip->mti_stats[i];
		temp_allocs += mtsp->mts_numallocs;
		temp_allocs -= mtsp->mts_numfrees;
		temp_bytes += mtsp->mts_memalloced;
		temp_bytes -= mtsp->mts_memfreed;
	}
	if (temp_allocs > 0 || temp_bytes > 0) {
		printf("Warning: memory type %s leaked memory on destroy "
		    "(%ld allocations, %ld bytes leaked).\n", mtp->ks_shortdesc,
		    temp_allocs, temp_bytes);
	}

	slab = vtoslab((vm_offset_t) mtip & (~UMA_SLAB_MASK));
	uma_zfree_arg(mt_zone, mtip, slab);
#endif
}

void
knote(struct knlist *list, long hint, int lockflags)
{
	
}

void
knlist_destroy(struct knlist *knl)
{
	
}

void
knlist_init_mtx(struct knlist *knl, struct mtx *lock)
{
	
}
