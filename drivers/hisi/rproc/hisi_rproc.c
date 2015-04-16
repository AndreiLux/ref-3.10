/*
 * hisi rproc communication interface
 *
 * Copyright (c) 2013- Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/huawei/mailbox.h>
#include <linux/huawei/hisi_rproc.h>

#define MODULE_NAME "hisi_rproc"
#define RPROC_IPC_MSG_MAX 8
#define PARAM_END -1

#define READY()				do { is_ready = 1; } while (0)
#define NOT_READY()			do { is_ready = 0; } while (0)
#define IS_READY()			({ is_ready; })
#define IS_NULL(ptr)			(!ptr)
#define VALID_RPROC(rp, args...)	valid_rproc(rp, args)

typedef enum {
	ASYNC_CALL = 0,
	SYNC_CALL
} call_type_t;

enum {
	LPM3 = 0,
	LPM3_FOR_REGULATOR,
	LPM3_FOR_DEBUG,
	LPM3_FOR_RDR,
	IOM3,
	HIFI,
	/* MCPU, */
	rproc_max
};

struct hisi_rproc_info {
	const char			*rp_name;
	const char			*mbox_rp;
	struct atomic_notifier_head	notifier;
	struct notifier_block		nb;
	struct hisi_mbox		*mbox;
};

struct hisi_rproc_context {
	rproc_complete_t		complete_fn;
	void				*data;
	struct completion		complete;
	rproc_msg_t			*ack_buffer;
	rproc_msg_len_t			ack_buffer_len;
	mbox_msg_t			*tx_buffer;
};

static int is_ready;

struct hisi_rproc_info rproc_table[rproc_max] = {
	{
		.rp_name = HISI_RPROC_LPM3,
		.mbox_rp = HI3630_MAILBOX_RP_LPM3,
	},
	{
		.rp_name = HISI_RPROC_LPM3_FOR_REGULATOR,
		.mbox_rp = HI3630_MAILBOX_RP_LPM3_SYNC,
	},
	{
		.rp_name = HISI_RPROC_LPM3_FOR_DEBUG,
		.mbox_rp = HI3630_MAILBOX_RP_LPM3,
	},
	{
		.rp_name = HISI_RPROC_LPM3_FOR_RDR,
		.mbox_rp = HI3630_MAILBOX_RP_LPM3_FOR_RDR,
	},
	{
		.rp_name = HISI_RPROC_IOM3,
		.mbox_rp = HI3630_MAILBOX_RP_IOM3,
	},
	{
		.rp_name = HISI_RPROC_HIFI,
		.mbox_rp = HI3630_MAILBOX_RP_HIFI,
	},
	/* {.name = HISI_RPROC_MCPU,}, */
};

static inline
struct hisi_rproc_info *find_rproc(const char *rproc_name)
{
	struct hisi_rproc_info *rproc = NULL;
	int i;

	for (i = LPM3; i < rproc_max; i++) {
		if (!strcmp(rproc_name, rproc_table[i].rp_name)) {
			rproc = &rproc_table[i];
			if (!rproc->mbox)
				rproc = NULL;
			break;
		}
	}

	return rproc;
}

static struct hisi_rproc_info *valid_rproc(const char *rproc_name, ...)
{
	va_list args;
	unsigned int ptr = (unsigned int)rproc_name;

	va_start(args, rproc_name);
	do {
		if (IS_NULL(ptr)) {
			va_end(args);
			return NULL;
		}

		ptr = va_arg(args, unsigned int);
	} while (PARAM_END != ptr);
	va_end(args);

	return find_rproc(rproc_name);
}

static void hisi_rproc_xfer_async_complete(struct hisi_mbox_task *task)
{
	struct hisi_rproc_context *context =
		(struct hisi_rproc_context *)task->context;

	if (context->complete_fn) {
		context->complete_fn((rproc_msg_t *)task->ack_buffer,
				(rproc_msg_len_t)task->ack_buffer_len,
				task->tx_error,
				context->data);
	}

	hisi_mbox_task_free(&task);
	kfree(context->tx_buffer);
	kfree(context);
	return;
}

