
#ifndef __hdmitable_h__
#define __hdmitable_h__

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

	HDMI_VIDEO_1280x720p3d_60Hz,	/* d */
	HDMI_VIDEO_1280x720p3d_50Hz,	/* e */
	HDMI_VIDEO_1920x1080i3d_60Hz,	/* f */
	HDMI_VIDEO_1920x1080i3d_50Hz,	/* 10 */
	HDMI_VIDEO_1920x1080p3d_24Hz,	/* 11 */
	HDMI_VIDEO_1920x1080p3d_23Hz,	/* 12 */

	VGA_VIDEO_1680x1050_60Hz,	/* 1 */
	VGA_VIDEO_1440x1050_60Hz,	/* 2 */
	VGA_VIDEO_1440x900_60Hz,	/* 3 */
	VGA_VIDEO_1366x768_60Hz,	/* 4 */
	VGA_VIDEO_1360x768_60Hz,	/* 5 */
	VGA_VIDEO_1280x1024_60Hz,	/* 6 */
	VGA_VIDEO_1280x960_60Hz,	/* 7 */
	VGA_VIDEO_1280x800_60Hz,	/* 8 */
	VGA_VIDEO_1280x768_60Hz,	/* 9 */
	VGA_VIDEO_1024x768_60Hz,	/* a */
	VGA_VIDEO_848x480_60Hz,	/* b */
	VGA_VIDEO_640x480_60Hz,	/* c */
	VGA_VIDEO_800x600_60Hz,	/* d */

	HDMI_VIDEO_RESOLUTION_NUM
} HDMI_VIDEO_RESOLUTION;

typedef enum {
	HDMI_DEEP_COLOR_AUTO = 0,
	HDMI_NO_DEEP_COLOR,
	HDMI_DEEP_COLOR_10_BIT,
	HDMI_DEEP_COLOR_12_BIT,
	HDMI_DEEP_COLOR_16_BIT
} HDMI_DEEP_COLOR_T;

typedef enum {
	SV_I2S = 0,
	SV_SPDIF
} HDMI_AUDIO_INPUT_TYPE_T;

typedef enum			/* new add 2007/9/12 */
{
	FS_16K = 0x00,
	FS_22K,
	FS_24K,
	FS_32K,
	FS_44K,
	FS_48K,
	FS_64K,
	FS_88K,
	FS_96K,
	FS_176K,
	FS_192K,
	FS512_44K,		/* for DSD */
	FS_768K,
	FS128_44k,
	FS_128K,
	FS_UNKOWN,
	FS_48K_MAX_CH
} AUDIO_SAMPLING_T;

typedef enum {
	IEC_48K = 0,
	IEC_96K,
	IEC_192K,
	IEC_768K,
	IEC_44K,
	IEC_88K,
	IEC_176K,
	IEC_705K,
	IEC_16K,
	IEC_22K,
	IEC_24K,
	IEC_32K,


} IEC_FRAME_RATE_T;

typedef enum {
	HDMI_FS_32K = 0,
	HDMI_FS_44K,
	HDMI_FS_48K,
	HDMI_FS_88K,
	HDMI_FS_96K,
	HDMI_FS_176K,
	HDMI_FS_192K
} HDMI_AUDIO_SAMPLING_T;

typedef enum {
	PCM_16BIT = 0,
	PCM_20BIT,
	PCM_24BIT
} PCM_BIT_SIZE_T;

typedef enum {
	HDMI_RGB = 0,
	HDMI_RGB_FULL,
	HDMI_YCBCR_444,
	HDMI_YCBCR_422,
	HDMI_XV_YCC,
	HDMI_YCBCR_444_FULL,
	HDMI_YCBCR_422_FULL
} HDMI_OUT_COLOR_SPACE_T;

typedef enum {
	HDMI_RJT_24BIT = 0,
	HDMI_RJT_16BIT,
	HDMI_LJT_24BIT,
	HDMI_LJT_16BIT,
	HDMI_I2S_24BIT,
	HDMI_I2S_16BIT
} HDMI_AUDIO_I2S_FMT_T;

