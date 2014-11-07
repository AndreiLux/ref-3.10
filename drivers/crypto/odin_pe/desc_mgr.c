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

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_DESC_MGR

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include "dx_driver.h"
#include "dx_bitops.h"
#include "sep_log.h"
#include "sep_sw_desc.h"
#include "crypto_ctx_mgr.h"
#include "sep_sysfs.h"
/* Registers definitions from shared/hw/include */
#include "dx_reg_base_host.h"
#include "dx_host.h"
#define DX_CC_HOST_VIRT /* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "sep_power.h"
#include "desc_mgr.h"

/* Queue buffer log(size in bytes) */
#define SEP_SW_DESC_Q_MEM_SIZE_LOG 12 /*4KB*/
#define SEP_SW_DESC_Q_MEM_SIZE (1 << SEP_SW_DESC_Q_MEM_SIZE_LOG)
#define WORD_SIZE_LOG 2 /*32b=4B=2^2*/
/* Number of entries (descriptors in a queue) */
#define SEP_DESC_Q_ENTRIES_NUM_LOG \
	(SEP_SW_DESC_Q_MEM_SIZE_LOG - WORD_SIZE_LOG - SEP_SW_DESC_WORD_SIZE_LOG)
#define SEP_DESC_Q_ENTRIES_NUM (1 << SEP_DESC_Q_ENTRIES_NUM_LOG)
#define SEP_DESC_Q_ENTRIES_MASK BITMASK(SEP_DESC_Q_ENTRIES_NUM_LOG)

/* This watermark is used to initiate dispatching after the queue entered
   the FULL state in order to avoid interrupts flooding at SeP */
#define SEP_DESC_Q_WATERMARK_MARGIN ((SEP_DESC_Q_ENTRIES_NUM)/4)

/* convert from descriptor counters index in descriptors array */
#define GET_DESC_IDX(cntr) ((cntr) & SEP_DESC_Q_ENTRIES_MASK)
#define GET_DESC_PTR(q_p, idx)    \
	((struct sep_sw_desc *)   \
	 ((q_p)->q_base_p + (idx << SEP_SW_DESC_WORD_SIZE_LOG)))
#define GET_Q_PENDING_DESCS(q_p) ((q_p)->sent_cntr - (q_p)->completed_cntr)
#define GET_Q_FREE_DESCS(q_p) \
	 (SEP_DESC_Q_ENTRIES_NUM - GET_Q_PENDING_DESCS(q_p))
#define IS_Q_FULL(q_p) (GET_Q_FREE_DESCS(q_p) == 0)


/* LUT for GPRs registers offsets (to be added to cc_regs_base) */
static const unsigned long host_to_sep_gpr_offset[] = {
	DX_CC_REG_OFFSET(HOST, HOST_SEP_GPR0),
	DX_CC_REG_OFFSET(HOST, HOST_SEP_GPR1),
	DX_CC_REG_OFFSET(HOST, HOST_SEP_GPR2),
	DX_CC_REG_OFFSET(HOST, HOST_SEP_GPR3),
};
static const unsigned long sep_to_host_gpr_offset[] = {
	DX_CC_REG_OFFSET(HOST, SEP_HOST_GPR0),
	DX_CC_REG_OFFSET(HOST, SEP_HOST_GPR1),
	DX_CC_REG_OFFSET(HOST, SEP_HOST_GPR2),
	DX_CC_REG_OFFSET(HOST, SEP_HOST_GPR3),
};

/* Interrupt mask assigned to GPRs */
/* Used for run time lookup, where SEP_HOST_GPR_IRQ_MASK() cannot be used */
static const uint32_t gpr_cause_id[] = {
	SEP_HOST_GPR_IRQ_SHIFT(0),
	SEP_HOST_GPR_IRQ_SHIFT(1),
	SEP_HOST_GPR_IRQ_SHIFT(2),
	SEP_HOST_GPR_IRQ_SHIFT(3),
};


/**
 * struct descs_backlog_item - Item of the queue descs_backlog_queue
 */
struct descs_backlog_item {
	struct list_head list;
	struct sep_sw_desc desc;
};

/**
 * struct descs_backlog_queue - Queue of backlog descriptors
 * @list:		List head item
 * @cur_q_len:		Current number of entries in the backlog_q
 * @backlog_items_pool:	Memory pool for allocating elements of this queue
 * @backlog_items_pool_name:	Pool name string for kmem_cache object
 */
struct descs_backlog_queue {
	struct list_head list;
	unsigned int cur_q_len;
	struct kmem_cache *backlog_items_pool;
	char backlog_items_pool_name[24];
};


/**
 * struct desc_q - Descriptor queue object
 * @qid:		The associated software queue ID
 * @qstate:		Operational state of the queue
 * @gpr_to_sep:		Pointer to host-to-sep GPR for this queue (requests)
 * @gpr_from_sep:	Pointer to sep-to-host GPR for this queue (completion)
 * @sep_q_dev:		SeP queue device context
 * @qlock:		Protect data structure in non-interrupt context
 * @q_base_p:		The base address of the descriptors cyclic queue buffer
 * @q_base_dma:		The DMA address for q_base_p
 * @sent_cntr:		Sent descriptors counter
 * @completed_cntr:	Completed descriptors counter as reported by SeP
 * @idle_jiffies:	jiffies value when the queue became idle (empty)
 * @backlog_q:		Queue of backlog descriptors - pending to be dispatched
 *			into the descriptors queue (were not dispatched because
 *			it was full or in "sleep" state)
 * @backlog_work:	Work task for handling/equeuing backlog descriptors
 * @enqueue_time:	Array to save descriptor start [ns] per descriptor
 */
struct desc_q {
	int qid;
	enum desc_q_state qstate;
	void __iomem *gpr_to_sep;
	void __iomem *gpr_from_sep;
	struct queue_drvdata *sep_q_dev;
	struct mutex qlock;
	uint32_t *q_base_p;
	dma_addr_t q_base_dma;
	uint32_t sent_cntr;
	uint32_t completed_cntr;
	unsigned long idle_jiffies;
	struct descs_backlog_queue backlog_q;
	struct work_struct backlog_work;
	unsigned long long *enqueue_time;
};

#ifdef DEBUG
static void dump_desc(const struct sep_sw_desc *desc_p);
#else
#define dump_desc(desc_p) do {} while (0)
#endif /*DEBUG*/

