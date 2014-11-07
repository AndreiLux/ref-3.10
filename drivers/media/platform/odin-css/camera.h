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

#ifndef __ASM_ARCH_CAMERA_H
#define __ASM_ARCH_CAMERA_H


/* Board & Sensor type definition end*/

typedef enum {
	SENSOR_REAR = 0,
	SENSOR_FRONT,
	SENSOR_DUMMY,
	SENSOR_S3D,
} sensor_index;

typedef enum {
	SENSOR_PREVIEW = 0,
	SENSOR_CAPTURE,
	SENSOR_VIDEO,
	SENSOR_ZERO_SHUTTER_LAG_PREVIEW,
	SENSOR_OIS_TEST,
	SENSOR_OIS_OFF,/*                                                          */
} sensor_mode;

typedef enum {
	SET_VERSION = 0,
	OIS_ON,
	OIS_MODE,
	OIS_MOVE_LENS
} ois_cmd_mode;

struct css_sensor_info {
	unsigned int index;
	unsigned int width;
	unsigned int height;
	sensor_mode mode;
};

struct css_ois_lens_info {
	int x;
	int y;
};

struct css_ois_control_args {
	ois_cmd_mode cmd;
	unsigned int version;
	unsigned int mode;
	struct css_ois_lens_info info;
};

struct css_ois_status_data{
	char ois_provider[32];
	short gyro[2];
	short target[2];
	short hall[2];
	unsigned char is_stable;
};

struct css_sensor_table {
	unsigned short addr;
	unsigned char data;
};

struct css_i2c_data {
	unsigned int size;
	/* MAX 64command *3BYte - otherwise IO ctrl is failed */
	struct css_sensor_table table[192];
};

enum {
	TORCH_OFF,
	TORCH_ON,
	TORCH_DEFAULT,
};

enum {
	STROBE_DEFAULT,
	STROBE_ON,
};

typedef enum {
	CSS_TORCH_OFF,
	CSS_TORCH_ON,
	CSS_STROBE_ON,
	CSS_LED_HIGH,
	CSS_LED_LOW,
	CSS_LED_OFF,
	CSS_PRE_FLASH_ON,
	CSS_PRE_FLASH_OFF
} flash_cmd_mode;

struct css_flash_control_args {
	flash_cmd_mode mode;
	unsigned int flash_current[2];
};

struct css_face_detect_info {
	unsigned int size;
	unsigned int center_x;
	unsigned int center_y;
	unsigned int confidence;
	unsigned int angle;
	unsigned int pose;
	/*
	unsigned int count;
	unsigned int offset_x;
	unsigned int offset_y;
	unsigned int position_x;
	unsigned int position_y;
	*/
};

struct coordinate {
	unsigned int x;
	unsigned int y;
};

struct css_fpd_eye_info {
	struct coordinate inner;
	struct coordinate outer;
	struct coordinate center;
};

struct css_fpd_boundary {
	struct coordinate left;
	struct coordinate right;
};

struct css_fpd_mouth_boundary {
	struct coordinate left;
	struct coordinate right;
	struct coordinate upper;
	struct coordinate center;
};

struct css_facial_parts_info {
	unsigned int mouth_conf;
	unsigned int execution_status;
	unsigned int left_eye_conf;
	unsigned int right_eye_conf;
	unsigned int yaw;
	unsigned int pitch;
	unsigned int roll;
	struct css_fpd_eye_info left_eye;
	struct css_fpd_eye_info right_eye;
	struct css_fpd_boundary nostril;
	struct css_fpd_mouth_boundary mouth;
};

struct css_fd_config {
	unsigned int resolution;
	unsigned int threshold;
	unsigned int dir;
	int compress_level;
};

/*
 * move config - for test
#define USE_FPD_HW_IP
 */
#define FD_PAIR_GET_DATA

#ifdef USE_FPD_HW_IP
#define FACE_BUF_MAX 3
#else
#define FACE_BUF_MAX 4
#endif

typedef enum {
	FD_SET_CONFIG = 0,
	FD_SET_CROP_SIZE,
} fd_cmd;

