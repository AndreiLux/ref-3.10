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

#define DEBUG

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

#include <video/odindss.h>

#include "../dss/dss.h"
#include "../dss/mipi-dsi.h"
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/pm_qos.h>

#define LCD_RESET_N 2
#define DSV_EN		29

enum {
	PANEL_LGHDK,
};

struct panel_config {
	const char *name;
	int type;
	enum odin_dss_dsi_mode dsi_mode;
	struct odin_video_timings timings;

	struct {
		unsigned int sleep_in;
		unsigned int sleep_out;
		unsigned int hw_reset;
		unsigned int enable_te;
	} sleep;

	struct {
		unsigned int high;
		unsigned int low;
	} reset_sequence;
};

static struct panel_config panel_configs[] = {
	{
		.name		= "lglh500wf1_panel",
		.type		= PANEL_LGHDK,
		.dsi_mode	= ODIN_DSS_DSI_VIDEO_MODE,
		.timings	= {
			.x_res		= 1080,
			.y_res		= 1920,
			.height		= 121,
			.width		= 68,
			/* mipi clk(864MHz) * 4lanes / 24bits (KHz)*/
			.pixel_clock	= 144000,
			.data_width = 24,
			.hsw = 4, /* hsync_width */
			.hbp = 50,
			.hfp = 100,
			.vsw = 1,
			.vfp = 4,
#if 0
			.vbp = 25, /* 4 + 0x15 */
#else
			.vbp = 14, /* 4 + 0xA */
#endif
			.vsync_pol = 1,
			.hsync_pol = 1,
			.blank_pol = 0,
			.pclk_pol = 0,
		},
		.sleep		= {
			.sleep_in	= 60,
			.sleep_out	= 120,
			.hw_reset	= 30,
			.enable_te	= 100, /* possible panel bug */
		},
		.reset_sequence	= {
			.high		= 15000,
			.low		= 15000,
		},
	},
};


struct lglh500wf1_dsi_data {
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

#if 0
static char mcap_setting[2] = {0xB0, 0x04};

static char nop_command[2] = {0x00, 0x00};
#endif

static char interface_setting[7] = { 0xB3, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef MIPI_300MHZ_MODE
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xA3 };
#else
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xD3 };
#endif

static char display_setting_1[35] = {
	0xC1,
	0x84, 0x60, 0x50, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0C,
	0x01, 0x58, 0x73, 0xAE, 0x31,
	0x20, 0x06, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x10,
	0x10, 0x10, 0x00, 0x00, 0x00,
	0x22, 0x02, 0x02, 0x00
};

static char vsync_setting[4] = { 0xC3, 0x00, 0x00, 0x00 };

static char display_setting_2[8] = {
	0xC2,
#if 0
	0x32, 0xF7, 0x80, 0x1F, 0x08,  /*VBP 0xA+ 0x15*/
#else
 	0x32, 0xF7, 0x80, 0x14, 0x08,  /*VBP 0xA+ 0xA*/
#endif
	0x00, 0x00
};

static char source_timing_setting[23] = {
	0xC4,
	0x70, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x11,
	0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x00, 0x00,
	0x11, 0x06
};

