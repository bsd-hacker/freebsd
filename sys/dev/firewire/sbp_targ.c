/*-
 * Copyright (C) 2003
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#if __FreeBSD_version < 500000
#include <sys/devicestat.h>
#endif

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/firewire/sbp.h>
#include <dev/firewire/fwmem.h>
#include <dev/firewire/fwcsr.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#define SBP_TARG_RECV_LEN	8
#define MAX_INITIATORS		8
#define MAX_LUN			63
#define MAX_LOGINS		63
#define MAX_NODES		63
/*
 * management/command block agent registers
 *
 * BASE 0xffff f001 0000 management port
 * BASE 0xffff f001 0020 command port for login id 0
 * BASE 0xffff f001 0040 command port for login id 1
 * BASE 0xffff f001 (0x20 * [login_id + 1]) for login_id
 *
 */
#define SBP_TARG_MGM	 0x10000	/* offset from 0xffff f000 0000 */
#define SBP_TARG_BIND_HI	0xffff
#define SBP_TARG_BIND_LO(l)	(0xf0000000 + SBP_TARG_MGM + 0x20 * ((l) + 1))
#define SBP_TARG_BIND_START	(((u_int64_t)SBP_TARG_BIND_HI << 32) | \
				    SBP_TARG_BIND_LO(-1))
#define SBP_TARG_BIND_END	(((u_int64_t)SBP_TARG_BIND_HI << 32) | \
				    SBP_TARG_BIND_LO(MAX_LOGINS))
#define SBP_TARG_LOGIN_ID(lo)	(((lo) - SBP_TARG_BIND_LO(0))/0x20)

#define FETCH_MGM	0
#define FETCH_CMD	1
#define FETCH_POINTER	2

#define F_LINK_ACTIVE	(1 << 0) /* The F/W link is active */
#define F_ATIO_STARVED	(1 << 1) /* We are out of ATIO's */
#define F_LOGIN		(1 << 2) /* This initiator has logged in */
#define F_HOLD		(1 << 3) /* Hold on to this login */
#define F_FREEZED	(1 << 4) /* Frozen login, usually a bus reset occured */
#define F_RECYCLE_LOGIN	(1 << 5) /* This login is to be reused due */
				 /* to login before SBP_TARG_HOLD_TIMEOUT */
#define F_EXCLUSIVE	(1 << 6) /* Reject all other logins. */

#define SBP_TARG_HOLD_TIMEOUT 1

MALLOC_DEFINE(M_SBP_TARG, "sbp_targ", "SBP-II/FireWire target mode");

static int debug = 1;

SYSCTL_INT(_debug, OID_AUTO, sbp_targ_debug, CTLFLAG_RW, &debug, 0,
        "SBP target mode debug flag");

struct sbp_targ_login {
	struct sbp_targ_lstate *lstate;
	struct fw_device *fwdev;
	struct sbp_login_res loginres;
	uint16_t fifo_hi; 
	uint16_t last_hi;
	uint32_t fifo_lo; 
	uint32_t last_lo;
	STAILQ_HEAD(, orb_info) orbs;
	STAILQ_ENTRY(sbp_targ_login) link;
	uint16_t hold_sec;
	uint16_t id;
	uint8_t flags; 
	uint8_t spd; 
	struct callout hold_callout;
};

struct sbp_targ_lstate {
	uint16_t lun;
	struct sbp_targ_softc *sc;
	struct cam_path *path;
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
	struct crom_chunk model;
	uint32_t flags; 
	STAILQ_HEAD(, sbp_targ_login) logins;
};

struct sbp_targ_softc {
        struct firewire_dev_comm fd;
	struct cam_sim *sim;
	struct cam_path *path;
	struct fw_bind fwb;
	struct fw_bind busy_timeout;
	struct fw_bind reset_start;
	int ndevs;
	int flags;
	struct crom_chunk unit;
	struct sbp_targ_lstate *lstate[MAX_LUN];
	struct sbp_targ_lstate *black_hole;
	struct sbp_targ_login *logins[MAX_LOGINS];
	struct mtx mtx;
};
#define SBP_LOCK(sc) mtx_lock(&(sc)->mtx)
#define SBP_UNLOCK(sc) mtx_unlock(&(sc)->mtx)

struct corb4 {
#if BYTE_ORDER == BIG_ENDIAN
	uint32_t n:1,
		  rq_fmt:2,
		  :1,
		  dir:1,
		  spd:3,
		  max_payload:4,
		  page_table_present:1,
		  page_size:3,
		  data_size:16;
#else
	uint32_t data_size:16,
		  page_size:3,
		  page_table_present:1,
		  max_payload:4,
		  spd:3,
		  dir:1,
		  :1,
		  rq_fmt:2,
		  n:1;
#endif
};

struct morb4 {
#if BYTE_ORDER == BIG_ENDIAN
	uint16_t reserved;
	uint16_t off_hi;
	uint32_t off_lo;
	uint64_t reserved2;	
	uint32_t n:1,
		  rq_fmt:2,
		  :9,
		  fun:4,
		  id:16;
#else
	uint32_t id:16,
		  fun:4,
		  :9,
		  rq_fmt:2,
		  n:1;
	uint64_t reserved2;
	uint32_t off_lo;
	uint16_t off_hi;
	uint16_t reserved;
#endif
};

/*
 * Urestricted page table format 
 * states that the segment length
 * and high base addr are in the first
 * 32 bits and the base low is in 
 * the second
 */
struct unrestricted_page_table_fmt {
	uint16_t segment_len;
	uint16_t segment_base_high;
	uint32_t segment_base_low;
};

struct orb_info {
	struct sbp_targ_softc *sc;
	struct fw_device *fwdev;
	struct sbp_targ_login *login;
	union ccb *ccb;
	struct ccb_accept_tio *atio;
	uint8_t state; 
#define ORBI_STATUS_NONE	0
#define ORBI_STATUS_FETCH	1
#define ORBI_STATUS_ATIO	2
#define ORBI_STATUS_CTIO	3
#define ORBI_STATUS_STATUS	4
#define ORBI_STATUS_POINTER	5
#define ORBI_STATUS_ABORTED	7
	uint8_t refcount; 
	uint16_t orb_hi;
	uint32_t orb_lo;
	uint32_t data_hi;
	uint32_t data_lo;
	struct corb4 orb4;
	STAILQ_ENTRY(orb_info) link;
	uint32_t orb[8];
	struct unrestricted_page_table_fmt *page_table;
	struct unrestricted_page_table_fmt *cur_pte;
	struct unrestricted_page_table_fmt *last_pte;
	uint32_t  last_block_read;
	struct sbp_status status;
};

struct agent_state {
	uint32_t fetch_agent_state;
#define AGENT_STATE_RESET 0
#define AGENT_STATE_ACTIVE 1
#define AGENT_STATE_SUSPENDED 2
#define AGENT_STATE_DEAD 3
	uint32_t bus_reset_command_reset_init_vals;
	uint32_t read_vals;
	uint32_t write_effects;
};

static char *orb_fun_name[] = {
	ORB_FUN_NAMES
};

static void sbp_targ_recv(struct fw_xfer *);
static void sbp_targ_fetch_orb(struct sbp_targ_softc *, struct fw_device *,
    uint16_t, uint32_t, struct sbp_targ_login *, int);
static void sbp_targ_abort(struct sbp_targ_softc *, struct orb_info *);
static void sbp_targ_xfer_pt(struct orb_info *);
static void sbp_targ_send_agent_state(struct fw_xfer *, int state);

static void
sbp_targ_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "sbp_targ", device_get_unit(parent));
}

static int
sbp_targ_probe(device_t dev)
{
	device_t pa;

	pa = device_get_parent(dev);
	if(device_get_unit(dev) != device_get_unit(pa)){
		return(ENXIO);
	}

	device_set_desc(dev, "SBP-2/SCSI over FireWire target mode");
	return (0);
}

static void
sbp_targ_dealloc_login(struct sbp_targ_login *login)
{
	struct orb_info *orbi, *next;

	if (login == NULL) {
		printf("%s: login = NULL\n", __func__);
		return;
	}
	for (orbi = STAILQ_FIRST(&login->orbs); orbi != NULL; orbi = next) {
		next = STAILQ_NEXT(orbi, link);
		if (debug)
			printf("%s: free orbi %p\n", __func__, orbi);
		free(orbi, M_SBP_TARG);
		orbi = NULL;
	}
	callout_stop(&login->hold_callout);

	STAILQ_REMOVE(&login->lstate->logins, login, sbp_targ_login, link);
	login->lstate->sc->logins[login->id] = NULL;
	if (debug)
		printf("%s: free login %p\n", __func__, login);
	free((void *)login, M_SBP_TARG);
	login = NULL;
}

static void
sbp_targ_hold_expire(void *arg)
{
	struct sbp_targ_login *login;

	login = (struct sbp_targ_login *)arg;

	/* if the login has been deallocated
	 * prior to the login timeout, login
	 * should be NULL, and we should do 
	 * nothing
	 */
	if (login != NULL ) {
		if (login->flags & F_HOLD) {
			if (debug)
				printf("%s: login(%p), "
					"login_id=%d expired\n",
					__func__, login, login->id);
			sbp_targ_dealloc_login(login);
		} else if (debug) {
			printf("%s: login(%p), "
				"login_id=%d not hold\n",
				__func__, login, login->id);
		}
	} else if (debug)
		printf("%s: woke up and this login was NULL\n", __func__);
}

