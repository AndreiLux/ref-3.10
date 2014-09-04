/*
 * drivers/platform/tegra/tegra13_simon_graders.c
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
#include <linux/tegra-fuse.h>
#include <linux/delay.h>
#include "tegra_ism.h"
#include "tegra_apb2jtag.h"
#include "tegra_simon.h"


#define CCROC_MINI_CORE0_TOP_T132_ID	0xC0
#define CCROC_MINI_CORE0_TOP_T132_WIDTH	284
#define CCROC_MINI_CORE0_TOP_T132_CHIPLET_SEL 3
#define CCROC_MINI_CORE0_TOP_T132_ROSC_BIN_CPU_DENVER_FEU_BIN 206

#define E_TPC0_CLUSTER_BIN_T132_ID	0xC8
#define E_TPC0_CLUSTER_BIN_T132_WIDTH	46
#define E_TPC0_CLUSTER_BIN_T132_CHIPLET_SEL	2
#define E_TPC0_CLUSTER_BIN_T132_ROSC_BIN_GPU_ET0TX0A_GPCCLK_TEX_P00 2

#define FUSE_CTRL			0x0
#define FUSE_CTRL_STATE_OFFSET		16
#define FUSE_CTRL_STATE_MASK		0x1F
#define FUSE_CTRL_STATE_IDLE		0x4
#define FUSE_CTRL_CMD_OFFSET		0
#define FUSE_CTRL_CMD_MASK		0x3
#define FUSE_CTRL_CMD_READ		0x1
#define FUSE_ADDR			0x4
#define FUSE_DATA			0x8
#define FUSE_SIMON_STATE		116
#define INITIAL_SHIFT_MASK		0x1F
#define CPU_INITIAL_SHIFT_OFFSET	5
#define GPU_INITIAL_SHIFT_OFFSET	0
#define FUSE_CP_REV			0x190

#define FIXED_SCALE			14
#define TEMP_COEFF_A			-49   /* -0.003 * (1 << FIXED_SCALE) */
#define TEMP_COEFF_B			17449 /* 1.065 * (1 << FIXED_SCALE) */
#define TEGRA_SIMON_THRESHOLD		54067 /* 3.3% */

DEFINE_MUTEX(tegra_simon_fuse_lock);

static void initialize_cpu_isms(void)
{
	u32 buf[3];

	memset(buf, 0, sizeof(buf));

	/* FCCPLEX jtag_reset_clamp_en */
	apb2jtag_read(0x0C, 85, 3, buf);
	set_buf_bits(buf, 1, 83, 83);
	apb2jtag_write(0x0C, 85, 3, buf);
}

static void reset_cpu_isms(void)
{
	u32 buf[3];

	memset(buf, 0, sizeof(buf));

	/* FCCPLEX jtag_reset_clamp_en */
	apb2jtag_read(0x0C, 85, 3, buf);
	set_buf_bits(buf, 0, 83, 83);
	apb2jtag_write(0x0C, 85, 3, buf);
}

/*
 * Reads the CPU0 ROSC BIN ISM frequency.
 * Assumes VDD_CPU is ON.
 */
static u32 read_cpu0_ism(u32 mode, u32 duration, u32 div, u32 sel)
{
	u32 ret = 0;

	initialize_cpu_isms();

	ret = read_ism(mode, duration, div, sel,
			CCROC_MINI_CORE0_TOP_T132_ROSC_BIN_CPU_DENVER_FEU_BIN,
			CCROC_MINI_CORE0_TOP_T132_CHIPLET_SEL,
			CCROC_MINI_CORE0_TOP_T132_WIDTH,
			CCROC_MINI_CORE0_TOP_T132_ID);

	reset_cpu_isms();

	return ret;
}

static void initialize_gpu_isms(void)
{
	u32 buf[2];

	memset(buf, 0, sizeof(buf));

	/* A_GPU0 power_reset_n, get the reg out of reset */
	apb2jtag_read(0x25, 33, 2, buf);
	set_buf_bits(buf, 1, 1, 1);
	apb2jtag_write(0x25, 33, 2, buf);
}

