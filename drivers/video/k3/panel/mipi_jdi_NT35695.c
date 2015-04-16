/* Copyright (c) 2008-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
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

#include "k3_fb.h"
#include "k3_mipi_dsi.h"
#include <linux/lcd_tuning.h>

#include <linux/of.h>

#define DTS_COMP_JDI_NT35695 "hisilicon,mipi_jdi_NT35695"
static struct k3_fb_panel_data jdi_panel_data;
static bool fastboot_display_enable = true;
extern u32 frame_count;
static struct k3_fb_data_type *g_k3fd = NULL;
extern unsigned char (*get_tp_color)(void);
static bool debug_enable = false;

/*******************************************************************************
** Power ON Sequence(sleep mode to Normal mode)
*/
static char caset_data[] = {
	0x2A,
	0x00,0x00,0x04, 0x37,
};

static char paset_data[] = {
	0x2B,
	0x00,0x00,0x07,0x7f,
};

static char tear_on[] = {
	0x35,
	0x00,
};

static char display_on[] = {
	0x29,
};

static char exit_sleep[] = {
	0x11,
};

static char bl_enable[] = {
	0x53,
	0x24,
};

static char te_line[] = {
	0x44,
	0x03, 0x80,
};
/*******************************************************************************
** Power OFF Sequence(Normal to power off)
*/

static char display_off[] = {
	0x28,
};

static char enter_sleep[] = {
	0x10,
};
/*******************************************************************************
** Display Effect Sequence(smart color, edge enhancement, smart contrast, cabc)
*/
//CMD2 Page2
static char cmd2_page2_0xFF[] = {
    0xFF,
    0x22,
};
//Non-reload
static char non_reload_0xFB[] = {
    0xFB,
    0x01,
};
//Color Enhancement Coefficient
static char color_enhancement_0x00[] = {
    0x00,
    0x28,
};
static char color_enhancement_0x01[] = {
    0x01,
    0x2C,
};
static char color_enhancement_0x02[] = {
    0x02,
    0x30,
};
static char color_enhancement_0x03[] = {
    0x03,
    0x34,
};
static char color_enhancement_0x04[] = {
    0x04,
    0x38,
};
static char color_enhancement_0x05[] = {
    0x05,
    0x3C,
};
static char color_enhancement_0x06[] = {
    0x06,
    0x40,
};
static char color_enhancement_0x07[] = {
    0x07,
    0x48,
};
static char color_enhancement_0x08[] = {
    0x08,
    0x4C,
};
static char color_enhancement_0x09[] = {
    0x09,
    0x50,
};
static char color_enhancement_0x0A[] = {
    0x0A,
    0x58,
};
static char color_enhancement_0x0B[] = {
    0x0B,
    0x60,
};
static char color_enhancement_0x0C[] = {
    0x0C,
    0x60,
};
static char color_enhancement_0x0D[] = {
    0x0D,
    0x58,
};
static char color_enhancement_0x0E[] = {
    0x0E,
    0x50,
};
static char color_enhancement_0x0F[] = {
    0x0F,
    0x48,
};
static char color_enhancement_0x10[] = {
    0x10,
    0x38,
};
static char color_enhancement_0x11[] = {
    0x11,
    0x28,
};
static char color_enhancement_0x12[] = {
    0x12,
    0x28,
};
static char color_enhancement_0x13[] = {
    0x13,
    0x28,
};
static char color_enhancement_0x32[] = {
    0x32,
    0x00,
};
static char color_enhancement_0x33[] = {
    0x33,
    0x00,
};
static char color_enhancement_0x34[] = {
    0x34,
    0x00,
};
static char color_enhancement_0x35[] = {
    0x35,
    0x02,
};
static char color_enhancement_0x36[] = {
    0x36,
    0x04,
};
static char color_enhancement_0x37[] = {
    0x37,
    0x06,
};
static char color_enhancement_0x38[] = {
    0x38,
    0x08,
};
static char color_enhancement_0x39[] = {
    0x39,
    0x0A,
};
static char color_enhancement_0x3A[] = {
    0x3A,
    0x0C,
};
static char color_enhancement_0x3B[] = {
    0x3B,
    0x0E,
};
static char color_enhancement_0x3F[] = {
    0x3F,
    0x10,
};
static char color_enhancement_0x40[] = {
    0x40,
    0x12,
};
static char color_enhancement_0x41[] = {
    0x41,
    0x14,
};
static char color_enhancement_0x42[] = {
    0x42,
    0x14,
};
static char color_enhancement_0x43[] = {
    0x43,
    0x12,
};
static char color_enhancement_0x44[] = {
    0x44,
    0x10,
};
static char color_enhancement_0x45[] = {
    0x45,
    0x0E,
};
static char color_enhancement_0x46[] = {
    0x46,
    0x0C,
};
static char color_enhancement_0x47[] = {
    0x47,
    0x0A,
};
static char color_enhancement_0x48[] = {
    0x48,
    0x08,
};
static char color_enhancement_0x49[] = {
    0x49,
    0x06,
};
static char color_enhancement_0x4A[] = {
    0x4A,
    0x04,
};
static char color_enhancement_0x4B[] = {
    0x4B,
    0x02,
};
static char color_enhancement_0x4C[] = {
    0x4C,
    0x00,
};
//Smart Color Ratio
static char smart_color_0x4D[] = {
    0x4D,
    0x00,
};
static char smart_color_0x4E[] = {
    0x4E,
    0x08,
};
static char smart_color_0x4F[] = {
    0x4F,
    0x10,
};
static char smart_color_0x50[] = {
    0x50,
    0x18,
};
static char smart_color_0x51[] = {
    0x51,
    0x20,
};
static char smart_color_0x52[] = {
    0x52,
    0x28,
};
//Vivid Color Disable
static char vivid_color_disable_0x1A[] = {
    0x1A,
    0x00,
};
//Smart Color Enable
static char smart_color_enable_0x53[] = {
    0x53,
    0x77,
};
//Contrast Disable
static char contrast_disable_0x56[] = {
    0x56,
    0x00,
};
//Sharpness
static char sharpness_0x68[] = {
    0x68,
    0x77,
};
static char sharpness_0x65[] = {
    0x65,
    0xA3,
};
static char sharpness_0x66[] = {
    0x66,
    0xC6,
};
static char sharpness_0x67[] = {
    0x67,
    0xF8,
};
static char sharpness_0x69[] = {
    0x69,
    0x02,
};
static char sharpness_0x97[] = {
    0x97,
    0x0A,
};
static char sharpness_0x98[] = {
    0x98,
    0x1C,
};
//CMD1
static char cmd1_0xFF[] = {
    0xFF,
    0x10,
};
/*static char display_effect_level1[] = {
    0x55,
    0x81,
};*/
static char display_effect_level2[] = {
    0x55,
    0x91,
};
/*static char display_effect_level3[] = {
    0x55,
    0xB1,
};*/

