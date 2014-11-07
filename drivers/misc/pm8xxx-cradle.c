/*
 * Copyright LG Electronics (c) 2011
 * All rights reserved.
 * Author: Fred Cho <fred.cho@lge.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/of_device.h>

#define HALL_STATUS_RESUME	0
#define HALL_STATUS_SUSPEND	1
#define HALL_STATUS_QUEUE_WORK	2

static int pre_set_flag;
struct pm8xxx_cradle {
    struct switch_dev sdev;
    struct work_struct work;
    struct device *dev;
    struct wake_lock wake_lock;
    const struct pm8xxx_cradle_platform_data *pdata;
    int carkit;
    int pouch;
    spinlock_t lock;
    int state;
    atomic_t flag;
};

static struct workqueue_struct *cradle_wq;
static struct pm8xxx_cradle *cradle;

static void boot_cradle_det_func(void)
{
    int state;

    if (cradle->pdata->pouch_detect_pin)
    cradle->pouch = !gpio_get_value(cradle->pdata->pouch_detect_pin);

    printk("%s : boot pouch === > %d \n", __func__ , cradle->pouch);

      if (cradle->pouch == 1)
        state = SMARTCOVER_POUCH_CLOSED;
      else
        state = SMARTCOVER_POUCH_OPENED;

    printk("%s : [Cradle] boot cradle value is %d\n", __func__ , state);
    cradle->state = state;
    switch_set_state(&cradle->sdev, cradle->state);

}

static void pm8xxx_cradle_work_func(struct work_struct *work)
{
    int state;
    unsigned long flags;

    spin_lock_irqsave(&cradle->lock, flags);

    if (cradle->pdata->pouch_detect_pin)
        cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);

    printk("%s : pouch === > %d \n", __func__ , cradle->pouch);

      if (cradle->pouch == 1)
        state = SMARTCOVER_POUCH_CLOSED;
      else
        state = SMARTCOVER_POUCH_OPENED;

    if (cradle->state != state) {
	cradle->state = state;
	spin_unlock_irqrestore(&cradle->lock, flags);
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
	switch_set_state(&cradle->sdev, cradle->state);
	printk("%s : [Cradle] cradle value is %d\n", __func__ , state);
    } else {
	spin_unlock_irqrestore(&cradle->lock, flags);
	printk("%s : [Cradle] cradle value is %d (no change)\n", __func__ , state);
    }

}

void cradle_set_deskdock(int state)
{
    unsigned long flags;

    if (&cradle->sdev) {
        spin_lock_irqsave(&cradle->lock, flags);
        cradle->state = state;
        spin_unlock_irqrestore(&cradle->lock, flags);
        switch_set_state(&cradle->sdev, cradle->state);
    } else {
        pre_set_flag = state;
    }
}

int cradle_get_deskdock(void)
{
    if (!cradle)
        return pre_set_flag;

    return cradle->state;
}


static irqreturn_t pm8xxx_pouch_irq_handler(int irq, void *handle)
{
    struct pm8xxx_cradle *cradle_handle = handle;
    printk("%s, irq : %d\n", __func__, irq);
    if (atomic_read(&cradle_handle->flag) == HALL_STATUS_SUSPEND) {
	atomic_set(&cradle_handle->flag, HALL_STATUS_QUEUE_WORK);
    } else {
	queue_work(cradle_wq, &cradle_handle->work);
    }
    return IRQ_HANDLED;
}


static ssize_t
cradle_pouch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len;
    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "pouch : %d\n", cradle->pouch);

    return len;

}

static ssize_t
cradle_sensing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len;
    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
    len = snprintf(buf, PAGE_SIZE, "sensing : %d\n", cradle->state);
    return len;

}

static struct device_attribute cradle_device_attrs[] = {
    __ATTR(pouch, S_IRUGO, cradle_pouch_show, NULL),
    __ATTR(sensing,  S_IRUGO, cradle_sensing_show, NULL),
};

static ssize_t cradle_print_name(struct switch_dev *sdev, char *buf)
{
    switch (switch_get_state(sdev)) {
    case 0:
        return sprintf(buf, "UNDOCKED\n");
    case 2:
        return sprintf(buf, "CARKIT\n");
    }
    return -EINVAL;
}

#ifdef CONFIG_OF
static struct of_device_id hall_ic_match[] = {
    { .compatible = "LG,Hall Sensor",},
    { },
};

static int cradle_parse_dt(struct device *dev, struct pm8xxx_cradle_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "gpio-base", &pdata->pouch_detect_pin) != 0) {
		printk("%s : get property from dt failed!!\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	pdata->irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	return 0;
}
#endif

static int __init pm8xxx_cradle_probe(struct platform_device *pdev)
{
    int ret, i;

    unsigned int hall_gpio_irq;
    const struct pm8xxx_cradle_platform_data *pdata;

    if (pdev->dev.of_node)
    {
	pdata = devm_kzalloc(&pdev->dev,
		sizeof(struct pm8xxx_cradle_platform_data), GFP_KERNEL);
	if (pdata == NULL) {
		pr_err("%s : Failed to allocated memory\n", __func__);
		return -ENOMEM;
	}
	pdev->dev.platform_data = pdata;
	cradle_parse_dt(&pdev->dev, pdata);
    } else {
	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -ENODEV;
    }

    cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
    if (!cradle) {
	pr_err("%s : Failed to allocated memory for cradle!\n", __func__);
        return -ENOMEM;
    }
    cradle->pdata = pdata;

    //cradle->sdev.name = "dock";
    cradle->sdev.name = "smartcover";
    cradle->sdev.print_name = cradle_print_name;
    cradle->pouch = cradle->carkit = 0;
    atomic_set(&cradle->flag, HALL_STATUS_RESUME);

    ret = gpio_request(cradle->pdata->pouch_detect_pin, "hall_detect_s");
    if (ret) {
	pr_err("%s : Failed to init gpio\n", __func__);
        gpio_free(cradle->pdata->pouch_detect_pin);
        return;
    }
    gpio_direction_input(cradle->pdata->pouch_detect_pin);

    spin_lock_init(&cradle->lock);

    ret = switch_dev_register(&cradle->sdev);

    if (ret < 0)
        goto err_switch_dev_register;

    if (pre_set_flag) {
        cradle_set_deskdock(pre_set_flag);
        cradle->state = pre_set_flag;
    }
    wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND, "hall_ic_wakeups");

    INIT_WORK(&cradle->work, pm8xxx_cradle_work_func);

    /* initialize irq of gpio_hall */
    hall_gpio_irq = gpio_to_irq(cradle->pdata->pouch_detect_pin);
    if (hall_gpio_irq < 0) {
	pr_err("%s : Failed to init irq!\n", __func__);
        ret = hall_gpio_irq;
        goto err_request_irq;
    }

    if (cradle->pdata->pouch_detect_pin == 150) {
	ret = request_irq(hall_gpio_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, "hall_sensor", (void *)cradle);
	if (ret) {
		pr_err("%s : Could not allocate IRQ\n", __func__);
		goto err_request_irq;
	}
    } else {
	ret = odin_gpio_sms_request_irq(hall_gpio_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, "hall_sensor", cradle);
	if (ret) {
		pr_err("%s : Could not allocate IRQ\n", __func__);
		goto err_request_irq;
	}
    }
