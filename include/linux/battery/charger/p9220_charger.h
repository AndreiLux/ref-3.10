/*
 * p9220_charger.h
 * Samsung p9220 Charger Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __p9220_CHARGER_H
#define __p9220_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/battery/sec_charging_common.h>

/* REGISTER MAPS */
#define INT_STATUS_H_REG			0x01
#define INT_STATUS_L_REG			0x02
#define INT_ENABLE_H_REG			0x03
#define INT_ENABLE_L_REG			0x04
#define CHG_STATUS_REG				0x05
#define EPT_REG						0x06
#define ADC_VRECT_REG				0x07
#define ADC_IOUT_REG				0x08
#define ADC_VOUT_REG				0x09
#define ADC_DIE_TEMP_REG			0x0A
#define ADC_ALLGN_X_REG				0x0B
#define ADC_ALIGN_Y_REG				0x0C
#define OP_FREQ_REG					0x0D
#define VOUT_SET_REG				0x0E  //need check
#define ILIM_SET_REG				0x0F  //need check
#define VRECT_SET_REG				0x0F  //need check
#define RXID_0_REG					0x10
#define RXID_1_REG					0x11
#define RXID_2_REG					0x12
#define RXID_3_REG					0x13
#define RXID_4_REG					0x14
#define RXID_5_REG					0x15
#define FW_MAJOR_REV_REG			0x16
#define FW_MAIN_REV_REG				0x17
#define CHIP_ID_MAJOR_1_REG			0x0070
#define CHIP_ID_MAJOR_0_REG			0x0074
#define CHIP_ID_MINOR_REG			0x0078
#define LDO_EN_REG					0x301C

/* WPC HEADER */
#define p9220_EPT_HEADER_EPT					0x02
#define p9220_EPT_HEADER_CS100					0x05

/* END POWER TRANSFER CODES IN WPC */
#define p9220_EPT_CODE_UNKOWN					0x00
#define p9220_EPT_CODE_CHARGE_COMPLETE			0x01
#define p9220_EPT_CODE_INTERNAL_FAULT			0x02
#define p9220_EPT_CODE_OVER_TEMPERATURE		0x03
#define p9220_EPT_CODE_OVER_VOLTAGE			0x04
#define p9220_EPT_CODE_OVER_CURRENT			0x05
#define p9220_EPT_CODE_BATTERY_FAILURE			0x06
#define p9220_EPT_CODE_RECONFIGURE				0x07
#define p9220_EPT_CODE_NO_RESPONSE				0x08

#define p9220_POWER_MODE_MASK					(0x1 << 0)
#define p9220_SEND_USER_PKT_DONE_MASK			(0x1 << 7)
#define p9220_SEND_USER_PKT_ERR_MASK			(0x3 << 5)
#define p9220_SEND_ALIGN_MASK					(0x1 << 3)
#define p9220_SEND_EPT_CC_MASK					(0x1 << 0)
#define p9220_SEND_EOC_MASK					(0x1 << 0)

#define p9220_PTK_ERR_NO_ERR					0x00
#define p9220_PTK_ERR_ERR						0x01
#define p9220_PTK_ERR_ILLEGAL_HD				0x02
#define p9220_PTK_ERR_NO_DEF					0x03

struct p9220_charger_platform_data {
	int irq_gpio;
	int irq_base;
	int tsb_gpio;
	int cs100_status;
	int pad_mode;
	int wireless_cc_cv;
	int siop_level;
	bool default_voreg;
	char *wireless_charger_name;
};

#define p9220_charger_platform_data_t \
	struct p9220_charger_platform_data

struct p9220_charger_data {
	struct i2c_client				*client;
	struct device           *dev;
	p9220_charger_platform_data_t *pdata;
	struct mutex io_lock;
	const struct firmware *firm_data_bin;

	struct power_supply psy_chg;
	struct wake_lock wpc_wake_lock;
	struct workqueue_struct *wqueue;
	struct work_struct	wcin_work;
	struct delayed_work	wpc_work;
	struct delayed_work	isr_work;

	int wpc_irq;
	int irq_base;
	int irq_gpio;

	int wpc_state;
};

enum {
    p9220_EVENT_IRQ = 0,
    p9220_IRQS_NR,
};

enum {
    p9220_PAD_MODE_NONE = 0,
    p9220_PAD_MODE_WPC,
    p9220_PAD_MODE_PMA,
};

#endif /* __p9220_CHARGER_H */