static struct dsi_cmd_desc jdi_display_effect_on_cmds[] = {
    //diplay effect
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(cmd2_page2_0xFF), cmd2_page2_0xFF},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(non_reload_0xFB), non_reload_0xFB},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x00), color_enhancement_0x00},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x01), color_enhancement_0x01},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x02), color_enhancement_0x02},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x03), color_enhancement_0x03},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x04), color_enhancement_0x04},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x05), color_enhancement_0x05},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x06), color_enhancement_0x06},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x07), color_enhancement_0x07},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x08), color_enhancement_0x08},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x09), color_enhancement_0x09},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0A), color_enhancement_0x0A},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0B), color_enhancement_0x0B},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0C), color_enhancement_0x0C},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0D), color_enhancement_0x0D},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0E), color_enhancement_0x0E},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x0F), color_enhancement_0x0F},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x10), color_enhancement_0x10},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x11), color_enhancement_0x11},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x12), color_enhancement_0x12},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x13), color_enhancement_0x13},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x32), color_enhancement_0x32},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x33), color_enhancement_0x33},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x34), color_enhancement_0x34},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x35), color_enhancement_0x35},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x36), color_enhancement_0x36},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x37), color_enhancement_0x37},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x38), color_enhancement_0x38},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x39), color_enhancement_0x39},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x3A), color_enhancement_0x3A},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x3B), color_enhancement_0x3B},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x3F), color_enhancement_0x3F},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x40), color_enhancement_0x40},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x41), color_enhancement_0x41},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x42), color_enhancement_0x42},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x43), color_enhancement_0x43},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x44), color_enhancement_0x44},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x45), color_enhancement_0x45},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x46), color_enhancement_0x46},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x47), color_enhancement_0x47},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x48), color_enhancement_0x48},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x49), color_enhancement_0x49},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x4A), color_enhancement_0x4A},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x4B), color_enhancement_0x4B},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(color_enhancement_0x4C), color_enhancement_0x4C},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x4D), smart_color_0x4D},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x4E), smart_color_0x4E},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x4F), smart_color_0x4F},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x50), smart_color_0x50},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x51), smart_color_0x51},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_0x52), smart_color_0x52},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(vivid_color_disable_0x1A), vivid_color_disable_0x1A},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(smart_color_enable_0x53), smart_color_enable_0x53},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(contrast_disable_0x56), contrast_disable_0x56},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x68), sharpness_0x68},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x65), sharpness_0x65},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x66), sharpness_0x66},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x67), sharpness_0x67},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x69), sharpness_0x69},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x97), sharpness_0x97},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(sharpness_0x98), sharpness_0x98},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(cmd1_0xFF), cmd1_0xFF},
    {DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
        sizeof(display_effect_level2), display_effect_level2},
};

