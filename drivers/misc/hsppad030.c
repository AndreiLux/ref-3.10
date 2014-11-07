/* hsppad030_i2c.c
 *
 * Pressure device driver for I2C (HSPPAD030)
 *
 * Copyright (C) 2012 ALPS ELECTRIC CO., LTD. All Rights Reserved.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/async.h>

/* If i2c_board_info structure of HSPPAD sensor already registered,
  please enable the "RESISTER_HSPPAD_I2C".  */
#define RESISTER_HSPPAD_I2C

//#define ALPS_PRS_DEBUG

#define HSPPAD_DEVICE_NAME    "alps_pressure"

#define I2C_RETRY_DELAY       5
#define I2C_RETRIES           5

/* Comannd for hsppad */
#define HSPPAD_GET            0xAC

#define HSPPAD_DRIVER_NAME    "hsppad030"
#define I2C_HSPPAD_ADDR       (0x48)        /* 100 1000    */
#define I2C_BUS_NUMBER        2

#define EVENT_TYPE_PRESSURE   ABS_PRESSURE

#define ALPS_INPUT_FUZZ       0    /* input event threshold */
#define ALPS_INPUT_FLAT       0

static DEFINE_MUTEX(hsppad_lock);

static struct input_polled_dev *hsppad_idev;
static struct i2c_client *client_hsppad = NULL;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hsppad_early_suspend_handler;
#endif

static void hsppad_poll(struct input_polled_dev *dev);

static int flgEna = 0;
static int delay = 200;
static int flgSuspend = 0;
static int flgSkip = 1;


/*-----------------------------------------------------------------------------------------------*/
/* i2c read/write function                                                                      */
/*-----------------------------------------------------------------------------------------------*/
static int hsppad_i2c_read(u8 *rxData, int length)
{
    int err;
    int tries = 0;

    struct i2c_msg msgs[] = {
        {
            .addr  = client_hsppad->addr,
            .flags = I2C_M_RD,
            .len   = length,
            .buf   = rxData,
        },
    };

    do {
        err = i2c_transfer(client_hsppad->adapter, msgs, 1);
    } while ((err != 1) && (++tries < I2C_RETRIES));

    if (err != 1) {
        dev_err(&client_hsppad->adapter->dev, "read transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}

static int hsppad_i2c_write(u8 *txData, int length)
{
    int err;
    int tries = 0;
#ifdef ALPS_PRS_DEBUG
    int i;
#endif
    struct i2c_msg msg[] = {
        {
            .addr  = client_hsppad->addr,
            .flags = 0,
            .len   = length,
            .buf   = txData,
        },
    };

#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] i2c_write : ");
    for (i=0; i<length;i++) printk("0x%02X, ", txData[i]);
    printk("\n");
#endif

    do {
        err = i2c_transfer(client_hsppad->adapter, msg, 1);
    } while ((err != 1) && (++tries < I2C_RETRIES));

    if (err != 1) {
        dev_err(&client_hsppad->adapter->dev, "write transfer error\n");
        printk("[HSPPAD] i2c_write write transfer error\n");
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}

/*-----------------------------------------------------------------------------------------------*/
/* hsppad function                                                                               */
/*-----------------------------------------------------------------------------------------------*/
static int hsppad_get_pressure_data(int *spt)
{
    int   err = -1;
    u8    sx[5], buf;

    if (flgSuspend != 0) return err;
    if (flgSkip == 0) {
        err = hsppad_i2c_read(sx, 5);
        if (err < 0) return err;
        spt[0] = (int) ((short)sx[0]);
        spt[1] = (int) (((unsigned int)sx[1] << 8) | ((unsigned int)sx[2]));
        spt[2] = (int) (((unsigned int)sx[3] << 8) | ((unsigned int)sx[4]));

#ifdef ALPS_PRS_DEBUG
        printk("Pres_I2C, s:0x%04X, p:0x%04X, t:0x%04X\n", spt[0], spt[1], spt[2]);
#endif
    }
    else {
#ifdef ALPS_PRS_DEBUG
        printk("[HSPPAD] hsppad_get_pressure_data, skip\n");
#endif
        flgSkip = 0;
    }

    buf = HSPPAD_GET;
    hsppad_i2c_write(&buf, 1);

    return err;
}

static void hsppad_activate(int flg)
{
    if (flg != 0) flg = 1;

    flgEna = flg;
    if (flg) {
        u8 buf = HSPPAD_GET;
        flgSkip = 1;
        hsppad_i2c_write(&buf, 1);
    }
    else {
        delay = 200;
    }
}


/*-----------------------------------------------------------------------------------------------*/
/* sysfs                                                                                         */
/*-----------------------------------------------------------------------------------------------*/
static ssize_t hsppad_delay_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", delay);
}

static ssize_t hsppad_delay_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t size)
{
    int err;
    long new_delay;

    err = strict_strtol(buf, 10, &new_delay);
    if (err < 0)
        return err;

#if 0
    if (new_delay < 10) new_delay = 10;
#else
    /* set max 90Hz, 11ms poll dealy. */
    /* hsppad's one cycle time is 10ms. */
    /* poll delay should be longer than one cycle time to get correct pressure data */
    if (new_delay <= 10) new_delay = 11;
#endif
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_delay_store, delay = %d\n", (int)new_delay);
#endif

    mutex_lock(&hsppad_lock);
    delay = (int)new_delay;
    mutex_unlock(&hsppad_lock);

    return size;
}

static ssize_t hsppad_enable_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", (flgEna) ? 1 : 0);
}

static ssize_t hsppad_enable_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t size)
{
    int new_value;

    if (sysfs_streq(buf, "1"))
        new_value = 1;
    else if (sysfs_streq(buf, "0"))
        new_value = 0;
    else {
        pr_err("%s: invalid value %d\n", __func__, *buf);
        return -EINVAL;
    }

#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_enable_store, enable = %d\n", new_value);
#endif

    mutex_lock(&hsppad_lock);
    hsppad_activate(new_value);
    mutex_unlock(&hsppad_lock);

    return size;
}

