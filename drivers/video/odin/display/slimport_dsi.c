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

#define MIPI_DSI1_LCD	0
#define SLIMPORT_DSI_MAX_MODEDB		20

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
#include <linux/spinlock.h>
#include <video/odindss.h>
#include <linux/slimport.h>
#include <linux/switch.h>

#include "../dss/dss.h"
#include "../dss/mipi-dsi.h"
#include <linux/odin_mailbox.h>

#if	MIPI_DSI1_LCD
#include "lcd-slimport-sample.h"
#else
enum {
	PANEL_LGHDK,
};
#endif

#ifdef  MIPI_DSI1_LCD==0
#define HDMI_EDID_MAX_LENGTH			256
/* static unchar	edidextblock[128] ={0,}; */
static unchar	edid_block[256] ={0,};
#endif

#define MIPI_DSI1_DEBUG
#ifdef MIPI_DSI1_DEBUG
#define MIPI_DSI1_DEBUG_INFO(fmt, args...) \
	pr_info("[MIPI_DSI1]: %s: " fmt, __func__, ##args)
#else
#define MIPI_DSI1_DEBUG_INFO(fmt, args...)  do { } while (0)
#endif
#define MIPI_DSI1_DEBUG_ERR(fmt, args...) \
	pr_err("[MIPI_DSI1]: %s: " fmt, __func__, ##args)

/*static DEFINE_SPINLOCK(slimport_cbl_det_dsi1_lock);*/

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

#if 0
static struct panel_config panel_configs[] = {
	{
		.name		= "mipi_dsi1",
		.type		= PANEL_LGHDK,
		.timings	= {
			.x_res		= 1080,
			.y_res		= 1920,
			.data_width = 24,
			.hsw = 4, /*hsync_width*/
			.hbp = 50,
			.hfp = 100,
			.vsw = 1,
			.vfp = 4,
			.vbp = 4,
			.vsync_pol = 1,
			.hsync_pol = 1,
			.blank_pol = 0,
			.pclk_pol = 1,
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
#endif

static struct panel_config panel_configs[] = {
	{
		.name		= "dsi_slimport_panel",
		.type		= PANEL_LGHDK,
		.dsi_mode	= ODIN_DSS_DSI_VIDEO_MODE,
		.timings	= {
			.x_res		= 1920,
			.y_res		= 1080,
			.pixel_clock = 148500, /*KHZ*/
			.data_width = 24,
			.hsw = 44,
			.hbp = 148,
			.hfp = 88,
			.vsw = 2,
			.vfp = 4,
			.vbp = 39,
			.vsync_pol = 1,
			.hsync_pol = 1,
			.blank_pol = 0,
			.pclk_pol = 0,
		},
	},
};


struct slimport_dsi_data {
	struct mutex lock;

	struct backlight_device *bldev;

	unsigned long	hw_guard_end;	/* next value of jiffies when we can
					 * issue the next sleep in/out command
					 */
	unsigned long	hw_guard_wait;	/* max guard time in jiffies */

	struct odin_dss_device *dssdev;

	struct fb_info *info;

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
	struct switch_dev sdev;
	struct switch_dev saudiodev;
};

struct slimport_dsi_data *g_td;

static DECLARE_COMPLETION(dss_ipc_completion);


static int slimport_dsi_check_validation(uint8_t *edid_buf)
{
	unchar checksum = 0;
	int i=0;

	for (i=0;i<128;i++)
		checksum= checksum + edid_buf[i];
	checksum = checksum&0xff;
	checksum = checksum - edid_buf[127];
	checksum = ~checksum + 1;
	if (checksum != edid_buf[127])
    {
         MIPI_DSI1_DEBUG_ERR("Bad EDID check sum!\n");
         return -1;
    }
	return 0;
}

static int slimport_dsi_find_best_resoultion(struct fb_monspecs *specs)
{
	int i;
	struct fb_videomode *vm;
	struct odin_video_timings dss_timings;
	int second_size;
	int best_score = 0;
	u32 cur_score = 0;
	int best_index = 0;

	for (i = 0; i < specs->modedb_len; i++)
	{
		pr_info("************************number : %d ************************ \n", i);
		vm = &specs->modedb[i];
		odinfb_fb2dss_timings(vm, &dss_timings); /* For Debbuging */
		cur_score =
			dss_timings.x_res * dss_timings.y_res * (dss_timings.pixel_clock / 1000);
		if (best_score < cur_score)
		{
			best_score = cur_score;
			best_index = i;
		}
 	}

	return best_index;
}

static void slimport_dsi_get_monspecs(
	struct fb_monspecs *specs, uint8_t *edid_buf)
{
	int i, j;
	char *edid = edid_buf;
	struct fb_videomode *vm;

	memset(specs, 0x0, sizeof(*specs));

	fb_edid_to_monspecs(edid, specs);
	if (specs->modedb == NULL)
		return;

	for (i = 1; i <= edid[0x7e] && i * 128 < HDMI_EDID_MAX_LENGTH; i++) {
		if (edid[i * 128] == 0x2)
			fb_edid_add_monspecs(edid + i * 128, specs);
	}

	/* filter out resolutions we don't support */
	for (i = j = 0; i < specs->modedb_len; i++) {
		vm = &specs->modedb[i];
		/* Use Continue */
		if ((vm->xres > 1920) || (vm->yres > 1080))
			continue;

		/* skip repeated pixel modes */
		if (vm->vmode & FB_VMODE_INTERLACED ||
				vm->vmode & FB_VMODE_DOUBLE)
			continue;

		specs->modedb[j++] = *vm;
	}

	specs->modedb_len = j;
}

int slimport_dsi_update_panel_config(uint8_t *edid_buf)
{
	struct fb_monspecs *specs = &g_td->dssdev->panel.monspecs;
	struct odin_video_timings *timings = &g_td->dssdev->panel.timings;
	int best_mode_db;

	/* get monspecs from edid */
	slimport_dsi_get_monspecs(specs, edid_buf);
	pr_info("panel size %d by %d\n", specs->max_x,	specs->max_y);
	g_td->dssdev->panel.width_in_um = specs->max_x * 10000;
	g_td->dssdev->panel.height_in_um = specs->max_y * 10000;

	best_mode_db = slimport_dsi_find_best_resoultion(specs);

	odinfb_fb2dss_timings(&specs->modedb[best_mode_db], timings);

	fb_videomode_to_var(&g_td->info->var, &specs->modedb[best_mode_db]);

	pr_info("best index : %d \n", best_mode_db);

	return 0;
}


static int slimport_dsi_free_modedb(void)
{
	struct fb_monspecs *specs = &g_td->dssdev->panel.monspecs;

	fb_destroy_modedb(specs->modedb);
	return 0;
}

static int slimport_dsi_set_hot_plug_status(
	struct odin_dss_device *dssdev, bool onoff)
{
	int ret = 0;

	/* reset the HPD event notification */
	/*switch_set_state(&td->sdev, 0);*/
	/* reset HDMI Audio swtich */
	/*switch_set_state(&td->saudiodev, 0);*/
	if (onoff) {
		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_ONLINE);
		MIPI_DSI1_DEBUG_INFO("HDMI HPD: CONNECTED: send ONLINE\n");
		if (ret) {
			MIPI_DSI1_DEBUG_ERR("kobject_uevent %d\n", ret);
		}
		switch_set_state(&g_td->sdev, 1);
		MIPI_DSI1_DEBUG_INFO("Hdmi state switch to %d\n", g_td->sdev.state);
		switch_set_state(&g_td->saudiodev, 1);
		MIPI_DSI1_DEBUG_INFO("hdmi_audio state switched to %d\n" ,
						g_td->saudiodev.state);
	}
	else {
		switch_set_state(&g_td->saudiodev, 0);
		MIPI_DSI1_DEBUG_INFO("hdmi_audio state switched to %d\n",
						g_td->saudiodev.state);
		switch_set_state(&g_td->sdev, 0);
		MIPI_DSI1_DEBUG_INFO("Hdmi state switch to %d\n",
						g_td->sdev.state);
		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_OFFLINE);
		if (ret) {
			MIPI_DSI1_DEBUG_ERR("kobject_uevent %d\n", ret);
		}
		MIPI_DSI1_DEBUG_INFO("HDMI HPD: CONNECTED: send OFFLINE\n");
	}
	return ret;
}
static int slimport_dsi_enable(struct odin_dss_device *dssdev);
static void slimport_dsi_disable(struct odin_dss_device *dssdev);

static ssize_t slimport_dsi_enable_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int en = 0;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &en);

	if (en)
		slimport_dsi_enable(dssdev);

	odin_set_rd_enable_plane(dssdev->channel, en);

	if (!en)
		slimport_dsi_disable(dssdev);

	return count;
}

static int slimport_dsi_enable(struct odin_dss_device *dssdev)
{
	struct slimport_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	enum odin_mipi_dsi_index resource_index =
		(enum odin_mipi_dsi_index)(dssdev->channel);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		dev_err(&dssdev->dev, "Calling %s, Slimport NOT disabled\n", __func__);
		r = -EINVAL;
		goto err;
	}

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

#if MIPI_DSI1_LCD
	r = slimport_panel_on(resource_index, dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to panel_off off du\n");
		goto err;
	}
#else
	odin_mipi_dsi_panel_config(resource_index, dssdev);
	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);
