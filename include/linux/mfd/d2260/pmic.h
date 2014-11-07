/*
 *  Power Managment declarations for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *  Author: Tony Olech <anthony.olech@diasemi.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#ifndef __D2260_PMIC_H
#define __D2260_PMIC_H

#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>

/*
 * Register values.
 */

enum d2260_regulator_id {
/*
 * DC-DC's
 */
	D2260_BUCK_0 = 0,
	D2260_BUCK_1,
	D2260_BUCK_2,
	D2260_BUCK_3,
	D2260_BUCK_4,
	D2260_BUCK_5,
	D2260_BUCK_6,
	D2260_BUCK_7,
/*
 * LDOs
 */
	D2260_LDO_1,
	D2260_LDO_2,
	D2260_LDO_3,
	D2260_LDO_4,
	D2260_LDO_5,
	D2260_LDO_6,
	D2260_LDO_7,
	D2260_LDO_8,
	D2260_LDO_9,
	D2260_LDO_10,

	D2260_LDO_11,
	D2260_LDO_12,
	D2260_LDO_13,
	D2260_LDO_14,
	D2260_LDO_15,
	D2260_LDO_16,
	D2260_LDO_17,
	D2260_LDO_18,
	D2260_LDO_19,
	D2260_LDO_20,
	D2260_LDO_21,
	D2260_LDO_22,
	D2260_LDO_23,
	D2260_LDO_24,
	D2260_LDO_25,

	D2260_GPIOCTRL_0,
	D2260_GPIOCTRL_1,

#ifdef D2260_GPO_ENABLE
	D2260_GPOCTRL_0,
	D2260_GPOCTRL_1,
	D2260_GPOCTRL_2,
#endif
	D2260_NUMBER_OF_REGULATORS  /* 33 */
};

/*TODO MW: Check Modes when final design is ready */
/* Global MCTL state controled by HW (M_CTL1 and M_CTL2 signals) */
enum d2260_mode_control {
	MCTL_0, /* M_CTL1 = 0, M_CTL2 = 0 */
	MCTL_1, /* M_CTL1 = 0, M_CTL2 = 1 */
	MCTL_2, /* M_CTL1 = 1, M_CTL2 = 0 */
	MCTL_3, /* M_CTL1 = 1, M_CTL2 = 1 */
};

/*regualtor DSM settings */
enum d2260_mode_in_dsm {
	D2260_REGULATOR_LPM_IN_DSM = 0,  /* LPM in DSM(deep sleep mode) */
	D2260_REGULATOR_OFF_IN_DSM,      /* OFF in DSM */
	D2260_REGULATOR_ON_IN_DSM,       /* LPM in DSM */
	D2260_REGULATOR_MAX,
};

/* TODO MW: move those to d2260_reg.h
 * - this are bimask specific to registers' map */
/* CONF and EN is same for all */

/* TODO MW: description*/
#define D2260_REGULATOR_MCTL3 (3<<6)
#define D2260_REGULATOR_MCTL2 (3<<4)
#define D2260_REGULATOR_MCTL1 (3<<2)
#define D2260_REGULATOR_MCTL0 (3<<0)

/* TODO MW:  figure out more descriptive names to distinguish
 * between global M_CTLx state determined by hardware,
 * and the mode for each regulator configured
 * in BUCKx/LDOx_MCTLy register */

/* MCTL values for regulator bits ... TODO finish description */
#define REGULATOR_MCTL_OFF    0
#define REGULATOR_MCTL_ON     1
#define REGULATOR_MCTL_SLEEP  2
#define REGULATOR_MCTL_TURBO  3 /* Available only for BUCK1 */

#define D2260_REG_MCTL3_SHIFT 6 /* Bits [7:6] in BUCKx/LDOx_MCTLy register */
#define D2260_REG_MCTL2_SHIFT 4 /* Bits [5:4] in BUCKx/LDOx_MCTLy register */
#define D2260_REG_MCTL1_SHIFT 2 /* Bits [3:2] in BUCKx/LDOx_MCTLy register */
#define D2260_REG_MCTL0_SHIFT 0 /* Bits [1:0] in BUCKx/LDOx_MCTLy register */

