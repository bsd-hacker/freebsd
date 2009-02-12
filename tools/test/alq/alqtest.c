/*-
 * Copyright (c) 2008 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/alq.h>
#include <sys/sbuf.h>
#include <machine/stdarg.h>

#include "alqtest.h"

MALLOC_DECLARE(M_ALQTEST);
MALLOC_DEFINE(M_ALQTEST, "alqtest", "dynamic memory used by alqtest");

#define ENABLE	0x01
#define DISABLE	0x02

#define NUM_TEST_RUNS 1

static volatile uint8_t run_test_thread;

static char logfile[PATH_MAX] = "/var/log/alqtest.log\0";

static struct thread *alq_test_thr = NULL;

static struct mtx alqtestmtx;

typedef int (*testfunc)(struct sbuf *, struct sbuf *);

typedef const enum {
	BLACK = 30,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE
} fgcolor_t;

static int
sbuf_printf_color(struct sbuf *s, fgcolor_t c, const char *fmt, ...)
{
	va_list ap;
	int ret;

	sbuf_printf(s, "\033[%dm", c);
	va_start(ap, fmt);
	ret = sbuf_vprintf(s, fmt, ap);
	va_end(ap);
	sbuf_printf(s, "\033[0m");

	return (ret);
}

static char
alqtest_randchar(void)
{
	uint32_t c;

	/* generate a random character in the ascii range [32, 126] */
	while ( (c = arc4random() % 126) < 32);

	return (char)c;
}

static void
alqtest_doio_callback(void)
{
	printf("doing io baby!\n");
}

static int
alqtest_writen(struct sbuf *s, struct sbuf *debug)
{
	struct alq *testalq;
	const int buflen = 100;
	int i = 0, ret = 0, errors = 0;
	char buf[buflen+1];

	sbuf_printf(s, "- variable length message writing\n");

	/* test variable length message writing */
	ret = alq_open(	&testalq,
			logfile,
			curthread->td_ucred,
			0600,
			buflen,
			0
	);

	testalq->doio_debugcallback = &alqtest_doio_callback;

	for (i = 0; i < sizeof(buf); i++)
		buf[i] = alqtest_randchar();

	sbuf_printf(s, "-- msglen==1,buflen=%d\n", buflen);
	alq_writen(testalq, buf, 1, ALQ_WAITOK | ALQ_NOACTIVATE);

	if ((buflen-1 != testalq->aq_freebytes) &&
		(1 != testalq->aq_writehead) &&
			(0 != testalq->aq_writetail)) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_freebytes",
				buflen-1,
				testalq->aq_freebytes
		);
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writehead",
				1,
				testalq->aq_writehead
		);
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writetail",
				0,
				testalq->aq_writetail
		);
	}

	sbuf_printf(s, "-- msglen==%d,buflen=%d\n", buflen, buflen);
	alq_writen(testalq, buf, buflen, ALQ_WAITOK);

	if ((0 != testalq->aq_freebytes) &&
		(0 != testalq->aq_writehead) &&
			(0 != testalq->aq_writetail)) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_freebytes",
				0,
				testalq->aq_freebytes
		);
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writehead",
				0,
				testalq->aq_writehead
		);
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writetail",
				0,
				testalq->aq_writetail
		);
	}

	alq_close(testalq);

	return errors;
}

static int
alqtest_open(struct sbuf *s, struct sbuf *debug)
{
	struct alq *testalq;
	const int buflen = 100;
	int ret = 0, errors = 0;

	sbuf_printf(s, "- variable length message queue creation\n");

	/* test variable length message queue creation */
	ret = alq_open(	&testalq,
			logfile,
			curthread->td_ucred,
			0600,
			buflen,
			0
	);

	if (0 != testalq->aq_entmax) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_entmax",
				0,
				testalq->aq_entmax
		);
	}

	if (0 != testalq->aq_entlen) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_entlen",
				0,
				testalq->aq_entlen
		);
	}

	if (buflen != testalq->aq_freebytes) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_freebytes",
				buflen,
				testalq->aq_freebytes
		);
	}

	if (buflen != testalq->aq_buflen) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_buflen",
				buflen,
				testalq->aq_buflen
		);
	}

	if (0 != testalq->aq_writehead) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writehead",
				0,
				testalq->aq_writehead
		);
	}

	if (0 != testalq->aq_writetail) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_writetail",
				0,
				testalq->aq_writetail
		);
	}

	if (0 != testalq->aq_flags) {
		errors++;
		sbuf_printf(	debug,
				"alq->%-15s\texpected=%d\tactual=%d\n",
				"aq_flags",
				0,
				testalq->aq_flags
		);
	}

	alq_close(testalq);

	return errors;
}

