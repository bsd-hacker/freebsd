/*-
 * Copyright (c) 2010 Andrey V. Elsukov <bu7cher@yandex.ru>
 * All rights reserved.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <libsade.h>

static struct de_class_desc {
	const char	*de_name;
	const char	*de_desc;
} classes[] = {
#define DISK_IDX	0
	{"DISK",	"disk device"},
	{"MD",		"memory backed virtual disk"},
#define ZVOL_IDX	2
	{"ZFS::ZVOL",	"ZFS volume"},
	{"MIRROR",	"GEOM based mirror (RAID1)"},
	{"STRIPE",	"GEOM based stripe (RAID0)"},
	{"RAID3",	"GEOM based RAID3 array"},
	{"CONCAT",	"GEOM based concatenated disk"},
	{"ELI",		"GEOM based encrypted disk"},
	{"JOURNAL",	"GEOM based journalled disk"},
	{"MULTIPATH",	"GEOM based disk with multiple paths"},
	{NULL, NULL}
};

static int
de_device_add(struct de_devlist *pd, struct gprovider *pp,
    const char *classname, const char *classdesc)
{
	struct de_device *pdev;

	pdev = malloc(sizeof(struct de_device));
	if (pdev == NULL)
		return (ENOMEM);
	bzero(pdev, sizeof(struct de_device));
	pdev->de_name = strdup(pp->lg_name);
	pdev->de_geom = strdup(pp->lg_geom->lg_name);
	pdev->de_class = __DECONST(char *, classname);
	pdev->de_desc = __DECONST(char *, classdesc);
	pdev->de_mediasize = pp->lg_mediasize;
	pdev->de_sectorsize = pp->lg_sectorsize;
	TAILQ_INIT(&pdev->de_part);
	TAILQ_INSERT_TAIL(pd, pdev, de_device);
	return (0);
}

int
de_devlist_partitioned_get(struct de_devlist *pd)
{
	struct gmesh mesh;
	struct gclass *cpp, *cpl;
	struct ggeom *gpp, *gpl;
	struct gconsumer *gcp;
	struct gprovider *pp;
	const char *classname_part = "PART";
	const char *classname_label = "LABEL";
	const char *classdesc = "partitioned device";
	int error;

	error = geom_gettree(&mesh);
	if (error)
		return (error);
	TAILQ_INIT(pd);
	cpp = find_class(&mesh, classname_part);
	cpl = find_class(&mesh, classname_label);
	if (cpp == NULL) {
		geom_deletetree(&mesh);
		return (ENODEV);
	}
	LIST_FOREACH(gpp, &cpp->lg_geom, lg_geom) {
		gcp = LIST_FIRST(&gpp->lg_consumer);
		if (gcp == NULL || gcp->lg_provider == NULL)
			continue;
		if (cpl) {
			/* skip labelled providers */
			LIST_FOREACH(gpl, &cpl->lg_geom, lg_geom) {
				pp = LIST_FIRST(&gpl->lg_provider);
				if (pp == NULL)
					continue;
				if (strcmp(gcp->lg_provider->lg_name,
				    pp->lg_name) == 0)
					goto skip;
			}
		}
		error = de_device_add(pd, gcp->lg_provider, classname_part,
		    classdesc);
		if (error)
			break;
skip:		;
	}
	geom_deletetree(&mesh);
	return (error);
}

int
de_devlist_get(struct de_devlist *pd)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	int error, i;

	error = geom_gettree(&mesh);
	if (error)
		return (error);
	TAILQ_INIT(pd);
	for (i = 0; classes[i].de_name != NULL; i++) {
		cp = find_class(&mesh, classes[i].de_name);
		if (cp == NULL)
			continue;
		LIST_FOREACH(gp, &cp->lg_geom, lg_geom) {
			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
				/* Skip CD disk devices */
				if (i == DISK_IDX &&
				    strncmp(pp->lg_name, "cd", 2) == 0)
					continue;
				/* Skip ZFS snapshots */
				if (i == ZVOL_IDX &&
				    strchr(pp->lg_name, '@') != NULL)
					continue;
				if (pp->lg_mediasize > 0 &&
				    pp->lg_sectorsize > 0)
					error = de_device_add(pd, pp,
					    classes[i].de_name,
					    classes[i].de_desc);
				if (error)
					goto done;
			}
		}
	}
done:
	geom_deletetree(&mesh);
	return (error);
}

void
de_devlist_free(struct de_devlist *pd)
{
	struct de_device *pdev;

	while(!TAILQ_EMPTY(pd)) {
		pdev = TAILQ_FIRST(pd);
		free(pdev->de_name);
		free(pdev->de_geom);
		TAILQ_REMOVE(pd, pdev, de_device);
		free(pdev);
	}
	TAILQ_INIT(pd);
}

int
de_devlist_count(struct de_devlist *pd)
{
	int count = 0;
	struct de_device *pdev;

	TAILQ_FOREACH(pdev, pd, de_device){
		count++;
	}
	return (count);
}

struct de_device *
de_dev_find(struct de_devlist *pd, const char *name)
{
	struct de_device *pdev = NULL;

	TAILQ_FOREACH(pdev, pd, de_device) {
		if (strcmp(pdev->de_name, name) == 0)
			return (pdev);
	}
	return (NULL);
}
