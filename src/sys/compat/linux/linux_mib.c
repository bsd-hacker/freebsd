/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sdt.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include "opt_compat.h"

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#else
#include <machine/../linux/linux.h>
#endif

#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_dtrace.h>

LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osname, entry);
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osname, sysctl_string_error);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_osname, sysctl_string_error, 0, "int");
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osname, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_osname, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osrelease, entry);
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osrelease, sysctl_string_error);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_osrelease, sysctl_string_error, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_osrelease, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_osrelease, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_oss_version, entry);
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_oss_version, sysctl_string_error);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_oss_version, sysctl_string_error, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_sysctl_oss_version, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_sysctl_oss_version, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_get_prison, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_prison, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_DEFINE(mib, linux_get_prison, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_prison, return, 0,
    "static struct prison *");
LIN_SDT_PROBE_DEFINE(mib, linux_get_osname, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_osname, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_osname, entry, 1,
    "char *");
LIN_SDT_PROBE_DEFINE(mib, linux_get_osname, return);
LIN_SDT_PROBE_DEFINE(mib, linux_set_osname, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osname, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osname, entry, 1,
    "char *");
LIN_SDT_PROBE_DEFINE(mib, linux_set_osname, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osname, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_get_osrelease, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_osrelease, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_osrelease, entry, 1,
    "char *");
LIN_SDT_PROBE_DEFINE(mib, linux_get_osrelease, return);
LIN_SDT_PROBE_DEFINE(mib, linux_use26, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_use26, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_DEFINE(mib, linux_use26, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_use26, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_set_osrelease, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osrelease, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osrelease, entry, 1,
    "char *");
LIN_SDT_PROBE_DEFINE(mib, linux_set_osrelease, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_osrelease, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_get_oss_version, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_oss_version, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_DEFINE(mib, linux_get_oss_version, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_get_oss_version, return, 0,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_set_oss_version, entry);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_oss_version, entry, 0,
    "struct thread *");
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_oss_version, entry, 1,
    "int");
LIN_SDT_PROBE_DEFINE(mib, linux_set_oss_version, return);
LIN_SDT_PROBE_ARGTYPE(mib, linux_set_oss_version, return, 0,
    "int");

struct linux_prison {
	char	pr_osname[LINUX_MAX_UTSNAME];
	char	pr_osrelease[LINUX_MAX_UTSNAME];
	int	pr_oss_version;
	int	pr_use_linux26;	/* flag to determine whether to use 2.6 emulation */
};

SYSCTL_NODE(_compat, OID_AUTO, linux, CTLFLAG_RW, 0,
	    "Linux mode");

static struct mtx osname_lock;
MTX_SYSINIT(linux_osname, &osname_lock, "linux osname", MTX_DEF);

static char	linux_osname[LINUX_MAX_UTSNAME] = "Linux";

static int	linux_set_osname(struct thread *td, char *osname);
static int	linux_set_osrelease(struct thread *td, char *osrelease);
static int	linux_set_oss_version(struct thread *td, int oss_version);

static int
linux_sysctl_osname(SYSCTL_HANDLER_ARGS)
{
	char osname[LINUX_MAX_UTSNAME];
	int error;

	LIN_SDT_PROBE(mib, linux_sysctl_osname, entry, 0, 0, 0, 0, 0);

	linux_get_osname(req->td, osname);
	error = sysctl_handle_string(oidp, osname, LINUX_MAX_UTSNAME, req);
	if (error || req->newptr == NULL) {
		LIN_SDT_PROBE(mib, linux_sysctl_osname, sysctl_string_error,
		    error, 0, 0, 0, 0);
		LIN_SDT_PROBE(mib, linux_sysctl_osname, return, error, 0, 0,
		    0, 0);
		return (error);
	}
	error = linux_set_osname(req->td, osname);

	LIN_SDT_PROBE(mib, linux_sysctl_osname, return, error, 0, 0, 0, 0);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, osname,
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON,
	    0, 0, linux_sysctl_osname, "A",
	    "Linux kernel OS name");

