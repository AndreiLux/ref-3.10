
/*
* Simple driver for Texas Instruments LM3646 LED Flash driver chip
* Copyright (C) 2013 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/
#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/leds-lm3646.h>
#include <linux/delay.h>

#define LM3646_FLASH_DRIVER_DEBUG
#undef CDBG
#ifdef LM3646_FLASH_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


#define REG_REV			0x00
#define REG_MODE		0x01
#define REG_STR_CTRL	0x04
#define REG_MAX_BR		0x05
#define REG_FLASH_BR	0x06
#define REG_TORCH_BR	0x07

#define STR_OFF "off"
#define NUM_OFF "0"

#define WARM_FLASH_CTRL_SIZE 8

struct warm_flash {
	u8 max_current;
	u8 led1_current;
};

enum lm3646_mode {
	MODE_STDBY = 0x0,
	MODE_TORCH = 0x2,
	MODE_FLASH = 0x3,
	MODE_MAX
};

enum lm3646_devfile {
	DFILE_FLASH_CTRL = 0,
	DFILE_FLASH_LED1,
	DFILE_FLASH_DUR,
	DFILE_TORCH_CTRL,
	DFILE_TORCH_LED1,
	DFILE_WARM_FLASH,
	DFILE_MAX
};

struct lm3646 {
	struct device *dev;

	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;

	struct work_struct work_flash;
	struct work_struct work_torch;

	u8 br_flash;
	u8 br_torch;
	u8 max_flash;
	u8 max_torch;

	struct lm3646_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};

static int gpio = 0;

int lm3646_gpio_enable(void) {
	if(gpio <= 0) {
		pr_err("failed to enable gpio(%d)\n", gpio);
		return -EFAULT;
	}

	if(gpio_is_valid(gpio)) {
		CDBG("enable gpio %d\n", gpio);
		gpio_request(gpio, "FLASH_CNTL_EN");
		gpio_direction_output(gpio, 0);
		gpio_direction_output(gpio, 1);
	} else {
		pr_err("failed to enable gpio(%d)\n", gpio);
		return -EFAULT;
	}
	return 0;
}
EXPORT_SYMBOL(lm3646_gpio_enable);

int lm3646_gpio_disable(void) {
	if(gpio <= 0) {
		pr_err("failed to disable gpio(%d)\n", gpio);
		return -EFAULT;
	}

	if(gpio_is_valid(gpio)) {
		CDBG("disable gpio %d\n", gpio);
		gpio_direction_output(gpio, 0);
		gpio_free(gpio);
	} else {
		pr_err("failed to enable gpio(%d)\n", gpio);
		return -EFAULT;
	}
	return 0;
}
EXPORT_SYMBOL(lm3646_gpio_disable);

static int lm3646_read_byte(struct lm3646 *pchip, u8 addr)
{
	int rval, ret;

	CDBG("E\n");

	ret = regmap_read(pchip->regmap, addr, &rval);
	if (ret < 0)
		return ret;
	return rval;
}

#if defined(LM3646_FLASH_DRIVER_DEBUG)
static void lm3646_reg_dump(struct lm3646 *pchip)
{
	int i = 0;
	int ret = 0;
	pr_err("========================\n");
	for(i=0; i<9; i++) {
		ret = lm3646_read_byte(pchip, (u8)i);
		pr_err("addr = 0x%2x, value = 0x%2x\n", i, ret);
		if(ret < 0)
			break;
	}
	pr_err("========================\n");

	return;
}
#endif

static int lm3646_update_byte(struct lm3646 *pchip, u8 addr, u8 mask, u8 data)
{
	CDBG("E\n");
/*	CDBG("E, 0x%x, 0x%x, 0x%x\n", addr, mask, data);*/
	return regmap_update_bits(pchip->regmap, addr, mask, data);
}

