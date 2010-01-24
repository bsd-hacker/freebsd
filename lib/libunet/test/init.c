#include <sys/cdefs.h>
#include <sys/param.h>
#include <stdlib.h>
#include <sys/pcpu.h>

extern void mi_startup(void);
extern void uma_startup(void *, int);
caddr_t kern_timeout_callwheel_alloc(caddr_t v);
void kern_timeout_callwheel_init(void);
extern int ncallout;


int
main(void)
{
	struct pcpu *pc;

	/* vm_init bits */
	ncallout = 64;
	pc = malloc(sizeof(struct pcpu));
	pcpu_init(pc, 0, sizeof(struct pcpu));
	kern_timeout_callwheel_alloc(malloc(512*1024));
	kern_timeout_callwheel_init();
	uma_startup(NULL, 0);

	mi_startup();

}
