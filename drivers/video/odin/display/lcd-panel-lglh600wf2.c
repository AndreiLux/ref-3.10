/*
 * LG ODIN DSI Video & Command Mode Panel Driver
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
#include "../dss/cabc/cabc-sysfs.h"
#include "lcd-panel-lglh600wf2.h"
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>

#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/odin_dfs.h>

#if defined(CONFIG_MACH_ODIN_LIGER)
#include <trace/events/cpufreq_interactive.h>
#include <linux/board_lge.h>
#endif

#define LCD_RESET_N 2
#define DSV_EN	    29
#define DSV_AVDD_ENP 80
#define DSV_AVEE_ENN 81

#if 1
#define MIPI_DSI_CMD_MODE
#endif

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
		.name		= "lglh600wf2_panel",
		.type		= PANEL_LGHDK,
#ifdef MIPI_DSI_CMD_MODE
		.dsi_mode	= ODIN_DSS_DSI_CMD_MODE,
#else
		.dsi_mode	= ODIN_DSS_DSI_VIDEO_MODE,
#endif
		.timings	= {
			.x_res	  = 1080,
			.y_res	  = 1920,
			.height	  = 132, // 132.48 mm
			.width	  = 74,  // 74.52 mm
			/* mipi clk(888MHz) * 4lanes / 24bits (KHz)*/
			.pixel_clock = 148000, /*151552, 148500,*/
			.data_width = 24,
#ifdef MIPI_DSI_CMD_MODE
			/* fake sync_width, fp, vp for refresh rate.
			   this is not used in real operation*/
			.hsw = 104,/*4,*/ /*hsync_width*/
			.hbp = 50,
			.hfp = 106, /*100,*/
			.vsw = 101,/*1,*/
			.vfp = 4,
			.vbp = 14, /* 4 + 0xA */
#else
			.hsw = 4, /*hsync_width*/
			.hbp = 50,
			.hfp = 100,
			.vsw = 1,
			.vfp = 4,
#if 0
			.vbp = 25, /* 4 + 0x15 */
#else
			.vbp = 14, /* 4 + 0xA */
#endif
#endif
			.vsync_pol = 1,
			.hsync_pol = 1,
			.blank_pol = 0,
			.pclk_pol  = 0,
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

struct lglh600wf2_dsi_data {
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

static bool lcd_cabc_onoff = true; //default value is true

extern void crg_set_irq_mask_all(u8 enable);

static int lglh600wf2_power_control(uint8_t on_off)
{
	int ret = 0;
	static int init_done = 0;

	if (!init_done) {
		ret = gpio_request(DSV_AVDD_ENP , "DSV_AVDD_ENP ");
		mdelay(1);
		if (ret) {
			pr_err("%s: failed to request DSV_AVDD_ENP gpio.\n", __func__);
			goto out;
		}
		ret = gpio_request(DSV_AVEE_ENN  , "DSV_AVEE_ENN  ");
		mdelay(1);
		if (ret) {
			pr_err("%s: failed to request DSV_AVEE_ENN gpio.\n", __func__);
			goto out;
		}
		ret = gpio_direction_output(DSV_AVDD_ENP, 1);
		mdelay(1);
		if (ret) {
			pr_err("%s: failed to set DSV_EN direction.\n", __func__);
			goto err_gpio;
		}
		ret = gpio_direction_output(DSV_AVEE_ENN, 1);
		mdelay(1);
		if (ret) {
			pr_err("%s: failed to set DSV_EN direction.\n", __func__);
			goto err_gpio;
		}
		init_done = 1;
	}

	/* DSV(5.0V SVSP/VSN) Control. */
	if (on_off == 1) {
		gpio_set_value(DSV_AVDD_ENP, on_off);
		mdelay(5); //1ms or more after AVDD ON
		gpio_set_value(DSV_AVEE_ENN, on_off);
		mdelay(20);
		goto out;

	} else if (on_off == 0) {
		gpio_set_value(DSV_AVEE_ENN, on_off);
		mdelay(5); //1ms or more after AVDD ON
		gpio_set_value(DSV_AVDD_ENP, on_off);
		mdelay(20);
		goto out;
	}

err_gpio:
	gpio_free(DSV_EN);
out:
	return ret;
}

static int lglh600wf2_reset_control(uint8_t value)
{
	int ret = 0;
	static int init_done = 0;

	if (!init_done) {
		ret = gpio_request(LCD_RESET_N, "LCD_RESET_N");
		if (ret) {
			pr_err("%s: failed to request LCD RESET gpio.\n", __func__);
			goto out;
		}
		ret = gpio_direction_output(LCD_RESET_N, 1);
		if (ret) {
			pr_err("%s: failed to set LCD RESET direction.\n", __func__);
			goto err_gpio;
		}
		init_done = 1;
	}

	if (value) {
		gpio_set_value(LCD_RESET_N, 1);
		mdelay(20);
		gpio_set_value(LCD_RESET_N, 0);
		mdelay(20);
		gpio_set_value(LCD_RESET_N, 1);
		mdelay(20);
	}
	else {
		gpio_set_value(LCD_RESET_N, 0);
		mdelay(5);
	}
	goto out;

err_gpio:
	gpio_free(LCD_RESET_N);
out:
	return ret;
}

static int lglh600wf2_dsi_panel_on(enum odin_mipi_dsi_index resource_index,
				struct odin_dss_device *dssdev)
{
	int ret;

