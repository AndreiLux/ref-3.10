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
/* API for handling IKE events emitted from IPsec core in SeP. */

#ifndef __IKE_EVENT_H__
#define __IKE_EVENT_H__

/**
 * ipsec_ike_event_listener_init() - Initialize the listener to IKE events
 */
int ipsec_ike_event_listener_init(void);

/**
 * ipsec_get_next_ike_event() - Get the next IKE event
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
	uint32_t *event_msg_size, uint32_t *event_msg);

/**
 * ipsec_ike_event_listener_terminate() - Terminate the listener to IKE events
 */
void ipsec_ike_event_listener_terminate(void);

#endif /*__IKE_EVENT_H__*/
