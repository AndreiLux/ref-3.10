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

#include "../hal/mipi_dsi_common.h"
#include "../mipi_dsi_drv.h"
#include <video/odin-dss/mipi_device.h>

#include <linux/odin_pmic.h>
#include <linux/vmalloc.h>

struct panel_desc
{
	char *name;
	unsigned int gpio_intr;
	unsigned int gpio_reset;
	unsigned int gpio_dsv_enable;
};
static struct panel_desc *desc = NULL;



static char mcap_setting[2] = {0xB0, 0x04};

//static char nop_command[2] = {0x00, 0x00};

static char interface_setting[7] = { 0xB3, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00 };

#ifdef MIPI_300MHZ_MODE
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xA3 };
#else
static char dsi_ctrl[3] = { 0xB6, 0x3A, 0xD3 };
#endif

static char display_setting_1[35] = {
    0xC1,
    0x84, 0x60, 0x50, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0C,
    0x01, 0x58, 0x73, 0xAE, 0x31,
    0x20, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x10,
    0x10, 0x10, 0x00, 0x00, 0x00,
    0x22, 0x02, 0x02, 0x00
};

static char vsync_setting[4] = { 0xC3, 0x00, 0x00, 0x00 };

static char display_setting_2[8] = {
    0xC2,
#if 0
    0x32, 0xF7, 0x80, 0x1F, 0x08,  /*VBP 0xA+ 0x15*/
#else
    0x32, 0xF7, 0x80, 0x14, 0x08,  /*VBP 0xA+ 0xA*/
#endif
    0x00, 0x00
};

static char source_timing_setting[23] = {
    0xC4,
    0x70, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x11,
    0x06, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00,
    0x11, 0x06
};

static char ltps_timing_setting[41] = {
    0xC6,
    0x06, 0x6D, 0x06, 0x6D, 0x06,
    0x6D, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x6D, 0x06, 0x6D, 0x06,
    0x6D, 0x15, 0x19, 0x07, 0x00,
    0x01, 0x06, 0x6D, 0x06, 0x6D,
    0x06, 0x6D, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x6D, 0x06, 0x6D,
    0x06, 0x6D, 0x15, 0x19, 0x07
};

static char gamma_setting_a[25] = {
    0xC7,
    0x00, 0x09, 0x14, 0x23, 0x30,
    0x48, 0x3D, 0x52, 0x5F, 0x67,
    0x6B, 0x70, 0x00, 0x09, 0x14,
    0x23, 0x30, 0x48, 0x3D, 0x52,
    0x5F, 0x67, 0x6B, 0x70
};

static char gamma_setting_b[25] = {
    0xC8,
    0x00, 0x09, 0x14, 0x23, 0x30,
    0x48, 0x3D, 0x52, 0x5F, 0x67,
    0x6B, 0x70, 0x00, 0x09, 0x14,
    0x23, 0x30, 0x48, 0x3D, 0x52,
    0x5F, 0x67, 0x6B, 0x70
};

static char gamma_setting_c[25] = {
    0xC9,
    0x00, 0x09, 0x14, 0x23, 0x30,
    0x48, 0x3D, 0x52, 0x5F, 0x67,
    0x6B, 0x70, 0x00, 0x09, 0x14,
    0x23, 0x30, 0x48, 0x3D, 0x52,
    0x5F, 0x67, 0x6B, 0x70
};

/*static char panel_interface_ctrl        [2] = {0xCC, 0x09};*/

static char pwr_setting_chg_pump[15] = {
    0xD0,
    0x00, 0x00, 0x19, 0x18, 0x99,
    0x99, 0x19, 0x01, 0x89, 0x00,
    0x55, 0x19, 0x99, 0x01
};

static char pwr_setting_internal_pwr[27] = {
    0xD3,
    0x1B, 0x33, 0xBB, 0xCC, 0xC4,
    0x33, 0x33, 0x33, 0x00, 0x01,
    0x00, 0xA0, 0xD8, 0xA0, 0x0D,
    0x39, 0x33, 0x44, 0x22, 0x70,
    0x02, 0x39, 0x03, 0x3D, 0xBF,
    0x00
}; 

