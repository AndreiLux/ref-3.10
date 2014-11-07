/*
 * linux/drivers/video/odin/s3d/svc_context.h
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRV_CONTEXT_H__
#define __DRV_CONTEXT_H__

#include "drv_customer.h"

struct svc_color_info {
	uint16 enable_y2r;
	uint16 enable_r2y;
	uint16 r11_offset;
	uint16 r12_offset;
	uint16 r13_offset;
	uint16 r14_offset;
	uint16 g12_offset;
	uint16 g13_offset;
	uint16 g14_offset;
	uint16 b12_offset;
	uint16 b13_offset;
	uint16 b14_offset;
	uint16 r11coef;
	uint16 r12coef;
	uint16 r13coef;
	uint16 g12coef;
	uint16 g13coef;
	uint16 b12coef;
	uint16 b13coef;
	uint16 y_rcoef;
	uint16 y_gcoef;
	uint16 y_bcoef;
	uint16 cb_rcoef;
	uint16 cb_gcoef;
	uint16 cb_bcoef;
	uint16 cr_rcoef;
	uint16 cr_gcoef;
	uint16 cr_bcoef;
	uint16 y_coef_offset ;
	uint16 c_coef_offset ;
	uint32 fill_rect_color;
};

struct svc_in_info {
  /*uint32 sx;             input image crop start x */
  /*uint32 sy;             input image crop start x */
  /*uint32 w;              input image crop width */
  /*uint32 h;              input image crop height */
  /*uint32 w_max;          input image width */
  /*uint32 h_max;          input image height */
  /*uint32 addr_p1;        input buffer start address */
  /*uint32 addr_p2;        input buffer start address */
  /*uint32 addr_p3;        input buffer start address */
  /*int32  format;         refer to enum mepsi_in_format, 19 : IN_RGB565_RGB,
  						2 : IN_YUV444_1P_UNPACKED_ZVUY */
  /*int8   v_filter_en;    Vertical direction filter enable */
  /*int8   h_filter_en;    Horizontal direction filter enable */

  enum in_mode_type mode;

  char *image_ptr;
};

struct svc_window_info {
  uint32 sx;            /* image crop start x */
  uint32 sy;            /* image crop start x */
  uint32 w;             /* image crop width */
  uint32 h;             /* image crop height */
  uint32 tw;            /* image total width */
  uint32 th;            /* image total height */

  uint32 addr1_p0;      /* buffer start address page0 plane1 */
  uint32 addr2_p0;      /* buffer start address page0 plane2 */
  uint32 addr3_p0;      /* buffer start address page0 plane3 */

  uint32 addr1_p1;      /* buffer start address page1 plane1 */
  uint32 addr2_p1;      /* buffer start address page1 plane2 */
  uint32 addr3_p1;      /* buffer start address page1 plane3 */

  uint16 format;
  uint32 *img_ptr;

  int16 pf_enabled;    /* input page flip enable */

};


struct svc_out_info {
  uint32 w;             /* display width */
  uint32 h;             /* display height */

  uint32 addr_p0;       /* display buffer start address page0 */
  uint32 addr_p1;       /* display buffer start address page1 */
  uint32 format;
  uint32 frame_rate;

  enum out_mode_type mode;

  /*int32 lcd_format;      0:16bit, 1:32bit color */
  /*int32 lcd_pf;          lcd page flip, 0:enable, 1:disable */
  /*int32 lcd_model;       lcd_model, 0: 800X480_RGB_SAMSUNG_NDIS_7INCH,
  							1:800X480_RGB */
  /*int32 lcd_frame;       capture total frame */
  /*int16 cam_switch; */
  /*int16 sidebar_enable; */

  int16 color_enabled;	/* color bar enable */
  int16 pf_enabled; 	/* output page flip enable */

  struct svc_window_info win[4];

};


struct svc_interface_info {
  struct svc_in_info            in;
  struct svc_out_info           out;
  struct svc_color_info         color;
};

struct svc_context_info {
  uint16 init;

  struct svc_interface_info     info;
  struct drv_core_info          core;
};

/*---------------------------------------------------------------------------
  EXTERNAL FUNCTION
---------------------------------------------------------------------------*/

int32 svc_context_init_parm   ( int32 cmd, void* parm1, void* parm2 );
int32 svc_context_init ( int32 cmd, void* parm1, void* parm2 );
int32 svc_context_exit ( int32 cmd, void* parm1, void* parm2 );
struct svc_context_info* get_context ( void );
int32 frame_buffer_setting ( int32 cmd, void* parm1, void* parm2 );
#endif