	ret = lglh600wf2_power_control(1);
	if (ret < 0) {
		pr_err("%s: panel's power on is failed.\n", __func__);
		goto err;
	}
	/* Panel Reset HIGH */
	ret = lglh600wf2_reset_control(1);
	if (ret < 0) {
		pr_err("%s: panel's reset is failed.\n", __func__);
		goto err;
	}

	odin_mipi_dsi_panel_config(resource_index, dssdev);
	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);

	/* Panel DCS write */
	ret = odin_mipi_dsi_cmd(resource_index, lglh600wf2_dsi_on_command,
					ARRAY_SIZE(lglh600wf2_dsi_on_command));
	if(ret < 0) {
		pr_err("%s: dsi command write is failed.\n", __func__);
		goto err;
	}

	if (lge_get_board_revno() >= HW_REV_J)
		lcd_cabc_onoff = true;

	if(lcd_cabc_onoff == true) {
		/* Panel DCS write */
		ret = odin_mipi_dsi_cmd(resource_index,
				lglh600wf2_dsi_bl_cabc_on_command,
				ARRAY_SIZE(lglh600wf2_dsi_bl_cabc_on_command));
		if(ret < 0) {
			pr_err("%s: dsi command write is failed.\n", __func__);
			goto err;
		}
	} else {
		/* Panel DCS write */
		ret = odin_mipi_dsi_cmd(resource_index,
				lglh600wf2_dsi_bl_cabc_off_command,
				ARRAY_SIZE(lglh600wf2_dsi_bl_cabc_off_command));
		if(ret < 0) {
			pr_err("%s: dsi command write is failed.\n", __func__);
			goto err;
		}
	}

	/* Panel DCS write */
	ret = odin_mipi_dsi_cmd(resource_index,
				lglh600wf2_dsi_power_on_command,
				ARRAY_SIZE(lglh600wf2_dsi_power_on_command));
	if(ret < 0) {
		pr_err("%s: dsi command write is failed.\n", __func__);
		goto err;
	}
	return 0;
err:
	return ret;
}

static int lglh600wf2_dsi_panel_off(enum odin_mipi_dsi_index resource_index,
				struct odin_dss_device *dssdev)
{
	int ret;

	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
		odin_mipi_hs_cmd_mode_stop(resource_index, dssdev);
	else
		odin_mipi_hs_video_mode_stop(resource_index, dssdev);

	/* Panel DCS write */
	ret = odin_mipi_dsi_cmd(resource_index, lglh600wf2_dsi_off_command,
				ARRAY_SIZE(lglh600wf2_dsi_off_command));
	if(ret < 0) {
		pr_err("%s: dsi command write is failed.\n", __func__);
		return ret;
	}

	odin_mipi_dsi_phy_power(resource_index, false);

	/* Panel Reset LOW */
	ret = lglh600wf2_reset_control(0);
	if (ret < 0) {
		pr_err("%s: panel's reset is failed.\n", __func__);
		return ret;
	}
	/* Panel Power Down */
	ret = lglh600wf2_power_control(0);
	if (ret < 0) {
		pr_err("%s: panel's power off is failed.\n", __func__);
		return ret;
	}

	return 0;
}

#if 0
static void lglh600wf2_dsi_ulps_work(struct work_struct *work)
{
	/* ToDo */
}

