/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
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
#include "efx_types.h"
#include "efx_regs_mcdi.h"
#include "efx_impl.h"

#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_FILTER

#define	HFE_SPEC(hftp, index)	((hftp)->hft_entry[(index)].hfe_spec)

static			efx_filter_spec_t *
hunt_filter_entry_spec(
	__in		const hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	return ((efx_filter_spec_t *)(HFE_SPEC(hftp, index) &
		~(uintptr_t)EFX_HUNT_FILTER_FLAGS));
}

static			boolean_t
hunt_filter_entry_is_busy(
	__in		const hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	if (HFE_SPEC(hftp, index) & EFX_HUNT_FILTER_FLAG_BUSY)
		return (B_TRUE);
	else
		return (B_FALSE);
}

static			boolean_t
hunt_filter_entry_is_auto_old(
	__in		const hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	if (HFE_SPEC(hftp, index) & EFX_HUNT_FILTER_FLAG_AUTO_OLD)
		return (B_TRUE);
	else
		return (B_FALSE);
}

static			void
hunt_filter_set_entry(
	__inout		hunt_filter_table_t *hftp,
	__in		unsigned int index,
	__in_opt	const efx_filter_spec_t *efsp)
{
	HFE_SPEC(hftp, index) = (uintptr_t)efsp;
}

static			void
hunt_filter_set_entry_busy(
	__inout		hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	HFE_SPEC(hftp, index) |= (uintptr_t)EFX_HUNT_FILTER_FLAG_BUSY;
}

static			void
hunt_filter_set_entry_not_busy(
	__inout		hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	HFE_SPEC(hftp, index) &= ~(uintptr_t)EFX_HUNT_FILTER_FLAG_BUSY;
}

static			void
hunt_filter_set_entry_auto_old(
	__inout		hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT(hunt_filter_entry_spec(hftp, index) != NULL);
	HFE_SPEC(hftp, index) |= (uintptr_t)EFX_HUNT_FILTER_FLAG_AUTO_OLD;
}

static			void
hunt_filter_set_entry_not_auto_old(
	__inout		hunt_filter_table_t *hftp,
	__in		unsigned int index)
{
	HFE_SPEC(hftp, index) &= ~(uintptr_t)EFX_HUNT_FILTER_FLAG_AUTO_OLD;
	EFSYS_ASSERT(hunt_filter_entry_spec(hftp, index) != NULL);
}

	__checkReturn	efx_rc_t
hunt_filter_init(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;
	hunt_filter_table_t *hftp;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

#define	MATCH_MASK(match) (EFX_MASK32(match) << EFX_LOW_BIT(match))
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_HOST ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_IP));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_HOST ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_IP));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_MAC ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_REM_PORT ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_MAC ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_MAC));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_LOC_PORT ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_DST_PORT));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_ETHER_TYPE ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_INNER_VID ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_OUTER_VID ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN));
	EFX_STATIC_ASSERT(EFX_FILTER_MATCH_IP_PROTO ==
	    MATCH_MASK(MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO));
#undef MATCH_MASK

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (hunt_filter_table_t), hftp);

	if (!hftp) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_filter.ef_hunt_filter_table = hftp;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
hunt_filter_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	if (enp->en_filter.ef_hunt_filter_table != NULL) {
		EFSYS_KMEM_FREE(enp->en_esip, sizeof (hunt_filter_table_t),
		    enp->en_filter.ef_hunt_filter_table);
	}
}

