/*-
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_descrip.c	8.6 (Berkeley) 4/19/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/conf.h>
#include <sys/domain.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mqueue.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/unistd.h>
#include <sys/user.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <security/audit/audit.h>

#include <vm/uma.h>

#include <ddb/ddb.h>

static MALLOC_DEFINE(M_FILEDESC, "filedesc", "Open file descriptor table");
static MALLOC_DEFINE(M_FILEDESC_TO_LEADER, "filedesc_to_leader",
		     "file desc to leader structures");
static MALLOC_DEFINE(M_SIGIO, "sigio", "sigio structures");

static uma_zone_t file_zone;

#define	vref(v)
#define VREF(x)	  vref((x))
#define knote_fdclose(x, y)


/* Flags for do_dup() */
#define DUP_FIXED	0x1	/* Force fixed allocation */
#define DUP_FCNTL	0x2	/* fcntl()-style errors */

static int do_dup(struct thread *td, int flags, int old, int new,
    register_t *retval);
static int	fd_first_free(struct filedesc *, int, int);
static int	fd_last_used(struct filedesc *, int, int);
static void	fdgrowtable(struct filedesc *, int);
static void	fdunused(struct filedesc *fdp, int fd);
static void	fdused(struct filedesc *fdp, int fd);

/*
 * A process is initially started out with NDFILE descriptors stored within
 * this structure, selected to be enough for typical applications based on
 * the historical limit of 20 open files (and the usage of descriptors by
 * shells).  If these descriptors are exhausted, a larger descriptor table
 * may be allocated, up to a process' resource limit; the internal arrays
 * are then unused.
 */
#define NDFILE		20
#define NDSLOTSIZE	sizeof(NDSLOTTYPE)
#define	NDENTRIES	(NDSLOTSIZE * __CHAR_BIT)
#define NDSLOT(x)	((x) / NDENTRIES)
#define NDBIT(x)	((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define	NDSLOTS(x)	(((x) + NDENTRIES - 1) / NDENTRIES)

/*
 * Storage required per open file descriptor.
 */
#define OFILESIZE (sizeof(struct file *) + sizeof(char))

/*
 * Storage to hold unused ofiles that need to be reclaimed.
 */
struct freetable {
	struct file	**ft_table;
	SLIST_ENTRY(freetable) ft_next;
};

/*
 * Basic allocation of descriptors:
 * one of the above, plus arrays for NDFILE descriptors.
 */
struct filedesc0 {
	struct	filedesc fd_fd;
	/*
	 * ofiles which need to be reclaimed on free.
	 */
	SLIST_HEAD(,freetable) fd_free;
	/*
	 * These arrays are used when the number of open files is
	 * <= NDFILE, and are then pointed to by the pointers above.
	 */
	struct	file *fd_dfiles[NDFILE];
	char	fd_dfileflags[NDFILE];
	NDSLOTTYPE fd_dmap[NDSLOTS(NDFILE)];
};

/*
 * Descriptor management.
 */
volatile int openfiles;			/* actual number of open files */
struct mtx sigio_lock;		/* mtx to protect pointers to sigio */
void	(*mq_fdclose)(struct thread *td, int fd, struct file *fp);

/* A mutex to protect the association between a proc and filedesc. */
static struct mtx	fdesc_mtx;

/*
 * Find the first zero bit in the given bitmap, starting at low and not
 * exceeding size - 1.
 */
static int
fd_first_free(struct filedesc *fdp, int low, int size)
{
	NDSLOTTYPE *map = fdp->fd_map;
	NDSLOTTYPE mask;
	int off, maxoff;

	if (low >= size)
		return (low);

	off = NDSLOT(low);
	if (low % NDENTRIES) {
		mask = ~(~(NDSLOTTYPE)0 >> (NDENTRIES - (low % NDENTRIES)));
		if ((mask &= ~map[off]) != 0UL)
			return (off * NDENTRIES + ffsl(mask) - 1);
		++off;
	}
	for (maxoff = NDSLOTS(size); off < maxoff; ++off)
		if (map[off] != ~0UL)
			return (off * NDENTRIES + ffsl(~map[off]) - 1);
	return (size);
}

/*
 * Find the highest non-zero bit in the given bitmap, starting at low and
 * not exceeding size - 1.
 */
static int
fd_last_used(struct filedesc *fdp, int low, int size)
{
	NDSLOTTYPE *map = fdp->fd_map;
	NDSLOTTYPE mask;
	int off, minoff;

	if (low >= size)
		return (-1);

	off = NDSLOT(size);
	if (size % NDENTRIES) {
		mask = ~(~(NDSLOTTYPE)0 << (size % NDENTRIES));
		if ((mask &= map[off]) != 0)
			return (off * NDENTRIES + flsl(mask) - 1);
		--off;
	}
	for (minoff = NDSLOT(low); off >= minoff; --off)
		if (map[off] != 0)
			return (off * NDENTRIES + flsl(map[off]) - 1);
	return (low - 1);
}

static int
fdisused(struct filedesc *fdp, int fd)
{
        KASSERT(fd >= 0 && fd < fdp->fd_nfiles,
            ("file descriptor %d out of range (0, %d)", fd, fdp->fd_nfiles));
	return ((fdp->fd_map[NDSLOT(fd)] & NDBIT(fd)) != 0);
}

/*
 * Mark a file descriptor as used.
 */
static void
fdused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);
	KASSERT(!fdisused(fdp, fd),
	    ("fd already used"));

	fdp->fd_map[NDSLOT(fd)] |= NDBIT(fd);
	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
	if (fd == fdp->fd_freefile)
		fdp->fd_freefile = fd_first_free(fdp, fd, fdp->fd_nfiles);
}

/*
 * Mark a file descriptor as unused.
 */
static void
fdunused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);
	KASSERT(fdisused(fdp, fd),
	    ("fd is already unused"));
	KASSERT(fdp->fd_ofiles[fd] == NULL,
	    ("fd is still in use"));

	fdp->fd_map[NDSLOT(fd)] &= ~NDBIT(fd);
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
	if (fd == fdp->fd_lastfile)
		fdp->fd_lastfile = fd_last_used(fdp, 0, fd);
}

/*
 * System calls on descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct getdtablesize_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
getdtablesize(struct thread *td, struct getdtablesize_args *uap)
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	td->td_retval[0] =
	    min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);
	return (0);
}

/*
 * Duplicate a file descriptor to a particular value.
 *
 * Note: keep in mind that a potential race condition exists when closing
 * descriptors from a shared descriptor table (via rfork).
 */
#ifndef _SYS_SYSPROTO_H_
struct dup2_args {
	u_int	from;
	u_int	to;
};
#endif
/* ARGSUSED */
int
dup2(struct thread *td, struct dup2_args *uap)
{

	return (do_dup(td, DUP_FIXED, (int)uap->from, (int)uap->to,
		    td->td_retval));
}

/*
 * Duplicate a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct dup_args {
	u_int	fd;
};
#endif
/* ARGSUSED */
int
dup(struct thread *td, struct dup_args *uap)
{

	return (do_dup(td, 0, (int)uap->fd, 0, td->td_retval));
}

