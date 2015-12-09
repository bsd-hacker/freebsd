/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_MCDI

#ifndef WITH_MCDI_V2
#error "WITH_MCDI_V2 required for Huntington MCDIv2 commands."
#endif

typedef enum efx_mcdi_header_type_e {
	EFX_MCDI_HEADER_TYPE_V1, /* MCDIv0 (BootROM), MCDIv1 commands */
	EFX_MCDI_HEADER_TYPE_V2, /* MCDIv2 commands */
} efx_mcdi_header_type_t;

/*
 * Return the header format to use for sending an MCDI request.
 *
 * An MCDIv1 (Siena compatible) command should use MCDIv2 encapsulation if the
 * request input buffer or response output buffer are too large for the MCDIv1
 * format. An MCDIv2 command must always be sent using MCDIv2 encapsulation.
 */
#define	EFX_MCDI_HEADER_TYPE(_cmd, _length)				\
	((((_cmd) & ~EFX_MASK32(MCDI_HEADER_CODE)) ||			\
	((_length) & ~EFX_MASK32(MCDI_HEADER_DATALEN)))	?		\
	EFX_MCDI_HEADER_TYPE_V2	: EFX_MCDI_HEADER_TYPE_V1)


/*
 * MCDI Header NOT_EPOCH flag
 * ==========================
 * A new epoch begins at initial startup or after an MC reboot, and defines when
 * the MC should reject stale MCDI requests.
 *
 * The first MCDI request sent by the host should contain NOT_EPOCH=0, and all
 * subsequent requests (until the next MC reboot) should contain NOT_EPOCH=1.
 *
 * After rebooting the MC will fail all requests with NOT_EPOCH=1 by writing a
 * response with ERROR=1 and DATALEN=0 until a request is seen with NOT_EPOCH=0.
 */


	__checkReturn	efx_rc_t
hunt_mcdi_init(
	__in		efx_nic_t *enp,
	__in		const efx_mcdi_transport_t *emtp)
{
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_dword_t dword;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);
	EFSYS_ASSERT(enp->en_features & EFX_FEATURE_MCDI_DMA);

	/* A host DMA buffer is required for Huntington MCDI */
	if (esmp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/*
	 * Ensure that the MC doorbell is in a known state before issuing MCDI
	 * commands. The recovery algorithm requires that the MC command buffer
	 * must be 256 byte aligned. See bug24769.
	 */
	if ((EFSYS_MEM_ADDR(esmp) & 0xFF) != 0) {
		rc = EINVAL;
		goto fail2;
	}
	EFX_POPULATE_DWORD_1(dword, EFX_DWORD_0, 1);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_HWRD_REG, &dword, B_FALSE);

	/* Save initial MC reboot status */
	(void) hunt_mcdi_poll_reboot(enp);

	/* Start a new epoch (allow fresh MCDI requests to succeed) */
	efx_mcdi_new_epoch(enp);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
hunt_mcdi_fini(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);

	emip->emi_new_epoch = B_FALSE;
}

			void
