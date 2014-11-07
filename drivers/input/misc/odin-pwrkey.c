/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/input.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/odin_mailbox.h>
#include <linux/module.h>
#include <linux/input/odin_key.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/wakelock.h>
#include <linux/platform_data/odin_tz.h>

#include <asm/io.h>

static struct input_dev *odin_pwrkey;
static struct wakeup_source power_key_lock;

static u32 wakeup_data = 0;
static DECLARE_COMPLETION(onkey_received);

#ifdef CONFIG_ODIN_TEE
static u32 check_power_key;
#else
static void __iomem *check_power_key;
#endif

static int odin_power_key_send(void)
{
    input_report_key(odin_pwrkey, KEY_POWER, 1);
    input_report_key(odin_pwrkey, KEY_POWER, 0);
    input_sync(odin_pwrkey);
	return 0;
}

static int odin_power_key_sleep(void)
{
    input_report_key(odin_pwrkey, KEY_SLEEP, 1);
    input_report_key(odin_pwrkey, KEY_SLEEP, 0);
    input_sync(odin_pwrkey);
	return 0;
}

static int odin_power_key_poweroff(void)
{
    input_report_key(odin_pwrkey, KEY_POWER, 0);
    input_report_key(odin_pwrkey, KEY_POWER, 1);
    input_sync(odin_pwrkey);
	return 0;
}

static irqreturn_t odin_onkey_irq_handler(int irq, void *data)
{
	unsigned int *mail = (unsigned int *)data;

	if (mail[2] == 0x0)
		odin_power_key_sleep();
	else
		wakeup_data = mail[2];

	complete(&onkey_received);

	return IRQ_HANDLED;
}

static int odin_pwrkey_suspend(void)
{
	return 0;
}

static int odin_pwrkey_resume(void)
{
	u8 keyint;

#ifdef CONFIG_ODIN_TEE
	keyint = tz_read(check_power_key + 0x0);
#else
	keyint = readw(check_power_key + 0x0);
#endif
	if (keyint == 0x1) {
		odin_power_key_send();
#ifdef CONFIG_ODIN_TEE
		tz_write(0, check_power_key);
#else
		writew(0, check_power_key);
#endif
		__pm_wakeup_event(&power_key_lock, 500);
	}
	else if (keyint == 0x2) {
#ifdef CONFIG_ODIN_TEE
		tz_write(0, check_power_key);
#else
		/* LPA resume */
		writew(0, check_power_key);
#endif
	}

	return 0;
}

static const struct dev_pm_ops odin_powerkey_ops = {
	.suspend = odin_pwrkey_suspend,
	.resume = odin_pwrkey_resume,
};

static int odin_pwrkeys_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *np_pwrkey;
	struct resource rs;

	np_pwrkey = of_find_compatible_node(NULL, NULL, "LG,odin-pwrkey");

#ifdef CONFIG_ODIN_TEE
	check_power_key = of_address_to_resource(np_pwrkey, 0, &rs);
	check_power_key = rs.start;
#else
	check_power_key = of_iomap(np_pwrkey, 0);
	if (check_power_key == NULL)
		return -ENOMEM;
#endif

	odin_pwrkey = input_allocate_device();
    if (!odin_pwrkey) {
       pr_err("%s: Failed to allocate input device for power key\n",
                __func__);
        return -ENOMEM;
    }

    odin_pwrkey->evbit[0] = BIT_MASK(EV_KEY);
    odin_pwrkey->name = "odin-pwrkey";
    set_bit(KEY_POWER, odin_pwrkey->keybit);
    set_bit(KEY_SLEEP, odin_pwrkey->keybit);

	ret = mailbox_request_irq(MB_ONKEY_HANDLER,
			odin_onkey_irq_handler, "MB_ONKEY_HANDLER");

    ret = input_register_device(odin_pwrkey);
    if (ret) {
        pr_err("%s: Failed to register input device for power key\n",
                __func__);
    }
	wakeup_source_init(&power_key_lock, "onkey_power");

	return 0;
};

static int odin_pwrkeys_remove(struct platform_device *pdev)
{
	input_unregister_device(odin_pwrkey);
	return 0;
}

static struct of_device_id odin_pwrkey_of_match[] = {
	{ .compatible = "LG,odin-pwrkey", },
	{ },
};
MODULE_DEVICE_TABLE(of, odin_pwrkey_of_match);


static struct platform_driver odin_pwrkey_device_driver = {
	.probe		= odin_pwrkeys_probe,
	.remove		= odin_pwrkeys_remove,
	.driver		= {
		.name	= "odin-pwrkey",
		.owner	= THIS_MODULE,
		.pm	= &odin_powerkey_ops,
		.of_match_table = of_match_ptr(odin_pwrkey_of_match),
	}
};

static int __init odin_powerkey_init(void)
{
	return platform_driver_register(&odin_pwrkey_device_driver);
}

static void __exit odin_powerkey_exit(void)
{
	platform_driver_unregister(&odin_pwrkey_device_driver);
}

module_init(odin_powerkey_init);
module_exit(odin_powerkey_exit);

MODULE_DESCRIPTION("ODIN Power key driver");
MODULE_AUTHOR("In Hyuk Kang <hugh.kang@lge.com>");
MODULE_LICENSE("GPL");