struct css_rect {
	int start_x;
	int start_y;
	int width;
	int height;
};

struct css_fd_contrl_args {
	fd_cmd cmd;
	unsigned int resolution;
	unsigned int threshold;
	unsigned int dir;
	unsigned int src_path;
	unsigned int fd_buf_addr[FACE_BUF_MAX];
	struct css_rect crop;
};

struct css_fd_result_data {
	struct css_face_detect_info fd_face_info[35];
#ifdef USE_FPD_HW_IP
	struct css_facial_parts_info fpd_face_info[35];
#else
#ifndef FD_PAIR_GET_DATA
	int get_frame;
	int fpd_enable;
#endif
	int state;
	int	result_buf_index;
#endif
	int face_num;
	struct css_rect crop;
	unsigned int direct;
};

struct css_fd_pair_result_data {
	struct css_fd_result_data fd_data[2];
	int get_frame;
	int fpd_enable;
};

struct css_isp_args {
	s32 left;
	s32 top;
	s32 width;
	s32 height;
	struct css_flash_control_args flash;
};

struct css_isp_control_args {
	unsigned int type;
	struct css_isp_args c;
};

struct css_mdis_control_args {
	u32 cmd;
	u32 vsize;
	u32 pass_frame;
	u32 offset_x;
	u32 offset_y;
	u32 frame_num;
};

struct css_afifo_ctrl_args {
	u32 index;
	u32 format;
};

struct css_afifo_pre_ctrl_args {
	u32 index;
	u32 width;
	u32 height;
	u32 format;
    u32 read_path_en;
    u32 read_path_sel;
};

struct css_afifo_post_ctrl_args {
	u32 index;
	u32 width;
	u32 height;
};

struct css_bayer_ctrl_args {
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

struct css_afifo_param_args {
	u32 							cmd;
	struct css_afifo_ctrl_args		ctrl;
	struct css_afifo_pre_ctrl_args	pre;
	struct css_afifo_post_ctrl_args post;
	struct css_bayer_ctrl_args		bayer;
};

struct css_afifo__args {
	u32 index;
	u32 format;
};

typedef enum {
	RESCHG_PREPARE,
	RESCHG_DONE,
} reschg_cmd;

struct css_sensor_resolution_change_ind {
	reschg_cmd cmd;
	u32 camid;
	u32 width;
	u32 height;
};

struct css_postview_info_args {
	u32 width;
	u32 height;
	u32 pix_fmt;
	size_t size;
};

struct css_frame_size_info_args {
	u32 index;
	u32 width;
	u32 height;
};

typedef enum {
	AFIFO_CTRL,
	AFIFO_PRE,
	AFIFO_POST,
	AFIFO_BAYER,
} afifo_cmd_mode;

typedef enum {
	/* Bayer Store operation command		*/
	ZSL_START,
	ZSL_PAUSE,
	ZSL_RESUME,
	ZSL_STOP,

	/* ZSL Store setting					*/
	ZSL_STORE_MODE,
	ZSL_STORE_FRAME_INTERVAL,
	ZSL_STORE_BIT_FORMAT,
	ZSL_STORE_WAIT_BUFFER_FULL,
	ZSL_STORE_STATISTIC,
	ZSL_STORE_GET_QUEUED_BUF_COUNT,

	/* ZSL Load setting 					*/
	ZSL_LOAD_MODE,
	ZSL_LOAD_BUFFER_OFFSET,
	ZSL_LOAD_ORDER,
	ZSL_LOAD_FRAME_NUMBER,

	/*ZSL bayer dump related for tunning	*/
	ZSL_OPEN_BAYER_FD,
	ZSL_RELEASE_BAYER_FD,
} zsl_cmd_mode;

typedef enum {
	ZSL_STORE_PATH0    = 0,
	ZSL_STORE_PATH1    = 1,
	ZSL_STORE_PATH_ALL = 2,
	ZSL_READ_PATH0	   = 0,
	ZSL_READ_PATH1	   = 1,
	ZSL_READ_PATH_ALL  = 2
} css_zsl_path;

typedef enum {
	ZSL_STORE_STREAM_MODE = 0,
	ZSL_STORE_FILL_BUFFER_MODE,
} zsl_store_mode;

typedef enum {
	RAWDATA_BIT_MODE_12BIT	= 0x0,
	RAWDATA_BIT_MODE_10BIT	= 0x1,
	RAWDATA_BIT_MODE_8BIT	= 0x2
} zsl_store_raw_bitfmt;

struct zsl_store_statistic {
	unsigned int enable;
	unsigned int hsize;
	unsigned int vline;
};

typedef enum {
	ZSL_LOAD_DECENDING,
	ZSL_LOAD_ASCENDING,
} zsl_load_order;

typedef enum {
	ZSL_LOAD_DISCONTINUOUS_FRAME_MODE = 0,
	ZSL_LOAD_CONTINUOUS_FRAME_MODE,
} zsl_load_mode;

typedef enum {
	ZSL_LOAD_ONESHOT_TYPE = 0,
	ZSL_LOAD_STREAM_TYPE,
} zsl_load_type;

struct css_zsl_buffer_args {
	unsigned int count;
	unsigned int width;
	unsigned int height;
};

struct css_zsl_control_args {
	/* Bayer Store Operation Command Modes */
	zsl_cmd_mode	cmode;

