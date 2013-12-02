/*-
 * Copyright (c) 2011,2012,2013
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#include "makevd.h"
#include "vmdk.h"
#include "vhd.h"

static LIST_HEAD(optlisthead_t, optlist) oplhead;

/* list of supported disk image type and dispatch functions */
static struct imtype {
	const char	*imt_type;
	int		(*imt_makeim)(struct iminfo *);
	int		(*imt_dumpim)(struct iminfo *);
} imtypes[] = {
	{ .imt_type = "vhd",
		.imt_makeim = vhd_makeim,
		.imt_dumpim = vhd_dumpim, },
	{ .imt_type = "vmdk",
		.imt_makeim = vmdk_makeim,
		.imt_dumpim = vmdk_dumpim, },
	{ .imt_type = "none",
		.imt_makeim = raw_makeim,
		.imt_dumpim = raw_dumpim, },
	{ .imt_type = "raw",
		.imt_makeim = raw_makeim,
		.imt_dumpim = raw_dumpim, },
	{ .imt_type = NULL },
};

static struct imtype *get_imtype(const char *);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct imtype *imt;
	struct iminfo imi;
	struct optlist *opl;
	struct stat sb;
	int ch;
	int dump;
	int ifd;
	int opl_new;
	char *val;

	setprogname(argv[0]);
	dump = (strcmp(basename(argv[0]), "dumpvd") == 0);

	if ((imt = get_imtype(DEFAULT_IMTYPE)) == NULL)
		errx(1, "Unknown default image type `%s'.", DEFAULT_IMTYPE);

	memset(&imi, 0, sizeof(imi));
	memset(&sb, 0, sizeof(sb));
	LIST_INIT(&oplhead);
	imi.imi_fd = -1;

	while ((ch = getopt(argc, argv, "B:t:o:")) != -1) {
		switch (ch) {
		case 'B':
			if (strcmp(optarg, "be") == 0 ||
			    strcmp(optarg, "4321") == 0 ||
			    strcmp(optarg, "big") == 0) {
#if BYTE_ORDER == LITTLE_ENDIAN
				imi.imi_needswap = 1;
#endif
			} else if (strcmp(optarg, "le") == 0 ||
			    strcmp(optarg, "1234") == 0 ||
			    strcmp(optarg, "little") == 0) {
#if BYTE_ORDER == BIG_ENDIAN
				imi.imi_needswap = 1;
#endif
			} else {
				warnx("Invalid endian `%s'.", optarg);
				usage();
			}
			break;

		case 't':
			if ((imt = get_imtype(optarg)) == NULL)
				errx(1, "Unknown image type `%s'.", optarg);
			break;

		case 'o':
			opl_new = 0;
			val = strchr(optarg, '=');
			if (val == '\0')
				val = NULL;
			else {
				*val = '\0';
				val += 1;
			}
			/* Check duplicate entry. */
			LIST_FOREACH(opl, &oplhead, opl_next)
			    if (strcmp(opl->opl_name, optarg) == 0)
				    break;
			if (opl == NULL) {
				opl = calloc(1, sizeof(*opl));
				if (opl == NULL)
					err(EX_OSERR, "%s", optarg);
				opl_new = 1;
			} else {
				free(opl->opl_name);
				opl->opl_name = NULL;
				if (opl->opl_val) {
					free(opl->opl_val);
					opl->opl_val = NULL;
				}
			}
			if (val)
				opl->opl_val = strdup(val);
			opl->opl_name = strdup(optarg);
			if (opl_new)
				LIST_INSERT_HEAD(&oplhead, opl, opl_next);
			break;
		case '?':
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	ifd = open(argv[0], O_RDONLY);
	if (ifd < 0)
		err(EX_NOINPUT, "%s", argv[0]);
	if (fstat(ifd, &sb) < 0)
		err(EX_IOERR, "%s", argv[0]);

	imi.imi_fd = ifd;
	imi.imi_size = sb.st_size;
	imi.imi_oplhead = &oplhead;
	LIST_FOREACH(opl, &oplhead, opl_next) {
#if 0
		printf("options: %s = %s\n", opl->opl_name, opl->opl_val);
#endif
		if (strcmp(opl->opl_name, "uuid") == 0)
			imi.imi_uuid = opl->opl_val;
		else if (strcmp(opl->opl_name, "imagename") == 0)
			imi.imi_imagename = opl->opl_val;
	}
	if (imi.imi_imagename == NULL)
		imi.imi_imagename = strdup(argv[0]);

	if (dump)
		imt->imt_makeim(&imi);
	else
		imt->imt_dumpim(&imi);

	return (0);
}

static struct imtype *
get_imtype(const char *type)
{
	int i;

	for (i = 0; imtypes[i].imt_type != NULL; i++)
		if (strcmp(imtypes[i].imt_type, type) == 0)
			return (&imtypes[i]);
	return (NULL);
}

static void
usage(void)
{
	const char *prog;

	prog = getprogname();
	fprintf(stderr, "usage: %s [-t image-type] [-o image-options] "
	    "input-file\n",prog);

	exit(1);
}
