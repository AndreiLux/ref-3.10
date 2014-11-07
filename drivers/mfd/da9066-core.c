/*
 * da9066-core.c  --  Device access for Dialog DA9066
 *
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd. D. Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/bug.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>

#include <linux/smp.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include <linux/mfd/da9066/da9066_version.h>
#include <linux/mfd/da9066/pmic.h>
#include <linux/mfd/da9066/da9066_reg.h>
#include <linux/mfd/da9066/rtc.h>
#include <linux/mfd/da9066/core.h>

#include "../../fs/proc/internal.h"

#if 0
#define DA9066_REG_DEBUG
#define DA9066_SUPPORT_I2C_HIGH_SPEED
#endif

typedef struct {
	unsigned long reg;
	unsigned short val;
} pmu_reg;


#define DA9066_PROC_NAME "pmu1"

#ifdef DA9066_REG_DEBUG
#define DA9066_MAX_HISTORY           	100
#define DA9066_HISTORY_READ_OP       	0
#define DA9066D_HISTORY_WRITE_OP      	1

#define da9066_write_reg_cache(reg,data)   gDA9066RegCache[reg] = data;

struct da9066_reg_history{
	u8 mode;
	u8 regnum;
	u8 value;
	long long time;
};

static u8 gDA9066RegCache[DA9066_MAX_REGISTER_CNT];
static u8 gDA9066CurHistory=0;
static struct da9066_reg_history gDA9066RegHistory[DA9066_MAX_HISTORY];
#endif /* DA9066_REG_DEBUG */

/*
 *   Static global variable
 */
static struct da9066 *da9066_dev_info;

/*
 * DA9066 Device IO
 */

#ifdef DA9066_REG_DEBUG
void da9066_write_reg_history(u8 opmode,u8 reg,u8 data) {

	if (gDA9066CurHistory == DA9066_MAX_HISTORY)
		gDA9066CurHistory=0;

	gDA9066RegHistory[gDA9066CurHistory].mode=opmode;
	gDA9066RegHistory[gDA9066CurHistory].regnum=reg;
	gDA9066RegHistory[gDA9066CurHistory].value=data;
	gDA9066CurHistory++;
}
#endif /* DA9066_REG_DEBUG */

