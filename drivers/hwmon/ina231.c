/*
 * Driver for ina230 or ina231 power monitor chips
 *
 * Copyright (c) 2013- Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <trace/events/hwmon.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/jiffies.h>

#include <linux/platform_data/ina231.h>

/* common register definitions */
#define INA231_CONFIG			0x00
#define INA231_SHUNT_VOLTAGE		0x01 /* readonly */
#define INA231_BUS_VOLTAGE		0x02 /* readonly */
#define INA231_POWER			0x03 /* readonly */
#define INA231_CURRENT			0x04 /* readonly */
#define INA231_CALIBRATION		0x05

/* INA226 register definitions */
#define INA231_MASK_ENABLE		0x06
#define INA231_ALERT_LIMIT		0x07
#define INA231_DIE_ID			0xFF


/* register count */
#define INA231_REGISTERS		8

#define INA2XX_MAX_REGISTERS		8

/* settings - depend on use case */
#define INA231_CONFIG_DEFAULT		0x4527	/* averages=16 */

/* worst case is 68.10 ms (~14.6Hz, ina219) */
#define INA231_CONVERSION_RATE		15

enum ina231_ids { ina231 };

struct ina231_config {
	u16 config_default;
	int calibration_factor;
	int registers;
	int shunt_lsb;
	int bus_voltage_shift;
	int bus_voltage_lsb;	/* uV */
	int current_lsb;	/* uA */
	int power_lsb;		/* uW */
};

struct ina231_data {
	struct device *hwmon_dev;
	const struct ina231_config *config;

	struct mutex update_lock;
	bool valid;
	unsigned long last_updated;

	int kind;
	u16 regs[INA2XX_MAX_REGISTERS];
	struct list_head list;
	int ft_enabled[INA231_CALIBRATION];
	char label[I2C_NAME_SIZE];
};

struct ftrace_timer {
	int is_init;
	struct delayed_work dwork;
	unsigned int timeout;
	struct mutex mtx;
	struct list_head datalist;
	int enabled;
};

static struct ftrace_timer ft_timer = {
	.timeout = HZ,
	.is_init = 0,
	.datalist = LIST_HEAD_INIT(ft_timer.datalist),
	.enabled = 0,
};

static DEFINE_SPINLOCK(ft_init_lock);

static const struct ina231_config ina231_config[] = {
	[ina231] = {
		.config_default = INA231_CONFIG_DEFAULT,
		.calibration_factor = 25600000,
		.registers = INA231_REGISTERS,
		.shunt_lsb = 2500,  /* 2.5uV/bit */
		.bus_voltage_shift = 0,
		.bus_voltage_lsb = 1250, /* 1.25mV/bit */
		.current_lsb = 200, /* 200uA/bit */
		.power_lsb = 5000, /* Current_LSB*25 = 5mW/bit */
	}
};

static struct ina231_data *ina231_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina231_data *data = i2c_get_clientdata(client);
	struct ina231_data *ret = data;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated +
		       HZ / INA231_CONVERSION_RATE) || !data->valid) {

		int i;

		dev_dbg(&client->dev, "Starting ina231 update\n");

		/* Read all registers */
		for (i = 0; i < data->config->registers; i++) {
			int rv = i2c_smbus_read_word_swapped(client, i);
			if (rv < 0) {
				ret = ERR_PTR(rv);
				goto abort;
			}
			data->regs[i] = rv;
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}
abort:
	mutex_unlock(&data->update_lock);
	return ret;
}

static int ina231_get_value(struct ina231_data *data, u8 reg)
{
	int val;

	switch (reg) {
	case INA231_SHUNT_VOLTAGE:
		val = ((s16)data->regs[reg]) * data->config->shunt_lsb;
		val = DIV_ROUND_CLOSEST(val, 1000);
		break;
	case INA231_BUS_VOLTAGE:
		val = (data->regs[reg] >> data->config->bus_voltage_shift)
		  * data->config->bus_voltage_lsb;
		val = DIV_ROUND_CLOSEST(val, 1000);
		break;
	case INA231_POWER:
		val = data->regs[reg] * data->config->power_lsb;
		if((s16)data->regs[INA231_CURRENT] < 0) {
			val = -val;
		}
		val = DIV_ROUND_CLOSEST(val, 1000);
		break;
	case INA231_CURRENT:
		/* LSB=200uA */
		val = ((s16)data->regs[reg]) * data->config->current_lsb;
		val = DIV_ROUND_CLOSEST(val, 1000);
		break;
	default:
		/* programmer goofed */
		WARN_ON_ONCE(1);
		val = 0;
		break;
	}

	return val;
}

