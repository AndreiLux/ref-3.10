/*
 * Facial Parts Detection driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Eonkyong Lee <tiny@mtekvision.com>
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

#ifndef __ODIN_FACIALPARTSDETECTION_H__
#define __ODIN_FACIALPARTSDETECTION_H__

typedef union {
	struct {
		volatile unsigned int execution_status : 3;
		volatile unsigned int reserved3: 13;
		volatile unsigned int mouth_confidence : 10;
		volatile unsigned int reserved26_0: 6;
		volatile unsigned int right_eye_confidence : 10;
		volatile unsigned int reserved9 : 6;
		volatile unsigned int left_eye_confidence : 10;
		volatile unsigned int reserved26_1 : 6;
		volatile unsigned int roll : 16;
		volatile unsigned int pitch : 8;
		volatile unsigned int yaw : 8;
		volatile unsigned int left_eye_inner_y : 16;
		volatile unsigned int left_eye_inner_x : 16;
		volatile unsigned int left_eye_outer_y : 16;
		volatile unsigned int left_eye_outer_x : 16;
		volatile unsigned int left_eye_center_y : 16;
		volatile unsigned int left_eye_center_x : 16;
		volatile unsigned int right_eye_inner_y : 16;
		volatile unsigned int right_eye_inner_x : 16;
		volatile unsigned int right_eye_outer_y : 16;
		volatile unsigned int right_eye_outer_x : 16;
		volatile unsigned int right_eye_center_y : 16;
		volatile unsigned int right_eye_center_x : 16;
		volatile unsigned int left_nostril_y : 16;
		volatile unsigned int left_nostril_x : 16;
		volatile unsigned int right_nostril_y : 16;
		volatile unsigned int right_nostril_x : 16;
		volatile unsigned int left_mouth_y : 16;
		volatile unsigned int left_mouth_x : 16;
		volatile unsigned int right_mouth_y : 16;
		volatile unsigned int right_mouth_x : 16;
		volatile unsigned int upper_mouth_y : 16;
		volatile unsigned int upper_mouth_x : 16;
		volatile unsigned int center_mouth_y : 16;
		volatile unsigned int center_mouth_x : 16;
		volatile unsigned int reserved0: 32;
	} asfield;
	volatile unsigned int as32bits[16];
} FPD_INFO;

typedef union {
	struct {
		volatile unsigned int fpd_reset : 1;
		volatile unsigned int fpd_start : 1;
		volatile unsigned int fpd_finish : 1;
		volatile unsigned int reserved3 : 29;
	} asfield;
	volatile unsigned int as32bits;
} FPD_CONTROL;

typedef union {
	struct {
		volatile unsigned int fpd_conflicted_index_error : 1;
		volatile unsigned int fpd_end_index_error : 1;
		volatile unsigned int fpd_start_index_error : 1;
		volatile unsigned int fpd_img_address_error : 1;
		volatile unsigned int fpd_desc_address_error : 1;
		volatile unsigned int fpd_result_address_error : 1;
		volatile unsigned int reserved6 : 26;
	} asfield;
	volatile unsigned int as32bits;
} FPD_JOB_STATUS;

typedef union {
	struct {
		volatile unsigned int end_index : 8;
		volatile unsigned int start_index : 8;
		volatile unsigned int reserved6 : 16;
	} asfield;
	volatile unsigned int as32bits;
} FPD_FACE_INDEX;


typedef union {
	struct {
		volatile unsigned int fpd_image_type : 2;
		volatile unsigned int fpd_skip_confidence : 1;
		volatile unsigned int reserved3 : 29;
	} asfield;
	volatile unsigned int as32bits;
} FPD_CONFIG;

typedef union {
	struct {
		volatile unsigned int value : 16;
	} asfield;
	volatile unsigned int as32bits;
} FPD_FDCOEF;

typedef union {
	struct {
		volatile unsigned int value : 16;
	} asfield;
	volatile unsigned int as32bits;
} FPD_ADDRESS;

typedef union {
	struct {
		volatile unsigned int value : 16;
	} asfield;
	volatile unsigned int as32bits;
} FPD_VERSION;

typedef union {
	struct {
		volatile unsigned int protection : 3;
		volatile unsigned int reserved3 : 1;
		volatile unsigned int cache : 4;
		volatile unsigned int lock : 2;
		volatile unsigned int reserved10 : 2;
		volatile unsigned int burst : 2;
		volatile unsigned int reserved14 : 2;
		volatile unsigned int burlength : 4;
		volatile unsigned int reserved20 : 12;

	} asfield;
	volatile unsigned int as32bits;
} FACIAL_PARTS_DETECTION_AXI_CONTROL;

typedef union {
	struct {
		volatile unsigned int detect : 1;
	} asfield;
	volatile unsigned int as32bits;
} FPD_INTERRUPT;

typedef struct {
	FPD_CONTROL control;					/* 0x0000 */
	FPD_JOB_STATUS job_status;				/* 0x0004 */
	FPD_FACE_INDEX face_index;				/* 0x0008 */
	FPD_CONFIG config;						/* 0x000C */
	FPD_FDCOEF fdcoef;						/* 0x0010 */
	FPD_ADDRESS img_address;				/* 0x0014 */
	FPD_ADDRESS desc_address;				/* 0x0018 */
	FPD_ADDRESS work_address;				/* 0x001C */
	FPD_ADDRESS result_address;				/* 0x0020 */
	FPD_VERSION version;					/* 0x0024 */

	volatile unsigned int src_addr;			/* 0x0028 */
	/* FACIAL_PARTS_DETECTION_AXI_CONTROL */
	volatile unsigned int burst_control;	/* 0x002C */
	FPD_INTERRUPT int_status;				/* 0x0030 */
	FPD_INTERRUPT int_enable;				/* 0x0034 */
	FPD_INTERRUPT int_clear;				/* 0x0038 */
} FPD_REGISTERS;

