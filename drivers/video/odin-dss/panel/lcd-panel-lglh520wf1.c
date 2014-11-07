/*
 * LGHDK HD  DSI Videom Mode Panel Driver
 *
 * modified from panel-taal.c
 * choongryeol.lee@lge.com
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
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
#include <linux/odin_pd.h>

#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/pm_qos.h>

#include "../hal/mipi_dsi_common.h"
#include "../mipi_dsi_drv.h"
#include <video/odin-dss/mipi_device.h>

#include <linux/odin_pmic.h>
#include <linux/vmalloc.h>

struct panel_desc
{
	char *name;
	unsigned int gpio_intr;
	unsigned int gpio_reset;
	unsigned int gpio_dsv_enable;
};
static struct panel_desc *desc = NULL;

struct mipi_dsi_desc panel_cfg = {
	.in_pixel_fmt = INPUT_RGB24,
	.dsi_op_mode = DSI_VIDEO_MODE,
	.out_pixel_fmt = OUTPUT_RGB24,
	.vm_fmt = NON_BURST_MODE_WITH_SYNC_PLUSES,
	.vm_timing = {
		.x_res      = 1080,
		.y_res      = 1920,
		.height     = 115,
		.width      = 65,
		/* mipi clk(864MHz) * 4lanes / 24bits (KHz)*/
		.pixel_clock = 144000,
		.data_width = 24,
		.bpp= 24,
		.hsw = 4, /* hsync_width */
		.hbp = 50,
		.hfp = 100,
		.vsw = 1,
		.vfp = 4,
		.vbp = 14, /* 4 + 0xA */
		.pclk_pol = 0,
		.vsync_pol = 1,
		.hsync_pol = 1,
	},
	.ecc = true,
	.checksum = true,
	.eotp = true,
	.num_of_lanes = 4,
};

static struct mipi_hal_interface *mipi_ops = NULL;

struct lglh520wf1_dsi_data {
	struct mutex lock;

	struct backlight_device *bldev;

	unsigned long	hw_guard_end;	/* next value of jiffies when we can
					 * issue the next sleep in/out command
					 */
	unsigned long	hw_guard_wait;	/* max guard time in jiffies */

	struct odin_dss_device *dssdev;

	bool enabled;
	u8 rotate;
	bool mirror;

	bool te_enabled;

	atomic_t do_update;
	struct {
		u16 x;
		u16 y;
		u16 w;
		u16 h;
	} update_region;
	int channel;

	struct delayed_work te_timeout_work;

	bool use_dsi_bl;

	bool cabc_broken;
	unsigned cabc_mode;

	bool intro_printed;

	struct workqueue_struct *workqueue;

	struct delayed_work esd_work;
	unsigned esd_interval;

	bool ulps_enabled;
	unsigned ulps_timeout;
	struct delayed_work ulps_work;

	struct panel_config *panel_config;
	struct pm_qos_request mem_qos;
};

static char mcap_setting[2] = {0xB0, 0x04};

// static char nop_command[2]  = {0x00, 0x00};

/* Ingerface Setting */
#ifdef MIPI_DSI_CMD_MODE
static char interface_setting[7] = {
	0xB3,
	0x04, 0x00, 0x00, 0x00, 0x00,
	0x00
};
#else
static char interface_setting[7] = {
	0xB3,
	0x14, 0x00, 0x00, 0x00, 0x00,
	0x00
};
#endif

