/*
 * Maxim MAX77819 WLED Backlight Class Driver
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/leds.h>

/* for Regmap */
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>

#include <linux/hrtimer.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#define LOG_LEVEL  1

#include <linux/mfd/max77819.h>
#include <linux/mfd/max77819-wled.h>

/*
              
                                        
                                     
*/
#include <linux/board_lge.h>
/*              */

#define DRIVER_DESC    "MAX77819 WLED Driver"
#define DRIVER_NAME    MAX77819_WLED_NAME
#define DRIVER_VERSION MAX77819_DRIVER_VERSION".0"
#define DRIVER_AUTHOR  "Gyungoh Yoo <jack.yoo@maximintegrated.com>"

#define MAX17047_I2C_SLOT				   4
#define MAX17047_SLAVE_ADDRESS			   0x90
#define WLED_MAX_BRIGHTNESS                0x39
#define WLED_MIN_BRIGHTNESS                0x00

/* Register map */
#define WLEDBSTCNTL1                       0x98
#define WLEDBSTCNTL1_WLED1EN               BIT (7)
#define WLEDBSTCNTL1_WLED2EN               BIT (6)
#define WLEDBSTCNTL1_WLEDPWM1EN            BIT (5)
#define WLEDBSTCNTL1_WLEDPWM2EN            BIT (4)
#define WLEDBSTCNTL1_WLEDFOSC              BITS(3,2)
#define WLEDBSTCNTL1_WLEDOVP               BIT (1)

#define WLEDBSTCNTL1_WLEDEN                WLEDBSTCNTL1_WLED1EN|\
                                           WLEDBSTCNTL1_WLED2EN
#define WLEDBSTCNTL1_WLEDPWMEN             WLEDBSTCNTL1_WLEDPWM1EN|\
                                           WLEDBSTCNTL1_WLEDPWM2EN

#define IWLED                              0x99

/* Register */
#define BL_ON  1
#define BL_OFF 0

struct max77819_wled {
    struct mutex            lock;
	struct max77819_dev *chip;
	struct max77819_io	*io;
	spinlock_t 		irq_lock;
	struct pwm_device       *pwm_dev;
    struct backlight_device *bl_dev;
	struct regmap	        *regmap;
    int                     brightness;
	struct device *dev;
	struct kobject *kobj;
};

#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

static int current_bl_level;
static int backlight_status = BL_ON;
struct backlight_device *p_bl_dev;

static void max77819_bl_on(struct max77819_wled *me, int level)
{
    int ret;

    __lock(me);

    if (backlight_status == BL_OFF) {
        backlight_status = BL_ON;
        /* WLED Driver Control
        - WLED Boost OVP Threshold setting(1bit) :: 00(28V)
        - WLEDBST Converter Switching Frequency Select(2~3bits) :: 00(switches at 733KHz)
        - CABC Enable for Current 'Source 2' & 'Source 1'(4,5bit) :: 00(Not Affect)
        - Current 'Source 2' & 'Source 1' Enable(6,7bit) :: 11(all enables)
        */
        ret = regmap_write(me->regmap, WLEDBSTCNTL1,
                WLEDBSTCNTL1_WLEDOVP | WLEDBSTCNTL1_WLEDFOSC |
				WLEDBSTCNTL1_WLEDEN | WLEDBSTCNTL1_WLEDPWMEN);
        if (IS_ERR_VALUE(ret))
            pr_err("%s: can't write WLEDBSTCNTL : %d\n", __func__, ret);
    }

    current_bl_level = level;

    /* write current value */
    ret = regmap_write(me->regmap, IWLED, level);
    if (IS_ERR_VALUE(ret))
        pr_err("%s: can't write IWLED : %d\n", __func__, ret);

    __unlock(me);

    return;
}

static void max77819_bl_off(struct max77819_wled *me)
{
    int ret;

    __lock(me);

    if(backlight_status == BL_OFF){
        __unlock(me);
        return;
    }
    current_bl_level = WLED_MIN_BRIGHTNESS;
    backlight_status = BL_OFF;

    /* write current value to 0 */
    ret = regmap_write(me->regmap, IWLED, 0x00);
    if (IS_ERR_VALUE(ret))
        pr_err("%s: can't write IWLED : %d\n", __func__, ret);

    /* update control to 0 */
    ret = regmap_update_bits(me->regmap, WLEDBSTCNTL1,
            WLEDBSTCNTL1_WLED2EN | WLEDBSTCNTL1_WLED1EN, 0x00);
    if (IS_ERR_VALUE(ret))
        pr_err("%s: can't write WLEDBSTCNTL : %d\n", __func__, ret);

	__unlock(me);

    return;
}

static int max77819_bl_update_status (struct backlight_device *bl_dev)
{
	int level = bl_dev->props.brightness;
	struct max77819_wled *me = bl_get_data(bl_dev);

	if (level > WLED_MAX_BRIGHTNESS)
		level = WLED_MAX_BRIGHTNESS;
	else if (level < WLED_MIN_BRIGHTNESS)
		level = WLED_MIN_BRIGHTNESS;
        pr_info("%s: level:%d\n", __func__, level);
	if (level)
		max77819_bl_on(me, level);
	else
		max77819_bl_off(me);

	return 0;
}

