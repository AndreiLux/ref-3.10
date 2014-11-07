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

#include <asm/io.h>
#include "../odin-css.h"
#include "../odin-css-system.h"
#include "../odin-css-clk.h"
#include "odin-jpegenc.h"
#include "odin-jpegenc-codec.h"


#define ODIN_JPEGENC_RDHOLDCNT_DEFAULT	0x64
#define ODIN_JPEGENC_RD_DEALY_DEFAULT	0x00

static JPEGENC_REGISTERS g_jpeg_reg = {0,};
struct jpegenc_platform *g_jpeg_plat;

int jpegenc_configuration(struct jpegenc_device *jpeg_dev)
{
	JPEGENC_CODEC_CONFIG *codec_config = NULL;
	struct jpegenc_config *user_config = NULL;

	if (!jpeg_dev) {
		css_err("jpeg_enc_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	codec_config = &jpeg_dev->codec_hw_config;
	user_config = &jpeg_dev->config_data;

	if (user_config->width == 0 || user_config->height == 0 ||
			user_config->outbufsize == 0) {
		css_err("invalid size w=%d, h=%d, size=%d\n", user_config->width,
								user_config->height, user_config->outbufsize);
		return CSS_ERROR_JPEG_INVALID_SIZE;
	}

	if ( user_config->input_buf == 0 || user_config->output_buf == 0) {
		css_err("invalid buf i = %d, o = %d\n",user_config->input_buf,
								user_config->output_buf);
		return CSS_ERROR_JPEG_INVALID_BUF;
	}

	if (user_config->src_format == CSS_COLOR_YUV422) {
		codec_config->pixel_format = CSS_COLOR_YUV422;
	} else if (user_config->src_format == CSS_COLOR_RGB565) {
		codec_config->pixel_format = CSS_COLOR_RGB565;
	} else if (user_config->src_format == CSS_COLOR_YUV420) {
		codec_config->pixel_format = CSS_COLOR_YUV420;
	} else {
		css_err("unsupported jpeg_enc_format\n");
		return CSS_ERROR_JPEG_UNSUPPORTED_PIXELFORMAT;
	}

	codec_config->image_size.h_size= user_config->width;
	codec_config->image_size.v_size= user_config->height;
	codec_config->input_buf = user_config->input_buf;
	codec_config->output_buf = user_config->output_buf;
	codec_config->compress_level = user_config->compress_level;
	codec_config->jpeg_buf_size = user_config->outbufsize;

	codec_config->source_path = JPEG_SOURCE_MEMORY;
	codec_config->input_endian = JPEG_BIG_ENDIAN;
	codec_config->output_endian = JPEG_LITTLE_ENDIAN;
	codec_config->restart_mark_interval = 0x0;
	codec_config->read_hold_count = ODIN_JPEGENC_RDHOLDCNT_DEFAULT;
	if (user_config->read_delay) {
		codec_config->rd_delay = user_config->read_delay;
	} else {
		codec_config->rd_delay = ODIN_JPEGENC_RD_DEALY_DEFAULT;
	}

	return CSS_SUCCESS;
}

void jpegenc_interrupt_disable_all(void)
{
	JPEGENC_INTERRUPT interrupt;

	interrupt.as32bits = readl(g_jpeg_plat->io_base + JENC_INTERRUPT);
	interrupt.as32bits = interrupt.as32bits | 0x000AAAAA;
	writel(interrupt.as32bits,
		g_jpeg_plat->io_base + JENC_INTERRUPT);
}

void jpegenc_interrupt_enable_all(void)
{
	JPEGENC_INTERRUPT interrupt;

	interrupt.as32bits = 0x00000220;
	writel(interrupt.as32bits,
		g_jpeg_plat->io_base + JENC_INTERRUPT);
}

void jpegenc_interrupt_clear_status(unsigned int int_flag)
{
	JPEGENC_INTERRUPT interrupt;
	int clear_interrupt = int_flag & 0x000AAAAA;

	interrupt.as32bits = readl(g_jpeg_plat->io_base + JENC_INTERRUPT);
	interrupt.as32bits = interrupt.as32bits | clear_interrupt;
	writel(interrupt.as32bits,
		g_jpeg_plat->io_base + JENC_INTERRUPT);
}

void jpegenc_interrupt_clear_status_all(void)
{	/*same disable */
	JPEGENC_INTERRUPT interrupt;

	interrupt.as32bits = readl(g_jpeg_plat->io_base + JENC_INTERRUPT);
	interrupt.as32bits = interrupt.as32bits | 0x000AAAAA;
	writel(interrupt.as32bits,
		g_jpeg_plat->io_base + JENC_INTERRUPT);
}

unsigned int jpegenc_get_interrupt_status(void)
{
	JPEGENC_INTERRUPT interrupt;

	interrupt.as32bits = readl(g_jpeg_plat->io_base + JENC_INTERRUPT);
	interrupt.as32bits = interrupt.as32bits & 0x000FFFFF;
	return interrupt.as32bits;
}

unsigned int jpegenc_get_encoded_data_size(void)
{
	unsigned int encoded_data_size = 0;

	encoded_data_size = (unsigned int)readl(g_jpeg_plat->io_base
										+ JENC_OUT_SIZE_JPEG_ONLY);
	return encoded_data_size;
}

unsigned long jpegenc_get_output_size(void)
{
	unsigned long jpeg_size = 0;

	jpeg_size = (unsigned long)readl(g_jpeg_plat->io_base
										+ JENC_OUT_SIZE_JPEG_N_DUMMY);

	return jpeg_size;
}

void jpegenc_set_wrap(unsigned int wrap_margin, unsigned int wrap_intr_address)
{
	writel(wrap_margin, g_jpeg_plat->io_base + JENC_OUT_WRAP_MARGIN);
	writel(wrap_intr_address, g_jpeg_plat->io_base + JENC_OUT_WRAP_WARN_ADDR);
}

int jpegenc_hw_deinit(struct jpegenc_platform *jpeg_plat)
{
	if (!jpeg_plat) {
		css_err("jpeg_enc_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	disable_irq(jpeg_plat->irq);

	css_power_domain_off(POWER_JPEG);
	/* g_jpeg_plat = NULL; */
	jpeg_plat->initialize = 0;

	return CSS_SUCCESS;
}

int jpegenc_hw_init(struct jpegenc_platform *jpeg_plat)
{
	if (!jpeg_plat) {
		css_err("jpeg_enc_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	/* if (jpeg_plat->initialize == 0)
		g_jpeg_plat = jpeg_plat; */

	css_power_domain_on(POWER_JPEG);
	css_block_reset(BLK_RST_JPEGENC);

	enable_irq(jpeg_plat->irq);

	jpeg_plat->initialize = 1;

	return CSS_SUCCESS;
}

void jpegenc_clear(void)
{
	JPEGENC_CONTROL control;

	control.as32bits = 0;
	control.asfield.enc_clear = 0x1;
	writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);
}

int jpegenc_start(struct jpegenc_device *jpeg_dev)
{
	JPEGENC_CODEC_CONFIG *codec_hw_config = NULL;
	JPEGENC_CONTROL control;
	unsigned int read_cb_address = 0;
	unsigned int read_cr_address = 0;
	unsigned int wrap_addr;
	unsigned int i = 0;
	JPEGENC_PROPERTY	property;
	JPEGENC_SIZE		size;

	if (!jpeg_dev) {
		css_err("jpeg_enc_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	codec_hw_config = &jpeg_dev->codec_hw_config;

	/* 0. initialize	*/
	control.as32bits = 0;
	control.asfield.read_hold_count = codec_hw_config->read_hold_count;

	/* interrupt enable */
	writel(0x00000220, g_jpeg_plat->io_base + JENC_INTERRUPT);

	/* 1. Set Path & read delay */
	if (codec_hw_config->source_path ==	JPEG_SOURCE_MEMORY)
		control.asfield.path_select = 0x0;
	else if (codec_hw_config->source_path == JPEG_SOURCE_SCALER)
		control.asfield.path_select = 0x1;

	if (codec_hw_config->pixel_format == CSS_COLOR_YUV422)
		control.asfield.rd_delay = codec_hw_config->rd_delay;
	else
		control.asfield.rd_delay = 0x00;
	css_jpeg_enc("jpeg rd-delay = %d\n", codec_hw_config->rd_delay);
	writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);

	/* 2. Set Address */
	writel(codec_hw_config->output_buf,
					g_jpeg_plat->io_base + JENC_OUT_BASEADDR);
	writel(codec_hw_config->input_buf,
					g_jpeg_plat->io_base + JENC_IN_Y_BASEADDR);

	read_cb_address = codec_hw_config->input_buf
						+ (codec_hw_config->image_size.h_size
						* codec_hw_config->image_size.v_size);
	writel(read_cb_address, g_jpeg_plat->io_base + JENC_IN_CB_BASEADDR);

	read_cr_address = read_cb_address
						+ (codec_hw_config->image_size.h_size
						* codec_hw_config->image_size.v_size / 4);
	writel(read_cr_address, g_jpeg_plat->io_base + JENC_IN_CR_BASEADDR);

	writel(0, g_jpeg_plat->io_base + JENC_EXIF_ADDR);

	/* 3. Set Pixel Format */
	if (codec_hw_config->pixel_format == CSS_COLOR_YUV422)
		writel(0x0, g_jpeg_plat->io_base + JENC_IN_FORMAT);
	else if (codec_hw_config->pixel_format == CSS_COLOR_YUV420)
		writel(0x1, g_jpeg_plat->io_base + JENC_IN_FORMAT);
	else if (codec_hw_config->pixel_format == CSS_COLOR_RGB565)
		writel(0x2, g_jpeg_plat->io_base + JENC_IN_FORMAT);

	/* 4. Set QTable */
	if (codec_hw_config->q_table_enable == JPEG_MODULE_ENABLE) {
		control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
		control.asfield.q_table_access_enable = 1;
		writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);

		for (i=0; i < 128; i++) {
			writel(i & 0x7F, g_jpeg_plat->io_base + JENC_QROM_ADDR);
			writel(codec_hw_config->q_table_data[i] & 0x7F,
					g_jpeg_plat->io_base + JENC_QROM_DATA);
		}
		control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
		control.asfield.q_table_access_enable = 0;
		writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);
	}

	/* 5. Set Quality Factor */
	property.as32bits = 0;
	if (codec_hw_config->compress_level < 0)
		property.asfield.quantization_factor = 0;
	else if (codec_hw_config->compress_level > 0xFF)
		property.asfield.quantization_factor = 0xFF;
	else
		property.asfield.quantization_factor = codec_hw_config->compress_level;

	property.asfield.restart_marker_interval
					= codec_hw_config->restart_mark_interval;
	writel(property.as32bits & 0xFFFFFF, g_jpeg_plat->io_base + JENC_PROPERTY);

	/* 6 Set Size */
	size.as32bits = 0;
	size.asfield.v_size = codec_hw_config->image_size.v_size;
	size.asfield.h_size = codec_hw_config->image_size.h_size;
	writel(size.as32bits, g_jpeg_plat->io_base + JENC_SIZE);
	css_jpeg_enc("jpegv_size = %d\n", size.asfield.v_size);

	wrap_addr = codec_hw_config->output_buf + codec_hw_config->jpeg_buf_size;
	writel(0x0, g_jpeg_plat->io_base + JENC_OUT_WRAP_MARGIN);
	writel(wrap_addr, g_jpeg_plat->io_base + JENC_OUT_WRAP_WARN_ADDR);

	/* 7. Set Endian */
	control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
	if (codec_hw_config->input_endian == JPEG_BIG_ENDIAN)
		control.asfield.input_endian = 0;
	else if (codec_hw_config->input_endian == JPEG_LITTLE_ENDIAN)
		control.asfield.input_endian = 0x1;

	if (codec_hw_config->output_endian == JPEG_BIG_ENDIAN)
		control.asfield.output_endian = 0;
	else if (codec_hw_config->output_endian == JPEG_LITTLE_ENDIAN)
		control.asfield.output_endian = 0x1;

	writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);

	/* 8. Set Exif */
	if (codec_hw_config->exif_enable == JPEG_MODULE_ENABLE) {
		control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
		control.asfield.exif_memory_access = 1;
		writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);

		for (i=0; i<10; i++) {
			writel(i, g_jpeg_plat->io_base + JENC_EXIF_ADDR);
			writel('A' + i, g_jpeg_plat->io_base + JENC_EXIF_DATA);
		}

		control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
		control.asfield.exif_memory_access = 0;
		control.asfield.exif_enable = 1;
		writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);
	}

	/* 9. Set Scalado */
	if (codec_hw_config->scalado_enable == JPEG_MODULE_ENABLE) {
		control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
		control.asfield.scalado_enable = 1;
		control.asfield.scalado_endian = JPEG_LITTLE_ENDIAN;
		writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);
		writel((unsigned int)codec_hw_config->scalado_buf,
				g_jpeg_plat->io_base + JENC_SCALADO_BASEADDR);
		codec_hw_config->scalado_size = (codec_hw_config->image_size.h_size
				* codec_hw_config->image_size.v_size) / 16;
	}

	control.as32bits = readl(g_jpeg_plat->io_base + JENC_CONTROL);
	control.asfield.enc_clear = 1;
	writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);
	control.asfield.enc_start = 1;
	control.asfield.enc_clear = 0;
	writel(control.as32bits, g_jpeg_plat->io_base + JENC_CONTROL);

	return CSS_SUCCESS;
}

