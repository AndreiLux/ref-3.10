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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
* GNU General Public License for more details.*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#define STR_ENERGY_METER	"energy_meter"

struct em_hwmon_data {
	struct device *hwmon_dev;
	struct mutex lock;
};

static s64 get_ns(void)
{
	struct timespec ts;
	getnstimeofday(&ts);
	return timespec_to_ns(&ts);
}

static ssize_t energy_meter_show_name(struct device *dev, struct device_attribute
				  *dev_attr, char *buf)
{
	return sprintf(buf, "%s\n", STR_ENERGY_METER);
}

static ssize_t energy_meter_show_help(struct device *dev, struct device_attribute
				  *dev_attr, char *buf)
{
	return sprintf(buf, "%s\n", "Usage: Energy Meter\n"
		 "[cat name] Display Energy Meter device name(energy_meter)\n"
		 "[cat start] Start power measurement & drop power sum(0,0,0,0(uJ))\n"
		 "[cat stop] Stop power measurement & drop power sum(x,x,x,x(uJ))\n"
		 "[cat channel1(channel2, channel3, channel4)]\n"
         "  Get input voltage, drop voltage of selected channel([x,x](uV))\n"
		 "[cat resistance] Get resistance of 4 channels([x,x,x,x](uOhm))\n"
		 "[echo"" channel, resistance"" > resistance]\n"
		 "  Set resistance of selected channel(echo"" 1, 5000"" > resistance)\n"
		 "[cat gain] Get gain([x])\n"
		 "[echo"" gain"" > gain] Set gain(echo"" 4"" > gain)\n"
		 "  <gain: 1,2,4,8,12,16,24,48,50>\n"
		 "[cat reset] Energy Meter reset\n"
		 "[cat help] Help about CMDs\n");
}

static ssize_t energy_meter_reset(struct device *dev, struct device_attribute
				  *dev_attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[2];
	int status = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	tx_buf[0] = 82; /* Reset CMD-ASCII "R(82)-Reset" */
	tx_buf[1] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */
	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	mutex_unlock(&data->lock);

	return 0;
}

