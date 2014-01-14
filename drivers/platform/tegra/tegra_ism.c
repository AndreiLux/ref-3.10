/*
 * drivers/platform/tegra/tegra_ism.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "../../../arch/arm/mach-tegra/clock.h"
#include "tegra_ism.h"
#include "tegra_apb2jtag.h"

#define NV_ISM_ROSC_BIN_IDDQ_LSB		8
#define NV_ISM_ROSC_BIN_IDDQ_MSB		8
#define NV_ISM_ROSC_BIN_MODE_LSB		9
#define NV_ISM_ROSC_BIN_MODE_MSB		10
#define NV_ISM_ROSC_BIN_OUT_DIV_LSB		5
#define NV_ISM_ROSC_BIN_OUT_DIV_MSB		7
#define NV_ISM_ROSC_BIN_LOCAL_DURATION_LSB	11
#define NV_ISM_ROSC_BIN_LOCAL_DRUATION_MSB	26
#define NV_ISM_ROSC_BIN_SRC_SEL_LSB		0
#define NV_ISM_ROSC_BIN_SRC_SEL_MSB		4
#define NV_ISM_ROSC_BIN_COUNT_LSB		27
#define NV_ISM_ROSC_BIN_COUNT_MSB		42


static u32 delay_us = 1000;

static u32 get_bits(u32 value, u32 lsb, u32 msb)
{
	u32 mask = 0xFFFFFFFF;
	u32 num_bits = msb - lsb + 1;
	if (num_bits + lsb > 32)
		return 0;
	if (num_bits < 32)
		mask = (1 << num_bits) - 1;

	return (value >> lsb) & mask;
}

static u32 get_buf_bits(u32 *buf, u32 lsb, u32 msb)
{
	u32 lower_addr = lsb / 32;
	u32 upper_addr = msb / 32;
	u32 ret = 0;

	if (msb < lsb)
		return 0;
	if (msb - lsb + 1 > 32)
		return 0;
	lsb = lsb % 32;
	msb = msb % 32;
	if (lower_addr == upper_addr) {
		ret = get_bits(buf[lower_addr], lsb, msb);
	} else {
		ret = get_bits(buf[lower_addr], lsb, 31);
		ret = ret | (get_bits(buf[upper_addr], 0, msb) << (32 - lsb));
	}
	return ret;
}

static inline u32 set_bits(u32 cur_val, u32 new_val, u32 lsb, u32 msb)
{
	u32 mask = 0xFFFFFFFF;
	u32 num_bits = msb - lsb + 1;
	if (num_bits + lsb > 32)
		return 0;
	if (num_bits < 32)
		mask = (1 << num_bits) - 1;
	return (cur_val & ~(mask << lsb)) | ((new_val & mask) << lsb);
}

void set_buf_bits(u32 *buf, u32 val, u32 lsb, u32 msb)
{
	u32 lower_addr = lsb / 32;
	u32 upper_addr = msb / 32;
	u32 num_bits;

	if (msb < lsb)
		return;
	if (msb - lsb + 1 > 32)
		return;
	num_bits = msb - lsb + 1;
	lsb = lsb % 32;
	msb = msb % 32;
	if (lower_addr == upper_addr) {
		buf[lower_addr] = set_bits(buf[lower_addr], val, lsb, msb);
	} else {
		buf[lower_addr] = set_bits(buf[lower_addr], val, lsb, 31);
		buf[upper_addr] = set_bits(buf[upper_addr], val >> (32 - lsb),
					   0, msb);
	}
}

static u32 set_disable_ism(u32 mode, u32 offset,
			u8 chiplet, u16 len, u8 instr_id, u32 disable)
{
	u32 buf[10];
	int ret;

	memset(buf, 0, sizeof(buf));

	apb2jtag_get();
	apb2jtag_read_locked(instr_id, len, chiplet, buf);
	set_buf_bits(buf, mode, NV_ISM_ROSC_BIN_MODE_LSB + offset,
			NV_ISM_ROSC_BIN_MODE_MSB + offset);
	set_buf_bits(buf, disable, NV_ISM_ROSC_BIN_IDDQ_LSB + offset,
			NV_ISM_ROSC_BIN_IDDQ_MSB + offset);
	ret = apb2jtag_write_locked(instr_id, len, chiplet, buf);
	apb2jtag_put();

	if (ret < 0)
		pr_err("%s: APB2JTAG write failed\n", __func__);

	udelay(delay_us);

	return ret;
}

u32 read_ism(u32 mode, u32 duration, u32 div, u32 sel, u32 offset,
	     u8 chiplet, u16 len, u8 instr_id)
{
	int ret;
	u32 count;
	u32 buf[10];

	memset(buf, 0, sizeof(buf));

	/* Toggle IDDQ to reset ISM */
	set_disable_ism(mode, offset, chiplet, len, instr_id, 1);

	apb2jtag_get();

	apb2jtag_read_locked(instr_id, len, chiplet, buf);

	set_buf_bits(buf, mode, NV_ISM_ROSC_BIN_MODE_LSB + offset,
			NV_ISM_ROSC_BIN_MODE_MSB + offset);
	set_buf_bits(buf, sel, NV_ISM_ROSC_BIN_SRC_SEL_LSB + offset,
			NV_ISM_ROSC_BIN_SRC_SEL_MSB + offset);
	set_buf_bits(buf, div, NV_ISM_ROSC_BIN_OUT_DIV_LSB + offset,
			NV_ISM_ROSC_BIN_OUT_DIV_MSB + offset);
	set_buf_bits(buf, duration, NV_ISM_ROSC_BIN_LOCAL_DURATION_LSB + offset,
			NV_ISM_ROSC_BIN_LOCAL_DRUATION_MSB + offset);
	set_buf_bits(buf, 0, NV_ISM_ROSC_BIN_IDDQ_LSB + offset,
			NV_ISM_ROSC_BIN_IDDQ_MSB + offset);

	ret = apb2jtag_write_locked(instr_id, len, chiplet, buf);
	if (ret < 0) {
		pr_err("read_ism: APB2JTAG write failed.\n");
		apb2jtag_put();
		set_disable_ism(mode, offset, chiplet, len, instr_id, 1);
		return 0;
	}

	/* Delay ensures that the apb2jtag interface has time to respond */
	udelay(delay_us);

	ret = apb2jtag_read_locked(instr_id, len, chiplet, buf);
	apb2jtag_put();

	/* Disable ISM */
	set_disable_ism(mode, offset, chiplet, len, instr_id, 1);

	if (ret < 0) {
		pr_err("read_ism: APB2JTAG read failed.\n");
		return 0;
	}
	count = get_buf_bits(buf, NV_ISM_ROSC_BIN_COUNT_LSB + offset,
				NV_ISM_ROSC_BIN_COUNT_MSB + offset);
	return (count * (tegra_clk_measure_input_freq() / 1000000) *
		(1 << div)) / duration;
}

#ifdef CONFIG_DEBUG_FS
static int __init debugfs_init(void)
{
	struct dentry *dfs_file, *dfs_dir;

	dfs_dir = debugfs_create_dir("tegra_ism", NULL);
	if (!dfs_dir)
		return -ENOMEM;

	dfs_file = debugfs_create_u32("delay_us", 0644, dfs_dir, &delay_us);
	if (!dfs_file)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dfs_dir);
	return -ENOMEM;
}
late_initcall_sync(debugfs_init);
#endif
