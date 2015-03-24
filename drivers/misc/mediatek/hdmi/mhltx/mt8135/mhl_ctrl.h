#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT

#ifndef _MHL_CTRL_H_
#define _MHL_CTRL_H_

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <mach/mt_typedefs.h>

#include "mhl_struct.h"
#include "mhl_edid.h"
#include "mhl_table.h"
#include "mhl_dbg.h"

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

/* ///////////////////////////////////////////////////////// */
typedef struct _HDMI_AV_INFO_T {
	HDMI_VIDEO_RESOLUTION e_resolution;
	unsigned char fgHdmiOutEnable;
	unsigned char u2VerFreq;
	unsigned char b_hotplug_state;
	HDMI_OUT_COLOR_SPACE_T e_video_color_space;
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

typedef enum {
	MHL_PLL_27,
	MHL_PLL_74175,
	MHL_PLL_7425,
	MHL_PLL_74175PP,
	MHL_PLL_7425PP,
} MHL_RES_PLL;

extern HDMI_AV_INFO_T _stAvdAVInfo;
extern unsigned char u1MhlConnectState;
extern bool fgCbusStop;
extern bool fgCbusFlowStop;
extern unsigned char u1MhlDpi1Pattern;
extern unsigned char u1ForceCbusStop;
extern unsigned char u1ForceTmdsOutput;
extern void vSetCTL0BeZero(unsigned char fgBeZero);
extern void vHDMIAVUnMute(void);
extern void vHDMIAVMute(void);
extern void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable);
extern void vChangeVpll(unsigned char bRes);
extern void vChgHDMIVideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
				    unsigned char ui1hdmifs);
extern void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex);
extern void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft,
			   unsigned char ui1hdmifs);
extern void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace);
extern void mhl_status(void);
extern unsigned char bCheckPordHotPlug(void);
extern void vSetHDMITxPLLTrigger(void);
extern void vResetHDMIPLL(void);
extern void vMhlAnalogPD(void);
extern void mhl_InfoframeSetting(unsigned char i1typemode, unsigned char i1typeselect);
extern void vMhlInit(void);
extern unsigned char vMhlConnectStatus(void);
extern void vMhlSignalOff(bool fgEn);
void vMhlDigitalPD(void);

/* #define MHL_INTER_PATTERN_FOR_DBG */
/* #define MHL_SUPPORT_SPEC_20 */
#endif
#endif
