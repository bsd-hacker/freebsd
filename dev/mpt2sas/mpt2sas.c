/*-
 * Copyright (c) 2009 by Alacritech Inc.
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
/*
 * Based largely upon the MPT FreeBSD driver under the following copyrights
 */
/*-
 * Generic defines for LSI '909 FC  adapters.
 * FreeBSD Version.
 *
 * Copyright (c)  2000, 2001 by Greg Ansley
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
/*-
 * Copyright (c) 2002, 2006 by Matthew Jacob
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Support from Chris Ellsworth in order to make SAS adapters work
 * is gratefully acknowledged.
 *
 *
 * Support from LSI-Logic has also gone a great deal toward making this a
 * workable subsystem and is gratefully acknowledged.
 */
/*
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/mpt2sas/mpt2sas.h>
#include <machine/stdarg.h>
#include <sys/kdb.h>

#define MPT2_MAX_TRYS 3
#define MPT2_MAX_WAIT 300000

static int mpt2sas_enable_ioc(mpt2sas_t *);
static int mpt2sas_query_ioc(mpt2sas_t *);
static int mpt2sas_read_config_header(mpt2sas_t *, U8, U8);
static int mpt2sas_read_iounit_page1(mpt2sas_t *);
static int mpt2sas_cfg_sas_iounit_page1(mpt2sas_t *);
static int mpt2sas_read_vpd(mpt2sas_t *, sas_dev_t *, int);

/******************************* Bus DMA Support ******************************/
void
mpt2sas_map_rquest(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct mpt2sas_map_info *map_info;
	map_info = (struct mpt2sas_map_info *)arg;
	map_info->error = error;
	map_info->physaddr = segs->ds_addr;
}

/***************************** Interrupt Handling *****************************/

#define	MPT2SAS_DISPOSE_OF(m, r, s)				\
	if ((r)->state & REQ_STATE_NEED_WAKEUP) {		\
		wakeup(r);					\
	} else if ((r)->state & REQ_STATE_NEED_CALLBACK) {	\
		(*((req_callback_t *)(r)->ccb))((m), (r), (s));	\
	} else if (((r)->state & REQ_STATE_POLLED) == 0)	\
		mpt2sas_cam_done((m), (r), (MPI2_SCSI_IO_REPLY *)(s))
	

static void
mpt2sas_scsi_reply(mpt2sas_t *mpt, MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR *dsc)
{
	request_t *req;
	uint16_t smid;

	smid = le16toh(dsc->SMID);
	if (smid == 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "no SMID for completing SCSI response\n");
		return;
	}
	req = MPT2_SMID2REQ(mpt, smid);
	/*
	 * Is the request already free and this a duplicate completion?
	 */
	if (req->state & REQ_STATE_FREE) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: duplicate completion for SMID %u\n", __func__, smid);
		return;
	}
	TAILQ_REMOVE(&mpt->request_pending_list, req, links);
	mpt->nactive--;
	req->state |= REQ_STATE_DONE;
	MPT2SAS_DISPOSE_OF(mpt, req, NULL);
}

static void
mpt2sas_rqest_completion(mpt2sas_t *mpt, request_t *req, MPI2_DEFAULT_REPLY *reply)
{
	int i;
	U8 function;

	if (reply == NULL) {
		MPI2_REQUEST_HEADER *rqs = MPT2_REQ2RQS(mpt, req);
		function = rqs->Function;
	} else {
		function = reply->Function;
	}
		
	switch (function) {
	case MPI2_FUNCTION_CONFIG:
	{
		MPI2_CONFIG_REPLY *m = (MPI2_CONFIG_REPLY *) reply;
		if (m) {
			req->IOCStatus = le16toh(m->IOCStatus);
			if ((req->IOCStatus & MPI2_IOCSTATUS_MASK) == MPI2_IOCSTATUS_SUCCESS) {
				if (m->Action == MPI2_CONFIG_ACTION_PAGE_HEADER) {
					mpt->cfg_hdr = m->Header;
					mpt->cfg_ExtPageLength = le16toh(m->ExtPageLength);
					mpt->cfg_ExtPageType = m->ExtPageType;
				}
			} else {
				mpt2sas_prt(mpt, MP2PRT_ERR, "%s: MPI2_CONFIG_REPLY IOC STATUS %#x\n", __func__, req->IOCStatus);
			}
		} else {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: MPI2_FUNCTION_CONFIG with no reply frame\n", __func__);
		}
		break;
	}
	case MPI2_FUNCTION_SCSI_IO_REQUEST:
		if (reply == NULL) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: MPI2_FUNCTION_SCSI_IO_REQUEST with no reply frame\n", __func__);
		}
		break;
	case MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR:
	{
		MPI2_SEP_REPLY *m = (MPI2_SEP_REPLY *) reply;
		req->IOCStatus = le16toh(m->IOCStatus);
		if ((req->IOCStatus & MPI2_IOCSTATUS_MASK) == MPI2_IOCSTATUS_SUCCESS) {
			sas_dev_t *dp = (sas_dev_t *) req->ccb;
			dp->SlotStatus = le32toh(m->SlotStatus);
		} else {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: MPI2_SEP_REPLY IOC STATUS %#x\n", __func__, req->IOCStatus);
		}
	}
	case MPI2_FUNCTION_PORT_ENABLE:
	case MPI2_FUNCTION_EVENT_NOTIFICATION:
		break;
	case MPI2_FUNCTION_SCSI_TASK_MGMT:
		req->IOCStatus = le16toh(((MPI2_SCSI_TASK_MANAGE_REPLY *)reply)->IOCStatus);
		break;
	case MPI2_FUNCTION_SAS_IO_UNIT_CONTROL:
		req->flags = le16toh(((MPI2_SAS_IOUNIT_CONTROL_REPLY *)reply)->DevHandle);
		req->IOCStatus = le16toh(((MPI2_SAS_IOUNIT_CONTROL_REPLY *)reply)->IOCStatus);
		break;
	case MPI2_FUNCTION_SATA_PASSTHROUGH:
		if (reply == NULL) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: MPI2_FUNCTION_SATA_PASSTHROUGH with no reply frame\n", __func__);
			req->IOCStatus = MPI2_IOCSTATUS_INTERNAL_ERROR;
		} else {
			req->IOCStatus = le16toh(((MPI2_SATA_PASSTHROUGH_REPLY *)reply)->IOCStatus);
		}
		if ((req->IOCStatus & MPI2_IOCSTATUS_MASK) == MPI2_IOCSTATUS_SUCCESS) {
			mpt->SASStatus = ((MPI2_SATA_PASSTHROUGH_REPLY *)reply)->SASStatus;
			for (i = 0; i < 20; i++) {
				mpt->StatusFIS[i] = ((MPI2_SATA_PASSTHROUGH_REPLY *)reply)->StatusFIS[i];
			}
		}
		break;
	default:
		if (reply) {
			mpt2sas_prt(mpt, MP2PRT_INFO, "%s: REQUEST COMPLETION FUNCTION CODE %x Status 0x%x\n", __func__, function, le16toh(reply->IOCStatus));
		} else {
			mpt2sas_prt(mpt, MP2PRT_INFO, "%s: REQUEST COMPLETION FUNCTION CODE %x, no reply frame\n", __func__, function);
		}
		break;
	}
}

