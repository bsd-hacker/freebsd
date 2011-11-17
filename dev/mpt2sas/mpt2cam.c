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

static void mpt2sas_action(struct cam_sim *, union ccb *);
static void mpt2sas_poll(struct cam_sim *);

int
mpt2sas_cam_attach(mpt2sas_t *mpt)
{
	struct cam_devq *devq;
	int maxq, error;

	/*
	 * Max requests already reflects global credits.
	 *
	 * We reserve a number of requests for both task management and async handling
	 */
	maxq = MPT2_MAX_REQUESTS(mpt) - 50;	/* XXXX SWAG XXXX */

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(maxq);
	if (devq == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Unable to allocate CAM SIMQ!\n");
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Construct our SIM entry.
	 */
	mpt->sim = cam_sim_alloc(mpt2sas_action, mpt2sas_poll, "mpt2sas", mpt, device_get_unit(mpt->dev), &mpt->lock, 1, maxq, devq);
	if (mpt->sim == NULL) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Unable to allocate CAM sim\n");
		cam_simq_free(devq);
		error = ENOMEM;
		goto cleanup;
	}

	/*
	 * Register exactly this bus.
	 */
	MPT2_LOCK(mpt);
	if (xpt_bus_register(mpt->sim, mpt->dev, 0) != CAM_SUCCESS) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Bus registration Failed\n");
		error = ENOMEM;
		MPT2_UNLOCK(mpt);
		goto cleanup;
	}

	if (xpt_create_path(&mpt->path, NULL, cam_sim_path(mpt->sim), CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "Unable to allocate Path!\n");
		error = ENOMEM;
		MPT2_UNLOCK(mpt);
		goto cleanup;
	}

	MPT2_UNLOCK(mpt);
	return (0);

cleanup:
	mpt2sas_cam_detach(mpt);
	return (error);
}


/*
 * Should be called locked
 */
int
mpt2sas_cam_detach(mpt2sas_t *mpt)
{
	if (mpt->sim == NULL)
		return (ENODEV);
	if (mpt->sim->refcount > 2)
		return (EBUSY);
	xpt_free_path(mpt->path);
	xpt_bus_deregister(cam_sim_path(mpt->sim));
	cam_sim_free(mpt->sim, TRUE);
	mpt->sim = NULL;
	return (0);
}

/* This routine is used after a system crash to dump core onto the swap device.
 */
static void
mpt2sas_poll(struct cam_sim *sim)
{
	mpt2sas_t *mpt;
	mpt = (mpt2sas_t *)cam_sim_softc(sim);
	mpt2sas_intr(mpt);
}

/*
 * Callback routine from "bus_dmamap_load" or, in simple cases, called directly.
 *
 * Takes a list of physical segments and builds the SGL for SCSI IO command
 * and forwards the commard to the IOC after one last check that CAM has not
 * aborted the transaction.
 */
