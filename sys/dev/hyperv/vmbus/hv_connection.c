/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "hv_vmbus_priv.h"

/*
 * Globals
 */
hv_vmbus_connection hv_vmbus_g_connection =
	{ .connect_state = HV_DISCONNECTED,
	  .next_gpadl_handle = 0xE1E10, };

uint32_t hv_vmbus_protocal_version = HV_VMBUS_VERSION_WS2008;

static uint32_t
hv_vmbus_get_next_version(uint32_t current_ver)
{
	switch (current_ver) {
	case (HV_VMBUS_VERSION_WIN7):
		return(HV_VMBUS_VERSION_WS2008);

	case (HV_VMBUS_VERSION_WIN8):
		return(HV_VMBUS_VERSION_WIN7);

	case (HV_VMBUS_VERSION_WIN8_1):
		return(HV_VMBUS_VERSION_WIN8);

	case (HV_VMBUS_VERSION_WS2008):
	default:
		return(HV_VMBUS_VERSION_INVALID);
	}
}

/**
 * Negotiate the highest supported hypervisor version.
 */
static int
hv_vmbus_negotiate_version(hv_vmbus_channel_msg_info *msg_info,
	uint32_t version)
{
	int					ret = 0;
	hv_vmbus_channel_initiate_contact	*msg;

	sema_init(&msg_info->wait_sema, 0, "Msg Info Sema");
	msg = (hv_vmbus_channel_initiate_contact*) msg_info->msg;

	msg->header.message_type = HV_CHANNEL_MESSAGE_INITIATED_CONTACT;
	msg->vmbus_version_requested = version;

	msg->interrupt_page = hv_get_phys_addr(
		hv_vmbus_g_connection.interrupt_page);

	msg->monitor_page_1 = hv_get_phys_addr(
		hv_vmbus_g_connection.monitor_pages);

	msg->monitor_page_2 =
		hv_get_phys_addr(
			((uint8_t *) hv_vmbus_g_connection.monitor_pages
			+ PAGE_SIZE));

	/**
	 * Add to list before we send the request since we may receive the
	 * response before returning from this routine
	 */
	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);

	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(
		msg,
		sizeof(hv_vmbus_channel_initiate_contact));

	if (ret != 0) {
		mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
		TAILQ_REMOVE(
			&hv_vmbus_g_connection.channel_msg_anchor,
			msg_info,
			msg_list_entry);
		mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);
		return (ret);
	}

	/**
	 * Wait for the connection response
	 */
	ret = sema_timedwait(&msg_info->wait_sema, 500); /* KYS 5 seconds */

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	/**
	 * Check if successful
	 */
	if (msg_info->response.version_response.version_supported) {
		hv_vmbus_g_connection.connect_state = HV_CONNECTED;
	} else {
		ret = ECONNREFUSED;
	}

	return (ret);
}

/**
 * Send a connect request on the partition service connection
 */
