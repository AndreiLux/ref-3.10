/*
 * TI LM3697 Backlight Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/lm3697_bl.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/board_lge.h>

/* Registers */
#define LM3697_REG_OUTPUT_CFG		0x10

#define LM3697_REG_BRT_CFG		0x16

#define LM3697_REG_BOOST_CTL	0x1A
#define LM3697_REG_BOOST		0x02

#define LM3697_REG_PWM_CFG		0x1C

#define LM3697_REG_IMAX_A		0x17
#define LM3697_REG_IMAX_B		0x18

#define LM3697_REG_BRT_A_LSB		0x20
#define LM3697_REG_BRT_A_MSB		0x21
#define LM3697_REG_BRT_B_LSB		0x22
#define LM3697_REG_BRT_B_MSB		0x23
#define LM3697_BRT_LSB_MASK		(BIT(0) | BIT(1) | BIT(2))
#define LM3697_BRT_MSB_SHIFT		3

#define LM3697_REG_ENABLE		0x24

/* Other definitions */
#define LM3697_PWM_ID			1
#define LM3697_MAX_REGISTERS		0xB4
#define LM3697_MAX_STRINGS		3
//#define LM3697_MAX_BRIGHTNESS		2047
#define LM3697_MAX_BRIGHTNESS		255


#define LM3697_IMAX_OFFSET		6
#define LM3697_DEFAULT_NAME		"lcd-backlight"
#define LM3697_DEFAULT_PWM		"lm3697-backlight"

#define LM3697_EXPONENTIAL		0
#define LM3697_LINEAR			1

enum lm3697_bl_ctrl_mode {
	BL_REGISTER_BASED,
	BL_PWM_BASED,
};

/*
 * struct lm3697_bl_chip
 * @dev: Parent device structure
 * @regmap: Used for I2C register access
 * @pdata: LM3697 platform data
 */
struct lm3697_bl_chip {
	struct device *dev;
	struct regmap *regmap;
	struct lm3697_platform_data *pdata;
};
#define BL_OFF 0
#define BL_ON 1
static int backlight_status = BL_OFF;
static struct backlight_device *lm3697_device;
static int cur_main_lcd_level = LM3697_MAX_BRIGHTNESS;


/*
 * struct lm3697_bl
 * @bank_id: Control bank ID. BANK A or BANK A and B
 * @bl_dev: Backlight device structure
 * @chip: LM3697 backlight chip structure for low level access
 * @bl_pdata: LM3697 backlight platform data
 * @mode: Backlight control mode
 * @pwm: PWM device structure. Only valid in PWM control mode
 * @pwm_name: PWM device name
 */
struct lm3697_bl {
	int bank_id;
	struct backlight_device *bl_dev;
	struct lm3697_bl_chip *chip;
	struct lm3697_backlight_platform_data *bl_pdata;
	enum lm3697_bl_ctrl_mode mode;
	struct pwm_device *pwm;
	char pwm_name[20];

	struct led_classdev *led_dev;
};

static int lm3697_bl_write_byte(struct lm3697_bl_chip *chip, u8 reg, u8 data)
{
	unsigned int reg_val;
	regmap_write(chip->regmap, reg, data);
	regmap_read(chip->regmap, reg, &reg_val);

	pr_debug("%s: REGISTER : 0x%x, VALUE : 0x%x \n", __func__, reg, reg_val);
	return 0;
//	return regmap_write(chip->regmap, reg, data);
}

static int lm3697_bl_update_bits(struct lm3697_bl_chip *chip, u8 reg, u8 mask,
				 u8 data)
{
	unsigned int reg_val;
	regmap_update_bits(chip->regmap, reg, mask, data);
	regmap_read(chip->regmap, reg, &reg_val);

	pr_debug("%s: REGISTER : 0x%x, VALUE : 0x%x \n", __func__, reg, reg_val);

	return 0;
	//return regmap_update_bits(chip->regmap, reg, mask, data);
}

