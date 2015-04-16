/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#include "k3_fb.h"


#define MAX_BACKLIGHT_BRIGHTNESS	(255)

#define K3_FB_DEFAULT_BL_LEVEL	(102)

static int lcd_backlight_registered;
static int is_recovery_mode;

#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
unsigned long backlight_duration = (3 * HZ / 60);
#endif


static void k3fb_set_backlight(struct k3_fb_data_type *k3fd, u32 bkl_lvl)
{
	struct k3_fb_panel_data *pdata = NULL;
	u32 temp = bkl_lvl;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->panel_power_on || !k3fd->backlight.bl_updated) {
		k3fd->bl_level = bkl_lvl;
		return;
	}

	if (pdata->set_backlight) {
		if (k3fd->backlight.bl_level_old == temp) {
			k3fd->bl_level = bkl_lvl;
			return;
		}
		k3fd->bl_level = bkl_lvl;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI)
			k3fb_activate_vsync(k3fd);
		pdata->set_backlight(k3fd->pdev);
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI)
			k3fb_deactivate_vsync(k3fd);
		k3fd->backlight.bl_level_old = temp;
	}
}

#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
static void k3fb_bl_workqueue_handler(struct work_struct *work)
#else
static void k3fb_bl_workqueue_handler(struct k3_fb_data_type *k3fd)
#endif
{
	struct k3_fb_panel_data *pdata = NULL;
#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
	struct k3fb_backlight *pbacklight = NULL;
	struct k3_fb_data_type *k3fd = NULL;

	pbacklight = container_of(to_delayed_work(work), struct k3fb_backlight, bl_worker);
	BUG_ON(pbacklight == NULL);

	k3fd = container_of(pbacklight, struct k3_fb_data_type, backlight);
#endif

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->backlight.bl_updated) {
		down(&k3fd->backlight.bl_sem);
		k3fd->backlight.bl_updated = 1;
		if (is_recovery_mode) {
			k3fd->bl_level = K3_FB_DEFAULT_BL_LEVEL;
		}
		k3fb_set_backlight(k3fd, k3fd->bl_level);
		up(&k3fd->backlight.bl_sem);
	}
}

void k3fb_backlight_update(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	if (!k3fd->backlight.bl_updated) {
	#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
		schedule_delayed_work(&k3fd->backlight.bl_worker,
			backlight_duration);
	#else
		k3fb_bl_workqueue_handler(k3fd);
	#endif
	}
}

void k3fb_backlight_cancel(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
	cancel_delayed_work_sync(&k3fd->backlight.bl_worker);
#endif
	k3fd->backlight.bl_updated = 0;
	k3fd->backlight.bl_level_old = 0;

	if (pdata->set_backlight) {
		down(&k3fd->backlight.bl_sem);
		k3fd->bl_level = 0;
		pdata->set_backlight(k3fd->pdev);
		up(&k3fd->backlight.bl_sem);
	}
}

#ifdef CONFIG_FB_BACKLIGHT
static int k3_fb_bl_get_brightness(struct backlight_device *pbd)
{
	return pbd->props.brightness;
}

static int k3_fb_bl_update_status(struct backlight_device *pbd)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 bl_lvl = 0;

	BUG_ON(pbd == NULL);
	k3fd = bl_get_data(pbd);
	BUG_ON(k3fd == NULL);

	bl_lvl = pbd->props.brightness;
	bl_lvl = k3fd->fbi->bl_curve[bl_lvl];

	down(&k3fd->backlight.bl_sem);
	k3fb_set_backlight(k3fd, bl_lvl);
	up(&k3fd->backlight.bl_sem);

	return 0;
}