#endif

	odin_mipi_hs_video_mode(resource_index, dssdev);

	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	return r;
err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&td->lock);
	return r;
}

static void slimport_dsi_disable(struct odin_dss_device *dssdev)
{
	struct slimport_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_SUSPENDED;

	odin_set_rd_enable_plane(dssdev->channel, 0);

#if MIPI_DSI1_LCD
	r = slimport_panel_off(dssdev->channel, dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to panel_off off du\n");
		goto err;
	}
#else
	odin_mipi_hs_video_mode_stop(dssdev->channel, dssdev);
#endif

	r = odin_du_power_off(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to du_power_off off du\n");
		goto err;
	}

	odin_crg_dss_channel_clk_enable(dssdev->channel, false);

	dssdev->state = ODIN_DSS_DISPLAY_DISABLED;

	mutex_unlock(&td->lock);

	return;
err:
	dev_dbg(&dssdev->dev, "disable failed\n");
	odin_crg_dss_channel_clk_enable(dssdev->channel, true);
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;
	mutex_unlock(&td->lock);
	return;
}

static DEVICE_ATTR(panel_enable, S_IWUSR,
		NULL, slimport_dsi_enable_test);

static int slimport_cbl_det_dsi1_handler(
	struct notifier_block *this, unsigned long event,void *ptr)
{
	unsigned long flags;