static int lm3697_bl_set_ctrl_mode(struct lm3697_bl *lm3697_bl)
{
	struct lm3697_backlight_platform_data *pdata = lm3697_bl->bl_pdata;
	int bank_id = lm3697_bl->bank_id;

	if (pdata->pwm_period > 0)
		lm3697_bl->mode = BL_PWM_BASED;
	else
		lm3697_bl->mode = BL_REGISTER_BASED;

	// Control bank assignment for PWM control
	if (lm3697_bl->mode == BL_PWM_BASED) {
		snprintf(lm3697_bl->pwm_name, sizeof(lm3697_bl->pwm_name),
			 "%s", LM3697_DEFAULT_PWM);

		return lm3697_bl_update_bits(lm3697_bl->chip,
					     LM3697_REG_PWM_CFG, BIT(bank_id),
					     1 << bank_id);
	}

	return 0;
}

static int lm3697_bl_string_configure(struct lm3697_bl *lm3697_bl)
{
	int bank_id = lm3697_bl->bank_id;
	int is_detected = 0;
	int i, ret;

	for (i = 0; i < LM3697_MAX_STRINGS; i++) {
		if (test_bit(i, &lm3697_bl->bl_pdata->bl_string)) {
			ret = lm3697_bl_update_bits(lm3697_bl->chip,
						    LM3697_REG_OUTPUT_CFG,
						    BIT(i), bank_id << i);
			if (ret)
				return ret;

			is_detected = 1;
		}
	}

	if (!is_detected) {
		dev_err(lm3697_bl->chip->dev, "No backlight string found\n");
		return -EINVAL;
	}

	return 0;
}

static int lm3697_bl_brightness_configure(struct lm3697_bl *lm3697_bl)
{
	int ret;

	if(lm3697_bl->bl_pdata->mapping_mode == LM3697_LINEAR){
		ret = lm3697_bl_write_byte(lm3697_bl->chip, LM3697_REG_BRT_CFG,
				     LM3697_LINEAR);
		pr_debug("%s: set Linear Mode\n", __func__);
	}
	else{
		ret = lm3697_bl_write_byte(lm3697_bl->chip, LM3697_REG_BRT_CFG,
				     LM3697_EXPONENTIAL);
		pr_debug("%s: set Exponential Mode\n", __func__);
	}

	return ret;
}

static int lm3697_bl_boost_configure(struct lm3697_bl *lm3697_bl)
{
	int ret;

	ret = lm3697_bl_write_byte(lm3697_bl->chip, LM3697_REG_BOOST_CTL,
			     LM3697_REG_BOOST);

	return ret;
}

static int lm3697_bl_set_current(struct lm3697_bl *lm3697_bl)
{
	u8 reg[] = { LM3697_REG_IMAX_A, LM3697_REG_IMAX_B, };

	return lm3697_bl_write_byte(lm3697_bl->chip, reg[lm3697_bl->bank_id],
				    lm3697_bl->bl_pdata->imax);
}

static int lm3697_bl_configure(struct lm3697_bl *lm3697_bl)
{
	int ret;

	ret = lm3697_bl_set_ctrl_mode(lm3697_bl);
	if (ret)
		return ret;
	mdelay(1);

	ret = lm3697_bl_string_configure(lm3697_bl);
	pr_debug("%s: lm3697_bl_string_configure \n", __func__);
	if (ret)
		return ret;
	mdelay(1);

	ret = lm3697_bl_brightness_configure(lm3697_bl);
	pr_debug("%s: lm3697_bl_brightness_configure \n", __func__);
	if (ret)
		return ret;
	mdelay(1);

	ret = lm3697_bl_boost_configure(lm3697_bl);
	pr_debug("%s: lm3697_bl_boost_configure \n", __func__);
	if (ret)
		return ret;
	mdelay(1);
	return lm3697_bl_set_current(lm3697_bl);
}


static int lm3697_bl_enable(struct lm3697_bl *lm3697_bl, int enable)
{
	backlight_status = enable;
	pr_debug("%s:enable:%d\n", __func__, enable);
	return lm3697_bl_update_bits(lm3697_bl->chip, LM3697_REG_ENABLE,
				     BIT(lm3697_bl->bank_id),
				     enable << lm3697_bl->bank_id);
}

