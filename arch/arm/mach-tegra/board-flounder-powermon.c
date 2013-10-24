/*
 * arch/arm/mach-tegra/board-flounder-powermon.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/i2c.h>
#include <linux/ina219.h>
#include <linux/platform_data/ina230.h>
#include <linux/i2c/pca954x.h>

#include "board.h"
#include "board-flounder.h"
#include "tegra-board-id.h"

#define PRECISION_MULTIPLIER_FLOUNDER	1000
#define FLOUNDER_POWER_REWORKED_CONFIG	0x10
#define VDD_SOC_SD1_REWORKED		10
#define VDD_CPU_BUCKCPU_REWORKED	10
#define VDD_1V35_SD2_REWORKED		10

#define AVG_32_SAMPLES (4 << 9)

/* AVG is specified from platform data */
#define INA230_CONT_CONFIG	(AVG_32_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_CONT_MODE)
#define INA230_TRIG_CONFIG	(AVG_32_SAMPLES | INA230_VBUS_CT | \
				INA230_VSH_CT | INA230_TRIG_MODE)

/* unused rail */
enum {
	UNUSED_RAIL,
};

/* following rails are present on Flounder */
/* rails on i2c2_1 */
enum {
	VDD_SYS_BAT,
	VDD_RTC_LDO5,
	VDD_3V3A_SMPS1_2,
	VDD_SOC_SMPS1_2,
	VDD_SYS_BUCKCPU,
	VDD_CPU_BUCKCPU,
	VDD_1V8A_SMPS3,
	VDD_1V8B_SMPS9,
	VDD_GPU_BUCKGPU,
	VDD_1V35_SMPS6,
	VDD_3V3A_SMPS1_2_2,
	VDD_3V3B_SMPS9,
	VDD_LCD_1V8B_DIS,
	VDD_1V05_SMPS8,
};

/* rails on i2c2_2 */
enum {
	VDD_SYS_BL,
	AVDD_1V05_LDO2,
};

/* following rails are present on Flounder A01 and onward boards */
/* rails on i2c2_1 */
enum {
	FLOUNDER_A01_VDD_SYS_BAT,
	FLOUNDER_A01_VDD_RTC_LDO3,
	FLOUNDER_A01_VDD_SYS_BUCKSOC,
	FLOUNDER_A01_VDD_SOC_SD1,
	FLOUNDER_A01_VDD_SYS_BUCKCPU,
	FLOUNDER_A01_VDD_CPU_BUCKCPU,
	FLOUNDER_A01_VDD_1V8_SD5,
	FLOUNDER_A01_VDD_3V3A_LDO1_6,
	FLOUNDER_A01_VDD_DIS_3V3_LCD,
	FLOUNDER_A01_VDD_1V35_SD2,
	FLOUNDER_A01_VDD_SYS_BUCKGPU,
	FLOUNDER_A01_VDD_LCD_1V8B_DIS,
	FLOUNDER_A01_VDD_1V05_LDO0,
};

/* rails on i2c2_2 */
enum {
	FLOUNDER_A01_VDD_1V05_SD4,
	FLOUNDER_A01_VDD_1V8A_LDO2_5_7,
	FLOUNDER_A01_VDD_SYS_BL,
};

static struct ina219_platform_data power_mon_info_0[] = {
	/* All unused INA219 devices use below data */
	[UNUSED_RAIL] = {
		.calibration_data = 0x369c,
		.power_lsb = 3.051979018 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "unused_rail",
		.divisor = 20,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},
};

