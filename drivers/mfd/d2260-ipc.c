/*
 *  IPC driver for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/rtc.h>
#include <linux/mfd/d2260/core.h>
#include <linux/mfd/d2260/pdata.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif

#define D2260_USING_ASYNC_SCHEDULE

#ifdef D2260_USING_ASYNC_SCHEDULE
#include <linux/async.h>
#endif


#ifdef D2260_USING_RANGED_REGMAP

#define COPMIC 	0x01	/* DA9210 register */
#define RD 		0x02	/* Readable register */
#define WR 		0x04	/* Writable register */
#define VOL 	0x08	/* Volatile register */

/* REGMAP IPC Register for D2260 */
#define D2260_I2C_SLOT	 	0
#define D2260_SLAVE_ADDRESS 0x92

static int set_single_rw = 0;
/* Flags for virtual registers (mapped starting from D2260_MAPPING_BASE) */
const unsigned char d2260_reg_flg[] = {
	/* Status / Config */
	[D2260_STATUS_A_REG] =		RD | VOL,
	[D2260_STATUS_B_REG] =		RD | VOL,
	[D2260_STATUS_C_REG] =		RD | VOL,
	[D2260_EVENT_A_REG] =		RD | WR | VOL,
	[D2260_EVENT_B_REG] =		RD | WR | VOL,
	[D2260_EVENT_C_REG] =		RD | WR | VOL,
	[D2260_FAULT_LOG_REG] =		RD | WR | VOL,
	[D2260_IRQ_MASK_A_REG] =	RD | WR,
	[D2260_IRQ_MASK_B_REG] =	RD | WR,
	[D2260_IRQ_MASK_C_REG] =	RD | WR,
	[D2260_CONTROL_A_REG] =		RD | WR,
	[D2260_CONTROL_B_REG] =		RD | WR,
	[D2260_CONTROL_C_REG] =		RD | WR,
	[D2260_CONTROL_D_REG] =		RD | WR,
	[D2260_PD_DIS_REG] =		RD | WR,
	[D2260_INTERFACE_REG] = 	RD | WR,
	[D2260_RESET_REG] =  		RD | WR,
	/* GPIO */
	[D2260_SMPL_GPIO_0_REG] = 	RD | WR,
	[D2260_TA_GPIO_1_REG] = 	RD | WR,
	[D2260_GPIO_0_NON_REG] =	RD | WR,
	/* Sequencer */
	[D2260_ID_0_1_REG] =		RD | WR,
	[D2260_ID_2_3_REG] =		RD | WR,
	[D2260_ID_4_5_REG] =		RD | WR,
	[D2260_ID_6_7_REG] =		RD | WR,
	[D2260_ID_8_9_REG] =		RD | WR,
	[D2260_ID_10_11_REG] =		RD | WR,
	[D2260_ID_12_13_REG] =		RD | WR,
	[D2260_ID_14_15_REG] =		RD | WR,
	[D2260_ID_16_17_REG] =		RD | WR,
	[D2260_ID_18_19_REG] =		RD | WR,
	[D2260_ID_20_21_REG] =		RD | WR,
	[D2260_SEQ_STATUS_REG] = 	RD | WR,
	[D2260_SEQ_A_REG] = 		RD | WR,
	[D2260_SEQ_B_REG] = 		RD | WR,
	[D2260_SEQ_TIMER_REG] = 	RD | WR,
	/*Supplies */
	[D2260_BUCK0_CONF0_REG] = 	RD | WR,
	[D2260_BUCK0_CONF1_REG] = 	RD | WR,
	[D2260_BUCK0_CONF2_REG] = 	RD | WR,
	[D2260_BUCK1_CONF0_REG] = 	RD | WR,
	[D2260_BUCK1_CONF1_REG] = 	RD | WR,
	[D2260_BUCK1_CONF2_REG] = 	RD | WR,
	[D2260_BUCK2_CONF0_REG] = 	RD | WR,
	[D2260_BUCK2_CONF1_REG] = 	RD | WR,
	[D2260_BUCK3_CONF0_REG] = 	RD | WR,
	[D2260_BUCK3_CONF1_REG] = 	RD | WR,
	[D2260_BUCK4_CONF0_REG] = 	RD | WR,
	[D2260_BUCK4_CONF1_REG] = 	RD | WR,
	[D2260_BUCK5_CONF0_REG] = 	RD | WR,
	[D2260_BUCK5_CONF1_REG] = 	RD | WR,
	[D2260_BUCK6_CONF0_REG] = 	RD | WR,
	[D2260_BUCK6_CONF1_REG] = 	RD | WR,
	[D2260_BUCKRF_THR_REG] = 	RD | WR,
	[D2260_BUCKRF_CONF_REG] = 	RD | WR,
	[D2260_LDO1_REG] = 		RD | WR,
	[D2260_LDO2_REG] = 		RD | WR,
	[D2260_LDO3_REG] = 		RD | WR,
	[D2260_LDO4_REG] = 		RD | WR,
	[D2260_LDO5_REG] = 		RD | WR,
	[D2260_LDO6_REG] = 		RD | WR,
	[D2260_LDO7_REG] = 		RD | WR,
	[D2260_LDO8_REG] = 		RD | WR,
	[D2260_LDO9_REG] = 		RD | WR,
	[D2260_LDO10_REG] = 	RD | WR,
	[D2260_LDO11_REG] = 	RD | WR,
	[D2260_LDO12_REG] = 	RD | WR,
	[D2260_LDO13_REG] = 	RD | WR,
	[D2260_LDO14_REG] = 	RD | WR,
	[D2260_LDO15_REG] = 	RD | WR,
	[D2260_LDO16_REG] = 	RD | WR,
	[D2260_LDO17_REG] = 	RD | WR,
	[D2260_LDO18_REG] = 	RD | WR,
	[D2260_LDO19_REG] = 	RD | WR,
	[D2260_LDO20_REG] = 	RD | WR,
	[D2260_LDO21_REG] = 	RD | WR,
	[D2260_LDO22_REG] = 	RD | WR,
	[D2260_LDO23_REG] = 	RD | WR,
	[D2260_LDO24_REG] = 	RD | WR,
	[D2260_LDO25_REG] = 	RD | WR,
	[D2260_SUPPLY_REG] = 	RD | WR,
	/* Mode Control */
	[D2260_LDO1_MCTL_REG] = RD | WR,
	[D2260_LDO2_MCTL_REG] = RD | WR,
	[D2260_LDO3_MCTL_REG] = RD | WR,
	[D2260_LDO4_MCTL_REG] = RD | WR,
	[D2260_LDO5_MCTL_REG] = RD | WR,
	[D2260_LDO6_MCTL_REG] = RD | WR,
	[D2260_LDO7_MCTL_REG] = RD | WR,
	[D2260_LDO8_MCTL_REG] = RD | WR,
	[D2260_LDO9_MCTL_REG] = RD | WR,
	[D2260_LDO10_MCTL_REG] = 	RD | WR,
	[D2260_LDO11_MCTL_REG] = 	RD | WR,
	[D2260_LDO12_MCTL_REG] = 	RD | WR,
	[D2260_LDO13_MCTL_REG] = 	RD | WR,
	[D2260_LDO14_MCTL_REG] = 	RD | WR,
	[D2260_LDO15_MCTL_REG] = 	RD | WR,
	[D2260_LDO16_MCTL_REG] = 	RD | WR,
	[D2260_LDO17_MCTL_REG] = 	RD | WR,
	[D2260_LDO18_MCTL_REG] = 	RD | WR,
	[D2260_LDO19_MCTL_REG] = 	RD | WR,
	[D2260_LDO20_MCTL_REG] = 	RD | WR,
	[D2260_LDO21_MCTL_REG] = 	RD | WR,
	[D2260_LDO22_MCTL_REG] = 	RD | WR,
	[D2260_LDO23_MCTL_REG] = 	RD | WR,
	[D2260_LDO24_MCTL_REG] = 	RD | WR,
	[D2260_LDO25_MCTL_REG] = 	RD | WR,
	[D2260_BUCK0_MCTL_REG] = 	RD | WR,
	[D2260_BUCK1_MCTL_REG] = 	RD | WR,
	[D2260_BUCK2_MCTL_REG] = 	RD | WR,
	[D2260_BUCK3_MCTL_REG] = 	RD | WR,
	[D2260_BUCK4_MCTL_REG] = 	RD | WR,
	[D2260_BUCK5_MCTL_REG] = 	RD | WR,
	[D2260_BUCK6_MCTL_REG] = 	RD | WR,
	[D2260_BUCK_RF_MCTL_REG] = 	RD | WR,
	[D2260_GPADC_MCTL_REG] = 	RD | WR,
	[D2260_MISC_MCTL_REG] = 	RD | WR,
	[D2260_VBUCK0_MCTL_RET_REG] = 	RD | WR,
	[D2260_VBUCK0_MCTL_TUR_REG] = 	RD | WR,
	[D2260_VBUCK1_MCTL_RET_REG] = 	RD | WR,
	[D2260_VBUCK4_MCTL_RET_REG] = 	RD | WR,
	/* Control */
	[D2260_WAIT_CONT_REG] = 	RD | WR,
	[D2260_ONKEY_CONT1_REG] = 	RD | WR,
	[D2260_ONKEY_CONT2_REG] = 	RD | WR,
	[D2260_POWER_CONT_REG] = 	RD | WR,
	[D2260_VDDFAULT_REG] = 		RD | WR,
	[D2260_BBAT_CONT_REG] = 	RD | WR,
	/* ADC */
	[D2260_ADC_MAN_REG] = 		RD | WR,
	[D2260_ADC_CONT_REG] = 		RD | WR,
	[D2260_ADC_RES_L_REG] = 	RD | VOL,
	[D2260_ADC_RES_H_REG] = 	RD | VOL,
	[D2260_VBAT_RES_REG] =  	RD | VOL,
	[D2260_VDDOUT_MON_REG] = 	RD | WR,
	[D2260_TEMP1_RES_REG] = 	RD | VOL,
	[D2260_TEMP1_HIGHP_REG] =	RD | WR,
	[D2260_TEMP1_HIGHN_REG] =	RD | WR,
	[D2260_TEMP1_LOW_REG] =		RD | WR,
	[D2260_T_OFFSET_REG] =		RD | WR,
	[D2260_VF_RES_REG] = 		RD | VOL,
	[D2260_VF_HIGH_REG] =		RD | WR,
	[D2260_VF_LOW_REG] =		RD | WR,
	[D2260_TEMP2_RES_REG] = 	RD | VOL,
	[D2260_TEMP2_HIGHP_REG] = 	RD | WR,
	[D2260_TEMP2_HIGHN_REG] = 	RD | WR,
	[D2260_TEMP2_LOW_REG] = 	RD | WR,
	[D2260_TJUNC_RES_REG] = 	RD | VOL,
	[D2260_ADC_RES_AUTO1_REG] = RD | VOL,
	[D2260_ADC_RES_AUTO2_REG] = RD | VOL,
	[D2260_ADC_RES_AUTO3_REG] = RD | VOL,
	/* RTC */
	[D2260_COUNT_S_REG] = 		RD | WR | VOL,
	[D2260_COUNT_MI_REG] = 		RD | WR | VOL,
	[D2260_COUNT_H_REG] = 		RD | WR | VOL,
	[D2260_COUNT_D_REG] = 		RD | WR | VOL,
	[D2260_COUNT_MO_REG] = 		RD | WR | VOL,
	[D2260_COUNT_Y_REG] = 		RD | WR | VOL,
	[D2260_ALARM_S_REG] = 		RD | WR,
	[D2260_ALARM_MI_REG] = 		RD | WR | VOL,
	[D2260_ALARM_H_REG] = 		RD | WR,
	[D2260_ALARM_D_REG] = 		RD | WR,
	[D2260_ALARM_MO_REG] = 		RD | WR,
	[D2260_ALARM_Y_REG] = 		RD | WR,
	/* OTP Config */
	[D2260_CHIP_ID_REG] = 		RD,
	[D2260_CONFIG_ID_REG] = 	RD,
	[D2260_OTP_CONT_REG] = 		RD | WR,
	[D2260_OSC_TRIM_REG] = 		RD | WR,
	[D2260_GP_ID_0_REG] = 		RD | WR,
	[D2260_GP_ID_1_REG] = 		RD | WR,
	[D2260_GP_ID_2_REG] = 		RD | WR,
	[D2260_GP_ID_3_REG] = 		RD | WR,
	[D2260_GP_ID_4_REG] = 		RD | WR,
	[D2260_GP_ID_5_REG] = 		RD | WR,
	/* Audio */
	[D2260_GEN_CONF_0_REG] = 	RD | WR,
	[D2260_GEN_CONF_1_REG] = 	RD | WR,
	/* Test Registers */
	[D2260_BUCK0_CONF5_REG] = 	RD | WR,
	[D2260_BUCK1_CONF5_REG] = 	RD | WR,
	[D2260_BUCK4_CONF4_REG] = 	RD | WR,
	[D2260_BUCK5_CONF4_REG] = 	RD | WR,
	/* Trim */
	[D2260_BUCK7_PDDIS_EXT_CTRL_32K_REG] = RD | WR,
	/* For page*/
	[D2260_PAGE0_CON_REG] = RD | WR,
	[D2260_PAGE1_CON_REG] = RD | WR,
};