static void lm3646_input_control(struct lm3646 *pchip,
				 const char *buf, u8 reg, u8 mask)
{
	int rval, ival;

	CDBG("E, buf = 0x%2x, mask = 0x%2x\n", *(u8*)buf, mask);

	rval = kstrtouint(buf, 10, &ival);
	if (rval) {
		dev_err(pchip->dev, "str to int fail. rval %d\n", rval);
		return;
	}

	mutex_lock(&pchip->lock);
	rval = lm3646_update_byte(pchip, reg, mask, ival);
	mutex_unlock(&pchip->lock);
	if (rval < 0)
		dev_err(pchip->dev, "i2c access fail.\n");
}

static int lm3646_chip_init(struct lm3646 *pchip,
			    struct lm3646_platform_data *pdata)
{
	int rval;
	char buf[2] = {0};

	CDBG("E\n");

	rval = lm3646_read_byte(pchip, REG_REV);
	if (rval < 0)
		goto out;
	dev_info(pchip->dev, "LM3646 CHIP_ID/REV[0x%x]\n", rval);

	if (pdata == NULL) {
		pdata =
		    kzalloc(sizeof(struct lm3646_platform_data), GFP_KERNEL);
		if (pdata == NULL)
			return -ENODEV;
		pdata->flash_imax = 0x0F;
		pdata->torch_imax = 0x07;
		pdata->led1_flash_imax = 0x7F;
		pdata->led1_torch_imax = 0x7F;
	}
	pchip->pdata = pdata;

/*
	sprintf(buf, "%d", pdata->flash_timing);
	lm3646_input_control(pchip, buf, REG_STR_CTRL, 0x07);

	rval = lm3646_update_byte(pchip, REG_MODE, 0x08, pdata->tx_pin);
	if (rval < 0)
		goto out;
	rval = lm3646_update_byte(pchip, REG_TORCH_BR, 0xFF,
				  pdata->torch_pin | pdata->led1_torch_imax);
	if (rval < 0)
		goto out;
	rval = lm3646_update_byte(pchip, REG_FLASH_BR, 0xFF,
				  pdata->strobe_pin | pdata->led1_flash_imax);
	if (rval < 0)
		goto out;
	rval = lm3646_update_byte(pchip, REG_MAX_BR, 0x7F,
				  (pdata->torch_imax << 4) | pdata->flash_imax);
	if (rval < 0)
		goto out;
	pchip->br_flash = pdata->flash_imax;
	pchip->br_torch = pdata->torch_imax;
*/
	return rval;
out:
	dev_err(pchip->dev, "i2c acces fail.\n");
	return rval;
}

static void lm3646_mode_ctrl(struct lm3646 *pchip,
			     const char *buf, enum lm3646_mode mode)
{
	int rval;

	CDBG("%s E, str = %s, mode = %d\n", __func__, buf, mode);

	if (strncmp(buf, STR_OFF, 3) == 0 || strncmp(buf, NUM_OFF, 1) == 0)
		mode = MODE_STDBY;

	mutex_lock(&pchip->lock);
	rval = lm3646_update_byte(pchip, REG_MODE, 0x03, mode);
	mutex_unlock(&pchip->lock);
	if (rval < 0)
		dev_err(pchip->dev, "i2c access fail.\n");
}

/* torch brightness(max current) control */
static void lm3646_deferred_torch_brightness_set(struct work_struct *work)
{
	int rval;
	struct lm3646 *pchip = container_of(work, struct lm3646, work_torch);

	CDBG("E, brightness = %d, max = %d\n", pchip->br_torch, pchip->max_torch);

	if(pchip->max_torch == 0x0) {
		CDBG("torch off\n");
		lm3646_mode_ctrl(pchip, "0", MODE_TORCH);
	} else {
		rval = lm3646_update_byte(pchip, REG_TORCH_BR, 0xFF,
					  pchip->pdata->torch_pin | pchip->br_torch);
		if (rval < 0)
			dev_err(pchip->dev, "i2c access fail.\n");

		rval = lm3646_update_byte(pchip, REG_MAX_BR, 0x7F,
				  (pchip->max_torch << 4) | pchip->max_flash);
		if (rval < 0)
			dev_err(pchip->dev, "i2c access fail.\n");

		//for test
		lm3646_mode_ctrl(pchip, "1", MODE_TORCH);
	}
}