static void lm3697_bl_pwm_ctrl(struct lm3697_bl *lm3697_bl, int br, int max_br)
{
	struct pwm_device *pwm;
	unsigned int duty, period;

	/* Request a PWM device with the consumer name */
	if (!lm3697_bl->pwm) {
		pwm = pwm_request(LM3697_PWM_ID, lm3697_bl->pwm_name);
		if (IS_ERR(pwm)) {
			dev_err(lm3697_bl->chip->dev,
				"Can not get PWM device: %s\n",
				lm3697_bl->pwm_name);
			return;
		}
		lm3697_bl->pwm = pwm;
	}

	period = lm3697_bl->bl_pdata->pwm_period;
	duty = br * period / max_br;

	pwm_config(lm3697_bl->pwm, duty, period);
	if (duty)
		pwm_enable(lm3697_bl->pwm);
	else
		pwm_disable(lm3697_bl->pwm);
}

static int lm3697_bl_set_brightness(struct lm3697_bl *lm3697_bl, int brightness)
{
	int ret;
	u8 data;
	u8 reg_lsb[] = { LM3697_REG_BRT_A_LSB, LM3697_REG_BRT_B_LSB, };
	u8 reg_msb[] = { LM3697_REG_BRT_A_MSB, LM3697_REG_BRT_B_MSB, };

	if (brightness > 2047) {
		brightness = 2047;
	}

	if (brightness < 0) {
		brightness = 0;
	}

// 	pr_info("%s: brightness : %d\n", __func__, brightness);
	cur_main_lcd_level = brightness;
//	lm3697_bl->bl_dev->props.brightness = cur_main_lcd_level;

	data = brightness & LM3697_BRT_LSB_MASK;
	ret = lm3697_bl_update_bits(lm3697_bl->chip,
				    reg_lsb[lm3697_bl->bank_id],
				    LM3697_BRT_LSB_MASK, data);
	if (ret)
		return ret;

	if (lm3697_bl->bl_pdata->blmap)
		data = (brightness >> LM3697_BRT_MSB_SHIFT) & 0xFF;
	else
		data = brightness;

	return lm3697_bl_write_byte(lm3697_bl->chip,
				    reg_msb[lm3697_bl->bank_id], data);
}

void lm3697_lcd_backlight_set_level(int level)
{
	struct lm3697_bl *lm3697_bl;
	int ret = 0;
	struct lm3697_backlight_platform_data *pdata;
	int cal_level = 0;

	if(lm3697_device == NULL)
	{
		pr_err("%s : lm3697 is not registered\n", __func__);
		return;
	}
	lm3697_bl = bl_get_data(lm3697_device);

	pdata = lm3697_bl->bl_pdata;

	lm3697_bl->bl_dev->props.brightness = level;
	lm3697_bl->led_dev->brightness = level;

	if (level == 0) {
		if (backlight_status == BL_ON){
			ret = lm3697_bl_enable(lm3697_bl, 0);
			pr_info("LM3697 Backlight Off \n");
			}
		return;
	} else{
		if (backlight_status == BL_OFF){
			lm3697_bl_configure(lm3697_bl);
			ret = lm3697_bl_enable(lm3697_bl, 1);
			pr_info("LM3697 Backlight On \n");
			}
	}

	if (ret) {
		pr_err("%s DEBUG error enable or disable backlight\n", __func__);
	}

	if (level >= pdata->blmap_size)
		level = pdata->blmap_size - 1;

	if(pdata->blmap)
		cal_level = pdata->blmap[level];
	else
		cal_level = level;

	//pr_info("%s: backlight level from table %d -> %d\n",__func__, level, cal_level);
	pr_info("backlight table %d -> %d\n", level, cal_level);

	ret = lm3697_bl_set_brightness(lm3697_bl, cal_level);


	if (ret) {
		pr_err("%s DEBUG error set backlight\n", __func__);
	}
}
EXPORT_SYMBOL(lm3697_lcd_backlight_set_level);

