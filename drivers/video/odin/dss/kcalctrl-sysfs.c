/*
 * linux/drivers/video/odin/dss/kcalctrl-sysfs.c
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

#include "kcalctrl.h"

static struct kcal_platform_data *kcal_pdata;
static int last_status_kcal_ctrl;

static ssize_t kcal_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int kcal_r = 0;
	int kcal_g = 0;
	int kcal_b = 0;

	if(!count)
		return -EINVAL;

	sscanf(buf, "%d %d %d", &kcal_r, &kcal_g, &kcal_b);
	kcal_pdata->set_values(kcal_r, kcal_g, kcal_b);
	return count;
}

static ssize_t kcal_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	int kcal_r = 0;
	int kcal_g = 0;
	int kcal_b = 0;

	kcal_pdata->get_values(&kcal_r, &kcal_g, &kcal_b);
	return sprintf(buf, "%d %d %d\n", kcal_r, kcal_g, kcal_b);
}


static ssize_t kcal_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = 0;

	if(!count)
		return last_status_kcal_ctrl = -EINVAL;

	sscanf(buf, "%d", &cmd);

	if(cmd != 1)
		return last_status_kcal_ctrl = -EINVAL;
	
	last_status_kcal_ctrl = kcal_pdata->refresh_display();

	if(last_status_kcal_ctrl){
		return -EINVAL;
	}else{
		return count;
	}

}


static ssize_t kcal_ctrl_show(struct device *dev,
		struct device_attribute *attr, const char *buf)
{
	if(last_status_kcal_ctrl){
		return sprintf(buf, "NG\n");
	}
	else{
		return sprintf(buf, "OK\n");
	}
}

static struct device_attribute kcal_ctrl_attrs[] = {
	__ATTR(kcal, S_IWUSR | S_IRUGO, kcal_show, kcal_store),
	__ATTR(kcal_ctrl, S_IWUSR | S_IRUGO, kcal_ctrl_show, kcal_ctrl_store),
};

int kcal_ctrl_create_file(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	kcal_pdata = pdev->dev.platform_data;

	if(!kcal_pdata->set_values || !kcal_pdata->get_values ||
			!kcal_pdata->refresh_display) {
			return -1;
	}

	for(i = 0; i < ARRAY_SIZE(kcal_ctrl_attrs); i++){
		rc = device_create_file(&pdev->dev, &kcal_ctrl_attrs[i]);
	}

	if(rc != 0){
		return -1;
	}
	return 0;
}



