/*
 * MDIS driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Jinyoung Park <parkjyb@mtekvision.com>
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

#ifndef __ODIN_MDIS_H__
#define __ODIN_MDIS_H__


/******************************************************************************
	Local Literals & definitions
******************************************************************************/
/* MDIS Interrupt Control */
#define MDIS_INTERRUPT_ENABLE_BIT		0x01
#define MDIS_INTERRUPT_DISABLE_BIT		0x00

/* MDIS Control Commands */
#define ODIN_CMD_MDIS_DISABLE			0
#define ODIN_CMD_MDIS_ENABLE			1
#define ODIN_CMD_MDIS_SET_VERTICAL_SIZE	2
#define ODIN_CMD_MDIS_GET_DATA			3

/* MDIS Control Configuration */
#define STAT_MDIS_WIN3_MODE 			0x80
#define STAT_MDIS_COMPARE_MODE			0x40
#define STAT_MDIS_VF_REVERSE_ON			0x20
#define STAT_MDIS_VF_MODE				0x10
#define STAT_MDIS_UPDATE_TRIGGER		0x02
#define STAT_MDIS_ENABLE				0x01

#define MDIS_ENABLE 					0x01
#define MDIS_DISABLE					0x00
#define MDIS_INTR_ENABLE				0x01
#define MDIS_INTR_DISABLE				0x00
#define MDIS_INTR_CLEAR 				0x00

#define MDIS_WINDOW_COUNT				10

/* MDIS Register Offsets */
#define REG_MDIS_CTRL									0x0000
#define REG_MDIS_TRUNCATION_THRESHOLD					0x0004
#define REG_MDIS_TRUNCATION_THRESHOLD_CENTER			0x0008
#define REG_MDIS_UPPER_LEFT_WINDOW_OFFSET_H 			0x000C
#define REG_MDIS_UPPER_LEFT_WINDOW_OFFSET_V 			0x0010
#define REG_MDIS_UPPER_RIGHT_WINDOW_OFFSET_H			0x0014
#define REG_MDIS_UPPER_RIGHT_WINDOW_OFFSET_V			0x0018
#define REG_MDIS_UPPER_CENTER_WINDOW_OFFSET_H			0x001C
#define REG_MDIS_UPPER_CENTER_WINDOW_OFFSET_V			0x0020
#define REG_MDIS_LOWER_LEFT_WINDOW_OFFSET_H 			0x0024
#define REG_MDIS_LOWER_LEFT_WINDOW_OFFSET_V 			0x0028
#define REG_MDIS_LOWER_RIGHT_WINDOW_OFFSET_H			0x002C
#define REG_MDIS_LOWER_RIGHT_WINDOW_OFFSET_V			0x0030
#define REG_MDIS_VERTICAL_SIZE							0x0034
#define REG_MDIS_UPPER_LOWER_WINDOW_SIZE_H				0x0038
#define REG_MDIS_UPPER_LOWER_WINDOW_SIZE_V				0x003C
#define REG_MDIS_CENTER_WINDOW_SIZE_H					0x0040
#define REG_MDIS_CENTER_WINDOW_SIZE_V					0x0044
#define REG_MDIS_SEARCH_INTERVAL_SIZE					0x0048
#define REG_MDIS_SEARCH_INTERVAL_SIZE_CENTER			0x004C
#define REG_MDIS_COMPARE_RANGE							0x0050
#define REG_MDIS_INTERRUPT_EN							0x0054
#define REG_MDIS_INTERRUPT_CLR							0x0058
#define REG_MDIS_INTERRUPT_ON							0x005C
#define REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H 			0x0060
#define REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V 			0x0064
#define REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_REVERSE 	0x0068
#define REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V_REVERSE 	0x006C
#define REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_V_5TH		0x0070
#define REG_MDIS_UPPER_RIGHT_WINDOW_VECTOR_H			0x0080
#define REG_MDIS_UPPER_RIGHT_WINDOW_VECTOR_V			0x0084
#define REG_MDIS_UPPER_RIGHT_WINDOW_VECTOR_H_REVERSE	0x0088
#define REG_MDIS_UPPER_RIGHT_WINDOW_VECTOR_V_REVERSE	0x008C
#define REG_MDIS_UPPER_RIGHT_WINDOW_VECTOR_H_V_5TH		0x0090
#define REG_MDIS_CENTER_WINDOW_VECTOR_H 				0x00A0
#define REG_MDIS_CENTER_WINDOW_VECTOR_V 				0x00A4
#define REG_MDIS_CENTER_WINDOW_VECTOR_H_REVERSE 		0x00A8
#define REG_MDIS_CENTER_WINDOW_VECTOR_V_REVERSE 		0x00AC
#define REG_MDIS_CENTER_WINDOW_VECTOR_H_V_5TH			0x00B0
#define REG_MDIS_LOWER_LEFT_WINDOW_VECTOR_H 			0x00C0
#define REG_MDIS_LOWER_LEFT_WINDOW_VECTOR_V 			0x00C4
#define REG_MDIS_LOWER_LEFT_WINDOW_VECTOR_H_REVERSE 	0x00C8
#define REG_MDIS_LOWER_LEFT_WINDOW_VECTOR_V_REVERSE 	0x00CC
#define REG_MDIS_LOWER_LEFT_WINDOW_VECTOR_H_V_5TH		0x00D0
#define REG_MDIS_LOWER_RIGHT_WINDOW_VECTOR_H			0x00E0
#define REG_MDIS_LOWER_RIGHT_WINDOW_VECTOR_V			0x00E4
#define REG_MDIS_LOWER_RIGHT_WINDOW_VECTOR_H_REVERSE	0x00E8
#define REG_MDIS_LOWER_RIGHT_WINDOW_VECTOR_V_REVERSE	0x00EC
#define REG_MDIS_LOWER_RIGHT_WINDOW_VECTOR_H_V_5TH		0x00F0
#define REG_MDIS_TRUNCATION_COEFFICIENT 				0x00F4
#define REG_MDIS_FSM_STATE								0x00F8
#define REG_MDIS_FRAME_PASS_NUMBER						0x00FC

