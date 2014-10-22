/* linux/drivers/media/platform/exynos/jpeg4/jpeg_regs.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file of the register interface for jpeg v4.x driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __JPEG_REGS_H__
#define __JPEG_REGS_H__

#include "jpeg.h"

void jpeg_sw_reset(void __iomem *base);
void jpeg_set_enc_dec_mode(void __iomem *base, enum jpeg_mode mode);
void jpeg_set_image_fmt(void __iomem *base, u32 cfg);
void jpeg_set_enc_tbl(void __iomem *base,
		enum jpeg_img_quality_level level);
void jpeg_set_dec_tbl(void __iomem *base,
		struct jpeg_tables *tables);
void jpeg_set_interrupt(void __iomem *base);
void jpeg_clean_interrupt(void __iomem *base);
unsigned int jpeg_get_int_status(void __iomem *base);
void jpeg_set_huf_table_enable(void __iomem *base, int value);
void jpeg_set_dec_scaling(void __iomem *base, enum jpeg_scale_value value);
void jpeg_set_image_addr(void __iomem *base, struct m2m1shot_buffer_dma *buf,
		struct jpeg_fmt *fmt, unsigned int width, unsigned int height);
void jpeg_set_stream_addr(void __iomem *base, unsigned int address);
void jpeg_set_stream_size(void __iomem *base,
		unsigned int x_value, unsigned int y_value);
void jpeg_set_encode_tbl_select(void __iomem *base,
		enum jpeg_img_quality_level level);
void jpeg_set_decode_tbl_select(void __iomem *base,
		struct jpeg_tables_info *tinfo);
void jpeg_set_encode_hoff_cnt(void __iomem *base, unsigned int fourcc);
void jpeg_set_dec_bitstream_size(void __iomem *base, unsigned int size);
void jpeg_set_timer_count(void __iomem *base, unsigned int size);
unsigned int jpeg_get_stream_size(void __iomem *base);
void jpeg_get_frame_size(void __iomem *base,
			unsigned int *width, unsigned int *height);
int jpeg_set_number_of_component(void __iomem *base,
		unsigned int num_component);
void jpeg_alpha_value_set(void __iomem *base, unsigned int alpha);
void jpeg_dec_window_ctrl(void __iomem *base, unsigned int is_start);
void jpeg_set_window_margin(void __iomem *base, unsigned int top,
		unsigned int bottom, unsigned int left, unsigned int right);
void jpeg_get_window_margin(void __iomem *base, unsigned int *top,
		unsigned int *bottom, unsigned int *left, unsigned int *right);
void jpeg_set_decode_huff_cnt(void __iomem *base, unsigned int cnt);
#endif /* __JPEG_REGS_H__ */