static char	linux_osrelease[LINUX_MAX_UTSNAME] = "2.6.16";
static int	linux_use_linux26 = 1;

static int
linux_sysctl_osrelease(SYSCTL_HANDLER_ARGS)
{
	char osrelease[LINUX_MAX_UTSNAME];
	int error;

	LIN_SDT_PROBE(mib, linux_sysctl_osrelease, entry, 0, 0, 0, 0, 0);

	linux_get_osrelease(req->td, osrelease);
	error = sysctl_handle_string(oidp, osrelease, LINUX_MAX_UTSNAME, req);
	if (error || req->newptr == NULL) {
		LIN_SDT_PROBE(mib, linux_sysctl_osrelease, sysctl_string_error,
		    error, 0, 0, 0, 0);
		LIN_SDT_PROBE(mib, linux_sysctl_osrelease, return, error, 0, 0,
		    0, 0);
		return (error);
	}
	error = linux_set_osrelease(req->td, osrelease);

	LIN_SDT_PROBE(mib, linux_sysctl_osrelease, return, error, 0, 0, 0, 0);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, osrelease,
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON,
	    0, 0, linux_sysctl_osrelease, "A",
	    "Linux kernel OS release");

static int	linux_oss_version = 0x030600;

static int
linux_sysctl_oss_version(SYSCTL_HANDLER_ARGS)
{
	int oss_version;
	int error;

	LIN_SDT_PROBE(mib, linux_sysctl_oss_version, entry, 0, 0, 0, 0, 0);

	oss_version = linux_get_oss_version(req->td);
	error = sysctl_handle_int(oidp, &oss_version, 0, req);
	if (error || req->newptr == NULL) {
		LIN_SDT_PROBE(mib, linux_sysctl_oss_version,
		    sysctl_string_error, error, 0, 0, 0, 0);
		LIN_SDT_PROBE(mib, linux_sysctl_oss_version, return, error, 0,
		    0, 0, 0);
		return (error);
	}
	error = linux_set_oss_version(req->td, oss_version);

	LIN_SDT_PROBE(mib, linux_sysctl_oss_version, return, error, 0, 0, 0, 0);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, oss_version,
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_PRISON,
	    0, 0, linux_sysctl_oss_version, "I",
	    "Linux OSS version");

/*
 * Returns holding the prison mutex if return non-NULL.
 */
static struct prison *
linux_get_prison(struct thread *td)
{
	struct prison *pr;
	struct linux_prison *lpr;

	LIN_SDT_PROBE(mib, linux_get_prison, entry, td, 0, 0, 0, 0);

	KASSERT(td == curthread, ("linux_get_prison() called on !curthread"));
	if (!jailed(td->td_ucred)) {
		LIN_SDT_PROBE(mib, linux_get_prison, return, NULL, 0, 0, 0, 0);
		return (NULL);
	}
	pr = td->td_ucred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_linux == NULL) {
		/*
		 * If we don't have a linux prison structure yet, allocate
		 * one.  We have to handle the race where another thread
		 * could be adding a linux prison to this process already.
		 */
		mtx_unlock(&pr->pr_mtx);
		lpr = malloc(sizeof(struct linux_prison), M_PRISON,
		    M_WAITOK | M_ZERO);
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_linux == NULL)
			pr->pr_linux = lpr;
		else
			free(lpr, M_PRISON);
	}

	LIN_SDT_PROBE(mib, linux_get_prison, return, pr, 0, 0, 0, 0);
	return (pr);
}

