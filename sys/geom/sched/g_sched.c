/*-
 * Copyright (c) 2007 Fabio Checconi <fabio@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include "g_gsched.h"
#include "g_sched.h"

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, sched, CTLFLAG_RW, 0, "GEOM_SCHED stuff");
static u_int g_sched_debug = 0;
SYSCTL_UINT(_kern_geom_sched, OID_AUTO, debug, CTLFLAG_RW, &g_sched_debug, 0,
    "Debug level");

static int g_sched_destroy(struct g_geom *gp, boolean_t force);
static int g_sched_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);
static void g_sched_config(struct gctl_req *req, struct g_class *mp,
    const char *verb);
static void g_sched_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp);
static void g_sched_init(struct g_class *mp);
static void g_sched_fini(struct g_class *mp);

struct g_class g_sched_class = {
	.name = G_SCHED_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_sched_config,
	.destroy_geom = g_sched_destroy_geom,
	.init = g_sched_init,
	.fini = g_sched_fini
};

static struct mtx g_gsched_mtx;
LIST_HEAD(gsched_list, g_gsched);
static struct gsched_list gsched_list;

void
g_sched_lock(struct g_geom *gp)
{
	struct g_sched_softc *sc = gp->softc;

	mtx_lock(&sc->sc_mtx);
}

void
g_sched_unlock(struct g_geom *gp)
{
	struct g_sched_softc *sc = gp->softc;

	mtx_unlock(&sc->sc_mtx);
}

static inline void
g_gsched_ref(struct g_gsched *gsp)
{

	atomic_add_int(&gsp->gs_refs, 1);
}

static inline void
g_gsched_unref(struct g_gsched *gsp)
{

	/*
	 * The last reference to a gsp before releasing it is the one
	 * of the gsched_list.  Elements are not released nor removed
	 * from the list until there is an external reference to them.
	 */
	atomic_add_int(&gsp->gs_refs, -1);
}

static struct g_gsched *
g_gsched_find(const char *name)
{
	struct g_gsched *gsp = NULL;

	mtx_lock(&g_gsched_mtx);
	LIST_FOREACH(gsp, &gsched_list, glist)
		if (strcmp(name, gsp->gs_name) == 0) {
			g_gsched_ref(gsp);
			break;
		}
	mtx_unlock(&g_gsched_mtx);

	return gsp;
}

static int
g_gsched_register(struct g_gsched *gsp)
{
	struct g_gsched *tmp;
	int error;

	error = 0;
	mtx_lock(&g_gsched_mtx);
	LIST_FOREACH(tmp, &gsched_list, glist)
		if (strcmp(gsp->gs_name, tmp->gs_name) == 0) {
			G_SCHED_DEBUG(0, "A scheduler named %s already"
			    "exists.", gsp->gs_name);
			error = EEXIST;
			goto out;
		}

	LIST_INSERT_HEAD(&gsched_list, gsp, glist);
	gsp->gs_refs = 1;

out:
	mtx_unlock(&g_gsched_mtx);

	return (error);
}

static int
g_gsched_unregister(struct g_gsched *gsp)
{
	struct g_gsched *cur, *tmp;
	int error;

	error = 0;
	mtx_lock(&g_gsched_mtx);
	LIST_FOREACH_SAFE(cur, &gsched_list, glist, tmp) {
		if (cur == gsp && gsp->gs_refs != 1) {
			G_SCHED_DEBUG(0, "%s still in use.", gsp->gs_name);
			error = EBUSY;
			goto out;
		} else if (cur == gsp && gsp->gs_refs == 1) {
			LIST_REMOVE(gsp, glist);
			goto out;
		}
	}

	G_SCHED_DEBUG(0, "%s not registered.", gsp->gs_name);

out:
	mtx_unlock(&g_gsched_mtx);

	return (error);
}

