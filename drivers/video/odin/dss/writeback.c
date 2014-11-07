/*
 * linux/drivers/video/odin/dss/writeback.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author: Jaeky Oh <jaeky.oh@lge.com>
 * Some code and ideas taken from drivers/video/omap/ driver
 * linux/drivers/video/omap2/dss/wb.c
 * Copyright (C) 2009 Texas Instruments
 * Author: mythripk <mythripk@ti.com>

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <video/odindss.h>
#include "linux/hrtimer.h"
#include "linux/ktime.h"

#include "dss.h"
#include "dss-features.h"

#define DSS_SUBSYS_NAME "WRITEBACK"

#define	WB_COMPLETE_CHECK_POLLING 1
#define	WB_COMPLETE_CHECK_TIMER   2
#define	WB_COMPLETE_CHECK_HRTIMER	3
#define	WB_COMPLETE_CHECK_INT	4

#ifdef WRITEBACK_FRAMEDONE_POLLONG
#define	WB_COMPLETE_CHECK_METHOD 	WB_COMPLETE_CHECK_POLLING
#else
#define	WB_COMPLETE_CHECK_METHOD 	WB_COMPLETE_CHECK_INT
#endif

static int num_writebacks;
static struct list_head wb_list;

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
#define MS_TO_NS(x) 		(x * 1E6L)
#define MEM2MEM_POLLING_TIME	1L
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_POLLING)
#define US_POLING 		100
#define WB_VERIFY_CNT	40
#define TIMEOUT_RETRY	5
#endif

static struct attribute *writeback_sysfs_attrs[] = {
	NULL
};

struct {
	spinlock_t irq_lock;
	struct odin_writeback *wb;
#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_TIMER)
	struct timer_list wb_timer;
#endif
#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
	struct hrtimer hr_timer;
#endif
	ktime_t ktime;
} wb_dev;

static ssize_t writeback_attr_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	return 0;
}

static ssize_t writeback_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static const struct sysfs_ops writeback_sysfs_ops = {
	.show = writeback_attr_show,
	.store = writeback_attr_store,
};

static struct kobj_type writeback_ktype = {
	.sysfs_ops = &writeback_sysfs_ops,
	.default_attrs = writeback_sysfs_attrs,
};

bool odin_dss_check_wb(struct writeback_cache_data *wb, int overlayId,
			int managerId)
{
	bool result = false;
	DSSDBG("ovl=%d,mgr=%d,mode=%s(%s),src=%d\n",
	   overlayId, managerId,
	   wb->mode == ODIN_WB_MEM2MEM_MODE ? "mem2mem" : "capture",
	   overlayId <= ODIN_WB_VID1 ? "overlay" : "manager",
	   managerId);

	/* TODO: */
		result = true;

	return result;
}

static bool dss_check_wb(struct odin_writeback *wb)
{
	DSSDBG("srctype=%s,src=%d\n",
	   wb->id <= ODIN_WB_VID1 ? "overlay" : "manager",
	   wb->id <= ODIN_WB_VID1 ? wb->id : wb->info.channel);

	return 0;
}

static int odin_dss_wb_set_info(struct odin_writeback *wb,
		struct odin_writeback_info *info)
{
	int r;
	struct odin_writeback_info old_info;
	old_info = wb->info;
	wb->info = *info;

	r = dss_check_wb(wb);
	if (r) {
		wb->info = old_info;
		return r;
	}

	wb->info_dirty = true;

