/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __REGULATOR_MAX77525_REGULATOR_H__
#define __REGULATOR_MAX77525_REGULATOR_H__

#include <linux/regulator/machine.h>

#define MAX77525_REGULATOR_DRIVER_NAME "maxim,max77525-regulator"

/* Operation Mode */
#define MAX77525_REGULATOR_OFF		0x00	/* Off */
#define MAX77525_REGULATOR_LPM		0x01	/* Low Power Mode */
#define MAX77525_REGULATOR_DLPM		0x02	/* Dynamic Low Power Mode */
#define MAX77525_REGULATOR_NM		0x03	/* Normal Mode */

/*
 * Used with enable parameters to specify that hardware default register values
 * should be left unaltered.
 */
#define MAX77525_REGULATOR_DISABLE				0
#define MAX77525_REGULATOR_ENABLE				1
#define MAX77525_REGULATOR_USE_HW_DEFAULT		0xFF

/**
 * struct max77525_regulator_data - max77525-regulator data
 * @reg_id:		regulator id
 * @enable_mode:	Operation Mode when the regulator turns on
 *				Its value should be one of
				1 = Low Power Mode
				2 = Dyamic Low Power Mode, controlled by GLPM_EN
				3 = Normal Mode
 * @pull_down_enable:	1 = Enable output pull down resistor when the
 *			        	regulator is disabled
 *			    		0 = Disable pull down resistor
 *			    		MAX77525_REGULATOR_USE_HW_DEFAULT = do not modify
 *			        	pull down state
 * @fsr_ad_enable:		Falling Slew Rate Active-Discharge Enable.
 *					It is used for buck 1A/1B/1C/1D/2A/2B/2C/2D/3.
 *					1 = Enable Active Discharge
 *					0 = Disable Active Discharge
 * @rsns_enable:	Remote Output Voltage Sense Enable.
 *				Only available in BUCK1A, BUCK1C, BUCK2A, and BUCK2C.
 * @forced_pwm_enable: 	1 = Turn forced PWM ON. The buck has the fixed frequency
 * 						under all load conditions.
 *						0 = Turn forced PWM OFF. The buck has automatically
 *						skip pulse under light load.
 * @buck_ramp_rate:	This parameter sets the ramp rate in mV/us of some buck type
 *					regulators. It is just used for buck 1A/1B/1C/1D/2A/2B/2C/2D/3.
 *					Its vaule should be one of
 *					MAX77525_BUCK_RAMP_RATE_*. If its value is
 *					MAX77525_REGULATOR_USE_HW_DEFAULT, then the ramp rate will
 *					be left at its default hardware value.
 * @boost_force_bypass:	This parameter sets the force_bypass_mode of
 *						boost type regulator.
					1 = Turn on Forced Bypass Mode
					0 = Disable Forced Bypass Mode
 * @initdata:		regulator constraints
 * @reg_node:	each regulator node
 **/
/* each regulator data */
struct max77525_regulator_data {
	int 	reg_id;
	int 	enable_mode;
	int 	pull_down_enable;
	int 	fsr_ad_enable;
	int 	rsns_enable;
	int 	forced_pwm_enable;
	int 	buck_ramp_rate;
	int 	boost_force_bypass;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};


/*
 * struct max77525_regulators_platform_data - max77525-regulators platform data
 * @reg_data:		each regulator data point
 * @num_regulators: total number of regulators
 * @dvs_enable:	dynamic voltage scale enable
 *				1 = Enable DVS
 *				0 = Disable DVS
 * @dvs_voltage: 	dynamic voltage to set in each regulators.
 * @dvs_gpio:		dvs GPIO number
 */
struct max77525_regulators_platform_data {
	struct max77525_regulator_data *reg_data;
	int 				num_regulators;
	int					dvs_enable[9];
	int					dvs_gpio;
};

#endif
