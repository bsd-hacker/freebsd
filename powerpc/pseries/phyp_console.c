/*-
 * Copyright (C) 2011 by Nathan Whitehorn. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/pseries/powerpc/phyp/phyp_console.c 214348 2010-10-25 15:41:12Z nwhitehorn $");

#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <dev/ofw/openfirm.h>

#include <ddb/ddb.h>

#include "phyp-hvcall.h"

static tsw_outwakeup_t phyptty_outwakeup;

static struct ttydevsw phyp_ttydevsw = {
	.tsw_flags	= TF_NOPREFIX,
	.tsw_outwakeup	= phyptty_outwakeup,
};

static int			polltime;
static cell_t			termno;
static struct callout		phyp_callout;
static union {
	uint64_t u64[2];
	char str[16];
} phyp_inbuf;
static uint64_t			phyp_inbuflen = 0;
static struct tty 		*tp = NULL;

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
static int			alt_break_state;
#endif

static void	phyp_timeout(void *);

static cn_probe_t	phyp_cnprobe;
static cn_init_t	phyp_cninit;
static cn_term_t	phyp_cnterm;
static cn_getc_t	phyp_cngetc;
static cn_putc_t	phyp_cnputc;

CONSOLE_DRIVER(phyp);

static void
cn_drvinit(void *unused)
{
	phandle_t dev;

	if (phyp_consdev.cn_pri != CN_DEAD &&
	    phyp_consdev.cn_name[0] != '\0') {
		dev = OF_finddevice("/vdevice/vty");
		if (dev == -1)
			return;

		OF_getprop(dev, "reg", &termno, sizeof(termno));
		tp = tty_alloc(&phyp_ttydevsw, NULL);
		tty_init_console(tp, 0);
		tty_makedev(tp, NULL, "%s", "phypvty");

		polltime = 1;

		callout_init(&phyp_callout, CALLOUT_MPSAFE);
		callout_reset(&phyp_callout, polltime, phyp_timeout, NULL);
	}
}

SYSINIT(cndev, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE, cn_drvinit, NULL);

static void
phyptty_outwakeup(struct tty *tp)
{
	int len, err;
	uint64_t buf[2];

	for (;;) {
		len = ttydisc_getc(tp, buf, sizeof buf);
		if (len == 0)
			break;

		do {
			err = phyp_hcall(H_PUT_TERM_CHAR, termno,
			    (register_t)len, buf[0], buf[1]);
		} while (err == H_BUSY);
	}
}

static void
phyp_timeout(void *v)
{
	int 	c;

	tty_lock(tp);
	while ((c = phyp_cngetc(NULL)) != -1)
		ttydisc_rint(tp, c, 0);
	ttydisc_rint_done(tp);
	tty_unlock(tp);

	callout_reset(&phyp_callout, polltime, phyp_timeout, NULL);
}

static void
phyp_cnprobe(struct consdev *cp)
{
	phandle_t dev;

	dev = OF_finddevice("/vdevice/vty");

	if (dev == -1) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	OF_getprop(dev, "reg", &termno, sizeof(termno));
	cp->cn_pri = CN_NORMAL;
}

static void
phyp_cninit(struct consdev *cp)
{

	/* XXX: This is the alias, but that should be good enough */
	strcpy(cp->cn_name, "phypcons");
}

static void
phyp_cnterm(struct consdev *cp)
{
}

static int
phyp_cngetc(struct consdev *cp)
{
	int ch, err;
#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
	int kdb_brk;
#endif

	/* XXX: thread safety */
	if (phyp_inbuflen == 0) {
		err = phyp_pft_hcall(H_GET_TERM_CHAR, termno, 0, 0, 0,
		    &phyp_inbuflen, &phyp_inbuf.u64[0], &phyp_inbuf.u64[1]);
		if (err != H_SUCCESS)
			return (-1);
	}

	if (phyp_inbuflen == 0)
		return (-1);

	ch = phyp_inbuf.str[0];
	phyp_inbuflen--;
	if (phyp_inbuflen > 0)
		memcpy(&phyp_inbuf.str[0], &phyp_inbuf.str[1], phyp_inbuflen);

#if defined(KDB) && defined(ALT_BREAK_TO_DEBUGGER)
	if ((kdb_brk = kdb_alt_break(ch, &alt_break_state)) != 0) {
		switch (kdb_brk) {
		case KDB_REQ_DEBUGGER:
			kdb_enter(KDB_WHY_BREAK,
			    "Break sequence on console");
			break;
		case KDB_REQ_PANIC:
			kdb_panic("Panic sequence on console");
			break;
		case KDB_REQ_REBOOT:
			kdb_reboot();
			break;

		}
	}
#endif
	return (ch);
}

static void
phyp_cnputc(struct consdev *cp, int c)
{
	uint64_t cbuf;

	cbuf = (uint64_t)c << 56;
	phyp_hcall(H_PUT_TERM_CHAR, termno, 1UL, cbuf, 0);
}