static struct backlight_ops k3_fb_bl_ops = {
	.get_brightness = k3_fb_bl_get_brightness,
	.update_status = k3_fb_bl_update_status,
};
#else
static void k3_fb_set_bl_brightness(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct k3_fb_data_type *k3fd = NULL;
	int bl_lvl = 0;

	BUG_ON(led_cdev == NULL);
	k3fd = dev_get_drvdata(led_cdev->dev->parent);
	BUG_ON(k3fd == NULL);

	if (value < 0)
		value = 0;

	if (value > MAX_BACKLIGHT_BRIGHTNESS)
		value = MAX_BACKLIGHT_BRIGHTNESS;

	/* This maps android backlight level 0 to 255 into
	   driver backlight level 0 to bl_max with rounding */
	bl_lvl = (2 * value * k3fd->panel_info.bl_max + MAX_BACKLIGHT_BRIGHTNESS)
		/(2 * MAX_BACKLIGHT_BRIGHTNESS);

	if (!bl_lvl && value)
		bl_lvl = 1;

	down(&k3fd->backlight.bl_sem);
	k3fb_set_backlight(k3fd, bl_lvl);
	up(&k3fd->backlight.bl_sem);
}

static struct led_classdev backlight_led = {
	.name = DEV_NAME_LCD_BKL,
	.brightness = MAX_BACKLIGHT_BRIGHTNESS,
	.brightness_set = k3_fb_set_bl_brightness,
};
#endif

void k3fb_backlight_register(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
#ifdef CONFIG_FB_BACKLIGHT
	struct backlight_device *pbd = NULL;
	struct fb_info *fbi = NULL;
	char name[16] = {0};
	struct backlight_properties props;
#endif

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	k3fd->backlight.bl_updated = 0;
	k3fd->backlight.bl_level_old = 0;
	sema_init(&k3fd->backlight.bl_sem, 1);
#ifdef CONFIG_K3_FB_BACKLIGHT_DELAY
	INIT_DELAYED_WORK(&k3fd->backlight.bl_worker, k3fb_bl_workqueue_handler);
#endif

	if (lcd_backlight_registered)
		return;

	is_recovery_mode = (int)get_boot_into_recovery_flag();

#ifdef CONFIG_FB_BACKLIGHT
	fbi = k3fd->fbi;

	snprintf(name, sizeof(name), "k3fb%d_bl", k3fd->index);
	props.max_brightness = FB_BACKLIGHT_LEVELS - 1;
	props.brightness = FB_BACKLIGHT_LEVELS - 1;
	pbd = backlight_device_register(name, fbi->dev, k3fd,
		&k3_fb_bl_ops, &props);
	if (IS_ERR(pbd)) {
		fbi->bl_dev = NULL;
		K3_FB_ERR("backlight_device_register failed!\n");
	}

	fbi->bl_dev = pbd;
	fb_bl_default_curve(fbi, 0,
		k3fd->panel_info.bl_min, k3fd->panel_info.bl_max);
#else
	/* android supports only one lcd-backlight/lcd for now */
	if (led_classdev_register(&pdev->dev, &backlight_led)) {
		K3_FB_ERR("led_classdev_register failed!\n");
		return;
	}
#endif

	lcd_backlight_registered = 1;
}

void k3fb_backlight_unregister(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (lcd_backlight_registered) {
		lcd_backlight_registered = 0;
	#ifdef CONFIG_FB_BACKLIGHT
		/* remove /sys/class/backlight */
		backlight_device_unregister(k3fd->fbi->bl_dev);
	#else
		led_classdev_unregister(&backlight_led);
	#endif
	}
}

void k3fb_sbl_isr_handler(struct k3_fb_data_type *k3fd)
{
	char __iomem *sbl_base = NULL;
	u32 bkl_from_AD = 0;

	BUG_ON(k3fd == NULL);

	if ((k3fd->sbl_enable == 0) || (k3fd->panel_info.sbl_support == 0)) {
		return;
	}

	sbl_base = k3fd->dss_base + DSS_DPP_SBL_OFFSET;
	bkl_from_AD = (inp32(sbl_base + SBL_BACKLIGHT_OUT_H) << 8)
		| inp32(sbl_base + SBL_BACKLIGHT_OUT_L);
	k3fb_set_backlight(k3fd, bkl_from_AD);
}