static	__checkReturn	efx_rc_t
efx_mcdi_filter_op_add(
	__in		efx_nic_t *enp,
	__in		efx_filter_spec_t *spec,
	__in		unsigned int filter_op,
	__inout		hunt_filter_handle_t *handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FILTER_OP_IN_LEN,
			    MC_CMD_FILTER_OP_OUT_LEN)];
	uint32_t match_fields = 0;
	efx_rc_t rc;

	memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FILTER_OP_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FILTER_OP_OUT_LEN;

	switch (filter_op) {
	case MC_CMD_FILTER_OP_IN_OP_REPLACE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_LO,
		    handle->hfh_lo);
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_HI,
		    handle->hfh_hi);
		/* Fall through */
	case MC_CMD_FILTER_OP_IN_OP_INSERT:
	case MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP, filter_op);
		break;
	default:
		EFSYS_ASSERT(0);
		rc = EINVAL;
		goto fail1;
	}

	if (spec->efs_match_flags & EFX_FILTER_MATCH_LOC_MAC_IG) {
		/*
		 * The LOC_MAC_IG match flag can represent unknown unicast
		 *  or multicast filters - use the MAC address to distinguish
		 *  them.
		 */
		if (EFX_MAC_ADDR_IS_MULTICAST(spec->efs_loc_mac))
			match_fields |= 1U <<
				MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN;
		else
			match_fields |= 1U <<
				MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN;
	}

	match_fields |= spec->efs_match_flags & (~EFX_FILTER_MATCH_LOC_MAC_IG);

	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_PORT_ID,
	    EVB_PORT_ID_ASSIGNED);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_MATCH_FIELDS,
	    match_fields);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_DEST,
	    MC_CMD_FILTER_OP_IN_RX_DEST_HOST);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_QUEUE,
	    spec->efs_dmaq_id);
	if (spec->efs_flags & EFX_FILTER_FLAG_RX_RSS) {
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_CONTEXT,
		    spec->efs_rss_context);
	}
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_RX_MODE,
	    spec->efs_flags & EFX_FILTER_FLAG_RX_RSS ?
	    MC_CMD_FILTER_OP_IN_RX_MODE_RSS :
	    MC_CMD_FILTER_OP_IN_RX_MODE_SIMPLE);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_TX_DEST,
	    MC_CMD_FILTER_OP_IN_TX_DEST_DEFAULT);

	if (filter_op != MC_CMD_FILTER_OP_IN_OP_REPLACE) {
		/*
		 * NOTE: Unlike most MCDI requests, the filter fields
		 * are presented in network (big endian) byte order.
		 */
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_SRC_MAC),
		    spec->efs_rem_mac, EFX_MAC_ADDR_LEN);
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_DST_MAC),
		    spec->efs_loc_mac, EFX_MAC_ADDR_LEN);

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_SRC_PORT,
		    __CPU_TO_BE_16(spec->efs_rem_port));
		MCDI_IN_SET_WORD(req, FILTER_OP_IN_DST_PORT,
		    __CPU_TO_BE_16(spec->efs_loc_port));

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_ETHER_TYPE,
		    __CPU_TO_BE_16(spec->efs_ether_type));

		MCDI_IN_SET_WORD(req, FILTER_OP_IN_INNER_VLAN,
		    __CPU_TO_BE_16(spec->efs_inner_vid));
		MCDI_IN_SET_WORD(req, FILTER_OP_IN_OUTER_VLAN,
		    __CPU_TO_BE_16(spec->efs_outer_vid));

		/* IP protocol (in low byte, high byte is zero) */
		MCDI_IN_SET_BYTE(req, FILTER_OP_IN_IP_PROTO,
		    spec->efs_ip_proto);

		EFX_STATIC_ASSERT(sizeof (spec->efs_rem_host) ==
		    MC_CMD_FILTER_OP_IN_SRC_IP_LEN);
		EFX_STATIC_ASSERT(sizeof (spec->efs_loc_host) ==
		    MC_CMD_FILTER_OP_IN_DST_IP_LEN);

		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_SRC_IP),
		    &spec->efs_rem_host.eo_byte[0],
		    MC_CMD_FILTER_OP_IN_SRC_IP_LEN);
		memcpy(MCDI_IN2(req, uint8_t, FILTER_OP_IN_DST_IP),
		    &spec->efs_loc_host.eo_byte[0],
		    MC_CMD_FILTER_OP_IN_DST_IP_LEN);
	}

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_FILTER_OP_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	handle->hfh_lo = MCDI_OUT_DWORD(req, FILTER_OP_OUT_HANDLE_LO);
	handle->hfh_hi = MCDI_OUT_DWORD(req, FILTER_OP_OUT_HANDLE_HI);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}

