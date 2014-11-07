/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * Some parts borrowed from various video4linux drivers, especially
 * mvzenith-files, bttv-driver.c and zoran.c, see original files for credits.
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

#ifndef __ODIN_CSS_H__
#define __ODIN_CSS_H__


#include <linux/module.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/odin_iommu.h>
#include <linux/pm_qos.h>

#include <media/v4l2-device.h>

#include "camera.h"

#define ENABLE_ERR_INT_CODE

#define CSS_MAX_FH	2
#define CSS_MAX_ZSL	2

#define CSS_MAX_FRAME_BUFFER_COUNT	32
#define CSS_POSTVIEW_DEFAULT_W		1920
#define CSS_POSTVIEW_DEFAULT_H		1080

#define CSS_PLATDRV_CAPTURE	 "odin-camera"
#define CSS_PLATDRV_CRG 	 "odin-css-crg"
#define CSS_PLATDRV_SCALER	 "odin-scaler"
#define CSS_PLATDRV_ISP 	 "odin-isp"
#define CSS_PLATDRV_FD		 "odin-facedection"
#define CSS_PLATDRV_FPD 	 "odin-fpd"
#define CSS_PLATDRV_CSI 	 "odin-mipicsi2"
#define CSS_PLATDRV_ISP_WRAP "odin-isp-wrap"
#define CSS_PLATDRV_MDIS	 "odin-mdis"

#define CSS_OF_CAPTURE_NAME	 "mtekvision,camera"
#define CSS_OF_CRG_NAME		 "mtekvision,css-crg"
#define CSS_OF_SCALER_NAME	 "mtekvision,scaler"
#define CSS_OF_ISP_NAME 	 "mtekvision,isp"
#define CSS_OF_FD_NAME		 "mtekvision,facedetection"
#define CSS_OF_FPD_NAME 	 "mtekvision,facial-part-detection"
#define CSS_OF_CSI_NAME 	 "mtekvision,mipi-csi2"
#define CSS_OF_ISP_WRAP_NAME "mtekvision,isp-wrap"
#define CSS_OF_MDIS_NAME	 "mtekvision,mdis"

#define CSS_CLEAR(x) memset(&(x), 0, sizeof(x))

#define CSS_INT32_MIN	(-0x7fffffff-1)
#define CSS_INT32_MAX	  0x7fffffff
#define CSS_UINT32_MAX	  0xffffffffU


#define CSS_FAILURE		-1
#define CSS_SUCCESS		0


#define CSS_BLKS_INT_CPU	0
#define CSS_ISP_INT_CPU		1

/* define CSS ERRORS */

/* camera errors */
#define CSS_ERROR_CAMERA_BASE				-1000
#define CSS_ERROR_TIMEOUT					(CSS_ERROR_CAMERA_BASE - 1)
#define CSS_ERROR_GET_RESOURCE				(CSS_ERROR_CAMERA_BASE - 2)
#define CSS_ERROR_IOREMAP					(CSS_ERROR_CAMERA_BASE - 3)
#define CSS_ERROR_BUFFER_INIT				(CSS_ERROR_CAMERA_BASE - 4)
#define CSS_ERROR_BUFFER_EMPTY				(CSS_ERROR_CAMERA_BASE - 5)
#define CSS_ERROR_REQUEST_IRQ				(CSS_ERROR_CAMERA_BASE - 6)
#define CSS_ERROR_INVALID_BEHAVIOR			(CSS_ERROR_CAMERA_BASE - 7)
#define CSS_ERROR_UNSUPPORTED				(CSS_ERROR_CAMERA_BASE - 8)
#define CSS_ERROR_CAPTURE_HANDLE			(CSS_ERROR_CAMERA_BASE - 9)
#define CSS_ERROR_DEV_HANDLE				(CSS_ERROR_CAMERA_BASE - 10)

/* sensor errors */
#define CSS_ERROR_SENSOR_BASE						-2000
#define CSS_ERROR_UNSUPPORTED_EFFECT				(CSS_ERROR_SENSOR_BASE - 1)
#define CSS_ERROR_CAM_DEV_IS_NOT_FOUND				(CSS_ERROR_SENSOR_BASE - 2)
#define CSS_ERROR_SENSOR_IS_NOT_FOUND				(CSS_ERROR_SENSOR_BASE - 3)
#define CSS_ERROR_SENSOR_I2C_FAIL					(CSS_ERROR_SENSOR_BASE - 4)
#define CSS_ERROR_SENSOR_I2C_DOES_NOT_READY			(CSS_ERROR_SENSOR_BASE - 5)
#define CSS_ERROR_SENSOR_DOES_NOT_READY				(CSS_ERROR_SENSOR_BASE - 6)
#define CSS_ERROR_INVALID_SENSOR_INFO				(CSS_ERROR_SENSOR_BASE - 7)
#define CSS_ERROR_INVALID_SENSOR_STATE				(CSS_ERROR_SENSOR_BASE - 8)
#define CSS_ERROR_SENSOR_INIT_FAIL					(CSS_ERROR_SENSOR_BASE - 9)
#define CSS_ERROR_SENSOR_MODE_CHANGE_FAIL			(CSS_ERROR_SENSOR_BASE - 10)
#define CSS_ERROR_SENSOR_CONFIG_SUBDEV_FAIL			(CSS_ERROR_SENSOR_BASE - 11)
#define CSS_ERROR_SENSOR_INIT_VALUE_EMPTY			(CSS_ERROR_SENSOR_BASE - 12)
#define CSS_ERROR_SENSOR_INIT_MEMORY_IS_NOT_FOUND	(CSS_ERROR_SENSOR_BASE - 13)
#define CSS_ERROR_SENSOR_INIT_MEMORY_EXIST_ALREADY	(CSS_ERROR_SENSOR_BASE - 14)

#define CSS_ERROR_OIS_SENSOR_BASE			-2500

/* framegrabber errors */
#define CSS_ERROR_FRAMEGRABBER_BASE 		-3000
#define CSS_ERROR_INVALID_ACTION			(CSS_ERROR_FRAMEGRABBER_BASE - 1)
#define CSS_ERROR_INVALID_ARG				(CSS_ERROR_FRAMEGRABBER_BASE - 2)
#define CSS_ERROR_INVALID_RANGE				(CSS_ERROR_FRAMEGRABBER_BASE - 3)
#define CSS_ERROR_INVALID_PIXEL_FORMAT		(CSS_ERROR_FRAMEGRABBER_BASE - 4)
#define CSS_ERROR_INVALID_SCALER_NUM		(CSS_ERROR_FRAMEGRABBER_BASE - 5)
#define CSS_ERROR_INVALID_SIZE				(CSS_ERROR_FRAMEGRABBER_BASE - 6)
#define CSS_ERROR_BUFFER_INDEX				(CSS_ERROR_FRAMEGRABBER_BASE - 7)
#define CSS_ERROR_ZSL_TIMEOUT				(CSS_ERROR_FRAMEGRABBER_BASE - 8)
#define CSS_ERROR_ZSL_NOT_ENABLED			(CSS_ERROR_FRAMEGRABBER_BASE - 9)
#define CSS_ERROR_ZSL_LOAD_FAIL				(CSS_ERROR_FRAMEGRABBER_BASE - 10)
#define CSS_ERROR_ZSL_LOAD_EMPTY			(CSS_ERROR_FRAMEGRABBER_BASE - 11)
#define CSS_ERROR_ZSL_CURRENT_INPUT 		(CSS_ERROR_FRAMEGRABBER_BASE - 12)
#define CSS_ERROR_ZSL_DEVICE				(CSS_ERROR_FRAMEGRABBER_BASE - 13)
#define CSS_ERROR_ZSL_BUF_READY 			(CSS_ERROR_FRAMEGRABBER_BASE - 14)
#define CSS_ERROR_ZSL_BUF_COUNT 			(CSS_ERROR_FRAMEGRABBER_BASE - 15)
#define CSS_ERROR_ZSL_BUF_SIZE				(CSS_ERROR_FRAMEGRABBER_BASE - 16)
#define CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH	(CSS_ERROR_FRAMEGRABBER_BASE - 17)
#define CSS_ERROR_ZSL_DEV_BUSY				(CSS_ERROR_FRAMEGRABBER_BASE - 18)
#define CSS_ERROR_ZSL_STAT_NOT_READY		(CSS_ERROR_FRAMEGRABBER_BASE - 19)
#define CSS_ERROR_ZSL_STAT_VSIZE			(CSS_ERROR_FRAMEGRABBER_BASE - 20)