static int backlog_q_init(struct desc_q *q_p);
static void backlog_q_cleanup(struct desc_q *q_p);
static void backlog_q_process(struct work_struct *work);
static void desc_q_process_completed(struct sep_drvdata *drvdata,
	int cause_id, desc_q_t q_h);

/**
 * desc_q_create() - Create descriptors queue object
 * @qid:	 The queue ID (index)
 * @sep_q_dev:	 The associated queue device data
 *
 * Returns Allocated queue object handle (DESC_Q_INVALID_HANDLE for failure)
 * The function desc_q_activate() must be invoked after this function before
 * the queue may be used.
 */
desc_q_t desc_q_create(int qid, struct queue_drvdata *sep_q_dev)
{
	struct sep_drvdata *drvdata = sep_q_dev->sep_data;
	struct device *dev = drvdata->dev;
	void __iomem *cc_regs_base = drvdata->cc_base;
	struct desc_q *new_q_p;

	new_q_p = kzalloc(sizeof(struct desc_q), GFP_KERNEL);
	if (unlikely(new_q_p == NULL)) {
		SEP_LOG_ERR("Q%d: Failed allocating %d B for new_q\n",
			    qid, sizeof(struct desc_q));
		goto desc_q_create_failed;
	}

	/* Initialize fields */
	mutex_init(&new_q_p->qlock);
	new_q_p->sep_q_dev = sep_q_dev;
	new_q_p->qid = qid;
	new_q_p->gpr_to_sep = cc_regs_base + host_to_sep_gpr_offset[qid];
	new_q_p->gpr_from_sep = cc_regs_base + sep_to_host_gpr_offset[qid];
	new_q_p->sent_cntr = new_q_p->completed_cntr = 0;
	new_q_p->idle_jiffies = jiffies;

	new_q_p->q_base_p = dma_alloc_coherent(dev, SEP_SW_DESC_Q_MEM_SIZE,
		&new_q_p->q_base_dma, GFP_KERNEL);
	if (unlikely(new_q_p->q_base_p == NULL)) {
		SEP_LOG_ERR("Q%d: Failed allocating %d B for desc buffer\n",
			    qid, SEP_SW_DESC_Q_MEM_SIZE);
		goto desc_q_create_failed;
	}

	new_q_p->enqueue_time = kmalloc(SEP_DESC_Q_ENTRIES_NUM *
		sizeof(unsigned long long), GFP_KERNEL);
	if (new_q_p->enqueue_time == NULL) {
		SEP_LOG_ERR("Q%d: Failed allocating time stats array\n", qid);
		goto desc_q_create_failed;
	}

	if (backlog_q_init(new_q_p) != 0) {
		SEP_LOG_ERR("Q%d: Failed creating backlog queue\n", qid);
		goto desc_q_create_failed;
	}
	INIT_WORK(&new_q_p->backlog_work, backlog_q_process);

	/* Initialize respective GPR before SeP would be initialized.
	   Required because the GPR may be non-zero as a result of CC-init
	   sequence leftovers */
	WRITE_REGISTER(new_q_p->gpr_to_sep, new_q_p->sent_cntr);

	new_q_p->qstate = DESC_Q_INACTIVE;
	return (desc_q_t)new_q_p;

	/* Error cases cleanup */
desc_q_create_failed:
	if (new_q_p != NULL) {
		if (new_q_p->enqueue_time != NULL)
			kfree(new_q_p->enqueue_time);
		if (new_q_p->q_base_p != NULL) {
			dma_free_coherent(dev, SEP_SW_DESC_Q_MEM_SIZE,
				new_q_p->q_base_p, new_q_p->q_base_dma);
		}
		mutex_destroy(&new_q_p->qlock);
		kfree(new_q_p);
	}
	return DESC_Q_INVALID_HANDLE;
}

/**
 * desc_q_destroy() - Destroy descriptors queue object (free resources)
 * @q_h:	 The queue object handle
 *
 */
void desc_q_destroy(desc_q_t q_h)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	struct sep_drvdata *drvdata = q_p->sep_q_dev->sep_data;


	if (q_p->sent_cntr != q_p->completed_cntr) {
		SEP_LOG_ERR("Q%d: destroyed while there are outstanding "
			    "descriptors\n", q_p->qid);
	}
	unregister_cc_interrupt_handler(drvdata, gpr_cause_id[q_p->qid]);
	backlog_q_cleanup(q_p);
	kfree(q_p->enqueue_time);
	dma_free_coherent(drvdata->dev, SEP_SW_DESC_Q_MEM_SIZE,
			  q_p->q_base_p, q_p->q_base_dma);
	mutex_destroy(&q_p->qlock);
	kfree(q_p);
}

/**
 * desc_q_activate() - Activate queue by enabling interrupts
 *
 * @q_h:	The queue object handle
 *
 * Invoke this function one time after desc_q_create()
 */
void desc_q_activate(desc_q_t q_h)
{
	struct desc_q *q_p = (struct desc_q *)q_h;

	/* Attach interrupt handler to GPR associated with this queue. */
	/* qid is also the index of the GPR used. */
	(void)register_cc_interrupt_handler(q_p->sep_q_dev->sep_data,
		gpr_cause_id[q_p->qid], desc_q_process_completed, q_p);

	q_p->qstate = DESC_Q_ACTIVE;
}

/**
 * desc_q_set_state() - Set queue state (SLEEP or ACTIVE)
 * @q_h:	The queue object handle
 * @state:	The requested state
 */
int desc_q_set_state(desc_q_t q_h, enum desc_q_state state)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	int rc = 0;

#ifdef DEBUG
	if ((q_p->qstate != DESC_Q_ACTIVE) && (q_p->qstate != DESC_Q_ASLEEP)) {
		SEP_LOG_ERR("Q%d is in invalid state: %d\n",
			q_p->qid, q_p->qstate);
		return -EBUG;
	}
#endif
	mutex_lock(&q_p->qlock);
	switch (state) {
	case DESC_Q_ASLEEP:
		if (q_p->qstate != DESC_Q_ASLEEP) {
			/* If not already in this state */
			if (desc_q_is_idle(q_h, NULL))
				q_p->qstate = DESC_Q_ASLEEP;
			else
				rc = -EBUSY;
		} /* else: already asleep */
		break;
	case DESC_Q_ACTIVE:
		if (q_p->qstate != DESC_Q_ACTIVE) {
			/* Initiate enqueue from backlog if any is pending */
			if (q_p->backlog_q.cur_q_len > 0)
				(void)schedule_work(&q_p->backlog_work);
			else /* Empty --> Back to idle state */
				q_p->idle_jiffies = jiffies;
			q_p->qstate = DESC_Q_ACTIVE;
		} /* else: already active */
		break;
	default:
		SEP_LOG_ERR("Invalid requested state: %d\n", state);
		rc = -EINVAL;
	}
	mutex_unlock(&q_p->qlock);
	return rc;
}

