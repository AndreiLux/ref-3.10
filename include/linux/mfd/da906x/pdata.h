/*
 * Platform configuration options for DA906x
 *
 * Copyright 2012 Dialog Semiconductor Ltd.
 *
 * Author: Michal Hajduk <michal.hajduk@diasemi.com>
 * Author: Krystian Garbaciak <krystian.garbaciak@diasemi.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_DA906X_PDATA_H__
#define __MFD_DA906X_PDATA_H__

#include <linux/regulator/machine.h>


/* DA9063 regulator IDs */
enum {
	/* BUCKs */
	DA9063_ID_BCORE1,
	DA9063_ID_BCORE2,
	DA9063_ID_BPRO,
	DA9063_ID_BMEM,
	DA9063_ID_BIO,
	DA9063_ID_BPERI,

	/* LDOs */
	DA9063_ID_LDO1,
	DA9063_ID_LDO2,
	DA9063_ID_LDO3,
	DA9063_ID_LDO4,
	DA9063_ID_LDO5,
	DA9063_ID_LDO6,
	DA9063_ID_LDO7,
	DA9063_ID_LDO8,
	DA9063_ID_LDO9,
	DA9063_ID_LDO10,
	DA9063_ID_LDO11,

	/* RTC internal oscilator switch */
	DA9063_ID_32K_OUT,
};

/* CoPMIC IDs */
enum {
	DA9210_ID_BUCK = 0x100,
};

/* Regulator flags. Nominal number of power phases for DA9210 buck to be used.
   If not defined, default value from register will be used. 8 phases are
   reserved for 2 DA9210 bucks merged.
   NOTE: Keep DA9210 current limits adjusted to nominal phase number. */
#define DA9210_FLG_1_PHASE			0x1
#define DA9210_FLG_2_PHASES			0x2
#define DA9210_FLG_4_PHASES			0x4
#define DA9210_FLG_8_PHASES			0x8
#define DA9210_FLG_PHASES_MASK		0xF

/* Regulator flags. DA9210 uses dedicated DVC interface for voltage control. */
#define DA9210_FLG_DVC_IF			0x20

/* Regulator flags. Step size selection for DVC interface. */
#define DA9210_FLG_DVC_STEP_10MV	0x00
#define DA9210_FLG_DVC_STEP_20MV	0x10

/* Regulators platform data structure */
struct da906x_regulator_data {
	int				id;
	struct regulator_init_data	*initdata;
	unsigned long			flags;
	struct device_node *of_node;

	/* For DVC interface, base and max voltage. */
	int				dvc_base_uv;
	int				dvc_max_uv;
};

struct da906x_regulators_pdata {
	int 			id;
	unsigned			n_regulators;
	struct da906x_regulator_data	*regulator_data;
	struct da906x_regulator_info *rinfo;
	struct device_node *of_node;
};

/* LED flags */
enum {
	DA906X_GPIO11_LED,
	DA906X_GPIO14_LED,
	DA906X_GPIO15_LED,

	DA906X_LED_NUM
};
#define DA906X_LED_ID_MASK				0x3
#define DA906X_LED_HIGH_LEVEL_ACTIVE	0x0
#define DA906X_LED_LOW_LEVEL_ACTIVE		0x4

/* DA906x platform data */
struct da906x_pdata {
	int				(*init)(struct da906x *da906x);
	int				irq_base;
	int				gpio_base;
	int				num_gpio;
	int				da9210;
	unsigned short	addr;
	struct da906x_regulators_pdata	*regulators_pdata;
	struct led_platform_data	*leds_pdata;
};

#endif	/* __MFD_DA906X_PDATA_H__ */
