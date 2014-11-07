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
#include <linux/device.h>
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

#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>

#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/odin_dfs.h>

#include "../hal/mipi_dsi_common.h"
#include "../mipi_dsi_drv.h"
#include <video/odin-dss/mipi_device.h>
#include <linux/vmalloc.h>

#include "lcd-panel-lglh600wf2.h"

struct panel_desc
{
	char *name;
	unsigned int gpio_intr;
	unsigned int gpio_reset;
	unsigned int gpio_dsv_enable;
};
static struct panel_desc *desc = NULL;

static struct mipi_dsi_desc panel_cfg = {
    .in_pixel_fmt = INPUT_RGB24,
    .dsi_op_mode = DSI_VIDEO_MODE,
    .out_pixel_fmt = OUTPUT_RGB24,
    .vm_fmt = NON_BURST_MODE_WITH_SYNC_PLUSES,
    .vm_timing = {
        .x_res      = 1080,
        .y_res      = 1920,
	.height     = 132,
	.width      = 74,
        /* mipi clk(888MHz) * 4lanes / 24bits (KHz)*/
        .pixel_clock = 148000,
        .data_width = 24,
        .bpp= 24,
        .hsw = 4, /* hsync_width */
        .hbp = 50,
        .hfp = 100,
        .vsw = 1,
        .vfp = 4,
        .vbp = 14, /* 4 + 0xA */
        .pclk_pol = 0,
        .blank_pol = 0,
        .vsync_pol = 1,
        .hsync_pol = 1,
    },
    .ecc = true,
    .checksum = true,
    .eotp = true,
    .num_of_lanes = 4,
};

static struct mipi_hal_interface *mipi_ops = NULL;

static int lglh600wf2_power_control(uint8_t on_off)
{
	int ret = 0;
	static int init_done = 0;

	if (!init_done) {
		ret = gpio_request(desc->gpio_dsv_enable, "DSV_EN");
		if (ret) {
			pr_err("%s: failed to request DSV_EN gpio.\n", __func__);
			goto out;
		}
		ret = gpio_direction_output(desc->gpio_dsv_enable, 1);
		if (ret) {
			pr_err("%s: failed to set DSV_EN direction.\n", __func__);
			goto err_gpio;
		}
		init_done = 1;
	}

	/* IOVCC(LVS3_1.8V) Control will be added. */

	/* DSV(5.5V SVSP/VSN) Control. */
	gpio_set_value(desc->gpio_dsv_enable, on_off);
	mdelay(20);
	goto out;

err_gpio:
	gpio_free(desc->gpio_dsv_enable);
out:
	return ret;
}

static int lglh600wf2_reset_control(uint8_t value)
{
	int ret = 0;
	static int init_done = 0;

	if (!init_done) {
		ret = gpio_request(desc->gpio_reset, "LCD_RESET_N");
		if (ret) {
			pr_err("%s: failed to request LCD RESET gpio.\n", __func__);
			goto out;
		}
		ret = gpio_direction_output(desc->gpio_reset, 1);
		if (ret) {
			pr_err("%s: failed to set LCD RESET direction.\n", __func__);
			goto err_gpio;
		}
		init_done = 1;
	}

	if (value) {
		gpio_set_value(desc->gpio_reset, 1);
		mdelay(10);
		gpio_set_value(desc->gpio_reset, 0);
		mdelay(10);
		gpio_set_value(desc->gpio_reset, 1);
		mdelay(10);
	}
	else {
		gpio_set_value(desc->gpio_reset, 0);
		mdelay(5);
	}
	goto out;

err_gpio:
	gpio_free(desc->gpio_reset);
out:
	return ret;
}


void _lglh600wf2_mipi_send_multi( struct dsi_cmd *dsi_cmd, unsigned int cnt_pkt)
{
	int i;
	u16 delay;
    struct dsi_packet packet;
	
	for (i = 0; i < cnt_pkt; i++, dsi_cmd++)
	{
		if (dsi_cmd->cmd_type == DSI_DELAY)
		{
			delay = dsi_cmd->delay_ms;
			mdelay(delay);
		}
		else if (dsi_cmd->cmd_type == LONG_CMD_MIPI)
		{
		    packet.escape_or_hs = dsi_cmd->pkt.escape_or_hs;
		    packet.ph.di= dsi_cmd->pkt.ph.di;
		    packet.ph.packet_data.lp.wc= dsi_cmd->pkt.ph.packet_data.lp.wc; 
		    packet.pPayload= dsi_cmd->pkt.pPayload;
		    mipi_ops->send(&packet);
		}
		else if (dsi_cmd->cmd_type == SHORT_CMD_MIPI)
		{
		    packet.escape_or_hs = dsi_cmd->pkt.escape_or_hs;
		    packet.ph.di= dsi_cmd->pkt.ph.di;
			packet.ph.packet_data.sp.data0= dsi_cmd->pkt.ph.packet_data.sp.data0;
			packet.ph.packet_data.sp.data1= dsi_cmd->pkt.ph.packet_data.sp.data1;
		    mipi_ops->send(&packet);
		}
	}
	
}

