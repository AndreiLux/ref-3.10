/*
 *  Device access for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *  Author: William Seo <william.seo@diasemi.com>
 *  Author: Tony Olech <anthony.olech@diasemi.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>

#include <linux/smp.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/registers.h>
#include <linux/mfd/d2260/rtc.h>
#include <linux/mfd/d2260/core.h>

#if 0 /* Skip D2260 Debug FS & GPO for Booting Time... */
#define D2260_GPO_ENABLE    1
#endif

#ifdef CONFIG_PROC_FS
#define D2260_PROC_NAME 	"pmu0"

#define D2260_IOCTL_READ_REG  	0xc0025083
#define D2260_IOCTL_WRITE_REG 	0x40025084
#endif

static struct d2260 *g_d2260 = NULL;

#ifndef D2260_USING_RANGED_REGMAP
static bool d2260_reg_readable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case D2260_PAGE0_CON_REG:
	case D2260_PAGE1_CON_REG:
	case D2260_STATUS_A_REG:
	case D2260_STATUS_B_REG:
	case D2260_STATUS_C_REG:
	case D2260_EVENT_A_REG:
	case D2260_EVENT_B_REG:
	case D2260_EVENT_C_REG:
	case D2260_FAULT_LOG_REG:
	case D2260_IRQ_MASK_A_REG:
	case D2260_IRQ_MASK_B_REG:
	case D2260_IRQ_MASK_C_REG:
	case D2260_CONTROL_A_REG:
	case D2260_CONTROL_B_REG:
	case D2260_CONTROL_C_REG:
	case D2260_CONTROL_D_REG:
	case D2260_PD_DIS_REG:
	case D2260_INTERFACE_REG:
	case D2260_RESET_REG:
	case D2260_TA_GPIO_1_REG:
	case D2260_GPIO_0_NON_REG:
	case D2260_ID_0_1_REG:
	case D2260_ID_2_3_REG:
	case D2260_ID_4_5_REG:
	case D2260_ID_6_7_REG:
	case D2260_ID_8_9_REG:
	case D2260_ID_10_11_REG:
	case D2260_ID_12_13_REG:
	case D2260_ID_14_15_REG:
	case D2260_ID_16_17_REG:
	case D2260_ID_18_19_REG:
	case D2260_ID_20_21_REG:
	case D2260_SEQ_STATUS_REG:
	case D2260_SEQ_A_REG:
	case D2260_SEQ_B_REG:
	case D2260_SEQ_TIMER_REG:
	case D2260_BUCK0_CONF0_REG:
	case D2260_BUCK0_CONF1_REG:
	case D2260_BUCK0_CONF2_REG:
	case D2260_BUCK1_CONF0_REG:
	case D2260_BUCK1_CONF1_REG:
	case D2260_BUCK1_CONF2_REG:
	case D2260_BUCK2_CONF0_REG:
	case D2260_BUCK2_CONF1_REG:
	case D2260_BUCK3_CONF0_REG:
	case D2260_BUCK3_CONF1_REG:
	case D2260_BUCK4_CONF0_REG:
	case D2260_BUCK4_CONF1_REG:
	case D2260_BUCK5_CONF0_REG:
	case D2260_BUCK5_CONF1_REG:
	case D2260_BUCK6_CONF0_REG:
	case D2260_BUCK6_CONF1_REG:
	case D2260_BUCKRF_THR_REG:
	case D2260_BUCKRF_CONF_REG:
	case D2260_LDO1_REG:
	case D2260_LDO2_REG:
	case D2260_LDO3_REG:
	case D2260_LDO4_REG:
	case D2260_LDO5_REG:
	case D2260_LDO6_REG:
	case D2260_LDO7_REG:
	case D2260_LDO8_REG:
	case D2260_LDO9_REG:
	case D2260_LDO10_REG:
	case D2260_LDO11_REG:
	case D2260_LDO12_REG:
	case D2260_LDO13_REG:
	case D2260_LDO14_REG:
	case D2260_LDO15_REG:
	case D2260_LDO16_REG:
	case D2260_LDO17_REG:
	case D2260_LDO18_REG:
	case D2260_LDO19_REG:
	case D2260_LDO20_REG:
	case D2260_LDO21_REG:
	case D2260_LDO22_REG:
	case D2260_LDO23_REG:
	case D2260_LDO24_REG:
	case D2260_LDO25_REG:
	case D2260_SUPPLY_REG:
	case D2260_LDO1_MCTL_REG:
	case D2260_LDO2_MCTL_REG:
	case D2260_LDO3_MCTL_REG:
	case D2260_LDO4_MCTL_REG:
	case D2260_LDO5_MCTL_REG:
	case D2260_LDO6_MCTL_REG:
	case D2260_LDO7_MCTL_REG:
	case D2260_LDO8_MCTL_REG:
	case D2260_LDO9_MCTL_REG:
	case D2260_LDO10_MCTL_REG:
	case D2260_LDO11_MCTL_REG:
	case D2260_LDO12_MCTL_REG:
	case D2260_LDO13_MCTL_REG:
	case D2260_LDO14_MCTL_REG:
	case D2260_LDO15_MCTL_REG:
	case D2260_LDO16_MCTL_REG:
	case D2260_LDO17_MCTL_REG:
	case D2260_LDO18_MCTL_REG:
	case D2260_LDO19_MCTL_REG:
	case D2260_LDO20_MCTL_REG:
	case D2260_LDO21_MCTL_REG:
	case D2260_LDO22_MCTL_REG:
	case D2260_LDO23_MCTL_REG:
	case D2260_LDO24_MCTL_REG:
	case D2260_LDO25_MCTL_REG:
	case D2260_BUCK0_MCTL_REG:
	case D2260_BUCK1_MCTL_REG:
	case D2260_BUCK2_MCTL_REG:
	case D2260_BUCK3_MCTL_REG:
	case D2260_BUCK4_MCTL_REG:
	case D2260_BUCK5_MCTL_REG:
	case D2260_BUCK6_MCTL_REG:
	case D2260_BUCK_RF_MCTL_REG:
	case D2260_GPADC_MCTL_REG:
	case D2260_MISC_MCTL_REG:
	case D2260_VBUCK0_MCTL_RET_REG:
	case D2260_VBUCK0_MCTL_TUR_REG:
	case D2260_VBUCK1_MCTL_RET_REG:
	case D2260_VBUCK4_MCTL_RET_REG:
	case D2260_WAIT_CONT_REG:
	case D2260_ONKEY_CONT1_REG:
	case D2260_ONKEY_CONT2_REG:
	case D2260_POWER_CONT_REG:
	case D2260_VDDFAULT_REG:
	case D2260_BBAT_CONT_REG:
	case D2260_ADC_MAN_REG:
	case D2260_ADC_CONT_REG:
	case D2260_ADC_RES_L_REG:
	case D2260_ADC_RES_H_REG:
	case D2260_VBAT_RES_REG:
	case D2260_VDDOUT_MON_REG:
	case D2260_TEMP1_RES_REG:
	case D2260_TEMP1_HIGHP_REG:
	case D2260_TEMP1_HIGHN_REG:
	case D2260_TEMP1_LOW_REG:
	case D2260_T_OFFSET_REG:
	case D2260_VF_RES_REG:
	case D2260_VF_HIGH_REG:
	case D2260_VF_LOW_REG:
	case D2260_TEMP2_RES_REG:
	case D2260_TEMP2_HIGHP_REG:
	case D2260_TEMP2_HIGHN_REG:
	case D2260_TEMP2_LOW_REG:
	case D2260_TJUNC_RES_REG:
	case D2260_ADC_RES_AUTO1_REG:
	case D2260_ADC_RES_AUTO2_REG:
	case D2260_ADC_RES_AUTO3_REG:
	case D2260_COUNT_S_REG:
	case D2260_COUNT_MI_REG:
	case D2260_COUNT_H_REG:
	case D2260_COUNT_D_REG:
	case D2260_COUNT_MO_REG:
	case D2260_COUNT_Y_REG:
	case D2260_ALARM_S_REG:
	case D2260_ALARM_MI_REG:
	case D2260_ALARM_H_REG:
	case D2260_ALARM_D_REG:
	case D2260_ALARM_MO_REG:
	case D2260_ALARM_Y_REG:
	case D2260_CHIP_ID_REG:
	case D2260_CONFIG_ID_REG:
	case D2260_OTP_CONT_REG:
	case D2260_OSC_TRIM_REG:
	case D2260_GP_ID_0_REG:
	case D2260_GP_ID_1_REG:
	case D2260_GP_ID_2_REG:
	case D2260_GP_ID_3_REG:
	case D2260_GP_ID_4_REG:
	case D2260_GP_ID_5_REG:
	case D2260_BUCK0_CONF5_REG:
	case D2260_BUCK1_CONF5_REG:
	case D2260_BUCK4_CONF4_REG:
	case D2260_BUCK5_CONF4_REG:
		return true;
	default:
		return false;
	}
}