static struct resource da9066_regulators_resources[] = {
	{
		.name	= "DA9066_LIM",
		.start	= 1,
		.end	= 1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct mfd_cell da9066_devs[] = {
	{
		.name	= DA9066_DRVNAME_REGULATORS,
		.num_resources  = ARRAY_SIZE(da9066_regulators_resources),
		.resources  = da9066_regulators_resources,
	},
};

/*
 *
 */
static int da9066_read(struct da9066 *da9066, u8 reg)
{
	unsigned int val;
	int ret;

	if (reg > DA9066_MAX_REGISTER_CNT) {
		dev_err(da9066->dev, "Invalid reg %x\n", reg);
		return -EINVAL;
	}

	/* Actually read it out */
	ret = regmap_read(da9066->regmap, reg, &val);
	if (ret < 0)
		return ret;

	return val;
}

static int da9066_bulk_read(struct da9066 *da9066, u8 reg,
				int num_regs, u8 dest)
{
	int bytes = num_regs;
	unsigned int val;

	if ((reg + num_regs - 1) > DA9066_MAX_REGISTER_CNT) {
		dev_err(da9066->dev, "Invalid reg %x\n", reg + num_regs - 1);
		return -EINVAL;
	}

	/* Actually read it out */
	return regmap_bulk_read(da9066->regmap, reg, bytes, dest);
}
/*
 *
 */
static int da9066_write(struct da9066 *da9066, u8 reg, u8 src)
{
	if (reg > DA9066_MAX_REGISTER_CNT) {
		dev_err(da9066->dev, "Invalid reg %x\n", reg);
		return -EINVAL;
	}
	/* Actually write it out */
	return regmap_write(da9066->regmap, reg, src);
}

static int da9066_bulk_write(struct da9066 *da9066, u8 reg,
				int num_regs, u8 src)
{
	int bytes = num_regs;

	if (da9066->write_dev == NULL)
		return -ENODEV;

	if ((reg + num_regs - 1) > DA9066_MAX_REGISTER_CNT) {
		dev_err(da9066->dev, "Invalid reg %x\n", reg + num_regs - 1);
		return -EINVAL;
	}
	/* Actually write it out */
	return regmap_bulk_write(da9066->regmap, reg, bytes, src);
}
/*
 * da9066_clear_bits -
 * @ da9066 :
 * @ reg :
 * @ mask :
 *
 */
int da9066_clear_bits(struct da9066 * const da9066, u8 const reg, u8 const mask)
{
	u8 data;
	int err, ret;

	mutex_lock(&da9066->da9066_io_mutex);
	ret = da9066_read(da9066, reg);
	if (ret < 0) {
		dev_err(da9066->dev, "read from reg R%d failed\n", reg);
		goto out;
	}
#ifdef DA9066_REG_DEBUG
	else
		da9066_write_reg_history(DA9066_HISTORY_READ_OP, reg, data);
#endif
	ret &= ~mask;
	ret = da9066_write(da9066, reg, ret);
	if (ret < 0)
		dlg_err("write to reg R%d failed\n", reg);
#ifdef DA9066_REG_DEBUG
	else  {
		da9066_write_reg_history(DA9066D_HISTORY_WRITE_OP, reg, data);
		da9066_write_reg_cache(reg,data);
	}
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
out:
	mutex_unlock(&da9066->da9066_io_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(da9066_clear_bits);

int da9066_reg_update(struct da9066 * const da9066, u8 reg, u8 mask, u8 val)
{
	return regmap_update_bits(da9066->regmap, reg ,mask, val);
}
EXPORT_SYMBOL_GPL(da9066_reg_update);

/*
 * da9066_set_bits -
 * @ da9066 :
 * @ reg :
 * @ mask :
 *
 */
int da9066_set_bits(struct da9066 * const da9066, u8 const reg, u8 const mask)
{
	unsigned int data;

	mutex_lock(&da9066->da9066_io_mutex);
	data = da9066_read(da9066, reg);
#ifdef DA9066_REG_DEBUG
	else {
		da9066_write_reg_history(DA9066_HISTORY_READ_OP, reg, data);
	}
#endif
	data |= mask;
	data = da9066_write(da9066, reg, data);
#ifdef DA9066_REG_DEBUG
	else  {
		da9066_write_reg_history(DA9066D_HISTORY_WRITE_OP, reg, data);
		da9066_write_reg_cache(reg, data);
	}
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
out:
	mutex_unlock(&da9066->da9066_io_mutex);

	return data;
}
EXPORT_SYMBOL_GPL(da9066_set_bits);


/*
 * da9066_reg_read -
 * @ da9066 :
 * @ reg :
 * @ *dest :
 *
 */
int da9066_reg_read(struct da9066 * const da9066, u8 const reg, u8 *dest)
{
	u8 data;
	int err;

	mutex_lock(&da9066->da9066_io_mutex);
	err = da9066_read(da9066, reg);
	if (err < 0)
		dlg_err("read from reg 0x%x failed\n", reg);
#ifdef DA9066_REG_DEBUG
	else
		da9066_write_reg_history(DA9066_HISTORY_READ_OP, reg, err);
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(da9066_reg_read);


/*
 * da9066_reg_write -
 * @ da9066 :
 * @ reg :
 * @ val :
 *
 */
int da9066_reg_write(struct da9066 * const da9066, u8 const reg, u8 const val)
{
	int ret;
	u8 data = val;

	mutex_lock(&da9066->da9066_io_mutex);
	ret = da9066_write(da9066, reg, val);
	if (ret < 0)
		dlg_err("write to reg R%d failed\n", reg);
#ifdef DA9066_REG_DEBUG
	else  {
		da9066_write_reg_history(DA9066D_HISTORY_WRITE_OP, reg, ret);
		da9066_write_reg_cache(reg, ret);
	}
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(da9066_reg_write);


/*
 * da9066_block_read -
 * @ da9066 :
 * @ start_reg :
 * @ regs :
 * @ *dest :
 *
 */
int da9066_block_read(struct da9066 * const da9066, u8 const start_reg,
			u8 const regs, u8 * const dest)
{
	int err = 0;
#ifdef DA9066_REG_DEBUG
	int i;
#endif

	mutex_lock(&da9066->da9066_io_mutex);
	err = da9066_bulk_read(da9066, start_reg, regs, dest);
	if (err != 0)
		dlg_err("block read starting from R%d failed\n", start_reg);
#ifdef DA9066_REG_DEBUG
	else {
		for (i=0; i<regs; i++)
			da9066_write_reg_history(DA9066D_HISTORY_WRITE_OP,start_reg+i,*(dest+i));
	}
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(da9066_block_read);


/*
 * da9066_block_write -
 * @ da9066 :
 * @ start_reg :
 * @ regs :
 * @ *src :
 *
 */
int da9066_block_write(struct da9066 * const da9066, u8 const start_reg,
			u8 const regs, u8 * const src)
{
	int ret = 0;
#ifdef DA9066_REG_DEBUG
	int i;
#endif

	mutex_lock(&da9066->da9066_io_mutex);
	ret = da9066_bulk_write(da9066, start_reg, regs, src);
	if (ret != 0)
		dlg_err("block write starting at R%d failed\n", start_reg);
#ifdef DA9066_REG_DEBUG
	else {
		for (i=0; i<regs; i++)
			da9066_write_reg_cache(start_reg+i,*(src+i));
	}
#endif
	mutex_unlock(&da9066->da9066_io_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(da9066_block_write);

/*
 * Register a client device.  This is non-fatal since there is no need to
 * fail the entire device init due to a single platform device failing.
 */
static void da9066_client_dev_register(struct da9066 *da9066,
				       const char *name,
				       struct platform_device **pdev)
{
	int ret;

	*pdev = platform_device_alloc(name, -1);
	if (*pdev == NULL) {
		dev_err(da9066->dev, "Failed to allocate %s\n", name);
		return;
	}

	(*pdev)->dev.parent = da9066->dev;
	platform_set_drvdata(*pdev, da9066);
	ret = platform_device_add(*pdev);
	if (ret != 0) {
		dev_err(da9066->dev, "Failed to register %s: %d\n", name, ret);
		platform_device_put(*pdev);
		*pdev = NULL;
	}
}

/*
 * da9066_system_event_handler
 */
static irqreturn_t da9066_system_event_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

/*
 * da9066_system_event_init
 */
static void da9066_system_event_init(struct da9066 *da9066)
{
	da9066_register_irq(da9066, DA9066_IRQ_EVDD_MON, da9066_system_event_handler,
                            0, "VDD MON", da9066);
}

/*****************************************/
/* 	Debug using proc entry           */
/*****************************************/
#ifdef CONFIG_PROC_FS
static int da9066_ioctl_open(struct inode *inode, struct file *file)
{
	dlg_info("%s\n", __func__);
	file->private_data = PDE(inode)->data;
	return 0;
}

int da9066_ioctl_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 * da9066_ioctl
 */
static long da9066_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct da9066 *da9066 =  file->private_data;
		pmu_reg reg;
		int ret = 0;
		u8 reg_val;

		if (!da9066)
			return -ENOTTY;

		switch (cmd) {

			case DA9066_IOCTL_READ_REG:
			if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
				return -EFAULT;

			ret = da9066_read(da9066, reg.reg);
			reg.val = (unsigned short)ret;
			if (copy_to_user((pmu_reg *)arg, &reg, sizeof(pmu_reg)) != 0)
				return -EFAULT;
			break;

		case DA9066_IOCTL_WRITE_REG:
			if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
				return -EFAULT;
			da9066_write(da9066, reg.reg, (u8 *)&reg.val);
			break;

		default:
			dlg_err("%s: unsupported cmd\n", __func__);
			return -ENOTTY;
		}

		return 0;
}

#define MAX_USER_INPUT_LEN      100
#define MAX_REGS_READ_WRITE     10

enum pmu_debug_ops {
	PMUDBG_READ_REG = 0UL,
	PMUDBG_WRITE_REG,
};

struct pmu_debug {
	int read_write;
	int len;
	int addr;
	u8 val[MAX_REGS_READ_WRITE];
};

/*
 * da9066_dbg_usage
 */
static void da9066_dbg_usage(void)
{
	printk(KERN_INFO "Usage:\n");
	printk(KERN_INFO "Read a register: echo 0x0800 > /proc/pmu1\n");
	printk(KERN_INFO
		"Read multiple regs: echo 0x0800 -c 10 > /proc/pmu1\n");
	printk(KERN_INFO
		"Write multiple regs: echo 0x0800 0xFF 0xFF > /proc/pmu1\n");
	printk(KERN_INFO
		"Write single reg: echo 0x0800 0xFF > /proc/pmu0\n");
	printk(KERN_INFO "Max number of regs in single write is :%d\n",
		MAX_REGS_READ_WRITE);
	printk(KERN_INFO "Register address is encoded as follows:\n");
	printk(KERN_INFO "0xSSRR, SS: i2c slave addr, RR: register addr\n");
}


/*
 * da9066_dbg_parse_args
 */
static int da9066_dbg_parse_args(char *cmd, struct pmu_debug *dbg)
{
	char *tok;                 /* used to separate tokens             */
	const char ct[] = " \t";   /* space or tab delimits the tokens    */
	bool count_flag = false;   /* whether -c option is present or not */
	int tok_count = 0;         /* total number of tokens parsed       */
	int i = 0;

	dbg->len        = 0;

	/* parse the input string */
	while ((tok = strsep(&cmd, ct)) != NULL) {
		dlg_info("token: %s\n", tok);

		/* first token is always address */
		if (tok_count == 0) {
			sscanf(tok, "%x", &dbg->addr);
		} else if (strnicmp(tok, "-c", 2) == 0) {
			/* the next token will be number of regs to read */
			tok = strsep(&cmd, ct);
			if (tok == NULL)
				return -EINVAL;

			tok_count++;
			sscanf(tok, "%d", &dbg->len);
			count_flag = true;
			break;
		} else {
			int val;

			/* this is a value to be written to the pmu register */
			sscanf(tok, "%x", &val);
			if (i < MAX_REGS_READ_WRITE) {
				dbg->val[i] = val;
				i++;
			}
		}

		tok_count++;
	}

	/* decide whether it is a read or write operation based on the
	 * value of tok_count and count_flag.
	 * tok_count = 0: no inputs, invalid case.
	 * tok_count = 1: only reg address is given, so do a read.
	 * tok_count > 1, count_flag = false: reg address and atleast one
	 *     value is present, so do a write operation.
	 * tok_count > 1, count_flag = true: to a multiple reg read operation.
	 */
	switch (tok_count) {
		case 0:
			return -EINVAL;
		case 1:
			dbg->read_write = PMUDBG_READ_REG;
			dbg->len = 1;
			break;
		default:
			if (count_flag == true) {
				dbg->read_write = PMUDBG_READ_REG;
			} else {
				dbg->read_write = PMUDBG_WRITE_REG;
				dbg->len = i;
		}
	}

	return 0;
}

/*
 * da9066_ioctl_write
 */
static ssize_t da9066_ioctl_write(struct file *file, const char __user *buffer,
	size_t len, loff_t *offset)
{
	struct da9066 *da9066 = file->private_data;
	struct pmu_debug dbg;
	char cmd[MAX_USER_INPUT_LEN];
	int ret, i;

	dlg_info("%s\n", __func__);

	if (!da9066) {
		dlg_err("%s: driver not initialized\n", __func__);
		return -EINVAL;
	}

	if (len > MAX_USER_INPUT_LEN)
		len = MAX_USER_INPUT_LEN;

	if (copy_from_user(cmd, buffer, len)) {
		dlg_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	/* chop of '\n' introduced by echo at the end of the input */
	if (cmd[len - 1] == '\n')
		cmd[len - 1] = '\0';

	if (da9066_dbg_parse_args(cmd, &dbg) < 0) {
		da9066_dbg_usage();
		return -EINVAL;
	}

	dlg_info("operation: %s\n", (dbg.read_write == PMUDBG_READ_REG) ?
		"read" : "write");
	dlg_info("address  : 0x%x\n", dbg.addr);
	dlg_info("length   : %d\n", dbg.len);

	if (dbg.read_write == PMUDBG_READ_REG) {
		ret = da9066_bulk_read(da9066, dbg.addr, dbg.len, dbg.val);
		if (ret < 0) {
			dlg_err("%s: pmu reg read failed\n", __func__);
			return -EFAULT;
		}

		for (i = 0; i < dbg.len; i++, dbg.addr++)
			dlg_info("[%x] = 0x%02x\n", dbg.addr,
				dbg.val[i]);
	} else {
		ret = da9066_bulk_write(da9066, dbg.addr, dbg.len, dbg.val);
		if (ret < 0) {
			dlg_err("%s: pmu reg write failed\n", __func__);
			return -EFAULT;
		}
	}

	*offset += len;

	return len;
}

static const struct file_operations da9066_pmu_ops = {
	.open = da9066_ioctl_open,
	.unlocked_ioctl = da9066_ioctl,
	.write = da9066_ioctl_write,
	.release = da9066_ioctl_release,
	.owner = THIS_MODULE,
};

/*
 * da9066_debug_proc_init
 */
void da9066_debug_proc_init(struct da9066 *da9066)
{
	struct proc_dir_entry *entry;

	disable_irq(da9066->chip_irq);
	entry = proc_create_data(DA9066_PROC_NAME, S_IRWXUGO, NULL,
						&da9066_pmu_ops, da9066);
	enable_irq(da9066->chip_irq);
	dlg_crit("\nDA9066-core.c: proc_create_data() = %p; name=\"%s\"\n",
						entry, (entry?entry->name:""));
}

/*
 * da9066_debug_proc_exit
 */
void da9066_debug_proc_exit(void)
{
	remove_proc_entry(DA9066_PROC_NAME, NULL);
}
#endif /* CONFIG_PROC_FS */

struct da9066 *da9066_regl_info = NULL;
EXPORT_SYMBOL(da9066_regl_info);


/*
 * da9066_device_init
 */
int da9066_device_init(struct da9066 *da9066, int irq,
		       struct da9066_platform_data *pdata)
{
	int ret = 0;
	int model;
	unsigned int val;
#ifdef DA9066_REG_DEBUG
	int i;
	u8 data;
#endif

	if (da9066 != NULL)
		da9066_regl_info = da9066;
	else {
		printk("%s Platform data not specified.\n",__func__);
		return -EINVAL;
	}

	dlg_info("DA9066 Driver : built at %s on %s\n", __TIME__, __DATE__);

	da9066->pdata = pdata;
	mutex_init(&da9066->da9066_io_mutex);

#ifdef DA9066_SUPPORT_I2C_HIGH_SPEED
	da9066_set_bits(da9066, DA9066_CONTROL_B_REG, DA9066_I2C_SPEED_MASK);

	/* NOT support repeated write and I2C speed set to 1.7MHz */
	da9066_clear_bits(da9066, DA9066_CONTROL_B_REG, DA9066_WRITE_MODE_MASK);
#else
	/* NOT  support repeated write and I2C speed set to 400KHz */
	da9066_clear_bits(da9066, DA9066_CONTROL_B_REG,
						DA9066_WRITE_MODE_MASK | DA9066_I2C_SPEED_MASK);
#endif

#if defined(__EXTERNAL_PIN_USE__)
	da9066_reg_write(da9066, DA9066_BUCK2_5_CONF1_REG, 0x80);
#else
	da9066_reg_write(da9066, DA9066_BUCK2_5_CONF1_REG, 0x0);
#endif

	da9066_reg_write(da9066, DA9066_PD_DIS_REG, 0x0);
	da9066_dev_info = da9066;

#ifndef __ODIN__
	ret = da9066_irq_init(da9066, irq, pdata);
	if (ret < 0)
		goto err;
#endif
	if (pdata && pdata->init) {
		ret = pdata->init(da9066);
		if (ret != 0) {
			dev_err(da9066->dev, "Platform init() failed: %d\n", ret);
			goto err_irq;
		}
	}
#ifndef __ODIN__
	da9066_system_event_init(da9066);
#endif
#ifdef CONFIG_PROC_FS
	da9066_debug_proc_init(da9066);
#endif

#ifdef DA9066_REG_DEBUG
	  for (i = 0; i < DA9066_MAX_REGISTER_CNT; i++)
	  {
	  	da9066_reg_read(da9066, i, &data);
		da9066_write_reg_cache(i,data);
	  }
#endif

	ret =  mfd_add_devices(da9066->dev, -1, da9066_devs,
				ARRAY_SIZE(da9066_devs), NULL, da9066->pdata->irq_base, NULL);
	if (ret)
		dev_err(da9066->dev, "Cannot add MFD cells\n");

	return 0;

err_irq:
#ifndef __ODIN__
	da9066_irq_exit(da9066);
#endif
	da9066_dev_info = NULL;
	pm_power_off = NULL;
err:
	dlg_crit("\n\nDA9066-core.c: device init failed ! \n\n");
	return ret;
}
EXPORT_SYMBOL_GPL(da9066_device_init);


/*
 * da9066_device_exit
 */
void da9066_device_exit(struct da9066 *da9066)
{
#ifdef CONFIG_PROC_FS
	da9066_debug_proc_exit();
#endif
	da9066_dev_info = NULL;

	platform_device_unregister(da9066->rtc.pdev);
	platform_device_unregister(da9066->onkey.pdev);

	da9066_free_irq(da9066, DA9066_IRQ_EVDD_MON);
#ifndef __ODIN__
	da9066_irq_exit(da9066);
#endif
}
EXPORT_SYMBOL_GPL(da9066_device_exit);


/*
 * da9066_system_poweroff
 */
void da9066_system_poweroff(void)
{
	u8 dst;
	struct da9066 *da9066 = da9066_dev_info;

	dlg_info("%s\n", __func__);

	if (da9066 == NULL || da9066->read_dev == NULL) {
		dlg_err("%s. Platform or Device data is NULL\n", __func__);
		return;
	}

	da9066_reg_read(da9066, DA9066_CONTROL_B_REG, &dst);
	dst |= DA9066_SHUTDOWN_MASK;
	da9066_reg_write(da9066, DA9066_CONTROL_B_REG, dst);

	return;
}
EXPORT_SYMBOL(da9066_system_poweroff);


MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("DA9066 PMIC Core");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DA9066_DRVNAME_CORE);