typedef enum {
	AUD_INPUT_1_0 = 0,
	AUD_INPUT_1_1,
	AUD_INPUT_2_0,
	AUD_INPUT_2_1,
	AUD_INPUT_3_0,		/* C,L,R */
	AUD_INPUT_3_1,		/* C,L,R */
	AUD_INPUT_4_0,		/* L,R,RR,RL */
	AUD_INPUT_4_1,		/* L,R,RR,RL */
	AUD_INPUT_5_0,
	AUD_INPUT_5_1,
	AUD_INPUT_6_0,
	AUD_INPUT_6_1,
	AUD_INPUT_7_0,
	AUD_INPUT_7_1,
	AUD_INPUT_3_0_LRS,	/* LRS */
	AUD_INPUT_3_1_LRS,	/* LRS */
	AUD_INPUT_4_0_CLRS,	/* C,L,R,S */
	AUD_INPUT_4_1_CLRS,	/* C,L,R,S */
	/* new layout added for DTS */
	AUD_INPUT_6_1_Cs,
	AUD_INPUT_6_1_Ch,
	AUD_INPUT_6_1_Oh,
	AUD_INPUT_6_1_Chr,
	AUD_INPUT_7_1_Lh_Rh,
	AUD_INPUT_7_1_Lsr_Rsr,
	AUD_INPUT_7_1_Lc_Rc,
	AUD_INPUT_7_1_Lw_Rw,
	AUD_INPUT_7_1_Lsd_Rsd,
	AUD_INPUT_7_1_Lss_Rss,
	AUD_INPUT_7_1_Lhs_Rhs,
	AUD_INPUT_7_1_Cs_Ch,
	AUD_INPUT_7_1_Cs_Oh,
	AUD_INPUT_7_1_Cs_Chr,
	AUD_INPUT_7_1_Ch_Oh,
	AUD_INPUT_7_1_Ch_Chr,
	AUD_INPUT_7_1_Oh_Chr,
	AUD_INPUT_7_1_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_6_0_Cs,
	AUD_INPUT_6_0_Ch,
	AUD_INPUT_6_0_Oh,
	AUD_INPUT_6_0_Chr,
	AUD_INPUT_7_0_Lh_Rh,
	AUD_INPUT_7_0_Lsr_Rsr,
	AUD_INPUT_7_0_Lc_Rc,
	AUD_INPUT_7_0_Lw_Rw,
	AUD_INPUT_7_0_Lsd_Rsd,
	AUD_INPUT_7_0_Lss_Rss,
	AUD_INPUT_7_0_Lhs_Rhs,
	AUD_INPUT_7_0_Cs_Ch,
	AUD_INPUT_7_0_Cs_Oh,
	AUD_INPUT_7_0_Cs_Chr,
	AUD_INPUT_7_0_Ch_Oh,
	AUD_INPUT_7_0_Ch_Chr,
	AUD_INPUT_7_0_Oh_Chr,
	AUD_INPUT_7_0_Lss_Rss_Lsr_Rsr,
	AUD_INPUT_8_0_Lh_Rh_Cs,
	AUD_INPUT_UNKNOWN = 0xFF
} AUD_CH_NUM_T;
typedef enum {
	MCLK_128FS,
	MCLK_192FS,
	MCLK_256FS,
	MCLK_384FS,
	MCLK_512FS,
	MCLK_768FS,
	MCLK_1152FS,
} SAMPLE_FREQUENCY_T;

static const unsigned char HDMI_VIDEO_ID_CODE[HDMI_VIDEO_RESOLUTION_NUM] = { 2, 17, 4, 19, 5, 20, 34, 33, 32, 32, 34, 16, 31 };	/* , , 480P,576P, ,, , ,720P60,720P50,1080I60,1080I50,,,1080P30,1080P25, , , 1080P24, 1080P23.97, 1080P29.97, 1080p60,1080p50 */

static const unsigned char POSDIV[3][4] = {
	{0x3, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x1, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x0, 0x0}	/* 148Mhz */
};

static const unsigned char FBKSEL[3][4] = {
	{0x1, 0x1, 0x1, 0x1},	/* 27Mhz */
	{0x1, 0x0, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x1, 0x1}	/* 148Mhz */
};

static const unsigned char FBKDIV[3][4] = {
	{19, 24, 29, 19},	/* 27Mhz */
	{19, 24, 14, 19},	/* 74Mhz */
	{19, 24, 14, 19}	/* 148Mhz */
};

static const unsigned char DIVEN[3][4] = {
	{0x2, 0x1, 0x1, 0x2},	/* 27Mhz */
	{0x2, 0x2, 0x2, 0x2},	/* 74Mhz */
	{0x2, 0x2, 0x2, 0x2}	/* 148Mhz */
};

static const unsigned char HTPLLBP[3][4] = {
	{0xc, 0xc, 0x8, 0xc},	/* 27Mhz */
	{0xc, 0xf, 0xf, 0xc},	/* 74Mhz */
	{0xc, 0xf, 0xf, 0xc}	/* 148Mhz */
};

static const unsigned char HTPLLBC[3][4] = {
	{0x2, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x3, 0x3, 0x2},	/* 74Mhz */
	{0x2, 0x3, 0x3, 0x2}	/* 148Mhz */
};

static const unsigned char HTPLLBR[3][4] = {
	{0x1, 0x1, 0x0, 0x1},	/* 27Mhz */
	{0x1, 0x2, 0x2, 0x1},	/* 74Mhz */
	{0x1, 0x2, 0x2, 0x1}	/* 148Mhz */
};