static bool d2260_reg_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case D2260_PAGE0_CON_REG:
	case D2260_PAGE1_CON_REG:
	case D2260_STATUS_A_REG:
	case D2260_STATUS_B_REG:
	case D2260_STATUS_C_REG:
	case D2260_EVENT_A_REG:
	case D2260_EVENT_B_REG:
	case D2260_EVENT_C_REG:
	case D2260_FAULT_LOG_REG:
	case D2260_IRQ_MASK_A_REG:
	case D2260_IRQ_MASK_B_REG:
	case D2260_IRQ_MASK_C_REG:
	case D2260_CONTROL_A_REG:
	case D2260_CONTROL_B_REG:
	case D2260_CONTROL_C_REG:
	case D2260_CONTROL_D_REG:
	case D2260_PD_DIS_REG:
	case D2260_INTERFACE_REG:
	case D2260_RESET_REG:
	case D2260_TA_GPIO_1_REG:
	case D2260_GPIO_0_NON_REG:
	case D2260_ID_0_1_REG:
	case D2260_ID_2_3_REG:
	case D2260_ID_4_5_REG:
	case D2260_ID_6_7_REG:
	case D2260_ID_8_9_REG:
	case D2260_ID_10_11_REG:
	case D2260_ID_12_13_REG:
	case D2260_ID_14_15_REG:
	case D2260_ID_16_17_REG:
	case D2260_ID_18_19_REG:
	case D2260_ID_20_21_REG:
	case D2260_SEQ_STATUS_REG:
	case D2260_SEQ_A_REG:
	case D2260_SEQ_B_REG:
	case D2260_SEQ_TIMER_REG:
	case D2260_BUCK0_CONF0_REG:
	case D2260_BUCK0_CONF1_REG:
	case D2260_BUCK0_CONF2_REG:
	case D2260_BUCK1_CONF0_REG:
	case D2260_BUCK1_CONF1_REG:
	case D2260_BUCK1_CONF2_REG:
	case D2260_BUCK2_CONF0_REG:
	case D2260_BUCK2_CONF1_REG:
	case D2260_BUCK3_CONF0_REG:
	case D2260_BUCK3_CONF1_REG:
	case D2260_BUCK4_CONF0_REG:
	case D2260_BUCK4_CONF1_REG:
	case D2260_BUCK5_CONF0_REG:
	case D2260_BUCK5_CONF1_REG:
	case D2260_BUCK6_CONF0_REG:
	case D2260_BUCK6_CONF1_REG:
	case D2260_BUCKRF_THR_REG:
	case D2260_BUCKRF_CONF_REG:
	case D2260_LDO1_REG:
	case D2260_LDO2_REG:
	case D2260_LDO3_REG:
	case D2260_LDO4_REG:
	case D2260_LDO5_REG:
	case D2260_LDO6_REG:
	case D2260_LDO7_REG:
	case D2260_LDO8_REG:
	case D2260_LDO9_REG:
	case D2260_LDO10_REG:
	case D2260_LDO11_REG:
	case D2260_LDO12_REG:
	case D2260_LDO13_REG:
	case D2260_LDO14_REG:
	case D2260_LDO15_REG:
	case D2260_LDO16_REG:
	case D2260_LDO17_REG:
	case D2260_LDO18_REG:
	case D2260_LDO19_REG:
	case D2260_LDO20_REG:
	case D2260_LDO21_REG:
	case D2260_LDO22_REG:
	case D2260_LDO23_REG:
	case D2260_LDO24_REG:
	case D2260_LDO25_REG:
	case D2260_SUPPLY_REG:
	case D2260_LDO1_MCTL_REG:
	case D2260_LDO2_MCTL_REG:
	case D2260_LDO3_MCTL_REG:
	case D2260_LDO4_MCTL_REG:
	case D2260_LDO5_MCTL_REG:
	case D2260_LDO6_MCTL_REG:
	case D2260_LDO7_MCTL_REG:
	case D2260_LDO8_MCTL_REG:
	case D2260_LDO9_MCTL_REG:
	case D2260_LDO10_MCTL_REG:
	case D2260_LDO11_MCTL_REG:
	case D2260_LDO12_MCTL_REG:
	case D2260_LDO13_MCTL_REG:
	case D2260_LDO14_MCTL_REG:
	case D2260_LDO15_MCTL_REG:
	case D2260_LDO16_MCTL_REG:
	case D2260_LDO17_MCTL_REG:
	case D2260_LDO18_MCTL_REG:
	case D2260_LDO19_MCTL_REG:
	case D2260_LDO20_MCTL_REG:
	case D2260_LDO21_MCTL_REG:
	case D2260_LDO22_MCTL_REG:
	case D2260_LDO23_MCTL_REG:
	case D2260_LDO24_MCTL_REG:
	case D2260_LDO25_MCTL_REG:
	case D2260_BUCK0_MCTL_REG:
	case D2260_BUCK1_MCTL_REG:
	case D2260_BUCK2_MCTL_REG:
	case D2260_BUCK3_MCTL_REG:
	case D2260_BUCK4_MCTL_REG:
	case D2260_BUCK5_MCTL_REG:
	case D2260_BUCK6_MCTL_REG:
	case D2260_BUCK_RF_MCTL_REG:
	case D2260_GPADC_MCTL_REG:
	case D2260_MISC_MCTL_REG:
	case D2260_VBUCK0_MCTL_RET_REG:
	case D2260_VBUCK0_MCTL_TUR_REG:
	case D2260_VBUCK1_MCTL_RET_REG:
	case D2260_VBUCK4_MCTL_RET_REG:
	case D2260_WAIT_CONT_REG:
	case D2260_ONKEY_CONT1_REG:
	case D2260_ONKEY_CONT2_REG:
	case D2260_POWER_CONT_REG:
	case D2260_VDDFAULT_REG:
	case D2260_BBAT_CONT_REG:
	case D2260_ADC_MAN_REG:
	case D2260_ADC_CONT_REG:
	case D2260_VDDOUT_MON_REG:
	case D2260_TEMP1_HIGHP_REG:
	case D2260_TEMP1_HIGHN_REG:
	case D2260_TEMP1_LOW_REG:
	case D2260_T_OFFSET_REG:
	case D2260_VF_HIGH_REG:
	case D2260_VF_LOW_REG:
	case D2260_TEMP2_HIGHP_REG:
	case D2260_TEMP2_HIGHN_REG:
	case D2260_TEMP2_LOW_REG:
	case D2260_COUNT_S_REG:
	case D2260_COUNT_MI_REG:
	case D2260_COUNT_H_REG:
	case D2260_COUNT_D_REG:
	case D2260_COUNT_MO_REG:
	case D2260_COUNT_Y_REG:
	case D2260_ALARM_S_REG:
	case D2260_ALARM_MI_REG:
	case D2260_ALARM_H_REG:
	case D2260_ALARM_D_REG:
	case D2260_ALARM_MO_REG:
	case D2260_ALARM_Y_REG:
	case D2260_OTP_CONT_REG:
	case D2260_OSC_TRIM_REG:
	case D2260_GP_ID_0_REG:
	case D2260_GP_ID_1_REG:
	case D2260_GP_ID_2_REG:
	case D2260_GP_ID_3_REG:
	case D2260_GP_ID_4_REG:
	case D2260_GP_ID_5_REG:
	case D2260_BUCK0_CONF5_REG:
	case D2260_BUCK1_CONF5_REG:
	case D2260_BUCK4_CONF4_REG:
	case D2260_BUCK5_CONF4_REG:
		return true;
	default:
		return false;
	}
}