typedef union {
	struct {
		volatile unsigned int center_y : 9;
		volatile unsigned int reserved9 : 7;
		volatile unsigned int center_x : 10;
		volatile unsigned int reserved26 : 6;
		volatile unsigned int angle : 9;
		volatile unsigned int pose : 3;
		volatile unsigned int reserved12 : 4;
		volatile unsigned int size : 9;
		volatile unsigned int reserved25 : 7;
	} asfield;
	volatile unsigned int as32bits[2];
} FPD_FORMAT;


#define FPD_CONTROL_RESET	(1<<0)
#define FPD_CONTROL_START	(1<<1)
#define FPD_CONTROL_STOP	(1<<2)

#define REG_FPD_CONTROL					0x0000
#define REG_FPD_JOB_STATUS				0x0004
#define REG_FPD_FACE_INDEX				0x0008
#define REG_FPD_CONFIG					0x000C
#define REG_FPD_FDCOEF					0x0010 /* FD_IP_version */
#define REG_FPD_IMAGE_ADDR				0x0014
#define REG_FPD_DESC_ADDR				0x0018
#define REG_FPD_WORK_ADDR				0x001C
#define REG_FPD_RESULT_ADDR				0x0020
#define REG_FPD_VERSION					0x0024
#define REG_FPD_SRCBASE_ADDR			0x0028
#define REG_FPD_INTR_AXIRDCTRL			0x002C
#define REG_FPD_INTR_STATUS				0x0030
#define REG_FPD_INTR_ENABLE				0x0034
#define REG_FPD_INTR_CLEAR				0x0038

/* #define FPD_PROFILING */

void odin_fpd_run(unsigned int src_addr, int face_num);
void odin_fpd_hw_init(struct css_device *cssdev);
void odin_fpd_hw_deinit(struct css_device *cssdev);

#endif /* __ODIN_FACIALPARTSDETECTION_H__ */
