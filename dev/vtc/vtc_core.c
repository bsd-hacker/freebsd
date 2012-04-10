/*-
 * Copyright (c) 2003-2005 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/tty.h>

#include <dev/vtc/vtc.h>
#include <dev/vtc/vtc_con.h>
#include <dev/vtc/vtc_te.h>
#include <dev/vtc/vtc_vtout.h>

#include "vtc_te_if.h"

char vtc_device_name[] = "vtc";

TAILQ_HEAD(, vtc_te_softc) vtc_te_devs =
    TAILQ_HEAD_INITIALIZER(vtc_te_devs);
TAILQ_HEAD(, vtc_vtout_softc) vtc_vtout_devs =
    TAILQ_HEAD_INITIALIZER(vtc_vtout_devs);

MALLOC_DEFINE(M_VTC, "VTC", "VTC driver");

extern struct vtc_te_class vt102_class;

static int
vtc_tty_open(struct tty *tp)
{
	struct vtc_te_softc *te;

	te = tty_softc(tp);
	if (te == NULL)
		return (ENXIO);

	/* XXX notify the TE that we're opened? */
	return (0);
}

static void
vtc_tty_close(struct tty *tp)
{
	struct vtc_te_softc *te;

	te = tty_softc(tp);
	if (te == NULL)
		return;

	wakeup(te);
}

static void
vtc_tty_outwakeup(struct tty *tp)
{
	char txbuf[80];
	struct vtc_te_softc *te;
	__wchar_t utf32;
	size_t nchars, bufidx;
	int c, utfbytes;

	te = tty_softc(tp);
	if (te == NULL)
		return;

	utf32 = te->te_utf32;
	utfbytes = te->te_utfbytes;

	nchars = ttydisc_getc(tp, txbuf, sizeof(txbuf));
	while (nchars > 0) {
		bufidx = 0;
		while (bufidx < nchars) {
			c = txbuf[bufidx++];
			/*
			 * Conditionalize on the two major character types:
			 * initial and followup characters.
			 */
			if ((c & 0xc0) != 0x80) {
				/* Initial characters. */
				if (utfbytes != 0)
					VTC_TE_WRITE(te, -1);
				if ((c & 0xf8) == 0xf0) {
					utf32 = c & 0x07;
					utfbytes = 3;
				} else if ((c & 0xf0) == 0xe0) {
					utf32 = c & 0x0f;
					utfbytes = 2;
				} else if ((c & 0xe0) == 0xc0) {
					utf32 = c & 0x1f;
					utfbytes = 1;
				} else {
					KASSERT((c & 0x80) == 0x00, ("oops"));
					utf32 = c & 0x7f;
					utfbytes = 0;
				}
			} else {
				/* Followup characters. */
				KASSERT((c & 0xc0) == 0x80, ("oops"));
				if (utfbytes > 0) {
					utf32 = (utf32 << 6) + (c & 0x3f);
					utfbytes--;
				} else if (utfbytes == 0)
					utfbytes = -1;
			}
			if (utfbytes == 0)
				VTC_TE_WRITE(te, utf32);
		}
		nchars = ttydisc_getc(tp, txbuf, sizeof(txbuf));
	}

	te->te_utf32 = utf32;
	te->te_utfbytes = utfbytes;
}

static void
vtc_tty_inwakeup(struct tty *tp)
{
	struct vtc_te_softc *te;

	te = tty_softc(tp);
	if (te == NULL)
		return;
}

static int
vtc_tty_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{
	struct vtc_te_softc *te;

	te = tty_softc(tp);
	if (te == NULL)
		return (ENXIO);

	return (ENOIOCTL);
}

static int
vtc_tty_param(struct tty *tp, struct termios *t)
{
	struct vtc_te_softc *te;

	te = tty_softc(tp);
	if (te == NULL)
		return (ENODEV);

	t->c_cflag |= CLOCAL;
	return (0);
}

