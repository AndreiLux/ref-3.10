#ifndef _MHL_AVD_H_
#define _MHL_AVD_H_

#define TMFL_TDA19989
#define _tx_c_
#include <generated/autoconf.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <mach/dma.h>
#include <mach/irqs.h>
#include <asm/tlbflush.h>
#include <asm/page.h>



#include "hdmi_drv.h"

typedef enum {
	HDMI_SET_VIDEO_RES_CHG = 1,
	HDMI_SET_AUDIO_OUT_CHANNEL,
	HDMI_SET_AUDIO_OUTPUT_TYPE,
	HDMI_SET_AUDIO_PACKET_OFF,
	HDMI_SET_VIDEO_COLOR_SPACE,
	HDMI_SET_VIDEO_CONTRAST,
	HDMI_SET_VIDEO_BRIGHTNESS,
	HDMI_SET_VIDEO_HUE,
	HDMI_SET_VIDEO_SATURATION,
	HDMI_SET_ASPECT_RATIO,
	HDMI_SET_AVD_NFY_FCT,
	HDMI_SET_TURN_OFF_TMDS,
	HDMI_SET_AUDIO_CHG_SETTING,
	HDMI_SET_AVD_INF_ADDRESS,
	HDMI_SET_HDCP_INITIAL_AUTH,
	HDMI_SET_VPLL,
	HDMI_SET_SOFT_NCTS,
	HDMI_SET_HDCP_OFF,
	HDMI_SET_VIDEO_SHARPNESS,
	HDMI_SET_TMDS,
	HDMI_SENT_GCP
} AV_D_HDMI_DRV_SET_TYPE_T;


extern void av_hdmiset(AV_D_HDMI_DRV_SET_TYPE_T e_set_type, const void *pv_set_info,
		       u8 z_set_info_len);

#endif
