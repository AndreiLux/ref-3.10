/*
 * ISP interface driver
 *
 * Copyright (C) 2010 - 2013 Mtekvision
 * Author: DongHyung Ko <kodh@mtekvision.com>
 *         Jinyoung Park <parkjyb@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_ISP_H__
#define __ODIN_ISP_H__


#include "odin-ispgeneral.h"
#include "odin-ispcore.h"
#include "odin-ispregmap.h"
#include "odin-flash.h"

/******************************************************************************
	Local literals & definitions
******************************************************************************/
/* ISP Interrupt Control */
#define VSYNC_INTERRUPT_POS_BIT 	   0x01
#define VSYNC_INTERRUPT_NEG_BIT 	   0x02
#define STAT_INTERRUPT_ENABLE_BIT	   0x01

/* ISP H/W Block Index */
#define ISP0	0
#define ISP1	1
#define ISP_MAX 2

/* Mask bits */
#define ODIN_CTRL_MASK					0x0000F000
#define ODIN_CMD_MASK					0x00000F0F

/* ISP Control Index */
#define ODIN_ISP0_CTRL					0x1000
#define ODIN_ISP1_CTRL					0x2000

/* ISP Control Commands */
#define ODIN_CMD_GET_ACC_BUFFER 		    0x100
#define ODIN_CMD_GET_SENSOR_RESOLUTION	    0x200
#define ODIN_CMD_SET_UPDATE_TRIGGER 	    0x300
#define ODIN_CMD_SET_HSLCURVE			    0x400
#define ODIN_CMD_SET_GAMMA				    0x500
#define ODIN_CMD_GET_VSYNC_COUNT		    0x600
#define ODIN_CMD_SET_INTERRUPT_ENABLE	    0x700
#define ODIN_CMD_SET_INTERRUPT_DISABLE	    0x800
#define ODIN_CMD_GET_HISTOGRAM_BUFFER	    0x900
#define ODIN_CMD_SET_STAT_INTERRUPT_COUNT	0xA00
#define ODIN_CMD_ISP_PAUSE					0xD00
#define ODIN_CMD_ISP_RESUME					0xE00
#define LED_HIGH_VIA_ISP					0xF01
#define LED_LOW_VIA_ISP						0xF02
#define TORCH_ON_VIA_ISP					0xF03
#define TORCH_OFF_VIA_ISP					0xF04
#define PRE_FLASH_ON_VIA_ISP				0xF0A
#define PRE_FLASH_OFF_VIA_ISP				0xF0B
#define STROBE_ON_VIA_ISP					0xF05
#define ODIN_CMD_SET_AWB_GAIN				0xF06
#define ODIN_CMD_SET_CC_MATRIX				0xF07
#define ODIN_CMD_SET_LDC_GLOBAL_GAIN		0xF08
#define ODIN_CMD_SET_LDC_SECOND_GAIN		0xF09

/* SENSOR Control Commands*/
#define ODIN_CMD_SENSOR_I2C_READ		0xA00
#define ODIN_CMD_SENSOR_I2C_WRITE		0xB00
#define ODIN_CMD_SENSOR_GET_STATISTIC	0xC00

/* ISP Operation Mode */
#define ODIN_ISP_OPMODE_DEFAULT	0x00	/* 30fps  */
#define ODIN_ISP_OPMODE_HALF	0x01	/* 60fps  */
#define ODIN_ISP_OPMODE_QUATER	0x02	/* 120fps */

#define STAT_INT_MAX_COUNT_DEFAULT	15	/* 30fps  */
#define STAT_INT_MAX_COUNT_HALF		7	/* 60fps  */
#define STAT_INT_MAX_COUNT_QUATER	3	/* 120fps */

#define HISTOGARAM_COUNT 256

#define IRQ_VSYNC		0
#define IRQ_STATIC		1

#define CC_COLOR        0
#define CC_GRAY         1
#define CC_COLOR_OFFSET	2

#define ODIN_ISP_CC_MATRIX_CNT		 9
#define ODIN_ISP_CC_4X3_MATRIX_CNT	12

struct odin_stat_l_histogram {
	u32 hist_pxl_cnt;
	u32 hist_l_sum;
	u32 level[HISTOGARAM_COUNT];
};

