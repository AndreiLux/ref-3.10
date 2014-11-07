/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/odin_pm_domain.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>

#include "hal/du_hal.h"
#include "hal/sync_hal.h"

#include "dss_irq.h"
#include "sync_src.h"


/* DEFAULT RASTER SIZE is 60hz when dispX_clk is 148.5Mhz */
#define DEFAULT_V_RASTER_SIZE 2048	/* 2^11 */
#define DEFAULT_H_RASTER_SIZE 1208
#define DIVIDE_V_RASTER_SIZE(x)	(x<<11)

#define DEFAULT_ACT_V 1	/* Youngjo Kim said Donggi CHOI recommends this value */
#define DEFAULT_ACT_H 1 /* Youngjo Kim said Donggi CHOI recommends this value */

struct sync_global_tick_isr {
	struct list_head node;
	void (*isr)(enum sync_ch_id sync_ch);
};

struct sync_desc_t {
	enum sync_ch_id global_tick_ref;
	unsigned int global_tick;

	struct list_head global_isr_head;
	spinlock_t  global_isr_head_lock;

	struct timer_list frequency_measure_timer;
};
static struct sync_desc_t *d = NULL;

struct sync_isr_t
{
	void *arg;
	void (*isr)(enum sync_ch_id sync_ch, void *arg);
	struct list_head node;
};

struct sync_db_t
{
	enum sync_ch_id sync_ch;
	atomic_t usage_count;
	struct device *dev;
	struct odin_pm_domain *pm_domain;

	unsigned long frequency;
	char *generating_src;

	enum dss_irq irq;
	struct list_head isr_head;
	spinlock_t isr_head_lock;

	unsigned int frequency_measure_count;

} sync_db[SYNC_CH_NUM] = {
	{.sync_ch = SYNC_CH_ID0, .pm_domain = &odin_pd_dss3_sync0, .irq = DSS_IRQ_SYNC0, .generating_src = "disp0_clk",},
	{.sync_ch = SYNC_CH_ID1, .pm_domain = &odin_pd_dss3_sync1, .irq = DSS_IRQ_SYNC1, .generating_src = "disp1_clk",},
	{.sync_ch = SYNC_CH_ID1, .pm_domain = &odin_pd_dss3_sync2, .irq = DSS_IRQ_SYNC2, .generating_src = "hdmi_disp_clk",},
};

static void _sync_isr(enum dss_irq irq, void *arg)
{
	struct sync_db_t *sync = (struct sync_db_t *)arg;
	struct sync_isr_t *local_isr = NULL, *local_isr_tmp = NULL;
	struct sync_global_tick_isr *global_isr = NULL, *globa_isr_tmp = NULL;
	unsigned long flags;

	if (irq > DSS_IRQ_SYNC2) {
		pr_err("invalid dss_irq no(%d)\n", irq);
		return;
	}

	if (sync == NULL) {
		pr_err("sync db ptr is NULL\n");
		return;
	}

	sync->frequency_measure_count++;

	if (d->global_tick_ref == sync->sync_ch) {
		d->global_tick++;
		if (d->global_tick >= SYNC_INVALID_TICK)
			d->global_tick = 0;

		list_for_each_entry_safe(global_isr, globa_isr_tmp, &d->global_isr_head, node)
			global_isr->isr(d->global_tick_ref);
	}

	spin_lock_irqsave(&sync->isr_head_lock, flags);

	list_for_each_entry_safe(local_isr, local_isr_tmp, &sync->isr_head, node)
		local_isr->isr(sync->sync_ch, local_isr->arg);

	spin_unlock_irqrestore(&sync->isr_head_lock, flags);

	return;
}

static void _sync_global_isr_unregister_clear(void)
{
	struct sync_global_tick_isr *global_isr, *tmp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&d->global_isr_head_lock, flags);
	list_for_each_entry_safe(global_isr, tmp, &d->global_isr_head, node) {
		list_del(&global_isr->node);
		vfree(global_isr);
	}
	spin_unlock_irqrestore(&d->global_isr_head_lock, flags);
}

bool sync_global_isr_register(void (*isr)(enum sync_ch_id sync_ch))
{
	struct sync_global_tick_isr *global_isr = NULL;
	unsigned long flags;

	if (isr == NULL) {
		pr_err("isr ptr is NULL\n");
		return false;
	}

	global_isr = vzalloc(sizeof(struct sync_global_tick_isr));
	if (global_isr == NULL) {
		pr_err("vzaloc failed\n");
		return false;
	}

	global_isr->isr = isr;

	spin_lock_irqsave(&d->global_isr_head_lock, flags);

	list_add(&global_isr->node, &d->global_isr_head);

	spin_unlock_irqrestore(&d->global_isr_head_lock, flags);

	return true;
}

