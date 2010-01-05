#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/turnstile.h>
#include <sys/vmmeter.h>
#include <sys/lock_profile.h>



void
mtx_init(struct mtx *m, const char *name, const char *type, int opts)
{

	panic("");
}

void
mtx_destroy(struct mtx *m)
{

	panic("");
}

void
mtx_sysinit(void *arg)
{
	
	panic("");
}

void
_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line)
{

	panic("");
}

void
_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line)
{

	panic("");
}



void
rm_init_flags(struct rmlock *rm, const char *name, int opts)
{
	
	panic("");
}

void
rm_destroy(struct rmlock *rm)
{

	panic("");
}

void
_rm_wlock(struct rmlock *rm)
{

	panic("");
}

void
_rm_wunlock(struct rmlock *rm)
{

	panic("");
}

void
_rm_rlock(struct rmlock *rm, struct rm_priotracker *tracker)
{

	panic("");
}

void
_rm_runlock(struct rmlock *rm,  struct rm_priotracker *tracker)
{

	panic("");
}



void
rw_sysinit(void *arg)
{

	panic("");
}

void
rw_init_flags(struct rwlock *rw, const char *name, int opts)
{

	panic("");
}

void
rw_destroy(struct rwlock *rw)
{
	
	panic("");
}

void
_rw_wlock(struct rwlock *rw, const char *file, int line)
{

	panic("");
}

int
_rw_try_wlock(struct rwlock *rw, const char *file, int line)
{

	panic("");
	return (0);
}

void
_rw_wunlock(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
}

void
_rw_rlock(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
}

int
_rw_try_rlock(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
	return (0);
}

void
_rw_runlock(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
}

void
_rw_wlock_hard(struct rwlock *rw, uintptr_t tid, const char *file,
    int line)
{
	
	panic("");
}

void
_rw_wunlock_hard(struct rwlock *rw, uintptr_t tid, const char *file,
    int line)
{
	
	panic("");
}

int
_rw_try_upgrade(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
	return (0);
}

void
_rw_downgrade(struct rwlock *rw, const char *file, int line)
{
	
	panic("");
}



void
sx_init_flags(struct sx *sx, const char *description, int opts)
{

	panic("");
}

void
sx_destroy(struct sx *sx)
{

	panic("");
}

int
_sx_xlock_hard(struct sx *sx, uintptr_t tid, int opts,
    const char *file, int line)
{
	
	panic("");
	return (0);
}

int
_sx_slock_hard(struct sx *sx, int opts, const char *file, int line)
{
	
	panic("");
	return (0);
}

void
_sx_xunlock_hard(struct sx *sx, uintptr_t tid, const char *file, int
    line)
{
	
	panic("");
}

void
_sx_sunlock_hard(struct sx *sx, const char *file, int line)
{
	
	panic("");
}

int
_sx_try_xlock(struct sx *sx, const char *file, int line)
{
	
	panic("");
	return (0);
}

