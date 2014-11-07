/*
 * LGHDK HD  DSI Videom Mode Panel Driver
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/mutex.h>

#include <linux/odin_mailbox.h>

#include "lcd-slimport-sample.h"

#define LCD_RESET_N 2
#define DSV_EN		29

/* MIPI COMMANDS */
char interface_setting[7] = { 0xB3, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef	MIPI_300MHZ_MODE
char dsi_ctrl[3] = { 0xB6, 0x3A, 0xA3 };
#else	/* MIPI_300MHZ_MODE */
char dsi_ctrl[3] = { 0xB6, 0x3A, 0xD3 };
#endif	/* !MIPI_300MHZ_MODE */

char display_setting_1[35] = {
	0xC1,
	0x84, 0x60, 0x50, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0C,
	0x01, 0x58, 0x73, 0xAE, 0x31,
	0x20, 0x06, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x10,
	0x10, 0x10, 0x00, 0x00, 0x00,
	0x22, 0x02, 0x02, 0x00
};

char vsync_setting[4] = { 0xC3, 0x00, 0x00, 0x00 };

char display_setting_2[8] = {
	0xC2,
	0x32, 0xF7, 0x80, 0x0A, 0x08,
	0x00, 0x00
};

char source_timing_setting[23] = {
	0xC4,
	0x70, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x11,
	0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x00, 0x00,
	0x11, 0x06
};

char ltps_timing_setting[41] = {
	0xC6,
	0x06, 0x6D, 0x06, 0x6D, 0x06,
	0x6D, 0x00, 0x00, 0x00, 0x00,
	0x06, 0x6D, 0x06, 0x6D, 0x06,
	0x6D, 0x15, 0x19, 0x07, 0x00,
	0x01, 0x06, 0x6D, 0x06, 0x6D,
	0x06, 0x6D, 0x00, 0x00, 0x00,
	0x00, 0x06, 0x6D, 0x06, 0x6D,
	0x06, 0x6D, 0x15, 0x19, 0x07
};

char gamma_setting_a[25] = {
	0xC7,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

char gamma_setting_b[25] = {
	0xC8,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

char gamma_setting_c[25] = {
	0xC9,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

char pwr_setting_chg_pump[15] = {
	0xD0,
	0x00, 0x00, 0x19, 0x18, 0x99,
	0x99, 0x19, 0x01, 0x89, 0x00,
	0x55, 0x19, 0x99, 0x01
};

char pwr_setting_internal_pwr[27] = {
	0xD3,
	0x1B, 0x33, 0xBB, 0xCC, 0xC4,
	0x33, 0x33, 0x33, 0x00, 0x01,
	0x00, 0xA0, 0xD8, 0xA0, 0x0D,
	0x39, 0x33, 0x44, 0x22, 0x70,
	0x02, 0x39, 0x03, 0x3D, 0xBF,
	0x00
};

char vcom_setting[8] = {
	0xD5,
	0x06, 0x00, 0x00, 0x01, 0x2C,
	0x01, 0x2C
};

char color_enhancement[33] = {
	0xCA,
	0x01, 0x70, 0xB0, 0xFF, 0xB0,
	0xB0, 0x98, 0x78, 0x3F, 0x3F,
	0x80, 0x80, 0x08, 0x38, 0x08,
	0x3F, 0x08, 0x90, 0x0C, 0x0C,
	0x0A, 0x06, 0x04, 0x04, 0x00,
	0xC0, 0x10, 0x10, 0x3F, 0x3F,
	0x3F, 0x3F
};

char auto_contrast[7] = {
	0xD8,
	0x00, 0x80, 0x80, 0x40, 0x42,
	0x55
};

char sharpening_control[3] = { 0xDD, 0x01, 0x95 };



struct odin_dsi_cmd slimport_dsi_flip_command[] = {
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_1_PARA, 0x36, 0x00),
};

struct odin_dsi_cmd slimport_dsi_off_command[] = {
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x28, 0x00),
	DSI_DLY_MS(17*3),
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x10, 0x00),
	DSI_DLY_MS(17*9),
};


struct odin_dsi_cmd slimport_dsi_init_cmd[] = {
	/* Display Initial Set*/
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x11, 0x00),
	DSI_DLY_MS(100),
	/*  mcap_setting*/
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xB0, 0x04),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, interface_setting),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, dsi_ctrl),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_1),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_2),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vsync_setting),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, source_timing_setting),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, ltps_timing_setting),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_a),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_b),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_c),

	/* panel_interface_ctrl */
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xcc, 0x09),

	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting_chg_pump),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting_internal_pwr),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),

