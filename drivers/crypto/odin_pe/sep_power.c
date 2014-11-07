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
/**
 * This file implements the power state control functions for SeP/CC
 */

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_POWER
#include <linux/workqueue.h>
#include <linux/export.h>
#define DX_CC_HOST_VIRT /* must be defined before including dx_cc_regs.h */
#include "dx_driver.h"
#include "dx_cc_regs.h"
#include "dx_init_cc_abi.h"
#include "sep_sw_desc.h"
#include "dx_sep_kapi.h"
#include "sep_init.h"

#define SEP_STATE_CHANGE_TIMEOUT_MSEC 5000
/**
 * struct sep_power_control - Control data for power state change operations
 * @drvdata:		The associated driver context
 * @state_changed:	Completion object to signal state change
 * @last_state:		Recorded last state
 * @state_jiffies:	jiffies at recorded last state
 *
 * last_state and state_jiffies are volatile because may be updated in
 * the interrupt context while tested in _state_get function.
 */
struct sep_power_control {
	struct sep_drvdata *drvdata;
	struct completion state_changed;
	volatile enum dx_sep_state last_state;
	volatile unsigned long state_jiffies;
};

/* Global context for power management */
static struct sep_power_control power_control;

static const char *power_state_str(enum dx_sep_power_state pstate)
{
	switch (pstate) {
	case DX_SEP_POWER_INVALID: return "INVALID";
	case DX_SEP_POWER_OFF: return "OFF";
	case DX_SEP_POWER_BOOT: return "BOOT";
	case DX_SEP_POWER_IDLE: return "IDLE";
	case DX_SEP_POWER_ACTIVE: return "ACTIVE";
	case DX_SEP_POWER_HIBERNATED: return "HIBERNATED";
	}
	return "(unknown)";
}

/**
 * dx_sep_state_change_handler() - Interrupt handler for SeP state changes
 */
void dx_sep_state_change_handler(struct sep_drvdata *drvdata,
	int cause_id, void *priv_context)
{
	SEP_LOG_WARN("State=0x%08X Status/RetCode=0x%08X\n",
		READ_REGISTER(drvdata->cc_base + SEP_STATE_GPR_OFFSET),
		READ_REGISTER(drvdata->cc_base + SEP_STATUS_GPR_OFFSET));
	power_control.state_jiffies = jiffies;
	power_control.last_state = GET_SEP_STATE(drvdata);
	complete(&power_control.state_changed);
}

/**
 * dx_sep_wait_for_state() - Wait for SeP to reach one of the states reflected
 *				with given state mask
 * @state_mask:		The OR of expected SeP states
 * @timeout_msec:	Timeout of waiting for the state (in millisec.)
 *
 * Returns the state reached. In case of wait timeout the returned state
 * may not be one of the expected states.
 */
enum dx_sep_state dx_sep_wait_for_state(uint32_t state_mask, int timeout_msec)
{
	int wait_jiffies = msecs_to_jiffies(timeout_msec);
	enum dx_sep_state sep_state;

	do { /* Poll for state transition completion or failure */
		/* Arm for next state change before reading current state */
		INIT_COMPLETION(power_control.state_changed);
		sep_state = GET_SEP_STATE(power_control.drvdata);
		if ((sep_state & state_mask) || (wait_jiffies == 0))
			/* It's a match or wait timed out */
			break;
		wait_jiffies = wait_for_completion_timeout(
			&power_control.state_changed, wait_jiffies);
	} while (1);

	return sep_state;
}

/**
 * set_desc_qs_state() - Modify states of all Desc. queues
 *
 * @state:	Requested new state
 */
static int set_desc_qs_state(enum desc_q_state state)
{
	int i, rc;

	for (i = 0, rc = 0;
	      (i < power_control.drvdata->num_of_desc_queues) && (rc == 0);
	      i++)
		rc = desc_q_set_state(
		      power_control.drvdata->queue[i].desc_queue, state);

#ifdef PE_IPSEC_QUEUES
	if (likely(rc == 0))
		rc = ipsec_qs_set_state(power_control.drvdata->ipsec_qs, state);
#endif

	if (unlikely(rc != 0))
		/* Error - revert state of queues that were already changed */
		for (i--; i >= 0; i--)
			desc_q_set_state(
				power_control.drvdata->queue[i].desc_queue,
				(state == DESC_Q_ASLEEP) ?
				DESC_Q_ACTIVE : DESC_Q_ASLEEP);
	return rc;
}

