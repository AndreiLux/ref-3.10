#ifndef _MHL_HDMI_H_
#define _MHL_HDMI_H_
/* --------------------------------------------------------------------------- */

#define HDMI_CHECK_RET(expr)                                                \
    do {                                                                    \
	HDMI_STATUS ret = (expr);                                           \
	if (HDMI_STATUS_OK != ret) {                                        \
	    printk("[ERROR][mtkfb] HDMI API return error code: 0x%x\n"      \
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

typedef struct {


} HDMI_CAPABILITY;

typedef struct {
	bool is_audio_enabled;
	bool is_video_enabled;
} hdmi_device_status;


#define HDMI_IOW(num, dtype)     _IOW('H', num, dtype)
#define HDMI_IOR(num, dtype)     _IOR('H', num, dtype)
#define HDMI_IOWR(num, dtype)    _IOWR('H', num, dtype)
#define HDMI_IO(num)             _IO('H', num)

#define MTK_HDMI_AUDIO_VIDEO_ENABLE		HDMI_IO(1)
#define MTK_HDMI_AUDIO_ENABLE			HDMI_IO(2)
#define MTK_HDMI_VIDEO_ENABLE			HDMI_IO(3)
#define MTK_HDMI_GET_CAPABILITY			HDMI_IOWR(4, HDMI_CAPABILITY)
#define MTK_HDMI_GET_DEVICE_STATUS		HDMI_IOWR(5, hdmi_device_status)
#define MTK_HDMI_VIDEO_CONFIG			HDMI_IOWR(6, unsigned int)
#define MTK_HDMI_AUDIO_CONFIG			HDMI_IOWR(7, int)
#define MTK_HDMI_FORCE_FULLSCREEN_ON	HDMI_IOWR(8, int)
#define MTK_HDMI_FORCE_FULLSCREEN_OFF	HDMI_IOWR(9, int)
#define MTK_HDMI_IPO_POWEROFF	        HDMI_IOWR(10, int)
#define MTK_HDMI_IPO_POWERON	        HDMI_IOWR(11, int)
#define MTK_HDMI_POWER_ENABLE           HDMI_IOW(12, int)
#define MTK_HDMI_PORTRAIT_ENABLE        HDMI_IOW(13, int)
#define MTK_HDMI_READ				HDMI_IOWR(14, unsigned int)
#define MTK_HDMI_WRITE				HDMI_IOWR(15, unsigned int)
#define MTK_HDMI_CMD				HDMI_IOWR(16, unsigned int)
#define MTK_HDMI_DUMP				HDMI_IOWR(17, unsigned int)
#define MTK_HDMI_STATUS			HDMI_IOWR(18, unsigned int)
#define MTK_HDMI_DUMP6397				HDMI_IOWR(19, unsigned int)
#define MTK_HDMI_DUMP6397_W				HDMI_IOWR(20, unsigned int)
#define MTK_HDMI_CBUS_STATUS				HDMI_IOWR(21, unsigned int)
#define MTK_HDMI_HDCP				HDMI_IOWR(22, unsigned int)


enum HDMI_report_state {
	NO_DEVICE = 0,
	HDMI_PLUGIN = 1,
};

typedef struct {
	unsigned int u4Addr;
	unsigned int u4Data;
} RW_VALUE;
typedef struct {
	unsigned int u4Cmd;
	unsigned int u4Para;
	unsigned int u4Para1;
	unsigned int u4Para2;
} stMhlCmd_st;


#endif
