#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/systm.h>

/*
 * ABI for arguments. Arguments should be aligned by
 * register size.
 */
#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? \
	0 : sizeof(register_t) - sizeof(t))
#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t)	0
#define PADR_(t)	PAD_(t)
#else
#define PADL_(t)	PAD_(t)
#define PADR_(t)	0
#endif
struct foo_args {
	char d_l_[PADL_(int)]; int d; char d_r_[PADR_(int)];
	char p_l_[PADL_(int)]; void *p; char p_r_[PADR_(int)];
};

/*
 * The function implementing the syscall.
 */
static int
foo_syscall(struct thread *td, void *arg)
{
	struct foo_args *args = (struct foo_args *)arg;

	printf("arguments %d %p\n",
	    args->d, args->p);
	return (0);
}

/*
 * The `sysent' for the new syscall.
 */
static struct sysent foo_sysent = {
	2,		/* sy_narg */
	foo_syscall	/* sy_call */
};

/*
 * The offset in sysent where the syscall is allocated.
 */
static int offset = NO_SYSCALL;

/*
 * The function called at load/unload.
 */
static int
foo_load(struct module *module, int cmd, void *arg)
{

	switch (cmd) {
	case MOD_LOAD :
		printf("syscall loaded at %d\n", offset);
		break;
	case MOD_UNLOAD :
		printf("syscall unloaded from %d\n", offset);
		break;
	default :
		return (EOPNOTSUPP);
	}

	return (0);
}

SYSCALL_MODULE(foo_syscall, &offset, &foo_sysent, foo_load, NULL);
