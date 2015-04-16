/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/
#ifndef K3_FB_PANEL_H
#define K3_FB_PANEL_H

#include "k3_fb_def.h"
#include "k3_mipi_dsi.h"


#define PRIMARY_PANEL_IDX	(0)
#define EXTERNAL_PANEL_IDX	(1)
#define AUXILIARY_PANEL_IDX	(2)

/* panel type list */
#define PANEL_NO	0xffff	/* No Panel */
#define PANEL_LCDC	BIT(1)	/* internal LCDC type */
#define PANEL_HDMI	BIT(2)	/* HDMI TV */
#define PANEL_MIPI_VIDEO	BIT(3)	/* MIPI */
#define PANEL_MIPI_CMD	BIT(4)	/* MIPI */
#define PANEL_DUAL_MIPI_VIDEO	BIT(5)	/* DUAL MIPI */
#define PANEL_DUAL_MIPI_CMD	BIT(6)	/* DUAL MIPI */
#define PANEL_EDP	BIT(7)	/* LVDS */
#define PANEL_MIPI2RGB	BIT(8)	/* MIPI to RGB */
#define PANEL_RGB2MIPI	BIT(9)	/* RGB to MIPI */
#define PANEL_OFFLINECOMPOSER	BIT(10)	/* offline composer */
#define PANEL_WRITEBACK	BIT(11)	/* Wifi display */

/* dts initial */
#define DTS_FB_RESOURCE_INIT_READY	BIT(0)
#define DTS_PWM_READY	BIT(1)
//#define DTS_BLPWM_READY	BIT(2)
#define DTS_SPI_READY	BIT(3)
#define DTS_PANEL_PRIMARY_READY	BIT(4)
#define DTS_PANEL_EXTERNAL_READY	BIT(5)
#define DTS_PANEL_OFFLINECOMPOSER_READY	BIT(6)
#define DTS_PANEL_WRITEBACK_READY	BIT(7)

/* device name */
#define DEV_NAME_DSS_DPE		"dss_dpe"
#define DEV_NAME_SPI				"spi_dev0"
#define DEV_NAME_HDMI			"hdmi"
#define DEV_NAME_EDP				"edp"
#define DEV_NAME_MIPI2RGB		"mipi2rgb"
#define DEV_NAME_RGB2MIPI		"rgb2mipi"
#define DEV_NAME_MIPIDSI			"mipi_dsi"
#define DEV_NAME_FB				"k3_fb"
#define DEV_NAME_PWM			"k3_pwm"
#define DEV_NAME_BLPWM			"k3_blpwm"
#define DEV_NAME_LCD_BKL		"lcd_backlight0"

/* vcc name */
#define REGULATOR_PDP_NAME	"regulator_dsssubsys"
#define REGULATOR_SDP_NAME	"regulator_sdp"
#define REGULATOR_ADP_NAME	"regulator_adp"

/* irq name */
#define IRQ_PDP_NAME	"irq_pdp"
#define IRQ_SDP_NAME	"irq_sdp"
#define IRQ_ADP_NAME	"irq_adp"
#define IRQ_MCU_PDP_NAME	"irq_mcu_pdp"
#define IRQ_MCU_SDP_NAME	"irq_mcu_sdp"
#define IRQ_MCU_ADP_NAME	"irq_mcu_adp"
#define IRQ_DSI0_NAME	"irq_dsi0"
#define IRQ_DSI1_NAME	"irq_dsi1"

/* dts compatible */
#define DTS_COMP_FB_NAME	"hisilicon,k3fb"
#define DTS_COMP_PWM_NAME	"hisilicon,k3pwm"
#define DTS_COMP_BLPWM_NAME	"hisilicon,k3blpwm"

/* backlight type */
#define BL_SET_BY_NONE	BIT(0)
#define BL_SET_BY_PWM	BIT(1)
#define BL_SET_BY_BLPWM	BIT(2)
#define BL_SET_BY_MIPI	BIT(3)

/* LCD init step */
enum {
	LCD_INIT_NONE = 0,
	LCD_INIT_POWER_ON,
	LCD_INIT_LDI_SEND_SEQUENCE,
	LCD_INIT_MIPI_LP_SEND_SEQUENCE,
	LCD_INIT_MIPI_HS_SEND_SEQUENCE,
};

enum IFBC_TYPE {
	IFBC_TYPE_NON = 0x0, /* no compression*/
	IFBC_TYPE_HISI_0 = 0x1, /* 1/2 compression ratio,  non-sorting */
	IFBC_TYPE_HISI_1 = 0x9, /* 1/3 compression ratio,  non-sorting */
	IFBC_TYPE_HISI_2 = 0x11, /* 1/2 compression ratio,  sorting*/
	IFBC_TYPE_HISI_3 = 0x19, /* 1/3 compression ratio,  sorting */
	IFBC_TYPE_ORISE = 0x2,
	IFBC_TYPE_RSP_0 = 0x3, /* RSP 1/2 compression ratio*/
	IFBC_TYPE_RSP_1 = 0x23, /* RSP 1/3 compression ratio*/
	IFBC_TYPE_NOVATEK = 0x4,
	IFBC_TYPE_HIMAX = 0x5,

};