static ssize_t hsppad_get_hw_data_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    int spt[3], val = 0;
    u8 sx = HSPPAD_GET;

    mutex_lock(&hsppad_lock);
    flgSkip = 0;
    hsppad_i2c_write(&sx, 1);
    mdelay(10);
    if (hsppad_get_pressure_data(spt) == 0) {
#ifdef ALPS_PRS_DEBUG
        printk("[HSPPAD] hsppad_get_hw_data_show, s:0x%04X, p:0x%04X, t:0x%04X\n", spt[0], spt[1], spt[2]);
#endif
        val = spt[1];
    }
    else {
        printk("[HSPPAD] hsppad_get_hw_data_show, read error.\n");
    }
    flgSkip = 1;
    mutex_unlock(&hsppad_lock);
    return sprintf(buf, "%d\n", val);
}

static DEVICE_ATTR(enable, S_IWUSR | S_IWGRP | S_IRUGO, hsppad_enable_show, hsppad_enable_store);
static DEVICE_ATTR(delay, S_IWUSR | S_IWGRP | S_IRUGO, hsppad_delay_show, hsppad_delay_store );
static DEVICE_ATTR(get_hw_data, S_IRUGO, hsppad_get_hw_data_show, NULL);

static struct attribute *hsppad_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_get_hw_data.attr,
    NULL,
};

static struct attribute_group hsppad_attribute_group = {
    .attrs = hsppad_attributes,
};


/*-----------------------------------------------------------------------------------------------*/
/* i2c device                                                                                    */
/*-----------------------------------------------------------------------------------------------*/
static int hsppad_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int rc, d[3];
    u8  buf = HSPPAD_GET;
    struct input_dev *idev;
