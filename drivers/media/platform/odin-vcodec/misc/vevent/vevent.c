/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/uaccess.h>
#include <asm-generic/errno.h>

#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <media/odin/vcodec/vevent_io.h>
#include <media/odin/vcodec/vlog.h>

#include "vevent_eq.h"

struct vevent_desc
{
	struct miscdevice misc;

	spinlock_t lock;
	struct list_head channel_list;
	struct kmem_cache *kmem;
};
static struct vevent_desc *vevent_desc;

struct vevent_channel
{
	void *vevent_id;
	void *eq;
	bool wait_event;

	wait_queue_head_t wq;
	spinlock_t lock;
	struct list_head list;

	struct
	{
		unsigned int nr_sent;
	} debug;
};


static int _vevent_send(struct vevent_channel *vevent_ch,
						void *vevent_id,
						void *payload,
						int payload_size,
						bool userspace_buf,
						bool end_of_event)
{
	struct vevent_eq_node *node;
	int ret = 0;

	if (payload == NULL) {
		vlog_error("[VEVENT] Invalid payload addr:0x08%X, size:%d\n",
			(int)payload, payload_size);
		return -EINVAL;
	}

	if (payload_size > VEVENT_MAX_MSG_SIZE) {
		vlog_error("[VEVENT] payload_size is overflowed. payload_size:%d MAX:%d\n",
			payload_size, VEVENT_MAX_MSG_SIZE);
		return -EINVAL;
	}
	node = kmem_cache_zalloc(vevent_desc->kmem ,GFP_ATOMIC);
	if (node == NULL) {
		vlog_error("[VEVENT] node vzalloc failed\n");
		return -ENOMEM;
	}
	node->event.vevent_id = vevent_id;

	if (end_of_event) {
		node->event.type = VEVENT_TYPE_END_OF_EVENT;
	}
	else {
		node->event.size = payload_size;
		node->event.type = VEVENT_TYPE_EVENT;
		if (payload_size) {
			if (userspace_buf) {
				ret  = copy_from_user(node->event.msg, payload, payload_size);
				if (ret != 0 ) {
					vlog_error("copy_from_user failed. ret:%d \n", ret);
					kmem_cache_free(vevent_desc->kmem, node);
					return -1;
				}
			}
			else
				memcpy(node->event.msg, payload, payload_size);
		}
	}

	spin_lock(&vevent_ch->lock);

	if (end_of_event){
		if (vevent_eq_find_del_by_id(vevent_ch->eq,  node->event.vevent_id) > 0) {
			vlog_error("[VEVENT] vevent 0x%08X is exist but deleted.\n",
				(unsigned int)node->event.vevent_id);
		}
	}

	vevent_eq_push(vevent_ch->eq, node);
	vevent_ch->debug.nr_sent++;

	spin_unlock(&vevent_ch->lock);

	wake_up_interruptible(&vevent_ch->wq);

	return payload_size;
}

int vevent_send(void *vevent_id, void *payload, int payload_size)
{
	struct vevent_channel *vevent_ch, *tmp = NULL;

	if (payload == NULL) {
		vlog_error("[VEVENT] Invalid payload addr:0x08%X, size:%d\n",
			(int)payload, payload_size);
		return -EINVAL;
	}

	if (payload_size > VEVENT_MAX_MSG_SIZE) {
		vlog_error("[VEVENT] payload_size is overflowed. payload_size:%d MAX:%d\n",
			payload_size, VEVENT_MAX_MSG_SIZE);
		return -EINVAL;
	}

	spin_lock(&vevent_desc->lock);

	list_for_each_entry_safe(vevent_ch, tmp, &vevent_desc->channel_list, list) {
		if (((int)vevent_ch->vevent_id & 0xffff0000) == \
			((int)vevent_id & 0xffff0000) )
			break;
	}

	spin_unlock(&vevent_desc->lock);

	if (&vevent_ch->list == &vevent_desc->channel_list) {
		vlog_error("[VEVENT]Can`t find vevent channel(vevent_id:0x%08X)\n", \
					(int)vevent_id);
		return -ENOTCONN;
	}

	return _vevent_send(vevent_ch, vevent_id, payload, payload_size, false, false);
}

static int _vevent_open(struct inode *inode, struct file *file)
{
	struct vevent_channel *vevent_ch = NULL;

	vlog_print(VLOG_VEVENT, "open\n");

	vevent_ch = vzalloc(sizeof(struct vevent_channel));
	if (vevent_ch == NULL) {
		vlog_error("[VEVENT] vzalloc failed\n");
		return -ENOMEM;
	}

	vevent_ch->vevent_id = (void*)(((current->tgid) << 16) & 0xffff0000);
	vevent_ch->wait_event = false;

	spin_lock_init(&vevent_ch->lock);
	init_waitqueue_head(&vevent_ch->wq);
	INIT_LIST_HEAD(&vevent_ch->list);

	vevent_ch->eq = vevent_eq_open();
	if (IS_ERR_OR_NULL(vevent_ch->eq)) {
		vlog_error("[VEVENT] vevent_eq_failed\n");
		vfree(vevent_ch);
		return -1;
	}

	spin_lock(&vevent_desc->lock);

	list_add(&vevent_ch->list, &vevent_desc->channel_list);

	spin_unlock(&vevent_desc->lock);

	file->private_data = (void*)vevent_ch;

	return 0;
}