void sync_global_isr_unregister(void (*isr)(enum sync_ch_id sync_ch))
{
	struct sync_global_tick_isr *global_isr = NULL;
	unsigned long flags;

	if (isr == NULL) {
		pr_err("isr ptr is NULL\n");
		return;
	}

	spin_lock_irqsave(&d->global_isr_head_lock, flags);

	list_for_each_entry(global_isr, &d->global_isr_head, node) {
		if (global_isr->isr == isr) {
			list_del(&global_isr->node);
			vfree(global_isr);
			break;
		}
	}

	spin_unlock_irqrestore(&d->global_isr_head_lock, flags);
}

unsigned int sync_global_tick_get(void)
{
	if (d == NULL) {
		pr_warn("sync is not intialized\n");
		return SYNC_INVALID_TICK;
	}

	if (d->global_tick_ref == SYNC_CH_INVALID) {
		pr_warn("global_tick_ref is invalied\n");
		return SYNC_INVALID_TICK;
	}

	return d->global_tick;
}

bool sync_is_enable(enum sync_ch_id sync_ch)
{
	struct sync_db_t *sync = NULL;

	if (sync_ch >= SYNC_CH_NUM) {
		pr_err("invalid sync_ch(%d)\n", sync_ch);
		return false;
	}

	sync = &sync_db[sync_ch];

	return (atomic_read(&sync->usage_count) > 0) ? true : false;
}

bool sync_isr_register(enum sync_ch_id sync_ch, void *arg, void (*isr)(enum sync_ch_id sync_ch, void *arg))
{
	struct sync_db_t *sync = NULL;
	struct sync_isr_t *local_isr = NULL;
	unsigned long flags;

	if (sync_ch >= SYNC_CH_NUM) {
		pr_err("[sync] invalid sync id(%d)\n", sync_ch);
		return false;
	}

	if (isr == NULL) {
		pr_err("isr isr is NULL");
		return false;
	}

	sync = &sync_db[sync_ch];

	local_isr = vzalloc(sizeof(struct sync_isr_t));
	if (local_isr == NULL) {
		pr_err("vzalloc failed\n");
		return false;
	}

	local_isr->isr = isr;
	local_isr->arg = arg;

	spin_lock_irqsave(&sync->isr_head_lock, flags);

	list_add(&local_isr->node, &sync->isr_head);

	spin_unlock_irqrestore(&sync->isr_head_lock, flags);

	return true;
}

bool sync_isr_unregister(enum sync_ch_id sync_ch, void (*isr)(enum sync_ch_id sync_ch, void *arg))
{
	struct sync_db_t *sync = NULL;
	struct sync_isr_t *local_isr = NULL;
	bool ret = false;
	unsigned long flags;

	if (sync_ch >= SYNC_CH_NUM) {
		pr_err("invalid sync id(%d)\n", sync_ch);
		return false;
	}

	if (isr == NULL) {
		pr_err("isr isr is NULL");
		return false;
	}

	sync = &sync_db[sync_ch];

	spin_lock_irqsave(&sync->isr_head_lock, flags);

	list_for_each_entry(local_isr, &sync->isr_head, node) {
		if (local_isr->isr == isr) {
			list_del(&local_isr->node);
			vfree(local_isr);
			ret = true;
			break;
		}
	}

	spin_unlock_irqrestore(&sync->isr_head_lock, flags);

	return ret;
}

enum sync_ch_id sync_enable(enum sync_ch_id sync_ch, unsigned long frequency)
{
	struct sync_db_t *sync = NULL;
	enum sync_generater sync_src;

	if (sync_ch > SYNC_CH_ID2) {
		pr_err("invalid sync id(%d)\n", sync_ch);
		return SYNC_CH_INVALID;
	}

	sync = &sync_db[sync_ch];

	if (atomic_add_return(1, &sync->usage_count) > 1) {
		pr_info("sync is alreay enabled\n");
		return sync_ch;
	}

	sync->frequency = frequency;

	if (d->global_tick_ref == SYNC_CH_INVALID) {
		d->global_tick_ref = sync_ch;
		pr_info("global tick ref is %d\n", d->global_tick_ref);
	}

	pm_runtime_get_sync(sync->dev);

	if (frequency) {
		struct clk *disp_clk = NULL;
		unsigned long h_raster = 0;

		pr_info("using internal src gernerater\n");

		disp_clk = clk_get(NULL, sync->generating_src);
		if (disp_clk == NULL) {
			pr_warn("%s clk_get failed. h raster use default(%d)\n", sync->generating_src, DEFAULT_V_RASTER_SIZE);
			h_raster = DEFAULT_V_RASTER_SIZE;
		}
		else
			h_raster = DIVIDE_V_RASTER_SIZE(clk_get_rate(disp_clk) / frequency);

		sync_hal_raster_size_set(sync->sync_ch, DEFAULT_V_RASTER_SIZE, h_raster);
		sync_src = SYNC_SYNC_GEN;
	}
	else
	{
		sync_hal_raster_size_set(sync->sync_ch, DEFAULT_V_RASTER_SIZE, DEFAULT_H_RASTER_SIZE);
		sync_src = SYNC_DIP;
	}

