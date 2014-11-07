/*
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

#ifndef __ODIN_CAMERA_H__
#define __ODIN_CAMERA_H__


#define ODIN_V4L2_DEV_NAME	"odin-v4l2"
#define ZSL_STORE_DROP_CAPTURE_LINE			4
#define ZSL_LOAD_1CH_BLKCNT_MULTIPLE(x)		(x * 0) + 100
#define ZSL_LOAD_2CH_BLKCNT_MULTIPLE(x)		(x * 4)
#define ZSL_LOAD_3D_BLKCNT_MULTIPLE(x)		(x * 5)
#define VSYNC_WAIT_TIME_MS					5000
#define FPS_TO_MS(x)						(1000/x)
#define ODIN_CAMERA_ERR_REPORT				1

/* #define TEST_INIT_VALUE */

#define capture_stream_mode_on(capfh)		capfh->stream_on = 1
#define capture_stream_mode_off(capfh)		capfh->stream_on = 0
#define capture_get_stream_mode(capfh)		capfh->stream_on
#define capture_get_zsl_dev(capfh, path)	&capfh->cssdev->zsl_device[path]
#define capture_get_path_mode(cssdev)		cssdev->path_mode

int capture_set_postview_config(struct css_postview *postview,
								css_scaler_select postview_scl,
								struct css_rect *crop_rect, u32 phy_addr);
u32 capture_get_frame_size(u32 pixelformat, u32 w, u32 h);
int capture_set_try_format(struct capture_fh *capfh, struct v4l2_format *fmt);
int capture_set_format(struct capture_fh *capfh, struct v4l2_format *fmt);
int capture_buffer_get_queued_index(struct css_bufq *bufq);
int capture_buffer_get_next_index(struct css_bufq *bufq);
struct css_bufq *capture_buffer_get_pointer(struct css_framebuffer *cap_fb,
								css_buffer_type buf_type);
int capture_get_stereo_status(struct css_device *cssdev);

int capture_buffer_init(struct capture_fh *capfh, int bufs_cnt);
int capture_buffer_deinit(struct capture_fh *capfh);
int capture_buffer_close(struct capture_fh *capfh);
void capture_buffer_flush(struct capture_fh *capfh);
int capture_buffer_push(struct css_bufq *bufq, int i);
struct css_buffer *capture_buffer_pop(struct css_bufq *bufq);

unsigned int capture_get_done_mask(struct capture_fh *capfh);
int capture_wait_scaler_vsync(struct css_device *cssdev,
								css_scaler_select scaler, unsigned int msec);
void capture_wait_any_scaler_vsync(unsigned int msec);
int capture_skip_frame(struct css_device *cssdev, css_scaler_select scaler,
								unsigned int framecnt);
int capture_wait_done(unsigned int mask, unsigned int msec);

int capture_start(struct capture_fh *capfh, struct css_bufq *bufq,
								int reset_frame_cnt);
int capture_stop(struct capture_fh *capfh);
void capture_get_fps_info(void);
void capture_fps_log(void *data);

int capture_fps_log_init(void);
int capture_fps_log_deinit(void);

#endif /* __ODIN_CAMERA_H__ */
