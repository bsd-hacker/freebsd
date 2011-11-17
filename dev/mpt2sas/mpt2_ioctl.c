/*-
 * Copyright (c) 2011 by Alacritech Inc.
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/mpt2sas_ioctl.h>

#include <dev/mpt/mpt.h>

static d_open_t		mpt2sas_open;
static d_close_t	mpt2sas_close;
static d_ioctl_t	mpt2sas_ioctl;
static struct cdevsw mpt2sas_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	mpt2sas_open,
	.d_close =	mpt2sas_close,
	.d_ioctl =	mpt2sas_ioctl,
	.d_name =	"mpt2sas",
};

static int
mpt2sas_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
mpt2sas_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}


#ifdef __amd64__
#define	PTRIN(p)		((void *)(uintptr_t)(p))
#define PTROUT(v)		((u_int32_t)(uintptr_t)(v))
#endif

static int
mpt2sas_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag, struct thread *td)
{
	struct mpt2sas_softc *mpt;
	struct mpt2sas_cfg_page_req *page_req;
	struct mpt2sas_ext_cfg_page_req *ext_page_req;
	struct mpt2sas_raid_action *raid_act;
	struct mpt2sas_page_memory mpt2sas_page;
#ifdef __amd64__
	struct mpt2sas_cfg_page_req32 *page_req32;
	struct mpt2sas_cfg_page_req page_req_swab;
	struct mpt2sas_ext_cfg_page_req32 *ext_page_req32;
	struct mpt2sas_ext_cfg_page_req ext_page_req_swab;
	struct mpt2sas_raid_action32 *raid_act32;
	struct mpt2sas_raid_action raid_act_swab;
#endif
	int error;

	mpt = dev->si_drv1;
	page_req = (void *)arg;
	ext_page_req = (void *)arg;
	raid_act = (void *)arg;
	mpt2sas_page.vaddr = NULL;

#ifdef __amd64__
	/* Convert 32-bit structs to native ones. */
	page_req32 = (void *)arg;
	ext_page_req32 = (void *)arg;
	raid_act32 = (void *)arg;
	switch (cmd) {
	case MPTIO_READ_CFG_HEADER32:
	case MPTIO_READ_CFG_PAGE32:
	case MPTIO_WRITE_CFG_PAGE32:
		page_req = &page_req_swab;
		page_req->header = page_req32->header;
		page_req->page_address = page_req32->page_address;
		page_req->buf = PTRIN(page_req32->buf);
		page_req->len = page_req32->len;
		page_req->ioc_status = page_req32->ioc_status;
		break;
	case MPTIO_READ_EXT_CFG_HEADER32:
	case MPTIO_READ_EXT_CFG_PAGE32:
		ext_page_req = &ext_page_req_swab;
		ext_page_req->header = ext_page_req32->header;
		ext_page_req->page_address = ext_page_req32->page_address;
		ext_page_req->buf = PTRIN(ext_page_req32->buf);
		ext_page_req->len = ext_page_req32->len;
		ext_page_req->ioc_status = ext_page_req32->ioc_status;
		break;
	case MPTIO_RAID_ACTION32:
		raid_act = &raid_act_swab;
		raid_act->action = raid_act32->action;
		raid_act->volume_bus = raid_act32->volume_bus;
		raid_act->volume_id = raid_act32->volume_id;
		raid_act->phys_disk_num = raid_act32->phys_disk_num;
		raid_act->action_data_word = raid_act32->action_data_word;
		raid_act->buf = PTRIN(raid_act32->buf);
		raid_act->len = raid_act32->len;
		raid_act->volume_status = raid_act32->volume_status;
		bcopy(raid_act32->action_data, raid_act->action_data,
		    sizeof(raid_act->action_data));
		raid_act->action_status = raid_act32->action_status;
		raid_act->ioc_status = raid_act32->ioc_status;
		raid_act->write = raid_act32->write;
		break;
	}
