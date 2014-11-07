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

#define SLIMPORT_HDMI_LCD	0
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
#include "../hdmi/hdmi.h"
#include "../hdmi_api_tx/api/api.h"
#include <linux/odin_mailbox.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>

enum {
	PANEL_LGHDK,
};

static struct {
	struct mutex hdmi_lock;
	struct switch_dev hpd_switch;
} hdmi;

#define HDMI_EDID_MAX_LENGTH	256


#define SLIMPORT_HDMI_DEBUG
#ifdef SLIMPORT_HDMI_DEBUG
#define SLIMPORT_HDMI_DEBUG_INFO(fmt, args...) \
	pr_info("[SLIMPORT_HDMI]: %s: " fmt, __func__, ##args)
#else
#define SLIMPORT_HDMI_DEBUG_INFO(fmt, args...)  do { } while (0)
#endif
#define SLIMPORT_HDMI_DEBUG_ERR(fmt, args...) \
	pr_err("[SLIMPORT_HDMI]: %s: " fmt, __func__, ##args)

static int current_event;

struct odin_dss_device *hdmidev_save;

struct hdmi_cb_work {
	struct work_struct work;
	int status;
};

static struct hdmi_cb_work hdmi_work;

static struct workqueue_struct *hdmi_wkq;

static struct panel_config panel_configs[] = {
	{
		.name		= "hdmi_slimport_panel",
		.type		= PANEL_LGHDK,	/* below values are useless */
		.timings	= {
			.x_res		= 1920,
			.y_res		= 1080,

			.mPixelRepetitionInput = 1,
			.mPixelClock = 7417,
			.mInterlaced = 0,

			.mHActive = 1920,
			.mHBlanking = 370,
			.mHImageSize = 16,
			.mHSyncOffset = 110,
			.mHSyncPulseWidth = 40,
			.mHSyncPolarity = 1,

			.mVActive = 1080,
			.mVBlanking = 30,
			.mVImageSize = 9,
			.mVSyncOffset = 5,
			.mVSyncPulseWidth = 5,
			.mVSyncPolarity = 1,
				.data_width = 24, /*bit*/
			.vfp = 22,
			.vsw = 10,
			.vbp = 23,
			.hfp = 210,
			.hsw = 20,
			.hbp = 46,
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


struct slimport_hdmi_data {
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

	bool use_hdmi_bl;

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

struct slimport_hdmi_data *g_td;

static DECLARE_COMPLETION(dss_ipc_completion);

static int slimport_hdmi_set_hot_plug_status(
	struct odin_dss_device *dssdev, bool onoff);
int slimport_hdmi_update_panel_config(void);
int slimport_hdmi_read_edid_block(int block, u8 *edid_buf)
{
	return slimport_read_edid_block(block, edid_buf);
}

void hdmi_do_apply(struct work_struct *work)
{
	int i, ret;
	static bool hdmi_vp_status = false;
	struct hdmi_cb_work *wk = container_of(work, typeof(*wk), work);

	printk("hdmi_do_apply %d\n", wk->status);

	if (wk->status == 1)
	{
		printk("wk->status 1\n");
		hdmidev_save->state = ODIN_DSS_DISPLAY_ACTIVE;

		/* hdmi enable */
		if (hdmi_vp_status == false)
		{
			ret = regulator_enable
				(hdmidev_save->clocks.hdmi.hdmi_vp);
			if (ret)
			{
				DSSERR("HDMI_VP PWR_ON Err ret=%d \n", ret);
				return ret;
			}
			hdmi_vp_status = true;
		}

		DSSINFO("pm_qos_update_request 800 start \n");
		pm_qos_update_request(&sync2_mem_qos, 800000);
		DSSINFO("pm_qos_update_request 800 end \n");
		odin_crg_dss_channel_clk_enable(ODIN_DSS_CHANNEL_HDMI, true);
		compliance_start(1);
	}
	else if (wk->status == 0)
	{
		DSSINFO("pm_qos_update_request 0 start \n");
		/*api_avmute(1);*/
		/*api_audiomute(1);*/

		pm_qos_update_request(&sync2_mem_qos, 0);
		DSSINFO("pm_qos_update_request 0 end \n");
		hdmidev_save->state = ODIN_DSS_DISPLAY_DISABLED;
		printk("wk->status 0\n");
		/* hdmi disable */
		if (hdmi_vp_status == true)
		{
			dsscomp_unplug_state_reset(g_td->dssdev);
			ret = regulator_disable
				(hdmidev_save->clocks.hdmi.hdmi_vp);
			if (ret)
			{
				DSSERR("HDMI_VP PWR_OFF Err ret=%d \n", ret);
				return ret;
			}
			hdmi_vp_status = false;
		}
		slimport_hdmi_set_hot_plug_status(g_td->dssdev, 0);

		api_deinitialize();
		/* todo .... hdmi disable*/
	}
	else
	{
		/* error */
	}
}

int slimport_hdmi_power_on(struct panel_config *panel_configs)
{
	int i, r = -1;

	hdmidev_save->panel.timings = panel_configs->timings;

	r = odin_dipc_power_on(hdmidev_save);
	if (r) {
		dev_err(&hdmidev_save->dev, "failed to power on dip-hdmi\n");
		goto err0;
	}

	r = odin_du_power_on(hdmidev_save);
	if (r) {
		dev_err(&hdmidev_save->dev, "failed to power on du-hdmi\n");
		goto err0;
	}

	hdmidev_save->type = ODIN_DISPLAY_TYPE_HDMI;
	hdmidev_save->state = ODIN_DSS_DISPLAY_ACTIVE;
	slimport_hdmi_update_panel_config();
	DSSINFO("odin_hdmi_power_on dssdev_save->type=0x%x\n", hdmidev_save->type);
	slimport_hdmi_set_hot_plug_status(g_td->dssdev, 1);

	return 0;
err0:
	return r;
}


static int slimport_hdmi_check_validation(uint8_t *edid_buf)
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
         SLIMPORT_HDMI_DEBUG_ERR("Bad EDID check sum!\n");
         return -1;
    }
	return 0;
}

static int slimport_hdmi_find_best_resoultion(struct fb_monspecs *specs)
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
		pr_info("*****************number : %d ***************** \n", i);
		vm = &specs->modedb[i];
		odinfb_fb2dss_timings(vm, &dss_timings); /* For Debbuging */
		cur_score =
			dss_timings.x_res * dss_timings.y_res *
			(dss_timings.pixel_clock / 1000);
		if (best_score < cur_score)
		{
			best_score = cur_score;
			best_index = i;
		}
 	}

	return best_index;
}

int slimport_hdmi_update_panel_config(void)
{
	struct odin_video_timings *timings = &hdmidev_save->panel.timings;
	const struct fb_videomode video_mode;

	odinfb_dss2fb_timings(timings, &video_mode);

	fb_videomode_to_var(&g_td->info->var, &video_mode);

	pr_info("fb size %d by %d\n", g_td->info->var.xres,
		g_td->info->var.yres);

	return 0;
}

static int slimport_hdmi_free_modedb(void)
{
	struct fb_monspecs *specs = &g_td->dssdev->panel.monspecs;

	fb_destroy_modedb(specs->modedb);
	return 0;
}

static int slimport_hdmi_set_hot_plug_status(
	struct odin_dss_device *dssdev, bool onoff)
{
	int ret = 0;

	/* reset the HPD event notification */
	/*switch_set_state(&td->sdev, 0);*/
	/* reset HDMI Audio swtich */
	/*switch_set_state(&td->saudiodev, 0);*/
	if (onoff) {
		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_ONLINE);
		SLIMPORT_HDMI_DEBUG_INFO("HDMI HPD: CONNECTED: send ONLINE\n");
		if (ret) {
			SLIMPORT_HDMI_DEBUG_ERR("kobject_uevent %d\n", ret);
		}
		switch_set_state(&g_td->sdev, 1);
		SLIMPORT_HDMI_DEBUG_INFO("Hdmi state switch to %d\n", g_td->sdev.state);
		switch_set_state(&g_td->saudiodev, 1);
		SLIMPORT_HDMI_DEBUG_INFO("hdmi_audio state switched to %d\n" ,
						g_td->saudiodev.state);
	}
	else {
		switch_set_state(&g_td->saudiodev, 0);
		SLIMPORT_HDMI_DEBUG_INFO("hdmi_audio state switched to %d\n",
						g_td->saudiodev.state);
		switch_set_state(&g_td->sdev, 0);
		SLIMPORT_HDMI_DEBUG_INFO("Hdmi state switch to %d\n",
						g_td->sdev.state);
		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_OFFLINE);
		if (ret) {
			SLIMPORT_HDMI_DEBUG_ERR("kobject_uevent %d\n", ret);
		}
		SLIMPORT_HDMI_DEBUG_INFO("HDMI HPD: CONNECTED: send OFFLINE\n");
	}
	return ret;
}
static int slimport_hdmi_enable(struct odin_dss_device *dssdev);
static void slimport_hdmi_disable(struct odin_dss_device *dssdev);