/*
 * The file control system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct fcntl_args {
	int	fd;
	int	cmd;
	long	arg;
};
#endif
/* ARGSUSED */
int
fcntl(struct thread *td, struct fcntl_args *uap)
{
	struct flock fl;
	struct oflock ofl;
	intptr_t arg;
	int error;
	int cmd;

	error = 0;
	cmd = uap->cmd;
	switch (uap->cmd) {
	case F_OGETLK:
	case F_OSETLK:
	case F_OSETLKW:
		/*
		 * Convert old flock structure to new.
		 */
		error = copyin((void *)(intptr_t)uap->arg, &ofl, sizeof(ofl));
		fl.l_start = ofl.l_start;
		fl.l_len = ofl.l_len;
		fl.l_pid = ofl.l_pid;
		fl.l_type = ofl.l_type;
		fl.l_whence = ofl.l_whence;
		fl.l_sysid = 0;

		switch (uap->cmd) {
		case F_OGETLK:
		    cmd = F_GETLK;
		    break;
		case F_OSETLK:
		    cmd = F_SETLK;
		    break;
		case F_OSETLKW:
		    cmd = F_SETLKW;
		    break;
		}
		arg = (intptr_t)&fl;
		break;
        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
	case F_SETLK_REMOTE:
                error = copyin((void *)(intptr_t)uap->arg, &fl, sizeof(fl));
                arg = (intptr_t)&fl;
                break;
	default:
		arg = uap->arg;
		break;
	}
	if (error)
		return (error);
	error = kern_fcntl(td, uap->fd, cmd, arg);
	if (error)
		return (error);
	if (uap->cmd == F_OGETLK) {
		ofl.l_start = fl.l_start;
		ofl.l_len = fl.l_len;
		ofl.l_pid = fl.l_pid;
		ofl.l_type = fl.l_type;
		ofl.l_whence = fl.l_whence;
		error = copyout(&ofl, (void *)(intptr_t)uap->arg, sizeof(ofl));
	} else if (uap->cmd == F_GETLK) {
		error = copyout(&fl, (void *)(intptr_t)uap->arg, sizeof(fl));
	}
	return (error);
}

static inline struct file *
fdtofp(int fd, struct filedesc *fdp)
{
	struct file *fp;

	FILEDESC_LOCK_ASSERT(fdp);
	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (NULL);
	return (fp);
}

int
kern_fcntl(struct thread *td, int fd, int cmd, intptr_t arg)
{
	struct filedesc *fdp;
	struct flock *flp;
	struct file *fp;
	struct proc *p;
	char *pop;
	struct vnode *vp;
	int error, flg, tmp;
	int vfslocked;
	u_int old, new;
	uint64_t bsize;

	vfslocked = 0;
	error = 0;
	flg = F_POSIX;
	p = td->td_proc;
	fdp = p->p_fd;

	switch (cmd) {
	case F_DUPFD:
		tmp = arg;
		error = do_dup(td, DUP_FCNTL, fd, tmp, td->td_retval);
		break;

	case F_DUP2FD:
		tmp = arg;
		error = do_dup(td, DUP_FIXED, fd, tmp, td->td_retval);
		break;

	case F_GETFD:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		pop = &fdp->fd_ofileflags[fd];
		td->td_retval[0] = (*pop & UF_EXCLOSE) ? FD_CLOEXEC : 0;
		FILEDESC_SUNLOCK(fdp);
		break;

	case F_SETFD:
		FILEDESC_XLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_XUNLOCK(fdp);
			error = EBADF;
			break;
		}
		pop = &fdp->fd_ofileflags[fd];
		*pop = (*pop &~ UF_EXCLOSE) |
		    (arg & FD_CLOEXEC ? UF_EXCLOSE : 0);
		FILEDESC_XUNLOCK(fdp);
		break;

	case F_GETFL:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		td->td_retval[0] = OFLAGS(fp->f_flag);
		FILEDESC_SUNLOCK(fdp);
		break;

	case F_SETFL:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		do {
			tmp = flg = fp->f_flag;
			tmp &= ~FCNTLFLAGS;
			tmp |= FFLAGS(arg & ~O_ACCMODE) & FCNTLFLAGS;
		} while(atomic_cmpset_int(&fp->f_flag, flg, tmp) == 0);
		tmp = fp->f_flag & FNONBLOCK;
		error = fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
		if (error) {
			fdrop(fp, td);
			break;
		}
		tmp = fp->f_flag & FASYNC;
		error = fo_ioctl(fp, FIOASYNC, &tmp, td->td_ucred, td);
		if (error == 0) {
			fdrop(fp, td);
			break;
		}
		atomic_clear_int(&fp->f_flag, FNONBLOCK);
		tmp = 0;
		(void)fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
		fdrop(fp, td);
		break;

	case F_GETOWN:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		error = fo_ioctl(fp, FIOGETOWN, &tmp, td->td_ucred, td);
		if (error == 0)
			td->td_retval[0] = tmp;
		fdrop(fp, td);
		break;

	case F_SETOWN:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		fhold(fp);
		FILEDESC_SUNLOCK(fdp);
		tmp = arg;
		error = fo_ioctl(fp, FIOSETOWN, &tmp, td->td_ucred, td);
		fdrop(fp, td);
		break;

	case F_SETLK_REMOTE:
		error = priv_check(td, PRIV_NFS_LOCKD);
		if (error)
			return (error);
		flg = F_REMOTE;
		goto do_setlk;

	case F_SETLKW:
		flg |= F_WAIT;
		/* FALLTHROUGH F_SETLK */

	case F_SETLK:
	do_setlk:
		FILEDESC_SLOCK(fdp);
		if ((fp = fdtofp(fd, fdp)) == NULL) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		if (fp->f_type != DTYPE_VNODE) {
			FILEDESC_SUNLOCK(fdp);
			error = EBADF;
			break;
		}
		flp = (struct flock *)arg;
		if (flp->l_whence == SEEK_CUR) {
			if (fp->f_offset < 0 ||
			    (flp->l_start > 0 &&
			     fp->f_offset > OFF_MAX - flp->l_start)) {
				FILEDESC_SUNLOCK(fdp);
				error = EOVERFLOW;
				break;
			}
			flp->l_start += fp->f_offset;
		}

		FILEDESC_SUNLOCK(fdp);
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 * Common code for dup, dup2, fcntl(F_DUPFD) and fcntl(F_DUP2FD).
 */
static int
do_dup(struct thread *td, int flags, int old, int new,
    register_t *retval)
{
	struct filedesc *fdp;
	struct proc *p;
	struct file *fp;
	struct file *delfp;
	int error, holdleaders, maxfd;

	p = td->td_proc;
	fdp = p->p_fd;

	/*
	 * Verify we have a valid descriptor to dup from and possibly to
	 * dup to. Unlike dup() and dup2(), fcntl()'s F_DUPFD should
	 * return EINVAL when the new descriptor is out of bounds.
	 */
	if (old < 0)
		return (EBADF);
	if (new < 0)
		return (flags & DUP_FCNTL ? EINVAL : EBADF);
	PROC_LOCK(p);
	maxfd = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);
	if (new >= maxfd)
		return (flags & DUP_FCNTL ? EINVAL : EMFILE);

	FILEDESC_XLOCK(fdp);
	if (old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	if (flags & DUP_FIXED && old == new) {
		*retval = new;
		FILEDESC_XUNLOCK(fdp);
		return (0);
	}
	fp = fdp->fd_ofiles[old];
	fhold(fp);

	/*
	 * If the caller specified a file descriptor, make sure the file
	 * table is large enough to hold it, and grab it.  Otherwise, just
	 * allocate a new descriptor the usual way.  Since the filedesc
	 * lock may be temporarily dropped in the process, we have to look
	 * out for a race.
	 */
	if (flags & DUP_FIXED) {
		if (new >= fdp->fd_nfiles)
			fdgrowtable(fdp, new + 1);
		if (fdp->fd_ofiles[new] == NULL)
			fdused(fdp, new);
	} else {
		if ((error = fdalloc(td, new, &new)) != 0) {
			FILEDESC_XUNLOCK(fdp);
			fdrop(fp, td);
			return (error);
		}
	}

	/*
	 * If the old file changed out from under us then treat it as a
	 * bad file descriptor.  Userland should do its own locking to
	 * avoid this case.
	 */
	if (fdp->fd_ofiles[old] != fp) {
		/* we've allocated a descriptor which we won't use */
		if (fdp->fd_ofiles[new] == NULL)
			fdunused(fdp, new);
		FILEDESC_XUNLOCK(fdp);
		fdrop(fp, td);
		return (EBADF);
	}
	KASSERT(old != new,
	    ("new fd is same as old"));

	/*
	 * Save info on the descriptor being overwritten.  We cannot close
	 * it without introducing an ownership race for the slot, since we
	 * need to drop the filedesc lock to call closef().
	 *
	 * XXX this duplicates parts of close().
	 */
	delfp = fdp->fd_ofiles[new];
	holdleaders = 0;
	if (delfp != NULL) {
		if (td->td_proc->p_fdtol != NULL) {
			/*
			 * Ask fdfree() to sleep to ensure that all relevant
			 * process leaders can be traversed in closef().
			 */
			fdp->fd_holdleaderscount++;
			holdleaders = 1;
		}
	}

	/*
	 * Duplicate the source descriptor
	 */
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	if (new > fdp->fd_lastfile)
		fdp->fd_lastfile = new;
	*retval = new;

	/*
	 * If we dup'd over a valid file, we now own the reference to it
	 * and must dispose of it using closef() semantics (as if a
	 * close() were performed on it).
	 *
	 * XXX this duplicates parts of close().
	 */
	if (delfp != NULL) {
		knote_fdclose(td, new);
		if (delfp->f_type == DTYPE_MQUEUE)
			mq_fdclose(td, new, delfp);
		FILEDESC_XUNLOCK(fdp);
		(void) closef(delfp, td);
		if (holdleaders) {
			FILEDESC_XLOCK(fdp);
			fdp->fd_holdleaderscount--;
			if (fdp->fd_holdleaderscount == 0 &&
			    fdp->fd_holdleaderswakeup != 0) {
				fdp->fd_holdleaderswakeup = 0;
				wakeup(&fdp->fd_holdleaderscount);
			}
			FILEDESC_XUNLOCK(fdp);
		}
	} else {
		FILEDESC_XUNLOCK(fdp);
	}
	return (0);
}

/*
 * If sigio is on the list associated with a process or process group,
 * disable signalling from the device, remove sigio from the list and
 * free sigio.
 */
void
funsetown(struct sigio **sigiop)
{
	struct sigio *sigio;

	SIGIO_LOCK();
	sigio = *sigiop;
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}
	*(sigio->sio_myref) = NULL;
	if ((sigio)->sio_pgid < 0) {
		struct pgrp *pg = (sigio)->sio_pgrp;
		PGRP_LOCK(pg);
		SLIST_REMOVE(&sigio->sio_pgrp->pg_sigiolst, sigio,
			     sigio, sio_pgsigio);
		PGRP_UNLOCK(pg);
	} else {
		struct proc *p = (sigio)->sio_proc;
		PROC_LOCK(p);
		SLIST_REMOVE(&sigio->sio_proc->p_sigiolst, sigio,
			     sigio, sio_pgsigio);
		PROC_UNLOCK(p);
	}
	SIGIO_UNLOCK();
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
}

