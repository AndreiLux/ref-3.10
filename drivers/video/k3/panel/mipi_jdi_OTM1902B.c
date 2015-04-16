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
#include <linux/log_jank.h>

extern struct dsm_client *lcd_dclient;

#define DTS_COMP_JDI_OTM1902B "hisilicon,mipi_jdi_OTM1902B"
static struct k3_fb_panel_data jdi_panel_data;
static bool fastboot_display_enable = true;
extern u32 frame_count;
static struct k3_fb_data_type *g_k3fd = NULL;
extern unsigned char (*get_tp_color)(void);
static bool debug_enable = false;

/*******************************************************************************
** Power ON Sequence(sleep mode to Normal mode)
*/
static char adrsft1[] = {
	0x00,
	0x00,
};

static char cmd2_ena1[] = {
	0xff,
	0x19, 0x02, 0x01, 0x00,
};

static char adrsft2[] = {
	0x00,
	0x80,
};

static char cmd2_ena2[] = {
	0xff,
	0x19, 0x02,
};

static char adrsft3[] = {
	0x00,
	0x83,
};

static char adrsft4[] = {
	0xf3,
	0xca,
};

static char adrsft5[] = {
	0x00,
	0x90,
};

static char adrsft6[] = {
	0xc4,
	0x00,
};

static char adrsft7[] = {
	0x00,
	0xb4,
};

static char adrsft8[] = {
	0xc0,
	0xc0,
};

static char adrsft9[] = {
	0x00,
	0x87,
};

static char pixel_eyes_setting[] = {
	0xa4,
	0x15,
};

static char adrsft10[] = {
	0x00,
	0x00,
};
/*
static char caset_data[] = {
	0x2A,
	0x00,0x00,0x04, 0x37,
};

static char paset_data[] = {
	0x2B,
	0x00,0x00,0x07,0x7f,
};
*/
static char tear_on[] = {
	0x35,
	0x00,
};

static char bl_enable[] = {
	0x53,
	0x24,
};
/*******************************************************************************
**display effect
*/
//ce
//0x00 0x00
static char orise_clever_mode[] = {
	0x59,
	0x03,
};

static char addr_shift_a0[] = {
	0x00,
	0xa0,
};

static char ce_d6_1[] = {
	0xD6,
	0x03, 0x01, 0x00, 0x00, 0x00,
	0x00, 0xfd, 0x00, 0x03, 0x06,
	0x06, 0x02,
};

static char addr_shift_b0[] = {
	0x00,
	0xb0,
};

static char ce_d6_2[] = {
	0xD6,
	0x00, 0x00, 0x66, 0xb3, 0xcd,
	0xb3, 0xcd, 0xb3, 0xa6, 0xb3,
	0xcd, 0xb3,
};

static char addr_shift_c0[] = {
	0x00,
	0xc0,
};

static char ce_d6_3[] = {
	0xD6,
	0x37, 0x00, 0x89, 0x77, 0x89,
	0x77, 0x89, 0x77, 0x6f, 0x77,
	0x89, 0x77,
};

static char addr_shift_d0[] = {
	0x00,
	0xd0,
};

static char ce_d6_4[] = {
	0xD6,
	0x37, 0x3c, 0x44, 0x3c, 0x44,
	0x3c, 0x44, 0x3c, 0x37, 0x3c,
	0x44, 0x3c,
};

//0x00 0x80
static char ce_cmd[] = {
	0xD6,
	0x3A,
};

//sharpness
//0x00 0x00
static char sp_cmd[] = {
	0x59,
	0x03,
};

static char sp_shift_0x90[] = {
	0x00,
	0x90,
};

static char sp_D7_1[] = {
	0xD7,
	0x83,
};

static char sp_shift_0x92[] = {
	0x00,
	0x92,
};

static char sp_D7_2[] = {
	0xD7,
	0xff,
};

static char sp_shift_0x93[] = {
	0x00,
	0x93,
};

static char sp_D7_3[] = {
	0xD7,
	0x00,
};

