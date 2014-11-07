/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/
/* Implementation of the IKE event agent.
   This module listens for IKE events emitted by the NanoSec core in SeP.
   The events are accumulated in a queue and delivered to the IKE server
   in user space for processing via an IOCTL */
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_REQUEST
#include "sep_log.h"
#include "dx_driver.h"
#include "dx_sep_kapi.h"
#include "dx_driver_abi.h"
#include "sep_ipsec_desc.h"
#include "ike_evt_agent.h"
#include <linux/kthread.h>
#include <linux/sched.h>

/* Max. pending events for user to read */
#define EVENT_Q_SIZE_LOG2 4
#define EVENT_Q_SIZE (1 << EVENT_Q_SIZE_LOG2)
#define EVENT_Q_SIZE_MASK (EVENT_Q_SIZE - 1)

struct event_entry {
	uint32_t event_msg_size; /* size of event_msg in bytes */
	uint32_t event_msg[IKE_EVENT_MSG_SIZE_MAX/sizeof(uint32_t)];
};

static struct event_entry ike_events_q[EVENT_Q_SIZE];
static volatile uint32_t event_q_head;
static uint32_t event_q_tail;
struct completion event_signal; /* Signal receiver about available events */
static struct task_struct *agent_task;

/**
 * ike_evt_agent_proc() - The IKE event agent task function
 *
 * @data:	void
 */
static int ike_evt_agent_proc(void *data)
{
	int rc;
	uint32_t timeout_jiffies = MAX_SCHEDULE_TIMEOUT;
	/* Wakeup only when event arrives or when interrupted */
	int cur_event_q_idx;

	while (1) { /* Exit when getting -ENODEV while waiting */
		cur_event_q_idx = event_q_head & EVENT_Q_SIZE_MASK;
		ike_events_q[cur_event_q_idx].event_msg_size =
			IKE_EVENT_MSG_SIZE_MAX;
		rc = dx_sep_req_wait_for_request(IPSEC2IKE_EVENTS_AGENT_ID,
			(uint8_t *)&ike_events_q[cur_event_q_idx].event_msg,
			&ike_events_q[cur_event_q_idx].event_msg_size,
			timeout_jiffies);
		if (likely(rc == 0)) {
			dx_sep_req_send_response(IPSEC2IKE_EVENTS_AGENT_ID,
				NULL, 0);
			event_q_head++;
			SEP_LOG_DEBUG("Got IKE event #%u\n", event_q_head);
			complete(&event_signal);
		} else { /* Handle errors */
			if (rc == -ENOMEM) {
				SEP_LOG_ERR("Got event of size %u > max. of %u"
					    " --> event lost\n",
					ike_events_q[cur_event_q_idx].
						event_msg_size,
					IKE_EVENT_MSG_SIZE_MAX);
			} else if (rc == -EAGAIN) {
				SEP_LOG_INFO("Wait timed-out...?\n");
			} else if (rc == -ERESTART) {
				SEP_LOG_INFO("Interrupted...\n");
			} else if (rc == -ENODEV) {
				SEP_LOG_INFO("IKE event agent unregistered.\n");
				break; /* This signals termination */
			} else
				SEP_LOG_ERR("Unknown error: %d\n", rc);
		}
	} /*while*/
	SEP_LOG_DEBUG("Exiting.\n");
	return 0;
}

/**
 * ipsec_ike_event_get_next() - Get the next IKE event
 *
 * @timeout_ms:		Timeout for waiting for next event in [msec]
 * @event_msg_size:	Returned event message size in bytes
 *			(at most IKE_EVENT_MSG_SIZE_MAX)
 * @event_msg:		The returned event buffer
 *
 * Returns 0 on success,
 *	-EAGAIN if no event arrived during given timeout,
 *	-ERESTARTSYS if wait interrupted
 */
