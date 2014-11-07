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

/*                            
                                                   
                                                          
                                                           
                                                                    
                                      
                                            
                                                                    
                                                  
                            
  
                                                       
                                                  
                   
                          
                                  
  
                            
      
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/rfkill.h>

#define BT_HOST_WAKEUP	1

#if BT_HOST_WAKEUP
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#define BT_WAKELOCK_RFKILL	1
#endif	/* BT_HOST_WAKEUP */

#include <linux/platform_data/gpio-odin.h>
#include <linux/regulator/consumer.h>

/* To check the system revision info @ Odin */
#include <asm/system_info.h>

#include <linux/board_lge.h>

#if BT_HOST_WAKEUP
static struct wakeup_source bt_hostwake_lock;
//BT_S : H5_Sniff_PowerConsumption_workaround
static struct wakeup_source bt_btwake_lock;
//BT_E : H5_Sniff_PowerConsumption_workaround
static int hostwake_irq = 0;
#endif /* BT_HOST_WAKEUP */

#define ODIN_BT_RFKILL_DBG	0

#define ODIN_BT_FIRSTBOOT	0
#define ODIN_BT_OFF			1
#define ODIN_BT_ON			2
#define ODIN_BT_DONTCARE	3

#define ODIN_REV_A			0x0a
//#define ODIN_REV_G			0x10

static struct regulator *bt_wifi_io = NULL;

struct odin_bt_platform_data {
    unsigned int    reset_gpio;
    unsigned int    wakeup_gpio;
    unsigned int    hostwake_gpio;
};

struct odin_bt_drvdata {
	struct odin_bt_platform_data *pdata;
	struct rfkill	*bt_rst_rfkill_dev;
	struct rfkill	*bt_wake_rfkill_dev;
	struct rfkill	*bt_oob_rfkill_dev;
	unsigned int reset_gpio;
	unsigned int wakeup_gpio;
	unsigned int hostwake_gpio;
};

static inline void odin_bt_wakeup(struct odin_bt_drvdata *drvdata, bool wake)
{
	gpio_set_value(drvdata->wakeup_gpio, wake);
}

static uint8_t odin_bt_state = ODIN_BT_OFF;
static uint8_t odin_bt_wake_rfkill_flag = ODIN_BT_FIRSTBOOT;

