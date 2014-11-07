/* Sensirion SHTC1 humidity and temperature sensor driver
 *
 * Copyright (C) 2012 Sensirion AG, Switzerland
 * Author: Johannes Winkelmann <johannes.winkelmann@sensirion.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Data sheet available soon
 *  TODO: add link
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_data/shtc1.h>

/* commands (high precision mode) */
static const unsigned char shtc1_cmd_measure_blocking_hpm[]    = { 0x7C, 0xA2 };
static const unsigned char shtc1_cmd_measure_nonblocking_hpm[] = { 0x78, 0x66 };

/* commands (low precision mode) */
static const unsigned char shtc1_cmd_measure_blocking_lpm[]    = { 0x64, 0x58 };
static const unsigned char shtc1_cmd_measure_nonblocking_lpm[] = { 0x60, 0x9c };

/* delays for non-blocking i2c commands */
/* TODO: use max timings */
#define SHTC1_NONBLOCKING_WAIT_TIME_HPM  10
#define SHTC1_NONBLOCKING_WAIT_TIME_LPM   1

#define SHTC1_CMD_LENGTH      2
#define SHTC1_RESPONSE_LENGTH 6

struct shtc1_data {
       struct device *hwmon_dev;
       struct mutex update_lock;
       bool valid;
       unsigned long last_updated; /* In jiffies */

       const unsigned char *command;
       unsigned int nonblocking_wait_time;

       struct shtc1_platform_data setup;

       int temperature;
       int humidity;
};

static void shtc1_select_command(struct shtc1_data *data)
{
       if (data->setup.high_precision) {
               data->command = data->setup.blocking_io ?
                               shtc1_cmd_measure_blocking_hpm :
                               shtc1_cmd_measure_nonblocking_hpm;
               data->nonblocking_wait_time = SHTC1_NONBLOCKING_WAIT_TIME_HPM;

       } else {
               data->command = data->setup.blocking_io ?
                               shtc1_cmd_measure_blocking_lpm :
                               shtc1_cmd_measure_nonblocking_lpm;
               data->nonblocking_wait_time = SHTC1_NONBLOCKING_WAIT_TIME_LPM;
       }
}

static int shtc1_update_values(struct i2c_client *client,
                              struct shtc1_data *data,
                              char *buf,
                              int bufsize)
{
       int ret = i2c_master_send(client, data->command, SHTC1_CMD_LENGTH);
       if (ret < 0) {
               dev_err(&client->dev, "failed to send command: %d", ret);
               return ret;
       }

       /*
        * in blocking mode (clock stretching mode) the I2C bus
        * is blocked for other traffic, thus the call to i2c_master_recv()
        * will wait until the data is ready. for non blocking mode, we
        * have to wait ourselves, thus the msleep()
        *
        * TODO: consider usleep_range
        */
       if (!data->setup.blocking_io)
               msleep(data->nonblocking_wait_time);

       ret = i2c_master_recv(client, buf, bufsize);
       if (ret != bufsize) {
               dev_err(&client->dev, "failed to read values: %d", ret);
               return ret;
       }

       return 0;
}

/* sysfs attributes */
static struct shtc1_data *shtc1_update_client(struct device *dev)
{
       struct i2c_client *client = to_i2c_client(dev);
       struct shtc1_data *data = i2c_get_clientdata(client);

       unsigned char buf[SHTC1_RESPONSE_LENGTH];
       int val;
       int ret;

       mutex_lock(&data->update_lock);

       /*
        * initialize 'ret' in case we had a valid result before, but
        * read too quickly in which case we return the last values
        */
       ret = !data->valid;

       if (time_after(jiffies, data->last_updated + HZ / 10) || !data->valid) {
               ret = shtc1_update_values(client, data, buf, sizeof(buf));

               if (ret)
                       goto out;

               /*
                        * From datasheet:
                        *   T = -45 + 175 * ST / 2^16
                        *   RH = 100 * SRH / 2^16
                        *
                        * Adapted for integer fixed point (3 digit) arithmetic
                        */
               val = be16_to_cpup((__be16 *)buf);
               data->temperature = ((21875 * val) >> 13) - 45000;
               val = be16_to_cpup((__be16 *)(buf+3));
               data->humidity = ((12500 * val) >> 13) ;

               data->last_updated = jiffies;
               data->valid = 1;
       }

out:
       mutex_unlock(&data->update_lock);