hunt_mcdi_request_copyin(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp,
	__in		unsigned int seq,
	__in		boolean_t ev_cpl,
	__in		boolean_t new_epoch)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_mcdi_header_type_t hdr_type;
	efx_dword_t dword;
	efx_dword_t hdr[2];
	unsigned int xflags;
	unsigned int pos;
	size_t offset;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	xflags = 0;
	if (ev_cpl)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	offset = 0;

	hdr_type = EFX_MCDI_HEADER_TYPE(emrp->emr_cmd,
	    MAX(emrp->emr_in_length, emrp->emr_out_length));

	if (hdr_type == EFX_MCDI_HEADER_TYPE_V2) {
		/* Construct MCDI v2 header */
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, MC_CMD_V2_EXTN,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, 0,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);
		EFSYS_MEM_WRITED(esmp, offset, &hdr[0]);
		offset += sizeof (efx_dword_t);

		EFX_POPULATE_DWORD_2(hdr[1],
		    MC_CMD_V2_EXTN_IN_EXTENDED_CMD, emrp->emr_cmd,
		    MC_CMD_V2_EXTN_IN_ACTUAL_LEN, emrp->emr_in_length);
		EFSYS_MEM_WRITED(esmp, offset, &hdr[1]);
		offset += sizeof (efx_dword_t);
	} else {
		/* Construct MCDI v1 header */
		EFX_POPULATE_DWORD_8(hdr[0],
		    MCDI_HEADER_CODE, emrp->emr_cmd,
		    MCDI_HEADER_RESYNC, 1,
		    MCDI_HEADER_DATALEN, emrp->emr_in_length,
		    MCDI_HEADER_SEQ, seq,
		    MCDI_HEADER_NOT_EPOCH, new_epoch ? 0 : 1,
		    MCDI_HEADER_ERROR, 0,
		    MCDI_HEADER_RESPONSE, 0,
		    MCDI_HEADER_XFLAGS, xflags);
		EFSYS_MEM_WRITED(esmp, 0, &hdr[0]);
		offset += sizeof (efx_dword_t);
	}

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context, EFX_LOG_MCDI_REQUEST,
		    &hdr, offset,
		    emrp->emr_in_buf, emrp->emr_in_length);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */

	/* Construct the payload */
	for (pos = 0; pos < emrp->emr_in_length; pos += sizeof (efx_dword_t)) {
		memcpy(&dword, MCDI_IN(*emrp, efx_dword_t, pos),
		    MIN(sizeof (dword), emrp->emr_in_length - pos));
		EFSYS_MEM_WRITED(esmp, offset + pos, &dword);
	}

	/* Ring the doorbell to post the command DMA address to the MC */
	EFSYS_ASSERT((EFSYS_MEM_ADDR(esmp) & 0xFF) == 0);

	/* Guarantee ordering of memory (MCDI request) and PIO (MC doorbell) */
	EFSYS_DMA_SYNC_FOR_DEVICE(esmp, 0, offset + emrp->emr_in_length);
	EFSYS_PIO_WRITE_BARRIER();

	EFX_POPULATE_DWORD_1(dword,
	    EFX_DWORD_0, EFSYS_MEM_ADDR(esmp) >> 32);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_LWRD_REG, &dword, B_FALSE);

	EFX_POPULATE_DWORD_1(dword,
	    EFX_DWORD_0, EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	EFX_BAR_WRITED(enp, ER_DZ_MC_DB_HWRD_REG, &dword, B_FALSE);
}

			void
hunt_mcdi_request_copyout(
	__in		efx_nic_t *enp,
	__in		efx_mcdi_req_t *emrp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_dword_t hdr[2];
	unsigned int hdr_len;
	size_t bytes;

	if (emrp->emr_out_buf == NULL)
		return;

	/* Read the command header to detect MCDI response format */
	hdr_len = sizeof (hdr[0]);
	hunt_mcdi_read_response(enp, &hdr[0], 0, hdr_len);
	if (EFX_DWORD_FIELD(hdr[0], MCDI_HEADER_CODE) == MC_CMD_V2_EXTN) {
		/*
		 * Read the actual payload length. The length given in the event
		 * is only correct for responses with the V1 format.
		 */
		hunt_mcdi_read_response(enp, &hdr[1], hdr_len, sizeof (hdr[1]));
		hdr_len += sizeof (hdr[1]);

		emrp->emr_out_length_used = EFX_DWORD_FIELD(hdr[1],
					    MC_CMD_V2_EXTN_IN_ACTUAL_LEN);
	}

	/* Copy payload out into caller supplied buffer */
	bytes = MIN(emrp->emr_out_length_used, emrp->emr_out_length);
	hunt_mcdi_read_response(enp, emrp->emr_out_buf, hdr_len, bytes);

#if EFSYS_OPT_MCDI_LOGGING
	if (emtp->emt_logger != NULL) {
		emtp->emt_logger(emtp->emt_context,
		    EFX_LOG_MCDI_RESPONSE,
		    &hdr, hdr_len,
		    emrp->emr_out_buf, bytes);
	}
#endif /* EFSYS_OPT_MCDI_LOGGING */
}

static	__checkReturn	boolean_t
hunt_mcdi_poll_response(
	__in		efx_nic_t *enp)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	efx_dword_t hdr;

	EFSYS_MEM_READD(esmp, 0, &hdr);
	return (EFX_DWORD_FIELD(hdr, MCDI_HEADER_RESPONSE) ? B_TRUE : B_FALSE);
}

			void
hunt_mcdi_read_response(
	__in		efx_nic_t *enp,
	__out		void *bufferp,
	__in		size_t offset,
	__in		size_t length)
{
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
	efsys_mem_t *esmp = emtp->emt_dma_mem;
	unsigned int pos;
	efx_dword_t data;

	for (pos = 0; pos < length; pos += sizeof (efx_dword_t)) {
		EFSYS_MEM_READD(esmp, offset + pos, &data);
		memcpy((uint8_t *)bufferp + pos, &data,
		    MIN(sizeof (data), length - pos));
	}
}

	__checkReturn	boolean_t
