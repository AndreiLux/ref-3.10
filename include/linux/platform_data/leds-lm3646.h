/*
 * Simple driver for Texas Instruments LM3642 LED Flash driver chip
 * Copyright (C) 2013 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_LM3646_H
#define __LINUX_LM3646_H

#define LM3646_NAME "leds-lm3646"
#define LM3646_ADDR 0x63

enum lm3646_tx_pin {
	LM3646_TX_PIN_DISABLED = 0,
	LM3646_TX_PIN_ENABLED = 0x08,
};

enum lm3646_torch_pin {
	LM3646_TORCH_PIN_DISABLED = 0,
	LM3646_TORCH_PIN_ENABLED = 0x80,
};

enum lm3646_strobe_pin {
	LM3646_STROBE_PIN_DISABLED = 0,
	LM3646_STROBE_PIN_ENABLED = 0x80,
};

/* struct lm3646 platform data
 * @flash_imax 0x0 =  93.4 mA
 *			   0x1 = 187.1 mA
 *				.....
 *			   0xF = 1500 mA
 * @torch_imax 0x0 = 23.1 mA
 *			   0x1 = 46.5 mA
 *				.....
 *			   0x7 = 187.1 mA
 * @led1_flash_imax 0x00 = 0 mA(disabled), led2=flash_imax
 *					0x01 = 23.1 mA, led2=flash_imax-23.1mA
 *					.....
 *					0x7F = 1.5A, led2=disabled
 * @led1_torch_imax 0x00 = 0 mA(disabled), led2=torch_imax
 *					0x01 = 2.6 mA, led2=torch_imax-2.6mA
 *					.....
 *					0x7F = 187.1 mA, led2=disabled
 * @tx_pin Enable tx pin and tx current reduction function
 * @torch_pin : torch pin enable
 * @strobe_pin : strobe pin enable
 */
struct lm3646_platform_data {

	u8 flash_imax;
	u8 torch_imax;
	u8 led1_flash_imax;
	u8 led1_torch_imax;

	enum lm3646_tx_pin tx_pin;
	enum lm3646_torch_pin torch_pin;
	enum lm3646_strobe_pin strobe_pin;

	u8 flash_timing;
};

#endif /* __LINUX_LM3646_H */