/* MDIS Register Bit Fields */
typedef union {
	struct {
		volatile u32 mdis_on : 1;
		volatile u32 update_trigger : 1;
		volatile u32 reserved0 : 2;
		volatile u32 vf_mode : 1;
		volatile u32 vf_reverse_on : 1;
		volatile u32 compare_mode : 1;
		volatile u32 win3_mode : 1;
		volatile u32 reserved1 : 24;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_CTRL;

typedef union {
	struct {
		volatile u32 v_trunc : 7;
		volatile u32 reserved0 : 9;
		volatile u32 h_trunc : 7;
		volatile u32 reserved1 : 9;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_TRUNC_THRESHOLD;

typedef union {
	struct {
		volatile u32 v_offset : 10;
		volatile u32 reserved0 : 6;
		volatile u32 h_offset : 10;
		volatile u32 reserved1 : 6;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_WINDOW_OFFSET;

typedef union {
	struct {
		volatile u32 v_size : 11;
		volatile u32 reserved : 21;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_VERTICAL_SIZE;

typedef union {
	struct {
		volatile u32 v_mask : 9;
		volatile u32 reserved0 : 7;
		volatile u32 h_mask : 11;
		volatile u32 reserved1 : 5;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_WINDOW_SIZE;

typedef union {
	struct {
		volatile u32 v_intv : 8;
		volatile u32 reserved0 : 8;
		volatile u32 h_intv : 8;
		volatile u32 reserved1 : 8;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_SEARCH_AREA_SIZE;

typedef union {
	struct {
		volatile u32 cv_cmpcnt : 7;
		volatile u32 reserved0 : 1;
		volatile u32 ch_cmpcnt : 7;
		volatile u32 reserved1 : 1;
		volatile u32 v_cmpcnt : 7;
		volatile u32 reserved2 : 1;
		volatile u32 h_cmpcnt : 7;
		volatile u32 reserved3 : 1;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_COMPARE_RANGE;

typedef union {
	struct {
		volatile u32 intr : 1;
		volatile u32 reserved : 31;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_INTR_CTRL;

typedef union {
	struct {
		volatile u32 vec_1 : 7;
		volatile u32 reserved0 : 1;
		volatile u32 vec_2 : 7;
		volatile u32 reserved1 : 1;
		volatile u32 vec_3 : 7;
		volatile u32 reserved2 : 1;
		volatile u32 vec_4 : 7;
		volatile u32 reserved3 : 1;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_WINDOW_VECTOR;

typedef union {
	struct {
		volatile u32 lorv_trunc_sel : 3;
		volatile u32 lorh_trunc_sel : 3;
		volatile u32 lolv_trunc_sel : 3;
		volatile u32 lolh_trunc_sel : 3;
		volatile u32 cenv_trunc_sel : 3;
		volatile u32 cenh_trunc_sel : 3;
		volatile u32 uprv_trunc_sel : 3;
		volatile u32 uprh_trunc_sel : 3;
		volatile u32 uplv_trunc_sel : 3;
		volatile u32 uplh_trunc_sel : 3;
		volatile u32 reserved : 2;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_TRUNC_COEFF;

typedef union {
	struct {
		volatile u32 state : 3;
		volatile u32 reserved : 29;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_FSM_STATE;

typedef union {
	struct {
		volatile u32 frame_pass : 3;
		volatile u32 reserved : 29;
	} asbits;
	volatile u32 as32bits;
} CSS_MDIS_FRAME_PASS_NUM;

typedef struct {
	CSS_MDIS_CTRL				mdis_ctrl;
	CSS_MDIS_TRUNC_THRESHOLD	h_trunc;
	CSS_MDIS_TRUNC_THRESHOLD	v_trunc;
	CSS_MDIS_WINDOW_OFFSET		h_offset_uplh;
	CSS_MDIS_WINDOW_OFFSET		v_offset_uplv;
	CSS_MDIS_WINDOW_OFFSET		h_offset_uprh;
	CSS_MDIS_WINDOW_OFFSET		v_offset_uprv;
	CSS_MDIS_WINDOW_OFFSET		h_offset_cenh;
	CSS_MDIS_WINDOW_OFFSET		v_offset_cenv;
	CSS_MDIS_WINDOW_OFFSET		h_offset_lolh;
	CSS_MDIS_WINDOW_OFFSET		v_offset_lolv;
	CSS_MDIS_WINDOW_OFFSET		h_offset_lorh;
	CSS_MDIS_WINDOW_OFFSET		v_offset_lorv;
	CSS_MDIS_VERTICAL_SIZE		v_size;
	CSS_MDIS_WINDOW_SIZE		h_mask;
	CSS_MDIS_WINDOW_SIZE		v_mask;
	CSS_MDIS_WINDOW_SIZE		cenhv_mask;
	CSS_MDIS_SEARCH_AREA_SIZE	hv_interval;
	CSS_MDIS_SEARCH_AREA_SIZE	cenhv_interval;
	CSS_MDIS_COMPARE_RANGE		cmp_cnt;
	CSS_MDIS_INTR_CTRL			intr_ctrl;
	CSS_MDIS_WINDOW_VECTOR		uplh_vec;
	CSS_MDIS_WINDOW_VECTOR		uplv_vec;
	CSS_MDIS_WINDOW_VECTOR		re_uplh_vec;
	CSS_MDIS_WINDOW_VECTOR		re_uplv_vec;
	CSS_MDIS_WINDOW_VECTOR		uprh_vec;
	CSS_MDIS_WINDOW_VECTOR		uprv_vec;
	CSS_MDIS_WINDOW_VECTOR		re_uprh_vec;
	CSS_MDIS_WINDOW_VECTOR		re_uprv_vec;
	CSS_MDIS_WINDOW_VECTOR		lolh_vec;
	CSS_MDIS_WINDOW_VECTOR		lolv_vec;
	CSS_MDIS_WINDOW_VECTOR		re_lolh_vec;
	CSS_MDIS_WINDOW_VECTOR		re_lolv_vec;
	CSS_MDIS_WINDOW_VECTOR		lolhv_vec_5th;
	CSS_MDIS_WINDOW_VECTOR		lorh_vec;
	CSS_MDIS_WINDOW_VECTOR		lorv_vec;
	CSS_MDIS_WINDOW_VECTOR		re_lorh_vec;
	CSS_MDIS_WINDOW_VECTOR		re_lorv_vec;
	CSS_MDIS_WINDOW_VECTOR		lorhv_vec_5th;
	CSS_MDIS_TRUNC_COEFF		trunc_coeff;
	CSS_MDIS_FSM_STATE			fsm_state;
	CSS_MDIS_FRAME_PASS_NUM		frame_pass_num;
} CSS_MDIS_REG_DATA;

/* MDIS Window Related Configuration */
struct css_mdiscfg_compare {
	s32 h_comp_cnt;
	s32 v_comp_cnt;
	s32 ch_comp_cnt;
	s32 cv_comp_cnt;
};

struct css_mdiscfg_interval {
	s32 h_intv;
	s32 v_intv;
	s32 ch_intv;
	s32 cv_intv;
};

struct css_mdiscfg_mask {
	s32 h_hmask;
	s32 h_vmask;
	s32 v_hmask;
	s32 v_vmask;
	s32 ch_hmask;
	s32 ch_vmask;
	s32 cv_hmask;
	s32 cv_vmask;
};

struct css_mdiscfg_offset {
	s32 h_offset;
	s32 v_offset;
};

struct css_mdiscfg_trunc {
	s32 h_trunc_thr;
	s32 v_trunc_thr;
	s32 ch_trunc_thr;
	s32 cv_trunc_thr;
};

struct css_mdis_configs {
	struct css_mdiscfg_trunc	trunc;
	struct css_mdiscfg_offset	offset[10];
	struct css_mdiscfg_mask 	mask;
	struct css_mdiscfg_interval	interval;
	struct css_mdiscfg_compare	compare;
	s32 vsync_interval;
};

struct css_mdis_vector_windows {
	u32 hvec[5][10];
	u32 hvec_re[5];
	u32 vvec[5][10];
	u32 vvec_re[5];
};

struct css_mdis_vector_offset {
	u32 x;
	u32 y;
	u32 frame_num;
	u32 index;
	struct list_head entry;
};

struct css_mdis_ctrl {
	u32 							mdis_base; /* MDIS register base address */
	struct css_mdis_configs 		config;
	struct css_mdis_vector_windows	vec_wds[2];
	struct css_mdis_vector_offset	vec_offset[CSS_MAX_FRAME_BUFFER_COUNT << 1];
	struct list_head				head;
};

/******************************************************************************
	Externally defined global data
******************************************************************************/
extern atomic_t 				frame_cnt;
extern atomic_t 				mdis_cnt;
extern u32						mdis_fill_idx;


/******************************************************************************
	Locally defined global data
******************************************************************************/
s32 odin_mdis_control(struct file *p_file, struct css_mdis_control_args *arg);
s32 odin_mdis_init_device(void);
s32 odin_mdis_deinit_device(void);

#endif /* __ODIN_MDIS_H__ */