/* DSI Control -300MHz */
#ifdef MIPI_300MHZ_MODE
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xA3 };
#else
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xD3 };
#endif
/* Display Setting1 -FWD */
static char display_setting_1[35] = {
	0xC1,
	0x84, 0x60, 0x10, 0xEB, 0xFF,
	0x6F, 0xCE, 0xFF, 0xFF, 0x17,
	0x12, 0x58, 0x73, 0xAE, 0x31,
	0x20, 0xC6, 0xFF, 0xFF, 0x1F,
	0xF3, 0xFF, 0x5F, 0x10, 0x10,
	0x10, 0x10, 0x00, 0x02, 0x01,
	0x22, 0x22, 0x00, 0x01
};
/* Display Setting2 */
static char display_setting_2[8] = {
	0xC2,
#if 0
	0x31, 0xF7, 0x80, 0x1F, 0x08,  /* 0xA+ 0x15 */
#else
	0x31, 0xF7, 0x80, 0x14, 0x08,  /* 0xA+ 0xA */
#endif
	0x00, 0x00
};
/* Touch Panel Sync Function -VSYNC Enable */
static char vsync_setting[4] = {
	0xC3,
	0x01, 0x00, 0x00
};
/* Source Timing Setting */
static char source_timing_setting[23] = {
	/*0xC4,
	  0x70, 0x00, 0x00, 0x00, 0x33,
	  0x04, 0x00, 0x33, 0x00, 0x0C,
	  0x06, 0x00, 0x00, 0x00, 0x00,
	  0x33, 0x04, 0x00, 0x33, 0x00,
	  0x0C, 0x06*/
	0xC4,
	0x70, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x0C,
	0x06, 0x00, 0x00, 0x00, 0x00,
	0x33, 0x04, 0x00, 0x33, 0x00,
	0x0C, 0x06
};
/* LTPS Timing Setting */
static char ltps_timing_setting[41] = {
	0xC6,
	0x77, 0x69, 0x00, 0x69, 0x00, 0x69, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x69,
	0x00, 0x69, 0x10, 0x19, 0x07, 0x00, 0x77,
	0x00, 0x69, 0x00, 0x69, 0x00, 0x69, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x69,
	0x00, 0x69, 0x10, 0x19, 0x07
};

#if 0
/* Panel Pin Control */
static char panel_pin_control[10] = {
	0xCB,
	0x31, 0xFC, 0x3F, 0x8C, 0x00,
	0x00, 0x00, 0x00, 0xC0
};
#endif

/* Gamma Setting Common Set */
static char gamma_setting_common_set[31] = {
	0xC7,
	0x01, 0x08, 0x10, 0x13, 0x1F,
	0x2D, 0x39, 0x4D, 0x36, 0x42,
	0x4C, 0x5C, 0x65, 0x69, 0x7C,
	0x01, 0x08, 0x10, 0x13, 0x1F,
	0x2D, 0x39, 0x4D, 0x36, 0x42,
	0x4C, 0x5C, 0x65, 0x69, 0x7C};
/* Digital Gamma Setting */
static char digital_gamma_setting[20] = {
	0xC8,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0xFC, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFC, 0x00
};
/* Power Segging */
static char pwr_setting[11] = {
	0xD0,
	0xCC, 0x81, 0xBB, 0x59, 0x59,
	0x4C, 0x19, 0x19, 0x04, 0x00
};
/* Power Segging for Internal Power */
static char pwr_setting_internal_pwr[26] = {
	0xD3,
	0x1B, 0x33, 0xBB, 0xBB, 0xB3,
	0x33, 0x33, 0x33, 0x00, 0x01,
	0x00, 0xA0, 0xD8, 0xA0, 0x00,
	0x57, 0x57, 0x44, 0x3B, 0x37,
	0x72, 0x07, 0x3D, 0xBF, 0x77
};
/* Vcom Setting */
static char vcom_setting[8] = {
	0xD5,
	0x06, 0x00, 0x00, 0x01, 0x40,
	0x01, 0x40
};
/* Backllight control 1 */
static char backlight_control_1[7] = {
	0xB8,
	0x07, 0x90, 0x1E, 0x10, 0x1E,
	0x32
};
/* Backllight control 2 */
static char backlight_control_2[7] = {
	0xB9,
	0x07, 0x82, 0x3C, 0x10, 0x3C,
	0x87
};
/* Backllight control 3 */
static char backlight_control_3[7] = {
	0xBA,
	0x07, 0x78, 0x64, 0x10, 0x64,
	0xB4
};
/* Backllight control 4 */
static char backlight_control_4[24] = {
	0xCE,
	0x75, 0x40, 0x43, 0x49, 0x55,
	0x62, 0x71, 0x82, 0x94, 0xA8,
	0xB9, 0xCB, 0xDB, 0xE9, 0xF5,
	0xFC, 0xFF, 0x07, 0x25, 0x04,
	0x04, 0x44, 0x20
};

#if 0
static char color_enhancement[33] = {
	0xCA,
	0x01, 0x70, 0xB0, 0xFF, 0xB0,
	0xB0, 0x98, 0x78, 0x3F, 0x3F,
	0x80, 0x80, 0x08, 0x38, 0x08,
	0x3F, 0x08, 0x90, 0x0C, 0x0C,
	0x0A, 0x06, 0x04, 0x04, 0x00,
	0xC0, 0x10, 0x10, 0x3F, 0x3F,
	0x3F, 0x3F
};

