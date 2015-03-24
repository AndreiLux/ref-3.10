#ifndef __HDMI_DRV_H__
#define __HDMI_DRV_H__


#include "mhl_edid.h"

#define MHL_LINK_TIME	(5)

typedef enum {
	HDMI_CEC_CMD = 0,
	HDMI_PLUG_DETECT_CMD,
	HDMI_HDCP_PROTOCAL_CMD,
	HDMI_DISABLE_HDMI_TASK_CMD,
	MAX_HDMI_TMR_NUMBER
} HDMI_TASK_COMMAND_TYPE_T;



#ifndef ARY_SIZE
#define ARY_SIZE(x) (sizeof((x)) / sizeof((x[0])))
#endif

typedef enum {
	HDMI_POLARITY_RISING = 0,
	HDMI_POLARITY_FALLING = 1
} HDMI_POLARITY;

typedef enum {
	HDMI_CLOCK_PHASE_0 = 0,
	HDMI_CLOCK_PHASE_90 = 1
} HDMI_CLOCK_PHASE;

typedef enum {
	HDMI_COLOR_ORDER_RGB = 0,
	HDMI_COLOR_ORDER_BGR = 1
} HDMI_COLOR_ORDER;

typedef enum {
	IO_DRIVING_CURRENT_8MA = (1 << 0),
	IO_DRIVING_CURRENT_4MA = (1 << 1),
	IO_DRIVING_CURRENT_2MA = (1 << 2),
	IO_DRIVING_CURRENT_SLEW_CNTL = (1 << 3),
} IO_DRIVING_CURRENT;

typedef enum {
	HDMI_VIDEO_720x480p_60Hz = 0,	/* 0 */
	HDMI_VIDEO_720x576p_50Hz,	/* 1 */
	HDMI_VIDEO_1280x720p_60Hz,	/* 2 */
	HDMI_VIDEO_1280x720p_50Hz,	/* 3 */
	HDMI_VIDEO_1920x1080i_60Hz,	/* 4 */
	HDMI_VIDEO_1920x1080i_50Hz,	/* 5 */
	HDMI_VIDEO_1920x1080p_30Hz,	/* 6 */
	HDMI_VIDEO_1920x1080p_25Hz,	/* 7 */
	HDMI_VIDEO_1920x1080p_24Hz,	/* 8 */
	HDMI_VIDEO_1920x1080p_23Hz,	/* 9 */
	HDMI_VIDEO_1920x1080p_29Hz,	/* a */
	HDMI_VIDEO_1920x1080p_60Hz,	/* b */
	HDMI_VIDEO_1920x1080p_50Hz,	/* c */
	HDMI_VIDEO_1280x720p_60Hz_3D,	/* d */
	HDMI_VIDEO_1280x720p_50Hz_3D,	/* e */
	HDMI_VIDEO_1920x1080p_24Hz_3D,	/* f */
	HDMI_VIDEO_1920x1080p_23Hz_3D,	/* 10 */
	HDMI_VIDEO_RESOLUTION_NUM
} HDMI_VIDEO_RESOLUTION;

typedef enum {
	HDMI_VIN_FORMAT_RGB565,
	HDMI_VIN_FORMAT_RGB666,
	HDMI_VIN_FORMAT_RGB888,
} HDMI_VIDEO_INPUT_FORMAT;

typedef enum {
	HDMI_VOUT_FORMAT_RGB888,
	HDMI_VOUT_FORMAT_YUV422,
	HDMI_VOUT_FORMAT_YUV444,
} HDMI_VIDEO_OUTPUT_FORMAT;

typedef enum {
	HDMI_AUDIO_PCM_16bit_48000,
	HDMI_AUDIO_PCM_16bit_44100,
	HDMI_AUDIO_PCM_16bit_32000,
	HDMI_AUDIO_SOURCE_STREAM,
} HDMI_AUDIO_FORMAT;

typedef struct {
	HDMI_VIDEO_RESOLUTION vformat;
	HDMI_VIDEO_INPUT_FORMAT vin;
	HDMI_VIDEO_OUTPUT_FORMAT vout;
	HDMI_AUDIO_FORMAT aformat;
} HDMI_CONFIG;