static struct dsi_cmd_desc jdi_display_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(tear_on), tear_on},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(caset_data), caset_data},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(paset_data), paset_data},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(te_line), te_line},
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc jdi_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 60, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

/*******************************************************************************
** LCD VCC
*/
#define VCC_LCDIO_NAME		"lcdio-vcc"
#define VCC_LCDANALOG_NAME	"lcdanalog-vcc"

static struct regulator *vcc_lcdio;
static struct regulator *vcc_lcdanalog;

static struct vcc_desc jdi_lcd_vcc_init_cmds[] = {
	/* vcc get */
	{DTYPE_VCC_GET, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
	{DTYPE_VCC_GET, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0},
	/* vcc set voltage */
	{DTYPE_VCC_SET_VOLTAGE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 3100000, 3100000},
};

static struct vcc_desc jdi_lcd_vcc_finit_cmds[] = {
	/* vcc put */
	{DTYPE_VCC_PUT, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
	{DTYPE_VCC_PUT, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0},
};

static struct vcc_desc jdi_lcd_vcc_enable_cmds[] = {
	/* vcc enable */
	{DTYPE_VCC_ENABLE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0},
	{DTYPE_VCC_ENABLE, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
};

static struct vcc_desc jdi_lcd_vcc_disable_cmds[] = {
	/* vcc disable */
	{DTYPE_VCC_DISABLE, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
	{DTYPE_VCC_DISABLE, VCC_LCDANALOG_NAME, &vcc_lcdanalog, 0, 0},
};

/*******************************************************************************
** LCD IOMUX
*/
static struct pinctrl_data pctrl;

static struct pinctrl_cmd_desc jdi_lcd_pinctrl_init_cmds[] = {
	{DTYPE_PINCTRL_GET, &pctrl, 0},
	{DTYPE_PINCTRL_STATE_GET, &pctrl, DTYPE_PINCTRL_STATE_DEFAULT},
	{DTYPE_PINCTRL_STATE_GET, &pctrl, DTYPE_PINCTRL_STATE_IDLE},
};

static struct pinctrl_cmd_desc jdi_lcd_pinctrl_normal_cmds[] = {
	{DTYPE_PINCTRL_SET, &pctrl, DTYPE_PINCTRL_STATE_DEFAULT},
};

static struct pinctrl_cmd_desc jdi_lcd_pinctrl_lowpower_cmds[] = {
	{DTYPE_PINCTRL_SET, &pctrl, DTYPE_PINCTRL_STATE_IDLE},
};

/*******************************************************************************
** LCD GPIO
*/
#define GPIO_LCD_BL_ENABLE_NAME	"gpio_lcd_bl_enable"
#define GPIO_LCD_RESET_NAME	"gpio_lcd_reset"
#define GPIO_LCD_ID_NAME	"gpio_lcd_id"
#define GPIO_LCD_P5V5_ENABLE_NAME	"gpio_lcd_p5v5_enable"
#define GPIO_LCD_N5V5_ENABLE_NAME "gpio_lcd_n5v5_enable"

static u32 gpio_lcd_bl_enable;  /*gpio_4_3, gpio_035*/
static u32 gpio_lcd_reset;  /*gpio_4_5, gpio_037*/
static u32 gpio_lcd_id;  /*gpio_4_6, gpio_038*/
static u32 gpio_lcd_p5v5_enable;  /*gpio_5_1, gpio_041*/
static u32 gpio_lcd_n5v5_enable;  /*gpio_5_2, gpio_042*/

static struct gpio_desc jdi_lcd_gpio_request_cmds[] = {
	/* backlight enable */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	/* reset */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 0},
	/* lcd id */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID_NAME, &gpio_lcd_id, 0},
	/* AVDD_5.5V */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_P5V5_ENABLE_NAME, &gpio_lcd_p5v5_enable, 0},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_N5V5_ENABLE_NAME, &gpio_lcd_n5v5_enable, 0},
};

