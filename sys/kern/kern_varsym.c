/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * Copyright (c) 2007-2009 The Aerospace Corporation.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD$
 * $DragonFly: src/sys/kern/kern_varsym.c,v 1.6 2005/01/14 02:25:08 joerg Exp $
 */

/*
 * This module implements variable storage and management for variant
 * symlinks.  These variables may also be used for general purposes.
 */

#include "opt_varsym.h"

#include <sys/param.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ucred.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/varsym.h>
#include <sys/sysproto.h>

#ifdef VARSYM

SYSCTL_NODE(_vfs, OID_AUTO, varsym, CTLFLAG_RD, NULL,
    "Variant symlink managment");

int varsym_enable = 0;
SYSCTL_INT(_vfs_varsym, OID_AUTO, enable, CTLFLAG_RW, &varsym_enable, 0,
    "Enable Variant Symlinks");
TUNABLE_INT("vfs.varsym.enable",
    &varsym_enable);

static int varsym_allow_default = 0;
SYSCTL_INT(_vfs_varsym, OID_AUTO, allow_default, CTLFLAG_RW,
    &varsym_allow_default, 0,
    "allow variables to have default values");
TUNABLE_INT("vfs.varsym.allow_default",
    &varsym_allow_default);

static int varsym_max_proc_setsize = 32;
SYSCTL_INT(_vfs_varsym, OID_AUTO, max_proc_setsize, CTLFLAG_RW,
    &varsym_max_proc_setsize, 0,
    "maximum number of varsym variables on a process");
TUNABLE_INT("vfs.varsym_max_proc_setsize",
    &varsym_max_proc_setsize);

static int unprivileged_varsym_set_proc = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_varsym_set_proc, CTLFLAG_RW,
    &unprivileged_varsym_set_proc, 0,
    "allow unprivileged users to set per-process varsym variables");
TUNABLE_INT("security.bsd.unprivileged_varsym_set_proc",
    &unprivileged_varsym_set_proc);

MALLOC_DEFINE(M_VARSYM, "varsym", "variable sets for variant symlinks");
static struct mtx	varsym_mutex;
static struct varsymset	varsymset_sys;

static int	varsymmake(int scope, const char *name, const char *data);
static void	varsymdrop(varsym_t var);
static struct varsyment * varsymlookup(struct varsymset *vss, const char *name,
                    int namelen);
static int	varsym_clear(struct thread *td, int scope, id_t which);
static int	vss2buf(struct varsymset *vss, char *buf, int *bytes,
		    int maxsize);
static varsym_t	varsymfind(int scope, const char *name, int namelen);

/*
 * Initialize the variant symlink subsystem
 */
static void
varsym_sysinit(void *dummy)
{
	mtx_init(&varsym_mutex, "varsym", NULL, MTX_DEF);
	varsymset_init(&varsymset_sys, NULL);
}
SYSINIT(announce, SI_SUB_INTRINSIC, SI_ORDER_FIRST, varsym_sysinit, NULL);

/*
 * Initialize the varsymset for proc0
 */
static void
varsym_p0init(void *dummy)
{
	PROC_LOCK(&proc0);
	varsymset_init(&proc0.p_varsymset, NULL);
	varsymset_init(&proc0.p_varsymset_priv, NULL);
	PROC_UNLOCK(&proc0);
}
SYSINIT(p0init, SI_SUB_INTRINSIC, SI_ORDER_SECOND, varsym_p0init, NULL);

/*
 * varsymreplace() - called from namei
 *
 *	Do variant symlink variable substitution
 */