static void
mpt2sas_exec(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	MPI2_SCSI_IO_REQUEST *rqs;
	MPI2_SGE_CHAIN64 *cp;
	request_t *req;
	union ccb *ccb;
	mpt2sas_dma_chunk_t *cl, *lw;
	mpt2sas_t *mpt;
	sas_dev_t *dp;
	uint32_t flags;
	int seg, lc, cur, nc, cn;
	bus_dmasync_op_t op;

	req = (request_t *)arg;
	ccb = req->ccb;
	mpt = ccb->ccb_h.ccb_mpt2sas_ptr;
	dp = mpt2_tgt2dev(mpt, ccb->ccb_h.target_id);
	rqs = MPT2_REQ2RQS(mpt, req);

bad:
	if (error != 0) {
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
			cam_status status;
			mpt2sas_freeze_ccb(ccb);
			if (error == EFBIG) {
				status = CAM_REQ_TOO_BIG;
			} else if (error == ENOMEM) {
				if (mpt->outofbeer == 0) {
					mpt->outofbeer = 1;
					xpt_freeze_simq(mpt->sim, 1);
				}
				status = CAM_REQUEUE_REQ;
			} else {
				status = CAM_REQ_CMP_ERR;
			}
			mpt2sas_set_ccb_status(ccb, status);
		}
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		xpt_done(ccb);
		mpt2sas_free_request(mpt, req);
		return;
	}

	if (nseg == 0) {
		rqs->SGL.MpiSimple.FlagsLength = htole32(ZERO_LENGTH_SGE);
		goto out;
	}
	rqs->SGLOffset0 = offsetof(MPI2_SCSI_IO_REQUEST, SGL) >> 2;

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		op = BUS_DMASYNC_PREREAD;
		flags = MPI2_SGE_FLAGS_IOC_TO_HOST;
	} else {
		flags = MPI2_SGE_FLAGS_HOST_TO_IOC;
		op = BUS_DMASYNC_PREWRITE;
	}

	if ((ccb->ccb_h.flags & (CAM_SG_LIST_PHYS|CAM_DATA_PHYS)) == 0) {
		bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
	}
	if (nseg == 1) {
		rqs->DataLength = htole32(segs->ds_len);
		mpt2sas_single_sge((MPI2_SGE_SIMPLE_UNION *)&rqs->SGL, segs->ds_addr, segs->ds_len, flags|SINGLE_SGE);
		goto out;
	}

	rqs->ChainOffset = offsetof(MPI2_SCSI_IO_REQUEST, SGL) >> 2;
	seg = 0;
	cl = NULL;
	seg = 0;
	nc = 1;	/* to account for the chain pointer in the initial request */
	lc = 0;
	while (seg < nseg) {
		mpt2sas_dma_chunk_t *x = mpt->dma_chunk_free;
		if (x == NULL) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "Out of DMA Chunks\n");
			break;
		}
		memset(x->segs, 0, mpt->sge_per_chunk * (sizeof (MPI2_MPI_SGE_IO_UNION)));
		mpt->dma_chunk_free = x->linkage;
		x->linkage = cl;
		cl = x;

		/*
		 * Calculate the current number of actual data describing segments in this chunk.
		 * We then get, for free, the number of data segments in the *last* chunk.
		 */
		lc = mpt->sge_per_chunk;
		seg += lc;
		if (seg < nseg) {
			seg--;
			lc--;
			nc++;
		} else if (seg > mpt->sge_per_chunk && (seg % mpt->sge_per_chunk) == 0) {
			lc = mpt->sge_per_chunk;
		} else {
			lc = mpt->sge_per_chunk - (seg - nseg);
		}
	}
	if (nc >= mpt->ioc_facts.MaxChainDepth || seg < nseg) {
		while (cl) {
			lw = cl->linkage;
			cl->linkage = mpt->dma_chunk_free;
			mpt->dma_chunk_free = cl;
			cl = lw;
		}
		if (nc > mpt->ioc_facts.MaxChainDepth) {
			error = EFBIG;
		} else {
			error = ENOMEM;
		}
		goto bad;
	}

	lw = cl;
	cn = cur = seg = 0;
	while (seg < nseg) {
		uint32_t cflags;

		rqs->DataLength += segs[seg].ds_len;

		/*
		 * Is this one the last one, period?
		 */
		if (seg == nseg-1) {
			mpt2sas_single_sge(&lw->segs[cur].u.Simple, segs[seg].ds_addr, segs[seg].ds_len, flags|SINGLE_SGE);
			seg++;
			continue;
		}

		cflags = flags|MPI2_SGE_FLAGS_SIMPLE_ELEMENT;

		if (lw->linkage) {
			/*
			 * Are we the last actual SGE in *this* chain?
			 */
			if (cur == mpt->sge_per_chunk - 2) {
				cflags |= MPI2_SGE_FLAGS_END_OF_LIST;
			}
		}
		mpt2sas_single_sge(&lw->segs[cur++].u.Simple, segs[seg].ds_addr, segs[seg].ds_len, cflags);
		if (cflags & MPI2_SGE_FLAGS_END_OF_LIST) {
			cp = (MPI2_SGE_CHAIN64 *)&lw->segs[cur];
			cp->Flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT|MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
			if (lw->linkage && lw->linkage->linkage) {
				cp->NextChainOffset = ((uint8_t *)&lw->segs[mpt->sge_per_chunk-1] - (uint8_t *)lw->segs) >> 2;
				cp->Length = htole16(mpt->sge_per_chunk * (sizeof (MPI2_MPI_SGE_IO_UNION)));
			} else {
				cp->Length = htole16(lc * (sizeof (MPI2_MPI_SGE_IO_UNION)));
			}
			cp->Address = htole64(MPT2_CHUNK2PADDR(mpt, lw->linkage));
			cn++;
			lw = lw->linkage;
			cur = 0;
		}
		seg++;
	}
	/*
	 * Fill in initial Chain SGE
	 */
	cp = (MPI2_SGE_CHAIN64 *)&rqs->SGL;
	cp->Flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT|MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
	if (nc > 1) {
		cp->NextChainOffset = ((uint8_t *)&lw->segs[mpt->sge_per_chunk-1] - (uint8_t *)lw->segs) >> 2;
		cp->Length = htole16(mpt->sge_per_chunk * (sizeof (MPI2_MPI_SGE_IO_UNION)));
	} else {
		cp->NextChainOffset = 0;
		cp->Length = htole16(seg * (sizeof (MPI2_MPI_SGE_IO_UNION)));
	}
	cp->Address = htole64(MPT2_CHUNK2PADDR(mpt, cl));
	req->chain = cl;
