/* drivers/video/decon_display/s6tnmr7_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>

#include <video/mipi_display.h>
#include "decon_mipi_dsi.h"
#include "decon_display_driver.h"

#if defined(CONFIG_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3

struct lcd_info {
	unsigned int			bl;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;

	struct device			*dev;
	struct lcd_device		*ld;
	unsigned char			id[LDI_ID_LEN];
	unsigned int			connected;

	struct mipi_dsim_device		*dsim;
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
};

static const unsigned char SEQ_VIDEO_STREAM[] = {
	0x2C,
};

int s6tnmr7_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret = 0;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	retry = 2;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto write_err;
	}

	if (len > 2)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_LONG_WRITE, (u32)seq, len);
	else if (len == 2)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_SHORT_WRITE_PARAM, seq[0], seq[1]);
	else if (len == 1)
		ret = s5p_mipi_dsi_wr_data(lcd->dsim,
					MIPI_DSI_DCS_SHORT_WRITE, seq[0], 0);
	else {
		ret = -EINVAL;
		goto write_err;
	}

	if (ret != 0) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

int s6tnmr7_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;

read_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf);
	if (ret != len) {
		dev_err(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}

read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

/* static void s6tnmr7_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6tnmr7_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
} */

static int s6tnmr7_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

	return ret;
}

static int s6tnmr7_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6tnmr7_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	dev_info(&lcd->ld->dev, "DISPLAY_ON\n");

	msleep(30);

	s6tnmr7_write(lcd, SEQ_VIDEO_STREAM, ARRAY_SIZE(SEQ_VIDEO_STREAM));

	return ret;
}

static int s6tnmr7_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	s6tnmr7_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dev_info(&lcd->ld->dev, "DISPLAY_OFF\n");

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}


static int s6tnmr7_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6tnmr7_ldi_init(lcd);

	ret = s6tnmr7_ldi_enable(lcd);

	lcd->ldi_enable = 1;

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6tnmr7_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = s6tnmr7_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6tnmr7_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6tnmr7_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6tnmr7_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6tnmr7_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6tnmr7_power(lcd, power);
}

static int s6tnmr7_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6tnmr7_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops s6tnmr7_lcd_ops = {
	.set_power = s6tnmr7_set_power,
	.get_power = s6tnmr7_get_power,
	.check_fb  = s6tnmr7_check_fb,
};

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "SDC_???????\n";

	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static int s6tnmr7_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6tnmr7_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
#ifdef CONFIG_S5P_LCD_INIT
	lcd->power = FB_BLANK_POWERDOWN;
#else
	lcd->power = FB_BLANK_UNBLANK;
#endif
	lcd->connected = 1;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);

	/* s6tnmr7_read_id(lcd, lcd->id); */

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	lcd->ldi_enable = 1;

#if defined(CONFIG_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)s6tnmr7_write, (mdnie_r)s6tnmr7_read);
#endif

	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", __FILE__);

	return 0;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}

static int s6tnmr7_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6tnmr7_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6tnmr7_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6tnmr7_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6tnmr7_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6tnmr7_mipi_lcd_driver = {
	.probe		= s6tnmr7_probe,
	.displayon	= s6tnmr7_displayon,
	.suspend	= s6tnmr7_suspend,
	.resume		= s6tnmr7_resume,
};

static int s6tnmr7_init(void)
{
	return 0;
}

static void s6tnmr7_exit(void)
{
	return;
}

module_init(s6tnmr7_init);
module_exit(s6tnmr7_exit);

MODULE_DESCRIPTION("MIPI-DSI S6TNMR7 (2560*1600) Panel Driver");
MODULE_LICENSE("GPL");

