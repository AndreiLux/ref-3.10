/*
 * Face Detection driver
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

#ifndef __ODIN_FACEDETECTION_H__
#define __ODIN_FACEDETECTION_H__

typedef union {
	struct {
		volatile unsigned int reset : 1;
		volatile unsigned int start : 1;
		volatile unsigned int finish : 1;
		volatile unsigned int reserved3 : 29;
	} asfield;
	volatile unsigned int as32bits;
} FD_CONTROL;

typedef union {
	struct {
		volatile unsigned int num : 6;
	} asfield;
	volatile unsigned int as32bits;
} FD_COUNT;

typedef union {
	struct {
		volatile unsigned int face_size_min : 2;
		volatile unsigned int direction : 3;
		volatile unsigned int reserved5 : 27;
	} asfield;
	volatile unsigned int as32bits;
} FD_CONDITION;

typedef union {
	struct {
		volatile unsigned int area : 9;
	} asfield;
	volatile unsigned int as32bits;
} FD_START_X;

typedef union {
	struct {
		volatile unsigned int area : 8;
	} asfield;
	volatile unsigned int as32bits;
} FD_START_Y;

typedef union {
	struct {
		volatile unsigned int area : 10;
	} asfield;
	volatile unsigned int as32bits;
} FD_SIZE_X;

typedef union {
	struct {
		volatile unsigned int area : 9;
	} asfield;
	volatile unsigned int as32bits;
} FD_SIZE_Y;

typedef union {
	struct {
		volatile unsigned int value : 4;
	} asfield;
	volatile unsigned int as32bits;
} FD_THRESHOLD;

typedef union {
	struct {
		volatile unsigned int value : 13;
	} asfield;
	volatile unsigned int as32bits;
} FD_VERSION;

typedef union {
	struct {
		volatile unsigned int onoff : 1;
	} asfield;
	volatile unsigned int as32bits;
} FD_ENABLE;

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
} FACE_DETECTION_AXI_CONTROL;

typedef union {
	struct {
		volatile unsigned int detect : 1;
	} asfield;
	volatile unsigned int as32bits;
} FD_INTERRUPT;

typedef union {
	struct {
		volatile unsigned int size : 2;
	} asfield;
	volatile unsigned int as32bits;
} FD_SRC_RESOLUTION;

typedef union {
	struct {
		volatile unsigned int center_x : 10;
		volatile unsigned int reserved10 : 22;
		volatile unsigned int center_y : 9;
		volatile unsigned int reserved9 : 23;
		volatile unsigned int size : 9;
		volatile unsigned int confidence : 4;
		volatile unsigned int reserved13 : 19;
		volatile unsigned int angle : 9;
		volatile unsigned int pose : 3;
		volatile unsigned int reserved12 : 20;
	} asfield;
	volatile unsigned int as32bits[4];
} FD_FACE;

typedef struct {
	FD_CONTROL				control;		/* 0x0000 */
	FD_COUNT				count;			/* 0x0004 */
	FD_CONDITION			condition;		/* 0x0008 */
	FD_START_X				start_x;		/* 0x000C */
	FD_START_Y				start_y;		/* 0x0010 */
	FD_SIZE_X				size_x;			/* 0x0014 */
	FD_SIZE_Y				size_y;			/* 0x0018 */
	FD_THRESHOLD			dhit;			/* 0x001C */
	FD_VERSION				version;		/* 0x0038 */
	FD_ENABLE				enable;			/* 0x0040 */
	FD_SRC_RESOLUTION		src_res;		/* 0x0044 */
	volatile unsigned int	src_addr;		/* 0x0048 */
	/* FACE_DETECTION_AXI_CONTROL */
	volatile unsigned int	burst_control;	/* 0x004C */
	FD_INTERRUPT			int_status;		/* 0x0050 */
	FD_INTERRUPT			int_enable;		/* 0x0054 */
	FD_INTERRUPT			int_clear;		/* 0x0058 */
	FD_FACE					face[35];
} FACE_DETECTION_REGISTERS;


#define REG_FD_CONTROL				0x0000
#define REG_FD_FACE_COUNT			0x0004
#define REG_FD_DETECT_CONDITION		0x0008
#define REG_FD_DETECT_START_X		0x000C
#define REG_FD_DETECT_START_Y		0x0010
#define REG_FD_DETECT_SIZE_X		0x0014
#define REG_FD_DETECT_SIZE_Y		0x0018
#define REG_FD_DETECT_THRESHOLD		0x001C

#define REG_FD_CORE_VERSION 		0x0038