int d2260_single_set(void)
{
	return set_single_rw;
}

static bool d2260_reg_readable(struct device *dev, unsigned int reg)
{
	if (reg < D2260_MAPPING_BASE) {
		return true;
	} else {
		reg -= D2260_MAPPING_BASE;
		if (d2260_reg_flg[reg] & RD) {
			if (d2260_reg_flg[reg] & COPMIC) {
				struct d2260 *d2260 = dev_get_drvdata(dev);

				if (!d2260->pdata)
					return false;
			}

			return true;
		}

		return false;
	}
}

static bool d2260_reg_writable(struct device *dev, unsigned int reg)
{
	if (reg < D2260_MAPPING_BASE) {
		return true;
	} else {
		reg -= D2260_MAPPING_BASE;
		if (d2260_reg_flg[reg] & WR) {
			if (d2260_reg_flg[reg] & COPMIC) {
				struct d2260 *d2260 = dev_get_drvdata(dev);

				if (!d2260->pdata)
					return false;
			}

			return true;
		}

		return false;
	}
}

static bool d2260_reg_volatile(struct device *dev, unsigned int reg)
{
	if (reg < D2260_MAPPING_BASE) {
		switch (reg) {
		case D2260_MAPPING_BASE:    /* Use cache for page selector */
			return false;
		default:
			return true;
		}
	} else {
		reg -= D2260_MAPPING_BASE;
		if (d2260_reg_flg[reg] & VOL) {
			if (d2260_reg_flg[reg] & COPMIC) {
				struct d2260 *d2260 = dev_get_drvdata(dev);

				if (!d2260->pdata)
					return false;
			}

			return true;
		}

		return false;
	}
}