static bool is_desc_qs_active(void)
{
	int i;
	enum desc_q_state qstate;

	for (i = 0; i < power_control.drvdata->num_of_desc_queues; i++) {
		qstate = desc_q_get_state(
			power_control.drvdata->queue[i].desc_queue);
		if (qstate != DESC_Q_ACTIVE)
			goto qs_are_inactive;
	}

#ifdef PE_IPSEC_QUEUES
	qstate = ipsec_qs_get_state(power_control.drvdata->ipsec_qs);
	if (qstate != DESC_Q_ACTIVE)
		goto qs_are_inactive;
#endif

	return true; /* All Qs are active */

qs_are_inactive:
	return false;
}

static bool is_desc_qs_idle(unsigned long *idle_jiffies_p)
{
	int i;
	unsigned long this_q_idle_jiffies;

	*idle_jiffies_p = 0; /* Max. of both queues if both idle */

	for (i = 0; i < power_control.drvdata->num_of_desc_queues; i++) {
		if (!desc_q_is_idle(
			power_control.drvdata->queue[i].desc_queue,
			&this_q_idle_jiffies))
			goto qs_are_active;

		if (this_q_idle_jiffies > *idle_jiffies_p)
			*idle_jiffies_p = this_q_idle_jiffies;
	}

#ifdef PE_IPSEC_QUEUES
	if (!ipsec_qs_are_idle(power_control.drvdata->ipsec_qs,
				&this_q_idle_jiffies))
		goto qs_are_active;

	if (this_q_idle_jiffies > *idle_jiffies_p)
		*idle_jiffies_p = this_q_idle_jiffies;
#endif

	return true; /* Qs are idle */

qs_are_active:
	return false;
}

#ifndef PE_IPSEC_QUEUES
#warning Remove for CC54-generic
/**
 * reset_desc_qs() - Initiate clearing of desc. queue counters
 * This function must be called only when transition to hibernation state
 * is completed successfuly, i.e., the desc. queue is empty and asleep
 */
static void reset_desc_qs(void)
{
	int i;

	for (i = 0; i < power_control.drvdata->num_of_desc_queues; i++)
		(void)desc_q_reset(power_control.drvdata->queue[i].desc_queue);
}
#endif

static int process_hibernation_req(void)
{
	enum dx_sep_state sep_state;
	struct sep_op_ctx op_ctx;
	int rc;
	sep_state = GET_SEP_STATE(power_control.drvdata);
	if (sep_state != DX_SEP_STATE_DONE_FW_INIT) {
		SEP_LOG_ERR("Requested hibernation while SeP state=0x%08X\n",
			sep_state);
		printk(KERN_INFO "Requested hibernation while SeP state=0x%08X\n", sep_state);
		return -EINVAL;
	}
	else {
		printk(KERN_INFO "[process_hibernation_req] GET_SEP_STATE=%d\n", sep_state);
	}
	rc = set_desc_qs_state(DESC_Q_ASLEEP);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed moving queues to SLEEP state (%d)\n", rc);
		printk(KERN_INFO "Failed moving queues to SLEEP state (%d)\n", rc);
		return rc;
	}
	else {
		printk(KERN_INFO "[process_hibernation_req] set_desc_qs_state returned 1\n");
	}
	op_ctx_init(&op_ctx, NULL);
	op_ctx.op_type = SEP_OP_SLEEP;
	rc = desc_q_enqueue_sleep_req(
		power_control.drvdata->queue[0].desc_queue,/* Always on Q0 */
		&op_ctx);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed dispatching hibernation request (%d)\n",
			rc);
		printk(KERN_INFO "Failed dispatching hibernation request (%d)\n", rc);
	} else {
		/* Wait for request descriptor to complete */
		wait_for_completion(&op_ctx.ioctl_op_compl);
		if (op_ctx.error_info != SEPSLP_MODE_REQ_RET_OK) {
			SEP_LOG_ERR("SLEEP_REQ failed (0x%08X)\n",
				op_ctx.error_info);
			printk(KERN_INFO "SLEEP_REQ failed (0x%08X)\n",op_ctx.error_info);
			if ((op_ctx.error_info == SEPSLP_MODE_REQ_EBUSY) ||
			    (op_ctx.error_info == SEPSLP_MODE_REQ_EABORT))
				/* SeP activity interrupted request */
				rc = -EBUSY;
			else	/* Unexpected error in SeP */
				rc = -EIO;
		}
	}
	if (likely(rc == 0)) { /* Process state change */
		sep_state = dx_sep_wait_for_state(
			DX_SEP_STATE_DONE_FW_INIT |
			DX_SEP_STATE_DONE_SLEEP_MODE,
			SEP_STATE_CHANGE_TIMEOUT_MSEC);
		if (sep_state == DX_SEP_STATE_DONE_FW_INIT) {
			SEP_LOG_ERR("Transition to SLEEP mode aborted.\n");
			printk(KERN_INFO "Transition to SLEEP mode aborted.\n");
			rc = -EBUSY;
		} else if (sep_state != DX_SEP_STATE_DONE_SLEEP_MODE) {
			if (sep_state == DX_SEP_STATE_PROC_SLEEP_MODE) {
				//SEP_LOG_ERR("Stuck in processing of SLEEP "req.\n");
				printk(KERN_INFO "Stuck in processing of SLEEP req.\n");
				rc = -ETIME;
			} else {
				//SEP_LOG_ERR("Unexpected SeP state after SLEEP "
				//	    "request: 0x%08X\n", sep_state);
				printk(KERN_INFO "Unexpected SeP state after SLEEP request: 0x%08X\n", sep_state);
				rc = -EBUG;
			}
		}
	}
	if (unlikely(rc != 0)) {
		sep_state = GET_SEP_STATE(power_control.drvdata);
		if (sep_state == DX_SEP_STATE_DONE_FW_INIT)
			/* Revert queues state on failure */
			/* (if remained on active state)  */
			set_desc_qs_state(DESC_Q_ACTIVE);
	} else {
#ifndef PE_IPSEC_QUEUES
#warning Remove for CC54-generic
		reset_desc_qs();
#endif
	}
	op_ctx_fini(&op_ctx);
	return rc;
}