static int odin_bt_rst_set_power(void *data, bool blocked)
{
	struct odin_bt_drvdata *drvdata = data;

#if ODIN_BT_RFKILL_DBG
	switch (odin_bt_state) {
		case ODIN_BT_FIRSTBOOT:
			printk(KERN_ERR "%s:odin_bt_state ODIN_BT_FIRSTBOOT\n", __func__);
			break;
		case ODIN_BT_OFF:
			printk(KERN_ERR "%s:odin_bt_state ODIN_BT_OFF\n", __func__);
			break;
		case ODIN_BT_ON:
			printk(KERN_ERR "%s:odin_bt_state ODIN_BT_ON\n", __func__);
			break;
		default:
			printk(KERN_ERR "%s:odin_bt_state UNKNOWN\n", __func__);
			break;
	}
#endif

	/*                                                                       
                                                                         
                                                                           
                                                   
  */
	switch (odin_bt_state) {
		case ODIN_BT_FIRSTBOOT:
			blocked = !blocked;
			if (!blocked) {
#if ODIN_BT_RFKILL_DBG
				printk(KERN_ERR "%s:RST_GPIO to 1 --> ODIN_BT_ON\n", __func__);
#endif
				odin_bt_state = ODIN_BT_ON;
			}
			else {
#if ODIN_BT_RFKILL_DBG
				printk(KERN_ERR "%s:RST_GPIO to 0 --> ODIN_BT_OFF\n", __func__);
#endif
				odin_bt_state = ODIN_BT_OFF;
			}
			break;
		case ODIN_BT_OFF:
			//if ((ODIN_REV_G <= (system_rev & 0xff)) || (ODIN_REV_A == (system_rev & 0xff))) {
				if (PTR_ERR(bt_wifi_io) == -ENODEV)
					break;
				if (!blocked) {
					if (regulator_enable(bt_wifi_io))
						printk(KERN_ERR "%s:cannot enable regulator, maybe it does not have it\n", __func__);
					else {
#if ODIN_BT_RFKILL_DBG
						printk(KERN_ERR "%s:[BT_WIFI_IO_REG] 1\n", __func__);
#endif
						udelay(30);	/* 2 sleep cycles (32.678kHz) required by BCM4339 datasheet */
					}

#if ODIN_BT_RFKILL_DBG
					printk(KERN_ERR "%s:RST_GPIO to 1 --> ODIN_BT_ON\n", __func__);
#endif
					odin_bt_state = ODIN_BT_ON;
				}
#if ODIN_BT_RFKILL_DBG
				else
					printk(KERN_ERR "%s:RST_GPIO to 0 --> ODIN_BT_OFF\n", __func__);
#endif
			//}
			if (odin_bt_state == ODIN_BT_ON)
				enable_irq_wake(hostwake_irq);
			break;
		case ODIN_BT_ON:
			//if ((ODIN_REV_G <= (system_rev & 0xff)) || (ODIN_REV_A == (system_rev & 0xff))) {
				if (PTR_ERR(bt_wifi_io) == -ENODEV)
					break;
				if (blocked) {
					if (regulator_disable(bt_wifi_io))
						printk(KERN_ERR "%s:cannot disable regulator, maybe it does not have it\n", __func__);
#if ODIN_BT_RFKILL_DBG
					else
						printk(KERN_ERR "%s:[BT_WIFI_IO_REG] 0\n", __func__);

					printk(KERN_ERR "%s:RST_GPIO to 0 --> ODIN_BT_OFF\n", __func__);
#endif
					odin_bt_state = ODIN_BT_OFF;
				}
				else
					printk(KERN_ERR "%s:RST_GPIO to 1 --> ODIN_BT_ON\n", __func__);
			//}
			if (odin_bt_state == ODIN_BT_OFF)
				disable_irq_wake(hostwake_irq);
			break;
		default:
			printk(KERN_ERR "%s:we are at an unknown bluetooth state\n", __func__);
			break;
	}

	gpio_set_value(drvdata->reset_gpio, !blocked);
#if BT_HOST_WAKEUP
	return 0;
#else
	gpio_set_value(drvdata->wakeup_gpio, !blocked);
#endif
	return 0;
}

static int odin_bt_wake_set_power(void *data, bool blocked)
{
	struct odin_bt_drvdata *drvdata = data;
	if (!odin_bt_wake_rfkill_flag) {
		blocked = !blocked;
		odin_bt_wake_rfkill_flag = ODIN_BT_DONTCARE;
	}
//BT_S : H5_Sniff_PowerConsumption_workaround
#if ODIN_BT_RFKILL_DBG
	printk(KERN_ERR "%s:odin_bt_wake_set_power blocked=%d\n", __func__,blocked);
#endif
//BT_E : H5_Sniff_PowerConsumption_workaround
#if BT_HOST_WAKEUP
	gpio_set_value(drvdata->wakeup_gpio, blocked);
#else
	gpio_set_value(drvdata->wakeup_gpio, !blocked);
#endif
	return 0;
}

#if BT_HOST_WAKEUP
static irqreturn_t bt_hostwake_irq(int irq, void *dev_id)
{
	irqreturn_t result;

	/* BT host wake up wakelock */
	int bt_wake_lock = 5000;
//BT_S : H5_Sniff_PowerConsumption_workaround
#if ODIN_BT_RFKILL_DBG
	printk(KERN_ERR "%s:bt_hostwake_irq 5 second wakelock\n", __func__);
#endif
//BT_E : H5_Sniff_PowerConsumption_workaround
	__pm_wakeup_event(&bt_hostwake_lock, bt_wake_lock);

	result = IRQ_HANDLED;
	return result;
}
#endif	/* BT_HOST_WAKEUP*/

#if BT_WAKELOCK_RFKILL
static int odin_bt_oob_set_blocked(void *data, bool blocked)
{
	int bt_wake_lock = 5000;
//BT_S : H5_Sniff_PowerConsumption_workaround
#if ODIN_BT_RFKILL_DBG
    printk(KERN_ERR "%s:odin_bt_oob_set_blocked 5 second wakelock\n", __func__);
#endif
//	if (blocked)
		__pm_wakeup_event(&bt_btwake_lock, bt_wake_lock);
//	else
//		__pm_relax(&bt_btwake_lock);
//BT_E : H5_Sniff_PowerConsumption_workaround
	return 0;
}
#endif /* BT_WAKELOCK_RFKILL */

