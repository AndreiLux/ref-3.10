/*
 * linux/drivers/video/odin/dss/mipi-dsi-set.c
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
#include "mipi-dsi-set.h"

#define NSC_MIN 5

int g_fout = 888;
int g_reg_PLL_NPC = 0, g_reg_PLL_NSC = 0, g_reg_PLL_M=0;
int g_mipi_dsi_active = 0;

static struct mipi_platform_data mipi_dev_pdata;

int convert_value(int value)
{
	int ret = 0;
	char c_value[20];
	char *str;
	sprintf(c_value,"%x",value);
	MIPI_INFO("%ld convert to %s\n", value, c_value);
	ret = simple_strtol(c_value,&str,16);

	return ret;

}

static int mipi_set_values(int fout)
{
	int middle, i;
	int reg_PLL_NPC, reg_PLL_NSC;

	middle = fout / 8;

	if (fout % 8 != 0){
		MIPI_INFO("You must input MIPI DSI Clock 8 multiple \n");
		return 0;
	}
	g_fout = fout;

	for (i = NSC_MIN; i<=15; i++){
		reg_PLL_NSC = middle - i;

		if (reg_PLL_NSC %4  == 0) {
			reg_PLL_NPC = reg_PLL_NSC / 4;
			if (reg_PLL_NSC > reg_PLL_NPC){
				MIPI_INFO("MIPI DSI CLOCK reg_PLL_NPC :  %d \n", reg_PLL_NPC);
				MIPI_INFO("MIPI DSI CLOCK reg_PLL_NSC :  %d \n\n", i);
				g_reg_PLL_NPC = convert_value(reg_PLL_NPC);
				g_reg_PLL_NSC = convert_value(i);
				//g_reg_PLL_NPC = reg_PLL_NPC;
				//g_reg_PLL_NSC = i;
				break ;
			}
		}
	}

	return 0;
}

static int mipi_get_values(int *fout, int *reg_PLL_NPC, int *reg_PLL_NSC)
{
	*fout = g_fout;
	*reg_PLL_NPC = g_reg_PLL_NPC;
	*reg_PLL_NSC = g_reg_PLL_NSC;

	return 0;
}
/*
static int mipi_set_active(int input)
{
	g_mipi_dsi_active = input;
	return g_mipi_dsi_active;
}
*/

static int mipi_set_active(int input)
{
	if (input == 0){
		MIPI_INFO("MIPI DSI Set Active : No \n");
		g_mipi_dsi_active = 0;
		}
	else if (input == 1){
		MIPI_INFO("MIPI DSI Set Active : Yes \n");
		g_mipi_dsi_active = 1;
	}
	else if (input == 2){
		MIPI_INFO("MIPI DSI Show Active \n");
		mipi_dsi_d_phy_read();
	}
	else
		MIPI_INFO("MIPI DSI Set Active : Wrong Value \n");

	return 0;
}

static int mipi_get_active(int *input)
{
	*input = g_mipi_dsi_active;
	return 0;
}



static struct mipi_platform_data mipi_dev_pdata = {
	.set_mipi_values = mipi_set_values,
	.get_mipi_values = mipi_get_values,
	.set_mipi_dsi_active = mipi_set_active,
	.get_mipi_dsi_active = mipi_get_active,


};

static struct platform_device mipi_platform_device = {
	.name = "mipi_set_command",
	.dev = {
		.platform_data = &mipi_dev_pdata,
	}
};

static int mipi_set_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = mipi_dsi_set_create_file(pdev);

	if(rc != 0){
		return -1;
	}
	return 0;
}

static struct platform_driver this_driver = {
	.probe 	= mipi_set_probe,
	.driver	= {
		.name	= "mipi_set_command",
	},
};

int __init mipi_set_init(void)
{
	if ( platform_device_register(&mipi_platform_device) ){
		MIPI_ERR("%s failed to register mipi-dsi-set device\n", __func__);
		return -ENODEV;
	}


	if ( platform_driver_register(&this_driver) ){
		MIPI_ERR("%s failed to register mipi-dsi-set driver\n", __func__);
		return -ENODEV;
	}

	return 0;
}

device_initcall(mipi_set_init);