/* MIPI CSI errors */
#define CSS_ERROR_MIPI_CSI_BASE 			-4000
#define CSS_ERROR_MIPI_CSI_ACCESS_DENIED	(CSS_ERROR_MIPI_CSI_BASE - 1)
#define CSS_ERROR_MIPI_CSI_INVALID_LANE_CNT (CSS_ERROR_MIPI_CSI_BASE - 2)
#define CSS_ERROR_MIPI_CSI_INVALID_LANE 	(CSS_ERROR_MIPI_CSI_BASE - 3)

/* S3DC errors */
#define CSS_ERROR_S3DC_BASE 				-5000
#define CSS_ERROR_S3DC_INVALID_CTXT			(CSS_ERROR_S3DC_BASE - 1)
#define CSS_ERROR_S3DC_INVALID_CONFIG		(CSS_ERROR_S3DC_BASE - 2)
#define CSS_ERROR_S3DC_NOT_SUPPORTED		(CSS_ERROR_S3DC_BASE - 3)
#define CSS_ERROR_S3DC_BUF_NEED_TO_FREE 	(CSS_ERROR_S3DC_BASE - 4)
#define CSS_ERROR_S3DC_BUF_ALLOC_FAIL		(CSS_ERROR_S3DC_BASE - 5)
#define CSS_ERROR_S3DC_BUF_FREE_FAIL		(CSS_ERROR_S3DC_BASE - 6)
#define CSS_ERROR_S3DC_INVALID_BUF			(CSS_ERROR_S3DC_BASE - 7)
#define CSS_ERROR_S3DC_INVALID_PATH 		(CSS_ERROR_S3DC_BASE - 8)
#define CSS_ERROR_S3DC_INVALID_ARG			(CSS_ERROR_S3DC_BASE - 9)

/* FD errors */
#define CSS_ERROR_FD_BASE					-6000
#define CSS_ERROR_FD_DEV_IS_NULL			(CSS_ERROR_FD_BASE - 1)
#define CSS_ERROR_FD_INVALID_BUF			(CSS_ERROR_FD_BASE - 2)
#define CSS_ERROR_FD_INFO_FULL				(CSS_ERROR_FD_BASE - 3)
#define CSS_ERROR_FD_INVALID_INFO_BUF		(CSS_ERROR_FD_BASE - 4)
#define CSS_ERROR_FD_BUF_FULL				(CSS_ERROR_FD_BASE - 5)
#define CSS_ERROR_FD_BUF_ALLOC_FAIL			(CSS_ERROR_FD_BASE - 6)
#define CSS_ERROR_FD_BUF_INDEX				(CSS_ERROR_FD_BASE - 7)
#define CSS_ERROR_FD_BUF_CLIENT				(CSS_ERROR_FD_BASE - 8)
#define CSS_ERROR_FD_BUF_CLIENT_EXIST		(CSS_ERROR_FD_BASE - 9)
#define CSS_ERROR_FD_BUF_INVALID_SIZE		(CSS_ERROR_FD_BASE - 10)
#define CSS_ERROR_FD_BUF_INVALID_COMMAND	(CSS_ERROR_FD_BASE - 11)
#define CSS_ERROR_FD_IS_RUN_ALREADY			(CSS_ERROR_FD_BASE - 12)

/* JPEG ENC errors */
#define CSS_ERROR_JPEG_BASE						-7000
#define CSS_ERROR_JPEG_INVALID_CTRL				(CSS_ERROR_JPEG_BASE - 1)
#define CSS_ERROR_JPEG_INVALID_BUF				(CSS_ERROR_JPEG_BASE - 2)
#define CSS_ERROR_JPEG_INVALID_SIZE				(CSS_ERROR_JPEG_BASE - 3)
#define CSS_ERROR_JPEG_INVALID_CONFIG			(CSS_ERROR_JPEG_BASE - 4)
#define CSS_ERROR_JPEG_UNSUPPORTED_PIXELFORMAT	(CSS_ERROR_JPEG_BASE - 5)
#define CSS_ERROR_JPEG_FILE_IS_NULL				(CSS_ERROR_JPEG_BASE - 6)
#define CSS_ERROR_JPEG_DEV_IS_NULL				(CSS_ERROR_JPEG_BASE - 7)
#define CSS_ERROR_JPEG_PLAT_IS_NULL				(CSS_ERROR_JPEG_BASE - 8)
#define CSS_ERROR_JPEG_DOES_NOT_READY			(CSS_ERROR_JPEG_BASE - 9)
#define CSS_ERROR_JPEG_WRITE_OVERFLOW			(CSS_ERROR_JPEG_BASE - 10)
#define CSS_ERROR_JPEG_WRAP_END					(CSS_ERROR_JPEG_BASE - 11)
#define CSS_ERROR_JPEG_SCALADO_FIFO_OVERFLOW	(CSS_ERROR_JPEG_BASE - 12)
#define CSS_ERROR_JPEG_BLOCK_FIFO_ERROR			(CSS_ERROR_JPEG_BASE - 13)
#define CSS_ERROR_JPEG_BLOCK_FIFO_OVERFLOW		(CSS_ERROR_JPEG_BASE - 14)
#define CSS_ERROR_JPEG_FLAT_FIFO_ERROR			(CSS_ERROR_JPEG_BASE - 15)
#define CSS_ERROR_JPEG_FLAT_FIFO_OVERFLOW		(CSS_ERROR_JPEG_BASE - 16)
#define CSS_ERROR_JPEG_IRQ						(CSS_ERROR_JPEG_BASE - 17)
#define CSS_ERROR_JPEG_BUF_ALLOC_FAIL			(CSS_ERROR_JPEG_BASE - 18)

#define CSS_SEL_ISP_WRAP0	0
#define CSS_SEL_ISP_WRAP1	1
#define ISP_WRAP0	 		0
#define ISP_WRAP1	 		1
#define ISP0_LINKTO_SC0_ISP0_LINKTO_SC1		00
#define ISP0_LINKTO_SC0_ISP1_LINKTO_SC1		01
#define ISP0_LINKTO_SC1_ISP1_LINKTO_SC0		10
#define ISP1_LINKTO_SC0_ISP1_LINKTO_SC1		11