int
g_gsched_modevent(module_t mod, int cmd, void *arg)
{
	struct g_gsched *gsp = arg;
	int error;

	error = EOPNOTSUPP;
	switch (cmd) {
	case MOD_LOAD:
		error = g_gsched_register(gsp);
		break;
	case MOD_UNLOAD:
		error = g_gsched_unregister(gsp);
		break;
	};

	return (error);
}

static void
g_sched_orphan(struct g_consumer *cp)
{

	g_topology_assert();
	g_sched_destroy(cp->geom, 1);
}

static void
g_sched_done(struct bio *bio)
{
	struct g_geom *gp = bio->bio_parent->bio_to->geom;
	struct g_sched_softc *sc = gp->softc;
	struct g_gsched *gsp = sc->sc_gsched;

	g_sched_lock(gp);
	gsp->gs_done(sc->sc_data, bio);
	g_std_done(bio);
	g_sched_unlock(gp);
}

static void
g_sched_start(struct bio *bp)
{
	struct g_sched_softc *sc;
	struct g_gsched *gsp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct bio *cbp;

	gp = bp->bio_to->geom;

	g_sched_lock(gp);
	sc = gp->softc;
	G_SCHED_LOGREQ(bp, "Request received.");

	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_sched_done;
	pp = LIST_FIRST(&gp->provider);
	KASSERT(pp != NULL, ("NULL pp"));
	cbp->bio_to = pp;
	G_SCHED_LOGREQ(cbp, "Sending request.");
	gsp = sc->sc_gsched;
	gsp->gs_start(sc->sc_data, cbp);
	g_sched_unlock(gp);
}

static int
g_sched_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	error = g_access(cp, dr, dw, de);

	return (error);
}

static int
g_sched_create(struct gctl_req *req, struct g_class *mp,
    struct g_provider *pp, struct g_gsched *gsp)
{
	struct g_sched_softc *sc;
	struct g_geom *gp;
	struct g_provider *newpp;
	struct g_consumer *cp;
	char name[64];
	int error;

	g_topology_assert();

	gp = NULL;
	newpp = NULL;
	cp = NULL;

	snprintf(name, sizeof(name), "%s%s", pp->name, G_SCHED_SUFFIX);
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0) {
			gctl_error(req, "Provider %s already exists.", name);
			return (EEXIST);
		}
	}

	gp = g_new_geomf(mp, name);
	if (gp == NULL) {
		gctl_error(req, "Cannot create geom %s.", name);
		return (ENOMEM);
	}

	sc = g_malloc(sizeof(*sc), M_WAITOK | M_ZERO);
	sc->sc_gsched = gsp;
	sc->sc_data = gsp->gs_init(gp);
	if (sc->sc_data == NULL) {
		error = EINVAL;
		goto fail;
	}

	mtx_init(&sc->sc_mtx, "g_sched_mtx", NULL, MTX_DEF);

	gp->softc = sc;
	gp->start = g_sched_start;
	gp->orphan = g_sched_orphan;
	gp->access = g_sched_access;
	gp->dumpconf = g_sched_dumpconf;

	newpp = g_new_providerf(gp, gp->name);
	if (newpp == NULL) {
		gctl_error(req, "Cannot create provider %s.", name);
		error = ENOMEM;
		goto fail;
	}

	newpp->mediasize = pp->mediasize;
	newpp->sectorsize = pp->sectorsize;

	cp = g_new_consumer(gp);
	if (cp == NULL) {
		gctl_error(req, "Cannot create consumer for %s.", gp->name);
		error = ENOMEM;
		goto fail;
	}

	error = g_attach(cp, pp);
	if (error != 0) {
		gctl_error(req, "Cannot attach to provider %s.", pp->name);
		goto fail;
	}

	g_gsched_ref(gsp);

	g_error_provider(newpp, 0);
	G_SCHED_DEBUG(0, "Device %s created.", gp->name);

	return (0);

