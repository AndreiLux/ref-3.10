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
#include <linux/clk.h>
#include <trace/events/power.h>

enum {
	MEM_CLOCK = 0,
	CCI_CLOCK,
	GPU_CLOCK,
	CLOCK_COUNTERS_NUM,
};

static ulong clock_enabled[CLOCK_COUNTERS_NUM];
static ulong clock_key[CLOCK_COUNTERS_NUM];
static unsigned long clock_buffer[CLOCK_COUNTERS_NUM * 2];
static int clock_length = 0;

GATOR_DEFINE_PROBE(clock_set_rate, TP_PROTO(const char *name,
			unsigned int state, unsigned int cpu_id))
{
	int i;

	if (strcmp("memc_clk", name) == 0) {
		i = MEM_CLOCK;
	} else if (strcmp("cclk", name) == 0) {
		i = CCI_CLOCK;
	} else if (strcmp("gpu_core_clk", name) == 0) {
		i = GPU_CLOCK;
	} else {
		return;
	}

	clock_buffer[i*2] = clock_key[i];
	clock_buffer[i*2+1] = state;
}

static int gator_events_clock_create_files(struct super_block *sb,
					    struct dentry *root)
{
	struct dentry *dir;
	int i;

	for (i = 0; i < CLOCK_COUNTERS_NUM; i++) {
		switch (i) {
		case MEM_CLOCK:
			dir = gatorfs_mkdir(sb, root, "Clock_memory_freq");
			break;
		case CCI_CLOCK:
			dir = gatorfs_mkdir(sb, root, "Clock_cci_freq");
			break;
		case GPU_CLOCK:
			dir = gatorfs_mkdir(sb, root, "Clock_gpu_freq");
			break;
		default:
			return -1;
		}
		if (!dir) {
			return -1;
		}
		gatorfs_create_ulong(sb, dir, "enabled", &clock_enabled[i]);
		gatorfs_create_ro_ulong(sb, dir, "key", &clock_key[i]);
	}

	return 0;
}


static int gator_events_clock_start(void)
{
	int len;
	unsigned long value = 0;
	struct clk *tmp_clk;
	int i;

	clock_length = len = 0;

	if (GATOR_REGISTER_TRACE(clock_set_rate))
		goto fail_clock_set_exit;

	tmp_clk = clk_get(NULL, "memc_clk");
	value = clk_get_rate(tmp_clk);
	clk_put(tmp_clk);
	clock_buffer[len++] = clock_key[MEM_CLOCK];
	clock_buffer[len++] = value;

	tmp_clk = clk_get(NULL, "cclk");
	value = clk_get_rate(tmp_clk);
	clk_put(tmp_clk);
	clock_buffer[len++] = clock_key[CCI_CLOCK];
	clock_buffer[len++] = value;

	tmp_clk = clk_get(NULL, "gpu_core_clk");
	value = clk_get_rate(tmp_clk);
	clk_put(tmp_clk);
	clock_buffer[len++] = clock_key[GPU_CLOCK];
	clock_buffer[len++] = value;

	clock_length = len;

	return 0;

fail_clock_set_exit:
	GATOR_UNREGISTER_TRACE(clock_set_rate);
	return -1;
}

static void gator_events_clock_stop(void)
{
	int i;

    GATOR_UNREGISTER_TRACE(clock_set_rate);

	for (i = 0; i < CLOCK_COUNTERS_NUM; i++) {
		clock_enabled[i] = 0;
	}
}

static int gator_events_clock_read(unsigned long **buffer)
{
	if (buffer)
		*buffer = clock_buffer;
	return clock_length;
}

static struct gator_interface gator_events_clock_interface = {
	.create_files = gator_events_clock_create_files,
	.start = gator_events_clock_start,
	.stop = gator_events_clock_stop,
	.read = gator_events_clock_read,
};

int __init gator_events_clock_init(void)
{
	int i;

	for (i = 0; i < CLOCK_COUNTERS_NUM; i++) {
		clock_enabled[i] = 0;
		clock_key[i] = gator_events_get_key();
	}

	return gator_events_install(&gator_events_clock_interface);
}

gator_events_init(gator_events_clock_init);