int
varsymreplace(char *cp, int linklen, int maxlen)
{
	int rlen;
	int xlen;
	int nlen;
	int i;
	int colon;
	varsym_t var;

	KASSERT(linklen <= maxlen, ("%s: linklen > maxlen", __func__));

	/*
	 * XXX: should bail here if there are no varsym variables in scope
	 * unless we want to change the code to expand failed lookups to ""
	 * instead of leaving them as literals.
	 */

	rlen = linklen;
	while (linklen > 1) {
		if (cp[0] == '%' && cp[1] == '{') {
			colon = 0;
			for (i = 2; i < linklen; ++i) {
				if (cp[i] == ':' && colon == 0)
					colon = i;
				if (cp[i] == '}')
					break;
			}
			if (i == linklen)
				break;
			var = varsymfind(VARSYM_ALL, cp + 2,
			    (colon != 0 ? colon : i) - 2);
			if (var != NULL ||
			    (colon != 0 && varsym_allow_default)) 
			{
				xlen = i + 1;		/* bytes to strike */
				/* bytes to add */
				if (var != NULL)
					nlen = strlen(var->vs_data);
				else
					nlen = i - (colon + 1);
				if (linklen + nlen - xlen >= maxlen)
					return(-1);
				KASSERT(linklen >= xlen,
				    ("%s: linklen < xlen", __func__));
				if (var != NULL) {
					if (linklen != xlen)
						bcopy(cp + xlen, cp + nlen, linklen - xlen);
					bcopy(var->vs_data, cp, nlen);
					varsymdrop(var);
				} else {
					cp[i] = '\0';
					bcopy(cp + colon + 1, cp, nlen);
					bcopy(cp + xlen, cp + nlen, linklen - xlen);
				}
				linklen += nlen - xlen;	/* new relative length */
				rlen += nlen - xlen;	/* returned total length */
				cp += nlen;		/* adjust past replacement */
				linklen -= nlen;	/* adjust past replacement */
				maxlen -= nlen;		/* adjust past replacement */
			} else {
				/*
				 * It's ok if i points to the '}', it will simply be
				 * skipped.  We could also have hit linklen.
				 */
				cp += i;
				linklen -= i;
				maxlen -= i;
			}
		} else {
			++cp;
			--linklen;
			--maxlen;
		}
	}
	return(rlen);
}

/*
 * varsym_set() system call
 *
 * (int scope, id_t which, const char *name, const char *data)
 */
int
sys_varsym_set(struct thread *td, struct varsym_set_args *uap)
{
	char name[MAXVARSYM_NAME];
	char *buf = NULL;
	int error;
	varsym_t sym;

	if (!varsym_enable)
		return(ENOSYS);

	if (uap->name == NULL)
		return (varsym_clear(td, uap->scope, uap->which));

	if ((error = copyinstr(uap->name, name, sizeof(name), NULL)) != 0)
		return(error);

	if (uap->data) {
		buf = malloc(MAXVARSYM_DATA, M_VARSYM, M_WAITOK);
		error = copyinstr(uap->data, buf, MAXVARSYM_DATA, NULL);
		if (error != 0)
			goto done;
	}

	switch(uap->scope) {
	case VARSYM_PROC:
		/* Disallow operation on other procs for now */
		if (uap->which != 0 && uap->which != td->td_proc->p_pid) {
			error = EPERM;
			break;
		}

		if (!unprivileged_varsym_set_proc) {
			error = priv_check(td, PRIV_VARSYM_SET_PROC);
		} else if (buf != NULL) {
			/*
			 * If normal users are allowed to set process varsym
			 * values, then enforce a maximum number per process.
			 * We do not enforce the limit in the privleged case
			 * because doing so increases the risk of security
			 * problems due to buggy applications that expect
			 * varsym_set() to always succeed.
			 */
			if ((sym = varsymfind(uap->scope, name, strlen(name)))
			    != NULL) {
				varsymdrop(sym);
			} else {
				PROC_LOCK(td->td_proc);
				if (td->td_proc->p_varsymset.vx_setsize ==
				    varsym_max_proc_setsize)
					error = E2BIG;
				PROC_UNLOCK(td->td_proc);
			}
		}
		break;
	case VARSYM_PROC_PRIV:
		/* Disallow operation on other procs for now */
		if (uap->which != 0 && uap->which != td->td_proc->p_pid) {
			error = EPERM;
			break;
		}

		error = priv_check(td, PRIV_VARSYM_SET_PROC_PRIV);
		break;
	case VARSYM_SYS:
		if (uap->which != 0) {
			error = EINVAL;
			break;
		}
		/* 
	 	 * Only root can change SYSTEM's varsyms. 
		 */
		error = priv_check(td, PRIV_VARSYM_SET_SYS);
		break;
	default:
		error = EINVAL;
	}

	if (error == 0) {
		(void) varsymmake(uap->scope, name, NULL);
		if (uap->data != NULL) 
			error = varsymmake(uap->scope, name, buf);
	}
done:
	free(buf, M_TEMP);
	return(error);
}

