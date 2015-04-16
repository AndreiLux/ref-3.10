/*
 * hisi_hi6421_powerkey.c - Hisilicon Hi6421 PMIC powerkey driver
 *
 * Copyright (C) 2013 Hisilicon Ltd.
 * Copyright (C) 2013 Linaro Ltd.
 * Author: Haojian Zhuang <haojian.zhuang@linaro.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/mfd/hisi_hi6421v300.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>

struct hi6421_powerkey_info {
	struct input_dev	*idev;
	int			irq[5];
	struct      wake_lock  pwr_wake_lock;
};

static irqreturn_t hi6421_powerkey_handler(int irq, void *data)
{
	struct hi6421_powerkey_info *info = (struct hi6421_powerkey_info *)data;

	wake_lock_timeout(&info->pwr_wake_lock, HZ);

	/* only handle power down & power up event at here */
	if (irq == info->irq[0]) {
		pr_info("[%s]: press\n", __func__);
		input_report_key(info->idev, KEY_POWER, 1);
		input_sync(info->idev);
	} else if (irq == info->irq[1]) {
		pr_info("[%s]: release\n", __func__);
		input_report_key(info->idev, KEY_POWER, 0);
		input_sync(info->idev);
	} else if (irq == info->irq[2]) {
		pr_info("1s\n");
	} else if (irq == info->irq[3]) {
		pr_info("8s\n");
	} else if (irq == info->irq[4]) {
		pr_info("10s\n");
	} else {
		pr_err("powerkey input irq handler error!");
	}
	return IRQ_HANDLED;
}

static int hi6421_powerkey_probe(struct platform_device *pdev)
{
	struct hi6421_powerkey_info *info;
	struct device *dev = &pdev->dev;
	int ret = 0;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input device\n");
		ret = -ENOENT;
		return ret;
	}

	info->idev->name = "hi6421_on";
	info->idev->phys = "hi6421_on/input0";
	info->idev->dev.parent = &pdev->dev;
	info->idev->evbit[0] = BIT_MASK(EV_KEY);
	__set_bit(KEY_POWER, info->idev->keybit);

	wake_lock_init(&info->pwr_wake_lock, WAKE_LOCK_SUSPEND, "android-pwr");

	ret = input_register_device(info->idev);
	if (ret) {
		dev_err(&pdev->dev, "Can't register input device: %d\n", ret);
		ret = -ENOENT;
		goto input_err;
	}

	info->irq[0] = platform_get_irq_byname(pdev, "down");
	if (info->irq[0] < 0) {
		dev_err(dev, "failed to get down irq id\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	ret = devm_request_irq(dev, info->irq[0], hi6421_powerkey_handler,
			       IRQF_NO_SUSPEND, "down", info);
	if (ret < 0) {
		dev_err(dev, "failed to request down irq\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	info->irq[1] = platform_get_irq_byname(pdev, "up");
	if (info->irq[1] < 0) {
		dev_err(dev, "failed to get up irq id\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	ret = devm_request_irq(dev, info->irq[1], hi6421_powerkey_handler,
			       IRQF_NO_SUSPEND, "up", info);
	if (ret < 0) {
		dev_err(dev, "failed to request up irq\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	info->irq[2] = platform_get_irq_byname(pdev, "hold 1s");
	if (info->irq[2] < 0) {
		dev_err(dev, "failed to get hold 1s irq id\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	ret = devm_request_irq(dev, info->irq[2], hi6421_powerkey_handler,
			       IRQF_DISABLED, "hold 1s", info);
	if (ret < 0) {
		dev_err(dev, "failed to request hold 1s irq\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	info->irq[3] = platform_get_irq_byname(pdev, "hold 8s");
	if (info->irq[3] < 0) {
		dev_err(dev, "failed to request hold 8s irq id\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	ret = devm_request_irq(dev, info->irq[3], hi6421_powerkey_handler,
			       IRQF_DISABLED, "hold 8s", info);
	if (ret < 0) {
		dev_err(dev, "failed to request hold 8s irq\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	info->irq[4] = platform_get_irq_byname(pdev, "hold 10s");
	if (info->irq[4] < 0) {
		dev_err(dev, "failed to request hold 10s irq id\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	ret = devm_request_irq(dev, info->irq[4], hi6421_powerkey_handler,
			       IRQF_DISABLED, "hold 10s", info);
	if (ret < 0) {
		dev_err(dev, "failed to request hold 10s irq\n");
		ret = -ENOENT;
		goto unregister_err;
	}

	platform_set_drvdata(pdev, info);
	return ret;
unregister_err:
	input_unregister_device(info->idev);
input_err:
    wake_lock_destroy(&info->pwr_wake_lock);
	input_free_device(info->idev);

	return ret;
}

static int hi6421_powerkey_remove(struct platform_device *pdev)
{
	struct hi6421_powerkey_info *info = platform_get_drvdata(pdev);
	wake_lock_destroy(&info->pwr_wake_lock);
	input_free_device(info->idev);
	input_unregister_device(info->idev);
	return 0;
}

static struct of_device_id hi6421_powerkey_of_match[] = {
	{ .compatible = "hisilicon,hi6421-powerkey", },
	{ },
};
MODULE_DEVICE_TABLE(of, hi6421_powerkey_of_match);

static struct platform_driver hi6421_powerkey_driver = {
	.probe		= hi6421_powerkey_probe,
	.remove		= hi6421_powerkey_remove,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "hi6421-powerkey",
		.of_match_table	= hi6421_powerkey_of_match,
	},
};
module_platform_driver(hi6421_powerkey_driver);

MODULE_AUTHOR("Haojian Zhuang <haojian.zhuang@linaro.org");
MODULE_DESCRIPTION("Hi6421 PMIC Power key driver");
MODULE_LICENSE("GPL v2");