	if (sync_hal_act_size_set(sync->sync_ch, DEFAULT_ACT_V, DEFAULT_ACT_H) == false) {
		pr_info("sync_hal_act_size_set failed hal_id:%d\n", sync->sync_ch);
		pm_runtime_put_sync(sync->dev);
		return SYNC_CH_INVALID;
	}

	dss_request_irq(sync->irq, _sync_isr, (void*)sync);

	if (sync_hal_enable(sync->sync_ch, SYNC_CONTINOUS, true, SYNC_VSYNC_SIGNAL, sync_src) == false) {
		pr_info("sync_hal_enable failed hal_id:%d\n", sync->sync_ch);
		dss_free_irq(sync_ch);
		pm_runtime_put_sync(sync->dev);
		return SYNC_CH_INVALID;
	}

	pr_info("sync_enabled ch:%d\n", sync_ch);

	return sync_ch;
}

static void _sync_disable(struct sync_db_t *sync)
{
	struct sync_isr_t *local_isr = NULL;
	unsigned long flags;

	if (d->global_tick_ref == sync->sync_ch) {
		int sync_i;

		for (sync_i = 0; sync_i < SYNC_CH_NUM; sync_i++) {
			if (sync_is_enable(sync_i) == true)
				break;
		}
		d->global_tick_ref = sync_i;

		if (d->global_tick_ref <= SYNC_CH_ID2)
			pr_info("change global tick reference(%d --> %d)\n", sync->sync_ch, sync_i);
		else {
			d->global_tick_ref = SYNC_CH_INVALID;
			pr_info("global tick is stoped\n");
		}
	}

	sync_hal_disable(sync->sync_ch);

	spin_lock_irqsave(&sync->isr_head_lock, flags);

	list_for_each_entry(local_isr, &sync->isr_head, node) {
		local_isr->isr(sync->sync_ch, local_isr->arg);
		list_del(&local_isr->node);
		vfree(local_isr);
	}

	spin_unlock_irqrestore(&sync->isr_head_lock, flags);

	pm_runtime_put_sync(sync->dev);

	atomic_set(&sync->usage_count, 0);
}

bool sync_disable(enum sync_ch_id sync_ch)
{
	int usage_count = 0xffffffff;
	struct sync_db_t *sync = NULL;

	if (sync_ch >= SYNC_CH_NUM) {
		pr_err("invalid sync id(%d)\n", sync_ch);
		return false;
	}

	sync = &sync_db[sync_ch];

	usage_count = atomic_dec_return(&sync->usage_count);
	if (usage_count > 0) {
		pr_info("Other module is still using this sync(id:%d). usage_count:%d\n", sync_ch, usage_count);
		return true;
	}

	if(usage_count < 0) {
		pr_warn("usage_count isn`t match. Set force to 0\n");
		atomic_set(&sync->usage_count, 0);
		return true;
	}

	_sync_disable(sync);

	return true;
}

static int _sync_runtime_resume(struct device *dev)
{
	struct clk *clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("sync clk get error\n");
		return -1;
	}

	//pr_info("%s %d %d\n", __func__, clk->enable_count, clk->prepare_count);

	clk_prepare_enable(clk);

	return 0;
}

static int _sync_runtime_suspend(struct device *dev)
{
	struct clk *clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("sync clk get error\n");
		return -1;
	}

	//pr_info("%s %d %d\n", __func__, clk->enable_count, clk->prepare_count);
	
	if (clk->enable_count == 0)
		pr_err("already clk disabled.\n");
	else
		clk_disable_unprepare(clk);

	return 0;
}

static const struct dev_pm_ops _sync_pm_ops = {
	.runtime_resume = _sync_runtime_resume,
	.runtime_suspend = _sync_runtime_suspend,
};

static struct of_device_id sync0_src_match[] =
{
	{ .name = "sync0", .compatible = "odin,dss,sync0"},
	{},
};

static struct platform_driver sync0_drv =
{
	.driver = {
		.name = "sync0",
		.owner = THIS_MODULE,
		.pm = &_sync_pm_ops,
		.of_match_table = of_match_ptr(sync0_src_match),
	},
};