       return ret == 0 ? data : ERR_PTR(ret);
}

static ssize_t shtc1_show_temperature(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
       struct shtc1_data *data = shtc1_update_client(dev);
       if (IS_ERR(data))
               return PTR_ERR(data);

       return sprintf(buf, "%d\n", data->temperature);
}

static ssize_t shtc1_show_humidity(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
       struct shtc1_data *data = shtc1_update_client(dev);
       if (IS_ERR(data))
               return PTR_ERR(data);

       return sprintf(buf, "%d\n", data->humidity);
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO,
                         shtc1_show_temperature, NULL, 0);
static SENSOR_DEVICE_ATTR(humidity1_input, S_IRUGO,
                         shtc1_show_humidity, NULL, 0);

static struct attribute *shtc1_attributes[] = {
       &sensor_dev_attr_temp1_input.dev_attr.attr,
       &sensor_dev_attr_humidity1_input.dev_attr.attr,
       NULL
};

static const struct attribute_group shtc1_attr_group = {
       .attrs = shtc1_attributes,
};

static int shtc1_probe(struct i2c_client *client,
                                const struct i2c_device_id *id)
{
       struct shtc1_data *data;
       int err;
       struct i2c_adapter *adap = client->adapter;

       if (!i2c_check_functionality(adap, I2C_FUNC_I2C)) {
               dev_err(&client->dev,
                         "adapter does not support plain i2c transactions\n");
               return -ENODEV;
       }

       data = devm_kzalloc(&client->dev, sizeof(struct shtc1_data),
                           GFP_KERNEL);
       if (!data)
               return -ENOMEM;

       /* defaults: blocking, high precision mode */
       data->setup.blocking_io = 1;
       data->setup.high_precision = 1;

       if (client->dev.platform_data)
               data->setup = *(struct shtc1_platform_data *)client->dev.platform_data;
       shtc1_select_command(data);

       i2c_set_clientdata(client, data);
       mutex_init(&data->update_lock);

       err = sysfs_create_group(&client->dev.kobj, &shtc1_attr_group);
       if (err) {
               dev_dbg(&client->dev, "could not create sysfs files\n");
               goto fail_free;
               return err;
       }
       data->hwmon_dev = hwmon_device_register(&client->dev);
       if (IS_ERR(data->hwmon_dev)) {
               dev_dbg(&client->dev, "unable to register hwmon device\n");
               err = PTR_ERR(data->hwmon_dev);
               goto fail_remove_sysfs;
       }


       dev_info(&client->dev, "initialized\n");

       return 0;

fail_remove_sysfs:
       sysfs_remove_group(&client->dev.kobj, &shtc1_attr_group);
fail_free:
       kfree(data);

       return err;
}

/**
 * shtc1_remove() - remove device
 * @client: I2C client device
 */
static int shtc1_remove(struct i2c_client *client)
{
       struct shtc1_data *data = i2c_get_clientdata(client);

       hwmon_device_unregister(data->hwmon_dev);
       sysfs_remove_group(&client->dev.kobj, &shtc1_attr_group);

       return 0;
}

/* Device ID table */
static const struct i2c_device_id shtc1_id[] = {
       { "shtc1", 0 },
       { }
};
MODULE_DEVICE_TABLE(i2c, shtc1_id);

static struct i2c_driver shtc1_i2c_driver = {
       .driver.name = "shtc1",
       .probe       = shtc1_probe,
       .remove      = shtc1_remove,
       .id_table    = shtc1_id,
};

/**
 * shtc1_init() - initialize driver
 *
 * Called when kernel is booted or module is inserted.
 * Returns 0 on success.
 */
static int __init shtc1_init(void)
{
    return i2c_add_driver(&shtc1_i2c_driver);
}
module_init(shtc1_init);

/*
 * shtc1_init() - clean up driver
 *
 * Called when module is removed.
 */
static void __exit shtc1_exit(void)
{
    i2c_del_driver(&shtc1_i2c_driver);
}
module_exit(shtc1_exit);


MODULE_AUTHOR("Johannes Winkelmann <johannes.winkelmann@sensirion.com>");
MODULE_DESCRIPTION("Sensirion SHTC1 humidity and temperature sensor driver");
MODULE_LICENSE("GPL");
