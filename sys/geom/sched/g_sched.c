/*-
 * Copyright (c) 2009 Fabio Checconi <fabio@FreeBSD.org>
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

/*
 * The main control module for geom-based disk schedulers
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
#include <sys/proc.h>	/* we access curthread */
#include <geom/geom.h>
#include "gs_scheduler.h"
#include "g_sched.h"	/* geom hooks */

static int g_sched_destroy(struct g_geom *gp, boolean_t force);
static int g_sched_destroy_geom(struct gctl_req *req,
    struct g_class *mp, struct g_geom *gp);
static void g_sched_config(struct gctl_req *req, struct g_class *mp,
    const char *verb);
static struct g_geom *
g_sched_taste(struct g_class *mp, struct g_provider *pp, int flags __unused);
static void g_sched_dumpconf(struct sbuf *sb, const char *indent,
    struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp);
static void g_sched_init(struct g_class *mp);
static void g_sched_fini(struct g_class *mp);

struct g_class g_sched_class = {
	.name = G_SCHED_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_sched_config,
	.taste = g_sched_taste,
	.destroy_geom = g_sched_destroy_geom,
	.init = g_sched_init,
	.fini = g_sched_fini
};

MALLOC_DEFINE(M_GEOM_SCHED, "GEOM_SCHED", "Geom schedulers data structures");

/*
 * Global variables describing the state of the geom_sched module.
 */
LIST_HEAD(gs_list, g_gsched);	/* type, link field */
struct geom_sched_vars {
	struct mtx	gs_mtx;
	struct gs_list	gs_scheds;	/* list of schedulers */
	int		gs_sched_count;	/* how many schedulers ? */
	u_int		gs_debug;
	int 		gs_patched;	/* g_io_request was patched */
	char		gs_names[256];	/* names of schedulers */
};

static struct geom_sched_vars me;

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, sched, CTLFLAG_RW, 0,
    "GEOM_SCHED stuff");
SYSCTL_UINT(_kern_geom_sched, OID_AUTO, debug, CTLFLAG_RW,
    &me.gs_debug, 0, "Debug level");
SYSCTL_UINT(_kern_geom_sched, OID_AUTO, sched_count, CTLFLAG_RD,
    &me.gs_sched_count, 0, "Number of schedulers");
SYSCTL_STRING(_kern_geom_sched, OID_AUTO, schedulers, CTLFLAG_RD,
    &me.gs_names, 0, "Scheduler names");

/*
 * This module calls the scheduler algorithms with this lock held.
 * The functions are exposed so the scheduler algorithms can also
 * protect themselves e.g. when running a callout handler.
 */
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

/*
 * Handle references to the module, which are coming from devices
 * using this scheduler.
 */
static inline void
g_gsched_ref(struct g_gsched *gsp)
{

	atomic_add_int(&gsp->gs_refs, 1);
}

static inline void
g_gsched_unref(struct g_gsched *gsp)
{

	atomic_add_int(&gsp->gs_refs, -1);
}

void
g_sched_dispatch(struct g_geom *gp)
{
	struct g_sched_softc *sc = gp->softc;
	struct g_gsched *gsp = sc->sc_gsched;
	struct bio *bp;

	mtx_assert(&sc->sc_mtx, MTX_OWNED);
	while ((bp = gsp->gs_next(sc->sc_data)) != NULL)
		g_io_request(bp, LIST_FIRST(&gp->consumer));
}

static struct g_gsched *
g_gsched_find(const char *name)
{
	struct g_gsched *gsp = NULL;

	mtx_lock(&me.gs_mtx);
	LIST_FOREACH(gsp, &me.gs_scheds, glist)
		if (strcmp(name, gsp->gs_name) == 0) {
			g_gsched_ref(gsp);
			break;
		}
	mtx_unlock(&me.gs_mtx);

	return gsp;
}

/*
 * rebuild the list of scheduler names.
 * To be called with lock held.
 */
static void
g_gsched_build_names(struct g_gsched *gsp)
{
	int pos, l;
	struct g_gsched *cur;

	pos = 0;
	LIST_FOREACH(cur, &me.gs_scheds, glist) {
		l = strlen(cur->gs_name);
		if (l + pos + 1 + 1 < sizeof(me.gs_names)) {
			if (pos != 0)
				me.gs_names[pos++] = ' ';
			strcpy(me.gs_names + pos, cur->gs_name);
			pos += l;
		}
	}
	me.gs_names[pos] = '\0';
}

/*
 * Register or unregister individual scheduling algorithms.
 */