static bool d2260_reg_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case D2260_CONTROL_C_REG:
	case D2260_STATUS_A_REG:
	case D2260_STATUS_B_REG:
	case D2260_STATUS_C_REG:
	case D2260_EVENT_A_REG:
	case D2260_EVENT_B_REG:
	case D2260_EVENT_C_REG:
	case D2260_FAULT_LOG_REG:
	case D2260_ADC_RES_L_REG:
	case D2260_ADC_RES_H_REG:
	case D2260_VBAT_RES_REG:
	case D2260_TEMP1_RES_REG:
	case D2260_VF_RES_REG:
	case D2260_TEMP2_RES_REG:
	case D2260_TJUNC_RES_REG:
	case D2260_ADC_RES_AUTO1_REG:
	case D2260_ADC_RES_AUTO2_REG:
	case D2260_ADC_RES_AUTO3_REG:
	case D2260_COUNT_S_REG:
	case D2260_COUNT_MI_REG:
	case D2260_COUNT_H_REG:
	case D2260_COUNT_D_REG:
	case D2260_COUNT_MO_REG:
	case D2260_COUNT_Y_REG:
	case D2260_ALARM_MI_REG:
		return true;
	default:
		return false;
	}
}

struct regmap_config d2260_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,

        .cache_type = REGCACHE_RBTREE,

        .max_register = D2260_PAGE1_CON_REG,
        .readable_reg = d2260_reg_readable,
        .writeable_reg = d2260_reg_writeable,
        .volatile_reg = d2260_reg_volatile,
};
EXPORT_SYMBOL_GPL(d2260_regmap_config);
#endif /* D2260_USING_RANGED_REGMAP */