static void
mpt2sas_device_status_change(mpt2sas_t *mpt, MPI2_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE *dsc)
{
	sas_dev_t *dp;
	U16 hdl = le16toh(dsc->DevHandle);

	switch (dsc->ReasonCode) {
	case MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA\n", __func__);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED\n", __func__);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 1;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET (handle %x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 1;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL (handle %x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 1;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 1;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_INTERNAL:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_SET_INTERNAL (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_ASYNC_NOTIFICATION:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_ASYNC_NOTIFICATION (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CMP_INTERNAL_DEV_RESET:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 0;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_CMP_INTERNAL_DEV_RESET (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_CMP_TASK_ABORT_INTERNAL:
		dp = mpt2_hdl2dev(mpt, hdl);
		if (dp) {
			dp->internal_tm_bsy = 0;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_CMP_TASK_ABORT_INTERNAL (handle 0x%x)\n", __func__, hdl);
		break;
	case MPI2_EVENT_SAS_DEV_STAT_RC_SATA_INIT_FAILURE:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: MPI2_EVENT_SAS_DEV_STAT_RC_SATA_INIT_FAILURE (handle 0x%x)\n", __func__, hdl);
		break;
	default:
		mpt2sas_prt(mpt, MP2PRT_INFO, "%s: unknown reason code 0x%x (handle 0x%x)\n", __func__, dsc->ReasonCode, hdl);
		break;
	}
}

static void
mpt2sas_discovery_change(mpt2sas_t *mpt)
{
	int off, error;
	MPI2_CONFIG_REQUEST *rqs;
	MPI2_CONFIG_PAGE_HEADER hdr;
	MPI2_CONFIG_PAGE_SAS_DEV_0 *sio;
	U16 handle, cfglen;
	struct topochg *tp;
	request_t *req;

	if (mpt2sas_get_cfgbuf(mpt, &off)) {
		mpt2sas_prt(mpt, MP2PRT_WARN, "%s: cannot allocate config buffer\n", __func__);
		return;
	}
	error = mpt2sas_read_config_header(mpt, MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE, 0);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return;
	}
	cfglen = mpt->cfg_ExtPageLength;
	hdr = mpt->cfg_hdr;
	handle = 0xffff;
	for (;;) {
		MPT2SAS_GET_REQUEST(mpt, req);
		if (req == NULL) {
			mpt2sas_free_cfgbuf(mpt, off);
			return;
		}
		req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
		rqs = MPT2_REQ2RQS(mpt, req);
		memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
		rqs->Function = MPI2_FUNCTION_CONFIG;
		rqs->Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
		rqs->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE;
		rqs->ExtPageLength = htole16(cfglen);
		rqs->PageAddress = htole32(MPI2_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE | handle);
		rqs->Header = hdr;
		mpt2sas_single_sge(&rqs->PageBufferSGE.MpiSimple, mpt->config.paddr + off, cfglen << 2, MPI2_SGE_FLAGS_IOC_TO_HOST|SINGLE_SGE);
		memset(&mpt->config.vaddr[off], 0, MPT2_CONFIG_DATA_SIZE(mpt));
		mpt2sas_send_cmd(mpt, req);
		error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
		if (error) {
			if (error != EIO || mpt->iocsts != MPI2_IOCSTATUS_CONFIG_INVALID_PAGE) {
				MPT2SAS_SYNC_ERR_NORET(mpt, error);
			}
			break;
		}
		bus_dmamap_sync(mpt->config.dmat, mpt->config.dmap, BUS_DMASYNC_POSTREAD);
		sio = (MPI2_CONFIG_PAGE_SAS_DEV_0 *) &mpt->config.vaddr[off];
		mpt2host_sas_dev_page0_convert(dsc);
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: DevHandle %x SAS Address 0x%016jx Name 0x%016jx DeviceInfo 0x%08x PhyNum 0x%02x\n", __func__,
		    sio->DevHandle, (uintmax_t) le64toh(sio->SASAddress), (uintmax_t) le64toh(sio->DeviceName),  le32toh(sio->DeviceInfo),
		    sio->PhyNum);
		handle = sio->DevHandle;
		if (mpt2_hdl2dev(mpt, handle)) {
			continue;
		}
		TAILQ_FOREACH(tp, &mpt->topo_wait_list, links) {
			if (tp->create && tp->hdl == handle) {
				break;
			}
		}
		if (tp) {
			continue;
		}
		tp = TAILQ_FIRST(&mpt->topo_free_list);
		if (tp == NULL) {
			mpt2sas_free_cfgbuf(mpt, off);
			return;
		}
		TAILQ_REMOVE(&mpt->topo_free_list, tp, links);
		tp->hdl = handle;
		tp->create = 1;
		TAILQ_INSERT_TAIL(&mpt->topo_wait_list, tp, links);
	}
	mpt2sas_free_cfgbuf(mpt, off);
	mpt->fabchanged = 0;
}

static void
mpt2sas_topology_change(mpt2sas_t *mpt, MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *tpl)
{
	static const char *lrstat[16] = {
		"Unknown Link Rate",
		"PHY Disabled",
		"Negotiation Failed",
		"OOB Complete",
		"Port Selector",
		"SMP Reset In Progress",
		"Unsupported PHY",
		"Unknown Value 0x7",
		"1.5Gbps Link Rate",
		"3Gbps Link Rate",
		"6Gbps Link Rate",
		"Unknown Value 0xb",
		"Unknown Value 0xc",
		"Unknown Value 0xd",
		"Unknown Value 0xe",
		"Unknown Value 0xf"
	};
	sas_dev_t *dp;
	struct topochg *tp;
	U16 hdl;
	U8 status, this_rate, prev_rate;
	int i;

	switch (tpl->ExpStatus) {
	case MPI2_EVENT_SAS_TOPO_ES_NO_EXPANDER:
		mpt2sas_prt(mpt, MP2PRT_CONFIG2, "%s: noexpander event\n", __func__);
		break;
	case MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: not responding event\n", __func__);
		break;
	case MPI2_EVENT_SAS_TOPO_ES_RESPONDING:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: responding event\n", __func__);
		break;
	case MPI2_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: delay not responding event\n", __func__);
		break;
	case MPI2_EVENT_SAS_TOPO_ES_ADDED:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: expander added event\n", __func__);
		return;
	default:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: unknown event 0x%x\n", __func__, tpl->ExpStatus);
		return;
	}

	for (i = 0; i < tpl->NumEntries; i++) {
		if (tpl->PHY[i].PhyStatus & MPI2_EVENT_SAS_TOPO_PHYSTATUS_VACANT) {
			continue;
		}
		hdl = le16toh(tpl->PHY[i].AttachedDevHandle);
		status = tpl->PHY[i].PhyStatus & MPI2_EVENT_SAS_TOPO_RC_MASK;
		switch (status) {
		case MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED:
			if (hdl) {
				/*
				 * See if we already have a topology event for creation for this handle.
				 */
				TAILQ_FOREACH(tp, &mpt->topo_wait_list, links) {
					if (tp->create && tp->hdl == hdl) {
						break;
					}
				}
				if (tp) {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x created more than once\n", hdl);
					break;
				}
				tp = TAILQ_FIRST(&mpt->topo_free_list);
				if (tp == NULL) {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x Arrived - event lost\n", hdl);
					break;
				}
				mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x - scheduling for attach\n", hdl);
				TAILQ_REMOVE(&mpt->topo_free_list, tp, links);
				tp->hdl = hdl;
				tp->create = 1;
				TAILQ_INSERT_TAIL(&mpt->topo_wait_list, tp, links);
			} else {
				mpt2sas_prt(mpt, MP2PRT_WARN, "%s: target added, but no handle available", __func__);
			}
			break;
		case MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING:
			if (hdl) {
				/*
				 * See if we already have a topology event for creation for this handle.
				 */
				TAILQ_FOREACH(tp, &mpt->topo_wait_list, links) {
					if (tp->create && tp->hdl == hdl) {
						TAILQ_REMOVE(&mpt->topo_wait_list, tp, links);
						break;
					}
				}
				if (tp) {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x departed before it was seen\n", hdl);
					break;
				}
				/*
				 * See if we already have a topology event for deletion for this handle.
				 */
				TAILQ_FOREACH(tp, &mpt->topo_wait_list, links) {
					if (tp->create == 0 && tp->hdl == hdl) {
						break;
					}
				}
				if (tp) {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x departing more than once\n", hdl);
					break;
				}

				/*
				 * See if we already have a device structure for this. If we do, set
				 * the state appropriately.
				 */
				dp = mpt2_hdl2dev(mpt, hdl);
				if (dp) {
					switch (dp->state) {
					case NEW:
					case ATTACHING:
						dp->state = DETACHING;
						break;
					default:
						dp->state = FAILED;
						break;
					}
				}
				tp = TAILQ_FIRST(&mpt->topo_free_list);
				if (tp == NULL) {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x Departed- event lost\n", hdl);
				} else {
					mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle 0x%x Departed- scheduling for detach\n", hdl);
					TAILQ_REMOVE(&mpt->topo_free_list, tp, links);
					tp->hdl = hdl;
					tp->create = 0;
					TAILQ_INSERT_TAIL(&mpt->topo_wait_list, tp, links);
				}
			} else {
				mpt2sas_prt(mpt, MP2PRT_WARN, "%s: target not responding, but no handle available", __func__);
			}
			break;
		case MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED:
			this_rate = (tpl->PHY[i].LinkRate & MPI2_EVENT_SAS_TOPO_LR_CURRENT_MASK) >> MPI2_EVENT_SAS_TOPO_LR_CURRENT_SHIFT;
			prev_rate = (tpl->PHY[i].LinkRate & MPI2_EVENT_SAS_TOPO_LR_PREV_MASK) >> MPI2_EVENT_SAS_TOPO_LR_PREV_SHIFT;
			mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: PHY Link Status changed for DevHandle 0x%x (%s -> %s)\n", __func__, hdl, lrstat[prev_rate], lrstat[this_rate]);
			break;
		case MPI2_EVENT_SAS_TOPO_RC_NO_CHANGE:
			this_rate = (tpl->PHY[i].LinkRate & MPI2_EVENT_SAS_TOPO_LR_CURRENT_MASK) >> MPI2_EVENT_SAS_TOPO_LR_CURRENT_SHIFT;
			mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: DevHandle 0x%x No Link Status Change (%s)\n", __func__, hdl, lrstat[this_rate]);
			break;
		case MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING:
			mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: DevHandle 0x%x delayed not responding\n", __func__, hdl);
			break;
		default:
			mpt2sas_prt(mpt, MP2PRT_WARN, "%s: unknown status %x at entry %d\n", __func__, status, i);
			break;
		}
	}
}

static void
mpt2sas_ack_event(mpt2sas_t *mpt, MPI2_EVENT_NOTIFICATION_REPLY *reply)
{
	MPI2_EVENT_ACK_REQUEST *rqs;
	request_t *req;
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: Unable to allocate request for Event=%x EventContext=%x\n", __func__, reply->Event, reply->EventContext);
		mpt->acks_needed = 1;
		reply->Reserved2 = 0xDEAD;
		return;
	}
	mpt2sas_prt(mpt, MP2PRT_ALL, "%s: Event=%x EventContext=%x\n", __func__, reply->Event, reply->EventContext);
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_EVENT_ACK_REQUEST));
	rqs->Function = MPI2_FUNCTION_EVENT_ACK;
	rqs->Event = reply->Event;
	rqs->EventContext = reply->EventContext;
	mpt2sas_send_cmd(mpt, req);
}

static void
mpt2sas_event_notification(mpt2sas_t *mpt, MPI2_EVENT_NOTIFICATION_REPLY *evp)
{
	switch (le16toh(evp->Event)) {
	case MPI2_EVENT_LOG_DATA:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_LOG_DATA\n");
		break;
	case MPI2_EVENT_STATE_CHANGE:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_STATE_CHANGE\n");
		break;
	case MPI2_EVENT_HARD_RESET_RECEIVED:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_HARD_RESET_RECEIVED\n");
		break;
	case MPI2_EVENT_EVENT_CHANGE:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_EVENT_CHANGE\n");
		break;
	case MPI2_EVENT_TASK_SET_FULL:
	{
		sas_dev_t *dp;
		dp = mpt2_hdl2dev(mpt, le32toh(evp->EventData[0]) & 0xffff);
		if (dp) {
			dp->set_qfull = 1;
		}
		break;
	}
	case MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE:
		mpt2sas_device_status_change(mpt, (MPI2_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE *)evp->EventData);
		break;
	case MPI2_EVENT_IR_OPERATION_STATUS:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_IS_OPERATION_STATUS\n");
		break;
	case MPI2_EVENT_SAS_DISCOVERY:
	{
		MPI2_EVENT_DATA_SAS_DISCOVERY *p = (MPI2_EVENT_DATA_SAS_DISCOVERY *)evp->EventData;
		mpt2host_sas_discovery_event_convert(p);
		mpt2sas_prt(mpt, MP2PRT_CONFIG2, "MPI2_EVENT_SAS_DISCOVERY: Flags=%x ReasonCode=%x PhysicalPort=%x DiscoveryStatus=%x\n",
		    p->Flags, p->ReasonCode, p->PhysicalPort, p->DiscoveryStatus);
		if (p->Flags == MPI2_EVENT_SAS_DISC_DEVICE_CHANGE && p->ReasonCode == MPI2_EVENT_SAS_DISC_RC_COMPLETED) {
			mpt->fabchanged = 1;
		}
		break;
	}
	case MPI2_EVENT_SAS_BROADCAST_PRIMITIVE:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "MPI2_EVENT_SAS_BROADCAST_PRIMITIVE\n");
		break;
	case MPI2_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "MPI2_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE\n");
		break;
	case MPI2_EVENT_SAS_INIT_TABLE_OVERFLOW:
		mpt2sas_prt(mpt, MP2PRT_ERR, "MPI2_EVENT_SAS_INIT_TABLE_OVERFLOW\n");
		break;
	case MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST\n");
		mpt2sas_topology_change(mpt, (MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *)evp->EventData);
		break;
	case MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE\n");
		break;
	case MPI2_EVENT_IR_VOLUME:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_IR_VOLUME\n");
		break;
	case MPI2_EVENT_IR_PHYSICAL_DISK:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_IR_PHYSICAL_DISK\n");
		break;
	case MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST\n");
		break;
	case MPI2_EVENT_LOG_ENTRY_ADDED:
		mpt2sas_prt(mpt, MP2PRT_INFO, "MPI2_EVENT_LOG_ENTRY_ADDEDD\n");
		break;
	default:
		mpt2sas_prt(mpt, MP2PRT_ERR, "Unknown Event %x\n", le16toh(evp->Event));
		break;
	}
	if (evp->AckRequired) {
		mpt2sas_ack_event(mpt, evp);
	}
}

static void
mpt2sas_async_completion(mpt2sas_t *mpt, MPI2_DEFAULT_REPLY *reply)
{
	switch (reply->Function) {
	case MPI2_FUNCTION_EVENT_NOTIFICATION:
		mpt2sas_event_notification(mpt, (MPI2_EVENT_NOTIFICATION_REPLY *)reply);
		break;
	default:
		mpt2sas_prt(mpt, MP2PRT_WARN, "unknown ASYNC COMPLETION 0x%x\n", reply->Function);
		break;
	}
}

