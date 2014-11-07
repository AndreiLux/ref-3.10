#ifndef _ODIN_OVERLAY_H
#define _ODIN_OVERLAY_H

#ifdef __KERNEL__
#include <video/odindss.h>
#include <linux/ion.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/ion.h>

#define MAX_OVERLAYS      7
#define MAX_WRITEBACKS    3
#define MAX_MANAGERS      3
#define MAX_DISPLAYS      3
#define MAX_SCALERS	      2

#define PRIMARY_DEFAULT_OVERLAY ODIN_DSS_GRA0
#define EXTERNAL_DEFAULT_OVERLAY ODIN_DSS_mSCALER

/* This is Odin WriteBack Index*/
enum odin_wb_idx {
	ODIN_WB_VID0			= 0, //Frame Buffer Dedicated
	ODIN_WB_VID1			= 1,
	ODIN_WB_mSCALER			= 2,
};

struct odin_video_timings {
	/* RGB Interface*/
	/* Unit: pixels */
	__u16 x_res;
	/* Unit: pixels */
	__u16 y_res;
	/* Unit: mm (LCD height)*/
	__u32 height;
	/* Unit: mm (LCD width)*/
	__u32 width;
	/* Unit: KHz */
	__u32 pixel_clock;
	/* Unit: bit */
	__u16 data_width;
	/* Unit: pixel clocks */
	__u16 hsw;	/* Horizontal synchronization pulse width */
	/* Unit: pixel clocks */
	__u16 hfp;	/* Horizontal front porch */
	/* Unit: pixel clocks */
	__u16 hbp;	/* Horizontal back porch */
	/* Unit: line clocks */
	__u16 vsw;	/* Vertical synchronization pulse width */
	/* Unit: line clocks */
	__u16 vfp;	/* Vertical front porch */
	/* Unit: line clocks */
	__u16 vbp;	/* Vertical back porch */

	__u16 rgb_swap;

	/* HDMI Interface */
	__u16 mPixelRepetitionInput;

	__u16 mPixelClock;

	__u8 mInterlaced;

	__u16 mHActive;

	__u16 mHBlanking;

	__u16 mHBorder;

	__u16 mHImageSize;

	__u16 mHSyncOffset;

	__u16 mHSyncPulseWidth;

	__u8 mHSyncPolarity;

	__u16 mVActive;

	__u16 mVBlanking;

	__u16 mVBorder;

	__u16 mVImageSize;

	__u16 mVSyncOffset;

	__u16 mVSyncPulseWidth;

	__u8 mVSyncPolarity;

	/*
	 * LCD specific representation
	 *
	 * sync polatiry
	 *    1: active high
	 *    0: active low
	 *
	 * pclk polarity
	 *    0: latch data at the falling edge
	 *    1: latch data at the rising edge
	 */

	/* Comon Interface */
	__u16 vsync_pol;	/* vsync_pol */
	__u16 hsync_pol;	/* hsync_pol */
	__u16 blank_pol;	/* de_pol */
	__u16 pclk_pol;	/* pclk_pol */
};

enum odin_dss_display_state {
	ODIN_DSS_DISPLAY_DISABLED = 0,
	ODIN_DSS_DISPLAY_ACTIVE,
	ODIN_DSS_DISPLAY_SUSPENDED,
	ODIN_DSS_DISPLAY_DISCONNECTED,
};

enum odin_channel {
	ODIN_DSS_CHANNEL_LCD0	= 0,
	ODIN_DSS_CHANNEL_LCD1	= 1,
	ODIN_DSS_CHANNEL_HDMI	= 2,
	ODIN_DSS_CHANNEL_MAX
};

/* This is Odin Plane for DMA */
enum odin_dma_plane {
	ODIN_DSS_VID0			= 0, //Frame Buffer Dedicated
	ODIN_DSS_VID1			= 1,
	ODIN_DSS_VID2_FMT		= 2,
	ODIN_DSS_GRA0			= 3,
	ODIN_DSS_GRA1			= 4,
	ODIN_DSS_GRA2			= 5,
	ODIN_DSS_mSCALER		= 6,
	ODIN_DSS_NONE			= 7,
};

