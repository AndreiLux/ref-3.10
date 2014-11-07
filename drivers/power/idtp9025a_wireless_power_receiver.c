/*
 * IDPT9025A Wireless Power Receiver driver
 *
 * Copyright (C) 2014 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/regulator/machine.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/board_lge.h>
#include <linux/power/unified_wireless_charger_alignment.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/board_lge.h>
#include <linux/wakelock.h>

#define IDT9025_DEVICE_NAME		"idtp9025a"

#define IDTP9025A_VOLT_H (0x40)
#define IDTP9025A_VOLT_L (0x41)

#define IDTP9025A_CURRENT_H (0x42)
#define IDTP9025A_CURRENT_L (0x43)

#define IDTP9025A_FREQ_H (0x44)
#define IDTP9025A_FREQ_L (0x45)

#define CONV_VOLTS(x) 	((x * 5 * 1800) / 4096)
#define CONV_CURRENT(x) 	((x * 1800) / 4096)
#define CONV_FREQ(x) 		((65536 / x))

#define OUTPUT_CURRENT_TH	512

#define FREQ_TH_1	157
#define FREQ_HYSTERESIS_UP_1	3
#define FREQ_HYSTERESIS_DOWN_1	3

#define FREQ_TH_2	145
#define FREQ_HYSTERESIS_UP_2	8
#define FREQ_HYSTERESIS_DOWN_2	5

#define FREQ_MIN	100

/* REGMAP IPC I2C_3*/
#define IDT9025_I2C_SLOT_3      4
#define IDT9025_SLAVE_ADDR_3    0x25

/* REGMAP IPC I2C_1*/
#define IDT9025_I2C_SLOT_1      2
#define IDT9025_SLAVE_ADDR_1    0x4A
#define IDT9025_WLC_FULL        133

static struct idt9025_dev *global_wlc = NULL;
int global_freq = 0;

struct idt9025_platform_data
{
	int idt9025_val;
};

struct idt9025_dev
{
	struct device *dev;
	struct regmap *regmap;
	struct wake_lock wireless_eoc_wake_lock;
};

static const struct regmap_config idt9025_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 8,
    .cache_type = REGCACHE_NONE,
};

enum {
	WLC_ALIGNMENT_LEVEL_NC = 0,
	WLC_ALIGNMENT_LEVEL_1,
	WLC_ALIGNMENT_LEVEL_2,
};

static int last_freq = 0;
static int last_current = 0;
static int last_alignment = WLC_ALIGNMENT_LEVEL_NC;

static int idtp9025_get_voltage(void)
{
	int ret = 0;
	unsigned int status_1 = 0;
	unsigned int status_2 = 0;
	unsigned int status_sum = 0;

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -1;
	}

	/* Volt_1 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_VOLT_H, &status_1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Volt_1_read_fail\n", __func__);
		return -1;
	}

	/* Volt_2 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_VOLT_L, &status_2);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Volt_2_read_fail\n", __func__);
		return -1;
	}

	/* Combination */
	status_2 = status_2 >> 4;
	status_sum = (status_1 << 4) | status_2;

	return CONV_VOLTS(status_sum);
}

static int idtp9025_get_current(void)
{
	int ret = 0;
	unsigned int status_1 = 0;
	unsigned int status_2 = 0;
	unsigned int status_sum = 0;

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -1;
	}

	/* Current_1 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_CURRENT_H, &status_1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Current_1_read_fail\n", __func__);
		return -1;
	}

	/* Current_2 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_CURRENT_L, &status_2);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Current_2_read_fail\n", __func__);
		return -1;
	}

	//pr_err("%s: status_1 = 0x%02X status_2 = 0x%02X\n", __func__, status_1, status_2);

	/* Combination */
	status_2 = status_2 >> 4;
	status_sum = (status_1 << 4) | status_2;
	//pr_err("%s: status_sum = %d\n", __func__, status_sum);

	if(status_sum <= 0) {
		pr_err("%s: zero value return\n", __func__);
		return -1;
	}

	return CONV_CURRENT(status_sum);
}

static int idtp9025_get_frequency(void)
{
	int ret = 0;
	unsigned int status_1 = 0;
	unsigned int status_2 = 0;
	unsigned int status_sum = 0;


	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -1;
	}

	/* Freq_1 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_FREQ_H, &status_1);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Freq_1_read_fail\n", __func__);
		return -1;
	}

	/* Freq_2 */
	ret = regmap_read(global_wlc->regmap, IDTP9025A_FREQ_L, &status_2);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: Freq_2_read_fail\n", __func__);
		return -1;
	}

	//pr_err("%s: status_1 = 0x%02X status_2 = 0x%02X\n", __func__, status_1, status_2);

	/* Combination */
	status_2 = status_2 >> 6;
	status_sum = (status_1 << 2) | status_2;
	//pr_err("%s: status_sum = %d\n", __func__, status_sum);

	if(status_sum <= 0){
		pr_err("%s: zero value return\n", __func__);
		return -1;
	}

	return CONV_FREQ(status_sum);
}

