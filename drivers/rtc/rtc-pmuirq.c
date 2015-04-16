/*
 * Hisilicon Hi6421 RTC interrupt driver
 *
 * Copyright (C) 2014 Hisilicon Ltd.
 * Copyright (C) 2014 huawei Ltd.
 *
 * Author: chenmudan <chenmudan@huawei.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/rtc.h>
#include <linux/mfd/hisi_hi6421v300.h>
/*add for reboot in recovery charging mode*/
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rtc-pmurtc.h>

struct hi6421_rtc_info {
	struct rtc_device	*rtc;
	struct hi6421_pmic	*pmic;
	int			irq;
};

/* add a workqueue for reboot */
static int oem_rtc_reboot_thread(void *u)
{
	printk(KERN_INFO "Entering Rebooting Causeed by Alarm...\n");
	emergency_remount();
	sys_sync();
	msleep(500);
	kernel_restart("oem_rtc");

	/* should not be here */
	panic("oem_rtc");
	return 0;
}

void oem_rtc_reboot(unsigned long t)
{
	kernel_thread(oem_rtc_reboot_thread, NULL, CLONE_FS | CLONE_FILES);
}
static DECLARE_TASKLET(oem_rtc_reboot_tasklet, oem_rtc_reboot, 0);

static irqreturn_t hi6421_rtc_handler(int irq, void *data)
{
	unsigned char alarm_status = 0;
	struct hi6421_rtc_info *info = (struct hi6421_rtc_info *)data;

	/*needn't clear interrupt irq because pmic has done*/
	alarm_status = hi6421_pmic_read(info->pmic, HI6421V300_ALARM_ON);
	alarm_status &= 0x01;
	printk(KERN_INFO "hi6421_rtc_handler alarm_status=%d\n", alarm_status);

	if (0 == alarm_status) {
		return IRQ_HANDLED;
	}

	if(unlikely(get_pd_charge_flag())) {
		tasklet_schedule(&oem_rtc_reboot_tasklet);
	}
	return IRQ_HANDLED;
}

static int hi6421_rtc_probe(struct platform_device *pdev)
{
	struct hi6421_rtc_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0)
		return -ENOENT;

	info->pmic = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, info);

	info->irq = platform_get_irq_byname(pdev,"alarmirq");
	if(info->irq < 0)
	{
		dev_err(&pdev->dev, "failed to get alarmirq\n");
		return -ENOENT;
	}
	ret = devm_request_irq(&pdev->dev, info->irq, hi6421_rtc_handler,
			       IRQF_DISABLED, "alarmirq", info);
	if (ret < 0)
		return ret;

	return 0;
}

static int hi6421_rtc_remove(struct platform_device *pdev)
{
	struct hi6421_rtc_info *info = dev_get_drvdata(&pdev->dev);

	devm_free_irq(&pdev->dev, info->irq, info);
	devm_kfree(&pdev->dev, info);

	return 0;
}

static struct of_device_id hi6421_rtc_of_match[] = {
	{ .compatible = "hisilicon,hi6421-rtc" },
	{ }
};
MODULE_DEVICE_TABLE(of, hi6421_rtc_of_match);

static struct platform_driver hi6421_rtc_driver = {
	.driver = {
		.name = "hi6421-rtc",
		.of_match_table = of_match_ptr(hi6421_rtc_of_match),
	},
	.probe = hi6421_rtc_probe,
	.remove = hi6421_rtc_remove,
};
module_platform_driver(hi6421_rtc_driver);

MODULE_LICENSE("GPLv2");
MODULE_AUTHOR("chenmudan <chenmudan@huawei.com");
MODULE_ALIAS("platform:hi6421-rtc");