static struct gpio_desc jdi_lcd_gpio_free_cmds[] = {
	/* backlight enable */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	/* reset */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 0},
	/* lcd id */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID_NAME, &gpio_lcd_id, 0},
	/* AVDD_5.5V */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_P5V5_ENABLE_NAME, &gpio_lcd_p5v5_enable, 0},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_N5V5_ENABLE_NAME, &gpio_lcd_n5v5_enable, 0},
};

static struct gpio_desc jdi_lcd_gpio_normal_cmds[] = {
	/* AVDD_5.5V*/
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_P5V5_ENABLE_NAME, &gpio_lcd_p5v5_enable, 1},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 15,
		GPIO_LCD_N5V5_ENABLE_NAME, &gpio_lcd_n5v5_enable, 1},
	/* reset */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 15,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 15,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 1},
	/* backlight enable */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 1},
	/* lcd id */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_ID_NAME, &gpio_lcd_id, 0},
};

static struct gpio_desc jdi_lcd_gpio_lowpower_cmds[] = {
	/* backlight enable */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	/* AVEE_-5.5V */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_N5V5_ENABLE_NAME, &gpio_lcd_n5v5_enable, 0},
	/* AVDD_5.5V*/
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_P5V5_ENABLE_NAME, &gpio_lcd_p5v5_enable, 0},
	/* reset */
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 0},
	/* backlight enable input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_US, 100,
		GPIO_LCD_BL_ENABLE_NAME, &gpio_lcd_bl_enable, 0},
	/* AVEE_-5.5V input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_N5V5_ENABLE_NAME, &gpio_lcd_n5v5_enable, 0},
	/* AVDD_5.5V input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_P5V5_ENABLE_NAME, &gpio_lcd_p5v5_enable, 0},
	/* reset input */
	{DTYPE_GPIO_INPUT, WAIT_TYPE_US, 100,
		GPIO_LCD_RESET_NAME, &gpio_lcd_reset, 0},
};

static ssize_t k3_lcd_model_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "jdi_OTM1902B 5.0' CMD TFT 1080 x 1920\n");
}

static ssize_t jdi_frame_count_show(struct device *dev,
          struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", frame_count);
}

static struct device_attribute k3_lcd_class_attrs[] = {
	__ATTR(lcd_model, S_IRUGO, k3_lcd_model_show, NULL),
	__ATTR(frame_count, S_IRUGO, jdi_frame_count_show, NULL),
	__ATTR_NULL,
};