/* following are power monitor parameters for Flounder */
static struct ina230_platform_data power_mon_info_1[] = {
	[VDD_SYS_BAT] = {
		.calibration_data  = 0x1366,
		.power_lsb = 2.577527185 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BAT",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_RTC_LDO5] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_RTC_LDO5",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_3V3A_SMPS1_2] = {
		.calibration_data  = 0x4759,
		.power_lsb = 1.401587736 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_3V3A_SMPS1_2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_SOC_SMPS1_2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SOC_SMPS1_2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_SYS_BUCKCPU] = {
		.calibration_data  = 0x1AC5,
		.power_lsb = 1.867795126 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_CPU_BUCKCPU] = {
		.calibration_data  = 0x2ECF,
		.power_lsb = 10.68179922 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_CPU_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_1V8A_SMPS3] = {
		.calibration_data  = 0x5BA7,
		.power_lsb = 0.545539786 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V8A_SMPS3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_1V8B_SMPS9] = {
		.calibration_data  = 0x50B4,
		.power_lsb = 0.309777348 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V8B_SMPS9",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_GPU_BUCKGPU] = {
		.calibration_data  = 0x369C,
		.power_lsb = 9.155937053 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_GPU_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_1V35_SMPS6] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V35_SMPS6",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	/* following rail is duplicate of VDD_3V3A_SMPS1_2 hence mark unused */
	[VDD_3V3A_SMPS1_2_2] = {
		.calibration_data  = 0x4759,
		.power_lsb = 1.401587736 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "unused_rail",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_3V3B_SMPS9] = {
		.calibration_data  = 0x3269,
		.power_lsb = 0.198372724 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_3V3B_SMPS9",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_LCD_1V8B_DIS] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.039063692 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_LCD_1V8B_DIS",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[VDD_1V05_SMPS8] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.130212307 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V05_SMPS8",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},
};

static struct ina230_platform_data power_mon_info_2[] = {
	[VDD_SYS_BL] = {
		.calibration_data  = 0x1A29,
		.power_lsb = 0.63710119 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},

	[AVDD_1V05_LDO2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "AVDD_1V05_LDO2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
	},
};

/* following are power monitor parameters for Flounder A01*/
static struct ina230_platform_data flounder_A01_power_mon_info_1[] = {
	[FLOUNDER_A01_VDD_SYS_BAT] = {
		.calibration_data  = 0x1366,
		.power_lsb = 2.577527185 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BAT",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_RTC_LDO3] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.078127384 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_RTC_LDO3",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 50,
	},

	[FLOUNDER_A01_VDD_SYS_BUCKSOC] = {
		.calibration_data  = 0x1AAC,
		.power_lsb = 0.624877954 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BUCKSOC",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 30,
	},

	[FLOUNDER_A01_VDD_SOC_SD1] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SOC_SD1",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 1,
	},

	[FLOUNDER_A01_VDD_SYS_BUCKCPU] = {
		.calibration_data  = 0x1AC5,
		.power_lsb = 1.867795126 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_CPU_BUCKCPU] = {
		.calibration_data  = 0x2ECF,
		.power_lsb = 10.68179922 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_CPU_BUCKCPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 1,
	},

	[FLOUNDER_A01_VDD_1V8_SD5] = {
		.calibration_data  = 0x45F0,
		.power_lsb = 0.714924039 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V8_SD5",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_3V3A_LDO1_6] = {
		.calibration_data  = 0x3A83,
		.power_lsb = 0.042726484 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_3V3A_LDO1_6",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 200,
	},

	[FLOUNDER_A01_VDD_DIS_3V3_LCD] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_DIS_3V3_LCD",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_1V35_SD2] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 3.906369213 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V35_SD2",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 1,
	},

	[FLOUNDER_A01_VDD_SYS_BUCKGPU] = {
		.calibration_data  = 0x1F38,
		.power_lsb = 1.601601602 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BUCKGPU",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_LCD_1V8B_DIS] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.039063692 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_LCD_1V8B_DIS",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 100,
	},

	[FLOUNDER_A01_VDD_1V05_LDO0] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.130212307 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V05_LDO0",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 30,
	},
};

static struct ina230_platform_data flounder_A01_power_mon_info_2[] = {
	[FLOUNDER_A01_VDD_1V05_SD4] = {
		.calibration_data  = 0x7FFF,
		.power_lsb = 0.390636921 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V05_SD4",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 10,
	},

	[FLOUNDER_A01_VDD_1V8A_LDO2_5_7] = {
		.calibration_data  = 0x5A04,
		.power_lsb = 0.277729561 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_1V8A_LDO2_5_7",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 20,
	},

	[FLOUNDER_A01_VDD_SYS_BL] = {
		.calibration_data  = 0x2468,
		.power_lsb = 0.274678112 * PRECISION_MULTIPLIER_FLOUNDER,
		.rail_name = "VDD_SYS_BL",
		.trig_conf = INA230_TRIG_CONFIG,
		.cont_conf = INA230_CONT_CONFIG,
		.divisor = 25,
		.precision_multiplier = PRECISION_MULTIPLIER_FLOUNDER,
		.resistor = 50,
	},
};

/* i2c addresses of rails present on Flounder */
/* addresses on i2c2_0 */
enum {
	INA_I2C_2_0_ADDR_40,
	INA_I2C_2_0_ADDR_41,
	INA_I2C_2_0_ADDR_42,
	INA_I2C_2_0_ADDR_43,
};