static int idtp9025_calc_alignment(int freq, int curr)
{
	int freq_th = 0;
	int freq_hystersis_up = 0;
	int freq_hystersis_down = 0;

#if 0
	if(curr < OUTPUT_CURRENT_TH) {
		freq_th = FREQ_TH_1;
		freq_hystersis_up = FREQ_HYSTERESIS_UP_1;
		freq_hystersis_down = FREQ_HYSTERESIS_DOWN_1;
	} else {
		freq_th = FREQ_TH_2;
		freq_hystersis_up = FREQ_HYSTERESIS_UP_2;
		freq_hystersis_down = FREQ_HYSTERESIS_DOWN_2;
	}
#endif
	freq_th = FREQ_TH_2;
	freq_hystersis_up = FREQ_HYSTERESIS_UP_2;
	freq_hystersis_down = FREQ_HYSTERESIS_DOWN_2;

	switch(last_alignment) {
		case WLC_ALIGNMENT_LEVEL_NC:
			{
				if(freq >= freq_th) {
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_2;
				} else if(freq > FREQ_MIN) { //FREQ_MIN = 100
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_1;
				} else {
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_NC;
				}
			}
			break;
		case WLC_ALIGNMENT_LEVEL_1:
			{
				if(freq > (freq_th+freq_hystersis_up)) { //158
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_2;
				} else if(freq < FREQ_MIN) {
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_NC;
				}
			}
			break;
		case WLC_ALIGNMENT_LEVEL_2:
			{
				if(freq < FREQ_MIN) {
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_NC;
				} else if(freq <= (freq_th-freq_hystersis_down)) { //140
					last_freq = freq;
					last_current = curr;
					last_alignment = WLC_ALIGNMENT_LEVEL_1;
				}
			}
			break;
		default:
			break;
	}

	return last_alignment;
}

static int idtp9025_get_alignment(void)
{
	int freq = 0;
	int curr = 0;
	int align = 0;
	int volt = 0;

	volt = idtp9025_get_voltage();

	freq = idtp9025_get_frequency();
	if(freq > 0) {
		curr = idtp9025_get_current();
		if(curr > 0) {
			align = idtp9025_calc_alignment(freq, curr);
			pr_info("%s = VOLT: %d FREQ: %d CURR: %d ALIGN: %d\n",__func__, volt, freq, curr, align);
			global_freq = freq;

			return align;
		}
	}
	pr_info("%s = volt: %d freq: %d curr: %d align: %d\n",__func__, volt, freq, curr, align);
	global_freq = freq;

	return -1;
}

int idtp9025_align_freq_value(void)
{
	return global_freq;
}

int idtp9025_align_init_check(void)
{
	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	} else {
		return 0;
	}
}

int idtp9025_align_start(void)
{
	last_freq = 0;
	last_current = 0;
	last_alignment = WLC_ALIGNMENT_LEVEL_NC;

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	}

	return idtp9025_get_alignment();
}

int idtp9025_align_stop(void)
{
	last_freq = 0;
	last_current = 0;
	last_alignment = WLC_ALIGNMENT_LEVEL_NC;

	return 0;
}

int idtp9025_align_get_value(void)
{
	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	}

	return idtp9025_get_alignment();
}

void idtp9025_align_set_wlc_full_gpio(void)
{
	//printk("idtp9025_align_set_wlc_full_gpio_called\n");

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	}

	wake_lock(&global_wlc->wireless_eoc_wake_lock);
	gpio_set_value(IDT9025_WLC_FULL, 1);
	pr_err("%s : Send HIGH signal!! \n", __func__);
	msleep(3500);
	gpio_set_value(IDT9025_WLC_FULL, 0);
	pr_err("%s : Send LOW signal!! \n", __func__);
	wake_unlock(&global_wlc->wireless_eoc_wake_lock);

}

void idtp9025_align_set_wlc_full_gpio_high(void)
{
	//printk("idtp9025_align_set_wlc_full_gpio_HIGH_called\n");

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	}

	wake_lock(&global_wlc->wireless_eoc_wake_lock);
	gpio_set_value(IDT9025_WLC_FULL, 1);
	//pr_err("%s : Send <<<HIGH>>> signal!! \n", __func__);
	wake_unlock(&global_wlc->wireless_eoc_wake_lock);
}

