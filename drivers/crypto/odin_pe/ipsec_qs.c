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
/** @file: ipsec_qs.c
 * IPsec packets queue dispatched for packet engine processing (Rx+Tx) */

/* "short-circuit" on SEP to allow testing with no SEP FW */
/*#define DEBUG_HOST_LOOPBACK */

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_IPSEC_QS

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <linux/spinlock.h>
#include <linux/module.h>

#include "sep_log.h"
#include "dx_driver.h"
#include "sep_ipsec_desc.h"
#include "sep_sysfs.h"
#include "ipsec_qs.h"

#ifdef CONFIG_NOT_COHERENT_CACHE
/* Reserve extra cache line around the packet buffer to avoid cache coherency
   issues when PE access the buffer of the packet on a cache line that may
   be accessed simultaneously from the CPU side. */
#define PKT_HEADROOM_MIN (SEP_IPSEC_HEADROOM_MIN + L1_CACHE_BYTES)
#define PKT_TAILROOM_MIN (SEP_IPSEC_TAILROOM_MIN + L1_CACHE_BYTES)

#else /* Memory is coherent in respect to PE device (e.g., ACP)*/
#define PKT_HEADROOM_MIN (SEP_IPSEC_HEADROOM_MIN)
#define PKT_TAILROOM_MIN (SEP_IPSEC_TAILROOM_MIN)
#endif

/* Uncommnet to record IN/OUT packets to/from F/W.
 *  Caution - this may significantly slow down the system */
/*#define RECORD_PACKET*/
#if defined(RECORD_PACKET) && defined(DEBUG)
#define DUMP_PACKET(pkt_addr, pkt_size) \
		dump_byte_array("packet", pkt_addr, pkt_size)
#else
#define DUMP_PACKET(pkt_addr, pkt_size) do {} while (0)
#endif

/* UDP port of ISAKMP should be always bypassed to reach the IKE application */
#define ISAKMP_UDP_PORT 500

/* Calculate buffer size for both queues:
   Number of packets * descriptor size * 2 queues */
#define GET_QS_BUFFER_SIZE(_log2_pkts_per_q) \
	(1 << ((_log2_pkts_per_q) + SEP_IPSEC_Q_DESC_SIZE_LOG2 + 1))

/* Debugging macros */
#define GET_IP_ADDR_BYTE(addr, idx) (((uint8_t *)&(addr))[idx])
#define GET_IP_ADDR_BYTES(addr) \
	GET_IP_ADDR_BYTE(addr, 0), GET_IP_ADDR_BYTE(addr, 1), \
	GET_IP_ADDR_BYTE(addr, 2), GET_IP_ADDR_BYTE(addr, 3)

enum ipsec_q_id {
	IPSEC_Q_INBOUND = 0,
	IPSEC_Q_OUTBOUND = 1,
	IPSEC_QS_NUM
};

#define _GET_Q_PENDING_DESCS(q_p) \
	((q_p)->dispatched_cntr - (q_p)->completed_cntr)

/* Checks if any pending descriptors for inbound or outbound
   Queues. Returns "0" if both Qs are empty "1" otherwise.    */
#define ARE_QS_EMPTY(qs_p) ( \
	(_GET_Q_PENDING_DESCS(&(qs_p)->qs_ctx[IPSEC_Q_INBOUND]) == 0) && \
	(_GET_Q_PENDING_DESCS(&(qs_p)->qs_ctx[IPSEC_Q_OUTBOUND]) == 0))

#define SPIN_LOCK_QS(q_p) \
	do { \
		spin_lock_bh(&((q_p)->qs_ctx[IPSEC_Q_OUTBOUND].q_lock)); \
		spin_lock_bh(&((q_p)->qs_ctx[IPSEC_Q_INBOUND].q_lock)); \
	} while (0)

#define SPIN_UNLOCK_QS(q_p) \
	do { \
		spin_unlock_bh(&((q_p)->qs_ctx[IPSEC_Q_INBOUND].q_lock)); \
		spin_unlock_bh(&((q_p)->qs_ctx[IPSEC_Q_OUTBOUND].q_lock)); \
	} while (0)

/* Marker values to provide verdict of SKBs that were already processed by us */
enum ipsec_q_verdict_mark {
	IPSEC_Q_VERDICT_NOP = 0, /* Before any processing */
	IPSEC_Q_VERDICT_BYPASS = 1 + SEP_IP_PKT_BYPASS, /* XT_RETURN */
	IPSEC_Q_VERDICT_DROP = 1 + SEP_IP_PKT_DROP,	/* NF_DROP */
	IPSEC_Q_VERDICT_IPSEC = 1 + SEP_IP_PKT_IPSEC    /* XT_RETURN */
};

static unsigned int ipsec_clean_mark_tg(
	struct sk_buff *skb, const struct xt_action_param *par);
static bool ipsec_bypass_mt(
	const struct sk_buff *skb, struct xt_action_param *par);
static bool ipsec_drop_mt(
	const struct sk_buff *skb, struct xt_action_param *par);

/**
 * ipsec_clean_mark_tg_reg - Registration record for a target to clean up
 *	IPsec verdict marks before letting a packet continue in the iptables.
 */
static struct xt_target ipsec_clean_mark_tg_reg __read_mostly = {
	.name           = "DX_IPSEC_CLEAN_MARK",
	.table          = "mangle",
	.family         = NFPROTO_IPV4,
	.hooks          = (1 << NF_INET_PRE_ROUTING) |
			(1 << NF_INET_POST_ROUTING),
	.target         = ipsec_clean_mark_tg,
	.targetsize     = 0,
	.me             = THIS_MODULE,
};

/**
 * ipsec_bypass_mt_reg - Registration record for IPsec bypass match handler
 * Match of this filter should result in "-j RETURN"
 * This filter matches skb->mark == IPSEC_Q_VERDICT_BYPASS and also
 * IPSEC_Q_VERDICT_IPSEC (Assuming PE IPsec processing was final)
 */
static struct xt_match ipsec_bypass_mt_reg __read_mostly = {
	.name           = "dx_ipsec_bypass",
	.table          = "mangle",
	.family         = NFPROTO_IPV4,
	.hooks          = (1 << NF_INET_PRE_ROUTING) |
			(1 << NF_INET_POST_ROUTING),
	.match          = ipsec_bypass_mt,
	.matchsize      = 0,
	.me             = THIS_MODULE,
};