static	__checkReturn	efx_rc_t
efx_mcdi_filter_op_delete(
	__in		efx_nic_t *enp,
	__in		unsigned int filter_op,
	__inout		hunt_filter_handle_t *handle)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_FILTER_OP_IN_LEN,
			    MC_CMD_FILTER_OP_OUT_LEN)];
	efx_rc_t rc;

	memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_FILTER_OP;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_FILTER_OP_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_FILTER_OP_OUT_LEN;

	switch (filter_op) {
	case MC_CMD_FILTER_OP_IN_OP_REMOVE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP,
		    MC_CMD_FILTER_OP_IN_OP_REMOVE);
		break;
	case MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE:
		MCDI_IN_SET_DWORD(req, FILTER_OP_IN_OP,
		    MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE);
		break;
	default:
		EFSYS_ASSERT(0);
		rc = EINVAL;
		goto fail1;
	}

	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_LO, handle->hfh_lo);
	MCDI_IN_SET_DWORD(req, FILTER_OP_IN_HANDLE_HI, handle->hfh_hi);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail2;
	}

	if (req.emr_out_length_used < MC_CMD_FILTER_OP_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	boolean_t
hunt_filter_equal(
	__in		const efx_filter_spec_t *left,
	__in		const efx_filter_spec_t *right)
{
	/* FIXME: Consider rx vs tx filters (look at efs_flags) */
	if (left->efs_match_flags != right->efs_match_flags)
		return (B_FALSE);
	if (!EFX_OWORD_IS_EQUAL(left->efs_rem_host, right->efs_rem_host))
		return (B_FALSE);
	if (!EFX_OWORD_IS_EQUAL(left->efs_loc_host, right->efs_loc_host))
		return (B_FALSE);
	if (memcmp(left->efs_rem_mac, right->efs_rem_mac, EFX_MAC_ADDR_LEN))
		return (B_FALSE);
	if (memcmp(left->efs_loc_mac, right->efs_loc_mac, EFX_MAC_ADDR_LEN))
		return (B_FALSE);
	if (left->efs_rem_port != right->efs_rem_port)
		return (B_FALSE);
	if (left->efs_loc_port != right->efs_loc_port)
		return (B_FALSE);
	if (left->efs_inner_vid != right->efs_inner_vid)
		return (B_FALSE);
	if (left->efs_outer_vid != right->efs_outer_vid)
		return (B_FALSE);
	if (left->efs_ether_type != right->efs_ether_type)
		return (B_FALSE);
	if (left->efs_ip_proto != right->efs_ip_proto)
		return (B_FALSE);

	return (B_TRUE);

}

static	__checkReturn	boolean_t
hunt_filter_same_dest(
	__in		const efx_filter_spec_t *left,
	__in		const efx_filter_spec_t *right)
{
	if ((left->efs_flags & EFX_FILTER_FLAG_RX_RSS) &&
	    (right->efs_flags & EFX_FILTER_FLAG_RX_RSS)) {
		if (left->efs_rss_context == right->efs_rss_context)
			return (B_TRUE);
	} else if ((~(left->efs_flags) & EFX_FILTER_FLAG_RX_RSS) &&
	    (~(right->efs_flags) & EFX_FILTER_FLAG_RX_RSS)) {
		if (left->efs_dmaq_id == right->efs_dmaq_id)
			return (B_TRUE);
	}
	return (B_FALSE);
}

static	__checkReturn	uint32_t
hunt_filter_hash(
	__in		efx_filter_spec_t *spec)
{
	EFX_STATIC_ASSERT((sizeof (efx_filter_spec_t) % sizeof (uint32_t))
			    == 0);
	EFX_STATIC_ASSERT((EFX_FIELD_OFFSET(efx_filter_spec_t, efs_outer_vid) %
			    sizeof (uint32_t)) == 0);

	/*
	 * As the area of the efx_filter_spec_t we need to hash is DWORD
	 * aligned and an exact number of DWORDs in size we can use the
	 * optimised efx_hash_dwords() rather than efx_hash_bytes()
	 */
	return (efx_hash_dwords((const uint32_t *)&spec->efs_outer_vid,
			(sizeof (efx_filter_spec_t) -
			EFX_FIELD_OFFSET(efx_filter_spec_t, efs_outer_vid)) /
			sizeof (uint32_t), 0));
}

/*
 * Decide whether a filter should be exclusive or else should allow
 * delivery to additional recipients.  Currently we decide that
 * filters for specific local unicast MAC and IP addresses are
 * exclusive.
 */
static	__checkReturn	boolean_t
hunt_filter_is_exclusive(
	__in		efx_filter_spec_t *spec)
{
	if ((spec->efs_match_flags & EFX_FILTER_MATCH_LOC_MAC) &&
	    !EFX_MAC_ADDR_IS_MULTICAST(spec->efs_loc_mac))
		return (B_TRUE);

	if ((spec->efs_match_flags &
		(EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_LOC_HOST)) ==
	    (EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_LOC_HOST)) {
		if ((spec->efs_ether_type == EFX_ETHER_TYPE_IPV4) &&
		    ((spec->efs_loc_host.eo_u8[0] & 0xf) != 0xe))
			return (B_TRUE);
		if ((spec->efs_ether_type == EFX_ETHER_TYPE_IPV6) &&
		    (spec->efs_loc_host.eo_u8[0] != 0xff))
			return (B_TRUE);
	}

	return (B_FALSE);
}

	__checkReturn	efx_rc_t
hunt_filter_restore(
	__in		efx_nic_t *enp)
{
	int tbl_id;
	efx_filter_spec_t *spec;
	hunt_filter_table_t *hftp = enp->en_filter.ef_hunt_filter_table;
	boolean_t restoring;
	int state;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	for (tbl_id = 0; tbl_id < EFX_HUNT_FILTER_TBL_ROWS; tbl_id++) {

		EFSYS_LOCK(enp->en_eslp, state);

		spec = hunt_filter_entry_spec(hftp, tbl_id);
		if (spec == NULL) {
			restoring = B_FALSE;
		} else if (hunt_filter_entry_is_busy(hftp, tbl_id)) {
			/* Ignore busy entries. */
			restoring = B_FALSE;
		} else {
			hunt_filter_set_entry_busy(hftp, tbl_id);
			restoring = B_TRUE;
		}

		EFSYS_UNLOCK(enp->en_eslp, state);

		if (restoring == B_FALSE)
			continue;

		if (hunt_filter_is_exclusive(spec)) {
			rc = efx_mcdi_filter_op_add(enp, spec,
			    MC_CMD_FILTER_OP_IN_OP_INSERT,
			    &hftp->hft_entry[tbl_id].hfe_handle);
		} else {
			rc = efx_mcdi_filter_op_add(enp, spec,
			    MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE,
			    &hftp->hft_entry[tbl_id].hfe_handle);
		}

		if (rc != 0)
			goto fail1;

		EFSYS_LOCK(enp->en_eslp, state);

		hunt_filter_set_entry_not_busy(hftp, tbl_id);

		EFSYS_UNLOCK(enp->en_eslp, state);
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * An arbitrary search limit for the software hash table. As per the linux net
 * driver.
 */
#define	EFX_HUNT_FILTER_SEARCH_LIMIT 200

static	__checkReturn	efx_rc_t
hunt_filter_add_internal(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace,
	__out_opt	uint32_t *filter_id)
{
	efx_rc_t rc;
	hunt_filter_table_t *hftp = enp->en_filter.ef_hunt_filter_table;
	efx_filter_spec_t *saved_spec;
	uint32_t hash;
	unsigned int depth;
	int ins_index;
	boolean_t replacing = B_FALSE;
	unsigned int i;
	int state;
	boolean_t locked = B_FALSE;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

#if EFSYS_OPT_RX_SCALE
	spec->efs_rss_context = enp->en_rss_context;
#endif

	hash = hunt_filter_hash(spec);

	/*
	 * FIXME: Add support for inserting filters of different priorities
	 * and removing lower priority multicast filters (bug 42378)
	 */

	/*
	 * Find any existing filters with the same match tuple or
	 * else a free slot to insert at.  If any of them are busy,
	 * we have to wait and retry.
	 */
	for (;;) {
		ins_index = -1;
		depth = 1;
		EFSYS_LOCK(enp->en_eslp, state);
		locked = B_TRUE;

		for (;;) {
			i = (hash + depth) & (EFX_HUNT_FILTER_TBL_ROWS - 1);
			saved_spec = hunt_filter_entry_spec(hftp, i);

			if (!saved_spec) {
				if (ins_index < 0) {
					ins_index = i;
				}
			} else if (hunt_filter_equal(spec, saved_spec)) {
				if (hunt_filter_entry_is_busy(hftp, i))
					break;
				if (saved_spec->efs_priority
					    == EFX_FILTER_PRI_AUTO) {
					ins_index = i;
					goto found;
				} else if (hunt_filter_is_exclusive(spec)) {
					if (may_replace) {
						ins_index = i;
						goto found;
					} else {
						rc = EEXIST;
						goto fail1;
					}
				}

				/* Leave existing */
			}

			/*
			 * Once we reach the maximum search depth, use
			 * the first suitable slot or return EBUSY if
			 * there was none.
			 */
			if (depth == EFX_HUNT_FILTER_SEARCH_LIMIT) {
				if (ins_index < 0) {
					rc = EBUSY;
					goto fail2;
				}
				goto found;
			}
			depth++;
		}
		EFSYS_UNLOCK(enp->en_eslp, state);
		locked = B_FALSE;
	}

found:
	/*
	 * Create a software table entry if necessary, and mark it
	 * busy.  We might yet fail to insert, but any attempt to
	 * insert a conflicting filter while we're waiting for the
	 * firmware must find the busy entry.
	 */
	saved_spec = hunt_filter_entry_spec(hftp, ins_index);
	if (saved_spec) {
		if (saved_spec->efs_priority == EFX_FILTER_PRI_AUTO) {
			/* This is a filter we are refreshing */
			hunt_filter_set_entry_not_auto_old(hftp, ins_index);
			goto out_unlock;

		}
		replacing = B_TRUE;
	} else {
		EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*spec), saved_spec);
		if (!saved_spec) {
			rc = ENOMEM;
			goto fail3;
		}
		*saved_spec = *spec;
		hunt_filter_set_entry(hftp, ins_index, saved_spec);
	}
	hunt_filter_set_entry_busy(hftp, ins_index);

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	/*
	 * On replacing the filter handle may change after after a successful
	 * replace operation.
	 */
	if (replacing) {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_REPLACE,
		    &hftp->hft_entry[ins_index].hfe_handle);
	} else if (hunt_filter_is_exclusive(spec)) {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_INSERT,
		    &hftp->hft_entry[ins_index].hfe_handle);
	} else {
		rc = efx_mcdi_filter_op_add(enp, spec,
		    MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE,
		    &hftp->hft_entry[ins_index].hfe_handle);
	}

	if (rc != 0)
		goto fail4;

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	if (replacing) {
		/* Update the fields that may differ */
		saved_spec->efs_priority = spec->efs_priority;
		saved_spec->efs_flags = spec->efs_flags;
		saved_spec->efs_rss_context = spec->efs_rss_context;
		saved_spec->efs_dmaq_id = spec->efs_dmaq_id;
	}

	hunt_filter_set_entry_not_busy(hftp, ins_index);