void idtp9025_align_set_wlc_full_gpio_low(void)
{
	printk("idtp9025_align_set_wlc_full_gpio_LOW_called\n");

	if(global_wlc == NULL) {
		pr_err("%s: global_wlc_not_set\n", __func__);
		return -EINVAL;
	}

	wake_lock(&global_wlc->wireless_eoc_wake_lock);
	gpio_set_value(IDT9025_WLC_FULL, 0);
	//pr_err("%s : Send <<<LOW>>> signal!! \n", __func__);
	wake_unlock(&global_wlc->wireless_eoc_wake_lock);
}

static int idtp9025_probe (struct platform_device *pdev)
{
	struct ipc_client *regmap_ipc;
	struct device *dev = &pdev->dev;
	struct idt9025_dev *idt9025;
	struct idt9025_platform_data *pdata;
	int wlc_full_gpio = 133;
	int ret;

	pr_err("%s: start\n", __func__);

	/* Memory Allocation */
	idt9025 = devm_kzalloc(dev, sizeof(*idt9025), GFP_KERNEL);
	if (unlikely(idt9025 == NULL)){
		pr_err("%s: devm_kzalloc_fail\n", __func__);
		return -ENOMEM;
	}

	/* Device set */
	idt9025->dev = dev;

	/* Regmap Setting */
    regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
    if (regmap_ipc == NULL) {
        pr_err("%s : Platform data not specified.\n", __func__);
        return -ENOMEM;
    }

	if(lge_get_board_revno() == HW_REV_A) { /* Olny Rev.A */
		regmap_ipc->slot = IDT9025_I2C_SLOT_3;
		regmap_ipc->addr = IDT9025_SLAVE_ADDR_3;
		printk("idtp9025_probe I2C Slot 3\n");
	} else {
		regmap_ipc->slot = IDT9025_I2C_SLOT_1;
		regmap_ipc->addr = IDT9025_SLAVE_ADDR_1;
		printk("idtp9025_probe I2C Slot 1\n");
	}

	regmap_ipc->dev  = dev;

	idt9025->regmap = devm_regmap_init_ipc(regmap_ipc, &idt9025_regmap_config);
	if (IS_ERR(idt9025->regmap)){
		pr_err("%s: regmap_init_ipc_fail\n", __func__);
		return PTR_ERR(idt9025->regmap);
	}

	dev_set_drvdata(dev, idt9025);

	global_wlc = idt9025;

	wake_lock_init(&idt9025->wireless_eoc_wake_lock, WAKE_LOCK_SUSPEND,"unified_wireless_eoc");

	/* GPIO Setting */
	if (!gpio_is_valid(wlc_full_gpio)) {
		printk("Fail to get read gpio for wlc_full\n");
	} else {
		ret = gpio_request(wlc_full_gpio, "wlc_full_gpio");
		if(ret) {
			printk("request reset wlc_full_gpio failed, rc=%d\n", ret);
		}
	}

	ret = gpio_direction_output(wlc_full_gpio, 0);
	if (ret < 0) {
		printk("request gpio_direction_output wlc_full_gpio failed, rc=%d\n", ret);
	}

	pr_err("%s: finish\n", __func__);
	return 0;
}

static int __exit idt9025_remove(struct platform_device *pdev)
{
	return 0;
}

static int idt9025_suspend(struct platform_device *pdev)
{
	return 0;
}

static int idt9025_resume(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id idt9025_of_match[] = {
        { .compatible = "idt,idt9025_wlc" },
        { },
};
MODULE_DEVICE_TABLE(of, idt9025_of_match);

static const struct platform_device_id idt9025_ids[] = {
	{ IDT9025_DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, idt9025_ids);

static struct platform_driver idtp9025_driver = {
	.driver	= {
		.name	= IDT9025_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = idt9025_of_match,
	},
	.probe		= idtp9025_probe,
	.suspend	= idt9025_suspend,
	.resume		= idt9025_resume,
	.remove		= idt9025_remove,
	.id_table	= idt9025_ids,
};

static int __init idt9025_init(void)
{
	return platform_driver_register(&idtp9025_driver);
}
late_initcall(idt9025_init);

static void __exit idt9025_exit(void)
{
	platform_driver_unregister(&idtp9025_driver);
}
module_exit(idt9025_exit);

MODULE_DESCRIPTION("IDTP9025");
MODULE_LICENSE("GPL V2");