#endif

	switch (cmd) {
#ifdef __amd64__
	case MPTIO_READ_CFG_HEADER32:
#endif
	case MPTIO_READ_CFG_HEADER:
		MPT_LOCK(mpt);
		error = mpt2sas_user_read_cfg_header(mpt, page_req);
		MPT_UNLOCK(mpt);
		break;
#ifdef __amd64__
	case MPTIO_READ_CFG_PAGE32:
#endif
	case MPTIO_READ_CFG_PAGE:
		error = mpt2sas_alloc_buffer(mpt, &mpt2sas_page, page_req->len);
		if (error)
			break;
		error = copyin(page_req->buf, mpt2sas_page.vaddr,
		    sizeof(CONFIG_PAGE_HEADER));
		if (error)
			break;
		MPT_LOCK(mpt);
		error = mpt2sas_user_read_cfg_page(mpt, page_req, &mpt2sas_page);
		MPT_UNLOCK(mpt);
		if (error)
			break;
		error = copyout(mpt2sas_page.vaddr, page_req->buf, page_req->len);
		break;
#ifdef __amd64__
	case MPTIO_READ_EXT_CFG_HEADER32:
#endif
	case MPTIO_READ_EXT_CFG_HEADER:
		MPT_LOCK(mpt);
		error = mpt2sas_user_read_extcfg_header(mpt, ext_page_req);
		MPT_UNLOCK(mpt);
		break;
#ifdef __amd64__
	case MPTIO_READ_EXT_CFG_PAGE32:
#endif
	case MPTIO_READ_EXT_CFG_PAGE:
		error = mpt2sas_alloc_buffer(mpt, &mpt2sas_page, ext_page_req->len);
		if (error)
			break;
		error = copyin(ext_page_req->buf, mpt2sas_page.vaddr,
		    sizeof(CONFIG_EXTENDED_PAGE_HEADER));
		if (error)
			break;
		MPT_LOCK(mpt);
		error = mpt2sas_user_read_extcfg_page(mpt, ext_page_req, &mpt2sas_page);
		MPT_UNLOCK(mpt);
		if (error)
			break;
		error = copyout(mpt2sas_page.vaddr, ext_page_req->buf,
		    ext_page_req->len);
		break;
#ifdef __amd64__
	case MPTIO_WRITE_CFG_PAGE32:
#endif
	case MPTIO_WRITE_CFG_PAGE:
		error = mpt2sas_alloc_buffer(mpt, &mpt2sas_page, page_req->len);
		if (error)
			break;
		error = copyin(page_req->buf, mpt2sas_page.vaddr, page_req->len);
		if (error)
			break;
		MPT_LOCK(mpt);
		error = mpt2sas_user_write_cfg_page(mpt, page_req, &mpt2sas_page);
		MPT_UNLOCK(mpt);
		break;
#ifdef __amd64__
	case MPTIO_RAID_ACTION32:
#endif
	case MPTIO_RAID_ACTION:
		if (raid_act->buf != NULL) {
			error = mpt2sas_alloc_buffer(mpt, &mpt2sas_page, raid_act->len);
			if (error)
				break;
			error = copyin(raid_act->buf, mpt2sas_page.vaddr,
			    raid_act->len);
			if (error)
				break;
		}
		MPT_LOCK(mpt);
		error = mpt2sas_user_raid_action(mpt, raid_act, &mpt2sas_page);
		MPT_UNLOCK(mpt);
		if (error)
			break;
		if (raid_act->buf != NULL)
			error = copyout(mpt2sas_page.vaddr, raid_act->buf,
			    raid_act->len);
		break;
	default:
		error = ENOIOCTL;
		break;
	}

	mpt2sas_free_buffer(&mpt2sas_page);

	if (error)
		return (error);

#ifdef __amd64__
	/* Convert native structs to 32-bit ones. */
	switch (cmd) {
	case MPTIO_READ_CFG_HEADER32:
	case MPTIO_READ_CFG_PAGE32:
	case MPTIO_WRITE_CFG_PAGE32:
		page_req32->header = page_req->header;
		page_req32->page_address = page_req->page_address;
		page_req32->buf = PTROUT(page_req->buf);
		page_req32->len = page_req->len;
		page_req32->ioc_status = page_req->ioc_status;
		break;
	case MPTIO_READ_EXT_CFG_HEADER32:
	case MPTIO_READ_EXT_CFG_PAGE32:		
		ext_page_req32->header = ext_page_req->header;
		ext_page_req32->page_address = ext_page_req->page_address;
		ext_page_req32->buf = PTROUT(ext_page_req->buf);
		ext_page_req32->len = ext_page_req->len;
		ext_page_req32->ioc_status = ext_page_req->ioc_status;
		break;
	case MPTIO_RAID_ACTION32:
		raid_act32->action = raid_act->action;
		raid_act32->volume_bus = raid_act->volume_bus;
		raid_act32->volume_id = raid_act->volume_id;
		raid_act32->phys_disk_num = raid_act->phys_disk_num;
		raid_act32->action_data_word = raid_act->action_data_word;
		raid_act32->buf = PTROUT(raid_act->buf);
		raid_act32->len = raid_act->len;
		raid_act32->volume_status = raid_act->volume_status;
		bcopy(raid_act->action_data, raid_act32->action_data,
		    sizeof(raid_act->action_data));
		raid_act32->action_status = raid_act->action_status;
		raid_act32->ioc_status = raid_act->ioc_status;
		raid_act32->write = raid_act->write;
		break;
	}
#endif

	return (0);
}