static char vcom_setting[8] = {
    0xD5,
    0x06, 0x00, 0x00, 0x01, 0x2C,
    0x01, 0x2C
};
#if 0
static char color_enhancement[33] = {
    0xCA,
    0x01, 0x70, 0xB0, 0xFF, 0xB0,
    0xB0, 0x98, 0x78, 0x3F, 0x3F,
    0x80, 0x80, 0x08, 0x38, 0x08,
    0x3F, 0x08, 0x90, 0x0C, 0x0C,
    0x0A, 0x06, 0x04, 0x04, 0x00,
    0xC0, 0x10, 0x10, 0x3F, 0x3F,
    0x3F, 0x3F
};

static char color_enhancement_off[33] = {
    0xCA,
    0x00, 0x80, 0xDC, 0xF0, 0xDC,
    0xBE, 0xA5, 0xDC, 0x14, 0x20,
    0x80, 0x8C, 0x0A, 0x4A, 0x37,
    0xA0, 0x55, 0xF8, 0x0C, 0x0C,
    0x20, 0x10, 0x20, 0x20, 0x18,
    0xE8, 0x10, 0x10, 0x3F, 0x3F,
    0x3F, 0x3F
};
static char auto_contrast[7] = {
    0xD8,
    0x00, 0x80, 0x80, 0x40, 0x42,
    0x55
};

static char auto_contrast_off[7] = {
    0xD8,
    0x00, 0x80, 0x80, 0x40, 0x42,
    0x55
};

static char sharpening_control[3] = { 0xDD, 0x01, 0x95 };
static char sharpening_control_off[3] = { 0xDD, 0x20, 0x45 };
#endif


static struct mipi_dsi_desc panel_cfg = {
    .in_pixel_fmt = INPUT_RGB24,
    .dsi_op_mode = DSI_VIDEO_MODE,
    .out_pixel_fmt = OUTPUT_RGB24,
    .vm_fmt = NON_BURST_MODE_WITH_SYNC_PLUSES,
    .vm_timing = {
        .x_res      = 1080,
        .y_res      = 1920,
	.height     = 121,
	.width      = 68,
        /* mipi clk(864MHz) * 4lanes / 24bits (KHz)*/
        .pixel_clock = 144000,
        .data_width = 24,
        .bpp= 24,
        .hsw = 4, /* hsync_width */
        .hbp = 50,
        .hfp = 100,
        .vsw = 1,
        .vfp = 4,
        .vbp = 14, /* 4 + 0xA */
        .pclk_pol = 0,
        .vsync_pol = 1,
        .hsync_pol = 1,
    },
    .ecc = true,
    .checksum = true,
    .eotp = true,
    .num_of_lanes = 4,
};

static struct mipi_hal_interface *mipi_ops = NULL;


static int lglh500wf1_power_on(void)
{

    int ret;

	ret = gpio_direction_output(desc->gpio_dsv_enable, 1);
	if (ret < 0)
	{
		printk("gpio_direction_output failed for desc->gpio_dsv_enable gpiod %d\n", desc->gpio_dsv_enable);
		return -1;
	}

	ret = gpio_direction_output(desc->gpio_reset, 1);
	if (ret < 0)
	{
		printk("gpio_direction_output failed for lcd_reset gpiod %d\n", desc->gpio_reset);
		return -1;
	}

	gpio_set_value(desc->gpio_dsv_enable , 0);
	gpio_set_value(desc->gpio_reset , 1);

    mdelay(20);

    mdelay(6);

	gpio_set_value(desc->gpio_dsv_enable , 1);
	mdelay(6);

	gpio_set_value(desc->gpio_reset , 0);
	mdelay(5);

	gpio_set_value(desc->gpio_reset , 1);
	mdelay(5);

	return 0;
}

static void lglh500wf1_power_off(void) 
{

	gpio_set_value(desc->gpio_reset , 0);
	mdelay(2);

	gpio_set_value(desc->gpio_dsv_enable , 0);
	mdelay(2);

    mdelay(4);
}