static struct mfd_cell d2260_subdev_info[] = {
	{
		.name = "d2260-gpio",
	},
	{
		.name = "d2260-regulator",
		.id = 1,
	},
	{
		.name = "d2260-regulator",
		.id = 2,
	},
	{
		.name = "d2260-regulator",
		.id = 3,
	},
	{
		.name = "d2260-regulator",
		.id = 4,
	},
	{
		.name = "d2260-regulator",
		.id = 5,
	},
	{
		.name = "d2260-regulator",
		.id = 6,
	},
	{
		.name = "d2260-regulator",
		.id = 7,
	},
	{
		.name = "d2260-regulator",
		.id = 8,
	},
	{
		.name = "d2260-regulator",
		.id = 9,
	},
	{
		.name = "d2260-regulator",
		.id = 10,
	},
	{
		.name = "d2260-regulator",
		.id = 11,
	},
	{
		.name = "d2260-regulator",
		.id = 12,
	},
	{
		.name = "d2260-regulator",
		.id = 13,
	},
	{
		.name = "d2260-regulator",
		.id = 14,
	},
	{
		.name = "d2260-regulator",
		.id = 15,
	},
	{
		.name = "d2260-regulator",
		.id = 16,
	},
	{
		.name = "d2260-regulator",
		.id = 17,
	},
	{
		.name = "d2260-regulator",
		.id = 18,
	},
	{
		.name = "d2260-regulator",
		.id = 19,
	},
	{
		.name = "d2260-regulator",
		.id = 20,
	},
	{
		.name = "d2260-regulator",
		.id = 21,
	},
	{
		.name = "d2260-regulator",
		.id = 22,
	},
	{
		.name = "d2260-regulator",
		.id = 23,
	},
	{
		.name = "d2260-regulator",
		.id = 24,
	},
	{
		.name = "d2260-regulator",
		.id = 25,
	},
	{
		.name = "d2260-regulator",
		.id = 26,
	},
	{
		.name = "d2260-regulator",
		.id = 27,
	},
	{
		.name = "d2260-regulator",
		.id = 28,
	},
	{
		.name = "d2260-regulator",
		.id = 29,
	},
	{
		.name = "d2260-regulator",
		.id = 30,
	},
	{
		.name = "d2260-regulator",
		.id = 31,
	},
	{
		.name = "d2260-regulator",
		.id = 32,
	},
	{
		.name = "d2260-regulator",
		.id = 33,
	},
	{
		.name = "d2260-regulator",
		.id = 34,
	},
	{
		.name = "d2260-regulator",
		.id = 35,
	},
#ifdef D2260_GPO_ENABLE
	{
		.name = "d2260-regulator",
		.id = 36,
	},
	{
		.name = "d2260-regulator",
		.id = 37,
	},
	{
		.name = "d2260-regulator",
		.id = 38,
	},
#endif
	{
		.name = "d2260-onkey",
	},
	{
		.name = "d2260-rtc",
	},
	{
		.name = "d2260-hwmon",
	},
	{
		.name = "d2260-bat",
	},
	{
		.name = "d2260-watchdog",
	},
};


#define D2260_SUPPORT_I2C_HIGH_SPEED

