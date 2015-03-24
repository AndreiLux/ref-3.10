#ifndef _MHL_CTRL_H_
#define _MHL_CTRL_H_

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
#include "mhl_edid.h"
#include "mhl_table.h"
#include "mhl_dbg.h"

#define MTK_MT8135_MHL_SUPPORT
#define TRUE		(1)
#define FALSE	(0)
typedef unsigned char BYTE;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
/* typedef bool BOOL; */
/* ///////////////////////////////////////////////////////// */
typedef struct _HDMI_AV_INFO_T {
	HDMI_VIDEO_RESOLUTION e_resolution;
	u8 fgHdmiOutEnable;
	u8 u2VerFreq;
	u8 b_hotplug_state;
	HDMI_OUT_COLOR_SPACE_T e_video_color_space;
	u8 ui1_aud_out_ch_number;
	HDMI_AUDIO_SAMPLING_T e_hdmi_fs;
	u8 bhdmiRChstatus[6];
	u8 bhdmiLChstatus[6];
	u8 bMuteHdmiAudio;
	u8 u1HdmiI2sMclk;
	u8 u1hdcponoff;
	u8 u1audiosoft;
	u8 fgHdmiTmdsEnable;
} HDMI_AV_INFO_T;

typedef enum {
	MHL_PLL_27,
	MHL_PLL_74175,
	MHL_PLL_7425,
	MHL_PLL_74175PP,
	MHL_PLL_7425PP,
} MHL_RES_PLL;

extern HDMI_AV_INFO_T _stAvdAVInfo;


extern void vSetCTL0BeZero(u8 fgBeZero);
extern void vHDMIAVUnMute(void);
extern void vHDMIAVMute(void);
extern void vTmdsOnOffAndResetHdcp(u8 fgHdmiTmdsEnable);
extern void vChangeVpll(u8 bRes);
extern void vChgHDMIVideoResolution(u8 ui1resindex, u8 ui1colorspace, u8 ui1hdmifs);
extern void vChgHDMIAudioOutput(u8 ui1hdmifs, u8 ui1resindex);
extern void vChgtoSoftNCTS(u8 ui1resindex, u8 ui1audiosoft, u8 ui1hdmifs);
extern void vSendAVIInfoFrame(u8 ui1resindex, u8 ui1colorspace);
extern void mhl_status(void);
extern u8 bCheckPordHotPlug(void);
extern void vSetHDMITxPLLTrigger(void);
extern void vResetHDMIPLL(void);
extern void vMhlAnalogPD(void);
extern void mhl_InfoframeSetting(u8 i1typemode, u8 i1typeselect);
extern void vMhlInit(void);

#define MHL_INTER_PATTERN_FOR_DBG
#endif