	return 0;
}

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_TIMER)
void wb_timer_callback(unsigned long arg)
{
	int ret;
	struct timer_list *ptr_wb_timer;
	struct odin_writeback *wb;
	unsigned long flags;

	wb = (struct odin_writeback *)arg;
	spin_lock_irqsave(&wb_dev.irq_lock, flags);
	ptr_wb_timer = &wb_dev.wb_timer;
	spin_unlock_irqrestore(&wb_dev.irq_lock, flags);

	static int cnt;
	static int correct_cnt=0;
	DSSERR( "wb_timer_callback called (%ld) cnt = %d.\n", jiffies, cnt++);
	ret = odin_du_query_sync_wb_done(wb->plane);
	if (ret == 1)
	{
		correct_cnt++;
		if (correct_cnt == 3)
		{
			del_timer(ptr_wb_timer);
			/* odin_du_set_channel_sync_enable((u8)wb->info.channel, 0); */
			DSSDBG( "wb_timer_callback ended addr = %x.\n", wb->info.p_y_addr);
			complete(&wb->wb_completion);
			cnt = 0;
			correct_cnt = 0;
		}
		else
			mod_timer(ptr_wb_timer, get_jiffies_64() + HZ/100);
	}
	else
		mod_timer(ptr_wb_timer, get_jiffies_64() + HZ/100);

}
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
enum hrtimer_restart wb_timer_callback(struct hrtimer *timer)
{
	int ret;
	struct odin_writeback *wb;
	static int cnt;
	unsigned long flags;
	ktime_t ktime;

	spin_lock_irqsave(&wb_dev.irq_lock, flags);
	wb = wb_dev.wb;
	spin_unlock_irqrestore(&wb_dev.irq_lock, flags);

	DSSDBG( "wb_timer_callback called (%ld) cnt = %d. plane = %d \n",
		jiffies, cnt++, wb->plane);
	ret = odin_du_query_sync_wb_done((enum odin_dma_plane) wb->plane);

	if (ret == 1)
	{
		/* odin_du_set_channel_sync_enable((u8)wb->info.channel, 0); */
		DSSDBG( "wb_timer_callback ended addr = %x , cnt = %d.\n",
			wb->info.p_y_addr, cnt);
		complete(&wb->wb_completion);
		cnt = 0;
		return HRTIMER_NORESTART;
	}
	else
	{
		hrtimer_forward(timer, timer->base->get_time(), wb_dev.ktime);
		return HRTIMER_RESTART;
	}
}
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_INT)
void wb_timer_callback(void *data, u32 mask, u32 sub_mask)
{
	struct odin_writeback* wb;

	wb = (struct odin_writeback*) data;

	DSSDBG("wb_timer_callback \n");
	/* ret = odin_du_query_sync_wb_done((enum odin_dma_plane) wb->plane); */

	/* if (ret == 1) */
	{
		complete(&wb->wb_completion);
	}
}
#endif

