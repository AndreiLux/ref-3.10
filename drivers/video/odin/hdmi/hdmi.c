/*
 * linux/drivers/video/odin/hdmi/hdmi.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/hardirq.h>
 #include <linux/switch.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <video/odindss.h>
#include <linux/mm.h>
#include <linux/clk.h>

#include <sound/hdmi_alink.h>

#include "../hdmi_api_tx/api/api.h"
#include "../hdmi_api_tx/core/videoParams.h"
#include "../hdmi_api_tx/core/audioParams.h"
#include "../hdmi_api_tx/core/productParams.h"
#include "../hdmi_api_tx/hdcp/hdcpParams.h"
#include "../hdmi_api_tx/bsp/access.h"

#include "../dss/dss.h"
#include "../dss/dipc.h"
#include "../display/panel_device.h"
#include "hdmi.h"
#include "hdmi_debug.h"

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
#include "../display/hdmi-panel.h"
#else
#include "hdmi_panel.h"
#endif
#include "compliance.h"
#include "../hdmi_api_tx/edid/edid.h"


#include <linux/slimport.h>
/* Add for regulator hdmi power control */
#include <linux/regulator/consumer.h>


#define SVD_AUTO_MODE	0


int g_hdmi_debug;
module_param(g_hdmi_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug level - higher value produces more verbose messages");

/* Add for changing resolution */
int aftmode_resolution;

static int hdcp_onoff_status=0;
static unchar g_edid_read_cntr;

struct hdmi_app_work {
	struct work_struct work;
	int status;
};

struct hdmi_device{
	struct platform_device *pdev;
	struct resource *iomem;
	void __iomem    *base;

	struct work_struct work;

	int irq_num;

	struct mutex lock;

	spinlock_t slock;

	struct hdmi_app_work hdmi_work;

	struct workqueue_struct *hdmi_wq;


	struct hdmi_audio_driver audio_adriver;
	audioParams_t	*paudioparms;

    struct switch_dev sdev;
    struct switch_dev saudiodev;

};

static struct hdmi_device *hdmi_dev;

//void hdmi_app(struct work_struct *work);
static void hpdcallback(void *param);

struct switch_dev switch_hdmi_detection = {
	    .name = "hdmi",
};

struct clk *hdmi_clk;
int hdmi_clock_setting(int clock)
{
	int r = 0;


	switch (clock)
	{
		case 2520:
			r = clk_set_rate(hdmi_clk, 25200000);
			hdmi_debug(LOG_DEBUG, "Clock setting 25.2Mhz\n");
			break;

		case 2700:
			r = clk_set_rate(hdmi_clk, 27000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 27Mhz\n");
			break;

		case 5400:
			r = clk_set_rate(hdmi_clk, 54000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 54Mhz\n");
			break;

		case 6500:
			r = clk_set_rate(hdmi_clk, 65000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 65Mhz\n");
			break;

		case 7200:
			r = clk_set_rate(hdmi_clk, 72000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 72Mhz\n");
			break;

		case 7425:
			r = clk_set_rate(hdmi_clk, 74250000);
			hdmi_debug(LOG_DEBUG, "Clock setting 74.25Mhz\n");
			break;

		case 10800:
			r = clk_set_rate(hdmi_clk, 108000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 108Mhz\n");
			break;

		case 13000:
			r = clk_set_rate(hdmi_clk, 130000000);
			hdmi_debug(LOG_DEBUG, "Clock setting 130Mhz\n");
			break;

		case 14850:
			r = clk_set_rate(hdmi_clk, 148500000);
			hdmi_debug(LOG_DEBUG, "Clock setting 148.5Mhz\n");
			break;

		default:
			hdmi_err("Clock setting Error = %d\n", clock);
			r = -1;
	}
	odin_crg_clk_update(4);		/* 4: hdmi update*/

	mdelay(1);

	return r;
}

int hdmi_cea_clock_setting(u8 cea_code)
{
	int r = 0;

	hdmi_debug(LOG_DEBUG, "CEA Setting %d\n", cea_code);

	switch (cea_code)
	{
		/* 25.2Mhz */
		case 1:					/* ( 640x480p @ 59.94/60 Hz) */
			r = hdmi_clock_setting(2520);
			break;

		/* 27Mhz */
		case 2:
		case 3:
		case 7:
		case 8:
		case 9:
		case 17:
		case 18:
		case 21:
		case 22:
		case 23:
		case 24:
			r = hdmi_clock_setting(2700);
			break;

		/* 54Mhz */
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			r = hdmi_clock_setting(5400);
			break;


		/* 72Mhz */
		case 39:
			r = hdmi_clock_setting(7200);
			break;

		/* 74.25Mhz */
		case 4:
		case 5:
		case 19:
		case 20:
		case 32:
		case 33:
		case 34:
			r = hdmi_clock_setting(7425);
			break;

		/* 108Mhz */
		case 35:
		case 36:
		case 37:
		case 38:
		case 52:
		case 53:
		case 54:
		case 55:
			r = hdmi_clock_setting(10800);
			break;

		/* 148.5Mhz */
		case 16:
		case 31:
			r = hdmi_clock_setting(14850);
			break;

		default:
			hdmi_err("Cea setting  Error = %d\n", cea_code);
			r = -1;
			break;
	}

	return r;
}

extern void odinfb_fb2dss_timings(struct fb_videomode *fb_timings,
			struct odin_video_timings *dss_timings);
extern void odinfb_dss2fb_timings(struct odin_video_timings *dss_timings,
			struct fb_videomode *fb_timings);

int odin_hdmi_display_check_timing(struct odin_dss_device *dssdev,
					struct odin_video_timings *timings)
{

	return 0;
}

int odin_hdmi_display_set_mode(struct odin_dss_device *dssdev,
	struct fb_videomode *vm)
{

	return 0;
}
EXPORT_SYMBOL(odin_hdmi_display_set_mode);

void odin_hdmi_display_set_timing(struct odin_dss_device *dssdev)
{
}
EXPORT_SYMBOL(odin_hdmi_display_set_timing);

int odin_hdmi_display_enable(struct odin_dss_device *dssdev)
{
	return 0;
}
EXPORT_SYMBOL(odin_hdmi_display_enable);

void odin_hdmi_display_disable(struct odin_dss_device *dssdev)
{
}
EXPORT_SYMBOL(odin_hdmi_display_disable);


#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_hdmi_match[] = {
	{
		.compatible = "odindss-hdmi",
	},
	{},
};
#endif

/***********************************************
 * globals
 **********************************************/
static int  hpdconnected = FALSE;
static int  ediddone = FALSE;

static int compliance_mode = 0;

/***********************************************
 * callbacks
 **********************************************/

enum {
	PANEL_LGHDK,
};

struct hdmi_panel_config panel_configs;

static void compliance_start(int svd_num)
{
	dtd_t 	dtd_desc;
	bool 	hdmi_mode=true;
	int 	ret;
#if 0  /* svd & dtd list view */

	int i;

	i = compliance_show_dtd_list();
	compliance_show_svd_list(i);

	compliance_show_sad();

#endif

	 mutex_lock(&hdmi_dev->lock);

	if(svd_num == SVD_AUTO_MODE)
	{
		hdmi_debug(LOG_DEBUG, "SVD Auto mode \n");
		compliance_svd_select();

	}
	else
	{
		hdmi_debug(LOG_DEBUG, "SVD Manual Mode  : %d\n", svd_num);
		compliance_set_currentmode(svd_num);

	}

	if (get_dtd(&dtd_desc) != 0)
	{
		hdmi_err("fail to get dtd!\n");
		mutex_unlock(&hdmi_dev->lock);
		return;
	}

	if (is_dtd())
	{
		hdmi_debug(LOG_INFO, "enter dtd clock:%d\n",get_pclock());
		hdmi_mode = false;
		if (hdmi_clock_setting(get_pclock()) != 0)
		{
			hdmi_debug(LOG_DEBUG, "fail to setting clock! ==> VGA MODE SET\n");
			dtd_fill(&dtd_desc, 1, 60000);
			hdmi_clock_setting(2520);
		}
	}
	else
	{
		hdmi_debug(LOG_INFO, "enter svd:%d\n", get_ceacode());
		hdmi_mode = true;
		if (hdmi_cea_clock_setting(get_ceacode()) != 0)
		{
			hdmi_err("fail to setting cea clock!\n");
			mutex_unlock(&hdmi_dev->lock);
			return;
		}
	}


	panel_configs.timings.mPixelRepetitionInput =
		dtd_desc.mpixelrepetitioninput;
	panel_configs.timings.mPixelClock = dtd_desc.mpixelclock;
	panel_configs.timings.mInterlaced = dtd_desc.minterlaced;
	panel_configs.timings.mHActive = dtd_desc.mhactive;
	panel_configs.timings.mHBlanking = dtd_desc.mhblanking;
	panel_configs.timings.mHBorder = dtd_desc.mhborder;
	panel_configs.timings.mHImageSize = dtd_desc.mhimagesize;
	panel_configs.timings.mHSyncOffset = dtd_desc.mhsyncoffset;
	panel_configs.timings.mHSyncPulseWidth = dtd_desc.mhsyncpulsewidth;
	panel_configs.timings.mHSyncPolarity = dtd_desc.mhsyncpolarity;
	panel_configs.timings.mVActive = dtd_desc.mvactive;
	panel_configs.timings.mVBlanking = dtd_desc.mvblanking;
	panel_configs.timings.mVBorder = dtd_desc.mvborder;
	panel_configs.timings.mVImageSize = dtd_desc.mvimagesize;
	panel_configs.timings.mVSyncOffset = dtd_desc.mvsyncoffset;
	panel_configs.timings.mVSyncPulseWidth = dtd_desc.mvsyncpulsewidth;
	panel_configs.timings.mVSyncPolarity = dtd_desc.mvsyncpolarity;

	panel_configs.timings.data_width = 4;

	panel_configs.timings.x_res = panel_configs.timings.mHActive;
	panel_configs.timings.y_res = panel_configs.timings.mVActive;
	panel_configs.timings.hfp = panel_configs.timings.mHSyncOffset;
	panel_configs.timings.vfp = panel_configs.timings.mVSyncOffset;
	panel_configs.timings.hbp = panel_configs.timings.mHBlanking -
				panel_configs.timings.hfp - panel_configs.timings.mHSyncPulseWidth;
	panel_configs.timings.vbp = panel_configs.timings.mVBlanking -
				panel_configs.timings.vfp - panel_configs.timings.mVSyncPulseWidth;


	ret = compliance_configure(1);
	if(ret != TRUE)
	{
		mutex_unlock(&hdmi_dev->lock);
		return;
	}

	 mutex_unlock(&hdmi_dev->lock);

 #if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
 	hdmi_panel_power_on(&panel_configs, hdmi_mode);
 #else
 	odin_hdmi_power_on(&panel_configs, hdmi_mode);
 #endif

}



/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t hdmi_hdcp_feature_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n",hdcp_onoff_status );
}

static ssize_t hdmi_hdcp_feature_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);

	if(val == TRUE)
		api_hdcpbypassencryption(FALSE);
	else if(val == FALSE)
		api_hdcpbypassencryption(TRUE);
	else
	{
		hdmi_err("hdcp setting error\n");
		return ret;
	}

	hdcp_onoff_status = val;

	if (ret)
		return ret;

	return count;
}
static ssize_t hdmi_debug_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", g_hdmi_debug);
}

static ssize_t hdmi_debug_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);

	g_hdmi_debug = val;

	return count;
}
/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t hdmi_svd_sel_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", compliance_get_currentmode());
}