#if defined(CONFIG_LGE_R63311_COLOR_ENGINE)
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, color_enhancement),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, auto_contrast),
	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, sharpening_control),
#endif

	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x29, 0x00),
	DSI_DLY_MS(17*3),
};

static DECLARE_COMPLETION(dss_ipc_completion);

static int slimport_power_on(void)
{
	int ret;

	ret = gpio_direction_output(DSV_EN, 1);
	if(ret < 0)
	{
		DSSERR("gpio_direction_input failed for dsv_en gpiod %d\n", DSV_EN);
		return -1;
	}

	ret = gpio_direction_output(LCD_RESET_N, 1);
	if(ret < 0)
	{
		DSSERR("gpio_direction_input failed for lcd_reset gpiod %d\n", LCD_RESET_N);
		return -1;
	 }

	gpio_set_value(DSV_EN , 0);
	gpio_set_value(LCD_RESET_N , 1);

	mdelay(20);

	gpio_set_value(DSV_EN , 1);
	mdelay(6);

	gpio_set_value(LCD_RESET_N , 0);
	mdelay(5);

	gpio_set_value(LCD_RESET_N , 1);
	mdelay(5);

	return 0;
}

void slimport_power_off(void)
{
	gpio_set_value(LCD_RESET_N , 0);
	mdelay(2);

	gpio_set_value(DSV_EN , 0);
	mdelay(2);

}

extern int slimport_panel_on(enum odin_mipi_dsi_index resource_index,
			     struct odin_dss_device *dssdev)
{
	int ret;
	u8 flip_data = 0;

	ret = slimport_power_on();
	if (ret < 0) {
		DSSERR("slimport_power_on() failed\n");
		return ret;
	}

	odin_mipi_dsi_panel_config(resource_index, dssdev);

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);

	if ((dssdev->panel.flip & ODIN_DSS_H_FLIP) &&
	    (dssdev->panel.flip_supported & ODIN_DSS_H_FLIP))
		flip_data |= 0x40;

	if ((dssdev->panel.flip & ODIN_DSS_V_FLIP) &&
	    (dssdev->panel.flip_supported & ODIN_DSS_V_FLIP))
		flip_data |= 0x80;

	slimport_dsi_flip_command[0].sp_len_dly.sp.data1 = flip_data;

	if (flip_data)
		odin_mipi_dsi_cmd(resource_index, slimport_dsi_flip_command,
				  ARRAY_SIZE(slimport_dsi_flip_command));

	odin_mipi_dsi_cmd(resource_index, slimport_dsi_init_cmd,
			  ARRAY_SIZE(slimport_dsi_init_cmd));

	return 0;
}
EXPORT_SYMBOL(slimport_panel_on);

extern int slimport_panel_off(enum odin_mipi_dsi_index resource_index,
			      struct odin_dss_device *dssdev)
{
	odin_mipi_hs_video_mode_stop(resource_index, dssdev);

	odin_mipi_dsi_cmd(resource_index, slimport_dsi_off_command,
			  ARRAY_SIZE(slimport_dsi_off_command));

	slimport_power_off();

	return 0;
}
EXPORT_SYMBOL(slimport_panel_off);

extern void slimport_panel_config(struct odin_dss_device *dssdev)
{
	dssdev->panel.dsi_mdoe = panel_configs->dsi_mode;
	dssdev->panel.config = ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF |
			       ODIN_DSS_LCD_RF ;
	dssdev->panel.dsi_pix_fmt = ODIN_DSI_OUTFMT_RGB24;
	dssdev->panel.flip = ODIN_DSS_H_FLIP | ODIN_DSS_V_FLIP;
	dssdev->panel.flip_supported = ODIN_DSS_H_FLIP;
	dssdev->phy.dsi.lane_num = ODIN_DSI_4LANE;

	return;
}
EXPORT_SYMBOL(slimport_panel_config);

extern int slimport_panel_init(void)
{
	int ret = 0;
	ret = gpio_request(DSV_EN, "dsv_en");
	if(ret < 0)
	{
		DSSERR("gpio_request failed for dsv_en gpio %d\n", DSV_EN);
		return -1;
	}

	ret = gpio_request(LCD_RESET_N, "lcd_reset");
	if(ret < 0)
	{
		DSSERR("gpio_request failed for lcd_reset gpio %d\n", LCD_RESET_N);
		return -1;
	}

	return ret;
}
EXPORT_SYMBOL(slimport_panel_init);

MODULE_AUTHOR("Jongmyung Park <jongmyung.park@lge.com>");
MODULE_DESCRIPTION("SLIMPORT Dummy Driver");
MODULE_LICENSE("GPL");