static void
sbp_targ_post_busreset(void *arg)
{
	struct sbp_targ_softc *sc;
	struct crom_src *src;
	struct crom_chunk *root;
	struct crom_chunk *unit;
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	int i;

	sc = (struct sbp_targ_softc *)arg;
	src = sc->fd.fc->crom_src;
	root = sc->fd.fc->crom_root;

	unit = &sc->unit;

	SBP_LOCK(sc);
	if ((sc->flags & F_FREEZED) == 0) {
		sc->flags |= F_FREEZED;
		printf("%s: freezing simq\n", __func__);
		xpt_freeze_simq(sc->sim, /*count*/1);
	} else if (debug)
		printf("%s: already freezed\n", __func__);
	SBP_UNLOCK(sc);
	

	bzero(unit, sizeof(struct crom_chunk));

	crom_add_chunk(src, root, unit, CROM_UDIR);
	crom_add_entry(unit, CSRKEY_SPEC, CSRVAL_ANSIT10);
	crom_add_entry(unit, CSRKEY_VER, CSRVAL_T10SBP2);
	crom_add_entry(unit, CSRKEY_COM_SPEC, CSRVAL_ANSIT10);
	crom_add_entry(unit, CSRKEY_COM_SET, CSRVAL_SCSI);

	crom_add_entry(unit, CROM_MGM, SBP_TARG_MGM >> 2);
	crom_add_entry(unit, CSRKEY_UNIT_CH, (10<<8) | 8);

	for (i = 0; i < MAX_LUN; i ++) {
		lstate = sc->lstate[i];
		if (lstate == NULL)
			continue;
		if (debug)
			printf("%s:  lstate not null\n", __func__);
		crom_add_entry(unit, CSRKEY_FIRM_VER, 1);
		crom_add_entry(unit, CROM_LUN, i);
		crom_add_entry(unit, CSRKEY_MODEL, 1);
		crom_add_simple_text(src, unit, &lstate->model, "TargetMode");
	}

	/* Process for reconnection hold time */
	for (i = 0; i < MAX_LOGINS; i ++) {
		login = sc->logins[i];
		if (login == NULL)
			continue;
		sbp_targ_abort(sc, STAILQ_FIRST(&login->orbs));
		if (login->flags & F_LOGIN) {
#if 0
			if (debug)
				printf("%s: setting sbp_targ_hold_expire "
					"for this login(%p)\n",
					__func__, login);
#endif
			login->flags |= F_HOLD;
			login->flags &= ~F_LOGIN;
			callout_reset(&login->hold_callout,
			    hz * login->hold_sec, 
			    sbp_targ_hold_expire, (void *)login);
		} else if (debug) {
			/* Do nothing, allow sbp_Targ_hold_expire */
			/* to delete the login */
			printf("%s login->flags = %0x, "
			       "not F_LOGIN\n",
			       __func__, login->flags);
		}
	}
}

static void
sbp_targ_post_explore(void *arg)
{
	struct sbp_targ_softc *sc;

	sc = (struct sbp_targ_softc *)arg;
	SBP_LOCK(sc);
	sc->flags &= ~F_FREEZED;
	printf("%s: releasing simq\n", __func__);
	xpt_release_simq(sc->sim, /*run queue*/TRUE);
	SBP_UNLOCK(sc);
	return;
}

static cam_status
sbp_targ_find_devs(struct sbp_targ_softc *sc, union ccb *ccb,
    struct sbp_targ_lstate **lstate, int notfound_failure)
{
	u_int lun;

	/* XXX 0 is the only vaild target_id */
	if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD &&
	    ccb->ccb_h.target_lun == CAM_LUN_WILDCARD) {
		*lstate = sc->black_hole;
		if (debug > 2)
			printf("%s: setting black hole for this target id(%d)\n",
			       __func__, ccb->ccb_h.target_id);
	} else {
		lun = ccb->ccb_h.target_lun;
		if (lun >= MAX_LUN)
			return (CAM_LUN_INVALID);
	
		*lstate = sc->lstate[lun];

		if (notfound_failure != 0 && *lstate == NULL) {
			if (debug)
				printf("%s: lstate for lun is invalid,"
			       		" target(%d), lun(%d)\n",
			       		__func__,
			       		ccb->ccb_h.target_id, lun);
			return (CAM_PATH_INVALID);
		} else if (debug > 2)
			printf("%s: setting lstate for tgt(%d) lun(%d)\n",
				__func__,ccb->ccb_h.target_id, lun);
	}
	return (CAM_REQ_CMP);
}