void
linux_get_osname(struct thread *td, char *dst)
{
	struct prison *pr;
	struct linux_prison *lpr;

	LIN_SDT_PROBE(mib, linux_get_osname, entry, td, dst, 0, 0, 0);

	pr = td->td_ucred->cr_prison;
	if (pr != NULL) {
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_linux != NULL) {
			lpr = (struct linux_prison *)pr->pr_linux;
			if (lpr->pr_osname[0]) {
				bcopy(lpr->pr_osname, dst, LINUX_MAX_UTSNAME);
				mtx_unlock(&pr->pr_mtx);
				LIN_SDT_PROBE(mib, linux_get_osname, return, 0,
				    0, 0, 0, 0);
				return;
			}
		}
		mtx_unlock(&pr->pr_mtx);
	}

	mtx_lock(&osname_lock);
	bcopy(linux_osname, dst, LINUX_MAX_UTSNAME);
	mtx_unlock(&osname_lock);

	LIN_SDT_PROBE(mib, linux_get_osname, return, 0, 0, 0, 0, 0);
}

static int
linux_set_osname(struct thread *td, char *osname)
{
	struct prison *pr;
	struct linux_prison *lpr;

	LIN_SDT_PROBE(mib, linux_set_osname, entry, td, osname, 0, 0, 0);

	pr = linux_get_prison(td);
	if (pr != NULL) {
		lpr = (struct linux_prison *)pr->pr_linux;
		strcpy(lpr->pr_osname, osname);
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		strcpy(linux_osname, osname);
		mtx_unlock(&osname_lock);
	}

	LIN_SDT_PROBE(mib, linux_set_osname, return, 0, 0, 0, 0, 0);
	return (0);
}

void
linux_get_osrelease(struct thread *td, char *dst)
{
	struct prison *pr;
	struct linux_prison *lpr;

	LIN_SDT_PROBE(mib, linux_get_osrelease, entry, td, dst, 0, 0, 0);

	pr = td->td_ucred->cr_prison;
	if (pr != NULL) {
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_linux != NULL) {
			lpr = (struct linux_prison *)pr->pr_linux;
			if (lpr->pr_osrelease[0]) {
				bcopy(lpr->pr_osrelease, dst,
				    LINUX_MAX_UTSNAME);
				mtx_unlock(&pr->pr_mtx);
				LIN_SDT_PROBE(mib, linux_get_osrelease, return,
				    0, 0, 0, 0, 0);
				return;
			}
		}
		mtx_unlock(&pr->pr_mtx);
	}

	mtx_lock(&osname_lock);
	bcopy(linux_osrelease, dst, LINUX_MAX_UTSNAME);
	mtx_unlock(&osname_lock);

	LIN_SDT_PROBE(mib, linux_get_osrelease, return, 0, 0, 0, 0, 0);
}

int
linux_use26(struct thread *td)
{
	struct prison *pr;
	struct linux_prison *lpr;
	int use26 = linux_use_linux26;

	LIN_SDT_PROBE(mib, linux_use26, entry, td, 0, 0, 0, 0);

	pr = td->td_ucred->cr_prison;
	if (pr != NULL) {
		if (pr->pr_linux != NULL) {
			lpr = (struct linux_prison *)pr->pr_linux;
			use26 = lpr->pr_use_linux26;
		}
	}

	LIN_SDT_PROBE(mib, linux_use26, return, use26, 0, 0, 0, 0);
	return (use26);
}

static int
linux_set_osrelease(struct thread *td, char *osrelease)
{
	struct prison *pr;
	struct linux_prison *lpr;
	int use26;

	LIN_SDT_PROBE(mib, linux_set_osrelease, entry, td, osrelease, 0, 0, 0);

	use26 = (strlen(osrelease) >= 3 && osrelease[2] == '6');

	pr = linux_get_prison(td);
	if (pr != NULL) {
		lpr = (struct linux_prison *)pr->pr_linux;
		strcpy(lpr->pr_osrelease, osrelease);
		lpr->pr_use_linux26 = use26;
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		strcpy(linux_osrelease, osrelease);
		linux_use_linux26 = use26;
		mtx_unlock(&osname_lock);
	}

	LIN_SDT_PROBE(mib, linux_set_osrelease, return, 0, 0, 0, 0, 0);
	return (0);
}

