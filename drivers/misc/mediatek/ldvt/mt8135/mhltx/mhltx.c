/*****************************************************************************/
/* Copyright (c) 2009 NXP Semiconductors BV                                  */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, using version 2 of the License.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307       */
/* USA.                                                                      */
/*                                                                           */
/*****************************************************************************/
/* #if defined(MTK_HDMI_SUPPORT) */
#if 1
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

#include "mach/eint.h"
#include "mach/irqs.h"

#include "mhltx.h"
#include "hdmi_drv.h"
#include "mhl_cbus.h"
#include "../../pmic_wrap/reg_pmic_wrap.h"
#include <mach/mt_pmic_wrap.h>

#define OUTREG32(x, y) {/*printk("[hdmi]write 0x%08x to 0x%08x\n", (y), (x)); */__OUTREG32((x), (y))}
#define __OUTREG32(x, y) {*(unsigned int*)(x) = (y); }

static size_t hdmi_log_on = true;
static struct switch_dev hdmi_switch_data;

static HDMI_DRIVER *hdmi_drv;
stMhlCmd_st stMhlCmd;


extern void vWriteCbus(unsigned short dAddr, unsigned int dVal);
extern unsigned int u4ReadCbus(unsigned int dAddr);

/* Configure video attribute */
static int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, HDMI_VIDEO_INPUT_FORMAT vin,
			     HDMI_VIDEO_OUTPUT_FORMAT vout)
{

	return hdmi_drv->video_config(vformat, vin, vout);
}

/* Configure audio attribute, will be called by audio driver */
int hdmi_audio_config(int samplerate)
{
	HDMI_AUDIO_FORMAT aud_fmt;


	/* HDMI_LOG("sample rate=%d\n", samplerate); */
	if (samplerate == 48000) {
		aud_fmt = HDMI_AUDIO_PCM_16bit_48000;
	} else if (samplerate == 44100) {
		aud_fmt = HDMI_AUDIO_PCM_16bit_44100;
	} else if (samplerate == 32000) {
		aud_fmt = HDMI_AUDIO_PCM_16bit_32000;
	} else {
		/* HDMI_LOG("samplerate not support:%d\n", samplerate); */
	}


	hdmi_drv->audio_config(aud_fmt);

	return 0;
}


static int hdmi_release(struct inode *inode, struct file *file)
{
	printk("[hdmi]%s\n", __func__);
	return 0;
}

static int hdmi_open(struct inode *inode, struct file *file)
{
	printk("[hdmi]%s\n", __func__);
	return 0;
}

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int tmp;
	RW_VALUE stSpi;
	int r = 0;
	unsigned int addr, u4data;


	switch (cmd) {
	case MTK_HDMI_VIDEO_CONFIG:
		{
			printk("MTK_HDMI_VIDEO_CONFIG\n");
			/* HDMI_LOG("video resolution configuration, arg=%ld\n", arg); */
			if (copy_from_user(&tmp, (void __user *)arg, sizeof(unsigned int))) {
				r = -EFAULT;
			} else {
				printk("vid : %d\n", tmp);
				/* dpi1 coding here */

				/* mhl */
				if (fgMhlPowerOn)
					hdmi_video_config(tmp, HDMI_VIN_FORMAT_RGB888,
							  HDMI_VOUT_FORMAT_RGB888);
			}

			break;
		}
	case MTK_HDMI_IPO_POWERON:
		{
			printk("MTK_HDMI_IPO_POWERON\n");
			fgMhlPowerOn = TRUE;
			hdmi_drv->power_on();
			break;
		}
	case MTK_HDMI_IPO_POWEROFF:
		{
			printk("MTK_HDMI_IPO_POWEROFF\n");
			fgMhlPowerOn = FALSE;
			hdmi_drv->power_off();
			break;
		}
	case MTK_HDMI_AUDIO_CONFIG:
		{

			break;
		}
	case MTK_HDMI_READ:
		{
			printk("MTK_HDMI_READ\n");
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE))) {
				r = -EFAULT;
			} else {
				hdmi_drv->read(stSpi.u4Addr, stSpi.u4Data);
			}
			break;
		}
	case MTK_HDMI_WRITE:
		{
			printk("MTK_HDMI_WRITE\n");
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE))) {
				r = -EFAULT;
			} else {
				hdmi_drv->write(stSpi.u4Addr, stSpi.u4Data);
			}
			break;
		}
	case MTK_HDMI_DUMP:
		{
			printk("MTK_HDMI_DUMP\n");
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE))) {
				r = -EFAULT;
			} else {
			}
			break;
		}
	case MTK_HDMI_DUMP6397:
		{
			hdmi_drv->dump6397();
			break;
		}
	case MTK_HDMI_DUMP6397_W:
		{
			printk("MTK_HDMI_DUMP6397_W\n");
			if (copy_from_user(&stSpi, (void __user *)arg, sizeof(RW_VALUE))) {
				r = -EFAULT;
			} else {
				hdmi_drv->write6397(stSpi.u4Addr, stSpi.u4Data);
			}
			break;
		}
	case MTK_HDMI_CBUS_STATUS:
		{
			printk("MTK_HDMI_STATUS,fgMhlPowerOn=%d\n", fgMhlPowerOn);
			hdmi_drv->cbusstatus();
			break;
		}
	case MTK_HDMI_CMD:
		{
			printk("MTK_HDMI_CMD\n");
			if (copy_from_user(&stMhlCmd, (void __user *)arg, sizeof(stMhlCmd_st))) {
				printk("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			printk("[MHL]cmd=%x%x%x%x\n", stMhlCmd.u4Cmd, stMhlCmd.u4Para,
			       stMhlCmd.u4Para1, stMhlCmd.u4Para2);
			hdmi_drv->mhl_cmd(stMhlCmd.u4Cmd, stMhlCmd.u4Para, stMhlCmd.u4Para1,
					  stMhlCmd.u4Para2);
			break;
		}
	case MTK_HDMI_HDCP:
		{
			if (copy_from_user(&tmp, (void __user *)arg, sizeof(unsigned int))) {
				printk("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_drv->enablehdcp(tmp);
			}
			break;
		}
	default:
		break;
	}

	return r;
}

static int hdmi_remove(struct platform_device *pdev)
{
	return 0;
}

#define MHL_DEVNAME "mhltx"

struct file_operations hdmi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hdmi_ioctl,
	.open = hdmi_open,
};

static struct miscdevice ts_mhl_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MHL_DEVNAME,
	.fops = &hdmi_fops,
};

static int __init hdmi_init(void)
{
	int ret;
	ret = misc_register(&ts_mhl_dev);
	if (ret) {
		printk("[mhl]: register driver failed (%d)\n", ret);
		return ret;
	}

	hdmi_drv = (HDMI_DRIVER *) HDMI_GetDriver();
	hdmi_drv->init();

	printk("[mhl]:  initialization\n");
	return 0;
}

/* should never be called */
static void __exit hdmi_exit(void)
{
	int ret;

	ret = misc_deregister(&ts_mhl_dev);
	if (ret) {
		printk("[ts_udvt]: unregister driver failed\n");
	}
}


module_init(hdmi_init);
module_exit(hdmi_exit);
MODULE_AUTHOR("bo.xu <bo.xu@mediatek.com>");
MODULE_DESCRIPTION("MHL Driver");
MODULE_LICENSE("GPL");

#endif