int
hv_vmbus_connect(void) {
	int					ret = 0;
	uint32_t				version;
	hv_vmbus_channel_msg_info*		msg_info = NULL;

	/**
	 * Make sure we are not connecting or connected
	 */
	if (hv_vmbus_g_connection.connect_state != HV_DISCONNECTED) {
		return (-1);
	}

	/**
	 * Initialize the vmbus connection
	 */
	hv_vmbus_g_connection.connect_state = HV_CONNECTING;
	hv_vmbus_g_connection.work_queue = hv_work_queue_create("vmbusQ");
	sema_init(&hv_vmbus_g_connection.control_sema, 1, "control_sema");

	TAILQ_INIT(&hv_vmbus_g_connection.channel_msg_anchor);
	mtx_init(&hv_vmbus_g_connection.channel_msg_lock, "vmbus channel msg",
		NULL, MTX_SPIN);

	TAILQ_INIT(&hv_vmbus_g_connection.channel_anchor);
	mtx_init(&hv_vmbus_g_connection.channel_lock, "vmbus channel",
		NULL, MTX_DEF);

	/**
	 * Setup the vmbus event connection for channel interrupt abstraction
	 * stuff
	 */
	hv_vmbus_g_connection.interrupt_page = contigmalloc(
					PAGE_SIZE, M_DEVBUF,
					M_NOWAIT | M_ZERO, 0UL,
					BUS_SPACE_MAXADDR,
					PAGE_SIZE, 0);
	KASSERT(hv_vmbus_g_connection.interrupt_page != NULL,
	    ("Error VMBUS: malloc failed to allocate Channel"
		" Request Event message!"));
	if (hv_vmbus_g_connection.interrupt_page == NULL) {
	    ret = ENOMEM;
	    goto cleanup;
	}

	hv_vmbus_g_connection.recv_interrupt_page =
		hv_vmbus_g_connection.interrupt_page;

	hv_vmbus_g_connection.send_interrupt_page =
		((uint8_t *) hv_vmbus_g_connection.interrupt_page +
		    (PAGE_SIZE >> 1));

	/**
	 * Set up the monitor notification facility. The 1st page for
	 * parent->child and the 2nd page for child->parent
	 */
	hv_vmbus_g_connection.monitor_pages = contigmalloc(
		2 * PAGE_SIZE,
		M_DEVBUF,
		M_NOWAIT | M_ZERO,
		0UL,
		BUS_SPACE_MAXADDR,
		PAGE_SIZE,
		0);
	KASSERT(hv_vmbus_g_connection.monitor_pages != NULL,
	    ("Error VMBUS: malloc failed to allocate Monitor Pages!"));
	if (hv_vmbus_g_connection.monitor_pages == NULL) {
	    ret = ENOMEM;
	    goto cleanup;
	}

	msg_info = (hv_vmbus_channel_msg_info*)
		malloc(sizeof(hv_vmbus_channel_msg_info) +
			sizeof(hv_vmbus_channel_initiate_contact),
			M_DEVBUF, M_NOWAIT | M_ZERO);
	KASSERT(msg_info != NULL,
	    ("Error VMBUS: malloc failed for Initiate Contact message!"));
	if (msg_info == NULL) {
	    ret = ENOMEM;
	    goto cleanup;
	}

	hv_vmbus_g_connection.channels = malloc(sizeof(hv_vmbus_channel*) *
		HV_CHANNEL_MAX_COUNT,
		M_DEVBUF, M_WAITOK | M_ZERO);
	/*
	 * Find the highest vmbus version number we can support.
	 */
	version = HV_VMBUS_VERSION_CURRENT;

	do {
		ret = hv_vmbus_negotiate_version(msg_info, version);
		if (ret == EWOULDBLOCK) {
			/*
			 * We timed out.
			 */
			goto cleanup;
		}

		if (hv_vmbus_g_connection.connect_state == HV_CONNECTED)
			break;

		version = hv_vmbus_get_next_version(version);
	} while (version != HV_VMBUS_VERSION_INVALID);

	hv_vmbus_protocal_version = version;
	if (bootverbose)
		printf("VMBUS: Protocol Version: %d.%d\n",
		    version >> 16, version & 0xFFFF);

	sema_destroy(&msg_info->wait_sema);
	free(msg_info, M_DEVBUF);

	return (0);

	/*
	 * Cleanup after failure!
	 */
	cleanup:

	hv_vmbus_g_connection.connect_state = HV_DISCONNECTED;

	hv_work_queue_close(hv_vmbus_g_connection.work_queue);
	sema_destroy(&hv_vmbus_g_connection.control_sema);
	mtx_destroy(&hv_vmbus_g_connection.channel_lock);
	mtx_destroy(&hv_vmbus_g_connection.channel_msg_lock);

	if (hv_vmbus_g_connection.interrupt_page != NULL) {
		contigfree(
			hv_vmbus_g_connection.interrupt_page,
			PAGE_SIZE,
			M_DEVBUF);
		hv_vmbus_g_connection.interrupt_page = NULL;
	}

	if (hv_vmbus_g_connection.monitor_pages != NULL) {
		contigfree(
			hv_vmbus_g_connection.monitor_pages,
			2 * PAGE_SIZE,
			M_DEVBUF);
		hv_vmbus_g_connection.monitor_pages = NULL;
	}

	if (msg_info) {
		sema_destroy(&msg_info->wait_sema);
		free(msg_info, M_DEVBUF);
	}

	free(hv_vmbus_g_connection.channels, M_DEVBUF);
	return (ret);
}

/**
 * Send a disconnect request on the partition service connection
 */