/*
 * Free a list of sigio structures.
 * We only need to lock the SIGIO_LOCK because we have made ourselves
 * inaccessible to callers of fsetown and therefore do not need to lock
 * the proc or pgrp struct for the list manipulation.
 */
void
funsetownlst(struct sigiolst *sigiolst)
{
	struct proc *p;
	struct pgrp *pg;
	struct sigio *sigio;

	sigio = SLIST_FIRST(sigiolst);
	if (sigio == NULL)
		return;
	p = NULL;
	pg = NULL;

	/*
	 * Every entry of the list should belong
	 * to a single proc or pgrp.
	 */
	if (sigio->sio_pgid < 0) {
		pg = sigio->sio_pgrp;
		PGRP_LOCK_ASSERT(pg, MA_NOTOWNED);
	} else /* if (sigio->sio_pgid > 0) */ {
		p = sigio->sio_proc;
		PROC_LOCK_ASSERT(p, MA_NOTOWNED);
	}

	SIGIO_LOCK();
	while ((sigio = SLIST_FIRST(sigiolst)) != NULL) {
		*(sigio->sio_myref) = NULL;
		if (pg != NULL) {
			KASSERT(sigio->sio_pgid < 0,
			    ("Proc sigio in pgrp sigio list"));
			KASSERT(sigio->sio_pgrp == pg,
			    ("Bogus pgrp in sigio list"));
			PGRP_LOCK(pg);
			SLIST_REMOVE(&pg->pg_sigiolst, sigio, sigio,
			    sio_pgsigio);
			PGRP_UNLOCK(pg);
		} else /* if (p != NULL) */ {
			KASSERT(sigio->sio_pgid > 0,
			    ("Pgrp sigio in proc sigio list"));
			KASSERT(sigio->sio_proc == p,
			    ("Bogus proc in sigio list"));
			PROC_LOCK(p);
			SLIST_REMOVE(&p->p_sigiolst, sigio, sigio,
			    sio_pgsigio);
			PROC_UNLOCK(p);
		}
		SIGIO_UNLOCK();
		crfree(sigio->sio_ucred);
		free(sigio, M_SIGIO);
		SIGIO_LOCK();
	}
	SIGIO_UNLOCK();
}

/*
 * This is common code for FIOSETOWN ioctl called by fcntl(fd, F_SETOWN, arg).
 *
 * After permission checking, add a sigio structure to the sigio list for
 * the process or process group.
 */