static int mipi_jdi_panel_set_fastboot(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

static int mipi_jdi_panel_on(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;
	char __iomem *mipi_dsi0_base = NULL;
	u32 pkg_status = 0, power_status = 0, try_times = 0;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +!\n", k3fd->index);

	pinfo = &(k3fd->panel_info);
	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		/* lcd vcc enable */
		if (likely(!fastboot_display_enable)) {
			vcc_cmds_tx(pdev, jdi_lcd_vcc_enable_cmds,
				ARRAY_SIZE(jdi_lcd_vcc_enable_cmds));
		}
		pinfo->lcd_init_step = LCD_INIT_MIPI_LP_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_LP_SEND_SEQUENCE) {
		/* lcd pinctrl normal */
		pinctrl_cmds_tx(pdev, jdi_lcd_pinctrl_normal_cmds,
			ARRAY_SIZE(jdi_lcd_pinctrl_normal_cmds));

		/* lcd gpio request */
		gpio_cmds_tx(jdi_lcd_gpio_request_cmds, \
			ARRAY_SIZE(jdi_lcd_gpio_request_cmds));

		/* lcd gpio normal */
		if (likely(!fastboot_display_enable)) {
			gpio_cmds_tx(jdi_lcd_gpio_normal_cmds, \
				ARRAY_SIZE(jdi_lcd_gpio_normal_cmds));
			mipi_dsi_cmds_tx(jdi_display_effect_on_cmds, \
				ARRAY_SIZE(jdi_display_effect_on_cmds), mipi_dsi0_base);
		}

		mipi_dsi_cmds_tx(jdi_display_on_cmds, \
			ARRAY_SIZE(jdi_display_on_cmds), mipi_dsi0_base);

		if (likely(!fastboot_display_enable)) {
			outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0A06);
			pkg_status = inp32(mipi_dsi0_base + MIPIDSI_CMD_PKT_STATUS_OFFSET);
			while (pkg_status & 0x10) {
				udelay(50);
				if (++try_times > 100) {
					K3_FB_ERR("Read lcd power status timeout\n");
					break;
				}
				pkg_status = inp32(mipi_dsi0_base + MIPIDSI_CMD_PKT_STATUS_OFFSET);
			}
			power_status = inp32(mipi_dsi0_base + MIPIDSI_GEN_PLD_DATA_OFFSET);

			K3_FB_INFO("LCD Power State = 0x%x\n", power_status);
		}
		fastboot_display_enable = false;
		debug_enable = true;
		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE) {
		;
	} else
		K3_FB_ERR("failed to init lcd!\n");

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM)
		k3_pwm_on(pdev);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM)
		k3_blpwm_on(pdev);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI)
		K3_FB_INFO("Set backlight by mipi\n");
	else
		K3_FB_ERR("No such bl_set_type!\n");

	K3_FB_INFO("fb%d, -!\n", k3fd->index);

	return 0;
}

static int mipi_jdi_panel_off(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_INFO("fb%d, +!\n", k3fd->index);

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM)
		k3_pwm_off(pdev);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM)
		k3_blpwm_off(pdev);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI)
		K3_FB_INFO("Set backlight by mipi\n");
	else
		K3_FB_ERR("No such bl_set_type!\n");

	/* lcd display off sequence */
	mipi_dsi_cmds_tx(jdi_display_off_cmds, \
		ARRAY_SIZE(jdi_display_off_cmds), mipi_dsi0_base);

	/* lcd gpio lowpower */
	gpio_cmds_tx(jdi_lcd_gpio_lowpower_cmds, \
		ARRAY_SIZE(jdi_lcd_gpio_lowpower_cmds));
	/* lcd gpio free */
	gpio_cmds_tx(jdi_lcd_gpio_free_cmds, \
		ARRAY_SIZE(jdi_lcd_gpio_free_cmds));

	/* lcd pinctrl lowpower */
	pinctrl_cmds_tx(pdev, jdi_lcd_pinctrl_lowpower_cmds,
		ARRAY_SIZE(jdi_lcd_pinctrl_lowpower_cmds));

	/* lcd vcc disable */
	vcc_cmds_tx(pdev, jdi_lcd_vcc_disable_cmds,
		ARRAY_SIZE(jdi_lcd_vcc_disable_cmds));

	K3_FB_INFO("fb%d, -!\n", k3fd->index);

	return 0;
}

static int mipi_jdi_panel_remove(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);

	if (!k3fd) {
		return 0;
	}

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	/* lcd vcc finit */
	vcc_cmds_tx(pdev, jdi_lcd_vcc_finit_cmds,
		ARRAY_SIZE(jdi_lcd_vcc_finit_cmds));

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

static int k3_mipi_set_backlight(struct k3_fb_data_type *k3fd)
{
	char __iomem *mipi_dsi0_base = NULL;

	u8 bl_level_adjust[2] = {
		0x51,
		0x00,
	};

	struct dsi_cmd_desc  jdi_bl_level_adjust[] = {
		{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
			sizeof(bl_level_adjust), bl_level_adjust},
	};

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	bl_level_adjust[1] = (u8)k3fd->bl_level;

	mipi_dsi_cmds_tx(jdi_bl_level_adjust, \
		ARRAY_SIZE(jdi_bl_level_adjust), mipi_dsi0_base);

	return 0;
}

static int mipi_jdi_panel_set_backlight(struct platform_device *pdev)
{
	int ret = 0;

	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	if (unlikely(debug_enable)) {
		K3_FB_INFO("Set backlight to %d\n", k3fd->bl_level);
		debug_enable = false;
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM)
		ret = k3_pwm_set_backlight(k3fd);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM)
		ret = k3_blpwm_set_backlight(k3fd);
	else if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI)
		ret = k3_mipi_set_backlight(k3fd);
	else
		K3_FB_ERR("No such bl_set_type!\n");

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return ret;
}