/* addresses on i2c2_1 */
enum {
	INA_I2C_2_1_ADDR_40,
	INA_I2C_2_1_ADDR_41,
	INA_I2C_2_1_ADDR_42,
	INA_I2C_2_1_ADDR_43,
	INA_I2C_2_1_ADDR_44,
	INA_I2C_2_1_ADDR_45,
	INA_I2C_2_1_ADDR_46,
	INA_I2C_2_1_ADDR_47,
	INA_I2C_2_1_ADDR_48,
	INA_I2C_2_1_ADDR_49,
	INA_I2C_2_1_ADDR_4B,
	INA_I2C_2_1_ADDR_4C,
	INA_I2C_2_1_ADDR_4E,
	INA_I2C_2_1_ADDR_4F,
};

/* addresses on i2c2_2 */
enum {
	INA_I2C_2_2_ADDR_49,
	INA_I2C_2_2_ADDR_4C,
};

/* i2c addresses of rails present on Flounder A01*/
/* addresses on i2c2_1 */
enum {
	FLOUNDER_A01_INA_I2C_2_1_ADDR_40,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_41,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_42,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_43,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_44,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_45,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_46,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_47,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_48,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_49,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_4B,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_4E,
	FLOUNDER_A01_INA_I2C_2_1_ADDR_4F,
};

/* addresses on i2c2_2 */
enum {
	FLOUNDER_A01_INA_I2C_2_2_ADDR_40,
	FLOUNDER_A01_INA_I2C_2_2_ADDR_41,
	FLOUNDER_A01_INA_I2C_2_2_ADDR_49,
};

/* following is the i2c board info for Flounder */
static struct i2c_board_info flounder_i2c2_0_ina219_board_info[] = {
	[INA_I2C_2_0_ADDR_40] = {
		I2C_BOARD_INFO("ina219", 0x40),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_41] = {
		I2C_BOARD_INFO("ina219", 0x41),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_42] = {
		I2C_BOARD_INFO("ina219", 0x42),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},

	[INA_I2C_2_0_ADDR_43] = {
		I2C_BOARD_INFO("ina219", 0x43),
		.platform_data = &power_mon_info_0[UNUSED_RAIL],
		.irq = -1,
	},
};

static struct i2c_board_info flounder_i2c2_1_ina230_board_info[] = {
	[INA_I2C_2_1_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data = &power_mon_info_1[VDD_SYS_BAT],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data = &power_mon_info_1[VDD_RTC_LDO5],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data = &power_mon_info_1[VDD_3V3A_SMPS1_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data = &power_mon_info_1[VDD_SOC_SMPS1_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data = &power_mon_info_1[VDD_SYS_BUCKCPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data = &power_mon_info_1[VDD_CPU_BUCKCPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data = &power_mon_info_1[VDD_1V8A_SMPS3],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data = &power_mon_info_1[VDD_1V8B_SMPS9],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data = &power_mon_info_1[VDD_GPU_BUCKGPU],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_1[VDD_1V35_SMPS6],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data = &power_mon_info_1[VDD_3V3A_SMPS1_2_2],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_1[VDD_3V3B_SMPS9],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data = &power_mon_info_1[VDD_LCD_1V8B_DIS],
		.irq = -1,
	},

	[INA_I2C_2_1_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data = &power_mon_info_1[VDD_1V05_SMPS8],
		.irq = -1,
	},
};

static struct i2c_board_info flounder_i2c2_2_ina230_board_info[] = {
	[INA_I2C_2_2_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data = &power_mon_info_2[VDD_SYS_BL],
		.irq = -1,
	},

	[INA_I2C_2_2_ADDR_4C] = {
		I2C_BOARD_INFO("ina230", 0x4C),
		.platform_data = &power_mon_info_2[AVDD_1V05_LDO2],
		.irq = -1,
	},

};

/* following is the i2c board info for Flounder A01 */
static struct i2c_board_info flounder_A01_i2c2_1_ina230_board_info[] = {
	[FLOUNDER_A01_INA_I2C_2_1_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SYS_BAT],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_RTC_LDO3],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_42] = {
		I2C_BOARD_INFO("ina230", 0x42),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SYS_BUCKSOC],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_43] = {
		I2C_BOARD_INFO("ina230", 0x43),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SOC_SD1],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_44] = {
		I2C_BOARD_INFO("ina230", 0x44),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SYS_BUCKCPU],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_45] = {
		I2C_BOARD_INFO("ina230", 0x45),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_CPU_BUCKCPU],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_46] = {
		I2C_BOARD_INFO("ina230", 0x46),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_1V8_SD5],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_47] = {
		I2C_BOARD_INFO("ina230", 0x47),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_3V3A_LDO1_6],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_48] = {
		I2C_BOARD_INFO("ina230", 0x48),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_DIS_3V3_LCD],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_1V35_SD2],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_4B] = {
		I2C_BOARD_INFO("ina230", 0x4B),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SYS_BUCKGPU],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_4E] = {
		I2C_BOARD_INFO("ina230", 0x4E),
		.platform_data =
		&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_LCD_1V8B_DIS],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_1_ADDR_4F] = {
		I2C_BOARD_INFO("ina230", 0x4F),
		.platform_data =
			&flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_1V05_LDO0],
		.irq = -1,
	},
};