int lglh500wf1_panel_on(void)
{
    
    struct dsi_packet packet;

    lglh500wf1_power_on();

    mipi_ops->open(&panel_cfg);

    // horizontal flip
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_SHORT_WRITE_1_PARAMETER;
    packet.ph.packet_data.sp.data0=  0x36;
    packet.ph.packet_data.sp.data1=  0x40; //H_FLIP
    mipi_ops->send(&packet);


    /* Display Initial Set*/
    //         DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x11, 0x00),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
    packet.ph.packet_data.sp.data0=  0x11;
    packet.ph.packet_data.sp.data1=  0x00;
    mipi_ops->send(&packet);

    // DSI_DLY_MS(100),
    mdelay(100);

    /*  mcap_setting*/
    //    DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xB0, 0x04),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
    packet.ph.packet_data.sp.data0= mcap_setting[0];
    packet.ph.packet_data.sp.data1= mcap_setting[1];
    mipi_ops->send(&packet);


    //   DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, interface_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(interface_setting); 
    packet.pPayload= interface_setting;
    mipi_ops->send(&packet);


    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, dsi_ctrl),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(dsi_ctrl); 
    packet.pPayload= dsi_ctrl;
    mipi_ops->send(&packet);



    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_1),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(display_setting_1); 
    packet.pPayload= display_setting_1;
    mipi_ops->send(&packet);

    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, display_setting_2),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(display_setting_2); 
    packet.pPayload= display_setting_2;
    mipi_ops->send(&packet);

    
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vsync_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(vsync_setting); 
    packet.pPayload= vsync_setting;
    mipi_ops->send(&packet);
    
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, source_timing_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(source_timing_setting); 
    packet.pPayload= source_timing_setting;
    mipi_ops->send(&packet);
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, ltps_timing_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(ltps_timing_setting); 
    packet.pPayload= ltps_timing_setting;
    mipi_ops->send(&packet);
    
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_a),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(gamma_setting_a); 
    packet.pPayload= gamma_setting_a;
    mipi_ops->send(&packet);
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_b),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(gamma_setting_b); 
    packet.pPayload= gamma_setting_b;
    mipi_ops->send(&packet);
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, gamma_setting_c),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(gamma_setting_c); 
    packet.pPayload= gamma_setting_c;
    mipi_ops->send(&packet);
    
    
    /* panel_interface_ctrl),*/
    //    DSI_CMD_SHORT(DSI_SRC_DATA_ID_GEN_SHORT_WRITE_2_PARA, 0xcc, 0x09),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_SHORT_WRITE_2_PARAMETERS;
    packet.ph.packet_data.sp.data0= 0xcc;
    packet.ph.packet_data.sp.data1= 0x09;
    mipi_ops->send(&packet);
    
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting_chg_pump),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc= ARRAY_SIZE(pwr_setting_chg_pump); 
    packet.pPayload= pwr_setting_chg_pump;
    mipi_ops->send(&packet);
    
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, pwr_setting_internal_pwr),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc = ARRAY_SIZE(pwr_setting_internal_pwr); 
    packet.pPayload = pwr_setting_internal_pwr;
    mipi_ops->send(&packet);
    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc = ARRAY_SIZE(vcom_setting); 
    packet.pPayload = vcom_setting;
    mipi_ops->send(&packet);

    
    //    DSI_CMD_LONG(DSI_SRC_DATA_ID_GEN_LONG_WRITE, vcom_setting),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= GENERIC_LONG_WRITE;
    packet.ph.packet_data.lp.wc = ARRAY_SIZE(vcom_setting); 
    packet.pPayload = vcom_setting;
    mipi_ops->send(&packet);
    
    //    DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x29, 0x00),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
    packet.ph.packet_data.sp.data0=  0x29;
    packet.ph.packet_data.sp.data1=  0x00;
    mipi_ops->send(&packet);
    
    //  DSI_DLY_MS(17*3),
    mdelay(17*3);


    mipi_ops->start_video_mode();


    return 0;
}