enum odin_color_mode {
	ODIN_DSS_COLOR_RGB565	 = 0x0,  /* RGB565 */
	ODIN_DSS_COLOR_RGB555	 = 0x1,  /* RGB555 - global alpha setting */
	ODIN_DSS_COLOR_RGB444	 = 0x2,  /* RGB444 - global alpha setting */
	ODIN_DSS_COLOR_RGB888	 = 0x3,  /* RGB888 - global alpha setting */
	ODIN_DSS_COLOR_RESERVED1 = 0x4,  /* RESERVED */
	ODIN_DSS_COLOR_YUV422S	 = 0x5,  /* YUV422 STREAM*/
	ODIN_DSS_COLOR_RESERVED2 = 0x6,  /* RESERVED */
	ODIN_DSS_COLOR_RESERVED3 = 0x7,  /* RESERVED */
	ODIN_DSS_COLOR_YUV422P_2 = 0x8,  /* YUV422 2PLANE*/
	ODIN_DSS_COLOR_YUV224P_2 = 0x9,  /* YUV224 2PLANE*/
	ODIN_DSS_COLOR_YUV420P_2 = 0xa,  /* YUV420 2PLANE*/
	ODIN_DSS_COLOR_YUV444P_2 = 0xb,  /* YUV444 2PLANE*/
	ODIN_DSS_COLOR_YUV422P_3 = 0xc,  /* YUV422 3PLANE*/
	ODIN_DSS_COLOR_YUV224P_3 = 0xd,  /* YUV224 3PLANE*/
	ODIN_DSS_COLOR_YUV420P_3 = 0xe,  /* YUV420 3PLANE*/
	ODIN_DSS_COLOR_YUV444P_3 = 0xf,  /* YUV444 3PLANE*/

	// Byte - swap
	ODIN_DSS_COLOR_BYTE_ABCD = 0x00,
	ODIN_DSS_COLOR_BYTE_BADC = 0x10,
	ODIN_DSS_COLOR_BYTE_CDAB = 0x20,
	ODIN_DSS_COLOR_BYTE_DCBA = 0x30,

	// Word - swap
	ODIN_DSS_COLOR_WORD_AB = 0x00,
	ODIN_DSS_COLOR_WORD_BA = 0x40,

	// Alpha - swap
	ODIN_DSS_COLOR_ALPHA_ARGB  = 0x00,
	ODIN_DSS_COLOR_ALPHA_RGBA  = 0x80,

	// RGB - swap
	ODIN_DSS_COLOR_RGB_RGB  = 0x000,
	ODIN_DSS_COLOR_RGB_RBG	= 0x100,
	ODIN_DSS_COLOR_RGB_BGR	= 0x200,
	ODIN_DSS_COLOR_RGB_BRG	= 0x300,
	ODIN_DSS_COLOR_RGB_GRB	= 0x400,
	ODIN_DSS_COLOR_RGB_GBR	= 0x500,

	// YUV - swap
	ODIN_DSS_COLOR_YUV_UV	  = 0x000,
	ODIN_DSS_COLOR_YUV_VU	  = 0x800,

	// global_alpha
	ODIN_DSS_COLOR_PIXEL_ALPHA= 0x1000,
	ODIN_DSS_COLOR_PLANE_UV_SWAP= 0x2000,
    ODIN_DSS_COLOR_NOT_SUPPORTED= 0x4000,

	ODIN_DSS_COLOR_RGB565_RBG = ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB_RBG,
	ODIN_DSS_COLOR_RGB565_BGR = ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB_BGR,
	ODIN_DSS_COLOR_RGB565_BRG = ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB_BRG,
	ODIN_DSS_COLOR_RGB565_GRB = ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB_GRB,
	ODIN_DSS_COLOR_RGB565_GBR = ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB_GBR,

	ODIN_DSS_COLOR_RGB5551 = ODIN_DSS_COLOR_RGB555 | ODIN_DSS_COLOR_PIXEL_ALPHA, /*HAL_PIXEL_FORMAT_RGBA_5551 */
	ODIN_DSS_COLOR_RGB4441 = ODIN_DSS_COLOR_RGB444 | ODIN_DSS_COLOR_PIXEL_ALPHA, /*HAL_PIXEL_FORMAT_RGBA_4444 */