/**
 * ipsec_drop_mt_reg - Registration record for IPsec drop match handler
 * Match of this filter should result in "-j DROP"
 * This filter matches skb->mark == IPSEC_Q_VERDICT_DROP and any invalid verdict
 */
static struct xt_match ipsec_drop_mt_reg __read_mostly = {
	.name           = "dx_ipsec_drop",
	.table          = "mangle",
	.family         = NFPROTO_IPV4,
	.hooks          = (1 << NF_INET_PRE_ROUTING) |
			(1 << NF_INET_POST_ROUTING),
	.match          = ipsec_drop_mt,
	.matchsize      = 0,
	.me             = THIS_MODULE,
};

/**
 * struct ipsec_q - Single IPsec queue context
 * @qid:	Queue ID (inbound or outbound)
 * @parent:	Owning ipsec_qs object/device
 * @q_lock:	Spinlock to prevent contention in multi-core multi-NIC
 *		environment
 * @q_base_p:	Pointer to to this queue
 * @q_skbs_p:		Auxilliary queue of the SKBs associated with each packet
 *			(a packet in descriptor X in the queue is associated
 *			with an SKB at the same offset in q_skbs_p)
 * @q_pkts_dma:		Auxilliary queue of the packets DMA addresses
 * @dispatched_cntr:	Number of packets dispatched per queue (host2sep GPR)
 * @completed_cntr:	Number of packets completed per queue  (sep2host GPR)
 * @gpr_to_sep:		Pointer to host-to-SEP GPR of this queue
 * @gpr_from_sep:	Pointer to SEP-to-host GPR of this queue
 * @completion_worker:	Work queue context for processing completions
 * @ipsec_target:	The Xtables target for this queue
 */
struct ipsec_q {
	enum ipsec_q_id qid;
	struct ipsec_qs *parent;
	spinlock_t q_lock;
	struct sep_ipsec_desc *q_base_p;
	struct sk_buff **q_skbs_p;
	dma_addr_t *q_pkts_dma;
	uint32_t dispatched_cntr;
	uint32_t completed_cntr;
	void __iomem *gpr_to_sep;
	void __iomem *gpr_from_sep;
	struct work_struct completion_worker;
	struct xt_target ipsec_target;
};

/**
 * struct ipsec_qs - Object of IPsec packets queues
 * @dev:		Device context
 * @qs_base_dma:	DMA address of queues' buffer (combind in/out)
 * @qs_base_virt:	Pointer to queues' buffer
 * @descs_per_q:	Number of descriptors per queue
 * @activated:		Indicate if the queues were activated
 *			(for proper cleanup)
 */
struct ipsec_qs {
	struct sep_drvdata *drvdata;
	dma_addr_t qs_base_dma;
	void *qs_base_p;
	unsigned int log2_pkts_per_q;
	struct ipsec_q qs_ctx[IPSEC_QS_NUM];
	enum desc_q_state qstate;
	unsigned long idle_jiffies;
};

static const int q_cause_id[IPSEC_QS_NUM] = {
	SEP_HOST_GPR_IRQ_SHIFT(SEP_IPSEC_INBOUND_GPR_IDX),
	SEP_HOST_GPR_IRQ_SHIFT(SEP_IPSEC_OUTBOUND_GPR_IDX)
};

/* LUT for GPRs registers offsets (to be added to cc_regs_base)
   The index into this table is the q_id.                       */
static const unsigned long host_to_sep_gpr_offset[IPSEC_QS_NUM] = {
	HOST_SEP_GPR_REG_OFFSET(SEP_IPSEC_INBOUND_GPR_IDX),
	HOST_SEP_GPR_REG_OFFSET(SEP_IPSEC_OUTBOUND_GPR_IDX),
};
static const unsigned long sep_to_host_gpr_offset[] = {
	SEP_HOST_GPR_REG_OFFSET(SEP_IPSEC_INBOUND_GPR_IDX),
	SEP_HOST_GPR_REG_OFFSET(SEP_IPSEC_OUTBOUND_GPR_IDX),
};


/* Xtables targets entry points */
static unsigned int ipsec_q_tg(
	struct sk_buff *skb, const struct xt_action_param *par);
static int ipsec_q_tg_check(const struct xt_tgchk_param *par);
static void ipsec_q_tg_destroy(const struct xt_tgdtor_param *par);

static void ipsec_q_completion_handler(struct work_struct *work);
static void ipsec_q_interrupt_handler(struct sep_drvdata *drvdata,
	int cause_id, void *priv_context);