static struct of_device_id sync1_src_match[] =
{
	{.name = "sync1", .compatible = "odin,dss,sync1"},
	{},
};
static struct platform_driver sync1_drv =
{
	.driver = {
		.name = "sync1",
		.owner = THIS_MODULE,
		.pm = &_sync_pm_ops,
		.of_match_table = of_match_ptr(sync1_src_match),
	},
};

static struct of_device_id sync2_src_match[] =
{
	{.name = "sync2", .compatible = "odin,dss,sync2"},
	{},
};

static struct platform_driver sync2_drv =
{
	.driver = {
		.name = "sync2",
		.owner = THIS_MODULE,
		.pm = &_sync_pm_ops,
		.of_match_table = of_match_ptr(sync2_src_match),
	},
};

static void _sync_pm_init(struct sync_db_t *sync)
{
	if (odin_pd_register_dev(sync->dev, sync->pm_domain) < 0)
		pr_err("%s odin_pd_register_dev failed\n", sync->dev->of_node->name);
	else
		pm_runtime_enable(sync->dev);
}

static void _sync_frequency_measure(unsigned long data)
{
	unsigned int sync_i;
	for (sync_i=SYNC_CH_ID0; sync_i<SYNC_CH_NUM; sync_i++) {
		if (sync_db[sync_i].frequency_measure_count > 65)
			pr_info("sync(%d) is too fast. count:%d\n", sync_i, sync_db[sync_i].frequency_measure_count);
		else if (sync_db[sync_i].frequency_measure_count != 0 && sync_db[sync_i].frequency_measure_count < 55)
			pr_info("sync(%d) is too slow. count:%d\n", sync_i, sync_db[sync_i].frequency_measure_count);

		sync_db[sync_i].frequency_measure_count = 0;
	}

	d->frequency_measure_timer.expires = jiffies + msecs_to_jiffies(1000);
	add_timer(&d->frequency_measure_timer); 
}

static void _sync_frequency_measure_init(void)
{
	init_timer(&d->frequency_measure_timer);
	d->frequency_measure_timer.data = 0;
	d->frequency_measure_timer.function = _sync_frequency_measure;
	d->frequency_measure_timer.expires = jiffies + msecs_to_jiffies(1000);
	add_timer(&d->frequency_measure_timer); 
}

static void _sync_frequency_measure_cleanup(void)
{
	del_timer_sync(&d->frequency_measure_timer);
}

bool sync_init(struct platform_device *pdev[])
{
	unsigned int sync_i;
	int ret = false;

	if (d) {
		pr_warn("already initialized\n");
		return true;
	}

	d = vzalloc(sizeof(struct sync_desc_t));
	if (d == NULL) {
		pr_err("vzalloc failed(size:%d)\n", sizeof(struct sync_desc_t));
		return false;
	}

	d->global_tick_ref = SYNC_CH_INVALID;
	d->global_tick = SYNC_INVALID_TICK;

	INIT_LIST_HEAD(&d->global_isr_head);
	spin_lock_init(&d->global_isr_head_lock);

	for (sync_i=SYNC_CH_ID0; sync_i<SYNC_CH_NUM; sync_i++) {
		INIT_LIST_HEAD(&sync_db[sync_i].isr_head);
		spin_lock_init(&sync_db[sync_i].isr_head_lock);

		sync_db[sync_i].frequency_measure_count = 0;
		atomic_set(&sync_db[sync_i].usage_count, 0);

		sync_db[sync_i].dev = &pdev[sync_i]->dev;
		_sync_pm_init(&sync_db[sync_i]);
	}

	ret = platform_driver_register(&sync0_drv);
	if ( ret < 0 ) {
		pr_err("sync0 platform_driver_register failed\n");
		vfree(d);
		d = NULL;
	}
	ret = platform_driver_register(&sync1_drv);
	if ( ret < 0 ) {
		pr_err("sync1 platform_driver_register failed\n");
		platform_driver_unregister(&sync0_drv);
		vfree(d);
		d = NULL;
	}
	ret = platform_driver_register(&sync2_drv);
	if ( ret < 0 ) {
		pr_err("sync2 platform_driver_register failed\n");
		platform_driver_unregister(&sync0_drv);
		platform_driver_unregister(&sync1_drv);
		vfree(d);
		d = NULL;
	}

	_sync_frequency_measure_init();

	return ret;
}

void sync_cleanup(void)
{
	enum sync_ch_id i;
	if (d == NULL)
		return;

	_sync_frequency_measure_cleanup();

	for (i=0; i<SYNC_CH_NUM; i++)
		_sync_disable(&sync_db[i]);

	_sync_global_isr_unregister_clear();

	platform_driver_unregister(&sync0_drv);
	platform_driver_unregister(&sync1_drv);
	platform_driver_unregister(&sync2_drv);

	vfree(d);
}
