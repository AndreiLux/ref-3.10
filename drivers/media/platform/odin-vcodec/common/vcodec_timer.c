/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Jonghun Yoo <jonghun.yoo@lge.com>
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

#include "vcodec_timer.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm-generic/param.h>
#include <linux/mutex.h>

#include <media/odin/vcodec/vlog.h>

#define	VCODEC_TIMER_INTERVAL		(HZ * 2)

struct vcodec_timer_channel
{
	struct list_head list;

	bool open_state;

	struct
	{
		void *vcodec_id;
		/*
			Getting status of a vcore to check activity of the running vcore.
			- If status of the vcore is "running", return true.
			- If status of the vcore is "idle", return false.
		 */
		bool (*check_vcore_status)(void *vcodec_id);

		/*
			When a running core does not answer anything in time
			(VCODEC_TIMER_INTERVAL), "expire_timer" function is called.
		*/
		void (*expire_timer)(void *vcodec_id);
	} config;

	struct workqueue_struct *feed_timer_wq;
	struct work_struct feed_timer_work;
	struct timer_list feed_timer;

	struct
	{
		unsigned long event_time;
	} status;
};

struct mutex vcodec_timer_lock;
struct list_head vcodec_timer_instance_list;

static void _vcodec_reset_timer(
	struct vcodec_timer_channel *vcodec_timer_instance)
{
	vcodec_timer_instance->status.event_time = 0;
}

static void _vcodec_wq_feed_timer(struct work_struct *work)
{
	struct vcodec_timer_channel *vcodec_timer_instance
		= container_of(work, struct vcodec_timer_channel, feed_timer_work);
	bool vcore_running = false;
	unsigned long current_time;

	vcore_running = vcodec_timer_instance->config.check_vcore_status(
		vcodec_timer_instance->config.vcodec_id);

	mutex_lock(&vcodec_timer_lock);

	if (vcodec_timer_instance->open_state == false) {
		vlog_warning("closed state\n");
		mutex_unlock(&vcodec_timer_lock);
		return;
	}

	current_time = jiffies;
	vcodec_timer_instance->feed_timer.expires =
		current_time + VCODEC_TIMER_INTERVAL;

	add_timer(&vcodec_timer_instance->feed_timer);

	mutex_unlock(&vcodec_timer_lock);

	if (current_time < vcodec_timer_instance->status.event_time) {
		vcodec_timer_instance->status.event_time = current_time;
		vlog_warning("wraparound of jiffies\n");
		return;
	}

	if (vcore_running && current_time -
		vcodec_timer_instance->status.event_time > VCODEC_TIMER_INTERVAL) {
		vlog_error("over-processing time\n");

		mutex_lock(&vcodec_timer_lock);

		_vcodec_reset_timer(vcodec_timer_instance);

		mutex_unlock(&vcodec_timer_lock);

		vcodec_timer_instance->config.expire_timer(
			vcodec_timer_instance->config.vcodec_id);
	}
}

void vcodec_update_timer_status(void *vcodec_timer_id)
{
	struct vcodec_timer_channel *vcodec_timer_instance
		= (struct vcodec_timer_channel *)vcodec_timer_id;

	mutex_lock(&vcodec_timer_lock);

	vcodec_timer_instance->status.event_time = jiffies;

	mutex_unlock(&vcodec_timer_lock);
}

static void _vcodec_feed_timer_func(unsigned long data)
{
	struct vcodec_timer_channel *vcodec_timer_instance
		= (struct vcodec_timer_channel *)data;

	if (!vcodec_timer_instance) {
		vlog_error("vcodec_timer_instance is NULL\n");
		return;
	}
	else if (!vcodec_timer_instance->feed_timer_wq) {
		vlog_error("feed_timer_wq is NULL\n");
		return;
	}

	queue_work(vcodec_timer_instance->feed_timer_wq,
				&vcodec_timer_instance->feed_timer_work);
}

void *vcodec_timer_open(void *vcodec_id,
						bool (*check_vcore_status)(void *vcodec_id),
						void (*expire_timer)(void *vcodec_id))
{
	struct vcodec_timer_channel *vcodec_timer_instance;

	mutex_lock(&vcodec_timer_lock);

	vcodec_timer_instance =
			(struct vcodec_timer_channel *)
				vzalloc(sizeof(struct vcodec_timer_channel));
	if (!vcodec_timer_instance) {
		vlog_error("failed to allocate memory\n");
		mutex_unlock(&vcodec_timer_lock);
		return (void *)NULL;
	}

	vcodec_timer_instance->config.vcodec_id = vcodec_id;
	vcodec_timer_instance->config.check_vcore_status = check_vcore_status;
	vcodec_timer_instance->config.expire_timer = expire_timer;

	_vcodec_reset_timer(vcodec_timer_instance);

	vcodec_timer_instance->feed_timer_wq \
		= create_singlethread_workqueue("vcodec_feed_timer_wq");
	INIT_WORK(&vcodec_timer_instance->feed_timer_work, _vcodec_wq_feed_timer);

	init_timer(&vcodec_timer_instance->feed_timer);
	vcodec_timer_instance->feed_timer.data = (unsigned long)vcodec_timer_instance;
	vcodec_timer_instance->feed_timer.function = _vcodec_feed_timer_func;
	vcodec_timer_instance->feed_timer.expires = jiffies + VCODEC_TIMER_INTERVAL;
	add_timer(&vcodec_timer_instance->feed_timer);

	vcodec_timer_instance->open_state = true;

	list_add_tail(&vcodec_timer_instance->list, &vcodec_timer_instance_list);

	mutex_unlock(&vcodec_timer_lock);
	return (void *)vcodec_timer_instance;
}

void vcodec_timer_close(void *vcodec_timer_id)
{
	struct vcodec_timer_channel *vcodec_timer_instance
		= (struct vcodec_timer_channel *)vcodec_timer_id;

	mutex_lock(&vcodec_timer_lock);

	vcodec_timer_instance->open_state = false;

	list_del(&vcodec_timer_instance->list);

	mutex_unlock(&vcodec_timer_lock);

	del_timer_sync(&vcodec_timer_instance->feed_timer);
	flush_workqueue(vcodec_timer_instance->feed_timer_wq);
	destroy_workqueue(vcodec_timer_instance->feed_timer_wq);

	vfree(vcodec_timer_instance);
}

void vcodec_timer_init(void)
{
	mutex_init(&vcodec_timer_lock);
	INIT_LIST_HEAD(&vcodec_timer_instance_list);
}

void vcodec_timer_cleanup(void)
{
	mutex_destroy(&vcodec_timer_lock);
}