static char ltps_timing_setting[41] = {
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

static char gamma_setting_a[25] = {
	0xC7,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

static char gamma_setting_b[25] = {
	0xC8,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

static char gamma_setting_c[25] = {
	0xC9,
	0x00, 0x09, 0x14, 0x23, 0x30,
	0x48, 0x3D, 0x52, 0x5F, 0x67,
	0x6B, 0x70, 0x00, 0x09, 0x14,
	0x23, 0x30, 0x48, 0x3D, 0x52,
	0x5F, 0x67, 0x6B, 0x70
};

/*static char panel_interface_ctrl        [2] = {0xCC, 0x09};*/

static char pwr_setting_chg_pump[15] = {
	0xD0,
	0x00, 0x00, 0x19, 0x18, 0x99,
	0x99, 0x19, 0x01, 0x89, 0x00,
	0x55, 0x19, 0x99, 0x01
};

static char pwr_setting_internal_pwr[27] = {
	0xD3,
	0x1B, 0x33, 0xBB, 0xCC, 0xC4,
	0x33, 0x33, 0x33, 0x00, 0x01,
	0x00, 0xA0, 0xD8, 0xA0, 0x0D,
	0x39, 0x33, 0x44, 0x22, 0x70,
	0x02, 0x39, 0x03, 0x3D, 0xBF,
	0x00
};

static char vcom_setting[8] = {
	0xD5,
	0x06, 0x00, 0x00, 0x01, 0x2C,
	0x01, 0x2C
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

struct odin_dsi_cmd lglh500wf1_dsi_flip_command[] = {
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_1_PARA, 0x36, 0x00),
};

struct odin_dsi_cmd lglh500wf1_dsi_off_command[] = {
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x28, 0x00),
	DSI_DLY_MS(17*3),
	DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x10, 0x00),
	DSI_DLY_MS(17*9),
};


struct odin_dsi_cmd lglh500wf1_dsi_init_cmd[] = {
/* AFTER TUNING */
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
	/* panel_interface_ctrl),*/
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

static int lglh500wf1_dsi_enable(struct odin_dss_device *dssdev);
static void lglh500wf1_dsi_disable(struct odin_dss_device *dssdev);
static int lglh500wf1_dsi_resume(struct odin_dss_device *dssdev);
static int lglh500wf1_dsi_suspend(struct odin_dss_device *dssdev);

static void lglh500wf1_dsi_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

#if 0
int lglh500wf1_dsi_panel_on(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev);
#endif

static ssize_t lglh500wf1_dsi_enable_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int on_off;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &on_off);

	if (on_off==1)
	{
		lglh500wf1_dsi_enable(dssdev);
		odin_set_rd_enable_plane(dssdev->channel, 1);
	}
	else
	{
		odin_set_rd_enable_plane(dssdev->channel, 0);
		lglh500wf1_dsi_disable(dssdev);
	}

	return count;
}

static ssize_t lglh500wf1_dsi_resume_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int on_off;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &on_off);

	if (on_off==1)
	{
		lglh500wf1_dsi_resume(dssdev);
		odin_set_rd_enable_plane(dssdev->channel, 1);
	}
	else
	{
		odin_set_rd_enable_plane(dssdev->channel, 0);
		lglh500wf1_dsi_suspend(dssdev);
	}

	return count;
}

void a_function(int enable)
{
   /* int ret; */

   return;
   /*struct mbox_data mbox_data = {0, };
   mbox_data.cmd_type = PM_CMD_TYPE;

   if (enable == 0)
	   mbox_data.cmd = 0x82000000; // +1V8_LCD_IOVCC OFF
   else
	   mbox_data.cmd = 0x82000001; // +1V8_LCD_IOVCC ON

   init_completion(&dss_ipc_completion);
   ret = mailbox_call_fast(&mbox_data);
   if (ret < 0)
   {
	   pr_err("%s: Failed to ipc_call_fast: errno(%d)\n",
			   __func__, ret);
	   goto out;
   }

   ret = wait_for_completion_interruptible_timeout(&dss_ipc_completion,
		   msecs_to_jiffies(1000));
   if (ret == 0) {
	   ret = -ETIMEDOUT;
	   pr_err("%s: Failed to recieve completion signal: errno(%d)\n",
			   __func__, ret);

	   goto out;
   }

out:
   return ret;*/
}

static void lglh500wf1_power_on(struct odin_dss_device *dssdev)
{
	int ret;

	ret = gpio_direction_output(DSV_EN, 1);
	if (ret < 0)
	{
		DSSERR("gpio_direction_output failed for dsv_en gpiod %d\n", DSV_EN);
		return;
	}

	ret = gpio_direction_output(LCD_RESET_N, 1);
	if (ret < 0)
	{
		DSSERR("gpio_direction_output failed for lcd_reset gpiod %d\n", LCD_RESET_N);
		return;
	 }

	a_function(0);
	gpio_set_value(DSV_EN , 0);
	gpio_set_value(LCD_RESET_N , 1);

	mdelay(20);

	a_function(1);
	mdelay(6);

	gpio_set_value(DSV_EN , 1);
	mdelay(6);

	gpio_set_value(LCD_RESET_N , 0);
	mdelay(5);

	gpio_set_value(LCD_RESET_N , 1);
	mdelay(5);
}

void lglh500wf1_power_off(struct odin_dss_device *dssdev)
{
	gpio_set_value(LCD_RESET_N , 0);
	mdelay(2);

	gpio_set_value(DSV_EN , 0);
	mdelay(2);

	a_function(0);
	mdelay(4);
}