hunt_mcdi_request_poll(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_MCDI_LOGGING
	const efx_mcdi_transport_t *emtp = enp->en_mcdi.em_emtp;
#endif /* EFSYS_OPT_MCDI_LOGGING */
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_mcdi_req_t *emrp;
	int state;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/* Serialise against post-watchdog efx_mcdi_ev* */
	EFSYS_LOCK(enp->en_eslp, state);

	EFSYS_ASSERT(emip->emi_pending_req != NULL);
	EFSYS_ASSERT(!emip->emi_ev_cpl);
	emrp = emip->emi_pending_req;

	/* Check if a response is available */
	if (hunt_mcdi_poll_response(enp) == B_FALSE) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		return (B_FALSE);
	}

	/* Read the response header */
	efx_mcdi_read_response_header(enp, emrp);

	/* Request complete */
	emip->emi_pending_req = NULL;

	/* Ensure stale MCDI requests fail after an MC reboot. */
	emip->emi_new_epoch = B_FALSE;

	EFSYS_UNLOCK(enp->en_eslp, state);

	if ((rc = emrp->emr_rc) != 0)
		goto fail1;

	hunt_mcdi_request_copyout(enp, emrp);
	goto out;

fail1:
	if (!emrp->emr_quiet)
		EFSYS_PROBE1(fail1, efx_rc_t, rc);

	/* Reboot/Assertion */
	if (rc == EIO || rc == EINTR)
		efx_mcdi_raise_exception(enp, emrp, rc);

out:
	return (B_TRUE);
}

			efx_rc_t
hunt_mcdi_poll_reboot(
	__in		efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_dword_t dword;
	uint32_t old_status;
	uint32_t new_status;
	efx_rc_t rc;

	old_status = emip->emi_mc_reboot_status;

	/* Update MC reboot status word */
	EFX_BAR_TBL_READD(enp, ER_DZ_BIU_MC_SFT_STATUS_REG, 0, &dword, B_FALSE);
	new_status = dword.ed_u32[0];

	/* MC has rebooted if the value has changed */
	if (new_status != old_status) {
		emip->emi_mc_reboot_status = new_status;

		/*
		 * FIXME: Ignore detected MC REBOOT for now.
		 *
		 * The Siena support for checking for MC reboot from status
		 * flags is broken - see comments in siena_mcdi_poll_reboot().
		 * As the generic MCDI code is shared the Huntington reboot
		 * detection suffers similar problems.
		 *
		 * Do not report an error when the boot status changes until
		 * this can be handled by common code drivers (and reworked to
		 * support Siena too).
		 */
		if (B_FALSE) {
			rc = EIO;
			goto fail1;
		}
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
hunt_mcdi_fw_update_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * Use privilege mask state at MCDI attach.
	 * Admin privilege must be used prior to introduction of
	 * specific flag.
	 */
	*supportedp = (encp->enc_privilege_mask &
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN)
	    == MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN;

	return (0);
}

	__checkReturn	efx_rc_t
hunt_mcdi_macaddr_change_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t privilege_mask = encp->enc_privilege_mask;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * Use privilege mask state at MCDI attach.
	 * Admin privilege must be used prior to introduction of
	 * mac spoofing privilege (at v4.6), which is used up to
	 * introduction of change mac spoofing privilege (at v4.7)
	 */
	*supportedp =
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_CHANGE_MAC) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_CHANGE_MAC) ||
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING) ||
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN);

	return (0);
}

	__checkReturn	efx_rc_t
hunt_mcdi_mac_spoofing_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t privilege_mask = encp->enc_privilege_mask;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * Use privilege mask state at MCDI attach.
	 * Admin privilege must be used prior to introduction of
	 * mac spoofing privilege (at v4.6), which is used up to
	 * introduction of mac spoofing TX privilege (at v4.7)
	 */
	*supportedp =
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING_TX) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING_TX) ||
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_MAC_SPOOFING) ||
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN);

	return (0);
}


	__checkReturn	efx_rc_t
hunt_mcdi_link_control_supported(
	__in		efx_nic_t *enp,
	__out		boolean_t *supportedp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t privilege_mask = encp->enc_privilege_mask;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_HUNTINGTON);

	/*
	 * Use privilege mask state at MCDI attach.
	 * Admin privilege used prior to introduction of
	 * specific flag.
	 */
	*supportedp =
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_LINK) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_LINK) ||
	    ((privilege_mask & MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN) ==
	    MC_CMD_PRIVILEGE_MASK_IN_GRP_ADMIN);

	return (0);
}

#endif	/* EFSYS_OPT_MCDI */

#endif	/* EFSYS_OPT_HUNTINGTON */