static char color_enhancement_off[33] = {
	0xCA,
	0x00, 0x80, 0xDC, 0xF0, 0xDC,
	0xBE, 0xA5, 0xDC, 0x14, 0x20,
	0x80, 0x8C, 0x0A, 0x4A, 0x37,
	0xA0, 0x55, 0xF8, 0x0C, 0x0C,
	0x20, 0x10, 0x20, 0x20, 0x18,
	0xE8, 0x10, 0x10, 0x3F, 0x3F,
	0x3F, 0x3F
};

static char auto_contrast[7] = {
	0xD8,
	0x00, 0x80, 0x80, 0x40, 0x42,
	0x55
};

static char auto_contrast_off[7] = {
	0xD8,
	0x00, 0x80, 0x80, 0x40, 0x42,
	0x55
};

static char sharpening_control[3] = { 0xDD, 0x01, 0x95 };

static char sharpening_control_off[3] = { 0xDD, 0x20, 0x45 };
#endif

/* Panel Pin Control(Mux On) */
static char mux_on_display_on[10] = {
	0xCB,
	0x31, 0xFC, 0x3F, 0x8C, 0x00,
	0x00, 0x00, 0x00, 0xC0
};
#if 0
/* Vcom Setting for Display Off & Sleep In Set */
static char vcom_setting_display_off[8] = {
	0xD5,
	0x06, 0x00, 0x00, 0x00, 0x48,
	0x00, 0x48
};
#endif


static int lglh520wf1_power_on(void)
{
	int ret;

	ret = gpio_direction_output(desc->gpio_dsv_enable, 1);
	if (ret < 0)
	{
		printk("gpio_direction_output failed for desc->gpio_dsv_enable gpiod %d\n", desc->gpio_dsv_enable);
		return -1;
	}

	ret = gpio_direction_output(desc->gpio_reset, 1);
	if (ret < 0)
	{
		printk("gpio_direction_output failed for lcd_reset gpiod %d\n", desc->gpio_reset);
		return -1;
	}

	gpio_set_value(desc->gpio_dsv_enable , 0);
	gpio_set_value(desc->gpio_reset , 1);

	mdelay(20);

	mdelay(6);

	gpio_set_value(desc->gpio_dsv_enable , 1);
	mdelay(6);

	gpio_set_value(desc->gpio_reset , 0);
	mdelay(5);

	gpio_set_value(desc->gpio_reset , 1);
	mdelay(5);

	return 0;
}

void lglh520wf1_power_off(void)
{

	gpio_set_value(desc->gpio_reset , 0);
	mdelay(2);

	gpio_set_value(desc->gpio_dsv_enable , 0);
	mdelay(2);

}