static ssize_t hdmi_svd_sel_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;

 	hdmi_power_off(false);

	mdelay(30);


	compliance_mode = val;

	hdmi_panel_power_on_manual();

	while(hdmi_dev->hdmi_work.status != API_INIT)
	{
		hdmi_debug(LOG_INFO, "manual api init wait\n");
		mdelay(100);
	}
	compliance_start(compliance_mode);

	return count;
}

static ssize_t hdmi_svd_list(char **buf)
{

	u8 svdno, dtdno, ceacode, j;
	int i=0;
	shortVideoDesc_t tmp_svd;
	dtd_t 	dtd_desc;
	char *temp=NULL;
	char svd_line[70]={0,};

	temp = *buf;

	dtdno = api_ediddtdcount();


	for (j = 0; j < dtdno; j++)
	{
		api_ediddtd(i, &dtd_desc);

		sprintf(svd_line, "index=%d \t mcode=%d \t mpixelclock=%d \t w:h=%d:%d\n",
				i++, dtd_desc.mcode, dtd_desc.mpixelclock, dtd_desc.mhactive,
				dtd_desc.mvactive);
		strcat(temp, svd_line);

	}


	svdno = api_edidsvdcount();

	for (j = 0; j < svdno; j++)
	{
		api_edidsvd(j, &tmp_svd);
		ceacode = shortvideodesc_getcode(&tmp_svd);
		dtd_fill(&dtd_desc, ceacode, board_supportedrefreshrate(ceacode));

		sprintf(svd_line, "index=%d \t ceacode=%d \t mpixelclock=%d \t w:h=%d:%d\n",
				i++, ceacode, dtd_desc.mpixelclock, dtd_desc.mhactive,
				dtd_desc.mvactive);
		strcat(temp, svd_line);

	}

	return i;

}
/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t hdmi_svd_list_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	hdmi_svd_list(&buf);
	return strlen(buf);
}

