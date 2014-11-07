#include "panel_device.h"

/*************** odin panel_data **********************/
static struct odin_rgb_panel_data at070tn94_panel = {
	.name			= "at070tn94_panel",
/*	.reset_gpio		= GPIO_LCD_RESET,*/
	.use_ext_te		= false,
 	.ext_te_gpio	= false,
	.esd_interval	= false,
	.set_backlight	= NULL
};

static struct odin_rgb_panel_data lms480_panel = {
	.name			= "lms480_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.set_backlight 		= NULL

};

#if 0
static struct odin_dsi_panel_data dsi_slimport_panel = {
	.name			= "dsi_slimport_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.set_backlight 		= NULL

};
#endif

static struct odin_dsi_panel_data dsi_lglh500wf1_panel = {
	.name			= "dsi_lglh500wf1_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.set_backlight 		= NULL

};

static struct odin_dsi_panel_data dsi_lglh520wf1_panel = {
	.name			= "dsi_lglh520wf1_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.set_backlight		= NULL
};

/*
              
                                
                                 
*/
static struct odin_dsi_panel_data dsi_lglh600wf2_panel = {
	.name			= "dsi_lglh600wf2_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.set_backlight		= NULL
};

#if 0
static struct odin_hdmi_panel_data hdmi_slimport_panel = {
	.name			= "hdmi_slimport_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.get_backlight 		= NULL,
	.set_backlight 		= NULL

};
#endif

static struct odin_hdmi_panel_data hdmi_panel = {
	.name			= "hdmi_panel",
	.use_ext_te		= false,
	.ext_te_gpio		= false,
	.esd_interval		= false,
	.get_backlight 		= NULL,
	.set_backlight 		= NULL

};


/*              */

/******* odin_dss_device (lcd0/lcd1/hdmi) *************/
struct odin_dss_device at070tn94_device = {
	.name			= "lcd0",
	.driver_name	= "at070tn94_panel",
	.type			= ODIN_DISPLAY_TYPE_RGB,
	.data = &at070tn94_panel,
	.pixel_size	= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD0,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};

struct odin_dss_device hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type 			= ODIN_DISPLAY_TYPE_HDMI,
	.channel		= ODIN_DSS_CHANNEL_HDMI,
};

struct odin_dss_device mem2mem_device = {
	.name			= "mem2mem",
	.driver_name		= "mem2mem",
	.type			= ODIN_DISPLAY_TYPE_MEM2MEM,
	.pixel_size		= 24,
/*	.channel		= ODIN_DSS_CHANNEL_LCD1,*/
        .platform_enable        = NULL,
        .platform_disable       = NULL,
};

#if defined (CONFIG_PANEL_LGLH500WF1)
struct odin_dss_device dsi_lglh500wf1_device = {
	.name			= "lcd0",
	.type			= ODIN_DISPLAY_TYPE_DSI,
	.driver_name		= "dsi_lglh500wf1_panel",
	.data			= &dsi_lglh500wf1_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD0,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

#if defined (CONFIG_SLIMPORT_ANX7805)
struct odin_dss_device dsi_slimport_device = {
	.name			= "lcd1",
	.type			= ODIN_DISPLAY_TYPE_DSI,
	.driver_name	= "dsi_slimport_panel",
	.data			= &dsi_slimport_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD1,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

#if defined (CONFIG_PANEL_LMS480)
struct odin_dss_device lms480_device = {
	.name			= "lcd0",
	.type			= ODIN_DISPLAY_TYPE_RGB,
	.driver_name		= "lms480_panel",
	.data			= &lms480_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD0,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

#if defined (CONFIG_PANEL_LGLH520WF1)
struct odin_dss_device dsi_lglh520wf1_device = {
	.name			= "lcd0",
	.type			= ODIN_DISPLAY_TYPE_DSI,
	.driver_name		= "dsi_lglh520wf1_panel",
	.data			= &dsi_lglh520wf1_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD0,
	.phy.rgb.data_lines	= 24,
	.caps			= ODIN_DSS_DISPLAY_CAP_MANUAL_UPDATE,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

/*
              
                                
                                 
*/
#if defined (CONFIG_PANEL_LGLH600WF2)
struct odin_dss_device dsi_lglh600wf2_device = {
	.name			= "lcd0",
	.type			= ODIN_DISPLAY_TYPE_DSI,
	.driver_name		= "dsi_lglh600wf2_panel",
	.data			= &dsi_lglh600wf2_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_LCD0,
	.phy.rgb.data_lines	= 24,
	.caps			= ODIN_DSS_DISPLAY_CAP_MANUAL_UPDATE,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
#if 0
struct odin_dss_device hdmi_slimport_device = {
	.name			= "lcd2",
	.type			= ODIN_DISPLAY_TYPE_HDMI,
	.driver_name		= "hdmi_slimport_panel",
	.data			= &hdmi_slimport_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_HDMI,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#else
struct odin_dss_device hdmi_panel_device = {
	.name			= "lcd2",
	.type			= ODIN_DISPLAY_TYPE_HDMI,
	.driver_name		= "hdmi_panel",
	.data			= &hdmi_panel,
	.pixel_size		= 24,
	.channel		= ODIN_DSS_CHANNEL_HDMI,
	.phy.rgb.data_lines	= 24,
	.platform_enable	= NULL,
	.platform_disable	= NULL,
};
#endif

#endif

/*              */