//CABC
//0x00 0x00
//0x59 0x03
//0x00 0x80
static char cabc_ca[] = {
	0xCA,
	0x80,0x88,0x90,0x98,0xa0,
	0xa8,0xb0,0xb8,0xc0,0xc7,
	0xcf,0xd7,0xdf,0xe7,0xef,
	0xf7,0xcc,0xff,0xa5,0xff,
	0x80,0xff,0x53,0x53,0x53,
};
//0x00 0x00
static char cabc_c6_G1[] = {
	0xc6,
	0x10,
};
static char cabc_c7_G1[] = {
	0xC7,
	0xf0,0x8e,0xbc,0x9d,0xac,
	0x9c,0xac,0x9b,0xab,0x8c,
	0x67,0x55,0x45,0x44,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G2[] = {
	0xc6,
	0x11,
};
static char cabc_c7_G2[] = {
	0xC7,
	0xf0,0xac,0xab,0xbc,0xba,
	0x9b,0xab,0xba,0xb8,0xab,
	0x78,0x56,0x55,0x44,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G3[] = {
	0xc6,
	0x12,
};
static char cabc_c7_G3[] = {
	0xC7,
	0xf0,0xab,0xaa,0xab,0xab,
	0xab,0xaa,0xaa,0xa9,0x9b,
	0x8a,0x67,0x55,0x45,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G4[] = {
	0xc6,
	0x13,
};
static char cabc_c7_G4[] = {
	0xC7,
	0xf0,0xaa,0xaa,0xab,0x9b,
	0x9b,0xaa,0xa9,0xa9,0xa9,
	0x9a,0x78,0x56,0x55,0x44,
	0x44,0x44,0x44
};

//0x00 0x00
static char cabc_c6_G5[] = {
	0xc6,
	0x14,
};
static char cabc_c7_G5[] = {
	0xC7,
	0xf0,0xa9,0xaa,0xab,0x9a,
	0xaa,0xa9,0xa9,0x8a,0xa9,
	0x99,0x8a,0x67,0x55,0x55,
	0x44,0x44,0x33
};

//0x00 0x00
static char cabc_c6_G6[] = {
	0xc6,
	0x15,
};
static char cabc_c7_G6[] = {
	0xC7,
	0xf0,0xa8,0xaa,0xaa,0x7b,
	0xab,0x99,0x9a,0x99,0x99,
	0x8b,0xa9,0x55,0x55,0x55,
	0x55,0x45,0x44
};

//0x00 0x00
static char cabc_c6_G7[] = {
	0xc6,
	0x16,
};
static char cabc_c7_G7[] = {
	0xC7,
	0xf0,0xa7,0xaa,0xaa,0x8a,
	0xaa,0x99,0x99,0x99,0xa8,
	0x89,0xa9,0x8a,0x67,0x54,
	0x44,0x44,0x33
};

//0x00 0x00
static char cabc_c6_G8[] = {
	0xc6,
	0x17,
};
static char cabc_c7_G8[] = {
	0xC7,
	0xf0,0x97,0xaa,0xaa,0x89,
	0xaa,0xa8,0x88,0x9a,0xa7,
	0xa8,0xa7,0x66,0x66,0x66,
	0x56,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G9[] = {
	0xc6,
	0x18,
};
static char cabc_c7_G9[] = {
	0xC7,
	0xf0,0x87,0x9a,0x9b,0x8a,
	0xa9,0xa8,0x88,0xa9,0x87,
	0x9a,0x88,0x89,0x67,0x56,
	0x55,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G10[] = {
	0xc6,
	0x19,
};
static char cabc_c7_G10[] = {
	0xC7,
	0xe0,0x97,0x8a,0x9b,0x8a,
	0x99,0x99,0x98,0xa8,0x87,
	0x8a,0x79,0x8a,0x67,0x66,
	0x56,0x55,0x55
};

//0x00 0x00
static char cabc_c6_G11[] = {
	0xc6,
	0x1a,
};
static char cabc_c7_G11[] = {
	0xC7,
	0xe0,0xa6,0x89,0xaa,0xa9,
	0x98,0x8a,0x88,0x89,0x79,
	0x7a,0x7a,0x98,0x78,0x66,
	0x66,0x56,0x45
};

//0x00 0x00
static char cabc_c6_G12[] = {
	0xc6,
	0x1b,
};
static char cabc_c7_G12[] = {
	0xC7,
	0xb0,0x99,0x99,0x99,0x9a,
	0x98,0x88,0x88,0x89,0x88,
	0x98,0x79,0x88,0x8a,0x67,
	0x66,0x66,0x55
};

//0x00 0x00
static char cabc_c6_G13[] = {
	0xc6,
	0x1c,
};
static char cabc_c7_G13[] = {
	0xC7,
	0xe0,0x96,0x89,0x9a,0x89,
	0xb7,0x88,0x88,0x88,0x88,
	0x88,0x89,0x87,0xa8,0x98,
	0x78,0x56,0x34
};

//0x00 0x00
static char cabc_c6_G14[] = {
	0xc6,
	0x1d,
};
static char cabc_c7_G14[] = {
	0xC7,
	0xb0,0x89,0x89,0xa9,0x98,
	0x88,0x7a,0x89,0x97,0x88,
	0x78,0x78,0x77,0x89,0x79,
	0x88,0x67,0x56
};

//0x00 0x00
static char cabc_c6_G15[] = {
	0xc6,
	0x1e,
};
static char cabc_c7_G15[] = {
	0xC7,
	0xb0,0x98,0xa8,0xa7,0x89,
	0x88,0x98,0x88,0x78,0x88,
	0x88,0x88,0x77,0x77,0x77,
	0x56,0x98,0x9A
};

//0x00 0x00
static char cabc_c6_G16[] = {
	0xc6,
	0x1f,
};
static char cabc_c7_G16[] = {
	0xC7,
	0x90,0x8a,0x89,0x99,0x98,
	0x88,0x88,0x88,0x88,0x88,
	0x77,0x88,0x78,0x88,0x77,
	0x77,0x89,0x78
};

static char cabc_shift_UI[] = {
	0x00,
	0x90,
};

static char cabc_UI_mode[] = {
	0xCA,
	0xCC, 0xFF,
};

static char cabc_shift_STILL[] = {
	0x00,
	0x92,
};

static char cabc_STILL_mode[] = {
	0xCA,
	0xA5, 0xFF,
};

static char cabc_shift_moving[] = {
	0x00,
	0x94,
};

static char cabc_moving_mode[] = {
	0xCA,
	0x80, 0xFF,
};

//0x00 0x00
static char cabc_disable_curve[] = {
	0xc6,
	0x00,
};

//0x00 0x00
static char cabc_disable_setting[] = {
	0x59,
	0x00,
};

static char cabc_53[] = {
	0x53,
	0x2c,
};
static char cabc_set_mode_UI[] = {
	0x55,
	0x90,
};
/*static char cabc_set_mode_STILL[] = {
	0x55,
	0x92,
};*/
static char cabc_set_mode_MOVING[] = {
	0x55,
	0x93,
};

/*******************************************************************************
** Power OFF Sequence(Normal to power off)
*/
static char exit_sleep[] = {
	0x11,
};

static char display_on[] = {
	0x29,
};

static char display_off[] = {
	0x28,
};

static char enter_sleep[] = {
	0x10,
};

static char Delay_TE[] = {
	0x44,
	0x07, 0x80,
};

/*static char soft_reset[] = {
	0x01,
};*/

static struct dsi_cmd_desc jdi_display_on_cmd[] = {
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(cmd2_ena1), cmd2_ena1},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
		sizeof(cmd2_ena2), cmd2_ena2},
	//CE
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(orise_clever_mode), orise_clever_mode},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(addr_shift_a0), addr_shift_a0},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(ce_d6_1), ce_d6_1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(addr_shift_b0), addr_shift_b0},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(ce_d6_2), ce_d6_2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(addr_shift_c0), addr_shift_c0},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(ce_d6_3), ce_d6_3},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(addr_shift_d0), addr_shift_d0},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(ce_d6_4), ce_d6_4},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(ce_cmd), ce_cmd},
	//sharpness
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_cmd), sp_cmd},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_shift_0x90), sp_shift_0x90},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_D7_1), sp_D7_1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_shift_0x92), sp_shift_0x92},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_D7_2), sp_D7_2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_shift_0x93), sp_shift_0x93},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_D7_3), sp_D7_3},
	//cabc
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft2), adrsft2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_ca), cabc_ca},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G1), cabc_c6_G1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G1), cabc_c7_G1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G2), cabc_c6_G2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G2), cabc_c7_G2},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G3), cabc_c6_G3},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G3), cabc_c7_G3},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G4), cabc_c6_G4},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G4), cabc_c7_G4},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G5), cabc_c6_G5},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G5), cabc_c7_G5},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G6), cabc_c6_G6},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G6), cabc_c7_G6},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G7), cabc_c6_G7},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G7), cabc_c7_G7},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G8), cabc_c6_G8},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G8), cabc_c7_G8},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G9), cabc_c6_G9},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G9), cabc_c7_G9},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G10), cabc_c6_G10},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G10), cabc_c7_G10},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G11), cabc_c6_G11},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G11), cabc_c7_G11},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G12), cabc_c6_G12},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G12), cabc_c7_G12},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G13), cabc_c6_G13},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G13), cabc_c7_G13},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G14), cabc_c6_G14},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G14), cabc_c7_G14},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G15), cabc_c6_G15},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G15), cabc_c7_G15},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c6_G16), cabc_c6_G16},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_c7_G16), cabc_c7_G16},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_shift_UI), cabc_shift_UI},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_UI_mode), cabc_UI_mode},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_shift_STILL), cabc_shift_STILL},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_STILL_mode), cabc_STILL_mode},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_shift_moving), cabc_shift_moving},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_moving_mode), cabc_moving_mode},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_disable_curve), cabc_disable_curve},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(adrsft1), adrsft1},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_disable_setting), cabc_disable_setting},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_53), cabc_53},
	{DTYPE_DCS_WRITE1, 0,1, WAIT_TYPE_MS,
		sizeof(adrsft3), adrsft3},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft4), adrsft4},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft5), adrsft5},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft6), adrsft6},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft7), adrsft7},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft8), adrsft8},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft9), adrsft9},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(pixel_eyes_setting), pixel_eyes_setting},
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(adrsft10), adrsft10},
};

