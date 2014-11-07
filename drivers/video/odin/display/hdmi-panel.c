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
#include <linux/spinlock.h>
#include <video/odindss.h>
#include <linux/slimport.h>
#include <linux/switch.h>

#include "../dss/dss.h"
#include "../dss/du.h"
#include "../hdmi/hdmi.h"
#include "../hdmi/hdmi_debug.h"
#include "../hdmi/compliance.h"
#include "../hdmi_api_tx/api/api.h"
#include "hdmi-panel.h"
#include <linux/odin_mailbox.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/regulator/consumer.h>
#include <linux/odin_pd.h>

#define HDMI_PANEL_LCD	0

enum {
	PANEL_LGHDK,
};


#define HDMI_API_INIT_CNT	2


static int current_event=FALSE;
static int g_api_init_cnt=0;
static int g_play_mode = 0;

static int hdmi_panel_enable(struct odin_dss_device *dssdev);
static void hdmi_panel_disable(struct odin_dss_device *dssdev);
static int hdmi_panel_set_fb_info(struct odin_dss_device *dssdev,
				  struct fb_info *info);
static int hdmi_panel_set_hot_plug_status(
	struct odin_dss_device *dssdev, bool onoff, bool hdmi_mode);

int hdmi_power_off(int powermode);

struct hdmi_cb_work {
	struct work_struct work;
	int status;
};


static struct hdmi_panel_config panel_configs[] = {
	{
		.name		= "hdmi_panel",
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


struct hdmi_panel_data {
	struct mutex lock;
	spinlock_t slock;

	struct odin_dss_device *dssdev;