static int process_activate_req(void)
{
	enum dx_sep_state sep_state;
	int rc;
	
	printk(KERN_INFO "[process_activate_req] !\n");	

	sep_state = GET_SEP_STATE(power_control.drvdata);

	printk(KERN_INFO "[process_activate_req] after GET_SEP_STATE: %d\n", sep_state);

	if ((sep_state != DX_SEP_STATE_PROC_WARM_BOOT) &&
	    (sep_state != DX_SEP_STATE_DONE_WARM_BOOT)) {
		//SEP_LOG_INFO("Requested activation while SeP state=0x%08X\n",
			//sep_state);
		printk(KERN_INFO "[process_activate_req] Requested activation while SeP state=0x%08X\n", sep_state);

		return -EINVAL;
	}
	if ((sep_state == DX_SEP_STATE_DONE_FW_INIT) && is_desc_qs_active()) {
		//SEP_LOG_INFO("Requested activation when in active state\n");
		printk(KERN_INFO "[process_activate_req] Requested activation when in active state\n");
		return -EINVAL; /* Already in this state */
	}

#ifndef PE_IPSEC_QUEUES
#warning Remove for CC54-generic
	/* SeP may have been reset - restore IMR if SeP is not off */
	/* This must be done before dx_sep_wait_for_state() */
	WRITE_REGISTER(power_control.drvdata->cc_base +
		DX_CC_REG_OFFSET(HOST, IMR), power_control.drvdata->irq_mask);
#endif
	/* Wait for DONE_WARM_BOOT */
	sep_state = dx_sep_wait_for_state(DX_SEP_STATE_DONE_WARM_BOOT,
		SEP_STATE_CHANGE_TIMEOUT_MSEC);
	if (sep_state != DX_SEP_STATE_DONE_WARM_BOOT) {
		rc = -ETIME;
		goto activation_failed;
	}

	/* Fire SEP "warm-boot" request */
	rc = sepinit_do_fw_init(power_control.drvdata, false);
	if (likely(rc == 0))
		rc = set_desc_qs_state(DESC_Q_ACTIVE);

activation_failed:
	return rc;
}