int
hv_vmbus_disconnect(void) {
	int			 ret = 0;
	hv_vmbus_channel_unload* msg;

	msg = malloc(sizeof(hv_vmbus_channel_unload),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	KASSERT(msg != NULL,
	    ("Error VMBUS: malloc failed to allocate Channel Unload Msg!"));
	if (msg == NULL)
	    return (ENOMEM);

	msg->message_type = HV_CHANNEL_MESSAGE_UNLOAD;

	ret = hv_vmbus_post_message(msg, sizeof(hv_vmbus_channel_unload));


	contigfree(hv_vmbus_g_connection.interrupt_page, PAGE_SIZE, M_DEVBUF);

	mtx_destroy(&hv_vmbus_g_connection.channel_msg_lock);

	hv_work_queue_close(hv_vmbus_g_connection.work_queue);
	sema_destroy(&hv_vmbus_g_connection.control_sema);

	free(hv_vmbus_g_connection.channels, M_DEVBUF);
	hv_vmbus_g_connection.connect_state = HV_DISCONNECTED;

	free(msg, M_DEVBUF);

	return (ret);
}

/**
 * Process a channel event notification
 */
static void
VmbusProcessChannelEvent(uint32_t relid) 
{
	void* arg;
	uint32_t bytes_to_read;
	hv_vmbus_channel* channel;
	boolean_t is_batched_reading;

	/**
	 * Find the channel based on this relid and invokes
	 * the channel callback to process the event
	 */

	channel = hv_vmbus_g_connection.channels[relid];

	if (channel == NULL) {
		return;
	}
	/**
	 * To deal with the race condition where we might
	 * receive a packet while the relevant driver is 
	 * being unloaded, dispatch the callback while 
	 * holding the channel lock. The unloading driver
	 * will acquire the same channel lock to set the
	 * callback to NULL. This closes the window.
	 */

	/*
	 * Disable the lock due to newly added WITNESS check in r277723.
	 * Will seek other way to avoid race condition.
	 * -- whu
	 */
	// mtx_lock(&channel->inbound_lock);
	if (channel->on_channel_callback != NULL) {
		arg = channel->channel_callback_context;
		is_batched_reading = channel->batched_reading;
		/*
		 * Optimize host to guest signaling by ensuring:
		 * 1. While reading the channel, we disable interrupts from
		 *    host.
		 * 2. Ensure that we process all posted messages from the host
		 *    before returning from this callback.
		 * 3. Once we return, enable signaling from the host. Once this
		 *    state is set we check to see if additional packets are
		 *    available to read. In this case we repeat the process.
		 */
		do {
			if (is_batched_reading)
				hv_ring_buffer_read_begin(&channel->inbound);

			channel->on_channel_callback(arg);

			if (is_batched_reading)
				bytes_to_read =
				    hv_ring_buffer_read_end(&channel->inbound);
			else
				bytes_to_read = 0;
		} while (is_batched_reading && (bytes_to_read != 0));
	}
	// mtx_unlock(&channel->inbound_lock);
}

/**
 * Handler for events
 */
void
hv_vmbus_on_events(void *arg) 
{
	int bit;
	int cpu;
	int dword;
	void *page_addr;
	uint32_t* recv_interrupt_page = NULL;
	int rel_id;
	int maxdword;
	hv_vmbus_synic_event_flags *event;
	/* int maxdword = PAGE_SIZE >> 3; */

	cpu = (int)(long)arg;
	KASSERT(cpu <= mp_maxid, ("VMBUS: hv_vmbus_on_events: "
	    "cpu out of range!"));

	if ((hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008) ||
	    (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)) {
		maxdword = HV_MAX_NUM_CHANNELS_SUPPORTED >> 5;
		/*
		 * receive size is 1/2 page and divide that by 4 bytes
		 */
		recv_interrupt_page =
		    hv_vmbus_g_connection.recv_interrupt_page;
	} else {
		/*
		 * On Host with Win8 or above, the event page can be
		 * checked directly to get the id of the channel
		 * that has the pending interrupt.
		 */
		maxdword = HV_EVENT_FLAGS_DWORD_COUNT;
		page_addr = hv_vmbus_g_context.syn_ic_event_page[cpu];
		event = (hv_vmbus_synic_event_flags *)
		    page_addr + HV_VMBUS_MESSAGE_SINT;
		recv_interrupt_page = event->flags32;
	}

	/*
	 * Check events
	 */
	if (recv_interrupt_page != NULL) {
	    for (dword = 0; dword < maxdword; dword++) {
		if (recv_interrupt_page[dword]) {
		    for (bit = 0; bit < HV_CHANNEL_DWORD_LEN; bit++) {
			if (synch_test_and_clear_bit(bit,
			    (uint32_t *) &recv_interrupt_page[dword])) {
			    rel_id = (dword << 5) + bit;
			    if (rel_id == 0) {
				/*
				 * Special case -
				 * vmbus channel protocol msg.
				 */
				continue;
			    } else {
				VmbusProcessChannelEvent(rel_id);

			    }
			}
		    }
		}
	    }
	}

	return;
}

/**
 * Send a msg on the vmbus's message connection
 */
int hv_vmbus_post_message(void *buffer, size_t bufferLen) {
	int ret = 0;
	hv_vmbus_connection_id connId;
	unsigned retries = 0;

	/* NetScaler delays from previous code were consolidated here */
	static int delayAmount[] = {100, 100, 100, 500, 500, 5000, 5000, 5000};

	/* for(each entry in delayAmount) try to post message,
	 *  delay a little bit before retrying
	 */
	for (retries = 0;
	    retries < sizeof(delayAmount)/sizeof(delayAmount[0]); retries++) {
	    connId.as_uint32_t = 0;
	    connId.u.id = HV_VMBUS_MESSAGE_CONNECTION_ID;
	    ret = hv_vmbus_post_msg_via_msg_ipc(connId, 1, buffer, bufferLen);
	    if (ret != HV_STATUS_INSUFFICIENT_BUFFERS)
		break;
	    /* TODO: KYS We should use a blocking wait call */
	    DELAY(delayAmount[retries]);
	}

	KASSERT(ret == 0, ("Error VMBUS: Message Post Failed\n"));

	return (ret);
}

/**
 * Send an event notification to the parent
 */
int
hv_vmbus_set_event(hv_vmbus_channel *channel) {
	int ret = 0;
	uint32_t child_rel_id = channel->offer_msg.child_rel_id;

	/* Each uint32_t represents 32 channels */

	synch_set_bit(child_rel_id & 31,
		(((uint32_t *)hv_vmbus_g_connection.send_interrupt_page
			+ (child_rel_id >> 5))));
	ret = hv_vmbus_signal_event(channel->signal_event_param);

	return (ret);
}