static int
varsym_clear(struct thread *td, int scope, id_t which)
{
	int error;
	struct proc *p;

	switch (scope) {
	case VARSYM_PROC:
	case VARSYM_PROC_PRIV:
		/* Disallow operation on other procs */
		if (which != 0 && which != td->td_proc->p_pid)
			return (EPERM);

		error = priv_check(td, (scope == VARSYM_PROC) ?
		    PRIV_VARSYM_SET_PROC : PRIV_VARSYM_SET_PROC_PRIV);
		if (error != 0)
			return (error);

		p = td->td_proc;
		PROC_LOCK(p);
		varsymset_clean((scope == VARSYM_PROC) ?
		    &p->p_varsymset : &p->p_varsymset_priv);
		PROC_UNLOCK(p);
		break;
	case VARSYM_SYS:
		if (which != 0)
			return(EINVAL);

		error = priv_check(td, PRIV_VARSYM_SET_SYS);
		if (error != 0)
			return (error);

		mtx_lock(&varsym_mutex);
		varsymset_clean(&varsymset_sys);
		mtx_unlock(&varsym_mutex);
		break;
	default:
		return(EINVAL);
	}

	return(0);
}

/*
 * varsym_get() system call
 *
 * (int scope, id_t which, const char *name, char *buf, size_t *size)
 */
int
sys_varsym_get(struct thread *td, struct varsym_get_args *uap)
{
	int error;
	size_t bufsize;

	if ((error = copyin(uap->size, &bufsize, sizeof(bufsize))) != 0)
		return(error);

	if ((error = kern_varsym_get(td, uap->scope, uap->which, uap->name,
	    uap->buf, &bufsize)) != 0)
		return(error);

	error = copyout(&bufsize, uap->size, sizeof(bufsize));

	return(error);
}

int
kern_varsym_get(struct thread *td, int scope, id_t which, const char *uname,
    char *ubuf, size_t *bufsize)
{
	int error;
	char name[MAXVARSYM_NAME];
	varsym_t sym;
	size_t dlen;

	if (!varsym_enable)
		return(ENOSYS);

	if ((error = copyinstr(uname, name, sizeof(name), NULL)) != 0)
		return(error);
	switch (scope) {
	case VARSYM_PROC:
	case VARSYM_PROC_PRIV:
		/* Disallow operation on other procs */
		if (which != 0 && which != td->td_proc->p_pid)
			return(EPERM);
		break;
	case VARSYM_SYS:
		if (which != 0)
			return(EINVAL);
		break;
	default:
		return(EINVAL);
	}

	sym = varsymfind(scope, name, strlen(name));
	if (sym == NULL)
		return(ENOENT);

	dlen = strlen(sym->vs_data);
	if (dlen < *bufsize)
		error = copyout(sym->vs_data, ubuf, dlen + 1);
	else 
		error = EOVERFLOW; /* buffer too small */
	if (error == 0)
		*bufsize = dlen + 1;

	varsymdrop(sym);

	return (error);
}

/*
 * varsym_list() system call
 *
 * (int scope, id_t which, char *buf, size_t *size)
 */
int
sys_varsym_list(struct thread *td, struct varsym_list_args *uap)
{
	int error;
	size_t bufsize;

	if ((error = copyin(uap->size, &bufsize, sizeof(bufsize))) != 0)
		return(error);

	if ((error = kern_varsym_list(td, uap->scope, uap->which, uap->buf,
	    &bufsize)) != 0)
		return(error);

	if (error == 0)
		error = copyout(&bufsize, uap->size, sizeof(bufsize));

	return(error);
}

