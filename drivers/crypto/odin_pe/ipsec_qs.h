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
/** @file: ipsec_q.h
 * IPsec packets queues dispatched for packet engine processing (Rx+Tx)
 * This module also registers iptables target for inbound/outbound packets */

#ifndef _IPSEC_QS_H_
#define _IPSEC_QS_H_

#include <linux/device.h>
#include "desc_mgr.h"

/**
 * Opaque handle to IPsec queues object (inbound + outbound)
 */
typedef void *ipsec_qs_h;
#define IPSEC_QS_INVALID_HANDLE NULL

/**
 * struct ipsec_info - Target instance private context (Target's parameter)
 * @spd_handle:	The SPD handle hint as propagated from the target parameter
 *		in the iptables entry.
 */
struct ipsec_info {
	uint8_t spd_handle;
};

struct sep_drvdata;

/**
 * ipsec_qs_create() - Create IPsec descriptors queue (inbound + outbound)
 *
 * @drvdata:		Associated device context
 * @log2_pkts_per_q:	Log2 of number of entries in each queue
 *
 * The queues are not activated or attached to Xtables until
 * ipsec_qs_activate() is invoked.
 */
ipsec_qs_h ipsec_qs_create(struct sep_drvdata *drvdata,
	unsigned int log2_pkts_per_q);

/**
 * ipsec_qs_get_base() - Get the DMA address of the base of the queues
 *
 * @qs_h:	The IPsec queues context
 *
 * This function is used by sep_init to inform SEP of queues location during
 * FW_INIT. The given buffer is the base of both queues, each of size
 * 1<<log2_q_size entries. Inbound packets queue is first.
 */
dma_addr_t ipsec_qs_get_base(ipsec_qs_h qs_h);

/**
 * ipsec_qs_activate() - Activate the queue.
 *
 * @qs_h:	The IPsec queues context
 *
 * Activate the queues after all other processing resources were set up.
 * This function attaches the XT targets to Xtables.
 * It should be invoked when the queue is inactive (after ipsec_qs_create).
 */
int ipsec_qs_activate(ipsec_qs_h qs_h);

/**
 * ipsec_qs_destroy() - Cleanup resources of IPsec queue and detach from Xtables
 * @qs_h:	The IPsec queues context
 */
void ipsec_qs_destroy(ipsec_qs_h qs_h);

/**
 * ipsec_qs_set_state() - Set queue state (SLEEP or ACTIVE)
 * @q_h:	The queue object handle
 * @state:	The requested state
 */
int ipsec_qs_set_state(ipsec_qs_h q_h, enum desc_q_state state);

/**
 * ipsec_qs_get_state() - Get queue state
 * @q_h:	The queue object handle
 */
enum desc_q_state ipsec_qs_get_state(ipsec_qs_h q_h);

/**
 * ipsec_qs_are_idle() - Report if given queue is active but empty/idle.
 * @q_h:		The queue object handle
 * @idle_jiffies_p:	Return jiffies at which the queue became idle
 */
bool ipsec_qs_are_idle(ipsec_qs_h q_h, unsigned long *idle_jiffies_p);

#endif /*_IPSEC_QS_H_*/
