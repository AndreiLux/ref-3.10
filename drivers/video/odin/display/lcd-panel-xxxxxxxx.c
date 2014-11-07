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

//	struct panel_regulator *regulators;
	int num_regulators;
};

enum {
	PANEL_LGHDK,
};

static struct panel_config panel_configs[] = {
	{
		.name		= "xxxxxxxx_panel",
		.type		= PANEL_LGHDK,
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

struct xxxxxxxx_data {
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


static void xxxxxxxx_esd_work(struct work_struct *work);
static void xxxxxxxx_ulps_work(struct work_struct *work);

static int xxxxxxxx_sleep_in(struct xxxxxxxx_data *td)

{
	return 0;
}

static int xxxxxxxx_sleep_out(struct xxxxxxxx_data *td)
{
	return 0;
}



static int xxxxxxxx_wake_up(struct odin_dss_device *dssdev)
{
	return 0;
}

static int xxxxxxxx_bl_update_status(struct backlight_device *dev)
{
	return 0;
}

static int xxxxxxxx_bl_get_intensity(struct backlight_device *dev)
{
	return 0;
}

static const struct backlight_ops xxxxxxxx_bl_ops = {
	.get_brightness = xxxxxxxx_bl_get_intensity,
	.update_status  = xxxxxxxx_bl_update_status,
};

static void xxxxxxxx_get_timings(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void xxxxxxxx_get_resolution(struct odin_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	struct xxxxxxxx_data *td = dev_get_drvdata(&dssdev->dev);

	if (td->rotate == 0 || td->rotate == 2) {
		*xres = dssdev->panel.timings.x_res;
		*yres = dssdev->panel.timings.y_res;
	} else {
		*yres = dssdev->panel.timings.x_res;
		*xres = dssdev->panel.timings.y_res;
	}
}


static void xxxxxxxx_hw_reset(struct odin_dss_device *dssdev)
{
	struct xxxxxxxx_data *td = dev_get_drvdata(&dssdev->dev);

}

static int xxxxxxxx_probe(struct odin_dss_device *dssdev)
{

	return 0;
}

static void __exit xxxxxxxx_remove(struct odin_dss_device *dssdev)
{

}

static int xxxxxxxx_power_on(struct odin_dss_device *dssdev)
{
	return 0;
}

static void xxxxxxxx_power_off(struct odin_dss_device *dssdev)
{
}

static int xxxxxxxx_panel_reset(struct odin_dss_device *dssdev)
{
	dev_err(&dssdev->dev, "performing LCD reset\n");

	xxxxxxxx_power_off(dssdev);
	xxxxxxxx_hw_reset(dssdev);
	return xxxxxxxx_power_on(dssdev);
}

static int xxxxxxxx_enable(struct odin_dss_device *dssdev)
{
	return 0;
}

static void xxxxxxxx_disable(struct odin_dss_device *dssdev)
{
}

static int xxxxxxxx_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static int xxxxxxxx_resume(struct odin_dss_device *dssdev)
{
	return 0;
}

static void xxxxxxxx_ulps_work(struct work_struct *work)
{
}

static void xxxxxxxx_esd_work(struct work_struct *work)
{

}

static int xxxxxxxx_set_update_mode(struct odin_dss_device *dssdev,
		enum odin_dss_update_mode mode)
{
	if (mode != ODIN_DSS_UPDATE_AUTO)
		return -EINVAL;
	return 0;
}

static enum odin_dss_update_mode xxxxxxxx_get_update_mode(
		struct odin_dss_device *dssdev)
{
	return ODIN_DSS_UPDATE_AUTO;
}

static struct odin_dss_driver xxxxxxxx_driver = {
	.probe		= xxxxxxxx_probe,
	.remove		= xxxxxxxx_remove,

	.enable		= xxxxxxxx_enable,
	.disable	= xxxxxxxx_disable,
	.suspend	= xxxxxxxx_suspend,
	.resume		= xxxxxxxx_resume,

	.set_update_mode = xxxxxxxx_set_update_mode,
	.get_update_mode = xxxxxxxx_get_update_mode,

	.get_resolution	= xxxxxxxx_get_resolution,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.get_timings	= xxxxxxxx_get_timings,

	.driver         = {
		.name   = "xxxxxxxx_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init lcd_panel_xxxx_init(void)
{
	odin_dss_register_driver(&xxxxxxxx_driver);

	return 0;
}

static void __exit lcd_panel_xxxx_exit(void)
{
	odin_dss_unregister_driver(&xxxxxxxx_driver);
}

module_init(lcd_panel_xxxx_init);
module_exit(lcd_panel_xxxx_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");