static void
sbp_targ_en_lun(struct sbp_targ_softc *sc, union ccb *ccb)
{
	struct ccb_en_lun *cel = &ccb->cel;
	struct sbp_targ_lstate *lstate;
	cam_status status;

	status = sbp_targ_find_devs(sc, ccb, &lstate, 0);
	if (status != CAM_REQ_CMP) {
		ccb->ccb_h.status = status;
		return;
	}

	if (cel->enable != 0) {
		if (lstate != NULL) {
			xpt_print_path(ccb->ccb_h.path);
			printf("Lun already enabled\n");
			ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
			return;
		}
		if (cel->grp6_len != 0 || cel->grp7_len != 0) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			printf("Non-zero Group Codes\n");
			return;
		}
		lstate = (struct sbp_targ_lstate *)
		    malloc(sizeof(*lstate), M_SBP_TARG, M_NOWAIT | M_ZERO);
		if (lstate == NULL) {
			xpt_print_path(ccb->ccb_h.path);
			printf("Couldn't allocate lstate\n");
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			return;
		} else {
			if (debug) {
				printf("%s: malloc'd lstate %p\n",
				       __func__, lstate);
			}
		}	
		if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD) {
			sc->black_hole = lstate;
			if (debug)
				printf("Blackhole set due to target id == %d\n",
					ccb->ccb_h.target_id);
		} else
			sc->lstate[ccb->ccb_h.target_lun] = lstate;

		lstate->sc = sc;
		status = xpt_create_path(&lstate->path, /*periph*/NULL,
					 xpt_path_path_id(ccb->ccb_h.path),
					 xpt_path_target_id(ccb->ccb_h.path),
					 xpt_path_lun_id(ccb->ccb_h.path));
		if (status != CAM_REQ_CMP) {
			free(lstate, M_SBP_TARG);
			lstate = NULL;
			xpt_print_path(ccb->ccb_h.path);
			printf("Couldn't allocate path\n");
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			return;
		}
		SLIST_INIT(&lstate->accept_tios);
		SLIST_INIT(&lstate->immed_notifies);
		STAILQ_INIT(&lstate->logins);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_print_path(ccb->ccb_h.path);
		printf("Lun now enabled for target mode\n");
	} else {
		struct sbp_targ_login *login, *next;

		if (lstate == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			printf("Invalid lstate for this target\n");
			return;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;

		if (SLIST_FIRST(&lstate->accept_tios) != NULL) {
			printf("ATIOs pending\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
		}

		if (SLIST_FIRST(&lstate->immed_notifies) != NULL) {
			printf("INOTs pending\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
		}

		if (ccb->ccb_h.status != CAM_REQ_CMP) {
			printf("status != CAM_REQ_CMP\n");
			return;
		}

		xpt_print_path(ccb->ccb_h.path);
		printf("Target mode disabled\n");
		xpt_free_path(lstate->path);

		for (login = STAILQ_FIRST(&lstate->logins); login != NULL;
		    login = next) {
			next = STAILQ_NEXT(login, link);
			sbp_targ_dealloc_login(login);
		}

		if (ccb->ccb_h.target_id == CAM_TARGET_WILDCARD)
			sc->black_hole = NULL;
		else
			sc->lstate[ccb->ccb_h.target_lun] = NULL;
		if (debug)
			printf("%s: free lstate %p\n", __func__, lstate);
		if (lstate != NULL) {
			free(lstate, M_SBP_TARG);
			lstate = NULL;
		} else if (debug)
			printf("%s: lstate was null\n", __func__);
	}
	/* initiate bus reset */
	sc->fd.fc->ibr(sc->fd.fc);
}

static void
sbp_targ_send_lstate_events(struct sbp_targ_softc *sc,
    struct sbp_targ_lstate *lstate)
{
#if 0
	struct ccb_hdr *ccbh;
	struct ccb_immed_notify *inot;

	printf("%s: not implemented yet\n", __func__);
#endif
}

static __inline void
sbp_targ_remove_orb_info_locked(struct sbp_targ_login *login, struct orb_info *orbi)
{
	STAILQ_REMOVE(&login->orbs, orbi, orb_info, link);
}

static __inline void
sbp_targ_remove_orb_info(struct sbp_targ_login *login, struct orb_info *orbi)
{
	SBP_LOCK(orbi->sc);
	STAILQ_REMOVE(&login->orbs, orbi, orb_info, link);
	SBP_UNLOCK(orbi->sc);
}

/*
 * tag_id/init_id encoding
 *
 * tag_id and init_id has only 32bit for each.
 * scsi_target can handle very limited number(up to 15) of init_id.
 * we have to encode 48bit orb and 64bit EUI64 into these
 * variables.
 *
 * tag_id represents lower 32bit of ORB address.
 * init_id represents login_id.
 *
 */

static struct orb_info *
sbp_targ_get_orb_info(struct sbp_targ_lstate *lstate,
    u_int tag_id, u_int init_id)
{
	struct sbp_targ_login *login;
	struct orb_info *orbi;

	login = lstate->sc->logins[init_id];
	if (login == NULL) {
		printf("%s: no such login\n", __func__);
		return (NULL);
	}
	STAILQ_FOREACH(orbi, &login->orbs, link) {
		if (orbi->orb_lo == tag_id)
			goto found;
		if ( orbi->state == ORBI_STATUS_ABORTED)
			printf("%s: orb aborted in flight tag_id=0x%08x\n",
				__func__, tag_id);
		else {
			if (debug)
				printf("%s: orb not found tag_id=0x%08x,"
					" state(%0x)\n",
			 		__func__, tag_id, orbi->state);
		}
	}
	return (NULL);
found:
	return (orbi);
}

static void
sbp_targ_abort(struct sbp_targ_softc *sc, struct orb_info *orbi)
{
	struct orb_info *norbi;

	SBP_LOCK(sc);
	for (; orbi != NULL; orbi = norbi) {
		printf("%s: status=%d ccb=%p\n",
		       __func__, orbi->state, orbi->ccb);
		norbi = STAILQ_NEXT(orbi, link);
		if (orbi->state != ORBI_STATUS_ABORTED) {
			if (orbi->ccb != NULL) {
				orbi->ccb->ccb_h.status = CAM_REQ_ABORTED;
				xpt_done(orbi->ccb);
				orbi->ccb = NULL;
			}
			if (orbi->state <= ORBI_STATUS_ATIO) {
				sbp_targ_remove_orb_info_locked(orbi->login, orbi);
				if (debug) {
					printf("%s: free orbi %p\n",
					       __func__, orbi);
				}
				free(orbi, M_SBP_TARG);
				orbi = NULL;
			} else
				orbi->state = ORBI_STATUS_ABORTED;
		}
	}
	SBP_UNLOCK(sc);
}

static void
sbp_targ_free_orbi(struct fw_xfer *xfer)
{
	struct orb_info *orbi;

	if (xfer->resp != 0) {
		/* XXX */
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
	}

	orbi = (struct orb_info *)xfer->sc;
	if ( orbi->page_table != NULL ) {
		if (debug)
			printf("%s:  free orbi->page_table %p\n",
			       __func__, orbi->page_table);
		free(orbi->page_table, M_SBP_TARG);
		orbi->page_table = NULL;
	}
	if (debug)
		printf("%s: free orbi %p\n", __func__, orbi);
	free(orbi, M_SBP_TARG);
	orbi = NULL;
	fw_xfer_free(xfer);
}

static void
sbp_targ_status_FIFO(struct orb_info *orbi,
    uint32_t fifo_hi, uint32_t fifo_lo, int dequeue)
{
	struct fw_xfer *xfer;
	if (dequeue)
		sbp_targ_remove_orb_info(orbi->login, orbi); 

	xfer = fwmem_write_block(orbi->fwdev, (void *)orbi,
    		orbi->fwdev->speed, fifo_hi, fifo_lo,
    		sizeof(uint32_t) * (orbi->status.len + 1),
		(char *)&orbi->status,
    		sbp_targ_free_orbi);

	if (xfer == NULL) {
		/* XXX */
		printf("%s: xfer == NULL\n", __func__);
	}
}

/*
 * Generate the appropriate CAM status for the
 * target.
 */
static void
sbp_targ_send_status(struct orb_info *orbi, union ccb *ccb)
{
	struct sbp_status *sbp_status;

	sbp_status = &orbi->status;

	orbi->state = ORBI_STATUS_STATUS;

	sbp_status->resp = 0; /* XXX */
	sbp_status->status = 0; /* XXX */
	sbp_status->dead = 0; /* XXX */

	ccb->ccb_h.status= CAM_REQ_CMP;
	switch (ccb->csio.scsi_status) {
	case SCSI_STATUS_OK:
		if (debug)
			printf("%s: STATUS_OK\n", __func__);
		sbp_status->len = 1;
		break;
	case SCSI_STATUS_CHECK_COND:
		if (debug)
			printf("%s: STATUS SCSI_STATUS_CHECK_COND\n", __func__);
		goto process_scsi_status;
	case SCSI_STATUS_BUSY:
		if (debug)
			printf("%s: STATUS SCSI_STATUS_BUSY\n", __func__);
		goto process_scsi_status;
	case SCSI_STATUS_CMD_TERMINATED:
		if (debug)
			printf("%s: STATUS SCSI_STATUS_CMD_TERMINATED\n",
			       __func__);
process_scsi_status:
	{
		struct sbp_cmd_status *sbp_cmd_status;
		struct scsi_sense_data *sense;

		sbp_cmd_status = (struct sbp_cmd_status *)&sbp_status->data[0];
		sbp_cmd_status->status = ccb->csio.scsi_status;
		sense = &ccb->csio.sense_data;

		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		if ((sense->error_code & SSD_ERRCODE) == SSD_CURRENT_ERROR)
			sbp_cmd_status->sfmt = SBP_SFMT_CURR;
		else
			sbp_cmd_status->sfmt = SBP_SFMT_DEFER;

		sbp_cmd_status->valid = 1;
		sbp_cmd_status->s_key = sense->flags & SSD_KEY;
		sbp_cmd_status->mark = (sense->flags & SSD_FILEMARK)? 1 : 0;
		sbp_cmd_status->eom = (sense->flags & SSD_EOM) ? 1 : 0;
		sbp_cmd_status->ill_len = (sense->flags & SSD_ILI) ? 1 : 0;

		bcopy(&sense->info[0], &sbp_cmd_status->info, 4);

		if (sense->extra_len <= 6)
			/* add_sense_code(_qual), info, cmd_spec_info */
			sbp_status->len = 4;
		else
			/* fru, sense_key_spec */
			sbp_status->len = 5;
			
		bcopy(&sense->cmd_spec_info[0], &sbp_cmd_status->cdb, 4);

		sbp_cmd_status->s_code = sense->add_sense_code;
		sbp_cmd_status->s_qlfr = sense->add_sense_code_qual;
		sbp_cmd_status->fru = sense->fru;

		bcopy(&sense->sense_key_spec[0],
		    &sbp_cmd_status->s_keydep[0], 3);

		ccb->ccb_h.status |= CAM_SENT_SENSE;
		break;
	}
	default:
		printf("%s: unknown scsi status 0x%x\n", __func__,
		    sbp_status->status);
	}

	sbp_targ_status_FIFO(orbi,
	    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);

}

/*
 * Invoked as a callback handler from fwmem_read/write_block
 *
 * Process read/write of initiator address space
 * completion and pass status onto the backend target.
 * If this is a partial read/write for a CCB then
 * we decrement the orbi's refcount to indicate
 * the status of the read/write is complete
 */
static void
sbp_targ_cam_done(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	union ccb *ccb;

	orbi = (struct orb_info *)xfer->sc;

	if (debug)
		printf("%s: resp=%d refcount=%d\n", __func__,
			xfer->resp, orbi->refcount);
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_DATA | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));
	}

	orbi->refcount--;

	ccb = orbi->ccb;
	if (orbi->refcount == 0) {
		if (orbi->state == ORBI_STATUS_ABORTED) {
			if (debug)
				printf("%s: orbi aborted\n", __func__);
			sbp_targ_remove_orb_info(orbi->login, orbi);
			if (orbi->page_table != NULL) {
				if (debug)
					printf("%s: free orbi->page_table %p\n",
						__func__, orbi->page_table);
				free(orbi->page_table, M_SBP_TARG);
				orbi->page_table = NULL;
			}
			if (debug)
				printf("%s: free orbi %p\n", __func__, orbi);
			free(orbi, M_SBP_TARG);
			orbi = NULL;
		} else if (orbi->status.resp == ORBI_STATUS_NONE) {
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
				if (debug) {
					printf("%s: CAM_SEND_STATUS set %0x\n",
					       __func__, ccb->ccb_h.flags);
				}
				sbp_targ_send_status(orbi, ccb);
			} else {
				if (debug) {
					printf("%s: CAM_SEND_STATUS !set %0x\n",
					       __func__, ccb->ccb_h.flags);
				}
				ccb->ccb_h.status = CAM_REQ_CMP;
			}
			SBP_LOCK(orbi->sc);
			xpt_done(ccb);
			SBP_UNLOCK(orbi->sc);
		} else {
			orbi->status.len = 1;
			sbp_targ_status_FIFO(orbi,
		    	    orbi->login->fifo_hi, orbi->login->fifo_lo,
			    /*dequeue*/1);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			SBP_LOCK(orbi->sc);
			xpt_done(ccb);
			SBP_UNLOCK(orbi->sc);
		}
	}

	fw_xfer_free(xfer);
}

static cam_status
sbp_targ_abort_ccb(struct sbp_targ_softc *sc, union ccb *ccb)
{
	union ccb *accb;
	struct sbp_targ_lstate *lstate;
	struct ccb_hdr_slist *list;
	struct ccb_hdr *curelm;
	int found;
	cam_status status;

	status = sbp_targ_find_devs(sc, ccb, &lstate, 0);
	if (status != CAM_REQ_CMP)
		return (status);

	accb = ccb->cab.abort_ccb;

	if (accb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO)
		list = &lstate->accept_tios;
	else if (accb->ccb_h.func_code == XPT_IMMED_NOTIFY)
		list = &lstate->immed_notifies;
	else
		return (CAM_UA_ABORT);

	curelm = SLIST_FIRST(list);
	found = 0;
	if (curelm == &accb->ccb_h) {
		found = 1;
		SLIST_REMOVE_HEAD(list, sim_links.sle);
	} else {
		while(curelm != NULL) {
			struct ccb_hdr *nextelm;

			nextelm = SLIST_NEXT(curelm, sim_links.sle);
			if (nextelm == &accb->ccb_h) {
				found = 1;
				SLIST_NEXT(curelm, sim_links.sle) =
				    SLIST_NEXT(nextelm, sim_links.sle);
				break;
			}
			curelm = nextelm;
		}
	}
	if (found) {
		accb->ccb_h.status = CAM_REQ_ABORTED;
		xpt_done(accb);
		return (CAM_REQ_CMP);
	}
	printf("%s: not found\n", __func__);
	return (CAM_PATH_INVALID);
}

/*
 * directly execute a read or write to the initiator
 * address space and set hand(sbp_targ_cam_done) to 
 * process the completion from the SIM to the target.
 * set orbi->refcount to inidicate that a read/write
 * is inflight to/from the initiator.
 */