static int d2260_page_write_mode(struct d2260 *d2260, int onoff)
{
	int ret = 0;

	if (onoff == 0)
		d2260_reg_write(d2260, D2260_CONTROL_B_REG, 0x24);
	else
		d2260_reg_write(d2260, D2260_CONTROL_B_REG, 0x4);

	return ret;
}

#ifdef D2260_USING_RANGED_REGMAP
inline unsigned int d2260_to_range_reg(u16 reg)
{
	return reg + D2260_MAPPING_BASE;
}

static int d2260_block_read(struct d2260 *d2260, u16 reg, int num_regs,
	unsigned int *dest)
{
	return regmap_bulk_read(d2260->regmap, d2260_to_range_reg(reg),
		dest, num_regs);
}

static int d2260_read(struct d2260 *d2260, u16 reg, unsigned int *dest)
{
	return regmap_read(d2260->regmap, d2260_to_range_reg(reg), dest);
}

static int d2260_block_write(struct d2260 *d2260, u16 reg, int num_regs,
	unsigned int * src)
{
	return regmap_bulk_write(d2260->regmap, d2260_to_range_reg(reg),
						src, num_regs);
}

static int d2260_write(struct d2260 *d2260, u16 reg, unsigned int *src)
{
	return regmap_write(d2260->regmap, d2260_to_range_reg(reg), *src);
}

int d2260_update_bits(struct d2260 *d2260, u16 reg, u8 mask, u8 val)
{
	return regmap_update_bits(d2260->regmap, d2260_to_range_reg(reg),
				  mask, val);
}
EXPORT_SYMBOL_GPL(d2260_update_bits);

int d2260_clear_bits(struct d2260 * const d2260, u16 const reg, u8 const mask)
{
	return d2260_update_bits(d2260, reg, mask, 0x00);
}
EXPORT_SYMBOL_GPL(d2260_clear_bits);

int d2260_set_bits(struct d2260 * const d2260, u16 const reg, u8 const mask)
{
	return d2260_update_bits(d2260, reg, mask, 0xFF);
}
EXPORT_SYMBOL_GPL(d2260_set_bits);

int d2260_reg_read(struct d2260 * const d2260, u16 const reg, u8 *dest)
{
	unsigned int data;
	int err;

	err = d2260_read(d2260, reg, &data);
	*dest = data;
	if (err != 0)
		dev_err(d2260->dev, "read from reg R%d failed\n", reg);
	return err;
}
EXPORT_SYMBOL_GPL(d2260_reg_read);

int d2260_reg_write(struct d2260 * const d2260, u16 const reg, u8 const val)
{
	int ret;
	unsigned int data = val;

	ret = d2260_write(d2260, reg, &data);
	if (ret != 0)
		dev_err(d2260->dev, "write to reg R%d failed\n", reg);
	return ret;
}
EXPORT_SYMBOL_GPL(d2260_reg_write);

int d2260_group_read(struct d2260 * const d2260, u16 const start_reg,
			u8 const regs, unsigned int * const dest)
{
	int err = 0, ret = 0;

	ret = d2260_page_write_mode(d2260, 1);
	if (ret < 0)
		dev_err(d2260->dev, "Page write mode fail \n");

	err = d2260_block_read(d2260, start_reg, regs, dest);
	if (err != 0)
		dev_err(d2260->dev, "block read starting from R%d failed\n", start_reg);

	ret = d2260_page_write_mode(d2260, 0);
	if (ret < 0)
		dev_err(d2260->dev, "Page write mode return fail \n");

	return err;
}
EXPORT_SYMBOL_GPL(d2260_group_read);

int d2260_group_write(struct d2260 * const d2260, u16 const start_reg,
			u8 const regs, unsigned int * const src)
{
	int err = 0, ret = 0;

	ret = d2260_page_write_mode(d2260, 1);
	if (ret < 0)
		dev_err(d2260->dev, "Page write mode fail \n");

	err = d2260_block_write(d2260, start_reg, regs, src);

	if (err < 0)
		dev_err(d2260->dev, "block write at R%d .. R%d failed\n",
			start_reg, start_reg + regs - 1);

	ret = d2260_page_write_mode(d2260, 0);
	if (ret < 0)
		dev_err(d2260->dev, "Page write mode return fail \n");

	return err;
}
EXPORT_SYMBOL_GPL(d2260_group_write);

#else
static int d2260_block_read(struct d2260 *d2260, u8 reg, int num_regs, u8 *dest)
{
	return regmap_bulk_read(d2260->regmap, reg, dest, num_regs);
}

static int d2260_read(struct d2260 *d2260, u8 reg, unsigned int *dest)
{
	return regmap_read(d2260->regmap, reg, dest);
}

static int d2260_block_write(struct d2260 *d2260, u8 reg, int num_regs, u8 *src)
{
	int i, ret = 0;

	for (i = 0; i < num_regs; i++) {
		ret = regmap_write(d2260->regmap, reg + i, src[i]);
		if (ret < 0)
			break;
	}
	if (ret < 0)
		dev_err(d2260->dev, "block write at R%d failed\n", reg + i);

	return ret;
}

static int d2260_write(struct d2260 *d2260, u8 reg, unsigned int *src)
{
	return regmap_write(d2260->regmap, reg, *src);
}

