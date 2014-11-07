/*
 * LCD Panel Driver
 * Innolux AT070TN94 RGB LCD driver
 *
 * modified from panel-taal.c
 * jaeky.oh@lge.com
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

#define DSS_SUBSYS_NAME "PANEL"

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
#include "../dss/dipc.h"

/**
 * struct panel_config - panel configuration
 * @name: panel name
 * @type: panel type
 * @timings: panel resolution
 * @sleep: various panel specific delays, passed to msleep() if non-zero
 * @reset_sequence: reset sequence timings, passed to udelay() if non-zero
 * @regulators: array of panel regulators
 * @num_regulators: number of regulators in the array
 */
struct panel_config {
	const char *name;
	int type;

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

	/*struct panel_regulator *regulators;*/
	int num_regulators;
};

enum {
	PANEL_LGHDK,
};

static struct panel_config panel_configs[] = {
/* It should be modified....soon.... */
	{
		.name		= "lms480_panel",
		.type		= PANEL_LGHDK,
		.timings	= {
			.x_res		= 800,
			.y_res		= 480,
			.data_width = ODIN_DATA_WIDTH_24BIT,
			.vfp = 4,
			.vsw = 10,	/* ?? minimum x default ?*/
			.vbp = 12,
			.hfp = 8,
			.hsw = 41, 	/*?? minumum x default ? hsync_width*/
			.hbp = 45,
			.rgb_swap = 1,
			.vsync_pol = 1,
			.hsync_pol = 1,
			.blank_pol = 0,
			/* I think it shall be 0 cause it latch data at falling edge...but....?*/
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

struct lms480_data {
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

	struct panel_config *panel_config;
};


static inline struct odin_rgb_panel_data
*get_panel_data(const struct odin_dss_device *dssdev)
{
	return (struct odin_rgb_panel_data *) dssdev->data;
}

static void lms480_esd_work(struct work_struct *work);
#if 0
static void lms480_ulps_work(struct work_struct *work);

static int lms480_sleep_in(struct lms480_data *td)

{
	return 0;
}

static int lms480_sleep_out(struct lms480_data *td)
{
	return 0;
}
#endif



static int lms480_wake_up(struct odin_dss_device *dssdev)
{
	return 0;
}

static int lms480_bl_update_status(struct backlight_device *dev)
{
	return 0;
}

static int lms480_bl_get_intensity(struct backlight_device *dev)
{
	return 0;
}

static const struct backlight_ops lms480_bl_ops = {
	.get_brightness = lms480_bl_get_intensity,
	.update_status  = lms480_bl_update_status,
};

static void lms480_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void lms480_get_resolution(struct odin_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	struct lms480_data *td = dev_get_drvdata(&dssdev->dev);

	DSSDBG("lms480_get_resolution\n");

	if (td->rotate == 0 || td->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}
}


static void lms480_hw_reset(struct odin_dss_device *dssdev)
{

#if 0
	if (panel_data->reset_gpio == -1)
		return;

	gpio_request(panel_data->reset_gpio, "lcd_reset");
	gpio_direction_output(panel_data->reset_gpio, 1);

	gpio_set_value(panel_data->reset_gpio, 1);
	if (td->panel_config->reset_sequence.high)
		udelay(td->panel_config->reset_sequence.high);
	/* reset the panel */
	gpio_set_value(panel_data->reset_gpio, 0);
	/* assert reset */
	if (td->panel_config->reset_sequence.low)
		udelay(td->panel_config->reset_sequence.low);
	gpio_set_value(panel_data->reset_gpio, 1);
	/* wait after releasing reset */
	if (td->panel_config->sleep.hw_reset)
		msleep(td->panel_config->sleep.hw_reset);
#endif
}

static int lms480_probe(struct odin_dss_device *dssdev)
{
	struct lms480_data *td;
	struct odin_rgb_panel_data *panel_data = get_panel_data(dssdev);
	int r;

	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.timings = panel_configs->timings;

	dssdev->panel.width_in_um = 56000; /* physical dimension in um */
	dssdev->panel.height_in_um = 99000; /* physical dimension in um */

	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	td->panel_config = panel_configs;
	td->esd_interval = panel_data->esd_interval;

	mutex_init(&td->lock);

	atomic_set(&td->do_update, 0);

	td->workqueue = create_singlethread_workqueue("lms480_panel_esd");
	if (td->workqueue == NULL) {
		dev_err(&dssdev->dev, "can't create ESD workqueue\n");
		r = -ENOMEM;
		goto err_wq;
	}
	/*INIT_DELAYED_WORK_DEFERRABLE(&td->esd_work, lms480_esd_work);*/
	INIT_DEFERRABLE_WORK(&td->esd_work, lms480_esd_work);

	dev_set_drvdata(&dssdev->dev, td);

	DSSINFO("lms480_probe\n");

	return 0;
err_wq:
err:
	kfree(td);
	return -1;
}

static void lms480_remove(struct odin_dss_device *dssdev)
{

}

static int lms480_power_on(struct odin_dss_device *dssdev)
{
	struct lms480_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	r = odin_dipc_power_on(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to power on dip\n");
		goto err0;
	}

	r = odin_du_power_on(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to power on du\n");
		goto err0;
	}

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
		if (r)
			goto err;
	}

	lms480_hw_reset(dssdev);

	td->enabled = 1;
	return 0;

err:
	dev_err(&dssdev->dev, "error while enabling panel, issuing HW reset\n");
	lms480_hw_reset(dssdev);
	r = odin_dipc_power_off(dssdev);
err0:
	return r;
}

static void lms480_power_off(struct odin_dss_device *dssdev)
{
	struct lms480_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

#if 0
	/* reset  the panel */
	if (panel_data->reset_gpio)
		gpio_set_value(panel_data->reset_gpio, 0);
#endif

	/* disable lcd ldo */
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	r = odin_dipc_power_off(dssdev);

	td->enabled = 0;

}

#if 0
static int lms480_panel_reset(struct odin_dss_device *dssdev)
{
	dev_err(&dssdev->dev, "performing LCD reset\n");

	lms480_power_off(dssdev);
	lms480_hw_reset(dssdev);
	return lms480_power_on(dssdev);
}
#endif

static int lms480_enable(struct odin_dss_device *dssdev)
{
	struct lms480_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev, "enable\n");
	DSSDBG("lms480_enable\n");

	mutex_lock(&td->lock);

	/*dsi_bus_lock(dssdev); semaphore for mipi or RGB*/

	r = lms480_power_on(dssdev);

	/*dsi_bus_unlock(dssdev);*/

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	return 0;
err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&td->lock);
	return r;

return 0;
}