static void
run_test(struct sbuf *s, const char *test_banner, testfunc test)
{
	struct sbuf *debug = NULL;

	if ((debug = sbuf_new(NULL, NULL, 1024, SBUF_AUTOEXTEND)) != NULL) {
		sbuf_printf(s, "########################################\n");
		sbuf_printf_color(s, GREEN, "%s\n", test_banner);
		if (test(s, debug)) {
			sbuf_finish(debug);
			sbuf_printf_color(s, RED, "!!ERROR(S) FOUND!!\n");
			sbuf_printf(s, "%s", sbuf_data(debug));
			sbuf_printf_color(s, RED, "!!ERROR(S) FOUND!!\n");
		}
		sbuf_printf(s, "########################################\n\n");
		sbuf_delete(debug);
	}
}

static void
alqtest_thread(void *arg)
{
	struct sbuf *s = NULL;
	long runs = 0;

	/* loop until thread is signalled to exit */
	while (run_test_thread && runs < NUM_TEST_RUNS) {
		if ((s = sbuf_new(NULL, NULL, 1024, SBUF_AUTOEXTEND)) != NULL) {
			sbuf_printf(s, "TEST RUN: %ld\n", ++runs);

			run_test(s, "alq_open", &alqtest_open);
			run_test(s, "alq_writen", &alqtest_writen);

			sbuf_finish(s);
			printf("%s", sbuf_data(s));
			sbuf_delete(s);
		}
	}

	kthread_exit();
}

static int
manage_test_ops(uint8_t action)
{
	int error = 0;
	//struct sbuf *s = NULL;

	/* init an autosizing sbuf that initially holds 200 chars */
	//if ((s = sbuf_new(NULL, NULL, 200, SBUF_AUTOEXTEND)) == NULL)
	//	return -1;

	if (action == ENABLE) {

		run_test_thread = 1;

		kthread_add(	&alqtest_thread,
				NULL,
				NULL,
				&alq_test_thr,
				RFNOWAIT,
				0,
				"alq_test_thr"
		);
	}
	else if (action == DISABLE && alq_test_thr != NULL) {
		/* tell the test thread that it should exit now */
		run_test_thread = 0;
		alq_test_thr = NULL;
	}

	return error;
}

static int
deinit(void)
{
	manage_test_ops(DISABLE);
	mtx_destroy(&alqtestmtx);
	return 0;
}

static int
init(void)
{
	mtx_init(&alqtestmtx, "alqtestmtx", NULL, MTX_DEF);
	manage_test_ops(ENABLE);
	return 0;
}

/*
 * This is the function that is called to load and unload the module.
 * When the module is loaded, this function is called once with
 * "what" == MOD_LOAD
 * When the module is unloaded, this function is called twice with
 * "what" = MOD_QUIESCE first, followed by "what" = MOD_UNLOAD second
 * When the system is shut down e.g. CTRL-ALT-DEL or using the shutdown command,
 * this function is called once with "what" = MOD_SHUTDOWN
 * When the system is shut down, the handler isn't called until the very end
 * of the shutdown sequence i.e. after the disks have been synced.
 */
static int alqtest_load_handler(module_t mod, int what, void *arg)
{
	switch(what) {
		case MOD_LOAD:
			return init();
			break;

		case MOD_QUIESCE:
		case MOD_SHUTDOWN:
			return deinit();
			break;

		case MOD_UNLOAD:
			return 0;
			break;

		default:
			return EINVAL;
			break;
	}
}

/* basic module data */
static moduledata_t alqtest_mod =
{
	"alqtest",
	alqtest_load_handler, /* execution entry point for the module */
	NULL
};

/*
 * Param 1: name of the kernel module
 * Param 2: moduledata_t struct containing info about the kernel module
 *          and the execution entry point for the module
 * Param 3: From sysinit_sub_id enumeration in /usr/include/sys/kernel.h
 *          Defines the module initialisation order
 * Param 4: From sysinit_elem_order enumeration in /usr/include/sys/kernel.h
 *          Defines the initialisation order of this kld relative to others
 *          within the same subsystem as defined by param 3
 */
DECLARE_MODULE(alqtest, alqtest_mod, SI_SUB_SMP, SI_ORDER_ANY);
MODULE_DEPEND(alqtest, alq, 1, 1, 1);


