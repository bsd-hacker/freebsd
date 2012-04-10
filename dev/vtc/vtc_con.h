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

#ifndef _DEV_VTC_CON_H_
#define	_DEV_VTC_CON_H_

struct vtc_conout;

typedef void vtc_con_bitblt_f(struct vtc_conout *, int, uintptr_t, uintptr_t,
    int, int, ...);
typedef void vtc_con_init_f(struct vtc_conout *);
typedef int  vtc_con_probe_f(struct vtc_conout *);

struct vtc_conout {
	struct consdev	*vtc_consdev;
	device_t	vtc_busdev;
	const char	*vtc_con_name;
	vtc_con_probe_f	*vtc_con_probe;
	vtc_con_init_f	*vtc_con_init;
	vtc_con_bitblt_f *vtc_con_bitblt;
	void		*vtc_con_cookie;
	int		vtc_con_width;
	int		vtc_con_height;
	int		vtc_con_depth;
};

#define	VTC_CONOUT(name, probe, init, bitblt)			\
	static struct vtc_conout name##_vtc_conout = {		\
		.vtc_con_name = #name,				\
		.vtc_con_probe = probe,				\
		.vtc_con_init = init,				\
		.vtc_con_bitblt = bitblt,			\
	};							\
	DATA_SET(vtc_conout_set, name##_vtc_conout)

SET_DECLARE(vtc_conout_set, struct vtc_conout);

#endif /* !_DEV_VTC_CON_H_ */