/**
 * desc_q_get_state() - Get queue state
 * @q_h:	The queue object handle
 */
enum desc_q_state desc_q_get_state(desc_q_t q_h)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	return q_p->qstate;
}

/**
 * desc_q_is_idle() - Report if given queue is active but empty/idle.
 * @q_h:		The queue object handle
 * @idle_jiffies_p:	Return jiffies at which the queue became idle
 */
bool desc_q_is_idle(desc_q_t q_h, unsigned long *idle_jiffies_p)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	if (idle_jiffies_p != NULL)
		*idle_jiffies_p = q_p->idle_jiffies;
	/* No need to lock the queue - returned information is "fluid" anyway */
	return ((q_p->qstate == DESC_Q_ACTIVE) &&
		(GET_Q_PENDING_DESCS(q_p) == 0) &&
		(q_p->backlog_q.cur_q_len == 0));
}


/**
 * desc_q_reset() - Reset sent/completed counters of queue
 * @q_h:	The queue object handle
 *
 * This function should be invoked only when the queue is in ASLEEP state
 * after the transition of SeP to sleep state completed.
 * Returns -EBUSY if the queue is not in the correct state for reset.
 */
int desc_q_reset(desc_q_t q_h)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	int rc = 0;

	mutex_lock(&q_p->qlock);
	if ((q_p->qstate == DESC_Q_ASLEEP) && (GET_Q_PENDING_DESCS(q_p) == 0)) {
		q_p->sent_cntr = 0;
		q_p->completed_cntr = 0;
	} else {
		SEP_LOG_ERR("Invoked when queue is not ASLEEP\n");
		rc = -EBUSY;
	}
	mutex_unlock(&q_p->qlock);
	return rc;
}

/**
 * dispatch_sw_desc() - Copy given descriptor into next free entry in the
 *			descriptors queue and signal SeP.
 *
 * @q_p:	Desc. queue context
 * @desc_p:	The descriptor to dispatch
 *
 * This function should be called with qlock locked (non-interrupt context)
 * and only if queue is not full (i.e., this function does not validate
 * queue utilization)
 */
static inline void dispatch_sw_desc(
	struct desc_q *q_p, struct sep_sw_desc *desc_p)
{
	const uint32_t desc_idx = GET_DESC_IDX(q_p->sent_cntr);

	dump_desc(desc_p);
	q_p->enqueue_time[desc_idx] = sched_clock(); /* Save start time */
	/* copy descriptor to free entry in queue */
	SEP_SW_DESC_COPY_TO_SEP(GET_DESC_PTR(q_p, desc_idx), desc_p);
	q_p->sent_cntr++;
}

/**
 * desc_q_enqueue_sleep_req() - Enqueue SLEEP_REQ descriptor
 * @q_h:	The queue object handle
 * @op_ctx:	The operation context for this descriptor
 * This function may be invoked only when the queue is in ASLEEP state
 * (assuming SeP is still active).
 * If the queue is not in ASLEEP state this function would return -EBUSY.
 */
int desc_q_enqueue_sleep_req(desc_q_t q_h, struct sep_op_ctx *op_ctx)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	struct sep_sw_desc desc;
	int rc = 0;

	SEP_SW_DESC_INIT(&desc);
	SEP_SW_DESC_SET(&desc, TYPE, SEP_SW_DESC_TYPE_SLEEP_REQ);
	SEP_SW_DESC_SET(&desc, COOKIE, (u32)op_ctx);

	mutex_lock(&q_p->qlock);
	if (q_p->qstate == DESC_Q_ASLEEP) {
		op_ctx->op_state = USER_OP_INPROC;
		/* In ASLEEP state the queue assumed to be empty... */
		dispatch_sw_desc(q_p, &desc);
		WRITE_REGISTER(q_p->gpr_to_sep, q_p->sent_cntr);
		SEP_LOG_DEBUG("Sent SLEEP_REQ\n");
	} else {
		rc = -EBUSY;
	}
	mutex_unlock(&q_p->qlock);
	return rc;
}

static int backlog_q_init(struct desc_q *q_p)
{
	struct descs_backlog_queue *backlog_q_p = &q_p->backlog_q;
	int rc = 0;

	snprintf(backlog_q_p->backlog_items_pool_name,
		sizeof(backlog_q_p->backlog_items_pool_name),
		"dx_sep_backlog%d", q_p->qid);
	backlog_q_p->backlog_items_pool = kmem_cache_create(
		backlog_q_p->backlog_items_pool_name,
		sizeof(struct descs_backlog_item), sizeof(uint32_t), 0, NULL);
	if (unlikely(backlog_q_p->backlog_items_pool == NULL)) {
		SEP_LOG_ERR("Q%d: Failed allocating backlog_items_pool\n",
			q_p->qid);
		rc = -ENOMEM;
	} else {
		INIT_LIST_HEAD(&backlog_q_p->list);
		backlog_q_p->cur_q_len = 0;
	}
	return rc;
}

static void backlog_q_cleanup(struct desc_q *q_p)
{
	struct descs_backlog_queue *backlog_q_p = &q_p->backlog_q;

	if (backlog_q_p->cur_q_len > 0) {
		SEP_LOG_ERR("Q%d: Cleanup while have %u pending items!",
			q_p->qid, backlog_q_p->cur_q_len);
		/* TODO: Handle freeing of pending items? */
	}
	kmem_cache_destroy(backlog_q_p->backlog_items_pool);
}

/**
 * backlog_q_enqueue() - Enqueue given descriptor for postponed processing
 *				(e.g., in case of full desc_q)
 *
 * @q_p:	Desc. queue object
 * @desc_p:	Descriptor to enqueue
 *
 * Caller must call this function with the qlock locked (non-interrupt context
 * only)
 */