/*    ret = odin_gpio_sms_request_threaded_irq(hall_gpio_irq, hall_pouch_irq_handler,NULL, pdata->irq_flags , "hall_sensor", hall);*/
/*    ret = odin_gpio_sms_request_irq(hall_gpio_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, "hall_sensor", cradle);*/
/*    ret = request_threaded_irq(hall_gpio_irq, NULL, pm8xxx_pouch_irq_handler, pdata->irq_flags, "hall_sensor", cradle); */

    if (ret<0) {
        printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, hall_gpio_irq, ret);
        goto err_request_irq;
    }

    enable_irq_wake(hall_gpio_irq);

    boot_cradle_det_func();

    for (i = 0; i < ARRAY_SIZE(cradle_device_attrs); i++) {
	ret = device_create_file(&pdev->dev, &cradle_device_attrs[i]);
	if (ret){
		goto err_request_irq;
	}
    }

    platform_set_drvdata(pdev, cradle);
    return 0;



err_request_irq:
    if (hall_gpio_irq)
        free_irq(hall_gpio_irq, 0);

err_switch_dev_register:
    switch_dev_unregister(&cradle->sdev);
    kfree(cradle);
    return ret;
}



static int __exit pm8xxx_cradle_remove(struct platform_device *pdev)
{
    struct pm8xxx_cradle *cradle = platform_get_drvdata(pdev);

    cancel_work_sync(&cradle->work);
    switch_dev_unregister(&cradle->sdev);
    platform_set_drvdata(pdev, NULL);
    kfree(cradle);

    return 0;
}

static int pm8xxx_cradle_suspend(struct device *dev)
{

    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);

    atomic_set(&cradle->flag, HALL_STATUS_SUSPEND);

    return 0;
}

static int pm8xxx_cradle_resume(struct device *dev)
{

    struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);

    if (atomic_read(&cradle->flag) == HALL_STATUS_QUEUE_WORK) {
	queue_work(cradle_wq, &cradle->work);
    }
    atomic_set(&cradle->flag, HALL_STATUS_RESUME);

    return 0;
}

static const struct dev_pm_ops pm8xxx_cradle_pm_ops = {
    .suspend = pm8xxx_cradle_suspend,
    .resume = pm8xxx_cradle_resume,
};

static struct platform_driver pm8xxx_cradle_driver = {
    .probe		= pm8xxx_cradle_probe,
    .remove		= __exit_p(pm8xxx_cradle_remove),
    .driver		= {
		.name	= HALL_IC_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= hall_ic_match,
#endif
#ifdef CONFIG_PM
		.pm	= &pm8xxx_cradle_pm_ops,
#endif
    },
};

static int __init pm8xxx_cradle_init(void)
{
    cradle_wq = create_singlethread_workqueue("cradle_wq");

    if (!cradle_wq)
        return -ENOMEM;
    return platform_driver_register(&pm8xxx_cradle_driver);
}
module_init(pm8xxx_cradle_init);

static void __exit pm8xxx_cradle_exit(void)
{
    if (cradle_wq)
        destroy_workqueue(cradle_wq);
    platform_driver_unregister(&pm8xxx_cradle_driver);
}
module_exit(pm8xxx_cradle_exit);


MODULE_ALIAS("platform:" HALL_IC_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("hall ic driver");
MODULE_LICENSE("GPL");