static void
mpt2sas_address_reply(mpt2sas_t *mpt, MPI2_ADDRESS_REPLY_DESCRIPTOR *dsc)
{
	MPI2_DEFAULT_REPLY *reply;
	uint16_t smid;

	/*
	 * NB: There is a weakness here if allocated exactly on a 4GiB boundary
	 */
	if (dsc->ReplyFrameAddress) {
		uint8_t *addr;
		reply = MPT2_RFA2PTR(mpt, le32toh(dsc->ReplyFrameAddress));
		addr = (uint8_t *)reply;
		bus_dmamap_sync(mpt->replies.dmat, mpt->replies.dmap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		if (addr < mpt->replies.vaddr || addr > &mpt->replies.vaddr[MPT2_REPLY_MEM_SIZE(mpt)]) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "bad reply address %p SMID %u\n", reply, le16toh(dsc->SMID));
			reply = NULL;
		}
		if (addr[0] == 0xff && addr[1] == 0xff && addr[2] == 0xff && addr[3] == 0xff) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "bad reply address %p free_host_index = %u\n", reply, mpt->free_host_index);
			reply = NULL;
		}
	} else {
		reply = NULL;
	}
	smid = le16toh(dsc->SMID);
	if (smid) {
		request_t *req = MPT2_SMID2REQ(mpt, smid);
		/*
		 * Is the request already free and this a duplicate completion?
		 */
		if (req->state & REQ_STATE_FREE) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: duplicate completion for SMID %u\n", __func__, smid);
			return;
		}
		TAILQ_REMOVE(&mpt->request_pending_list, req, links);
		req->state |= REQ_STATE_DONE;
		mpt->nactive--;
		mpt2sas_rqest_completion(mpt, req, reply);
		MPT2SAS_DISPOSE_OF(mpt, req, reply);
	} else if (reply) {
		mpt2sas_async_completion(mpt, reply);
	} else {
		/*
		 * XXX: We have gotten an address reply, but with no valid SMID,
		 * XXX: and no reply pointer. HUH?
		 */
		mpt2sas_prt(mpt, MP2PRT_ERR, "address reply interrupt but with no address\n");
	}
	if (reply && reply->Reserved1 != 0xDEAD) {
		memset(reply, 0xff, MPT2_REPLY_SIZE(mpt));
		mpt->free_host_index++;
		if (mpt->free_host_index == MPT2_RPF_QDEPTH(mpt)) {
			mpt->free_host_index = 0;
		}
		MPT_REPLYF_QIDX(mpt, mpt->free_host_index) = htole32(dsc->ReplyFrameAddress);
		bus_dmamap_sync(mpt->replyf.dmat, mpt->replyf.dmap, BUS_DMASYNC_PREREAD);
		mpt2sas_write(mpt, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, htole32(mpt->free_host_index));
	}
}

static void
mpt2sas_handle_reply_int(mpt2sas_t *mpt)
{
	uint16_t phi = mpt->post_host_index;
	MPI2_REPLY_DESCRIPTORS_UNION *dsc = &MPT_REPLYQ_QIDX(mpt, phi);
	int nreply = 0;

	mpt->rpintr++;
	while (dsc->Words != (U64) ~0ULL) {
		switch (dsc->Default.ReplyFlags & MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK) {
		case MPI2_RPY_DESCRIPT_FLAGS_SCSI_IO_SUCCESS:
			mpt2sas_scsi_reply(mpt, &dsc->SCSIIOSuccess);
			break;
		case MPI2_RPY_DESCRIPT_FLAGS_ADDRESS_REPLY:
			if (dsc->AddressReply.ReplyFrameAddress == 0 && dsc->AddressReply.SMID) {
				mpt2sas_prt(mpt, MP2PRT_ERR, "%s: NULL REPLY, PHI=%u nreply=%lu thisreply %u rpf=%u rpq=%u\n", __func__, phi, (mpt->nreply + nreply + 1),
				    nreply, mpt2sas_read(mpt, MPI2_REPLY_FREE_HOST_INDEX_OFFSET), mpt2sas_read(mpt, MPI2_REPLY_POST_HOST_INDEX_OFFSET));
			}
			mpt2sas_address_reply(mpt, &dsc->AddressReply);
			break;
		case MPI2_RPY_DESCRIPT_FLAGS_TARGETASSIST_SUCCESS:
			mpt2sas_prt(mpt, MP2PRT_INFO, "%s: target assist success\n", __func__);
			break;
		case MPI2_RPY_DESCRIPT_FLAGS_TARGET_COMMAND_BUFFER:
			mpt2sas_prt(mpt, MP2PRT_INFO, "%s: target command buffer\n", __func__);
			break;
		default:
			mpt2sas_prt(mpt, MP2PRT_ERR, "%s: unknown reply type 0x%x\n", __func__,
			    dsc->Default.ReplyFlags & MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK);
			break;
		}
		phi++;
		nreply++;
		if (phi == MPT2_RPQ_QDEPTH(mpt)) {
			phi = 0;
		}
		dsc->Words = (U64) ~0ULL;
		dsc = &MPT_REPLYQ_QIDX(mpt, phi);
	}
	if (nreply) {
		bus_dmamap_sync(mpt->replyq.dmat, mpt->replyq.dmap, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		mpt->post_host_index = phi;
		mpt2sas_write(mpt, MPI2_REPLY_POST_HOST_INDEX_OFFSET, htole32(mpt->post_host_index));
	}
	mpt->nreply += nreply;
}

void
mpt2sas_intr(void *arg)
{
	mpt2sas_t *mpt = (mpt2sas_t *)arg;
	uint32_t isrval;

	isrval = mpt2sas_read(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET);
	if ((isrval & (MPI2_HIS_DOORBELL_INTERRUPT|MPI2_HIS_REPLY_DESCRIPTOR_INTERRUPT)) == 0) {
		return;
	}
	if (isrval & MPI2_HIS_DOORBELL_INTERRUPT) {
		isrval ^= MPI2_HIS_DOORBELL_INTERRUPT;
	}
	if (isrval & MPI2_HIS_REPLY_DESCRIPTOR_INTERRUPT) {
		bus_dmamap_sync(mpt->replyq.dmat, mpt->replyq.dmap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		mpt2sas_handle_reply_int(mpt);
		isrval ^= MPI2_HIS_REPLY_DESCRIPTOR_INTERRUPT;
	}
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);
}

/******************************* Doorbell Access ******************************/
static __inline uint32_t mpt2sas_rd_db(mpt2sas_t *mpt);

static __inline uint32_t
mpt2sas_rd_db(mpt2sas_t *mpt)
{
	return mpt2sas_read(mpt, MPI2_DOORBELL_OFFSET);
}

/* Busy wait for a door bell to be read by IOC */
static int
mpt2sas_wait_db_ack(mpt2sas_t *mpt)
{
	int i;
	for (i=0; i < MPT2_MAX_WAIT; i++) {
		if (!MPT2_DB_IS_BUSY(mpt)) {
			return (MPT2_OK);
		}
		DELAY(200);
	}
	return (MPT2_FAIL);
}

/* Busy wait for a door bell interrupt */
static int
mpt2sas_wait_db_int(mpt2sas_t *mpt)
{
	int i;
	for (i = 0; i < MPT2_MAX_WAIT; i++) {
		if (MPT2_DB_INTR(mpt)) {
			return MPT2_OK;
		}
		DELAY(100);
	}
	return (MPT2_FAIL);
}

/* Wait for IOC to transition to a give state */
static int
mpt2sas_wait_state(mpt2sas_t *mpt, uint32_t state)
{
	int i;

	for (i = 0; i < MPT2_MAX_WAIT; i++) {
		uint32_t db = mpt2sas_rd_db(mpt);
		if (MPT2_STATE(db) == state) {
			return (MPT2_OK);
		}
		DELAY(100);
	}
	return (MPT2_FAIL);
}

static int
mpt2sas_recv_handshake_reply(mpt2sas_t *mpt, size_t reply_len, void *reply)
{
	int left, reply_left;
	u_int16_t *data16;
	uint32_t data;
	MPI2_DEFAULT_REPLY *hdr;

	/* We move things out in 16 bit chunks */
	reply_len >>= 1;
	data16 = (u_int16_t *)reply;

	hdr = (MPI2_DEFAULT_REPLY *)reply;

	/* Get first word */
	if (mpt2sas_wait_db_int(mpt) != MPT2_OK) {
		return ETIMEDOUT;
	}
	data = mpt2sas_read(mpt, MPI2_DOORBELL_OFFSET);
	*data16++ = le16toh(data & MPI2_DOORBELL_DATA_MASK);
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);

	/* Get Second Word */
	if (mpt2sas_wait_db_int(mpt) != MPT2_OK) {
		return ETIMEDOUT;
	}
	data = mpt2sas_read(mpt, MPI2_DOORBELL_OFFSET);
	*data16++ = le16toh(data & MPI2_DOORBELL_DATA_MASK);
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);

	/*
	 * With the second word, we can now look at the length.
	 * Warn about a reply that's too short (except for IOC FACTS REPLY)
	 */
	if ((reply_len >> 1) != hdr->MsgLength && (hdr->Function != MPI2_FUNCTION_IOC_FACTS)) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "got %x; expected %zx for function %x\n",
		hdr->MsgLength << 2, reply_len << 1, hdr->Function);
	}

	/* Get rest of the reply; but don't overflow the provided buffer */
	left = (hdr->MsgLength << 1) - 2;
	reply_left =  reply_len - 2;
	while (left--) {
		u_int16_t datum;

		if (mpt2sas_wait_db_int(mpt) != MPT2_OK) {
			return ETIMEDOUT;
		}
		data = mpt2sas_read(mpt, MPI2_DOORBELL_OFFSET);
		datum = le16toh(data & MPI2_DOORBELL_DATA_MASK);

		if (reply_left-- > 0)
			*data16++ = datum;

		mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);
	}

	/* One more wait & clear at the end */
	if (mpt2sas_wait_db_int(mpt) != MPT2_OK) {
		return ETIMEDOUT;
	}
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);

	if ((hdr->IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		return (MPT2_FAIL | hdr->IOCStatus);
	}

	return (0);
}

int
mpt2sas_handshake_cmd(mpt2sas_t *mpt, size_t len, void *cmd, size_t reply_len, void *reply)
{
	int i;
	uint32_t data, *data32;

	/* Check condition of the IOC */
	data = mpt2sas_rd_db(mpt);
	if ((MPT2_STATE(data) != MPI2_IOC_STATE_READY && MPT2_STATE(data) != MPI2_IOC_STATE_OPERATIONAL &&
	    MPT2_STATE(data) != MPI2_IOC_STATE_FAULT) || MPT2_DB_IS_IN_USE(data)) {
		return (EBUSY);
	}

	/* We move things in 32 bit chunks */
	len = (len + 3) >> 2;
	data32 = cmd;

	/* Clear any left over pending doorbell interrupts */
	if (MPT2_DB_INTR(mpt)) {
		mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);
	}

	/*
	 * Tell the handshake reg. we are going to send a command
	 * and how long it is going to be.
	 */
	data = (MPI2_FUNCTION_HANDSHAKE << MPI2_DOORBELL_FUNCTION_SHIFT) |
	    (len << MPI2_DOORBELL_ADD_DWORDS_SHIFT);
	mpt2sas_write(mpt, MPI2_DOORBELL_OFFSET, data);

	/* Wait for the chip to notice */
	if (mpt2sas_wait_db_int(mpt) != MPT2_OK) {
		return (ETIMEDOUT);
	}

	/* Clear the interrupt */
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);

	if (mpt2sas_wait_db_ack(mpt) != MPT2_OK) {
		return (ETIMEDOUT);
	}

	/* Send the command */
	for (i = 0; i < len; i++) {
		mpt2sas_write(mpt, MPI2_DOORBELL_OFFSET, htole32(*data32++));
		if (mpt2sas_wait_db_ack(mpt) != MPT2_OK) {
			return (ETIMEDOUT);
		}
	}
	if (reply_len == 0)
		return MPT2_OK;
	return(mpt2sas_recv_handshake_reply(mpt, reply_len, reply));
}
/***************************** Misc Functions *****************************/