static int backlog_q_enqueue(struct desc_q *q_p,
	struct sep_sw_desc *desc_p)
{
	struct descs_backlog_queue *backlog_q_p = &q_p->backlog_q;
	struct sep_op_ctx *op_ctx =
		(struct sep_op_ctx *)SEP_SW_DESC_GET(desc_p, COOKIE);
	struct descs_backlog_item *new_q_item;

	SEP_LOG_DEBUG("->backlog(op_ctx=%p):\n", op_ctx);
	dump_desc(desc_p);

	new_q_item =
		kmem_cache_alloc(backlog_q_p->backlog_items_pool, GFP_KERNEL);
	if (unlikely(new_q_item == NULL)) {
		SEP_LOG_ERR("Failed allocating descs_queue_item");
		op_ctx->op_state = USER_OP_NOP;
		return -ENOMEM;
	}
	op_ctx->op_state = USER_OP_PENDING;
	memcpy(&new_q_item->desc, desc_p, sizeof(struct sep_sw_desc));
	list_add_tail(&new_q_item->list, &backlog_q_p->list);
	op_ctx->backlog_descs_cntr++;
	backlog_q_p->cur_q_len++;
	return 0;
}

/**
 * backlog_q_dequeue() - Dequeue from pending descriptors queue and dispatch
 *			into the SW-q the first pending descriptor
 *
 * @q_p:	Desc. queue object
 *
 * This function must be called with qlock locked and only if there is free
 * space in the given descriptor queue.
 * It returns 0 on success and -ENOMEM if there is no pending request
 */
static int backlog_q_dequeue(struct desc_q *q_p)
{
	struct descs_backlog_queue *backlog_q_p = &q_p->backlog_q;
	struct descs_backlog_item *first_item;
	struct sep_sw_desc *desc_p;
	struct sep_op_ctx *op_ctx;

	if (list_empty(&backlog_q_p->list))
		return -ENOMEM;
	/* Remove from the first item from the list but keep the item */
	first_item = list_first_entry(&backlog_q_p->list,
		struct descs_backlog_item, list);
	list_del(&first_item->list);
	backlog_q_p->cur_q_len--;
	/* Process/dispatch the descriptor to the SW-q. */
	desc_p = &first_item->desc;
	dump_desc(desc_p);
	op_ctx = (struct sep_op_ctx *)SEP_SW_DESC_GET(desc_p, COOKIE);
	if (unlikely(op_ctx == NULL)) {
		SEP_LOG_ERR("Invalid desc - COOKIE is NULL\n");
		return -EBUG;
	}
	SEP_LOG_DEBUG("backlog(op_ctx=%p)->descQ:\n", op_ctx);
	dispatch_sw_desc(q_p, desc_p);
	op_ctx->backlog_descs_cntr--;
	/* Now we can free the list item */
	kmem_cache_free(backlog_q_p->backlog_items_pool, first_item);
	if (op_ctx->backlog_descs_cntr == 0) {
		/* All the operation descriptors reached the SW-q. */
		op_ctx->op_state = USER_OP_INPROC;
		if (op_ctx->comp_work != NULL) /* Async. (CryptoAPI) */
			/* Invoke completion callback directly because
			   we are already in work_queue context and we wish
			   to assure this state update (EINPROGRESS)
			   is delivered before the request is completed */
			op_ctx->comp_work->func(op_ctx->comp_work);
	}
	return 0;
}

/**
 * backlog_q_process() - Handler for dispatching backlog descriptors
 *			into the SW desc.Q when possible (dispatched from
 *			the completion interrupt handler)
 *
 * @work:	The work context
 */
static void backlog_q_process(struct work_struct *work)
{
	int descs_to_enqueue;
	struct desc_q *q_p = container_of(work, struct desc_q, backlog_work);

	mutex_lock(&q_p->qlock);
	if (q_p->qstate == DESC_Q_ACTIVE) { /* Avoid on ASLEEP state */
		descs_to_enqueue = GET_Q_FREE_DESCS(q_p);
		/* Not more than pending descriptors */
		if (descs_to_enqueue > q_p->backlog_q.cur_q_len)
			descs_to_enqueue = q_p->backlog_q.cur_q_len;
		SEP_LOG_DEBUG("Q%d: Dispatching %d descs. from pendQ\n",
			q_p->qid, descs_to_enqueue);
		while (descs_to_enqueue > 0) {
			/* From backlog queue to SW descriptors queue */
			backlog_q_dequeue(q_p);
			descs_to_enqueue--;
		}
		/* Signal SeP once of all new descriptors
		  (interrupt coalescing) */
		WRITE_REGISTER(q_p->gpr_to_sep, q_p->sent_cntr);
	}
	mutex_unlock(&q_p->qlock);
}

/**
 * desc_q_get_info4sep() - Get queue address and size to be used in FW init
 *				phase
 * @q_h:	 The queue object handle
 * @base_addr_p:	 Base address return parameter
 * @size_p:	 Queue size (in bytes) return parameter
 *
 */
void desc_q_get_info4sep(desc_q_t q_h,
			 dma_addr_t *base_addr_p, unsigned long *size_p)
{
	struct desc_q *q_p = (struct desc_q *)q_h;

	*base_addr_p = q_p->q_base_dma;
	*size_p = SEP_SW_DESC_Q_MEM_SIZE;
}

/**
 * desc_q_enqueue() - Enqueue given descriptor in given queue
 * @q_h:		The queue object handle
 * @desc_p:		Pointer to descriptor
 * @may_backlog:	When "true" and descQ is full or ASLEEP, may enqueue
 *			the given desc. in the backlog queue.
 *			When "false", any of the above cases would cause
 *			returning -ENOMEM.
 *
 * The function updates the op_ctx->op_state accoring to its results.
 * Returns -EINPROGRESS on success to dispatch into the SW desc. q.
 * Returns -EBUSY if may_backlog==true and the descriptor was enqueued in the
 * the backlog queue.
 * Returns -ENOMEM if queue is full and cannot enqueue in the backlog queue
 */