/*sysfs interface : Get resolution info through system property*/
static ssize_t hdmi_aft_mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int ret;
	ret = aftmode_resolution;
	return sprintf(buf, "%d", ret);
}

static ssize_t hdmi_aft_mode_store(struct device *dev, struct device_attribute *attr,
		char *buf, size_t count)
{
	int ret;
	long val;

	ret = strict_strtol(buf, 10, &val);

	if (!ret)
		aftmode_resolution = val;
	else
		aftmode_resolution = 0;

	return count;
}

static struct device_attribute hdmi_device_attrs[] = {
	__ATTR(debug, S_IRUGO | S_IWUSR, hdmi_debug_show, hdmi_debug_store),
	__ATTR(hdcp, S_IRUGO | S_IWUSR, hdmi_hdcp_feature_show, hdmi_hdcp_feature_store),
	__ATTR(svdlist, S_IRUGO, hdmi_svd_list_show, NULL),
	__ATTR(svd, S_IRUGO | S_IWUSR, hdmi_svd_sel_show, hdmi_svd_sel_store),
	__ATTR(aftmode, S_IRUGO | S_IWUSR, hdmi_aft_mode_show, hdmi_aft_mode_store),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hdmi_device_attrs); i++)
		if (device_create_file(dev, &hdmi_device_attrs[i]))
			goto error;
	return 0;