out:
	dp->active++;
	req->timeout = ccb->ccb_h.timeout;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	mpt2sas_cam_prt(mpt, ccb->ccb_h.path, MP2PRT_CFLOW0, "%s: cmd cdb[0]=0x%x datalen %u\n", __func__, rqs->CDB.CDB32[0], rqs->DataLength);
	mpt2sas_send_cmd(mpt, req);
}

static void
mpt2sas_start(struct cam_sim *sim, union ccb *ccb)
{
	request_t *req;
	mpt2sas_t *mpt;
	MPI2_SCSI_IO_REQUEST *rqs;
	struct ccb_scsiio *csio = &ccb->csio;
	struct ccb_hdr *ccbh = &ccb->ccb_h;
	sas_dev_t *dp;
	int error;

	mpt = ccb->ccb_h.ccb_mpt2sas_ptr;

	dp = mpt2_tgt2dev(mpt, ccb->ccb_h.target_id);
	if (dp == NULL || dp->state != STABLE) {
		mpt2sas_set_ccb_status(ccb, CAM_SEL_TIMEOUT);
		xpt_done(ccb);
		return;
	}

	if (dp->set_qfull) {
		dp->set_qfull = 0;
		mpt2sas_freeze_ccb(ccb);
		ccb->csio.scsi_status = SCSI_STATUS_QUEUE_FULL;
		mpt2sas_set_ccb_status(ccb, CAM_SCSI_STATUS_ERROR);
		xpt_done(ccb);
		return;
	}
	if (dp->qdepth && dp->active >= dp->qdepth) {
		mpt2sas_freeze_ccb(ccb);
		ccb->csio.scsi_status = SCSI_STATUS_QUEUE_FULL;
		mpt2sas_set_ccb_status(ccb, CAM_SCSI_STATUS_ERROR);
		xpt_done(ccb);
		return;
	}

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		if (mpt->outofbeer == 0) {
			mpt->outofbeer = 1;
			xpt_freeze_simq(mpt->sim, 1);
		}
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}
	req->handle = dp->AttachedDevHandle;

	/*
	 * Link the ccb and the request structure so we can find
	 * the other knowing either the request or the ccb
	 */
	req->ccb = ccb;
	ccb->ccb_h.ccb_req_ptr = req;

	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0, sizeof (*rqs));
	rqs->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	rqs->DevHandle = htole16(dp->AttachedDevHandle);
	if (ccb->ccb_h.target_lun >= 256) {
		rqs->LUN[0] = 0x40 | ((ccb->ccb_h.target_lun >> 8) & 0x3f);
		rqs->LUN[1] = ccb->ccb_h.target_lun & 0xff;
	} else {
		rqs->LUN[1] = ccb->ccb_h.target_lun;
	}

	/* Set the direction of the transfer */
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		rqs->Control = MPI2_SCSIIO_CONTROL_READ;
	} else if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
		rqs->Control = MPI2_SCSIIO_CONTROL_WRITE;
	} else {
		rqs->Control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;
	}

	if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0) {
		switch(ccb->csio.tag_action) {
		case MSG_HEAD_OF_Q_TAG:
			rqs->Control |= MPI2_SCSIIO_CONTROL_HEADOFQ;
			break;
		case MSG_ACA_TASK:
			rqs->Control |= MPI2_SCSIIO_CONTROL_ACAQ;
			break;
		case MSG_ORDERED_Q_TAG:
			rqs->Control |= MPI2_SCSIIO_CONTROL_ORDEREDQ;
			break;
		case MSG_SIMPLE_Q_TAG:
		default:
			rqs->Control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
			break;
		}
	} else {
		rqs->Control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
	}

	rqs->Control = htole32(rqs->Control);
	rqs->IoFlags = htole16(csio->cdb_len);

	/* Copy the scsi command block into place */
	if (ccb->ccb_h.flags & CAM_CDB_POINTER) {
		memcpy(rqs->CDB.CDB32, csio->cdb_io.cdb_ptr, csio->cdb_len);
	} else {
		memcpy(rqs->CDB.CDB32, csio->cdb_io.cdb_bytes, csio->cdb_len);
	}

	rqs->SenseBufferLowAddress = htole32(mpt->sense.paddr + (MPT2_REQ2SMID(mpt, req) * MPT2_SENSE_SIZE));
	rqs->SenseBufferLength = MPT2_USABLE_SENSE_SIZE;

	/*
	 * If we have any data to send with this command map it into bus space.
	 */
	if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if ((ccbh->flags & CAM_SCATTER_VALID) == 0) {
			/*
			 * We've been given a pointer to a single buffer.
			 */
			if ((ccbh->flags & CAM_DATA_PHYS) == 0) {
				error = bus_dmamap_load(mpt->buffer_dmat, req->dmap, csio->data_ptr, csio->dxfer_len, mpt2sas_exec, req, 0);
				if (error == EINPROGRESS) {
					xpt_freeze_simq(mpt->sim, 1);
					ccbh->status |= CAM_RELEASE_SIMQ;
				}
			} else {
				struct bus_dma_segment seg;
				seg.ds_addr = (bus_addr_t)(vm_offset_t)csio->data_ptr;
				seg.ds_len = csio->dxfer_len;
				mpt2sas_exec(req, &seg, 1, 0);
			}
		} else {
			if ((ccbh->flags & CAM_SG_LIST_PHYS) == 0) {
				mpt2sas_exec(req, NULL, 0, EFAULT);
			} else {
				struct bus_dma_segment *segs;
				segs = (struct bus_dma_segment *)csio->data_ptr;
				mpt2sas_exec(req, segs, csio->sglist_cnt, 0);
			}
		}
	} else {
		mpt2sas_exec(req, NULL, 0, 0);
	}
}