static int ipsec_qs_deactivate(struct ipsec_qs *qs_p);

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
	unsigned int log2_pkts_per_q)
{
	struct device *dev = drvdata->dev;
	struct ipsec_qs *new_qs;
	struct ipsec_q *cur_q;
	int i;

	/* Verify size macro value */
	BUILD_BUG_ON(sizeof(struct sep_ipsec_desc) !=
		(1 << SEP_IPSEC_Q_DESC_SIZE_LOG2));

	new_qs = kzalloc(sizeof(struct ipsec_qs), GFP_KERNEL);
	if (unlikely(new_qs == NULL)) {
		SEP_LOG_ERR("Failed allocating IPsec queues object\n");
		goto create_err;
	}
	new_qs->drvdata = drvdata;
	new_qs->log2_pkts_per_q = log2_pkts_per_q;
	/* allocate descriptor queues buffer */
	new_qs->qs_base_p = dma_alloc_coherent(dev,
		GET_QS_BUFFER_SIZE(log2_pkts_per_q) , &new_qs->qs_base_dma,
		GFP_KERNEL);
	if (unlikely(new_qs->qs_base_p == NULL)) {
		SEP_LOG_ERR("Failed allocating two queus of %d entries\n",
			    1 << log2_pkts_per_q);
		goto create_err;
	}
	/* Initiazlize each queue */
	for (cur_q = new_qs->qs_ctx , i = 0; i < IPSEC_QS_NUM; i++ , cur_q++) {
		cur_q->q_skbs_p = kmalloc(/* SKB pointer per pending packet */
			sizeof(struct sk_buff *) << log2_pkts_per_q,
			GFP_KERNEL);
		if (cur_q->q_skbs_p == NULL) {
			SEP_LOG_ERR("Failed allocating aux. SKB queue (%d pkts)"
				    "\n", 1 << log2_pkts_per_q);
			goto create_err;
		}
		cur_q->q_pkts_dma = kmalloc(
			sizeof(dma_addr_t) << log2_pkts_per_q, GFP_KERNEL);
		if (cur_q->q_pkts_dma == NULL) {
			SEP_LOG_ERR("Failed allocating aux. DMA queue (%d pkts)"
				    "\n", 1 << log2_pkts_per_q);
			goto create_err;
		}
		cur_q->qid = i;
		cur_q->parent = new_qs;
		/* Initialize Xt target context */
		/* No real reason to make "name" field a const if the rest
		   of the fields in struct xt_target are not... We need to
		   cast to non-const char* in order to initialize.
		   (since the whole structure is not allocated in a read-only
		   memory, there is no real problem with this) */
		snprintf((char *)cur_q->ipsec_target.name,
			XT_EXTENSION_MAXNAMELEN,
			"DX_IPSEC_%s", (i == IPSEC_Q_INBOUND) ? "IN" : "OUT");
		cur_q->ipsec_target.family = NFPROTO_IPV4;
		cur_q->ipsec_target.hooks =
			(i == IPSEC_Q_INBOUND) ? (1 << NF_INET_PRE_ROUTING) :
			(1 << NF_INET_POST_ROUTING);
		cur_q->ipsec_target.target = ipsec_q_tg;
		cur_q->ipsec_target.targetsize = sizeof(struct ipsec_info);
		cur_q->ipsec_target.table = "mangle";
		cur_q->ipsec_target.checkentry = ipsec_q_tg_check;
		cur_q->ipsec_target.destroy = ipsec_q_tg_destroy;
		cur_q->ipsec_target.me = THIS_MODULE;
		/* Calculate and save base for each queue */
		cur_q->q_base_p = new_qs->qs_base_p +
			(i << (log2_pkts_per_q + SEP_IPSEC_Q_DESC_SIZE_LOG2));
		cur_q->gpr_to_sep = drvdata->cc_base +
			host_to_sep_gpr_offset[i];
#ifdef DEBUG_HOST_LOOPBACK
#warning Compiled with DEBUG_HOST_LOOPBACK (internal host loopback, bypass SEP)
		/* Cause reading what we written into the GPR */
		cur_q->gpr_from_sep = cur_q->gpr_to_sep;
#else
		cur_q->gpr_from_sep = drvdata->cc_base +
			sep_to_host_gpr_offset[i];
#endif
		INIT_WORK(&cur_q->completion_worker,
			ipsec_q_completion_handler);
		spin_lock_init(&cur_q->q_lock);
		/* Initialize counters */
		cur_q->dispatched_cntr = 0;
		cur_q->completed_cntr = 0;
		WRITE_REGISTER(cur_q->gpr_to_sep, 0);
	}
	SEP_LOG_DEBUG("Created ipsec_qs@%p (log2_pkts_per_q=%u)\n",
		new_qs, log2_pkts_per_q);

	new_qs->qstate = DESC_Q_INACTIVE;
	return (ipsec_qs_h)new_qs;

create_err:
	if (new_qs != NULL) {
		for (cur_q = new_qs->qs_ctx , i = 0;
		      i < IPSEC_QS_NUM;
		      i++ , cur_q++) {
			if (cur_q->q_pkts_dma != NULL)
				kfree(cur_q->q_pkts_dma);
			if (cur_q->q_skbs_p != NULL)
				kfree(cur_q->q_skbs_p);
		}
		kfree(new_qs);
	}
	return IPSEC_QS_INVALID_HANDLE;
}

/**
 * ipsec_qs_get_base() - Get the DMA address of the base of the queues
 *
 * @q_h:	The IPsec queues context
 *
 * This function is used by sep_init to inform SEP of queues location during
 * FW_INIT. The given buffer is the base of both queues, each of size
 * 1<<log2_q_size entries. Inbound packets queue is first.
 */
dma_addr_t ipsec_qs_get_base(ipsec_qs_h qs_h)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)qs_h;

	return qs_p->qs_base_dma;
}

/**
 * ipsec_qs_destroy() - Cleanup resources of IPsec queue and detouch from Xtables
 */
void ipsec_qs_destroy(ipsec_qs_h qs_h)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)qs_h;
	int i;

	if (qs_p == NULL) {
		/* Possible if SEP reported queues size 0 */
		SEP_LOG_DEBUG("Invoked for invalid handle\n");
		return;
	}
	ipsec_qs_deactivate(qs_p);
	/* Sanity check */
	for (i = 0; i < IPSEC_QS_NUM; i++) {
		if (qs_p->qs_ctx[i].dispatched_cntr !=
		    qs_p->qs_ctx[i].completed_cntr) {
			/* Not supposed to happen if was able to unregister
			   the Xtables target */
			SEP_LOG_ERR("%s queue have %u pending packets!\n",
				(i == IPSEC_Q_INBOUND) ? "Inbound" : "Outbound",
				qs_p->qs_ctx[i].dispatched_cntr -
				qs_p->qs_ctx[i].completed_cntr);
		}
	}
	dma_free_coherent(qs_p->drvdata->dev,
		GET_QS_BUFFER_SIZE(qs_p->log2_pkts_per_q),
		qs_p->qs_base_p, qs_p->qs_base_dma);
	kfree(qs_p);
}


/*** Queues state handlers ***/

static inline bool ipsec_q_is_full(struct ipsec_q *q_p)
{
	return ((q_p->dispatched_cntr - q_p->completed_cntr) >=
	    (1 << q_p->parent->log2_pkts_per_q));
}

/**
 * ipsec_qs_are_idle() - Report if given queue is active but empty/idle.
 * @q_h:		The queue object handle
 * @idle_jiffies_p:	Return jiffies at which the queue became idle
 */
bool ipsec_qs_are_idle(ipsec_qs_h q_h, unsigned long *idle_jiffies_p)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)q_h;
	if (idle_jiffies_p != NULL)
		*idle_jiffies_p = qs_p->idle_jiffies;
	/* No need to lock the queue - returned information is "fluid" anyway */
	return ((qs_p->qstate == DESC_Q_ACTIVE) &&
		(ARE_QS_EMPTY(qs_p)));
}

/**
 * ipsec_qs_activate() - Activate the queue.
 *
 * @q_h:	The IPsec queues context
 *
 * Activate the queues after all other processing resources were set up.
 * This function attaches the XT targets to Xtables.
 */