error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, &hdmi_device_attrs[i]);

	hdmi_err("Unable to create interface\n");
	return -EINVAL;
}

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
static void edidcallback(void *param)
{
	hdmi_debug(LOG_DEBUG, "edidcall back\n");
	hdmi_dev->hdmi_work.status = API_EDID_READ;
	queue_work(hdmi_dev->hdmi_wq, &hdmi_dev->hdmi_work.work);

}
#endif

static void hpdcallback(void *param)
{
	u8 hpd = *((u8*)(param));
	hpdconnected = hpd;

	if (hpd != TRUE)
	{
		hdmi_debug(LOG_DEBUG, "HPD LOST\n");
		hdmi_panel_power_off();
	}
	else
	{
		hdmi_debug(LOG_DEBUG, "HPD ASSERTED\n");
		queue_work(hdmi_dev->hdmi_wq, &hdmi_dev->hdmi_work.work);
	}
}

int hdmi_start(u8 fixed_color_screen)
{
	hdmi_api_enter();

	g_edid_read_cntr = 0;
	hdmi_dev->hdmi_work.status = API_NOT_INIT;

	compliance_init();
	api_eventenable(HPD_EVENT, hpdcallback, FALSE);
	if (odin_crg_register_isr(
		(odin_crg_isr_t)api_eventhandler, NULL, CRG_IRQ_HDMI, 0) != 0)
	{
		hdmi_err("api eventenable failed \n");
	}

	if(hdmi_dev->hdmi_work.status == API_NOT_INIT)
	{
		hdmi_dev->hdmi_work.status = API_INIT;
		if (!api_initialize(0, 1, 2500, 1))
		{
			hdmi_err("API initialize %d\n", error_get());
			hdmi_dev->hdmi_work.status = API_NOT_INIT;
			if(fixed_color_screen != 1) /* not manual mode */
				return -1;
		}
		api_avmute(false);
		api_audiomute(false);
	}

	hdmi_api_leave();
	return 0;
}