#define REG_FD_ENABLE				0x0040
#define REG_FD_SRCIMAG_RESOLUTION	0x0044
#define REG_FD_SRCBASE_ADDR			0x0048
#define REG_FD_AXI_RD_CTRL			0x004C
#define REG_FD_INTR_STATUS 			0x0050
#define REG_FD_INTR_ENABLE 			0x0054
#define REG_FD_INTR_CLEAR			0x0058

#define REG_FD_FACE0_X		0x0400
#define REG_FD_FACE0_Y		0x0404
#define REG_FD_FACE0_SIZE	0x0408
#define REG_FD_FACE0_ANGLE	0x040C

#define REG_FD_FACE1_X		0x0410
#define REG_FD_FACE1_Y		0x0414
#define REG_FD_FACE1_SIZE	0x0418
#define REG_FD_FACE1_ANGLE	0x041C

#define REG_FD_FACE2_X		0x0420
#define REG_FD_FACE2_Y		0x0424
#define REG_FD_FACE2_SIZE	0x0428
#define REG_FD_FACE2_ANGLE	0x042C

#define REG_FD_FACE3_X		0x0430
#define REG_FD_FACE3_Y		0x0434
#define REG_FD_FACE3_SIZE	0x0438
#define REG_FD_FACE3_ANGLE	0x043C

#define REG_FD_FACE4_X		0x0440
#define REG_FD_FACE4_Y		0x0444
#define REG_FD_FACE4_SIZE	0x0448
#define REG_FD_FACE4_ANGLE	0x044C

#define REG_FD_FACE5_X		0x0450
#define REG_FD_FACE5_Y		0x0454
#define REG_FD_FACE5_SIZE	0x0458
#define REG_FD_FACE5_ANGLE	0x045C

#define REG_FD_FACE6_X		0x0460
#define REG_FD_FACE6_Y		0x0464
#define REG_FD_FACE6_SIZE	0x0468
#define REG_FD_FACE6_ANGLE	0x046C

#define REG_FD_FACE7_X		0x0470
#define REG_FD_FACE7_Y		0x0474
#define REG_FD_FACE7_SIZE	0x0478
#define REG_FD_FACE7_ANGLE	0x047C

#define REG_FD_FACE8_X		0x0480
#define REG_FD_FACE8_Y		0x0484
#define REG_FD_FACE8_SIZE	0x0488
#define REG_FD_FACE8_ANGLE	0x048C

#define REG_FD_FACE9_X		0x0490
#define REG_FD_FACE9_Y		0x0494
#define REG_FD_FACE9_SIZE	0x0498
#define REG_FD_FACE9_ANGLE	0x049C

#define REG_FD_FACE10_X		0x04A0
#define REG_FD_FACE10_Y		0x04A4
#define REG_FD_FACE10_SIZE	0x04A8
#define REG_FD_FACE10_ANGLE	0x04AC

#define REG_FD_FACE11_X		0x04B0
#define REG_FD_FACE11_Y		0x04B4
#define REG_FD_FACE11_SIZE	0x04B8
#define REG_FD_FACE11_ANGLE	0x04BC

#define REG_FD_FACE12_X		0x04C0
#define REG_FD_FACE12_Y		0x04C4
#define REG_FD_FACE12_SIZE	0x04C8
#define REG_FD_FACE12_ANGLE	0x04CC

#define REG_FD_FACE13_X		0x04D0
#define REG_FD_FACE13_Y		0x04D4
#define REG_FD_FACE13_SIZE	0x04D8
#define REG_FD_FACE13_ANGLE	0x04DC

#define REG_FD_FACE14_X		0x04E0
#define REG_FD_FACE14_Y		0x04E4
#define REG_FD_FACE14_SIZE	0x04E8
#define REG_FD_FACE14_ANGLE	0x04EC

#define REG_FD_FACE15_X		0x04F0
#define REG_FD_FACE15_Y		0x04F4
#define REG_FD_FACE15_SIZE	0x04F8
#define REG_FD_FACE15_ANGLE	0x04FC

#define REG_FD_FACE16_X		0x0500
#define REG_FD_FACE16_Y		0x0504
#define REG_FD_FACE16_SIZE	0x0508
#define REG_FD_FACE16_ANGLE	0x050C

#define REG_FD_FACE17_X		0x0510
#define REG_FD_FACE17_Y		0x0514
#define REG_FD_FACE17_SIZE	0x0518
#define REG_FD_FACE17_ANGLE	0x051C

#define REG_FD_FACE18_X		0x0520
#define REG_FD_FACE18_Y		0x0524
#define REG_FD_FACE18_SIZE	0x0528
#define REG_FD_FACE18_ANGLE	0x052C