/* When M_CTL1 = 1, M_CTL2 = 1
 * (M_CTL3: global Turbo Mode), regultor is: */
#define D2260_REGULATOR_MCTL3_OFF   \
		(REGULATOR_MCTL_OFF   << D2260_REG_MCTL3_SHIFT)
#define D2260_REGULATOR_MCTL3_ON    \
		(REGULATOR_MCTL_ON    << D2260_REG_MCTL3_SHIFT)
#define D2260_REGULATOR_MCTL3_SLEEP \
		(REGULATOR_MCTL_SLEEP << D2260_REG_MCTL3_SHIFT)
#define D2260_REGULATOR_MCTL3_TURBO \
		(REGULATOR_MCTL_TURBO << D2260_REG_MCTL3_SHIFT)

/* When M_CTL1 = 1, M_CTL2 = 0
 * (M_CTL2: TBD: To Be Defined Mode), regulator is: */
#define D2260_REGULATOR_MCTL2_OFF   \
		(REGULATOR_MCTL_OFF   << D2260_REG_MCTL2_SHIFT)
#define D2260_REGULATOR_MCTL2_ON    \
		(REGULATOR_MCTL_ON    << D2260_REG_MCTL2_SHIFT)
#define D2260_REGULATOR_MCTL2_SLEEP \
		(REGULATOR_MCTL_SLEEP << D2260_REG_MCTL2_SHIFT)
#define D2260_REGULATOR_MCTL2_TURBO \
		(REGULATOR_MCTL_TURBO << D2260_REG_MCTL2_SHIFT)

/* When M_CTL1 = 0, M_CTL2 = 1
 * (M_CTL1: Normal Mode), regulator is: */
#define D2260_REGULATOR_MCTL1_OFF   \
		(REGULATOR_MCTL_OFF   << D2260_REG_MCTL1_SHIFT)
#define D2260_REGULATOR_MCTL1_ON    \
		(REGULATOR_MCTL_ON    << D2260_REG_MCTL1_SHIFT)
#define D2260_REGULATOR_MCTL1_SLEEP \
		(REGULATOR_MCTL_SLEEP << D2260_REG_MCTL1_SHIFT)
#define D2260_REGULATOR_MCTL1_TURBO \
		(REGULATOR_MCTL_TURBO << D2260_REG_MCTL1_SHIFT)

/* When M_CTL1 = 0, M_CTL2 = 0
 * (M_CTL0: Sleep Mode), regulator is: */
#define D2260_REGULATOR_MCTL0_OFF   \
		(REGULATOR_MCTL_OFF   << D2260_REG_MCTL0_SHIFT)
#define D2260_REGULATOR_MCTL0_ON    \
		(REGULATOR_MCTL_ON    << D2260_REG_MCTL0_SHIFT)
#define D2260_REGULATOR_MCTL0_SLEEP \
		(REGULATOR_MCTL_SLEEP << D2260_REG_MCTL0_SHIFT)
#define D2260_REGULATOR_MCTL0_TURBO \
		(REGULATOR_MCTL_TURBO << D2260_REG_MCTL0_SHIFT)


struct d2260;
struct platform_device;
struct regulator_init_data;


struct d2260_pmic {
	/* Number of regulators of each type on this device */
	/* regulator devices */
	struct platform_device *pdev[D2260_NUMBER_OF_REGULATORS];
};

/* Battery Temperature */
int d2260_get_battery_temp(void);
/* PCB TOP Thermistor */
int d2260_get_pcb_therm_top_voltage(void);
/* PCB BOTTOM Thermistor */
int d2260_get_pcb_therm_bot_voltage(void);
#endif  /* __D2260_PMIC_H */