static ssize_t energy_meter_start_stop(struct device *dev, struct device_attribute
				  *dev_attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[2];
	uint16_t rx_buf[8];
	uint32_t result_watt[4], result_joules[4];
	int64_t time = 0;
	static int64_t start_time, stop_time;
	int status = 0;
	int i;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	tx_buf[0] = attr->index;
	tx_buf[1] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */
	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if(attr->index == 10) {
		start_time = get_ns();
		time = start_time;
	} else if(attr->index == 11) {
		stop_time = get_ns();
		time = (stop_time - start_time);
	}

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	udelay(1000);
	status = spi_read(spi, rx_buf, sizeof(rx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	result_watt[0] = (rx_buf[0] << 16) + rx_buf[1];
	result_watt[1] = (rx_buf[2] << 16) + rx_buf[3];
	result_watt[2] = (rx_buf[4] << 16) + rx_buf[5];
	result_watt[3] = (rx_buf[6] << 16) + rx_buf[7];

	do_div(time, 1000000);
	for(i=0;i<4;i++)
		result_joules[i] = (result_watt[i] / 1000) * (uint32_t)time;

	mutex_unlock(&data->lock);

	return sprintf(buf, "%u,%u,%u,%u\n", result_joules[0],
				   result_joules[1], result_joules[2], result_joules[3]);
}

static ssize_t energy_meter_set_channel(struct device *dev, struct device_attribute
				  *dev_attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[2];
	uint16_t rx_buf[8];
	uint32_t delta_value = 0, input_value = 0;
	int status = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	tx_buf[0] = attr->index;
	tx_buf[1] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */
	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	udelay(1000);
	status = spi_read(spi, rx_buf, sizeof(rx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	delta_value = (rx_buf[0] << 16) + rx_buf[1];
	input_value = (rx_buf[2] << 16) + rx_buf[3];

	mutex_unlock(&data->lock);

	return sprintf(buf, "[%u,%u]\n", input_value, delta_value);
}

static ssize_t energy_meter_show_resistance(struct device *dev,
			   struct device_attribute *dev_attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[3];
	uint16_t rx_buf[8];
	uint32_t r_value[4];
	int status = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	tx_buf[0] = attr->index;
	tx_buf[1] = 70;
	tx_buf[2] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */
	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	udelay(1000);
	status = spi_read(spi, rx_buf, sizeof(rx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	r_value[0] = (rx_buf[0] << 16) + rx_buf[1];
	r_value[1] = (rx_buf[2] << 16) + rx_buf[3];
	r_value[2] = (rx_buf[4] << 16) + rx_buf[5];
	r_value[3] = (rx_buf[6] << 16) + rx_buf[7];

	mutex_unlock(&data->lock);

	return sprintf(buf, "[%d, %d, %d, %d]\n", r_value[0],
				   r_value[1], r_value[2], r_value[3]);
}

static ssize_t energy_meter_store_resistance(struct device *dev,
			   struct device_attribute *dev_attr, const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[6];
	uint32_t r_value = 0;
	int index = 0, status = 0, ret = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	ret = sscanf(buf, "%d, %d", &index, &r_value);
    if(ret < 0)
		return ret;

	tx_buf[0] = attr->index;
	tx_buf[1] = 71;
	tx_buf[2] = (uint16_t)index;
	tx_buf[3] = r_value >> 16;
	tx_buf[4] = r_value & 0xFFFF;
	tx_buf[5] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */

	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t energy_meter_show_gain(struct device *dev,
			   struct device_attribute *dev_attr, char *buf)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[3];
	uint16_t rx_buf[8];
	uint16_t g_value[1];
	int status = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	tx_buf[0] = attr->index;
	tx_buf[1] = 90;
	tx_buf[2] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */

	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	udelay(1000);
	status = spi_read(spi, rx_buf, sizeof(rx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	g_value[0] = rx_buf[0];

	mutex_unlock(&data->lock);

	return sprintf(buf, "[%d]\n", g_value[0]);
}

static ssize_t energy_meter_store_gain(struct device *dev,
			   struct device_attribute *dev_attr, const char *buf, size_t count)
{
	struct spi_device *spi = to_spi_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(dev_attr);
	struct em_hwmon_data *data = spi_get_drvdata(spi);
	uint16_t tx_buf[4];
	int g_value = 0, status = 0, ret = 0;

	if (mutex_lock_interruptible(&data->lock))
		return -ERESTARTSYS;

	ret = sscanf(buf, "%d", &g_value);
    if(ret < 0)
		return ret;

	tx_buf[0] = attr->index;
	tx_buf[1] = 91;
	tx_buf[2] = (uint16_t)g_value;
	tx_buf[3] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */

	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(dev, "SPI synch. transfer failed with status %d\n",
				status);
	}

	mutex_unlock(&data->lock);

	return count;
}

static struct sensor_device_attribute energy_meter_input[] = {
	SENSOR_ATTR(name, S_IRUGO, energy_meter_show_name, NULL, 0),
	SENSOR_ATTR(help, S_IRUGO, energy_meter_show_help, NULL, 0),
	SENSOR_ATTR(reset, S_IRUGO, energy_meter_reset, NULL, 0),
	SENSOR_ATTR(start, S_IRUGO, energy_meter_start_stop, NULL, 10),
	SENSOR_ATTR(stop, S_IRUGO, energy_meter_start_stop, NULL, 11),
	SENSOR_ATTR(channel1, S_IRUGO, energy_meter_set_channel, NULL, 1),
	SENSOR_ATTR(channel2, S_IRUGO, energy_meter_set_channel, NULL, 2),
	SENSOR_ATTR(channel3, S_IRUGO, energy_meter_set_channel, NULL, 3),
	SENSOR_ATTR(channel4, S_IRUGO, energy_meter_set_channel, NULL, 4),
	SENSOR_ATTR(resistance, S_IRUGO | S_IWUSR, energy_meter_show_resistance, 
				energy_meter_store_resistance, 7),
	SENSOR_ATTR(gain, S_IRUGO | S_IWUSR, energy_meter_show_gain,
				energy_meter_store_gain, 9),
};

static int energy_meter_probe(struct spi_device *spi)
{
	struct em_hwmon_data *data;
	uint16_t tx_buf[2];
	int status = 0;
	int i;

	spi->bits_per_word = 16;

	data = devm_kzalloc(&spi->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);
	mutex_lock(&data->lock);

	spi_set_drvdata(spi, data);

	for (i = 0; i < ARRAY_SIZE(energy_meter_input); i++) {
		status = device_create_file(&spi->dev, &energy_meter_input[i].dev_attr);
		if (status) {
			dev_err(&spi->dev, "device_create_file failed.\n");
			goto out_err;
		}

	}

	data->hwmon_dev = hwmon_device_register(&spi->dev);
	if (IS_ERR(data->hwmon_dev)) {
		dev_err(&spi->dev, "hwmon_device_register failed.\n");
		status = PTR_ERR(data->hwmon_dev);
		goto out_err;
	}

    /* Energy Meter Reset */
#if CONFIG_ARCH_ODIN_REVISION == 2
	tx_buf[0] = 82; /* Reset CMD-ASCII "R(82)-Reset" */
	tx_buf[1] = 6977; /* End CMD-ASCII "EM(6977)-Energy Meter" */
#else
    /* Before reset, Energy Meter receives shift right values below Revision2.
       So, Driver transfers shift left values for Energy Meter reset */
	tx_buf[0] = 82 << 1; /* Reset CMD-ASCII "R(82)-Reset" */
	tx_buf[1] = 6977 << 1; /* End CMD-ASCII "EM(6977)-Energy Meter" */
#endif
	status = spi_write(spi, tx_buf, sizeof(tx_buf));

	if (status < 0) {
		dev_warn(&spi->dev, "SPI synch. transfer failed with status %d\n",
				status);
		goto out_err;
	}

	mutex_unlock(&data->lock);

	dev_info(&spi->dev, "%s : em_spi_prove!!\n",  __func__);
	return 0;

out_err:
	for (i--; i >= 0; i--)
		device_remove_file(&spi->dev, &energy_meter_input[i].dev_attr);

	spi_set_drvdata(spi, NULL);
	mutex_unlock(&data->lock);
	kfree(data);
	return status;
}

static int energy_meter_remove(struct spi_device *spi)
{
	struct em_hwmon_data *data  = spi_get_drvdata(spi);
	int i;

	mutex_lock(&data->lock);
	hwmon_device_unregister(data->hwmon_dev);
	for (i = 0; i < ARRAY_SIZE(energy_meter_input); i++)
		device_remove_file(&spi->dev, &energy_meter_input[i].dev_attr);

	spi_set_drvdata(spi, NULL);
	mutex_unlock(&data->lock);
	kfree(data);

	return 0;
}

static struct of_device_id energy_meter_hwmon_of_match[] = {
	{
		.compatible = "lge,energy_meter",
	},
	{}
};
MODULE_DEVICE_TABLE(of, energy_meter__hwmon_of_match);

static struct spi_driver energy_meter = {
	.driver = {
		.name	= STR_ENERGY_METER,
		.owner	= THIS_MODULE,
		.of_match_table = energy_meter_hwmon_of_match,
	},
	.probe	= energy_meter_probe,
	.remove	= energy_meter_remove,
};

module_spi_driver(energy_meter);

MODULE_AUTHOR("DongMin Shin<alexia.shin@lge.com>");
MODULE_DESCRIPTION("LGE,Energy Meter");
MODULE_LICENSE("GPL");