static char lcd_disp_x[] = {
	0x2A,
	0x00, 0x00,0x04,0x37
};

static char lcd_disp_y[] = {
	0x2B,
	0x00, 0x00,0x07,0x7F
};

static struct dsi_cmd_desc set_display_address[] = {
	{DTYPE_DCS_LWRITE, 0, 5, WAIT_TYPE_US,
		sizeof(lcd_disp_x), lcd_disp_x},
	{DTYPE_DCS_LWRITE, 0, 5, WAIT_TYPE_US,
		sizeof(lcd_disp_y), lcd_disp_y},
};

static int mipi_jdi_panel_set_display_region(struct platform_device *pdev,
	struct dss_rect *dirty)
{
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL || dirty == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	lcd_disp_x[1] = (dirty->x >> 8) & 0xff;
	lcd_disp_x[2] = dirty->x & 0xff;
	lcd_disp_x[3] = ((dirty->x + dirty->w - 1) >> 8) & 0xff;
	lcd_disp_x[4] = (dirty->x + dirty->w - 1) & 0xff;
	lcd_disp_y[1] = (dirty->y >> 8) & 0xff;
	lcd_disp_y[2] = dirty->y & 0xff;
	lcd_disp_y[3] = ((dirty->y + dirty->h - 1) >> 8) & 0xff;
	lcd_disp_y[4] = (dirty->y + dirty->h - 1) & 0xff;

	mipi_dsi_cmds_tx(set_display_address, \
		ARRAY_SIZE(set_display_address), mipi_dsi0_base);

	return 0;
}

#ifdef CONFIG_LCD_CHECK_REG
static int mipi_jdi_check_reg(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 read_value = 0xFF;
	int ret = LCD_CHECK_REG_PASS;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0A06);
	if (!mipi_dsi_read(&read_value, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x0A timeout\n");
		goto check_fail;
	}
	if ((0x9c != (read_value & 0xFF))) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Register 0x0A value is wrong: 0x%x\n", read_value);
		goto check_fail;
	}

	read_value = 0xFF;
	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0B06);
	if (!mipi_dsi_read(&read_value, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x0B timeout\n");
		goto check_fail;
	}
	if ((0x00 != (read_value & 0xFF))) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Register 0x0B value is wrong: 0x%x\n", read_value);
		goto check_fail;
	}

	read_value = 0xFF;
	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0C06);
	if (!mipi_dsi_read(&read_value, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x0C timeout\n");
		goto check_fail;
	}
	if ((0x77 != (read_value & 0xFF))) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Register 0x0C value is wrong: 0x%x\n", read_value);
		goto check_fail;
	}

	read_value = 0xFF;
	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0D06);
	if (!mipi_dsi_read(&read_value, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x0D timeout\n");
		goto check_fail;
	}
	if ((0x00 != (read_value & 0xFF))) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Register 0x0D value is wrong: 0x%x\n", read_value);
		goto check_fail;
	}

check_fail:
	return ret;
}
#endif

#ifdef CONFIG_LCD_MIPI_DETECT
static int mipi_jdi_mipi_detect(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 err_bit = 0;
	u32 err_num = 0;
	int ret;
	char __iomem *mipi_dsi0_base = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0E06);
	if (!mipi_dsi_read(&err_bit, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x0E timeout\n");
		goto read_reg_failed;
	}

	outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0506);
	if (!mipi_dsi_read(&err_num, mipi_dsi0_base)) {
		ret = LCD_CHECK_REG_FAIL;
		K3_FB_ERR("Read register 0x05 timeout\n");
		goto read_reg_failed;
	}

	ret = ((err_bit & 0xFF) << 8) | (err_num & 0xFF);
	if (ret != 0x8000)
		K3_FB_INFO("ret_val: 0x%x\n", ret);
read_reg_failed:
	return ret;
}
#endif

static struct k3_panel_info jdi_panel_info = {0};
static struct k3_fb_panel_data jdi_panel_data = {
	.panel_info = &jdi_panel_info,
	.set_fastboot = mipi_jdi_panel_set_fastboot,
	.on = mipi_jdi_panel_on,
	.off = mipi_jdi_panel_off,
	.remove = mipi_jdi_panel_remove,
	.set_backlight = mipi_jdi_panel_set_backlight,
	.set_display_region = mipi_jdi_panel_set_display_region,
#ifdef CONFIG_LCD_CHECK_REG
	.lcd_check_reg = mipi_jdi_check_reg,
#endif
#ifdef CONFIG_LCD_MIPI_DETECT
	.lcd_mipi_detect = mipi_jdi_mipi_detect,
#endif
};