int
fsetown(pid_t pgid, struct sigio **sigiop)
{
	struct proc *proc;
	struct pgrp *pgrp;
	struct sigio *sigio;
	int ret;

	if (pgid == 0) {
		funsetown(sigiop);
		return (0);
	}

	ret = 0;

	/* Allocate and fill in the new sigio out of locks. */
	sigio = malloc(sizeof(struct sigio), M_SIGIO, M_WAITOK);
	sigio->sio_pgid = pgid;
	sigio->sio_ucred = crhold(curthread->td_ucred);
	sigio->sio_myref = sigiop;

	SIGIO_LOCK();
	*sigiop = sigio;
	SIGIO_UNLOCK();
	return (0);

fail:
	sx_sunlock(&proctree_lock);
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
	return (ret);
}

/*
 * This is common code for FIOGETOWN ioctl called by fcntl(fd, F_GETOWN, arg).
 */
pid_t
fgetown(sigiop)
	struct sigio **sigiop;
{
	pid_t pgid;

	SIGIO_LOCK();
	pgid = (*sigiop != NULL) ? (*sigiop)->sio_pgid : 0;
	SIGIO_UNLOCK();
	return (pgid);
}

/*
 * Close a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct close_args {
	int     fd;
};
#endif
/* ARGSUSED */
int
close(td, uap)
	struct thread *td;
	struct close_args *uap;
{

	return (kern_close(td, uap->fd));
}

int
kern_close(td, fd)
	struct thread *td;
	int fd;
{
	struct filedesc *fdp;
	struct file *fp;
	int error;
	int holdleaders;

	error = 0;
	holdleaders = 0;
	fdp = td->td_proc->p_fd;

	AUDIT_SYSCLOSE(td, fd);

	FILEDESC_XLOCK(fdp);
	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	fdp->fd_ofiles[fd] = NULL;
	fdp->fd_ofileflags[fd] = 0;
	fdunused(fdp, fd);
	if (td->td_proc->p_fdtol != NULL) {
		/*
		 * Ask fdfree() to sleep to ensure that all relevant
		 * process leaders can be traversed in closef().
		 */
		fdp->fd_holdleaderscount++;
		holdleaders = 1;
	}

	/*
	 * We now hold the fp reference that used to be owned by the
	 * descriptor array.  We have to unlock the FILEDESC *AFTER*
	 * knote_fdclose to prevent a race of the fd getting opened, a knote
	 * added, and deleteing a knote for the new fd.
	 */
	knote_fdclose(td, fd);
	if (fp->f_type == DTYPE_MQUEUE)
		mq_fdclose(td, fd, fp);
	FILEDESC_XUNLOCK(fdp);

	error = closef(fp, td);
	if (holdleaders) {
		FILEDESC_XLOCK(fdp);
		fdp->fd_holdleaderscount--;
		if (fdp->fd_holdleaderscount == 0 &&
		    fdp->fd_holdleaderswakeup != 0) {
			fdp->fd_holdleaderswakeup = 0;
			wakeup(&fdp->fd_holdleaderscount);
		}
		FILEDESC_XUNLOCK(fdp);
	}
	return (error);
}

/*
 * Close open file descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct closefrom_args {
	int	lowfd;
};
#endif
/* ARGSUSED */
int
closefrom(struct thread *td, struct closefrom_args *uap)
{
	struct filedesc *fdp;
	int fd;

	fdp = td->td_proc->p_fd;
	AUDIT_ARG_FD(uap->lowfd);

	/*
	 * Treat negative starting file descriptor values identical to
	 * closefrom(0) which closes all files.
	 */
	if (uap->lowfd < 0)
		uap->lowfd = 0;
	FILEDESC_SLOCK(fdp);
	for (fd = uap->lowfd; fd < fdp->fd_nfiles; fd++) {
		if (fdp->fd_ofiles[fd] != NULL) {
			FILEDESC_SUNLOCK(fdp);
			(void)kern_close(td, fd);
			FILEDESC_SLOCK(fdp);
		}
	}
	FILEDESC_SUNLOCK(fdp);
	return (0);
}

#if defined(COMPAT_43)
/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct ofstat_args {
	int	fd;
	struct	ostat *sb;
};
#endif
/* ARGSUSED */
int
ofstat(struct thread *td, struct ofstat_args *uap)
{
	struct ostat oub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0) {
		cvtstat(&ub, &oub);
		error = copyout(&oub, uap->sb, sizeof(oub));
	}
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fstat_args {
	int	fd;
	struct	stat *sb;
};
#endif
/* ARGSUSED */
int
fstat(struct thread *td, struct fstat_args *uap)
{
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0)
		error = copyout(&ub, uap->sb, sizeof(ub));
	return (error);
}

int
kern_fstat(struct thread *td, int fd, struct stat *sbp)
{
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);

	if ((error = fget(td, fd, &fp)) != 0)
		return (error);

	AUDIT_ARG_FILE(td->td_proc, fp);

	error = fo_stat(fp, sbp, td->td_ucred, td);
	fdrop(fp, td);
#ifdef KTRACE
	if (error == 0 && KTRPOINT(td, KTR_STRUCT))
		ktrstat(sbp);
#endif
	return (error);
}

/*
 * Grow the file table to accomodate (at least) nfd descriptors.  This may
 * block and drop the filedesc lock, but it will reacquire it before
 * returning.
 */
static void
fdgrowtable(struct filedesc *fdp, int nfd)
{
	struct filedesc0 *fdp0;
	struct freetable *fo;
	struct file **ntable;
	struct file **otable;
	char *nfileflags;
	int nnfiles, onfiles;
	NDSLOTTYPE *nmap;

	FILEDESC_XLOCK_ASSERT(fdp);

	KASSERT(fdp->fd_nfiles > 0,
	    ("zero-length file table"));

	/* compute the size of the new table */
	onfiles = fdp->fd_nfiles;
	nnfiles = NDSLOTS(nfd) * NDENTRIES; /* round up */
	if (nnfiles <= onfiles)
		/* the table is already large enough */
		return;

	/* allocate a new table and (if required) new bitmaps */
	FILEDESC_XUNLOCK(fdp);
	ntable = malloc((nnfiles * OFILESIZE) + sizeof(struct freetable),
	    M_FILEDESC, M_ZERO | M_WAITOK);
	nfileflags = (char *)&ntable[nnfiles];
	if (NDSLOTS(nnfiles) > NDSLOTS(onfiles))
		nmap = malloc(NDSLOTS(nnfiles) * NDSLOTSIZE,
		    M_FILEDESC, M_ZERO | M_WAITOK);
	else
		nmap = NULL;
	FILEDESC_XLOCK(fdp);

	/*
	 * We now have new tables ready to go.  Since we dropped the
	 * filedesc lock to call malloc(), watch out for a race.
	 */
	onfiles = fdp->fd_nfiles;
	if (onfiles >= nnfiles) {
		/* we lost the race, but that's OK */
		free(ntable, M_FILEDESC);
		if (nmap != NULL)
			free(nmap, M_FILEDESC);
		return;
	}
	bcopy(fdp->fd_ofiles, ntable, onfiles * sizeof(*ntable));
	bcopy(fdp->fd_ofileflags, nfileflags, onfiles);
	otable = fdp->fd_ofiles;
	fdp->fd_ofileflags = nfileflags;
	fdp->fd_ofiles = ntable;
	/*
	 * We must preserve ofiles until the process exits because we can't
	 * be certain that no threads have references to the old table via
	 * _fget().
	 */
	if (onfiles > NDFILE) {
		fo = (struct freetable *)&otable[onfiles];
		fdp0 = (struct filedesc0 *)fdp;
		fo->ft_table = otable;
		SLIST_INSERT_HEAD(&fdp0->fd_free, fo, ft_next);
	}
	if (NDSLOTS(nnfiles) > NDSLOTS(onfiles)) {
		bcopy(fdp->fd_map, nmap, NDSLOTS(onfiles) * sizeof(*nmap));
		if (NDSLOTS(onfiles) > NDSLOTS(NDFILE))
			free(fdp->fd_map, M_FILEDESC);
		fdp->fd_map = nmap;
	}
	fdp->fd_nfiles = nnfiles;
}