static const struct regmap_range_cfg d2260_range_cfg[] = {
	{
		.range_min = D2260_MAPPING_BASE,
		.range_max = D2260_MAPPING_BASE +
			     ARRAY_SIZE(d2260_reg_flg) - 1,
		.selector_reg = D2260_REG_PAGE_CON,
		.selector_mask = 1 << D2260_I2C_PAGE_SEL_SHIFT,
		.selector_shift = D2260_I2C_PAGE_SEL_SHIFT,
		.window_start = 0,
		.window_len = 256,
	}
};

struct regmap_config d2260_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
		.ranges = d2260_range_cfg,
		.num_ranges = ARRAY_SIZE(d2260_range_cfg),
		.max_register = D2260_MAPPING_BASE + ARRAY_SIZE(d2260_reg_flg) - 1,
        .cache_type = REGCACHE_NONE,
		.use_single_rw = 1,
		.reg_stride = 1,
        .readable_reg = d2260_reg_readable,
        .writeable_reg = d2260_reg_writable,
        .volatile_reg = d2260_reg_volatile,
};
#else
extern struct regmap_config d2260_regmap_config;
#endif /* D2260_USING_RANGED_REGMAP */

int d2260_device_init(struct d2260 *d2260, u8 chip_id);
void d2260_device_exit(struct d2260 *d2260);