int d2260_clear_bits(struct d2260 * const d2260, u8 const reg, u8 const mask)
{
	return regmap_update_bits(d2260->regmap, reg, mask, 0x00);
}
EXPORT_SYMBOL_GPL(d2260_clear_bits);

int d2260_set_bits(struct d2260 * const d2260, u8 const reg, u8 const mask)
{
	return regmap_update_bits(d2260->regmap, reg, mask, 0xFF);
}
EXPORT_SYMBOL_GPL(d2260_set_bits);

int d2260_update_bits(struct d2260 * const d2260, u8 const reg,
		u8 const mask, u8 const bits)
{
	return regmap_update_bits(d2260->regmap, reg, mask, bits);
}
EXPORT_SYMBOL_GPL(d2260_update_bits);

int d2260_reg_read(struct d2260 * const d2260, u8 const reg, u8 *dest)
{
	unsigned int data;
	int err;

	err = d2260_read(d2260, reg, &data);
	*dest = data;
	if (err != 0)
		dev_err(d2260->dev, "read from reg R%d failed\n", reg);
	return err;
}
EXPORT_SYMBOL_GPL(d2260_reg_read);

int d2260_reg_write(struct d2260 * const d2260, u8 const reg, u8 const val)
{
	int ret;
	unsigned int data = val;

	ret = d2260_write(d2260, reg, &data);
	if (ret != 0)
		dev_err(d2260->dev, "write to reg R%d failed\n", reg);
	return ret;
}
EXPORT_SYMBOL_GPL(d2260_reg_write);

int d2260_group_read(struct d2260 * const d2260, u8 const start_reg,
		u8 const regs, u8 * const dest)
{
	int err = 0;

	err = d2260_block_read(d2260, start_reg, regs, dest);
	if (err != 0)
		dev_err(d2260->dev, "block read starting from R%d failed\n", start_reg);
	return err;
}
EXPORT_SYMBOL_GPL(d2260_group_read);

int d2260_group_write(struct d2260 * const d2260, u8 const start_reg,
		u8 const regs, u8 * const src)
{
	int err = 0;

	err = d2260_block_write(d2260, start_reg, regs, src);
	if (err < 0)
		dev_err(d2260->dev, "block write at R%d .. R%d failed\n",
			start_reg, start_reg + regs - 1);
	return err;
}
EXPORT_SYMBOL_GPL(d2260_group_write);
#endif /* D2260_USING_RANGED_REGMAP */

#ifdef CONFIG_PROC_FS
static int d2260_ioctl_open(struct inode *inode, struct file *file)
{
	dlg_info("%s\n", __func__);
#if 1
	file->private_data = PDE_DATA(inode); /*PDE(inode)->data;*/
#else
	file->private_data = PDE(inode)->data; /* 3.8 */
#endif
	return 0;
}

int d2260_ioctl_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 * d2260_ioctl
 */
static long d2260_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct d2260 *d2260 =  file->private_data;
		pmu_reg reg;
		int ret = 0;
		u8 reg_val;

		if (!d2260)
			return -ENOTTY;

		switch (cmd) {

			case D2260_IOCTL_READ_REG:
			if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
				return -EFAULT;

			ret = d2260_reg_read(d2260, reg.reg, &reg_val);
			reg.val = (unsigned short)reg_val;

			if (copy_to_user((pmu_reg *)arg, &reg, sizeof(pmu_reg)) != 0)
				return -EFAULT;
			break;

		case D2260_IOCTL_WRITE_REG:
			if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
				return -EFAULT;

			d2260_reg_write(d2260, reg.reg, reg.val);
			break;

		default:
			dlg_err("%s: unsupported cmd\n", __func__);
			ret = -ENOTTY;
		}

		return 0;
}

#define MAX_USER_INPUT_LEN      100
#define MAX_REGS_READ_WRITE     10

enum pmu_debug_ops {
	PMUDBG_READ_REG = 0UL,
	PMUDBG_WRITE_REG,
};

struct pmu_debug {
	int read_write;
	int len;
	int addr;
#ifdef D2260_USING_RANGED_REGMAP
	unsigned int val[MAX_REGS_READ_WRITE];
#else
	u8 val[MAX_REGS_READ_WRITE];
#endif
};

/*
 * d2260_dbg_usage
 */
static void d2260_dbg_usage(void)
{
	printk(KERN_INFO "### Usage:\n");
	printk(KERN_INFO "	Read a register	 : echo 0x08 > /proc/pmu0 \n");
	printk(KERN_INFO "	Read multiple regs : echo 0x08 -c 10 > /proc/pmu0 \n");
	printk(KERN_INFO "	Write multiple regs: echo 0x08 0xFF 0xFF > /proc/pmu0 \n");
	printk(KERN_INFO "	Write single reg	 : echo 0x08 0xFF > /proc/pmu0 \n");
	printk(KERN_INFO "	Max number of regs in single write is :%d\n",
					MAX_REGS_READ_WRITE);
}


/*
 * d2260_dbg_parse_args
 */
