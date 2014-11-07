/*
 * linux/drivers/video/odin/dss/dss-core.c
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

#define DSS_SUBSYS_NAME "DSS-CORE"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>

#include <video/odindss.h>

#include "dss-features.h"
#include "dss.h"
#include "../display/panel_device.h"
#include "mipi-dsi.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/odin_pmic.h>

#ifdef DEBUG
unsigned int dss_debug =1 ;
module_param_named(debug, dss_debug, bool, 0644);
#else
unsigned int dss_debug =0 ;
#endif

static int odin_dss_register_device(struct odin_dss_device *dssdev);
static void odin_dss_unregister_device(struct odin_dss_device *dssdev);

static struct {
	struct platform_device *pdev;
} dss_core;

struct odin_dss_device *board_odin_dss_devices[3];
struct odin_dss_board_info board_odin_dss_data;

#ifdef CONFIG_OF
struct device odin_device_parent = {
	.init_name	= "odindss_parent",
	.parent         = &platform_bus,
};

static struct of_device_id odin_dss_match[] = {
	{
		.compatible = "odindss",
	},
	{},
};
#endif

static void board_odin_dss_data_register(void)
{
/*
              
                                 
                                 
*/
#ifdef CONFIG_PANEL_LGLH600WF2
	board_odin_dss_devices[0] = &dsi_lglh600wf2_device;
#else /* SIC Original */
	/* platform_data register */
#ifndef CONFIG_ODIN_DSS_FPGA /* FPGA */
#ifdef CONFIG_ARABICA_CHECK_REV
	int revision_data = odin_pmic_revision_get();
	printk("DSS Revision Check Value %x \n", revision_data);

		/* HDK (5.2 or 5.5) */
	if (revision_data == 0xd)
		board_odin_dss_devices[0] = &dsi_lglh520wf1_device;
	else
#endif
		board_odin_dss_devices[0] = &dsi_lglh500wf1_device;
#else
	board_odin_dss_devices[0] = &at070tn94_device;
#endif
#endif

	board_odin_dss_devices[1] = &mem2mem_device;

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
	board_odin_dss_devices[2] = &hdmi_panel_device;
#else
	board_odin_dss_devices[2] = &hdmi_device;
#endif

	board_odin_dss_data.num_devices = ARRAY_SIZE(board_odin_dss_devices);
	board_odin_dss_data.devices = board_odin_dss_devices;
	board_odin_dss_data.default_device = board_odin_dss_devices[0];

	printk("board_odin_dss_devices[0]addr :0x%p\n", board_odin_dss_devices[0]);

	return;
}

/* PLATFORM DEVICE */
static int odin_dss_probe(struct platform_device *pdev)
{
	struct odin_dss_board_info *pdata;
	int r;
	int i;
	unsigned int ch_set_dmask = 0;
	struct odin_dss_device *mem2mem_dev = NULL;

	/* platform_data register */
	board_odin_dss_data_register();
	pdev->dev.platform_data = &board_odin_dss_data;

	pdata = pdev->dev.platform_data;

	DSSINFO("odin_dss_probe\n");

	dss_core.pdev = pdev;

	dss_features_init();

	odin_mgr_dss_init_overlay_managers(pdev);
	odin_ovl_dss_init_overlays(pdev);
	odin_wb_dss_init_writeback(pdev);

	r = odin_crg_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize CRG platform driver\n");
		goto err_crg;
	}

	r = odin_du_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize DSS platform driver\n");
		goto err_du;
	}

	r = odin_dipc_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize dispc platform driver\n");
		goto err_dip;
	}

	r = odin_cabc_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize CABC platform driver\n");
		goto err_cabc;
	}
	odin_mipi_dsi_init_platform_driver();

#ifdef CONFIG_ODIN_XDENGINE
	r = odin_mxd_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize Mobile XD Engine platform driver\n");
		goto err_mxd;
	}
#endif

#ifdef CONFIG_ODIN_S3D
	r = odin_formatter_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize formatter platform driver\n");
		goto err_formatter;
	}
#endif

#ifdef CONFIG_ODIN_HDMI
	r = odin_hdmi_init_platform_driver();
	if (r) {
		DSSERR("Failed to initialize Mobile HDMI platform driver\n");
		goto err_hdmi;
	}
#endif


	for (i = 0; i < odin_mgr_dss_get_num_overlay_managers(); i++)
		ch_set_dmask |= (1 << i);

	for (i = 0; i < pdata->num_devices; ++i) {
		struct odin_dss_device *dssdev = pdata->devices[i];

		r = odin_dss_register_device(dssdev);
		if (r) {
			DSSERR("device %d %s register failed %d\n", i,
				dssdev->name ?: "unnamed", r);

			while (--i >= 0)
				odin_dss_unregister_device(pdata->devices[i]);

			goto err_register;
		}

		if (!(dssdev->type & ODIN_DISPLAY_TYPE_MEM2MEM))
			ch_set_dmask &= ~(1 << dssdev->channel);
		else
			mem2mem_dev = dssdev;

#if 0
		if (def_disp_name && strcmp(def_disp_name, dssdev->name) == 0)
			pdata->default_device = dssdev;
#endif
	}

	if (ch_set_dmask)
	{
		if(mem2mem_dev){
			mem2mem_dev->channel = fls(ch_set_dmask) - 1;
			DSSDBG("mem2mem channel is %d \n", mem2mem_dev->channel);
		}
	}

	return 0;

