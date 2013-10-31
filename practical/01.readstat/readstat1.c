#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/tree.h>

#include <vm/uma.h>

static struct mtx tree_mtx;
static int (*original_read)(struct thread *, void *);

struct elm {
	LIST_ENTRY(elm) entry;
	size_t size;
	u_int count;
};

struct node {
	RB_ENTRY(node) tree;
	LIST_HEAD(,elm) elms;
	struct mtx list_mtx;
	char name[MAXCOMLEN+1];
};

static int
node_compare(struct node *a, struct node *b)
{

	return (strcmp(a->name, b->name));
}

static RB_HEAD(head, node) readstat = RB_INITIALIZER(&readstat);
static RB_PROTOTYPE(head, node, tree, node_compare);
static RB_GENERATE(head, node, tree, node_compare);

static MALLOC_DEFINE(M_LEAF, "rsnode", "read stat node");
static MALLOC_DEFINE(M_ELM, "rselm", "read stat element");

static int
stat_read(struct thread *td, void *v)
{
	struct read_args *uap = (struct read_args *)v;
	struct node *key, *node;
	struct elm *elm, *elm0;

	key = malloc(sizeof(*node), M_LEAF, M_WAITOK | M_ZERO);
	mtx_init(&key->list_mtx, "list lock", NULL, MTX_DEF);
	LIST_INIT(&key->elms);

	elm0 = malloc(sizeof(*elm), M_ELM, M_WAITOK | M_ZERO);

	strcpy(key->name, td->td_proc->p_comm);

	mtx_lock(&tree_mtx);
	node = RB_INSERT(head, &readstat, key);
	if (node == NULL)
		node = key;
	mtx_lock(&node->list_mtx);
	mtx_unlock(&tree_mtx);

	if (node != key) {
		mtx_destroy(&key->list_mtx);
		free(key, M_LEAF);
	}

	LIST_FOREACH(elm, &node->elms, entry)
		if (uap->nbyte == elm->size)
			break;
	if (elm == NULL) {
		elm = elm0;
		LIST_INSERT_HEAD(&node->elms, elm, entry);
		elm->size = uap->nbyte;
	} else
		free(elm0, M_ELM);

	elm->count++;

	mtx_unlock(&node->list_mtx);

	return (original_read(td, v));
}

static int
readstat_load(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		mtx_init(&tree_mtx, "tree lock", NULL, MTX_DEF);
		original_read = sysent[SYS_read].sy_call;
		sysent[SYS_read].sy_call = stat_read;
		break;
	case MOD_UNLOAD:
		sysent[SYS_read].sy_call = original_read;
		mtx_destroy(&tree_mtx);
		break;
	}

	return (0);
}

static moduledata_t mod_data= {
	.name = "readstat",
	.evhand = readstat_load,
};

MODULE_VERSION(readstat, 1);
DECLARE_MODULE(readstat, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