int odin_dss_wb_wait_framedone(struct odin_writeback *wb)
{
	unsigned long timeout = msecs_to_jiffies(500);

	int r = 0;

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_TIMER)
	struct timer_list *ptr_wb_timer;
	ptr_wb_timer = &wb_dev.wb_timer;

	DSSDBG( "STRAT: odin_dss_wb_wait_framedone add = %x\n", wb->info.p_y_addr);
	timeout = wait_for_completion_interruptible_timeout(&wb->wb_completion,
			timeout);

	DSSDBG( "END : odin_dss_wb_wait_framedone add = %x\n", wb->info.p_y_addr);
	if (timeout == 0)
	{
		del_timer(ptr_wb_timer);
		r = -ETIMEDOUT;
		DSSWARN("timeout \n");

	}
	else  if (timeout == -ERESTARTSYS)
	{
		del_timer(ptr_wb_timer);
		r = timeout;
		DSSWARN("ERESTARTSYS \n");
	}
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
	struct hrtimer *ptr_wb_timer;
	ptr_wb_timer = &wb_dev.hr_timer;

	DSSDBG( "STRAT: odin_dss_wb_wait_framedone add = %x\n", wb->info.p_y_addr);
	timeout = wait_for_completion_interruptible_timeout(&wb->wb_completion,
		timeout);

	DSSDBG( "END : odin_dss_wb_wait_framedone add = %x\n", wb->info.p_y_addr);
	if (timeout == 0)
	{
		r = -ETIMEDOUT;
		DSSERR("timeout \n");
	}
	else  if (timeout == -ERESTARTSYS)
	{
		r = timeout;
		DSSERR("ERESTARTSYS \n");
	}

	hrtimer_cancel(ptr_wb_timer);
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_POLLING)
		int i, j;
		int retry_cnt = 0;
		volatile int ret;
		unsigned int poll_cnt;
		bool preview_cb = false;
		int timeout_cnt;
		if ((wb->info.color_mode & 0xf) == ODIN_DSS_COLOR_YUV420P_2)
			preview_cb = true;

		if (preview_cb)
		{
			poll_cnt = 50000 / US_POLING;
			timeout_cnt = 499;
		}
		else
		{
			poll_cnt = 100000 / US_POLING;
			timeout_cnt = 999;
		}

		while(retry_cnt < TIMEOUT_RETRY)
		{
			for (i = 0; i < poll_cnt; i++) {
				udelay(US_POLING);
				ret = odin_du_query_sync_wb_done(
					(enum odin_dma_plane) wb->plane);
				if (ret == 1) {
					for (j = 0; j < WB_VERIFY_CNT; j++) {
						udelay(5);
						ret = odin_du_query_sync_wb_done(
						(enum odin_dma_plane) wb->plane);
						if (ret != 1)
							break;
						else
							continue;
					}

					if (j < WB_VERIFY_CNT)
						continue;
					else
						break;
				}
			}

			if (i > timeout_cnt)
				r = -ETIMEDOUT;
			else
			{
				r = 0;
				if (retry_cnt > 0)
					DSSDBG( "WB retry is started retry=%d\n", retry_cnt);
				break;
			}
			odin_du_enable_wb_plane(wb->info.channel,
				wb->plane, ODIN_WB_MEM2MEM_MODE, true);
			retry_cnt++;
		}

		DSSDBG( "END : odin_dss_wb_wait_framedone add = %x, cnt = %d\n",
			wb->info.p_y_addr, i);
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_INT)
	timeout = wait_for_completion_interruptible_timeout(&wb->wb_completion,
			timeout);

	DSSDBG( "END : odin_dss_wb_wait_framedone \n");

	if (timeout == 0)
	{
		r = -ETIMEDOUT;
		DSSINFO("enabled:%d, info_dirty:%d, y_addr:%x, uv_addr:%x \n", wb->info.enabled, wb->info.info_dirty, wb->info.p_y_addr, wb->info.p_uv_addr);
		DSSINFO("width:%d, height:%d, outx:%d, outy:%d, outw:%d, outh:%d \n", wb->info.width, wb->info.height, wb->info.out_x, wb->info.out_y, wb->info.out_w, wb->info.out_h);;
		DSSINFO("color_mode:%x, mode:%d, ch:%d \n",  wb->info.color_mode,  wb->info.mode,  wb->info.channel);
	}
	else  if (timeout == -ERESTARTSYS)
	{
		r = timeout;
	}
#endif

	return r;
}

static int odin_dss_wb_register_framedone(struct odin_writeback *wb)
{
#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_TIMER)
	struct timer_list *ptr_wb_timer;
	ptr_wb_timer = &wb_dev.wb_timer;
	ptr_wb_timer->data = (unsigned long) wb;

	/*  INIT_COMPLETION(wb->wb_completion); */
	add_timer(ptr_wb_timer);
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
	ktime_t ktime;
	struct hrtimer *ptr_wb_timer;
	wb_dev.wb = wb;
	ptr_wb_timer = &wb_dev.hr_timer;
	ktime = ktime_set(0, MS_TO_NS(MEM2MEM_POLLING_TIME));
	wb_dev.ktime = ktime;

	/* INIT_COMPLETION(wb->wb_completion); */
	hrtimer_start(ptr_wb_timer, ktime, HRTIMER_MODE_REL);
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_INT)
	int r;
	r = odin_crg_register_isr(wb_timer_callback, wb, CRG_IRQ_mXD,
		NULL);
	/* INIT_COMPLETION(wb->wb_completion); */
#endif

	return 0;
}

static int odin_dss_wb_unregister_framedone(struct odin_writeback *wb)
{
#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_INT)
	odin_crg_unregister_isr(wb_timer_callback, wb, CRG_IRQ_mXD,
		NULL);
#endif

	return 0;
}