static ssize_t slimport_hdmi_enable_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int en = 0;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &en);

	if (en)
		slimport_hdmi_enable(dssdev);

	odin_set_rd_enable_plane(dssdev->channel, en);

	if (!en)
		slimport_hdmi_disable(dssdev);

	return count;
}

static int slimport_hdmi_enable(struct odin_dss_device *dssdev)
{
	struct slimport_hdmi_data *td = dev_get_drvdata(&dssdev->dev);

	int r = 0;

	printk("slimport_hdmi_enable  slimport_hdmi_enable  slimport_hdmi_enable\n");

	dev_dbg(&dssdev->dev, "enable\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		dev_err(&dssdev->dev, "Calling %s, Slimport NOT disabled\n", __func__);
		r = -EINVAL;
		goto err;
	}

	dssdev->panel.timings = panel_configs->timings;

	/* dssdev->state = ODIN_DSS_DISPLAY_ACTIVE; */ /*move to poweon */

	hdmidev_save = dssdev;

	odin_hdmi_init_platform_driver();

	mutex_unlock(&td->lock);

	return r;
err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&td->lock);
	return r;
}

static void slimport_hdmi_disable(struct odin_dss_device *dssdev)
{
	struct slimport_hdmi_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_SUSPENDED;

	odin_set_rd_enable_plane(dssdev->channel, 0);

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
		NULL, slimport_hdmi_enable_test);