int lglh500wf1_panel_on(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev)
{
	u8 flip_data = 0;

	/*if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev->driver);
		if (r)
			return r;
	}*/

	lglh500wf1_power_on(dssdev);

	odin_mipi_dsi_panel_config(resource_index, dssdev);

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);

	if ((dssdev->panel.flip & ODIN_DSS_H_FLIP) &&
		(dssdev->panel.flip_supported & ODIN_DSS_H_FLIP))
		flip_data |= 0x40;

	if ((dssdev->panel.flip & ODIN_DSS_V_FLIP) &&
		(dssdev->panel.flip_supported & ODIN_DSS_V_FLIP))
		flip_data |= 0x80;

	lglh500wf1_dsi_flip_command[0].sp_len_dly.sp.data1 = flip_data;

	if (flip_data)
		odin_mipi_dsi_cmd(resource_index, lglh500wf1_dsi_flip_command,
					ARRAY_SIZE(lglh500wf1_dsi_flip_command));

	odin_mipi_dsi_cmd(resource_index, lglh500wf1_dsi_init_cmd,
					ARRAY_SIZE(lglh500wf1_dsi_init_cmd));

	return 0;
}

int lglh500wf1_panel_off(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev)
{
	odin_mipi_hs_video_mode_stop(resource_index, dssdev);

	odin_mipi_dsi_cmd(resource_index, lglh500wf1_dsi_off_command,
				ARRAY_SIZE(lglh500wf1_dsi_off_command));

	lglh500wf1_power_off(dssdev);

	return 0;
}

static int lglh500wf1_dsi_enable(struct odin_dss_device *dssdev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&td->lock);

	if (dssdev->boot_display)
	{
		dssdev->boot_display = false;
		dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;
		dssdev->manager->mgr_flip =
			(~(dssdev->panel.flip_supported) & (dssdev->panel.flip));
		dssdev->manager->frame_skip = false;
		odin_crg_set_err_mask(dssdev, true);

		mutex_unlock(&td->lock);

		return 0;
	}

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}
	odin_du_plane_init();

	odin_crg_dss_channel_clk_enable(dssdev->channel, true);

	odin_set_rd_enable_plane(dssdev->channel, 0);

	odin_du_display_init(dssdev);

	odin_dipc_display_init(dssdev);

	odin_mipi_dsi_init(dssdev);

	r = odin_du_power_on(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to power on du\n");
		goto err;
	}

	r = lglh500wf1_panel_on(dssdev->channel, dssdev);

	odin_mipi_hs_video_mode(dssdev->channel, dssdev);
/*
	dsscomp_refresh_syncpt_value(dssdev);
*/
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	return 0;

err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&td->lock);
	return r;
}

extern void crg_set_irq_mask_all(u8 enable);
static void lglh500wf1_dsi_disable(struct odin_dss_device *dssdev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&td->lock);

	if (dssdev->state == ODIN_DSS_DISPLAY_ACTIVE) {

		dssdev->state = ODIN_DSS_DISPLAY_DISABLED;

		dsscomp_complete_workqueue(dssdev, false, true);

		odin_set_rd_enable_plane(dssdev->channel, 0);

		r =lglh500wf1_panel_off(dssdev->channel, dssdev);
		if (r) {
			dev_err(&dssdev->dev, "failed to panel_off off du\n");
			goto err;
		}

		r = odin_du_power_off(dssdev);
		if (r) {
			dev_err(&dssdev->dev, "failed to du_power_off off du\n");
			goto err;
		}

		odin_crg_dss_channel_clk_enable(dssdev->channel, false);

		crg_set_irq_mask_all(0);

		pm_runtime_put_sync(&dssdev->dev);

		pr_info("%s: finished\n", __func__);
	}

	mutex_unlock(&td->lock);

	return;

err:
	dev_dbg(&dssdev->dev, "disable failed\n");
	mutex_unlock(&td->lock);
}