static struct i2c_board_info flounder_A01_i2c2_2_ina230_board_info[] = {
	[FLOUNDER_A01_INA_I2C_2_2_ADDR_40] = {
		I2C_BOARD_INFO("ina230", 0x40),
		.platform_data =
			&flounder_A01_power_mon_info_2[FLOUNDER_A01_VDD_1V05_SD4],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_2_ADDR_41] = {
		I2C_BOARD_INFO("ina230", 0x41),
		.platform_data =
		&flounder_A01_power_mon_info_2[FLOUNDER_A01_VDD_1V8A_LDO2_5_7],
		.irq = -1,
	},

	[FLOUNDER_A01_INA_I2C_2_2_ADDR_49] = {
		I2C_BOARD_INFO("ina230", 0x49),
		.platform_data =
			&flounder_A01_power_mon_info_2[FLOUNDER_A01_VDD_SYS_BL],
		.irq = -1,
	},
};

static struct pca954x_platform_mode flounder_pca954x_modes[] = {
	{ .adap_id = PCA954x_I2C_BUS0, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS1, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS2, .deselect_on_exit = true, },
	{ .adap_id = PCA954x_I2C_BUS3, .deselect_on_exit = true, },
};

static struct pca954x_platform_data flounder_pca954x_data = {
	.modes    = flounder_pca954x_modes,
	.num_modes      = ARRAY_SIZE(flounder_pca954x_modes),
};

static const struct i2c_board_info flounder_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x71),
		.platform_data = &flounder_pca954x_data,
	},
};

static void __init register_devices_flounder_A01(void)
{
	i2c_register_board_info(PCA954x_I2C_BUS1,
			flounder_A01_i2c2_1_ina230_board_info,
			ARRAY_SIZE(flounder_A01_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			flounder_A01_i2c2_2_ina230_board_info,
			ARRAY_SIZE(flounder_A01_i2c2_2_ina230_board_info));
}

static void __init register_devices_flounder(void)
{
	i2c_register_board_info(PCA954x_I2C_BUS1,
			flounder_i2c2_1_ina230_board_info,
			ARRAY_SIZE(flounder_i2c2_1_ina230_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS2,
			flounder_i2c2_2_ina230_board_info,
			ARRAY_SIZE(flounder_i2c2_2_ina230_board_info));
}

static void modify_reworked_rail_data(void)
{
	flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_1V35_SD2].resistor
					= VDD_1V35_SD2_REWORKED;
	flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_CPU_BUCKCPU].resistor
					= VDD_CPU_BUCKCPU_REWORKED;
	flounder_A01_power_mon_info_1[FLOUNDER_A01_VDD_SOC_SD1].resistor
					= VDD_SOC_SD1_REWORKED;
}

int __init flounder_pmon_init(void)
{
	/*
	* Get power_config of board and check whether
	* board is power reworked or not.
	* In case board is reworked, modify rail data
	* for which rework was done.
	*/
	u8 power_config;
	struct board_info bi;
	power_config = get_power_config();
	if (power_config & FLOUNDER_POWER_REWORKED_CONFIG)
		modify_reworked_rail_data();

	tegra_get_board_info(&bi);

	i2c_register_board_info(1, flounder_i2c2_board_info,
		ARRAY_SIZE(flounder_i2c2_board_info));

	i2c_register_board_info(PCA954x_I2C_BUS0,
			flounder_i2c2_0_ina219_board_info,
			ARRAY_SIZE(flounder_i2c2_0_ina219_board_info));

	if (bi.fab >= BOARD_FAB_A01)
		register_devices_flounder_A01();
	else
		register_devices_flounder();

	return 0;
}