static int d2260_dbg_parse_args(char *cmd, struct pmu_debug *dbg)
{
	char *tok;                 /* used to separate tokens             */
	const char ct[] = " \t";   /* space or tab delimits the tokens    */
	bool count_flag = false;   /* whether -c option is present or not */
	int tok_count = 0;         /* total number of tokens parsed       */
	int i = 0;

	dbg->len = 0;

	/* parse the input string */
	while ((tok = strsep(&cmd, ct)) != NULL) {
		dlg_info("token: %s\n", tok);

		/* first token is always address */
		if (tok_count == 0) {
			sscanf(tok, "%x", &dbg->addr);
		} else if (strnicmp(tok, "-c", 2) == 0) {
			/* the next token will be number of regs to read */
			tok = strsep(&cmd, ct);
			if (tok == NULL)
				return -EINVAL;

			tok_count++;
			sscanf(tok, "%d", &dbg->len);
			count_flag = true;
			break;
		} else {
			int val;

			/* this is a value to be written to the pmu register */
			sscanf(tok, "%x", &val);
			if (i < MAX_REGS_READ_WRITE) {
				dbg->val[i] = val;
				i++;
			}
		}

		tok_count++;
	}

	/* decide whether it is a read or write operation based on the
	 * value of tok_count and count_flag.
	 * tok_count = 0: no inputs, invalid case.
	 * tok_count = 1: only reg address is given, so do a read.
	 * tok_count > 1, count_flag = false: reg address and atleast one
	 *     value is present, so do a write operation.
	 * tok_count > 1, count_flag = true: to a multiple reg read operation.
	 */
	switch (tok_count) {
		case 0:
			return -EINVAL;
		case 1:
			dbg->read_write = PMUDBG_READ_REG;
			dbg->len = 1;
			break;
		default:
			if (count_flag == true) {
				dbg->read_write = PMUDBG_READ_REG;
			} else {
				dbg->read_write = PMUDBG_WRITE_REG;
				dbg->len = i;
		}
	}

	return 0;
}

/*
 * d2260_ioctl_write
 */