	/* Bayer Store setting */
	zsl_store_mode	storemode;
	zsl_store_raw_bitfmt bitformat;
	unsigned int	frame_interval;
	struct zsl_store_statistic statistic;

	/* Bayer Load setting */
	zsl_load_mode	loadmode;
	unsigned int	buf_offset;
	zsl_load_order	order;
	unsigned int	frame_number;
	unsigned int 	queued_count;

	/* Bayer Dump setting */
	int				bayer_fd;
	unsigned int	bayer_size;
	unsigned int	bayer_align_size;

	unsigned int	private_data;
};

typedef enum {
	CMD_GET_VERSION,
	CMD_SET_INPUT,
	CMD_SET_OUTPUT,
	CMD_GET_RSVD_BUFFER,
	CMD_START,
	CMD_STOP,
	CMD_OUT_S3D_MODE,
	CMD_OUT_EXT_MODE,
	CMD_MANUAL_CONVERGENCE,
	CMD_AUTO_CONVERGENCE,
	CMD_AUTO_CONVERGENCE_SET_AREA,
	CMD_AUTO_CONVERGENCE_SET_EDGE_THRESHOLD,
	CMD_AUTO_CONVERGENCE_SET_DP_REDUN,
	CMD_CONVERGENCE_DP_STEP,
	CMD_SET_ROTATE_DEGREE,
	CMD_SET_TILT_ANGLE,
	CMD_AUTO_LUMINANCE_CONTROL,
	CMD_LUMINANCE_CONTROL,
	CMD_SATURATION_CONTROL,
	CMD_BRIGHTNESS_CONTROL,
	CMD_CONTRAST_CONTROL,
	CMD_HUE_CONTROL,
	CMD_COLOR_TEST_PATTERN_CONTROL,
	CMD_SET_ZOOM,
	CMD_CHANGE_LR,
	CMD_GET_CROP_SIZE_INFO,
	CMD_SET_COLOR_COEFF,
	CMD_INIT_INPUT_MEM,
	CMD_DEINIT_INPUT_MEM,
	CMD_INIT_OUT_MEM,
	CMD_DEINIT_OUT_MEM,
} s3dc_cmd_mode;

typedef enum {
	INPUT_MEM,
	INPUT_SENSOR
} s3dc_input_path;

typedef enum {
	OUTPUT_V4L2_MEM,
	OUTPUT_MEM,
} s3dc_output_path;

typedef enum {
	IN_FMT_YUV422,
	IN_FMT_RGB565,
	IN_FMT_RGB888
} s3dc_input_format;

typedef enum {
	 OUT_FMT_YUV422,
	 OUT_FMT_YUV420,
	 OUT_FMT_NV12,
	 OUT_FMT_NV21,
	 OUT_FMT_RGB565,
	 OUT_FMT_RGB888
} s3dc_output_format;

typedef enum {
	MODE_2D 					= 0,
	MODE_S3D_MANUAL 			= 1,
} s3dc_output_ext_mode;

typedef enum {
	MODE_S3D_PIXEL				= 2,
	MODE_S3D_SIDE_BY_SIDE		= 3,
	MODE_S3D_LINE				= 4,
	MODE_S3D_RED_BLUE			= 7,
	MODE_S3D_RED_GREEN			= 6,
	MODE_S3D_SUB_PIXEL			= 5,
	MODE_S3D_TOP_BOTTOM			= 8,
	MODE_S3D_SIDE_BY_SIDE_FULL	= 9,
	MODE_S3D_TOP_BOTTOM_FULL	= 10,
} s3dc_output_mode;

enum {
	INPUT_0,
	INPUT_1,
	OUTPUT
};

typedef enum {
	CAMERA_0,
	CAMERA_1
} camera_id;

typedef enum {
	LEFT,
	RIGHT,
	UP,
	DOWN
} s3dc_direction;

typedef enum {
	PLUS,
	MINUS
} s3dc_sign;

struct s3dc_rotate_set {
	int 			rot_en;
	int 			degree_sign;
	unsigned short	rot_sin_l;
	unsigned short	rot_sin_h;
	unsigned short	rot_cos_l;
	unsigned short	rot_cos_h;
	unsigned short	rot_csc_l;
	unsigned short	rot_csc_m;
	unsigned short	rot_csc_h;
	unsigned short	rot_cot_l;
	unsigned short	rot_cot_m;
	unsigned short	rot_cot_h;
};

struct s3dc_luminance_set {
	unsigned int camera;
	unsigned int y_gain_0;
	unsigned int y_gain_1;
	unsigned int l_bright_th;
	unsigned int h_bright_th;
	unsigned int ly_gain_0;
	unsigned int ly_gain_1;
	unsigned int hy_gain_0;
	unsigned int hy_gain_1;
};

struct s3dc_saturation_set {
	unsigned int camera;
	unsigned int c_gain_0;
	unsigned int c_gain_1;
	unsigned int l_color_th;
	unsigned int h_color_th;
	unsigned int lc_gain_0;
	unsigned int lc_gain_1;
	unsigned int hc_gain_0;
	unsigned int hc_gain_1;
};

struct s3dc_crop_size_set {
	int camera;
	int pixel_start;
	int line_start;
	int pixel_count;
	int line_count;
	int camera_width;
	int camera_height;
};

struct s3dc_coeff_set {
	int coeff[9];
	int offset[12];
};

struct s3dc_set_param {
	unsigned int param[100];
};

struct s3dc_buf_param {
	unsigned int	index;
	unsigned int	buf_size;
	unsigned int	dma_addr;
	void			*cpu_addr;
};

struct s3dc_control {
	union {
		struct s3dc_set_param	s_param;
		struct s3dc_buf_param	s_buf;
		unsigned int			buf_size[5];
	} mode;
};

struct css_s3dc_control_args {
	s3dc_cmd_mode		cmode;
	struct s3dc_control	ctrl;
};

#define MMAP_OFFSET_SENSOR		0
#define MMAP_OFFSET_ISP1		0x33000000
#define MMAP_OFFSET_ISP2		0x33080000
#define MMAP_OFFSET_MDIS		0x330F8000

/* Type definition of Vendor for Control Device on Odin */
/* Supporting Odin only */
#define V4L2_CID_CSS					(V4L2_CID_PRIVATE_BASE) /* 0x08000000 */
#define V4L2_CID_CSS_FD_SRC				(V4L2_CID_CSS + 1)
#define V4L2_CID_CSS_FD_CMD				(V4L2_CID_CSS + 2)
#define V4L2_CID_CSS_FD_START			(V4L2_CID_CSS + 3)
#define V4L2_CID_CSS_FD_STOP			(V4L2_CID_CSS + 4)
#define V4L2_CID_CSS_FD_SET_DIR			(V4L2_CID_CSS + 5)
#define V4L2_CID_CSS_FD_SET_THRESHOLD	(V4L2_CID_CSS + 6)
#define V4L2_CID_CSS_FD_SET_RESOLUTION	(V4L2_CID_CSS + 7)
#define V4L2_CID_CSS_FD_SET_BUF			(V4L2_CID_CSS + 8)

#define V4L2_CID_CSS_JPEG						(V4L2_CID_CSS | 0x1000)
#define V4L2_CID_CSS_JPEG_ENC_SIZE				(V4L2_CID_CSS_JPEG + 1)
#define V4L2_CID_CSS_JPEG_ENC_ERROR				(V4L2_CID_CSS_JPEG + 2)
#define V4L2_CID_CSS_JPEG_ENC_SET_READ_DELAY	(V4L2_CID_CSS_JPEG + 3)

/* last CID + 1 */
#define V4L2_CID_CSS_LASTP1 			(V4L2_CID_CSS + 100)

#define V4L2_CID_CSS_SENSOR 				(V4L2_CID_USER_BASE | 0x1004)
#define V4L2_CID_CSS_SENSOR_SET_OIS_VERSION			(V4L2_CID_CSS_SENSOR + 1)
#define V4L2_CID_CSS_SENSOR_OIS_ON					(V4L2_CID_CSS_SENSOR + 2)
#define V4L2_CID_CSS_SENSOR_SET_OIS_MODE			(V4L2_CID_CSS_SENSOR + 3)
#define V4L2_CID_CSS_SENSOR_GET_OIS_STATUS			(V4L2_CID_CSS_SENSOR + 4)
#define V4L2_CID_CSS_SENSOR_MOVE_OIS_LENS			(V4L2_CID_CSS_SENSOR + 5)
#define V4L2_CID_CSS_SENSOR_STROBE					(V4L2_CID_CSS_SENSOR + 6)
#define V4L2_CID_CSS_SENSOR_TORCH					(V4L2_CID_CSS_SENSOR + 7)

#define V4L2_CID_CSS_BASE					(V4L2_CTRL_CLASS_CAMERA | 0x1000)
#define V4L2_CID_CSS_SENSOR_I2C_WRITE				(V4L2_CID_CSS_BASE + 1)
#define V4L2_CID_CSS_SENSOR_I2C_READ				(V4L2_CID_CSS_BASE + 2)

#define V4L2_CID_CSS_FD_GET_FACE_COUNT				(V4L2_CID_CSS_BASE + 14)
#define V4L2_CID_CSS_FD_GET_FACE_CENTER_X			(V4L2_CID_CSS_BASE + 15)
#define V4L2_CID_CSS_FD_GET_FACE_CENTER_Y			(V4L2_CID_CSS_BASE + 16)
#define V4L2_CID_CSS_FD_GET_FACE_SIZE				(V4L2_CID_CSS_BASE + 17)
#define V4L2_CID_CSS_FD_GET_FACE_CONFIDENCE			(V4L2_CID_CSS_BASE + 18)
#define V4L2_CID_CSS_FD_GET_ANGLE					(V4L2_CID_CSS_BASE + 19)
#define V4L2_CID_CSS_FD_GET_POSE					(V4L2_CID_CSS_BASE + 21)
#define V4L2_CID_CSS_FPD_GET_EXEC_STATUS			(V4L2_CID_CSS_BASE + 22)
#define V4L2_CID_CSS_FPD_GET_MOUTH_CONFIDENCE		(V4L2_CID_CSS_BASE + 23)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CONFIDENCE	(V4L2_CID_CSS_BASE + 24)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_CONFIDENCE	(V4L2_CID_CSS_BASE + 25)
#define V4L2_CID_CSS_FPD_GET_ROLL					(V4L2_CID_CSS_BASE + 26)
#define V4L2_CID_CSS_FPD_GET_PITCH					(V4L2_CID_CSS_BASE + 27)
#define V4L2_CID_CSS_FPD_GET_YAW					(V4L2_CID_CSS_BASE + 28)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_INNER_X		(V4L2_CID_CSS_BASE + 29)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_INNER_Y		(V4L2_CID_CSS_BASE + 30)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_OUTER_X		(V4L2_CID_CSS_BASE + 31)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_OUTER_Y		(V4L2_CID_CSS_BASE + 32)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_CENTER_X		(V4L2_CID_CSS_BASE + 33)
#define V4L2_CID_CSS_FPD_GET_LEFT_EYE_CENTER_Y		(V4L2_CID_CSS_BASE + 34)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_INNER_X		(V4L2_CID_CSS_BASE + 35)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_INNER_Y		(V4L2_CID_CSS_BASE + 36)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_OUTER_X		(V4L2_CID_CSS_BASE + 37)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_OUTER_Y		(V4L2_CID_CSS_BASE + 38)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CENTER_X		(V4L2_CID_CSS_BASE + 39)
#define V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CENTER_Y		(V4L2_CID_CSS_BASE + 40)
#define V4L2_CID_CSS_FPD_GET_LEFT_NOSTRIL_X			(V4L2_CID_CSS_BASE + 41)
#define V4L2_CID_CSS_FPD_GET_LEFT_NOSTRIL_Y			(V4L2_CID_CSS_BASE + 42)
#define V4L2_CID_CSS_FPD_GET_RIGHT_NOSTRIL_X		(V4L2_CID_CSS_BASE + 43)
#define V4L2_CID_CSS_FPD_GET_RIGHT_NOSTRIL_Y		(V4L2_CID_CSS_BASE + 44)
#define V4L2_CID_CSS_FPD_GET_LEFT_MOUTH_X			(V4L2_CID_CSS_BASE + 45)
#define V4L2_CID_CSS_FPD_GET_LEFT_MOUTH_Y			(V4L2_CID_CSS_BASE + 46)
#define V4L2_CID_CSS_FPD_GET_RIGHT_MOUTH_X			(V4L2_CID_CSS_BASE + 47)
#define V4L2_CID_CSS_FPD_GET_RIGHT_MOUTH_Y			(V4L2_CID_CSS_BASE + 48)
#define V4L2_CID_CSS_FPD_GET_UPPER_MOUTH_X			(V4L2_CID_CSS_BASE + 49)
#define V4L2_CID_CSS_FPD_GET_UPPER_MOUTH_Y			(V4L2_CID_CSS_BASE + 50)
#define V4L2_CID_CSS_FPD_GET_CENTER_MOUTH_X			(V4L2_CID_CSS_BASE + 51)
#define V4L2_CID_CSS_FPD_GET_CENTER_MOUTH_Y			(V4L2_CID_CSS_BASE + 52)
#define V4L2_CID_CSS_FD_GET_NEXT					(V4L2_CID_CSS_BASE + 53)

enum fd_cmd_mode {
	FD_COMMAND_STOP,
	FD_COMMAND_START,
	FD_COMMAND_MAX
};

#define CSS_IOC_ISP_CONTROL 			_IOWR('v', BASE_VIDIOC_PRIVATE + 0,\
					struct css_isp_control_args)

