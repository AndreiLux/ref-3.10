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

#ifndef __ODIN_JPEGENC_H__
#define __ODIN_JPEGENC_H__


#define ODIN_JPEGENC_V4L2_DEV_NAME	"odin-jpeg-enc-v4l2"
#define ODIN_JPEGENC_DRV_NAME		"odin-jpeg-enc"
#define CSS_OF_JPEGENC_NAME			"mtekvision,jpeg-encoder"

#define JPEGENC_BUFFER_SIZE			(3 * 1024 * 1024)
#define JPEGENC_FOR_HEADER_SIZE		4192
#define JPEGENC_HEIGHT_POSITION		141

#define JPEGENC_WRITE_END					(1 << 0)
#define JPEGENC_WRITE_OVERFLOW				(1 << 2)
#define JPEGENC_HEADER_END					(1 << 4)
#define JPEGENC_WRAP_END					(1 << 6)
#define JPEGENC_MEM_READ_END				(1 << 8)
#define JPEGENC_SCALADO_FIFO_OVERFLOW		(1 << 10)
#define JPEGENC_BLOCK_FIFO_ERROR			(1 << 12)
#define JPEGENC_BLOCK_FIFO_OVERFLOW			(1 << 14)
#define JPEGENC_FLAT_FIFO_ERROR				(1 << 16)
#define JPEGENC_FLAT_FIFO_OVERFLOW			(1 << 18)

#define JPEGENC_WRITE_END_MASK				(1 << 1)
#define JPEGENC_WRITE_OVERFLOW_MASK			(1 << 3)
#define JPEGENC_HEADER_END_MASK				(1 << 5)
#define JPEGENC_WRAP_END_MASK				(1 << 7)
#define JPEGENC_MEM_READ_END_MASK			(1 << 9)
#define JPEGENC_SCALADO_FIFO_OVERFLOW_MASK	(1 << 11)
#define JPEGENC_BLOCK_FIFO_ERROR_MASK		(1 << 13)
#define JPEGENC_BLOCK_FIFO_OVERFLOW_MASK	(1 << 15)
#define JPEGENC_FLAT_FIFO_ERROR_MASK		(1 << 17)
#define JPEGENC_FLAT_FIFO_OVERFLOW_MASK		(1 << 19)

struct jpegenc_config {
	unsigned int		width;
	unsigned int		height;
	unsigned int		outbufsize;
	int 				compress_level;
	css_pixel_format		src_format;
	unsigned int		input_buf;
	unsigned int		output_buf;
	/* if src_path is memory then we can use the this member */
	void				*inbuf;
	unsigned int		bufsize;
	unsigned int 		read_delay;
};

typedef struct {
	unsigned int	h_size;
	unsigned int	v_size;
} JPEG_IMAGE_SIZE;

typedef struct {
	unsigned int		input_buf;
	unsigned int		output_buf;
	JPEG_IMAGE_SIZE		image_size;
	int					source_path;
	int					scalado_enable;
	unsigned int		*scalado_buf;
	unsigned int		scalado_size;
	int					exif_enable;
	unsigned int		*exif_buf;
	unsigned int		input_endian;
	unsigned int		output_endian;
	css_pixel_format	pixel_format;
	int					compress_level;
	int					q_table_enable;
	char				q_table_data[128];
	unsigned int		read_hold_count;
	unsigned int		rd_delay;
	unsigned int		restart_mark_interval;
	unsigned int		jpeg_buf_size;
	unsigned long		jpeg_size;
} JPEGENC_CODEC_CONFIG;

struct jpegenc_device {
	struct jpegenc_config	config_data;	/* user_config from app */
	JPEGENC_CODEC_CONFIG	codec_hw_config;
	struct completion		enc_done;
	int 					error;
	int						error_num;
	int						modify_header;
	unsigned int			backup_height;
};

static struct jpegenc_fh *g_jpeg_fh;
extern struct jpegenc_platform *g_jpeg_plat;

struct jpegenc_fh {
	struct jpegenc_platform	*jpeg_plat;	/* jpeg enc platform info */
	struct jpegenc_device	*jpeg_dev;	/* jpeg encode devices */
};

#endif /* __ODIN_JPEGENC_H__ */
