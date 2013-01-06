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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <unistd.h>

#include "makevd.h"
#include "vmdk.h"

int
vmdk_makeim(struct iminfo *imi)
{
	struct SparseExtentHeader SEH, *imh;
	uint64_t sectors, heads, cylinders, imagesize;
	char vmdkfile[PATH_MAX + 10], *vmdkfilebase;
	char buf[BUFSIZ];
	char desc[1024];
	ssize_t len0, len = 0;
	int ifd, ofd;

	imh = &SEH;
	ifd = imi->imi_fd;
	imagesize = imi->imi_size;

	memset(imh, 0, sizeof(*imh));
	memset(desc, 0, sizeof(desc));

	if (imi->imi_uuid == NULL)
		errx(EX_USAGE, "-o uuid option must be specified.");

	snprintf(vmdkfile, sizeof(vmdkfile), "%s.vmdk", imi->imi_imagename);
	ofd = open(vmdkfile, O_WRONLY|O_CREAT|O_TRUNC,
	    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (ofd < 0)
		err(EX_CANTCREAT, "%s", vmdkfile);

	vmdkfilebase = strrchr(vmdkfile, '/');
	if (vmdkfilebase == '\0')
		vmdkfilebase = vmdkfile;

	/* All of the fields are in LE byte order. */
	imh->magicNumber = htole32(SEH_MAGICNUMBER);
	imh->version = htole32(SEH_VERSION_DEFAULT);
	imh->flags = htole32(1);
	imh->capacity = htole64(0);
	imh->grainSize = htole64(16);
	imh->descriptorOffset = htole64((sizeof(*imh) + 511) / 512);
	imh->descriptorSize = htole64((sizeof(desc) + 511) / 512);
	imh->numGTEsPerGT = htole32(512);
	imh->rgdOffset = htole64(0);
	imh->gdOffset = htole64(0);
	imh->overHead = htole64(imh->descriptorOffset + imh->descriptorSize);
	imh->uncleanShutdown = 0;
	imh->singleEndLineChar = '\n';
	imh->nonEndLineChar = ' ';
	imh->doubleEndLineChar1 = '\r';
	imh->doubleEndLineChar2 = '\n';

	sectors = 63;
	heads = 16;
	cylinders = imagesize / (sectors * heads * 512);
	while (cylinders > 1024) {
		cylinders >>= 1;
		heads <<= 1;
	}

	snprintf(desc, sizeof(desc),
	    "# Disk Descriptor File\n"
	    "version=1\n"
	    "CID=fffffffe\n"
	    "parentCID=ffffffff\n"
	    "createType=\"monolithicFlat\"\n"
	    "# Extent Description\n"
	    "RW %" PRIu64 " FLAT \"%s\" %" PRIu64 "\n"
	    "# Disk Data Base\n"
	    "ddb.toolsVersion = \"0\"\n"
	    "ddb.virtualHWVersion = \"3\"\n"
	    "ddb.geometry.sectors = \"%" PRIu64 "\"\n"
	    "ddb.adapterType = \"ide\"\n"
	    "ddb.geometry.heads = \"%" PRIu64 "\"\n"
	    "ddb.geometry.cylinders = \"%" PRIu64 "\"\n"
	    "ddb.uuid.image=\"%s\"\n",
	    cylinders * sectors * heads,
	    vmdkfilebase,
	    imh->overHead,
	    sectors,
	    heads,
	    cylinders,
	    imi->imi_uuid);

	len0 = write(ofd, imh, sizeof(*imh));
	if (len0 != sizeof(*imh)) {
		warn("write error");
		return (1);
	}
	len0 = write(ofd, desc, sizeof(desc));
	if (len0 != sizeof(desc)) {
		warn("write error");
		return (1);
	}
	for (;;) {
		len0 = read(ifd, buf, sizeof(buf));
		if (len0 == 0)
			break;
		if (len0 < 0) {
			warn("read error");
			return (1);
		}
		len = write(ofd, buf, len0);
		if (len < 0) {
			warn("write error");
			return (1);
		}
	}

	return (0);
}