static struct dsi_cmd_desc jdi_display_on_cmd1[] = {
	{DTYPE_DCS_WRITE1, 0,10, WAIT_TYPE_US,
		sizeof(tear_on), tear_on},
	//{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
	//	sizeof(caset_data), caset_data},
	//{DTYPE_DCS_LWRITE, 0,10, WAIT_TYPE_US,
	//	sizeof(paset_data), paset_data},
	{DTYPE_DCS_WRITE1, 0, 200, WAIT_TYPE_US,
		sizeof(bl_enable), bl_enable},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(Delay_TE), Delay_TE},
	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc jdi_cabc_ui_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_UI), cabc_set_mode_UI},
};

/*static struct dsi_cmd_desc jdi_cabc_still_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_STILL), cabc_set_mode_STILL},
};*/

static struct dsi_cmd_desc jdi_cabc_moving_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(cabc_set_mode_MOVING), cabc_set_mode_MOVING},
};
static struct dsi_cmd_desc jdi_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 0, 60, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static char command_2_enable[] = {
	0x00,
	0x00,
};

static char command_2_enable_1_para[] = {
	0xFF,
	0x19, 0x02, 0x01,
};

static char command_2_enable_2[] = {
	0x00,
	0x80,
};

static char command_2_enable_2_para[] = {
	0xFF,
	0x19, 0x02,
};