unsigned char get_mipi_jdi_NT35695_tp_color(void)
{
	u32 out = 0;
	u8 lcd_chip_id = 0xFF;
	char __iomem *mipi_dsi0_base = NULL;
	u8 retry_nums = 3;

	if (!g_k3fd->panel_power_on) {
		K3_FB_ERR("power off\n");
		return lcd_chip_id;
	}

	/*enable vsync to avoid mipi into ULPS*/
	k3fb_activate_vsync(g_k3fd);
	do {
		mipi_dsi0_base = g_k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;
		/*Read LCD ID*/
		outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0xDA06);
		if (!mipi_dsi_read(&out, mipi_dsi0_base)) {
			K3_FB_ERR("Read register 0xDA timeout\n");
		} else
			break;
		msleep(5);
	} while (-- retry_nums);

	k3fb_deactivate_vsync(g_k3fd);

	if (!retry_nums) {
		K3_FB_ERR("Read register 0xDA Failed\n");
		return lcd_chip_id;
	}

	lcd_chip_id = (u8)out;
	K3_FB_INFO("LCD Id = %d, mipi_dsi0_base = %p\n", lcd_chip_id, mipi_dsi0_base);
	return lcd_chip_id;
};

static u32 get_bl_type(struct device_node *np)
{
	u32 bl_type = -1;
	int ret = 0;
	ret = of_property_read_u32(np, "lcd-bl-type", &bl_type);
	if (ret) {
		K3_FB_ERR("of_property_read_u32 fail\n");
		return -1;
	}
	return bl_type;
}