int lglh600wf2_panel_on(void)
{
	int ret;
	bool result;

	//  added reset code temporary  to turn off the lcd panel turned on in the LK.
	/* Panel Reset LOW */
	ret = lglh600wf2_reset_control(0);
	if (ret < 0) {
		pr_err("%s: panel's reset is failed.\n", __func__);
		return ret;
	}

	/* Panel Power up */
	ret = lglh600wf2_power_control(1);
	if (ret < 0) {
		pr_err("%s: panel's power on is failed.\n", __func__);
		return ret;
	}
	/* Panel Reset HIGH */
	ret = lglh600wf2_reset_control(1);
	if (ret < 0) {
		pr_err("%s: panel's reset is failed.\n", __func__);
		return ret;
	}

        result = mipi_ops->open(&panel_cfg);
	if (result == false) {
		ret = -EBADRQC;
		return ret;
	}
	_lglh600wf2_mipi_send_multi(lglh600wf2_dsi_on_command, ARRAY_SIZE(lglh600wf2_dsi_on_command));

	mipi_ops->start_video_mode();

	return 0;
}

static int lglh600wf2_panel_off(void)
{
	int ret;
	bool result;

        result = mipi_ops->stop_video_mode();
	if(result == false) {
		ret = -EBADRQC;
		return ret;
	}
	_lglh600wf2_mipi_send_multi(lglh600wf2_dsi_off_command, ARRAY_SIZE(lglh600wf2_dsi_off_command));

        mipi_ops->close();

//	odin_mipi_dsi_phy_power(resource_index, false);


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

static bool lglh600wf2_mipi_dsi_init( struct mipi_hal_interface *hal_ops)
{
    pr_info("%s \n",__func__);

    mipi_ops = hal_ops;

    return true;
}

static bool lglh600wf2_blank( bool blank)
{
    pr_info("%s \n",__func__);

    if(!mipi_ops)
        return false;

    if(blank) {
        lglh600wf2_panel_off();

    } else {
        lglh600wf2_panel_on();
    }

    return true;
}

static bool lglh600wf2_get_screen_info(struct panel_info *s_info)
{
    s_info->name= desc->name;
    s_info->bpp = panel_cfg.vm_timing.bpp;
    s_info->data_width = panel_cfg.vm_timing.data_width;
    s_info->hbp = panel_cfg.vm_timing.hbp;
    s_info->hfp = panel_cfg.vm_timing.hfp;
    s_info->hsw = panel_cfg.vm_timing.hsw;
    s_info->hsync_pol= panel_cfg.vm_timing.hsync_pol;
    s_info->pclk_pol = panel_cfg.vm_timing.pclk_pol;
    s_info->pixel_clock = panel_cfg.vm_timing.pixel_clock;
    s_info->vbp = panel_cfg.vm_timing.vbp;
    s_info->vfp = panel_cfg.vm_timing.vfp;
    s_info->vsw = panel_cfg.vm_timing.vsw;
    s_info->vsync_pol = panel_cfg.vm_timing.vsync_pol;
    s_info->x_res = panel_cfg.vm_timing.x_res;
    s_info->y_res = panel_cfg.vm_timing.y_res;
    s_info->vsync_pol = panel_cfg.vm_timing.vsync_pol;
    s_info->hsync_pol = panel_cfg.vm_timing.hsync_pol;

    return true;
}

static bool lglh600wf2_is_command_mode(void)
{
	return false;
}

static int lglh600wf2_probe(struct platform_device *pdev)
{
    struct mipi_device_interface mipi_ops;
    int ret;

    const char *port_str;
    enum dsi_module dsi_idx;
    
    of_property_read_string(pdev->dev.of_node,"port",&port_str);
	of_property_read_u32(pdev->dev.of_node,"gpio_intr",&desc->gpio_intr);
	of_property_read_u32(pdev->dev.of_node,"gpio_reset",&desc->gpio_reset);
	of_property_read_u32(pdev->dev.of_node,"gpio_dsv_enable",&desc->gpio_dsv_enable);
	
    pr_info("%s port=%s gpio_intr:%d, gpio_reset: %d, gpio_dsv_enable: %d \n",__func__,port_str,desc->gpio_intr, desc->gpio_reset, desc->gpio_dsv_enable);

    if(strcmp(port_str,"lcd0") == 0) 
        dsi_idx = MIPI_DSI_DIP0;
    else if (strcmp(port_str,"lcd1") == 0)
        dsi_idx = MIPI_DSI_DIP1;
    else  {
        pr_err("invalid port name !!\n");
        ret = EINVAL;
        goto err;
    }

    mipi_ops.init= &lglh600wf2_mipi_dsi_init;
    mipi_ops.blank= &lglh600wf2_blank;
    mipi_ops.get_screen_info= &lglh600wf2_get_screen_info;
    mipi_ops.is_command_mode = &lglh600wf2_is_command_mode;
    mipi_device_ops_register(dsi_idx, &mipi_ops);

	desc->name = (char*)pdev->dev.driver->name;

    return 0;
err:
    return ret;
}

static struct of_device_id lglh600wf2_match[]= {
        { .compatible="lglh600wf2"},
        {},
};
 
static struct platform_driver lglh600wf2_driver = {
    .probe      = lglh600wf2_probe,
    .driver         = {
        .name   = "lglh600wf2",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(lglh600wf2_match),
    },

 };


static int __init lglh600wf2_init(void)
{
	desc = vzalloc(sizeof(struct panel_desc));
	if (desc == NULL) {
		pr_err("vzalloc failed\n");
		return -1;
	}
	platform_driver_register(&lglh600wf2_driver);

	return 0;
}

static void __exit lglh600wf2_exit(void)
{
	platform_driver_unregister(&lglh600wf2_driver);
	if (desc) {
		vfree(desc);
		desc = NULL;
	}
}

late_initcall(lglh600wf2_init);
module_exit(lglh600wf2_exit);

MODULE_DESCRIPTION("lglh600wf2 video mode Driver");
MODULE_LICENSE("GPL");