/*
 * Build a SATA Passthrough request. The caller will send it.
 */
void
mpt2sas_build_ata_passthru(mpt2sas_t *mpt, sas_dev_t *dp, U8 *fis, request_t *req, bus_addr_t paddr, uint32_t flags)
{
	U32 dlen;
	U16 pflag;
	MPI2_SATA_PASSTHROUGH_REQUEST *rqs;
	int i;

	/*
	 * Note that setting the protocol flags can be
	 * independent of actually moving any data.
	 */
	switch (flags & MPT2_APT_PMASK) {
	case MPT2_APT_PIO:
		pflag = MPI2_SATA_PT_REQ_PT_FLAGS_PIO;
		break;
	case MPT2_APT_DMA:
		pflag = MPI2_SATA_PT_REQ_PT_FLAGS_DMA;
		break;
	default:
		pflag = 0;
		break;
	}
	/*
	 * Note that setting the data direction flags can be
	 * independent of actually moving any data.
	 */
	if (flags & MPT2_APT_READ) {
		pflag |= MPI2_SATA_PT_REQ_PT_FLAGS_READ;
	} else if (flags & MPT2_APT_WRITE) {
		pflag |= MPI2_SATA_PT_REQ_PT_FLAGS_WRITE;
	}
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0, sizeof (MPI2_SATA_PASSTHROUGH_REQUEST));
	rqs->DevHandle = htole16(dp->AttachedDevHandle);
	rqs->Function = MPI2_FUNCTION_SATA_PASSTHROUGH;
	dlen = (flags & MPT2_APT_DLMASK) >> MPT2_APT_DLSHFT;
	if (dlen) {
		if (flags & MPT2_APT_READ) {
			mpt2sas_single_sge(&rqs->SGL.MpiSimple, paddr, dlen, MPI2_SGE_FLAGS_IOC_TO_HOST|SINGLE_SGE);
		} else if (flags & MPT2_APT_WRITE) {
			mpt2sas_single_sge(&rqs->SGL.MpiSimple, paddr, dlen, MPI2_SGE_FLAGS_HOST_TO_IOC|SINGLE_SGE);
		}
		rqs->DataLength = htole32(dlen);
	} else {
		rqs->SGL.MpiSimple.FlagsLength = htole32(ZERO_LENGTH_SGE);
	}
	rqs->PassthroughFlags = htole16(pflag);
	for (i = 0; i < 20; i++) {
		rqs->CommandFIS[i] = fis[i];
	}
	req->handle = dp->AttachedDevHandle;
}

int
mpt2sas_check_sata_passthru_failure(mpt2sas_t *mpt, MPI2_SATA_PASSTHROUGH_REPLY *rqs)
{
	int failure = 0;
	if ((rqs->IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		mpt2sas_prt(mpt, MP2PRT_WARN, "ATA PassThru IOC Status 0x%x\n", rqs->IOCStatus);
		failure = 1;
	}
	if (failure == 0 && (rqs->StatusFIS[2] & 0x1) != 0) {
		mpt2sas_prt(mpt, MP2PRT_WARN, "ATA PassThru Status FIS=0x%x 0x%x 0x%x 0x%x\n", rqs->StatusFIS[0], rqs->StatusFIS[1], rqs->StatusFIS[2], rqs->StatusFIS[3]);
		failure = 2;
	}
	return (failure);
}
/************************** Endian Functions ***************************/
/*
 * Endian Conversion Functions- only used on Big Endian machines
 */
#if	_BYTE_ORDER == _BIG_ENDIAN
void
mpt2host_iocfacts_convert(MPI2_IOC_FACTS_REPLY *rp)
{
	MPT2_2_HOST16(rp, MsgVersion);
	MPT2_2_HOST16(rp, HeaderVersion);
	MPT2_2_HOST16(rp, Reserved1);
	MPT2_2_HOST16(rp, IOCExceptions);
	MPT2_2_HOST16(rp, IOCStatus);
	MPT2_2_HOST32(rp, IOCLogInfo);
	MPT2_2_HOST16(rp, RequestCredit);
	MPT2_2_HOST16(rp, ProductID);
	MPT2_2_HOST32(rp, IOCCapabilities);
	MPT2_2_HOST32(rp, FWVersion.Word);
	MPT2_2_HOST16(rp, IOCRequestFrameSize);
	MPT2_2_HOST16(rp, Reserved3);
	MPT2_2_HOST16(rp, MaxInitiators);
	MPT2_2_HOST16(rp, MaxTargets);
	MPT2_2_HOST16(rp, MaxSasExpanders);
	MPT2_2_HOST16(rp, MaxEnclosures);
	MPT2_2_HOST16(rp, ProtocolFlags);
	MPT2_2_HOST16(rp, HighPriorityCredit);
	MPT2_2_HOST16(rp, MaxReplyDescriptorPostQueueDepth);
	MPT2_2_HOST16(rp, MaxDevHandle);
	MPT2_2_HOST32(rp, Reserved4);
}

void
mpt2host_portfacts_convert(MPI2_PORT_FACTS_REPLY *pfp)
{
	MPT2_2_HOST16(pfp, Reserved1);
	MPT2_2_HOST16(pfp, Reserved2);
	MPT2_2_HOST16(pfp, Reserved3);
	MPT2_2_HOST16(pfp, Reserved4);
	MPT2_2_HOST16(pfp, IOCStatus);
	MPT2_2_HOST32(pfp, IOCLogInfo);
	MPT2_2_HOST16(pfp, Reserved6);
	MPT2_2_HOST16(pfp, MaxPostedCmdBuffers);
	MPT2_2_HOST16(pfp, Reserved7);
}

void
mpt2host_phydata_convert(MPI2_SAS_IO_UNIT0_PHY_DATA *pd)
{
	MPT2_2_HOST32(pd, ControllerPhyDeviceInfo);
	MPT2_2_HOST16(pd, AttachedDevHandle);
    	MPT2_2_HOST16(pd, ControllerDevHandle);
    	MPT2_2_HOST32(pd, DiscoveryStatus);
    	MPT2_2_HOST32(pd, Reserved);
}

void
mpt2host_sas_dev_page0_convert(MPI2_CONFIG_PAGE_SAS_DEV_0 *sio)
{
	MPT2_2_HOST16(sio, Header.ExtPageLength);
	MPT2_2_HOST16(sio, Slot);
	MPT2_2_HOST16(sio, EnclosureHandle);
	MPT2_2_HOST16(sio, ParentDevHandle);
	MPT2_2_HOST16(sio, DevHandle);
	MPT2_2_HOST16(sio, Flags);
	MPT2_2_HOST64(sio, SASAddress);
	MPT2_2_HOST64(sio, DeviceName);
	MPT2_2_HOST32(sio, DeviceInfo);
	MPT2_2_HOST32(sio, Reserved2);
	MPT2_2_HOST32(sio, Reserved3);
}

void mpt2host_sas_discovery_event_convert(MPI2_EVENT_DATA_SAS_DISCOVERY *sp)
{
	MPT2_2_HOST32(sp, DiscoveryStatus);
}
#endif
/******************************* Discovery and SAS/IOC Config Routines **************************/

int
mpt2sas_destroy_dev(mpt2sas_t *mpt, U16 hdl)
{
	MPI2_SCSI_TASK_MANAGE_REQUEST *reset;
	request_t *req;
	sas_dev_t *dp;
	int error;
	int nreq;

	KASSERT(hdl < mpt->ioc_facts.MaxTargets, ("%s: oops", __func__));
	dp = &mpt->sas_dev_pool[hdl];
	if (dp->AttachedDevHandle == 0) {
		return (0);
	}
	if (dp->state == DRAINING) {
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: handle 0x%x already draining\n", __func__, hdl);
		return (0);
	}
	KASSERT(hdl == dp->AttachedDevHandle, ("%s: oops2", __func__));
	if (dp->internal_tm_bsy) {
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: internal task management busy for handle 0x%x- retrying\n", __func__, hdl);
		return (EBUSY);
	}
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: cannot allocate request\n", __func__);
		return (ENOBUFS);
	}
	dp->state = DRAINING;
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY;
	reset = MPT2_REQ2RQS(mpt, req);
	memset(reset, 0,  sizeof (MPI2_SCSI_TASK_MANAGE_REQUEST));
	reset->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	reset->DevHandle = htole16(dp->AttachedDevHandle);
	reset->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: sending target reset for handle 0x%x\n", __func__, hdl);
	mpt2sas_send_cmd(mpt, req);
	nreq = dp->active;
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 10000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: %d requests were still in play at reset target reset time, %s mpt2sas_destroy_dev_part2\n", __func__, nreq,
		dp->active? "calling" : "*not* calling");
	if (dp->active == 0) {
		mpt2sas_destroy_dev_part2(dp);
	} else {
		dp->destroy_needed = 1;
	}
	return (0);
}

static void
mpt2sas_destroy_dev_cb(void *arg)
{
	sas_dev_t *dp = arg;
	MPT2_LOCK(dp->mpt);
	mpt2sas_destroy_dev_part2(dp);
	MPT2_UNLOCK(dp->mpt);
}

void
mpt2sas_destroy_dev_part2(sas_dev_t *dp)
{
	MPI2_SAS_IOUNIT_CONTROL_REQUEST *ucr;
	mpt2sas_t *mpt;
	request_t *req;
	int error;

	KASSERT(dp, ("aiee, null dp!"));
	if (dp->AttachedDevHandle == 0) {
		return;
	}
	KASSERT(dp->AttachedDevHandle == dp->AttachedDevHandle, ("%s: handle %x != handle %x", __func__, dp->AttachedDevHandle, dp->AttachedDevHandle));
	mpt = dp->mpt;
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: destroy DeviceHandle %x Phy %x active %u\n", __func__, dp->AttachedDevHandle, dp->PhyNum, dp->active);
	dp->destroy_needed = 0;
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: cannot allocate request\n", __func__);
		callout_reset(&dp->actions, 5, mpt2sas_destroy_dev_cb, dp);
	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	ucr = MPT2_REQ2RQS(mpt, req);
	memset(ucr, 0,  sizeof (MPI2_SAS_IOUNIT_CONTROL_REQUEST));
	ucr->Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	ucr->Operation = MPI2_SAS_OP_REMOVE_DEVICE;
	ucr->DevHandle = htole16(dp->AttachedDevHandle);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 10000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	dp->state = DETACHING;
	mpt->devchanged = 1;
}