int ipsec_qs_activate(ipsec_qs_h qs_h)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)qs_h;
	int rc, i;

	if (qs_p == NULL) {
		SEP_LOG_ERR("Invoked for invalid handle\n");
		return -EINVAL;
	}

	if (qs_p->qstate != DESC_Q_INACTIVE)
		return 0; /* Already active */

#ifndef DEBUG_HOST_LOOPBACK
	/* Register interrupt handlers (Ignored with host loopback test) */
	for (i = 0; i < IPSEC_QS_NUM; i++) {
		rc = register_cc_interrupt_handler(qs_p->drvdata, q_cause_id[i],
			ipsec_q_interrupt_handler, qs_p->qs_ctx + i);
		if (unlikely(rc != 0)) {
			SEP_LOG_ERR("Failed registering interrupt handler for "
				    "%s queue\n",
				i == 0 ? "inbound" : "outbound");
			goto register_cc_interrupt_err;
		}
	}
#endif
	/* register Xtables matches */
	rc = xt_register_match(&ipsec_bypass_mt_reg);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed registering IPsec bypass match\n");
		goto register_cc_interrupt_err;
	}
	rc = xt_register_match(&ipsec_drop_mt_reg);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed registering IPsec drop match\n");
		goto register_match_drop_err;
	}
	/* register Xtables targets */
	rc = xt_register_target(&ipsec_clean_mark_tg_reg);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed registering IPsec clean mark target\n");
		goto register_clean_mark_err;
	}
	for (i = 0; i < IPSEC_QS_NUM; i++) {
		rc = xt_register_target(&qs_p->qs_ctx[i].ipsec_target);
		if (unlikely(rc != 0)) {
			SEP_LOG_ERR("Failed registering Xtables target %s\n",
				qs_p->qs_ctx[i].ipsec_target.name);
			goto register_xt_target_err;
		}
		SEP_LOG_DEBUG("Registered Xtables target %s\n",
			qs_p->qs_ctx[i].ipsec_target.name);
	}
	qs_p->idle_jiffies = jiffies;
	qs_p->qstate = DESC_Q_ACTIVE;
	SEP_LOG_DEBUG("IPsec queues activated.\n");
	return 0;

register_xt_target_err:
	for (i -= 1; i >= 0; i--) /* Clean allocated resources */
		xt_unregister_target(&qs_p->qs_ctx[i].ipsec_target);
	i = 2; /* For the following cleanup loop */
	xt_unregister_target(&ipsec_clean_mark_tg_reg);
register_clean_mark_err:
	xt_unregister_match(&ipsec_drop_mt_reg);
register_match_drop_err:
	xt_unregister_match(&ipsec_bypass_mt_reg);
#ifndef DEBUG_HOST_LOOPBACK
register_cc_interrupt_err:
	for (i -= 1; i >= 0; i--) /* Clean allocated resources */
		unregister_cc_interrupt_handler(qs_p->drvdata, q_cause_id[i]);
#endif
	return rc;
}

/**
 * ipsec_qs_deactivate() - Revert operation of ipsec_qs_activate()
 *	The queues _cannot_ be asleep when requesting this.
 *
 * @qs_p:	The IPsec queues context
 *
 * Returns 0 on success, -EINVAL for invalid current state
 */
static int ipsec_qs_deactivate(struct ipsec_qs *qs_p)
{
	int i;

	if (qs_p->qstate == DESC_Q_INACTIVE)
		return 0; /* Already inactive */
	if (qs_p->qstate != DESC_Q_ACTIVE) {
		SEP_LOG_ERR("Invalid state to request deactivation: %d\n",
			qs_p->qstate);
		return -EINVAL;
	}
	if (!ipsec_qs_are_idle((ipsec_qs_h)qs_p, NULL)) {
		SEP_LOG_ERR("Queues have pending requests\n");
		return -EBUSY;
	}
	for (i = 0; i < IPSEC_QS_NUM; i++)
		xt_unregister_target(&qs_p->qs_ctx[i].ipsec_target);
	xt_unregister_target(&ipsec_clean_mark_tg_reg);
	xt_unregister_match(&ipsec_drop_mt_reg);
	xt_unregister_match(&ipsec_bypass_mt_reg);
#ifndef DEBUG_HOST_LOOPBACK
	for (i = 0; i < IPSEC_QS_NUM; i++)
		unregister_cc_interrupt_handler(qs_p->drvdata, q_cause_id[i]);
#endif
	qs_p->qstate = DESC_Q_INACTIVE;
	SEP_LOG_DEBUG("IPsec queues deactivated.\n");
	return 0;
}

/**
 * ipsec_qs_put_asleep() - Move queue to ASLEEP state
 *
 * @qs_p:
 */
static int ipsec_qs_put_asleep(struct ipsec_qs *qs_p)
{
	int rc = 0;

	if (qs_p->qstate == DESC_Q_ACTIVE) {
		if (ipsec_qs_are_idle((ipsec_qs_h)qs_p, NULL))
			qs_p->qstate = DESC_Q_ASLEEP;
		else
			rc = -EBUSY;
	} else if (qs_p->qstate == DESC_Q_INACTIVE) {
		SEP_LOG_DEBUG("Sleep requested while inactive\n");
		qs_p->qstate = DESC_Q_INACTIVE_ASLEEP;
	} else {
		SEP_LOG_ERR("Invalid state (%d) to put queues asleep\n",
			qs_p->qstate);
		rc = -EINVAL;
	}
	if (likely(rc == 0))
		SEP_LOG_DEBUG("IPsec queues are asleep.\n");
	return rc;
}

/**
 * ipsec_qs_wakeup() - Move queues out of ASLEEP state to the relevant state
 *
 * @qs_p:
 */
static int ipsec_qs_wakeup(struct ipsec_qs *qs_p)
{
	if (qs_p->qstate == DESC_Q_ASLEEP) {
		qs_p->idle_jiffies = jiffies;
		qs_p->qstate = DESC_Q_ACTIVE;
	} else if (qs_p->qstate == DESC_Q_INACTIVE_ASLEEP) {
		qs_p->qstate = DESC_Q_INACTIVE;
	}
	return 0;
}

/**
 * ipsec_qs_set_state() - Set queue state (SLEEP or ACTIVE)
 * @q_h:	The queue object handle
 * @state:	The requested state
 */
int ipsec_qs_set_state(ipsec_qs_h q_h, enum desc_q_state state)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)q_h;
	int rc = 0;