static int
g_gsched_register(struct g_gsched *gsp)
{
	struct g_gsched *cur;
	int error = 0;

	mtx_lock(&me.gs_mtx);
	LIST_FOREACH(cur, &me.gs_scheds, glist) {
		if (strcmp(gsp->gs_name, cur->gs_name) == 0)
			break;
	}
	if (cur != NULL) {
		G_SCHED_DEBUG(0, "A scheduler named %s already"
		    "exists.", gsp->gs_name);
		error = EEXIST;
	} else {
		LIST_INSERT_HEAD(&me.gs_scheds, gsp, glist);
		gsp->gs_refs = 1;
		me.gs_sched_count++;
		g_gsched_build_names(gsp);
	}
	mtx_unlock(&me.gs_mtx);

	return (error);
}

static int
g_gsched_unregister(struct g_gsched *gsp)
{
	struct g_gsched *cur, *tmp;
	int error = 0;
	struct g_geom *gp, *gp_tmp;

	error = 0;
	mtx_lock(&me.gs_mtx);

	/* scan stuff attached here ? */
	printf("%s, scan attached providers\n", __FUNCTION__);
        LIST_FOREACH_SAFE(gp, &g_sched_class.geom, geom, gp_tmp) {
		if (gp->class != &g_sched_class)
			continue; /* should not happen */
		g_sched_destroy(gp, 0);
        }


	LIST_FOREACH_SAFE(cur, &me.gs_scheds, glist, tmp) {
		if (cur != gsp)
			continue;
		if (gsp->gs_refs != 1) {
			G_SCHED_DEBUG(0, "%s still in use.", gsp->gs_name);
			error = EBUSY;
		} else {
			LIST_REMOVE(gsp, glist);
			me.gs_sched_count--;
			g_gsched_build_names(gsp);
		}
		break;
	}
	if (cur == NULL)
		G_SCHED_DEBUG(0, "%s not registered.", gsp->gs_name);

	mtx_unlock(&me.gs_mtx);

	return (error);
}

/*
 * Module event called when a scheduling algorithm module is loaded or
 * unloaded.
 */
int
g_gsched_modevent(module_t mod, int cmd, void *arg)
{
	struct g_gsched *gsp = arg;
	int error;

	error = EOPNOTSUPP;
	switch (cmd) {
	case MOD_LOAD:
		error = g_gsched_register(gsp);
		printf("loaded module %s error %d\n", gsp->gs_name, error);
		if (error == 0)
			g_retaste(&g_sched_class);
		break;
	case MOD_UNLOAD:
		error = g_gsched_unregister(gsp);
		printf("unloading for scheduler %s error %d\n",
			gsp->gs_name, error);
		break;
	};

	return (error);
}

/*
 * Lookup the identity of the issuer of the original request.
 * In the current implementation we use the curthread of the
 * issuer, but different mechanisms may be implemented later
 * so we do not make assumptions on the return value which for
 * us is just an opaque identifier.
 */
u_long
g_sched_classify(struct bio *bp)
{

        if (bp == NULL) {
                printf("g_sched_classify: NULL bio\n");
                return (0);     /* as good as anything */
        }
        while (bp->bio_parent != NULL)
                bp = bp->bio_parent;
        return ((u_long)(bp->bio_caller1));
}

/*
 * g_sched_done() and g_sched_start() dispatch the geom requests to
 * the scheduling algorithm in use.
 */
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
	/*
	 * Call the algorithm's gs_start to queue the request in the
	 * scheduler. If gs_start fails then pass the request down,
	 * otherwise call g_sched_dispatch() which tries to push
	 * one or more requests down.
	 */
	if (gsp->gs_start(sc->sc_data, cbp))
		g_io_request(cbp, LIST_FIRST(&gp->consumer));
	g_sched_dispatch(gp);
	g_sched_unlock(gp);
}

/*
 * The next few functions are the geom glue
 */