int
mpt2sas_create_dev(mpt2sas_t *mpt, U16 hdl)
{
	sas_dev_t *dp;
	int off, error;

	KASSERT(hdl < mpt->ioc_facts.MaxTargets, ("%s: oops", __func__));
	dp = &mpt->sas_dev_pool[hdl];
	if (dp->AttachedDevHandle) {
		mpt2sas_prt(mpt, MP2PRT_WARN, "%s: device 0x%x already attached as hdl 0x%x\n", __func__, dp->AttachedDevHandle, hdl);
		return (0);
	}
	if (mpt2sas_get_cfgbuf(mpt, &off)) {
		mpt2sas_prt(mpt, MP2PRT_WARN, "%s: cannot allocate config buffer\n", __func__);
		return (ENOMEM);
	}
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "creating device (hdl %x)\n", hdl);
	dp->AttachedDevHandle = hdl;
	dp->state = NEW;
	error = mpt2sas_read_vpd(mpt, dp, off);
	mpt2sas_free_cfgbuf(mpt, off);
	if (error == ENOMEM) {
		dp->AttachedDevHandle = 0;
		dp->state = NIL;
		return (error);
	} else if (error) {
		dp->AttachedDevHandle = 0;
		dp->state = NIL;
		mpt2sas_prt(mpt, MP2PRT_ERR, "mpt2sas_read_vpd failed (error %d)\n", error);
		return (0);
	}
	/*
	 * Catch the case here where we never made it.
	 */
	if (dp->state == NEW) {
		dp->state = ATTACHING;
		mpt->devchanged = 1;
	}
	return (0);
}

static int
mpt2sas_read_config_header(mpt2sas_t *mpt, U8 PageType, U8 PageNumber)
{
	MPI2_CONFIG_REQUEST *rqs;
	request_t *req;
	int error;

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
	rqs->Function = MPI2_FUNCTION_CONFIG;
	rqs->Action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	if (PageType >= MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT) {
		rqs->ExtPageType = PageType;
		rqs->Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
	} else {
		rqs->Header.PageType = PageType;
	}
	rqs->Header.PageNumber = PageNumber;
	rqs->PageBufferSGE.MpiSimple.FlagsLength = htole32(ZERO_LENGTH_SGE);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR(mpt, error);
}

static int
mpt2sas_read_iounit_page1(mpt2sas_t *mpt)
{
	MPI2_CONFIG_REQUEST *rqs;
	MPI2_CONFIG_PAGE_IO_UNIT_1 *ptr;
	request_t *req;
	int error, off;

	if (mpt2sas_get_cfgbuf(mpt, &off)) {
		return (ENOMEM);
	}
	error = mpt2sas_read_config_header(mpt, MPI2_CONFIG_PAGETYPE_IO_UNIT, 1);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (error);
	}
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (ENOMEM);
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
	rqs->Function = MPI2_FUNCTION_CONFIG;
	rqs->Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	rqs->Header = mpt->cfg_hdr;
	mpt2sas_single_sge(&rqs->PageBufferSGE.MpiSimple, mpt->config.paddr+off, rqs->Header.PageLength << 2, MPI2_SGE_FLAGS_IOC_TO_HOST);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (error);
 	}
	bus_dmamap_sync(mpt->config.dmat, mpt->config.dmap, BUS_DMASYNC_POSTREAD);
	ptr = (MPI2_CONFIG_PAGE_IO_UNIT_1 *) &mpt->config.vaddr[off];
	mpt->iounit_pg1_flags = le32toh(ptr->Flags);
	mpt2sas_free_cfgbuf(mpt, off);
 	return (MPT2_OK);
}

static int
mpt2sas_cfg_sas_iounit_page1(mpt2sas_t *mpt)
{
	MPI2_CONFIG_REQUEST *rqs;
	MPI2_CONFIG_PAGE_SASIOUNIT_1 *ptr;
	request_t *req;
	int error, off;

	if (mpt2sas_get_cfgbuf(mpt, &off)) {
		return (ENOMEM);
	}
	error = mpt2sas_read_config_header(mpt, MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT, 1);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (error);
	}
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (ENOMEM);
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
	rqs->Function = MPI2_FUNCTION_CONFIG;
	rqs->Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	rqs->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	rqs->ExtPageLength = htole16(mpt->cfg_ExtPageLength);
	rqs->Header = mpt->cfg_hdr;
	mpt2sas_single_sge(&rqs->PageBufferSGE.MpiSimple, mpt->config.paddr+off, mpt->cfg_ExtPageLength << 2, MPI2_SGE_FLAGS_IOC_TO_HOST);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 10000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (error);
 	}
	bus_dmamap_sync(mpt->config.dmat, mpt->config.dmap, BUS_DMASYNC_POSTREAD);
	ptr = (MPI2_CONFIG_PAGE_SASIOUNIT_1 *) &mpt->config.vaddr[off];
	mpt2sas_prt(mpt, MP2PRT_ALL, "ReportDeviceMissingDelay %x IODeviceMissingDelay %x\n", ptr->ReportDeviceMissingDelay, ptr->IODeviceMissingDelay);
	if (ptr->ReportDeviceMissingDelay == 3 && ptr->IODeviceMissingDelay == 3) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (MPT2_OK);
	}
	ptr->ReportDeviceMissingDelay = 3;
	ptr->IODeviceMissingDelay = 3;
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (ENOMEM);
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
	rqs->Function = MPI2_FUNCTION_CONFIG;
	rqs->Action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	rqs->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT;
	rqs->ExtPageLength = htole16(mpt->cfg_ExtPageLength);
	rqs->Header = mpt->cfg_hdr;
	mpt2sas_single_sge(&rqs->PageBufferSGE.MpiSimple, mpt->config.paddr+off, mpt->cfg_ExtPageLength << 2, MPI2_SGE_FLAGS_HOST_TO_IOC);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 10000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	if (error) {
		mpt2sas_free_cfgbuf(mpt, off);
		return (error);
 	}
	mpt2sas_free_cfgbuf(mpt, off);
 	return (MPT2_OK);
}

static int
mpt2sas_read_vpd(mpt2sas_t *mpt, sas_dev_t *dp, int off)
{
	MPI2_CONFIG_REQUEST *rqs;
	MPI2_CONFIG_PAGE_SAS_DEV_0 *sio;
	request_t *req;
	int error;

	error = mpt2sas_read_config_header(mpt, MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE, 0);
	if (error) {
		return (error);
	}
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_CONFIG_REQUEST));
	rqs->Function = MPI2_FUNCTION_CONFIG;
	rqs->Action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	rqs->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE;
	rqs->ExtPageLength = htole16(mpt->cfg_ExtPageLength);
	rqs->PageAddress = htole32(MPI2_SAS_DEVICE_PGAD_FORM_HANDLE | dp->AttachedDevHandle);
	rqs->Header = mpt->cfg_hdr;
	mpt2sas_single_sge(&rqs->PageBufferSGE.MpiSimple, mpt->config.paddr + off, mpt->cfg_ExtPageLength << 2, MPI2_SGE_FLAGS_IOC_TO_HOST|SINGLE_SGE);
	memset(&mpt->config.vaddr[off], 0, MPT2_CONFIG_DATA_SIZE(mpt));
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	if (error) {
		return (error);
 	}
	bus_dmamap_sync(mpt->config.dmat, mpt->config.dmap, BUS_DMASYNC_POSTREAD);
	sio = (MPI2_CONFIG_PAGE_SAS_DEV_0 *) &mpt->config.vaddr[off];
	mpt2host_sas_dev_page0_convert(dsc);
	if ((sio->DeviceInfo & MPI2_SAS_DEVICE_INFO_MASK_DEVICE_TYPE) != MPI2_SAS_DEVICE_INFO_END_DEVICE) {
		return (ENODEV);
	}
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "DevHandle %x SAS Address 0x%016jx Name 0x%016jx DeviceInfo 0x%08x PhyNum 0x%02x\n",
	    sio->DevHandle, (uintmax_t) le64toh(sio->SASAddress), (uintmax_t) le64toh(sio->DeviceName),  le32toh(sio->DeviceInfo),
	    sio->PhyNum);
	dp->PhyNum = sio->PhyNum;
	if (sio->EnclosureHandle) {
		dp->Slot = sio->Slot;
		dp->EnclosureHandle = sio->EnclosureHandle;
		dp->has_slot_info = 1;
	}
	if (sio->DeviceInfo & MPI2_SAS_DEVICE_INFO_SATA_DEVICE) {
		struct ata_params *ata;
		U8 fis[20];
		uint32_t flags;

		MPT2SAS_GET_REQUEST(mpt, req);
		if (req == NULL) {
			return (ENOMEM);
		}
		memset(fis, 0, sizeof (fis));
 		fis[0] = 0x27;
		fis[1] = 0x80;
		fis[2] = ATA_ATA_IDENTIFY;
		flags = MPT2_APT_READ | MPT2_APT_PIO | (512 << MPT2_APT_DLSHFT);
		mpt2sas_build_ata_passthru(mpt, dp, fis, req, mpt->config.paddr+off, flags);
		mpt2sas_send_cmd(mpt, req);
		error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
		MPT2SAS_SYNC_ERR_NORET(mpt, error);
		if (error) {
			return (error);
		}
		/*
		 * Okay, now that we have this ATA IDENTIFY data, we can find our NCQ queue depth
		 */
		bus_dmamap_sync(mpt->config.dmat, mpt->config.dmap, BUS_DMASYNC_POSTREAD);
		ata = (struct ata_params *)&mpt->config.vaddr[off];
		if (ata->satacapabilities && ata->satacapabilities != 0xffff && (le16toh(ata->satacapabilities) & ATA_SUPPORT_NCQ)) {
			dp->qdepth = ATA_QUEUE_LEN(le16toh(ata->queue));
		} else {
			dp->qdepth = 1;
		}
		dp->is_sata = 1;
	} else {
		dp->is_sata = 0;
	}
 	return (MPT2_OK);
}

/******************************* Reset Routines ******************************/
/*
 * Issue a reset command to the IOC
 */
static int
mpt2sas_soft_reset(mpt2sas_t *mpt)
{
	/* Have to use hard reset if we are not in Running state */
	if (MPT2_STATE(mpt2sas_rd_db(mpt)) != MPI2_IOC_STATE_OPERATIONAL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "soft reset failed: device not running\n");
		return (MPT2_FAIL);
	}

	/*
	 * If door bell is in use we don't have a chance of getting
	 * a word in since the IOC probably crashed in message
	 * processing. So don't waste our time.
	 */
	if (MPT2_DB_IS_IN_USE(mpt2sas_rd_db(mpt))) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "soft reset failed: doorbell wedged\n");
		return (MPT2_FAIL);
	}

	/* Send the reset request to the IOC */
	mpt2sas_write(mpt, MPI2_DOORBELL_OFFSET, MPI2_FUNCTION_IOC_MESSAGE_UNIT_RESET << MPI2_DOORBELL_FUNCTION_SHIFT);
	if (mpt2sas_wait_db_ack(mpt) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "soft reset failed: ack timeout\n");
		return (MPT2_FAIL);
	}

	/* Wait for the IOC to reload and come out of reset state */
	if (mpt2sas_wait_state(mpt, MPI2_IOC_STATE_READY) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "soft reset failed: device did not restart- state now 0x%x\n", MPT2_STATE(mpt2sas_rd_db(mpt)));
		return (MPT2_FAIL);
	}
	return (MPT2_OK);
}

