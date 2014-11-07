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

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_REQUEST

#include <linux/completion.h>
#include <linux/export.h>
#include "dx_driver.h"
#include "dx_bitops.h"
#include "sep_log.h"
#include "dx_reg_base_host.h"
#include "dx_host.h"
#define DX_CC_HOST_VIRT /* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "sep_request.h"
#include "sep_request_mgr.h"
#include "dx_sep_kapi.h"


/* The request/response coherent buffer size */
#define DX_SEP_REQUEST_BUF_SIZE (4*1024)
#if (DX_SEP_REQUEST_BUF_SIZE < DX_SEP_REQUEST_MIN_BUF_SIZE)
#error DX_SEP_REQUEST_BUF_SIZE too small
#endif
#if (DX_SEP_REQUEST_BUF_SIZE > DX_SEP_REQUEST_MAX_BUF_SIZE)
#error DX_SEP_REQUEST_BUF_SIZE too big
#endif
#if (DX_SEP_REQUEST_BUF_SIZE & DX_SEP_REQUEST_4KB_MASK)
#error DX_SEP_REQUEST_BUF_SIZE must be a 4KB multiple
#endif

/* The maximum number of sep request agents */
/* Valid IDs are 0 to (DX_SEP_REQUEST_MAX_AGENTS-1) */
#define DX_SEP_REQUEST_MAX_AGENTS 4
#define DX_SEP_REQUEST_INVALID_AGENT_ID 0xFF

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Sep Request state object */
static struct {
	struct mutex state_lock;
	struct sep_drvdata *drvdata;
	uint8_t *sep_req_buf_p;
	dma_addr_t sep_req_buf_dma;
	uint8_t *host_resp_buf_p;
	dma_addr_t host_resp_buf_dma;
	uint8_t req_counter;
	struct completion agent_event[DX_SEP_REQUEST_MAX_AGENTS];
	volatile bool agent_valid[DX_SEP_REQUEST_MAX_AGENTS];
	volatile int pending_req_agent_id;
	uint32_t *sep_host_gpr_adr;
	uint32_t *host_sep_gpr_adr;
} sep_req_state;


/* TODO:
   1) request_pending should use agent id instead of a global flag
   2) agent id 0 should be changed to non-valid
   3) Change sep request params for sep init to [] array instead pointer
   4) Consider usage of a mutex for syncing all access to the state
*/

/**
 * emit_error_response() - Respond to SeP with failure of request
 *
 * @drvdata:		Device context
 * @sep_req_error:	Request error code
 */
static void emit_error_response(
	struct sep_drvdata *drvdata, uint32_t sep_req_error)
{
	uint32_t gpr_val;
	/* Build the new GPR3 value out of the req_counter from the
	   state, the error condition and zero response length value */
	gpr_val = 0;
	SEP_REQUEST_SET_COUNTER(gpr_val, sep_req_state.req_counter);
	SEP_REQUEST_SET_RESP_LEN(gpr_val, 0);
	SEP_REQUEST_SET_RETURN_CODE(gpr_val, sep_req_error);
	WRITE_REGISTER(drvdata->cc_base +
		HOST_SEP_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX),
		gpr_val);
}
/**
 * dx_sep_req_handler() - SeP request interrupt handler
 */
void dx_sep_req_handler(struct sep_drvdata *drvdata,
	int cause_id, void *priv_context)
{
	uint8_t agent_id;
	uint32_t gpr_val;
	uint32_t sep_req_error = DX_SEP_REQUEST_SUCCESS;
	uint32_t counter_val;
	uint32_t req_len;

	/* Read GPR3 value */
	gpr_val = READ_REGISTER(drvdata->cc_base +
		SEP_HOST_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX));

	/* Parse the new gpr value */
	counter_val = SEP_REQUEST_GET_COUNTER(gpr_val);
	agent_id = SEP_REQUEST_GET_AGENT_ID(gpr_val);
	req_len = SEP_REQUEST_GET_REQ_LEN(gpr_val);

	/* Increase the req_counter value in the state structure */
	sep_req_state.req_counter++;

	if (unlikely(counter_val != sep_req_state.req_counter))
		/* Verify new req_counter value is equal to the req_counter
		   value from the state. If not, proceed to critical error flow
		   below with error code SEP_REQUEST_OUT_OF_SYNC_ERR. */
		sep_req_error = DX_SEP_REQUEST_OUT_OF_SYNC_ERR;
	else if (unlikely((agent_id >= DX_SEP_REQUEST_MAX_AGENTS) ||
		(!sep_req_state.agent_valid[agent_id])))
		/* Verify that the SeP Req Agent ID is registered in the LUT, if
		   not proceed to critical error flow below with error code
		   SEP_REQUEST_INVALID_AGENT_ID_ERR. */
		sep_req_error = DX_SEP_REQUEST_INVALID_AGENT_ID_ERR;
	else if (unlikely(req_len > DX_SEP_REQUEST_BUF_SIZE))
		/* Verify the request length is not bigger than the maximum
		   allocated request buffer size. In bigger, proceed to critical
		   error flow below with the SEP_REQUEST_INVALID_REQ_SIZE_ERR
		   error code. */
		sep_req_error = DX_SEP_REQUEST_INVALID_REQ_SIZE_ERR;

	if (likely(sep_req_error == DX_SEP_REQUEST_SUCCESS))
		/* Signal the completion event according to the LUT */
		complete(&sep_req_state.agent_event[agent_id]);

	else {  /* Critical error flow */
		emit_error_response(drvdata, sep_req_error);
	}
}


