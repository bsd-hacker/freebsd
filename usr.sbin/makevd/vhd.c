/*-
 * Copyright (c) 2011-2013
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <unistd.h>

#include "makevd.h"
#include "common.h"
#include "vhd.h"

static uint32_t vhd_checksum(struct HardDiskFooter *);

int
vhd_makeim(struct iminfo *imi)
{
	struct HardDiskFooter HDF = VHD_HDF_FIXEDHDD_INIT;
	struct HardDiskFooter *imh;
	struct blhead_t blhead;
	struct blist *bl;
	uint64_t sectors, heads, cylinders, imagesize;
	uint8_t uuid[16];
	char vhdfile[PATH_MAX + 10];
	int ifd, ofd;

	TAILQ_INIT(&blhead);
	imh = &HDF;
	ifd = imi->imi_fd;
	imagesize = imi->imi_size;

	if (imi->imi_uuid == NULL)
		errx(EX_USAGE, "-o uuid option must be specified.");
	uuid_str2bin(&uuid, imi->imi_uuid);
	if (uuid == NULL)
		return (1);

	snprintf(vhdfile, sizeof(vhdfile), "%s.vhd", imi->imi_imagename);
	ofd = open(vhdfile, O_WRONLY|O_CREAT|O_TRUNC,
	    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (ofd < 0)
		err(EX_CANTCREAT, "%s", vhdfile);

	/* All of the fields are in BE byte order. */
	imh->TimeStamp = 0; /* XXX */
	imh->OriginalSize = htobe64(imagesize);
	imh->CurrentSize = htobe64(imagesize);

	sectors = 63;
	heads = 16;
	cylinders = imagesize / (sectors * heads * 512);
	while (cylinders > 1024) {
		cylinders >>= 1;
		heads <<= 1;
	}
	imh->DiskGeometry.cylinder = htobe16(cylinders);
	imh->DiskGeometry.heads = heads;
	imh->DiskGeometry.sectcyl = sectors;

	memcpy((char *)imh->UniqueId, (char *)&uuid, sizeof(imh->UniqueId));

	imh->Checksum = htobe32(vhd_checksum(imh));

	bl = calloc(1, sizeof(*bl));
	if (bl == NULL)
		err(EX_OSERR, NULL);
	bl->bl_type = BL_RAWCOPY;
	bl->bl_name = "Rawcopy";
	bl->bl_tf.fd = ifd;
	TAILQ_INSERT_TAIL(&blhead, bl, bl_next);

	bl = calloc(1, sizeof(*bl));
	if (bl == NULL)
		err(EX_OSERR, NULL);
	bl->bl_type = BL_RAWDATA;
	bl->bl_name = "Hard Disk Footer";
	bl->bl_tr.data = imh;
	bl->bl_tr.len = sizeof(*imh);
	TAILQ_INSERT_TAIL(&blhead, bl, bl_next);

	return (dispatch_bl(ofd, &blhead));
}

static uint32_t
vhd_checksum(struct HardDiskFooter *imh)
{
	uint32_t sum;
	size_t len;

	sum = 0;
	for (len = 0; len < sizeof(*imh); len++)
		sum += ((uint8_t *)imh)[0];

	return (~sum);
}