static int _vevent_release(struct inode *inode, struct file *file)
{
	struct vevent_channel *vevent_ch = (struct vevent_channel*)file->private_data;
	if (vevent_ch == NULL)
		return -1;

	vlog_print(VLOG_VEVENT, "close\n");
	vlog_print(VLOG_VEVENT, "id:0x%08X sent:%d\n",
		(int)vevent_ch->vevent_id, vevent_ch->debug.nr_sent);

	spin_lock(&vevent_desc->lock);

	list_del(&vevent_ch->list);

	spin_unlock(&vevent_desc->lock);

	if (vevent_ch->wait_event)
		wake_up_interruptible_sync(&vevent_ch->wq);

	if (spin_is_locked(&vevent_ch->lock))
		spin_unlock_wait(&vevent_ch->lock);

	vevent_eq_close(vevent_ch->eq);

	vfree(vevent_ch);

	return 0;
}

int _vevent_read(struct file *file,
					char __user *buf, size_t size, loff_t *offset)
{
	struct vevent_channel *vevent_ch = (struct vevent_channel*)file->private_data;
	struct vevent *event = (struct vevent *)buf;
	struct vevent_eq_node *node = NULL;
	int ret = 0;

woken_up:
	/*vlog_print(VLOG_VEVENT, "blocking read wake up\n");*/
	spin_lock(&vevent_ch->lock);
	node = vevent_eq_peep(vevent_ch->eq);

	if (node == NULL) {
		spin_unlock(&vevent_ch->lock);
		if (file->f_flags & O_NONBLOCK)
			return -1;
		else {
			vevent_ch->wait_event = true;
			/*vlog_print(VLOG_VEVENT, "blocking read went to sleep\n");*/
			ret = wait_event_interruptible(
										vevent_ch->wq,
										vevent_eq_peep(vevent_ch->eq)) ;
			if (ret < 0) {
				vlog_error("[VEVENT] wait_event_interruptible failed. ret:%d\n", ret);
				if (ret == -ERESTARTSYS)
					vlog_error("[VEVENT] exit_signal is %d\n", current->exit_signal);
				vevent_ch->wait_event = false;
				return -1;
			}
			goto woken_up;
		}
	}

	vevent_ch->wait_event = false;

	if (copy_to_user(event, &node->event, sizeof(struct vevent)) != 0) {
		vlog_error("[VEVENT] copy_to_user failed\n");
		spin_unlock(&vevent_ch->lock);
		return 0;
	}

	node = vevent_eq_pop(vevent_ch->eq);
	kmem_cache_free(vevent_desc->kmem, node);
	spin_unlock(&vevent_ch->lock);

	return sizeof(struct vevent);
}

int _vevent_write(struct file *file,
					const char __user *buf, size_t size, loff_t *offset)
{
	struct vevent_channel *vevent_ch = (struct vevent_channel*)file->private_data;
	struct vevent *event = (struct vevent*)buf;

	/*vlog_print(VLOG_VEVENT, "write\n");*/

	if (event->type == VEVENT_TYPE_END_OF_EVENT)
		return _vevent_send(vevent_ch, event->vevent_id,
				(void*)event->msg, event->size, true, true);
	else
		return _vevent_send(vevent_ch, event->vevent_id,
				(void*)event->msg, event->size, true, false);
}

void vevent_cb_node_free(void *node)
{
	kmem_cache_free(vevent_desc->kmem, node);
}

static struct file_operations fops =
{
	.open = _vevent_open,
	.release = _vevent_release,
	.read = _vevent_read,
	.write = _vevent_write,
};

static int __init vevent_init(void)
{
	vevent_desc = vzalloc(sizeof(struct vevent_desc));
	if (vevent_desc == NULL) {
		vlog_error("[VEVNET] vzalloc failed\n");
		goto err_vzalloc;
	}

	spin_lock_init(&vevent_desc->lock);
	INIT_LIST_HEAD(&vevent_desc->channel_list);

	vevent_desc->kmem = kmem_cache_create("vevent",
						sizeof(struct vevent_eq_node), 0, 0, NULL);
	if (vevent_desc->kmem == NULL) {
		vlog_error("[VEVENT] kmem_cache_create failed\n");
		goto err_kmem_cache_create;
	}

	vevent_eq_init(vevent_cb_node_free);

	vevent_desc->misc.minor = MISC_DYNAMIC_MINOR;
	vevent_desc->misc.name = "vevent";
	vevent_desc->misc.mode = 0777;
	vevent_desc->misc.fops = &fops;

	if (misc_register(&vevent_desc->misc) < 0) {
		vlog_error("[VEVENT] misc_register failed\n");
		goto err_misc_register;
	}


	return 0;

err_misc_register:
	if (vevent_desc->kmem) {
		kmem_cache_destroy(vevent_desc->kmem);
		vevent_desc->kmem = NULL;
	}

err_kmem_cache_create:
	vfree(vevent_desc);

err_vzalloc:
	return -1;
}

static void __exit vevent_cleanup(void)
{
	if (vevent_desc == NULL)
		return;

	misc_deregister(&vevent_desc->misc);
	if (vevent_desc->kmem) {
		kmem_cache_destroy(vevent_desc->kmem);
		vevent_desc->kmem = NULL;
	}
	vfree(vevent_desc);
}

module_init(vevent_init);
module_exit(vevent_cleanup);

MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("VCODEC EVENT HANDLER DRIVER");
MODULE_LICENSE("GPL");

