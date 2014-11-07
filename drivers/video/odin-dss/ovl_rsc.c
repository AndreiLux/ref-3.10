/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Jungmin Park <jungmin016.park@lge.com>
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
#include <linux/module.h>
#include <linux/odin_mailbox.h> 
#include <linux/odin_pm_domain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include "hal/ovl_hal.h"
#include "hal/sync_hal.h"

static struct ovl_rsc
{
	struct odin_pm_domain *pm_domain;
	struct device *dev;

	struct dss_image_size ovl_size;
	unsigned char pattern_r;
	unsigned char pattern_g;
	unsigned char pattern_b;
} ovl_rsc[OVL_CH_NUM] = {
	{.pm_domain = &odin_pd_dss3_ovl0, .dev = NULL},
	{.pm_domain = &odin_pd_dss3_ovl1, .dev = NULL},
	{.pm_domain = &odin_pd_dss3_ovl2, .dev = NULL},
};

static int _ovl_runtime_resume(struct device *dev)
{
	struct ovl_rsc *ovl = NULL;
	enum ovl_ch_id ovl_ch = (enum ovl_ch_id)dev->platform_data;

	struct clk *clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("ovl clk get error\n");
		return -1;
	}

	clk_enable(clk);

	ovl = &ovl_rsc[ovl_ch];
	ovl_hal_enable(ovl_ch, ovl->ovl_size, false, false, OVL_RASTER_OPS_0, 
			ovl->pattern_r, ovl->pattern_g, ovl->pattern_b);

	sync_hal_ovl_st_dly(ovl_ch, 1, 1);
	sync_hal_ovl_enable(ovl_ch);

	return 0;
}

static int _ovl_runtime_suspend(struct device *dev)
{
	enum ovl_ch_id ovl_ch = (enum ovl_ch_id)dev->platform_data;

	struct clk *clk = clk_get(dev, NULL);

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("ovl clk get error\n");
		return -1;
	}


	ovl_hal_disable(ovl_ch);

	clk_disable(clk);

	sync_hal_ovl_disable(ovl_ch);
	return 0;
}

static const struct dev_pm_ops _ovl_pm_ops = 
{
	.runtime_resume = _ovl_runtime_resume,
	.runtime_suspend = _ovl_runtime_suspend,
};

static struct of_device_id _ovl0_match[] = 
{
	{.name = "ovl0", .compatible = "odin,dss,ovl0"},
	{},
};

static struct of_device_id _ovl1_match[] = 
{
	{.name = "ovl1", .compatible = "odin,dss,ovl1"},
	{},
};

static struct of_device_id _ovl2_match[] = 
{
	{.name = "ovl2", .compatible = "odin,dss,ovl2"},
	{},
};

static struct platform_driver _ovl0_drv = 
{
	.driver = {
		.name = "ovl0",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _ovl0_match ),
		.pm = &_ovl_pm_ops,
	},
};

static struct platform_driver _ovl1_drv = 
{
	.driver = {
		.name = "ovl1",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _ovl1_match ),
		.pm = &_ovl_pm_ops,
	},
};

static struct platform_driver _ovl2_drv = 
{
	.driver = {
		.name = "ovl2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _ovl2_match ),
		.pm = &_ovl_pm_ops,
	},
};

void ovl_rsc_set(enum ovl_ch_id ovl_ch, struct dss_image_size *ovl_size, unsigned char pattern_r, unsigned char pattern_g, unsigned char pattern_b)
{
	struct ovl_rsc *ovl = NULL;

	if (ovl_ch >= OVL_CH_NUM) {
		pr_err("invalid id %d\n", ovl_ch);
		return;
	}

	ovl = &ovl_rsc[ovl_ch];

	ovl->pattern_r = pattern_r;
	ovl->pattern_g = pattern_g;
	ovl->pattern_b = pattern_b;
	if (ovl_size != NULL)
		ovl->ovl_size = *ovl_size;
}

bool ovl_rsc_enable(enum ovl_ch_id ovl_ch)
{
	if (ovl_ch >= OVL_CH_NUM) {
		pr_err("invalid id %d\n", ovl_ch);
		return false;
	}

	pm_runtime_get_sync(ovl_rsc[ovl_ch].dev);

	return true;
}

bool ovl_rsc_disable(enum ovl_ch_id ovl_ch)
{
	if (ovl_ch >= OVL_CH_NUM) {
		pr_err("invalid id %d\n", ovl_ch);
		return false;
	}
	pm_runtime_put_sync(ovl_rsc[ovl_ch].dev);

	return true;
}

void ovl_rsc_init(struct platform_device *pdev_ovl[])
{
	enum ovl_ch_id ovl_i = 0;
	struct device *dev = NULL;
	struct clk *clk = NULL;
	int ret = -1;

	ret = platform_driver_register(&_ovl0_drv);
	if (ret < 0) {
		pr_err("ovl0 platform_driver_register failed\n");
		goto err_ovl0_platform_driver_register;
	}

	ret = platform_driver_register(&_ovl1_drv);
	if (ret < 0) {
		pr_err("ovl1 platform_driver_register failed\n");
		goto err_ovl1_platform_driver_register;
	}

	ret = platform_driver_register(&_ovl2_drv);
	if (ret < 0) {
		pr_err("ovl2 platform_driver_register failed\n");
		goto err_ovl2_platform_driver_register;
	}

	for (ovl_i = OVL_CH_ID0; ovl_i < OVL_CH_NUM; ovl_i++) {
		dev = &pdev_ovl[ovl_i]->dev;

		if (odin_pd_register_dev(dev, ovl_rsc[ovl_i].pm_domain) < 0) 
			pr_err("%s pd_registe fail\n", dev->of_node->name);

		clk = clk_get(dev, NULL);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("%s get clk fail", dev->of_node->name);
			goto err_clk_get;
		}
		clk_prepare(clk);

		dev->platform_data = (void*)ovl_i;
		ovl_rsc[ovl_i].dev = dev;

		pm_runtime_enable(dev);
	}
	return;

err_clk_get:
		platform_driver_unregister(&_ovl2_drv);
err_ovl2_platform_driver_register:
		platform_driver_unregister(&_ovl1_drv);
err_ovl1_platform_driver_register:
		platform_driver_unregister(&_ovl0_drv);
err_ovl0_platform_driver_register:
	return;
}

void ovl_rsc_cleanup(void)
{
	enum ovl_ch_id ovl_i = 0;
	struct clk *clk = NULL;

	platform_driver_unregister(&_ovl0_drv);
	platform_driver_unregister(&_ovl1_drv);
	platform_driver_unregister(&_ovl2_drv);

	for (ovl_i = OVL_CH_ID0; ovl_i < OVL_CH_NUM; ovl_i++) {
		clk = clk_get(ovl_rsc[ovl_i].dev, NULL);

		if (clk->prepare_count == 1)
			clk_unprepare(clk); 
	}
}
