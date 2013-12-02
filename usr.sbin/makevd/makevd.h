/*-
 * Copyright (c) 2011
 *	Hiroki Sato <hrs@FreeBSD.org>  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef	_MAKEVD_H
#define	_MAKEVD_H

#include <sys/queue.h>

struct iminfo {
	int	imi_fd;
	off_t	imi_size;
	int	imi_needswap;

	char	*imi_imagename;	/* vmdk specific */
	char	*imi_uuid;	/* vmdk specific */

	struct optlisthead_t	*imi_oplhead;
};

struct optlist {
	LIST_ENTRY(optlist) opl_next;
	char *opl_name;
	char *opl_val;
};

int	vhd_makeim(struct iminfo *);
int	vhd_dumpim(struct iminfo *);
int	vmdk_makeim(struct iminfo *);
int	vmdk_dumpim(struct iminfo *);
int	raw_makeim(struct iminfo *);
int	raw_dumpim(struct iminfo *);

#ifndef	DEFAULT_IMTYPE
#define	DEFAULT_IMTYPE	"raw"
#endif

#endif	/* _MAKEVD_H */