int lglh520wf1_panel_on(void)
{
	struct dsi_packet packet;

	lglh520wf1_power_on();

	mipi_ops->open(&panel_cfg);

	// horizontal flip
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_1_PARAMETER;
	packet.ph.packet_data.sp.data0=  0x36;
	packet.ph.packet_data.sp.data1=  0x80; //H_FLIP
	mipi_ops->send(&packet);

	/* Display Initial Set*/
	//  DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x11, 0x00),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x11;
	packet.ph.packet_data.sp.data1=  0x00;
	mipi_ops->send(&packet);

	// DSI_DLY_MS(100),
	mdelay(100);

	/*  mcap_setting*/
	//    DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xB0, 0x04),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0= mcap_setting[0];
	packet.ph.packet_data.sp.data1= mcap_setting[1];
	mipi_ops->send(&packet);

	//   DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, interface_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(interface_setting); 
	packet.pPayload= interface_setting;
	mipi_ops->send(&packet);


	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, dsi_ctrl),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(dsi_ctrl); 
	packet.pPayload= dsi_ctrl;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_1),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(display_setting_1); 
	packet.pPayload= display_setting_1;
	mipi_ops->send(&packet);


	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_2),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(display_setting_2); 
	packet.pPayload= display_setting_2;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vsync_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(vsync_setting); 
	packet.pPayload= vsync_setting;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, source_timing_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(source_timing_setting); 
	packet.pPayload= source_timing_setting;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, ltps_timing_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(ltps_timing_setting); 
	packet.pPayload= ltps_timing_setting;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_common_set),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(gamma_setting_common_set); 
	packet.pPayload= gamma_setting_common_set;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, digital_gamma_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(digital_gamma_setting); 
	packet.pPayload= digital_gamma_setting;
	mipi_ops->send(&packet);

	/* Panel Interface Control */
	//    DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xCC, 0x0B),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0= 0xcc;
	packet.ph.packet_data.sp.data1= 0x0B;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc= ARRAY_SIZE(pwr_setting); 
	packet.pPayload= pwr_setting;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting_internal_pwr),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(pwr_setting_internal_pwr); 
	packet.pPayload = pwr_setting_internal_pwr;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(vcom_setting); 
	packet.pPayload = vcom_setting;
	mipi_ops->send(&packet);

	//    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(vcom_setting); 
	packet.pPayload = vcom_setting;
	mipi_ops->send(&packet);

	/* Sequencer Control */
	//DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xD6, 0x01),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0xD6;
	packet.ph.packet_data.sp.data1=  0x01;
	mipi_ops->send(&packet);
	/* Write Display Brightness */
	//	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0x51, 0xFF),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x51;
	packet.ph.packet_data.sp.data1=  0xFF;
	mipi_ops->send(&packet);

	/* Write Control Display */
	//	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0x53, 0x24),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x53;
	packet.ph.packet_data.sp.data1=  0x24;
	mipi_ops->send(&packet);

	/* Write CABC */
	//	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0x55, 0x02),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x55;
	packet.ph.packet_data.sp.data1=  0x02;
	mipi_ops->send(&packet);

	/* Write CABC Minimum Brightness */
	//	DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0x5E, 0x00),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x5E;
	packet.ph.packet_data.sp.data1=  0x00;
	mipi_ops->send(&packet);

	//	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, backlight_control_1),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(backlight_control_1); 
	packet.pPayload = backlight_control_1;
	mipi_ops->send(&packet);

	//	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, backlight_control_2),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(backlight_control_2); 
	packet.pPayload = backlight_control_2;
	mipi_ops->send(&packet);

	//	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, backlight_control_3),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(backlight_control_3); 
	packet.pPayload = backlight_control_3;
	mipi_ops->send(&packet);

	//	DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, backlight_control_4),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(backlight_control_4); 
	packet.pPayload = backlight_control_4;
	mipi_ops->send(&packet);

	// DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, mux_on_display_on),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(mux_on_display_on); 
	packet.pPayload = mux_on_display_on;
	mipi_ops->send(&packet);

	// DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, mux_on_display_on),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= GENERIC_LONG_WRITE;
	packet.ph.packet_data.lp.wc = ARRAY_SIZE(mux_on_display_on); 
	packet.pPayload = mux_on_display_on;
	mipi_ops->send(&packet);

	/* Display On */
	//    DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x29, 0x00),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x29;
	packet.ph.packet_data.sp.data1=  0x00;
	mipi_ops->send(&packet);

	//  DSI_DLY_MS(17*3),
	mdelay(17*3);


    mipi_ops->start_video_mode();
	return 0;
}

int lglh520wf1_panel_off(void)
{
	struct dsi_packet packet;

	mipi_ops->stop_video_mode();

	// DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x28, 0x00),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x28;
	packet.ph.packet_data.sp.data1=  0x00;
	mipi_ops->send(&packet);

	// DSI_DLY_MS(17*3),
	mdelay(17*3);

	// DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x10, 0x00),
	packet.escape_or_hs = DPHY_ESCAPE;
	packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
	packet.ph.packet_data.sp.data0=  0x10;
	packet.ph.packet_data.sp.data1=  0x00;
	mipi_ops->send(&packet);

	// DSI_DLY_MS(17*9),
	mdelay(17*9);

    mipi_ops->close();

	lglh520wf1_power_off();

	return 0;
}

static bool lglh520wf1_mipi_dsi_init( struct mipi_hal_interface *hal_ops) {
	pr_info("%s \n",__func__);

	mipi_ops = hal_ops;

	return true;
}

static bool  lglh520wf1_blank( bool blank) {
	pr_info("%s \n",__func__);

	if(!mipi_ops)
		return false;

	if(blank) {
		lglh520wf1_panel_off();

	} else {
		lglh520wf1_panel_on();
	}

	return true;
}