static void
sbp_targ_xfer_buf(struct orb_info *orbi, u_int offset,
    uint16_t dst_hi, uint32_t dst_lo, u_int size,
    void (*hand)(struct fw_xfer *))
{
	struct fw_xfer *xfer;
	u_int len, ccb_dir, off = 0;
	char *ptr;

	if (debug > 1)
		printf("%s: offset=%d size=%d\n", __func__, offset, size);
	ccb_dir = orbi->ccb->ccb_h.flags & CAM_DIR_MASK;
	ptr = (char *)orbi->ccb->csio.data_ptr + offset;

	while (size > 0) {
		/* XXX assume dst_lo + off doesn't overflow */
		len = MIN(size, 2048 /* XXX */);
		size -= len;
		orbi->refcount++;
		if ((ccb_dir & CAM_DIR_OUT) == CAM_DIR_OUT) {
			if (debug)
				printf("%s: CAM_DIR_OUT --> read block in?\n",
				       __func__);
			xfer = fwmem_read_block(orbi->fwdev,
			   (void *)orbi, orbi->fwdev->speed,
			    dst_hi, dst_lo + off, len,
			    ptr + off, hand);
	       } else {
			if (debug)
				printf("%s: CAM_DIR_IN --> write block out?\n",
				       __func__);
			xfer = fwmem_write_block(orbi->fwdev,
			   (void *)orbi, orbi->fwdev->speed,
			    dst_hi, dst_lo + off, len,
			    ptr + off, hand);
		}
		if (xfer == NULL) {
			printf("%s: xfer == NULL, ccb_dir(%08x)\n", __func__, ccb_dir);
			/* XXX what should we do?? */
			orbi->refcount --;
		}
		off += len;
	}
}

static void
sbp_targ_pt_done(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	struct unrestricted_page_table_fmt *pt;
	uint32_t i;

	orbi = (struct orb_info *)xfer->sc;

	if (orbi->state == ORBI_STATUS_ABORTED) {
		if (debug)
			printf("%s: orbi aborted\n", __func__);
		sbp_targ_remove_orb_info(orbi->login, orbi);
		if (debug) {
			printf("%s: free orbi->page_table %p\n",
			       __func__, orbi->page_table);
			printf("%s: free orbi %p\n", __func__, orbi);
		}
		free(orbi->page_table, M_SBP_TARG);
		orbi->page_table = NULL;
		free(orbi, M_SBP_TARG);
		orbi = NULL;
		fw_xfer_free(xfer);
		return;
	}
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_PT | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);
		if (debug)
			printf("%s: free orbi->page_table %p\n",
			       __func__, orbi->page_table);
		free(orbi->page_table, M_SBP_TARG);
		orbi->page_table = NULL;
		fw_xfer_free(xfer);
		return;
	}
	orbi->refcount++;
	/*
	 * Set endianess here so we don't have 
	 * to deal with is later
	 */
	for (i = 0, pt = orbi->page_table; i < orbi->orb4.data_size; i++,pt++) {
		pt->segment_len = ntohs(pt->segment_len);
		if (debug)
			printf("%s:segment_len = %u\n",
			       __func__,pt->segment_len);
		pt->segment_base_high = ntohs(pt->segment_base_high);
		pt->segment_base_low = ntohl(pt->segment_base_low);
	}

	sbp_targ_xfer_pt(orbi);

	orbi->refcount--;
	if (orbi->refcount == 0)
		printf("%s: refcount == 0\n", __func__);
	fw_xfer_free(xfer);
	return;
}

static void sbp_targ_xfer_pt(struct orb_info *orbi)
{
	union ccb *ccb;
	uint32_t res, offset, len;

	ccb = orbi->ccb;
	if (debug)
		printf("%s: dxfer_len=%d\n", __func__, ccb->csio.dxfer_len);
	res = ccb->csio.dxfer_len;

	/*
	 * If the page table required multiple CTIO's to 
	 * complete, then cur_pte is non NULL 
	 * and we need to start from the last position
	 * If this is the first pass over a page table
         * then we just start at the beginning of the page
         * table.
	 *
	 * Parse the unrestricted page table and figure out where we need
	 * to shove the data from this read request.
	 */
	for (offset = 0, len = 0;
	    (res != 0) && (orbi->cur_pte < orbi->last_pte);
	     offset += len) {
		len = MIN(orbi->cur_pte->segment_len, res);
		res -= len;
		if (debug)
			printf("%s:page_table: %04x:%08x segment_len(%u)"
			       " res(%u) len(%u)\n", 
				__func__, orbi->cur_pte->segment_base_high,
				orbi->cur_pte->segment_base_low,
				orbi->cur_pte->segment_len,
				res, len);
		sbp_targ_xfer_buf(orbi, offset, 
				orbi->cur_pte->segment_base_high,
				orbi->cur_pte->segment_base_low,
				len, sbp_targ_cam_done);
               /*
                * If we have only written partially to
                * this page table, then we need to save
                * our position for the next CTIO.  If we
                * have completed the page table, then we
                * are safe to move on to the next entry.
                */
		if (len == orbi->cur_pte->segment_len) {
                       orbi->cur_pte++;
               } else {
                       uint32_t saved_base_low;

                       /* Handle transfers that cross a 4GB boundary. */
                       saved_base_low = orbi->cur_pte->segment_base_low;
                       orbi->cur_pte->segment_base_low += len;
                       if (orbi->cur_pte->segment_base_low < saved_base_low)
                           orbi->cur_pte->segment_base_high++;

                       orbi->cur_pte->segment_len -= len;
               }
	}
	if (res != 0)
           printf("Warning - short pt encountered.  "
                  "Could not transfer all data.\n");
	return;
}

/*
 * Create page table in local memory
 * and transfer it from the initiator
 * in order to know where we are supposed
 * to put the data.
 */
static void
sbp_targ_fetch_pt(struct orb_info *orbi)
{
	struct fw_xfer *xfer;

	/*
	 * Pull in page table from initiator
 	 * and setup for data from our
	 * backend device.
	 */
	if (orbi->page_table == NULL) {
		orbi->page_table = malloc(orbi->orb4.data_size*
				    sizeof(struct unrestricted_page_table_fmt),
				    M_SBP_TARG,
				    M_NOWAIT|M_ZERO);
		if (orbi->page_table == NULL)
			goto error;
		orbi->cur_pte = orbi->page_table;
		orbi->last_pte = orbi->page_table + orbi->orb4.data_size;
		orbi->last_block_read = orbi->orb4.data_size;
		if (debug && orbi->page_table != NULL) 
			printf("%s: malloc'd orbi->page_table(%p),"
			       " orb4.data_size(%u)\n",
			       __func__,
			       orbi->page_table,
			       orbi->orb4.data_size);

		xfer = fwmem_read_block(orbi->fwdev, (void *)orbi,
				orbi->fwdev->speed, orbi->data_hi,
				orbi->data_lo, orbi->orb4.data_size*
				sizeof(struct unrestricted_page_table_fmt),
			    	(void *)orbi->page_table, sbp_targ_pt_done);

		if (xfer != NULL)
			return;
	} else {
		/*
		 * This is a CTIO for a page table we have
		 * already malloc'd, so just directly invoke
		 * the xfer function on the orbi.
		 */
		sbp_targ_xfer_pt(orbi);

		return;
	}
error:
	orbi->ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
	if (debug)	
		printf("%s: free orbi->page_table %p due to xfer == NULL\n",
		       __func__, orbi->page_table);
	if (orbi->page_table != NULL) {
		free(orbi->page_table, M_SBP_TARG);
		orbi->page_table = NULL;
	}
	xpt_done(orbi->ccb);
	return;
}