/**
 * dx_sep_power_state_set() - Change power state of SeP (CC)
 *
 * @req_state:	The requested power state (_HIBERNATED or _ACTIVE)
 *
 * Request changing of power state to given state and block until transition
 * is completed.
 * Requesting _HIBERNATED is allowed only from _ACTIVE state.
 * Requesting _ACTIVE is allowed only after CC was powered back on (warm boot).
 * Return codes:
 * 0 -	Power state change completed.
 * -EINVAL -	This request is not allowed in current SeP state or req_state
 *		value is invalid.
 * -EBUSY -	State change request ignored because SeP is busy (primarily,
 *		when requesting hibernation while SeP is processing something).
 * -ETIME -	Request timed out (primarily, when asking for _ACTIVE)
 */
int dx_sep_power_state_set(enum dx_sep_power_state req_state)
{
	int rc = 0;

	switch (req_state) {
	case DX_SEP_POWER_HIBERNATED:
		printk(KERN_INFO "[dx_sep_power_state_set]:: DX_SEP_POWER_HIBERNATED\n");
		rc = process_hibernation_req();
		break;
	case DX_SEP_POWER_IDLE:
	case DX_SEP_POWER_ACTIVE:
		printk(KERN_INFO "[dx_sep_power_state_set]:: DX_SEP_POWER_ACTIVE\n");
		rc = process_activate_req();
		break;
	default:
		SEP_LOG_ERR("Invalid state to request (%s)\n",
			power_state_str(req_state));
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(dx_sep_power_state_set);

/**
 * dx_sep_power_state_get() - Get the current power state of SeP (CC)
 * @state_jiffies_p:	The "jiffies" value at which given state was detected.
 */
enum dx_sep_power_state dx_sep_power_state_get(unsigned long *state_jiffies_p)
{
	enum dx_sep_state sep_state;
	enum dx_sep_power_state rc;
	unsigned long idle_jiffies;

	sep_state = GET_SEP_STATE(power_control.drvdata);
	if (sep_state != power_control.last_state) {
		/* Probably off or after warm-boot with lost IMR */
		/* Recover last_state */
		power_control.last_state = sep_state;
		power_control.state_jiffies = jiffies;
	}
	if (state_jiffies_p != NULL)
		*state_jiffies_p = power_control.state_jiffies;
	switch (sep_state) {
	case DX_SEP_STATE_PROC_WARM_BOOT:
		/*FALLTRHOUGH*/
	case DX_SEP_STATE_DONE_WARM_BOOT:
		rc = DX_SEP_POWER_BOOT;
		break;
	case DX_SEP_STATE_DONE_FW_INIT:
		if (is_desc_qs_active()) {
			if (is_desc_qs_idle(&idle_jiffies)) {
				rc = DX_SEP_POWER_IDLE;
				if (state_jiffies_p != NULL)
					*state_jiffies_p = idle_jiffies;
			} else
				rc = DX_SEP_POWER_ACTIVE;
		} else
			/* SeP was woken up but dx_sep_power_state_set was not
			   invoked to activate the queues */
			rc = DX_SEP_POWER_BOOT;
		break;
	case DX_SEP_STATE_PROC_SLEEP_MODE:
		/* Report as active until actually asleep */
		rc = DX_SEP_POWER_ACTIVE;
		break;
	case DX_SEP_STATE_DONE_SLEEP_MODE:
		rc = DX_SEP_POWER_HIBERNATED;
		break;
	case DX_SEP_STATE_OFF:
		rc = DX_SEP_POWER_OFF;
		break;
	case DX_SEP_STATE_FATAL_ERROR:
		/*FALLTRHOUGH*/
	default: /* Any state not supposed to happen for the driver */
		rc = DX_SEP_POWER_INVALID;
	}
	return rc;
}
EXPORT_SYMBOL(dx_sep_power_state_get);

/**
 * dx_sep_power_init() - Init resources for this module
 */
//void __devinit dx_sep_power_init(struct sep_drvdata *drvdata)
void dx_sep_power_init(struct sep_drvdata *drvdata)
{
	power_control.drvdata = drvdata;
	init_completion(&power_control.state_changed);
	/* Init. recorded last state */
	power_control.last_state = GET_SEP_STATE(drvdata);
	power_control.state_jiffies = jiffies;
	(void)register_cc_interrupt_handler(drvdata,
		SEP_HOST_GPR_IRQ_SHIFT(DX_SEP_STATE_GPR_IDX),
		dx_sep_state_change_handler, NULL);
}

/**
 * dx_sep_power_exit() - Cleanup resources for this module
 */
void dx_sep_power_exit(struct sep_drvdata *drvdata)
{
}