#define VT_ISP_PERFORMANCE			 15913920
#define MINIMUM_ISP_PERFORMANCE		102000000
#define DEFAULT_ISP_PERFORMANCE		204000000
#define MAXIMUM_ISP_PERFORMANCE		408000000

#define DUAL_SRAM_ENABLE		0x2
#define S3DC_SRAM_ENABLE		0x1
#define SRAM_DISABLE			0x3

#define CSS_PATH_MIPI			0
#define CSS_PATH_PARALLEL		1
#define CSS_PATH_MEMORY			2

#define CSS_ISP_WRAP_PATH_MIPI_BAYER		0x00	/* MIPI SENSOR ONLY */
#define CSS_ISP_WRAP_PATH_PARA_BYAER		0x01	/* PARALLEL BAYER SENSOR */
#define CSS_ISP_WRAP_PATH_PARA_YUV			0x02	/* PARALLEL YUV SENSOR */
#define CSS_ISP_WRAP_PATH_MIPI_DUPLE		0x03	/* MIPI DUPLICATED */
#define CSS_ISP_WRAP_PATH_MIPI_YUV			0x04
		/* MIPI(PRIMARY) + YUV(SENCONDARY) */
#define CSS_ISP_WRAP_PATH_PARA_YUV_DUPLE	0x05	/* YUV DUPLICATED */
#define CSS_ISP_WRAP_PATH_MIPI_DUPLE_YUV	0x06
		/* MIPI(0) dupulicated+ YUV (Third) */
#define CSS_ISP_WRAP_PATH_YUV_DUPLE_YUV 	0x07
#define CSS_ISP_WRAP_PATH_YUV_YUV_YUV		0x08

#define CSS_ISP_WRAP_PATHSEL_BAYER			0x10
#define CSS_ISP_WRAP_PATHSEL_YUV			0x11
#define CSS_ISP_WRAP_PATHSEL_MEM_BAYER		0x12
#define CSS_ISP_WRAP_PATHSEL_MEM_YUV		0x13

/* Single Sensor(Bayer Raw) */
#define CSS_ISP_WRAP_MODE_00				0x00
		/* Multi Sensor(Bayer Dual, 21M, FHD) */
#define CSS_ISP_WRAP_MODE_01				0x01
		/* Multi Sensor(Bayer <10M, <10M), Memory Sharing */
#define CSS_ISP_WRAP_MODE_02				0x02
		/* 3D Sensor(Bayer 3D, 10M, 10M) */
#define CSS_ISP_WRAP_MODE_03				0x03
		/* YUV Sensor Single(8M)*/
#define CSS_ISP_WRAP_MODE_10				0x10
		/* YUV Multi Sensor(Full HD Dual) */
#define CSS_ISP_WRAP_MODE_11				0x11
		/* Memory Read Path(Bayer) Single */
#define CSS_ISP_WRAP_MODE_20				0x20
		/* Memory Read Path(Bayer Dual, 21M, FHD) */
#define CSS_ISP_WRAP_MODE_21				0x21
		/* Memory Read Path(Bayer Dual, 21M, FHD), Memory Sharing */
#define CSS_ISP_WRAP_MODE_22				0x22
		/* Memory Read Path(Bayer 3D, 13M, 13M) */
#define CSS_ISP_WRAP_MODE_23				0x23

#define CSS_ISP_WRAP_DELAY_DEFAULT			0x00

#define CSS_ISP_WRAP_OUTSEL_PATH0			0x00
#define CSS_ISP_WRAP_OUTSEL_PATH1			0x01

#define CSS_BUF_3D_TYPE 		0x1000

#define CSS_SENSOR_STATISTIC_SIZE			4192

typedef enum {
	CSS_POLARITY_ACTIVE_LOW = 0,
	CSS_POLARITY_ACTIVE_HIGH = 1,
} css_polarity_active;

typedef enum {
	CSS_COLOR_YUV422 = 0,
	CSS_COLOR_YUV420,
	CSS_COLOR_RGB565
} css_pixel_format;

typedef enum {
	CSS_SCALER_ACT_NONE	= 0,
	CSS_SCALER_ACT_CAPTURE,
	CSS_SCALER_ACT_CAPTURE_3D,
	CSS_SCALER_ACT_CAPTURE_FD,
	CSS_SCALER_ACT_SNAPSHOT,
} css_scaler_action_type;

typedef enum {
	CSS_EFFECT_NO_OPERATION 	= 0x0,
	CSS_EFFECT_BLACK_N_WHITE	= 0x1,
	CSS_EFFECT_SEPIA			= 0x2,
	CSS_EFFECT_NEGATIVE 		= 0x3,
	CSS_EFFECT_EMBOSS			= 0x4,
	CSS_EFFECT_SKETCH			= 0x5
} css_scaler_effect_mode;

typedef enum {
	CSS_EFFECT_STRENGTH_ONE_EIGHTH	= 0x0,
	CSS_EFFECT_STRENGTH_ONE_FOURTH	= 0x1,
	CSS_EFFECT_STRENGTH_ONE_HALF	= 0x2,
	CSS_EFFECT_STRENGTH_ONE			= 0x3,
	CSS_EFFECT_STRENGTH_TWO			= 0x4,
	CSS_EFFECT_STRENGTH_FOUR		= 0x5,
	CSS_EFFECT_STRENGTH_EIGHT		= 0x6,
} css_scaler_effect_strength;

typedef enum {
	CSS_SENSOR_BASIC_TYPE_INVALID	= 0,
	CSS_SENSOR_BASIC_TYPE_BAYER		= 0x0100,
	CSS_SENSOR_BASIC_TYPE_YUV		= 0x0200,
	CSS_SENSOR_BASIC_TYPE_CCIR		= 0x0400
} css_sensor_basic_type;

typedef enum {
	CSS_SENSOR_DISABLE = 0x0,
	CSS_SENSOR_INIT,
	CSS_SENSOR_MODE_CHANGE,
	CSS_SENSOR_MODE_CHANGE_FOR_AAT
} css_sensor_status;

typedef enum {
	CSS_RAWDATA_BIT_MODE_8BIT	= 0x0,
	CSS_RAWDATA_BIT_MODE_10BIT	= 0x1,
	CSS_RAWDATA_BIT_MODE_12BIT	= 0x2
} css_scaler_rawdata_bit_mode;

typedef enum {
	CSS_LOAD_IMG_YUV422		= 0x0,
	CSS_LOAD_IMG_YUV420		= 0x1,
	CSS_LOAD_IMG_RGB565		= 0x2,
	CSS_LOAD_IMG_RAW		= 0x3
} css_scaler_load_img_format;

typedef enum {
	CSS_JPEG_STREAM_BIT_MODE_8BIT	= 0x0,
	CSS_JPEG_STREAM_BIT_MODE_16BIT	= 0x1,
	CSS_JPEG_STREAM_BIT_MODE_32BIT	= 0x2
} css_scaler_jpegstream_bit_mode;

typedef enum {
	CSS_AVERAGE_NO_OPERATION	= 0x0,
	CSS_AVERAGE_ONE_HALF		= 0x1,	/* 1/2 */
	CSS_AVERAGE_ONE_FOURTH		= 0x2,	/* 1/4 */
	CSS_AVERAGE_ONE_EIGHTH		= 0x3	/* 1/8 */
} css_scaler_average_mode;