static void
sbp_targ_action1(struct cam_sim *sim, union ccb *ccb)
{
	struct sbp_targ_softc *sc;
	struct sbp_targ_lstate *lstate;
	cam_status status;
	u_int ccb_dir;

	sc =  (struct sbp_targ_softc *)cam_sim_softc(sim);

	status = sbp_targ_find_devs(sc, ccb, &lstate, TRUE);

	switch (ccb->ccb_h.func_code) {
	case XPT_CONT_TARGET_IO:
	{
		struct orb_info *orbi;

		if (debug)
			printf("%s: XPT_CONT_TARGET_IO(0x%08x)\n",
			       __func__, ccb->csio.tag_id);

		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		/* XXX transfer from/to initiator */
		orbi = sbp_targ_get_orb_info(lstate,
		    ccb->csio.tag_id, ccb->csio.init_id);
		if (orbi == NULL) {
			ccb->ccb_h.status = CAM_REQ_ABORTED; /* XXX */
			xpt_done(ccb);
			break;
		}
		if (orbi->state == ORBI_STATUS_ABORTED) {
			if (debug)
				printf("%s: ctio aborted\n", __func__);
			sbp_targ_remove_orb_info(orbi->login, orbi);
			if (debug)
				printf("%s: free orbi %p\n", __func__, orbi);
			free(orbi, M_SBP_TARG);
			orbi = NULL;
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			xpt_done(ccb);
			break;
		}
		orbi->state = ORBI_STATUS_CTIO;

		orbi->ccb = ccb;
		ccb_dir = ccb->ccb_h.flags & CAM_DIR_MASK;

		/* XXX */
		if (ccb->csio.dxfer_len == 0)
			ccb_dir = CAM_DIR_NONE;

		/* Sanity check */
		if (ccb_dir == CAM_DIR_IN && orbi->orb4.dir == 0)
			printf("%s: direction mismatch\n", __func__);

		/* check page table */
		if (ccb_dir != CAM_DIR_NONE && orbi->orb4.page_table_present) {
			if (debug)
				printf("%s: page_table_present\n",
				    __func__);
			if (orbi->orb4.page_size != 0) {
				printf("%s: unsupported pagesize %d != 0\n",
			 	    __func__, orbi->orb4.page_size);
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}
			sbp_targ_fetch_pt(orbi);
			break;
		}

		if (ccb_dir != CAM_DIR_NONE) {
			sbp_targ_xfer_buf(orbi, 0, orbi->data_hi,
			    orbi->data_lo,
			    MIN(orbi->orb4.data_size, ccb->csio.dxfer_len),
			    sbp_targ_cam_done);
			if ( orbi->orb4.data_size > ccb->csio.dxfer_len ) {
                       		orbi->data_lo += ccb->csio.dxfer_len;
                       		orbi->orb4.data_size -= ccb->csio.dxfer_len;
               		}

		}

		if (ccb_dir == CAM_DIR_NONE) {
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
				if (debug) 
					printf("%s: CAM_SEND_STATUS set %0x\n",
					       __func__, ccb->ccb_h.flags);
				SBP_UNLOCK(sc);
				sbp_targ_send_status(orbi, ccb);
				SBP_LOCK(sc);
			} else {
				if (debug)
					printf("%s: CAM_SEND_STATUS !set %0x\n",
					       __func__, ccb->ccb_h.flags);
				ccb->ccb_h.status = CAM_REQ_CMP;
			}
			xpt_done(ccb);
		}
		break;
	}
	case XPT_ACCEPT_TARGET_IO:	/* Add Accept Target IO Resource */
		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		SLIST_INSERT_HEAD(&lstate->accept_tios, &ccb->ccb_h,
		    sim_links.sle);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		if ((lstate->flags & F_ATIO_STARVED) != 0) {
			struct sbp_targ_login *login;

			if (debug)
				printf("%s: new atio arrived\n", __func__);
			lstate->flags &= ~F_ATIO_STARVED;
			STAILQ_FOREACH(login, &lstate->logins, link)
				if ((login->flags & F_ATIO_STARVED) != 0) {
					login->flags &= ~F_ATIO_STARVED;
					sbp_targ_fetch_orb(lstate->sc,
					    login->fwdev,
					    login->last_hi, login->last_lo,
					    login, FETCH_CMD);
				}
		}
		break;
	case XPT_NOTIFY_ACK:		/* recycle notify ack */
	case XPT_IMMED_NOTIFY:		/* Add Immediate Notify Resource */
		if (status != CAM_REQ_CMP) {
			ccb->ccb_h.status = status;
			xpt_done(ccb);
			break;
		}
		SLIST_INSERT_HEAD(&lstate->immed_notifies, &ccb->ccb_h,
		    sim_links.sle);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		sbp_targ_send_lstate_events(sc, lstate);
		break;
	case XPT_EN_LUN:
		sbp_targ_en_lun(sc, ccb);
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_TAG_ABLE;
		cpi->target_sprt = PIT_PROCESSOR
				 | PIT_DISCONNECT
				 | PIT_TERM_IO;
		cpi->transport = XPORT_UNSPECIFIED;
		cpi->hba_misc = PIM_NOINITIATOR
			      | PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 7; /* XXX */
		cpi->max_lun = MAX_LUN - 1;
		cpi->initiator_id = 7; /* XXX */
		cpi->bus_id = sim->bus_id;
		cpi->base_transfer_speed = 400 * 1000 / 8;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "SBP_TARG", HBA_IDLEN);
		strncpy(cpi->dev_name, sim->sim_name, DEV_IDLEN);
		cpi->unit_number = sim->unit_number;

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:
	{
		union ccb *accb = ccb->cab.abort_ccb;

		switch (accb->ccb_h.func_code) {
		case XPT_ACCEPT_TARGET_IO:
		case XPT_IMMED_NOTIFY:
			ccb->ccb_h.status = sbp_targ_abort_ccb(sc, ccb);
			break;
		case XPT_CONT_TARGET_IO:
			/* XXX */
			ccb->ccb_h.status = CAM_UA_ABORT;
			break;
		default:
			printf("%s: aborting unknown function %d\n", 
				__func__, accb->ccb_h.func_code);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_GET_TRAN_SETTINGS:
	{
                struct ccb_trans_settings *cts = &ccb->cts;
                struct ccb_trans_settings_scsi *scsi =
                    &cts->proto_specific.scsi;
                struct ccb_trans_settings_spi *spi =
                    &cts->xport_specific.spi;

                cts->protocol = PROTO_SCSI;
                cts->protocol_version = SCSI_REV_2;
                cts->transport = XPORT_UNSPECIFIED;     /* should have a FireWire */
                cts->transport_version = 2;
                spi->valid = CTS_SPI_VALID_DISC;
                spi->flags = CTS_SPI_FLAGS_DISC_ENB;
                scsi->valid = CTS_SCSI_VALID_TQ;
                scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
#if 0
                printf("%s:%d:%d XPT_GET_TRAN_SETTINGS:\n",
                        device_get_nameunit(sc->fd.dev),
                        ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
#endif
                cts->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
		break;
	}
	default:
		printf("%s: unknown function 0x%x\n",
		    __func__, ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_PROVIDE_FAIL;
		xpt_done(ccb);
		break;
	}
	return;
}

static void
sbp_targ_action(struct cam_sim *sim, union ccb *ccb)
{
	int s;

	s = splfw();
	sbp_targ_action1(sim, ccb);
	splx(s);
}

static void
sbp_targ_poll(struct cam_sim *sim)
{
	/* XXX */
	return;
}

static void
sbp_targ_cmd_handler(struct fw_xfer *xfer)
{
	struct fw_pkt *fp;
	uint32_t *orb;
	struct corb4 *orb4;
	struct orb_info *orbi;
	struct ccb_accept_tio *atio;
	u_char *bytes;
	int i;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_ORB | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/1);
		fw_xfer_free(xfer);
		return;
	}
	fp = &xfer->recv.hdr;

	atio = orbi->atio;

	if (orbi->state == ORBI_STATUS_ABORTED) {
		printf("%s: aborted\n", __func__);
		sbp_targ_remove_orb_info(orbi->login, orbi);
		if (debug)
			printf("%s: free orbi %p\n", __func__, orbi);
		free(orbi, M_SBP_TARG);
		orbi = NULL;
		atio->ccb_h.status = CAM_REQ_ABORTED;
		SBP_LOCK(orbi->sc);
		xpt_done((union ccb*)atio);
		SBP_UNLOCK(orbi->sc);
		goto done0;
	}
	orbi->state = ORBI_STATUS_ATIO;

	orb = orbi->orb;
	/* swap payload except SCSI command */
	for (i = 0; i < 5; i ++)
		orb[i] = ntohl(orb[i]);

	orb4 = (struct corb4 *)&orb[4];
	if (orb4->rq_fmt != 0) {
		/* XXX */
		printf("%s: rq_fmt(%d) != 0\n", __func__, orb4->rq_fmt);
	}

	atio->ccb_h.target_id = 0; /* XXX */
	atio->ccb_h.target_lun = orbi->login->lstate->lun;
	atio->sense_len = 0;
	atio->tag_action = MSG_SIMPLE_TASK;
	atio->tag_id = orbi->orb_lo;
	atio->init_id = orbi->login->id;

	atio->ccb_h.flags = CAM_TAG_ACTION_VALID;
	bytes = (u_char *)&orb[5];
	if (debug)
		printf("%s: %p %02x %02x %02x %02x %02x"
		       " %02x %02x %02x %02x %02x\n",
		       __func__, (void *)atio,
		       bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
		       bytes[5], bytes[6], bytes[7], bytes[8], bytes[9]);
	switch (bytes[0] >> 5) {
	case 0:
		atio->cdb_len = 6;
		break;
	case 1:
	case 2:
		atio->cdb_len = 10;
		break;
	case 4:
		atio->cdb_len = 16;
		break;
	case 5:
		atio->cdb_len = 12;
		break;
	case 3:
	default:
		/* Only copy the opcode. */
		atio->cdb_len = 1;
		printf("Reserved or VU command code type encountered\n");
		break;
	}

	memcpy(atio->cdb_io.cdb_bytes, bytes, atio->cdb_len);

	atio->ccb_h.status |= CAM_CDB_RECVD;

	/* next ORB */
	if ((orb[0] & (1<<31)) == 0) {
		if (debug)
			printf("%s: fetch next orb\n", __func__);
		orbi->status.src = SRC_NEXT_EXISTS;
		sbp_targ_fetch_orb(orbi->sc, orbi->fwdev,
		    orb[0], orb[1], orbi->login, FETCH_CMD);
	} else {
		orbi->status.src = SRC_NO_NEXT;
		orbi->login->flags &= ~F_LINK_ACTIVE;
	}

	orbi->data_hi = orb[2];
	orbi->data_lo = orb[3];
	orbi->orb4 = *orb4;

	SBP_LOCK(orbi->sc);
	xpt_done((union ccb*)atio);
	SBP_UNLOCK(orbi->sc);
done0:
	fw_xfer_free(xfer);
	return;
}

