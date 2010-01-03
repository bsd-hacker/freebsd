#undef _KERNEL
#define _WANT_UCRED
#include <stdlib.h>
#include <sys/types.h>
#include <sys/refcount.h>
#include <sys/ucred.h>

struct malloc_type;

void *
unet_malloc(unsigned long size, struct malloc_type *type, int flags)
{

	return (malloc(size));
}

void
unet_free(void *addr, struct malloc_type *type)
{

	free(addr);
}

/*
 * Claim another reference to a ucred structure.
 */
struct ucred *
crhold(struct ucred *cr)
{

	refcount_acquire(&cr->cr_ref);
	return (cr);
}

/*
 * Free a cred structure.  Throws away space when ref count gets to 0.
 */
void
crfree(struct ucred *cr)
{
	if (refcount_release(&cr->cr_ref)) {
		free(cr);
	}
}       

void
panic(const char *fmt, ...)
{

	abort();
}