out_unlock:

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	if (filter_id)
		*filter_id = ins_index;

	return (0);

fail4:
	EFSYS_PROBE(fail4);

	if (!replacing) {
		EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), saved_spec);
		saved_spec = NULL;
	}
	hunt_filter_set_entry_not_busy(hftp, ins_index);
	hunt_filter_set_entry(hftp, ins_index, NULL);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	if (locked)
		EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

	__checkReturn	efx_rc_t
hunt_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace)
{
	efx_rc_t rc;

	rc = hunt_filter_add_internal(enp, spec, may_replace, NULL);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn	efx_rc_t
hunt_filter_delete_internal(
	__in		efx_nic_t *enp,
	__in		uint32_t filter_id)
{
	efx_rc_t rc;
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;
	efx_filter_spec_t *spec;
	int state;
	uint32_t filter_idx = filter_id % EFX_HUNT_FILTER_TBL_ROWS;

	/*
	 * Find the software table entry and mark it busy.  Don't
	 * remove it yet; any attempt to update while we're waiting
	 * for the firmware must find the busy entry.
	 *
	 * FIXME: What if the busy flag is never cleared?
	 */
	EFSYS_LOCK(enp->en_eslp, state);
	while (hunt_filter_entry_is_busy(table, filter_idx)) {
		EFSYS_UNLOCK(enp->en_eslp, state);
		EFSYS_SPIN(1);
		EFSYS_LOCK(enp->en_eslp, state);
	}
	if ((spec = hunt_filter_entry_spec(table, filter_idx)) != NULL) {
		hunt_filter_set_entry_busy(table, filter_idx);
	}
	EFSYS_UNLOCK(enp->en_eslp, state);

	if (spec == NULL) {
		rc = ENOENT;
		goto fail1;
	}

	/*
	 * Try to remove the hardware filter. This may fail if the MC has
	 * rebooted (which frees all hardware filter resources).
	 */
	if (hunt_filter_is_exclusive(spec)) {
		rc = efx_mcdi_filter_op_delete(enp,
		    MC_CMD_FILTER_OP_IN_OP_REMOVE,
		    &table->hft_entry[filter_idx].hfe_handle);
	} else {
		rc = efx_mcdi_filter_op_delete(enp,
		    MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE,
		    &table->hft_entry[filter_idx].hfe_handle);
	}

	/* Free the software table entry */
	EFSYS_LOCK(enp->en_eslp, state);
	hunt_filter_set_entry_not_busy(table, filter_idx);
	hunt_filter_set_entry(table, filter_idx, NULL);
	EFSYS_UNLOCK(enp->en_eslp, state);

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), spec);

	/* Check result of hardware filter removal */
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
hunt_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	efx_rc_t rc;
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;
	efx_filter_spec_t *saved_spec;
	unsigned int hash;
	unsigned int depth;
	unsigned int i;
	int state;
	boolean_t locked = B_FALSE;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON);

	hash = hunt_filter_hash(spec);

	EFSYS_LOCK(enp->en_eslp, state);
	locked = B_TRUE;

	depth = 1;
	for (;;) {
		i = (hash + depth) & (EFX_HUNT_FILTER_TBL_ROWS - 1);
		saved_spec = hunt_filter_entry_spec(table, i);
		if (saved_spec && hunt_filter_equal(spec, saved_spec) &&
		    hunt_filter_same_dest(spec, saved_spec)) {
			break;
		}
		if (depth == EFX_HUNT_FILTER_SEARCH_LIMIT) {
			rc = ENOENT;
			goto fail1;
		}
		depth++;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	locked = B_FALSE;

	rc = hunt_filter_delete_internal(enp, i);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	if (locked)
		EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_get_parser_disp_info(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_GET_PARSER_DISP_INFO_IN_LEN,
			    MC_CMD_GET_PARSER_DISP_INFO_OUT_LENMAX)];
	efx_rc_t rc;
	uint32_t i;
	boolean_t support_unknown_ucast = B_FALSE;
	boolean_t support_unknown_mcast = B_FALSE;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_GET_PARSER_DISP_INFO;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_GET_PARSER_DISP_INFO_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_GET_PARSER_DISP_INFO_OUT_LENMAX;

	MCDI_IN_SET_DWORD(req, GET_PARSER_DISP_INFO_OUT_OP,
	    MC_CMD_GET_PARSER_DISP_INFO_IN_OP_GET_SUPPORTED_RX_MATCHES);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	*length = MCDI_OUT_DWORD(req,
	    GET_PARSER_DISP_INFO_OUT_NUM_SUPPORTED_MATCHES);

	if (req.emr_out_length_used <
	    MC_CMD_GET_PARSER_DISP_INFO_OUT_LEN(*length)) {
		rc = EMSGSIZE;
		goto fail2;
	}

	memcpy(list,
	    MCDI_OUT2(req,
	    uint32_t,
	    GET_PARSER_DISP_INFO_OUT_SUPPORTED_MATCHES),
	    (*length) * sizeof (uint32_t));
	EFX_STATIC_ASSERT(sizeof (uint32_t) ==
	    MC_CMD_GET_PARSER_DISP_INFO_OUT_SUPPORTED_MATCHES_LEN);

	/*
	 * Remove UNKNOWN UCAST and MCAST flags, and if both are present, change
	 * the lower priority one to LOC_MAC_IG.
	 */
	for (i = 0; i < *length; i++) {
		if (list[i] & MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN) {
			list[i] &=
			(~MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN);
			support_unknown_ucast = B_TRUE;
		}
		if (list[i] & MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN) {
			list[i] &=
			(~MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN);
			support_unknown_mcast = B_TRUE;
		}

		if (support_unknown_ucast && support_unknown_mcast) {
			list[i] &= EFX_FILTER_MATCH_LOC_MAC_IG;
			break;
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
hunt_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_get_parser_disp_info(enp, list, length) != 0))
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
hunt_filter_unicast_refresh(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *addr,
	__in				boolean_t all_unicst,
	__in				efx_filter_flag_t filter_flags)
{
	hunt_filter_table_t *hftp = enp->en_filter.ef_hunt_filter_table;
	efx_filter_spec_t spec;
	efx_rc_t rc;

	if (all_unicst == B_TRUE)
		goto use_uc_def;

	/* Insert the filter for the local station address */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    hftp->hft_default_rxq);
	efx_filter_spec_set_eth_local(&spec, EFX_FILTER_SPEC_VID_UNSPEC, addr);

	rc = hunt_filter_add_internal(enp, &spec, B_TRUE,
	    &hftp->hft_unicst_filter_index);
	if (rc != 0) {
		/*
		 * Fall back to an unknown filter. We may be able to subscribe
		 * to it even if we couldn't insert the unicast filter.
		 */
		goto use_uc_def;
	}
	hftp->hft_unicst_filter_set = B_TRUE;

	return (0);