static const struct rfkill_ops odin_bt_rst_rfkill_ops = {
	.set_block = odin_bt_rst_set_power,
};

static const struct rfkill_ops odin_bt_wake_rfkill_ops = {
	.set_block = odin_bt_wake_set_power,
};

#if BT_WAKELOCK_RFKILL
static const struct rfkill_ops odin_bt_oob_rfkill_ops = {
	.set_block = odin_bt_oob_set_blocked,
};
#endif /* BT_WAKELOCK_RFKILL */

static int __init odin_bt_probe(struct platform_device *pdev)
{
	struct odin_bt_platform_data *pdata;
	struct odin_bt_drvdata *drvdata;
	int ret = 0;
#if BT_HOST_WAKEUP
	unsigned long flags = 0;
#endif
#ifdef CONFIG_OF
	int val = 0;
	pdata = (struct odin_bt_platform_data *)kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (IS_ERR(pdata)) {
		dev_err(&pdev->dev, "Failed to alloc bluetooth platform data\n");
		ret = PTR_ERR(pdata);
		goto err;
	}

	if (!of_property_read_u32(pdev->dev.of_node,"reset_gpio", &val))
		pdata->reset_gpio = val;
	val = 0;

	if (!of_property_read_u32(pdev->dev.of_node,"wakeup_gpio", &val))
		pdata->wakeup_gpio = val;
	val = 0;

	if (!of_property_read_u32(pdev->dev.of_node,"hostwake_gpio", &val))
		pdata->hostwake_gpio = val;
	val = 0;

	/* Taking the regulator for BT @ Odin platform (WIFI_IO/VDDIO_EXT) */
	//if ((ODIN_REV_G <= (system_rev & 0xff)) || (ODIN_REV_A == (system_rev & 0xff))) {
		printk(KERN_NOTICE "%s:system revision is 0x%02x\n", __func__, system_rev & 0xff);
		bt_wifi_io = regulator_get(&pdev->dev,
					"+1.8V_VDDIO_EXT");
		if (IS_ERR(bt_wifi_io))
			printk(KERN_ERR "%s:unable to get regulator, maybe it does not have it\n", __func__);
	//}
#else
	pdata = pdev->dev.platform_data;
#endif
	drvdata = kzalloc(sizeof(struct odin_bt_drvdata), GFP_KERNEL);
	if (IS_ERR(drvdata)) {
		dev_err(&pdev->dev, "Failed to alloc bluetooth driver data\n");
		ret = PTR_ERR(drvdata);
		goto free_pdata;
	}

	drvdata->pdata = pdata;
	drvdata->reset_gpio = pdata->reset_gpio;
	drvdata->wakeup_gpio = pdata->wakeup_gpio;
	drvdata->hostwake_gpio = pdata->hostwake_gpio;

	if (pdata->reset_gpio) {
		ret = gpio_request(pdata->reset_gpio, "odin_bt_reset");
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't get bluetooth reset gpio\n");
			goto free_drvdata;
		}
		gpio_direction_output(pdata->reset_gpio, 0);
	}
	else {
		dev_err(&pdev->dev,
				"Did not set bluetooth reset gpio in the bootloader\n");
		goto free_drvdata;
	}

	if (pdata->wakeup_gpio) {
		ret = gpio_request(pdata->wakeup_gpio, "odin_bt_wakeup");
		if (ret < 0) {
			dev_err(&pdev->dev, "Can't get bluetooth wakeup gpio\n");
			goto free_reset;
		}
		gpio_direction_output(pdata->wakeup_gpio, 0);
	}
	else {
		dev_err(&pdev->dev,
				"Did not set bluetooth wakeup gpio in the bootloader\n");
	}