int ipsec_ike_event_get_next(uint32_t timeout_ms,
	uint32_t *event_msg_size, uint32_t *event_msg)
{
	long rc;
	int cur_tail_index;
	unsigned long timeout_jiffies = msecs_to_jiffies(timeout_ms);

	do {
		if ((event_q_head - event_q_tail) >= EVENT_Q_SIZE) {
			SEP_LOG_ERR("Lost %u events\n",
				event_q_head - (EVENT_Q_SIZE - 1) -
				event_q_tail);
			/* Skip lost events */
			event_q_tail = event_q_head - (EVENT_Q_SIZE - 1);
			/* Clear any pending completions - we need them only
			   when we empty the queue */
			INIT_COMPLETION(event_signal);
		} else if (event_q_head == event_q_tail) { /* Empty */
			/* Block until new event arrives or timeout */
			rc = wait_for_completion_interruptible_timeout(
				&event_signal, timeout_jiffies);
			if (rc < 0) {
				if (rc == -ERESTARTSYS) {
					SEP_LOG_DEBUG("Waiting interrupted "
						      "(%ld)\n", rc);
					rc = -ERESTART; /* Map to user code */
				} else {
					SEP_LOG_ERR("Unknown error while "
						    "waiting for event (%ld)\n",
						rc);
				}
				return (int)rc;
			}
			if (rc == 0) {
				SEP_LOG_DEBUG("Timed out waiting for event %u "
					      "msec.\n", timeout_ms);
				return -ETIMEDOUT;
			}
			if (event_q_head == event_q_tail) {
				SEP_LOG_DEBUG("Woke up but no event in queue!"
					      "\n");
				continue; /* Try again... */
			}
		}
		/* Event is available - copy it */
		cur_tail_index = event_q_tail & EVENT_Q_SIZE_MASK;
		*event_msg_size = ike_events_q[cur_tail_index].event_msg_size;
		if (unlikely(*event_msg_size > IKE_EVENT_MSG_SIZE_MAX)) {
			SEP_LOG_ERR("Invalid message size = %u\n",
				*event_msg_size);
			event_q_tail++;
			continue; /* Wait for next event */
		}
		memcpy(event_msg,
			ike_events_q[cur_tail_index].event_msg,
			*event_msg_size);
		event_q_tail++;
		SEP_LOG_DEBUG("Forwarding IKE event #%u\n", event_q_tail);
	/* Try again if identified queue overflow... */
	} while ((event_q_head - event_q_tail) >= EVENT_Q_SIZE);
	return 0;
}

/**
 * ipsec_ike_event_listener_init() - Initialize the listener to IKE events
 */
int ipsec_ike_event_listener_init(void)
{
	int rc;
	uint32_t max_buf_size;

	event_q_head = event_q_tail = 0;
	init_completion(&event_signal);
	agent_task = NULL;
	/* Register sep request agent */
	rc = dx_sep_req_register_agent(IPSEC2IKE_EVENTS_AGENT_ID,
		&max_buf_size);
	if (rc != 0) {
		SEP_LOG_ERR("Failed registering IKE events agent (rc=%d)\n",
			rc);
		return rc;
	}
	if (max_buf_size < IKE_EVENT_MSG_SIZE_MAX) {
		SEP_LOG_ERR("The SeP request queue max supported buffer size is"
			" too small for IKE events (only %u < %u)\n",
			max_buf_size, IKE_EVENT_MSG_SIZE_MAX);
		rc = -ENOMEM;
		goto init_done;
	}
	/* Create polling kernel thread */
	agent_task = kthread_create(ike_evt_agent_proc, NULL, "ike_evt_agent");
	if (agent_task != NULL)
		wake_up_process(agent_task);
	else {
		SEP_LOG_ERR("Failed creating agent task.\n");
		rc = -ENOENT;
	}
init_done:
	if (unlikely(rc != 0))
		dx_sep_req_unregister_agent(IPSEC2IKE_EVENTS_AGENT_ID);
	return rc;
}

/**
 * ipsec_ike_event_listener_terminate() - Terminate the listener to IKE events
 */
void ipsec_ike_event_listener_terminate(void)
{
	dx_sep_req_unregister_agent(IPSEC2IKE_EVENTS_AGENT_ID);
	if (agent_task != NULL) {
		kthread_stop(agent_task);
		agent_task = NULL;
	}
}