#define CSS_IOC_MDIS_CONTROL			_IOWR('v', BASE_VIDIOC_PRIVATE + 1,\
					struct css_mdis_control_args)

#define CSS_IOC_ZSL_ALLOC_BUFFER		_IOWR('v', BASE_VIDIOC_PRIVATE + 2,\
					struct css_zsl_buffer_args)

#define CSS_IOC_ZSL_FREE_BUFFER 		_IOWR('v', BASE_VIDIOC_PRIVATE + 3,\
					unsigned int)

#define CSS_IOC_ZSL_CONTROL 			_IOWR('v', BASE_VIDIOC_PRIVATE + 4,\
					struct css_zsl_control_args)

#define CSS_IOC_S3DC_CONTROL			_IOWR('v', BASE_VIDIOC_PRIVATE + 5,\
					struct css_s3dc_control_args)

#define CSS_IOC_FD_CONTROL				_IOWR('v', BASE_VIDIOC_PRIVATE + 6,\
					struct css_fd_contrl_args)

#define CSS_IOC_SET_POSTVIEW_CONFIG		_IOWR('v', BASE_VIDIOC_PRIVATE + 7,\
					struct css_postview_info_args)

#define CSS_IOC_GET_POSTVIEW_INFO		_IOWR('v', BASE_VIDIOC_PRIVATE + 8,\
					struct css_postview_info_args)