/*
 * Allocate a file descriptor for the process.
 */
int
fdalloc(struct thread *td, int minfd, int *result)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	int fd = -1, maxfd;

	FILEDESC_XLOCK_ASSERT(fdp);

	if (fdp->fd_freefile > minfd)
		minfd = fdp->fd_freefile;	   

	PROC_LOCK(p);
	maxfd = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);

	/*
	 * Search the bitmap for a free descriptor.  If none is found, try
	 * to grow the file table.  Keep at it until we either get a file
	 * descriptor or run into process or system limits; fdgrowtable()
	 * may drop the filedesc lock, so we're in a race.
	 */
	for (;;) {
		fd = fd_first_free(fdp, minfd, fdp->fd_nfiles);
		if (fd >= maxfd)
			return (EMFILE);
		if (fd < fdp->fd_nfiles)
			break;
		fdgrowtable(fdp, min(fdp->fd_nfiles * 2, maxfd));
	}

	/*
	 * Perform some sanity checks, then mark the file descriptor as
	 * used and return it to the caller.
	 */
	KASSERT(!fdisused(fdp, fd),
	    ("fd_first_free() returned non-free descriptor"));
	KASSERT(fdp->fd_ofiles[fd] == NULL,
	    ("free descriptor isn't"));
	fdp->fd_ofileflags[fd] = 0; /* XXX needed? */
	fdused(fdp, fd);
	*result = fd;
	return (0);
}

/*
 * Check to see whether n user file descriptors are available to the process
 * p.
 */
int
fdavail(struct thread *td, int n)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = td->td_proc->p_fd;
	struct file **fpp;
	int i, lim, last;

	FILEDESC_LOCK_ASSERT(fdp);

	PROC_LOCK(p);
	lim = min((int)lim_cur(p, RLIMIT_NOFILE), maxfilesperproc);
	PROC_UNLOCK(p);
	if ((i = lim - fdp->fd_nfiles) > 0 && (n -= i) <= 0)
		return (1);
	last = min(fdp->fd_nfiles, lim);
	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	for (i = last - fdp->fd_freefile; --i >= 0; fpp++) {
		if (*fpp == NULL && --n <= 0)
			return (1);
	}
	return (0);
}

/*
 * Create a new open file structure and allocate a file decriptor for the
 * process that refers to it.  We add one reference to the file for the
 * descriptor table and one reference for resultfp. This is to prevent us
 * being preempted and the entry in the descriptor table closed after we
 * release the FILEDESC lock.
 */
int
falloc(struct thread *td, struct file **resultfp, int *resultfd)
{
	struct proc *p = td->td_proc;
	struct file *fp;
	int error, i;
	int maxuserfiles = maxfiles - (maxfiles / 20);
	static struct timeval lastfail;
	static int curfail;

	fp = uma_zalloc(file_zone, M_WAITOK | M_ZERO);
	if ((openfiles >= maxuserfiles &&
	    priv_check(td, PRIV_MAXFILES) != 0) ||
	    openfiles >= maxfiles) {
		if (ppsratecheck(&lastfail, &curfail, 1)) {
			printf("kern.maxfiles limit exceeded by uid %i, please see tuning(7).\n",
				td->td_ucred->cr_ruid);
		}
		uma_zfree(file_zone, fp);
		return (ENFILE);
	}
	atomic_add_int(&openfiles, 1);

	/*
	 * If the process has file descriptor zero open, add the new file
	 * descriptor to the list of open files at that point, otherwise
	 * put it at the front of the list of open files.
	 */
	refcount_init(&fp->f_count, 1);
	if (resultfp)
		fhold(fp);
	fp->f_cred = crhold(td->td_ucred);
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fp->f_vnode = NULL;
	FILEDESC_XLOCK(p->p_fd);
	if ((error = fdalloc(td, 0, &i))) {
		FILEDESC_XUNLOCK(p->p_fd);
		fdrop(fp, td);
		if (resultfp)
			fdrop(fp, td);
		return (error);
	}
	p->p_fd->fd_ofiles[i] = fp;
	FILEDESC_XUNLOCK(p->p_fd);
	if (resultfp)
		*resultfp = fp;
	if (resultfd)
		*resultfd = i;
	return (0);
}

static struct filedesc *
fdhold(struct proc *p)
{
	struct filedesc *fdp;

	mtx_lock(&fdesc_mtx);
	fdp = p->p_fd;
	if (fdp != NULL)
		fdp->fd_holdcnt++;
	mtx_unlock(&fdesc_mtx);
	return (fdp);
}

static void
fddrop(struct filedesc *fdp)
{
	struct filedesc0 *fdp0;
	struct freetable *ft;
	int i;

	mtx_lock(&fdesc_mtx);
	i = --fdp->fd_holdcnt;
	mtx_unlock(&fdesc_mtx);
	if (i > 0)
		return;

	FILEDESC_LOCK_DESTROY(fdp);
	fdp0 = (struct filedesc0 *)fdp;
	while ((ft = SLIST_FIRST(&fdp0->fd_free)) != NULL) {
		SLIST_REMOVE_HEAD(&fdp0->fd_free, ft_next);
		free(ft->ft_table, M_FILEDESC);
	}
	free(fdp, M_FILEDESC);
}

/*
 * Share a filedesc structure.
 */
struct filedesc *
fdshare(struct filedesc *fdp)
{

	FILEDESC_XLOCK(fdp);
	fdp->fd_refcnt++;
	FILEDESC_XUNLOCK(fdp);
	return (fdp);
}

/*
 * Unshare a filedesc structure, if necessary by making a copy
 */
void
fdunshare(struct proc *p, struct thread *td)
{

	FILEDESC_XLOCK(p->p_fd);
	if (p->p_fd->fd_refcnt > 1) {
		struct filedesc *tmp;

		FILEDESC_XUNLOCK(p->p_fd);
		tmp = fdcopy(p->p_fd);
		fdfree(td);
		p->p_fd = tmp;
	} else
		FILEDESC_XUNLOCK(p->p_fd);
}