static char HD720_setting_1_para[] = {
	0x2A,
	0x00, 0x00, 0x02, 0xCF,
};

static char HD1080_setting_1_para[] = {
	0x2a,
	0x00, 0x00, 0x04, 0x37,
};

static char HD720_setting_2_para[] = {
	0x2B,
	0x00, 0x00, 0x04, 0xFF,
};

static char HD1080_setting_2_para[] = {
	0x2b,
	0x00, 0x00, 0x07, 0x7f,
};

static char cleveredge_1_5x_para[] = {
	0x1C,
	0x05,
};

static char cleveredge_disable[] = {
	0x1C,
	0x00,
};

static char cleveredge_P1[] = {
	0x00,
	0x91,
};

static char cleveredge_P1_para[] = {
	0xD7,
	0xC8,
};

static char cleveredge_P2[] = {
	0x00,
	0x93,
};

static char cleveredge_P2_para[] = {
	0xD7,
	0x08,
};

static char cleveredge_use_setting[] = {
	0x00,
	0xAC,
};

static char cleveredge_use_setting_para[] = {
	0xC0,
	0x04,
};

static char command_2_disable_para[] = {
	0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

static char command_clevermode[] = {
	0x59,
	0x03,
};

static struct dsi_cmd_desc cleveredge_inital_720P_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_1_para), command_2_enable_1_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_2), command_2_enable_2},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_2_para), command_2_enable_2_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_clevermode), command_clevermode},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(HD720_setting_1_para), HD720_setting_1_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(HD720_setting_2_para), HD720_setting_2_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_1_5x_para), cleveredge_1_5x_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_P1), cleveredge_P1},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_P1_para), cleveredge_P1_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_P2), cleveredge_P2},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_P2_para), cleveredge_P2_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_use_setting), cleveredge_use_setting},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_use_setting_para), cleveredge_use_setting_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_disable_para), command_2_disable_para}
};

