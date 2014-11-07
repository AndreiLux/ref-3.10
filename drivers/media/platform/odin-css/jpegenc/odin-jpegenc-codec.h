/*
 * JPEG Encoder driver
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

#ifndef __ODIN_JPEGENC_CODEC_H__
#define __ODIN_JPEGENC_CODEC_H__


enum jpeg_source_path {
	JPEG_SOURCE_INVALID	= -1,
	JPEG_SOURCE_MEMORY	= 0,
	JPEG_SOURCE_SCALER	= 1
};

enum jpeg_endian {
	JPEG_BIG_ENDIAN = 0,
	JPEG_LITTLE_ENDIAN = 1
};

enum jpeg_module_status {
	JPEG_MODULE_INVALID = -1,
	JPEG_MODULE_DISABLE = 0,
	JPEG_MODULE_ENABLE = 1
};

typedef enum {
	JPEG_INVALID = -1,
	JPEG_YUV422 = 0,
	JPEG_YUV420 = 1,
	JPEG_RGB565 = 2
} JPEG_PIXEL_FORMAT;

typedef union {
	struct {
		volatile unsigned int enc_start : 1;				/* 0 */
		volatile unsigned int enc_clear : 1;				/* 1 */
		volatile unsigned int path_select : 2;				/* 2-3 */
		volatile unsigned int input_endian : 1;				/* 4 */
		volatile unsigned int output_endian : 1;			/* 5 */
		volatile unsigned int q_table_access_enable : 1;	/* 6 */
		volatile unsigned int exif_memory_access : 1;		/* 7 */
		volatile unsigned int exif_enable : 1;				/* 8 */
		volatile unsigned int scalado_enable : 1;			/* 9 */
		volatile unsigned int delete_marker_soi : 1;		/* 10 */
		volatile unsigned int delete_marker_dqt : 1;		/* 11 */
		volatile unsigned int delete_marker_sofo : 1;		/* 12 */
		volatile unsigned int delete_marker_dri : 1;		/* 13 */
		volatile unsigned int delete_marker_dht : 1;		/* 14 */
		volatile unsigned int delete_marker_sos : 1;		/* 15 */
		volatile unsigned int scalado_endian : 1;			/* 16 */
		volatile unsigned int read_hold_count : 9;			/* 17-25 */
		volatile unsigned int rd_delay : 4;					/* 26-28 */
		volatile unsigned int reserved30 : 2;				/* 29-31 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_CONTROL;

typedef union {
	struct {
		volatile unsigned int v_size : 16;					/* 0-15 */
		volatile unsigned int h_size : 16;					/* 16-31 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_SIZE;

typedef union {
	struct {
		volatile unsigned int restart_marker_interval : 16;	/* 0-15 */
		volatile unsigned int quantization_factor : 8;		/* 16-23 */
		volatile unsigned int reserved24 : 8;				/* 24-31 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_PROPERTY;

typedef union {
	struct {
		volatile unsigned int q_table_address : 7;			/* 0-6 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_QTABLE_ADDR;

typedef union {
	struct {
		volatile unsigned int q_table_data : 8;				/* 0-7 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_QTABLE_DATA;

typedef union {
	struct {
		volatile unsigned int format : 2;					/* 0-1 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_FORMAT;

typedef union {
	struct {
		volatile unsigned int enc_done_status : 1;				/* 0 */
		volatile unsigned int enc_done_mask : 1;				/* 1 */
		volatile unsigned int write_fifo_overflow_status : 1;	/* 2 */
		volatile unsigned int write_fifo_overflow_mask : 1;		/* 3 */
		volatile unsigned int header_done_status : 1;			/* 4 */
		volatile unsigned int header_done_mask : 1;				/* 5 */
		volatile unsigned int wrap_done_status : 1;				/* 6 */
		volatile unsigned int wrap_done_mask : 1;				/* 7 */
		volatile unsigned int read_done_status : 1;				/* 8 */
		volatile unsigned int read_done_mask : 1;				/* 9 */
		volatile unsigned int scalado_fifo_overflow_status : 1;	/* 10 */
		volatile unsigned int scalado_fifo_overflow_mask : 1;	/* 11 */
		volatile unsigned int block_fifo_error_status : 1;		/* 12 */
		volatile unsigned int block_fifo_error_mask : 1;		/* 13 */
		volatile unsigned int block_fifo_overflow_status : 1;	/* 14 */
		volatile unsigned int block_fifo_overflow_mask : 1;		/* 15 */
		volatile unsigned int flat_fifo_error_status : 1;		/* 16 */
		volatile unsigned int flat_fifo_error_mask : 1;			/* 17 */
		volatile unsigned int flat_fifo_overflow_status : 1;	/* 18 */
		volatile unsigned int flat_fifo_overflow_mask : 1;		/* 19 */
		volatile unsigned int reserved20 : 12;					/* 20-31 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_INTERRUPT;

typedef union {
	struct {
		volatile unsigned int exif_address : 24;			/* 0-23 */
	} asfield;
	volatile unsigned int as32bits;
} JPEGENC_EXIF_ADDR;