static int lm3697_bl_update_status(struct backlight_device *bl_dev)
{
	int ret = 0;

	lm3697_lcd_backlight_set_level(bl_dev->props.brightness);
/*
	struct lm3697_bl *lm3697_bl = bl_get_data(bl_dev);
	int brt;

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
		bl_dev->props.brightness = 0;

	brt = bl_dev->props.brightness;
	if (brt > 0)
		ret = lm3697_bl_enable(lm3697_bl, 1);
	else
		ret = lm3697_bl_enable(lm3697_bl, 0);

	if (ret)
		return ret;

	if (lm3697_bl->mode == BL_PWM_BASED)
		lm3697_bl_pwm_ctrl(lm3697_bl, brt,
				   bl_dev->props.max_brightness);
*/
	/*
	 * Brightness register should always be written
	 * not only register based mode but also in PWM mode.
	 */
//	return lm3697_bl_set_brightness(lm3697_bl, brt);

	return ret;

}

static int lm3697_bl_get_brightness(struct backlight_device *bl_dev)
{
	struct lm3697_bl *lm3697_bl = bl_get_data(bl_dev);
	struct lm3697_backlight_platform_data *pdata = lm3697_bl->bl_pdata;
	if (pdata->blmap_size)
		return pdata->blmap[bl_dev->props.brightness];
	else
		return bl_dev->props.brightness;
}

static const struct backlight_ops lm3697_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3697_bl_update_status,
	.get_brightness = lm3697_bl_get_brightness,
};


static enum lm3697_max_current lm3697_get_current_code(u8 imax_mA)
{
	const enum lm3697_max_current imax_table[] = {
		LM3697_IMAX_6mA,  LM3697_IMAX_7mA,  LM3697_IMAX_8mA,
		LM3697_IMAX_9mA,  LM3697_IMAX_10mA, LM3697_IMAX_11mA,
		LM3697_IMAX_12mA, LM3697_IMAX_13mA, LM3697_IMAX_14mA,
		LM3697_IMAX_15mA, LM3697_IMAX_16mA, LM3697_IMAX_17mA,
		LM3697_IMAX_18mA, LM3697_IMAX_19mA, LM3697_IMAX_20mA,
		LM3697_IMAX_21mA, LM3697_IMAX_22mA, LM3697_IMAX_23mA,
		LM3697_IMAX_24mA, LM3697_IMAX_25mA, LM3697_IMAX_26mA,
		LM3697_IMAX_27mA, LM3697_IMAX_28mA, LM3697_IMAX_29mA,
	};

	/*
	 * Convert milliampere to appropriate enum code value.
	 * Input range : 5 ~ 30mA
	 */

	if (imax_mA <= 5)
		return LM3697_IMAX_5mA;

	if (imax_mA >= 30)
		return LM3697_IMAX_30mA;

	return imax_table[imax_mA - LM3697_IMAX_OFFSET];
}

/* This helper funcion is moved from linux-v3.9 */
static inline int _of_get_child_count(const struct device_node *np)
{
	struct device_node *child;
	int num = 0;

	for_each_child_of_node(np, child)
		num++;

	return num;
}

static ssize_t lm3697_bl_show_tuning_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "lm3697_bl_tuning_brightness : %d\n",
			lm3697_device->props.brightness);
	return r;
}

static ssize_t lm3697_bl_store_tuning_brightness(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct lm3697_bl *lm3697_bl = bl_get_data(lm3697_device);
	int level;

	if(!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);
	lm3697_device->props.brightness = level;
	lm3697_bl_set_brightness(lm3697_bl, level);

	return count;
}

/* sysfs for tuning brightness(no use mapping table) */
DEVICE_ATTR(tuning_brightness, 0644, lm3697_bl_show_tuning_brightness,
		lm3697_bl_store_tuning_brightness);

