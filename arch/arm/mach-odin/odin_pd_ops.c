/* arch/arm/mach-odin/odin_pd_ops.c
 *
 * ODIN PM domain IPC driver
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>

#include <linux/debugfs.h>

static struct dentry *efuse_data_stats_dentry;

/* global value */
static unsigned int efuse_data_device;
static unsigned int efuse_data_oem;
static unsigned int efuse_data_lcs;

int odin_pd_off(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PD << PD_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (PD_OFF);
	mb_data[2] = id;

#ifdef CONFIG_AUD_RPM
	ret = mailbox_call(IN_ATOMIC_IPC_CALL,
			sub_block + MB_COMPLETION_PD_BASE, mb_data, 1000);
#else
	ret = mailbox_call(FAST_IPC_CALL, sub_block + MB_COMPLETION_PD_BASE,
			mb_data, 1000);
#endif

	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
		pr_err("  odin_pd_off: sub_block: %5d, id: %5d ", sub_block, id);
	}

	return ret;
}

int odin_pd_on(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PD << PD_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (PD_ON);
	mb_data[2] = id;

#ifdef CONFIG_AUD_RPM
	ret = mailbox_call(IN_ATOMIC_IPC_CALL,
			sub_block + MB_COMPLETION_PD_BASE, mb_data, 1000);
#else
	ret = mailbox_call(FAST_IPC_CALL, sub_block + MB_COMPLETION_PD_BASE,
			mb_data, 1000);
#endif

	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
		pr_err("  odin_pd_on:  sub_block: %5d, id: %5d ", sub_block, id);
	}

	return ret;
}

int odin_power_domain_off(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PD << PD_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (PD_OFF);
	mb_data[2] = id;
	mb_data[3] = PD_FRAME_WORK;
	ret = ipc_call_fast(mb_data);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_power_domain_on(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PD << PD_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (PD_ON);
	mb_data[2] = id;
	mb_data[3] = PD_FRAME_WORK;
	ret = ipc_call_fast(mb_data);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

/* memory thermal control */
int odin_mem_thermal_ctl(void)
{
	int ret;
	int mb_data[7] = {0, };

	/* printk("\n odin_mem_thermal_ctl \n"); */
	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_MEMORY & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET );

	/* ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_THERMAL, mb_data, 1000); */
	ret = ipc_call_fast(mb_data);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

/* Kernel mailbox - hdmi driver key request */
int odin_ses_hdmi_key_request(void)
{
	int ret;
	int mb_data[7] = {0, };

	pr_debug("\n ** TRUE: odin_ses_hdmi_key_request : start. \n");
	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_SES & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(SES_HDMI_KEY_REQUEST);

	/* ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_DSS, mb_data, 1000); */
	ret = ipc_call_fast(mb_data);
	pr_debug("\n ** TRUE: odin_ses_hdmi_key_request : finished. \n");

	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

/* Kernel mailbox - hdmi driver key fail */
int odin_ses_hdmi_key_fail(void)
{
	int ret;
	int mb_data[7] = {0, };

	pr_debug("\n ** FAIL: odin_ses_hdmi_key_fail() \n");
	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_SES & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(SES_HDMI_KEY_FAIL);

	/* ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_DSS, mb_data, 1000); */
	ret = ipc_call_fast(mb_data);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

/* Kernel Mailbox - LPA_DSP_PLL_CTL */
int odin_lpa_dsp_pll_ctl(unsigned int dsp_pll_ctl)
{
	int ret;
	int mb_data[7] = {0, };

	printk("odin_lpa_dsp_pll_ctl(), dsp_pll_ctl: %x \n", dsp_pll_ctl);
	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_LPA & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(LPA_DSP_PLL_CTL);
	mb_data[2] = dsp_pll_ctl;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

/* Kernel Mailbox - eFUSE READ */
int odin_efuse_read_device(void)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_EFUSE & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(EFUSE_READ_DEVICE);

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_EFUSE, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	/* printk("odin_eFUSE_read_device(): %d \n", mb_data[2]); */

	efuse_data_device = mb_data[2];

	return ret;
}

/* Kernel Mailbox - eFUSE READ */
int odin_efuse_read_oem(void)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_EFUSE & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(EFUSE_READ_OEM);

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_EFUSE, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	/* printk("odin_eFUSE_read_oem(): %d \n", mb_data[2]); */

	efuse_data_oem = mb_data[2];

	return ret;
}

/* Kernel Mailbox - eFUSE READ */
int odin_efuse_read_life_cycle(void)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PM_SUB_BLOCK << PD_OFFSET) |
		( (PM_SUB_BLOCK_EFUSE & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(EFUSE_READ_LIFE_CYCLE);

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_EFUSE, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	/* printk("odin_eFUSE_read_life_cycle(): %d \n", mb_data[2]); */

	efuse_data_lcs = mb_data[2];

	return ret;
}


static int efuse_data_stats_show_device(struct seq_file *m, void *unused)
{
	odin_efuse_read_device();
	seq_printf(m, "%x\n", efuse_data_device);
	return 0;
}

static int efuse_data_stats_show_oem(struct seq_file *m, void *unused)
{
	odin_efuse_read_oem();
	seq_printf(m, "%x\n", efuse_data_oem);
	return 0;
}

static int efuse_data_stats_show_lcs(struct seq_file *m, void *unused)
{
	odin_efuse_read_life_cycle();
	seq_printf(m, "%x\n", efuse_data_lcs);
	return 0;
}

static int efuse_data_stats_open_device(struct inode *inode, struct file *file)
{
	return single_open(file, efuse_data_stats_show_device, NULL);
}

static int efuse_data_stats_open_oem(struct inode *inode, struct file *file)
{
	return single_open(file, efuse_data_stats_show_oem, NULL);
}

static int efuse_data_stats_open_lcs(struct inode *inode, struct file *file)
{
	return single_open(file, efuse_data_stats_show_lcs, NULL);
}

static const struct file_operations efuse_data_stats_fops_device = {
	.owner = THIS_MODULE,
	.open = efuse_data_stats_open_device,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations efuse_data_stats_fops_oem= {
	.owner = THIS_MODULE,
	.open = efuse_data_stats_open_oem,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations efuse_data_stats_fops_lcs= {
	.owner = THIS_MODULE,
	.open = efuse_data_stats_open_lcs,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init efuse_data_debugfs_init(void)
{
	printk("efuse_data_debugfs_init() \n");

	efuse_data_stats_dentry = debugfs_create_file("efuse_data_device",
			S_IRUGO, NULL, NULL, &efuse_data_stats_fops_device);
	efuse_data_stats_dentry = debugfs_create_file("efuse_data_oem",
			S_IRUGO, NULL, NULL, &efuse_data_stats_fops_oem);
	efuse_data_stats_dentry = debugfs_create_file("efuse_data_lcs",
			S_IRUGO, NULL, NULL, &efuse_data_stats_fops_lcs);

	return 0;
}
postcore_initcall(efuse_data_debugfs_init);


MODULE_DESCRIPTION("ODIN PM Domain IPC Driver");
MODULE_LICENSE("GPL v2");