use_uc_def:
	/* Insert the unknown unicast filter */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    hftp->hft_default_rxq);
	efx_filter_spec_set_uc_def(&spec);
	rc = hunt_filter_add_internal(enp, &spec, B_TRUE,
	    &hftp->hft_unicst_filter_index);
	if (rc != 0)
		goto fail1;

	hftp->hft_unicst_filter_set = B_TRUE;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	if (hftp->hft_unicst_filter_set != B_FALSE) {
		(void) hunt_filter_delete_internal(enp,
		    hftp->hft_unicst_filter_index);

		hftp->hft_unicst_filter_set = B_FALSE;
	}

	return (rc);
}

static	__checkReturn	efx_rc_t
hunt_filter_multicast_refresh(
	__in				efx_nic_t *enp,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count,
	__in				efx_filter_flag_t filter_flags)
{
	hunt_filter_table_t *hftp = enp->en_filter.ef_hunt_filter_table;
	efx_filter_spec_t spec;
	uint8_t addr[6];
	unsigned i;
	efx_rc_t rc;

	if (all_mulcst == B_TRUE)
		goto use_mc_def;

	if (mulcst == B_FALSE)
		count = 0;

	if (count + (brdcst ? 1 : 0) >
	    EFX_ARRAY_SIZE(hftp->hft_mulcst_filter_indexes)) {
		/* Too many MAC addresses; use unknown multicast filter */
		goto use_mc_def;
	}

	/* Insert/renew multicast address list filters */
	hftp->hft_mulcst_filter_count = count;
	for (i = 0; i < hftp->hft_mulcst_filter_count; i++) {
		efx_filter_spec_init_rx(&spec,
		    EFX_FILTER_PRI_AUTO,
		    filter_flags,
		    hftp->hft_default_rxq);

		efx_filter_spec_set_eth_local(&spec,
		    EFX_FILTER_SPEC_VID_UNSPEC,
		    &addrs[i * EFX_MAC_ADDR_LEN]);

		rc = hunt_filter_add_internal(enp, &spec, B_TRUE,
		    &hftp->hft_mulcst_filter_indexes[i]);
		if (rc != 0) {
			/* Rollback, then use unknown multicast filter */
			goto rollback;
		}
	}

	if (brdcst == B_TRUE) {
		/* Insert/renew broadcast address filter */
		hftp->hft_mulcst_filter_count++;
		efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
		    filter_flags,
		    hftp->hft_default_rxq);

		EFX_MAC_BROADCAST_ADDR_SET(addr);
		efx_filter_spec_set_eth_local(&spec, EFX_FILTER_SPEC_VID_UNSPEC,
		    addr);

		rc = hunt_filter_add_internal(enp, &spec, B_TRUE,
		    &hftp->hft_mulcst_filter_indexes[
			hftp->hft_mulcst_filter_count - 1]);
		if (rc != 0) {
			/* Rollback, then use unknown multicast filter */
			goto rollback;
		}
	}

	return (0);