#ifdef DEBUG
	if ((qs_p->qstate != DESC_Q_INACTIVE) &&
	    (qs_p->qstate != DESC_Q_ACTIVE) &&
	    (qs_p->qstate != DESC_Q_INACTIVE_ASLEEP) &&
	    (qs_p->qstate != DESC_Q_ASLEEP)) {
		SEP_LOG_ERR("Invalid state: %d\n", qs_p->qstate);
		return -EBUG;
	}
#endif
	SPIN_LOCK_QS(qs_p);
	switch (state) {
	case DESC_Q_ASLEEP:
		rc = ipsec_qs_put_asleep(qs_p);
		break;
	case DESC_Q_ACTIVE:
		if (qs_p->qstate == DESC_Q_ACTIVE)
			rc = 0; /* Already active */
		else if (qs_p->qstate == DESC_Q_INACTIVE)
			rc = ipsec_qs_activate((ipsec_qs_h)qs_p);
		else /* Wakeup from ASLEEP state */
			rc = ipsec_qs_wakeup(qs_p);
		break;
	case DESC_Q_INACTIVE:
		rc = ipsec_qs_deactivate(qs_p);
		break;
	default:
		SEP_LOG_ERR("Invalid requested state: %d\n", state);
		rc = -EINVAL;
	}
	SPIN_UNLOCK_QS(qs_p);
	return rc;
}

/**
 * ipsec_qs_get_state() - Get queue state
 * @q_h:	The queue object handle
 */
enum desc_q_state ipsec_qs_get_state(ipsec_qs_h q_h)
{
	struct ipsec_qs *qs_p = (struct ipsec_qs *)q_h;
	return qs_p->qstate;
}

/**
 * ipsec_q_enqueue() - Enqueue a packet descriptor in the ipsec-Q of SEP/PE
 *
 * @q_p:	Queue context
 * @skb:	The SKbuff associated with this packet (must be linear)
 * @packet_dma_addr:	DMA address of dispatched packet.
 * @packets_size:	Packet size in bytes
 * @spd_handle:		(optional) Hint from filter on associated SPD
 *
 * This function is used by the Xtables target to dispatch IP packets for
 * processing in PE. The queue must be activated before invoking this
 * function, but anyway, ipsec_qs_activate() takes care of registering
 * the client target after activating the queue (setting up its interrupt).
 * This function lock-bh since handlers in the IP stack might be in BH context.
 * (correct me if I am wrong...)
 */
static int ipsec_q_enqueue(struct ipsec_q *q_p, struct sk_buff *skb,
	uint8_t spd_handle)
{
	struct sep_ipsec_desc *desc_p;
	int cur_desc_idx;
	struct ipsec_qs *qs_p = q_p->parent;

	spin_lock_bh(&q_p->q_lock);
	if (ipsec_q_is_full(q_p) || (qs_p->qstate == DESC_Q_ASLEEP)) {
		SEP_LOG_ERR("ipsec_q(%s): is %s.\n",
			q_p->qid == IPSEC_Q_INBOUND ? "IN" : "OUT",
			qs_p->qstate == DESC_Q_ASLEEP ?
				"ASLEEP" : "FULL");
		spin_unlock_bh(&q_p->q_lock);
		sysfs_update_ipsec_drop_count(q_p->qid == IPSEC_Q_OUTBOUND,
			IPSEC_DROP_Q_FULL);
		return -ENOMEM;
	}
	q_p->dispatched_cntr++;
	cur_desc_idx = SEP_IPSEC_Q_DESC_INDEX(
		1 << q_p->parent->log2_pkts_per_q, q_p->dispatched_cntr);

	DUMP_PACKET(skb->data, skb->len);

	/* Map SK buffer for DMA as bi-directional (may be updated by PE ) */
	/* Map the whole SK buffer, since PE may expand the packet */
	q_p->q_pkts_dma[cur_desc_idx] =
		dma_map_single(q_p->parent->drvdata->dev, skb->head,
			skb_end_pointer(skb) - skb->head,
			DMA_BIDIRECTIONAL);
	if (dma_mapping_error(q_p->parent->drvdata->dev,
			q_p->q_pkts_dma[cur_desc_idx])) {
		SEP_LOG_ERR("Mapping IP packet DMA failed\n");
		sysfs_update_ipsec_drop_count(q_p->qid == IPSEC_Q_OUTBOUND,
			IPSEC_DROP_DMA_MAP);
		return -ENOMEM;
	}

	/* Build descriptor in SEP endianess (little) */
	desc_p = q_p->q_base_p + cur_desc_idx;
	desc_p->pkt_addr = cpu_to_le32(
		q_p->q_pkts_dma[cur_desc_idx] + skb_headroom(skb));
	desc_p->pkt_size = cpu_to_le16((uint16_t)skb->len);
	desc_p->spd_handle = spd_handle; /* Single byte - no endianess */
	q_p->q_skbs_p[cur_desc_idx] = skb; /* Save for completion */
	WRITE_REGISTER(q_p->gpr_to_sep, q_p->dispatched_cntr);
	spin_unlock_bh(&q_p->q_lock);
#ifdef DEBUG_HOST_LOOPBACK
	/* SeP does not respond so we immediately schedule virtual completion */
	desc_p->ipsec_verdict = SEP_IP_PKT_BYPASS;
	(void)schedule_work(&q_p->completion_worker);
#endif
	return 0;
}

/**
 * ipsec_q_interrupt_handler() - Interrupt handler registered for GPR updates
 * (both inbound+outbound queues)
 *
 * @drvdata:	SEP device context
 * @cause_id:	Interrupt cause ID
 * @priv_context:	ipsec_q context
 */
static void ipsec_q_interrupt_handler(struct sep_drvdata *drvdata,
	int cause_id, void *priv_context)
{
	struct ipsec_q *q_p = (struct ipsec_q *)priv_context;

	if (q_cause_id[q_p->qid] != cause_id) { /* Sanity check */
		SEP_LOG_ERR("Got cause_id=%d for qid=%d\n", cause_id, q_p->qid);
		return;
	}

	/* Dispatch completion work handler
	   (we cannot perform network stack processing in interrupt context) */
	(void)schedule_work(&q_p->completion_worker);
}

