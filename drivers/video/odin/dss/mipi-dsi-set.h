/*
 * linux/drivers/video/odin/dss/mipi-dsi-set.h
 *
 * Copyright (C) 2012 LG Electronics, Inc.
 * Author:
 *
 * Some code and ideas taken from drivers/video/odin/ driver
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

#define DEBUG

#define MIPI_ERR(format, ...) \
	printk(KERN_ERR " MIPI error =====> " format, ## __VA_ARGS__)
#define MIPI_WARN(format, ...) \
	printk(KERN_WARNING " MIPI warn =====> " format, ## __VAR_ARGS__)
#define MIPI_INFO(format, ...) \
	printk(KERN_INFO " MIPI info =====> " format, ## __VA_ARGS__)


struct mipi_platform_data {
	int (*set_mipi_values) (int fout);
	int (*get_mipi_values) (int *fout, int *reg_PLL_NPC, int *reg_PLL_NSC);
	int (*set_mipi_dsi_active) (int active);
	int (*get_mipi_dsi_active) (int *active);
};


extern int mipi_dsi_set_create_file(struct platform_device *pdev);

extern int g_fout;
extern int g_reg_PLL_M;
extern int g_reg_PLL_NPC;
extern int g_reg_PLL_NSC;
extern int g_mipi_dsi_active;
extern void mipi_dsi_d_phy_read(void);