static int lm3697_bl_parse_dt(struct device *dev, struct lm3697_bl_chip *chip)
{
	struct lm3697_platform_data *pdata;
	struct lm3697_backlight_platform_data *bl_pdata;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	int num_backlights;
	int i = 0;
	unsigned int imax_mA;
	static lge_boot_mode_type bl_boot_mode = LGE_BOOT_MODE_NORMAL;

	int j = 0;
	u32* array;

	if (!node) {
		dev_err(dev, "No device node exists\n");
		return -ENODEV;
	}

	bl_boot_mode = lge_get_boot_mode();
	pr_info("[%s]boot mode : %d \n", __func__, bl_boot_mode);
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	of_property_read_u32(node, "ti,enable-gpio", &pdata->en_gpio);
	if (pdata->en_gpio < 0)
		return pdata->en_gpio;

	num_backlights = _of_get_child_count(node);
	if (num_backlights == 0) {
		dev_err(dev, "No backlight strings\n");
		devm_kfree(dev, pdata);
		return -EINVAL;
	}

	bl_pdata = devm_kzalloc(dev, sizeof(*bl_pdata) * num_backlights,
				GFP_KERNEL);
	if (!bl_pdata) {
		devm_kfree(dev, pdata);
		return -ENOMEM;
	}

	for_each_child_of_node(node, child) {
		of_property_read_string(child, "backlight-name",
					&bl_pdata[i].name);

		/* Make backlight strings */
		bl_pdata[i].bl_string = 0;
		if (of_property_read_bool(child, "hvled1-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED1;
		if (of_property_read_bool(child, "hvled2-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED2;
		if (of_property_read_bool(child, "hvled3-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED3;

		imax_mA = 0;
		of_property_read_u32(child, "max-current-milliamp", &imax_mA);
		bl_pdata[i].imax = lm3697_get_current_code(imax_mA);

		if (bl_boot_mode >= LGE_BOOT_MODE_FACTORY &&
				bl_boot_mode <= LGE_BOOT_MODE_PIFBOOT3)
		{
			bl_pdata[i].init_brightness = 1;
		} else {
			of_property_read_u32(child, "initial-brightness",
					     &bl_pdata[i].init_brightness);
		}

		of_property_read_u32(child, "mapping-mode",
				&bl_pdata[i].mapping_mode);

		/* backlight LUT */
		of_property_read_u32(child, "blmap_size",
			&bl_pdata[i].blmap_size);

		if (bl_pdata[i].blmap_size) {
			array = kzalloc(sizeof(u32) * bl_pdata[i].blmap_size, GFP_KERNEL);
			if (!array) {
				pr_err("no more mem for array\n");
				devm_kfree(dev, pdata);
				devm_kfree(dev, bl_pdata);
				return -ENOMEM;
			}
			of_property_read_u32_array(child, "blmap", array, bl_pdata[i].blmap_size);
			bl_pdata[i].blmap = kzalloc(sizeof(u16) * bl_pdata[i].blmap_size, GFP_KERNEL);
			if (!bl_pdata[i].blmap){
					pr_err("no more mem for blmap\n");
					devm_kfree(dev, pdata);
					devm_kfree(dev, bl_pdata);
					kfree(array);
					return -ENOMEM;
				}

				for (j = 0; j < bl_pdata[i].blmap_size; j++)
					bl_pdata[i].blmap[j] = (u16)array[j];

				kfree(array);
		} else {
				pr_err("not defined blmap_size");
		}

		/* PWM mode */
		of_property_read_u32(child, "pwm-period",
				     &bl_pdata[i].pwm_period);

		i++;
	}

	pdata->bl_pdata = bl_pdata;
	pdata->num_backlights = num_backlights;
	chip->pdata = pdata;

	return 0;
}

static int lm3697_bl_enable_hw(struct lm3697_bl_chip *chip)
{
	return gpio_request_one(chip->pdata->en_gpio, GPIOF_OUT_INIT_HIGH,
			"lm3697_hwen");
}

static void lm3697_bl_disable_hw(struct lm3697_bl_chip *chip)
{
	if (chip->pdata->en_gpio) {
		gpio_set_value(chip->pdata->en_gpio, 0);
		gpio_free(chip->pdata->en_gpio);
	}
}

void lm3697_lcd_backlight_set_level_For_ESD(int level,int on_off)
{
	struct lm3697_bl *lm3697_bl;
	int ret = 0;
	struct lm3697_backlight_platform_data *pdata;
	int cal_level = 0;

	if(lm3697_device == NULL)
	{
		pr_err("%s : lm3697 is not registered\n", __func__);
		return;
	}
	lm3697_bl = bl_get_data(lm3697_device);

	pdata = lm3697_bl->bl_pdata;

	lm3697_bl->bl_dev->props.brightness = level;
	lm3697_bl->led_dev->brightness = level;

	if (on_off == 0) {

			ret = lm3697_bl_enable(lm3697_bl, 0);
			lm3697_bl_disable_hw(lm3697_bl->chip);

			pr_info("ESD LM3697 Backlight Off \n");

		return;
	} else{

			lm3697_bl_enable_hw(lm3697_bl->chip);
			lm3697_bl_configure(lm3697_bl);
			ret = lm3697_bl_enable(lm3697_bl, 1);
			pr_info("ESD LM3697 Backlight On \n");

	}

	if (ret) {
		pr_err("%s DEBUG error enable or disable backlight\n", __func__);
	}

	if (level >= pdata->blmap_size)
		level = pdata->blmap_size - 1;

	if(pdata->blmap)
		cal_level = pdata->blmap[level];
	else
		cal_level = level;

	//pr_info("%s: backlight level from table %d -> %d\n",__func__, level, cal_level);
	pr_info("backlight table %d -> %d\n", level, cal_level);

	ret = lm3697_bl_set_brightness(lm3697_bl, cal_level);


	if (ret) {
		pr_err("%s DEBUG error set backlight\n", __func__);
	}
}
EXPORT_SYMBOL(lm3697_lcd_backlight_set_level_For_ESD);

static void lm3697_bl_update_status_led(struct led_classdev *led_cdev,
												enum led_brightness value)
{
	struct lm3697_bl *lm3697_bl = dev_get_drvdata(led_cdev->dev->parent);
	if (value > LM3697_MAX_BRIGHTNESS)
		value = LM3697_MAX_BRIGHTNESS;

	lm3697_bl->bl_dev->props.brightness = value;

	lm3697_bl_update_status(lm3697_bl->bl_dev);
}

static struct led_classdev lm3697_bl_led = {
	.name                   = LM3697_DEFAULT_NAME,
	.brightness             = 0,
	.max_brightness = LM3697_MAX_BRIGHTNESS,
	.brightness_set = lm3697_bl_update_status_led,
};

static int lm3697_bl_add_device(struct lm3697_bl *lm3697_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct lm3697_backlight_platform_data *pdata = lm3697_bl->bl_pdata;
	char name[20];
	int ret = 0;

        if(pdata == NULL)
                return -EINVAL;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = pdata->init_brightness;
	props.max_brightness = LM3697_MAX_BRIGHTNESS;

	/* Backlight device name */
	if (!pdata->name)
		snprintf(name, sizeof(name), "%s:%d", LM3697_DEFAULT_NAME,
			 lm3697_bl->bank_id);
	else
		snprintf(name, sizeof(name), "%s", pdata->name);

	bl_dev = backlight_device_register(name, lm3697_bl->chip->dev,
					   lm3697_bl, &lm3697_bl_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lm3697_bl->bl_dev = bl_dev;

	lm3697_device = bl_dev;

	lm3697_bl_led.name = name;
	lm3697_bl_led.brightness = props.brightness;

	ret = led_classdev_register(lm3697_bl->chip->dev, &lm3697_bl_led);
	if (ret) {
		pr_err("led_classdev_register failed\n");
		return ret;
	}

	lm3697_bl->led_dev = &lm3697_bl_led;

	return 0;
}

static struct lm3697_bl *lm3697_bl_register(struct lm3697_bl_chip *chip)
{
	struct lm3697_backlight_platform_data *pdata = chip->pdata->bl_pdata;
	struct lm3697_bl *lm3697_bl, *each;
	int num_backlights = chip->pdata->num_backlights;
	int i, ret;

	lm3697_bl = devm_kzalloc(chip->dev, sizeof(*lm3697_bl) * num_backlights,
				 GFP_KERNEL);
	if (!lm3697_bl)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_backlights; i++) {
		each = lm3697_bl + i;
		each->bank_id = i;
		each->chip = chip;
		each->bl_pdata = pdata + i;

//		ret = lm3697_bl_configure(lm3697_bl);
		ret = lm3697_bl_configure(each);
		if (ret) {
			dev_err(chip->dev, "Backlight config err: %d\n", ret);
			goto err;
		}

		ret = lm3697_bl_add_device(each);
		if (ret) {
			dev_err(chip->dev, "Backlight device err: %d\n", ret);
			goto cleanup_backlights;
		}

		ret = device_create_file(chip->dev, &dev_attr_tuning_brightness);
                if(ret != 0)
                       dev_err(chip->dev, "dev_attr_tuning_brightness err:%d\n", ret);

		backlight_status = BL_ON;
		backlight_update_status(each->bl_dev);
	}

	return lm3697_bl;

cleanup_backlights:
	while (--i >= 0) {
		each = lm3697_bl + i;
		backlight_device_unregister(each->bl_dev);
		led_classdev_unregister(each->led_dev);
	}
err:
	return ERR_PTR(ret);
}

static int lm3697_bl_unregister(struct lm3697_bl *lm3697_bl)
{
	struct lm3697_bl *each;
	struct backlight_device *bl_dev;
	int num_backlights = lm3697_bl->chip->pdata->num_backlights;
	int i;

	for (i = 0; i < num_backlights; i++) {
		each = lm3697_bl + i;

		bl_dev = each->bl_dev;
		bl_dev->props.brightness = 0;
		backlight_update_status(bl_dev);
		if (each->bl_pdata->blmap)
			kfree(each->bl_pdata->blmap);
		backlight_device_unregister(bl_dev);
		led_classdev_unregister(each->led_dev);
	}

	return 0;
}

static struct regmap_config lm3697_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LM3697_MAX_REGISTERS,
};

int bl_get_brightness(void)
{
	return cur_main_lcd_level;
}

static int lm3697_bl_probe(struct i2c_client *cl,
			   const struct i2c_device_id *id)
{
	struct device *dev = &cl->dev;
	struct lm3697_platform_data *pdata = dev_get_platdata(dev);
	struct lm3697_bl_chip *chip;
	struct lm3697_bl *lm3697_bl;
	int ret;
	int num_backlights;
	pr_info("%s : probe start \n", __func__);

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(cl, &lm3697_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		goto error;
	}

	if (!chip->pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = lm3697_bl_parse_dt(dev, chip);
		else
			ret = -ENODEV;

		if (ret)
			goto error;
	}

	ret = lm3697_bl_enable_hw(chip);
	if (ret)
		goto error_enable;

	lm3697_bl = lm3697_bl_register(chip);
	if (IS_ERR(lm3697_bl)) {
		ret = PTR_ERR(lm3697_bl);
		goto error_register;
	}

	i2c_set_clientdata(cl, lm3697_bl);

	pr_info("%s : probe done\n", __func__);
	return 0;

error_register:
	gpio_free(chip->pdata->en_gpio);

error_enable:
	/* chip->pdata and chip->pdata->bl_pdata
	 * are allocated in lm3697_bl_parse_dt() by devm_kzalloc()
	 * bl_pdata->blmap is allocated by zalloc()
	 */
	num_backlights = chip->pdata->num_backlights;
	devm_kfree(dev, chip->pdata->bl_pdata);
	devm_kfree(dev, chip->pdata);

error:
	devm_kfree(dev, chip);
	pr_err("%s : probe failed\n", __func__);
	return ret;

}

static int lm3697_bl_remove(struct i2c_client *cl)
{
	struct lm3697_bl *lm3697_bl = i2c_get_clientdata(cl);
	pr_info("%s : lm3697_bl_remove \n", __func__);
	lm3697_bl_unregister(lm3697_bl);
	lm3697_bl_disable_hw(lm3697_bl->chip);

	return 0;
}

static const struct i2c_device_id lm3697_bl_ids[] = {
	{ "lm3697", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3697_bl_ids);

#ifdef CONFIG_OF
static const struct of_device_id lm3697_bl_of_match[] = {
	{ .compatible = "ti,lm3697", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3697_bl_of_match);
#endif

static struct i2c_driver lm3697_bl_driver = {
	.probe = lm3697_bl_probe,
	.remove = lm3697_bl_remove,
	.driver = {
		.name = "lm3697",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3697_bl_of_match),
	},
	.id_table = lm3697_bl_ids,
};
module_i2c_driver(lm3697_bl_driver);

MODULE_DESCRIPTION("TI LM3697 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