static int lglh500wf1_dsi_suspend(struct odin_dss_device *dssdev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev, "suspend\n");
	printk("lglh500wf1_dsi_suspend\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_SUSPENDED;

	dsscomp_complete_workqueue(dssdev, false, true);

	odin_set_rd_enable_plane(dssdev->channel, 0);

	r =lglh500wf1_panel_off(dssdev->channel, dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to panel_off off du\n");
		goto err;
	}

	r = odin_du_power_off(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to du_power_off off du\n");
		goto err;
	}

	odin_crg_dss_channel_clk_enable(dssdev->channel, false);

	pm_runtime_put_sync(&dssdev->dev);

	mutex_unlock(&td->lock);

	return 0;
err:
	mutex_unlock(&td->lock);
	return r;
}

static int lglh500wf1_dsi_resume(struct odin_dss_device *dssdev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(&dssdev->dev);

	int r;

	dev_dbg(&dssdev->dev, "resume\n");
	printk("lglh500wf1_dsi_resume\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_SUSPENDED) {
		r = -EINVAL;
		goto err;
	}

	pm_runtime_get_sync(&dssdev->dev);

	odin_crg_dss_channel_clk_enable(dssdev->channel, true);

	odin_du_plane_init();

	odin_set_rd_enable_plane(dssdev->channel, 0);

	odin_du_display_init(dssdev);

	odin_dipc_display_init(dssdev);

	odin_mipi_dsi_init(dssdev);

	/* lcd panel enable */
	r = odin_du_power_on(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to power on du\n");
		goto err;
	}

	r = lglh500wf1_panel_on(dssdev->channel, dssdev);

	odin_mipi_hs_video_mode(dssdev->channel, dssdev);
/*
	dsscomp_refresh_syncpt_value(dssdev);
*/
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	return r;
err:
	mutex_unlock(&td->lock);
	return r;
}


static DEVICE_ATTR(panel_enable, S_IWUSR,
		NULL, lglh500wf1_dsi_enable_test);
static DEVICE_ATTR(panel_resume, S_IWUSR,
		NULL, lglh500wf1_dsi_resume_test);

static struct attribute *lglh500wf1_attrs[] = {
	&dev_attr_panel_enable.attr,
	&dev_attr_panel_resume.attr,
	NULL,
};

static struct attribute_group lglh500wf1_attr_group = {
	.attrs = lglh500wf1_attrs,
};

static int lglh500wf1_dsi_probe(struct odin_dss_device *dssdev)
{
	int r;
	struct lglh500wf1_dsi_data *td;
	enum odin_mipi_dsi_index resource_index =
		(enum odin_mipi_dsi_index) dssdev->channel;
	int ret;

	dssdev->panel.dsi_mode = panel_configs->dsi_mode;
	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.timings = panel_configs->timings;
	dssdev->panel.dsi_pix_fmt = ODIN_DSI_OUTFMT_RGB24;
	dssdev->panel.flip = ODIN_DSS_H_FLIP | ODIN_DSS_V_FLIP;
	dssdev->panel.flip_supported = ODIN_DSS_H_FLIP;
	dssdev->phy.dsi.lane_num = ODIN_DSI_4LANE;

	DSSDBG("lglh500wf1_dsi_probe\n");

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	mutex_init(&td->lock);

	pm_qos_add_request(&td->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ,
			PM_QOS_ODIN_MEM_MIN_FREQ_DEFAULT_VALUE);

	dev_set_drvdata(&dssdev->dev, td);

	if (resource_index > ODIN_DSI_CH1 || resource_index < ODIN_DSI_CH0) {
		r = -EINVAL;
		goto err;
	}

	ret = gpio_request(DSV_EN, "dsv_en");
	if (ret < 0)
	{
		DSSERR("gpio_request failed for dsv_en gpio %d\n", DSV_EN);
		return -1;
	}

	ret = gpio_request(LCD_RESET_N, "lcd_reset");
	if (ret < 0)
	{
	 DSSERR("gpio_request failed for lcd_reset gpio %d\n", LCD_RESET_N);
	 return -1;
	}

#if 0
	odin_mipi_dsi_init(dssdev);
#endif

	r = sysfs_create_group(&dssdev->dev.kobj, &lglh500wf1_attr_group);
	if (r) {
		dev_err(&dssdev->dev, "failed to create sysfs files\n");
		goto err_device_create_file;
	}

	r = odin_pd_register_dev(&dssdev->dev, &odin_pd_dss3_lcd0_panel);
	if (r < 0)
	{
		dev_err(&dssdev->dev, "failed to register power domain\n");
		goto err;
	}

	/* FIXME: comment */
	pm_runtime_get_noresume(&dssdev->dev);
	pm_runtime_set_active(&dssdev->dev);
	pm_runtime_enable(&dssdev->dev);

	return 0;

err_device_create_file:
	sysfs_remove_group(&dssdev->dev.kobj, &lglh500wf1_attr_group);

err:

	return r;
}