static ssize_t ina231_show_value(struct device *dev,
				 struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ina231_data *data = ina231_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return snprintf(buf, PAGE_SIZE, "%d\n",
				ina231_get_value(data, attr->index));
}

static ssize_t ina231_show_enabled(struct device *dev,
				   struct device_attribute *da, char *buf)
{
	ssize_t retval;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ina231_data *data = i2c_get_clientdata(client);

	mutex_lock(&ft_timer.mtx);
	retval = snprintf(buf, PAGE_SIZE, "%d\n",
			  data->ft_enabled[attr->index]);
	mutex_unlock(&ft_timer.mtx);
	return retval;
}

static ssize_t ina231_store_enabled(struct device *dev,
				    struct device_attribute *da,
				    const char *buf, size_t count)
{
	int value;
	int n;

	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ina231_data *data = i2c_get_clientdata(client);

	n = sscanf(buf, "%d", &value);

	mutex_lock(&ft_timer.mtx);
	if (n == 1 && (value == 0 || value == 1)) {
		if (value == 1 && ft_timer.enabled == 0) {
			ft_timer.enabled = 1;
			schedule_delayed_work(&ft_timer.dwork,
					      ft_timer.timeout);
		}
		data->ft_enabled[attr->index] = value;
	}
	mutex_unlock(&ft_timer.mtx);

	return count;
}

static ssize_t ina231_show_period(struct device *dev,
				   struct device_attribute *da, char *buf)
{
	ssize_t retval;

	mutex_lock(&ft_timer.mtx);
	retval = snprintf(buf, PAGE_SIZE, "%d\n",
			  ft_timer.timeout);
	mutex_unlock(&ft_timer.mtx);
	return retval;
}

static ssize_t ina231_store_period(struct device *dev,
				    struct device_attribute *da,
				   const char *buf, size_t count)
{
	int value;
	int n;

	n = sscanf(buf, "%d", &value);

	mutex_lock(&ft_timer.mtx);
	if (n == 1 && value > 0)
		ft_timer.timeout = (unsigned int) value;
	mutex_unlock(&ft_timer.mtx);

	return count;
}

/* shunt voltage */
static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO, ina231_show_value, NULL,
			  INA231_SHUNT_VOLTAGE);
static SENSOR_DEVICE_ATTR(in0_input_ftrace, S_IRUGO | S_IWUSR,
			  ina231_show_enabled, ina231_store_enabled,
			  INA231_SHUNT_VOLTAGE);

/* bus voltage */
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, ina231_show_value, NULL,
			  INA231_BUS_VOLTAGE);
static SENSOR_DEVICE_ATTR(in1_input_ftrace, S_IRUGO | S_IWUSR,
			  ina231_show_enabled, ina231_store_enabled,
			  INA231_BUS_VOLTAGE);

/* calculated current */
static SENSOR_DEVICE_ATTR(curr1_input, S_IRUGO, ina231_show_value, NULL,
			  INA231_CURRENT);
static SENSOR_DEVICE_ATTR(curr1_input_ftrace, S_IRUGO | S_IWUSR,
			  ina231_show_enabled, ina231_store_enabled,
			  INA231_CURRENT);

/* calculated power */
static SENSOR_DEVICE_ATTR(power1_input, S_IRUGO, ina231_show_value, NULL,
			  INA231_POWER);
static SENSOR_DEVICE_ATTR(power1_input_ftrace, S_IRUGO | S_IWUSR,
			  ina231_show_enabled, ina231_store_enabled,
			  INA231_POWER);

/* ftrace period */
static SENSOR_DEVICE_ATTR(ftrace_period, S_IRUGO | S_IWUSR, ina231_show_period,
			  ina231_store_period, 0);


/* pointers to created device attributes */
static struct attribute *ina231_attributes[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_input_ftrace.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_input_ftrace.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr1_input_ftrace.dev_attr.attr,
	&sensor_dev_attr_power1_input.dev_attr.attr,
	&sensor_dev_attr_power1_input_ftrace.dev_attr.attr,
	&sensor_dev_attr_ftrace_period.dev_attr.attr,
	NULL,
};

static const struct attribute_group ina231_group = {
	.attrs = ina231_attributes,
};

static __always_inline void trace_value(struct ina231_data *data, u8 reg)
{
	int value;
	ina231_update_device(data->hwmon_dev->parent);

	value = ina231_get_value(data, reg);
	trace_hwmon_sensor(value, data->label);
}

static __always_inline void check_and_trace(struct ina231_data *data)
{
	u8 index;

	for (index = INA231_SHUNT_VOLTAGE; index < INA231_CALIBRATION; index++)
		if (data->ft_enabled[index])
			trace_value(data, index);
}