static void reset_gpu_isms(void)
{
	u32 buf[2];

	memset(buf, 0, sizeof(buf));

	/*
	 * A_GPU0 power_reset_n, put back the reg in reset otherwise
	 * it will be in a bad state after the domain unpowergates
	 */
	apb2jtag_read(0x25, 33, 2, buf);
	set_buf_bits(buf, 0, 1, 1);
	apb2jtag_write(0x25, 33, 2, buf);
}

/*
 * Reads the GPU ROSC BIN ISM frequency.
 * Assumes VDD_GPU is ON.
 */
static u32 read_gpu_ism(u32 mode, u32 duration, u32 div, u32 sel)
{
	u32 ret = 0;

	initialize_gpu_isms();

	ret = read_ism(mode, duration, div, sel,
		E_TPC0_CLUSTER_BIN_T132_ROSC_BIN_GPU_ET0TX0A_GPCCLK_TEX_P00,
		E_TPC0_CLUSTER_BIN_T132_CHIPLET_SEL,
		E_TPC0_CLUSTER_BIN_T132_WIDTH,
		E_TPC0_CLUSTER_BIN_T132_ID);

	reset_gpu_isms();

	return ret;
}

struct volt_scale_entry {
	int mv;
	int scale;
};

static struct volt_scale_entry volt_scale_table[] = {
	[0] = {
		.mv = 800,
		.scale = 16384, /* 1 << FIXED_SCALE */
	},
	[1] = {
		.mv = 900,
		.scale = 11469, /* 0.7 * (1 << FIXED_SCALE) */
	},
	[2] = {
		.mv = 1000,
		.scale = 8356, /* 0.51 * (1 << FIXED_SCALE) */
	},
};

static s64 scale_voltage(int mv, s64 num)
{
	int i;
	s64 scale;

	for (i = 1; i < ARRAY_SIZE(volt_scale_table); i++) {
		if (volt_scale_table[i].mv >= mv)
			break;
	}

	/* Invalid voltage */
	WARN_ON(i == ARRAY_SIZE(volt_scale_table));
	if (i == ARRAY_SIZE(volt_scale_table)) {
		do_div(num, volt_scale_table[i - 1].scale);
		return num;
	}


	/* Interpolate/Extrapolate for exacte scale value */
	scale = (volt_scale_table[i].scale - volt_scale_table[i - 1].scale) /
		(volt_scale_table[i].mv - volt_scale_table[i - 1].mv);
	scale = scale * (mv - volt_scale_table[i - 1].mv) +
		volt_scale_table[i - 1].scale;

	do_div(num, scale);

	return num;
}

static s64 scale_temp(int temperature_mc, s64 num)
{
	/* num / (a * T + b) */
	int sign = 1;
	s64 scale = TEMP_COEFF_A;
	scale = scale * temperature_mc;
	if (scale < 0) {
		sign = -1;
		scale = scale * sign;
	}
	do_div(scale, 1000);
	scale = scale * sign;
	scale += TEMP_COEFF_B;

	do_div(num, scale);

	return num;
}

#define FUSE_TIMEOUT		20