	struct fb_info *info;
 	struct hdmi_cb_work hdmi_work;
 	struct workqueue_struct *hdmi_panel_wq;
	struct panel_config *panel_config;
	struct switch_dev sdev;
	struct switch_dev saudiodev;
	bool   hdmi_mode;
};

struct hdmi_panel_data *g_td;

static bool hdmi_vp_status = false;

//kyusuk.lee
int hdmi_power_status;

int hdmi_read_edid_block(int block, u8 *edid_buf)
{
	return slimport_read_edid_block(block, edid_buf);
}

void hdmi_do_apply(struct work_struct *work)
{
	int  ret;
	struct hdmi_cb_work *wk = container_of(work, typeof(*wk), work);

	hdmi_debug(LOG_DEBUG,"hdmi_do_apply %d\n", wk->status);
	if (wk->status == 1)
	{
		hdmi_power_status = 2;
		g_td->dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

		pm_runtime_get_sync(&g_td->dssdev->dev);
		/* hdmi enable */
		if (hdmi_vp_status == false)
		{
#if 0
			ret = regulator_enable
					(g_td->dssdev->clocks.hdmi.hdmi_vp);
			if (ret)
			{
				hdmi_err("HDMI_VP PWR_ON Err ret=%d \n", ret);
				return ;
			}
#endif
			hdmi_vp_status = true;
		}

		hdmi_debug(LOG_DEBUG,"pm_qos_update_request 800 set \n");
		pm_qos_update_request(&sync2_mem_qos, 800000);


		hdmi_debug(LOG_DEBUG,"HDMI Clk enable \n");

		odin_crg_dss_channel_clk_enable(ODIN_DSS_CHANNEL_HDMI, true);


		odin_crg_set_channel_int_mask(g_td->dssdev, true);

		odin_ses_hdmi_key_request();

		/* hdmi start send */
		ret = hdmi_start(g_play_mode);
		if(ret != 0)
		{
			hdmi_err("hdmi start error ->  restart\n");
			hdmi_power_off(true);
			mdelay(10);
			if(g_api_init_cnt > HDMI_API_INIT_CNT)
				return;
			current_event = 1;
			g_td->hdmi_work.status = 1;
			g_api_init_cnt++;
			queue_work(g_td->hdmi_panel_wq, &g_td->hdmi_work.work);
		}
	}
	else if (wk->status == 0)
	{
		hdmi_power_off(true);

	}
	else
	{
		hdmi_err
			("hdmi_do_apply(%d)  oops status !!!\n", wk->status);
	}
}

int hdmi_panel_power_on(struct hdmi_panel_config *panel_configs,
			bool hdmi_mode)
{
	int r = -1;
	unsigned long flags;

	g_td->hdmi_mode = hdmi_mode;
	g_td->dssdev->panel.timings = panel_configs->timings;

	hdmi_panel_update_panel_config();

	hdmi_panel_set_hot_plug_status(g_td->dssdev, true, g_td->hdmi_mode);

	dsscomp_refresh_syncpt_value(g_td->dssdev->channel);

	dsscomp_hdmi_free();

	odin_du_display_init(g_td->dssdev);

	hdmi_debug(LOG_DEBUG,"power on mVActive = %d, mHActive = %d\n",
			panel_configs->timings.mVActive , panel_configs->timings.mHActive);

	spin_lock_irqsave(&g_td->slock, flags);
	odin_du_sync_set_act_size(g_td->dssdev->channel,
			panel_configs->timings.mVBlanking+10,
			panel_configs->timings.mHBlanking);


	r = odin_du_power_on(g_td->dssdev);
	if (r) {
		dev_err(&g_td->dssdev->dev, "failed to power on du-hdmi\n");
		goto err0;
	}
	r = odin_dipc_power_on(g_td->dssdev);
	if (r) {
		dev_err(&g_td->dssdev->dev, "failed to power on dip-hdmi\n");
		goto err0;
	}

	odin_du_set_ovl_sync_enable(g_td->dssdev->channel, true);

	g_td->dssdev->type = ODIN_DISPLAY_TYPE_HDMI;
	g_td->dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	spin_unlock_irqrestore(&g_td->slock, flags);
	hdmi_debug(LOG_DEBUG,"odin_hdmi_power_on dssdev_save->type=0x%x\n",
			      g_td->dssdev->type);


	return 0;
err0:
	spin_unlock_irqrestore(&g_td->slock, flags);
	pm_runtime_put_sync(&g_td->dssdev->dev);
	return r;
}

/* call from  hdmi module driver */
int hdmi_panel_power_off(void)
{
	current_event = FALSE;
	g_td->hdmi_work.status = FALSE;
	g_play_mode = 0;
	queue_work(g_td->hdmi_panel_wq, &g_td->hdmi_work.work);

	return 0;

}
int hdmi_panel_power_on_manual(void)
{
	hdmi_debug(LOG_DEBUG,"%s  start\n", __func__);
	current_event = TRUE;
	g_td->hdmi_work.status = TRUE;
	g_play_mode = 1;
	queue_work(g_td->hdmi_panel_wq, &g_td->hdmi_work.work);

	return 0;
}

//kyusuk.lee
int get_hdmi_power_status(void)
{
	return hdmi_power_status;
}
/* call from  hdmi module driver */
int hdmi_power_off(int powermode)
{
	int ret;

	hdmi_power_status = 1; //power off start

	/* hdmi av mute call */
	api_avmute(true);
	api_audiomute(true);

	api_standby();

	if(powermode == true)
	{
		compliance_standby();
		api_deinitialize();
	}

	if (hdmi_vp_status == true)
	{
		if(powermode == true)
		{
#if 0
			ret = regulator_disable
				(g_td->dssdev->clocks.hdmi.hdmi_vp);
			if (ret)
			{
				hdmi_err
					("HDMI_VP PWR_OFF Err ret=%d \n", ret);
				pm_runtime_put_sync(&g_td->dssdev->dev);
				return ret;
			}
#endif
		}
		hdmi_vp_status = false;
	}

	hdmi_panel_disable(g_td->dssdev);

	hdmi_panel_set_hot_plug_status(g_td->dssdev, 0, g_td->hdmi_mode);

	mdelay(1); /*  time delay for rotate view */

	pm_qos_update_request(&sync2_mem_qos, 0);

	odin_crg_set_channel_int_mask(g_td->dssdev, false);

	if(powermode == true)
		odin_crg_dss_channel_clk_enable(g_td->dssdev->channel, false);

	mdelay(1);

	pm_runtime_put_sync(&g_td->dssdev->dev);

	//kyusuk.lee
	hdmi_debug(LOG_DEBUG,"hdmi doing power off\n"); //delay for unstable status
	mdelay(2000);
	hdmi_power_status = 0; //power off complete
	hdmi_debug(LOG_DEBUG,"hdmi power off end\n");
	return 0;

}


#if 0
static int hdmi_panel_check_validation(uint8_t *edid_buf)
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
         hdmi_err("Bad EDID check sum!\n");
         return -1;
    }
	return 0;
}