typedef enum {
	HDMI_OUTPUT_MODE_LCD_MIRROR,
	HDMI_OUTPUT_MODE_VIDEO_MODE,
	HDMI_OUTPUT_MODE_DPI_BYPASS
} HDMI_OUTPUT_MODE;

typedef struct {
	unsigned int width;
	unsigned int height;

	HDMI_CONFIG init_config;

	/* polarity parameters */
	HDMI_POLARITY clk_pol;
	HDMI_POLARITY de_pol;
	HDMI_POLARITY vsync_pol;
	HDMI_POLARITY hsync_pol;

	/* timing parameters */
	unsigned int hsync_pulse_width;
	unsigned int hsync_back_porch;
	unsigned int hsync_front_porch;
	unsigned int vsync_pulse_width;
	unsigned int vsync_back_porch;
	unsigned int vsync_front_porch;

	/* output format parameters */
	HDMI_COLOR_ORDER rgb_order;

	/* intermediate buffers parameters */
	unsigned int intermediat_buffer_num;	/* 2..3 */

	/* iopad parameters */
	IO_DRIVING_CURRENT io_driving_current;
	HDMI_OUTPUT_MODE output_mode;

	int is_force_awake;
	int is_force_landscape;

	unsigned int scaling_factor;	/* determine the scaling of output screen size, valid value 0~10 */
	/* 0 means no scaling, 5 means scaling to 95%, 10 means 90% */
} HDMI_PARAMS;

typedef enum {
	HDMI_STATE_NO_DEVICE,
	HDMI_STATE_ACTIVE,
#if defined(CONFIG_MTK_MT8193_HDMI_SUPPORT)
	HDMI_STATE_PLUGIN_ONLY,
	HDMI_STATE_EDID_UPDATE,
	HDMI_STATE_CEC_UPDATE
#endif
} HDMI_STATE;


/* --------------------------------------------------------------------------- */

typedef struct {
	void (*set_reset_pin) (unsigned int value);
	int (*set_gpio_out) (unsigned int gpio, unsigned int value);
	void (*udelay) (unsigned int us);
	void (*mdelay) (unsigned int ms);
	void (*wait_transfer_done) (void);
	void (*state_callback) (HDMI_STATE state);
} HDMI_UTIL_FUNCS;


typedef struct {
	void (*set_util_funcs) (const HDMI_UTIL_FUNCS *util);
	void (*get_params) (HDMI_PARAMS *params);

	int (*init) (void);
	int (*enter) (void);
	int (*exit) (void);
	void (*suspend) (void);
	void (*resume) (void);
	int (*audio_config) (HDMI_AUDIO_FORMAT aformat);
	int (*video_config) (HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
			     HDMI_VIDEO_OUTPUT_FORMAT vou);
	int (*video_enable) (bool enable);
	int (*audio_enable) (bool enable);
	int (*irq_enable) (bool enable);
	int (*power_on) (void);
	void (*power_off) (void);
	 HDMI_STATE(*get_state) (void);
	void (*set_mode) (unsigned char ucMode);
	void (*dump) (void);

	void (*read) (unsigned int u4Reg, unsigned int u4Len);
	void (*write) (unsigned int u4Reg, unsigned int u4Data);
	void (*log_enable) (u16 enable);
	void (*InfoframeSetting) (u8 i1typemode, u8 i1typeselect);
	void (*checkedid) (u8 i1noedid);
	void (*enablehdcp) (u8 u1hdcponoff);
	void (*hdmistatus) (void);
	void (*hdcpkey) (u8 *pbhdcpkey);
	void (*getedid) (HDMI_EDID_INFO_T *pv_get_info);
	void (*mhl_cmd) (unsigned int u4Cmd, unsigned int u4Para, unsigned int u4Para1,
			 unsigned int u4Para2);

	void (*dump6397) (void);
	void (*write6397) (unsigned int u4Addr, unsigned int u4Data);
	void (*cbusstatus) (void);
} HDMI_DRIVER;


/* --------------------------------------------------------------------------- */
/* HDMI Driver Functions */
/* --------------------------------------------------------------------------- */

const HDMI_DRIVER *HDMI_GetDriver(void);
extern void vMhlTriggerIntTask(void);
extern bool fgMhlPowerOn;
#endif				/* __HDMI_DRV_H__ */
