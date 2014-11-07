/*
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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/cpu_pm.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/platform_data/gpio-odin.h>
#include <sound/pcm_params.h>
#include "odin_asoc_utils.h"

static void __iomem *aud_clk_base;

/* Next functions must be called by machine driver in ASoC */
int odin_get_aud_io_func(int base_num, int aud_gpio, int *value)
{
	int ret;
	int gpio_number;
	if (aud_gpio < AUD_GPIO_00 || aud_gpio > AUD_GPIO_43)
		return -EINVAL;

	gpio_number = base_num + aud_gpio;

	ret = odin_gpio_get_pinmux(gpio_number);

	if (ret == AUD_IO_IN || ret == AUD_IO_OUT) {
		*value = gpio_get_value(gpio_number);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(odin_get_aud_io_func);

int odin_set_aud_io_func(int base_num, int aud_gpio, int func, int value)
{
	int ret;
	int gpio_number;

	if (aud_gpio < AUD_GPIO_00 ||
	    aud_gpio > AUD_GPIO_43 ||
	    func < AUD_IO_IN ||
	    func > AUD_IO_IP1) {
		ret = -EINVAL;
		goto val_err;
	}

	gpio_number = base_num + aud_gpio;

	if (func == AUD_IO_OUT)
		pr_info("gpio %d try to set func %d & value %d\n", gpio_number,
								func, !!value);
	else
		pr_info("gpio %d try to set func %d\n", gpio_number, func);

	ret = gpio_request(gpio_number, "aud_gpio_set");
	if (ret < 0) {
		pr_err("Faile to request gpio %d\n", gpio_number);
		goto val_err;
	}

	ret = odin_gpio_set_pinmux(gpio_number, func);
	if (ret < 0)
		goto free_gpio;

	if (func == AUD_IO_IN) /* AUD_IO_FUNC0 */
		ret = gpio_direction_input(gpio_number);

	if (func == AUD_IO_OUT) /* AUD_IO_FUNC1 */
		ret = gpio_direction_output(gpio_number, !!value);

free_gpio:
	gpio_free(gpio_number);
val_err:
	if (ret < 0)
		pr_err("odin_set_aud_io_func op err %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(odin_set_aud_io_func);

int odin_of_get_base_bank_num(struct device *dev, int *aud_gpio_base)
{
	struct device_node *np = dev->of_node;
	struct device_node *pnode = NULL;
	int ret;

	if (!np)
		return 0;

	pnode = of_parse_phandle(np, "aud-gpio-base", 0);
	if (pnode) {
		ret = of_property_read_u32(pnode, "bank_num", aud_gpio_base);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(odin_of_get_base_bank_num);

int odin_of_dai_property_get(struct device *dev, struct snd_soc_card *card)
{
	struct snd_soc_dai_link *dai_link = NULL;
	struct device_node *np = dev->of_node;
	struct device_node *pnode = NULL;
	char strbuf[256] = { 0, };
	int i;

	if (!np)
		return 0;

	for (i = 0; i < card->num_links; i++) {
		dai_link = &card->dai_link[i];

		if (!dai_link) {
			dev_dbg(dev, "%d dai_link pass\n",i);
			continue;
		}

		sprintf(strbuf, "dai-link%d-cpu-dai", i);
		pnode = NULL;
		pnode = of_parse_phandle(np, strbuf, 0);
		if (pnode) {
			dev_dbg(dev, "Found %s pnode!", strbuf);
			dai_link->cpu_of_node = pnode;
			dai_link->cpu_name = NULL;
			dai_link->cpu_dai_name = NULL;
		}

		sprintf(strbuf, "dai-link%d-platform", i);
		pnode = NULL;
		pnode = of_parse_phandle(np, strbuf, 0);
		if (pnode) {
			dev_dbg(dev, "Found %s pnode!", strbuf);
			dai_link->platform_of_node = pnode;
			dai_link->platform_name = NULL;
		}

		sprintf(strbuf, "dai-link%d-codec", i);
		pnode = NULL;
		pnode = of_parse_phandle(np, strbuf, 0);
		if (pnode) {
			dev_dbg(dev, "Found %s pnode!", strbuf);
			dai_link->codec_of_node = pnode;
			dai_link->codec_name = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(odin_of_dai_property_get);

/**
 * Some ALSA internal helper functions
 */
int snd_interval_refine_set(struct snd_interval *i, unsigned int val)
{
	struct snd_interval t;
	t.empty = 0;
	t.min = t.max = val;
	t.openmin = t.openmax = 0;
	t.integer = 1;
	return snd_interval_refine(i, &t);
}

int _snd_pcm_hw_param_set(struct snd_pcm_hw_params *params,
				 snd_pcm_hw_param_t var, unsigned int val,
				 int dir)
{
	int changed;
	if (hw_is_mask(var)) {
		struct snd_mask *m = hw_param_mask(params, var);
		if (val == 0 && dir < 0) {
			changed = -EINVAL;
			snd_mask_none(m);
		} else {
			if (dir > 0)
				val++;
			else if (dir < 0)
				val--;
			changed = snd_mask_refine_set(
					hw_param_mask(params, var), val);
		}
	} else if (hw_is_interval(var)) {
		struct snd_interval *i = hw_param_interval(params, var);
		if (val == 0 && dir < 0) {
			changed = -EINVAL;
			snd_interval_none(i);
		} else if (dir == 0)
			changed = snd_interval_refine_set(i, val);
		else {
			struct snd_interval t;
			t.openmin = 1;
			t.openmax = 1;
			t.empty = 0;
			t.integer = 0;
			if (dir < 0) {
				t.min = val - 1;
				t.max = val;
			} else {
				t.min = val;
				t.max = val+1;
			}
			changed = snd_interval_refine(i, &t);
		}
	} else
		return -EINVAL;
	if (changed) {
		params->cmask |= 1 << var;
		params->rmask |= 1 << var;
	}
	return changed;
}



#if defined(CONFIG_DEBUG_FS)
struct aud_io_debugfs {
	struct dentry *aud_io_debugfs_root;
	struct dentry *aud_io_debugfs_file;
	int gpio_base_num;
};

struct aud_io_debugfs *aud_io_dfs = NULL;

static ssize_t odin_aud_dump_status(struct aud_io_debugfs *io_dfs, char *buf)
{
	ssize_t count = 0;
	int i, value, func;

	count += sprintf(buf, "Odin Audio IO Status\n");

	for (i = 0; i < AUD_GPIO_43+1; i++) {
		func = odin_get_aud_io_func(io_dfs->gpio_base_num, i, &value);

		if (func == AUD_IO_IN || func == AUD_IO_OUT) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					"AUD_GPIO_%02d-func: %d, value: %d\n",
					i, func, value);
		} else {
			count += snprintf(buf + count, PAGE_SIZE - count,
					"AUD_GPIO_%02d-func: %d\n",
					i, func);
		}

		if (count >= PAGE_SIZE - 1)
			break;
	}

	if (count >= PAGE_SIZE)
		count = PAGE_SIZE - 1;

	return count;
}

static ssize_t odin_aud_ios_read_file(struct file *file, char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct aud_io_debugfs *io_dfs = file->private_data;
	char *buf;
	ssize_t ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = odin_aud_dump_status(io_dfs, buf);
	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);

	return ret;
}

static ssize_t odin_aud_ios_write_file(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	struct aud_io_debugfs *io_dfs = file->private_data;
	char buf[32];
	ssize_t buf_size;
	char *start = buf;
	unsigned long aud_n, func;
	unsigned long val = 0;
	int ret;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size)) {
		pr_err("Failed to copy from user\n");
		return -EFAULT;
	}
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;

	aud_n = simple_strtoul(start, &start, 10);
	if (aud_n < AUD_GPIO_00 || aud_n > AUD_GPIO_43 ) {
		pr_err("Invalid audio gpio number, range to %d ~ %d\n",
						AUD_GPIO_00, AUD_GPIO_43);
		return -EINVAL;
	}

	while (*start == ' ')
		start++;

	func = simple_strtoul(start, &start, 10);
	if (func < AUD_IO_IN || func > AUD_IO_IP1) {
		pr_err("Invalid function, range to %d ~ %d\n",
						AUD_IO_IN, AUD_IO_IP1);
		return -EINVAL;
	}

	if (func == AUD_IO_OUT) {
		while (*start == ' ')
			start++;

		if (strict_strtoul(start, 10, &val))
			return -EINVAL;
	}

	ret = odin_set_aud_io_func(io_dfs->gpio_base_num, aud_n, func, !!val);
	if (ret < 0)
		return ret;

	return buf_size;
}

static const struct file_operations aud_ios_fops = {
	.open = simple_open,
	.read = odin_aud_ios_read_file,
	.write = odin_aud_ios_write_file,
};

static int odin_asoc_utils_notifier(struct notifier_block *self,
		unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_CLUSTER_PM_EXIT:
		/* Audio reset release */
		writel(0xffffffff , aud_clk_base + 0x100);
		writel(0xffffffff , aud_clk_base + 0x104);

		/* Audio Async PCLK setting */
		writel(0x0, aud_clk_base + 0x30);      /* Main clock source= OSC_CLK */
		writel(0x0, aud_clk_base + 0x38);      /* div= 0 */
		writel(0x1, aud_clk_base + 0x3c);      /* gate = clock enable */
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata odin_asoc_utils_notifier_block = {
	.notifier_call = odin_asoc_utils_notifier,
};

void odin_aud_io_debugfs_init(int aud_gpio_base)
{
	if (aud_gpio_base < 0) {
		pr_warn("Wrong gpio base number\n");
		return;
	}

	if (aud_io_dfs) {
		pr_warn("Already initialized aud_io debugfs\n");
		return;
	}

	aud_io_dfs = kmalloc(sizeof(struct aud_io_debugfs), GFP_KERNEL);
	if (!aud_io_dfs) {
		pr_warn("Failed to alloc memory for aud_io debugfs\n");
		return;
	}

	aud_io_dfs->gpio_base_num = aud_gpio_base;
	aud_io_dfs->aud_io_debugfs_root = debugfs_create_dir("aud_io", NULL);
	if (!aud_io_dfs->aud_io_debugfs_root) {
		pr_warn("Failed to create aud_io debugfs root\n");
		return;
	}

	aud_io_dfs->aud_io_debugfs_file = debugfs_create_file("aud_ios",
					0400, aud_io_dfs->aud_io_debugfs_root,
					aud_io_dfs, &aud_ios_fops);

	pr_info("odin_aud_io_debugfs init\n");
	return;
}
EXPORT_SYMBOL_GPL(odin_aud_io_debugfs_init);

void odin_aud_io_debugfs_exit(void)
{
	if (aud_io_dfs) {
		debugfs_remove_recursive(aud_io_dfs->aud_io_debugfs_root);
		kfree(aud_io_dfs);
		aud_io_dfs = NULL;
	}
	return;
}
EXPORT_SYMBOL_GPL(odin_aud_io_debugfs_exit);
#endif

static int __init
odin_asoc_utils_init(void)
{
	int ret;

	aud_clk_base = ioremap(0x34670000, SZ_4K);

	ret = cpu_pm_register_notifier(&odin_asoc_utils_notifier_block);

	return 0;
}
static void __exit
odin_asoc_utils_exit(void)
{
	iounmap(aud_clk_base);
	cpu_pm_unregister_notifier(&odin_asoc_utils_notifier_block);
}

module_init(odin_asoc_utils_init);
module_exit(odin_asoc_utils_exit);