static u32 get_tegra_simon_fuse(void)
{
	u32 ctrl_state = ~FUSE_CTRL_STATE_IDLE;
	int timeout = 0;
	u32 reg;

	mutex_lock(&tegra_simon_fuse_lock);

	/* Wait for fuse controller to go idle */
	while (ctrl_state != FUSE_CTRL_STATE_IDLE && timeout < FUSE_TIMEOUT) {
		ctrl_state = tegra_fuse_readl(FUSE_CTRL);
		ctrl_state >>= FUSE_CTRL_STATE_OFFSET;
		ctrl_state &= FUSE_CTRL_STATE_MASK;
		msleep(50);
		timeout++;
	}

	if (timeout == FUSE_TIMEOUT) {
		mutex_unlock(&tegra_simon_fuse_lock);
		return 0;
	}

	/* Setup fuse to read */
	tegra_fuse_writel(FUSE_SIMON_STATE, FUSE_ADDR);
	reg = tegra_fuse_readl(FUSE_CTRL);
	reg = reg & ~(FUSE_CTRL_CMD_MASK << FUSE_CTRL_CMD_OFFSET);
	reg = reg | (FUSE_CTRL_CMD_READ << FUSE_CTRL_CMD_OFFSET);
	tegra_fuse_writel(reg, FUSE_CTRL);

	/* Wait for read to complete */
	ctrl_state = ~FUSE_CTRL_STATE_IDLE;
	timeout = 0;
	while (ctrl_state != FUSE_CTRL_STATE_IDLE && timeout < FUSE_TIMEOUT) {
		ctrl_state = tegra_fuse_readl(FUSE_CTRL);
		ctrl_state >>= FUSE_CTRL_STATE_OFFSET;
		ctrl_state &= FUSE_CTRL_STATE_MASK;
		msleep(50);
		timeout++;
	}

	if (timeout == FUSE_TIMEOUT) {
		mutex_unlock(&tegra_simon_fuse_lock);
		return 0;
	}

	reg = tegra_fuse_readl(FUSE_DATA);

	mutex_unlock(&tegra_simon_fuse_lock);

	return reg;
}

static bool is_rev_valid(void)
{
	u32 reg = tegra_fuse_readl(FUSE_CP_REV);
	u32 major = (reg >> 5) & 0x3f;
	u32 minor = reg & 0x1f;

	if (major || minor >= 12)
		return true;

	return false;
}

static s64 get_current_threshold(s64 ro29, s64 ro30, s64 initial_shift, int mv,
					int temperature)
{
	s64 shift = ro30;


	shift = (shift << FIXED_SCALE) * 100;
	do_div(shift, ro29);

	/* Normalize for voltage */
	shift = (shift << FIXED_SCALE);
	shift = scale_voltage(mv, shift);

	/* Normalize for temperature */
	shift = (shift << FIXED_SCALE);
	shift = scale_temp(temperature, shift);

	if (initial_shift) {
		initial_shift = (initial_shift << FIXED_SCALE) * 8;
		do_div(initial_shift, 31);
		initial_shift = initial_shift - (4 << FIXED_SCALE);
	}

	return shift - initial_shift;
}

static int grade_gpu_simon_domain(int domain, int mv, int temperature)
{
	u32 ro29, ro30;
	s64 cur_shift, initial_shift;

	if (domain != TEGRA_SIMON_DOMAIN_GPU)
		return 0;

	/* Older rev = Eng boards */
	if (!is_rev_valid())
		return 1;

	initial_shift = get_tegra_simon_fuse();

	/* Invalid fuse */
	if (!initial_shift) {
		pr_err("%s: Invalid fuse\n", __func__);
		return 0;
	}

	initial_shift = (initial_shift >> GPU_INITIAL_SHIFT_OFFSET) &
				INITIAL_SHIFT_MASK;

	ro29 = read_gpu_ism(0, 600, 3, 29);
	ro30 = read_gpu_ism(2, 3000, 0, 30);

	if (!ro29)
		return 0;

	cur_shift = get_current_threshold(ro29, ro30, initial_shift, mv,
						temperature);

	return cur_shift < TEGRA_SIMON_THRESHOLD;
}

static int grade_cpu_simon_domain(int domain, int mv, int temperature)
{
	u32 ro29, ro30;
	s64 cur_shift, initial_shift;

	if (domain != TEGRA_SIMON_DOMAIN_CPU)
		return 0;

	/* Older rev = Eng boards */
	if (!is_rev_valid())
		return 1;

	initial_shift = get_tegra_simon_fuse();

	/* Invalid fuse */
	if (!initial_shift) {
		pr_err("%s: Invalid fuse\n", __func__);
		return 0;
	}

	initial_shift = (initial_shift >> CPU_INITIAL_SHIFT_OFFSET) &
				INITIAL_SHIFT_MASK;

	ro29 = read_cpu0_ism(0, 600, 3, 29);
	ro30 = read_cpu0_ism(2, 3000, 0, 30);

	if (!ro29)
		return 0;

	cur_shift = get_current_threshold(ro29, ro30, initial_shift, mv,
						temperature);

	return cur_shift < TEGRA_SIMON_THRESHOLD;
}

