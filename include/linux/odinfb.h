/*
 * File: include/linux/odinfb.h
 *
 * Framebuffer driver for Odin boards
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __LINUX_ODINFB_H__
#define __LINUX_ODINFB_H__

#include <linux/fb.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/ion.h>

/* IOCTL commands. */

#define ODIN_IOW(num, dtype)    _IOW('O', num, dtype)
#define ODIN_IOR(num, dtype)    _IOR('O', num, dtype)
#define ODIN_IOWR(num, dtype)   _IOWR('O', num, dtype)
#define ODIN_IO(num)            _IO('O', num)

#define ODINFB_MIRROR           ODIN_IOW(31, int)
#define ODINFB_SYNC_GFX         ODIN_IO(37)
#define ODINFB_VSYNC            ODIN_IO(38)
#define ODINFB_SET_UPDATE_MODE  ODIN_IOW(40, int)
#define ODINFB_GET_CAPS         ODIN_IOR(42, struct odinfb_caps)
#define ODINFB_GET_UPDATE_MODE  ODIN_IOW(43, int)
#define ODINFB_LCD_TEST         ODIN_IOW(45, int)
#define ODINFB_CTRL_TEST        ODIN_IOW(46, int)
#define ODINFB_UPDATE_WINDOW_OLD ODIN_IOW(47, struct odinfb_update_window_old)
#define ODINFB_SET_COLOR_KEY    ODIN_IOW(50, struct odinfb_color_key)
#define ODINFB_GET_COLOR_KEY    ODIN_IOW(51, struct odinfb_color_key)
#define ODINFB_SETUP_PLANE      ODIN_IOW(52, struct odinfb_plane_info)
#define ODINFB_QUERY_PLANE      ODIN_IOW(53, struct odinfb_plane_info)
#define ODINFB_UPDATE_WINDOW    ODIN_IOW(54, struct odinfb_update_window)
#define ODINFB_SETUP_MEM        ODIN_IOW(55, struct odinfb_mem_info)
#define ODINFB_QUERY_MEM        ODIN_IOW(56, struct odinfb_mem_info)
#define ODINFB_WAITFORVSYNC     ODIN_IO(57)
#define ODINFB_MEMORY_READ      ODIN_IOR(58, struct odinfb_memory_read)
#define ODINFB_GET_OVERLAY_COLORMODE ODIN_IOR(59, struct odinfb_ovl_colormode)
#define ODINFB_GET_VRAM_INFO    ODIN_IOR(61, struct odinfb_vram_info)
#define ODINFB_SET_TEARSYNC     ODIN_IOW(62, struct odinfb_tearsync_info)
#define ODINFB_GET_DISPLAY_INFO ODIN_IOR(63, struct odinfb_display_info)
#define ODINFB_ENABLE_VSYNC	ODIN_IOW(64, int)
#define ODINFB_ADDR_USING	ODIN_IOWR(65, struct odinfb_addr_check)
#if 0
#define ODINFB_BUFFER_SYNC	ODIN_IOW(66, struct odin_buf_sync)
#define ODINFB_DISPLAY_COMMIT	ODIN_IOW(67, struct odinfb_display_commit)
#endif
#define ODINFB_CAPS_GENERIC_MASK        0x00000fff
#define ODINFB_CAPS_LCDC_MASK           0x00fff000
#define ODINFB_CAPS_PANEL_MASK          0xff000000

#define ODINFB_CAPS_MANUAL_UPDATE       0x00001000
#define ODINFB_CAPS_TEARSYNC            0x00002000
#define ODINFB_CAPS_PLANE_RELOCATE_MEM  0x00004000
#define ODINFB_CAPS_PLANE_SCALE         0x00008000
#define ODINFB_CAPS_WINDOW_PIXEL_DOUBLE 0x00010000
#define ODINFB_CAPS_WINDOW_SCALE        0x00020000
#define ODINFB_CAPS_WINDOW_OVERLAY      0x00040000
#define ODINFB_CAPS_WINDOW_ROTATE       0x00080000
#define ODINFB_CAPS_SET_BACKLIGHT       0x01000000

/* Values from DSP must map to lower 16-bits */
#define ODINFB_FORMAT_MASK              0x00ff
#define ODINFB_FORMAT_FLAG_DOUBLE       0x0100
#define ODINFB_FORMAT_FLAG_TEARSYNC     0x0200
#define ODINFB_FORMAT_FLAG_FORCE_VSYNC  0x0400
#define ODINFB_FORMAT_FLAG_ENABLE_OVERLAY       0x0800
#define ODINFB_FORMAT_FLAG_DISABLE_OVERLAY      0x1000

#define ODINFB_MEMTYPE_SDRAM            0
#define ODINFB_MEMTYPE_SRAM             1
#define ODINFB_MEMTYPE_MAX              1

#define ODINFB_MEM_IDX_ENABLED  0x80
#define ODINFB_MEM_IDX_MASK     0x7f

enum odinfb_color_format {
	ODINFB_COLOR_NONSTD = 0,
	ODINFB_COLOR_YUV422S = 1,
	ODINFB_COLOR_YUV422P_2,
	ODINFB_COLOR_YUV224P_2,
	ODINFB_COLOR_YUV420P_2,
	ODINFB_COLOR_YUV444P_2,
	ODINFB_COLOR_YUV422P_3,
	ODINFB_COLOR_YUV224P_3,
	ODINFB_COLOR_YUV420P_3,
	ODINFB_COLOR_YUV444P_3,

	ODINFB_COLOR_RGB565,
	ODINFB_COLOR_ARGB1555,
	ODINFB_COLOR_XRGB1555,
	ODINFB_COLOR_ARGB4444,
	ODINFB_COLOR_XRGB4444,
	ODINFB_COLOR_ARGB8888,
	ODINFB_COLOR_XRGB8888,
	ODINFB_COLOR_RGBA8888,
};