#if BT_HOST_WAKEUP
	if (pdata->hostwake_gpio) {
		ret = gpio_request(pdata->hostwake_gpio, "odin_bt_hostwake");
		if (ret < 0) {
			dev_err(&pdev->dev,
					"Can't get bluetooth hostwake gpio\n");
		}
		else {
			printk(KERN_ERR "%s hostwake_gpio(%d) for Bluetooth\n",
					__func__, pdata->hostwake_gpio);
			/* set the gpio to irq */
			gpio_direction_input(pdata->hostwake_gpio);
			hostwake_irq = gpio_to_irq(pdata->hostwake_gpio);

			flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_SHAREABLE;
			odin_gpio_sms_request_irq(hostwake_irq, bt_hostwake_irq, flags,
					"BT_HOSTWAKE_IRQ", NULL);
			wakeup_source_init(&bt_hostwake_lock, "bt_hostwake_lock");
//BT_S : H5_Sniff_PowerConsumption_workaround
			wakeup_source_init(&bt_btwake_lock, "bt_wake_lock");
//BT_E : H5_Sniff_PowerConsumption_workaround
		}
	}
	else {
		dev_err(&pdev->dev,
				"Did not set bluetooth hostwake gpio in the bootloader\n");
	}
#endif	/* BT_HOST_WAKEUP */

	drvdata->bt_rst_rfkill_dev = rfkill_alloc("odin_bt_rst_rfkill", &pdev->dev,
					    RFKILL_TYPE_BLUETOOTH,
					    &odin_bt_rst_rfkill_ops, drvdata);
	if (IS_ERR(drvdata->bt_rst_rfkill_dev)) {
		dev_err(&pdev->dev, "Failed to alloc bluetooth rst rfkill\n");
		ret = PTR_ERR(drvdata->bt_rst_rfkill_dev);
		goto free_wakeup;
	}

#if (BT_WAKELOCK_RFKILL == 0)
	drvdata->bt_wake_rfkill_dev = rfkill_alloc("odin_bt_wake_rfkill", &pdev->dev,
					    RFKILL_TYPE_BLUETOOTH,
					    &odin_bt_wake_rfkill_ops, drvdata);
	if (IS_ERR(drvdata->bt_wake_rfkill_dev)) {
		dev_err(&pdev->dev, "Failed to alloc bluetooth wake rfkill\n");
		ret = PTR_ERR(drvdata->bt_wake_rfkill_dev);
		goto free_rst_rfkill;
	}
#endif

	rfkill_init_sw_state(drvdata->bt_rst_rfkill_dev, true);

	ret = rfkill_register(drvdata->bt_rst_rfkill_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register bluetooth rst rfkill\n");
		goto free_wake_rfkill;
	}
#if (BT_WAKELOCK_RFKILL == 0)
	rfkill_init_sw_state(drvdata->bt_wake_rfkill_dev, true);

	ret = rfkill_register(drvdata->bt_wake_rfkill_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register bluetooth wakeup rfkill\n");
		goto free_wake_rfkill_unreg;
	}
#endif

#if BT_WAKELOCK_RFKILL
	drvdata->bt_oob_rfkill_dev = rfkill_alloc("odin_bt_oob_rfkill", &pdev->dev,
					    RFKILL_TYPE_BLUETOOTH,
					    &odin_bt_oob_rfkill_ops, drvdata);
	if (IS_ERR(drvdata->bt_oob_rfkill_dev)) {
		dev_err(&pdev->dev, "Failed to alloc bluetooth OOB rfkill\n");
		ret = PTR_ERR(drvdata->bt_oob_rfkill_dev);
		goto free_rfkill_lock;
	}

	rfkill_init_sw_state(drvdata->bt_oob_rfkill_dev, true);

	ret = rfkill_register(drvdata->bt_oob_rfkill_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register bluetooth OOB rfkill\n");
		goto free_rfkill_all;
	}
	dev_err(&pdev->dev, "Registered bluetooth OOB rfkill\n");
#endif /* BT_WAKELOCK_RFKILL */
	platform_set_drvdata(pdev, drvdata);

	return 0;

#if BT_WAKELOCK_RFKILL
free_rfkill_all:
	rfkill_destroy(drvdata->bt_oob_rfkill_dev);
free_rfkill_lock:
#if (BT_WAKELOCK_RFKILL == 0)
	rfkill_unregister(drvdata->bt_wake_rfkill_dev);
free_wake_rfkill_unreg:
#endif
	rfkill_unregister(drvdata->bt_rst_rfkill_dev);