/**
 * dx_sep_req_register_agent() - Regsiter an agent
 * @agent_id: The agent ID
 * @max_buf_size: A pointer to the max buffer size
 *
 * Returns int 0 on success
 */
int dx_sep_req_register_agent(uint8_t agent_id,
	uint32_t *max_buf_size)
{
	int rc = 0;

	SEP_LOG_DEBUG("Regsiter SeP Request agent (id=%d)\n",
		agent_id);

	/* Verify max_buf_size pointer is not NULL */
	if (max_buf_size == NULL) {
		SEP_LOG_ERR("max_buf_size is NULL\n");
		return -EINVAL;
	}
	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		SEP_LOG_ERR("Invalid agent ID\n");
		return -EINVAL;
	}

	mutex_lock(&sep_req_state.state_lock);
	/* Verify SeP Req Agent ID is not valid */
	if (sep_req_state.agent_valid[agent_id]) {
		SEP_LOG_ERR("Agent already registered\n");
		rc = -EINVAL;
		goto register_done;
	}
	/* Initialize agent state */
	sep_req_state.agent_valid[agent_id] = true;
	init_completion(&sep_req_state.agent_event[agent_id]);
	/* Return the request/response max buffer size */
	*max_buf_size = DX_SEP_REQUEST_BUF_SIZE;
register_done:
	mutex_unlock(&sep_req_state.state_lock);
	return rc;
}
EXPORT_SYMBOL(dx_sep_req_register_agent);


/**
 * dx_sep_req_unregister_agent() - Unregsiter an agent
 * @agent_id: The agent ID
 *
 * Returns int 0 on success
 */
int dx_sep_req_unregister_agent(uint8_t agent_id)
{
	int rc = 0;

	SEP_LOG_DEBUG("Unregsiter SeP Request agent (id=%d)\n",
		agent_id);

	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		SEP_LOG_ERR("Invalid agent ID (%d)\n", agent_id);
		return -EINVAL;
	}

	mutex_lock(&sep_req_state.state_lock);
	/* Verify SeP Req Agent ID is valid */
	if (sep_req_state.agent_valid[agent_id] == false) {
		SEP_LOG_ERR("Agent %u not registered\n", agent_id);
		rc = -EINVAL;
		goto unregister_done;
	}
	/* Invalidate agent */
	sep_req_state.agent_valid[agent_id] = false;
	/* Signal (release) another thread if pending on event */
	complete(&sep_req_state.agent_event[agent_id]);
	/* Release pending request if of this agent */
	if (sep_req_state.pending_req_agent_id == agent_id) {
		SEP_LOG_ERR("Unregistering agent %u with pending request!",
			agent_id);
		/* Dismiss the pending request with error */
		sep_req_state.pending_req_agent_id =
			DX_SEP_REQUEST_INVALID_AGENT_ID;
		emit_error_response(sep_req_state.drvdata,
			DX_SEP_REQUEST_INVALID_AGENT_ID_ERR);
	}
unregister_done:
	mutex_unlock(&sep_req_state.state_lock);
	return rc;
}
EXPORT_SYMBOL(dx_sep_req_unregister_agent);


/**
 * dx_sep_req_wait_for_request() - Wait from an incoming sep request
 * @agent_id: The agent ID
 * @sep_req_buf_p:	Pointer to the incoming request output buffer
 * @req_buf_size:	Pointer to the incoming request output buffer size
 *			This field is updated with the actual incoming buffer
 *			size, even if given buffer is too small.
 * @timeout:		Time to wait for an incoming request in jiffies
 *
 * Returns int 0 on success, -ENOMEM if not enough space in given
 * output buffer (output buffer would hold whatever fitted in it - the rest
 * of the request is dropped).
 */