static void
g_sched_orphan(struct g_consumer *cp)
{

	g_topology_assert();
	g_sched_destroy(cp->geom, 1);
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

/*
 * Create a geom node for the device passed as *pp.
 * If successful, add a reference to this gsp.
 */
static int
g_sched_create(struct gctl_req *req, struct g_class *mp,
    struct g_provider *pp, struct g_gsched *gsp)
{
	struct g_sched_softc *sc;
	struct g_geom *gp;
	struct g_provider *newpp = NULL;
	struct g_consumer *cp = NULL;
	char name[64];
	int error;

	g_topology_assert();

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
		error = ENOMEM;
		goto fail;
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
		const char *msg = force ?
			"but we force removal" : "cannot remove";

		G_SCHED_DEBUG( (force ? 0 : 1) ,
			 "Device %s is still open (r%dw%de%d), %s.",
			    pp->name, pp->acr, pp->acw, pp->ace, msg);
		if (!force)
			return (EBUSY);
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

/*
 * Functions related to the classification of requests.
 * In principle, we need to store in the 'struct bio' a reference
 * to the issuer of the request and all info that can be useful for
 * classification, accounting and so on.
 * In the final version this should be done by adding some extra
 * field(s) to struct bio, and marking the bio as soon as it is
 * posted to the geom queue (but not later, as requests are managed
 * by the g_down thread afterwards).
 *
 * XXX TEMPORARY SOLUTION:
 * The 'struct bio' in 7.x and 6.x does not have a field for storing
 * the classification info, so we abuse the caller1 field in the
 * root element of the bio tree. The marking is done at the beginning
 * of g_io_request() and only if we find that the field is NULL.
 *
 * To avoid rebuilding the kernel, this module will patch the
 * initial part of g_io_request() so it jumps to a trampoline code
 * that calls the marking function (  g_new_io_request() ) and then
 * executes the original body of g_io_request().
 * THIS IS A HACK THAT WILL GO AWAY IN THE FINAL VERSION.
 *
 * We must be careful with the compiler, as it may clobber the
 * parameters on the stack so they are not preserved for the
 * continuation of the original function.
 * Ideally we should write everything in assembler:

	mov 0x8(%esp), %eax	// load bp
    2:	mov %eax, %edx
	mov    0x64(%edx),%eax	// load bp->bio_parent
	test   %eax,%eax
	jne	2b		// follow the pointer
    	mov    0x30(%edx),%eax	// load bp->bio_caller1
	test   %eax,%eax
	jne	1f		// already set, never mind
	mov    %fs:0x0,%eax	// pcpu pointer
	mov    0x34(%eax),%eax	// curthread
	mov    %eax,0x30(%edx)	// store in bp->bio_caller1
    1:  // header of the old function
	push %ebp
	mov %esp, %ebp
	push %edi
	push %esi
	  jmp x+5

 */

#if !defined(__i386__)
#error please add the code in g_new_io_request() to the beginning of \
	/sys/geom/geom_io.c::g_io_request(), and remove this line.
#else
/* i386-only code, trampoline + patching support */
static unsigned char
g_io_trampoline[] = {
        0xe8, 0x00, 0x00, 0x00, 0x00,   /* call foo */
        0x55,                           /* push %ebp */
        0x89, 0xe5,                     /* mov    %esp,%ebp */
        0x57,                           /* push %edi */
        0x56,                           /* push %esi */
        0xe9, 0x00, 0x00, 0x00, 0x00,   /* jmp x+5 */
};

static int
g_new_io_request(const char *ret, struct bio *bp, struct g_consumer *cp)
{

        /*
         * bio classification: if bio_caller1 is available in the
         * root of the 'struct bio' tree, store there the thread id
         * of the thread that originated the request.
         * More sophisticated classification schemes can be used.
         * XXX do not change this code without making sure that
	 * the compiler does not clobber the arguments.
         */
	struct bio *top = bp;
	if (top) {
                while (top->bio_parent)
                        top = top->bio_parent;
                if (top->bio_caller1 == NULL)
                        top->bio_caller1 = (void *)curthread->td_tid;
        }
	return (bp != top); /* prevent compiler from clobbering bp */
}

static int
g_io_patch(void *f, void *p, void *new_f)
{
	int found = bcmp(f, (const char *)p + 5, 5);

	printf("match result %d\n", found);
        if (found == 0) {
                int ofs;

		printf("patching g_io_request\n");
                /* link the trampoline to the new function */
                ofs = (int)new_f - ((int)p + 5);
                bcopy(&ofs, (char *)p + 1, 4);
                /* jump back to the original + 5 */
                ofs = ((int)f + 5) - ((int)p + 15);
                bcopy(&ofs, (char *)p + 11, 4);
                /* patch the original address with a jump to the trampoline */
                *(unsigned char *)f = 0xe9;     /* jump opcode */
                ofs = (int)p - ((int)f + 5);
                bcopy(&ofs, (char *)f + 1, 4);
		me.gs_patched = 1;
        }
        return 0;
}
#endif /* __i386__ */

static void
g_sched_init(struct g_class *mp)
{

	mtx_init(&me.gs_mtx, "gsched", NULL, MTX_DEF);
	LIST_INIT(&me.gs_scheds);

	printf("%s loading...\n", __FUNCTION__);
#if defined(__i386__)
	/* patch g_io_request to set the thread */
	g_io_patch(g_io_request, g_io_trampoline, g_new_io_request);
#endif
}

static void
g_sched_fini(struct g_class *mp)
{

#if defined(__i386__)
	if (me.gs_patched) {
		printf("/* restore the original g_io_request */\n");
		bcopy(g_io_trampoline + 5, (char *)g_io_request, 5);
	}
#endif
	printf("%s unloading...\n", __FUNCTION__);
	KASSERT(LIST_EMPTY(&gs_scheds), ("still registered schedulers"));
	mtx_destroy(&me.gs_mtx);
}

/*
 * We accept a "/dev/" prefix on device names, we want the
 * provider name that is after that.
 */
static const char *dev_prefix = "/dev/";

/*
 * read the i-th argument for a request
 */
static const char *
g_sched_argi(struct gctl_req *req, int i)
{
	const char *name;
	char param[16];
	int l = strlen(dev_prefix);

	snprintf(param, sizeof(param), "arg%d", i);
	name = gctl_get_asciiparam(req, param);
	if (name == NULL) {
		gctl_error(req, "No 'arg%d' argument", i);
		return NULL;
	}
	if (strncmp(name, dev_prefix, l) == 0)
		name += l;
	return name;
}

/*
 * fetch nargs and do appropriate checks.
 */
static int
g_sched_get_nargs(struct gctl_req *req)
{
	int *nargs;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No 'nargs' argument");
		return 0;
	}
	if (*nargs <= 0)
		gctl_error(req, "Missing device(s).");
	return *nargs;
}

/*
 * Check whether we should add the class on certain volumes when
 * this geom is created. Right now this is under control of a kenv
 * variable containing the names of all devices that we care about.
 */
static struct g_geom *
g_sched_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_gsched *gsp = NULL;	/* the sched. algorithm we want */
	const char *s;		/* generic string pointer */
	const char *taste_names;	/* devices we like */
	int l;
    
        g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
        g_topology_assert();
 
        G_SCHED_DEBUG(2, "Tasting %s.", pp->name);

	do {
		/* do not allow taste on ourselves */
		if (strcmp(pp->geom->class->name, mp->name) == 0)
                	break;

		taste_names = getenv("geom.sched.taste");
		if (taste_names == NULL)
			break;

		l = strlen(pp->name);
		for (s = taste_names; *s &&
		    (s = strstr(s, pp->name)); s++) {
			/* further checks for an exact match */
			if ( (s == taste_names || s[-1] == ' ') &&
			     (s[l] == '\0' || s[l] == ' ') )
				break;
		}
		if (s == NULL)
			break;
		printf("attach device %s match [%s]\n",
			pp->name, s);

		/* look up the provider name in the list */
		s = getenv("geom.sched.algo");
		if (s == NULL)
			s = "rr";

		gsp = g_gsched_find(s);	/* also get a reference */
		if (gsp == NULL) {
			printf("Bad '%s' algorithm\n", s);
			break;
		}

		g_sched_create(NULL, mp, pp, gsp);
		g_gsched_unref(gsp);
	} while (0);
	return NULL;
}