static struct sbp_targ_login *
sbp_targ_get_login(struct sbp_targ_softc *sc, struct fw_device *fwdev, int lun)
{
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	int i;

	lstate = sc->lstate[lun];
	
	STAILQ_FOREACH(login, &lstate->logins, link)
		if (login->fwdev == fwdev) {
			/*
			 * We are receiving a login from
			 * an initiator that left and didn't
			 * logout properly.  We need to kill
			 * the callout before returning the 
			 * login struct to the caller.
			 * For 7.X and higher, this is not safe
			 * and will need to be protected by 
			 * a lock.
			 * For 6.X we are safe as we hold Giant
			 * and the callout must aquire Giant.
			 */
			if (!(callout_stop(&login->hold_callout)) && (debug))
				printf("%s:callout in progress for login(%p)\n",
				       __func__, login);
			login->flags |= F_RECYCLE_LOGIN;
			return (login);
		}

	for (i = 0; i < MAX_LOGINS; i ++)
		if (sc->logins[i] == NULL)
			goto found;

	printf("%s: increase MAX_LOGIN\n", __func__);
	return (NULL);

found:
	login = (struct sbp_targ_login *)malloc(
	    sizeof(struct sbp_targ_login), M_SBP_TARG, M_NOWAIT | M_ZERO);

	if (login == NULL) {
		printf("%s: malloc failed\n", __func__);
		return (NULL);
	} else if (debug) {
		printf("%s: malloc'd login %p\n", __func__, login);
	}

	login->id = i;
	login->fwdev = fwdev;
	login->lstate = lstate;
	login->last_hi = 0xffff;
	login->last_lo = 0xffffffff;
	login->hold_sec = SBP_TARG_HOLD_TIMEOUT;
	login->flags |= F_LOGIN ;
	login->flags &= ~F_RECYCLE_LOGIN ;
	STAILQ_INIT(&login->orbs);
	CALLOUT_INIT(&login->hold_callout);
	sc->logins[i] = login;
	return (login);
}

static void
sbp_targ_mgm_handler(struct fw_xfer *xfer)
{
	struct sbp_targ_lstate *lstate;
	struct sbp_targ_login *login;
	struct fw_pkt *fp;
	uint32_t *orb;
	struct morb4 *orb4;
	struct orb_info *orbi, *aborted_orb = NULL;
	int i;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		orbi->status.resp = SBP_TRANS_FAIL;
		orbi->status.status = OBJ_ORB | SBE_TIMEOUT/*XXX*/;
		orbi->status.dead = 1;
		orbi->status.len = 1;
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));

		sbp_targ_status_FIFO(orbi,
		    orbi->login->fifo_hi, orbi->login->fifo_lo, /*dequeue*/0);
		fw_xfer_free(xfer);
		return;
	}
	fp = &xfer->recv.hdr;

	orb = orbi->orb;
	/* swap payload */
	for (i = 0; i < 8; i ++) {
		orb[i] = ntohl(orb[i]);
	}
	orb4 = (struct morb4 *)&orb[0];
	if (debug)
		printf("%s: %s\n", __func__, orb_fun_name[orb4->fun]);

	orbi->status.src = SRC_NO_NEXT;

	switch (orb4->fun << 16) {
	case ORB_FUN_LGI:
	{
		int exclusive = 0, lun;

		if (orb[4] & ORB_EXV)
			exclusive = 1;

		lun = orb4->id;
		lstate = orbi->sc->lstate[lun];

		if (debug)
			printf("%s:  NEW LOGIN, exclusive %d\n", __func__, exclusive);

 		if (lun >= MAX_LUN) {
			/* error */
			if (debug) {
				printf("%s: lun(%d) >= MAX_LUN(%d)\n",
					__func__, lun, MAX_LUN);
			}
			orbi->status.dead = 1;
			orbi->status.status = STATUS_LUR;
			orbi->status.len = 1;
			break;
		}
		if ( lstate != NULL && STAILQ_FIRST(&lstate->logins) != NULL) {
			if (exclusive
			   && !(FW_EUI64_EQUAL(STAILQ_FIRST(&lstate->logins)->fwdev->eui, orbi->fwdev->eui))) {
			       /*
				* If we ask for an exclusive login
				* and someone else is logged in
				* reject the login attempt
				*/
				if (debug) {
					printf("%s: rejecting exclusive login for dev %x:%x\n",
						__func__, orbi->fwdev->eui.hi, orbi->fwdev->eui.lo);
					printf("%s: old dev %x:%x\n", __func__,
						STAILQ_FIRST(&lstate->logins)->fwdev->eui.hi,
						STAILQ_FIRST(&lstate->logins)->fwdev->eui.lo);
				}
				orbi->status.dead = 1;
				orbi->status.status = STATUS_ACCESS_DENY;
				orbi->status.len = 1;
				break;
			} else if (STAILQ_FIRST(&lstate->logins)->flags & F_EXCLUSIVE) {
			       /* 
				* If the logged in initiator already
				* has the exclusive lock,
				* reject the login attempt
				*/
				if (debug)
					printf("%s: rejecting due to exclusive"
					       " login in progress\n", __func__);
				orbi->status.dead = 1;
				orbi->status.status = STATUS_ACCESS_DENY;
				orbi->status.len = 1;
				break;
			}
		}

		/* allocate login */
		login = sbp_targ_get_login(orbi->sc, orbi->fwdev, lun);
		if (login == NULL) {
			if(debug) {
				printf("%s: sbp_targ_get_login failed\n",
			    		__func__);
			}
			orbi->status.dead = 1;
			orbi->status.status = STATUS_RES_UNAVAIL;
			orbi->status.len = 1;
			break;
		}

		if (debug) {
			printf("%s:  New login(%p), login id=%d\n",
		       		__func__, login, login->id);
		}
		login->fifo_hi = orb[6];
		login->fifo_lo = orb[7];
		login->loginres.len = htons(sizeof(uint32_t) * 4);
		login->loginres.id = htons(login->id);
		login->loginres.cmd_hi = htons(SBP_TARG_BIND_HI);
		login->loginres.cmd_lo = htonl(SBP_TARG_BIND_LO(login->id));
		login->loginres.recon_hold = htons(login->hold_sec);
		/*
		 * If an exclusive login is requested
		 * set F_EXCLUSIVE
		 */
		if (exclusive)
			login->flags &= F_EXCLUSIVE;

		fwmem_write_block(orbi->fwdev, NULL, orbi->fwdev->speed, orb[2],
		    orb[3], sizeof(struct sbp_login_res), 
		    (void *)&login->loginres, fw_asy_callback_free);
		/* 
		 * If this is a new initiator and a login
		 * was recycled, then it is already in the
		 * login STAILQ, don't re-insert it.
		 */
		if (login->flags & F_RECYCLE_LOGIN) {
			if (debug)
				printf("%s: login(%p) already in STAILQ,"
				       " flags(%0x) no reinsertion needed\n",
				       __func__, login, login->flags);
			/*
			 * Clear RECYCLE flag
			 */
			login->flags &= ~F_RECYCLE_LOGIN;
		} else  {
			if (debug)
				printf("%s: login(%p) not in STAILQ,"
				       " inserting\n", __func__, login);
			STAILQ_INSERT_TAIL(&lstate->logins, login, link);
		}

		/* XXX return status after loginres is successfully written */
		break;
	}
	case ORB_FUN_RCN:
		login = orbi->sc->logins[orb4->id];
		if (login != NULL && login->fwdev == orbi->fwdev) {
			login->flags &= ~F_HOLD;
			callout_stop(&login->hold_callout);
			login->flags |= F_LOGIN;
			printf("%s: reconnected id=%d\n",
			    __func__, login->id);
		} else {
			orbi->status.dead = 1;
			orbi->status.status = STATUS_ACCESS_DENY;
			printf("%s: reconnection faild id=%d\n",
			    __func__, orb4->id);
		}
		break;
	case ORB_FUN_LGO:
		login = orbi->sc->logins[orb4->id];
		if (login->fwdev != orbi->fwdev) {
			printf("%s: wrong initiator\n", __func__);
			break;
		}
		sbp_targ_dealloc_login(login);
		break;
	case ORB_FUN_ATA: /* TODO */
		/*
		 * Find the orb to be aborted.
		 * If we haven't fetched, just drop it on the floor.
		 * If we have already fetched,
		 * process normally and don't worry about it.
		 */
		printf("%s: Abort Task recieved\n", __func__);
		login = orbi->sc->logins[orb4->id];
		sbp_targ_abort(orbi->sc, STAILQ_NEXT(orbi, link));
		sbp_targ_status_FIFO(orbi,
				     orbi->login->fifo_hi,
				     orbi->login->fifo_lo,
 				     /*dequeue*/0);
		break;
	case ORB_FUN_ATS: /* TODO */
		printf("%s: Abort Task Set recieved for orb %0x\n",__func__, orb4->id );
		login = orbi->sc->logins[orb4->id];
		if (login != NULL) {
			STAILQ_FOREACH(aborted_orb, &login->orbs, link) {
				if (aborted_orb != NULL) {
					printf("%s: aborting orb\n", __func__);
					sbp_targ_abort(orbi->sc, aborted_orb);
				}
			}
			printf("%s: all orb aborted\n", __func__);
		} else {
			printf("%s: not logged in, no need to abort\n", __func__);
		}
		break;
	case ORB_FUN_LUR: /* TODO */
		printf("%s: Logical Unit Reset recieved for orb\n",__func__);
		orbi->status.resp = SBP_REQ_CMP;
		orbi->status.dead = 1;
		break;
	case ORB_FUN_RST: /* TODO */
		printf("%s: Target Reset recieved for orb\n", __func__);
		sbp_targ_send_agent_state(xfer, AGENT_STATE_DEAD);
		break;
	default:
		printf("%s: %s not implemented yet\n",
		    __func__, orb_fun_name[orb4->fun]);
		break;
	}
	orbi->status.len = 1;
	sbp_targ_status_FIFO(orbi, orb[6], orb[7], /*dequeue*/0);
	fw_xfer_free(xfer);
	return;
}