int dx_sep_req_wait_for_request(uint8_t agent_id, uint8_t *sep_req_buf_p,
	uint32_t *req_buf_size, uint32_t timeout)
{
	int rc = 0;
	uint32_t gpr_val;
	uint32_t actual_req_size;
	uint32_t sep_req_err = DX_SEP_REQUEST_SUCCESS;

	SEP_LOG_DEBUG("Wait for sep request\n");
	SEP_LOG_DEBUG("agent_id=%d sep_req_buf_p=0x%08X timeout=%d\n",
		agent_id, (u32)sep_req_buf_p, timeout);

	/* Verify sep_req_buf_p pointer is not NULL */
	if (sep_req_buf_p == NULL) {
		SEP_LOG_ERR("sep_req_buf_p is NULL\n");
		return -EINVAL;
	}

	/* Verify req_buf_size pointer is not NULL */
	if (req_buf_size == NULL) {
		SEP_LOG_ERR("req_buf_size is NULL\n");
		return -EINVAL;
	}
	/* Verify *req_buf_size is not zero and not bigger than the
	   allocated request buffer */
	if ((*req_buf_size == 0) ||
	    (*req_buf_size > DX_SEP_REQUEST_BUF_SIZE)) {
		SEP_LOG_ERR("Invalid request buffer size = %u\n",
			*req_buf_size);
		return -EINVAL;
	}
	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		SEP_LOG_ERR("Invalid agent ID\n");
		return -EINVAL;
	}
	/* Verify SeP Req Agent ID is valid */
	if (!sep_req_state.agent_valid[agent_id]) {
		SEP_LOG_ERR("Agent %u is not registered\n", agent_id);
		return -ENODEV;
	}

	/* Wait for incoming request */
	rc = wait_for_completion_interruptible_timeout(
		&sep_req_state.agent_event[agent_id], timeout);
	if (rc == 0) { /* operation timed-out */
		SEP_LOG_DEBUG("Request wait timed-out\n");
		return -EAGAIN;
	} else if (rc == -ERESTARTSYS) {
		SEP_LOG_DEBUG("Request wait interrupted\n");
		return -ERESTART;
	} else if (rc < 0) {
		SEP_LOG_ERR("Unexpected error from wait_for_completion (%d)\n",
			rc);
		return rc;
	} else /* Event arrived before timeout (ret. val. is the time left) */
		rc = 0;

	mutex_lock(&sep_req_state.state_lock);
	if (!sep_req_state.agent_valid[agent_id]) {
		/* Verify that we are still valid */
		/* (in case woken up due to "unregister" call) */
		SEP_LOG_DEBUG("Woke up into invalid agent %d (unregistered)\n",
			agent_id);
		rc = -ENODEV;
		sep_req_err = DX_SEP_REQUEST_INVALID_AGENT_ID_ERR;
		goto wait_done;
	}
	gpr_val = READ_REGISTER(sep_req_state.sep_host_gpr_adr);
	/* If the request length is bigger than the callers' specified
	   buffer size, the request is partially copied to the callers'
	   buffer (only the first relevant bytes). The caller will not be
	   indicated for an error condition in this case. The remaining bytes
	   in the callers' request buffer are left as is without clearing.
	   If the request length is smaller than the callers' specified buffer
	   size, the relevant bytes from the allocated kernel request buffer
	   are copied to the callers' request buffer */
	actual_req_size = SEP_REQUEST_GET_REQ_LEN(gpr_val);
	if (*req_buf_size < actual_req_size) {
		SEP_LOG_DEBUG("Got request of size %u while given buffer is %u"
			      "\n", actual_req_size, *req_buf_size);
		rc = -ENOMEM;
		sep_req_err = DX_SEP_REQUEST_INVALID_REQ_SIZE_ERR;
	} else {/* Get actual request size */
		*req_buf_size = actual_req_size;
		sep_req_state.pending_req_agent_id = agent_id;
	}
	/* copy as much as possible even on -ENOMEM error */
	memcpy(sep_req_buf_p, sep_req_state.sep_req_buf_p, *req_buf_size);
	/* Before returning, update the actual size (-ENOMEM case) */
	/* (could be: if (rc == -ENOMEM) ...) */
	*req_buf_size = actual_req_size;
