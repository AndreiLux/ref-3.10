/*
 * linux/drivers/video/odin/dss/kcalctrl.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 * Some code and ideas taken from drivers/video/omap2/ driver
 * by Imre Deak.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/mm.h>

#include <video/odindss.h>

#include "dss.h"
#include "kcalctrl.h"

int g_kcal_r = 255;
int g_kcal_g = 255;
int g_kcal_b = 255;
int g_kcal_ctrl_mode = 0;

static struct kcal_platform_data kcal_dev_pdata;

static int kcal_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	g_kcal_r = kcal_r;
	g_kcal_g = kcal_g;
	g_kcal_b = kcal_b;

	return 0;
}

static int kcal_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = g_kcal_r;
	*kcal_g = g_kcal_g;
	*kcal_b = g_kcal_b;

	return 0;
}

static int kcal_refresh_values(void)
{
	return update_screen_dipc_lut();	
}


static struct kcal_platform_data kcal_dev_pdata = {
	.set_values = kcal_set_values,
	.get_values = kcal_get_values,
	.refresh_display = kcal_refresh_values,
}; 

static struct platform_device kcal_platform_device = {
	.name = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_dev_pdata,
	}
};

static int kcal_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;
	
	rc = kcal_ctrl_create_file(pdev);

	if(rc != 0){
		return -1;
	}
	return 0;
}

static struct platform_driver this_driver = {
	.probe 	= kcal_ctrl_probe,
	.driver	= {
		.name	= "kcal_ctrl",
	},
};

int __init kcal_ctrl_init(void)
{
	if ( platform_device_register(&kcal_platform_device) ){
		KCALERR("%s failed to register kcalctrl device\n", __func__);	
		return -ENODEV;
	}


	if ( platform_driver_register(&this_driver) ){
		KCALERR("%s failed to register kcalctrl driver\n", __func__);	
		return -ENODEV;
	}

	return 0;
}

device_initcall(kcal_ctrl_init);