void hdmi_proc(struct work_struct *work)
{
	int ret;
	struct hdmi_app_work *wk = container_of(work, typeof(*wk), work);

	hdmi_debug(LOG_DEBUG, "%s, status = %x\n", __func__, wk->status);
#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
	if (!api_edidread(edidcallback))
	{
		hdmi_err("cannot read E-EDID %d\n", error_get());
	}
#endif

	if(wk->status == API_INIT)
	{
		api_read_edid();
		ret = compliance_total_count();
		if(ret <= 0)
		{
			if(g_edid_read_cntr < 3)
			{
				g_edid_read_cntr++;
				queue_work(hdmi_dev->hdmi_wq, &hdmi_dev->hdmi_work.work);
				return;
			}
			else
				return;
		}
		wk->status = API_EDID_READ;
	}


	if(wk->status == API_EDID_READ)
	{
		wk->status = API_CONFIGURED;
		compliance_start(SVD_AUTO_MODE);
	}


}

static int hdmi_inner_audio_open(void)
{
	dev_dbg(&(hdmi_dev->pdev->dev), "hdmi_inner_audio_open\n");

	hdmi_dev->paudioparms = kzalloc(sizeof(audioParams_t),GFP_KERNEL);

	if (!hdmi_dev->paudioparms) {
		dev_err(&(hdmi_dev->pdev->dev), "Failed to alloc paud param\n");
		return -ENOMEM;
	}

	return 0;
}

static int hdmi_inner_audio_set_params(struct hdmi_audio_data_conf *dataconf)
{
	dev_dbg(&(hdmi_dev->pdev->dev), "hdmi_inner_audio_set_params\n");

	if (!hdmi_dev->paudioparms) {
		dev_err(&(hdmi_dev->pdev->dev),
				"error set_params - audioparm is NULL\n");
		return -ENXIO;
	}

	audioparams_setinterfacetype(hdmi_dev->paudioparms ,I2S);
	audioparams_setcodingtype(hdmi_dev->paudioparms ,PCM);
	audioparams_setpackettype(hdmi_dev->paudioparms ,AUDIO_SAMPLE);

	hdmi_dev->paudioparms->mieccopyright = 1;

	hdmi_dev->paudioparms->mieccgmsa = 3;
	hdmi_dev->paudioparms->miiecpcmmode = 0;

	hdmi_dev->paudioparms->mieccgmsa = 0;
	hdmi_dev->paudioparms->miiecpcmmode = 1;

	hdmi_dev->paudioparms->mieccategorycode = 0xaa;
	hdmi_dev->paudioparms->miecsourcenumber = 1;
	hdmi_dev->paudioparms->miecclockaccuracy = 1;

	audioparams_setsamplingfrequency(hdmi_dev->paudioparms,
							dataconf->sample_rate);
	audioparams_setsamplesize(hdmi_dev->paudioparms, dataconf->chn_width);
	audioparams_setclockfsfactor(hdmi_dev->paudioparms, 256);
	audioparams_setchannelallocation(hdmi_dev->paudioparms,
						dataconf->spk_placement_idx);

	dev_dbg(&(hdmi_dev->pdev->dev), "set sample_rate %d\n",
						dataconf->sample_rate);

	dev_dbg(&(hdmi_dev->pdev->dev), "set chn_width %d\n",
						dataconf->chn_width);

	dev_dbg(&(hdmi_dev->pdev->dev), "set speaker index %d\n",
						dataconf->spk_placement_idx);

	return 0;
}