int desc_q_enqueue(desc_q_t q_h, struct sep_sw_desc *desc_p, bool may_backlog)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	struct sep_op_ctx *op_ctx =
		(struct sep_op_ctx *)SEP_SW_DESC_GET(desc_p, COOKIE);
	int rc;

	mutex_lock(&q_p->qlock);

	if (IS_Q_FULL(q_p) ||			/* Queue is full */
	    (q_p->backlog_q.cur_q_len > 0) ||	/* or already have pending d. */
	    (q_p->qstate == DESC_Q_ASLEEP))  {	/* or in sleep state */
		if (may_backlog) {
			SEP_LOG_DEBUG("Enqueuing desc. to queue@%s\n",
				q_p->qstate == DESC_Q_ASLEEP ?
					"ASLEEP" : "FULL");
			rc = backlog_q_enqueue(q_p, desc_p);
			if (unlikely(rc != 0)) {
				SEP_LOG_ERR("Failed enqueuing desc. to "
					    "queue@%s\n",
					q_p->qstate == DESC_Q_ASLEEP ?
					"ASLEEP" : "FULL");
			} else
				rc = -EBUSY;
		} else {
			SEP_LOG_DEBUG("Q%d: %s and may not backlog.\n",
				q_p->qid,
				q_p->qstate == DESC_Q_ASLEEP ?
					"ASLEEP" : "FULL");
			rc = -ENOMEM;
		}

	} else { /* Can dispatch to actual descriptors queue */
		op_ctx->op_state = USER_OP_INPROC;
		dispatch_sw_desc(q_p, desc_p);
		/* Signal SeP of new descriptors */
		WRITE_REGISTER(q_p->gpr_to_sep, q_p->sent_cntr);
		SEP_LOG_DEBUG("Q#%d: Sent SwDesc #%u (op_ctx=%p)\n",
			q_p->qid, q_p->sent_cntr, op_ctx);
		rc = -EINPROGRESS;
	}

	mutex_unlock(&q_p->qlock);

	return rc; /* Enqueued to desc. queue */
}

/**
 * desc_q_mark_invalid_cookie() - Mark given cookie as invalid in case marked as
 *					completed after a timeout
 * @q_h:	 Descriptor queue handle
 * @cookie:	 Invalidate descriptors with this cookie
 *
 * Mark given cookie as invalid in case marked as completed after a timeout
 * Invoke this before releasing the op_ctx object.
 * There is no race with the interrupt because the op_ctx (cookie) is still
 * valid when invoking this function.
 */
void desc_q_mark_invalid_cookie(desc_q_t q_h, uint32_t cookie)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	struct sep_sw_desc desc;
	uint32_t cur_desc_cntr, cur_desc_idx;
	unsigned int drop_cnt = 0;

	mutex_lock(&q_p->qlock);

	for (cur_desc_cntr = q_p->completed_cntr;
	      cur_desc_cntr < q_p->sent_cntr;
	      cur_desc_cntr++) {
		/* Mark all outstanding of given cookie as invalid with NULL */
		cur_desc_idx = GET_DESC_IDX(cur_desc_cntr);
		/* Copy descriptor to spad (endianess fix-up) */
		/* TODO: Optimize to avoid full copy back...  */
		/* (we only need the cookie) */
		SEP_SW_DESC_COPY_FROM_SEP(&desc,
			GET_DESC_PTR(q_p, cur_desc_idx));
		if (SEP_SW_DESC_GET(&desc, COOKIE) == cookie) {
			SEP_SW_DESC_SET(&desc, COOKIE, 0); /* Invalidate */
			SEP_SW_DESC_COPY_TO_SEP(GET_DESC_PTR(q_p, cur_desc_idx),
				&desc);
			SEP_LOG_DEBUG("Invalidated desc at desc_cnt=%u\n",
				cur_desc_idx);
			drop_cnt++;
		}
	}

	mutex_unlock(&q_p->qlock);

	if (drop_cnt > 0)
		SEP_LOG_WARN("Invalidated %u descriptors of cookie=0x%08X\n",
			drop_cnt, cookie);

}

/**
 * desc_q_process_completed() - Dequeue and process any completed descriptors in
 *				the queue (the queue/GPR interrupt handler)
 * @q_h:	 The queue object handle
 *
 * Dequeue and process any completed descriptors in the queue
 * (This function assumes non-reentrancy since it is invoked from
 *  either interrupt handler or in workqueue context)
 */
static void desc_q_process_completed(struct sep_drvdata *drvdata,
	int cause_id, desc_q_t q_h)
{
	struct desc_q *q_p = (struct desc_q *)q_h;
	struct sep_op_ctx *op_ctx;
	struct sep_sw_desc desc;
	enum sep_sw_desc_type desc_type;
	uint32_t cookie;
	uint32_t ret_code;
	uint32_t desc_idx;
	uint32_t new_completed_cntr;

	new_completed_cntr = READ_REGISTER(q_p->gpr_from_sep);
	/* Sanity check for read GPR value (must be between sent and completed).
	   This arithmetic is cyclic so should work even after the counter
	   wraps around. */
	if ((q_p->sent_cntr - new_completed_cntr) >
	    (q_p->sent_cntr - q_p->completed_cntr)) {
		/* More completions than outstanding descriptors ?! */
		SEP_LOG_ERR("sent_cntr=0x%08X completed_cntr=0x%08X "
			    "gpr=0x%08X\n",
			q_p->sent_cntr, q_p->completed_cntr,
			new_completed_cntr);
		return;/*SEP_DRIVER_BUG()*/
		/* This is a (sep) bug case that is not supposed to happen,
		   but we must verify this to avoid accessing stale descriptor
		   data (which may cause system memory corruption).
		   Returning to the caller may result in interrupt loss, but we
		   prefer losing a completion and blocking the caller forever
		   than invoking BUG() that would crash the whole system and may
		   even loose the error log message. This would give a chance
		   for a subsequent pending descriptor completion recover this
		   case or in the worst case let the system administrator
		   understand what is going on and let her perform a graceful
		   reboot. */
	}

	while (new_completed_cntr > q_p->completed_cntr) {

		desc_idx = GET_DESC_IDX(q_p->completed_cntr);
		/* Copy descriptor to spad (endianess fix-up) */
		/* TODO: Optimize to avoid full copy back...  */
		/* (we only need type the fields: type, retcode, cookie) */
		SEP_SW_DESC_COPY_FROM_SEP(&desc, GET_DESC_PTR(q_p, desc_idx));
		desc_type = SEP_SW_DESC_GET(&desc, TYPE);
		cookie = SEP_SW_DESC_GET(&desc, COOKIE);
		ret_code = SEP_SW_DESC_GET(&desc, RET_CODE);
		sysfs_update_sep_stats(q_p->qid, desc_type,
				 q_p->enqueue_time[desc_idx], sched_clock());
		q_p->completed_cntr++; /* prepare for next */
		SEP_LOG_DEBUG("type=%u retcode=0x%08X cookie=0x%08X",
			desc_type, ret_code, cookie);
		if (cookie == 0) {
			/* Probably late completion on invalidated cookie */
			SEP_LOG_ERR("Got completion with NULL cookie\n");
			continue;
		}
		op_ctx = (struct sep_op_ctx *)cookie;
		if (desc_type == SEP_SW_DESC_TYPE_APP_REQ) { /* Applet Req. */
			/* "internal error" flag is currently available only
			   in this descriptor type. */
			op_ctx->internal_error = SEP_SW_DESC_GET4TYPE(
				&desc, APP_REQ, INTERNAL_ERR);
			/* Get session ID for SESSION_OPEN case */
			op_ctx->session_ctx->sep_session_id =
				SEP_SW_DESC_GET4TYPE(&desc,
					APP_REQ, SESSION_ID);
		}
#ifdef DEBUG
		if (op_ctx->pending_descs_cntr > MAX_PENDING_DESCS)
			SEP_LOG_ERR("Invalid num of pending descs %d\n",
				op_ctx->pending_descs_cntr);
#endif
		/* pending descriptors counter (apply for transactions composed
		   of more than a single descriptor) */
		op_ctx->pending_descs_cntr--;
		/* Update associated operation context and notify it */
		op_ctx->error_info |= ret_code;
		if (op_ctx->pending_descs_cntr == 0) {
			op_ctx->op_state = USER_OP_COMPLETED;
			if (op_ctx->comp_work != NULL) /* Async. (CryptoAPI) */
				(void)schedule_work(op_ctx->comp_work);
			else /* Sync. (IOCTL or dx_sepapp_ API) */
				complete(&(op_ctx->ioctl_op_compl));
		}
	} /* while(new_completed_cntr) */

	/* Dispatch pending requests */
	/* if any pending descs. & utilization is below watermark & !ASLEEP */
	if ((q_p->backlog_q.cur_q_len > 0) &&
	    (GET_Q_FREE_DESCS(q_p) > SEP_DESC_Q_WATERMARK_MARGIN) &&
	    (q_p->qstate != DESC_Q_ASLEEP)) {
		(void)schedule_work(&q_p->backlog_work);
	} else if (desc_q_is_idle(q_h, NULL)) {
		q_p->idle_jiffies = jiffies;
	}

}