static int hisi_rproc_xfer(struct hisi_rproc_info *rproc,
				rproc_msg_t *msg,
				rproc_msg_len_t len,
				rproc_msg_type_t is_sync_msg,
				struct hisi_rproc_context *context,
				call_type_t is_sync_call)
{
	struct hisi_mbox *mbox = rproc->mbox;
	struct hisi_mbox_task *tx_task;
	const char *rproc_name = rproc->mbox_rp;
	mbox_msg_t tx_buffer_for_sync[RPROC_IPC_MSG_MAX] = {0};
	mbox_msg_t *tx_buffer = NULL;
	mbox_msg_len_t tx_buffer_len = len;
	mbox_complete_t complete_fn = hisi_rproc_xfer_async_complete;
	mbox_ack_type_t ack_type = is_sync_msg ? MANUAL_ACK : AUTO_ACK;
	mbox_msg_t *ack_buffer;
	mbox_msg_t ack_buffer_len;
	int ret = 0;

	tx_buffer = is_sync_call ? tx_buffer_for_sync :
				kzalloc(sizeof(*msg) * len, GFP_ATOMIC);
	if (!tx_buffer) {
		pr_err("%s: %s, %d, no mem\n", MODULE_NAME, __func__, __LINE__);
		ret = -ENOMEM;
		return ret;
	}
	memcpy(tx_buffer, msg, (sizeof(*msg) / sizeof(u8) * len));

	switch (is_sync_call) {
	case SYNC_CALL:
		ack_buffer = context->ack_buffer;
		ack_buffer_len = context->ack_buffer_len;
		ret = hisi_mbox_msg_send_sync(mbox,
					rproc_name,
					tx_buffer,
					tx_buffer_len,
					ack_type,
					ack_buffer,
					ack_buffer_len);
		if (ret) {
			pr_err("%s: %s, %d, fail to sync send\n",
					MODULE_NAME, __func__, __LINE__);
		}

		break;
	case ASYNC_CALL:
		context->tx_buffer = tx_buffer;
		tx_task = hisi_mbox_task_alloc(mbox,
					rproc_name,
					tx_buffer,
					tx_buffer_len,
					ack_type,
					complete_fn,
					context);
		if (!tx_task) {
			pr_err("%s: %s, %d, no mem\n",
				MODULE_NAME, __func__, __LINE__);
			kfree(tx_buffer);
			ret = -ENOMEM;
			break;
		}

		ret = hisi_mbox_msg_send_async(mbox, tx_task);
		if (ret) {
			pr_err("%s: %s, %d, fail to async send\n",
					MODULE_NAME, __func__, __LINE__);
			hisi_mbox_task_free(&tx_task);
			kfree(tx_buffer);
			context->tx_buffer = NULL;
		}

		break;
	default:
		pr_warn("%s: %s, %d, unexpected branch\n",
				MODULE_NAME, __func__, __LINE__);
		break;
	}

	return ret;
}