enum VSYNC_CTRL_TYPE {
	VSYNC_CTRL_NONE = 0x0,
	VSYNC_CTRL_ISR_OFF = 0x1,
	VSYNC_CTRL_MIPI_ULPS = 0x2,
	VSYNC_CTRL_CLK_OFF = 0x4,
	VSYNC_CTRL_VCC_OFF = 0x8,
};

/* resource desc */
struct resource_desc {
	u32 flag;
	char *name;
	u32 *value;
};

/* dtype for vcc */
enum {
	DTYPE_VCC_GET,
	DTYPE_VCC_PUT,
	DTYPE_VCC_ENABLE,
	DTYPE_VCC_DISABLE,
	DTYPE_VCC_SET_VOLTAGE,
};

/* vcc desc */
struct vcc_desc {
	int dtype;
	char *id;
	struct regulator **regulator;
	int min_uV;
	int max_uV;
};

/* pinctrl operation */
enum {
	DTYPE_PINCTRL_GET,
	DTYPE_PINCTRL_STATE_GET,
	DTYPE_PINCTRL_SET,
	DTYPE_PINCTRL_PUT,
};

/* pinctrl state */
enum {
	DTYPE_PINCTRL_STATE_DEFAULT,
	DTYPE_PINCTRL_STATE_IDLE,
};

/* pinctrl data */
struct pinctrl_data {
	struct pinctrl *p;
	struct pinctrl_state *pinctrl_def;
	struct pinctrl_state *pinctrl_idle;
};
struct pinctrl_cmd_desc {
	int dtype;
	struct pinctrl_data *pctrl_data;
	int mode;
};

/* dtype for gpio */
enum {
	DTYPE_GPIO_REQUEST,
	DTYPE_GPIO_FREE,
	DTYPE_GPIO_INPUT,
	DTYPE_GPIO_OUTPUT,
};

#ifdef CONFIG_LCD_CHECK_REG
enum {
	LCD_CHECK_REG_FAIL = -1,
	LCD_CHECK_REG_PASS = 0,
};
#endif

#ifdef CONFIG_LCD_MIPI_DETECT
enum {
	LCD_MIPI_DETECT_FAIL = -1,
	LCD_MIPI_DETECT_PASS = 0,
};
#endif

/* gpio desc */
struct gpio_desc {
	int dtype;
	int waittype;
	int wait;
	char *label;
	u32 *gpio;
	int value;
};

struct spi_cmd_desc {
	int reg_len;
	char *reg;
	int val_len;
	char *val;
	int waittype;
	int wait;
};

struct ldi_panel_info {
	u32 h_back_porch;
	u32 h_front_porch;
	u32 h_pulse_width;

	/*
	** note: vbp > 8 if used overlay compose,
	** also lcd vbp > 8 in lcd power on sequence
	*/
	u32 v_back_porch;
	u32 v_front_porch;
	u32 v_pulse_width;

	u8 hsync_plr;
	u8 vsync_plr;
	u8 pixelclk_plr;
	u8 data_en_plr;

	/* for cabc */
	u8 dpi0_overlap_size;
	u8 dpi1_overlap_size;
};

/* DSI PHY configuration */
struct mipi_dsi_phy_ctrl {
	u32 burst_mode;
	u32 lane_byte_clk;
	u32 clk_division;
	u32 clk_lane_lp2hs_time;
	u32 clk_lane_hs2lp_time;
	u32 data_lane_lp2hs_time;
	u32 data_lane_hs2lp_time;
	u32 hsfreqrange;
	u32 pll_unlocking_filter;
	u32 cp_current;
	u32 lpf_ctrl;
	u32 n_pll;
	u32 m_pll_1;
	u32 m_pll_2;
	u32 factors_effective;
};

struct mipi_panel_info {
	u8 vc;
	u8 lane_nums;
	u8 color_mode;
	u32 dsi_bit_clk; /* clock lane(p/n) */
	/*u32 dsi_pclk_rate;*/
};

struct sbl_panel_info {
	u32 strength_limit;
	u32 calibration_a;
	u32 calibration_b;
	u32 calibration_c;
	u32 calibration_d;
	u32 t_filter_control;
	u32 backlight_min;
	u32 backlight_max;
	u32 backlight_scale;
	u32 ambient_light_min;
	u32 filter_a;
	u32 filter_b;
	u32 logo_left;
	u32 logo_top;
};