fail:
	if (cp != NULL) {
		if (cp->provider != NULL)
			g_detach(cp);
		g_destroy_consumer(cp);
	}

	if (newpp != NULL)
		g_destroy_provider(newpp);

	if (gp != NULL) {
		if (gp->softc != NULL)
			g_free(gp->softc);
		g_destroy_geom(gp);
	}

	return (error);
}

static int
g_sched_destroy(struct g_geom *gp, boolean_t force)
{
	struct g_provider *pp;
	struct g_sched_softc *sc;
	struct g_gsched *gsp;

	g_topology_assert();
	sc= gp->softc;
	if (sc == NULL)
		return (ENXIO);
	pp = LIST_FIRST(&gp->provider);
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_SCHED_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_SCHED_DEBUG(1, "Device %s is still open (r%dw%de%d).",
			    pp->name, pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	} else {
		G_SCHED_DEBUG(0, "Device %s removed.", gp->name);
	}

	gsp = sc->sc_gsched;
	gsp->gs_fini(sc->sc_data);
	mtx_destroy(&sc->sc_mtx);
	g_gsched_unref(gsp);

	g_free(gp->softc);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);

	return (0);
}

static int
g_sched_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{

	return (g_sched_destroy(gp, 0));
}

static void
g_sched_init(struct g_class *mp)
{

	mtx_init(&g_gsched_mtx, "gsched", NULL, MTX_DEF);
	LIST_INIT(&gsched_list);
}

static void
g_sched_fini(struct g_class *mp)
{

	KASSERT(LIST_EMPTY(&gsched_list), ("still registered schedulers"));
	mtx_destroy(&g_gsched_mtx);
}

static void
g_sched_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	struct g_provider *pp;
	struct g_gsched *gsp;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	name = gctl_get_asciiparam(req, "sched");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument", "sched");
		return;
	}

	gsp = g_gsched_find(name);
	if (gsp == NULL) {
		gctl_error(req, "Bad '%s' argument", "sched");
		return;
	}

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		goto out;
	}

	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		goto out;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			goto out;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_SCHED_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			goto out;
		}
		if (g_sched_create(req, mp, pp, gsp) != 0)
			break;
	}

out:
	g_gsched_unref(gsp);
}

static void
g_sched_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_sched_softc *sc;
	struct g_provider *pp;
	const char *name;
	char param[16];
	int i, *nargs;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}

	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);

		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}

		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL || pp->geom->class != mp) {
			G_SCHED_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			return;
		}

		sc = pp->geom->softc;
		gctl_error(req, "Reconfiguration not supported yet.");
		return;
	}
}

static struct g_geom *
g_sched_find_geom(struct g_class *mp, const char *name)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		if (strcmp(gp->name, name) == 0)
			return (gp);
	}
	return (NULL);
}

static void
g_sched_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	int *nargs, *force, error, i;
	struct g_geom *gp;
	const char *name;
	char param[16];

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument", "nargs");
		return;
	}

	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}

	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No 'force' argument");
		return;
	}

	for (i = 0; i < *nargs; i++) {
		snprintf(param, sizeof(param), "arg%d", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument", i);
			return;
		}

		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");

		gp = g_sched_find_geom(mp, name);
		if (gp == NULL) {
			G_SCHED_DEBUG(1, "Device %s is invalid.", name);
			gctl_error(req, "Device %s is invalid.", name);
			return;
		}

		error = g_sched_destroy(gp, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    gp->name, error);
			return;
		}
	}
}

static void
g_sched_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}

	if (*version != G_SCHED_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0) {
		g_sched_ctl_create(req, mp);
		return;
	} else if (strcmp(verb, "configure") == 0) {
		g_sched_ctl_configure(req, mp);
		return;
	} else if (strcmp(verb, "destroy") == 0) {
		g_sched_ctl_destroy(req, mp);
		return;
	}

	gctl_error(req, "Unknown verb.");
}

static void
g_sched_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{

}

DECLARE_GEOM_CLASS(g_sched_class, g_sched);
MODULE_VERSION(g_sched, 0);