typedef enum {
	CSS_ROTATION_NO_OPERATION	= 0x0,
	CSS_ROTATION_MIRROR			= 0x1,
	CSS_ROTATION_MIRROR_180CW	= 0x2,
	CSS_ROTATION_180CW			= 0x3,
	CSS_ROTATION_MIRROR_270CW	= 0x4,
	CSS_ROTATION_90CW			= 0x5,
	CSS_ROTATION_270CW			= 0x6,
	CSS_ROTATION_MIRROR90CW		= 0x7
} css_rotation_mode;

typedef enum {
	CSS_INSTRUCTION_NO_OPERATION	= 0x00,
	CSS_INSTRUCTION_CAPTURE_FD		= 0x01,
	CSS_INSTRUCTION_CAPTURE			= 0x21,
	CSS_INSTRUCTION_SNAPSHOT		= 0x22, /* jpeg encoder direct path */
	CSS_INSTRUCTION_3D_CREATOR_PATH = 0x24,
	CSS_INSTRUCTION_PREVIEW			= 0x31,
} css_scaler_instruction_mode;

typedef enum {
	CSS_SENSOR_MASKING_FACTOR_INVALID	= 0x0,
	CSS_SENSOR_MASKING_FACTOR_FULL		= 0x1,
	CSS_SENSOR_MASKING_FACTOR_HALF		= 0x2,
	CSS_SENSOR_MASKING_FACTOR_QUATER	= 0x3,
} css_sensor_masking_factor;

typedef enum {
	CSS_SCALER_0 = 0x0,
	CSS_SCALER_1,
	CSS_SCALER_FD,
	CSS_SCALER_NONE
} css_scaler_select;

typedef enum {
	CSS_BAYER_OUT_POSITION_INVALID = 0,
	CSS_BAYER_OUT_POSITION_PRE,
	CSS_BAYER_OUT_POSITION_POST
} css_bayer_out_position;

typedef enum {
	CSS_BAYER_ISP_OUT_BYPASS_INVALID = 0,
	CSS_BAYER_ISP_OUT_BYPASS_DISABLE,
	CSS_BAYER_ISP_OUT_BYPASS_ENABLE
} css_bayer_isp_out;

typedef enum {
	CMRDI_MODULE_DISABLE = 0x0,
	CMRDI_MODULE_ENABLE
} css_asyncfifo_module_set;

typedef enum {
	CSS_MODULE_DISABLE = 0x0,
	CSS_MODULE_ENABLE
} css_scaler_module_set;

typedef enum {
	CSS_DITHERING_DISABLE = 0x0,
	CSS_DITHERING_ENABLE
} css_scaler_dithering_set;

typedef enum {
	CSS_DITHERING_SEL_YUV	= 0x0,
	CSS_DITHERING_SEL_YCBCR	= 0x1,
} css_scaler_dithering_sel;

typedef enum {
	CSS_ISPDI_TASK_STATUS_INVALID	= 0x00000000,
	CSS_ISPDI_TASK_STATUS_IDLE		= 0x00000001,
	CSS_ISPDI_TASK_STATUS_EXCUTE	= 0x00000010,
} css_ispdi_status_of_task;

typedef enum {
	CSS_ISPDI_TASK_COMMAND_NONE = 0x0,
	CSS_ISPDI_TASK_COMMNAD_CREATE,
	CSS_ISPDI_TASK_COMMAND_RUN,
	CSS_ISPDI_TASK_COMMAND_STOP,
	CSS_ISPDI_TASK_COMMAND_TERMINATE,
} css_ispdi_task_command;

typedef enum {
	CSS_VDIS_SCALER_SELECT_NONE = 0x0,
	CSS_VDIS_SCALER_SELECT_PRIMARY,
	CSS_VDIS_SCALER_SELECT_SECONDARY
} css_vdis_scaler_select;

typedef enum {
	CSS_VDIS_BUFFER_SELECT_NONE = 0x0,
	CSS_VDIS_BUFFER_SELECT_PRIMARY,
	CSS_VDIS_BUFFER_SELECT_SECONDARY
} css_vdis_buffer_select;

typedef enum {
	CSS_JPEG_COMPRESSION_LOW = 0x0,
	CSS_JPEG_COMPRESSION_NORMAL,
	CSS_JPEG_COMPRESSION_MEDIUM,
	CSS_JPEG_COMPRESSION_HIGH,
	CSS_JPEG_COMPRESSION_SUPER_HIGH
} css_jpeg_compression_level;

typedef enum {
	CSS_MEM_ADDR_USER		= 0,
	CSS_MEM_ADDR_KERNEL,
	CSS_MEM_ADDR_PHYSICAL,
	CSS_MEMORY_ADDR_MAX		= 0x7FFFFFFF
} css_memory_addr_type;

typedef enum {
	CSS_CONTROL_TYPE_INTEGER	= 1,
	CSS_CONTROL_TYPE_BOOLEAN	= 2,
	CSS_CONTROL_TYPE_MENU		= 3,
	CSS_CONTROL_TYPE_BUTTON 	= 4,
	CSS_CONTROL_TYPE_INTEGER64	= 5,
	CSS_CONTROL_TYPE_CTRL_CLASS	= 6,
} css_control_type;

typedef enum {
	CSS_NORMAL_PATH_MODE = 0,
	CSS_LIVE_PATH_MODE,
	CSS_NONE_PATH_MODE
} css_path_mode;

struct css_sensor_gpio {
	u16 num;
	css_polarity_active polarity;
};

struct css_image_size {
	u32 width;
	u32 height;
};

struct	css_cam_input_desc {
	struct css_rect margin;
	struct css_image_size size;
	u32 format;	/* BAYER, YUV, CCIR 	  */
	u32 align;	/* RGBG First, YUYV Order */
};

struct css_cam_output_desc {
	struct css_rect			offset;
	struct css_image_size	mask;
	u32 format;	/* YUV422, YUV420, BAYER, JPEG(with JPEG Block) */
	u32 align;
};

struct css_crop_data {
	struct css_image_size src;
	struct css_image_size dst;
	struct css_rect crop;
};

struct css_strobe_set {
	css_scaler_module_set enable;
	u16 time;
	u16 high;
};

struct css_effect_set {
	css_scaler_effect_mode		command;
	css_scaler_effect_strength	strength;
	u8 cb_value;
	u8 cr_value;
};

struct stm {
	u32 base32;
	u32 filled;
}; /* stereo memory */

/* Zero Shutter lag configurations */
struct css_zsl_config {
	u16							enable;
	css_scaler_rawdata_bit_mode	bit_mode;
	css_scaler_load_img_format	ld_fmt;
	struct css_image_size		img_size;
	u32							blank_count;
	u32 						next_capture_index;
	union {
		u32						base32;
		struct stm				base32st[2];
	} mem;
};

struct zsl_statistic {
	unsigned int enable;
	unsigned int hsize;
	unsigned int vline;
};

typedef enum {
	FD_SOURCE_PATH0,
	FD_SOURCE_PATH1
} css_fd_source;

typedef enum {
	FD_QVGA,
	FD_VGA
} css_fd_resolution;

typedef enum {
	FD_DIRECTION_UP_45 = 0,
	FD_DIRECTION_RIGHT_45,
	FD_DIRECTION_LEFT_45,
	FD_DIRECTION_DOWN_45,
	FD_DIRECTION_UP_135,
	FD_DIRECTION_RIGHT_135,
	FD_DIRECTION_LEFT_135,
	FD_DIRECTION_DOWN_135
} css_fd_direction;

