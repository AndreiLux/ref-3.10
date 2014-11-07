/*
 * linux/drivers/video/odin/dss/mipi-dsi-set-sysfs.c
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

#include "mipi-dsi-set.h"

static struct mipi_platform_data *mipi_dev_pdata;
static int last_status_mipi_dsi_active;



static ssize_t mipi_dsi_speed_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int fout = 0;

	if(!count)
		return -EINVAL;

	sscanf(buf, "%d", &fout);
	mipi_dev_pdata->set_mipi_values(fout);
	return count;
}

static ssize_t mipi_dsi_speed_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	int fout = 0;
	int reg_PLL_NPC = 0;
	int reg_PLL_NSC = 0;

	mipi_dev_pdata->get_mipi_values(&fout, &reg_PLL_NPC, &reg_PLL_NSC);
	return sprintf(buf, "MIPI DSI Clock Speed %d, NPC : %d, NSC : %d \n", fout,reg_PLL_NPC, reg_PLL_NSC );
}

static ssize_t mipi_dsi_active_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input = 0;

	if(!count)
		return -EINVAL;

	sscanf(buf, "%d", &input);
	mipi_dev_pdata->set_mipi_dsi_active(input);
	return count;
/*
	int cmd = 0;
//	int input = 1;
	int input = 0;


	if(!count)
		return last_status_mipi_dsi_active = -EINVAL;

	sscanf(buf, "%d", &cmd);

//	if(cmd != 1)
//		return last_status_mipi_dsi_active = -EINVAL;

	last_status_mipi_dsi_active = mipi_dev_pdata->mipi_dsi_active(cmd);

	if(last_status_mipi_dsi_active){
		return -EINVAL;
	}else{
		return count;
	}
*/

}


static ssize_t mipi_dsi_active_show(struct device *dev,
		struct device_attribute *attr, const char *buf)
{
	int mipi_active = 0;
	mipi_dev_pdata->get_mipi_dsi_active(&mipi_active);
	if(mipi_active == 0){
		MIPI_INFO("MIPI DSI COMMAND NO ACTIVE  \n");
		return sprintf(buf, "No Active\n");
	}
	else if (mipi_active == 1){
		MIPI_INFO("MIPI DSI COMMAND ACTIVE  \n");
		return sprintf(buf, "Active \n");
	}
	else {
		MIPI_INFO("MIPI DSI ACTIVE Value : %d \n",mipi_active);
		return sprintf(buf, "Problem \n");
	}

	/*
	if(last_status_mipi_dsi_active == 0){
		MIPI_INFO("MIPI DSI COMMAND NO ACTIVE  \n");
		return sprintf(buf, "No Active\n");
	}
	else if (last_status_mipi_dsi_active == 1){
		MIPI_INFO("MIPI DSI COMMAND ACTIVE  \n");
		return sprintf(buf, "Active \n");
	}
	else {
		MIPI_INFO("MIPI DSI ACTIVE Value : %d \n",last_status_mipi_dsi_active);
		return sprintf(buf, "Problem \n");
	}*/
}


static struct device_attribute mipi_set_attrs[] = {
	__ATTR(mipi_dsi_clock, S_IWUSR | S_IRUGO, mipi_dsi_speed_show, mipi_dsi_speed_store),
	__ATTR(mipi_dsi_clock_active, S_IWUSR | S_IRUGO, mipi_dsi_active_show, mipi_dsi_active_store),
};

int mipi_dsi_set_create_file(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	mipi_dev_pdata = pdev->dev.platform_data;

	if(!mipi_dev_pdata->set_mipi_values || !mipi_dev_pdata->get_mipi_values) {
			return -1;
	}

	for(i = 0; i < ARRAY_SIZE(mipi_set_attrs); i++){
		rc = device_create_file(&pdev->dev, &mipi_set_attrs[i]);
	}

	if(rc != 0){
		return -1;
	}
	return 0;
}