#define REG_FD_FACE19_X		0x0530
#define REG_FD_FACE19_Y		0x0534
#define REG_FD_FACE19_SIZE	0x0538
#define REG_FD_FACE19_ANGLE	0x053C

#define REG_FD_FACE20_X		0x0540
#define REG_FD_FACE20_Y		0x0544
#define REG_FD_FACE20_SIZE	0x0548
#define REG_FD_FACE20_ANGLE	0x054C

#define REG_FD_FACE21_X		0x0550
#define REG_FD_FACE21_Y		0x0554
#define REG_FD_FACE21_SIZE	0x0558
#define REG_FD_FACE21_ANGLE	0x055C

#define REG_FD_FACE22_X		0x0560
#define REG_FD_FACE22_Y		0x0564
#define REG_FD_FACE22_SIZE	0x0568
#define REG_FD_FACE22_ANGLE	0x056C

#define REG_FD_FACE23_X		0x0570
#define REG_FD_FACE23_Y		0x0574
#define REG_FD_FACE23_SIZE	0x0578
#define REG_FD_FACE23_ANGLE	0x057C

#define REG_FD_FACE24_X		0x0580
#define REG_FD_FACE24_Y		0x0584
#define REG_FD_FACE24_SIZE	0x0588
#define REG_FD_FACE24_ANGLE	0x058C

#define REG_FD_FACE25_X		0x0590
#define REG_FD_FACE25_Y		0x0594
#define REG_FD_FACE25_SIZE	0x0598
#define REG_FD_FACE25_ANGLE	0x059C

#define REG_FD_FACE26_X		0x05A0
#define REG_FD_FACE26_Y		0x05A4
#define REG_FD_FACE26_SIZE	0x05A8
#define REG_FD_FACE26_ANGLE	0x05AC

#define REG_FD_FACE27_X		0x05B0
#define REG_FD_FACE27_Y		0x05B4
#define REG_FD_FACE27_SIZE	0x05B8
#define REG_FD_FACE27_ANGLE	0x05BC

#define REG_FD_FACE28_X		0x05C0
#define REG_FD_FACE28_Y		0x05C4
#define REG_FD_FACE28_SIZE	0x05C8
#define REG_FD_FACE28_ANGLE	0x05CC

#define REG_FD_FACE29_X		0x05D0
#define REG_FD_FACE29_Y		0x05D4
#define REG_FD_FACE29_SIZE	0x05D8
#define REG_FD_FACE29_ANGLE	0x05DC

#define REG_FD_FACE30_X		0x05E0
#define REG_FD_FACE30_Y		0x05E4
#define REG_FD_FACE30_SIZE	0x05E8
#define REG_FD_FACE30_ANGLE	0x05EC

#define REG_FD_FACE31_X		0x05F0
#define REG_FD_FACE31_Y		0x05F4
#define REG_FD_FACE31_SIZE	0x05F8
#define REG_FD_FACE31_ANGLE	0x05FC

#define REG_FD_FACE32_X		0x0600
#define REG_FD_FACE32_Y		0x0604
#define REG_FD_FACE32_SIZE	0x0608
#define REG_FD_FACE32_ANGLE	0x060C

#define REG_FD_FACE33_X		0x0610
#define REG_FD_FACE33_Y		0x0614
#define REG_FD_FACE33_SIZE	0x0618
#define REG_FD_FACE33_ANGLE	0x061C

#define REG_FD_FACE34_X		0x0620
#define REG_FD_FACE34_Y		0x0624
#define REG_FD_FACE34_SIZE	0x0628
#define REG_FD_FACE34_ANGLE	0x062C

#define FACE_DETECTION_RESET	(1<<0)
#define FACE_DETECTION_START	(1<<1)
#define FACE_DETECTION_STOP		(1<<2)

/* #define FD_PROFILING */
/* #define FD_USE_TEST_IMAGE */
/* #define USE_FPD_HW_IP */


void odin_fd_set_resolution(unsigned int value);
void odin_fd_set_direction(unsigned int value);
void odin_fd_set_threshold(unsigned int value);
void fd_get_face_info(struct css_face_detect_info *info, unsigned int count);
int odin_fd_get_face_count(void);
unsigned int fd_get_src_addr(void);
int odin_fd_hw_init(struct css_device *cssdev);
int odin_fd_hw_deinit(struct css_device *cssdev);
int odin_fd_init_resource(struct css_device *cssdev);
int odin_fd_deinit_resource(struct css_device *cssdev);
int odin_face_detection_run(unsigned int buf_index);


#endif /* __ODIN_FACEDETECTION_H__ */