typedef enum {
	FD_READY = 0,
	FD_RUN,
	FD_DONE,
	FPD_RUN
} css_fd_state;

/* Async Buffer configuration */
struct css_isp_wrap_config {
	s32 path;							/* mipi, parallel, memory	*/
	s32 delay;							/* Vsync delay				*/
	s32 polarity;						/* Polarity 				*/
	s32 dummy_pixel;					/* Dummy pixel				*/
	s32 pixel_order;					/* Pixel order				*/
	u32 wrap_sel;						/* Async 0/1 selection		*/
	u32 write_clock_delay;				/* Write Clock Delay		*/
	s32 crop_enable;					/* Cropping Enable			*/
	s32 scaling_enable; 				/* Scaling Enable			*/
	struct css_cam_input_desc	input; 	/* Input Description		*/
	struct css_cam_output_desc	output;	/* Output Description		*/
};

struct css_crop {
	u32 m_crop_h_offset;
	u32 m_crop_v_offset;
	u32 m_crop_h_size;
	u32 m_crop_v_size;
};

struct	css_scaling_factor {
	u32 m_scl_h_factor;
	u32 m_scl_v_factor;
};

struct css_dithering_set {
	css_scaler_dithering_set set;
	css_scaler_dithering_sel select;
	bool pedestal;
	bool y_gain;
	bool c_gain;
};

struct css_scaler_extern_config {
	u16						vsync_delay;
	struct css_crop			crop;
	struct css_strobe_set	strobe;
	struct css_effect_set	effect;
};

struct css_scaler_instruction {
	bool scaler_path;
	bool activate_read_path;
	bool activate_scaler_2;
	bool activate_scaler_1;
	bool activate_scaler_0;
};

struct css_async_delay {
	u32 async_pre_bayer_delay;
	u32 async_post_bayer_delay;
	u32 async_yuv_delay;
};

struct css_task_info {
	css_ispdi_status_of_task status_of_task;
};

struct css_vdis_instruction {
	css_vdis_scaler_select scaler_select;
	css_vdis_buffer_select dram_region_select;
	bool ls_vector_crop_enable;
	bool is_vector_sum_enable;
	bool param_set;
};

struct css_vdis_config {
	/* Motion Digital Image Stabilizer */
	u32 x_range;
	u32 y_range;
};

struct css_vdis {
	struct css_vdis_instruction vector_instruction;
	struct css_crop vector_crop;

	u32 input_frame_size;
	u32 h_sum_datasize;
	u32 v_sum_datasize;
	u32 h_sum_buf_size;
	u32 v_sum_buf_size;
	u32 v_rearrange_area_startpoint;
	u32 v_rearrange_area_endpoint;
	u32 v_rearrange_area_size;
	u32 h_sumarea_startpoint_0;
	u32 h_sumarea_endpoint_0;
	u32 h_sumarea_startpoint_1;
	u32 h_sumarea_endpoint_1;
	u32 h_sumarea_startpoint_2;
	u32 h_sumarea_endpoint_2;
	u32 v_sumarea_startpoint;
	u32 v_sumarea_endpoint;
	unsigned char *pre_h_sum_buffer;
	unsigned char *pre_v_sum_buffer;
	unsigned char *cur_h_sum_buffer;
	unsigned char *cur_v_sum_buffer;
};

struct css_query_control {
	u32 id;
	css_control_type type;
	u8	name[32];
	s32	minimum;
	s32	maximum;
	s32	step;
	s32	default_value;
	u32 flags;
	u32	reserved[2];
};

struct css_scaler_config {
	struct v4l2_format		v4l2_fmt;
	css_sensor_basic_type	sensor_type;
	css_scaler_action_type	action;
	/* scaler input size */
	unsigned short			src_width;
	unsigned short			src_height;
	/* scaler output size, setted by user */
	unsigned short			dst_width;
	unsigned short			dst_height;
	css_pixel_format		pixel_fmt;
	u32						frame_size;
	bool					dithering;
	u16						hw_buf_index;
	struct css_rect			crop;
	struct css_effect_set	effect;
	u32						y_padding;
};

struct css_scaler_data {
	u16 						vsync_delay;
	css_scaler_average_mode 	average_mode;
	css_pixel_format			output_pixel_fmt;
	css_scaler_instruction_mode instruction_mode;
	css_fd_source				fd_source;
	struct css_crop				crop;
	struct css_strobe_set		strobe;
	struct css_effect_set		effect;
	struct css_image_size		output_image_size;
	struct css_scaling_factor	scaling_factor;
};

typedef enum {
	CSI_CH0 = 0,
	CSI_CH1,
	CSI_CH2,
	CSI_CH_MAX
} css_mipi_csi_ch;

enum css_buffer_state {
	CSS_BUF_UNUSED = 0,
	CSS_BUF_QUEUED,
	CSS_BUF_CAPTURING,
	CSS_BUF_CAPTURE_DONE,
	CSS_BUF_ERR,
};

typedef enum {
	CSS_BUF_UNKNOWN = 0,
	CSS_BUF_PREVIEW,
	CSS_BUF_CAPTURE,
	CSS_BUF_CAPTURE_WITH_POSTVIEW,
#ifdef CONFIG_VIDEO_ODIN_S3DC
	CSS_BUF_PREVIEW_3D = CSS_BUF_3D_TYPE,
	CSS_BUF_CAPTURE_3D,
#endif
} css_buffer_type;


typedef enum {
	CSS_BUF_POP_TOP,
	CSS_BUF_POP_BOTTOM,
} css_buffer_pop_types;

enum {
	ONE_SHOT_MODE = 1,
	STREAM_MODE
};

/*
 * (1) use offset				E:empty
 *					size
 * |<--------------------------------->|
 * |<-------------->|//E//|<---------->|
 *	   byteused 		  offset
 *
 * (2) not use offset
 *					size
 * |<--------------------------------->|
 * |<-------------->|///////E//////////|
 * |	byteused
 * offset = 0
 */

struct css_buffer {
	int				id;
	void			*cpu_addr;
	dma_addr_t		dma_addr;
	int				state;
	unsigned long	size;
	unsigned short	width;
	unsigned short	height;
	ssize_t			offset;
	ssize_t			byteused;
	struct timeval	timestamp;
	bool			allocated;
	unsigned long	frame_cnt;
	unsigned int	vector[3]; /* [0]: x, [1]:y, [2]:frame_num */
	struct list_head	list;
	int 			ion_fd;
	struct ion_handle_data ion_hdl;
	int				imported;
};

struct css_bufq {
	int 				mode;
	int					ready;
	int 				error;
	int					count;
	int					index;
	int					q_pos;
	struct css_buffer	*bufs;
	spinlock_t			lock;
	struct list_head	head;
	struct completion	complete;
	struct ion_client	*cam_client;
	unsigned int 		capt_tot;	/* capture total count */
	unsigned int		drop_cnt;	/* for report fps only */
	unsigned int		capt_cnt;	/* for report fps only */
	unsigned int		skip_cnt;	/* for report fps only */
	unsigned int 		frminterval;
};