	/*spin_lock_irqsave(&slimport_cbl_det_dsi1_lock, flags);*/

	MIPI_DSI1_DEBUG_INFO("slimport_cbl_det_dsi1_handler event=%d\n", event);

	if (event==1) {
#ifdef  MIPI_DSI1_LCD==0
#if 0
		memset(edidextblock, 0x00,
		       sizeof(edidextblock)/sizeof(edidextblock[0]));
		if (slimport_read_edid_block(1, edidextblock) != 0)
		{
			MIPI_DSI1_DEBUG_ERR("slimport get edid error\n");
			return NOTIFY_DONE;
		}
		if (slimport_dsi_check_validation(edidextblock) != 0)
		{
			MIPI_DSI1_DEBUG_ERR("slimport get edid check sum error\n");
			return NOTIFY_DONE;
		}
#else
		memset(edid_block, 0x00,
		       sizeof(edid_block)/sizeof(edid_block[0]));
		if (slimport_read_edid_block(0, &edid_block[0]) != 0)
		{
			MIPI_DSI1_DEBUG_ERR("slimport get edid error\n");
			return NOTIFY_DONE;
		}
		if (slimport_read_edid_block(1, &edid_block[128]) != 0)
		{
			MIPI_DSI1_DEBUG_ERR("slimport get edid error\n");
			return NOTIFY_DONE;
		}
#endif
		if (slimport_dsi_update_panel_config(edid_block)  != 0)
		{
			MIPI_DSI1_DEBUG_ERR("slimport update panel config error\n");
			return NOTIFY_DONE;
		}
#if 1 /* TV Setting to Slimport */
		slimport_set_mipi_timming_update(3);
#else
		slimport_set_mipi_timming_update(15);
#endif

#endif
		slimport_dsi_enable(g_td->dssdev);
		slimport_dsi_set_hot_plug_status(g_td->dssdev, 1);
	}

