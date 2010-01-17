
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


struct taskqueue *taskqueue_swi;

int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{

	panic("should be unreachable");
	return (0);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{
	
	panic("should be unreachable");
}

