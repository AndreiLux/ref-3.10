#ifndef _MHL_DBG_H_
#define _MHL_DBG_H_

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


extern size_t mhl_log_on;

#define hdmiplllog         (0x1)
#define hdmidgilog         (0x2)
#define hdmitxhotpluglog   (0x4)
#define hdmitxvideolog     (0x8)
#define hdmitxaudiolog     (0x10)
#define hdmihdcplog        (0x20)
#define hdmiceclog         (0x40)
#define hdmiddclog         (0x80)
#define hdmiedidlog        (0x100)
#define hdmidrvlog         (0x200)
#define hdmicbuslog         (0x400)

#define hdmialllog   (0xffff)

/* ////////////////////////////////////////////PLL////////////////////////////////////////////////////// */
#define MHL_PLL_LOG(fmt, arg...) \
	do { \
		if (mhl_log_on&hdmiplllog) {printk("[mhl_pll]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
	} while (0)

#define MHL_PLL_FUNC()	\
	do { \
		if (mhl_log_on&hdmiplllog) {printk("[mhl_pll] %s\n", __func__); } \
	} while (0)

/* ////////////////////////////////////////////PLUG////////////////////////////////////////////////////// */

#define MHL_PLUG_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxhotpluglog) {printk("[mhl_plug]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_PLUG_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxhotpluglog) {printk("[mhl_plug] %s\n", __func__); } \
    } while (0)


/* //////////////////////////////////////////////VIDEO//////////////////////////////////////////////////// */

#define MHL_VIDEO_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxvideolog) {printk("[mhl_video]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_VIDEO_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxvideolog) {printk("[mhl_video] %s\n", __func__); } \
    } while (0)

/* //////////////////////////////////////////////AUDIO//////////////////////////////////////////////////// */

#define MHL_AUDIO_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmitxaudiolog) {printk("[mhl_audio]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_AUDIO_FUNC()	\
    do { \
        if (mhl_log_on&hdmitxaudiolog) {printk("[mhl_audio] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////HDCP/////////////////////////////////////////////////// */

#define MHL_HDCP_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmihdcplog) {printk("[mhl_hdcp]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_HDCP_FUNC()	\
    do { \
        if (mhl_log_on&hdmihdcplog) {printk("[mhl_hdcp] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////EDID////////////////////////////////////////////////// */
#define MHL_EDID_LOG(fmt, arg...) \
	do { \
		if (mhl_log_on&hdmiedidlog) {printk("[mhl_edid]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
	} while (0)

#define MHL_EDID_FUNC()	\
	do { \
		if (mhl_log_on&hdmiedidlog) {printk("[mhl_edid] %s\n", __func__); } \
	} while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */

#define MHL_DRV_LOG(fmt, arg...) \
    do { \
        if (mhl_log_on&hdmidrvlog) {printk("[mhl_drv]%s,%d ", __func__, __LINE__); printk(fmt, ##arg); } \
    } while (0)

#define MHL_DRV_FUNC()	\
    do { \
        if (mhl_log_on&hdmidrvlog) {printk("[mhl_drv] %s\n", __func__); } \
    } while (0)
/* ////////////////////////////////////////////////DRV///////////////////////////////////////////////// */
#define MHL_CBUS_LOG(fmt, arg...) \
    do { \
		if (mhl_log_on&hdmicbuslog) {printk("[cbus] "); printk(fmt, ##arg); } \
    } while (0)

#define MHL_CBUS_FUNC()	\
    do { \
        if (mhl_log_on&hdmicbuslog) {printk("[Cbus] %s\n", __func__); } \
    } while (0)
/* ///////////////////////////////////////////////////////////////////////////////////////////////// */

#define RETNULL(cond)       if ((cond)) {MHL_DRV_LOG("return in %d\n", __LINE__); return; }
#define RETINT(cond, rslt)       if ((cond)) {MHL_DRV_LOG("return in %d\n", __LINE__); return (rslt); }


#endif