static void lm3646_torch_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3646 *pchip = container_of(cdev, struct lm3646, cdev_torch);

	pchip->br_torch = brightness;
	schedule_work(&pchip->work_torch);
}

static void lm3646_torch_brightness_set2(struct led_classdev *cdev,
					enum led_brightness brightness, enum led_brightness max_brightness)
{
	struct lm3646 *pchip = container_of(cdev, struct lm3646, cdev_torch);

	pchip->br_torch = brightness;
	pchip->max_torch = max_brightness;
	schedule_work(&pchip->work_torch);
}

/* torch on/off(mode) control */
static ssize_t lm3646_torch_ctrl_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_torch);

	CDBG("%s E, buf = 0x%2x, size = %d\n", __func__, *(unsigned int *)buf, size);

	lm3646_mode_ctrl(pchip, buf, MODE_TORCH);
	return size;
}
void lm3646_torch_ctrl_show(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{

	return 0;
}


/* torch dual led control */
static ssize_t lm3646_torch_iled1_ctrl_store(struct device *dev,
					     struct device_attribute *devAttr,
					     const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_torch);

	CDBG("%s E, buf = 0x%2x, size = %d\n", __func__, *(unsigned int *)buf, size);

	lm3646_input_control(pchip, buf, REG_TORCH_BR, 0x7F);
	return size;
}
void lm3646_torch_iled1_ctrl_show(struct device *dev,
					     struct device_attribute *devAttr,
					     const char *buf, size_t size)
{

	return 0;
}


/* flash brightness(max current) control */
static void lm3646_deferred_flash_brightness_set(struct work_struct *work)
{
	int rval;
	struct lm3646 *pchip = container_of(work, struct lm3646, work_flash);

	CDBG("E, brightness = %d, max = %d\n", pchip->br_flash, pchip->max_flash);

	if(pchip->br_flash == 0x0) {
		CDBG("flash off\n");
		lm3646_mode_ctrl(pchip, "0", MODE_FLASH);
	} else {

		#if 1
		rval = lm3646_update_byte(pchip, REG_STR_CTRL,0xFF,0x45);

		if (rval < 0)
			dev_err(pchip->dev, "i2c access fail.\n");
		#endif

		rval = lm3646_update_byte(pchip, REG_FLASH_BR, 0xFF,
					  pchip->pdata->strobe_pin | pchip->br_flash);
		if (rval < 0)
			dev_err(pchip->dev, "i2c access fail.\n");

		rval = lm3646_update_byte(pchip, REG_MAX_BR, 0x7F,
				  (pchip->max_torch << 4) | pchip->max_flash);
		if (rval < 0)
			dev_err(pchip->dev, "i2c access fail.\n");

		lm3646_mode_ctrl(pchip, "1", MODE_FLASH);
	}
}

static void lm3646_flash_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm3646 *pchip = container_of(cdev, struct lm3646, cdev_flash);

	pchip->br_flash = brightness;
	schedule_work(&pchip->work_flash);
}

static void lm3646_flash_brightness_set2(struct led_classdev *cdev,
					enum led_brightness brightness, enum led_brightness max_brightness)
{
	struct lm3646 *pchip = container_of(cdev, struct lm3646, cdev_flash);

	pchip->br_flash = brightness;
	pchip->max_flash = max_brightness;
	schedule_work(&pchip->work_flash);
}

