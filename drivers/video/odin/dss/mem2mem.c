/*
 * Mem2Mem Driver
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

struct mem2mem_data {
	struct mutex lock;
	struct odin_dss_device *dssdev;
};


static void mem2mem_get_resolution(struct odin_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	DSSDBG("mem2mem_get_resolution\n");

	*xres = 800;
	*yres = 480;
}


static int mem2mem_probe(struct odin_dss_device *dssdev)
{
	struct mem2mem_data *md;
	int r;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (!md) {
		r = -ENOMEM;
		goto err;
	}
	md->dssdev = dssdev;
	mutex_init(&md->lock);

	dev_set_drvdata(&dssdev->dev, md);

	DSSDBG("mem2mem_probe\n");

err:
	return 0;
}

static void mem2mem_remove(struct odin_dss_device *dssdev)
{
	struct mem2mem_data *md = dev_get_drvdata(&dssdev->dev);

	kfree(md);
}


static int mem2mem_enable(struct odin_dss_device *dssdev)
{
	int r;
	struct mem2mem_data *md = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "enable\n");
	DSSDBG("mem2mem_enable\n");

	mutex_lock(&md->lock);

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = ODIN_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&md->lock);

	return 0;
err:
	dev_dbg(&dssdev->dev, "enable failed\n");
	mutex_unlock(&md->lock);
	return r;

return 0;
}

static void mem2mem_disable(struct odin_dss_device *dssdev)
{
}

static int mem2mem_suspend(struct odin_dss_device *dssdev)
{
	return 0;
}

static int mem2mem_resume(struct odin_dss_device *dssdev)
{
	return 0;
}


static struct odin_dss_driver mem2mem_driver = {
	.probe		= mem2mem_probe,
	.remove		= mem2mem_remove,

	.enable		= mem2mem_enable,
	.disable	= mem2mem_disable,
	.suspend	= mem2mem_suspend,
	.resume		= mem2mem_resume,

	.get_resolution	= mem2mem_get_resolution,
	.get_recommended_bpp = odindss_default_get_recommended_bpp,

	.driver         = {
		.name   = "mem2mem",
		.owner  = THIS_MODULE,
	},
};

static int __init mem2mem_init(void)
{
	odin_dss_register_driver(&mem2mem_driver);

	return 0;
}

static void __exit mem2mem_exit(void)
{
	odin_dss_unregister_driver(&mem2mem_driver);
}

module_init(mem2mem_init);
module_exit(mem2mem_exit);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");

