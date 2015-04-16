/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>

#include <linux/scatterlist.h>
#include <linux/mmc/mmc.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/genhd.h>

#include <linux/uaccess.h>
#include <linux/errno.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/* <DTS2014010906937 h00211444 20140109 begin */
/* Enum of power state */
enum sd_type {
    SDHC = 0,
    SDXC,
};
/* DTS2014010906937 h00211444 20140109 end> */

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc high-speed SDR200";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		str = "3.30 V";
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		str = "1.80 V";
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		str = "1.20 V";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "signal voltage:\t%u (%s)\n", ios->chip_select, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

/* <DTS2014010906937 h00211444 20140109 begin */
static int mmc_sdxc_opt_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;

	if (mmc_card_ext_capacity(card))
	{
		*val = SDXC;
		printk(KERN_INFO "sd card SDXC type is detected\n");
		return 0;
	}
	*val = SDHC;
	printk(KERN_INFO "sd card SDHC type is detected\n");
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_sdxc_fops, mmc_sdxc_opt_get,
			NULL, "%llu\n");
/* DTS2014010906937 h00211444 20140109 end? */

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_get_card(card);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_put_card(card);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

#define EXT_CSD_STR_LEN 1025

static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
	u8 *ext_csd;
	int err, i;

	buf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free;
	}

	mmc_get_card(card);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_put_card(card);
	if (err)
		goto out_free;

	for (i = 0; i < 512; i++)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");
	BUG_ON(n != EXT_CSD_STR_LEN);

	filp->private_data = buf;
	kfree(ext_csd);
	return 0;

out_free:
	kfree(buf);
	kfree(ext_csd);
	return err;
}

static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
};

static int is_within_group(unsigned int addr, unsigned int addr_start, unsigned int size, unsigned int wp_group_size){
	unsigned int trimed_wp_group_size;

	trimed_wp_group_size = (addr_start + size) / wp_group_size * wp_group_size;

	if( (addr >= addr_start) && (addr <trimed_wp_group_size ) )
		return 0;
	else
		return 1;
}

static int mmc_wr_prot_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;

	filp->private_data = card;
	return 0;
}