#ifndef RESISTER_HSPPAD_I2C
    struct i2c_board_info i2c_info;
    struct i2c_adapter *adapter;
#endif

    printk("[HSPPAD] hsppad_probe\n");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->adapter->dev, "client not i2c capable\n");
        return -ENOMEM;
    }
#if 0 /*fix memory leak */
	/*                         
                                                                                             
                                                                           
                          
 */
    client_hsppad = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
    if (!client_hsppad) {
        dev_err(&client->adapter->dev, "failed to allocate memory for module data\n");
        return -ENOMEM;
    }
#endif
    client_hsppad = client;

    dev_info(&client->adapter->dev, "detected HSPPAD030 pressure sensor\n");

#ifndef RESISTER_HSPPAD_I2C
    /* It is adding i2c_bord_info in adapter. If you already have added
     i2c_board_info in adapter, you need to remove this code.           */
    memset(&i2c_info, 0, sizeof(struct i2c_board_info));
    i2c_info.addr = I2C_HSPPAD_ADDR;
    strlcpy(i2c_info.type, HSPPAD_DRIVER_NAME , I2C_NAME_SIZE);
    adapter = i2c_get_adapter(I2C_BUS_NUMBER);
    if (!adapter) {
        printk("can't get i2c adapter %d\n", I2C_BUS_NUMBER);
        rc = -ENOTSUPP;
    }

    /* It is adding i2c_bord_info in adapter. If you already have added
     i2c_board_info in adapter, you need to remove this code.           */
    client_hsppad = i2c_new_device(adapter, &i2c_info);
    if (!client_hsppad) {
        printk("can't add i2c device at 0x%x\n",(unsigned int)i2c_info.addr);
        rc = -ENOTSUPP;
    }
    client_hsppad->adapter->timeout = 0;
    client_hsppad->adapter->retries = 0;

    /* It is adding i2c_bord_info in adapter. If you already have added
     i2c_board_info in adapter, you need to remove this code.           */
    i2c_put_adapter(adapter);
#endif

    hsppad_idev = input_allocate_polled_device();
    if (!hsppad_idev) {
        printk(KERN_INFO "hsppad_probe: can't allocate polled device\n");
        rc = -ENOMEM;
    }
    printk(KERN_INFO "hsppad_probe: input_allocate_polled_device\n");

    hsppad_idev->poll = hsppad_poll;
    hsppad_idev->poll_interval = delay;

    /* initialize the input class */
    idev = hsppad_idev->input;
    idev->name = HSPPAD_DEVICE_NAME;
    idev->phys = HSPPAD_DEVICE_NAME "/input4";
    idev->id.bustype = BUS_HOST;
    idev->evbit[0] = BIT_MASK(EV_ABS);

    input_set_abs_params(idev, EVENT_TYPE_PRESSURE,
            0, 65535, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);

    rc = input_register_polled_device(hsppad_idev);
    if (rc)
        goto out_idev;
    printk(KERN_INFO "hsppad_probe: input_register_polled_device\n");

	rc = sysfs_create_group(&client->dev.kobj, &hsppad_attribute_group);
    if (rc)
        goto out_pdev;
    printk(KERN_INFO "hsppad_probe: sysfs_create_group\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&hsppad_early_suspend_handler);
    printk("hsppad_init: early_suspend_register\n");
#endif

    mutex_lock(&hsppad_lock);
    hsppad_i2c_write(&buf, 1);
    mdelay(15);
    hsppad_get_pressure_data(d);
    mutex_unlock(&hsppad_lock);
    printk("[HSPPAD] x:%04X y:%04X z:%04X\n",d[0],d[1],d[2]);
	printk("[HSPPAD] hsppad_probe success\n");
    return 0;

out_pdev:
    input_unregister_polled_device(hsppad_idev);
    printk(KERN_INFO "hsppad_probe: input_unregister_polled_device\n");
out_idev:
    input_free_polled_device(hsppad_idev);
    printk(KERN_INFO "hsppad_probe: input_free_polled_device\n");

    return rc;
}