static struct dsi_cmd_desc cleveredge_inital_1080P_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_1_para), command_2_enable_1_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_2), command_2_enable_2},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable_2_para), command_2_enable_2_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(HD1080_setting_1_para), HD1080_setting_1_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_GEN_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(HD1080_setting_2_para), HD1080_setting_2_para},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(command_2_enable), command_2_enable},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(cleveredge_disable), cleveredge_disable},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_shift_0x93), sp_shift_0x93},
	{DTYPE_DCS_LWRITE, 0, 10, WAIT_TYPE_US,
		sizeof(sp_D7_3), sp_D7_3},
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
#if 0
static struct pinctrl_cmd_desc jdi_lcd_pinctrl_finit_cmds[] = {
	{DTYPE_PINCTRL_PUT, &pctrl, 0},
};
#endif


/*******************************************************************************
** LCD GPIO
*/
#define GPIO_LCD_BL_ENABLE_NAME	"gpio_lcd_bl_enable"
#define GPIO_LCD_RESET_NAME	"gpio_lcd_reset"
#define GPIO_LCD_ID_NAME	"gpio_lcd_id"
#define GPIO_LCD_P5V5_ENABLE_NAME	"gpio_lcd_p5v5_enable"
#define GPIO_LCD_N5V5_ENABLE_NAME "gpio_lcd_n5v5_enable"

static u32	gpio_lcd_bl_enable;  /*gpio_4_3, gpio_035*/
static u32	gpio_lcd_reset;  /*gpio_4_5, gpio_037*/
static u32	gpio_lcd_id;  /*gpio_4_6, gpio_038*/
static u32	gpio_lcd_p5v5_enable;  /*gpio_5_1, gpio_041*/
static u32	gpio_lcd_n5v5_enable;  /*gpio_5_2, gpio_042*/

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
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_US, 50,
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


/******************************************************************************/
static struct lcd_tuning_dev *p_tuning_dev = NULL;
static int cabc_mode = 1; /* allow application to set cabc mode to ui mode */
static char __iomem *mipi_dsi0_base_display_effect = NULL;
static volatile bool g_display_on = false;

static int jdi_set_gamma(struct lcd_tuning_dev *ltd, enum lcd_gamma gamma)
{
	int ret = 0;
	struct platform_device *pdev = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	char __iomem *dss_base = 0;

	BUG_ON(ltd == NULL);
	pdev = (struct platform_device *)(ltd->data);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	dss_base = k3fd->dss_base;

	/* Fix me: Implement it */

	return ret;
}

static int jdi_set_cabc(struct lcd_tuning_dev *ltd, enum  tft_cabc cabc)
{
	int ret = 0;
	if (!g_k3fd->panel_power_on) {
		K3_FB_INFO("power off\n");
		return ret;
	}
	/* Fix me: Implement it */
	switch (cabc)
	{
		case CABC_UI:
			mipi_dsi_cmds_tx(jdi_cabc_ui_on_cmds, \
							ARRAY_SIZE(jdi_cabc_ui_on_cmds),\
							mipi_dsi0_base_display_effect);
			break;
		case CABC_VID:
			mipi_dsi_cmds_tx(jdi_cabc_moving_on_cmds, \
							ARRAY_SIZE(jdi_cabc_moving_on_cmds),\
							mipi_dsi0_base_display_effect);
			break;
		case CABC_OFF:
			break;
		default:
			ret = -1;
	}
	return ret;
}

static unsigned int g_csc_value[9];
static unsigned int g_is_csc_set;
static struct semaphore ct_sem;

static void k3_dss_color_temperature_init(struct k3_fb_data_type *k3fd)
{
	char __iomem *dss_base = 0;
	unsigned int i;

	dss_base = k3fd->dss_base;//0xe8500000

	set_reg(dss_base + DSS_DPP_GAMMA_OFFSET + GAMMA_BYPASS_EN, 0x0, 1, 0);//gamma enable
	set_reg(dss_base + DSS_DPP_GAMMA_OFFSET + GAMMA_CORE_GT, 0x2, 32, 0);//lut clock enable

	set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_BYPASS_EN, 0x0, 1, 0);//xcc enable
	set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_IGM_GT, 0x2, 32, 0);//xcc clock enable

	for(i = 0; i < 256; i++)
	{
		set_reg(dss_base + DSS_DPP_GAMMA_OFFSET + (GAMMA_R_LUT + i * 4), i, 32, 0);
		set_reg(dss_base + DSS_DPP_GAMMA_OFFSET + (GAMMA_G_LUT + i * 4), i, 32, 0);
		set_reg(dss_base + DSS_DPP_GAMMA_OFFSET + (GAMMA_B_LUT + i * 4), i, 32, 0);

		set_reg(dss_base + DSS_DPP_LCP_OFFSET + (LCP_IGM_R_LUT + i * 4), i * 16, 32, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + (LCP_IGM_G_LUT + i * 4), i * 16, 32, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + (LCP_IGM_B_LUT + i * 4), i * 16, 32, 0);
	}
}