static int max77819_bl_get_brightness (struct backlight_device *bl_dev)
{
    struct max77819_wled *me = bl_get_data(bl_dev);
    int rc;

    __lock(me);

    rc = me->brightness;

    __unlock(me);
    return rc;
}

const struct regmap_config max77819b_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static const struct backlight_ops max77819_bl_ops = {
	.update_status	= max77819_bl_update_status,
    .get_brightness = max77819_bl_get_brightness,
};

int bl_update_status (int val)
{
	p_bl_dev->props.brightness = val;
	max77819_bl_update_status(p_bl_dev);
	return 0;
}

int bl_get_brightness(void)
{
	return current_bl_level;
}

static int max77819_bl_probe (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
	struct max77819_dev *chip = dev_get_drvdata(dev->parent);
	struct max77819_wled *me;
    struct backlight_properties bl_props;
	struct ipc_client *regmap_ipc;
	unsigned int value;
    int rc;

	me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
	if (unlikely(!me)) {
		log_err("out of memory (%uB requested)\n", sizeof(*me));
		return -ENOMEM;
	}

    dev_set_drvdata(dev, me);

	spin_lock_init(&me->irq_lock);
	mutex_init(&me->lock);
	me->io   = max77819_get_io(chip);
	me->dev  = dev;
	me->kobj = &dev->kobj;

	memset(&bl_props, 0x00, sizeof(bl_props));
	bl_props.type			= BACKLIGHT_RAW;
	bl_props.brightness = me->brightness;
	bl_props.brightness = 0x39;
	bl_props.max_brightness = 0xFF;

	current_bl_level = bl_props.brightness;

/*
               
                                                     
                                      
 */

	if (lge_get_board_revno() == HW_REV_G)
		me->bl_dev = backlight_device_register("lcd-backlight", dev, me,
			&max77819_bl_ops, &bl_props);
	else
		goto abort;
/*              */
    if (unlikely(IS_ERR(me->bl_dev))) {
        rc = PTR_ERR(me->bl_dev);
        me->bl_dev = NULL;
        log_err("failed to register backlight device [%d]\n", rc);
        goto abort;
    }

/* Regmap IPC register */
	regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if(regmap_ipc == NULL){
		dev_err(dev, "Platform data not specified.\n");
		return -ENOMEM;
	}

	regmap_ipc->slot = MAX17047_I2C_SLOT;
	regmap_ipc->addr = MAX17047_SLAVE_ADDRESS;
	regmap_ipc->dev  = dev;

	me->regmap = devm_regmap_init_ipc(regmap_ipc, &max77819b_regmap_config);
	if (IS_ERR(me->regmap))
		return PTR_ERR(me->regmap);

	rc = regmap_write (me->regmap, IWLED, bl_props.brightness);
	rc = regmap_read (me->regmap, IWLED, &value);
	pr_info("%s: read reg 0x%02x = 0x%02x, ret=%d.\n", __func__, IWLED, value, rc);

	rc = regmap_write(me->regmap, WLEDBSTCNTL1,
				WLEDBSTCNTL1_WLEDOVP | WLEDBSTCNTL1_WLEDFOSC |
				WLEDBSTCNTL1_WLEDEN | WLEDBSTCNTL1_WLEDPWMEN);
	rc = regmap_read (me->regmap, WLEDBSTCNTL1, &value);
	pr_info("%s: read reg 0x%02x = 0x%02x, ret=%d.\n", __func__, WLEDBSTCNTL1, value, rc);

	p_bl_dev = me->bl_dev;

	log_info("driver "DRIVER_VERSION" installed\n");
	return 0;
abort:
    dev_set_drvdata(dev, NULL);
	return rc;
}

void max77819_bl_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77819_wled *me = dev_get_drvdata(dev);
	int rc;

	rc = regmap_write(me->regmap, WLEDBSTCNTL1, 0x00);
	log_info("%s: rc:%d \n",__func__, rc);

}

#ifdef CONFIG_OF
static struct of_device_id max77819_bl_of_ids[] = {
    { .compatible = "maxim,max77819-wled"},
    { },
};
MODULE_DEVICE_TABLE(of, max77819_bl_of_ids);
#endif /* CONFIG_OF */

static struct platform_driver max77819_bl_driver = {
    .driver.name            = DRIVER_NAME,
    .driver.owner           = THIS_MODULE,
#ifdef CONFIG_OF
    .driver.of_match_table  = max77819_bl_of_ids,
#endif /* CONFIG_OF */
    .probe                  = max77819_bl_probe,
    .shutdown               = max77819_bl_shutdown,
};

static int max77819_bl_init (void)
{
    return platform_driver_register(&max77819_bl_driver);
}

static void max77819_bl_exit (void)
{
    platform_driver_unregister(&max77819_bl_driver);
}

module_init(max77819_bl_init);
module_exit(max77819_bl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);