	ODIN_DSS_COLOR_RGB888_BGRA = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_PIXEL_ALPHA,  /*HAL_PIXEL_FORMAT_BGRA_8888*/
	ODIN_DSS_COLOR_RGB888_RGBA = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_RGB_BGR | ODIN_DSS_COLOR_PIXEL_ALPHA,  /*HAL_PIXEL_FORMAT_RGBA_8888*/

	ODIN_DSS_COLOR_RGB888_RGBX = ODIN_DSS_COLOR_RGB888 | ODIN_DSS_COLOR_RGB_BGR,  /*HAL_PIXEL_FORMAT_RGBX_8888*/
	ODIN_DSS_COLOR_YUV12 = ODIN_DSS_COLOR_YUV420P_3 | ODIN_DSS_COLOR_PLANE_UV_SWAP,  /*YUV12*/
};

enum dsscomp_setup_mode {
	DSSCOMP_SETUP_MODE_DISPLAY 			 = (1 << 0),	/* calls display update */
	DSSCOMP_SETUP_MODE_DISPLAY_CAPTURE   = (1 << 1),	/* display and capture in same sync*/
	DSSCOMP_SETUP_MODE_CAPTURE 			 = (1 << 2),	/* Indpendent display capture*/
	DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM	 = (1 << 3),	/* display and mem2mem in same sync*/
	DSSCOMP_SETUP_MODE_MEM2MEM			 = (1 << 4),	/* mem2mem*/
};
#endif
/*
 * Stereoscopic Panel types
 * row, column, overunder, sidebyside options
 * are with respect to native scan order
 */
enum s3d_disp_type {
	S3D_DISP_NONE = 0,
	S3D_DISP_FRAME_SEQ,
	S3D_DISP_ROW_IL,
	S3D_DISP_COL_IL,
	S3D_DISP_PIX_IL,
	S3D_DISP_CHECKB,
	S3D_DISP_OVERUNDER,
	S3D_DISP_SIDEBYSIDE,
};

/* Subsampling direction is based on native panel scan order.*/
enum s3d_disp_sub_sampling {
	S3D_DISP_SUB_SAMPLE_NONE = 0,
	S3D_DISP_SUB_SAMPLE_V,
	S3D_DISP_SUB_SAMPLE_H,
};

/*
 * Indicates if display expects left view first followed by right or viceversa
 * For row interlaved displays, defines first row view
 * For column interleaved displays, defines first column view
 * For checkerboard, defines first pixel view
 * For overunder, defines top view
 * For sidebyside, defines west view
 */
enum s3d_disp_order {
	S3D_DISP_ORDER_L = 0,
	S3D_DISP_ORDER_R = 1,
};

/*
 * Indicates current view
 * Used mainly for displays that need to trigger a sync signal
 */
enum s3d_disp_view {
	S3D_DISP_VIEW_L = 0,
	S3D_DISP_VIEW_R,
};

struct s3d_disp_info {
	enum s3d_disp_type type;
	enum s3d_disp_sub_sampling sub_samp;
	enum s3d_disp_order order;
	/*
	 * Gap between left and right views
	 * For over/under units are lines
	 * For sidebyside units are pixels
	 * For other types ignored
	 */
	unsigned int gap;
};

/* copy of fb_videomode */
struct dsscomp_videomode {
	const char *name;	/* optional */
	__u32 refresh;		/* optional */
	__u32 xres;
	__u32 yres;
	__u32 pixclock;
	__u32 left_margin;
	__u32 right_margin;
	__u32 upper_margin;
	__u32 lower_margin;
	__u32 hsync_len;
	__u32 vsync_len;
	__u32 sync;
	__u32 vmode;
	__u32 flag;
};

/* standard rectangle */
struct dss_rect_t {
	__s32 x;	/* left */
	__s32 y;	/* top */
	__u32 w;	/* width */
	__u32 h;	/* height */
} __attribute__ ((aligned(4)));