struct css_framebuffer {
	struct css_bufq		preview_bufq;
	struct css_bufq		stereo_bufq;
	struct css_bufq		capture_bufq;
	struct css_bufq		drop_bufq;
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_client	*drop_bufq_client;
#endif
};

struct css_scaler_device {
	struct css_scaler_config config;
	struct css_scaler_data	 data;
	unsigned int roop_err_cnt;
};

struct face_detect_buf {
	unsigned long	size;
	int				state;
	/* int			newest; */
	void			*cpu_addr;
	dma_addr_t		dma_addr;
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_handle_data ion_hdl;
	unsigned long ion_cpu_addr;
#endif
};

struct face_detect_config {
	unsigned short selete_path;
	unsigned int threshold;
	unsigned int direct;
	unsigned int res;
	unsigned short width;
	unsigned short height;
	unsigned short fd_scaler_src_width;
	unsigned short fd_scaler_src_height;
	css_fd_source fd_source;
	css_scaler_action_type action;
	struct css_rect crop;
};

struct face_info {
	struct css_face_detect_info fd_face_info[35];
#ifdef USE_FPD_HW_IP
	struct css_facial_parts_info fpd_face_info[35];
#endif
	int face_num;
	css_fd_state state;
	int buf_newest; /* smaller is the newset*/
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_handle_data ion_hdl;
	unsigned long ion_cpu_addr;
#endif
	struct css_rect crop;
};

struct css_fd_device {
	struct css_scaler_data 		fd_scaler_config;
	struct face_detect_config	config_data;		/* user_config from app */
	struct face_info			fd_info[FACE_BUF_MAX];
	unsigned int				buf_index;
	struct ion_client			*client;
	struct face_detect_buf		bufs[FACE_BUF_MAX]; /* image from fd_scaler */
#ifdef FD_PAIR_GET_DATA
	struct css_fd_result_data	result_data[2];
	struct css_fd_result_data	last_result_data[2]; /* copy to app */
#else
	struct css_fd_result_data	result_data; /* copy to app */
#endif
	int							fd_run;		/* fd run state */
	int							fpd_run;	/* fpd run state */
	int							init;		/* set fd config done */
	int							start;
};

struct css_zsl_device {
	css_zsl_path				path;
	int							load_running;
	int							frame_pos;
	int							frame_cnt;
	unsigned int				buf_width;
	unsigned int				buf_height;
	unsigned int				buf_count;
	unsigned int				buf_size;
	zsl_store_mode				storemode;
	int							frame_interval;
	zsl_store_raw_bitfmt		bitfmt;
	struct zsl_statistic		statistic;
	zsl_load_mode				loadmode;
	zsl_load_type				loadtype;
	unsigned int				loadbuf_idx;
	unsigned int				loadbuf_order;
	unsigned int				setloadframe;
	unsigned int				loadcnt;
	ktime_t 					cts;
	ktime_t 					pts;
	unsigned long				cfrm;
	unsigned long				pfrm;
	unsigned int				exp_frame;
	unsigned int				frame_per_ms;
	unsigned int				error;
	struct css_bufq				raw_buf;
	struct css_zsl_config		config;
	struct completion			store_done;
	struct completion			store_full;
	struct completion			load_done;
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_client 			*ion_client;
#endif
};

enum css_log_list {
	LOG_ERR		= 0x00000001,
	LOG_WARN	= 0x00000002,
	LOG_LOW		= 0x00000004,
	LOG_INFO	= 0x00000008,
	CAM_INT		= 0x00000010,
	CAM_V4L2	= 0x00000020,
	ZSL			= 0x00000040,
	ZSL_LOW		= 0x00000080,
	VNR			= 0x00000100,
	AFIFO		= 0x00000200,
	FD			= 0x00000400,
	FPD			= 0x00000800,
	FRGB		= 0x00001000,
	ISP			= 0x00002000,
	MDIS		= 0x00004000,
	MIPICSI0	= 0x00008000,
	MIPICSI1	= 0x00010000,
	MIPICSI2	= 0x00020000,
#ifdef CONFIG_VIDEO_ODIN_S3DC
	S3DC		= 0x00040000,
#endif
	CSS_SYS		= 0x00080000,
	JPEG_ENC	= 0x00100000,
	JPEG_V4L2	= 0x00200000,
	SENSOR		= 0x00400000,
};

#define SENSOR_MAX 2+1 /* +1 = dummy sensor */

struct css_sensor_init {
	void *value;
	void *cpu_addr;
	dma_addr_t dma_addr;
	unsigned int mmap_size;
	unsigned int init_size;
	unsigned int mode_size;
};

struct css_sensor_power {
	struct regulator *cam_1_2v_avdd;
	struct regulator *cam_vcm_1_8v_avdd;
	struct regulator *cam_2_8v_avdd;
	struct regulator *cam_1_8v_io;
	struct regulator *vtcam_1_2v_avdd;
	struct regulator *vtcam_2_8v_avdd;
	struct regulator *vtcam_1_8v_io;
};

struct css_ois_info {
	struct css_ois_lens_info lens;
	struct css_ois_status_data ois_status;
};

enum ois_mode_t{
	OIS_MODE_PREVIEW_CAPTURE,
	OIS_MODE_CAPTURE,
	OIS_MODE_VIDEO,
	OIS_MODE_CENTERING_ONLY,
	OIS_MODE_CENTERING_OFF,
	OIS_MODE_END,
};

enum ois_lens_mode{
	OIS_MODE_LENS_MOVE_START,
	OIS_MODE_LENS_MOVE_SET,
	OIS_MODE_LENS_MOVE_END,
};
struct css_i2cboard_platform_data { /* use sensor driver */
	unsigned int current_width;
	unsigned int current_height;
	unsigned int pixelformat;
	int freq;

	struct css_sensor_gpio power_gpio;
	struct css_sensor_gpio pwdn_gpio;
	struct css_sensor_gpio reset_gpio;
	struct css_sensor_gpio detect_gpio;
	struct css_sensor_gpio ois_reset_gpio;
	struct css_sensor_gpio ois_power_gpio;
	struct css_sensor_gpio io_enable_gpio;

	struct css_sensor_power power;

	/*
	 * css_polarity_active	vsync_polarity;
	 * css_polarity_active	hsync_polarity;
	 * unsigned int			pllhz;
	 * unsigned int			mclkhz;
	 */

	struct css_ois_info ois_info;

	char ois_sid;
	char sensor_sid;
	char eeprom_sid;

	int is_ois;
	int is_mipi;

#if 0
	void *init_value;
	int sensor_reg_size;
#else
	struct css_sensor_init sensor_init;
#endif
};

struct css_platform_sensor { /* use v4l2 driver */
	sensor_index			id;
	char					*name;
	/* css_sensor_logic		sensor_logic; */
	/* css_sensor_type		sensor_type; */
	char					initialize;
	char					power;
	char					ois_support;
	unsigned short			init_width, init_height;

	unsigned int			i2c_busnum;

	struct i2c_board_info	*info;
	struct v4l2_subdev		*sd;

	int						(*sensor_power)(int on);
};

struct css_platform_sensor_group {
	sensor_index	default_cam;
	struct css_platform_sensor	*sensor[SENSOR_MAX];
	/* unsigned int	i2c_busnum; */
};

struct css_sensor_device {
	struct css_platform_sensor *cur_sensor;
	struct css_platform_sensor *s3d_sensor;
	struct css_sensor_init mmap_sensor_init;
	/* 4bite init size + 4bite modee size+ value */
};