/**
 * desq_q_pack_debug_desc() - Create a debug descriptor in given buffer
 * @desc_p:	 The descriptor buffer
 * @op_ctx:	 The operation context
 *
 * TODO: Get additional debug descriptors (in addition to loopback)
 *
 */
void desq_q_pack_debug_desc(
	struct sep_sw_desc *desc_p, struct sep_op_ctx *op_ctx)
{
	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_DEBUG);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);
}


/**
 * desc_q_pack_crypto_op_desc() - Pack a CRYPTO_OP descriptor in given
 *				descriptor buffer
 * @desc_p:	 The descriptor buffer
 * @op_ctx:	 The operation context
 * @sep_ctx_load_req:	 Context load request flag
 * @sep_ctx_init_req:	 Context initialize request flag
 * @proc_mode:	 Descriptor processing mode
 *
 */
void desc_q_pack_crypto_op_desc(struct sep_sw_desc *desc_p,
			   struct sep_op_ctx *op_ctx,
			   int sep_ctx_load_req, int sep_ctx_init_req,
			   enum sep_proc_mode proc_mode)
{
	uint32_t xlli_addr;
	uint16_t xlli_size;
	uint16_t table_count;

	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_CRYPTO_OP);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);

	SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, FW_CACHE_IDX,
				ctxmgr_get_sep_cache_idx(&op_ctx->ctx_info));
	SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, L, sep_ctx_load_req);
	SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, HCB_ADDR,
			ctxmgr_get_sep_ctx_dma_addr(&op_ctx->ctx_info));
	SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, I, sep_ctx_init_req);
	SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, PROC_MODE, proc_mode);

	if (proc_mode != SEP_PROC_MODE_NOP) { /* no need for IFT/OFT in NOP*/
		/* IFT details */
		llimgr_get_mlli_desc_info(&op_ctx->ift,
			&xlli_addr, &xlli_size, &table_count);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_ADDR, xlli_addr);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_SIZE, xlli_size);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_NUM, table_count);

		/* OFT details */
		llimgr_get_mlli_desc_info(&op_ctx->oft,
			&xlli_addr, &xlli_size, &table_count);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_ADDR, xlli_addr);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_SIZE, xlli_size);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_NUM, table_count);
	}
}

/**
 * desc_q_pack_combined_op_desc() - Pack a COMBINED_OP descriptor in given
 *					descriptor buffer
 * @desc_p:	 The descriptor buffer
 * @op_ctx:	 The operation context
 * @sep_ctx_load_req:	 Context load request flag
 * @sep_ctx_init_req:	 Context initialize request flag
 * @proc_mode:	 Descriptor processing mode
 * @cfg_scheme:	 The SEP format configuration scheme claimed by the user
 *
 */
void desc_q_pack_combined_op_desc(struct sep_sw_desc *desc_p,
			struct sep_op_ctx *op_ctx,
			int sep_ctx_load_req, int sep_ctx_init_req,
			enum sep_proc_mode proc_mode,
			uint32_t cfg_scheme)
{
	uint32_t xlli_addr;
	uint16_t xlli_size;
	uint16_t table_count;

	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_COMBINED_OP);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);

	SEP_SW_DESC_SET4TYPE(desc_p, COMBINED_OP, L, sep_ctx_load_req);
	SEP_SW_DESC_SET4TYPE(desc_p, COMBINED_OP, CONFIG_SCHEME, cfg_scheme);
	SEP_SW_DESC_SET4TYPE(desc_p, COMBINED_OP, I, sep_ctx_init_req);
	SEP_SW_DESC_SET4TYPE(desc_p, COMBINED_OP, PROC_MODE, proc_mode);

	if (proc_mode != SEP_PROC_MODE_NOP) { /* no need for IFT/OFT in NOP*/
		/* IFT details */
		llimgr_get_mlli_desc_info(&op_ctx->ift,
			&xlli_addr, &xlli_size, &table_count);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_ADDR, xlli_addr);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_SIZE, xlli_size);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, IFT_NUM, table_count);

		/* OFT details */
		llimgr_get_mlli_desc_info(&op_ctx->oft,
			&xlli_addr, &xlli_size, &table_count);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_ADDR, xlli_addr);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_SIZE, xlli_size);
		SEP_SW_DESC_SET4TYPE(desc_p, CRYPTO_OP, OFT_NUM, table_count);
	}
}