static int mipi_jdi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_panel_info *pinfo = NULL;
	struct device_node *np = NULL;
	struct class *k3_lcd_class;
	struct platform_device *this_dev = NULL;

	if (k3_fb_device_probe_defer(PANEL_MIPI_CMD))
		goto err_probe_defer;

	K3_FB_DEBUG("+.\n");

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_JDI_NT35695);
	if (!np) {
		K3_FB_ERR("NOT FOUND device node %s!\n", DTS_COMP_JDI_NT35695);
		goto err_return;
	}

	gpio_lcd_bl_enable = of_get_named_gpio(np, "gpios", 0);  /*gpio_4_3, gpio_035*/
	gpio_lcd_reset = of_get_named_gpio(np, "gpios", 1);  /*gpio_4_5, gpio_037*/
	gpio_lcd_id = of_get_named_gpio(np, "gpios", 2);  /*gpio_4_6, gpio_038*/
	gpio_lcd_p5v5_enable = of_get_named_gpio(np, "gpios", 3);  /*gpio_5_1, gpio_041*/
	gpio_lcd_n5v5_enable = of_get_named_gpio(np, "gpios", 4);  /*gpio_5_2, gpio_042*/

	pdev->id = 1;
	/* init lcd panel info */
	pinfo = jdi_panel_data.panel_info;
	memset(pinfo, 0, sizeof(struct k3_panel_info));
	pinfo->xres = 1080;
	pinfo->yres = 1920;
	pinfo->width  = 76;  //mm
	pinfo->height = 135; //mm
	pinfo->type = PANEL_MIPI_CMD;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = LCD_RGB888;
	pinfo->bgr_fmt = LCD_RGB;

	if (get_bl_type(np) == 1)
		pinfo->bl_set_type = BL_SET_BY_MIPI;
	else
		pinfo->bl_set_type = BL_SET_BY_PWM;

	pinfo->bl_min = 1;
	pinfo->bl_max = 255;
	pinfo->vsync_ctrl_type = (VSYNC_CTRL_ISR_OFF |
		VSYNC_CTRL_MIPI_ULPS);

	pinfo->frc_enable = 0;
	pinfo->esd_enable = 0;
	pinfo->dirty_region_updt_support = 0;

	pinfo->sbl_support = 0;
	pinfo->smart_bl.strength_limit = 128;
	pinfo->smart_bl.calibration_a = 60;
	pinfo->smart_bl.calibration_b = 95;
	pinfo->smart_bl.calibration_c = 5;
	pinfo->smart_bl.calibration_d = 1;
	pinfo->smart_bl.t_filter_control = 5;
	pinfo->smart_bl.backlight_min = 480;
	pinfo->smart_bl.backlight_max = 4096;
	pinfo->smart_bl.backlight_scale = 0xff;
	pinfo->smart_bl.ambient_light_min = 14;
	pinfo->smart_bl.filter_a = 1738;
	pinfo->smart_bl.filter_b = 6;
	pinfo->smart_bl.logo_left = 0;
	pinfo->smart_bl.logo_top = 0;

	pinfo->ifbc_type = IFBC_TYPE_NON;

	pinfo->ldi.h_back_porch = 23;
	pinfo->ldi.h_front_porch = 50;
	pinfo->ldi.h_pulse_width = 20;
	pinfo->ldi.v_back_porch = 12;
	pinfo->ldi.v_front_porch = 14;
	pinfo->ldi.v_pulse_width = 4;

	pinfo->mipi.lane_nums = DSI_4_LANES;
	pinfo->mipi.color_mode = DSI_24BITS_1;
	pinfo->mipi.vc = 0;
	pinfo->mipi.dsi_bit_clk = 480;

	pinfo->pxl_clk_rate = 150*1000000;

	/* lcd vcc init */
	ret = vcc_cmds_tx(pdev, jdi_lcd_vcc_init_cmds,
		ARRAY_SIZE(jdi_lcd_vcc_init_cmds));
	if (ret != 0) {
		K3_FB_ERR("LCD vcc init failed!\n");
		goto err_return;
	}

	if (fastboot_display_enable) {
		vcc_cmds_tx(pdev, jdi_lcd_vcc_enable_cmds,
			ARRAY_SIZE(jdi_lcd_vcc_enable_cmds));
	}

	/* lcd pinctrl init */
	ret = pinctrl_cmds_tx(pdev, jdi_lcd_pinctrl_init_cmds,
		ARRAY_SIZE(jdi_lcd_pinctrl_init_cmds));
	if (ret != 0) {
	        K3_FB_ERR("Init pinctrl failed, defer\n");
		goto err_return;
	}

	/* alloc panel device data */
	ret = platform_device_add_data(pdev, &jdi_panel_data,
		sizeof(struct k3_fb_panel_data));
	if (ret) {
		K3_FB_ERR("platform_device_add_data failed!\n");
		goto err_device_put;
	}

	this_dev = k3_fb_add_device(pdev);

	BUG_ON(this_dev == NULL);
	g_k3fd = platform_get_drvdata(this_dev);
	BUG_ON(g_k3fd == NULL);

	k3_lcd_class = class_create(THIS_MODULE, "k3_lcd");
	if (IS_ERR(k3_lcd_class)) {
		K3_FB_ERR("k3_lcd class create failed\n");
		return 0;
	}
	k3_lcd_class->dev_attrs = k3_lcd_class_attrs;

	if (IS_ERR(device_create(k3_lcd_class, &pdev->dev, 0, NULL, "lcd_info"))) {
		K3_FB_ERR("lcd_info create failed\n");
		class_destroy(k3_lcd_class);
		return 0;
	}

	get_tp_color = get_mipi_jdi_NT35695_tp_color;

	K3_FB_DEBUG("-.\n");

	return 0;

err_device_put:
	platform_device_put(pdev);
err_return:
	return ret;
err_probe_defer:
	return -EPROBE_DEFER;

	return ret;
}

static const struct of_device_id k3_panel_match_table[] = {
	{
		.compatible = DTS_COMP_JDI_NT35695,
		.data = NULL,
	},
};
MODULE_DEVICE_TABLE(of, k3_panel_match_table);

static struct platform_driver this_driver = {
	.probe = mipi_jdi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "mipi_jdi_NT35695",
		.of_match_table = of_match_ptr(k3_panel_match_table),
	},
};

static int __init mipi_jdi_panel_init(void)
{
	int ret = 0;

	if (read_lcd_type() != JDI_NT35695_LCD) {
		K3_FB_INFO("lcd type is not jdi_NT35695_LCD, return!\n");
		return ret;
	}

	ret = platform_driver_register(&this_driver);
	if (ret) {
		K3_FB_ERR("platform_driver_register failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(mipi_jdi_panel_init);