typedef struct {
	JPEGENC_CONTROL				control;					/* 0x00 */
	JPEGENC_SIZE				size;						/* 0x04 */
	JPEGENC_PROPERTY 			property;					/* 0x08 */
	volatile unsigned int		base_address;				/* 0x0c */
	volatile unsigned int		wrap_margin;				/* 0x10 */
	volatile unsigned int		wrap_intr_address;			/* 0x14 */
	JPEGENC_QTABLE_ADDR			q_table_address;			/* 0x18 [0-6] */
	JPEGENC_QTABLE_DATA			q_table_data;				/* 0x1C [0-7] */
	volatile unsigned int		current_address;			/* 0x20 */
	volatile unsigned int		jpeg_stream_size;			/* 0x24 */
	volatile unsigned int		jpeg_and_dummy_size;		/* 0x28 */
	JPEGENC_FORMAT				input_format;				/* 0x2C */
	volatile unsigned int		read_y_address;				/* 0x30 */
	volatile unsigned int		read_cb_address;			/* 0x34 */
	volatile unsigned int		read_cr_address;			/* 0x38 */
	JPEGENC_INTERRUPT			interrupt;					/* 0x3C */
	JPEGENC_EXIF_ADDR			exif_write_address;			/* 0x40 */
	volatile unsigned int		exif_data;					/* 0x44 */
	volatile unsigned int		scalado_index_address;		/* 0x48 */
} JPEGENC_REGISTERS;

#define JENC_CONTROL				0x00
#define JENC_SIZE					0x04
#define JENC_PROPERTY				0x08
#define JENC_OUT_BASEADDR			0x0C
#define JENC_OUT_WRAP_MARGIN		0X10
#define JENC_OUT_WRAP_WARN_ADDR		0X14
#define JENC_QROM_ADDR				0X18
#define JENC_QROM_DATA				0X1C
#define JENC_OUT_ADDR				0X20
#define JENC_OUT_SIZE_JPEG_ONLY		0X24
#define JENC_OUT_SIZE_JPEG_N_DUMMY	0X28
#define JENC_IN_FORMAT				0X2C
#define JENC_IN_Y_BASEADDR			0X30
#define JENC_IN_CB_BASEADDR			0X34
#define JENC_IN_CR_BASEADDR			0X38
#define JENC_INTERRUPT				0X3C
#define JENC_EXIF_ADDR				0X40
#define JENC_EXIF_DATA				0X44
#define JENC_SCALADO_BASEADDR		0X48


int jpegenc_configuration(struct jpegenc_device *jpeg_dev);
void jpegenc_interrupt_disable_all(void);
void jpegenc_interrupt_enable_all(void);
void jpegenc_interrupt_clear_status(unsigned int int_flag);
void jpegenc_interrupt_clear_status_all(void);
unsigned int jpegenc_get_interrupt_status(void);
unsigned int jpegenc_get_encoded_data_size(void);
unsigned long jpegenc_get_output_size(void);
void jpegenc_set_wrap(unsigned int wrap_margin,unsigned int wrap_intr_address);
int jpegenc_hw_deinit(struct jpegenc_platform *jpeg_platform);
int jpegenc_hw_init(struct jpegenc_platform *jpeg_platform);
void jpegenc_clear(void);
int jpegenc_start(struct jpegenc_device *jpeg_dev);


#endif /* __ODIN_JPEGENC_CODEC_H__ */
