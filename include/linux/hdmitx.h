/* --------------------------------------------------------------------------- */

#ifndef HDMITX_H
#define     HDMITX_H

#define HDMI_CHECK_RET(expr)                                                \
    do {                                                                    \
	HDMI_STATUS ret = (expr);                                           \
	if (HDMI_STATUS_OK != ret) {                                        \
	    pr_info("[ERROR][mtkfb] HDMI API return error code: 0x%x\n"      \
		   "  file : %s, line : %d\n"                               \
		   "  expr : %s\n", ret, __FILE__, __LINE__, #expr);        \
	}                                                                   \
    } while (0)


typedef enum {
	HDMI_STATUS_OK = 0,

	HDMI_STATUS_NOT_IMPLEMENTED,
	HDMI_STATUS_ALREADY_SET,
	HDMI_STATUS_ERROR,
} HDMI_STATUS;

typedef enum {
	HDMI_POWER_STATE_OFF = 0,
	HDMI_POWER_STATE_ON,
	HDMI_POWER_STATE_STANDBY,
} HDMI_POWER_STATE;

typedef struct {

} HDMI_CAPABILITY;



typedef struct {
	bool is_audio_enabled;
	bool is_video_enabled;
} hdmi_device_status;

typedef struct {
	void *src_base_addr;
	void *src_phy_addr;
	int src_fmt;
	unsigned int src_pitch;
	unsigned int src_offset_x, src_offset_y;
	unsigned int src_width, src_height;

	int identity;
	int connected_type;
	unsigned int security;
	int fenceFd;
	int ionFd;
} hdmi_video_buffer_info;

#if 1				/* defined(MTK_MT8193_HDMI_SUPPORT)|| defined(MTK_INTERNAL_HDMI_SUPPORT)||defined(MTK_INTERNAL_MHL_SUPPORT) */
typedef struct {
	unsigned int u4Addr;
	unsigned int u4Data;
} hdmi_device_write;

typedef struct {
	unsigned int u4Data1;
	unsigned int u4Data2;
} hdmi_para_setting;

typedef struct {
	unsigned char u1Hdcpkey[287];
} hdmi_hdcp_key;

typedef struct {
	unsigned char u1Hdcpkey[384];
} hdmi_hdcp_drmkey;

typedef struct {
	unsigned char u1sendsltdata[15];
} send_slt_data;

typedef struct _HDMI_EDID_T {
	unsigned int ui4_ntsc_resolution;	/* use EDID_VIDEO_RES_T, there are many resolution */
	unsigned int ui4_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_native_ntsc_resolution;	/* use EDID_VIDEO_RES_T, only one NTSC resolution, Zero means none native NTSC resolution is avaiable */
	unsigned int ui4_sink_native_pal_resolution;	/* use EDID_VIDEO_RES_T, only one resolution, Zero means none native PAL resolution is avaiable */
	unsigned int ui4_sink_cea_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_cea_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_ntsc_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned int ui4_sink_1st_dtd_pal_resolution;	/* use EDID_VIDEO_RES_T */
	unsigned short ui2_sink_colorimetry;	/* use EDID_VIDEO_COLORIMETRY_T */
	unsigned char ui1_sink_rgb_color_bit;	/* color bit for RGB */
	unsigned char ui1_sink_ycbcr_color_bit;	/* color bit for YCbCr */
	unsigned short ui2_sink_aud_dec;	/* use EDID_AUDIO_DECODER_T */
	unsigned char ui1_sink_is_plug_in;	/* 1: Plug in 0:Plug Out */
	unsigned int ui4_hdmi_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	unsigned int ui4_hdmi_pcm_ch3ch4ch5ch7_type;	/* use EDID_A_FMT_CH_TYPE1 */
	unsigned int ui4_dac_pcm_ch_type;	/* use EDID_A_FMT_CH_TYPE */
	unsigned char ui1_sink_i_latency_present;
	unsigned char ui1_sink_p_audio_latency;
	unsigned char ui1_sink_p_video_latency;
	unsigned char ui1_sink_i_audio_latency;
	unsigned char ui1_sink_i_video_latency;
	unsigned char ui1ExtEdid_Revision;
	unsigned char ui1Edid_Version;
	unsigned char ui1Edid_Revision;
	unsigned char ui1_Display_Horizontal_Size;
	unsigned char ui1_Display_Vertical_Size;
	unsigned int ui4_ID_Serial_Number;
	unsigned int ui4_sink_cea_3D_resolution;
	unsigned char ui1_sink_support_ai;	/* 0: not support AI, 1:support AI */
	unsigned short ui2_sink_cec_address;
	unsigned short ui1_sink_max_tmds_clock;
	unsigned short ui2_sink_3D_structure;
	unsigned int ui4_sink_cea_FP_SUP_3D_resolution;
	unsigned int ui4_sink_cea_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_cea_SBS_SUP_3D_resolution;
	unsigned short ui2_sink_ID_manufacturer_name;	/* (08H~09H) */
	unsigned short ui2_sink_ID_product_code;	/* (0aH~0bH) */
	unsigned int ui4_sink_ID_serial_number;	/* (0cH~0fH) */
	unsigned char ui1_sink_week_of_manufacture;	/* (10H) */
	unsigned char ui1_sink_year_of_manufacture;	/* (11H)  base on year 1990 */
} HDMI_EDID_T;

#if 1				/* def MTK_INTERNAL_MHL_SUPPORT */
typedef struct {
	unsigned int ui4_sink_FP_SUP_3D_resolution;
	unsigned int ui4_sink_TOB_SUP_3D_resolution;
	unsigned int ui4_sink_SBS_SUP_3D_resolution;
} MHL_3D_SUPP_T;
#endif

typedef struct {
	unsigned char ui1_la_num;
	unsigned char e_la[3];
	unsigned short ui2_pa;
	unsigned short h_cecm_svc;
} CEC_DRV_ADDR_CFG;

typedef struct {
	unsigned char destination:4;
	unsigned char initiator:4;
} CEC_HEADER_BLOCK_IO;

typedef struct {
	CEC_HEADER_BLOCK_IO header;
	unsigned char opcode;
	unsigned char operand[15];
} CEC_FRAME_BLOCK_IO;

typedef struct {
	unsigned char size;
	unsigned char sendidx;
	unsigned char reTXcnt;
	void *txtag;
	CEC_FRAME_BLOCK_IO blocks;
} CEC_FRAME_DESCRIPTION_IO;

typedef struct _CEC_FRAME_INFO {
	unsigned char ui1_init_addr;
	unsigned char ui1_dest_addr;
	unsigned short ui2_opcode;
	unsigned char aui1_operand[14];
	unsigned int z_operand_size;
} CEC_FRAME_INFO;

typedef struct _CEC_SEND_MSG {
	CEC_FRAME_INFO t_frame_info;
	unsigned char b_enqueue_ok;
} CEC_SEND_MSG;

typedef struct {
	unsigned char ui1_la;
	unsigned short ui2_pa;
} CEC_ADDRESS_IO;

typedef struct {
	unsigned char u1Size;
	unsigned char au1Data[14];
} CEC_GETSLT_DATA;

typedef struct {
	unsigned int u1adress;
	unsigned int pu1Data;
} READ_REG_VALUE;

#endif

typedef struct mtk_hdmi_info {
	unsigned int display_id;
	unsigned int isHwVsyncAvailable;
	unsigned int displayWidth;
	unsigned int displayHeight;
	unsigned int displayFormat;
	unsigned int vsyncFPS;
	unsigned int xDPI;
	unsigned int yDPI;
	unsigned int isConnected;
} mtk_hdmi_info_t;

#if 1				/* defined(MTK_INTERNAL_HDMI_SUPPORT) ||defined(MTK_INTERNAL_MHL_SUPPORT) */
typedef struct {
	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_hdmi_fs;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
	unsigned char u1HdmiI2sMclk;
	unsigned char bhdmi_LCh_status[5];
	unsigned char bhdmi_RCh_status[5];
} HDMITX_AUDIO_PARA;
#endif

typedef struct {
	/* Input */
	int ion_fd;
	/* Output */
	unsigned int index;	/* fence count */
	int fence_fd;		/* fence fd */
} hdmi_buffer_info;

#define MTK_HDMI_NO_FENCE_FD	((int)(-1))	/* ((int)(~0U>>1)) */
#define MTK_HDMI_NO_ION_FD        ((int)(-1))	/* ((int)(~0U>>1)) */

#define HDMI_IOW(num, dtype)     _IOW('H', num, dtype)
#define HDMI_IOR(num, dtype)     _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype)    _IOWR('H', num, dtype)
#define HDMI_IO(num)             _IO('H', num)

#define MTK_HDMI_AUDIO_VIDEO_ENABLE			HDMI_IO(1)
#define MTK_HDMI_AUDIO_ENABLE							HDMI_IO(2)
#define MTK_HDMI_VIDEO_ENABLE							HDMI_IO(3)
#define MTK_HDMI_GET_CAPABILITY						HDMI_IOWR(4, HDMI_CAPABILITY)
#define MTK_HDMI_GET_DEVICE_STATUS				HDMI_IOWR(5, hdmi_device_status)
#define MTK_HDMI_VIDEO_CONFIG							HDMI_IOWR(6, int)
#define MTK_HDMI_AUDIO_CONFIG							HDMI_IOWR(7, int)
#define MTK_HDMI_FORCE_FULLSCREEN_ON		HDMI_IOWR(8, int)
#define MTK_HDMI_FORCE_FULLSCREEN_OFF	HDMI_IOWR(9, int)
#define MTK_HDMI_IPO_POWEROFF				HDMI_IOWR(10, int)
#define MTK_HDMI_IPO_POWERON					HDMI_IOWR(11, int)
#define MTK_HDMI_POWER_ENABLE				HDMI_IOW(12, int)
#define MTK_HDMI_PORTRAIT_ENABLE			HDMI_IOW(13, int)
#define MTK_HDMI_FORCE_OPEN							HDMI_IOWR(14, int)
#define MTK_HDMI_FORCE_CLOSE							HDMI_IOWR(15, int)
#define MTK_HDMI_IS_FORCE_AWAKE                 HDMI_IOWR(16, int)
#define MTK_HDMI_ENTER_VIDEO_MODE               HDMI_IO(17)
#define MTK_HDMI_LEAVE_VIDEO_MODE               HDMI_IO(18)
#define MTK_HDMI_REGISTER_VIDEO_BUFFER          HDMI_IOW(19, hdmi_video_buffer_info)
#define MTK_HDMI_POST_VIDEO_BUFFER              HDMI_IOW(20, hdmi_video_buffer_info)
#define MTK_HDMI_FACTORY_MODE_ENABLE            HDMI_IOW(21, int)
#if 1				/* defined(MTK_INTERNAL_HDMI_SUPPORT)||defined(MTK_INTERNAL_MHL_SUPPORT) */
#define MTK_HDMI_WRITE_DEV           HDMI_IOWR(22, hdmi_device_write)
#define MTK_HDMI_READ_DEV            HDMI_IOWR(23, unsigned int)
#define MTK_HDMI_ENABLE_LOG          HDMI_IOWR(24, unsigned int)
#define MTK_HDMI_CHECK_EDID          HDMI_IOWR(25, unsigned int)
#define MTK_HDMI_INFOFRAME_SETTING   HDMI_IOWR(26, hdmi_para_setting)
#define MTK_HDMI_COLOR_DEEP          HDMI_IOWR(27, hdmi_para_setting)
#define MTK_HDMI_ENABLE_HDCP         HDMI_IOWR(28, unsigned int)
#define MTK_HDMI_STATUS           HDMI_IOWR(29, unsigned int)
#define MTK_HDMI_HDCP_KEY         HDMI_IOWR(30, hdmi_hdcp_key)
#define MTK_HDMI_GET_EDID         HDMI_IOWR(31, HDMI_EDID_T)
#define MTK_HDMI_SETLA            HDMI_IOWR(32, CEC_DRV_ADDR_CFG)
#define MTK_HDMI_GET_CECCMD       HDMI_IOWR(33, CEC_FRAME_DESCRIPTION_IO)
#define MTK_HDMI_SET_CECCMD       HDMI_IOWR(34, CEC_SEND_MSG)
#define MTK_HDMI_CEC_ENABLE       HDMI_IOWR(35, unsigned int)
#define MTK_HDMI_GET_CECADDR      HDMI_IOWR(36, CEC_ADDRESS_IO)
#define MTK_HDMI_CECRX_MODE       HDMI_IOWR(37, unsigned int)
#define MTK_HDMI_SENDSLTDATA      HDMI_IOWR(38, send_slt_data)
#define MTK_HDMI_GET_SLTDATA      HDMI_IOWR(39, CEC_GETSLT_DATA)
#define MTK_HDMI_VIDEO_MUTE		HDMI_IOWR(40, int)
#endif
#define MTK_HDMI_NOTIFY_SF_RESTART		HDMI_IOWR(90, int)



#define MTK_HDMI_USBOTG_STATUS	    HDMI_IOWR(80, int)
#define MTK_HDMI_GET_DRM_ENABLE     HDMI_IOWR(81, int)


struct ext_memory_info {
	unsigned int buffer_num;
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
};

struct ext_buffer {
	unsigned int id;
	unsigned int ts_sec;
	unsigned int ts_nsec;
};

#define MTK_EXT_DISPLAY_ENTER									HDMI_IO(40)
#define MTK_EXT_DISPLAY_LEAVE									HDMI_IO(41)
#define MTK_EXT_DISPLAY_START									HDMI_IO(42)
#define MTK_EXT_DISPLAY_STOP									HDMI_IO(43)
#define MTK_EXT_DISPLAY_SET_MEMORY_INFO		HDMI_IOW(44, struct ext_memory_info)
#define MTK_EXT_DISPLAY_GET_MEMORY_INFO		HDMI_IOW(45, struct ext_memory_info)
#define MTK_EXT_DISPLAY_GET_BUFFER						HDMI_IOW(46, struct ext_buffer)
#define MTK_EXT_DISPLAY_FREE_BUFFER						HDMI_IOW(47, struct ext_buffer)
#if 1				/* defined(MTK_INTERNAL_HDMI_SUPPORT)||defined(MTK_INTERNAL_MHL_SUPPORT) */
#define MTK_HDMI_AUDIO_SETTING      HDMI_IOWR(48, HDMITX_AUDIO_PARA)
#endif


#define MTK_HDMI_READ				HDMI_IOWR(49, unsigned int)
#define MTK_HDMI_WRITE				HDMI_IOWR(50, unsigned int)
#define MTK_HDMI_CMD				HDMI_IOWR(51, unsigned int)
#define MTK_HDMI_DUMP				HDMI_IOWR(52, unsigned int)
#define MTK_HDMI_DUMP6397			HDMI_IOWR(53, unsigned int)
#define MTK_HDMI_DUMP6397_W		HDMI_IOWR(54, unsigned int)
#define MTK_HDMI_CBUS_STATUS		HDMI_IOWR(55, unsigned int)
#define MTK_HDMI_CONNECT_STATUS				HDMI_IOWR(56, unsigned int)
#define MTK_HDMI_DUMP6397_R		HDMI_IOWR(57, unsigned int)
/* #define MTK_HDMI_GET_EDID                     HDMI_IOWR(58, unsigned int) */
#define MTK_MHL_GET_DCAP		HDMI_IOWR(58, unsigned int)
#define MTK_MHL_GET_3DINFO		HDMI_IOWR(59, unsigned int)
#define MTK_HDMI_HDCP				HDMI_IOWR(60, unsigned int)

#define MTK_HDMI_GET_DEV_INFO                                HDMI_IOWR(61, mtk_dispif_info_t)
#define MTK_HDMI_GET_FENCE_ID						            HDMI_IOW(62, int)
#define MTK_HDMI_SCREEN_CAPTURE                              HDMI_IOW(63, unsigned long)
#define MTK_HDMI_PREPARE_BUFFER                              HDMI_IOW(64, hdmi_buffer_info)

enum HDMI_report_state {
	NO_DEVICE = 0,
	HDMI_PLUGIN = 1,
};

typedef enum {
	HDMI_CHARGE_CURRENT,

} HDMI_QUERY_TYPE;

/* add for cancel mtk key word */
#if 1
#define HDMI_AUDIO_VIDEO_ENABLE			MTK_HDMI_AUDIO_VIDEO_ENABLE
#define HDMI_AUDIO_ENABLE							MTK_HDMI_AUDIO_ENABLE
#define HDMI_VIDEO_ENABLE							MTK_HDMI_VIDEO_ENABLE
#define HDMI_GET_CAPABILITY						MTK_HDMI_GET_CAPABILITY
#define HDMI_GET_DEVICE_STATUS				MTK_HDMI_GET_DEVICE_STATUS
#define HDMI_VIDEO_CONFIG							MTK_HDMI_VIDEO_CONFIG
#define HDMI_AUDIO_CONFIG							MTK_HDMI_AUDIO_CONFIG
#define HDMI_FORCE_FULLSCREEN_ON		MTK_HDMI_FORCE_FULLSCREEN_ON
#define HDMI_FORCE_FULLSCREEN_OFF	MTK_HDMI_FORCE_FULLSCREEN_OFF
#define HDMI_IPO_POWEROFF				MTK_HDMI_IPO_POWEROFF
#define HDMI_IPO_POWERON					MTK_HDMI_IPO_POWERON
#define HDMI_POWER_ENABLE				MTK_HDMI_POWER_ENABLE
#define HDMI_PORTRAIT_ENABLE			MTK_HDMI_PORTRAIT_ENABLE
#define HDMI_FORCE_OPEN							MTK_HDMI_FORCE_OPEN
#define HDMI_FORCE_CLOSE							MTK_HDMI_FORCE_CLOSE
#define HDMI_IS_FORCE_AWAKE                 MTK_HDMI_IS_FORCE_AWAKE
#define HDMI_ENTER_VIDEO_MODE               MTK_HDMI_ENTER_VIDEO_MODE
#define HDMI_LEAVE_VIDEO_MODE               MTK_HDMI_LEAVE_VIDEO_MODE
#define HDMI_REGISTER_VIDEO_BUFFER          MTK_HDMI_REGISTER_VIDEO_BUFFER
#define HDMI_POST_VIDEO_BUFFER              MTK_HDMI_POST_VIDEO_BUFFER
#define HDMI_FACTORY_MODE_ENABLE            MTK_HDMI_FACTORY_MODE_ENABLE
#define HDMI_WRITE_DEV           MTK_HDMI_WRITE_DEV
#define HDMI_READ_DEV            MTK_HDMI_READ_DEV
#define HDMI_ENABLE_LOG          MTK_HDMI_ENABLE_LOG
#define HDMI_CHECK_EDID          MTK_HDMI_CHECK_EDID
#define HDMI_INFOFRAME_SETTING   MTK_HDMI_INFOFRAME_SETTING
#define HDMI_COLOR_DEEP          MTK_HDMI_COLOR_DEEP
#define HDMI_ENABLE_HDCP         MTK_HDMI_ENABLE_HDCP
#define HDMI_GET_STATUS           MTK_HDMI_STATUS
#define HDMI_HDCP_KEY         MTK_HDMI_HDCP_KEY
#define HDMI_GET_EDID         MTK_HDMI_GET_EDID
#define HDMI_SETLA            MTK_HDMI_SETLA
#define HDMI_GET_CECCMD       MTK_HDMI_GET_CECCMD
#define HDMI_SET_CECCMD       MTK_HDMI_SET_CECCMD
#define HDMI_CEC_ENABLE       MTK_HDMI_CEC_ENABLE
#define HDMI_GET_CECADDR      MTK_HDMI_GET_CECADDR
#define HDMI_CECRX_MODE       MTK_HDMI_CECRX_MODE
#define HDMI_SENDSLTDATA      MTK_HDMI_SENDSLTDATA
#define HDMI_GET_SLTDATA      MTK_HDMI_GET_SLTDATA
#define HDMI_VIDEO_MUTE		MTK_HDMI_VIDEO_MUTE
#define HDMI_USBOTG_STATUS	    MTK_HDMI_USBOTG_STATUS
#define HDMI_GET_DRM_ENABLE         MTK_HDMI_GET_DRM_ENABLE
#define HDMI_AUDIO_SETTING      MTK_HDMI_AUDIO_SETTING
#define HDMI_READ				MTK_HDMI_READ
#define HDMI_WRITE				MTK_HDMI_WRITE
#define HDMI_CMD				MTK_HDMI_CMD
#define HDMI_DUMP				MTK_HDMI_DUMP
#define HDMI_DUMP6397			MTK_HDMI_DUMP6397
#define HDMI_DUMP6397_W		MTK_HDMI_DUMP6397_W
#define HDMI_CBUS_STATUS		MTK_HDMI_CBUS_STATUS
#define HDMI_CONNECT_STATUS				MTK_HDMI_CONNECT_STATUS
#define HDMI_DUMP6397_R		MTK_HDMI_DUMP6397_R
#define MHL_GET_DCAP		MTK_MHL_GET_DCAP
#define MHL_GET_3DINFO		MTK_MHL_GET_3DINFO
#define HDMI_HDCP				MTK_HDMI_HDCP
#define HDMI_GET_DEV_INFO                                MTK_HDMI_GET_DEV_INFO
#define HDMI_GET_FENCE_ID						            MTK_HDMI_GET_FENCE_ID
#define HDMI_SCREEN_CAPTURE                              MTK_HDMI_SCREEN_CAPTURE
#define HDMI_PREPARE_BUFFER                              MTK_HDMI_PREPARE_BUFFER

#endif
/* end*/

int get_hdmi_dev_info(HDMI_QUERY_TYPE type);
bool is_hdmi_enable(void);
void hdmi_setorientation(int orientation);
void hdmi_suspend(void);
void hdmi_resume(void);
void hdmi_power_on(void);
void hdmi_power_off(void);
void hdmi_update_buffer_switch(void);
void hdmi_update(void);
void hdmi_dpi_config_clock(void);
void hdmi_dpi_power_switch(bool enable);
int hdmi_audio_config(int samplerate);
int hdmi_video_enable(bool enable);
int hdmi_audio_enable(bool enable);
int hdmi_audio_delay_mute(int latency);
void hdmi_set_mode(unsigned char ucMode);
void hdmi_reg_dump(void);

void hdmi_read_reg(unsigned char u8Reg);
void hdmi_write_reg(unsigned char u8Reg, unsigned char u8Data);


void hdmi_log_enable(int enable);
void hdmi_log_level(int loglv);
void hdmi_purescreen(bool enable);
void hdmi_cable_fake_plug_in(void);
void hdmi_cable_fake_plug_out(void);
void hdmi_vga_res(int res);


void DBG_OnTriggerHDMI(void);
void HDMI_DBG_Init(void);


#endif
