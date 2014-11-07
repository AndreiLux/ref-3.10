/*
 * LCD Panel Driver Template.
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
	PANEL_SAMPLE1,
};

static struct panel_config panel_configs[] = {
	{
		.name		= "sample1_panel",
		.type		= PANEL_SAMPLE1,
		.timings	= {
			.x_res		= 720,
			.y_res		= 1280,
			.vfp = 8,
			.vsw = 2,
			.vbp = 32,
			.hfp = 99,
			.hsw = 4,
			.hbp = 88,
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

struct sample1_data {
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
};

static inline struct odin_dsi_panel_data
*get_panel_data(const struct odin_dss_device *dssdev)
{
	return (struct odin_dsi_panel_data *) dssdev->data;
}

#if 0
static void sample1_esd_work(struct work_struct *work);
static void sample1_ulps_work(struct work_struct *work);

static int sample1_sleep_in(struct sample1_data *td)

{
	return 0;
}

static int sample1_sleep_out(struct sample1_data *td)
{
	return 0;
}



static int sample1_wake_up(struct odin_dss_device *dssdev)
{
	return 0;
}
#endif

static int sample1_bl_update_status(struct backlight_device *dev)
{
	return 0;
}

static int sample1_bl_get_intensity(struct backlight_device *dev)
{
	return 0;
}

static const struct backlight_ops sample1_bl_ops = {
	.get_brightness = sample1_bl_get_intensity,
	.update_status  = sample1_bl_update_status,
};

static void sample1_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void sample1_get_resolution(struct odin_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	struct sample1_data *td = dev_get_drvdata(&dssdev->dev);

	if (td->rotate == 0 || td->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}

	dev_dbg(&dssdev->dev, "timings.x_res = %d, timings.y_res = %d \n",
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);
}

#if 0
static void sample1_hw_reset(struct odin_dss_device *dssdev)
{
	/*struct sample1_data *td = dev_get_drvdata(&dssdev->dev);*/

}
#endif

static int sample1_probe(struct odin_dss_device *dssdev)
{
	struct sample1_data *td;
	struct odin_dsi_panel_data *panel_data = get_panel_data(dssdev);
	struct panel_config *panel_config = NULL;
	int r, i;

	if (!panel_data || !panel_data->name) {
		r = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(panel_configs); i++) {
		if (strcmp(panel_data->name, panel_configs[i].name) == 0) {
			panel_config = &panel_configs[i];
			break;
		}
	}

	if (!panel_config) {
		r = -EINVAL;
		goto err;
	}

	dssdev->panel.config =
		ODIN_DSS_LCD_TFT | ODIN_DSS_LCD_ONOFF | ODIN_DSS_LCD_RF ;
	dssdev->panel.timings = panel_config->timings;

	dssdev->panel.width_in_um = 56000; /* physical dimension in um */
	dssdev->panel.height_in_um = 99000; /* physical dimension in um */


	td = kzalloc(sizeof(*td), GFP_KERNEL);
	if (!td) {
		r = -ENOMEM;
		goto err;
	}

	td->dssdev = dssdev;
	td->panel_config = panel_config;
	td->esd_interval = panel_data->esd_interval;
	td->ulps_enabled = false;
	td->ulps_timeout = panel_data->ulps_timeout;

	mutex_init(&td->lock);

#if 0
	atomic_set(&td->do_update, 0);

	td->workqueue = create_singlethread_workqueue("lh430wv4_panel_panel_esd");
	if (td->workqueue == NULL) {
		dev_err(&dssdev->dev, "can't create ESD workqueue\n");
		r = -ENOMEM;
		goto err_wq;
	}
	INIT_DELAYED_WORK_DEFERRABLE(&td->esd_work, lh430wv4_panel_esd_work);
	INIT_DELAYED_WORK(&td->ulps_work, lh430wv4_panel_ulps_work);
#endif

	dev_set_drvdata(&dssdev->dev, td);

	dev_dbg(&dssdev->dev, "timings.x_res = %d, timings.y_res = %d \n",
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);

	dev_dbg(&dssdev->dev, "probe\n");
err:
	return 0;
}

static void sample1_remove(struct odin_dss_device *dssdev)
{

}

#if 0
static int sample1_power_on(struct odin_dss_device *dssdev)
{
	return 0;
}

static void sample1_power_off(struct odin_dss_device *dssdev)
{
}

static int sample1_panel_reset(struct odin_dss_device *dssdev)
{
	dev_err(&dssdev->dev, "performing LCD reset\n");

	sample1_power_off(dssdev);
	/*sample1_hw_reset(dssdev);*/
	return sample1_power_on(dssdev);
}
#endif

static int sample1_enable(struct odin_dss_device *dssdev)
{
	struct sample1_data *td = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev,"enable\n");
	DSSDBG("sample1_enable\n");

	mutex_lock(&td->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&td->lock);

	return 0;
err:
	dev_dbg(&dssdev->dev,"enable failed\n");
	mutex_unlock(&td->lock);
	return r;

return 0;
}

static void sample1_disable(struct odin_dss_device *dssdev)
{
}

static int sample1_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static int sample1_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

#if 0
static void sample1_ulps_work(struct work_struct *work)
{
}

static void sample1_esd_work(struct work_struct *work)
{

}
#endif

static int sample1_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_AUTO)
		return -EINVAL;
	return 0;
}

static enum odin_dss_update_mode sample1_get_update_mode(
		struct odin_dss_device *dssdev)
{
	return ODIN_DSS_UPDATE_AUTO;
}

static struct odin_dss_driver sample1_driver = {
	.probe		= sample1_probe,
	.remove		= sample1_remove,

	.enable		= sample1_enable,
	.disable	= sample1_disable,
	.suspend	= sample1_suspend,
	.resume		= sample1_resume,

	.set_update_mode = sample1_set_update_mode,
	.get_update_mode = sample1_get_update_mode,

	.get_resolution	= sample1_get_resolution,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= sample1_get_timings,

	.driver         = {
		.name   = "sample1_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init lcd_panel_xxxx_init(void)
{
	odin_dss_register_driver(&sample1_driver);

	return 0;
}

static void __exit lcd_panel_xxxx_exit(void)
{
	odin_dss_unregister_driver(&sample1_driver);
}

module_init(lcd_panel_xxxx_init);
module_exit(lcd_panel_xxxx_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");