/* flash on(mode) control */
static ssize_t lm3646_flash_ctrl_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_flash);

	CDBG("%s E, buf = 0x%2x, size = %d\n", __func__, *(unsigned int *)buf, size);

	lm3646_mode_ctrl(pchip, buf, MODE_FLASH);
	return size;
}
void lm3646_flash_ctrl_show(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{

	return 0;
}


/* flash dual led control */
static ssize_t lm3646_flash_iled1_ctrl_store(struct device *dev,
					     struct device_attribute *devAttr,
					     const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_flash);

	CDBG("%s E, buf = 0x%2x, size = %d\n", __func__, *(unsigned int *)buf, size);

	lm3646_input_control(pchip, buf, REG_FLASH_BR, 0x7F);
	return size;
}
void lm3646_flash_iled1_ctrl_show(struct device *dev,
					     struct device_attribute *devAttr,
					     const char *buf, size_t size)
{

	return 0;
}

/* flash duration(timeout) control */
static ssize_t lm3646_flash_duration_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_flash);

	CDBG("%s E, buf = %x, size = %d, len = %d\n", __func__, *(unsigned char *)buf, size, strlen(buf));

	lm3646_input_control(pchip, buf, REG_STR_CTRL, 0x07);
	return size;
}
/* flash duration(timeout) control */
void lm3646_flash_duration_show(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{

	return 0;
}


/* warm-flash setting data */
static struct warm_flash warm_flash_set[WARM_FLASH_CTRL_SIZE] = {
	/* LED1 = MAX, LED2 = Diabled */
	[0] = {0x0F, 0x7F},
	[1] = {0x0F, 0x3F},
	[2] = {0x0F, 0x1F},
	[3] = {0x0F, 0x0F},
	[4] = {0x0F, 0x07},
	[5] = {0x0F, 0x03},
	[6] = {0x0F, 0x01},
	/* LED1 = Diabled, LED2 = MAX */
	[7] = {0x0F, 0x00},
};

/* flash duration(timeout) control */
static ssize_t lm3646_warm_flash_store(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3646 *pchip =
	    container_of(led_cdev, struct lm3646, cdev_flash);

	int rval, ival;

	CDBG("E\n");

	rval = kstrtouint(buf, 10, &ival);
	if (rval) {
		dev_err(pchip->dev, "str to int fail.\n");
		goto out_err;
	}

	if (ival > WARM_FLASH_CTRL_SIZE - 1) {
		dev_err(pchip->dev, "input error.\n");
		goto out_err;
	}

	mutex_lock(&pchip->lock);
	rval =
	    lm3646_update_byte(pchip, REG_MAX_BR, 0x0F,
			       warm_flash_set[ival].max_current);
	if (rval < 0)
		goto out;
	rval =
	    lm3646_update_byte(pchip, REG_FLASH_BR, 0x7F,
			       warm_flash_set[ival].led1_current);
	if (rval < 0)
		goto out;
	if (pchip->pdata->strobe_pin == LM3646_STROBE_PIN_DISABLED)
		lm3646_update_byte(pchip, REG_MODE, 0x03, MODE_FLASH);
out:
	mutex_unlock(&pchip->lock);
	if (rval < 0)
		dev_err(pchip->dev, "i2c access fail.\n");
out_err:
	return size;
}
void lm3646_warm_flash_show(struct device *dev,
				       struct device_attribute *devAttr,
				       const char *buf, size_t size)
{

	return 0;
}

#define lm3646_attr(_name, _show, _store)\
{\
	.attr = {\
		.name = _name,\
		.mode = 0644,\
	},\
	.show = _show,\
	.store = _store,\
}

static struct device_attribute dev_attr_ctrl[DFILE_MAX] = {
	[DFILE_FLASH_CTRL] = lm3646_attr("ctrl", lm3646_flash_ctrl_show, lm3646_flash_ctrl_store),
	[DFILE_FLASH_LED1] =
	    lm3646_attr("iled1", lm3646_flash_iled1_ctrl_show, lm3646_flash_iled1_ctrl_store),
	[DFILE_FLASH_DUR] = lm3646_attr("duration",
					lm3646_flash_duration_show, lm3646_flash_duration_store),
	[DFILE_TORCH_CTRL] = lm3646_attr("ctrl", lm3646_torch_ctrl_show, lm3646_torch_ctrl_store),
	[DFILE_TORCH_LED1] =
	    lm3646_attr("iled1", lm3646_torch_iled1_ctrl_show, lm3646_torch_iled1_ctrl_store),
	[DFILE_WARM_FLASH] =
	    lm3646_attr("warmness", lm3646_warm_flash_show, lm3646_warm_flash_store),
};

static const struct regmap_config lm3646_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

#if defined(CONFIG_MACH_LGE) || defined (CONFIG_LEDS_LM3646)
static int lm3646_get_dt_data(struct device *dev,
		struct lm3646_platform_data *pdata) {

	struct device_node *np = dev->of_node;
	struct device_node *flash_src_node = NULL;

	int32_t val = 0;
	int32_t rc = 0;

	if (of_property_read_bool(np, "lm3646,tx-pin-enable") == true) {
		rc = of_property_read_u32(np, "lm3646,tx-pin-enable", &val);
		if (rc < 0) {
			pr_err("read lm3646,tx-pin-enable failed rc %d\n", rc);
			goto ERROR;
		} else {
			pdata->tx_pin = val ?
				LM3646_TX_PIN_ENABLED : LM3646_TX_PIN_DISABLED;
		}
	}

	if (of_property_read_bool(np, "lm3646,torch-pin-enable") == true) {
		rc = of_property_read_u32(np, "lm3646,torch-pin-enable", &val);
		if (rc < 0) {
			pr_err("read lm3646,torch-pin-enable failed rc %d\n", rc);
			goto ERROR;
		} else {
			pdata->torch_pin = val ?
				LM3646_TORCH_PIN_ENABLED : LM3646_TORCH_PIN_DISABLED;
		}
	}

	if (of_property_read_bool(np, "lm3646,strobe-pin-enable") == true) {
		rc = of_property_read_u32(np, "lm3646,strobe-pin-enable", &val);
		if (rc < 0) {
			pr_err("read lm3646,strobe-pin-enable failed rc %d\n", rc);
			goto ERROR;
		} else {
			pdata->strobe_pin = val ?
				LM3646_STROBE_PIN_ENABLED : LM3646_STROBE_PIN_DISABLED;
		}
	}
#if 0
	gpio = of_get_named_gpio_flags(np,
			"lm3646,gpio-enable", 0, NULL);
	if(gpio < 0) {

#else
	rc = of_property_read_u32(np,
			"lm3646,gpio-enable",&gpio);
	if (rc < 0) {
		pr_err("read lm3646,gpio-enable failed rc %d\n", rc);
		goto ERROR;
	}
#endif
	if(gpio < 0) {
		pr_err("read lm3646,gpio-enable failed\n");
		return gpio;
	}

	/* Flash source */
	flash_src_node = of_find_node_by_name(np,"lm3646,flash");
	if (!flash_src_node) {
		pr_err("flash_src_node(flash-source) NULL\n");
	}

	if(flash_src_node) {
		rc = of_property_read_u32(flash_src_node,
			"lm3646,flash-max-current",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,flash-max-current failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->flash_imax = 0x0F & val;

		rc = of_property_read_u32(flash_src_node,
			"lm3646,flash-op-current",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,flash-op-current failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->led1_flash_imax = 0xFF & val;

/*
		rc = of_property_read_u32(flash_src_node,
			"lm3646,flash-ramp-time",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,flash-ramp-time failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->flash_timing = 0x0;
		pdata->flash_timing |= (u8)((0x0F & val) << 3);
*/
		rc = of_property_read_u32(flash_src_node,
			"lm3646,flash-time-out",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,flash-time-out failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->flash_timing = (u8)(0x0F & val);

		of_node_put(flash_src_node);
	}


	/* Torch source */
	flash_src_node = of_find_node_by_name(np,"lm3646,torch");
	if (!flash_src_node) {
		pr_err("flash_src_node(torch-source) NULL\n");
	}

	if(flash_src_node) {
		rc = of_property_read_u32(flash_src_node,
			"lm3646,torch-max-current",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,torch-max-current failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->torch_imax = 0x0F & val;

		rc = of_property_read_u32(flash_src_node,
			"lm3646,torch-op-current",
			&val);
		if (rc < 0) {
			pr_err("read lm3646,torch-op-current failed rc %d\n", rc);
			of_node_put(flash_src_node);
			goto ERROR;
		}

		pdata->led1_torch_imax = 0xFF & val;

		of_node_put(flash_src_node);
	}

	CDBG("tx_pin = %d, strobe_pin = %d, torch_pin = %d,	"
			"flash_max_cur = 0x%2x, flash_op_cur = 0x%2x, flash_timing = 0x%2x, "
			"torch_max_cur = 0x%2x, torch_op_cur = 0x%2x, "
			"gpio-enable = %d\n",
			pdata->tx_pin, pdata->strobe_pin, pdata->torch_pin,
			pdata->flash_imax, pdata->led1_flash_imax, pdata->flash_timing,
			pdata->torch_imax, pdata->led1_torch_imax,
			gpio);

	return 0;
ERROR:
	return rc;
}
#endif

/* module initialize */
static int lm3646_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3646_platform_data *pdata = NULL;
	struct lm3646 *pchip = NULL;

	int err = 0;

	CDBG("E\n");

#if defined(CONFIG_MACH_LGE) || defined (CONFIG_LEDS_LM3646)
	if(client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct lm3646_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("Not enough memory!!\n");
			return -ENOMEM;
		}
		err = lm3646_get_dt_data(&client->dev, pdata);
		if (err != 0) {
			pr_err("lm3646_get_dt_data error!!\n");
			return err;
		}
	} else {
		pdata = client->dev.platform_data;
	}
#endif

	if(lm3646_gpio_enable()) {
		dev_err(&client->dev, "failed to enable gpio.\n");
		return -EFAULT;
	}

	/* i2c check */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3646), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &client->dev;
	pchip->regmap = devm_regmap_init_i2c(client, &lm3646_regmap);
	if (IS_ERR(pchip->regmap)) {
		err = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			err);
		return err;
	}
	mutex_init(&pchip->lock);
	i2c_set_clientdata(client, pchip);

	/* platform data check */
	err = lm3646_chip_init(pchip, pdata);
	if (err < 0)
		goto err_out;

	/* flash brightness control */
	INIT_WORK(&pchip->work_flash, lm3646_deferred_flash_brightness_set);
	pchip->cdev_flash.name = "lm3646:flash";
	pchip->cdev_flash.max_brightness = 0x7F;
	pchip->cdev_flash.brightness = 0;
	pchip->cdev_flash.brightness_set = lm3646_flash_brightness_set;
	pchip->cdev_flash.brightness_set2 = lm3646_flash_brightness_set2;
	pchip->cdev_flash.default_trigger = "lm3646:flash";

	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_flash);
	if (err < 0)
		goto err_out;
	/* flash on control */
	err = device_create_file(pchip->cdev_flash.dev,
				 &dev_attr_ctrl[DFILE_FLASH_CTRL]);
	if (err < 0)
		goto err_create_flash_ctrl_file;
	/* flash duration control */
	err = device_create_file(pchip->cdev_flash.dev,
				 &dev_attr_ctrl[DFILE_FLASH_DUR]);
	if (err < 0)
		goto err_create_flash_duration_file;
	/* flash - dual led control */
	err = device_create_file(pchip->cdev_flash.dev,
				 &dev_attr_ctrl[DFILE_FLASH_LED1]);
	if (err < 0)
		goto err_create_flash_iled1_file;

	/* flash - warmness input */
	err = device_create_file(pchip->cdev_flash.dev,
				 &dev_attr_ctrl[DFILE_WARM_FLASH]);
	if (err < 0)
		goto err_create_flash_warmness_file;

	/* torch brightness control */
	INIT_WORK(&pchip->work_torch, lm3646_deferred_torch_brightness_set);
	pchip->cdev_torch.name = "lm3646:torch";
	pchip->cdev_torch.max_brightness = 0x7F;
	pchip->cdev_torch.brightness = 0;
	pchip->cdev_torch.brightness_set = lm3646_torch_brightness_set;
	pchip->cdev_torch.brightness_set2 = lm3646_torch_brightness_set2;
	pchip->cdev_torch.default_trigger = "lm3646:torch";

	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_torch);
	if (err < 0)
		goto err_create_torch_file;
	/* torch on/off control */
	err = device_create_file(pchip->cdev_torch.dev,
				 &dev_attr_ctrl[DFILE_TORCH_CTRL]);
	if (err < 0)
		goto err_create_torch_ctrl_file;
	/* torch - dual led control */
	err = device_create_file(pchip->cdev_torch.dev,
				 &dev_attr_ctrl[DFILE_TORCH_LED1]);
	if (err < 0)
		goto err_create_torch_iled1_file;

	if(lm3646_gpio_disable())
		dev_err(&client->dev, "failed to disable gpio.\n");

#if defined(LM3646_FLASH_DRIVER_DEBUG)
	lm3646_reg_dump(pchip);
#endif

	dev_err(&client->dev, "%s probe succeed\n", __func__);

	return 0;

err_create_torch_iled1_file:
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_TORCH_CTRL]);
err_create_torch_ctrl_file:
	led_classdev_unregister(&pchip->cdev_torch);