static ssize_t mmc_wr_prot_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
#define PARTITION_NOT_PROTED 0
#define PARTITION_PROTED 1

	struct mmc_card *card = filp->private_data;

	//used for mmcrequest
	unsigned int wp_group_size;
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;

	void *data_buf;
	unsigned char buf[8];
	unsigned int addr = 0;
	unsigned int init_addr = 0;
	char line_buf[128];

	int i, j, k;
	unsigned char ch;
	unsigned char wp_flag;
	int len = 8;
	unsigned int loop_count = 0;
	unsigned int size = 0;
	unsigned int status_prot = PARTITION_NOT_PROTED;

	struct emmc_partition *p_emmc_partition;

	pr_info("[HW]: eMMC protect driver built on %s @ %s\n", __DATE__, __TIME__);

	p_emmc_partition = g_emmc_partition;
	for(i = 0; i < MAX_EMMC_PARTITION_NUM; i++){
		if(p_emmc_partition->flags == 0)
			break;

		if(strcmp(p_emmc_partition->name, "system")  == 0){
			addr = (unsigned int)(p_emmc_partition->start);
			size = (unsigned int)(p_emmc_partition->size_sectors);
			pr_info("[HW]:%s: partitionname = %s \n", __func__, p_emmc_partition->name);
			pr_info("[HW]:%s: partition start from = 0x%08x \n", __func__, addr);
			pr_info("[HW]:%s: partition size = 0x%08x \n", __func__, size);
		}
		p_emmc_partition++;
	}

	init_addr = addr;

	if(addr < 0)
	{
		pr_err("[HW]:%s:invalid addr = 0x%08x.", __func__, addr);
		if(copy_to_user(ubuf, "fail", strlen("fail "))){
			pr_info("[HW]: %s: copy to user error \n", __func__);
			return -EFAULT;;
		}
		return -1;
	}

	wp_group_size =(512 * 1024) * card->ext_csd.raw_hc_erase_gap_size
		* card->ext_csd.raw_hc_erase_grp_size/512;

	if(addr % wp_group_size == 0){

	}else{
		addr = (addr / wp_group_size) * wp_group_size + wp_group_size;
		pr_info("[HW]:%s: setting start area is not muti size of wp_group_size\n", __func__);
	}

	loop_count = (init_addr + size - addr) / wp_group_size;

	pr_info("[HW]:%s: EXT_CSD_HC_WP_GRP_SIZE = 0x%02x. \n", __func__, card->ext_csd.raw_hc_erase_gap_size);
	pr_info("[HW]:%s: EXT_CSD_HC_ERASE_GRP_SIZE = 0x%02x. \n", __func__, card->ext_csd.raw_hc_erase_grp_size);

	pr_info("[HW]:%s: addr = 0x%08x, wp_group_size=0x%08x, size = 0x%08x \n",__func__, addr, wp_group_size, size);
	pr_info("[HW]:%s: loop_count = 0x%08x \n",__func__, loop_count);

	/* dma onto stack is unsafe/nonportable, but callers to this
	 * routine normally provide temporary on-stack buffers ...
	 */
	addr = addr - wp_group_size * 32;
	for(k=0; k< loop_count/32 + 2; k++){
		data_buf = kmalloc(32, GFP_KERNEL); //dma size 32
		if (data_buf == NULL)
			return -ENOMEM;

		mrq.cmd = &cmd;
		mrq.data = &data;

		cmd.opcode = 31;
		cmd.arg = addr;
		cmd.flags =  MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = len;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.sg = &sg;
		data.sg_len = 1;

		sg_init_one(&sg, data_buf, len);
		mmc_set_data_timeout(&data, card);
		mmc_claim_host(card->host);
		mmc_wait_for_req(card->host, &mrq);
		mmc_release_host(card->host);

		memcpy(buf, data_buf, len);
		kfree(data_buf);

/*
 *		start to show the detailed read status from the response
*/
#if 0
		for(i = 0; i < 8; i++){
			pr_info("[HW]:%s: buffer = 0x%02x \n", __func__, buf[i]);
		}
#endif
/*
 *		end of show the detailed read status from the response
*/

		for(i = 7; i >= 0; i--)
		{
			ch = buf[i];
			for(j = 0; j < 4; j++)
			{
				wp_flag = ch & 0x3;
				memset(line_buf, 0x00, sizeof(line_buf));
				sprintf(line_buf, "[0x%08x~0x%08x] Write protection group is ", addr, addr + wp_group_size - 1);

				switch(wp_flag)
				{
					case 0:
						strcat(line_buf, "disable");
						break;

					case 1:
						strcat(line_buf, "temporary write protection");
						break;

					case 2:
						strcat(line_buf, "power-on write protection");
						break;

					case 3:
						strcat(line_buf, "permanent write protection");
						break;

					default:
						break;
				}

				pr_info("%s: %s\n", mmc_hostname(card->host), line_buf);

				if( wp_flag == 1){
					if(is_within_group(addr, init_addr, size, wp_group_size) == 0){
						status_prot = PARTITION_PROTED;
						// pr_info("[HW]: %s: addr = 0x%08x, init_addr = 0x%08x, size = 0x%08x, group protected \n", __func__, addr, init_addr, size);
					}
				}
				addr += wp_group_size;
				ch = ch >> 2;
			}
		}
	}

	pr_info("[HW]: %s: end sector = 0x%08x \n", __func__, size + init_addr);

	if (cmd.error)
	{
		pr_err("[HW]:%s:cmd.error=%d.", __func__, cmd.error);
		if(copy_to_user(ubuf, "fail", strlen("fail "))){
			pr_info("[HW]: %s: copy to user error \n", __func__);
			return -EFAULT;;
		}
		return cmd.error;
	}

	if (data.error)
	{
		pr_err("[HW]:%s:data.error=%d.", __func__, data.error);
		if(copy_to_user(ubuf, "fail", strlen("fail "))){
			pr_info("[HW]: %s: copy to user error \n", __func__);
			return -EFAULT;;
		}
		return data.error;
	}

	switch(status_prot){
		case PARTITION_PROTED:
			if(copy_to_user(ubuf, "protected", strlen("protected "))){
				pr_info("[HW]: %s: copy to user error \n", __func__);
				return -EFAULT;;
			}
			pr_info("[HW]: %s: protected \n", __func__);
		break;

		case PARTITION_NOT_PROTED:
			if(copy_to_user(ubuf, "not_protected", strlen("not_protected "))){
				pr_info("[HW]: %s: copy to user error \n", __func__);
				return -EFAULT;;
			}
			pr_info("[HW]: %s: not_protected \n", __func__);
		break;

		default:break;
	}
	return 0;
}
// extern struct partition partitions[];
static ssize_t mmc_wr_prot_write(struct file *filp,
		const char __user *ubuf, size_t cnt,
		loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	unsigned int wp_group_size;
	unsigned int set_clear_wp, status;
	int ret, i;
	unsigned int addr = 0;
	unsigned int init_addr = 0;
	unsigned int loop_count = 0;
	unsigned int size = 0;
	char *cmd_buffer;

	// struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};

	struct emmc_partition *p_emmc_partition;

	pr_info("[HW]: eMMC protect driver built on %s @ %s\n", __DATE__, __TIME__);

	if( ubuf == NULL){
		pr_info("[HW]:%s: NULL pointer \n", __func__);
		return -1;
	}

	cmd_buffer = kmalloc(sizeof(char)*cnt, GFP_KERNEL);
	if (cmd_buffer == NULL) {
		return -ENOMEM;
	}
	memset(cmd_buffer, 0, sizeof(char)*cnt);

	if(copy_from_user(cmd_buffer, ubuf, cnt)){
		kfree(cmd_buffer);
		return -EFAULT;
	}

	pr_info("[HW]:%s: input arg = %s, cnt = %d \n", __func__, cmd_buffer, cnt);

	if(strncmp(cmd_buffer, "disable_prot", strlen("disable_prot")) == 0){
		set_clear_wp = 0;
	}else if(strncmp(cmd_buffer, "enable_prot", strlen("enable_prot")) == 0){
		set_clear_wp = 1;
	}
	else{
		kfree(cmd_buffer);
		return -1;
	}

	// mrq.cmd = &cmd;
	// mrq.data = &data;

	wp_group_size =(512 * 1024) * card->ext_csd.raw_hc_erase_gap_size
		* card->ext_csd.raw_hc_erase_grp_size / 512;

	p_emmc_partition = g_emmc_partition;
	for(i = 0; i < MAX_EMMC_PARTITION_NUM; i++){
		if(p_emmc_partition->flags == 0)
			break;
		if(strcmp(p_emmc_partition->name, "system") == 0){
			addr = (unsigned int)(p_emmc_partition->start);
			size = (unsigned int)(p_emmc_partition->size_sectors);
			pr_info("[HW]:%s: partitionname = %s \n", __func__, p_emmc_partition->name);
			pr_info("[HW]:%s: partition start from = 0x%08x \n", __func__, addr);
			pr_info("[HW]:%s: partition size = 0x%08x \n", __func__, size);
			break;
		}
		p_emmc_partition++;
	}

	if(strcmp(p_emmc_partition->name, "")  == 0){
		pr_info("[HW]:%s: can not find partition system \n", __func__);
		kfree(cmd_buffer);
		return -1;
	}

	pr_info("[HW]:%s: card->ext_csd.raw_hc_erase_gap_size = 0x%02x,  card->ext_csd.raw_hc_erase_grp_size = 0x%02x \n", __func__, \
			card->ext_csd.raw_hc_erase_gap_size, card->ext_csd.raw_hc_erase_grp_size);

	pr_info("[HW]:%s, size = 0x%08x, wp_group_size = 0x%08x, unit is block \n", \
			__func__, size, wp_group_size);
	if (wp_group_size == 0) {
		pr_info("[HW]:%s:invalid wp_group_size=0x%08x.", __func__, wp_group_size);
		kfree(cmd_buffer);
		return -2;
	}

	init_addr = addr;

	if(addr % wp_group_size == 0){

	}else{
		addr = (addr / wp_group_size) * wp_group_size + wp_group_size;
		pr_info("[HW]:%s: setting start area is not muti size of wp_group_size\n", __func__);
	}

	loop_count = (init_addr + size - addr) / wp_group_size;

	pr_info("[HW]:%s:prot_start_sec_addr = 0x%08x \n", __func__, addr);
	pr_info("[HW]:%s:loop_count = %x \n", __func__, loop_count);

	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;

	if (set_clear_wp){
		cmd.opcode = MMC_SET_WRITE_PROT;
	}else{
		cmd.opcode = MMC_CLR_WRITE_PROT;
	}

	for (i = 0; i < loop_count; i++) {

		/* Sending CMD28 for each WP group size
		   address is in sectors already */

		cmd.arg = addr + (i * wp_group_size);
		pr_info("[HW:%s:loop_count = %d, cmd.arg = 0x%08x, cmd.opcode = %d, \n", __func__, i, cmd.arg, cmd.opcode);

		mmc_claim_host(card->host);
		ret = mmc_wait_for_cmd(card->host, &cmd, 3);
		mmc_release_host(card->host);

		if (ret) {
			pr_err("[HW]:%s:mmc_wait_for_cmd return err = %d \n", __func__, ret);
			kfree(cmd_buffer);
			return -3;
		}

		/* Sending CMD13 to check card status */
		do {
			mmc_claim_host(card->host);
			ret = mmc_send_status(card, &status);
			mmc_release_host(card->host);
			if (R1_CURRENT_STATE(status) == R1_STATE_TRAN)
				break;
		}while ((!ret) && (R1_CURRENT_STATE(status) == R1_STATE_PRG));

		if (ret) {
			pr_err("[HW]:%s: mmc_send_status return err = %d \n", __func__, ret);
			kfree(cmd_buffer);
			return -4;
		}
	}

	pr_info("[HW]: %s: end sector = 0x%08x \n", __func__, size + init_addr);
	pr_info("[HW]: %s: size = 0x%08x \n", __func__,  size);
	kfree(cmd_buffer);
	return size;
}