err_register:

#ifdef CONFIG_ODIN_HDMI
err_hdmi:
	odin_hdmi_uninit_platform_driver();
#endif

#ifdef CONFIG_ODIN_S3D
err_formatter:
	odin_formatter_uninit_platform_driver();
#endif

#ifdef CONFIG_ODIN_XDENGINE
err_mxd:
 	odin_mxd_uninit_platform_driver();
#endif
err_cabc:
 	odin_cabc_uninit_platform_driver();

err_dip:
	odin_dipc_uninit_platform_driver();

err_du:
	odin_du_uninit_platform_driver();

err_crg:
	odin_crg_uninit_platform_driver();

	return r;
}

static int odin_dss_remove(struct platform_device *pdev)
{
	struct odin_dss_board_info *pdata = pdev->dev.platform_data;
	int i;

	DSSINFO("%s\n", __func__);

#ifdef CONFIG_ODIN_HDMI
	odin_hdmi_uninit_platform_driver();
#endif

#ifdef CONFIG_ODIN_S3D
	odin_formatter_uninit_platform_driver();
#endif

#ifdef CONFIG_ODIN_XDENGINE
	odin_mxd_uninit_platform_driver();
#endif


	odin_dipc_uninit_platform_driver();

	odin_crg_uninit_platform_driver();

	odin_du_uninit_platform_driver();

	odin_wb_dss_uninit_writeback(pdev);
	odin_ovl_dss_uninit_overlays(pdev);
	odin_mgr_dss_uninit_overlay_managers(pdev);

	for (i = 0; i < pdata->num_devices; ++i)
		odin_dss_unregister_device(pdata->devices[i]);

	return 0;
}

static void odin_dss_shutdown(struct platform_device *pdev)
{
	DSSDBG("shutdown\n");
	dss_disable_all_devices();
}

static int odin_dss_suspend(struct platform_device *pdev, pm_message_t state)
{
	DSSDBG("suspend %d\n", state.event);

	return dss_suspend_all_devices();
}

static int odin_dss_resume(struct platform_device *pdev)
{
	DSSDBG("resume\n");

	return dss_resume_all_devices();
}

static struct platform_driver odin_dss_driver = {
	.probe          = odin_dss_probe,
	.remove         = odin_dss_remove,
	.shutdown	= odin_dss_shutdown,
	.suspend	= odin_dss_suspend,
	.resume		= odin_dss_resume,
	.driver         = {
		.name   = "odindss",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_dss_match),
#endif
	},
};


/*------------------------------------------------------------------------
  odin_dss_device platform register....
 ------------------------------------------------------------------------*/

/* BUS */
static int dss_bus_match(struct device *dev, struct device_driver *driver)
{
	struct odin_dss_device *dssdev = to_dss_device(dev);

	DSSDBG("bus_match. dev %s/%s, drv %s\n",
			dev_name(dev), dssdev->driver_name, driver->name);

	return strcmp(dssdev->driver_name, driver->name) == 0;
}


static struct bus_type dss_bus_type = {
	.name = "odindss",
	.match = dss_bus_match,
#if 0
	.dev_attrs = default_dev_attrs,
	.drv_attrs = default_drv_attrs,
#endif
};

static void dss_bus_release(struct device *dev)
{
	DSSDBG("bus_release\n");
}

static struct device dss_bus = {
	.release = dss_bus_release,
};

struct bus_type *dss_get_bus(void)
{
	return &dss_bus_type;
}

/* DRIVER */
static int dss_driver_probe(struct device *dev)
{
	int r;
	struct odin_dss_driver *dssdrv = to_dss_driver(dev->driver);
	struct odin_dss_device *dssdev = to_dss_device(dev);
	struct odin_dss_board_info *pdata = dss_core.pdev->dev.platform_data;
	bool force;

	DSSDBG("driver_probe: dev %s/%s, drv %s\n",
				dev_name(dev), dssdev->driver_name,
				dssdrv->driver.name);

	dss_init_device(dss_core.pdev, dssdev);

	force = pdata->default_device == dssdev;
	odin_ovl_dss_recheck_connections(dssdev, force);

	r = dssdrv->probe(dssdev);

	if (r) {
		DSSERR("driver probe failed: %d\n", r);
		dss_uninit_device(dss_core.pdev, dssdev);
		return r;
	}

	DSSDBG("probe done for device %s\n", dev_name(dev));

	dssdev->driver = dssdrv;

/* need hdmi-check */
#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
	if (strcmp(dssdev->driver_name, "hdmi_panel") == 0)
		dssdrv->enable(dssdev);
#else
	if (strcmp(dssdev->driver_name, "hdmi_panel") == 0)
		dssdrv->enable(dssdev);
#endif
	return 0;
}