static void lglh600wf2_dsi_esd_work(struct work_struct *work)
{
	/* ToDo */
}
#endif

static int lglh600wf2_dsi_update(struct odin_dss_device *dssdev,
					u16 x, u16 y, u16 h, u16 w){

	/*struct lglh600wf2_dsi_data *data = dev_get_drvdata(&dssdev->dev);*/

	odin_mipi_hs_command_update_loop(0, dssdev);

	return 0;
}

static int lglh600wf2_dsi_sync(struct odin_dss_device *dssdev) {

	odin_mipi_hs_command_sync(dssdev->channel);

	return 0;
}

static int lglh600wf2_dsi_enable(struct odin_dss_device *dssdev)
{
	int ret = 0;
	struct lglh600wf2_dsi_data *td = dev_get_drvdata(&dssdev->dev);

	pr_info("%s: started\n", __func__);

#if 1 /*def RUNTIME_CLK_GATING_LOG*/
	check_clk_gating_status();
#endif
	mutex_lock(&td->lock);

	/* Skip lcd initial when the kernel booting. */
	if (dssdev->boot_display)
	{
		dssdev->boot_display = false;
		if (dssdev->state != ODIN_DSS_DISPLAY_DISCONNECTED)
			dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;
		dssdev->manager->mgr_flip =
			(~(dssdev->panel.flip_supported) & (dssdev->panel.flip));
		dssdev->manager->frame_skip = false;

		/* sweep trash IRQs*/
		odin_crg_set_channel_int_mask(dssdev, false);

		odin_mipi_dsi_set_hs_cmd_size(dssdev);

		/* masking MIPI0 TX UNDERRUN ERROR IRQ */
		odin_crg_set_err_mask(dssdev, true);

		mutex_unlock(&td->lock);
		pr_info("%s: skip the enable routine.\n", __func__);

		return 0;
	}

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		ret = -EINVAL;
		goto err;
	}

	odin_crg_dss_channel_clk_enable(dssdev->channel, true);

	odin_set_rd_enable_plane(dssdev->channel, 0);

	odin_du_display_init(dssdev);

	odin_dipc_display_init(dssdev);

	odin_mipi_dsi_init(dssdev);

	odin_mipi_dsi_set_hs_cmd_size(dssdev);

	ret = odin_du_power_on(dssdev); /* default: gra0 clk enable */
	if (ret) {
		dev_err(&dssdev->dev, "failed to power on du\n");
		goto err;
	}

	ret = lglh600wf2_dsi_panel_on(dssdev->channel, dssdev);

	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
		odin_mipi_hs_cmd_mode(dssdev->channel, dssdev);
	else
		odin_mipi_hs_video_mode(dssdev->channel, dssdev);

/*	dsscomp_refresh_syncpt_value(dssdev);*/
#if RUNTIME_CLK_GATING
	disable_runtime_clk (dssdev->manager->id);
#endif
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	pr_info("%s: finished\n", __func__);

	return 0;
err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&td->lock);
	return ret;

}

static void lglh600wf2_dsi_disable(struct odin_dss_device *dssdev)
{
	struct lglh600wf2_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int ret = 0;

	pr_info("%s: start.\n", __func__);

	mutex_lock(&td->lock);
	if (dssdev->state == ODIN_DSS_DISPLAY_ACTIVE) {

		dssdev->state = ODIN_DSS_DISPLAY_DISABLED;
#if RUNTIME_CLK_GATING
		enable_runtime_clk(dssdev->manager, false);
#endif
		dsscomp_complete_workqueue(dssdev, true, true);

		ret =lglh600wf2_dsi_panel_off(dssdev->channel, dssdev);
		if (ret < 0) {
			pr_err("%s: failed to panel_off.\n", __func__);
#if RUNTIME_CLK_GATING
			release_clk_status(1);
#endif
			goto err;
		}

		ret = odin_du_power_off(dssdev);
		if (ret < 0) {
			pr_err("%s: failed to du_power_off.\n", __func__);
#if RUNTIME_CLK_GATING
			release_clk_status(1);
#endif
			goto err;
		}

		odin_crg_dss_channel_clk_enable(dssdev->channel, false);

		odin_mipi_dsi_unregister_irq(dssdev);

		odin_crg_set_channel_int_mask(dssdev, false);

		pm_runtime_put_sync(&dssdev->dev);

#if RUNTIME_CLK_GATING
		release_clk_status(1);
#endif
		mutex_unlock(&td->lock);

#if 1 /*def RUNTIME_CLK_GATING_LOG*/
		check_clk_gating_status();
#endif
		pr_info("%s: finished\n", __func__);
		return;
	}
err:
	mutex_unlock(&td->lock);
	return;
}