static int
mpt2sas_enable_diag_mode(mpt2sas_t *mpt)
{
	int try;

	for (try = 0; try < 20; try++) {
		if ((mpt2sas_read(mpt, MPI2_HOST_DIAGNOSTIC_OFFSET) & MPI2_DIAG_DIAG_WRITE_ENABLE) != 0) {
			break;
		}
		/* Enable diagnostic registers */
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_FLUSH_KEY_VALUE);
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_1ST_KEY_VALUE);
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_2ND_KEY_VALUE);
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_3RD_KEY_VALUE);
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_4TH_KEY_VALUE);
		mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, MPI2_WRSEQ_5TH_KEY_VALUE);

		DELAY(100000);
	}
	if (try == 20) {
		return (EIO);
	}
	return (0);
}

static void
mpt2sas_disable_diag_mode(mpt2sas_t *mpt)
{
	mpt2sas_write(mpt, MPI2_WRITE_SEQUENCE_OFFSET, 0xFFFFFFFF);
}

/* This is a magic diagnostic reset that resets all the ARM
 * processors in the chip.
 */
static void
mpt2sas_hard_reset(mpt2sas_t *mpt)
{
	int error;
	int wait;
	uint32_t diagreg;

	mpt2sas_prt(mpt, MP2PRT_ERR, "%s\n", __func__);

	error = mpt2sas_enable_diag_mode(mpt);
	if (error) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "WARNING - Could not enter diagnostic mode !\n");
		return;
	}

	diagreg = mpt2sas_read(mpt, MPI2_HOST_DIAGNOSTIC_OFFSET);
	mpt2sas_write(mpt, MPI2_HOST_DIAGNOSTIC_OFFSET, diagreg | MPI2_DIAG_HOLD_IOC_RESET);
	DELAY(1000);

	/* Diag. port is now active so we can now hit the reset bit */
	mpt2sas_write(mpt, MPI2_HOST_DIAGNOSTIC_OFFSET, diagreg | MPI2_DIAG_RESET_ADAPTER);

	/*
	 * Ensure that the reset has finished.  We delay 1ms
 	 * prior to reading the register to make sure the chip
	 * has sufficiently completed its reset to handle register
	 * accesses.
	 */
	wait = 5000;
	do {
		DELAY(1000);
		diagreg = mpt2sas_read(mpt, MPI2_HOST_DIAGNOSTIC_OFFSET);
	} while (--wait && (diagreg & MPI2_DIAG_RESET_ADAPTER) == 0);

	if (wait == 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "WARNING - Failed hard reset!\n");
		return;
	}

	/*
	 * Reseting the controller should have disabled write
	 * access to the diagnostic registers, but disable
	 * manually to be sure.
	 */
	mpt2sas_disable_diag_mode(mpt);
}

/*
 * Reset the IOC when needed. Try software command first then if needed
 * poke at the magic diagnostic reset.
 */
int
mpt2sas_reset(mpt2sas_t *mpt)
{
	mpt2sas_prt(mpt, MP2PRT_ERR, "%s\n", __func__);
	if (mpt2sas_soft_reset(mpt) == MPT2_FAIL) {
		mpt2sas_hard_reset(mpt);
		if (mpt2sas_wait_state(mpt, MPI2_IOC_STATE_READY) == MPT2_FAIL) {
			return (MPT2_FAIL);
		}
	}
	return (MPT2_OK);
}

/******************************* Request Management ******************************/
void
mpt2sas_free_request(mpt2sas_t *mpt, request_t *req)
{
	mpt2sas_dma_chunk_t *cl, *nxt;

	KASSERT((req > &mpt->request_pool[0] && req < &mpt->request_pool[MPT2_MAX_REQUESTS(mpt)]), ("bad request pointer"));
	KASSERT((mpt->nreq_allocated > 1), ("bad allocation arithmetic"));

	if ((cl = req->chain) != NULL) {
		while (cl) {
			nxt = cl->linkage;
			cl->linkage = mpt->dma_chunk_free;
			mpt->dma_chunk_free = cl;
			cl = nxt;
		}
	}
	req->chain = NULL;
	req->ccb = NULL;
	if (req->state & REQ_STATE_TIMEDOUT) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "request %p:%u now being freed\n", req, req->serno);
	}
	req->serno = 0;
	req->state = REQ_STATE_FREE;
	req->IOCStatus = 0;
	req->flags = 0;
	req->handle = 0;
	req->timeout = 0;
	TAILQ_INSERT_TAIL(&mpt->request_free_list, req, links);
	if (mpt->getreqwaiter != 0) {
		mpt->getreqwaiter = 0;
		wakeup(&mpt->request_free_list);
	}
	mpt->nreq_allocated--;
}

char *
mpt2sas_decode_request(mpt2sas_t *mpt, request_t *req, char *buf, size_t len)
{
	MPI2_REQUEST_HEADER *rqs = MPT2_REQ2RQS(mpt, req);
	if (req->func)
		snprintf(buf, len, "%p:%u opcode 0x%02x from %s:%u", req, req->serno, rqs->Function, req->func, req->lineno);
	else
		snprintf(buf, len, "%p:%u opcode 0x%02x", req, req->serno, rqs->Function);
	return (buf);
}

request_t *
mpt2sas_get_request(mpt2sas_t *mpt)
{
	request_t *req = TAILQ_FIRST(&mpt->request_free_list);
	if (req != NULL) {
		TAILQ_REMOVE(&mpt->request_free_list, req, links);
		req->state = REQ_STATE_ALLOCATED;
		mpt2sas_assign_serno(mpt, req);
		mpt->nreq_allocated++;
		req->func = NULL;
	}
	return (req);
}

void
mpt2sas_send_cmd(mpt2sas_t *mpt, request_t *req)
{
	MPI2_REQUEST_DESCRIPTOR_UNION u;

	TAILQ_INSERT_TAIL(&mpt->request_pending_list, req, links);
	req->state |= REQ_STATE_QUEUED;
	u.Default.RequestFlags = req->flags;
	u.Default.MSIxIndex = 0;
	u.Default.SMID = htole16(MPT2_REQ2SMID(mpt, req));
	u.Default.LMID = 0;
	switch (req->flags & MPI2_REQ_DESCRIPT_FLAGS_TYPE_MASK) {
	default:
		u.Default.DescriptorTypeDependent = 0;
		break;
	case MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO:
		u.SCSIIO.DevHandle = htole16(req->handle);	
		break;
	}
	bus_dmamap_sync(mpt->requests.dmat, mpt->requests.dmap, BUS_DMASYNC_PREWRITE);
	mpt->nreq++;
	mpt->nactive++;
	mpt2sas_write_request_descriptor(mpt, u.Words);
}

int
mpt2sas_wait_req(mpt2sas_t *mpt, request_t *req, req_state_t state, req_state_t mask, int time_ms)
{
	int timeout;
	u_int saved_cnt;

	/*
	 * timeout is in ms.  0 indicates infinite wait.
	 */
	saved_cnt = mpt->reset_cnt;
	timeout = time_ms;
	req->state |= REQ_STATE_POLLED;
	mask &= ~REQ_STATE_POLLED;
	timeout++;
	while ((req->state & mask) != state && mpt->reset_cnt == saved_cnt) {
		uint32_t isrval;
		if (time_ms != 0 && --timeout == 0) {
			break;
		}
		DELAY(1000);
		isrval = mpt2sas_read(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET);
		if (isrval & MPI2_HIS_REPLY_DESCRIPTOR_INTERRUPT) {
			bus_dmamap_sync(mpt->replyq.dmat, mpt->replyq.dmap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			mpt2sas_handle_reply_int(mpt);
			mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);
		}
	}
	if (mpt->reset_cnt != saved_cnt) {
		return (EIO);
	}
	if (time_ms && timeout <= 0) {
		req->state |= REQ_STATE_TIMEDOUT;
		return (ETIMEDOUT);
	}
	mpt->iocsts = req->IOCStatus;
	mpt->dhdl = req->flags;
	mpt2sas_free_request(mpt, req);
	if ((mpt->iocsts & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		return (EIO);
	} else {
		return (0);
	}
}

/******************************* Initialization ******************************/
static int
mpt2sas_get_iocfacts(mpt2sas_t *mpt, MPI2_IOC_FACTS_REPLY *freplp)
{
	MPI2_IOC_FACTS_REQUEST f_req;
	memset(&f_req, 0, sizeof f_req);
	f_req.Function = MPI2_FUNCTION_IOC_FACTS;
	return (mpt2sas_handshake_cmd(mpt, sizeof f_req, &f_req, sizeof (*freplp), freplp));
}


static int
mpt2sas_get_portfacts(mpt2sas_t *mpt, U8 port, MPI2_PORT_FACTS_REPLY *freplp)
{
	MPI2_PORT_FACTS_REQUEST f_req;
	memset(&f_req, 0, sizeof f_req);
	f_req.Function = MPI2_FUNCTION_PORT_FACTS;
	f_req.PortNumber = port;
	return (mpt2sas_handshake_cmd(mpt, sizeof f_req, &f_req, sizeof (*freplp), freplp));
}

/*
 * Send the initialization request. This is where we specify how many
 * SCSI busses and how many devices per bus we wish to emulate.
 * This is also the command that specifies the max size of the reply
 * frames from the IOC that we will be allocating.
 */
static int
mpt2sas_send_ioc_init(mpt2sas_t *mpt, uint32_t who)
{
	int error = 0;
	struct timeval tv;
	MPI2_IOC_INIT_REQUEST init;
	MPI2_IOC_INIT_REPLY reply;

	memset(&init, 0, sizeof init);
	init.WhoInit = who;
	init.Function = MPI2_FUNCTION_IOC_INIT;
	init.MsgVersion = htole16(MPI2_VERSION);
	init.HeaderVersion = htole16(MPI2_HEADER_VERSION);
	init.SystemRequestFrameSize = htole16(MPT2_REQUEST_SIZE(mpt) >> 2);

	init.ReplyDescriptorPostQueueDepth = htole16(MPT2_RPQ_QDEPTH(mpt));
	init.ReplyFreeQueueDepth = htole16(MPT2_RPF_QDEPTH(mpt));

	init.SenseBufferAddressHigh = htole32(((uint64_t)(mpt->sense.paddr)) >> 32);
	init.SystemReplyAddressHigh = htole32(((uint64_t)(mpt->replies.paddr)) >> 32);
	init.SystemRequestFrameBaseAddress = htole64(mpt->requests.paddr);
	init.ReplyDescriptorPostQueueAddress = htole64(mpt->replyq.paddr);
	init.ReplyFreeQueueAddress = htole64(mpt->replyf.paddr);
	microtime(&tv);
	init.TimeStamp = (((U64) tv.tv_sec) * 1000) + (tv.tv_usec / 1000);	/* milliseconds since the Epoch */
	error = mpt2sas_handshake_cmd(mpt, sizeof init, &init, sizeof reply, &reply);
	if ((reply.IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "IOC init failed (0x%x)\n", reply.IOCStatus & MPI2_IOCSTATUS_MASK);
		error = EIO;
	}
	return (error);
}

/*
 * Enable IOC port
 */
void
mpt2sas_send_port_enable(mpt2sas_t *mpt)
{
	MPI2_PORT_ENABLE_REQUEST *rqs;
	request_t *req;
	int error;
 
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: unable to get request\n", __func__);
		return;
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_PORT_ENABLE_REQUEST));
	rqs->Function = MPI2_FUNCTION_PORT_ENABLE;
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 30000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	if (error == 0) {
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s: port enabled\n", __func__);
		mpt->portenabled = 1;
	}
}

