/*
 * driver/power/lge_smpl_count.c
 *
 * Copyright (C) 2013 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/qpnp/power-on.h>
#include <soc/qcom/smem.h>

#define MODULE_NAME "lge_smpl_count"
#define PWR_ON_EVENT_KEYPAD           0x80
#define PWR_ON_EVENT_CABLE            0x40
#define PWR_ON_EVENT_PON1             0x20
#define PWR_ON_EVENT_USB              0x10
#define PWR_ON_EVENT_DC               0x08
#define PWR_ON_EVENT_RTC              0x04
#define PWR_ON_EVENT_SMPL             0x02
#define PWR_ON_EVENT_HARD_RESET       0x01

static uint smpl_count(void)
{
	uint *boot_cause = 0;
	int warm_reset = 0;
	uint smpl_check;

	boot_cause = (uint *)smem_alloc(SMEM_POWER_ON_STATUS_INFO,
				sizeof(boot_cause),
				0,
				SMEM_ANY_HOST_FLAG);
	warm_reset = qpnp_pon_is_warm_reset();

	if (boot_cause == NULL) {
		pr_err("%s : smem_alloc returns NULL\n", __func__);
		return 0;
	} else {
		pr_info("[BOOT_CAUSE] %d, warm_reset = %d\n", *boot_cause,
			warm_reset);
		smpl_check = *boot_cause & PWR_ON_EVENT_SMPL;
		if ((smpl_check != 0) && (warm_reset == 0)) {
			pr_info("[SMPL_CNT] ===> is smpl boot\n");
			return 1;
		} else {
			pr_info("[SMPL_CNT] ===> not smpl boot!!!!!\n");
			return 0;
		}
	}
}

static int smpl_boot;
static int smpl_count_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_info("SMPL counter is set to %d\n", smpl_boot);

	return 0;
}
module_param_call(smpl_boot, smpl_count_set,
	param_get_uint, &smpl_boot, 0644);

static int lge_smpl_count_probe(struct platform_device *pdev)
{
	int ret = 0;

	smpl_boot = smpl_count();
	return ret;
}

static int lge_smpl_count_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver lge_smpl_count_driver = {
	.probe = lge_smpl_count_probe,
	.remove = lge_smpl_count_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device lge_smpl_count_device = {
	.name = MODULE_NAME,
	.dev = {
		.platform_data = NULL,
	}
};

static int __init lge_smpl_count_init(void)
{
	platform_device_register(&lge_smpl_count_device);

	return platform_driver_register(&lge_smpl_count_driver);
}

static void __exit lge_smpl_count_exit(void)
{
	platform_driver_unregister(&lge_smpl_count_driver);
}

late_initcall(lge_smpl_count_init);
module_exit(lge_smpl_count_exit);

MODULE_DESCRIPTION("LGE smpl_count");
MODULE_LICENSE("GPL");

