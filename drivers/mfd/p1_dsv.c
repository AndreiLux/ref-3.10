/*
 * P1 DSV  MFD Driver
 *
 * Copyright 2014 LG Electronics Inc,
 *
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#include <linux/mfd/p1_dsv.h>

static struct p1_dsv *p1_dsv_base;
static struct mfd_cell p1_dsv_devs[] = {
	{ .name = "p1_dsv_dev" },
};

int p1_dsv_read_byte(struct p1_dsv *p1_dsv, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(p1_dsv->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(p1_dsv_read_byte);

int p1_dsv_write_byte(struct p1_dsv *p1_dsv, u8 reg, u8 data)
{
	return regmap_write(p1_dsv->regmap, reg, data);
}
EXPORT_SYMBOL_GPL(p1_dsv_write_byte);

int p1_dsv_update_bits(struct p1_dsv *p1_dsv, u8 reg, u8 mask, u8 data)
{
	int ret;
	ret = regmap_update_bits(p1_dsv->regmap, reg, mask, data);
	return ret;
}
EXPORT_SYMBOL_GPL(p1_dsv_update_bits);

int sm5107_mode_change(int mode)
{
	int ret = 0;

	pr_info("%s mode = %d\n", __func__, mode);
	if (p1_dsv_base == NULL)
		return -EINVAL;

	if (mode == 0)
		ret = p1_dsv_write_byte(p1_dsv_base, SM5107_CTRL_SET_REG, 0);
	else if (mode == 1)
		ret = p1_dsv_write_byte(p1_dsv_base, SM5107_CTRL_SET_REG, 0x40);

	return ret;
}
EXPORT_SYMBOL_GPL(sm5107_mode_change);

int dw8768_fast_discharge(void)
{
	int ret = 0;

	if (p1_dsv_base == NULL)
		return -EINVAL;

	pr_err("[LGD_SIC] DW8783 Fast Discharge.\n");
	ret = p1_dsv_write_byte(p1_dsv_base, DW8768_DISCHARGE_REG, 0x83);
	msleep(20);
	ret = p1_dsv_write_byte(p1_dsv_base, DW8768_DISCHARGE_REG, 0x80);
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_fast_discharge);

int dw8768_mode_change(int mode)
{
	int ret = 0;

	pr_info("%s mode = %d\n", __func__, mode);
	if (p1_dsv_base == NULL)
		return -EINVAL;

	if (mode == 0)
		ret = p1_dsv_write_byte(p1_dsv_base, DW8768_ENABLE_REG, 0x07);
	else if (mode == 1)
		ret = p1_dsv_write_byte(p1_dsv_base, DW8768_ENABLE_REG, 0x0F);

	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_mode_change);

int dw8768_lgd_dsv_setting(int enable)
{
	int ret = 0;

	if (p1_dsv_base == NULL)
		return -EINVAL;
	if (enable == 2) {
		ret += p1_dsv_write_byte(p1_dsv_base, 0x03, 0x87);
		pr_info("DW DSV FD ON set, ret = %d\n", ret);
	} else if (enable == 1) {
		ret  = p1_dsv_write_byte(p1_dsv_base, 0x00, 0x0F);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x01, 0x0F);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x03, 0x87);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x05, 0x07);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x07, 0x08);
		pr_info("DW DSV Normal mode set, ret = %d\n", ret);
	} else {
		ret  = p1_dsv_write_byte(p1_dsv_base, 0x00, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x01, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x03, 0x84);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x05, 0x07);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x07, 0x08);
		pr_info("DW DSV LPWG mode set, ret = %d\n", ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_lgd_dsv_setting);

int dw8768_lgd_fd_mode_change(int enable)
{
	int ret = 0;

	if (p1_dsv_base == NULL)
		return -EINVAL;

	if (enable) {
		ret = p1_dsv_write_byte(p1_dsv_base, 0x00, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x01, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x03, 0x87);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x05, 0x07);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x07, 0x08);
		pr_info("dw8768_lgd_fd_mode_change enable : %d, ret = %d\n",
				enable, ret);

	} else {
		ret = p1_dsv_write_byte(p1_dsv_base, 0x00, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x01, 0x09);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x03, 0x84);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x05, 0x07);
		ret += p1_dsv_write_byte(p1_dsv_base, 0x07, 0x08);
		pr_info("dw8768_lgd_fd_mode_change enable : %d, ret = %d\n",
				enable, ret);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dw8768_lgd_fd_mode_change);

static struct regmap_config p1_dsv_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = P1_DSV_MAX_REGISTERS,
};

static int p1_dsv_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct p1_dsv *p1_dsv;
	struct device *dev = &cl->dev;
	struct p1_dsv_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	pr_info("%s start\n", __func__);

	p1_dsv = devm_kzalloc(dev, sizeof(*p1_dsv), GFP_KERNEL);
	if (!p1_dsv)
		return -ENOMEM;

	p1_dsv->pdata = pdata;

	p1_dsv->regmap = devm_regmap_init_i2c(cl, &p1_dsv_regmap_config);
	if (IS_ERR(p1_dsv->regmap)) {
		pr_err("Failed to allocate register map\n");
		devm_kfree(dev, p1_dsv);
		return PTR_ERR(p1_dsv->regmap);
	}

	p1_dsv->dev = &cl->dev;
	i2c_set_clientdata(cl, p1_dsv);
	p1_dsv_base = p1_dsv;

	rc = mfd_add_devices(dev, -1, p1_dsv_devs, ARRAY_SIZE(p1_dsv_devs),
			       NULL, 0, NULL);
	if (rc) {
		pr_err("Failed to add p1_dsv subdevice ret=%d\n", rc);
		return -ENODEV;
	}

	return rc;
}

static int p1_dsv_remove(struct i2c_client *cl)
{
	struct p1_dsv *p1_dsv = i2c_get_clientdata(cl);

	mfd_remove_devices(p1_dsv->dev);

	return 0;
}

static const struct i2c_device_id p1_dsv_ids[] = {
	{ "p1_dsv", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, p1_dsv_ids);

#ifdef CONFIG_OF
static const struct of_device_id p1_dsv_of_match[] = {
	{ .compatible = "sm_dw,p1_dsv", },
	{ }
};
MODULE_DEVICE_TABLE(of, p1_dsv_of_match);
#endif

static struct i2c_driver p1_dsv_driver = {
	.driver = {
		.name = "p1_dsv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(p1_dsv_of_match),
#endif
	},
	.id_table = p1_dsv_ids,
	.probe = p1_dsv_probe,
	.remove = p1_dsv_remove,
};
module_i2c_driver(p1_dsv_driver);

MODULE_DESCRIPTION("p1_dsv MFD Core");
