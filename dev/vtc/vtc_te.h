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

#ifndef _DEV_VTC_TE_H_
#define	_DEV_VTC_TE_H_

/*
 * VTC TE class & instance (=softc)
 */
struct vtc_te_class {
	KOBJ_CLASS_FIELDS;
};

struct vtc_te_softc {
	KOBJ_FIELDS;
	struct vtc_te_class *te_class;
	TAILQ_ENTRY(vtc_te_softc) te_alldevs;
	TAILQ_HEAD(, vtc_vtout_softc) te_vodevs;
	struct tty	*te_tty;
	__wchar_t	te_utf32;
	int		te_utfbytes;
	int		te_maxcol;
	int		te_maxrow;
};

/*
 * TE support functions.
 */
int vtc_te_bell(struct vtc_te_softc *);
int vtc_te_putc(struct vtc_te_softc *, int, int, int, int, int, __wchar_t);
int vtc_te_repos(struct vtc_te_softc *, int, int);
int vtc_te_scroll(struct vtc_te_softc *, int, int, int, int, int);

#endif /* !_DEV_VTC_TE_H_ */