static void ftrace_timer_func(struct work_struct *work)
{
	struct ina231_data *data;

	mutex_lock(&ft_timer.mtx);

	list_for_each_entry(data, &ft_timer.datalist, list)
		check_and_trace(data);

	if (ft_timer.enabled)
		schedule_delayed_work(&ft_timer.dwork, ft_timer.timeout);
	mutex_unlock(&ft_timer.mtx);
}

static void register_ina231_data(struct ina231_data *data)
{
	mutex_lock(&ft_timer.mtx);
	list_add(&data->list, &ft_timer.datalist);
	mutex_unlock(&ft_timer.mtx);
}

static void unregister_ina231_data(struct ina231_data *data)
{
	mutex_lock(&ft_timer.mtx);
	list_del(&data->list);
	mutex_unlock(&ft_timer.mtx);
}

static void init_ftrace_timer(void)
{
	unsigned long flags;
	/* This lock is probably not even necessary as I guess the probe
	   functions will probably be called non-concurrently. The
	   irqsave is just unnecessesary precaution :) */
	spin_lock_irqsave(&ft_init_lock, flags);
	if (ft_timer.is_init)
		goto out;
	ft_timer.is_init = 1;
	/* this will be run only once */
	INIT_DELAYED_WORK(&ft_timer.dwork, ftrace_timer_func);
	mutex_init(&ft_timer.mtx);
 out:
	spin_unlock_irqrestore(&ft_init_lock, flags);
}

static int ina231_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ina231_data *data;
	struct ina231_platform_data *pdata;
	int ret;
	long shunt = 1000; /* default shunt value 1000uOhms */

	init_ftrace_timer();

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (client->dev.platform_data) {
		pdata =
		  (struct ina231_platform_data *)client->dev.platform_data;
		shunt = pdata->shunt_uohms;
	}

	if (shunt <= 0)
		return -ENODEV;

	/* set the device type */
	data->kind = id->driver_data;
	data->config = &ina231_config[data->kind];

	/* device configuration */
	i2c_smbus_write_word_swapped(client, INA231_CONFIG,
				     data->config->config_default);
	/* set current LSB to 1mA, shunt is in uOhms */
	/* (equation 13 in datasheet) */
	i2c_smbus_write_word_swapped(client, INA231_CALIBRATION,
				     data->config->calibration_factor / shunt);

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	ret = sysfs_create_group(&client->dev.kobj, &ina231_group);
	if (ret)
		return ret;

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		goto out_err_hwmon;
	}

	dev_info(&client->dev, "power monitor %s (Rshunt = %li uOhm)\n",
		 id->name, shunt);

	strncpy(data->label, id->name, I2C_NAME_SIZE);
	register_ina231_data(data);

	return 0;

out_err_hwmon:
	sysfs_remove_group(&client->dev.kobj, &ina231_group);
	return ret;
}

static int ina231_remove(struct i2c_client *client)
{
	struct ina231_data *data = i2c_get_clientdata(client);

	unregister_ina231_data(data);

	mutex_lock(&ft_timer.mtx);

	ft_timer.enabled = 0;
	cancel_delayed_work(&ft_timer.dwork);
	flush_delayed_work(&ft_timer.dwork);
	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &ina231_group);
	mutex_unlock(&ft_timer.mtx);

	return 0;
}

static const struct i2c_device_id ina231_id[] = {
	{ "a15-mon", ina231 },
	{ "a7-mon", ina231 },
	{ "gpu-mon", ina231 },
	{ "ddr-mon", ina231 },
	{ "peri-mon", ina231 },
	{ "battery-mon", ina231 },
};
MODULE_DEVICE_TABLE(i2c, ina231_id);

static struct of_device_id ina231_of_match[] = {
	{ .compatible = "hisilicon,a15-mon" },
	{ .compatible = "hisilicon,a7-mon" },
	{ .compatible = "hisilicon,gpu-mon" },
	{ .compatible = "hisilicon,ddr-mon" },
	{ .compatible = "hisilicon,peri-mon" },
	{ .compatible = "hisilicon,battery-mon" },
	{ },
};

MODULE_DEVICE_TABLE(of, ina231_of_match);

static struct i2c_driver ina231_driver = {
	.driver = {
		.name	= "ina231",
		.of_match_table = ina231_of_match,
	},
	.probe		= ina231_probe,
	.remove		= ina231_remove,
	.id_table	= ina231_id,
};

module_i2c_driver(ina231_driver);

MODULE_AUTHOR("w00221409 <hw.wangxiaoyin@huawei.com>");
MODULE_DESCRIPTION("ina231 driver");
MODULE_LICENSE("GPL");
