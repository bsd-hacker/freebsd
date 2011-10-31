/*
 * SYS/VARSYM.H
 *
 *	Implements structures used for variant symlink support.
 * 
 * $FreeBSD$
 * $DragonFly: src/sys/sys/varsym.h,v 1.3 2005/01/14 02:25:08 joerg Exp $
 */

#ifndef _SYS_VARSYM_H_
#define _SYS_VARSYM_H_
#include <sys/queue.h>		/* TAILQ_* macros */

extern int varsym_enable;

struct varsym {
    u_int	vs_refs;	/* a lot of sharing occurs */
    int		vs_namelen;
    char	*vs_name;	/* variable name */
    char	*vs_data;	/* variable contents */
};
typedef struct varsym	*varsym_t;

struct varsyment {
    TAILQ_ENTRY(varsyment) ve_entry;
    varsym_t	ve_sym;
};

struct varsymset {
    TAILQ_HEAD(, varsyment) vx_queue;
    int		vx_setsize;
};

#define VARSYM_ALL		0
#define VARSYM_SYS		1
#define VARSYM_PROC		2
#define VARSYM_PROC_PRIV	3

#define MAXVARSYM_NAME	64
#define MAXVARSYM_DATA	256

#ifdef _KERNEL
#include <sys/sysproto.h>
void	varsymset_init(struct varsymset *varsymset, struct varsymset *copy);
void	varsymset_clean(struct varsymset *varsymset);
int	varsymreplace(char *cp, int linklen, int maxlen);
int	kern_varsym_get(struct thread *td, int scope, id_t which,
	    const char *uname, char *ubuf, size_t *bufsize);
int	kern_varsym_list(struct thread *td, int scope, id_t which, char *ubuf,
	    size_t *bufsize);
/*
int	varsym_set(struct thread *td, struct varsym_set_args *uap);
int	varsym_get(struct thread *td, struct varsym_get_args *uap);
int	varsym_list(struct thread *td, struct varsym_list_args *uap);
*/

#else /* _KERNEL */
int	varsym_set(int scope, id_t which, const char *name, const char *data);
int	varsym_get(int scope, id_t which, const char *name, char *buf, size_t *size);
int	varsym_list(int scope, id_t which, char *buf, size_t *size);
#endif	/* _KERNEL */

#endif