wait_done:
	if (sep_req_err != DX_SEP_REQUEST_SUCCESS)
		/* SeP waits - signal processing failure */
		emit_error_response(sep_req_state.drvdata, sep_req_err);
	mutex_unlock(&sep_req_state.state_lock);
	return rc;
}
EXPORT_SYMBOL(dx_sep_req_wait_for_request);


/**
 * dx_sep_req_send_response() - Send a response to the sep
 * @agent_id: The agent ID
 * @host_resp_buf_p: Pointer to the outgoing response buffer
 * @resp_buf_size: Pointer to the outgoing response size
 *
 * Returns int 0 on success
 */
int dx_sep_req_send_response(uint8_t agent_id, uint8_t *host_resp_buf_p,
	uint32_t resp_buf_size)
{
	int rc = 0;
	uint32_t gpr_val;

	SEP_LOG_DEBUG("Send host response\n");
	SEP_LOG_DEBUG("agent_id=%d host_resp_buf_p=0x%08X resp_buf_size=%d\n",
		agent_id, (u32)host_resp_buf_p, resp_buf_size);

	/* Verify host_resp_buf_p pointer is not NULL */
	if ((host_resp_buf_p == NULL) && (resp_buf_size > 0)) {
		SEP_LOG_ERR("host_resp_buf_p is NULL for buf_size=%u\n",
			resp_buf_size);
		return -EINVAL;
	}
	/* Verify resp_buf_size is not zero and not bigger than the allocated
	   request buffer */
	if (resp_buf_size > DX_SEP_REQUEST_BUF_SIZE) {
		SEP_LOG_ERR("Invalid response buffer size\n");
		return -EINVAL;
	}
	/* Validate agent ID is in range */
	if (agent_id >= DX_SEP_REQUEST_MAX_AGENTS) {
		SEP_LOG_ERR("Invalid agent ID\n");
		return -EINVAL;
	}

	mutex_lock(&sep_req_state.state_lock);
	/* Verify SeP Req Agent ID is valid */
	if (sep_req_state.agent_valid[agent_id] == false) {
		SEP_LOG_ERR("Agent %u not registered\n", agent_id);
		rc = -ENODEV;
		goto send_done;
	}
	/* Verify that a sep request is pending */
	if (sep_req_state.pending_req_agent_id != agent_id) {
		SEP_LOG_ERR("No requests are pending for agent %u\n",
			agent_id);
		rc = -ENOENT;
		goto send_done;
	}
	/* The request is copied from the callers' buffer to the global
	   response buffer, only up to the callers' response length */
	if (resp_buf_size > 0)
		memcpy(sep_req_state.host_resp_buf_p, host_resp_buf_p,
			resp_buf_size);

	/* Clear the request message buffer */
	memset(sep_req_state.sep_req_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);

	/* Clear the response message buffer for all remaining bytes
	   beyond the response buffer actual size */
	memset(sep_req_state.host_resp_buf_p+resp_buf_size, 0,
		DX_SEP_REQUEST_BUF_SIZE-resp_buf_size);

	/* Build the new GPR3 value out of the req_counter from the state,
	   the response length and the DX_SEP_REQUEST_SUCCESS return code.
	   Place the value in GPR3. */
	gpr_val = 0;
	SEP_REQUEST_SET_COUNTER(gpr_val, sep_req_state.req_counter);
	SEP_REQUEST_SET_RESP_LEN(gpr_val, resp_buf_size);
	SEP_REQUEST_SET_RETURN_CODE(gpr_val, DX_SEP_REQUEST_SUCCESS);
	sep_req_state.pending_req_agent_id = DX_SEP_REQUEST_INVALID_AGENT_ID;
	/* Invalidate pending req. agent ID before releasing SEP to next req. */
	WRITE_REGISTER(sep_req_state.host_sep_gpr_adr, gpr_val);
send_done:
	mutex_unlock(&sep_req_state.state_lock);
	return rc;
}
EXPORT_SYMBOL(dx_sep_req_send_response);


/**
 * dx_sep_req_get_sep_init_params() - Setup sep init params
 * @sep_request_params: The sep init parameters array
 */
void dx_sep_req_get_sep_init_params(uint32_t *sep_request_params)
{
	sep_request_params[0] = (uint32_t)sep_req_state.sep_req_buf_dma;
	sep_request_params[1] = (uint32_t)sep_req_state.host_resp_buf_dma;
	sep_request_params[2] = DX_SEP_REQUEST_BUF_SIZE;
}