static void jdi_store_ct_cscValue(unsigned int csc_value[])
{
	down(&ct_sem);
	g_csc_value [0] = csc_value[0];
	g_csc_value [1] = csc_value[1];
	g_csc_value [2] = csc_value[2];
	g_csc_value [3] = csc_value[3];
	g_csc_value [4] = csc_value[4];
	g_csc_value [5] = csc_value[5];
	g_csc_value [6] = csc_value[6];
	g_csc_value [7] = csc_value[7];
	g_csc_value [8] = csc_value[8];
	g_is_csc_set = 1;
	up(&ct_sem);

	return;
}

static int jdi_set_ct_cscValue(struct k3_fb_data_type *k3fd)
{
	char __iomem *dss_base = 0;
	dss_base = k3fd->dss_base;//0xe8500000
	down(&ct_sem);
	if (1 == g_is_csc_set && g_display_on) {
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_01, g_csc_value[0], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_02, g_csc_value[1], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_03, g_csc_value[2], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_11, g_csc_value[3], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_12, g_csc_value[4], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_13, g_csc_value[5], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_21, g_csc_value[6], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_22, g_csc_value[7], 17, 0);
		set_reg(dss_base + DSS_DPP_LCP_OFFSET + LCP_XCC_COEF_23, g_csc_value[8], 17, 0);
	}
	up(&ct_sem);

	return 0;
}

static int jdi_set_color_temperature(struct lcd_tuning_dev *ltd, unsigned int csc_value[])
{

	int flag = 0;
	struct platform_device *pdev;
	struct k3_fb_data_type *k3fd;

	if (ltd == NULL) {
		return -1;
	}
	pdev = (struct platform_device *)(ltd->data);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);

	if (k3fd == NULL) {
		return -1;
	}

	jdi_store_ct_cscValue(csc_value);
	flag = jdi_set_ct_cscValue(k3fd);
	return flag;

}

static struct lcd_tuning_ops sp_tuning_ops = {
	.set_gamma = jdi_set_gamma,
	.set_cabc = jdi_set_cabc,
	.set_color_temperature = jdi_set_color_temperature,
};
static ssize_t k3_lcd_model_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "jdi_OTM1902B 5.0' CMD TFT 1080 x 1920\n");
}

static ssize_t show_cabc_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", cabc_mode);
}

static ssize_t store_cabc_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	unsigned long val = 0;

	ret = strict_strtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (val == 1) {
		/* allow application to set cabc mode to ui mode */
		cabc_mode =1;
		jdi_set_cabc(p_tuning_dev, CABC_UI);
	} else if (val == 2) {
		/* force cabc mode to video mode */
		cabc_mode =2;
		jdi_set_cabc(p_tuning_dev, CABC_VID);
	}
	return snprintf((char *)buf, count, "%d\n", cabc_mode);
}
static ssize_t jdi_frame_count_show(struct device *dev,
          struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", frame_count);
}

static struct device_attribute k3_lcd_class_attrs[] = {
	__ATTR(lcd_model, S_IRUGO, k3_lcd_model_show, NULL),
	__ATTR(cabc_mode, 0644, show_cabc_mode, store_cabc_mode),
	__ATTR(frame_count, S_IRUGO, jdi_frame_count_show, NULL),
	__ATTR_NULL,
};

