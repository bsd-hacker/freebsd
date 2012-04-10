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

#ifndef _DEV_VTC_H_
#define	_DEV_VTC_H_

#define	SI_SUB_VTC	(SI_SUB_SMP + 0xf00000)

/* BitBlt operations. */
#define	BITBLT_CTOFB		0x0001
#define	BITBLT_H1TOFB		0x0002
#define	BITBLT_H4TOFB		0x0003
#define	BITBLT_H8TOFB		0x0004
#define	BITBLT_H16TOFB		0x0005
#define	BITBLT_H24TOFB		0x0006
#define	BITBLT_H32TOFB		0x0007
#define	BITBLT_FBTOFB		0x0008

/* Beastie logos. */
extern int vtc_logo4_width;
extern int vtc_logo4_height;
extern unsigned char vtc_logo4_image[];

/* Built-in fonts. */
extern unsigned char vtc_font_8x16[];

/* Miscellaneous. */
extern char vtc_device_name[];

#endif /* !_DEV_VTC_H_ */
