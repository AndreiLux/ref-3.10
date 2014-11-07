/*
 * linux/drivers/video/odin/dss/display.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author: Seungwon Shin <m4seungwon.shin@lge.com>
 *
 * Some code and ideas taken from drivers/video/omap2/ driver
 * by Tomi Valkeinen.
 * Copyright (C) 2009 Texas Instruments
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
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

#define DSS_SUBSYS_NAME "DISPLAY"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>

#include <video/odindss.h>
#include "dss.h"

void odindss_default_get_resolution(struct odin_dss_device *dssdev,
			u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}
EXPORT_SYMBOL(odindss_default_get_resolution);

void default_get_overlay_fifo_thresholds(enum odin_dma_plane plane,
		u32 fifo_size, enum odin_burst_size *burst_size,
		u32 *fifo_low, u32 *fifo_high)
{
	unsigned burst_size_bytes;

	*burst_size = ODIN_DSS_BURST_16X32;
	burst_size_bytes = 16 * 32 / 8;

	*fifo_high = fifo_size - 1;
	*fifo_low = fifo_size - burst_size_bytes;
}

void odindss_display_get_dimensions(struct odin_dss_device *dssdev,
				u32 *width_in_um, u32 *height_in_um)
{
	if (dssdev->driver->get_dimensions) {
		dssdev->driver->get_dimensions(dssdev, width_in_um,
					       height_in_um);
	} else {
		*width_in_um = dssdev->panel.width_in_um;
		*height_in_um = dssdev->panel.height_in_um;
	}
}

int odindss_default_get_recommended_bpp(struct odin_dss_device *dssdev)
{
	switch (dssdev->type) {
	case ODIN_DISPLAY_TYPE_RGB:
		if (dssdev->pixel_size == 24)
			return 24;
		else
			return 16;

	case ODIN_DISPLAY_TYPE_DSI:
		if (dssdev->pixel_size == 24)
			return 24;
		else
			return 16;
	case ODIN_DISPLAY_TYPE_HDMI:
		return 24;
/* MC lab request */
	case ODIN_DISPLAY_TYPE_MEM2MEM :
		return 24;
	default:
		BUG();
	}
}
EXPORT_SYMBOL(odindss_default_get_recommended_bpp);

bool dss_use_replication(struct odin_dss_device *dssdev,
		enum odin_color_mode mode)
{
	int bpp;

#if 0
	if (mode != ODIN_DSS_COLOR_RGB12U && mode != ODIN_DSS_COLOR_RGB16)
		return false;

	if (dssdev->type == ODIN_DISPLAY_TYPE_DPI &&
			(dssdev->panel.config & ODIN_DSS_LCD_TFT) == 0)
		return false;
#endif

	switch (dssdev->type) {
	case ODIN_DISPLAY_TYPE_RGB:
		bpp = dssdev->phy.rgb.data_lines;
		break;
	case ODIN_DISPLAY_TYPE_HDMI:
		bpp = 24;
		break;
	case ODIN_DISPLAY_TYPE_DSI:
		bpp = dssdev->pixel_size;
		break;
	case ODIN_DISPLAY_TYPE_MEM2MEM:
		bpp = dssdev->pixel_size;
		break;
	default:
		BUG();
	}

	return bpp > 16;
}

void dss_init_device(struct platform_device *pdev,
		struct odin_dss_device *dssdev)
{
#if 0
	int r;

	switch (dssdev->type) {
	case ODIN_DISPLAY_TYPE_RGB:
		r = rgb_init_display(dssdev);
		break;
	case ODIN_DISPLAY_TYPE_DSI:
		r = dsi_init_display(dssdev);
		break;
	case ODIN_DISPLAY_TYPE_HDMI:
		r = hdmi_init_display(dssdev);
		break;
	case ODIN_DISPLAY_TYPE_MEM2MEM:
		r = hdmi_init_display(dssdev);
		break;
	default:
		DSSERR("Support for display '%s' not compiled in.\n",
				dssdev->name);
		return;
	}
#endif

#if 0
	r = odin_du_display_init(dssdev);

	r = odin_dipc_display_init(dssdev);

	if (r) {
		DSSERR("failed to init display %s\n", dssdev->name);
		return;
	}
#endif

	/* BLOCKING_INIT_NOTIFIER_HEAD(&dssdev->state_notifiers); */

}