struct k3_panel_info {
	u32 type;
	u32 xres;
	u32 yres;
	u32 width;
	u32 height;
	u8 bpp;
	u8 orientation;
	u8 bgr_fmt;
	u8 bl_set_type;
	u32 bl_min;
	u32 bl_max;
	unsigned long pxl_clk_rate;
	u8 ifbc_type;
	u8 vsync_ctrl_type;

	u8 lcd_init_step;

	u8 sbl_support;
	u8 frc_enable;
	u8 esd_enable;
	u8 dirty_region_updt_support;

	struct spi_device *spi_dev;
	struct ldi_panel_info ldi;
	struct mipi_panel_info mipi;
	struct sbl_panel_info smart_bl;
};

struct k3_fb_data_type;
struct dss_rect;

struct k3_fb_panel_data {
	struct k3_panel_info *panel_info;

	/* function entry chain */
	int (*set_fastboot) (struct platform_device *pdev);
	int (*on) (struct platform_device *pdev);
	int (*off) (struct platform_device *pdev);
	int (*remove) (struct platform_device *pdev);
	int (*set_backlight) (struct platform_device *pdev);
	int (*sbl_ctrl) (struct platform_device *pdev, int enable);
	int (*vsync_ctrl) (struct platform_device *pdev, int enable);

	int (*frc_handle)(struct platform_device *pdev, int fps);
	int (*esd_handle) (struct platform_device *pdev);
	int (*set_display_region)(struct platform_device *pdev, struct dss_rect *dirty);
	int (*set_timing)(struct platform_device *pdev);
#ifdef CONFIG_LCD_CHECK_REG
	int (*lcd_check_reg)(struct platform_device *pdev);
#endif
#ifdef CONFIG_LCD_MIPI_DETECT
	int (*lcd_mipi_detect)(struct platform_device *pdev);
#endif
	int (*set_disp_resolution)(struct platform_device *pdev);
	struct platform_device *next;
};


/*******************************************************************************
** FUNCTIONS PROTOTYPES
*/
extern u32 g_dts_resouce_ready;

int resource_cmds_tx(struct platform_device *pdev,
	struct resource_desc *cmds, int cnt);
int vcc_cmds_tx(struct platform_device *pdev, struct vcc_desc *cmds, int cnt);
int pinctrl_cmds_tx(struct platform_device *pdev, struct pinctrl_cmd_desc *cmds, int cnt);
int gpio_cmds_tx(struct gpio_desc *cmds, int cnt);
#if defined(CONFIG_ARCH_HI3630FPGA)
extern struct spi_device *g_spi_dev;
int spi_cmds_tx(struct spi_device *spi, struct spi_cmd_desc *cmds, int cnt);
#endif

int panel_next_set_fastboot(struct platform_device *pdev);
int panel_next_on(struct platform_device *pdev);
int panel_next_off(struct platform_device *pdev);
int panel_next_remove(struct platform_device *pdev);
int panel_next_set_backlight(struct platform_device *pdev);
int panel_next_sbl_ctrl(struct platform_device *pdev, int enable);
int panel_next_vsync_ctrl(struct platform_device *pdev, int enable);
int panel_next_frc_handle(struct platform_device *pdev, int fps);
int panel_next_esd_handle(struct platform_device *pdev);
int panel_next_set_display_region(struct platform_device *pdev, struct dss_rect *dirty);
#ifdef CONFIG_LCD_CHECK_REG
int panel_next_lcd_check_reg(struct platform_device *pdev);
#endif
#ifdef CONFIG_LCD_MIPI_DETECT
int panel_next_lcd_mipi_detect(struct platform_device *pdev);
#endif
int k3_pwm_set_backlight(struct k3_fb_data_type *k3fd);
int k3_pwm_off(struct platform_device *pdev);
int k3_pwm_on(struct platform_device *pdev);

int k3_blpwm_set_backlight(struct k3_fb_data_type *k3fd);
int k3_blpwm_off(struct platform_device *pdev);
int k3_blpwm_on(struct platform_device *pdev);

bool is_ldi_panel(struct k3_fb_data_type *k3fd);
bool is_mipi_cmd_panel(struct k3_fb_data_type *k3fd);
bool is_mipi_video_panel(struct k3_fb_data_type *k3fd);
bool is_mipi_panel(struct k3_fb_data_type *k3fd);
bool is_dual_mipi_panel(struct k3_fb_data_type *k3fd);
bool is_ifbc_panel(struct k3_fb_data_type *k3fd);
bool is_k3_writeback_panel(struct k3_fb_data_type *k3fd);

void k3_fb_device_set_status0(u32 status);
int k3_fb_device_set_status1(struct k3_fb_data_type *k3fd);
bool k3_fb_device_probe_defer(u32 panel_type);


#endif /* K3_FB_PANEL_H */
