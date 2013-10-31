#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

static int
foo_handler(module_t mod, int what, void *arg)
{

	printf("demo: %d, %p\n", what, arg);

	return (0);
}

static moduledata_t mod_data= {
	.name = "foo",
	.evhand = foo_handler,
};

MODULE_VERSION(foo, 1);
DECLARE_MODULE(foo, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