static const struct file_operations mmc_dbg_wr_prot_fops = {
	.open		= mmc_wr_prot_open,
	.read		= mmc_wr_prot_read,
	.write		= mmc_wr_prot_write,
};

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root = NULL;
	struct dentry   *sdxc_root = NULL;

	if (!host->debugfs_root)
		return;

        /* <DTS2014010906937 h00211444 20140109 begin */
        sdxc_root = debugfs_create_dir("sdxc_root", host->debugfs_root);
        if (IS_ERR(sdxc_root))
            return;
        if (!sdxc_root)
            goto err;
        /* DTS2014010906937 h00211444 20140109 end> */

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

        /* <DTS2014010906937 h00211444 20140109 begin */
        card->debugfs_sdxc = sdxc_root;
        /* DTS2014010906937 h00211444 20140109 end> */
	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

        /* <DTS2014010906937 h00211444 20140109 begin */
	if (mmc_card_sd(card))
		if (!debugfs_create_file("sdxc", S_IRUSR, sdxc_root, card,
					&mmc_sdxc_fops))
			goto err;
        /* DTS2014010906937 h00211444 20140109 end> */

	if (mmc_card_mmc(card))
	if (!debugfs_create_file("wr_prot", S_IFREG|S_IRWXU|S_IRGRP|S_IROTH, root, card, &mmc_dbg_wr_prot_fops))
		goto err;

	return;

err:
	debugfs_remove_recursive(root);
        /* <DTS2014010906937 h00211444 20140109 begin */
	debugfs_remove_recursive(sdxc_root);
	card->debugfs_root = NULL;
	card->debugfs_sdxc = NULL;
        /* DTS2014010906937 h00211444 20140109 end> */
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
	/* <DTS2014010906937 h00211444 20140109 begin */
	debugfs_remove_recursive(card->debugfs_sdxc);
	/* DTS2014010906937 h00211444 20140109 end> */
}