static bool lglh520wf1_get_screen_info(struct panel_info *s_info) 
{
	s_info->name= desc->name;

	s_info->bpp = panel_cfg.vm_timing.bpp;
	s_info->data_width = panel_cfg.vm_timing.data_width;
	s_info->hbp = panel_cfg.vm_timing.hbp;
	s_info->hfp = panel_cfg.vm_timing.hfp;
	s_info->hsw = panel_cfg.vm_timing.hsw;
	s_info->hsync_pol= panel_cfg.vm_timing.hsync_pol;
	s_info->pclk_pol = panel_cfg.vm_timing.pclk_pol;
	s_info->pixel_clock = panel_cfg.vm_timing.pixel_clock;
	s_info->vbp = panel_cfg.vm_timing.vbp;
	s_info->vfp = panel_cfg.vm_timing.vfp;
	s_info->vsw = panel_cfg.vm_timing.vsw;
	s_info->vsync_pol = panel_cfg.vm_timing.vsync_pol;
	s_info->x_res = panel_cfg.vm_timing.x_res;
	s_info->y_res = panel_cfg.vm_timing.y_res;
	s_info->vsync_pol = panel_cfg.vm_timing.vsync_pol;
	s_info->hsync_pol = panel_cfg.vm_timing.hsync_pol;

	return true;
}

static bool lglh520wf1_is_command_mode(void)
{
	return false;
}

static int lglh520wf1_probe(struct platform_device *pdev)
{
	enum dsi_module dsi_idx;

	struct mipi_device_interface mipi_device_inf;
	int ret;
	const char *port_str;

	of_property_read_string(pdev->dev.of_node,"port",&port_str);

	of_property_read_u32(pdev->dev.of_node,"gpio_intr",&desc->gpio_intr);
	of_property_read_u32(pdev->dev.of_node,"gpio_reset",&desc->gpio_reset);
	of_property_read_u32(pdev->dev.of_node,"gpio_dsv_enable",&desc->gpio_dsv_enable);

    pr_info("%s port=%s gpio_intr:%d, gpio_reset: %d, gpio_dsv_enable: %d \n",__func__,port_str,desc->gpio_intr, desc->gpio_reset, desc->gpio_dsv_enable);

	if(strcmp(port_str,"lcd0") == 0) 
		dsi_idx = MIPI_DSI_DIP0;
	else if (strcmp(port_str,"lcd1") == 0)
		dsi_idx = MIPI_DSI_DIP1;
	else  {
		pr_err("invalid port name !!\n");
		ret = EINVAL;
		goto err;
	}

	mipi_device_inf.init= &lglh520wf1_mipi_dsi_init;
	mipi_device_inf.blank= &lglh520wf1_blank;
	mipi_device_inf.get_screen_info = &lglh520wf1_get_screen_info;
	mipi_device_inf.is_command_mode = &lglh520wf1_is_command_mode;
	mipi_device_ops_register(MIPI_DSI_DIP0, &mipi_device_inf);

	desc->name = (char*)pdev->dev.driver->name;
	pr_info("%s \n",__func__);


	//	pm_qos_add_request(&td->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ,
	//			PM_QOS_ODIN_MEM_MIN_FREQ_DEFAULT_VALUE);


	ret = gpio_request(desc->gpio_dsv_enable, "gpio_dsv_enable");
	if (ret < 0)
	{
		pr_debug("gpio_request failed for desc->gpio_dsv_enable gpio %d\n", desc->gpio_dsv_enable);
		return -1;
	}

	ret = gpio_request(desc->gpio_reset, "lcd_reset");
	if (ret < 0)
	{
		pr_debug("gpio_request failed for lcd_reset gpio %d\n", desc->gpio_reset);
		return -1;
	}

	return 0;
err:
	return ret;

}

static struct of_device_id lglh520wf1_match[]= {
	{ .compatible="lglh520wfi"},
	{},
};

static struct platform_driver lglh520wf1_driver = {
	.probe		= lglh520wf1_probe,
	.driver         = {
		.name   = "lglh520wf1",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(lglh520wf1_match),
	},
};

static int __init lglh520wf1_init(void)
{
	int revision_data;
	desc = vzalloc(sizeof(struct panel_desc));
	if (desc == NULL) {
		pr_err("vzalloc failed\n");
		return -1;
	}

#ifndef CONFIG_ODIN_DSS_FPGA
#ifdef CONFIG_ARABICA_CHECK_REV
	revision_data = odin_pmic_revision_get();
	printk("DSS Revision Check Value %x \n", revision_data);

	if(revision_data == 0xd)
		platform_driver_register(&lglh520wf1_driver);

#endif
#endif
	return 0;
}

static void __exit lglh520wf1_exit(void)
{
	platform_driver_unregister(&lglh520wf1_driver);

	if (desc) {
		vfree(desc);
		desc = NULL;
	}
}

late_initcall(lglh520wf1_init);
module_exit(lglh520wf1_exit);

MODULE_DESCRIPTION("lglh520wf1 video mode Driver");
MODULE_LICENSE("GPL");