struct odinfb_update_window {
	__u32 x, y;
	__u32 width, height;
	__u32 format;
	__u32 out_x, out_y;
	__u32 out_width, out_height;
	__u32 reserved[8];
};

struct odinfb_update_window_old {
	__u32 x, y;
	__u32 width, height;
	__u32 format;
};

enum odinfb_plane {
	ODINFB_PLANE_GFX = 0,
	ODINFB_PLANE_VID1,
	ODINFB_PLANE_VID2,
};

enum odinfb_channel_out {
	ODINFB_CHANNEL_OUT_LCD = 0,
	ODINFB_CHANNEL_OUT_DIGIT,
};

struct odinfb_plane_info {
	__u8  enabled;
	__u8  channel_out;
	__u8  mirror;
	__u8  mem_idx;

	__u32 crop_x;
	__u32 crop_y;
	__u32 crop_w;
	__u32 crop_h;
	__u32 win_x;
	__u32 win_y;
	__u32 win_w;
	__u32 win_h;

	__u32 reserved2[12];
};

struct odinfb_mem_info {
	__u32 size;
	__u8  type;
	__u8  reserved[3];
};

struct odinfb_caps {
	__u32 ctrl;
	__u32 plane_color;
	__u32 wnd_color;
};

enum odinfb_color_key_type {
	ODINFB_COLOR_KEY_DISABLED = 0,
	ODINFB_COLOR_KEY_GFX_DST,
	ODINFB_COLOR_KEY_VID_SRC,
};

struct odinfb_color_key {
	__u8  channel_out;
	__u32 background;
	__u32 trans_key;
	__u8  key_type;
};

enum odinfb_update_mode {
	ODINFB_UPDATE_DISABLED = 0,
	ODINFB_AUTO_UPDATE,
	ODINFB_MANUAL_UPDATE
};

struct odinfb_memory_read {
	__u16 x;
	__u16 y;
	__u16 w;
	__u16 h;
	size_t buffer_size;
	void __user *buffer;
};

struct odinfb_ovl_colormode {
	__u8 overlay_idx;
	__u8 mode_idx;
	__u32 bits_per_pixel;
	__u32 nonstd;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
};

struct odinfb_vram_info {
	__u32 total;
	__u32 free;
	__u32 largest_free_block;
	__u32 reserved[5];
};

struct odinfb_tearsync_info {
	__u8 enabled;
	__u8 reserved1[3];
	__u16 line;
	__u16 reserved2;
};

struct odinfb_display_info {
	__u16 xres;
	__u16 yres;
	__u32 width;    /* phys width of the display in micrometers */
	__u32 height;   /* phys height of the display in micrometers */
	__u32 reserved[5];
};

struct odinfb_addr_check {
	struct ion_handle_data handle_data;
	__u8 iova_using;
};
#if 0
#define ODINFB_MAX_FENCE_FD	10
#define ODINFB_BUF_SYNC_FLAG_WAIT	1
struct odin_buf_sync {
	uint32_t flags;
	uint32_t acq_fen_fd_cnt;
	int *acq_fen_fd;
	int *rel_fen_fd;
};
#define ODINFB_DISPLAY_COMMIT_OVERLAY	1
struct odinfb_buf_fence {
	uint32_t flags;
	uint32_t acq_fen_fd_cnt;
	int acq_fen_fd[ODINFB_MAX_FENCE_FD];
	int rel_fen_fd[ODINFB_MAX_FENCE_FD];
};

struct odinfb_display_commit {
	uint32_t flags;
	uint32_t wait_for_finish;
	struct fb_var_screeninfo var;
	struct odinfb_buf_fence buf_fence;
};
#endif

#ifdef __KERNEL__

#define ODINFB_PLANE_NUM                3

struct odinfb_mem_region {
	int             id;
	u32             paddr;
	void __iomem    *vaddr;
	unsigned long   size;
	u8              type;

	enum odinfb_color_format format;	// Input format

	unsigned	format_used:1;	/* Must be set when format is set.
		 * Needed b/c of the badly chosen 0
		 * base for ODINFB_COLOR_* values
		 */

	u32       	alloc;        /* allocated by the driver */
	bool		 map;		 /* kernel mapped by the driver */
	atomic_t	 map_count;
	struct rw_semaphore lock;
	atomic_t	 lock_count;
};

struct odin_lcd_config {
	char panel_name[16];
	char ctrl_name[16];
	s16  nreset_gpio;
	u8   data_lines;
};


struct odinfb_mem_desc {
	int                             region_cnt;
	struct odinfb_mem_region        region[ODINFB_PLANE_NUM];
};

struct odinfb_platform_data {
	struct odin_lcd_config          lcd;
	struct odinfb_mem_desc          mem_desc;
	void                            *ctrl_platform_data;
};

/* in arch/arm/plat-odin/fb.c */
extern void odinfb_set_platform_data(struct odinfb_platform_data *data);
extern void odinfb_set_ctrl_platform_data(void *pdata);
//extern void odinfb_reserve_sdram_memblock(void);

/* helper methods that may be used by other modules */
enum odin_color_mode;
struct odin_video_timings;
int odinfb_mode_to_dss_mode(struct fb_var_screeninfo *var,
                        enum odin_color_mode *mode);

void odinfb_fb2dss_timings(struct fb_videomode *fb_timings,
                        struct odin_video_timings *dss_timings);

void odinfb_dss2fb_timings(struct odin_video_timings *dss_timings,
                        struct fb_videomode *fb_timings);

#endif

#endif /* __ODINFB_H */