struct dss_ovl_cfg {
	__u16 width;	/* buffer width */
	__u16 height;	/* buffer height */
	__u32 stride;	/* buffer stride */

	enum odin_color_mode color_mode;
	__u8 blending;	/* bool */
	__u8 pre_mult_alpha;	/* bool */
	__u8 global_alpha;	/* 0..255 */
	__u8 rotation;		/* 0..3 (*90 degrees clockwise) */
	__u8 mirror;	/* left-to-right: mirroring is applied after rotation */

	struct dss_rect_t win;		/* output window - on display */
	struct dss_rect_t crop;	/* crop window - in source buffer */

	__u8 ix;		/* ovl index same as sysfs/overlay# */
	__u8 zorder;	/* 0..3 */
	__u8 enabled;	/* bool */
	__u8 invisible;	/* bool */
	__u8 zonly;	/* only set zorder and enabled bit */
	__u8 mgr_ix;	/* mgr index */

/* mXD */
	__u8 mxd_use;

/* formatter */
	__u8 formatter_use;

/* secure */
	__u8 secure_use;

} __attribute__ ((aligned(4)));

struct dss_wb_cfg {
	__u16 width;	/* buffer width */
	__u16 height;	/* buffer height */

	enum odin_color_mode color_mode;

	struct dss_rect_t win;		/* output window - on display */

	__u8 ix;		/* wb index same as sysfs/overlay# */
	__u8 enabled;	/* bool */
	__u8 mgr_ix;	/* mgr index */
} __attribute__ ((aligned(4)));

enum dss_buffer_addr_type {
	ODIN_DSS_BUFADDR_DIRECT,	/* using direct addresses */
	ODIN_DSS_BUFADDR_FB,		/* using Framebuffer addresses */
	ODIN_DSS_BUFADDR_ION		/* using ion fd */
};

struct dss_ovl_info {
	struct dss_ovl_cfg cfg;
	enum dss_buffer_addr_type addr_type;
	void *addr;	/* buffer address */
	int fd;
	struct ion_client *client;
};

struct dss_wb_info {
	struct dss_wb_cfg cfg;
	enum dss_buffer_addr_type addr_type;
	void *addr;	/* buffer address */
	int fd;
	struct ion_client *client;
};

struct dss_mgr_info {
	__u32 ix;		/* display index same as sysfs/display# */

	__u32 default_color;

	__u32 trans_key;

	__u8 trans_enabled;	/* bool */

	__u8 alpha_blending;	/* alpha_blending */
	__u8 swap_rb;		/* bool - swap red and blue */
} __attribute__ ((aligned(4)));

enum dsscomp_wait_phase {
	DSSCOMP_WAIT_PROGRAMMED = 1,
	DSSCOMP_WAIT_DISPLAYED,
	DSSCOMP_WAIT_RELEASED,
};

struct dsscomp_setup_mgr_data {
	__u32 sync_id;		/* synchronization ID - for debugging */

	struct dss_rect_t win; /* update region, set w/h to 0 for fullscreen */
	enum dsscomp_setup_mode mode;
	__u16 num_ovls;		/* # of overlays used in the composition */
	__u16 num_wbs;		/* # of writebacks used in the composition */
	__u16 get_sync_obj;	/* ioctl should return a sync object */

	struct dss_mgr_info mgr;
	struct dss_ovl_info ovls[0]; /* up to 5 overlays to set up */
};

struct dsscomp_check_ovl_data {
	enum dsscomp_setup_mode mode;
	struct dss_mgr_info mgr;
	struct dss_ovl_info ovl;
};

struct dsscomp_setup_du_data {
	__u32 sync_id;		/* synchronization ID - for debugging */

	enum dsscomp_setup_mode mode;
	__u16 num_ovls;		/* # of overlays used in the composition */
	__u16 num_wbs;		/* # of writeback */
	__u16 num_mgrs;		/* # of managers used in the composition */
	__u16 get_sync_obj;	/* ioctl should return a sync object */

	struct dss_mgr_info mgrs[MAX_MANAGERS];
	struct dss_ovl_info ovls[MAX_OVERLAYS];		/* up to 7 overlays to set up*/
	struct dss_wb_info wbs[MAX_WRITEBACKS];		/* up to 3 writebacks to set up*/
};

