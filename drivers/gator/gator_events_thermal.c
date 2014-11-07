/**
 *
 * Copyright (C) ARM Limited 2010-2013. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "gator.h"
#include <linux/odin_thermal.h>
#include <trace/events/power.h>

enum {
	THERMAL_TEMP0 = 0,
	THERMAL_TEMP1,
	THERMAL_TEMP2,
	THERMAL_TEMP3,
	THERMAL_TEMP4,
	THERMAL_TEMP5,
	THERMAL_TEMP6,
	THERMAL_COUNTERS_NUM,
};

static ulong thermal_enabled[THERMAL_COUNTERS_NUM];
static ulong thermal_key[THERMAL_COUNTERS_NUM];
static unsigned long thermal_buffer[THERMAL_COUNTERS_NUM * 2];
static int thermal_length = 0;

GATOR_DEFINE_PROBE(odin_ts_temp, TP_PROTO(const char *name,
					u32 id, unsigned long temp))
{
	int i;

	if (id == 0) {
		i = THERMAL_TEMP0;
	} else if (id == 1) {
		i = THERMAL_TEMP1;
	} else if (id == 2) {
		i = THERMAL_TEMP2;
	} else if (id == 3) {
		i = THERMAL_TEMP3;
	} else if (id == 4) {
		i = THERMAL_TEMP4;
	} else if (id == 5) {
		i = THERMAL_TEMP5;
	} else if (id == 6) {
		i = THERMAL_TEMP0;
	} else {
		return;
	}

	thermal_buffer[i*2] = thermal_key[i];
	thermal_buffer[i*2+1] = temp/1000;
}

static int gator_events_thermal_create_files(struct super_block *sb,
					    struct dentry *root)
{
	struct dentry *dir;
	int i;

	for (i = 0; i < THERMAL_COUNTERS_NUM; i++) {
		switch (i) {
		case THERMAL_TEMP0:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp0");
			break;
		case THERMAL_TEMP1:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp1");
			break;
		case THERMAL_TEMP2:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp2");
			break;
		case THERMAL_TEMP3:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp3");
			break;
		case THERMAL_TEMP4:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp4");
			break;
		case THERMAL_TEMP5:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp5");
			break;
		case THERMAL_TEMP6:
			dir = gatorfs_mkdir(sb, root, "Thermal_temp6");
			break;
		}
		if (!dir) {
			return -1;
		}
		gatorfs_create_ulong(sb, dir, "enabled", &thermal_enabled[i]);
		gatorfs_create_ro_ulong(sb, dir, "key", &thermal_key[i]);
	}

	return 0;
}

static int gator_events_thermal_start(void)
{
	int len;
	unsigned long value = 0;

	thermal_length = len = 0;

	if (GATOR_REGISTER_TRACE(odin_ts_temp))
		goto fail_thermal_set_exit;

    odin_tsens_get_temp(0, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP0];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(1, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP1];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(2, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP2];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(3, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP3];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(4, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP4];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(5, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP5];
	thermal_buffer[len++] = value/1000;

    odin_tsens_get_temp(6, &value);
	thermal_buffer[len++] = thermal_key[THERMAL_TEMP6];
	thermal_buffer[len++] = value/1000;

	thermal_length = len;

	return 0;

fail_thermal_set_exit:
	GATOR_UNREGISTER_TRACE(odin_ts_temp);
	return -1;
}

static void gator_events_thermal_stop(void)
{
	int i;

    GATOR_UNREGISTER_TRACE(odin_ts_temp);

	for (i = 0; i < THERMAL_COUNTERS_NUM; i++) {
		thermal_enabled[i] = 0;
	}
}

static int gator_events_thermal_read(unsigned long **buffer)
{
	if (buffer)
		*buffer = thermal_buffer;

	return thermal_length;
}

static struct gator_interface gator_events_thermal_interface = {
	.create_files = gator_events_thermal_create_files,
	.start = gator_events_thermal_start,
	.stop = gator_events_thermal_stop,
	.read = gator_events_thermal_read,
};

int __init gator_events_thermal_init(void)
{
	int i;

	for (i = 0; i < THERMAL_COUNTERS_NUM; i++) {
		thermal_enabled[i] = 0;
		thermal_key[i] = gator_events_get_key();
	}

	return gator_events_install(&gator_events_thermal_interface);
}

gator_events_init(gator_events_thermal_init);