/*******************************************************************************
**
*/
static int mipi_jdi_panel_set_fastboot(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

#if 0
	/* lcd vcc enable */
	vcc_cmds_tx(pdev, jdi_lcd_vcc_enable_cmds,
		ARRAY_SIZE(jdi_lcd_vcc_enable_cmds));

	/* lcd pinctrl normal */
	pinctrl_cmds_tx(pdev, jdi_lcd_pinctrl_normal_cmds,
		ARRAY_SIZE(jdi_lcd_pinctrl_normal_cmds));

	/* lcd gpio request */
	gpio_cmds_tx(jdi_lcd_gpio_request_cmds,
		ARRAY_SIZE(jdi_lcd_gpio_request_cmds));
#endif

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
	mipi_dsi0_base_display_effect = mipi_dsi0_base;

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

			if(DISPLAY_LOW_POWER_LEVEL_HD == k3fd->switch_res_flag)
				mipi_dsi_cmds_tx(cleveredge_inital_720P_cmds, ARRAY_SIZE(cleveredge_inital_720P_cmds), mipi_dsi0_base);
			else
				mipi_dsi_cmds_tx(cleveredge_inital_1080P_cmds, ARRAY_SIZE(cleveredge_inital_1080P_cmds), mipi_dsi0_base);

		/* lcd display on sequence*/
			mipi_dsi_cmds_tx(jdi_display_on_cmd, \
				ARRAY_SIZE(jdi_display_on_cmd), mipi_dsi0_base);
		}

		mipi_dsi_cmds_tx(jdi_display_on_cmd1, \
			ARRAY_SIZE(jdi_display_on_cmd1), mipi_dsi0_base);

		/* jdi_cabc_ui_on_cmds*/
		mipi_dsi_cmds_tx(jdi_cabc_ui_on_cmds, \
			ARRAY_SIZE(jdi_cabc_ui_on_cmds), mipi_dsi0_base);

		g_display_on = true;
		k3_dss_color_temperature_init(k3fd);
		jdi_set_ct_cscValue(k3fd);
		/*Read LCD power state*/
		if (likely(!fastboot_display_enable)) {
			outp32(mipi_dsi0_base + MIPIDSI_GEN_HDR_OFFSET, 0x0A06);
			pkg_status = inp32(mipi_dsi0_base + MIPIDSI_CMD_PKT_STATUS_OFFSET);
			while (pkg_status & 0x10) {
				udelay(50);
				if (++try_times > 100) {
					try_times = 0;
					K3_FB_ERR("Read lcd power status timeout\n");
					break;
				}
				pkg_status = inp32(mipi_dsi0_base + MIPIDSI_CMD_PKT_STATUS_OFFSET);
			}
			debug_enable = true;
			power_status = inp32(mipi_dsi0_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
			if ( 0x9c != power_status && !dsm_client_ocuppy(lcd_dclient)) {
				dsm_client_record(lcd_dclient, "LCD Power State = 0x%x\n", power_status);
				dsm_client_notify(lcd_dclient, DSM_LCD_LCD_STATUS_ERROR_NO);
			}
			K3_FB_INFO("LCD Power State = 0x%x\n", power_status);
		}
		fastboot_display_enable = false;
		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	} else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE) {
		pr_jank(JL_KERNEL_LCD_POWER_ON, "%s", "JL_KERNEL_LCD_POWER_ON");
	} else {
		K3_FB_ERR("failed to init lcd!\n");
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		k3_pwm_on(pdev);
	} else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM) {
		k3_blpwm_on(pdev);
	} else {
		K3_FB_ERR("No such bl_set_type!\n");
	}

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
    pr_jank(JL_KERNEL_LCD_POWER_OFF, "%s", "JL_KERNEL_LCD_POWER_OFF");

	mipi_dsi0_base = k3fd->dss_base + DSS_MIPI_DSI0_OFFSET;

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		k3_pwm_off(pdev);
	} else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM) {
		k3_blpwm_off(pdev);
	} else {
		K3_FB_ERR("No such bl_set_type!\n");
	}

	g_display_on = false;
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
	/*BUG_ON(k3fd == NULL);*/

	if (!k3fd) {
		return 0;
	}

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	/* lcd vcc finit */
	vcc_cmds_tx(pdev, jdi_lcd_vcc_finit_cmds,
		ARRAY_SIZE(jdi_lcd_vcc_finit_cmds));

#if 0
	/* lcd pinctrl finit */
	pinctrl_cmds_tx(pdev, jdi_lcd_pinctrl_finit_cmds,
		ARRAY_SIZE(jdi_lcd_pinctrl_finit_cmds));
#endif

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);

	return 0;
}