static bool ipsec_tunnel_reroute4(struct sk_buff *skb)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct net *net = dev_net(skb_dst(skb)->dev);
	struct rtable *rt;
	struct flowi4 fl4;

	memset(&fl4, 0, sizeof(fl4));
	fl4.daddr = iph->daddr;
	fl4.flowi4_tos = RT_TOS(iph->tos);
	fl4.flowi4_scope = RT_SCOPE_UNIVERSE;
	rt = ip_route_output_key(net, &fl4);
	if (IS_ERR(rt))
		return false;

	skb_dst_drop(skb);
	skb_dst_set(skb, &rt->dst);
	skb->dev      = rt->dst.dev;
	skb->protocol = htons(ETH_P_IP);
	return true;
}

static void ipsec_q_completion_handler(struct work_struct *work)
{
	struct ipsec_q *q_p =
		container_of(work, struct ipsec_q, completion_worker);
	struct ipsec_qs *qs_p = q_p->parent;
	/* Completed descriptors are reported by PE */
	uint32_t pe_completed = READ_REGISTER(q_p->gpr_from_sep);
	struct sep_ipsec_desc *cur_desc_p = NULL;
	int cur_desc_idx;
	uint16_t cur_pkt_size;
	struct sk_buff *cur_skb;
	int pkt_start_diff; /* Packet changed */
	struct iphdr *iph;
	unsigned int offset;
	uint32_t mtu;

	SEP_LOG_DEBUG("GPR#%d=%u\n",
		q_p->qid == IPSEC_Q_INBOUND ?
		SEP_IPSEC_INBOUND_GPR_IDX : SEP_IPSEC_OUTBOUND_GPR_IDX,
		pe_completed);
	/* Sanity check. This check support wrap of counters over their 32b */
	if ((q_p->dispatched_cntr - pe_completed) >
	    (q_p->dispatched_cntr - q_p->completed_cntr)) {
		SEP_LOG_ERR("GPR%d out of range = 0x%08X when "
			    "dispatched=0x%08X and completed=0x%08X\n",
			q_p->qid == IPSEC_Q_INBOUND ?
			SEP_IPSEC_INBOUND_GPR_IDX : SEP_IPSEC_OUTBOUND_GPR_IDX,
			pe_completed, q_p->dispatched_cntr,
			q_p->completed_cntr);
		return;
	}

	/* Process completed. Cyclic counter condition. */
	while (q_p->completed_cntr != pe_completed) {
		q_p->completed_cntr++;
		cur_desc_idx = SEP_IPSEC_Q_DESC_INDEX(
			1 << q_p->parent->log2_pkts_per_q, q_p->completed_cntr);
		cur_skb = q_p->q_skbs_p[cur_desc_idx];
		cur_desc_p = q_p->q_base_p + cur_desc_idx;
		/* Unmap as DMA buffer (sync to host) */
		dma_unmap_single(q_p->parent->drvdata->dev,
			q_p->q_pkts_dma[cur_desc_idx],
			skb_end_pointer(cur_skb) - cur_skb->head,
			DMA_BIDIRECTIONAL);

		/* Handle verdict */
		switch (cur_desc_p->ipsec_verdict) {
		case SEP_IP_PKT_IPSEC:
			/* Get packet location in host endianess */
			pkt_start_diff = le32_to_cpu(cur_desc_p->pkt_offset);
			cur_pkt_size = le16_to_cpu(cur_desc_p->pkt_size);
			SEP_LOG_DEBUG("pkt_start_diff=%d cur_pkt_size=%u\n",
				pkt_start_diff, cur_pkt_size);
			/* Adjust SKB to match changes in the packet */
			/* Calculate changes (reminder: SKB is linearized) */
			/* Adjust skb to match new data/size */
			if (pkt_start_diff < 0) {
				/* Packet was encapsulated */
				/* Sanity check for overflow */
				/* This is not supposed to happen if we
				   prepared the SKB properly before dispatching
				   it to PE */
				if ((-pkt_start_diff) > skb_headroom(cur_skb)) {
					SEP_LOG_ERR("IP packet was extended "
						    "beyond SKB headroom!\n");
					BUG();/* Avoid further mem. corruption*/
				}

#if defined(DEBUG) && defined(CONFIG_NOT_COHERENT_CACHE)
				if ((cur_skb->data + pkt_start_diff +
				     cur_pkt_size + L1_CACHE_BYTES) >=
					cur_skb->end) {
					SEP_LOG_ERR("Encap. pkt reached offset "
						    "%u-%u for skbuf of %d "
						    "(pkt_start_diff=%d "
						    "cur_pkt_size=%u)\n",
						skb_headroom(cur_skb) +
						pkt_start_diff,
						skb_headroom(cur_skb) +
						pkt_start_diff + cur_pkt_size,
						skb_end_pointer(cur_skb) -
						cur_skb->head, pkt_start_diff,
						cur_pkt_size);
				}
#endif
				/* Expand packet boundaries: */
				skb_push(cur_skb, -pkt_start_diff);
				skb_put(cur_skb, cur_pkt_size - cur_skb->len);
			} else if (pkt_start_diff > 0) {
				/* Packet was decapsulated. Shrink: */
				skb_pull(cur_skb, pkt_start_diff);
				skb_trim(cur_skb, cur_pkt_size);
			}
			skb_reset_network_header(cur_skb);
			skb_reset_transport_header(cur_skb);
			/* Should we reset other things in the SKB? */
			/* For inbound packets checksum should not be checked
			   because the encapsulating packet was already
			   validated, and the new packet was extracted out of
			   its payload.
			   For outbound packets it should re-calculated */
			cur_skb->ip_summed = CHECKSUM_NONE;
			if (q_p->qid == IPSEC_Q_OUTBOUND) {
				/* Force local fragmenttion before sending */
				iph = ip_hdr(cur_skb);
				mtu = cur_skb->dev ? cur_skb->dev->mtu : 0;
				if ((iph->frag_off & htons(IP_DF)) &&
				    (mtu < cur_pkt_size)) {
					cur_skb->local_df = 1;
					/* We also wish to disable segmentation
					offload since the TCP header is gone.
					This would force ip_fragment. */
					skb_shinfo(cur_skb)->gso_size = 0;
				}
			}

			cur_skb->mark = IPSEC_Q_VERDICT_IPSEC;
			break;
		case SEP_IP_PKT_BYPASS:
			SEP_LOG_INFO("Bypass verdict\n");
			cur_skb->mark = IPSEC_Q_VERDICT_BYPASS;
			break; /* No change in the packet - just pass */
		case SEP_IP_PKT_DROP:
			SEP_LOG_INFO("Dropping rejected packet!\n");
			cur_skb->mark = IPSEC_Q_VERDICT_DROP;
			break;
		case SEP_IP_PKT_ERROR:
		default:
			SEP_LOG_ERR("Error (%u) when processing packet!\n",
				cur_desc_p->ipsec_verdict);
			/* Error is handled as DROPed */
			cur_skb->mark = IPSEC_Q_VERDICT_DROP;
		}

		if (!(cur_skb->dev->features & NETIF_F_IP_CSUM)) {
			SEP_LOG_INFO("Calc. UDP-ENC checksum\n");
			/* Checksum offload for encapsulated UDP packets only */
			iph = ip_hdr(cur_skb);
			offset = iph->ihl * sizeof(uint32_t);
			skb_set_transport_header(cur_skb, offset);
			if (iph->protocol == IPPROTO_UDP) {
				cur_skb->csum = 0;
				udp_hdr(cur_skb)->check = 0;
				cur_skb->csum = skb_checksum(cur_skb, offset,
						cur_skb->len - offset, 0);

				udp_hdr(cur_skb)->check =
						csum_tcpudp_magic(iph->saddr,
							  iph->daddr,
							  cur_skb->len - offset,
							  IPPROTO_UDP,
							  cur_skb->csum);
			}
		}

		DUMP_PACKET(cur_skb->data, cur_skb->len);

		/* Re-inject packets for sync. handling of verdict */
		if (q_p->qid == IPSEC_Q_INBOUND) {
			netif_rx(cur_skb); /* Re-inject */
		} else {/* outbound */
			if (!ipsec_tunnel_reroute4(cur_skb))
				SEP_LOG_ERR("ipsec_tunnel_reroute4 error\n");
			dst_output(cur_skb);
		}
	}

	/* Set Qs timestamp in-case are idle */
	if (ipsec_qs_are_idle(qs_p, NULL))
		qs_p->idle_jiffies = jiffies;
}