rollback:
	/*
	 * Rollback by removing any filters we have inserted
	 * before inserting the unknown multicast filter.
	 */
	while (i--) {
		(void) hunt_filter_delete_internal(enp,
		    hftp->hft_mulcst_filter_indexes[i]);
	}
	hftp->hft_mulcst_filter_count = 0;

use_mc_def:
	/* Insert the unknown multicast filter */
	efx_filter_spec_init_rx(&spec, EFX_FILTER_PRI_AUTO,
	    filter_flags,
	    hftp->hft_default_rxq);
	efx_filter_spec_set_mc_def(&spec);

	rc = hunt_filter_add_internal(enp, &spec, B_TRUE,
	    &hftp->hft_mulcst_filter_indexes[0]);
	if (rc != 0)
		goto fail1;

	hftp->hft_mulcst_filter_count = 1;

	/*
	 * FIXME: If brdcst == B_FALSE, add a filter to drop broadcast traffic.
	 */

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}


static	__checkReturn	efx_rc_t
hunt_filter_get_workarounds(
	__in				efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	uint32_t implemented = 0;
	uint32_t enabled = 0;
	efx_rc_t rc;

	rc = efx_mcdi_get_workarounds(enp, &implemented, &enabled);
	if (rc == 0) {
		/* Check if chained multicast filter support is enabled */
		if (implemented & enabled & MC_CMD_GET_WORKAROUNDS_OUT_BUG26807)
			encp->enc_bug26807_workaround = B_TRUE;
		else
			encp->enc_bug26807_workaround = B_FALSE;
	} else if (rc == ENOTSUP) {
		/*
		 * Firmware is too old to support GET_WORKAROUNDS, and support
		 * for this workaround was implemented later.
		 */
		encp->enc_bug26807_workaround = B_FALSE;
	} else {
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);

}