struct css_postview {
	u32 width;
	u32 height;
	u32 fmt;
	u32 size;
	u32 scaler;
};

struct css_rsvd_memory {
	struct resource	*res;
	unsigned int	base;
	unsigned int	size;
	unsigned int	next_base;
	int 			alloc_size;
};

struct css_afifo_ctrl {
	u32 index;
	u32 format;
};

struct css_afifo_pre {
	u32 index;
	u32 width;
	u32 height;
	u32 format;
    u32 read_path_en;
    u32 read_path_sel;
};

struct css_afifo_post {
	u32 index;
	u32 width;
	u32 height;
};

struct css_bayer_scaler {
	u32 index;
	u32 format;
	u32 src_w;
	u32 src_h;
	u32 dest_w;
	u32 dest_h;
	u32 margin_x;
	u32 margin_y;
	u32 offset_x;
	u32 offset_y;
	s32 scl_en;
	s32 crop_en;
};

struct css_crg_hardware
{
	struct platform_device	*pdev;
	struct resource *iores;
	void __iomem	*iobase;
	spinlock_t		 slock;
	struct mutex	 mutex;

	atomic_t pd_on;
	atomic_t pd0_user;
	atomic_t pd1_user;
	atomic_t pd2_user;
	atomic_t pd3_user;

	struct pm_qos_request mem_qos;
	struct pm_qos_request cci_qos;
};

struct scaler_hardware {
	struct platform_device	*pdev;

	int 			 irq;
	irq_handler_t	 irq_handler;

	struct resource *iores;
	void __iomem	*iobase;
	atomic_t		 users[3];
	spinlock_t		 slock;
	struct mutex	 hwmutex;
};

struct isp_hardware {
	struct platform_device	*pdev[2];
	struct resource *iores[2];
	void __iomem	*iobase[2];
	int irq_vsync[2];
	int irq_isp[2];
	int hw_init;
	spinlock_t slock[2];
};

struct fd_hardware {
	struct platform_device	*pdev;

	struct resource *iores;
	void __iomem	*iobase;
	int 			irq;
	spinlock_t		slock;
};

struct fpd_hardware {
	struct platform_device	*pdev;

	struct resource *iores;
	void __iomem	*iobase;
	int 			irq;
	spinlock_t		slock;
};


struct csi_hardware {
	struct platform_device	*pdev;

	struct resource *iores[CSI_CH_MAX];
	void __iomem	*iobase[CSI_CH_MAX];
	int 			irq[CSI_CH_MAX];
	spinlock_t		slock[CSI_CH_MAX];
	int	hw_init;
};

struct ispwrap_hardware {
	struct platform_device	*pdev[2];

	struct resource *iores[2];
	void __iomem	*iobase[2];
	spinlock_t		slock[2];
	int livepath;
	int hw_init;
};

struct mdis_hardware {
	struct platform_device	*pdev;

	struct resource *iores;
	void __iomem	*iobase;
	int 			irq;
	spinlock_t		slock;
	int	hw_init;
};

struct css_isp_clock {
	unsigned int cur_clk_hz;
	unsigned int set_clk_hz;
	unsigned int clk_hz[2];
};

struct css_afifo_config {
	int ctrl_index;
	int pre_index;
	int post_index;
	int bscl_index;
	struct css_afifo_ctrl	ctrl[2];
	struct css_afifo_pre	pre[2];
	struct css_afifo_post	post[2];
	struct css_bayer_scaler bayer_scl[2];
};

struct css_device {
	struct platform_device	*pdev;
	struct video_device		*vid_dev;
	struct v4l2_device		v4l2_dev;

	unsigned int 			*capfh;

	struct css_platform_sensor *sensor[SENSOR_MAX];

	struct css_postview		postview;
	css_path_mode			path_mode;
	unsigned int 			livepath_support;
	int 					stereo_enable;

	struct css_fd_device	*fd_device;
	struct css_zsl_device	zsl_device[CSS_MAX_ZSL];
	struct css_afifo_config	afifo;

	struct css_rsvd_memory	*rsvd_mem;
	struct css_isp_clock	ispclk;

	struct scaler_hardware	scl_hw;
	struct isp_hardware 	isp_hw;
	struct fd_hardware		fd_hw;
	struct fpd_hardware 	fpd_hw;
	struct csi_hardware 	csi_hw;
	struct ispwrap_hardware ispwrap_hw;
	struct mdis_hardware	mdis_hw;

	struct work_struct		work_bayer_loadpath0;
	struct work_struct		work_bayer_loadpath1;
	struct completion		vsync0_done;
	struct completion		vsync1_done;
	struct completion		vsync_fd_done;

	struct mutex			bayer_mutex;
};

struct css_buf_attribute {
	css_buffer_type			buf_type;
};

typedef enum {
	CSS_BEHAVIOR_NONE = 0x0,
	CSS_BEHAVIOR_PREVIEW,				/* To get preview data. */
	CSS_BEHAVIOR_CAPTURE_ZSL,			/* To get ZSL data */
	CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW,	/* To get ZSL data + POST VIEW(FHD) */
	CSS_BEHAVIOR_SNAPSHOT,				/* To get snapshot data */
	CSS_BEHAVIOR_SNAPSHOT_POSTVIEW,	/* To get snapshot data + POST VIEW(FHD) */
	CSS_BEHAVIOR_RECORDING,				/* To get record data */
	CSS_BEHAVIOR_PREVIEW_3D,			/* To get 3D preview data */
	CSS_BEHAVIOR_CAPTURE_ZSL_3D,		/* To get 3D ZSL data */
	CSS_BEHAVIOR_SET_DUMMI_SENSOR
} css_behavior;

#ifdef CONFIG_VIDEO_ODIN_JPEG_ENCODER

struct jpegenc_buffer {
	void			*cpu_addr;		/* linear address		*/
	dma_addr_t		dma_addr;		/* physical address		*/
	int 			state;			/* state of buffer		*/
	unsigned long	size;			/* size of jpg frame	*/
	ssize_t			offset;
	int 			alloc;
#ifdef CONFIG_ODIN_ION_SMMU
	int ion_fd;
	struct ion_client *ion_client;
	struct ion_handle_data ion_hdl;
#endif
};

enum jpegenc_state {
	JPEGENC_BUF_UNUSED = 0,
	JPEGENC_BUF_USING,
	JPEGENC_BUF_DONE,
};

struct jpegenc_platform {
	struct platform_device	*pdev;
	struct video_device		*video_dev;

	/* virtual register memory address */
	void __iomem			*io_base;
	struct resource			*iores;
	struct resource			*iomem;
	struct jpegenc_buffer	input_buf;
	struct jpegenc_buffer	output_buf;

	unsigned int			pmem_base;
	unsigned int			pmem_size;
	spinlock_t				hwlock; /* h/w controller lock */
	struct mutex			hwmutex;
	int						irq;
	int						initialize;
};

#endif

struct capture_fh {
	int							main_scaler;
	int							stream_on;
	int							zslstorefh;
	css_behavior				behavior;
	struct css_buf_attribute	buf_attrb;
	struct css_device			*cssdev;		/* css platform info */
	struct css_framebuffer		*fb;			/* frame buffers	 */
	struct css_scaler_device	*scl_dev;		/* scaler devices	 */
	struct css_sensor_device	*cam_dev;		/* camera devices	 */
	struct mutex				v4l2_mutex;
};