static int lglh600wf2_dsi_suspend(struct odin_dss_device *dssdev)
{
	struct lglh600wf2_dsi_data *td = dev_get_drvdata(&dssdev->dev);
	int ret;

	pr_info("%s: started\n", __func__);

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		ret = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_SUSPENDED;
#if RUNTIME_CLK_GATING
	enable_runtime_clk(dssdev->manager, false);
#endif
	dsscomp_complete_workqueue(dssdev, true, true);

	ret =lglh600wf2_dsi_panel_off(dssdev->channel, dssdev);
	if (ret < 0) {
		pr_err("%s: failed to panel_off.\n", __func__);
#if RUNTIME_CLK_GATING
		release_clk_status(1);
#endif
		goto err;
	}

	ret = odin_du_power_off(dssdev);
	if (ret < 0) {
		pr_err("%s: failed to du_power_off.\n", __func__);
#if RUNTIME_CLK_GATING
		release_clk_status(1);
#endif
		goto err;

	}

	odin_crg_dss_channel_clk_enable(dssdev->channel, false);

	odin_mipi_dsi_unregister_irq(dssdev);

#ifdef DSS_PD_CRG_DUMP_LOG
	printk("==>before_store)crg mask:0x%x, core_clk:0x%x, other_clk:0x%x\n",
		odin_crg_get_register(0xf804, 0), odin_crg_get_register(0x0004, 1),
		odin_crg_get_register(0x0008, 1));
#endif

	pm_runtime_put_sync(&dssdev->dev);

#if RUNTIME_CLK_GATING
	release_clk_status(1);
#endif
	mutex_unlock(&td->lock);

#if 1 /*def RUNTIME_CLK_GATING_LOG*/
	check_clk_gating_status();
#endif
	pr_info("%s: finished\n", __func__);

#if defined(CONFIG_MACH_ODIN_LIGER)
	cpufreq_gov_early_suspend();
#endif

	return 0;
err:
	mutex_unlock(&td->lock);
	return ret;
}

static int lglh600wf2_dsi_resume(struct odin_dss_device *dssdev)
{
	int ret;
	struct lglh600wf2_dsi_data *td = dev_get_drvdata(&dssdev->dev);

#if defined(CONFIG_MACH_ODIN_LIGER)
	cpufreq_gov_late_resume();
#endif

	pr_info("%s: started\n", __func__);
#if 1 /*def RUNTIME_CLK_GATING_LOG*/
	check_clk_gating_status();
#endif
	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_SUSPENDED) {
		ret = -EINVAL;
		goto err;
	}

	pm_runtime_get_sync(&dssdev->dev);
#ifdef DSS_PD_CRG_DUMP_LOG
	printk("==> after_restore)crg mask:0x%x, core clk:0x%x, other_clk:0x%x\n",
		odin_crg_get_register(0xf804, 0), odin_crg_get_register(0x0004, 1),
		odin_crg_get_register(0x0008, 1));
#endif

	odin_crg_dss_channel_clk_enable(dssdev->channel, true);

	odin_du_plane_init();

	odin_set_rd_enable_plane(dssdev->channel, 0);

	odin_du_display_init(dssdev);

	odin_dipc_display_init(dssdev);

	odin_mipi_dsi_init(dssdev);

	odin_mipi_dsi_set_hs_cmd_size(dssdev);

	/* lcd panel enable */
	ret = odin_du_power_on(dssdev);
	if (ret < 0) {
		pr_err("%s: failed to du power on.\n", __func__);
		goto err;
	}

	ret = lglh600wf2_dsi_panel_on(dssdev->channel, dssdev);
	if (ret < 0) {
		pr_err("%s: failed to panel on.\n", __func__);
		goto err;
	}

	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
	{
		odin_mipi_hs_cmd_mode(dssdev->channel, dssdev);
		dssdev->manager->blank(dssdev->manager, true);
	}
	else
		odin_mipi_hs_video_mode(dssdev->channel, dssdev);
/*
	dsscomp_refresh_syncpt_value(dssdev);*/