static int slimport_hdmi_handler(
	struct notifier_block *this, unsigned long event,void *ptr)
{
	unsigned long flags;
	int i;

	/*spin_lock_irqsave(&slimport_cbl_det_dsi1_lock, flags);*/

	if (event == 1)
	{
		if (event != current_event)
		{
			SLIMPORT_HDMI_DEBUG_INFO("slimport_hdmi_handler event=%d\n", event);
			current_event = event;
			hdmi_work.status = 1;
			queue_work(hdmi_wkq, &hdmi_work.work);
		}

	}
	else
	{
		if (event != current_event)
		{
			SLIMPORT_HDMI_DEBUG_INFO("slimport_hdmi_handler event=%d\n", event);
			current_event = event;
			flush_workqueue(hdmi_wkq);
			hdmi_work.status = 0;
			queue_work(hdmi_wkq, &hdmi_work.work);
		}
	}

	/*spin_unlock_irqrestore(&slimport_cbl_det_dsi1_lock, flags);*/

	return NOTIFY_DONE;
}

static int odin_hdmi_regulator_get(struct odin_dss_device *dssdev)
{
	int ret;

	dssdev->clocks.hdmi.hdmi_vp = regulator_get(&dssdev->dev,
				"+0.9V_HDMI_VP"); /*max77525_LDO_5 */
	if (IS_ERR(dssdev->clocks.hdmi.hdmi_vp)) {
		ret = PTR_ERR(dssdev->clocks.hdmi.hdmi_vp);
		DSSERR("unable to get regulator - hdmi_vp, error: %d\n", ret);
		return ret;
	}

	return 0;
}

/* register notifier handler */
static struct notifier_block slimport_hdmi_handler_block = {
	.notifier_call  = slimport_hdmi_handler,
};

static struct attribute *slimport_attrs[] = {
	&dev_attr_panel_enable.attr,
	NULL,
};

static struct attribute_group slimport_attr_group = {
	.attrs = slimport_attrs,
};

static int slimport_hdmi_probe(struct odin_dss_device *dssdev)
{
	int r;
	struct slimport_hdmi_data *td;
	struct fb_monspecs *specs = &dssdev->panel.monspecs;

	/*odin_crg_dss_channel_clk_enable(ODIN_DSS_CHANNEL_HDMI, true);*/

	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.timings = panel_configs->timings;

	printk("slimport_hdmi_probe  slimport_hdmi_probe  slimport_hdmi_probe\n");

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	/*hdmidev_save = dssdev;	// temporary ...... enable..... */
	td->dssdev = dssdev;
	mutex_init(&td->lock);

	dev_set_drvdata(&dssdev->dev, td);

	/* Setup notifier */
	atomic_notifier_chain_register(
		&hdmi_hpd_notifier_list, &slimport_hdmi_handler_block);

	/* Initialize hdmi node and register with switch driver */
	td->sdev.name = "hdmi";
	if (switch_dev_register(&td->sdev) < 0)
		SLIMPORT_HDMI_DEBUG_ERR("Hdmi switch registration failed\n");

	/* Initialize the hdmi audio node and register with switch driver */
	td->saudiodev.name = "hdmi_audio";
	if (switch_dev_register(&td->saudiodev) < 0)
		SLIMPORT_HDMI_DEBUG_ERR("hdmi_audio switch registration failed\n");

	odin_hdmi_regulator_get(dssdev);

	g_td = td;

	hdmi_wkq = create_singlethread_workqueue("hdmi_apply_start");

	INIT_WORK(&hdmi_work.work, hdmi_do_apply);

	if (!hdmi_wkq)
	{
		r = -ENOMEM;
		goto err;
	}

	return 0;

/*err_device_create_file:
	sysfs_remove_group(&dssdev->dev.kobj, &slimport_attr_group);*/

err:
	return r;
}