extern unsigned int css_log;

#define CSS_LOG_INIT(init_value)				\
	{	css_log = init_value;					\
	}
#define CSS_LOG_WARN(fmt, ...)					\
	do {										\
			if (css_log & LOG_WARN)				\
				printk(KERN_WARNING "CSS(WARN): %s(%d): " fmt, \
						__func__, __LINE__, 	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_ERROR(fmt, ...)					\
	do {										\
			if (css_log & LOG_ERR)				\
				printk(KERN_ERR "CSS(ERR): %s(%d): " fmt, __func__, __LINE__, \
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_LOW(fmt, ...)					\
	do {										\
			if (css_log & LOG_LOW)				\
				printk(KERN_INFO "[low]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_INFO(fmt, ...)					\
	do {										\
			if (css_log & LOG_INFO)				\
				printk(KERN_INFO "[info]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_CAM_INT(fmt, ...)				\
	do {										\
			if (css_log & CAM_INT)				\
				printk(KERN_INFO "[int]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_CAM_V4L2(fmt, ...)				\
	do {										\
			if (css_log & CAM_V4L2)			\
				printk(KERN_INFO "[v4l2]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_ZSL(fmt, ...)					\
	do {										\
			if (css_log & ZSL)					\
				printk(KERN_INFO "[zsl]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_ZSL_LOW(fmt, ...)				\
		do {									\
				if (css_log & ZSL_LOW)			\
					printk(KERN_INFO "[zsl_low]"fmt,	\
						##__VA_ARGS__);			\
		} while (0);
#define CSS_LOG_VNR(fmt, ...)					\
	do {										\
			if (css_log & VNR)					\
				printk(KERN_INFO "[vnr]"fmt,	\
						##__VA_ARGS__);			\
	} while (0);
#define CSS_LOG_AFIFO(fmt, ...)					\
		do {									\
				if (css_log & AFIFO)			\
					printk(KERN_INFO "[afifo]"fmt,		\
						##__VA_ARGS__);			\
		} while (0);
#define CSS_LOG_FD(fmt, ...)					\
		do {									\
				if (css_log & FD)				\
					printk(KERN_INFO "[fd]"fmt, \
						##__VA_ARGS__);			\
		} while (0);
#define CSS_LOG_FPD(fmt, ...)					\
				do {							\
						if (css_log & FPD)		\
							printk(KERN_INFO "[fpd]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_FRGB(fmt, ...)					\
				do {							\
						if (css_log & FRGB) 	\
							printk(KERN_INFO "[frgb]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_ISP(fmt, ...)					\
				do {							\
						if (css_log & ISP)		\
							printk(KERN_INFO "[isp]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_MDIS(fmt, ...)					\
				do {							\
						if (css_log & MDIS) 	\
							printk(KERN_INFO "[mdis]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_MIPICSI(fmt, ...)				\
				do {										\
						if (css_log & MIPICSI0 || css_log & MIPICSI1 || \
							css_log & MIPICSI2)			\
							printk(KERN_INFO "[mipi]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_S3DC(fmt, ...)					\
				do {							\
						if (css_log & S3DC) 	\
							printk(KERN_INFO "[s3dc]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_SYS(fmt, ...)					\
				do {							\
						if (css_log & CSS_SYS)	\
							printk(KERN_INFO "[sys]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_JPEG_ENC(fmt, ...)				\
				do {							\
						if (css_log & JPEG_ENC)			\
							printk(KERN_INFO "[jenc]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);
#define CSS_LOG_JPEG_V4L2(fmt, ...)				\
				do {							\
						if (css_log & JPEG_V4L2)	\
							printk(KERN_INFO "[jv4l2]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);

#define CSS_LOG_SENSOR(fmt, ...)				\
				do {							\
						if (css_log & SENSOR)	\
							printk(KERN_INFO "[sensor]"fmt,	\
						##__VA_ARGS__);			\
				} while (0);

#define css_log_init(init)		CSS_LOG_INIT(init);

#define css_err(fmt, ...)		CSS_LOG_ERROR(fmt, ##__VA_ARGS__);
#define css_warn(fmt, ...)		CSS_LOG_WARN(fmt, ##__VA_ARGS__);
#define css_low(fmt, ...)		CSS_LOG_LOW(fmt, ##__VA_ARGS__);
#define css_info(fmt, ...)		CSS_LOG_INFO(fmt, ##__VA_ARGS__);
#define css_cam_int(fmt, ...)	CSS_LOG_CAM_INT(fmt, ##__VA_ARGS__);
#define css_cam_v4l2(fmt, ...)	CSS_LOG_CAM_V4L2(fmt, ##__VA_ARGS__);
#define css_zsl(fmt, ...)		CSS_LOG_ZSL(fmt, ##__VA_ARGS__);
#define css_zsl_low(fmt, ...)	CSS_LOG_ZSL_LOW(fmt, ##__VA_ARGS__);
#define css_3dnr(fmt, ...)		CSS_LOG_VNR(fmt, ##__VA_ARGS__);
#define css_afifo(fmt, ...) 	CSS_LOG_AFIFO(fmt, ##__VA_ARGS__);
#define css_fd(fmt, ...)		CSS_LOG_FD(fmt, ##__VA_ARGS__);
#define css_fpd(fmt, ...)		CSS_LOG_FPD(fmt, ##__VA_ARGS__);
#define css_frgb(fmt, ...)		CSS_LOG_FRGB(fmt, ##__VA_ARGS__);
#define css_isp(fmt, ...)		CSS_LOG_ISP(fmt, ##__VA_ARGS__);
#define css_mdis(fmt, ...)		CSS_LOG_MDIS(fmt, ##__VA_ARGS__);
#define css_mipicsi(fmt, ...)	CSS_LOG_MIPICSI(fmt, ##__VA_ARGS__);
#define css_s3dc(fmt, ...)		CSS_LOG_S3DC(fmt, ##__VA_ARGS__);
#define css_sys(fmt, ...)		CSS_LOG_SYS(fmt, ##__VA_ARGS__);
#define css_jpeg_enc(fmt, ...)	CSS_LOG_JPEG_ENC(fmt, ##__VA_ARGS__);
#define css_jpeg_v4l2(fmt, ...) CSS_LOG_JPEG_V4L2(fmt, ##__VA_ARGS__);
#define css_sensor(fmt, ...)	CSS_LOG_SENSOR(fmt, ##__VA_ARGS__);

static int match(struct device *dev, void *data)
{
	const char *str = data;
	if (dev && dev->driver && dev->driver->name)
		return sysfs_streq(dev->driver->name, str);
	else
		return false;
}

static inline struct css_device *get_css_plat(void)
{
	struct device *dev;
	struct platform_device *pdev;

	dev = bus_find_device(&platform_bus_type, NULL, CSS_PLATDRV_CAPTURE, match);
	if (!dev) {
		css_err("failed to find device for %s\n", CSS_PLATDRV_CAPTURE);
		return NULL;
	}
	pdev = to_platform_device(dev);

	return (struct css_device *)platform_get_drvdata(pdev);
}

#define hw_to_plat(__hw, hw_id) container_of(__hw, struct css_device, hw_id)
#define ion_align_size(x) __ALIGN_KERNEL(x, SZ_2M)


#endif /* __ODIN_CSS_H__ */