err_create_torch_file:
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_WARM_FLASH]);
err_create_flash_warmness_file:
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_FLASH_LED1]);
err_create_flash_iled1_file:
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_FLASH_DUR]);
err_create_flash_duration_file:
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_FLASH_CTRL]);
err_create_flash_ctrl_file:
	led_classdev_unregister(&pchip->cdev_flash);
err_out:
	dev_err(&client->dev, "%s probe failed\n", __func__);
	if(lm3646_gpio_disable())
		dev_err(&client->dev, "failed to disable gpio.\n");

	return err;
}

static int lm3646_remove(struct i2c_client *client)
{
	struct lm3646 *pchip = i2c_get_clientdata(client);
	/* set standby mode */
	lm3646_update_byte(pchip, REG_MODE, 0x03, MODE_STDBY);
	/* flash */
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_FLASH_LED1]);
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_FLASH_DUR]);
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_TORCH_CTRL]);
	device_remove_file(pchip->cdev_flash.dev,
			   &dev_attr_ctrl[DFILE_WARM_FLASH]);
	led_classdev_unregister(&pchip->cdev_flash);
	/* torch */
	device_remove_file(pchip->cdev_torch.dev,
			   &dev_attr_ctrl[DFILE_TORCH_LED1]);
	device_remove_file(pchip->cdev_torch.dev,
			   &dev_attr_ctrl[DFILE_TORCH_CTRL]);
	led_classdev_unregister(&pchip->cdev_torch);

	return 0;
}

static const struct i2c_device_id lm3646_id[] = {
	{LM3646_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3646_id);

static struct of_device_id lm3646_match_table[] = {
	{ .compatible = "lm3646,camera-led-flash-driver",},
	{ },
};

static struct i2c_driver lm3646_i2c_driver = {
	.driver = {
		.name = LM3646_NAME,
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = lm3646_match_table,
	},
	.probe = lm3646_probe,
	.remove = lm3646_remove,
	.id_table = lm3646_id,
};

module_i2c_driver(lm3646_i2c_driver);
/*
static int __init lm3646_mod_init(void)
{
	return i2c_add_driver(&lm3646_i2c_driver);
}

static void __exit lm3646_mod_exit(void)
{
	i2c_del_driver(&lm3646_i2c_driver);
}

//late_initcall(imx135_mod_init);
late_initcall_sync(lm3646_mod_init);
module_exit(lm3646_mod_exit);
*/

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3646");
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>");
MODULE_LICENSE("GPL v2");