#if RUNTIME_CLK_GATING
	disable_runtime_clk (dssdev->manager->id);
#endif
	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	pr_info("%s: finished\n", __func__);

	return ret;
err:
	mutex_unlock(&td->lock);
	return ret;
}

static ssize_t show_lcd_cabc_onoff( struct device *dev,
				    struct device_attribute *attr, char *buf )
{
	if ( lcd_cabc_onoff == false )
		pr_info( "LCD CABC disable~!\n"   );
	else if ( lcd_cabc_onoff == true )
		pr_info( "LCD CABC enable~!\n"   );

	return 0;
}

static ssize_t store_lcd_cabc_onoff( struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count )
{
    if ( (*(buf+0)=='o') && (*(buf+1)=='n') )
    {
        lcd_cabc_onoff = true;
        pr_info( "LCD CABC Turn ON\n" );
    }

    else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') )
    {
        lcd_cabc_onoff = false;
        pr_info( "LCD CABC Turn OFF\n" );
    }
    else
    {
        pr_info( "what?" );
    }
    return count;

}

static DEVICE_ATTR( lcd_cabc_onoff, 0644,
		    show_lcd_cabc_onoff,
		    store_lcd_cabc_onoff );

static ssize_t lglh600wf2_dsi_enable_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int on_off;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &on_off);

	if (on_off==1)
	{
		lglh600wf2_dsi_enable(dssdev);
		odin_set_rd_enable_plane(dssdev->channel, 1);
	}
	else
	{
		odin_set_rd_enable_plane(dssdev->channel, 0);
		lglh600wf2_dsi_disable(dssdev);
	}

	return count;
}

static ssize_t lglh600wf2_dsi_resume_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int on_off;
	struct odin_dss_device *dssdev =
		container_of(dev, struct odin_dss_device, dev);

	sscanf(buf, "%d", &on_off);

	if (on_off==1)
	{
		lglh600wf2_dsi_resume(dssdev);
		odin_set_rd_enable_plane(dssdev->channel, 1);
	}
	else
	{
		odin_set_rd_enable_plane(dssdev->channel, 0);
		lglh600wf2_dsi_suspend(dssdev);
	}

	return count;
}

static DEVICE_ATTR(panel_enable, S_IWUSR,
		NULL, lglh600wf2_dsi_enable_test);
static DEVICE_ATTR(panel_resume, S_IWUSR,
		NULL, lglh600wf2_dsi_resume_test);

static struct attribute *lglh600wf2_attrs[] = {
	&dev_attr_panel_enable.attr,
	&dev_attr_panel_resume.attr,
	NULL,
};

static struct attribute_group lglh600wf2_attr_group = {
	.attrs = lglh600wf2_attrs,
};


static int lglh600wf2_dsi_probe(struct odin_dss_device *dssdev)
{
	int ret, r;
	struct lglh600wf2_dsi_data *td;
	struct device *lcd_cabc;

	if (lge_get_lcd_connect()== LGE_LCD_CONNECT) {
		pr_info("lge_get_lcd_connect : connect \n");
	} else {
		dssdev->state = ODIN_DSS_DISPLAY_DISCONNECTED;
		pr_info("lge_get_lcd_connect : disconnect \n");
	}

	enum odin_mipi_dsi_index resource_index =
		(enum odin_mipi_dsi_index) dssdev->channel;

	dssdev->panel.dsi_mode = panel_configs->dsi_mode;
	dssdev->panel.config   = ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF |
				 ODIN_DSS_LCD_RF ;
	dssdev->panel.timings  = panel_configs->timings;
	dssdev->panel.dsi_pix_fmt = ODIN_DSI_OUTFMT_RGB24;

	/* H_FLIP & V_FLIP Support */
	dssdev->panel.flip = 0x00;
	dssdev->panel.flip_supported = ODIN_DSS_H_FLIP;

	dssdev->phy.dsi.lane_num = ODIN_DSI_4LANE;

	pr_info("%s: started\n", __func__);

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		ret = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	mutex_init(&td->lock);

#if 0
	pm_qos_add_request(&td->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ,
			PM_QOS_ODIN_MEM_MIN_FREQ_DEFAULT_VALUE);
#endif

	dev_set_drvdata(&dssdev->dev, td);

	if (resource_index > ODIN_DSI_CH1 || resource_index < ODIN_DSI_CH0) {
		ret = -EINVAL;
		goto err;
	}

	ret = sysfs_create_group(&dssdev->dev.kobj, &lglh600wf2_attr_group);
	if (ret) {
		pr_err("%s: failed to create sysfs files\n", __func__);
		goto err_device_create_file;
	}

	lcd_cabc = cabc_device_register(&dssdev->dev, "lcd_cabc");
#if 0
    if ( IS_ERR( lcd_cabc ) )
    {
        return PTR_ERR( lcd_cabc );
    }
#endif
	ret = device_create_file( lcd_cabc, &dev_attr_lcd_cabc_onoff );
	if ( ret != 0 ) {
		dev_err(&dssdev->dev, "Error creating sysfs files \n");
		goto err;
	}

	r = odin_pd_register_dev(&dssdev->dev, &odin_pd_dss3_lcd0_panel);
	if (r < 0)
	{
		dev_err(&dssdev->dev, "failed to register power domain\n");
		goto err;
	}

	pm_runtime_get_noresume(&dssdev->dev);
	pm_runtime_set_active(&dssdev->dev);
	pm_runtime_enable(&dssdev->dev);

	pr_info("%s: finished\n", __func__);

	return 0;

err_device_create_file:
	sysfs_remove_group(&dssdev->dev.kobj, &lglh600wf2_attr_group);
err:
	return ret;
}