static int hdmi_inner_audio_set_fmt(unsigned int fmt)
{
	dev_dbg(&(hdmi_dev->pdev->dev), "hdmi_inner_audio_set_fmt\n");

	if (!hdmi_dev->paudioparms) {
		dev_err(&(hdmi_dev->pdev->dev),
				"error set_fmt - audioparm is NULL\n");
		return -ENXIO;
	}

	return 0;
}

static int hdmi_inner_audio_operation(int cmd)
{
	if (!hdmi_dev->paudioparms) {
		dev_err(&(hdmi_dev->pdev->dev),
				"error operation - audioparm is NULL\n");
		return -ENXIO;
	}

	switch (cmd) {
	case HDMI_AUDIO_CMD_START:
		compliance_audio_params_set((audioParams_t *)hdmi_dev->paudioparms);
		dev_dbg(&(hdmi_dev->pdev->dev), "CMD HDMI_AUDIO_CMD_START\n");
		break;
	case HDMI_AUDIO_CMD_STOP:
		dev_dbg(&(hdmi_dev->pdev->dev), "CMD HDMI_AUDIO_CMD_STOP\n");
		break;
	default:
		dev_err(&(hdmi_dev->pdev->dev), "No parse cmd 0x%x\n",cmd);
		break;
	}
	return 0;
}

static int hdmi_inner_audio_close(void)
{
	dev_dbg(&(hdmi_dev->pdev->dev), "hdmi_inner_audio_close\n");
	kfree(hdmi_dev->paudioparms);
	hdmi_dev->paudioparms = NULL;
	return 0;
}

static int hdmi_inner_audio_hw_switch(int onoff)
{
	return 0;
}

static int hdmi_inner_audio_get_sad_byte_cnt(void)
{
	return get_sad_count();
}

static int hdmi_inner_audio_get_sad_bytes(unsigned char *sadbuf, int cnt)
{
	return get_edid_sad(sadbuf, cnt);
}

static int hdmi_inner_audio_get_get_spk_allocation(unsigned char *spk_alloc)
{
	return get_edid_spk_alloc_data(spk_alloc);
}

static int hdmi_audio_driver_init(struct platform_device *pdev,
						struct hdmi_device *hdmi_device)
{
	memset(&hdmi_device->audio_adriver, 0x0,
					sizeof(struct hdmi_audio_driver));
	hdmi_device->paudioparms = NULL;

	hdmi_device->audio_adriver.open = hdmi_inner_audio_open;
	hdmi_device->audio_adriver.set_params = hdmi_inner_audio_set_params;
	hdmi_device->audio_adriver.set_fmt = hdmi_inner_audio_set_fmt;
	hdmi_device->audio_adriver.operation = hdmi_inner_audio_operation;
	hdmi_device->audio_adriver.close = hdmi_inner_audio_close;
	hdmi_device->audio_adriver.hw_switch = hdmi_inner_audio_hw_switch;
	hdmi_device->audio_adriver.get_sad_byte_cnt =
					hdmi_inner_audio_get_sad_byte_cnt;
	hdmi_device->audio_adriver.get_sad_bytes =
					hdmi_inner_audio_get_sad_bytes;
	hdmi_device->audio_adriver.get_spk_allocation =
					hdmi_inner_audio_get_get_spk_allocation;

	return register_hdmi_alink_drv(&pdev->dev, &hdmi_device->audio_adriver);
}

static int hdmi_audio_driver_deinit(struct platform_device *pdev)
{
	deregister_hdmi_alink_drv(&pdev->dev);
	return 0;
}

extern void phy_poweron_standby(u16 baseAddr);
/* DISPC HW IP initialisation */
static int hdmi_probe(struct platform_device *pdev)
{
#ifndef CONFIG_OF
	struct resource *res;
	resource_size_t size;
#endif

	struct hdmi_device *dev;
	int ret;
#ifdef CONFIG_OF
	/*const struct of_device_id *of_id;*/
	/*of_id = of_match_device(odindss_hdmi_match, &pdev->dev);*/
	pdev->dev.parent = &odin_device_parent;
#endif

	dev = devm_kzalloc(&pdev->dev, sizeof(struct hdmi_device), GFP_KERNEL);

	if (!dev) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		return ret;
	}