static int slimport_hdmi_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

static int slimport_hdmi_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static void __exit slimport_hdmi_remove(struct odin_dss_device *dssdev)
{
	/* Unregister hdmi node from switch driver */
	switch_dev_unregister(&g_td->sdev);

	/* Unregister hdmi node from switch driver */
	switch_dev_unregister(&g_td->saudiodev);

	blocking_notifier_chain_unregister(
		&hdmi_hpd_notifier_list, &slimport_hdmi_handler_block);

	kfree(g_td);
	return;
}

static int slimport_hdmi_get_modedb(struct odin_dss_device *dssdev,
			   struct fb_videomode *modedb, int modedb_len)
{
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	if (specs->modedb_len < modedb_len)
		modedb_len = specs->modedb_len;
	memcpy(modedb, specs->modedb, sizeof(*modedb) * modedb_len);
	return modedb_len;
}

static void slimport_hdmi_get_resolution(
	struct odin_dss_device *dssdev, u16 *xres, u16 *yres)
{
#if SLIMPORT_HDMI_LCD
	*xres = (u16)panel_configs->timings.x_res;
	*yres = (u16)panel_configs->timings.y_res;
#else
	*xres = (u16)dssdev->panel.timings.x_res;
	*yres = (u16)dssdev->panel.timings.y_res;
#endif
	return;
}

static int slimport_hdmi_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	return 0;
}

static enum odin_dss_update_mode slimport_hdmi_get_update_mode(
		struct odin_dss_device *dssdev)
{
	return 0;
}

static void slimport_hdmi_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{

	*timings = dssdev->panel.timings;
	SLIMPORT_HDMI_DEBUG_INFO("slimport_hdmi_get_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	return;
}

static void slimport_hdmi_set_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	SLIMPORT_HDMI_DEBUG_INFO("slimport_hdmi_set_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	dssdev->panel.timings = *timings;
}

static int slimport_hdmi_check_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	SLIMPORT_HDMI_DEBUG_INFO("slimport_hdmi_check_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	if (memcmp(&dssdev->panel.timings, timings, sizeof(*timings)) == 0)
		return 0;

	SLIMPORT_HDMI_DEBUG_INFO("#### different!!!####\n");
	return -EINVAL;
}

static int slimport_hdmi_set_fb_info(struct odin_dss_device *dssdev,
			struct fb_info *info)
{
	struct slimport_hdmi_data *td = dev_get_drvdata(&dssdev->dev);

	td->info = info;
	return 0;
}


static struct odin_dss_driver slimport_hdmi_driver = {
	.probe		= slimport_hdmi_probe,
	.remove		= __exit_p(slimport_hdmi_remove),

	.enable		= slimport_hdmi_enable,
	.disable	= slimport_hdmi_disable,
	.suspend	= slimport_hdmi_suspend,
	.resume		= slimport_hdmi_resume,

	.set_update_mode = slimport_hdmi_set_update_mode,
	.get_update_mode = slimport_hdmi_get_update_mode,

	.get_resolution	= slimport_hdmi_get_resolution,
	.get_modedb	= slimport_hdmi_get_modedb,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= slimport_hdmi_get_timings,
	.set_timings	= slimport_hdmi_set_timings,
	.check_timings	= slimport_hdmi_check_timings,
	.set_fb_info	= slimport_hdmi_set_fb_info,
	.driver         = {
		.name   = "hdmi_slimport_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init slimport_hdmi_init(void)
{
	odin_dss_register_driver(&slimport_hdmi_driver);

	return 0;
}

static void __exit slimport_hdmi_exit(void)
{
	odin_dss_unregister_driver(&slimport_hdmi_driver);
}

module_init(slimport_hdmi_init);
module_exit(slimport_hdmi_exit);

MODULE_AUTHOR("Jongmyung Park <jongmyung.park@lge.com>");
MODULE_DESCRIPTION("SLIMPORT Dummy Driver");
MODULE_LICENSE("GPL");