static int
vtc_tty_modem(struct tty *tp, int biton, int bitoff)
{

	return (0);
}

static void
vtc_tty_free(void *arg)
{
}

static struct ttydevsw vtc_tty_class = {
	.tsw_flags	= TF_INITLOCK|TF_CALLOUT,
	.tsw_open	= vtc_tty_open,
	.tsw_close	= vtc_tty_close,
	.tsw_outwakeup	= vtc_tty_outwakeup,
	.tsw_inwakeup	= vtc_tty_inwakeup,
	.tsw_ioctl	= vtc_tty_ioctl,
	.tsw_param	= vtc_tty_param,
	.tsw_modem	= vtc_tty_modem,
	.tsw_free	= vtc_tty_free,
};

int
vtc_vtout_attach(device_t dev, vtout_init_f init, vtout_bitblt_f bitblt,
    int width, int height)
{
	struct vtc_vtout_softc *vo;

	vo = malloc(sizeof(*vo), M_VTC, M_WAITOK|M_ZERO);
	TAILQ_INSERT_TAIL(&vtc_vtout_devs, vo, vo_alldevs);
	vo->vo_dev = dev;
	vo->vo_init = init;
	vo->vo_bitblt = bitblt;
	vo->vo_width = width;
	vo->vo_height = height;
	return (0);
}

int
vtc_vtout_console(device_t dev)
{
	struct vtc_conout *vc, **iter;

	SET_FOREACH(iter, vtc_conout_set) {
		vc = *iter;
		if (vc->vtc_busdev == dev)
			return (1);
	}
	return (0);
}

/*
 * Create the initial VT.
 */
static void
vtc_initial(void *data __unused)
{
	struct tty *tp;
	struct vtc_conout *vc, **iter;
	struct vtc_te_softc *te;

	printf("%s: create initial VT\n", vtc_device_name);

	if (TAILQ_EMPTY(&vtc_vtout_devs))
		return;

	te = malloc(vt102_class.size, M_VTC, M_WAITOK|M_ZERO);
	kobj_init((kobj_t)te, (kobj_class_t)&vt102_class);
	TAILQ_INSERT_TAIL(&vtc_te_devs, te, te_alldevs);
	TAILQ_INIT(&te->te_vodevs);

	VTC_TE_RESET(te);

	te->te_tty = tp = tty_alloc(&vtc_tty_class, te);
	tty_makedev(tp, NULL, "V%r", 0);
	SET_FOREACH(iter, vtc_conout_set) {
		vc = *iter;
		if (vc->vtc_consdev != NULL) {
			strcpy(vc->vtc_consdev->cn_name, "ttyV0");
			tty_init_console(tp, 0);
		}
	}
}
SYSINIT(vtc_initial, SI_SUB_INT_CONFIG_HOOKS, SI_ORDER_ANY, vtc_initial, NULL);

/*
 * Preempt the low-level console driver with the accelerated driver.
 */
static void
vtc_finalize(void *data __unused)
{
	struct vtc_te_softc *te;
	struct vtc_vtout_softc *vo;

	printf("%s: finalize initial VT\n", vtc_device_name);

	te = TAILQ_FIRST(&vtc_te_devs);
	if (te == NULL)
		return;

	TAILQ_FOREACH(vo, &vtc_vtout_devs, vo_alldevs) {
		if (vo->vo_init == NULL || (*vo->vo_init)(vo->vo_dev) == 0)
			TAILQ_INSERT_TAIL(&te->te_vodevs, vo, vo_tedevs);
	}
}
SYSINIT(vtc_finalize, SI_SUB_VTC, SI_ORDER_FIRST, vtc_finalize, NULL);

static int
vtc_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		printf("%s: <console, video terminal>\n", vtc_device_name);
		return (0);

	case MOD_UNLOAD:
		return (0);

	case MOD_SHUTDOWN:
		return (0);
	}

	return (EOPNOTSUPP);
}
DEV_MODULE(vtc, vtc_modevent, NULL);