void dss_uninit_device(struct platform_device *pdev,
		struct odin_dss_device *dssdev)
{
	if (dssdev->manager)
		dssdev->manager->unset_device(dssdev->manager);


	/* TODO: */
}

static int dss_suspend_device(struct device *dev, void *data)
{
	int r;
	struct odin_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->state != ODIN_DSS_DISPLAY_ACTIVE) {
		dssdev->activate_after_resume = false;
		return 0;
	}

	if (!dssdev->driver->suspend) {
		DSSERR("display '%s' doesn't implement suspend\n",
				dssdev->name);
		return -ENOSYS;
	}

	DSSWARN("retry: dss_suspend_device:%s, activate:%d\n",
			dssdev->name, dssdev->activate_after_resume);
	r = dssdev->driver->suspend(dssdev);
	if (r)
		return r;

	dssdev->activate_after_resume = true;

	return 0;
}

int dss_suspend_all_devices(void)
{
	int r;
	struct bus_type *bus = dss_get_bus();

	DSSDBG("dss_suspend_all_devices\n");
	r = bus_for_each_dev(bus, NULL, NULL, dss_suspend_device);
	if (r) {
		/* resume all displays that were suspended */
		DSSERR("dss_suspend_device error, so we resume all\n");
		dss_resume_all_devices();
		return r;
	}

	return 0;
}

static int dss_resume_device(struct device *dev, void *data)
{
	int r;
	struct odin_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->activate_after_resume && dssdev->driver->resume) {
		DSSWARN("retry: dss_resume_device:%s, activate:%d\n",
				dssdev->name, dssdev->activate_after_resume);
		r = dssdev->driver->resume(dssdev);
		if (r)
			return r;
	}

	dssdev->activate_after_resume = false;

	return 0;
}

int dss_resume_all_devices(void)
{
	struct bus_type *bus = dss_get_bus();
	DSSDBG("dss_resume_all_devices\n");
	return bus_for_each_dev(bus, NULL, NULL, dss_resume_device);
}

static int dss_disable_device(struct device *dev, void *data)
{
	struct odin_dss_device *dssdev = to_dss_device(dev);

	if (dssdev->state != ODIN_DSS_DISPLAY_DISABLED)
		dssdev->driver->disable(dssdev);

	return 0;
}

void dss_disable_all_devices(void)
{
	struct bus_type *bus = dss_get_bus();
	bus_for_each_dev(bus, NULL, NULL, dss_disable_device);
}


void odin_dss_get_device(struct odin_dss_device *dssdev)
{
	get_device(&dssdev->dev);
}
EXPORT_SYMBOL(odin_dss_get_device);

void odin_dss_put_device(struct odin_dss_device *dssdev)
{
	put_device(&dssdev->dev);
}
EXPORT_SYMBOL(odin_dss_put_device);

/* ref count of the found device is incremented. ref count
 * of from-device is decremented. */
struct odin_dss_device *odin_dss_get_next_device(struct odin_dss_device *from)
{
	struct device *dev;
	struct device *dev_start = NULL;
	struct odin_dss_device *dssdev = NULL;

	int match(struct device *dev, void *data)
	{
		return 1;
	}

	if (from)
		dev_start = &from->dev;
	dev = bus_find_device(dss_get_bus(), dev_start, NULL, match);
	if (dev)
		dssdev = to_dss_device(dev);
	if (from)
		put_device(&from->dev);

	return dssdev;
}
EXPORT_SYMBOL(odin_dss_get_next_device);

struct odin_dss_device *odin_dss_find_device(void *data,
		int (*match)(struct odin_dss_device *dssdev, void *data))
{
	struct odin_dss_device *dssdev = NULL;

	while ((dssdev = odin_dss_get_next_device(dssdev)) != NULL) {
		if (match(dssdev, data))
			return dssdev;
	}

	return NULL;
}
EXPORT_SYMBOL(odin_dss_find_device);

int odin_dss_start_device(struct odin_dss_device *dssdev)
{
	if (!dssdev->driver) {
		DSSERR("no driver\n");
		return -ENODEV;
	}

	if (!try_module_get(dssdev->dev.driver->owner)) {
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(odin_dss_start_device);

void odin_dss_stop_device(struct odin_dss_device *dssdev)
{
	module_put(dssdev->dev.driver->owner);
}
EXPORT_SYMBOL(odin_dss_stop_device);