/*
 * Enable/Disable asynchronous event reporting.
 */
static int
mpt2sas_send_event_request(mpt2sas_t *mpt, int onoff)
{
	int error;
	MPI2_EVENT_NOTIFICATION_REQUEST *rqs;
	request_t *req;

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
 	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_EVENT_NOTIFICATION_REQUEST));
	rqs->Function = MPI2_FUNCTION_EVENT_NOTIFICATION;
	if (onoff == 0) {
		memset(rqs->EventMasks, 0xff, MPI2_EVENT_NOTIFY_EVENTMASK_WORDS * 4);
	}
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 5000);
	MPT2SAS_SYNC_ERR(mpt, error);
}

/*
 * Un-mask the interrupts on the chip.
 */
void
mpt2sas_enable_ints(mpt2sas_t *mpt)
{
	mpt->intr_mask = mpt2sas_read(mpt, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mpt->intr_mask &= ~MPI2_HIM_REPLY_INT_MASK;
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_MASK_OFFSET, mpt->intr_mask);
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0);
}

/*
 * Mask the interrupts on the chip.
 */
void
mpt2sas_disable_ints(mpt2sas_t *mpt)
{
	uint32_t debug;
	mpt->intr_mask = mpt2sas_read(mpt, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	mpt->intr_mask |= (MPI2_HIM_RESET_IRQ_MASK|MPI2_HIM_REPLY_INT_MASK|MPI2_HIM_IOC2SYS_DB_MASK);
	mpt2sas_write(mpt, MPI2_HOST_INTERRUPT_MASK_OFFSET, mpt->intr_mask);
	/* force a flush by reading to guarantee disabled before we return */
	debug = mpt2sas_read(mpt, MPI2_HOST_INTERRUPT_MASK_OFFSET);
	if (debug != mpt->intr_mask) {
		;	/* do nothing */
	}
}

static void
mpt2sas_intr_enable(void *arg)
{
	mpt2sas_t *mpt = arg;
	config_intrhook_disestablish(&mpt->ehook);
	mpt->ehook_active = 0;
	mpt2sas_enable_ints(mpt);
}

/******************************* Attachment ******************************/
/*
 * Initialize per-instance driver data and perform initial controller configuration.
 */
static int
mpt2sas_init(mpt2sas_t *mpt)
{
	uint32_t base;
	uint8_t *ptr;
	MPI2_MPI_SGE_IO_UNION *cptr;
	mpt2sas_dma_chunk_t dummy;
	int val, error;

	TAILQ_INIT(&mpt->topo_free_list);
	TAILQ_INIT(&mpt->topo_wait_list);
	TAILQ_INIT(&mpt->request_pending_list);
	TAILQ_INIT(&mpt->request_free_list);

	mpt2sas_disable_ints(mpt);

	if (mpt2sas_soft_reset(mpt) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "failed to do a soft reset\n");
		return (EIO);
	}

	error = mpt2sas_query_ioc(mpt);
	if (error) {
		return (error);
	}

	/*
	 * Check and adjust request frame size
	 */
	if (mpt->request_frame_size < (mpt->ioc_facts.IOCRequestFrameSize << 2)) {
		mpt->request_frame_size = mpt->ioc_facts.IOCRequestFrameSize;
	}

	/*
	 * Check and adjust the number of request queue elements based upon credits
	 */
	mpt->max_requests = DEFAULT_MPT_REQUESTS;
	if (mpt->ioc_facts.RequestCredit < DEFAULT_MPT_REQUESTS) {
		mpt->max_requests = mpt->ioc_facts.RequestCredit;
	}

	mpt->max_replies =  mpt->max_requests + 32;
	/*
	 * The number of reply frames cannot be a multiple of 16
	 */
	if ((mpt->max_replies % 16) == 0)
		mpt->max_replies -= 1;

	mpt->reply_free_queue_depth = roundup(mpt->max_replies, 16);
	mpt->reply_post_queue_depth = roundup(mpt->max_requests + mpt->max_replies + 1, 16);
	if (mpt->reply_post_queue_depth > mpt->ioc_facts.MaxReplyDescriptorPostQueueDepth) {
		uint32_t qd = roundup(mpt->reply_post_queue_depth - mpt->ioc_facts.MaxReplyDescriptorPostQueueDepth, 16);
		mpt->max_requests -= qd;
		mpt->reply_free_queue_depth -= qd;
		mpt->reply_post_queue_depth -= qd;
	}
	/*
	 * Figure out our chunk size.
	 */
	cptr = &dummy.segs[0];
	mpt->sge_per_chunk = 0;
	for (;;) {
		size_t off;

		ptr = (uint8_t *)cptr;

		off = ptr - (uint8_t *) &dummy;
		if (off >= MPT2_CHUNK_SIZE(mpt)) {
			mpt->sge_per_chunk--;
			break;
		}
		mpt->sge_per_chunk++;
		cptr++;
	}

	/*
	 * Now allocate dma resources and structure elements
	 */
	if (mpt2sas_mem_alloc(mpt)) {
		return (ENOMEM);
	}

	/*
	 * Init some registers and other values
	 */
	memset(mpt->requests.vaddr, 0,  MPT2_REQ_MEM_SIZE(mpt));
	memset(mpt->replies.vaddr, 0,  MPT2_REPLY_MEM_SIZE(mpt));
	memset(mpt->replyq.vaddr, 0xff,  MPT2_RPQ_QDEPTH(mpt) << 3);

	/* initialize the reply free queue to point to the replies */
	base = mpt->replies.paddr;
	for (val = 0; val < MPT2_RPF_QDEPTH(mpt); val++) {
		MPT_REPLYF_QIDX(mpt, val) = htole32(base);
		base += MPT2_REPLY_SIZE(mpt);
	}

	mpt->post_host_index = 0;
	mpt->free_host_index = MPT2_RPF_QDEPTH(mpt) - 1;

	/*
	 * Free all allocated host side requests
	 */
	mpt->nreq_total = MPT2_MAX_REQUESTS(mpt);
	mpt->nreq_allocated = MPT2_MAX_REQUESTS(mpt);
	for (val = 1; val < MPT2_MAX_REQUESTS(mpt); val++) {
		request_t *req = &mpt->request_pool[val];
		req->state = REQ_STATE_ALLOCATED;
		mpt2sas_free_request(mpt, req);
	}

	/*
	 * Initialize the DMA sge chunks
	 */

	for (ptr = mpt->chunks.vaddr; ptr < &mpt->chunks.vaddr[MPT2_CHUNK_MEM_SIZE(mpt)]; ptr += MPT2_CHUNK_SIZE(mpt)) {
		mpt2sas_dma_chunk_t *ss = (mpt2sas_dma_chunk_t *) ptr;
		if (ptr == &mpt->chunks.vaddr[MPT2_CHUNK_MEM_SIZE(mpt)-1]) {
			ss->linkage = NULL;
		} else {
			ss->linkage = (mpt2sas_dma_chunk_t *) &ptr[MPT2_CHUNK_SIZE(mpt)];
		}
	}
	mpt->dma_chunk_free = (mpt2sas_dma_chunk_t *) mpt->chunks.vaddr;

	/*
	 * Enable the IOC
	 */
	MPT2_LOCK(mpt);
	if (mpt2sas_enable_ioc(mpt) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "unable to initialize IOC\n");
		MPT2_UNLOCK(mpt);
		return (ENXIO);
	}

	/*
	 * Set things up.
	 */
	mpt2sas_write(mpt, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, htole32(mpt->free_host_index));
	mpt2sas_write(mpt, MPI2_REPLY_POST_HOST_INDEX_OFFSET, htole32(mpt->post_host_index));


	/*
	 * Enable Interrupts
	 */
	mpt2sas_enable_ints(mpt);

	/*
	 * From this point out, real requests and replies should be working
	 */

	/*
	 * Enable events
	 */
	if (mpt2sas_send_event_request(mpt, 1)) {
		mpt2sas_disable_ints(mpt);
		MPT2_UNLOCK(mpt);
		return (ENXIO);
	}


	/*
	 * Read IO Unit Page 1.
	 */

	if (mpt2sas_read_iounit_page1(mpt) != MPT2_OK) {
		mpt2sas_disable_ints(mpt);
		MPT2_UNLOCK(mpt);
		return (EIO);
	}

	mpt2sas_prt(mpt, MP2PRT_CONFIG, "IO Unit Page 1 Flags = 0x%x\n", mpt->iounit_pg1_flags);
	base = mpt->iounit_pg1_flags;
	if (mpt->ioc_facts.IOCCapabilities & MPI2_IOCFACTS_CAPABILITY_TASK_SET_FULL_HANDLING) {
		base &= ~MPI2_IOUNITPAGE1_DISABLE_TASK_SET_FULL_HANDLING;
	} else {
		base |= MPI2_IOUNITPAGE1_DISABLE_TASK_SET_FULL_HANDLING;
	}
	if (base != mpt->iounit_pg1_flags) {
		mpt->iounit_pg1_flags = base;
#if	0
		if (mpt2sas_write_iounit_page1(mpt) != MPT2_OK) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "unable to change task set full handling\n");
		}
#else
		mpt2sas_prt(mpt, MP2PRT_INFO, "change task set full handling needs implementation\n");
#endif
	}

	mpt2sas_prt(mpt, MP2PRT_CONFIG, "MaxRequest=%u MaxReply=%u RPF_QDEPTH=%u RPQ_QDEPTH=%u MaxChainDepth=%u %lu Dma Chunks, sge_per_chunk=%u\n",
	    MPT2_MAX_REQUESTS(mpt), MPT2_MAX_REPLIES(mpt), MPT2_RPF_QDEPTH(mpt), MPT2_RPQ_QDEPTH(mpt), mpt->ioc_facts.MaxChainDepth,
	    MPT2_CHUNK_MEM_SIZE(mpt) / MPT2_CHUNK_SIZE(mpt), mpt->sge_per_chunk);

	/*
	 * Enable the port. Port enable can fail if you have a bad drive, so if we fail to eanble the port,
	 * we still consider ourselves 'here' (but need to try again later).
	 */
	mpt2sas_send_port_enable(mpt);

	MPT2_UNLOCK(mpt);
	return (0);
}

/*
 * Allocate/Initialize data structures for the controller.
 * Called once at instance startup.
 */
