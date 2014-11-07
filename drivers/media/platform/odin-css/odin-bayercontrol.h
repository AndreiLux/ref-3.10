/*
 * Camera Bayer Control (Store/Load) driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
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

#ifndef __ODIN_BAYERCONTROL_H__
#define __ODIN_BAYERCONTROL_H__

#define BAYER_LOAD_STREAM_TYPE	0x5253524D

int bayer_load_init_count(struct capture_fh *capfh);
void bayer_load_set_type(struct capture_fh *capfh, zsl_load_type type);
zsl_load_type bayer_load_get_type(struct capture_fh *capfh);
int bayer_buffer_alloc(struct capture_fh *capfh);
int bayer_buffer_free(struct capture_fh *capfh);
int bayer_buffer_flush(struct capture_fh *capfh);
int bayer_buffer_push(struct css_bufq *bufq, int i);
struct css_buffer *bayer_buffer_pop_frame(struct css_bufq *bufq,
											css_buffer_pop_types type);
int bayer_buffer_get_queued_count(struct css_bufq *bufq);
struct css_buffer *bayer_buffer_get_frame(struct css_bufq *bufq,
											css_buffer_pop_types type);
void bayer_buffer_release_frame(struct css_bufq *bufq,
											struct css_buffer *cssbuf);

int bayer_store_get_status(struct capture_fh *capfh);
int bayer_store_enable(struct capture_fh *capfh);
int bayer_store_pause(struct capture_fh *capfh);
int bayer_store_resume(struct capture_fh *capfh);
int bayer_store_disable(struct capture_fh *capfh);
int bayer_store_mode(struct capture_fh *capfh, zsl_store_mode mode);
int bayer_store_frame_interval(struct capture_fh *capfh, unsigned int interval);
int bayer_store_bit_format(struct capture_fh *capfh, zsl_store_raw_bitfmt fmt);
int bayer_store_wait_buffer_full(struct capture_fh *capfh);
int bayer_store_statistic(struct capture_fh *capfh,
								struct zsl_store_statistic *info);
int bayer_store_get_queued_count(struct capture_fh *capfh, unsigned int *count);
int bayer_load_frame_number(struct capture_fh *capfh, int num);
int bayer_load_mode(struct capture_fh *capfh, zsl_load_mode mode);
int bayer_load_wait_done(struct capture_fh *capfh);
int bayer_load_buf_index(struct capture_fh *capfh, unsigned int idx);
int bayer_load_buf_order(struct capture_fh *capfh, unsigned int order);
int bayer_load_schedule(struct capture_fh *load_fh);
int bayer_load_frame(struct capture_fh *load_fh);
int bayer_store_int_handler(struct capture_fh *capfh);
int bayer_stereo_store_intr_handler(struct css_device *cssdev,
									unsigned int int_status);
int bayer_stereo_load_frame(struct capture_fh *load_fh);
void bayer_fd_open(int *fd, unsigned int *size, unsigned int *align);
void bayer_fd_release(void);


#endif /* __ODIN_BAYERCONTROL_H__ */