static int hdmi_panel_find_best_resoultion(struct fb_monspecs *specs)
{
	int i;
	struct fb_videomode *vm;
	struct odin_video_timings dss_timings;
	int best_score = 0;
	u32 cur_score = 0;
	int best_index = 0;

	for (i = 0; i < specs->modedb_len; i++)
	{
		pr_info("******************number : %d *************** \n", i);
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
#endif

int hdmi_panel_update_panel_config(void)
{
	struct odin_video_timings *timings = &g_td->dssdev->panel.timings;
	struct fb_videomode video_mode;

	/* mutex_lock(&g_td->lock); */
	odinfb_dss2fb_timings(timings, &video_mode);

	if(g_td->info == NULL)
		return -1;


	fb_videomode_to_var(&g_td->info->var, &video_mode);

	hdmi_panel_set_fb_info(g_td->dssdev, g_td->info);

	pr_info("fb size %d by %d\n", g_td->info->var.xres,
		g_td->info->var.yres);

	/* mutex_unlock(&g_td->lock); */
	return 0;
}

#if 0
static int hdmi_panel_free_modedb(void)
{
	struct fb_monspecs *specs = &g_td->dssdev->panel.monspecs;

	fb_destroy_modedb(specs->modedb);
	return 0;
}
#endif

static int hdmi_panel_set_hot_plug_status(
	struct odin_dss_device *dssdev, bool onoff, bool hdmi_mode)
{
	int ret = 0;
	int sp_mode=0;

	sp_mode = is_slimport_vga();
	if (onoff) {

		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_ONLINE);
		hdmi_debug(LOG_DEBUG,"HDMI HPD: CONNECTED: send ONLINE\n");
		if (ret) {
			hdmi_err("kobject_uevent %d\n", ret);
		}

		switch_set_state(&g_td->sdev, 1);

		hdmi_debug(LOG_DEBUG,"Hdmi state switch to %d\n",
				      g_td->sdev.state);
		if(g_td->hdmi_mode == true)
		{
			switch_set_state(&g_td->saudiodev, 1);
			hdmi_debug(LOG_DEBUG,"hdmi_audio state switched to %d\n" ,
					g_td->saudiodev.state);
		}
	}
	else {
		if(g_td->hdmi_mode == true)
		{
			switch_set_state(&g_td->saudiodev, 0);
			hdmi_debug(LOG_DEBUG,"hdmi_audio state switched to %d\n",
					g_td->saudiodev.state);
		}
		switch_set_state(&g_td->sdev, 0);
		hdmi_debug(LOG_DEBUG,"Hdmi state switch to %d\n",
						g_td->sdev.state);
		ret = kobject_uevent(&dssdev->dev.kobj, KOBJ_OFFLINE);
		if (ret) {
			hdmi_err("kobject_uevent %d\n", ret);
		}
		hdmi_debug(LOG_DEBUG,"HDMI HPD: CONNECTED: send OFFLINE\n");
	}
	return ret;
}

static int hdmi_panel_enable(struct odin_dss_device *dssdev)
{
	/* struct hdmi_panel_data *td = dev_get_drvdata(&dssdev->dev); */

	return 0;
}

static void hdmi_panel_disable(struct odin_dss_device *dssdev)
{

	struct hdmi_panel_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_SUSPENDED;

	dsscomp_complete_workqueue(g_td->dssdev, false, true);
	dsscomp_unplug_state_reset(g_td->dssdev);

	r = odin_du_power_off(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to du_power_off off du\n");
		goto err;
	}

	odin_crg_dss_reset((1<<19), true);
	udelay(1);
	odin_crg_dss_reset((1<<19), false);




	r = odin_dipc_power_off(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to dipc_power_off off du\n");
		goto err;
	}


	dssdev->state = ODIN_DSS_DISPLAY_DISABLED;

	mutex_unlock(&td->lock);

	return;
err:
	dev_dbg(&dssdev->dev, "hdmi panel disable failed\n");
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;
	mutex_unlock(&td->lock);
	return;
}



static int hdmi_panel_handler(
	struct notifier_block *this, unsigned long event,void *ptr)
{
	if (event == 1)
	{
		hdmi_debug(LOG_DEBUG,"hdmi_panel_handler event=%ld\n", event);
		current_event = event;
		g_td->hdmi_work.status = 1;
		queue_work(g_td->hdmi_panel_wq, &g_td->hdmi_work.work);

	}
	else
	{
		hdmi_debug(LOG_DEBUG,"hdmi_panel_handler event=%ld not work\n", event);
	}


	return NOTIFY_DONE;
}

static int odin_hdmi_regulator_get(struct odin_dss_device *dssdev)
{
	int ret;

	dssdev->clocks.hdmi.hdmi_vp = regulator_get(&dssdev->dev,
				"+0.9V_HDMI_VP"); /*max77525_LDO_5 */
	if (IS_ERR(dssdev->clocks.hdmi.hdmi_vp)) {
		ret = PTR_ERR(dssdev->clocks.hdmi.hdmi_vp);
		hdmi_err("unable to get regulator - hdmi_vp, error: %d\n", ret);
		return ret;
	}

	return 0;
}

/* register notifier handler */
static struct notifier_block hdmi_panel_handler_block = {
	.notifier_call  = hdmi_panel_handler,
};


static int hdmi_panel_probe(struct odin_dss_device *dssdev)
{
	int r;
	struct hdmi_panel_data *td;



	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.timings = panel_configs->timings;

	td = devm_kzalloc(&dssdev->dev, sizeof(struct hdmi_panel_data),
			  GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	mutex_init(&td->lock);

	dev_set_drvdata(&dssdev->dev, td);

	/* Setup notifier */
	atomic_notifier_chain_register(
		&hdmi_hpd_notifier_list, &hdmi_panel_handler_block);

	/* Initialize hdmi node and register with switch driver */
	td->sdev.name = "hdmi";
	if (switch_dev_register(&td->sdev) < 0)
	{
		hdmi_err("Hdmi switch registration failed\n");
	}
	/* Initialize the hdmi audio node and register with switch driver */
	td->saudiodev.name = "hdmi_audio";
	if (switch_dev_register(&td->saudiodev) < 0)
	{
		hdmi_err("hdmi_audio switch registration failed\n");
	}
#if 0
	r = odin_hdmi_regulator_get(dssdev);
	if(r < 0)
	{
		r = -ENOMEM;
		goto err;
	}
#endif
	g_td = td;

	spin_lock_init(&g_td->slock);

	r = odin_pd_register_dev(&g_td->dssdev->dev, &odin_pd_dss3_hdmi_panel);
	if (r < 0)
	{
		    dev_err(&dssdev->dev, "failed to register power domain\n");
			    goto err;
	}

	pm_runtime_enable(&g_td->dssdev->dev);


	g_td->hdmi_panel_wq = create_singlethread_workqueue("hdmi_apply_start");

	INIT_WORK(&g_td->hdmi_work.work, hdmi_do_apply);

	if (!g_td->hdmi_panel_wq)
	{
		r = -ENOMEM;
		goto err;
	}

	return 0;

err:
	if(&g_td->sdev != NULL)
		switch_dev_unregister(&g_td->sdev);
	if(&g_td->saudiodev != NULL)
		switch_dev_unregister(&g_td->saudiodev);


	return r;
}

static int hdmi_panel_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

static int hdmi_panel_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static void __exit hdmi_panel_remove(struct odin_dss_device *dssdev)
{

	/* Unregister hdmi node from switch driver */
	if(&g_td->sdev != NULL)
		switch_dev_unregister(&g_td->sdev);

	if(&g_td->saudiodev != NULL)
		switch_dev_unregister(&g_td->saudiodev);


	atomic_notifier_chain_unregister(
		&hdmi_hpd_notifier_list, &hdmi_panel_handler_block);

	destroy_workqueue(g_td->hdmi_panel_wq);

	return;
}

static int hdmi_panel_get_modedb(struct odin_dss_device *dssdev,
			   struct fb_videomode *modedb, int modedb_len)
{
	struct fb_monspecs *specs = &dssdev->panel.monspecs;
	if (specs->modedb_len < modedb_len)
		modedb_len = specs->modedb_len;
	memcpy(modedb, specs->modedb, sizeof(*modedb) * modedb_len);
	return modedb_len;
}

static void hdmi_panel_get_resolution(
	struct odin_dss_device *dssdev, u16 *xres, u16 *yres)
{
#if HDMI_PANEL_LCD
	*xres = (u16)panel_configs->timings.x_res;
	*yres = (u16)panel_configs->timings.y_res;
#else
	*xres = (u16)dssdev->panel.timings.x_res;
	*yres = (u16)dssdev->panel.timings.y_res;
#endif

	return;
}

static int hdmi_panel_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	return 0;
}

static enum odin_dss_update_mode hdmi_panel_get_update_mode(
		struct odin_dss_device *dssdev)
{
	return 0;
}

static void hdmi_panel_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{

	*timings = dssdev->panel.timings;
	hdmi_debug(LOG_DEBUG,"hdmi_panel_get_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	return;
}

static void hdmi_panel_set_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	hdmi_debug(LOG_DEBUG,"hdmi_panel_set_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	dssdev->panel.timings = *timings;
}

static int hdmi_panel_check_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	hdmi_debug(LOG_DEBUG,"hdmi_panel_check_timings, x_res=%d, y_res=%d\n",
					timings->x_res, timings->y_res);
	if (memcmp(&dssdev->panel.timings, timings, sizeof(*timings)) == 0)
		return 0;

	hdmi_debug(LOG_DEBUG,"#### different!!!####\n");
	return -EINVAL;
}

static int hdmi_panel_set_fb_info(struct odin_dss_device *dssdev,
			struct fb_info *info)
{
	struct hdmi_panel_data *td = dev_get_drvdata(&dssdev->dev);

	td->info = info;
	return 0;
}


static struct odin_dss_driver hdmi_panel_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= __exit_p(hdmi_panel_remove),

	.enable		= hdmi_panel_enable,
	.disable	= hdmi_panel_disable,
	.suspend	= hdmi_panel_suspend,
	.resume		= hdmi_panel_resume,

	.set_update_mode = hdmi_panel_set_update_mode,
	.get_update_mode = hdmi_panel_get_update_mode,

	.get_resolution	= hdmi_panel_get_resolution,
	.get_modedb	= hdmi_panel_get_modedb,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= hdmi_panel_get_timings,
	.set_timings	= hdmi_panel_set_timings,
	.check_timings	= hdmi_panel_check_timings,
	.set_fb_info	= hdmi_panel_set_fb_info,
	.driver         = {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init hdmi_panel_init(void)
{
	odin_dss_register_driver(&hdmi_panel_driver);

	return 0;
}

static void __exit hdmi_panel_exit(void)
{
	odin_dss_unregister_driver(&hdmi_panel_driver);
}

module_init(hdmi_panel_init);
module_exit(hdmi_panel_exit);

MODULE_AUTHOR("Metkvision <metkvision@mtekvision.com>");
MODULE_DESCRIPTION("HDMI Panel Driver");
MODULE_LICENSE("GPL");