struct odin_isp_data_bunch {
	struct {
		u32 y_sum[256];
	} ae;

	struct {
		u32 r_sum;
		u32 g_sum;
		u32 b_sum;
	} wga;

	struct {
		u32 r_sum[256];
		u32 g_sum[256];
		u32 b_sum[256];
	} awb;

	struct {
		u32 count;
		u32 r_sum;
		u32 g_sum;
		u32 b_sum;
	} ctd[10];

	struct {
		u32 count;
		u32 r_sum;
		u32 g_sum;
		u32 b_sum;
	} ctdtotal;

	struct {
		u32 hpf_sum[256];
		u32 lpf_sum[256];
	} edge;

	struct {
		u32 y_sum[16];
	} afd;
};


struct odin_isp_bayer_statistic {
	u32 frmcnt;
	u8 data[CSS_SENSOR_STATISTIC_SIZE];
};

struct odin_isp_arg {
	u32 offset;
	u32 data;
	u32 size;
	struct css_flash_control_args flash;
};

struct odin_isp_resolution {
	u32 width;
	u32 height;
};

struct odin_isp_awb_gain {
	s32 r_gain;
	s32 g_gain;
	s32 b_gain;
};

struct odin_isp_cc_matrix {
	u32 value[12];
	u32 mode;
};

struct odin_isp_ldc_global_gain {
	u32 gain;
	u32 drc_level;
	u32 on;
};

struct odin_isp_ldc_second_gain {
	u32 r_gain;
	u32 g_gain;
	u32 b_gain;
};

struct odin_isp_ctrl {
	u32 probe_status;
	struct odin_isp_fh *isp_fh[ISP_MAX];
	u32 isp_base[ISP_MAX];
	u32 vsync_cnt[ISP_MAX];
	u32 intr_cnt[ISP_MAX];
	u32 irq_cnt[ISP_MAX];
	u32 remain_statistic[ISP_MAX];
	u32 bad_statistic[ISP_MAX];
	u32 drop_3a[ISP_MAX];
	u32 fill_idx[ISP_MAX];
	ktime_t fill_time[ISP_MAX][16];
	u32 avg_time[ISP_MAX];
	u32 update_idx[ISP_MAX];
	u32 update_trigger[ISP_MAX];
	u32 hsl_curve_update_flag[ISP_MAX];
	u32 gamma_curve_update_flag[ISP_MAX];
	u32 *hsl_table[ISP_MAX][ISP_HSLCURVE_CNT];
	u32 *gamma_table[ISP_MAX][ISP_GAMMA_CNT];
	u32 stat_irq_max_cnt[2];
	u32 stat_irq_mode[2];

	struct odin_isp_data_bunch acc_buff[ISP_MAX][2];
	struct odin_isp_bayer_statistic bayer_stat[ISP_MAX];
	struct odin_stat_l_histogram his_buff[ISP_MAX][2];
	struct isp_hardware *isp_hw;
	struct completion isp_int_done[ISP_MAX];
	struct completion isp_wait_done[ISP_MAX];
	struct mutex	  isp_mutex[ISP_MAX];
	struct css_device *cssdev;
	struct odin_isp_awb_gain awb_gain[2];
	struct odin_isp_cc_matrix cc_matrix[2];
	struct odin_isp_ldc_global_gain global_gain[2];
	struct odin_isp_ldc_second_gain second_gain[2];
};

/******************************************************************************
	Externally defined global data
******************************************************************************/

/******************************************************************************
	Locally defined global data
******************************************************************************/
s32 odin_isp_tp_control(int id, int enable);
u32 odin_isp_get_vsync_count(int id);
u32 odin_isp_get_dropped_3a(int id);
s32 odin_isp_get_bad_statistic(int id);
s32 odin_isp_get_bayer_statistic(int id, struct odin_isp_bayer_statistic *data);
s32 odin_isp_control(struct file *p_file, u32 cmds, u32 arg);
s32 odin_isp_wait_vsync(u32 isp_idx, u32 msec);
void odin_isp_wait_vsync_poll(u32 isp_idx, u32 msec);
void odin_isp_init_device(int id);
s32 odin_isp_deinit_device(int id);
void odin_isp_enlarge_statistics(u32);

#endif /* __ODIN_ISP_H__ */
