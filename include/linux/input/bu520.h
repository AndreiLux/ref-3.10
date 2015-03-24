/*
 * Hall sensor driver capable of dealing with more than one sensor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _BU520_H
#define _BU520_H

struct hall_sensor_data {
	unsigned int gpio_pin;
	int irq;
	char *name;
	int gpio_state;
};

struct hall_sensors {
	struct hall_sensor_data *hall_sensor_data;
	int hall_sensor_num;
};

#endif