/*** Xtables target functions ***/

static unsigned int ipsec_q_tg(
	struct sk_buff *skb, const struct xt_action_param *par)
{
	struct ipsec_q *q_p =
		container_of(par->target, struct ipsec_q, ipsec_target);
	struct ipsec_info *info = (struct ipsec_info *)par->targinfo;
	struct iphdr *iph;
	struct sk_buff *expanded_skb;
	int rc;


	iph = ip_hdr(skb);
	SEP_LOG_DEBUG("DX_IPSEC_%s(%s): pkt@0x%p (%u B) src=%d.%d.%d.%d "
		      "dst=%d.%d.%d.%d spd=%u mark=0x%08X\n",
		q_p->qid == IPSEC_Q_OUTBOUND ? "OUT" : "IN", par->target->name,
		skb->data, skb->len,
		GET_IP_ADDR_BYTES(iph->saddr),
		GET_IP_ADDR_BYTES(iph->daddr),
		info->spd_handle, skb->mark);
	if (skb->mark != IPSEC_Q_VERDICT_NOP)
		SEP_LOG_WARN("Got a packet with mark != 0\n");

	if (q_p->qid == IPSEC_Q_INBOUND) {
		/* Incoming packets must be defragmented first */
		/* ??? Isn't this already done by lower layers ??? */
		if (iph->frag_off & htons(IP_MF|IP_OFFSET)) {
			SEP_LOG_DEBUG("Got a fragment.\n");
			rc = ip_defrag(skb, IP_DEFRAG_LOCAL_DELIVER);
			if (rc != 0) {
				/* -EINPROGRESS implies that the fragement was
				   queued */
				if (rc != -EINPROGRESS) {
					SEP_LOG_ERR("Failed defragmentation "
						    "(%d)\n", rc);
					sysfs_update_ipsec_drop_count(0,
						IPSEC_DROP_DEFRAG);
				}
				return NF_STOLEN; /*Already freed by ip_defrag*/
			}
		}

		/* skb must be linearized and writable for SEP access to
		   packet buffer */
		rc = skb_linearize_cow(skb);
		if (rc != 0) {
			SEP_LOG_ERR("Failed linearizing SKB (rc=%d)\n", rc);
			sysfs_update_ipsec_drop_count(0, IPSEC_DROP_SKB_MEM);
			return NF_DROP;
		}
	} else { /* Outbound */
		if ((skb_headroom(skb) < PKT_HEADROOM_MIN) ||
		    (skb_tailroom(skb) < PKT_TAILROOM_MIN) ||
		    skb_is_nonlinear(skb)) {
			SEP_LOG_DEBUG("Not enough head(%u)/tail(%u) room for "
				    "outbound packet. Expanding skb.\n",
				skb_headroom(skb), skb_tailroom(skb));
			expanded_skb = skb_copy_expand(skb,
				PKT_HEADROOM_MIN, PKT_TAILROOM_MIN, GFP_ATOMIC);
			if (expanded_skb == NULL) {
				SEP_LOG_ERR("Failed expanding skb\n");
				sysfs_update_ipsec_drop_count(1,
					IPSEC_DROP_SKB_MEM);
				return NF_DROP;
			}

			kfree_skb(skb); /* not needed anymore */

			/* The new SKB must be linear buffer */
			SKB_LINEAR_ASSERT(expanded_skb);

			skb = expanded_skb;
		}
		/* We must calculate TCP/UDP checksum before it is being
		   encapsulated in the ESP protocol header
		   (The NIC would not identify it as TCP/UDP and would
		   not calculate the TCP/UDP checksum) */
		/* Checksum code derived from
		   drivers/net/ethernet/oki-semi/pch-gbe/pch_gbe_main.c */
		if (skb->ip_summed != CHECKSUM_UNNECESSARY) {
			unsigned int offset = iph->ihl * sizeof(uint32_t);
			skb_set_transport_header(skb, offset);
			if (iph->protocol == IPPROTO_TCP) {
				skb->csum = 0;
				tcp_hdr(skb)->check = 0;
				skb->csum = skb_checksum(skb, offset,
							 skb->len - offset, 0);
				tcp_hdr(skb)->check =
					csum_tcpudp_magic(iph->saddr,
							  iph->daddr,
							  skb->len - offset,
							  IPPROTO_TCP,
							  skb->csum);
			} else if (iph->protocol == IPPROTO_UDP) {
				skb->csum = 0;
				udp_hdr(skb)->check = 0;
				skb->csum =
					skb_checksum(skb, offset,
						     skb->len - offset, 0);
				udp_hdr(skb)->check =
					csum_tcpudp_magic(iph->saddr,
							  iph->daddr,
							  skb->len - offset,
							  IPPROTO_UDP,
							  skb->csum);
			}
		} /* if Checksum necessary */
		/* End of derived code */
	}

