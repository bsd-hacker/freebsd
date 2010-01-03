#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/refcount.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/priv.h>

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