/*
 * Copy a filedesc structure.  A NULL pointer in returns a NULL reference,
 * this is to ease callers, not catch errors.
 */
struct filedesc *
fdcopy(struct filedesc *fdp)
{
	struct filedesc *newfdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return (NULL);

	newfdp = fdinit(fdp);
	FILEDESC_SLOCK(fdp);
	while (fdp->fd_lastfile >= newfdp->fd_nfiles) {
		FILEDESC_SUNLOCK(fdp);
		FILEDESC_XLOCK(newfdp);
		fdgrowtable(newfdp, fdp->fd_lastfile + 1);
		FILEDESC_XUNLOCK(newfdp);
		FILEDESC_SLOCK(fdp);
	}
	/* copy everything except kqueue descriptors */
	newfdp->fd_freefile = -1;
	for (i = 0; i <= fdp->fd_lastfile; ++i) {
		if (fdisused(fdp, i) &&
		    fdp->fd_ofiles[i]->f_type != DTYPE_KQUEUE &&
		    fdp->fd_ofiles[i]->f_ops != &badfileops) {
			newfdp->fd_ofiles[i] = fdp->fd_ofiles[i];
			newfdp->fd_ofileflags[i] = fdp->fd_ofileflags[i];
			fhold(newfdp->fd_ofiles[i]);
			newfdp->fd_lastfile = i;
		} else {
			if (newfdp->fd_freefile == -1)
				newfdp->fd_freefile = i;
		}
	}
	newfdp->fd_cmask = fdp->fd_cmask;
	FILEDESC_SUNLOCK(fdp);
	FILEDESC_XLOCK(newfdp);
	for (i = 0; i <= newfdp->fd_lastfile; ++i)
		if (newfdp->fd_ofiles[i] != NULL)
			fdused(newfdp, i);
	if (newfdp->fd_freefile == -1)
		newfdp->fd_freefile = i;
	FILEDESC_XUNLOCK(newfdp);
	return (newfdp);
}

/*
 * Build a new filedesc structure from another.
 * Copy the current, root, and jail root vnode references.
 */
struct filedesc *
fdinit(struct filedesc *fdp)
{
	struct filedesc0 *newfdp;

	newfdp = malloc(sizeof *newfdp, M_FILEDESC, M_WAITOK | M_ZERO);
	FILEDESC_LOCK_INIT(&newfdp->fd_fd);
	if (fdp != NULL) {
		FILEDESC_XLOCK(fdp);
		newfdp->fd_fd.fd_cdir = fdp->fd_cdir;
		if (newfdp->fd_fd.fd_cdir)
			VREF(newfdp->fd_fd.fd_cdir);
		newfdp->fd_fd.fd_rdir = fdp->fd_rdir;
		if (newfdp->fd_fd.fd_rdir)
			VREF(newfdp->fd_fd.fd_rdir);
		newfdp->fd_fd.fd_jdir = fdp->fd_jdir;
		if (newfdp->fd_fd.fd_jdir)
			VREF(newfdp->fd_fd.fd_jdir);
		FILEDESC_XUNLOCK(fdp);
	}

	/* Create the file descriptor table. */
	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_holdcnt = 1;
	newfdp->fd_fd.fd_cmask = CMASK;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_map = newfdp->fd_dmap;
	newfdp->fd_fd.fd_lastfile = -1;
	return (&newfdp->fd_fd);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(struct thread *td)
{
	struct filedesc *fdp;
	struct file **fpp;
	int i, locked;
	struct filedesc_to_leader *fdtol;
	struct file *fp;
	struct vnode *cdir, *jdir, *rdir, *vp;
	struct flock lf;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

	/* Check for special need to clear POSIX style locks */
	fdtol = td->td_proc->p_fdtol;
	if (fdtol != NULL) {
		FILEDESC_XLOCK(fdp);
		KASSERT(fdtol->fdl_refcount > 0,
			("filedesc_to_refcount botch: fdl_refcount=%d",
			    fdtol->fdl_refcount));
	
	retry:
		if (fdtol->fdl_refcount == 1) {
			if (fdp->fd_holdleaderscount > 0 &&
			    (td->td_proc->p_leader->p_flag & P_ADVLOCK) != 0) {
				/*
				 * close() or do_dup() has cleared a reference
				 * in a shared file descriptor table.
				 */
				fdp->fd_holdleaderswakeup = 1;
				sx_sleep(&fdp->fd_holdleaderscount,
				    FILEDESC_LOCK(fdp), PLOCK, "fdlhold", 0);
				goto retry;
			}
			if (fdtol->fdl_holdcount > 0) {
				/*
				 * Ensure that fdtol->fdl_leader remains
				 * valid in closef().
				 */
				fdtol->fdl_wakeup = 1;
				sx_sleep(fdtol, FILEDESC_LOCK(fdp), PLOCK,
				    "fdlhold", 0);
				goto retry;
			}
		}
		fdtol->fdl_refcount--;
		if (fdtol->fdl_refcount == 0 &&
		    fdtol->fdl_holdcount == 0) {
			fdtol->fdl_next->fdl_prev = fdtol->fdl_prev;
			fdtol->fdl_prev->fdl_next = fdtol->fdl_next;
		} else
			fdtol = NULL;
		td->td_proc->p_fdtol = NULL;
		FILEDESC_XUNLOCK(fdp);
		if (fdtol != NULL)
			free(fdtol, M_FILEDESC_TO_LEADER);
	}
	FILEDESC_XLOCK(fdp);
	i = --fdp->fd_refcnt;
	FILEDESC_XUNLOCK(fdp);
	if (i > 0)
		return;

	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i-- >= 0; fpp++) {
		if (*fpp) {
			FILEDESC_XLOCK(fdp);
			fp = *fpp;
			*fpp = NULL;
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
		}
	}
	FILEDESC_XLOCK(fdp);

	/* XXX This should happen earlier. */
	mtx_lock(&fdesc_mtx);
	td->td_proc->p_fd = NULL;
	mtx_unlock(&fdesc_mtx);

	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);
	if (NDSLOTS(fdp->fd_nfiles) > NDSLOTS(NDFILE))
		free(fdp->fd_map, M_FILEDESC);

	fdp->fd_nfiles = 0;

	cdir = fdp->fd_cdir;
	fdp->fd_cdir = NULL;
	rdir = fdp->fd_rdir;
	fdp->fd_rdir = NULL;
	jdir = fdp->fd_jdir;
	fdp->fd_jdir = NULL;
	FILEDESC_XUNLOCK(fdp);

	fddrop(fdp);
}

/*
 * For setugid programs, we don't want to people to use that setugidness
 * to generate error messages which write to a file which otherwise would
 * otherwise be off-limits to the process.  We check for filesystems where
 * the vnode can change out from under us after execve (like [lin]procfs).
 *
 * Since setugidsafety calls this only for fd 0, 1 and 2, this check is
 * sufficient.  We also don't check for setugidness since we know we are.
 */
static int
is_unsafe(struct file *fp)
{
	return (0);
}

/*
 * Make this setguid thing safe, if at all possible.
 */