	return (ipsec_q_enqueue(q_p, skb, info->spd_handle) != 0) ?
		NF_DROP : NF_STOLEN; /* DROP if failed to enqueue */
}

static unsigned int ipsec_clean_mark_tg(
	struct sk_buff *skb, const struct xt_action_param *par)
{
	/* Clean any leftovers of IPsec marks before any further processing */
	skb->mark = 0;
	return XT_CONTINUE;
}

static int ipsec_q_tg_check(const struct xt_tgchk_param *par)
{
#ifdef DEBUG
	struct ipsec_q *q_p =
		container_of(par->target, struct ipsec_q, ipsec_target);
	struct ipsec_info *info = (struct ipsec_info *)par->targinfo;
	struct ipt_entry *ipte = (struct ipt_entry *)par->entryinfo;

	if (par->family != NFPROTO_IPV4) {
		SEP_LOG_ERR("Invalid family (%d) for this target!\n",
			par->family);
		return -EINVAL;
	}
	SEP_LOG_DEBUG("DX_IPSEC_%s: table=%s src=%d.%d.%d.%d/%d.%d.%d.%d "
		      "dst=%d.%d.%d.%d/%d.%d.%d.%d spd=%u\n",
		q_p->qid == IPSEC_Q_OUTBOUND ? "OUT" : "IN", par->table,
		GET_IP_ADDR_BYTES(ipte->ip.src),
		GET_IP_ADDR_BYTES(ipte->ip.smsk),
		GET_IP_ADDR_BYTES(ipte->ip.dst),
		GET_IP_ADDR_BYTES(ipte->ip.dmsk),
		info->spd_handle);
#endif
	return 0;
}

/**
 * ipsec_bypass_mt() - Match IPsec verdict recorded in "skb->mark" field.
 *	Return 'true' on IPSEC_Q_VERDICT_BYPASS or IPSEC_Q_VERDICT_IPSEC
 *
 * @skb:
 * @par:
 */
static bool ipsec_bypass_mt(
	const struct sk_buff *skb, struct xt_action_param *par)
{
	enum ipsec_q_verdict_mark the_verdict = skb->mark;
	bool verdict_match;
	struct iphdr *iphdr;
	struct udphdr *udph;
#ifdef DEBUG
	struct tcphdr *tcph;

	if (the_verdict != 0) {/* Dump only processed packets */
		iphdr = ip_hdr(skb);
		/* Get transport headers */
		if (iphdr->protocol == IPPROTO_UDP) {
			udph = (struct udphdr *)
				(((uint32_t *)iphdr) + iphdr->ihl);
			tcph = NULL;
		} else if (iphdr->protocol == IPPROTO_TCP) {
			tcph = (struct tcphdr *)
				((uint32_t *)iphdr) + iphdr->ihl;
			udph = NULL;
		} else {
			udph = NULL;
			tcph = NULL;
		}
		SEP_LOG_DEBUG("dx_ipsec_verdict: pkt@0x%p (%u B) "
			      "src=%d.%d.%d.%d:%d dst=%d.%d.%d.%d:%d "
			      "proto=%d verdict=%d\n",
			skb->data, skb->len,
			GET_IP_ADDR_BYTES(iphdr->saddr),
			udph ? udph->source : tcph ? tcph->source : 0,
			GET_IP_ADDR_BYTES(iphdr->daddr),
			udph ? udph->dest : tcph ? tcph->dest : 0,
			iphdr->protocol,
			the_verdict);
	}
#endif

	verdict_match = ((the_verdict == IPSEC_Q_VERDICT_BYPASS) ||
			 (the_verdict == IPSEC_Q_VERDICT_IPSEC));
	if (!verdict_match) {
		iphdr = ip_hdr(skb);
		/* Bypass UDP/500 (ISAKMP) traffic - Reach IKE server process */
		if (iphdr->protocol == IPPROTO_UDP) {
			udph = (struct udphdr *)
				(((uint32_t *)iphdr) + iphdr->ihl);
			verdict_match = ((udph->dest == ISAKMP_UDP_PORT) ||
					 (udph->source == ISAKMP_UDP_PORT));
#ifdef DEBUG
			if (verdict_match)
				SEP_LOG_DEBUG("Bypass ISAKMP packet\n");
#endif
		}
	}
	return verdict_match;
}

/**
 * ipsec_drop_mt() - Match IPsec verdict recorded in "skb->mark" field.
 *	Return 'true' on IPSEC_Q_VERDICT_DROP or any invalid verdict code
 *
 * @skb:
 * @par:
 */
static bool ipsec_drop_mt(
	const struct sk_buff *skb, struct xt_action_param *par)
{
	enum ipsec_q_verdict_mark the_verdict = skb->mark;
	bool verdict_match;
#ifdef DEBUG
	struct iphdr *iphdr;

	iphdr = ip_hdr(skb);
	if (the_verdict != 0) /* Dump only processed packets */
		SEP_LOG_DEBUG("dx_ipsec_verdict: pkt@0x%p (%u B) "
			      "src=%d.%d.%d.%d dst=%d.%d.%d.%d "
			      "verdict=%d\n",
			skb->data, skb->len,
			GET_IP_ADDR_BYTES(iphdr->saddr),
			GET_IP_ADDR_BYTES(iphdr->daddr),
			the_verdict);
#endif

	switch (the_verdict) {
	case IPSEC_Q_VERDICT_NOP:
	case IPSEC_Q_VERDICT_BYPASS:
	case IPSEC_Q_VERDICT_IPSEC:
		verdict_match = false;
		break;
	case IPSEC_Q_VERDICT_DROP:
		verdict_match = true;
		break;
	default:
		verdict_match = true;
		par->hotdrop = 1; /* hot drop on error */
		SEP_LOG_ERR("Invalid verdict = %d\n", the_verdict);
	}
	return verdict_match;
}

static void ipsec_q_tg_destroy(const struct xt_tgdtor_param *par)
{
#ifdef DEBUG
	struct ipsec_info *ipsec_info = (struct ipsec_info *)par->targinfo;

	SEP_LOG_DEBUG("Destroying target for spd=%u\n", ipsec_info->spd_handle);
#endif
}