static int hsppad_remove(struct i2c_client *client)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_remove\n");
#endif

#if 0  /*fix memory leak */
    kfree(client_hsppad);
#else
    client_hsppad = NULL;
#endif

    return 0;
}

static int hsppad_suspend(struct i2c_client *client,pm_message_t mesg)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_suspend\n");
#endif

    mutex_lock(&hsppad_lock);
    flgSuspend = 1;
    mutex_unlock(&hsppad_lock);
    return 0;
}

static int hsppad_resume(struct i2c_client *client)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_resume\n");
#endif

    mutex_lock(&hsppad_lock);
    flgSuspend = 0;
    hsppad_activate(flgEna);
    mutex_unlock(&hsppad_lock);
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hsppad_early_suspend(struct early_suspend *handler)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_early_suspend\n");
#endif
    hsppad_suspend(client_hsppad, PMSG_SUSPEND);
}

static void hsppad_early_resume(struct early_suspend *handler)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_early_resume\n");
#endif
    hsppad_resume(client_hsppad);
}
#endif

static const struct i2c_device_id hsppad_id[] = {
    { HSPPAD_DRIVER_NAME, 0 },
    { }
};

static struct i2c_driver hsppad_driver = {
    .probe     = hsppad_probe,
    .remove    = hsppad_remove,
    .id_table  = hsppad_id,
    .driver    = {
        .name  = HSPPAD_DRIVER_NAME,
    },
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend   = hsppad_suspend,
    .resume    = hsppad_resume,
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hsppad_early_suspend_handler = {
    .suspend = hsppad_early_suspend,
    .resume  = hsppad_early_resume,
};
#endif


/*-----------------------------------------------------------------------------------------------*/
/* input device                                                                                  */
/*-----------------------------------------------------------------------------------------------*/
static void hsppad_poll(struct input_polled_dev *dev)
{
    struct input_dev *idev = dev->input;

    mutex_lock(&hsppad_lock);
    dev->poll_interval = delay;
    if (flgSuspend == 0) {
        if (flgEna) {
            int spt[3];
            if (hsppad_get_pressure_data(spt) == 0) {
                input_report_abs(idev, EVENT_TYPE_PRESSURE, spt[1]);
                input_sync(idev);
            }
        }
    }
    mutex_unlock(&hsppad_lock);
}

static void hsppad_init_async(void *data, async_cookie_t cookie)
{
    int rc;

#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_init\n");
#endif

    rc = i2c_add_driver(&hsppad_driver);
    if (rc != 0) {
        printk("%s: can't add i2c driver\n", __FUNCTION__);
        rc = -ENOTSUPP;
    }

    return;
}

static int __init hsppad_init(void)
{
	async_schedule(hsppad_init_async, NULL);

	return 0;
}

static void __exit hsppad_exit(void)
{
#ifdef ALPS_PRS_DEBUG
    printk("[HSPPAD] hsppad_exit\n");
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&hsppad_early_suspend_handler);
    printk(KERN_INFO "hsppad_exit: unregister_early_suspend\n");
#endif
    sysfs_remove_group(&hsppad_idev->input->dev.kobj, &hsppad_attribute_group);
    printk(KERN_INFO "hsppad_exit: sysfs_remove_group\n");
    input_unregister_polled_device(hsppad_idev);
    printk(KERN_INFO "hsppad_exit: input_unregister_polled_device\n");
    input_free_polled_device(hsppad_idev);
    printk(KERN_INFO "hsppad_exit: input_free_polled_device\n");
    i2c_del_driver(&hsppad_driver);
    printk(KERN_INFO "hsppad_exit: i2c_del_driver\n");

}

module_init(hsppad_init);
module_exit(hsppad_exit);

MODULE_DESCRIPTION("Alps Pressure Input Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