void
setugidsafety(struct thread *td)
{
	struct filedesc *fdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

	/*
	 * Note: fdp->fd_ofiles may be reallocated out from under us while
	 * we are blocked in a close.  Be careful!
	 */
	FILEDESC_XLOCK(fdp);
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (i > 2)
			break;
		if (fdp->fd_ofiles[i] && is_unsafe(fdp->fd_ofiles[i])) {
			struct file *fp;

			knote_fdclose(td, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			fdunused(fdp, i);
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
			FILEDESC_XLOCK(fdp);
		}
	}
	FILEDESC_XUNLOCK(fdp);
}

/*
 * If a specific file object occupies a specific file descriptor, close the
 * file descriptor entry and drop a reference on the file object.  This is a
 * convenience function to handle a subsequent error in a function that calls
 * falloc() that handles the race that another thread might have closed the
 * file descriptor out from under the thread creating the file object.
 */
void
fdclose(struct filedesc *fdp, struct file *fp, int idx, struct thread *td)
{

	FILEDESC_XLOCK(fdp);
	if (fdp->fd_ofiles[idx] == fp) {
		fdp->fd_ofiles[idx] = NULL;
		fdunused(fdp, idx);
		FILEDESC_XUNLOCK(fdp);
		fdrop(fp, td);
	} else
		FILEDESC_XUNLOCK(fdp);
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(struct thread *td)
{
	struct filedesc *fdp;
	int i;

	/* Certain daemons might not have file descriptors. */
	fdp = td->td_proc->p_fd;
	if (fdp == NULL)
		return;

	FILEDESC_XLOCK(fdp);

	/*
	 * We cannot cache fd_ofiles or fd_ofileflags since operations
	 * may block and rip them out from under us.
	 */
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (fdp->fd_ofiles[i] != NULL &&
		    (fdp->fd_ofiles[i]->f_type == DTYPE_MQUEUE ||
		    (fdp->fd_ofileflags[i] & UF_EXCLOSE))) {
			struct file *fp;

			knote_fdclose(td, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			fdunused(fdp, i);
			if (fp->f_type == DTYPE_MQUEUE)
				mq_fdclose(td, i, fp);
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
			FILEDESC_XLOCK(fdp);
		}
	}
	FILEDESC_XUNLOCK(fdp);
}

/*
 * Internal form of close.  Decrement reference count on file structure.
 * Note: td may be NULL when closing a file that was being passed in a
 * message.
 *
 * XXXRW: Giant is not required for the caller, but often will be held; this
 * makes it moderately likely the Giant will be recursed in the VFS case.
 */
int
closef(struct file *fp, struct thread *td)
{
	struct vnode *vp;
	struct flock lf;
	struct filedesc_to_leader *fdtol;
	struct filedesc *fdp;

	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor, and the thread pointer
	 * will be NULL.  Callers should be careful only to pass a
	 * NULL thread pointer when there really is no owning
	 * context that might have locks, or the locks will be
	 * leaked.
	 */
	return (fdrop(fp, td));
}

/*
 * Initialize the file pointer with the specified properties.
 * 
 * The ops are set with release semantics to be certain that the flags, type,
 * and data are visible when ops is.  This is to prevent ops methods from being
 * called with bad data.
 */
void
finit(struct file *fp, u_int flag, short type, void *data, struct fileops *ops)
{
	fp->f_data = data;
	fp->f_flag = flag;
	fp->f_type = type;
	atomic_store_rel_ptr((volatile uintptr_t *)&fp->f_ops, (uintptr_t)ops);
}

struct file *
fget_unlocked(struct filedesc *fdp, int fd)
{
	struct file *fp;
	u_int count;

	if (fd < 0 || fd >= fdp->fd_nfiles)
		return (NULL);
	/*
	 * Fetch the descriptor locklessly.  We avoid fdrop() races by
	 * never raising a refcount above 0.  To accomplish this we have
	 * to use a cmpset loop rather than an atomic_add.  The descriptor
	 * must be re-verified once we acquire a reference to be certain
	 * that the identity is still correct and we did not lose a race
	 * due to preemption.
	 */
	for (;;) {
		fp = fdp->fd_ofiles[fd];
		if (fp == NULL)
			break;
		count = fp->f_count;
		if (count == 0)
			continue;
		/*
		 * Use an acquire barrier to prevent caching of fd_ofiles
		 * so it is refreshed for verification.
		 */
		if (atomic_cmpset_acq_int(&fp->f_count, count, count + 1) != 1)
			continue;
		if (fp == fdp->fd_ofiles[fd])
			break;
		fdrop(fp, curthread);
	}

	return (fp);
}

/*
 * Extract the file pointer associated with the specified descriptor for the
 * current user process.
 *
 * If the descriptor doesn't exist or doesn't match 'flags', EBADF is
 * returned.
 *
 * If an error occured the non-zero error is returned and *fpp is set to
 * NULL.  Otherwise *fpp is held and set and zero is returned.  Caller is
 * responsible for fdrop().
 */
static __inline int
_fget(struct thread *td, int fd, struct file **fpp, int flags)
{
	struct filedesc *fdp;
	struct file *fp;

	*fpp = NULL;
	if (td == NULL || (fdp = td->td_proc->p_fd) == NULL)
		return (EBADF);
	if ((fp = fget_unlocked(fdp, fd)) == NULL)
		return (EBADF);
	if (fp->f_ops == &badfileops) {
		fdrop(fp, td);
		return (EBADF);
	}
	/*
	 * FREAD and FWRITE failure return EBADF as per POSIX.
	 *
	 * Only one flag, or 0, may be specified.
	 */
	if ((flags == FREAD && (fp->f_flag & FREAD) == 0) ||
	    (flags == FWRITE && (fp->f_flag & FWRITE) == 0)) {
		fdrop(fp, td);
		return (EBADF);
	}
	*fpp = fp;
	return (0);
}

int
fget(struct thread *td, int fd, struct file **fpp)
{

	return(_fget(td, fd, fpp, 0));
}

int
fget_read(struct thread *td, int fd, struct file **fpp)
{

	return(_fget(td, fd, fpp, FREAD));
}

int
fget_write(struct thread *td, int fd, struct file **fpp)
{

	return(_fget(td, fd, fpp, FWRITE));
}

/*
 * Like fget() but loads the underlying vnode, or returns an error if the
 * descriptor does not represent a vnode.  Note that pipes use vnodes but
 * never have VM objects.  The returned vnode will be vref()'d.
 *
 * XXX: what about the unused flags ?
 */
static __inline int
_fgetvp(struct thread *td, int fd, struct vnode **vpp, int flags)
{
	struct file *fp;
	int error;

	*vpp = NULL;
	if ((error = _fget(td, fd, &fp, flags)) != 0)
		return (error);
	if (fp->f_vnode == NULL) {
		error = EINVAL;
	} else {
		*vpp = fp->f_vnode;
		vref(*vpp);
	}
	fdrop(fp, td);

	return (error);
}

int
fgetvp(struct thread *td, int fd, struct vnode **vpp)
{

	return (_fgetvp(td, fd, vpp, 0));
}

int
fgetvp_read(struct thread *td, int fd, struct vnode **vpp)
{

	return (_fgetvp(td, fd, vpp, FREAD));
}

#ifdef notyet
int
fgetvp_write(struct thread *td, int fd, struct vnode **vpp)
{

	return (_fgetvp(td, fd, vpp, FWRITE));
}
#endif

/*
 * Like fget() but loads the underlying socket, or returns an error if the
 * descriptor does not represent a socket.
 *
 * We bump the ref count on the returned socket.  XXX Also obtain the SX lock
 * in the future.
 *
 * Note: fgetsock() and fputsock() are deprecated, as consumers should rely
 * on their file descriptor reference to prevent the socket from being free'd
 * during use.
 */
int
fgetsock(struct thread *td, int fd, struct socket **spp, u_int *fflagp)
{
	struct file *fp;
	int error;

	*spp = NULL;
	if (fflagp != NULL)
		*fflagp = 0;
	if ((error = _fget(td, fd, &fp, 0)) != 0)
		return (error);
	if (fp->f_type != DTYPE_SOCKET) {
		error = ENOTSOCK;
	} else {
		*spp = fp->f_data;
		if (fflagp)
			*fflagp = fp->f_flag;
		SOCK_LOCK(*spp);
		soref(*spp);
		SOCK_UNLOCK(*spp);
	}
	fdrop(fp, td);

	return (error);
}

/*
 * Drop the reference count on the socket and XXX release the SX lock in the
 * future.  The last reference closes the socket.
 *
 * Note: fputsock() is deprecated, see comment for fgetsock().
 */
void
fputsock(struct socket *so)
{

	ACCEPT_LOCK();
	SOCK_LOCK(so);
	sorele(so);
}

/*
 * Handle the last reference to a file being closed.
 */
int
_fdrop(struct file *fp, struct thread *td)
{
	int error;

	error = 0;
	if (fp->f_count != 0)
		panic("fdrop: count %d", fp->f_count);
	if (fp->f_ops != &badfileops)
		error = fo_close(fp, td);

	atomic_subtract_int(&openfiles, 1);
	crfree(fp->f_cred);
	uma_zfree(file_zone, fp);

	return (error);
}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(struct thread *td, struct filedesc *fdp, int indx, int dfd, int mode, int error)
{
	struct file *wfp;
	struct file *fp;

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, then reject.
	 */
	FILEDESC_XLOCK(fdp);
	if (dfd < 0 || dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and store it in
	 * (indx).  (dfd) is effectively closed by this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag) {
			FILEDESC_XUNLOCK(fdp);
			return (EACCES);
		}
		fp = fdp->fd_ofiles[indx];
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		if (fp == NULL)
			fdused(fdp, indx);
		fhold(wfp);
		FILEDESC_XUNLOCK(fdp);
		if (fp != NULL)
			/*
			 * We now own the reference to fp that the ofiles[]
			 * array used to own.  Release it.
			 */
			fdrop(fp, td);
		return (0);

	case ENXIO:
		/*
		 * Steal away the file pointer from dfd and stuff it into indx.
		 */
		fp = fdp->fd_ofiles[indx];
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofileflags[dfd] = 0;
		fdunused(fdp, dfd);
		if (fp == NULL)
			fdused(fdp, indx);
		FILEDESC_XUNLOCK(fdp);

		/*
		 * We now own the reference to fp that the ofiles[] array
		 * used to own.  Release it.
		 */
		if (fp != NULL)
			fdrop(fp, td);
		return (0);

	default:
		FILEDESC_XUNLOCK(fdp);
		return (error);
	}
	/* NOTREACHED */
}