#define NCTS_BYTES          0x07
static const unsigned char HDMI_NCTS[7][7][NCTS_BYTES] = {
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x10, 0x00},	/* 32K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x10, 0x00},	/* 32K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x03, 0x37, 0xf9, 0x00, 0x2d, 0x80},	/* 32K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x10, 0x00},	/* 32K, 720p@50/1080i@50 */
	 {0x00, 0x06, 0x6f, 0xf3, 0x00, 0x2d, 0x80},	/* 32K, 1080p@60 */
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x10, 0x00},	/* 32K, 1080p@50 */
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x10, 0x00}	/* 32K, 480p@108MHz/576p@108MHz */
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x18, 0x80},	/* 44K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x18, 0x80},	/* 44K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},	/* 44K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x18, 0x80},	/* 44K, 720p@50/1080i@50 */
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x22, 0xd6},	/* 44K, 1080p@60 */
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x18, 0x80},	/* 44K, 1080p@50 */
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x18, 0x80}	/* 44K, 480p@108MHz/576p@108MHz */
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x18, 0x00},	/* 48K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x18, 0x00},	/* 48K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},	/* 48K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x18, 0x00},	/* 48K, 720p@50/1080i@50 */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x16, 0xc0},	/* 48K, 1080p@60 */
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x18, 0x00},	/* 48K, 1080p@50 */
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x18, 0x00}	/* 48K, 108p@54MHz/576p@108MHz */
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x31, 0x00},	/* 88K 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x31, 0x00},	/* 88K 480p@54MHz/576p@54MHz */
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},	/* 88K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x31, 0x00},	/* 88K, 720p@50/1080i@50 */
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},	/* 88K, 1080p@60 */
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x31, 0x00},	/* 88K, 1080p@50 */
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x31, 0x00}	/* 88K 480p@108MHz/576p@108MHz */
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x30, 0x00},	/* 96K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x30, 0x00},	/* 96K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},	/* 96K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x30, 0x00},	/* 96K, 720p@50/1080i@50 */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},	/* 96K, 1080p@60 */
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x30, 0x00},	/* 96K, 1080p@50 */
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x30, 0x00}	/* 96K, 480p@108MHz/576p@108MHz */
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x62, 0x00},	/* 176K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x62, 0x00},	/* 176K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x03, 0x93, 0x87, 0x01, 0x16, 0xb0},	/* 176K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x62, 0x00},	/* 176K, 720p@50/1080i@50 */
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},	/* 176K, 1080p@60 */
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x62, 0x00},	/* 176K, 1080p@50 */
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x62, 0x00}	/* 176K, 480p@54MHz/576p@54MHz */
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x60, 0x00},	/* 192K, 480i/576i/480p@27MHz/576p@27MHz */
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x60, 0x00},	/* 192K, 480p@54MHz/576p@54MHz */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0xb6, 0x00},	/* 192K, 720p@60/1080i@60 */
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x60, 0x00},	/* 192K, 720p@50/1080i@50 */
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},	/* 192K, 1080p@60 */
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x60, 0x00},	/* 192K, 1080p@50 */
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x60, 0x00}	/* 192K, 480p@108MHz/576p@108MHz */
	 }
};

/* /////////////////////////////////////////////////////////// */
typedef struct _AUDIO_DEC_OUTPUT_CHANNEL_T {
	unsigned short FL:1;	/* bit0 */
	unsigned short FR:1;	/* bit1 */
	unsigned short LFE:1;	/* bit2 */
	unsigned short FC:1;	/* bit3 */
	unsigned short RL:1;	/* bit4 */
	unsigned short RR:1;	/* bit5 */
	unsigned short RC:1;	/* bit6 */
	unsigned short FLC:1;	/* bit7 */
	unsigned short FRC:1;	/* bit8 */
	unsigned short RRC:1;	/* bit9 */
	unsigned short RLC:1;	/* bit10 */

} HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T;

typedef union _AUDIO_DEC_OUTPUT_CHANNEL_UNION_T {
	HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T bit;	/* HDMI_AUDIO_DEC_OUTPUT_CHANNEL_T */
	unsigned short word;

} AUDIO_DEC_OUTPUT_CHANNEL_UNION_T;

/* //////////////////////////////////////////////////////// */
typedef struct _HDMI_AV_INFO_T {
	HDMI_VIDEO_RESOLUTION e_resolution;
	unsigned char fgHdmiOutEnable;
	unsigned char u2VerFreq;
	unsigned char b_hotplug_state;
	HDMI_OUT_COLOR_SPACE_T e_video_color_space;
	HDMI_DEEP_COLOR_T e_deep_color_bit;
	unsigned char ui1_aud_out_ch_number;
	HDMI_AUDIO_SAMPLING_T e_hdmi_fs;
	unsigned char bhdmiRChstatus[6];
	unsigned char bhdmiLChstatus[6];
	unsigned char bMuteHdmiAudio;
	unsigned char u1HdmiI2sMclk;
	unsigned char u1hdcponoff;
	unsigned char u1audiosoft;
	unsigned char fgHdmiTmdsEnable;
	AUDIO_DEC_OUTPUT_CHANNEL_UNION_T ui2_aud_out_ch;

	unsigned char e_hdmi_aud_in;
	unsigned char e_iec_frame;
	unsigned char e_aud_code;
	unsigned char u1Aud_Input_Chan_Cnt;
	unsigned char e_I2sFmt;
} HDMI_AV_INFO_T;

/* ///////////////////////////////////////////////////////// */

#endif
