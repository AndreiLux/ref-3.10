/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 *  Author: Wonsuk Chang <wonsuk.chang@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/usb/odin_usb3.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <asm/system_info.h>

#ifdef CONFIG_CHARGER_MAX77819
#include <linux/mfd/max77819.h>
#endif
#ifdef CONFIG_CHARGER_MAX77807
#include <linux/mfd/max77807/max77807.h>
#endif
#ifdef CONFIG_MFD_D2260
#include <linux/mfd/d2260/core.h>
#endif

#ifdef CONFIG_MACH_ODIN_LIGER
#include <linux/board_lge.h>
#endif

static int otg_gpio = 0;

int odin_usb3_otg_en_gpio_init(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_MACH_ODIN_LIGER
	if(lge_get_board_revno() != HW_REV_I)
#else
	if ((system_rev & 0xFF) < 0x0D)
#endif
		return 0;

	if (of_property_read_u32(pdev->dev.of_node,
				"otg_en_gpio", &otg_gpio) != 0)
		return -EINVAL;
	usb_dbg("%s: get irq_gpio %d\n", __func__, otg_gpio);

	ret = gpio_is_valid(otg_gpio);
	if (!ret) {
		dev_err(&pdev->dev, "gpio %d is invalid\n", otg_gpio);
		return -EINVAL;
	}

	ret = devm_gpio_request(&pdev->dev, otg_gpio, "usb_otg_en");
	if (ret) {
		dev_err(&pdev->dev, "Request gpio %d fail : %d\n", otg_gpio, ret);
		return -EINVAL;
	}

	gpio_direction_output(otg_gpio, 0);
	return 0;
}

#ifdef CONFIG_MACH_ODIN_LIGER
static void odin_usb3_vbus_onoff(struct odin_usb3_otg *odin_otg, int onoff)
{
	usb_dbg("[%s] ==> %s \n", __func__, onoff? "ON":"OFF");

#ifdef CONFIG_CHARGER_MAX77819
	if(lge_get_board_revno() == HW_REV_I) {
		gpio_set_value(otg_gpio, onoff);
		max77819_otg_vbus_onoff(odin_otg, onoff);
	}
	else
#endif
	{
#ifdef CONFIG_CHARGER_MAX77807
		max77807_otg_vbus_onoff(odin_otg, onoff);
#endif
	}
}

static bool odin_usb3_vbus_init_detect(struct odin_usb3_otg *odin_otg)
{
	usb_dbg("[%s] \n", __func__);

#ifdef CONFIG_CHARGER_MAX77819
	if(lge_get_board_revno() == HW_REV_I) {
		max77819_drd_charger_init_detect(odin_otg);
		return false;
	}
	else
#endif
	{
#ifdef CONFIG_CHARGER_MAX77807
		return max77807_drd_charger_init_detect(odin_otg);
#endif
	}
}
#else /* CONFIG_MACH_ODIN_LIGER */
static void odin_usb3_vbus_onoff(struct odin_usb3_otg *odin_otg, int onoff)
{
	usb_dbg("[%s] ==> %s \n", __func__, onoff? "ON":"OFF");

	if ((system_rev & 0xFF) > 0x0C)
		gpio_set_value(otg_gpio, onoff);

#ifdef CONFIG_CHARGER_MAX77807
	if (((system_rev & 0xFF) > 0x12) || ((system_rev & 0xFF) == 0x0A))
		max77807_otg_vbus_onoff(odin_otg, onoff);
#endif

#ifdef CONFIG_CHARGER_MAX77819
	if (((system_rev & 0xFF) > 0x0C) && ((system_rev & 0xFF) < 0x13))
		max77819_otg_vbus_onoff(odin_otg, onoff);
#endif
}

static bool odin_usb3_vbus_init_detect(struct odin_usb3_otg *odin_otg)
{
	usb_dbg("[%s] \n", __func__);

#ifdef CONFIG_CHARGER_MAX77807
	if (((system_rev & 0xFF) > 0x12) || ((system_rev & 0xFF) == 0x0A))
		max77807_drd_charger_init_detect(odin_otg);
#endif

#ifdef CONFIG_CHARGER_MAX77819
	if (((system_rev & 0xFF) > 0x0C) && ((system_rev & 0xFF) < 0x13))
		max77819_drd_charger_init_detect(odin_otg);
#endif

	return false;
}
#endif /* CONFIG_MACH_ODIN_LIGER */

static void odin_usb3_id_init_detect(struct odin_usb3_otg *odin_otg)
{
	usb_dbg("[%s] \n", __func__);

#ifdef CONFIG_MFD_D2260
#ifndef CONFIG_MACH_ODIN_LIGER
	if (((system_rev & 0xFF) > 0x0F) || ((system_rev & 0xFF) == 0x0A))
#endif
		d2260_drd_id_init_detect(odin_otg);
#endif
}

struct odin_usb3_ic_ops usb3_ic_ops = {
	.vbus_det = odin_usb3_vbus_init_detect,
	.id_det = odin_usb3_id_init_detect,
	.otg_vbus = odin_usb3_vbus_onoff,
};
