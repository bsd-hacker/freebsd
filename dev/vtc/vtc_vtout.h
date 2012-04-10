/*-
 * Copyright (c) 2005 Marcel Moolenaar
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
 *
 * $FreeBSD$
 */

#ifndef _DEV_VTC_VTOUT_H_
#define	_DEV_VTC_VTOUT_H_

#include <sys/bus.h>
#include <sys/queue.h>

typedef int (*vtout_init_f)(device_t);
typedef int (*vtout_bitblt_f)(device_t, int, uintptr_t, uintptr_t, int, int,
    ...);

struct vtc_vtout_softc {
	TAILQ_ENTRY(vtc_vtout_softc) vo_alldevs;
	TAILQ_ENTRY(vtc_vtout_softc) vo_tedevs;
	device_t vo_dev;
	vtout_init_f vo_init;
	vtout_bitblt_f vo_bitblt;
	int	vo_width;
	int	vo_height;

	/* Font cache */
	int	vo_ch;
	int	vo_cw;
	int	vo_hs;
	int	vo_ws;
	uint8_t	*vo_font;
};

int vtc_vtout_attach(device_t, vtout_init_f, vtout_bitblt_f, int, int);
int vtc_vtout_console(device_t);

#endif /* !_DEV_VTC_VTOUT_H_ */