	if (event == 0) {
		slimport_dsi_free_modedb();
		slimport_dsi_set_hot_plug_status(g_td->dssdev, 0);
		slimport_dsi_disable(g_td->dssdev);
	}

	/*spin_unlock_irqrestore(&slimport_cbl_det_dsi1_lock, flags);*/

	return NOTIFY_DONE;
}
/* register notifier handler */
static struct notifier_block slimport_cbl_det_handler_block = {
	.notifier_call  = slimport_cbl_det_dsi1_handler,
};

static struct attribute *slimport_attrs[] = {
	&dev_attr_panel_enable.attr,
	NULL,
};

static struct attribute_group slimport_attr_group = {
	.attrs = slimport_attrs,
};

static int slimport_dsi_probe(struct odin_dss_device *dssdev)
{
	int r;
	struct slimport_dsi_data *td;
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	enum odin_mipi_dsi_index resource_index =
		(enum odin_mipi_dsi_index) dssdev->channel;

	dssdev->panel.dsi_mode = panel_configs->dsi_mode;
	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.dsi_pix_fmt = ODIN_DSI_OUTFMT_RGB24;
	dssdev->panel.flip = 0;
	dssdev->panel.flip_supported = 0;
	dssdev->phy.dsi.lane_num = ODIN_DSI_4LANE;
	dssdev->panel.timings = panel_configs->timings;

	MIPI_DSI1_DEBUG_INFO("slimport_dsi_probe(index:%d)\n", resource_index);

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	mutex_init(&td->lock);

	dev_set_drvdata(&dssdev->dev, td);

	if (resource_index > ODIN_DSI_CH1 || resource_index < ODIN_DSI_CH0) {
		r = -EINVAL;
		goto err;
	}

#if 0
	odin_mipi_dsi_init(dssdev);
#endif

	r = sysfs_create_group(&dssdev->dev.kobj, &slimport_attr_group);
	if (r) {
		dev_err(&dssdev->dev, "failed to create sysfs files\n");
		goto err_device_create_file;
	}

#if MIPI_DSI1_LCD
	slimport_panel_init();
#endif	/* MIPI_DSI1_LCD */


	/* Setup notifier */
	atomic_notifier_chain_register(
		&slimport_notifier_list, &slimport_cbl_det_handler_block);

	/* Initialize hdmi node and register with switch driver */
	td->sdev.name = "mipi";
	if (switch_dev_register(&td->sdev) < 0)
		MIPI_DSI1_DEBUG_ERR("mipi switch registration failed\n");

	/* Initialize the hdmi audio node and register with switch driver */
	td->saudiodev.name = "mipi_audio";
	if (switch_dev_register(&td->saudiodev) < 0)
		MIPI_DSI1_DEBUG_ERR("mipi_audio switch registration failed\n");

	g_td = td;

	MIPI_DSI1_DEBUG_INFO("mipi_dsi1_probe finish\n");

	return 0;

err_device_create_file:
	sysfs_remove_group(&dssdev->dev.kobj, &slimport_attr_group);

err:
	return r;
}

static int slimport_dsi_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