#define CSS_IOC_GET_FRAME_SIZE_INFO		_IOWR('v', BASE_VIDIOC_PRIVATE + 9,\
					struct css_frame_size_info_args)

#ifdef FD_PAIR_GET_DATA
#define CSS_IOC_GET_FD_DATA 			_IOWR('v', BASE_VIDIOC_PRIVATE + 10,\
					struct css_fd_pair_result_data)
#else
#define CSS_IOC_GET_FD_DATA 			_IOWR('v', BASE_VIDIOC_PRIVATE + 10,\
					struct css_fd_result_data)
#endif
#define CSS_IOC_SET_SENSOR_SIZE 		_IOWR('v', BASE_VIDIOC_PRIVATE + 11,\
					struct css_sensor_info)

#define CSS_IOC_GET_SENSOR_INIT_SIZE	_IOWR('v', BASE_VIDIOC_PRIVATE + 12,\
					struct css_sensor_info)

#define CSS_IOC_I2C_READ				_IOWR('v', BASE_VIDIOC_PRIVATE + 13,\
					struct css_i2c_data)

#define CSS_IOC_I2C_WRITE				_IOWR('v', BASE_VIDIOC_PRIVATE + 14,\
					struct css_i2c_data)

#define CSS_IOC_SET_SENSOR_MODE_CHANGE	_IOWR('v', BASE_VIDIOC_PRIVATE + 15,\
					struct css_sensor_info)