static void
sbp_targ_pointer_handler(struct fw_xfer *xfer)
{
	struct orb_info *orbi;
	uint32_t orb0, orb1;

	orbi = (struct orb_info *)xfer->sc;
	if (xfer->resp != 0) {
		printf("%s: xfer->resp = %d\n", __func__, xfer->resp);
		goto done;
	}

	orb0 = ntohl(orbi->orb[0]);
	orb1 = ntohl(orbi->orb[1]);
	if ((orb0 & (1 << 31)) != 0) {
		printf("%s: invalid pointer\n", __func__);
		goto done;
	}
	sbp_targ_fetch_orb(orbi->login->lstate->sc, orbi->fwdev,
	    (uint16_t)orb0, orb1, orbi->login, FETCH_CMD);
done:
	if (debug)
		printf("%s: free orbi %p\n", __func__, orbi);
	free(orbi, M_SBP_TARG);
	orbi = NULL;
	fw_xfer_free(xfer);
	return;
}

static void
sbp_targ_fetch_orb(struct sbp_targ_softc *sc, struct fw_device *fwdev,
    uint16_t orb_hi, uint32_t orb_lo, struct sbp_targ_login *login,
    int mode)
{
	struct orb_info *orbi;

	if (debug)
		printf("%s: fetch orb mode %d, %04x:%08x\n",
		       __func__, mode, orb_hi, orb_lo);
	orbi = malloc(sizeof(struct orb_info), M_SBP_TARG, M_NOWAIT | M_ZERO);
	if (orbi == NULL) {
		printf("%s: malloc failed\n", __func__);
		return;
	} else if (debug) {
		printf("%s: malloc'd orbi %p\n", __func__, orbi);
	}
	orbi->sc = sc;
	orbi->fwdev = fwdev;
	orbi->login = login;
	orbi->orb_hi = orb_hi;
	orbi->orb_lo = orb_lo;
	orbi->status.orb_hi = htons(orb_hi);
	orbi->status.orb_lo = htonl(orb_lo);
	orbi->page_table = NULL;

	switch (mode) {
	case FETCH_MGM:
		if (debug)
			printf("%s: FETCH_MGM for orbi %p\n", __func__, orbi);
		fwmem_read_block(fwdev, (void *)orbi,
				fwdev->speed, orb_hi, orb_lo,
		    		sizeof(uint32_t) * 8, &orbi->orb[0],
		    sbp_targ_mgm_handler);
		break;
	case FETCH_CMD:
		if (debug)
			printf("%s: FETCH_CMD for orbi %p\n", __func__, orbi);
		orbi->state = ORBI_STATUS_FETCH;
		login->last_hi = orb_hi;
		login->last_lo = orb_lo;
		login->flags |= F_LINK_ACTIVE;
		/* dequeue */
		SBP_LOCK(orbi->sc);
		orbi->atio = (struct ccb_accept_tio *)
		    SLIST_FIRST(&login->lstate->accept_tios);
		if (orbi->atio == NULL) {
			SBP_UNLOCK(orbi->sc);
			printf("%s: no free atio\n", __func__);
			login->lstate->flags |= F_ATIO_STARVED;
			login->flags |= F_ATIO_STARVED;
#if 0
			/* XXX ?? */
			login->fwdev = fwdev;
#endif
			break;
		}
		SLIST_REMOVE_HEAD(&login->lstate->accept_tios, sim_links.sle);
		STAILQ_INSERT_TAIL(&login->orbs, orbi, link);
		SBP_UNLOCK(sc);
		fwmem_read_block(fwdev, (void *)orbi,
				fwdev->speed, orb_hi, orb_lo,
		    		sizeof(uint32_t) * 8, &orbi->orb[0],
		    		sbp_targ_cmd_handler);
		break;
	case FETCH_POINTER:
		if (debug)
			printf("%s: FETCH_POINTER for orbi %p\n",
			       __func__, orbi);
		orbi->state = ORBI_STATUS_POINTER;
		login->flags |= F_LINK_ACTIVE;
		fwmem_read_block(fwdev, (void *)orbi,
				fwdev->speed, orb_hi, orb_lo,
		    		sizeof(uint32_t) * 2, &orbi->orb[0],
		    		sbp_targ_pointer_handler);
		break;
	default:
		printf("%s: invalid mode %d\n", __func__, mode);
	}
}

static void
sbp_targ_resp_callback(struct fw_xfer *xfer)
{
	struct sbp_targ_softc *sc;
	int s;

	if (debug)
		printf("%s: xfer=%p\n", __func__, xfer);
	sc = (struct sbp_targ_softc *)xfer->sc;
	fw_xfer_unload(xfer);
	xfer->recv.pay_len = SBP_TARG_RECV_LEN;
	xfer->hand = sbp_targ_recv;
	s = splfw();
	STAILQ_INSERT_TAIL(&sc->fwb.xferlist, xfer, link);
	splx(s);
}

static int
sbp_targ_cmd(struct fw_xfer *xfer, struct fw_device *fwdev, int login_id,
    int reg)
{
	struct sbp_targ_login *login;
	struct sbp_targ_softc *sc;
#if 0
	struct orb_info *unsolicited_orbi;
#endif
	int rtcode = 0;

	if (login_id < 0 || login_id >= MAX_LOGINS)
		return(RESP_ADDRESS_ERROR);

	sc = (struct sbp_targ_softc *)xfer->sc;
	login = sc->logins[login_id];
	if (login == NULL)
		return(RESP_ADDRESS_ERROR);

	if (login->fwdev != fwdev) {
		/* XXX */
		return(RESP_ADDRESS_ERROR);
	}

	switch (reg) {
	case 0x08:	/* ORB_POINTER */
		if (debug)
			printf("%s: ORB_POINTER (%d)\n", __func__, login_id);
		if ((login->flags & F_LINK_ACTIVE) != 0) {
			if (debug)
				printf("link active (ORB_POINTER)\n");
			break;
		}
		sbp_targ_fetch_orb(sc, fwdev,
		    ntohl(xfer->recv.payload[0]),
		    ntohl(xfer->recv.payload[1]),
		    login, FETCH_CMD);
		break;
	case 0x04:	/* AGENT_RESET */
		if (debug)
			printf("%s: AGENT RESET (%d)\n", __func__, login_id);
		login->last_hi = 0xffff;
		login->last_lo = 0xffffffff;
		sbp_targ_abort(sc, STAILQ_FIRST(&login->orbs));
		break;
	case 0x10:	/* DOORBELL */
		if (debug)
			printf("%s: DOORBELL (%d)\n", __func__, login_id);
		if (login->last_hi == 0xffff &&
		    login->last_lo == 0xffffffff) {
			printf("%s: no previous pointer(DOORBELL)\n",
			    __func__);
			break;
		}
		if ((login->flags & F_LINK_ACTIVE) != 0) {
			if (debug)
				printf("link active (DOORBELL)\n");
			break;
		}
		sbp_targ_fetch_orb(sc, fwdev,
		    login->last_hi, login->last_lo,
		    login, FETCH_POINTER);
		break;
	case 0x00:	/* AGENT_STATE */
		printf("%s: AGENT_STATE (%d)\n", __func__, login_id);
		sbp_targ_send_agent_state(xfer, AGENT_STATE_ACTIVE);
		break;
	case 0x14:	/* UNSOLICITED_STATE_ENABLE */
		if (debug)
			printf("%s: UNSOLICITED_STATE_ENABLE (%d)\n",
				__func__, login_id);
#if 0
		unsolicited_orbi = malloc(sizeof(struct orb_info),
				          M_SBP_TARG,
				  	  M_NOWAIT | M_ZERO);
		unsolicited_orbi->fwdev = login->fwdev;
		unsolicited_orbi->status.src = SRC_UNSOL;
                unsolicited_orbi->status.resp = SBP_REQ_CMP;
                unsolicited_orbi->status.dead = 0;
                unsolicited_orbi->status.len = 1;
                unsolicited_orbi->status.status = 0;
		sbp_targ_status_FIFO(unsolicited_orbi,
				     login->fifo_hi,
				     login->fifo_lo,
				     /*No dequeue*/0);
#endif
		break;
	default:
		printf("%s: invalid register %d(%d)\n",
		       __func__, reg, login_id);
		rtcode = RESP_ADDRESS_ERROR;
	}

	return (rtcode);
}

static void
sbp_targ_send_agent_state(struct fw_xfer *xfer, int state)
{
	struct agent_state *current_state;	
	struct fw_pkt *rfp; /* response to request --> from target */
	
	xfer->send.payload = malloc(sizeof(struct agent_state),
				    M_SBP_TARG,
				    M_NOWAIT | M_ZERO);
	xfer->send.pay_len = ntohs(sizeof(struct agent_state));
	xfer->send.spd = FWSPD_S400;

	current_state = (struct agent_state *)xfer->send.payload;
	current_state->fetch_agent_state = state;

        rfp = &xfer->recv.hdr;
        rfp->mode.rresb.tcode = FWTCODE_RRESB;
        rfp->mode.rresb.rtcode = 0;
	rfp->mode.rresb.extcode = 0;
        xfer->send.hdr.mode.hdr.dst = ntohs(rfp->mode.hdr.src);
        xfer->hand = fw_xfer_free_buf;
        rfp->mode.hdr.pri = 0;
        fw_asyreq(xfer->fc, -1, xfer);

}

static int
sbp_targ_mgm(struct fw_xfer *xfer, struct fw_device *fwdev)
{
	struct sbp_targ_softc *sc;
	struct fw_pkt *fp;

	sc = (struct sbp_targ_softc *)xfer->sc;

	fp = &xfer->recv.hdr;
	if (fp->mode.wreqb.tcode != FWTCODE_WREQB){
		printf("%s: tcode = %d\n", __func__, fp->mode.wreqb.tcode);
		return(RESP_TYPE_ERROR);
        }

	sbp_targ_fetch_orb(sc, fwdev,
	    ntohl(xfer->recv.payload[0]),
	    ntohl(xfer->recv.payload[1]),
	    NULL, FETCH_MGM);
	
	return(0);
}
#if 0
/*
 * handler for the busy timeout CSR
 */
