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


#ifndef _SEP_IPSEC_DESC_H_
#define _SEP_IPSEC_DESC_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* Agent ID for "sep requests" agent that process IPSEC-to-IKE events */
#define IPSEC2IKE_EVENTS_AGENT_ID 1

/* GPR allocation for each packets queue */
#define SEP_IPSEC_INBOUND_GPR_IDX 1
#define SEP_IPSEC_OUTBOUND_GPR_IDX 2

/* Headroom/Tailroom for IPv4 only */
#define IP_HDR_SIZE 20
#define UDP_HDR_SIZE 8
#define IPSEC_HDR_SIZE 8 /* ESP header only supported */
#define MAX_IPSEC_IV_SIZE 16 /* AES IV */
#define SEP_IPSEC_HEADROOM_MIN (IP_HDR_SIZE + \
		IPSEC_HDR_SIZE + MAX_IPSEC_IV_SIZE + UDP_HDR_SIZE)

#define SEP_IPSEC_PADDING_OVERHEAD (15 + 2)
#define SEP_IPSEC_ICV_SIZE_MAX 12
#define SEP_IPSEC_TAILROOM_MIN \
		(SEP_IPSEC_PADDING_OVERHEAD + SEP_IPSEC_ICV_SIZE_MAX)

/* Value set by host if it has no hint on SPD handle/index */
/* We use 0 because this is the value of the "index" field in struct ipsecConf
   that denotes invalid index (see mocana/ipsec/ipsecconf.h:182) */
#define SEP_SPD_UNKNOWN 0

#define SEP_IPSEC_Q_DESC_SIZE_LOG2 3 /* log2(sizeof(struct sep_ipsec_desc)) */
/* Descriptor queue location functions. Assumes that queue size is power of 2 */
/**
 * SEP_IPSEC_Q_DESC_INDEX() - Get the index of a descriptor
 * @qdescs_num:	Number of descriptors in the queue cyclic buffer
 * @desc_cntr:	The (GPR) counter value of the descriptor we need
 */
#define SEP_IPSEC_Q_DESC_INDEX(qdescs_num, desc_cntr) \
	(((desc_cntr) & ((qdescs_num) - 1)))
/**
 * SEP_IPSEC_QUEUE_DESC_PTR() - Get the pointer to a descriptor
 * @qbase:	Base address of given queue
 * @qdescs_num:	Number of descriptors in the queue cyclic buffer
 * @desc_cntr:	The (GPR) counter value of the descriptor we need
 */
#define SEP_IPSEC_Q_DESC_PTR(qbase, qdescs_num, desc_cntr) \
	((struct sep_ipsec_desc *)(((uint32_t)(qbase)) + \
			(SEP_IPSEC_Q_DESC_INDEX(qdescs_num, desc_cntr) << \
			 SEP_IPSEC_Q_DESC_SIZE_LOG2)))

/**
 * struct ipsec_desc - The descriptor of an IP(sec) packet in the packet queue
 * @pkt_addr:	Host DMA address of given packet.
 * @pkt_offset:	Change in packet start location as applied by sep/ipsec.
 * @pkt_size:	Packet size in bytes. May be updated by sep.
 * @spd_handle:	Hint from host to sep on associated SPD, or SEP_SPD_UNKNOWN.
 * @ipsec_verdict:	Response from SEP on what to do with the packet
 *			(encoded with enum sep_ipsec_verdict)
 *
 * Note: For outbound queue, the given packet must have enough head/tail room
 *	for worst case as defined above:
 *	SEP_IPSEC_HEADROOM_MIN/SEP_IPSEC_TAILROOM_MIN
 */
struct sep_ipsec_desc {
	union {
		uint32_t pkt_addr;
		int32_t pkt_offset;
	};
	uint16_t pkt_size;
	union {
		uint8_t spd_handle;
		uint8_t ipsec_verdict;
	};
	uint8_t reserved;
};

/* IP packet verdict returned from SEP/PE */
enum sep_ipsec_verdict {
	SEP_IP_PKT_BYPASS,	/* Packet remained unchanged. Let it continue */
	SEP_IP_PKT_DROP,	/* Packet should be dropped */
	SEP_IP_PKT_IPSEC,	/* Packet was modified per-IPSEC processing */
	SEP_IP_PKT_ERROR	/* Packet processing error (...implies drop) */
};

#endif /*_SEP_IPSEC_DESC_H_*/