#ifdef CONFIG_OF
	/* irq get later */
	dev->base = of_iomap(pdev->dev.of_node, 0);

	if (dev->base == NULL) {
		ret = -ENXIO;
		goto free_memory_region;
	}
#else
	dev->irq_num = platform_get_irq(pdev, 0);
	if (dev->irq_num < 0) {
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL ) {
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENXIO;
	}

	size = resource_size(res);
	dev->iomem = request_mem_region(res->start, size,
			(const char*)pdev->name);
	if (dev->iomem == NULL) {
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENOENT;
	}

	dev->base = ioremap_nocache(res->start, size);
	if (dev->base == NULL) {
		ret = -ENXIO;
		goto free_memory_region;
	}

	/*Check if region is already locked by any other driver ? */
	if (check_mem_region ((u32)dev->base, size))
	{
		dev_err(&pdev->dev, "Memory Already Locked !!\n");
		iounmap (dev->base);
		return -EBUSY;
	}
#endif

	hdmi_dev = dev;

	platform_set_drvdata(pdev, hdmi_dev);

	ret = create_sysfs_interfaces(&pdev->dev);
	if (ret < 0) {
		hdmi_err("sysfs register failed\n");
		goto free_memory_region;
	}

	hdmi_dev->pdev = pdev;
	/* hdmi tx control */

	mutex_init(&hdmi_dev->lock);
	spin_lock_init(&hdmi_dev->slock);

	ret = hdmi_audio_driver_init(pdev, hdmi_dev);
	if (ret < 0) {
		dev_warn(&pdev->dev, "Failed to init audio driver %d\n", ret);
	}

	hdmi_clk = clk_get(NULL, "hdmi_disp_clk");

	access_initialize((u8*)hdmi_dev->base);
	/* hdmi_debug("register address setup %x\n",(u8 *)hdmi_dev->base); */
#if 1
	odin_crg_dss_channel_clk_enable(ODIN_DSS_CHANNEL_HDMI, true);
	phy_poweron_standby(0);
	odin_crg_dss_channel_clk_enable(ODIN_DSS_CHANNEL_HDMI, false);
#endif

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
	hdmi_debug(LOG_INFO, "HDMI driver for slimport_anx7808 probe!\n");
	hdmi_dev->hdmi_wq = create_singlethread_workqueue("hdmi_app");

	INIT_WORK(&hdmi_dev->hdmi_work.work, hdmi_proc);
#else
	hdmi_debug(LOG_INFO, "HDMI driver for any panel!\n");
	compliance_start(0);
#endif
	return 0;

free_memory_region:
#ifndef CONFIG_OF
	release_mem_region(res->start, size);
#endif

	return ret;
}

static int hdmi_remove(struct platform_device *pdev)
{
	int i=0;
	for (i = 0; i < ARRAY_SIZE(hdmi_device_attrs); i++)
		device_remove_file(&pdev->dev, &hdmi_device_attrs[i]);

	hdmi_audio_driver_deinit(pdev);

	destroy_workqueue(hdmi_dev->hdmi_wq);

	/* kfree(hdmi_dev); */

	return 0;
}


static int hdmi_runtime_suspend(struct device *dev)
{
	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{

	return 0;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
};

static struct platform_driver odindss_hdmi_driver = {
	.probe          = hdmi_probe,
	.remove         = hdmi_remove,
	.driver         = {
		.name   = "odindss-hdmi",
		.owner  = THIS_MODULE,
		.pm	= &hdmi_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_hdmi_match),
#endif

	},
};

int odin_hdmi_init_platform_driver(void)
{
	return platform_driver_register(&odindss_hdmi_driver);
}

void odin_hdmi_uninit_platform_driver(void)
{
	platform_driver_unregister(&odindss_hdmi_driver);
}