static int slimport_dsi_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static void __exit slimport_dsi_remove(struct odin_dss_device *dssdev)
{
	/* Unregister hdmi node from switch driver */
	switch_dev_unregister(&g_td->sdev);

	/* Unregister hdmi node from switch driver */
	switch_dev_unregister(&g_td->saudiodev);

	blocking_notifier_chain_unregister(
		&slimport_notifier_list, &slimport_cbl_det_handler_block);

	kfree(g_td);
	return;
}

static int slimport_dsi_get_modedb(struct odin_dss_device *dssdev,
			   struct fb_videomode *modedb, int modedb_len)
{
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	if (specs->modedb_len < modedb_len)
		modedb_len = specs->modedb_len;
	memcpy(modedb, specs->modedb, sizeof(*modedb) * modedb_len);
	return modedb_len;
}

static void slimport_dsi_get_resolution(
	struct odin_dss_device *dssdev, u16 *xres, u16 *yres)
{
#if MIPI_DSI1_LCD
	*xres = (u16)panel_configs->timings.x_res;
	*yres = (u16)panel_configs->timings.y_res;
#else
	*xres = (u16)dssdev->panel.timings.x_res;
	*yres = (u16)dssdev->panel.timings.y_res;
#endif
	return;
}

static int slimport_dsi_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_MANUAL)
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_VIDEO_MODE;
	else
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_CMD_MODE;

	return 0;
}

static enum odin_dss_update_mode slimport_dsi_get_update_mode(
		struct odin_dss_device *dssdev)
{
	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
		return ODIN_DSS_UPDATE_MANUAL;
	else
		return ODIN_DSS_UPDATE_AUTO;
}

static void slimport_dsi_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{

	*timings = dssdev->panel.timings;
	MIPI_DSI1_DEBUG_INFO("slimport_dsi_get_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	return;
}

static void slimport_dsi_set_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	MIPI_DSI1_DEBUG_INFO("slimport_dsi_set_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	dssdev->panel.timings = *timings;
}

static int slimport_dsi_check_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	MIPI_DSI1_DEBUG_INFO("slimport_dsi_check_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	if (memcmp(&dssdev->panel.timings, timings, sizeof(*timings)) == 0)
		return 0;

	MIPI_DSI1_DEBUG_INFO("#### different!!!####\n");
	return -EINVAL;
}

static int slimport_dsi_set_fb_info(struct odin_dss_device *dssdev,
			struct fb_info *info)
{
	struct slimport_dsi_data *td = dev_get_drvdata(&dssdev->dev);

	td->info = info;
	return 0;
}


static struct odin_dss_driver slimport_dsi_driver = {
	.probe		= slimport_dsi_probe,
	.remove		= __exit_p(slimport_dsi_remove),

	.enable		= slimport_dsi_enable,
	.disable	= slimport_dsi_disable,
	.suspend	= slimport_dsi_suspend,
	.resume		= slimport_dsi_resume,

	.set_update_mode = slimport_dsi_set_update_mode,
	.get_update_mode = slimport_dsi_get_update_mode,

	.get_resolution	= slimport_dsi_get_resolution,
	.get_modedb	= slimport_dsi_get_modedb,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= slimport_dsi_get_timings,
	.set_timings	= slimport_dsi_set_timings,
	.check_timings	= slimport_dsi_check_timings,
	.set_fb_info	= slimport_dsi_set_fb_info,
	.driver         = {
		.name   = "dsi_slimport_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init slimport_dsi_init(void)
{
	odin_dss_register_driver(&slimport_dsi_driver);

	return 0;
}

static void __exit slimport_dsi_exit(void)
{
	odin_dss_unregister_driver(&slimport_dsi_driver);
}

module_init(slimport_dsi_init);
module_exit(slimport_dsi_exit);

MODULE_AUTHOR("Jongmyung Park <jongmyung.park@lge.com>");
MODULE_DESCRIPTION("SLIMPORT Dummy Driver");
MODULE_LICENSE("GPL");