struct dsscomp_display_info {
	__u32 ix;			/* display index (sysfs/display#) */
	__u32 overlays_available;	/* bitmask of available overlays */
	__u32 overlays_owned;		/* bitmask of owned overlays */
	enum odin_channel channel;
	enum odin_dss_display_state state;
	__u8 enabled;			/* bool: resume-state if suspended */
	struct odin_video_timings timings;
	struct s3d_disp_info s3d_info;	/* any S3D specific information */
	struct dss_mgr_info mgr;	/* manager information */
	__u16 width_in_mm;		/* screen dimensions */
	__u16 height_in_mm;

	__u32 modedb_len;		/* number of video timings */
	struct dsscomp_videomode modedb[];	/* display supported timings */
};

struct dsscomp_setup_display_data {
	__u32 ix;			/* display index (sysfs/display#) */
	struct dsscomp_videomode mode;	/* video timings */
};

struct dsscomp_wait_for_vsync_data {
	__u32 mgr_ix;			/* mgr index */
	__u16 wait_vsync_try;	/* trt to wait_vsync */
	__u32 fps_value;	/* fps value if you want to control fps manually */
				/* fps_value default 60 */
};

struct dsscomp_wait_data {
	__u32 timeout_us;
	enum dsscomp_wait_phase phase;
};

struct dsscomp_vsync_ctrl_data {
    __u32 ix;
    __u8 enable;
};

struct dsscomp_get_pipe_status {
	__u32 mgr_pipe[MAX_MANAGERS];
};

#if 1
#define DSSCOMP_MAX_FENCE_FD	10
#define DSSCOMP_BUF_SYNC_FLAG_WAIT	1
struct odin_buf_sync {
	uint32_t flags;
	uint32_t acq_fen_fd_cnt;
	int *acq_fen_fd;
	int *rel_fen_fd;

	int dpy;
};
#define DSSCOMP_DISPLAY_COMMIT_OVERLAY	1
struct dsscomp_buf_fence {
	uint32_t flags;
	uint32_t acq_fen_fd_cnt;
	int acq_fen_fd[DSSCOMP_MAX_FENCE_FD];
	int rel_fen_fd[DSSCOMP_MAX_FENCE_FD];
};

struct dsscomp_display_commit {
	uint32_t flags;
	uint32_t wait_for_finish;
	struct dsscomp_buf_fence buf_fence;
};
#endif

/* IOCTLS */
#define DSSCOMP_SETUP_MGR	_IOW('O', 128, struct dsscomp_setup_mgr_data)
#define DSSCOMP_CHECK_OVL	_IOWR('O', 129, struct dsscomp_check_ovl_data)
#define DSSCOMP_QUERY_DISPLAY	_IOWR('O', 131, struct dsscomp_display_info)
#define DSSCOMP_WAIT		_IOW('O', 132, struct dsscomp_wait_data)

#define DSSCOMP_SETUP_DU	_IOW('O', 133, struct dsscomp_setup_du_data)
#define DSSCOMP_SETUP_DISPLAY	_IOW('O', 134, struct dsscomp_setup_display_data)
#define DSSCOMP_WAIT_VSYNC	_IOW('O', 135, struct dsscomp_wait_for_vsync_data)
#define DSSCOMP_MGR_BLANK	_IOW('O', 136, int )
#define DSSCOMP_WAIT_WB_DONE	_IOW('O', 137, int)
#define DSSCOMP_BUFFER_SYNC	_IOW('O', 138, struct odin_buf_sync)
#define DSSCOMP_GET_PIPE_STATUS	_IOW('O', 139, struct dsscomp_get_pipe_status)
#define DSSCOMP_VSYNC_CTRL	_IOW('O', 140, struct dsscomp_vsync_ctrl_data)
#define DSSCOMP_RELEASE_SYNC	_IOW('O', 141, struct odin_buf_sync)
#define DSSCOMP_REFRESH_SYNC	_IOW('O', 142, struct odin_buf_sync)
#define DSSCOMP_MEM2MEM_ENABLE	_IOW('O', 143, __u8)

#endif