static void
sbp_targ_busy_timeout_handler(struct fw_xfer *xfer)
{
        struct fw_pkt *fp, *sfp;
        struct fw_device *fwdev;
        uint32_t lo = 0;
        int s, rtcode;
        struct firewire_softc *sc;

        printf("%s:  access to BUSY_TIMEOUT\n", __func__);
        mtx_assert(&Giant, MA_OWNED);
        s = splfw();
        sc = (struct firewire_softc *)xfer->sc;
        fp = &xfer->recv.hdr;
        fwdev = fw_noderesolve_nodeid(sc->fc, fp->mode.wreqb.src & 0x3f);
        if (fwdev == NULL) {
                printf("%s: cannot resolve nodeid=%d\n",
                    __func__, fp->mode.wreqb.src & 0x3f);
                rtcode = RESP_TYPE_ERROR; /* XXX */
        } else {
                rtcode = 0;
        }
        if (rtcode != 0)
                printf("%s: rtcode = %d lo == 0x%x\n", __func__, rtcode, lo);

        sfp = &xfer->send.hdr;
        xfer->send.spd = fwdev->speed;
        sfp->mode.wres.dst = fp->mode.wreqb.src;
        sfp->mode.wres.tlrt = fp->mode.wreqb.tlrt;
        sfp->mode.wres.tcode = FWTCODE_WRES;
        sfp->mode.wres.rtcode = rtcode;
        sfp->mode.wres.pri = 0;

        fw_asyreq(xfer->fc, -1, xfer);
        splx(s);
}

/*
 * handler for the reset start CSR
 */
static void
sbp_targ_reset_start_handler(struct fw_xfer *xfer)
{
        struct fw_pkt *fp, *sfp;
        struct fw_device *fwdev;
        uint32_t lo = 0;
        int s, rtcode;
        struct firewire_softc *sc;

        s = splfw();
	printf("%s: access to RESET_START\n",__func__);
        sc = (struct firewire_softc *)xfer->sc;
        fp = &xfer->recv.hdr;
        fwdev = fw_noderesolve_nodeid(sc->fc, fp->mode.wreqb.src & 0x3f);
        if (fwdev == NULL) {
                printf("%s: cannot resolve nodeid=%d\n",
                    __func__, fp->mode.wreqb.src & 0x3f);
                rtcode = RESP_TYPE_ERROR; /* XXX */
        } else {
                rtcode = 0;
        }
        if (rtcode != 0)
                printf("%s: rtcode = %d lo == 0x%x\n", __func__, rtcode, lo);

        sfp = &xfer->send.hdr;
        xfer->send.spd = fwdev->speed;
        sfp->mode.wres.dst = fp->mode.wreqb.src;
        sfp->mode.wres.tlrt = fp->mode.wreqb.tlrt;
        sfp->mode.wres.tcode = FWTCODE_WRES;
        sfp->mode.wres.rtcode = rtcode;
        sfp->mode.wres.pri = 0;

        fw_asyreq(xfer->fc, -1, xfer);
        splx(s);
}
#endif
static void
sbp_targ_recv(struct fw_xfer *xfer)
{
	struct fw_pkt *fp, *sfp;
	struct fw_device *fwdev;
	uint32_t lo = 0;
	int s, rtcode;
	struct sbp_targ_softc *sc;
	struct timeval tv;

	s = splfw();
	sc = (struct sbp_targ_softc *)xfer->sc;
	fp = &xfer->recv.hdr;
	fwdev = fw_noderesolve_nodeid(sc->fd.fc, fp->mode.wreqb.src & 0x3f);
	if (fwdev == NULL) {
		getmicrotime(&tv);
		printf("(%ld)(%ld) %s: cannot resolve nodeid=%d\n",
		    (long)tv.tv_sec, tv.tv_usec,__func__, fp->mode.wreqb.src & 0x3f);
		rtcode = RESP_TYPE_ERROR; /* XXX */
	} else {
		lo = fp->mode.wreqb.dest_lo;
		if (lo == SBP_TARG_BIND_LO(-1))
			rtcode = sbp_targ_mgm(xfer, fwdev);
		else if (lo >= SBP_TARG_BIND_LO(0))
			rtcode = sbp_targ_cmd(xfer, fwdev,
					      SBP_TARG_LOGIN_ID(lo),
		    			      lo % 0x20);
		else
			rtcode = RESP_ADDRESS_ERROR;
	}

	if (rtcode != 0)
		printf("%s: rtcode = %d lo == 0x%x\n", __func__, rtcode, lo);
	sfp = &xfer->send.hdr;
	xfer->send.spd = fwdev->speed; 
	xfer->hand = sbp_targ_resp_callback;
	sfp->mode.wres.dst = fp->mode.wreqb.src;
	sfp->mode.wres.tlrt = fp->mode.wreqb.tlrt;
	sfp->mode.wres.tcode = FWTCODE_WRES;
	sfp->mode.wres.rtcode = rtcode;
	sfp->mode.wres.pri = 0;

	fw_asyreq(xfer->fc, -1, xfer);
	splx(s);
}

static int
sbp_targ_attach(device_t dev)
{
	struct sbp_targ_softc *sc;
	struct cam_devq *devq;
	int i;

        sc = (struct sbp_targ_softc *) device_get_softc(dev);
	bzero((void *)sc, sizeof(struct sbp_targ_softc));

	mtx_init(&sc->mtx, "sbp_targ", NULL, MTX_DEF);
	sc->fd.fc = device_get_ivars(dev);
	sc->fd.dev = dev;
	sc->fd.post_explore = (void *) sbp_targ_post_explore;
	sc->fd.post_busreset = (void *) sbp_targ_post_busreset;

	sc->fwb.start = SBP_TARG_BIND_START;
	sc->fwb.end = SBP_TARG_BIND_END;

#if 0
	/*
	 * setup CSR for RESET_START from 
	 * initiator(0x000c)
	 */
	fwcsr_reset_start_init(&sc->reset_start, &sc->fd, sc,
				sbp_targ_reset_start_handler,
				M_SBP_TARG, 0,
				SBP_TARG_RECV_LEN,
				MAX_LUN);

	/*
	 * setup CSR for BUSY_TIMEOUT from 
	 * initiator(0x0210)
	 */
	fwcsr_busy_timeout_init(&sc->busy_timeout, &sc->fd, sc, 
			     sbp_targ_busy_timeout_handler,
			     M_SBP_TARG, 0, 
			     SBP_TARG_RECV_LEN,
			     MAX_LUN);
#endif
	/* pre-allocate xfer */
	STAILQ_INIT(&sc->fwb.xferlist);
	fw_xferlist_add(&sc->fwb.xferlist, M_SBP_TARG,
	    /*send*/ 0, /*recv*/ SBP_TARG_RECV_LEN, MAX_LUN /* XXX */,
	    sc->fd.fc, (void *)sc, sbp_targ_recv);
	fw_bindadd(sc->fd.fc, &sc->fwb);

        devq = cam_simq_alloc(/*maxopenings*/MAX_LUN*MAX_INITIATORS);
	if (devq == NULL)
		return (ENXIO);

	sc->sim = cam_sim_alloc(sbp_targ_action, sbp_targ_poll,
	    "sbp_targ", sc, device_get_unit(dev), &sc->mtx,
	    /*untagged*/ 1, /*tagged*/ 1, devq);

	if (sc->sim == NULL) {
		cam_simq_free(devq);
		return (ENXIO);
	}

	SBP_LOCK(sc);
	if (xpt_bus_register(sc->sim, dev, /*bus*/0) != CAM_SUCCESS)
		goto fail;

	if (xpt_create_path(&sc->path, /*periph*/ NULL, cam_sim_path(sc->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sim));
		goto fail;
	}
	for (i = 0; i < MAX_LUN; i ++) {
		sc->lstate[i] = NULL;
	}

	sc->black_hole = NULL;
	SBP_UNLOCK(sc);

	printf("%s: finished\n",__func__);
	return 0;

fail:
	cam_sim_free(sc->sim, /*free_devq*/TRUE);
	SBP_UNLOCK(sc);
	printf("%s: failed\n",__func__);
	return (ENXIO);
}

static int
sbp_targ_detach(device_t dev)
{
	struct sbp_targ_softc *sc;

	sc = (struct sbp_targ_softc *)device_get_softc(dev);

	fw_bindremove(sc->fd.fc, &sc->fwb);
	fw_xferlist_remove(&sc->fwb.xferlist);

	SBP_LOCK(sc);
	sc->fd.post_busreset = NULL;
	sc->fd.post_explore = NULL;

	xpt_free_path(sc->path);
	xpt_bus_deregister(cam_sim_path(sc->sim));

#if 0
	fwcsr_busy_timeout_stop(&sc->busy_timeout, &sc->fd);
	fwcsr_reset_start_stop(&sc->reset_start, &sc->fd);
#endif
	cam_sim_free(sc->sim, /*free_devq*/TRUE); 
	SBP_UNLOCK(sc);
	mtx_destroy(&sc->mtx);
	return 0;
}

static devclass_t sbp_targ_devclass;

static device_method_t sbp_targ_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	sbp_targ_identify),
	DEVMETHOD(device_probe,		sbp_targ_probe),
	DEVMETHOD(device_attach,	sbp_targ_attach),
	DEVMETHOD(device_detach,	sbp_targ_detach),
	{ 0, 0 }
};

static driver_t sbp_targ_driver = {
	"sbp_targ",
	sbp_targ_methods,
	sizeof(struct sbp_targ_softc),
};

DRIVER_MODULE(sbp_targ, firewire, sbp_targ_driver, sbp_targ_devclass, 0, 0);
MODULE_VERSION(sbp_targ, 1);
MODULE_DEPEND(sbp_targ, firewire, 1, 1, 1);
MODULE_DEPEND(sbp_targ, cam, 1, 1, 1);