int
linux_get_oss_version(struct thread *td)
{
	struct prison *pr;
	struct linux_prison *lpr;
	int version;

	LIN_SDT_PROBE(mib, linux_get_oss_version, entry, td, 0, 0, 0, 0);

	pr = td->td_ucred->cr_prison;
	if (pr != NULL) {
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_linux != NULL) {
			lpr = (struct linux_prison *)pr->pr_linux;
			if (lpr->pr_oss_version) {
				version = lpr->pr_oss_version;
				mtx_unlock(&pr->pr_mtx);
				LIN_SDT_PROBE(mib, linux_get_oss_version,
				    return, version, 0, 0, 0, 0);
				return (version);
			}
		}
		mtx_unlock(&pr->pr_mtx);
	}

	mtx_lock(&osname_lock);
	version = linux_oss_version;
	mtx_unlock(&osname_lock);

	LIN_SDT_PROBE(mib, linux_get_oss_version, return, version, 0, 0, 0, 0);
	return (version);
}

static int
linux_set_oss_version(struct thread *td, int oss_version)
{
	struct prison *pr;
	struct linux_prison *lpr;

	LIN_SDT_PROBE(mib, linux_set_oss_version, entry, td, oss_version, 0, 0,
	    0);
	pr = linux_get_prison(td);
	if (pr != NULL) {
		lpr = (struct linux_prison *)pr->pr_linux;
		lpr->pr_oss_version = oss_version;
		mtx_unlock(&pr->pr_mtx);
	} else {
		mtx_lock(&osname_lock);
		linux_oss_version = oss_version;
		mtx_unlock(&osname_lock);
	}

	LIN_SDT_PROBE(mib, linux_set_oss_version, return, 0, 0, 0, 0, 0);
	return (0);
}

#ifdef DEBUG
/* XXX: can be removed when every ldebug(...) is removed. */

u_char linux_debug_map[howmany(LINUX_SYS_MAXSYSCALL, sizeof(u_char))];

static int
linux_debug(int syscall, int toggle, int global)
{

	if (global) {
		char c = toggle ? 0 : 0xff;

		memset(linux_debug_map, c, sizeof(linux_debug_map));
		return (0);
	}
	if (syscall < 0 || syscall >= LINUX_SYS_MAXSYSCALL)
		return (EINVAL);
	if (toggle)
		clrbit(linux_debug_map, syscall);
	else
		setbit(linux_debug_map, syscall);
	return (0);
}

/*
 * Usage: sysctl linux.debug=<syscall_nr>.<0/1>
 *
 *    E.g.: sysctl linux.debug=21.0
 *
 * As a special case, syscall "all" will apply to all syscalls globally.
 */
#define LINUX_MAX_DEBUGSTR	16
static int
linux_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	char value[LINUX_MAX_DEBUGSTR], *p;
	int error, sysc, toggle;
	int global = 0;

	value[0] = '\0';
	error = sysctl_handle_string(oidp, value, LINUX_MAX_DEBUGSTR, req);
	if (error || req->newptr == NULL)
		return (error);
	for (p = value; *p != '\0' && *p != '.'; p++);
	if (*p == '\0')
		return (EINVAL);
	*p++ = '\0';
	sysc = strtol(value, NULL, 0);
	toggle = strtol(p, NULL, 0);
	if (strcmp(value, "all") == 0)
		global = 1;
	error = linux_debug(sysc, toggle, global);
	return (error);
}

SYSCTL_PROC(_compat_linux, OID_AUTO, debug,
            CTLTYPE_STRING | CTLFLAG_RW,
            0, 0, linux_sysctl_debug, "A",
            "Linux debugging control");

#endif /* DEBUG */