int odin_dss_wb_init_completion(struct odin_writeback *wb)
{
	DSSDBG( "START : odin_dss_wb_wait_framedone \n");
	INIT_COMPLETION(wb->wb_completion);
}

static void odin_dss_wb_get_info(struct odin_writeback *wb,
		struct odin_writeback_info *info)
{
	*info = wb->info;
}

struct odin_writeback *odin_dss_get_writeback(int num)
{
	int i = 0;
	struct odin_writeback *wb;

	list_for_each_entry(wb, &wb_list, list) {
		if (i++ == num)
			return wb;
	}

	return NULL;
}
EXPORT_SYMBOL(odin_dss_get_writeback);

static void dss_add_writeback(struct odin_writeback *wb)
{
	++num_writebacks;
	list_add_tail(&wb->list, &wb_list);
}

int odin_wb_dss_get_num_writebacks(void)
{
	return num_writebacks;
}

void odin_wb_dss_init_writeback(struct platform_device *pdev)
{
	int r;
	struct odin_writeback *wb;
	int i;

	INIT_LIST_HEAD(&wb_list);

	for (i = 0; i < dss_feat_get_num_wbs(); i++) {
		wb = kzalloc(sizeof(*wb), GFP_KERNEL);

		BUG_ON(wb == NULL);

		wb->check_wb = &dss_check_wb;
		wb->set_wb_info = &odin_dss_wb_set_info;
		wb->get_wb_info = &odin_dss_wb_get_info;
		wb->register_framedone = &odin_dss_wb_register_framedone;
		wb->unregister_framedone = &odin_dss_wb_unregister_framedone;
		wb->wait_framedone = &odin_dss_wb_wait_framedone;
		wb->initial_completion = &odin_dss_wb_init_completion;

		switch (i) {
		case 0:
			wb->name = "vid0";
			wb->id = ODIN_WB_VID0;
			wb->plane = ODIN_DSS_VID0;
			break;
		case 1:
			wb->name = "vid1";
			wb->id = ODIN_WB_VID1;
			wb->plane = ODIN_DSS_VID1;
			break;
 		case 2:
			wb->name = "mScaler";
			wb->id = ODIN_WB_mSCALER;
			wb->plane = ODIN_DSS_mSCALER;
			break;

		default:
			DSSERR("writeback number is over \n");
			continue;
		}

		mutex_init(&wb->lock);

		init_completion(&wb->wb_completion);
#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_INT)
		complete(&wb->wb_completion);
#endif
		dss_add_writeback(wb);

		r = kobject_init_and_add(&wb->kobj, &writeback_ktype,
				&pdev->dev.kobj, "writeback%d", i);

		if (r)
		{
			DSSERR("failed to create sysfs file\n");
			continue;
		}
	}

	spin_lock_init(&wb_dev.irq_lock);

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_TIMER)
	struct timer_list *ptr_wb_timer;
	ptr_wb_timer = &wb_dev.wb_timer;

	init_timer(ptr_wb_timer);
	ptr_wb_timer->expires = get_jiffies_64() + HZ/100;
	ptr_wb_timer->data = (unsigned long) wb;
	ptr_wb_timer->function = &wb_timer_callback;
#endif

#if (WB_COMPLETE_CHECK_METHOD == WB_COMPLETE_CHECK_HRTIMER)
	struct hrtimer *ptr_wb_timer;
	ptr_wb_timer = &wb_dev.hr_timer;
	hrtimer_init(ptr_wb_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ptr_wb_timer->function = &wb_timer_callback;
#endif

}

void odin_wb_dss_uninit_writeback(struct platform_device *pdev)
{
	struct odin_writeback *wb;

	while (!list_empty(&wb_list)) {
		wb = list_first_entry(&wb_list,
				struct odin_writeback, list);
		list_del(&wb->list);
		kobject_del(&wb->kobj);
		kobject_put(&wb->kobj);
		kfree(wb);
	}
}