static int dss_driver_remove(struct device *dev)
{
	struct odin_dss_driver *dssdrv = to_dss_driver(dev->driver);
	struct odin_dss_device *dssdev = to_dss_device(dev);

	DSSDBG("driver_remove: dev %s/%s\n", dev_name(dev),
			dssdev->driver_name);

	dssdrv->remove(dssdev);

	dss_uninit_device(dss_core.pdev, dssdev);

	dssdev->driver = NULL;

	return 0;
}

int odin_dss_register_driver(struct odin_dss_driver *dssdriver)
{
	dssdriver->driver.bus = &dss_bus_type;
	dssdriver->driver.probe = dss_driver_probe;
	dssdriver->driver.remove = dss_driver_remove;

	if (dssdriver->get_resolution == NULL)
		dssdriver->get_resolution = odindss_default_get_resolution;
	if (dssdriver->get_recommended_bpp == NULL)
		dssdriver->get_recommended_bpp =
			odindss_default_get_recommended_bpp;

	return driver_register(&dssdriver->driver);
}
EXPORT_SYMBOL(odin_dss_register_driver);

void odin_dss_unregister_driver(struct odin_dss_driver *dssdriver)
{
	driver_unregister(&dssdriver->driver);
}
EXPORT_SYMBOL(odin_dss_unregister_driver);

/* DEVICE */
static void reset_device(struct device *dev, int check)
{
	u8 *dev_p = (u8 *)dev;
	u8 *dev_end = dev_p + sizeof(*dev);
	void *saved_pdata;

	saved_pdata = dev->platform_data;
	if (check) {
		/*
		 * Check if there is any other setting than platform_data
		 * in struct device; warn that these will be reset by our
		 * init.
		 */
		dev->platform_data = NULL;
		while (dev_p < dev_end) {
			if (*dev_p) {
				WARN("%s: struct device fields will be "
						"discarded\n",
				     __func__);
				break;
			}
			dev_p++;
		}
	}
	memset(dev, 0, sizeof(*dev));
	dev->platform_data = saved_pdata;
}


static void odin_dss_dev_release(struct device *dev)
{
	reset_device(dev, 0);
}

static int odin_dss_register_device(struct odin_dss_device *dssdev)
{
	static int dev_num;

	WARN_ON(!dssdev->driver_name);

	reset_device(&dssdev->dev, 1);
	dssdev->dev.bus = &dss_bus_type;
	dssdev->dev.parent = &dss_bus;
	dssdev->dev.release = odin_dss_dev_release;
	dev_set_name(&dssdev->dev, "display%d", dev_num++);
	return device_register(&dssdev->dev);
}

static void odin_dss_unregister_device(struct odin_dss_device *dssdev)
{
	device_unregister(&dssdev->dev);
}

/* BUS */
static int odin_dss_bus_register(void)
{
	int r;

	r = bus_register(&dss_bus_type);
	if (r) {
		DSSERR("bus register failed\n");
		return r;
	}

	dev_set_name(&dss_bus, "odindss");
	r = device_register(&dss_bus);
	if (r) {
		DSSERR("bus driver register failed\n");
		bus_unregister(&dss_bus_type);
		return r;
	}

	return 0;
}

/* INIT */

#ifdef CONFIG_ODIN_DSS_MODULE
static void odin_dss_bus_unregister(void)
{
	device_unregister(&dss_bus);

	bus_unregister(&dss_bus_type);
}

static int __init odin_dss_init(void)
{
	int r;

	r = odin_dss_bus_register();
	if (r)
		return r;
#ifdef CONFIG_OF
	device_register(&odin_device_parent);
#endif
	r = platform_driver_register(&odin_dss_driver);
	if (r) {
		odin_dss_bus_unregister();
		return r;
	}

	return 0;
}

static void __exit odin_dss_exit(void)
{
	platform_driver_unregister(&odin_dss_driver);
	odin_dss_bus_unregister();
}

module_init(odin_dss_init);
module_exit(odin_dss_exit);
#else
static int __init odin_dss_init(void)
{
	printk("odin_dss_init \n");
	return odin_dss_bus_register();
}

#ifdef CONFIG_OF
static int __init odin_device_init(void)
{
	return device_register(&odin_device_parent);
}
core_initcall(odin_device_init);
#endif

static int __init odin_dss_init2(void)
{
	return platform_driver_register(&odin_dss_driver);
}

core_initcall(odin_dss_init);
device_initcall(odin_dss_init2);
#endif

MODULE_AUTHOR("");
MODULE_DESCRIPTION("ODIN Display Subsystem");
MODULE_LICENSE("GPL v2");