struct filedesc_to_leader *
filedesc_to_leader_alloc(struct filedesc_to_leader *old, struct filedesc *fdp, struct proc *leader)
{
	struct filedesc_to_leader *fdtol;

	fdtol = malloc(sizeof(struct filedesc_to_leader),
	       M_FILEDESC_TO_LEADER,
	       M_WAITOK);
	fdtol->fdl_refcount = 1;
	fdtol->fdl_holdcount = 0;
	fdtol->fdl_wakeup = 0;
	fdtol->fdl_leader = leader;
	if (old != NULL) {
		FILEDESC_XLOCK(fdp);
		fdtol->fdl_next = old->fdl_next;
		fdtol->fdl_prev = old;
		old->fdl_next = fdtol;
		fdtol->fdl_next->fdl_prev = fdtol;
		FILEDESC_XUNLOCK(fdp);
	} else {
		fdtol->fdl_next = fdtol;
		fdtol->fdl_prev = fdtol;
	}
	return (fdtol);
}

#ifdef KINFO_OFILE_SIZE
CTASSERT(sizeof(struct kinfo_ofile) == KINFO_OFILE_SIZE);
#endif

SYSCTL_INT(_kern, KERN_MAXFILESPERPROC, maxfilesperproc, CTLFLAG_RW,
    &maxfilesperproc, 0, "Maximum files allowed open per process");

SYSCTL_INT(_kern, KERN_MAXFILES, maxfiles, CTLFLAG_RW,
    &maxfiles, 0, "Maximum number of files");

SYSCTL_INT(_kern, OID_AUTO, openfiles, CTLFLAG_RD,
    __DEVOLATILE(int *, &openfiles), 0, "System-wide number of open files");

/* ARGSUSED*/
static void
filelistinit(void *dummy)
{

	file_zone = uma_zcreate("Files", sizeof(struct file), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	mtx_init(&sigio_lock, "sigio lock", NULL, MTX_DEF);
	mtx_init(&fdesc_mtx, "fdesc", NULL, MTX_DEF);
}
SYSINIT(select, SI_SUB_LOCK, SI_ORDER_FIRST, filelistinit, NULL);

/*-------------------------------------------------------------------*/

static int
badfo_readwrite(struct file *fp, struct uio *uio, struct ucred *active_cred, int flags, struct thread *td)
{

	return (EBADF);
}

static int
badfo_truncate(struct file *fp, off_t length, struct ucred *active_cred, struct thread *td)
{

	return (EINVAL);
}

static int
badfo_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred, struct thread *td)
{

	return (EBADF);
}

static int
badfo_poll(struct file *fp, int events, struct ucred *active_cred, struct thread *td)
{

	return (0);
}

static int
badfo_kqfilter(struct file *fp, struct knote *kn)
{

	return (EBADF);
}

static int
badfo_stat(struct file *fp, struct stat *sb, struct ucred *active_cred, struct thread *td)
{

	return (EBADF);
}

static int
badfo_close(struct file *fp, struct thread *td)
{

	return (EBADF);
}

struct fileops badfileops = {
	.fo_read = badfo_readwrite,
	.fo_write = badfo_readwrite,
	.fo_truncate = badfo_truncate,
	.fo_ioctl = badfo_ioctl,
	.fo_poll = badfo_poll,
	.fo_kqfilter = badfo_kqfilter,
	.fo_stat = badfo_stat,
	.fo_close = badfo_close,
};