static int
mpt2sas_query_ioc(mpt2sas_t *mpt)
{
	int i;
	static const struct {
		char *val;
		unsigned int mask;
	} capabilities[] = {
		{ "HOST_BASED_DISCOVERY",	0x00010000	},
		{ "MSI_X_INDEX",		0x00008000	},
		{ "RAID_ACCELERATOR",		0x00004000	},
		{ "EVENT_REPLAY",		0x00002000	},
		{ "INTEGRATED_RAID",		0x00001000	},
		{ "TLR",			0x00000800	},
		{ "MULTICAST",			0x00000100	},
		{ "BIDIRECTIONAL_TARGET",	0x00000080	},
		{ "EEDP",			0x00000040	},
		{ "EXTENDED_BUFFER",		0x00000020	},
		{ "SNAPSHOT_BUFFER",		0x00000010	},
		{ "DIAG_TRACE_BUFFER",		0x00000008	},
		{ "TASK_SET_FULL_HANDLING",	0x00000004	},
		{ NULL,				0		}
	};

	if (mpt2sas_get_iocfacts(mpt, &mpt->ioc_facts) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "cannot get IOC Facts\n");
		return (EIO);
	}
	mpt2host_iocfacts_convert(&mpt->ioc_facts);
	snprintf(mpt->fwversion, sizeof (mpt->fwversion), "%d.%d.%d.%d", (mpt->ioc_facts.FWVersion.Word >> 24) & 0xff,
	    (mpt->ioc_facts.FWVersion.Word >> 16) & 0xff, (mpt->ioc_facts.FWVersion.Word >>  8) & 0xff, (mpt->ioc_facts.FWVersion.Word >>  0) & 0xff);
	mpt2sas_prt(mpt, MP2PRT_ALL, "FW Version=%s MPI2 Version=%d.%d.%d.%d\n", mpt->fwversion,
	    mpt->ioc_facts.MsgVersion >> 8, mpt->ioc_facts.MsgVersion & 0xFF, mpt->ioc_facts.HeaderVersion >> 8, mpt->ioc_facts.HeaderVersion & 0xFF);
	mpt2sas_prt(mpt, MP2PRT_CONFIG, "IOCCapabilities Report:\n");
	for (i = 0; capabilities[i].val != NULL; i++) {
		if (mpt->ioc_facts.IOCCapabilities & capabilities[i].mask)
			mpt2sas_prt(mpt, MP2PRT_CONFIG, "\t%s\n", capabilities[i].val);
	}
	if (mpt->ioc_facts.NumberOfPorts == 0) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "zero ports\n");
		return (EIO);
	}
	if (mpt2sas_get_portfacts(mpt, 0, &mpt->port_facts) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "unable to get Port Facts for Port 0\n");
		return (EIO);
	}
	mpt2host_portfacts_convert(&mpt->port_facts);
	if (mpt->port_facts.PortType != MPI2_PORTFACTS_PORTTYPE_SAS_PHYSICAL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Port Type 0x%x on not supported\n", mpt->port_facts.PortType);
		return (ENXIO);
	}
	return (0);
}

static int
mpt2sas_enable_ioc(mpt2sas_t *mpt)
{
	if (mpt2sas_send_ioc_init(mpt, MPI2_WHOINIT_HOST_DRIVER) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "mpt2sas_send_ioc_init failed\n");
		return (EIO);
	}

	if (mpt2sas_wait_state(mpt, MPI2_IOC_STATE_OPERATIONAL) != MPT2_OK) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "IOC failed to go to run state\n");
		return (ENXIO);
	}
	return (MPT2_OK);
}

/*
 * Read SEP status for this device
 */
int
mpt2sas_read_sep(mpt2sas_t *mpt, sas_dev_t *dp)
{
	MPI2_SEP_REQUEST *rqs;
	request_t *req;
	int error;

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_SEP_REQUEST));
	rqs->DevHandle = htole16(dp->AttachedDevHandle);
	rqs->Function = MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR;
	rqs->Action = MPI2_SEP_REQ_ACTION_READ_STATUS;
	rqs->Flags = MPI2_SEP_REQ_FLAGS_DEVHANDLE_ADDRESS;
	req->ccb = (union ccb *) dp;
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR(mpt, error);
}

/*
 * Write SEP status for this device
 */
int
mpt2sas_write_sep(mpt2sas_t *mpt, sas_dev_t *dp, U32 SepStatus)
{
	MPI2_SEP_REQUEST *rqs;
	request_t *req;
	int error;

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0,  sizeof (MPI2_SEP_REQUEST));
	rqs->Function = MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR;
	rqs->Action = MPI2_SEP_REQ_ACTION_WRITE_STATUS;
	if (dp->has_slot_info) {
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s Using Slot Addressing (%x:%u) to write %x\n", __func__, dp->EnclosureHandle, dp->Slot, SepStatus);
		rqs->EnclosureHandle = htole16(dp->EnclosureHandle);
		rqs->Slot = htole16(dp->Slot);
		rqs->Flags = MPI2_SEP_REQ_FLAGS_ENCLOSURE_SLOT_ADDRESS;
	} else {
		mpt2sas_prt(mpt, MP2PRT_CONFIG, "%s Using Device Handle Addressing (%x) to write %x\n", __func__, dp->AttachedDevHandle, SepStatus);
		rqs->DevHandle = htole16(dp->AttachedDevHandle);
		rqs->Flags = MPI2_SEP_REQ_FLAGS_DEVHANDLE_ADDRESS;
	}
	req->ccb = (union ccb *) dp;
	rqs->SlotStatus = htole32(SepStatus);
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR_NORET(mpt, error);
	return (0);
}

static void
mpt2sas_watchdog(void *arg)
{
	mpt2sas_t *mpt = arg;
	uint32_t mselapsed;
	struct timeval now, save;
	request_t *req;
	char buf[64];
	int error;
	struct topochg *tp;
	int r;

	MPT2_LOCK(mpt);

	microtime(&save);
	now = save;
	timevalsub(&now, &mpt->watchdog_then);
	mselapsed = (now.tv_sec * 1000) + (now.tv_usec / 1000);
	mpt->watchdog_then = save;
	TAILQ_FOREACH(req, &mpt->request_pending_list, links) {
		if (req->timeout == 0 || req->timeout == CAM_TIME_INFINITY) {
			continue;
		}
		if (req->timeout > mselapsed) {
			req->timeout -= mselapsed;
			continue;
		}
		req->timeout = 0;
		req->state |= REQ_STATE_TIMEDOUT;
		mpt2sas_prt(mpt, MP2PRT_ERR, "request %s timed out\n", mpt2sas_decode_request(mpt, req, buf, sizeof (buf)));
		if ((req->state & (REQ_STATE_NEED_CALLBACK|REQ_STATE_NEED_WAKEUP)) == 0 && req->ccb) {
			error = mpt2sas_scsi_abort(mpt, req);
			if (error == ENOMEM) {
				req->state &= ~REQ_STATE_TIMEDOUT;
				req->timeout = 1000;
			} else if (error) {
				mpt2sas_prt(mpt, MP2PRT_ERR, "request %s failed to abort\n", mpt2sas_decode_request(mpt, req, buf, sizeof (buf)));
			}
		}
	}

	while ((tp = TAILQ_FIRST(&mpt->topo_wait_list)) != NULL) {
		if (tp->create) {
			r = mpt2sas_create_dev(mpt, tp->hdl);
		} else {
			r = mpt2sas_destroy_dev(mpt, tp->hdl);
		}
		if (r == 0) {
			TAILQ_REMOVE(&mpt->topo_wait_list, tp, links);
			TAILQ_INSERT_TAIL(&mpt->topo_free_list, tp, links);
		} else {
			break;
		}
	}
	if (mpt->fabchanged) {
		mpt2sas_discovery_change(mpt);
	}
	if (mpt->ehook_active == 0 && mpt->path && mpt->devchanged) {
		sas_dev_t *dp;
		int r;
		for (dp = mpt->sas_dev_pool; dp < &mpt->sas_dev_pool[mpt->ioc_facts.MaxTargets]; dp++) {
			if (dp->state == DETACHING) {
				ClearSasDev(dp);
			} else if (dp->state == ATTACHING) {
				dp->state = STABLE;
			}
		}
		mpt->devchanged = 0;
		MPT2_UNLOCK(mpt);
		r = mpt2sas_cam_rescan(mpt);
		MPT2_LOCK(mpt);
		if (r) {
			mpt->devchanged = 1;
		}
	}
	callout_reset(&mpt->watchdog, hz, mpt2sas_watchdog, mpt);
	MPT2_UNLOCK(mpt);
}

void
mpt2sas_prt(mpt2sas_t *mpt, int mask, const char *fmt, ...)
{
	char buf[256];
	int used;
	va_list ap;
	if (mask != MP2PRT_ALL && (mask & mpt->prt_mask) == 0) {
		return;
	}
	snprintf(buf, sizeof buf, "%s: ", device_get_nameunit(mpt->dev));
	used = strlen(buf);
	va_start(ap, fmt);
	vsnprintf(&buf[used], sizeof (buf) - used, fmt, ap);
	va_end(ap);
	printf("%s", buf);
}

void
mpt2sas_cam_prt(mpt2sas_t *mpt, struct cam_path *path, int mask, const char *fmt, ...)
{
	va_list ap;
	if (mask != MP2PRT_ALL && (mask & mpt->prt_mask) == 0) {
		return;
	}
	xpt_print_path(path);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void
mpt2sas_prt_cont(mpt2sas_t *mpt, int mask, const char *fmt, ...)
{
	va_list ap;
	if (mask != MP2PRT_ALL && (mask & mpt->prt_mask) == 0) {
		return;
	}
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

int
mpt2sas_attach(mpt2sas_t *mpt)
{
	int error;

	MPT2_LOCK(mpt);
	mpt->ehook.ich_func = mpt2sas_intr_enable;
	mpt->ehook.ich_arg = mpt;
	mpt->ehook_active = 1;
	if (config_intrhook_establish(&mpt->ehook) != 0) {
		mpt->ehook_active = 0;
		return (-EIO);
	}
	callout_init(&mpt->watchdog, 1);
	MPT2_UNLOCK(mpt);
	error = mpt2sas_init(mpt);
	if (error) {
		MPT2_LOCK(mpt);
		if (mpt->ehook_active) {
			mpt->ehook_active = 0;
			config_intrhook_disestablish(&mpt->ehook);
		}
		MPT2_UNLOCK(mpt);
		return (error);
	}

	MPT2_LOCK(mpt);
	mpt2sas_cfg_sas_iounit_page1(mpt);
	MPT2_UNLOCK(mpt);

	error = mpt2sas_cam_attach(mpt);
	if (error) {
		MPT2_LOCK(mpt);
		if (mpt->ehook_active) {
			mpt->ehook_active = 0;
			config_intrhook_disestablish(&mpt->ehook);
		}
		MPT2_UNLOCK(mpt);
		return (error);
	}
	MPT2_LOCK(mpt);
	TAILQ_INSERT_TAIL(&mpt2sas_tailq, mpt, links);
	MPT2_UNLOCK(mpt);
	callout_reset(&mpt->watchdog, hz, mpt2sas_watchdog, mpt);
	return (0);
}

int
mpt2sas_detach(mpt2sas_t *mpt)
{
	int res;

	MPT2_LOCK(mpt);
	res = mpt2sas_cam_detach(mpt);
	if (res) {
		MPT2_UNLOCK(mpt);
		return (res);
	}
	if (mpt->ehook_active) {
		config_intrhook_disestablish(&mpt->ehook);
		mpt->ehook_active = 0;
	}
	if (callout_active(&mpt->watchdog))
		callout_drain(&mpt->watchdog);
	TAILQ_REMOVE(&mpt2sas_tailq, mpt, links);
	MPT2_UNLOCK(mpt);
	return (0);
}