static int lglh500wf1_panel_off(void)
{

    struct dsi_packet packet;

    mipi_ops->stop_video_mode();
    //  odin_mipi_hs_video_mode_stop(resource_index);

    //    odin_mipi_dsi_cmd(resource_index, lglh500wf1_dsi_off_command,
    //                ARRAY_SIZE(lglh500wf1_dsi_off_command));

    // DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x28, 0x00),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
    packet.ph.packet_data.sp.data0=  0x28;
    packet.ph.packet_data.sp.data1=  0x00;
    mipi_ops->send(&packet);

    // DSI_DLY_MS(17*3),
    mdelay(17*3);

    // DSI_CMD_SHORT(DSI_SRC_DATA_ID_DCS_SHORT_WRITE_NO_PARA, 0x10, 0x00),
    packet.escape_or_hs = DPHY_ESCAPE;
    packet.ph.di= DCS_SHORT_WRITE_NO_PARAMETERS;
    packet.ph.packet_data.sp.data0=  0x10;
    packet.ph.packet_data.sp.data1=  0x00;
    mipi_ops->send(&packet);

    // DSI_DLY_MS(17*9),
    mdelay(17*9);

    mipi_ops->close();

    lglh500wf1_power_off();

    return 0;
}

static bool lglh500wf1_mipi_dsi_init( struct mipi_hal_interface *hal_ops)
{
    pr_info("%s \n",__func__);

    mipi_ops = hal_ops;

    return true;

}

static bool  lglh500wf1_blank( bool blank)
{
    pr_info("%s \n",__func__);

    if(!mipi_ops)
        return false;

    if(blank) {
        lglh500wf1_panel_off();

    } else {
        lglh500wf1_panel_on();
    }

    return true;
}

static bool lglh500wf1_get_screen_info(struct panel_info *s_info)
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

static bool lglh500wf1_is_command_mode(void)
{
	return false;
}

static int lglh500wf1_probe(struct platform_device *pdev)
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

    mipi_ops.init= &lglh500wf1_mipi_dsi_init;
    mipi_ops.blank= &lglh500wf1_blank;
    mipi_ops.get_screen_info= &lglh500wf1_get_screen_info;
	mipi_ops.is_command_mode = &lglh500wf1_is_command_mode;
    mipi_device_ops_register(dsi_idx, &mipi_ops);

	desc->name = (char*)pdev->dev.driver->name;

	ret = gpio_request(desc->gpio_dsv_enable, "gpio_dsv_enable");
	if (ret < 0)
	{
		pr_debug("gpio_request failed for desc->gpio_dsv_enable gpio %d\n", desc->gpio_dsv_enable);
        ret = -EIO;
        goto err;
    }
    
	ret = gpio_request(desc->gpio_reset, "lcd_reset");
	if (ret < 0)
	{
		pr_debug("gpio_request failed for lcd_reset gpio %d\n", desc->gpio_reset);
        ret =-EIO;
        goto err;
    }

    return 0;

err:
    return ret;

}

static struct of_device_id lglh500wf1_match[]= {
        { .compatible="lglh500wfi"},
        {},
};
    

static struct platform_driver lglh500wf1_driver = {
    .probe      = lglh500wf1_probe,
    .driver         = {
        .name   = "lglh500wf1",
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(lglh500wf1_match),
    },

};

static int __init lglh500wf1_init(void)
{
	desc = vzalloc(sizeof(struct panel_desc));
	if (desc == NULL) {
		pr_err("vzalloc failed\n");
		return -1;
	}

#ifndef CONFIG_ODIN_DSS_FPGA
#ifdef CONFIG_ARABICA_CHECK_REV
    int revision_data = odin_pmic_revision_get();
    printk("DSS Revision Check Value %x \n", revision_data);

    if(revision_data != 0xd)
        platform_driver_register(&lglh500wf1_driver);

#endif
#endif
     return 0;
}

static void __exit lglh500wf1_exit(void)
{
    platform_driver_unregister(&lglh500wf1_driver);
	if (desc) {
		vfree(desc);
		desc = NULL;
	}
}



late_initcall(lglh500wf1_init);
module_exit(lglh500wf1_exit);

MODULE_AUTHOR("yunsh <yunsh@mtekvision.com>");
MODULE_DESCRIPTION("lglh500wf1 video mode Driver");
MODULE_LICENSE("GPL");