/**
 * dx_sep_req_enable() - Enable the sep request interrupt handling
 * @drvdata: Driver private data
 */
void dx_sep_req_enable(struct sep_drvdata *drvdata)
{
	(void)register_cc_interrupt_handler(drvdata,
		SEP_HOST_GPR_IRQ_SHIFT(DX_SEP_REQUEST_GPR_IDX),
		dx_sep_req_handler, NULL);
}

/**
 * dx_sep_req_init() - Initialize the sep request state
 * @drvdata: Driver private data
 */
//int __devinit dx_sep_req_init(struct sep_drvdata *drvdata)
int dx_sep_req_init(struct sep_drvdata *drvdata)
{
	int rc = 0;
	int i;

	SEP_LOG_DEBUG("Initialize SeP Request state\n");

	sep_req_state.drvdata = drvdata;
	sep_req_state.pending_req_agent_id = DX_SEP_REQUEST_INVALID_AGENT_ID;
	sep_req_state.req_counter = 0;

	for (i = 0; i < DX_SEP_REQUEST_MAX_AGENTS; i++) {
		sep_req_state.agent_valid[i] = false;
		init_completion(&sep_req_state.agent_event[i]);
	}

	/* allocate coherent request buffer */
	sep_req_state.sep_req_buf_p = dma_alloc_coherent(drvdata->dev,
					DX_SEP_REQUEST_BUF_SIZE,
					&sep_req_state.sep_req_buf_dma,
					GFP_KERNEL);
	SEP_LOG_DEBUG("sep_req_buf_dma=0x%08X sep_req_buf_p=0x%p size=0x%08X\n",
		(u32)sep_req_state.sep_req_buf_dma,
		sep_req_state.sep_req_buf_p, DX_SEP_REQUEST_BUF_SIZE);
	if (sep_req_state.sep_req_buf_p == NULL) {
		SEP_LOG_ERR("Unable to allocate coherent request buffer\n");
		return -ENOMEM;
	}

	/* Clear the request buffer */
	memset(sep_req_state.sep_req_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);

	/* allocate coherent response buffer */
	sep_req_state.host_resp_buf_p = dma_alloc_coherent(drvdata->dev,
					DX_SEP_REQUEST_BUF_SIZE,
					&sep_req_state.host_resp_buf_dma,
					GFP_KERNEL);
	SEP_LOG_DEBUG("host_resp_buf_dma=0x%08X host_resp_buf_p=0x%p "
		      "size=0x%08X\n",
		(u32)sep_req_state.host_resp_buf_dma,
		sep_req_state.host_resp_buf_p, DX_SEP_REQUEST_BUF_SIZE);
	if (sep_req_state.host_resp_buf_p == NULL) {
		SEP_LOG_ERR("Unable to allocate coherent response buffer\n");
		return -ENOMEM;
	}

	/* Clear the response buffer */
	memset(sep_req_state.host_resp_buf_p, 0, DX_SEP_REQUEST_BUF_SIZE);
	mutex_init(&sep_req_state.state_lock);
	/* Setup the GPR address */
	sep_req_state.sep_host_gpr_adr = drvdata->cc_base +
		SEP_HOST_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX);

	sep_req_state.host_sep_gpr_adr = drvdata->cc_base +
		HOST_SEP_GPR_REG_OFFSET(DX_SEP_REQUEST_GPR_IDX);

	return rc;
}


/**
 * dx_sep_req_fini() - Finalize the sep request state
 * @drvdata: Driver private data
 */
void dx_sep_req_fini(struct sep_drvdata *drvdata)
{
	int i;

	SEP_LOG_DEBUG("Finalize SeP Request state\n");

	unregister_cc_interrupt_handler(drvdata,
		SEP_HOST_GPR_IRQ_SHIFT(DX_SEP_REQUEST_GPR_IDX));
	mutex_destroy(&sep_req_state.state_lock);
	sep_req_state.pending_req_agent_id = DX_SEP_REQUEST_INVALID_AGENT_ID;
	sep_req_state.req_counter = 0;
	for (i = 0; i < DX_SEP_REQUEST_MAX_AGENTS; i++)
		sep_req_state.agent_valid[i] = false;

	dma_free_coherent(drvdata->dev, DX_SEP_REQUEST_BUF_SIZE,
		sep_req_state.sep_req_buf_p, sep_req_state.sep_req_buf_dma);

	dma_free_coherent(drvdata->dev, DX_SEP_REQUEST_BUF_SIZE,
		sep_req_state.host_resp_buf_p, sep_req_state.host_resp_buf_dma);
}