int k3_mipi_set_backlight(struct k3_fb_data_type *k3fd){
	char __iomem *dss_base = 0;
	u32 level = 0;
	char __iomem *mipi_dsi0_base = NULL;

	char bl_level_adjust[2] = {
		0x51,
		0x00,
	};

	struct dsi_cmd_desc  jdi_bl_level_adjust[] = {
		{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
			sizeof(bl_level_adjust), bl_level_adjust},
	};

	dss_base = k3fd->dss_base;
	level = k3fd->bl_level;
	mipi_dsi0_base = dss_base + DSS_MIPI_DSI0_OFFSET;

    //if need to avoid some level

	bl_level_adjust[1] = level;

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
		pr_jank(JL_KERNEL_LCD_RESUME, "%s, %d", "JL_KERNEL_LCD_RESUME", k3fd->bl_level);
		debug_enable = false;
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		ret = k3_pwm_set_backlight(k3fd);
	} else if (k3fd->panel_info.bl_set_type & BL_SET_BY_BLPWM) {
		ret = k3_blpwm_set_backlight(k3fd);
	} else if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI) {
		ret = k3_mipi_set_backlight(k3fd);
	} else {
		;
	}

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
	if ((0x07 != (read_value & 0xFF))) {
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

static int mipi_jdi_set_disp_resolution(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;
	int retval = 0;

	BUG_ON(pdev == NULL);

	K3_FB_INFO("enter succ!\n");
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	pinfo = &(k3fd->panel_info);

	switch (k3fd->switch_res_flag) {
	case DISPLAY_LOW_POWER_LEVEL_HD:
		pinfo->xres = 720;
		pinfo->yres = 1280;
		pinfo->pxl_clk_rate = 76 * 1000000;
		pinfo->mipi.dsi_bit_clk = 241;
		pinfo->dirty_region_updt_support = 0;
		break;
	case DISPLAY_LOW_POWER_LEVEL_FHD:
		pinfo->xres = 1080;
		pinfo->yres = 1920;
		pinfo->pxl_clk_rate = 150 * 1000000;
		pinfo->mipi.dsi_bit_clk = 480;
		pinfo->dirty_region_updt_support = 1;
		break;
	default:
		retval =  -1;
	}

	return retval;
}

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
	.set_disp_resolution = mipi_jdi_set_disp_resolution,
};

unsigned char get_mipi_jdi_otm1902_tp_color(void)
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

/*******************************************************************************
**
*/
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
	struct lcd_properities lcd_props;

	if (k3_fb_device_probe_defer(PANEL_MIPI_CMD)) {
		goto err_probe_defer;
	}

	K3_FB_DEBUG("+.\n");

	np = of_find_compatible_node(NULL, NULL, DTS_COMP_JDI_OTM1902B);
	if (!np) {
		K3_FB_ERR("NOT FOUND device node %s!\n", DTS_COMP_JDI_OTM1902B);
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
	pinfo->width  = 61;  //mm
	pinfo->height = 109; //mm
	pinfo->type = PANEL_MIPI_CMD;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = LCD_RGB888;
	pinfo->bgr_fmt = LCD_RGB;
	if(get_bl_type(np) == 1){
		pinfo->bl_set_type = BL_SET_BY_MIPI;
	}else{
		pinfo->bl_set_type = BL_SET_BY_PWM;
	}

	pinfo->bl_min = 1;
	pinfo->bl_max = 255;
	pinfo->vsync_ctrl_type = (VSYNC_CTRL_ISR_OFF |
		VSYNC_CTRL_MIPI_ULPS);

	pinfo->frc_enable = 0;
	pinfo->esd_enable = 0;
	pinfo->dirty_region_updt_support = 1;

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

	sema_init(&ct_sem, 1);
	g_csc_value[0] = 0;
	g_csc_value[1] = 0;
	g_csc_value[2] = 0;
	g_csc_value[3] = 0;
	g_csc_value[4] = 0;
	g_csc_value[5] = 0;
	g_csc_value[6] = 0;
	g_csc_value[7] = 0;
	g_csc_value[8] = 0;
	g_is_csc_set = 0;
	/* for cabc */
	lcd_props.type = TFT;
	lcd_props.default_gamma = GAMMA25;
	p_tuning_dev = lcd_tuning_dev_register(&lcd_props, &sp_tuning_ops, (void *)this_dev);
	if (IS_ERR(p_tuning_dev)) {
		K3_FB_ERR("lcd_tuning_dev_register failed!\n");
		return -1;
	}

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

	get_tp_color = get_mipi_jdi_otm1902_tp_color;

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
		.compatible = DTS_COMP_JDI_OTM1902B,
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
		.name = "mipi_jdi_OTM1902B",
		.of_match_table = of_match_ptr(k3_panel_match_table),
	},
};

static int __init mipi_jdi_panel_init(void)
{
	int ret = 0;

	if (read_lcd_type() != JDI_OTM1902B_LCD) {
		K3_FB_ERR("lcd type is not jdi_OTM1902B_LCD, return!\n");
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