static ssize_t d2260_ioctl_write(struct file *file, const char __user *buffer,
	size_t len, loff_t *offset)
{
	struct d2260 *d2260 = file->private_data;
	struct pmu_debug dbg;
	char cmd[MAX_USER_INPUT_LEN];
	int ret, i;

	dlg_info("%s\n", __func__);

	if (!d2260) {
		dlg_err("%s: driver not initialized\n", __func__);
		return -EINVAL;
	}

	if (len > MAX_USER_INPUT_LEN)
		len = MAX_USER_INPUT_LEN;

	if (copy_from_user(cmd, buffer, len)) {
		dlg_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	/* chop of '\n' introduced by echo at the end of the input */
	if (cmd[len - 1] == '\n')
		cmd[len - 1] = '\0';

	if (d2260_dbg_parse_args(cmd, &dbg) < 0) {
		d2260_dbg_usage();
		return -EINVAL;
	}

	dlg_info("operation: %s\n", (dbg.read_write == PMUDBG_READ_REG) ?
		"read" : "write");
	dlg_info("address  : 0x%x\n", dbg.addr);
	dlg_info("length   : %d\n", dbg.len);

	if (dbg.read_write == PMUDBG_READ_REG) {
		ret = d2260_block_read(d2260, dbg.addr, dbg.len, dbg.val);
		if (ret < 0) {
			dlg_err("%s: pmu reg read failed\n", __func__);
			return -EFAULT;
		}

		for (i = 0; i < dbg.len; i++, dbg.addr++)
			dlg_info("[%x] = 0x%02x\n", dbg.addr,
				dbg.val[i]);
	} else {
		ret = d2260_block_write(d2260, dbg.addr, dbg.len, dbg.val);
		if (ret < 0) {
			dlg_err("%s: pmu reg write failed\n", __func__);
			return -EFAULT;
		}
	}

	*offset += len;

	return len;
}

static const struct file_operations d2260_pmu_ops = {
	.open = d2260_ioctl_open,
	.unlocked_ioctl = d2260_ioctl,
	.write = d2260_ioctl_write,
	.release = d2260_ioctl_release,
	.owner = THIS_MODULE,
};

/*
 * d2260_debug_proc_init
 */
void d2260_debug_proc_init(struct d2260 *d2260)
{
	struct proc_dir_entry *entry;

	disable_irq(d2260->chip_irq);
	entry = proc_create_data(D2260_PROC_NAME, S_IRWXUGO, NULL,
				&d2260_pmu_ops, d2260);
	enable_irq(d2260->chip_irq);
}

/*
 * d2260_debug_proc_exit
 */
void d2260_debug_proc_exit(void)
{
	/* disable_irq(client->irq); */
	remove_proc_entry(D2260_PROC_NAME, NULL);
	/* enable_irq(client->irq); */
}
#endif /* CONFIG_PROC_FS */

static irqreturn_t d2260_system_event_handler(int irq, void *data)
{
	/* todo DLG export the event?? */
	return IRQ_HANDLED;
}

static void d2260_system_event_init(struct d2260 *d2260)
{
	int ret;

	ret = d2260_request_irq(d2260, D2260_IRQ_EVDD_MON, "VDD MON",
				d2260_system_event_handler, d2260);
}

static void d2260_set_mctl_enable(struct d2260 *d2260)
{
	u8 reg_val;
	d2260_reg_read(d2260, D2260_POWER_CONT_REG, &reg_val);
	reg_val |= D2260_MCTRL_EN_MASK;
	d2260_reg_write(d2260, D2260_POWER_CONT_REG, reg_val);
	d2260->mctl_status = 1;
}

static void d2260_set_mctl_status(struct d2260 *d2260)
{
	u8 reg_val;
	d2260_reg_read(d2260, D2260_POWER_CONT_REG, &reg_val);
    if ((reg_val & D2260_MCTRL_EN_MASK) == 0x1)
	    d2260->mctl_status = 1;
    else
	    d2260->mctl_status = 0;

    printk("[%s] mctl_status [%d]\n", __func__, d2260->mctl_status);
}

void d2260_set_bbat_chrg_current(void)
{
	u8 reg_val;

	if(g_d2260 == NULL){
		printk("d2260_set_bbat_chrg_current => Global d2260 is not set\n");
		return;
	}
	d2260_reg_write(g_d2260, D2260_BBAT_CONT_REG, 0x1F);
	d2260_reg_read(g_d2260, D2260_BBAT_CONT_REG, &reg_val);

	printk("suspend_d2260_set_bbat_chrg_current = 0x%02X\n",reg_val);
}

int d2260_device_init(struct d2260 *d2260, u8 chip_id)
{
	struct d2260_platform_data *pdata = d2260->dev->platform_data;
	int ret = 0;
	u8 reg_val;

	DIALOG_DEBUG(d2260->dev,
		"D2260 Driver : built at %s on %s\n",__TIME__,__DATE__);

	mutex_init(&d2260->lock);

	if (pdata && pdata->init != NULL)
		pdata->init(d2260);

	d2260->chip_irq = chip_id;
	d2260->pdata = pdata;

#ifndef MCTL_ENABLE_IN_SMS
	d2260_reg_write(d2260, D2260_GPADC_MCTL_REG, 0x55);
#endif

#ifdef D2260_SUPPORT_I2C_HIGH_SPEED
	d2260_set_bits(d2260, D2260_CONTROL_B_REG, D2260_I2C_SPEED_MASK);
#else
	d2260_clear_bits(d2260, D2260_CONTROL_B_REG, D2260_I2C_SPEED_MASK);
#endif

#ifndef MCTL_ENABLE_IN_SMS
	d2260_reg_write(d2260, D2260_PD_DIS_REG, 0x0);
	d2260_reg_write(d2260, D2260_BBAT_CONT_REG, 0xFF);
	d2260_set_bits(d2260,  D2260_SUPPLY_REG, D2260_BBCHG_EN_MASK);
	d2260_set_bits(d2260,  D2260_SUPPLY_REG, D2260_OUT2_32K_EN_MASK);
#endif

	ret = d2260_irq_init(d2260);
	if (ret < 0)
		goto err;

	if (pdata && pdata->init) {
		ret = pdata->init(d2260);
		if (ret != 0) {
			dev_err(d2260->dev, "Platform init() failed: %d\n", ret);
			goto err_irq;
		}
	}

	d2260_system_event_init(d2260);
#if MCTL_ENABLE_IN_SMS
	d2260_set_mctl_status(d2260);
#else
	d2260_set_mctl_enable(d2260);
	d2260_reg_write(d2260, D2260_MISC_MCTL_REG, 0x0F);
#endif

	ret = mfd_add_devices(d2260->dev, -1, d2260_subdev_info,
			      ARRAY_SIZE(d2260_subdev_info), NULL, 0, NULL);
	if (ret) {
		dev_err(d2260->dev, "mfd_add_devices failed: %d\n", ret);
		goto err;
	}

	g_d2260 = d2260;

	d2260_reg_write(d2260, D2260_BBAT_CONT_REG, 0xFF);
	d2260_reg_read(d2260, D2260_BBAT_CONT_REG, &reg_val);
	printk("probe_d2260_set_bbat_chrg_current = 0x%02X\n",reg_val);

	//d2260_debug_proc_init(d2260);


	return 0;

err_irq:
	d2260_irq_exit(d2260);
err:
	DIALOG_DEBUG(d2260->dev,"\n\n%s() failed ! err=%d\n\n",__FUNCTION__,ret);
	return ret;
}
EXPORT_SYMBOL_GPL(d2260_device_init);

void d2260_device_exit(struct d2260 *d2260)
{
#ifdef CONFIG_PROC_FS
	d2260_debug_proc_exit();
#endif
	mfd_remove_devices(d2260->dev);

	d2260_irq_exit(d2260);
}
EXPORT_SYMBOL_GPL(d2260_device_exit);

#ifdef CONFIG_PM_RUNTIME
static int d2260_runtime_poweroff(struct device *dev)
{
	struct d2260 *d2260 = dev_get_drvdata(dev);
	int ret;

	DIALOG_DEBUG(d2260->dev,"%s\n", __func__);
	if (d2260 == NULL) {
		dev_err(d2260->dev, "%s. Platform or Device data is NULL\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int d2260_runtime_resume(struct device *dev)
{
    return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int d2260_sleep_poweroff(struct device *dev)
{
	struct d2260 *d2260 = dev_get_drvdata(dev);
	int ret;

	DIALOG_DEBUG(d2260->dev,"%s\n", __func__);

	if (d2260 == NULL) {
		dev_err(d2260->dev, "%s. Platform or Device data is NULL\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int d2260_sleep_resume(struct device *dev)
{
	return 0;
}
#endif

struct dev_pm_ops d2260_pm_ops = {
	SET_RUNTIME_PM_OPS(d2260_runtime_poweroff,
			d2260_runtime_resume,
			NULL)
        SET_SYSTEM_SLEEP_PM_OPS(d2260_sleep_poweroff,
			d2260_sleep_resume)
};