/**
 * desc_q_pack_load_op_desc() - Pack a LOAD_OP descriptor in given descriptor
 *				buffer
 * @desc_p:	 The descriptor buffer
 * @op_ctx:	 The operation context
 * @sep_ctx_load_req:	 Context load request flag
 *
 */
void desc_q_pack_load_op_desc(struct sep_sw_desc *desc_p,
			struct sep_op_ctx *op_ctx,
			int *sep_ctx_load_req)
{
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	uint32_t *p = (uint32_t *)desc_p;
	int idx;

	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_LOAD_OP);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);

	for (idx = 0; idx < SEP_MAX_COMBINED_ENGINES; idx++) {
		BITFIELD_SET(p[SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_WORD_OFFSET],
			SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_OFFSET(idx),
			SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_SIZE,
			(ctx_info_p[idx].ctx_kptr == NULL) ? (-1) :
			ctxmgr_get_sep_cache_idx(&ctx_info_p[idx]));

		BITFIELD_SET(p[SEP_SW_DESC_LOAD_OP_HCB_ADDR_WORD_OFFSET(idx)],
			SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_OFFSET,
			SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_SIZE,
			(ctx_info_p[idx].ctx_kptr == NULL) ? 0 :
			(ctxmgr_get_sep_ctx_dma_addr(&ctx_info_p[idx]) >>
			SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_OFFSET));
			/* shiffting DMA address due to the "L" bit in the
			LS bit */

		BITFIELD_SET(p[SEP_SW_DESC_LOAD_OP_L_WORD_OFFSET(idx)],
			SEP_SW_DESC_LOAD_OP_L_BIT_OFFSET,
			SEP_SW_DESC_LOAD_OP_L_BIT_SIZE,
			sep_ctx_load_req[idx]);
	}
}

/**
 * desc_q_pack_rpc_desc() - Pack the RPC (message) descriptor type
 * @desc_p:	 The descriptor buffer
 * @op_ctx:	 The operation context
 * @agent_id:	 RPC agent (API) ID
 * @func_id:	 Function ID (index)
 * @rpc_msg_size:	 Size of RPC parameters message buffer
 * @rpc_msg_dma_addr:	 DMA address of RPC parameters message buffer
 *
 */
void desc_q_pack_rpc_desc(struct sep_sw_desc *desc_p,
	struct sep_op_ctx *op_ctx,
	seprpc_agent_id_t agent_id,
	seprpc_func_id_t func_id,
	unsigned long rpc_msg_size,
	dma_addr_t rpc_msg_dma_addr)
{
	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_RPC_MSG);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);

#ifdef DEBUG
	/* Verify that given agent_id is not too large for AGENT_ID field */
	if (agent_id >= (1 << SEP_SW_DESC_RPC_MSG_AGENT_ID_BIT_SIZE)) {
		SEP_LOG_ERR("Given agent_id=%d is too large for AGENT_ID "
			    "field. Value truncated!", agent_id);
	}
#endif
	SEP_SW_DESC_SET4TYPE(desc_p, RPC_MSG, AGENT_ID, agent_id);
	SEP_SW_DESC_SET4TYPE(desc_p, RPC_MSG, FUNC_ID, func_id);
	SEP_SW_DESC_SET4TYPE(desc_p, RPC_MSG, HMB_SIZE, rpc_msg_size);
	SEP_SW_DESC_SET4TYPE(desc_p, RPC_MSG, HMB_ADDR, rpc_msg_dma_addr);
}

/**
 * desc_q_pack_app_req_desc() - Pack the Applet Request descriptor
 * @desc_p:	The descriptor buffer
 * @op_ctx:	The operation context
 * @req_type:	The Applet request type
 * @session_id:	Session ID - Required only for SESSION_CLOSE and
 *		COMMAND_INVOKE requests
 * @inparams_addr:	DMA address of the "In Params." structure for the
 *			request.
 *
 */
void desc_q_pack_app_req_desc(struct sep_sw_desc *desc_p,
	struct sep_op_ctx *op_ctx,
	enum sepapp_req_type req_type,
	uint16_t session_id,
	dma_addr_t inparams_addr)
{
	SEP_SW_DESC_INIT(desc_p);
	SEP_SW_DESC_SET(desc_p, TYPE, SEP_SW_DESC_TYPE_APP_REQ);
	SEP_SW_DESC_SET(desc_p, COOKIE, (u32)op_ctx);

	SEP_SW_DESC_SET4TYPE(desc_p, APP_REQ, REQ_TYPE, req_type);
	SEP_SW_DESC_SET4TYPE(desc_p, APP_REQ, SESSION_ID, session_id);
	SEP_SW_DESC_SET4TYPE(desc_p, APP_REQ, IN_PARAMS_ADDR, inparams_addr);
}

/**
 * crypto_proc_mode_to_str() - Convert from crypto_proc_mode to string
 * @proc_mode:	 The proc_mode enumeration value
 *
 * Returns A string description of the processing mode (NULL if invalid mode)
 */
const char *crypto_proc_mode_to_str(enum sep_proc_mode proc_mode)
{
	switch (proc_mode) {
	case SEP_PROC_MODE_NOP: return "NOP";
	case SEP_PROC_MODE_PROC_T: return "PROC_T";
	case SEP_PROC_MODE_FIN: return "FIN";
	case SEP_PROC_MODE_PROC_A: return "PROC_A";
	default: return "?";
	}
}


