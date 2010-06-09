/*-
 * Copyright (c) 2010, Andrey V. Elsukov <bu7cher@yandex.ru>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <libgeom.h>
#include <libsade.h>
#include <sade.h>


struct gclass *
find_class(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, name) == 0)
			return (classp);
	}
	return (NULL);
}

struct ggeom *
find_geom(struct gclass *classp, const char *name)
{
	struct ggeom *gp;

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		if (strcmp(gp->lg_name, name) == 0)
			return (gp);
	}
	return (NULL);
}

const char *
find_geomcfg(struct ggeom *gp, const char *cfg)
{
	struct gconfig *gc;

	LIST_FOREACH(gc, &gp->lg_config, lg_config) {
		if (!strcmp(gc->lg_name, cfg))
			return (gc->lg_val);
	}
	return (NULL);
}

const char *
find_provcfg(struct gprovider *pp, const char *cfg)
{
	struct gconfig *gc;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (!strcmp(gc->lg_name, cfg))
			return (gc->lg_val);
	}
	return (NULL);
}

struct gprovider *
find_provider(struct ggeom *gp, unsigned long long minsector)
{
	struct gprovider *pp, *bestpp;
	const char *s;
	unsigned long long sector, bestsector;

	bestpp = NULL;
	bestsector = 0;
	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			sector = atoll(s) / pp->lg_sectorsize;
		} else
			sector = atoll(s);

		if (sector < minsector)
			continue;
		if (bestpp != NULL && sector >= bestsector)
			continue;

		bestpp = pp;
		bestsector = sector;
	}
	return (bestpp);
}