#endif /* BT_WAKELOCK_RFKILL */
free_wake_rfkill:
#if (BT_WAKELOCK_RFKILL == 0)
//BT_S : H5_Sniff_PowerConsumption_workaround
free_wake_rfkill_unreg:
//BT_E : H5_Sniff_PowerConsumption_workaround
	rfkill_destroy(drvdata->bt_wake_rfkill_dev);
free_rst_rfkill:
#endif
	rfkill_destroy(drvdata->bt_rst_rfkill_dev);
free_wakeup:
#if BT_HOST_WAKEUP
	wakeup_source_trash(&bt_hostwake_lock);
//BT_S : H5_Sniff_PowerConsumption_workaround
    wakeup_source_trash(&bt_btwake_lock);
//BT_E : H5_Sniff_PowerConsumption_workaround
#endif
	gpio_free(pdata->wakeup_gpio);
free_reset:
	gpio_free(pdata->reset_gpio);
free_drvdata:
	kfree(drvdata);
free_pdata:
	kfree(pdata);
err:
	dev_err(&pdev->dev, "Probing of odin bt driver failed: %d\n", ret);
	return ret;
}

static int __exit odin_bt_remove(struct platform_device *pdev)
{
	struct odin_bt_drvdata *drvdata = platform_get_drvdata(pdev);

	//if ((ODIN_REV_G <= (system_rev & 0xff)) || (ODIN_REV_A == (system_rev & 0xff))) {
		if (PTR_ERR(bt_wifi_io) != -ENODEV)
			regulator_put(bt_wifi_io);
//	}

	rfkill_unregister(drvdata->bt_rst_rfkill_dev);

#if (BT_WAKELOCK_RFKILL == 0)
	rfkill_unregister(drvdata->bt_wake_rfkill_dev);
#endif
	rfkill_destroy(drvdata->bt_rst_rfkill_dev);
#if (BT_WAKELOCK_RFKILL == 0)
	rfkill_destroy(drvdata->bt_wake_rfkill_dev);
#endif
#if BT_HOST_WAKEUP
#if BT_WAKELOCK_RFKILL
	rfkill_unregister(drvdata->bt_oob_rfkill_dev);
	rfkill_destroy(drvdata->bt_oob_rfkill_dev);
#endif /* BT_WAKELOCK_RFKILL */
	wakeup_source_trash(&bt_hostwake_lock);
//BT_S : H5_Sniff_PowerConsumption_workaround
    wakeup_source_trash(&bt_btwake_lock);
//BT_E : H5_Sniff_PowerConsumption_workaround
#endif
	gpio_free(drvdata->reset_gpio);
	gpio_free(drvdata->wakeup_gpio);
	kfree(drvdata->pdata);
	kfree(drvdata);

	return 0;
}

#if CONFIG_PM
static int odin_bt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct odin_bt_drvdata *drvdata = platform_get_drvdata(pdev);

	odin_bt_wakeup(drvdata, 0);

	return 0;
}

static int odin_bt_resume(struct platform_device *pdev)
{
	struct odin_bt_drvdata *drvdata = platform_get_drvdata(pdev);

	odin_bt_wakeup(drvdata, 1);

	return 0;
}
#else
#define odin_bt_suspend	NULL
#define odin_bt_resume  NULL
#endif
#ifdef CONFIG_OF
static struct of_device_id odin_bt_match[] = {
	{ .compatible = "woden,broadcom,bcm4334-bt"},
	{ .compatible = "odin,broadcom,bcm4335-bt"},
	{ .compatible = "odin,broadcom,bcm4339-bt"},
	{},
};
#endif /* CONFIG_OF */
static struct platform_driver odin_bt_driver = {
	.driver = {
		.name	= "odin-bt",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(odin_bt_match),
#endif /* CONFIG_OF */
	},
	.probe		= odin_bt_probe,
	.remove		= __exit_p(odin_bt_remove),
	.suspend	= odin_bt_suspend,
	.resume		= odin_bt_resume,
};

static int __init odin_bt_init(void)
{
	return platform_driver_register(&odin_bt_driver);
}

static void __exit odin_bt_exit(void)
{
	platform_driver_unregister(&odin_bt_driver);
}

module_init(odin_bt_init);
module_exit(odin_bt_exit);