#ifdef DEBUG
static void dump_crypto_op_desc(const struct sep_sw_desc *desc_p)
{
	SEP_LOG_DEBUG("CRYPTO_OP::%s (type=%lu,cookie=0x%08lX)\n",
		      crypto_proc_mode_to_str(
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, PROC_MODE)),
		      SEP_SW_DESC_GET(desc_p, TYPE),
		      SEP_SW_DESC_GET(desc_p, COOKIE));

	SEP_LOG_DEBUG("HCB=0x%08lX @ FwIdx=%lu %s%s\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, HCB_ADDR),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, FW_CACHE_IDX),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, L) ?
		      "(load)" : "",
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, I) ?
		      "(init)" : "");

	SEP_LOG_DEBUG("IFT: addr=0x%08lX , size=0x%08lX , tbl_num=%lu\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, IFT_ADDR),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, IFT_SIZE),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, IFT_NUM));

	SEP_LOG_DEBUG("OFT: addr=0x%08lX , size=0x%08lX , tbl_num=%lu\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, OFT_ADDR),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, OFT_SIZE),
		      SEP_SW_DESC_GET4TYPE(desc_p, CRYPTO_OP, OFT_NUM));

	/*SEP_LOG_DEBUG("0x%08X 0x%08X 0x%08X 0x%08X "
		      "0x%08X 0x%08X 0x%08X 0x%08X\n",
		      ((uint32_t*)desc_p)[0], ((uint32_t*)desc_p)[1],
		      ((uint32_t*)desc_p)[2], ((uint32_t*)desc_p)[3],
		      ((uint32_t*)desc_p)[4], ((uint32_t*)desc_p)[5],
		      ((uint32_t*)desc_p)[6], ((uint32_t*)desc_p)[7]);*/
}

static void dump_load_op_desc(const struct sep_sw_desc *desc_p)
{
	uint32_t *p = (uint32_t *)desc_p;
	uint32_t hcb, cache_idx, is_load;
	int idx;

	SEP_LOG_DEBUG("LOAD_OP (type=%lu,cookie=0x%08lX)\n",
			SEP_SW_DESC_GET(desc_p, TYPE),
			SEP_SW_DESC_GET(desc_p, COOKIE));

	for (idx = 0; idx < SEP_MAX_COMBINED_ENGINES; idx++) {
		cache_idx = BITFIELD_GET(
			p[SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_WORD_OFFSET],
			SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_OFFSET(idx),
			SEP_SW_DESC_LOAD_OP_FW_CACHE_IDX_BIT_SIZE);

		hcb = BITFIELD_GET(
			p[SEP_SW_DESC_LOAD_OP_HCB_ADDR_WORD_OFFSET(idx)],
			SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_OFFSET,
			SEP_SW_DESC_LOAD_OP_HCB_ADDR_BIT_SIZE);

		is_load = BITFIELD_GET(
			p[SEP_SW_DESC_LOAD_OP_L_WORD_OFFSET(idx)],
			SEP_SW_DESC_LOAD_OP_L_BIT_OFFSET,
			SEP_SW_DESC_LOAD_OP_L_BIT_SIZE);

		SEP_LOG_DEBUG("[%d] HCB=0x%08X FwIdx=%u %s\n",
				idx, hcb, cache_idx,
				is_load ? "(load)" : "(do not load)");
	}
}

static void dump_combined_op_desc(const struct sep_sw_desc *desc_p)
{
	SEP_LOG_DEBUG("COMBINED_OP::%s (type=%lu,cookie=0x%08lX)\n",
		      crypto_proc_mode_to_str(
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, PROC_MODE)),
		      SEP_SW_DESC_GET(desc_p, TYPE),
		      SEP_SW_DESC_GET(desc_p, COOKIE));

	SEP_LOG_DEBUG("SCHEME=0x%08lX %s%s\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, CONFIG_SCHEME),
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, L) ?
		      "(load)" : "",
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, I) ?
		      "(init)" : "");

	SEP_LOG_DEBUG("IFT: addr=0x%08lX , size=0x%08lX , tbl_num=%lu\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, IFT_ADDR),
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, IFT_SIZE),
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, IFT_NUM));

	SEP_LOG_DEBUG("OFT: addr=0x%08lX , size=0x%08lX , tbl_num=%lu\n",
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, OFT_ADDR),
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, OFT_SIZE),
		      SEP_SW_DESC_GET4TYPE(desc_p, COMBINED_OP, OFT_NUM));
}

static void dump_rpc_msg_desc(const struct sep_sw_desc *desc_p)
{
	SEP_LOG_DEBUG("RPC_MSG: agentId=%lu , funcId=%lu , "
		      "HmbAddr=0x%08lX, HmbSize=%lu\n",
		SEP_SW_DESC_GET4TYPE(desc_p, RPC_MSG, AGENT_ID),
		SEP_SW_DESC_GET4TYPE(desc_p, RPC_MSG, FUNC_ID),
		SEP_SW_DESC_GET4TYPE(desc_p, RPC_MSG, HMB_ADDR),
		SEP_SW_DESC_GET4TYPE(desc_p, RPC_MSG, HMB_SIZE));
}

static void dump_app_req_desc(const struct sep_sw_desc *desc_p)
{
	SEP_LOG_DEBUG("APP_REQ: reqType=%lu , sessionId=%lu , "
		      "InParamsAddr=0x%08lX\n",
		SEP_SW_DESC_GET4TYPE(desc_p, APP_REQ, REQ_TYPE),
		SEP_SW_DESC_GET4TYPE(desc_p, APP_REQ, SESSION_ID),
		SEP_SW_DESC_GET4TYPE(desc_p, APP_REQ, IN_PARAMS_ADDR));
}

static void dump_desc(const struct sep_sw_desc *desc_p)
{	/* dump descriptor based on its type */
	switch (SEP_SW_DESC_GET(desc_p, TYPE)) {
	case SEP_SW_DESC_TYPE_NULL:
		SEP_LOG_DEBUG("NULL descriptor type.\n");
		break;
	case SEP_SW_DESC_TYPE_CRYPTO_OP:
		dump_crypto_op_desc(desc_p);
		break;
	case SEP_SW_DESC_TYPE_LOAD_OP:
		dump_load_op_desc(desc_p);
		break;
	case SEP_SW_DESC_TYPE_COMBINED_OP:
		dump_combined_op_desc(desc_p);
		break;
	case SEP_SW_DESC_TYPE_RPC_MSG:
		dump_rpc_msg_desc(desc_p);
		break;
	case SEP_SW_DESC_TYPE_APP_REQ:
		dump_app_req_desc(desc_p);
		break;
	case SEP_SW_DESC_TYPE_SLEEP_REQ:
		SEP_LOG_DEBUG("SLEEP descriptor type.\n");
		break;
	case SEP_SW_DESC_TYPE_DEBUG:
		SEP_LOG_DEBUG("DEBUG descriptor type.\n");
		break;
	default:
		SEP_LOG_WARN("Unknown descriptor type = %lu\n",
			SEP_SW_DESC_GET(desc_p, TYPE));
	}
}
#endif /*DEBUG*/