static struct tegra_simon_grader_desc gpu_grader_desc = {
	.domain = TEGRA_SIMON_DOMAIN_GPU,
	.grading_mv_max = 850,
	.grading_temperature_min = 20000,
	.settle_us = 3000,
	.grade_simon_domain = grade_gpu_simon_domain,
};

static struct tegra_simon_grader_desc cpu_grader_desc = {
	.domain = TEGRA_SIMON_DOMAIN_CPU,
	.grading_rate_max = 850000000,
	.grading_temperature_min = 20000,
	.settle_us = 3000,
	.grade_simon_domain = grade_cpu_simon_domain,
};


#ifdef CONFIG_DEBUG_FS

static int fuse_show(struct seq_file *s, void *data)
{
	char buf[150];
	int ret;
	u32 fuse = get_tegra_simon_fuse();

	ret = snprintf(buf, sizeof(buf),
			"GPU Fuse: %u\nCPU Fuse: %u\nRaw: 0x%x\n",
			(fuse >> GPU_INITIAL_SHIFT_OFFSET) & INITIAL_SHIFT_MASK,
			(fuse >> CPU_INITIAL_SHIFT_OFFSET) &
			INITIAL_SHIFT_MASK,
			fuse);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int fuse_open(struct inode *inode, struct file *file)
{
	return single_open(file, fuse_show, inode->i_private);
}

static const struct file_operations fuse_fops = {
	.open		= fuse_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cpu_ism_show(struct seq_file *s, void *data)
{
	char buf[150];
	u32 ro29, ro30, ro30_2;
	int ret;

	ro29 = read_cpu0_ism(0, 600, 3, 29);
	ro30 = read_cpu0_ism(0, 600, 3, 30);
	ro30_2 = read_cpu0_ism(2, 3000, 0, 30);
	ret = snprintf(buf, sizeof(buf),
			"RO29: %u RO30: %u RO30_2: %u diff: %d\n",
			ro29, ro30, ro30_2, ro29 - ro30);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int cpu_ism_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpu_ism_show, inode->i_private);
}

static const struct file_operations cpu_ism_fops = {
	.open		= cpu_ism_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int gpu_ism_show(struct seq_file *s, void *data)
{
	char buf[150];
	u32 ro29, ro30, ro30_2;
	int ret;

	ro29 = read_gpu_ism(0, 600, 3, 29);
	ro30 = read_gpu_ism(0, 600, 3, 30);
	ro30_2 = read_gpu_ism(2, 3000, 0, 30);
	ret = snprintf(buf, sizeof(buf),
			"RO29: %u RO30: %u RO30_2: %u diff: %d\n",
			ro29, ro30, ro30_2, ro29 - ro30);
	if (ret < 0)
		return ret;

	seq_write(s, buf, ret);
	return 0;
}

static int gpu_ism_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpu_ism_show, inode->i_private);
}

static const struct file_operations gpu_ism_fops = {
	.open		= gpu_ism_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init debugfs_init(void)
{
	struct dentry *dfs_file, *dfs_dir;

	dfs_dir = debugfs_create_dir("tegra13_simon", NULL);
	if (!dfs_dir)
		return -ENOMEM;

	dfs_file = debugfs_create_file("cpu_ism", 0644, dfs_dir, NULL,
					&cpu_ism_fops);
	if (!dfs_file)
		goto err;
	dfs_file = debugfs_create_file("gpu_ism", 0644, dfs_dir, NULL,
					&gpu_ism_fops);
	if (!dfs_file)
		goto err;

	dfs_file = debugfs_create_file("fuses", 0644, dfs_dir, NULL,
					&fuse_fops);
	if (!dfs_file)
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dfs_dir);
	return -ENOMEM;
}
#endif

static int __init tegra13_simon_graders_init(void)
{
	tegra_simon_add_grader(&gpu_grader_desc);
	tegra_simon_add_grader(&cpu_grader_desc);

#ifdef CONFIG_DEBUG_FS
	debugfs_init();
#endif
	return 0;
}
late_initcall_sync(tegra13_simon_graders_init);