static void lms480_disable(struct odin_dss_device *dssdev)
{
	struct lms480_data *td = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	mutex_lock(&td->lock);

	/*dsi_bus_lock(dssdev);*/

	if (dssdev->state == ODIN_DSS_DISPLAY_ACTIVE) {
		int r;

		r = lms480_wake_up(dssdev);
		if (!r)
			lms480_power_off(dssdev);
	}

	/*dsi_bus_unlock(dssdev);*/

	dssdev->state = ODIN_DSS_DISPLAY_DISABLED;

	mutex_unlock(&td->lock);

}

static int lms480_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static int lms480_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

#if 0
static void lms480_ulps_work(struct work_struct *work)
{
}
#endif

static void lms480_esd_work(struct work_struct *work)
{

}

static int lms480_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_AUTO)
		return -EINVAL;
	return 0;
}

static enum odin_dss_update_mode lms480_get_update_mode(
		struct odin_dss_device *dssdev)
{
	return ODIN_DSS_UPDATE_AUTO;
}

static struct odin_dss_driver lms480_driver = {
	.probe		= lms480_probe,
	.remove		= lms480_remove,

	.enable		= lms480_enable,
	.disable	= lms480_disable,
	.suspend	= lms480_suspend,
	.resume		= lms480_resume,

	.set_update_mode = lms480_set_update_mode,
	.get_update_mode = lms480_get_update_mode,

	.get_resolution	= lms480_get_resolution,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= lms480_get_timings,

	.driver         = {
		.name   = "lms480_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init lcd_panel_lms480_init(void)
{
	odin_dss_register_driver(&lms480_driver);

	return 0;
}

static void __exit lcd_panel_lms480_exit(void)
{
	odin_dss_unregister_driver(&lms480_driver);
}

module_init(lcd_panel_lms480_init);
module_exit(lcd_panel_lms480_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");