int hisi_rproc_xfer_async(const char *rproc_name,
			rproc_msg_t *msg,
			rproc_msg_len_t len,
			rproc_msg_type_t is_sync_msg,
			rproc_complete_t complete_fn,
			void *data)
{
	struct hisi_rproc_info *rproc;
	struct hisi_rproc_context *context;
	int ret = 0;

	BUG_ON(!IS_READY());

	rproc = VALID_RPROC(rproc_name, msg, PARAM_END);
	if (!rproc) {
		pr_err("%s: %s, %d, invalid rproc xfer\n",
				MODULE_NAME, __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	context = kzalloc(sizeof(*context), GFP_ATOMIC);
	if (!context) {
		pr_err("%s: %s, %d, no mem\n", MODULE_NAME, __func__, __LINE__);
		ret = -ENOMEM;
		goto out;
	}

	context->complete_fn = complete_fn;
	context->data = data;

	ret = hisi_rproc_xfer(rproc, msg, len, is_sync_msg,
					context, ASYNC_CALL);
	if (ret)
		kfree(context);

out:
	return ret;
}
EXPORT_SYMBOL(hisi_rproc_xfer_async);

int hisi_rproc_xfer_sync(const char *rproc_name,
			rproc_msg_t *msg,
			rproc_msg_len_t len,
			rproc_msg_type_t is_sync_msg,
			rproc_msg_t *ack_buffer,
			rproc_msg_len_t ack_buffer_len)
{
	struct hisi_rproc_info *rproc;
	struct hisi_rproc_context context;
	int ret = 0;

	BUG_ON(!IS_READY());

	rproc = VALID_RPROC(rproc_name, msg, PARAM_END);
	if (!rproc) {
		pr_err("%s: %s, %d, invalid rproc xfer\n",
				MODULE_NAME, __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	context.ack_buffer = ack_buffer;
	context.ack_buffer_len = ack_buffer_len;

	ret = hisi_rproc_xfer(rproc, msg, len, is_sync_msg,
					&context, SYNC_CALL);

out:
	return ret;
}
EXPORT_SYMBOL(hisi_rproc_xfer_sync);

static int
hisi_rproc_rx_notifier(struct notifier_block *nb, unsigned long len, void *msg)
{
	struct hisi_rproc_info *rproc =
		container_of(nb, struct hisi_rproc_info, nb);

	atomic_notifier_call_chain(&rproc->notifier, len, msg);
	return 0;
}

int
hisi_rproc_rx_register(const char *rproc_name, struct notifier_block *nb)
{
	struct hisi_rproc_info *rproc;
	int ret = 0;

	BUG_ON(!IS_READY());

	rproc = VALID_RPROC(rproc_name, nb, PARAM_END);
	if (!rproc) {
		pr_err("%s: %s, %d, invalid rproc xfer\n",
				MODULE_NAME, __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	atomic_notifier_chain_register(&rproc->notifier, nb);
out:
	return ret;
}
EXPORT_SYMBOL(hisi_rproc_rx_register);

int
hisi_rproc_rx_unregister(const char *rproc_name, struct notifier_block *nb)
{
	struct hisi_rproc_info *rproc;
	int ret = 0;

	BUG_ON(!IS_READY());

	rproc = VALID_RPROC(rproc_name, nb, PARAM_END);
	if (!rproc) {
		pr_err("%s: %s, %d, invalid rproc xfer\n",
				MODULE_NAME, __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	atomic_notifier_chain_unregister(&rproc->notifier, nb);
out:
	return ret;
}
EXPORT_SYMBOL(hisi_rproc_rx_unregister);


static int __init hisi_rproc_init(void)
{
	struct hisi_rproc_info *rproc;
	int i;

	for (i = 0; i < rproc_max; i++) {
		rproc = &rproc_table[i];

		ATOMIC_INIT_NOTIFIER_HEAD(&rproc->notifier);

		rproc->nb.next = NULL;
		rproc->nb.notifier_call = hisi_rproc_rx_notifier;

		rproc->mbox = hisi_mbox_get(rproc->mbox_rp, &rproc->nb);
		if (!rproc->mbox) {
			pr_warn("%s: func, %s line, %d!\nrproc %s failed to get mailbox. Now, bug on...\n\n",
						MODULE_NAME, __func__,
						__LINE__, rproc->rp_name);
			/* For debugging */
			BUG_ON(1);
		}
	}

	if (rproc_table[LPM3_FOR_DEBUG].mbox)
		MBOX_DEBUG_ON(rproc_table[LPM3_FOR_DEBUG].mbox);

	READY();

	pr_info("%s: ready\n", MODULE_NAME);
	return 0;
}
arch_initcall(hisi_rproc_init);

static void __exit hisi_rproc_exit(void)
{
	struct hisi_rproc_info *rproc;
	int i;

	NOT_READY();

	for (i = 0; i < rproc_max; i++) {
		rproc =  &rproc_table[i];
		if (rproc->mbox)
			hisi_mbox_put(&rproc->mbox);
	}

	return;
}
module_exit(hisi_rproc_exit);

MODULE_DESCRIPTION("HISI rproc communication interface");
MODULE_LICENSE("GPL V2");
