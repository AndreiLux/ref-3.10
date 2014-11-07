/*
 * max17048.h
 *
 * Copyright 2013 Maxim Integrated Products, Inc.
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#ifndef __MAX17048_H__
#define __MAX17048_H__

/* registers */
#define MAX17048_VCELL_REG		0x02
#define MAX17048_SOC_REG		0x04
#define MAX17048_MODE_REG		0x06
#define MAX17048_VERSION_REG	0x08
#define MAX17048_HIBRT_REG		0x0A
#define MAX17048_CONFIG_REG		0x0C
#define MAX17048_VALRT_REG		0x14
#define MAX17048_CRATE_REG		0x16
#define MAX17048_VRESET_REG		0x18
#define MAX17048_STATUS_REG		0x1A
#define MAX17048_TABLE_REG		0x40
#define MAX17048_CMD_REG		0xFE

struct max17048_platform_data
{
	u8 rcomp;
	int empty_alert_threshold;	/* Empty Alert Threshold. 1% to 32% */
	int voltage_alert_threshold;	/* Minimum Voltage Alert. 0uV to 5120000uV in 20000uV steps */
	unsigned short model_data[32];
/*
               
                                     
                                
 */
	int irq;
	int irq_gpio;
	int rcomp_co_hot;
	int rcomp_co_cold;
/*              */
};

#endif /* __MAX17058_BATTERY_H__ */