int
kern_varsym_list(struct thread *td, int scope, id_t which, char *ubuf,
    size_t *bufsize)
{
	struct proc *p = NULL;
	char *buf;
	int error;
	int bytes;

	if (!varsym_enable)
		return(ENOSYS);

	if (*bufsize > MAXPHYS)
		*bufsize = MAXPHYS;
	buf = malloc(*bufsize, M_VARSYM, M_WAITOK|M_ZERO);
	bytes = 0;

	/*
	 * Figure out the varsym set.
	 */
	switch (scope) {
	case VARSYM_PROC:
	case VARSYM_PROC_PRIV:
		/* Disallow operation on other procs */
		if (which != 0 && which != td->td_proc->p_pid) {
			error = EPERM;
			goto done;
		}
		p = td->td_proc;
		PROC_LOCK(p);
		error = vss2buf(scope == VARSYM_PROC ?
		    &p->p_varsymset : &p->p_varsymset_priv,
		    buf, &bytes, *bufsize);
		PROC_UNLOCK(p);
		break;
	case VARSYM_SYS:
		if (which != 0) {
			error = EINVAL;
			goto done;
		}
		mtx_lock(&varsym_mutex);
		error = vss2buf(&varsymset_sys, buf, &bytes, *bufsize);
		mtx_unlock(&varsym_mutex);
		break;
	default:
		error=EINVAL;
		break;
	}

	if (error == 0) {
		error = copyout(buf, ubuf, bytes);
		*bufsize = bytes;
	}

done:
	free(buf, M_VARSYM);

	return(error);
}

static int
vss2buf(struct varsymset *vss, char *buf, int *bytes, int maxsize)
{
	struct varsyment *ve;
	varsym_t sym;
	int namelen, datalen, totlen;
	int startbytes;

	startbytes = *bytes;

	TAILQ_FOREACH(ve, &vss->vx_queue, ve_entry) {
		sym = ve->ve_sym;

		namelen = strlen(sym->vs_name);
		datalen = strlen(sym->vs_data);
		totlen = namelen + datalen + 2;
		if (*bytes + totlen > maxsize)
			return (EOVERFLOW);

		bcopy(sym->vs_name, buf + *bytes, namelen + 1);
		*bytes += namelen + 1;
		bcopy(sym->vs_data, buf + *bytes, datalen + 1);
		*bytes += datalen + 1;
	}

	return(0);
}

/*
 * Lookup a variant symlink.  XXX use a hash table.
 */
static
struct varsyment *
varsymlookup(struct varsymset *vss, const char *name, int namelen)
{
	struct varsyment *ve;

	TAILQ_FOREACH(ve, &vss->vx_queue, ve_entry) {
		varsym_t var = ve->ve_sym;
		if (var->vs_namelen == namelen && 
		    bcmp(name, var->vs_name, namelen) == 0) 
			return(ve);
	}
	return(NULL);
}

static varsym_t
varsymfind(int scope, const char *name, int namelen)
{
	struct proc *p;
	struct varsyment *ve = NULL;
	varsym_t sym = NULL;

	/*
	 * system variable override per-process variables
	 */
	if (scope == VARSYM_ALL || scope == VARSYM_SYS) {
		mtx_lock(&varsym_mutex);
		ve = varsymlookup(&varsymset_sys, name, namelen);
		if (ve != NULL) {
			sym = ve->ve_sym;
			refcount_acquire(&sym->vs_refs);
		}
		mtx_unlock(&varsym_mutex);
	}
	if (sym == NULL && (scope == VARSYM_ALL || scope == VARSYM_PROC_PRIV)) {
		p = curproc;
		PROC_LOCK(p);
		ve = varsymlookup(&p->p_varsymset_priv, name, namelen);
		if (ve != NULL) {
			sym = ve->ve_sym;
			refcount_acquire(&sym->vs_refs);
		}
		PROC_UNLOCK(p);
	}
	if (sym == NULL && (scope == VARSYM_ALL || scope == VARSYM_PROC)) {
		p = curproc;
		PROC_LOCK(p);
		ve = varsymlookup(&p->p_varsymset, name, namelen);
		if (ve != NULL) {
			sym = ve->ve_sym;
			refcount_acquire(&sym->vs_refs);
		}
		PROC_UNLOCK(p);
	}
	return(sym);
}