/*
 * Reconfigure all filters.
 * If all_unicst and/or all mulcst filters cannot be applied then
 * return ENOTSUP (Note the filters for the specified addresses are
 * still applied in this case).
 */
	__checkReturn	efx_rc_t
hunt_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count)
{
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;
	efx_filter_flag_t filter_flags;
	unsigned i;
	int all_unicst_rc;
	int all_mulcst_rc;
	efx_rc_t rc;

	if (table->hft_default_rxq == NULL) {
		/*
		 * Filters direct traffic to the default RXQ, and so cannot be
		 * inserted until it is available. Any currently configured
		 * filters must be removed (ignore errors in case the MC
		 * has rebooted, which removes hardware filters).
		 */
		if (table->hft_unicst_filter_set != B_FALSE) {
			(void) hunt_filter_delete_internal(enp,
						table->hft_unicst_filter_index);
			table->hft_unicst_filter_set = B_FALSE;
		}
		for (i = 0; i < table->hft_mulcst_filter_count; i++) {
			(void) hunt_filter_delete_internal(enp,
					table->hft_mulcst_filter_indexes[i]);
		}
		table->hft_mulcst_filter_count = 0;

		return (0);
	}

	if (table->hft_using_rss)
		filter_flags = EFX_FILTER_FLAG_RX_RSS;
	else
		filter_flags = 0;

	/* Mark old filters which may need to be removed */
	if (table->hft_unicst_filter_set != B_FALSE) {
		hunt_filter_set_entry_auto_old(table,
					    table->hft_unicst_filter_index);
	}
	for (i = 0; i < table->hft_mulcst_filter_count; i++) {
		hunt_filter_set_entry_auto_old(table,
					table->hft_mulcst_filter_indexes[i]);
	}

	/* Insert or renew unicast filters */
	if ((all_unicst_rc = hunt_filter_unicast_refresh(enp, mac_addr,
		    all_unicst, filter_flags)) !=  0) {
		if (all_unicst == B_FALSE) {
			rc = all_unicst_rc;
			goto fail1;
		}
		/* Retry without all_unicast flag */
		rc = hunt_filter_unicast_refresh(enp, mac_addr,
			B_FALSE, filter_flags);
		if (rc != 0)
			goto fail2;
	}

	/*
	 * WORKAROUND_BUG26807 controls firmware support for chained multicast
	 * filters, and can only be enabled or disabled when the hardware filter
	 * table is empty.
	 *
	 * Firmware will reset (FLR) functions which have inserted filters in
	 * the hardware filter table when the workaround is enabled/disabled.
	 * Functions without any hardware filters are not reset.
	 *
	 * Re-check if the workaround is enabled after adding unicast hardware
	 * filters. This ensures that encp->enc_workaround_bug26807 matches the
	 * firmware state, and that later changes to enable/disable the
	 * workaround will result in this function seeing a reset (FLR).
	 */
	if ((rc = hunt_filter_get_workarounds(enp)) != 0)
		goto fail3;

	/* Insert or renew multicast filters */
	if ((all_mulcst_rc = hunt_filter_multicast_refresh(enp, mulcst,
				all_mulcst, brdcst,
				addrs, count, filter_flags)) != 0) {
		if (all_mulcst == B_FALSE) {
			rc = all_mulcst_rc;
			goto fail4;
		}
		/* Retry without all_mulcast flag */
		rc = hunt_filter_multicast_refresh(enp, mulcst,
						B_FALSE, brdcst,
						addrs, count, filter_flags);
		if (rc != 0)
			goto fail5;
	}

	/* Remove old filters which were not renewed */
	for (i = 0; i < EFX_ARRAY_SIZE(table->hft_entry); i++) {
		if (hunt_filter_entry_is_auto_old(table, i)) {
			(void) hunt_filter_delete_internal(enp, i);
		}
	}

	/* report if any optional flags were rejected */
	if (((all_unicst != B_FALSE) && (all_unicst_rc != 0)) ||
	    ((all_mulcst != B_FALSE) && (all_mulcst_rc != 0))) {
		rc = ENOTSUP;
	}

	return (rc);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	/* Clear auto old flags */
	for (i = 0; i < EFX_ARRAY_SIZE(table->hft_entry); i++) {
		if (hunt_filter_entry_is_auto_old(table, i)) {
			hunt_filter_set_entry_not_auto_old(table, i);
		}
	}

	return (rc);
}

		void
hunt_filter_get_default_rxq(
	__in		efx_nic_t *enp,
	__out		efx_rxq_t **erpp,
	__out		boolean_t *using_rss)
{
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;

	*erpp = table->hft_default_rxq;
	*using_rss = table->hft_using_rss;
}


		void
hunt_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss)
{
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;

#if EFSYS_OPT_RX_SCALE
	EFSYS_ASSERT((using_rss == B_FALSE) ||
	    (enp->en_rss_context != HUNTINGTON_RSS_CONTEXT_INVALID));
	table->hft_using_rss = using_rss;
#else
	EFSYS_ASSERT(using_rss == B_FALSE);
	table->hft_using_rss = B_FALSE;
#endif
	table->hft_default_rxq = erp;
}

		void
hunt_filter_default_rxq_clear(
	__in		efx_nic_t *enp)
{
	hunt_filter_table_t *table = enp->en_filter.ef_hunt_filter_table;

	table->hft_default_rxq = NULL;
	table->hft_using_rss = B_FALSE;
}


#endif /* EFSYS_OPT_FILTER */

#endif /* EFSYS_OPT_HUNTINGTON */
