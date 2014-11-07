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
/*
 * MAXIM PMIC MAX77525 ADC driver header file
 *
 */

#ifndef __MAX77525_ADC_H
#define __MAX77525_ADC_H

#include <linux/kernel.h>
#include <linux/list.h>
/**
 * enum max77525_adc_channels - AMUX arbiter channels
 */
enum max77525_adc_channels {
	CH0_RSVD = 0,
	CH1_TDIE,
	CH2_VBBATT,
	CH3_VSYS,
	CH4_RSVD,
	CH5_RSVD,
	CH6_ADC0,
	CH7_ADC1,
	CH8_VMBATDETB,
	CH9_ADC2,
	CH10_ADC3,
	CH11_ADC4,
	CH12_ADC5,
	CH13_ADC6,
	ADC_MAX_NUM,
};

/**
 * enum max77525_adc_scale_fn_type
 * - Scaling function for max77525 pre calibrated
 *   digital data relative to ADC reference.
 * %SCALE_DEFAULT: Default scaling to convert raw adc code to voltage (uV).
 * %SCALE_TDIE: Conversion to temperature(decidegC) based on tdie
 *			parameters.
 * %SCALE_VBBATT: return Battery voltage in mV
 * %SCALE_VSYS: Returns result in mV.
 * %SCALE_VMBATDETB: Returns MBATDETB voltage in mV.
 * %SCALE_NONE: Do not use this scaling type.
 */
enum max77525_adc_scale_fn_type {
	SCALE_DEFAULT = 0,
	SCALE_TDIE,
	SCALE_VBBATT,
	SCALE_VSYS,
	SCALE_VMBATDETB,
	SCALE_NONE,
};

/**
 * enum adc_avg_type - ADC sample average rate
 */
enum adc_avg_type {
	ADC_AVG_SAMPLE_1,
	ADC_AVG_SAMPLE_2,
	ADC_AVG_SAMPLE_16,
	ADC_AVG_SAMPLE_32,
	ADC_AVG_SAMPLE_NONE
};

/**
 * struct max77525_adc - MAX77525 ADC device structure.
 * @max77525 - max77525 device for ADC peripheral.
 * @regmap - register map
 * @adc_channels - ADC channel properties for the ADC peripheral.
 * @adc_irq - ADC IRQ.
 * @adc_lock - ADC lock for access to the peripheral.
 * @adc_rslt_completion - ADC result notification after interrupt
 *			  is received.
 */

struct max77525_adc {
	struct regmap			*regmap;
	struct max77525_adc_amux	*adc_channels;
	struct mutex			adc_lock;
	struct completion		adc_rslt_completion;
	struct device			*adc_hwmon;
	bool					adc_initialized;
	enum adc_avg_type		adc_avg;
	int						adc_delay;
	int						adc_irq;
	int						num_channels;
	struct sensor_device_attribute	sens_attr[0];
};

/**
 * struct max77525_adc_result - Represent the result of the MAX77525 ADC.
 * @chan: The channel number of the requested conversion.
 * @adc_code: The pre-calibrated digital output of a given ADC relative to the
 *	      the ADC reference.
 * @physical: The data meaningful for each individual channel whether it is
 *	      voltage, current, temperature, etc.
 *	      All voltage units are represented in micro - volts.
 *	      -PMIC Die temperature units are represented as 1 DegC.
 */
struct max77525_adc_result {
	uint8_t	chan;
	int16_t	adc_code;
	int32_t	physical;
};

/**
 * struct max77525_adc_amux - AMUX properties for individual channel
 * @name: Channel string name.
 * @channel_num: Channel in integer used from max77525_adc_channels.
 * @adc_scale_fn: Scaling function to convert to the data meaningful for
 *		 each individual channel whether it is voltage, current,
 *		 temperature, etc and compensates the channel properties.
 */
struct max77525_adc_amux {
	char					*name;
	enum max77525_adc_channels		channel_num;
	enum max77525_adc_scale_fn_type		adc_scale_fn;
};

/**
 * struct max77525_adc_scale_fn - Scaling function prototype
 * @chan: Function pointer to one of the scaling functions
 *	which takes the adc properties, channel properties,
 *	and returns the physical result
 */
struct max77525_adc_scale_fn {
	int32_t (*chan) (int32_t, int32_t,
		struct max77525_adc_result *);
};

/* Public API */
/**
 * max77525_adc_read() - Performs ADC read on the channel.
 * @channel:	Input channel to perform the ADC read.
 * @result:	Structure pointer of type adc_chan_result
 *		in which the ADC read results are stored.
 */
int32_t max77525_adc_read(struct max77525_adc *adc,
				enum max77525_adc_channels channel,
				struct max77525_adc_result *result);

#endif
