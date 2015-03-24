/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ANX3618_H
#define __LINUX_ANX3618_H

struct anx3618_platform_data {
	int gpio_hdmi;
	int gpio_vmch;
	int gpio_usb_plug;
	int gpio_pd;
	int gpio_rst;
	int gpio_cable_det;
	int gpio_intp;
	int gpio_usb_det;
	int gpio_sel_uart;
	int gpio_1_0_v_on;
	int gpio_extclk; /* GPIO to be used as external clk source */
	int vreg_gpio_1_0v;
	int vreg_gpio_1_0v_config;
	int vreg_3_3v;
	void (*anx3618_clk_enable)(bool);
};

#endif /* __LINUX_ANX3618_H */