static void
g_sched_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	struct g_provider *pp;
	struct g_gsched *gsp;
	const char *name;
	int i, nargs;

	g_topology_assert();

	name = gctl_get_asciiparam(req, "sched");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument", "sched");
		return;
	}

	gsp = g_gsched_find(name);	/* also get a reference */
	if (gsp == NULL) {
		gctl_error(req, "Bad '%s' argument", "sched");
		return;
	}

	nargs = g_sched_get_nargs(req);

	/*
	 * Run on the arguments, and break on any error.
	 * We look for a device name, but skip the /dev/ prefix if any.
	 */
	for (i = 0; i < nargs; i++) {
		name = g_sched_argi(req, i);
		if (name == NULL)
			break;
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_SCHED_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			break;
		}
		if (g_sched_create(req, mp, pp, gsp) != 0)
			break;
	}

	g_gsched_unref(gsp);
}

static void
g_sched_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_sched_softc *sc;
	struct g_provider *pp;
	const char *name;
	int i, nargs;

	g_topology_assert();

	nargs = g_sched_get_nargs(req);

	for (i = 0; i < nargs; i++) {
		name = g_sched_argi(req, i);
		if (name == NULL)
			break;
		pp = g_provider_by_name(name);
		if (pp == NULL || pp->geom->class != mp) {
			G_SCHED_DEBUG(1, "Provider %s is invalid.", name);
			gctl_error(req, "Provider %s is invalid.", name);
			break;
		}

		sc = pp->geom->softc;
		/* still unimplemented, so we exit! */
		gctl_error(req, "Reconfiguration not supported yet.");
		break;
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
	int nargs, *force, error, i;
	struct g_geom *gp;
	const char *name;

	g_topology_assert();

	nargs = g_sched_get_nargs(req);

	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No 'force' argument");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = g_sched_argi(req, i);
		if (name == NULL)
			break;

		gp = g_sched_find_geom(mp, name);
		if (gp == NULL) {
			G_SCHED_DEBUG(1, "Device %s is invalid.", name);
			gctl_error(req, "Device %s is invalid.", name);
			break;
		}

		error = g_sched_destroy(gp, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    gp->name, error);
			break;
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
MODULE_VERSION(geom_sched, 0);