#define CSS_IOC_OIS_CONTROL 			_IOWR('v', BASE_VIDIOC_PRIVATE + 16,\
					struct css_ois_control_args)

#define CSS_IOC_GET_OIS_DATA			_IOWR('v', BASE_VIDIOC_PRIVATE + 17,\
					struct css_ois_status_data)

#define CSS_IOC_SET_BAYER_OUTPUT_SIZE	_IOWR('v', BASE_VIDIOC_PRIVATE + 18,\
					struct css_bayer_ctrl_args)

#define CSS_IOC_SENSOR_RESCHG_IND		_IOWR('v', BASE_VIDIOC_PRIVATE + 19,\
					struct css_sensor_resolution_change_ind)

#define CSS_IOC_SET_ISP_PERFORMANCE		_IOWR('v', BASE_VIDIOC_PRIVATE + 20,\
					unsigned int)

#define CSS_IOC_GET_LIVE_PATH_SUPPORT	_IOWR('v', BASE_VIDIOC_PRIVATE + 21,\
					unsigned int)

#define CSS_IOC_SET_PATH_MODE			_IOWR('v', BASE_VIDIOC_PRIVATE + 22,\
					unsigned int)

#define CSS_IOC_SET_ASYNC_CTRL_PARAMS	_IOWR('v', BASE_VIDIOC_PRIVATE + 23,\
					struct css_afifo_param_args)

#define CSS_IOC_SET_LIVE_PATH_SET       _IOWR('v', BASE_VIDIOC_PRIVATE + 24,\
					unsigned int)

#define CSS_IOC_FLASH_CONTROL			_IOWR('v', BASE_VIDIOC_PRIVATE + 25,\
					struct css_flash_control_args)

#define CSS_IOC_SENSOR_MODE_CHANGE_AAT	_IOWR('v', BASE_VIDIOC_PRIVATE + 26,\
					struct css_sensor_info)

#define CSS_IOC_SET_ZSL_READ_PATH_SET	_IOWR('v', BASE_VIDIOC_PRIVATE + 27,\
					struct css_afifo_param_args)

#define CSS_IOC_FLASH_LED_CONTROL		_IOWR('v', BASE_VIDIOC_PRIVATE + 28,\
					struct css_flash_control_args)

#define CSS_IOC_SET_CAPTURE_FRAME_INTERVAL _IOWR('v', BASE_VIDIOC_PRIVATE + 29,\
					unsigned int)

#define CSS_IOC_SET_SENSOR_ROM_POWER	_IOWR('v', BASE_VIDIOC_PRIVATE + 30,\
					unsigned int)

#endif /* __ASM_AARCH_CAMERA_H */