static const struct i2c_device_id d2260_i2c_id[] = {
	{ "d2200-aa", D2200_AA},	/* cheetah */
	{ "d2260", D2260 },			/* generic */
	{ "d2260-aa", D2260_AA },	/* puma    */
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2260_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id d2260_dt_ids[] = {
        { .compatible = "dlg,d2200-aa", .data = &d2260_i2c_id[0] },
        { .compatible = "dlg,d2260", .data = &d2260_i2c_id[1] },
        { .compatible = "dlg,d2260-aa", .data = &d2260_i2c_id[2] },
        { /* sentinel */ }
};
#endif


static int d2260_i2c_enable_multiwrite(struct d2260 *d2260)
{
	return d2260_update_bits(d2260,
                D2260_CONTROL_B_REG, D2260_WRITE_MODE_MASK, 0xFF);
}

static int d2260_ipc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ipc_client *ipc;
	struct device *dev = &pdev->dev;
	struct d2260_pdata *pdata;
	struct d2260 *d2260;
	int ret = 0, irq;

	if (pdev->dev.of_node) {
		pdata = kzalloc(sizeof(struct d2260_pdata), GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;

		ret = of_property_read_u32(pdev->dev.of_node, "num_gpio", &pdata->num_gpio);
		if (ret < 0) {
			ret = -ENODEV;
			goto out_free;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL)
			return -ENODEV;
	}

	ret = gpio_request(pdata->num_gpio, "pmic-irq");
	if (ret < 0)
		goto out_free;

	ret = gpio_direction_input(pdata->num_gpio);
	if (ret < 0)
		goto out_free;

	irq = gpio_to_irq(pdata->num_gpio);
	if (ret < 0)
		goto out_free;

	ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if (ipc == NULL) {
		dev_err(d2260->dev, "Platform data not specified.\n");
		ret = -ENOMEM;
		goto out_free;
	}
	ipc->slot = D2260_I2C_SLOT;
	ipc->addr = D2260_SLAVE_ADDRESS;
	ipc->dev = dev;

	d2260 = kzalloc(sizeof(struct d2260), GFP_KERNEL);
	if (d2260 == NULL) {
		ret = -ENOMEM;
		goto free_mem_ipc;
	}

	platform_set_drvdata(pdev, d2260);
	d2260->dev = dev;

	d2260->regmap = devm_regmap_init_ipc(ipc, &d2260_regmap_config);
	if (IS_ERR(d2260->regmap)) {
		ret = PTR_ERR(d2260->regmap);
		dev_err(d2260->dev, "Failed to allocate register map: %d\n",
			ret);
		goto free_mem;
	}

	if (d2260_regmap_config.use_single_rw == 1)
		set_single_rw = 1;

	kfree(pdata);

	return d2260_device_init(d2260, irq);

free_mem:
	kfree(d2260);
free_mem_ipc:
	kfree(ipc);
	gpio_free(pdata->num_gpio);
out_free:
	kfree(pdata);

	return ret;
}

static int d2260_ipc_remove(struct platform_device *pdev)
{
	struct d2260 *d2260 = platform_get_drvdata(pdev);

	d2260_device_exit(d2260);

	return 0;
}


extern struct dev_pm_ops d2260_pm_ops;

static struct platform_driver d2260_ipc_driver = {
	.driver = {
		.name = "d2260-aa",	/* puma */
		.owner = THIS_MODULE,
		.pm = &d2260_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = d2260_dt_ids,
#endif
	},
	.probe    = d2260_ipc_probe,
	.remove   = d2260_ipc_remove,
	.id_table = d2260_i2c_id,
};

#ifdef D2260_USING_ASYNC_SCHEDULE
static void d2260_ipc_init_async(void)
{
	int ret;

	ret = platform_driver_register(&d2260_ipc_driver);
	if (ret != 0)
		pr_err("Failed to register d2260 IPC driver\n");
}
#endif


static int __init d2260_ipc_init(void)
{
#ifdef D2260_USING_ASYNC_SCHEDULE
	async_schedule(d2260_ipc_init_async, NULL);

	return 0;
#else
	int ret;

	ret = platform_driver_register(&d2260_ipc_driver);
	if (ret != 0)
		pr_err("Failed to register d2260 IPC driver\n");

	return ret;
#endif
}
fs_initcall(d2260_ipc_init);

static void __exit d2260_ipc_exit(void)
{
	platform_driver_unregister(&d2260_ipc_driver);
}
module_exit(d2260_ipc_exit);

MODULE_AUTHOR("William Seo <william.seo@diasemi.com>");
MODULE_AUTHOR("Tony Olech <anthony.olech@diasemi.com>");
MODULE_DESCRIPTION("I2C MFD driver for Dialog D2260 PMIC");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:d2260");