static void __exit lglh600wf2_dsi_remove(struct odin_dss_device *dssdev)
{
	struct lglh600wf2_dsi_data *td = dev_get_drvdata(&dssdev->dev);

	pr_info("%s: started\n", __func__);

	sysfs_remove_group(&dssdev->dev.kobj, &lglh600wf2_attr_group);

	/* ToDo */
	/* reset, to be sure that the panel is in a valid state */

	kfree(td);
}

static void lglh600wf2_dsi_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void lglh600wf2_dsi_get_resolution(
	struct odin_dss_device *dssdev, u16 *xres, u16 *yres)
{
	*xres = (u16)panel_configs->timings.x_res;
	*yres = (u16)panel_configs->timings.y_res;
}

static int lglh600wf2_dsi_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_MANUAL)
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_VIDEO_MODE;
	else
		dssdev->panel.dsi_mode = ODIN_DSS_DSI_CMD_MODE;

	return 0;
}

static enum odin_dss_update_mode lglh600wf2_dsi_get_update_mode(
		struct odin_dss_device *dssdev)
{
	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
		return ODIN_DSS_UPDATE_MANUAL;
	else
		return ODIN_DSS_UPDATE_AUTO;
}

static int lglh600wf2_dsi_flush_pending(struct odin_dss_device *dssdev)
{
	int r = 0;

	r = mipi_hs_command_pending(dssdev);

	return r;
}

static struct odin_dss_driver lglh600wf2_dsi_driver = {
	.probe			= lglh600wf2_dsi_probe,
	.remove			= __exit_p(lglh600wf2_dsi_remove),
	.enable			= lglh600wf2_dsi_enable,
	.disable		= lglh600wf2_dsi_disable,
	.suspend		= lglh600wf2_dsi_suspend,
	.resume			= lglh600wf2_dsi_resume,
	.sync			= lglh600wf2_dsi_sync,
	.update			= lglh600wf2_dsi_update,
	.set_update_mode	= lglh600wf2_dsi_set_update_mode,
	.get_update_mode	= lglh600wf2_dsi_get_update_mode,
	.get_resolution		= lglh600wf2_dsi_get_resolution,
	.get_recommended_bpp	= odindss_default_get_recommended_bpp,
	.get_timings		= lglh600wf2_dsi_get_timings,
	.flush_pending		= lglh600wf2_dsi_flush_pending,

	.driver			= {
		.name		= "dsi_lglh600wf2_panel",
		.owner		= THIS_MODULE,
	},
};

static int __init lglh600wf2_dsi_init(void)
{
	odin_dss_register_driver(&lglh600wf2_dsi_driver);

	return 0;
}

static void __exit lglh600wf2_dsi_exit(void)
{
	odin_dss_unregister_driver(&lglh600wf2_dsi_driver);
}

module_init(lglh600wf2_dsi_init);
module_exit(lglh600wf2_dsi_exit);

MODULE_DESCRIPTION("lglh600wf2 video mode Driver");
MODULE_LICENSE("GPL");