static void
mpt2sas_parse_reply(mpt2sas_t *mpt, request_t *req, MPI2_SCSI_IO_REPLY *srf)
{
	union ccb *ccb;
	U16 ioc_status;
	U8 sstate;

	ioc_status = le16toh(srf->IOCStatus);
	ioc_status &= MPI2_IOCSTATUS_MASK;
	sstate = srf->SCSIState;

	ccb = req->ccb;
	ccb->csio.resid = ccb->csio.dxfer_len - le32toh(srf->TransferCount);
	if ((sstate & MPI2_SCSI_STATE_AUTOSENSE_VALID) && (ccb->ccb_h.flags & (CAM_SENSE_PHYS|CAM_SENSE_PTR)) == 0) {
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
		ccb->csio.sense_resid = ccb->csio.sense_len - le32toh(srf->SenseCount);
		bus_dmamap_sync(mpt->sense.dmat, mpt->sense.dmap, BUS_DMASYNC_POSTREAD);
		memcpy(&ccb->csio.sense_data, MPT2_REQ2SNS(mpt, req), min(ccb->csio.sense_len, le32toh(srf->SenseCount)));
	}

	switch(ioc_status) {
	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		/*
		 * XXX
		 * Linux driver indicates that a zero
		 * transfer length with this error code
		 * indicates a CRC error.
		 *
		 * No need to swap the bytes for checking
		 * against zero.
		 */
		if (srf->TransferCount == 0) {
			mpt2sas_set_ccb_status(ccb, CAM_UNCOR_PARITY);
			break;
		}
		/* FALLTHROUGH */
	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		mpt2sas_prt(mpt, MP2PRT_UNDERRUN, "underrun for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		/* FALLTHROUGH */
	case MPI2_IOCSTATUS_SUCCESS:
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
		if (ioc_status == MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR) {
			mpt2sas_prt(mpt, MP2PRT_WARN, "recovered error for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		}
		if ((sstate & MPI2_SCSI_STATE_NO_SCSI_STATUS) != 0) {
			/*
			 * Status was never returned for this transaction.
			 */
			mpt2sas_set_ccb_status(ccb, CAM_UNEXP_BUSFREE);
			mpt2sas_prt(mpt, MP2PRT_ERR, "No SCSI Status for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		} else if (srf->SCSIStatus != SCSI_STATUS_OK) {
			ccb->csio.scsi_status = srf->SCSIStatus;
			if ((sstate & MPI2_SCSI_STATE_AUTOSENSE_FAILED) != 0) {
				mpt2sas_set_ccb_status(ccb, CAM_AUTOSENSE_FAIL);
				mpt2sas_prt(mpt, MP2PRT_ERR, "AUTOSENSE Failed for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
			} else {
				mpt2sas_set_ccb_status(ccb, CAM_SCSI_STATUS_ERROR);
				mpt2sas_prt(mpt, MP2PRT_INFO, "SCSI Status 0x%x for CDB[0]=0x%02x\n", srf->SCSIStatus, ccb->csio.cdb_io.cdb_bytes[0]);
			}
		} else if ((sstate & MPI2_SCSI_STATE_RESPONSE_INFO_VALID) != 0) {
			mpt2sas_prt(mpt, MP2PRT_ERR, "AUTOSENSE Failed for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
			mpt2sas_set_ccb_status(ccb, CAM_REQ_CMP_ERR);
		} else {
			mpt2sas_set_ccb_status(ccb, CAM_REQ_CMP);
		}
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
		mpt2sas_prt(mpt, MP2PRT_ERR, "Data Overrun error for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		mpt2sas_set_ccb_status(ccb, CAM_DATA_RUN_ERR);
		break;
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
		mpt2sas_prt(mpt, MP2PRT_ERR, "Data I/O error for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		mpt2sas_set_ccb_status(ccb, CAM_UNCOR_PARITY);
		break;
	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		/*
		 * Since selection timeouts and "device really not
		 * there" are grouped into this error code, report
		 * selection timeout.  Selection timeouts are
		 * typically retried before giving up on the device
		 * whereas "device not there" errors are considered
		 * unretryable.
		 */
		mpt2sas_prt(mpt, MP2PRT_INFO, "Device Not There CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		mpt2sas_set_ccb_status(ccb, CAM_SEL_TIMEOUT);
		break;
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		mpt2sas_prt(mpt, MP2PRT_ERR, "Protocol Error for CDB[0]=0x%02x\n", ccb->csio.cdb_io.cdb_bytes[0]);
		mpt2sas_set_ccb_status(ccb, CAM_SEQUENCE_FAIL);
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		mpt2sas_prt(mpt, MP2PRT_ERR, "Task Managment failed\n");
		ccb->ccb_h.status = CAM_UA_TERMIO;
		break;
	case MPI2_IOCSTATUS_INVALID_STATE:
		/*
		 * The IOC has been reset.  Emulate a bus reset.
		 */
		/* FALLTHROUGH */
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		ccb->ccb_h.status = CAM_SCSI_BUS_RESET; 
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
		/*
		 * Don't clobber any timeout status that has
		 * already been set for this transaction.  We
		 * want the SCSI layer to be able to differentiate
		 * between the command we aborted due to timeout
		 * and any innocent bystanders.
		 */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			break;
		}
		mpt2sas_prt(mpt, MP2PRT_INFO, "Task Terminated\n");
		mpt2sas_set_ccb_status(ccb, CAM_REQ_TERMIO);
		break;

	case MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES:
		mpt2sas_prt(mpt, MP2PRT_WARN, "Insufficient Resources\n");
		mpt2sas_set_ccb_status(ccb, CAM_RESRC_UNAVAIL);
		break;
	case MPI2_IOCSTATUS_BUSY:
		mpt2sas_prt(mpt, MP2PRT_WARN, "IOC Busy\n");
		mpt2sas_set_ccb_status(ccb, CAM_BUSY);
		break;
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
	case MPI2_IOCSTATUS_INVALID_SGL:
	case MPI2_IOCSTATUS_INTERNAL_ERROR:
	case MPI2_IOCSTATUS_INVALID_FIELD:
	default:
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: IOC_STATUS 0x%x sstate 0x%x CDB[0]=0x%02x\n", __func__, ioc_status, sstate, ccb->csio.cdb_io.cdb_bytes[0]);
		ccb->ccb_h.status = CAM_UNREC_HBA_ERROR;
		break;
	}
}


void
mpt2sas_cam_done(mpt2sas_t *mpt, request_t *req, MPI2_SCSI_IO_REPLY *srf)
{
	MPI2_SCSI_IO_REQUEST *scsi_req;
	sas_dev_t *dp;
	union ccb *ccb;

	KASSERT((req->state & REQ_STATE_FREE) == 0, ("%s: request state already free", __func__));

	ccb = req->ccb;
	KASSERT(ccb, ("%s: null CCB", __func__));

	dp = mpt2_tgt2dev(mpt, ccb->ccb_h.target_id);
	KASSERT(dp, ("%s: null dp", __func__));

	scsi_req = (MPI2_SCSI_IO_REQUEST *) MPT2_REQ2RQS(mpt, req);

	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(mpt->buffer_dmat, req->dmap, op);
		bus_dmamap_unload(mpt->buffer_dmat, req->dmap);
	}

	if (srf == NULL) {
		ccb->csio.resid = 0;
		if (dp->set_qfull) {
			dp->set_qfull = 0;
			mpt2sas_set_ccb_status(ccb, CAM_SCSI_STATUS_ERROR);
			ccb->csio.scsi_status = SCSI_STATUS_QUEUE_FULL;
		} else {
			mpt2sas_set_ccb_status(ccb, CAM_REQ_CMP);
			ccb->csio.scsi_status = SCSI_STATUS_OK;
		}
	} else {
		mpt2sas_parse_reply(mpt, req, srf);
	}

	if (mpt->outofbeer) {
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		mpt->outofbeer = 0;
	}
	if (dp->active) {
		dp->active--;
	}
	if (dp->active == 0 && dp->destroy_needed) {
		mpt2sas_prt(mpt, MP2PRT_ERR, "%s: DevHandle 0x%x dying, active count now %d\n", __func__, dp->AttachedDevHandle, dp->active);
		mpt2sas_destroy_dev_part2(dp);
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		mpt2sas_freeze_ccb(ccb);
	}
	xpt_done(ccb);
	if ((req->state & (REQ_STATE_LOCKED|REQ_STATE_POLLED)) == 0) {
		mpt2sas_free_request(mpt, req);
	}
}

static void
mpt2sas_action(struct cam_sim *sim, union ccb *ccb)
{
	mpt2sas_t *mpt;
	struct ccb_trans_settings *cts;
	target_id_t tgt;
	lun_id_t lun;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("%s\n", __func__));
	mpt = (mpt2sas_t *)cam_sim_softc(sim);
	tgt = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;
	ccb->ccb_h.ccb_mpt2sas_ptr = mpt;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		if (mpt->portenabled == 0) {
			mpt2sas_send_port_enable(mpt);
			if (mpt->portenabled == 0) {
				ccb->ccb_h.status = CAM_UNREC_HBA_ERROR;
				break;
			}
		}
		/*
		 * We don't support external CDBs
		 */
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) && (ccb->ccb_h.flags & CAM_CDB_PHYS)) {
			mpt2sas_prt(mpt, MP2PRT_WARN, "CAM_CDB_POINTER or CAM_CDB_PHYS commands not supported\n");
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		/*
		 * We don't support CDBs larger than we can stuff into a single SGE
		 */
		if (ccb->csio.cdb_len > sizeof (MPI2_SCSI_IO_CDB_UNION)) {
			mpt2sas_prt(mpt, MP2PRT_WARN, "cdb length %u not supported\n", ccb->csio.cdb_len);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		mpt2sas_start(sim, ccb);
		return;

	case XPT_RESET_DEV:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;

	case XPT_SET_TRAN_SETTINGS:
		cts = &ccb->cts;
		if (cts->type != CTS_TYPE_CURRENT_SETTINGS) {
			mpt2sas_prt(mpt, MP2PRT_WARN, "XPT_SET_TRAN_SETTING type no current (%x)\n", cts->type);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_sas *sas;

		cts = &ccb->cts;
		scsi = &cts->proto_specific.scsi;
		sas = &cts->xport_specific.sas;
		tgt = cts->ccb_h.target_id;

		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_2;
		cts->transport = XPORT_SAS;
		cts->transport_version = 0;

		scsi->valid = CTS_SCSI_VALID_TQ;
		scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
		sas->valid = CTS_SAS_VALID_SPEED;
		sas->bitrate = 300000;
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_CALC_GEOMETRY:
		cam_calc_geometry(&ccb->ccg, 1);
		break;

	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->target_sprt = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = mpt->ioc_facts.MaxTargets;
		cpi->max_lun = MPT2_MAX_LUN;
		cpi->initiator_id = ~0;
		cpi->bus_id = cam_sim_bus(sim);

		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->base_transfer_speed = 300000;
		cpi->hba_inquiry = PI_TAG_ABLE;
		cpi->transport = XPORT_SAS;
		cpi->transport_version = 0;
		cpi->protocol_version = SCSI_REV_SPC2;

		cpi->target_sprt = 0;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "LSI", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	default:
		mpt2sas_prt(mpt, MP2PRT_WARN, "%s: unsupported function code %x\n", __func__, ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	xpt_done(ccb);
}

int
mpt2sas_run_scsicmd(mpt2sas_t *mpt, U16 handle, U8 *cdbp, int cdblen, bus_addr_t paddr, bus_size_t len, boolean_t iswrite)
{
	MPI2_SCSI_IO_REQUEST *rqs;
	request_t *req;
	int error;
 
	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
 	}
	req->handle = handle;
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0, sizeof (MPI2_SCSI_IO_REQUEST));
	rqs->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	rqs->DevHandle = htole16(handle);
	rqs->Control = MPI2_SCSIIO_CONTROL_SIMPLEQ;
	if (paddr) {
		int f;
		if (iswrite == TRUE) {
			rqs->Control |= MPI2_SCSIIO_CONTROL_WRITE;
			f = MPI2_SGE_FLAGS_HOST_TO_IOC;
		} else {
			rqs->Control |= MPI2_SCSIIO_CONTROL_READ;
			f = MPI2_SGE_FLAGS_IOC_TO_HOST;
		}
		rqs->SGLOffset0 = offsetof(MPI2_SCSI_IO_REQUEST, SGL) >> 2;
		rqs->DataLength = htole32(len);
		mpt2sas_single_sge((MPI2_SGE_SIMPLE_UNION *)&rqs->SGL, paddr, len, f|SINGLE_SGE);
	} else {
		rqs->SGL.MpiSimple.FlagsLength = htole32(ZERO_LENGTH_SGE);
	}
	rqs->Control = htole32(rqs->Control);
	rqs->IoFlags = htole16(cdblen);
	memcpy(rqs->CDB.CDB32, cdbp, cdblen);
	rqs->SenseBufferLowAddress = htole32(mpt->sense.paddr + (MPT2_REQ2SMID(mpt, req) * MPT2_SENSE_SIZE));
	rqs->SenseBufferLength = MPT2_USABLE_SENSE_SIZE;
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 10000);
	MPT2SAS_SYNC_ERR(mpt, error);
}

static int
mpt2sas_tmf(mpt2sas_t *mpt, lun_id_t lun, U16 handle, U8 tasktype, U8 msgflags, U16 mid)
{
	MPI2_SCSI_TASK_MANAGE_REQUEST *rqs;
	request_t *req;
	int error;

	MPT2SAS_GET_REQUEST(mpt, req);
	if (req == NULL) {
		return (ENOMEM);
	}
	req->flags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	rqs = MPT2_REQ2RQS(mpt, req);
	memset(rqs, 0, sizeof (MPI2_CONFIG_REQUEST));
	rqs->DevHandle = htole16(handle);
	rqs->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	rqs->TaskType = tasktype;
	rqs->MsgFlags = msgflags;
	if (tasktype == MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK || tasktype == MPI2_SCSITASKMGMT_TASKTYPE_QUERY_TASK) {
		rqs->TaskMID = htole16(mid);
	}
	if (lun >= 256) {
		rqs->LUN[0] = 0x40 | ((lun >> 8) & 0x3f);
		rqs->LUN[1] = lun & 0xff;
	} else {
		rqs->LUN[1] = lun;
	}
	mpt2sas_send_cmd(mpt, req);
	error = mpt2sas_wait_req(mpt, req, REQ_STATE_DONE, REQ_STATE_DONE, 1000);
	MPT2SAS_SYNC_ERR(mpt, error);
}

int
mpt2sas_scsi_abort(mpt2sas_t *mpt, request_t *req)
{
	sas_dev_t *dp;
	int error;

	dp = mpt2_tgt2dev(mpt, req->ccb->ccb_h.target_id);
	if (dp == NULL) {
		return (ENODEV);
	}
	error = mpt2sas_tmf(mpt, req->ccb->ccb_h.target_lun, dp->AttachedDevHandle, MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK, 0, MPT2_RQS2SMID(mpt, MPT2_REQ2RQS(mpt, req)));
	return (error);
}

/*
 * Called unlocked
 */
int
mpt2sas_cam_rescan(mpt2sas_t *mpt)
{
	union ccb *ccb;
	uint32_t pathid;

	pathid = cam_sim_path(mpt->sim);
	/*
	 * Allocate a CCB, create a wildcard path for this bus,
	 * and schedule a rescan.
	 */
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		return (ENOMEM);
	}
	if (xpt_create_path(&ccb->ccb_h.path, xpt_periph, pathid, CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		return (ENOMEM);
	}
	xpt_rescan(ccb);
	return (0);
}

