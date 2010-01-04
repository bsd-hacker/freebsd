
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

static MALLOC_DEFINE(M_TASKQUEUE, "taskqueue", "Task Queues");
static void	*taskqueue_giant_ih;
static void	*taskqueue_ih;

struct taskqueue {
	STAILQ_HEAD(, task)	tq_queue;
	const char		*tq_name;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	struct task		*tq_running;
	struct mtx		tq_mutex;
	struct thread		**tq_threads;
	int			tq_tcount;
	int			tq_spin;
	int			tq_flags;
};

#define	TQ_FLAGS_ACTIVE		(1 << 0)
#define	TQ_FLAGS_BLOCKED	(1 << 1)
#define	TQ_FLAGS_PENDING	(1 << 2)


struct taskqueue *
taskqueue_create(const char *name, int mflags,
				    taskqueue_enqueue_fn enqueue,
    void *context)
{

	panic("");
	return (NULL);
	
}

int
taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
    const char *name, ...)
{


	panic("");
	return (0);
}


void
taskqueue_run(struct taskqueue *queue)
{

	panic("");
}


int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{

	panic("");
	return (0);
}


void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{
	
	panic("");
}

void
taskqueue_free(struct taskqueue *queue)
{

	panic("");	
}

void
taskqueue_thread_enqueue(void *context)
{
	panic("");
	
}

static void
taskqueue_swi_enqueue(void *context)
{
	swi_sched(taskqueue_ih, 0);
}

static void
taskqueue_swi_run(void *dummy)
{
	taskqueue_run(taskqueue_swi);
}

TASKQUEUE_DEFINE(swi, taskqueue_swi_enqueue, NULL,
		 swi_add(NULL, "task queue", taskqueue_swi_run, NULL, SWI_TQ,
		     INTR_MPSAFE, &taskqueue_ih)); 