static void __exit lglh500wf1_dsi_remove(struct odin_dss_device *dssdev)
{

	/* reset, to be sure that the panel is in a valid state */
	/*lglh500wf1_dsi_hw_reset(dssdev);
*/
}

#if 0
static void lglh500wf1_dsi_power_off(struct odin_dss_device *dssdev)
{

}

static int lglh500wf1_dsi_panel_reset(struct odin_dss_device *dssdev)
{
	dev_err(&dssdev->dev, "performing LCD reset\n");

	lglh500wf1_dsi_power_off(dssdev);
	/*lglh500wf1_dsi_hw_reset(dssdev);*/
	return lglh500wf1_dsi_panel_on(dssdev->channel, dssdev);
}
#endif

static void lglh500wf1_dsi_get_resolution(
	struct odin_dss_device *dssdev, u16 *xres, u16 *yres)
{
	*xres = (u16)panel_configs->timings.x_res;
	*yres = (u16)panel_configs->timings.y_res;
}


#if 0
static void lglh500wf1_dsi_ulps_work(struct work_struct *work)
{

}

static void lglh500wf1_dsi_esd_work(struct work_struct *work)
{

}
#endif

static int lglh500wf1_dsi_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_MANUAL)
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_VIDEO_MODE;
	else
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_CMD_MODE;

	return 0;
}

static enum odin_dss_update_mode lglh500wf1_dsi_get_update_mode(
		struct odin_dss_device *dssdev)
{
	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
		return ODIN_DSS_UPDATE_MANUAL;
	else
		return ODIN_DSS_UPDATE_AUTO;
}

static int lglh500wf1_dsi_runtime_suspend(struct device *dev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(dev);

	dev_info(dev, "lglh500wf1_dsi_runtime_suspend\n");

	pm_qos_update_request(&td->mem_qos, 0);
	return 0;
}

static int lglh500wf1_dsi_runtime_resume(struct device *dev)
{
	struct lglh500wf1_dsi_data *td = dev_get_drvdata(dev);

	dev_info(dev, "lglh500wf1_dsi_runtime_resume\n");

	pm_qos_update_request(&td->mem_qos, 0);
	return 0;
}


static const struct dev_pm_ops lglh500wf1_dsi_pm_ops = {
	.runtime_suspend = lglh500wf1_dsi_runtime_suspend,
	.runtime_resume	 = lglh500wf1_dsi_runtime_resume,
};

static struct odin_dss_driver lglh500wf1_dsi_driver = {
	.probe		= lglh500wf1_dsi_probe,
	.remove		= __exit_p(lglh500wf1_dsi_remove),

	.enable		= lglh500wf1_dsi_enable,
	.disable	= lglh500wf1_dsi_disable,
	.suspend	= lglh500wf1_dsi_suspend,
	.resume		= lglh500wf1_dsi_resume,

	.set_update_mode = lglh500wf1_dsi_set_update_mode,
	.get_update_mode = lglh500wf1_dsi_get_update_mode,

	.get_resolution	= lglh500wf1_dsi_get_resolution,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= lglh500wf1_dsi_get_timings,

	.driver         = {
		.name   = "dsi_lglh500wf1_panel",
		.owner  = THIS_MODULE,
		.pm	= &lglh500wf1_dsi_pm_ops,
	},
};

static int __init lglh500wf1_dsi_init(void)
{
	odin_dss_register_driver(&lglh500wf1_dsi_driver);

	return 0;
}

static void __exit lglh500wf1_dsi_exit(void)
{
	odin_dss_unregister_driver(&lglh500wf1_dsi_driver);
}

module_init(lglh500wf1_dsi_init);
module_exit(lglh500wf1_dsi_exit);

MODULE_AUTHOR("yunsh <yunsh@mtekvision.com>");
MODULE_DESCRIPTION("lglh500wf1 video mode Driver");
MODULE_LICENSE("GPL");