static int
varsymmake(int scope, const char *name, const char *data)
{
	varsym_t sym = NULL;
	struct varsymset *vss = NULL;
	struct varsyment *ve = NULL;
	struct proc *p = NULL;
	int namelen = strlen(name);
	int datalen = 0;
	int error;

	if (data != NULL) {
		datalen = strlen(data);
		ve = malloc(sizeof(struct varsyment), M_VARSYM, M_WAITOK|M_ZERO);
		sym = malloc(sizeof(struct varsym) + namelen + datalen + 2,
		    M_VARSYM, M_WAITOK);
		ve->ve_sym = sym;
		refcount_init(&sym->vs_refs, 1);
		sym->vs_namelen = namelen;
		sym->vs_name = (char *)(sym + 1);
		sym->vs_data = sym->vs_name + namelen + 1;
		strcpy(sym->vs_name, name);
		strcpy(sym->vs_data, data);
	}

	switch(scope) {
	case VARSYM_PROC:
	case VARSYM_PROC_PRIV:
		p = curproc;
		PROC_LOCK(p);
		vss = (scope == VARSYM_PROC) ?
		    &p->p_varsymset : &p->p_varsymset_priv;
		break;
	case VARSYM_SYS:
		mtx_lock(&varsym_mutex);
		vss = &varsymset_sys;
		break;
	}
	if (vss == NULL) {
		error = EINVAL;
	} else if (data != NULL) {
		TAILQ_INSERT_TAIL(&vss->vx_queue, ve, ve_entry);
		vss->vx_setsize++;
		error = 0;
	} else {
		if ((ve = varsymlookup(vss, name, namelen)) != NULL) {
			TAILQ_REMOVE(&vss->vx_queue, ve, ve_entry);
			vss->vx_setsize--;
			varsymdrop(ve->ve_sym);
			free(ve, M_VARSYM);
			error = 0;
		} else {
			error = ENOENT;
		}
	}
	switch(scope) {
	case VARSYM_PROC:
	case VARSYM_PROC_PRIV:
		PROC_UNLOCK(p);
		break;
	case VARSYM_SYS:
		mtx_unlock(&varsym_mutex);
		break;
	}

	if (data != NULL && error != 0) {
		varsymdrop(sym);
		free(ve, M_VARSYM);
	}

	return(error);
}

static void
varsymdrop(varsym_t sym)
{
	KASSERT(sym->vs_refs > 0, ("%s: sym->vs_refs <= 0", __func__));
	if (refcount_release(&sym->vs_refs)) {
		free(sym, M_VARSYM);
	}
}

void
varsymset_init(struct varsymset *vss, struct varsymset *copy)
{
	struct varsyment *ve, *nve;

	TAILQ_INIT(&vss->vx_queue);

	if (copy != NULL) {
		TAILQ_FOREACH(ve, &copy->vx_queue, ve_entry) {
			nve = malloc(sizeof(struct varsyment), M_VARSYM, M_WAITOK|M_ZERO);
			nve->ve_sym = ve->ve_sym;
			refcount_acquire(&nve->ve_sym->vs_refs);
			TAILQ_INSERT_TAIL(&vss->vx_queue, nve, ve_entry);
		}
		vss->vx_setsize = copy->vx_setsize;
	}
}

void
varsymset_clean(struct varsymset *vss)
{
	struct varsyment *ve;

	while ((ve = TAILQ_FIRST(&vss->vx_queue)) != NULL) {
		TAILQ_REMOVE(&vss->vx_queue, ve, ve_entry);
		varsymdrop(ve->ve_sym);
		free(ve, M_VARSYM);
	}
	vss->vx_setsize = 0;
}

#else /* VARSYM */
int
sys_varsym_set(struct thread *td, struct varsym_set_args *uap)
{
	return (ENOSYS);
}

int
sys_varsym_get(struct thread *td, struct varsym_get_args *uap)
{
	return (ENOSYS);
}

int
sys_varsym_list(struct thread *td, struct varsym_list_args *uap)
{
	return (ENOSYS);
}

#endif /* VARSYM */
