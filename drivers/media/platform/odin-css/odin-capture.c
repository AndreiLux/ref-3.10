/*
 * Camera video4linux driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/poll.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>

#include "odin-css.h"
#include "odin-css-utils.h"
#include "odin-css-platdrv.h"
#include "odin-css-clk.h"
#include "odin-css-system.h"
#include "odin-facedetection.h"
#ifdef USE_FPD_HW_IP
#include "odin-facialpartsdetection.h"
#endif
#include "odin-mipicsi.h"
#include "odin-framegrabber.h"
#include "odin-isp.h"
#include "odin-isp-wrap.h"
#include "odin-capture.h"
#include "odin-camera.h"
#include "odin-s3dc.h"
#include "odin-bayercontrol.h"

#ifdef FD_USE_TEST_IMAGE
extern void *fd_image_cpu_addr[FACE_BUF_MAX];;
extern dma_addr_t fd_image_dma_addr[FACE_BUF_MAX];;
#endif

#define CAM_USE_SYSFS_LOG_LEVEL	1
#define CAM_IOREMAP(...) ioremap_nocache(__VA_ARGS__)

struct capture_fh *g_capfh[CSS_MAX_FH] = {NULL, };
struct css_device *g_cssdev = NULL;


#ifdef ODIN_CAMERA_ERR_REPORT
struct work_struct err_report;
unsigned int cur_error = 0;

static void capture_set_error(unsigned int err)
{
	cur_error |= err;
}

static void capture_clr_error(unsigned int err)
{
	cur_error &= ~err;
}

static void capture_err_log_wq(struct work_struct *work)
{
	if (cur_error & ERR_PATH0_BUF_OVERFLOW)	{
		capture_clr_error(ERR_PATH0_BUF_OVERFLOW);
		css_warn("path0-bof\n");
	}
	if (cur_error & ERR_PATH0_ROOP)	{
		capture_clr_error(ERR_PATH0_ROOP);
		css_warn("path0-roop ch0:0x%08x ch2:0x%08x\n",
			odin_mipicsi_get_error(0), odin_mipicsi_get_error(2));
	}
	if (cur_error & ERR_PATH1_BUF_OVERFLOW)	{
		capture_clr_error(ERR_PATH1_BUF_OVERFLOW);
		css_warn("path1-bof\n");
	}
	if (cur_error & ERR_PATH1_ROOP)	{
		capture_clr_error(ERR_PATH1_ROOP);
		css_warn("path1-roop ch0:0x%08x ch2:0x%08x\n",
			odin_mipicsi_get_error(0), odin_mipicsi_get_error(2));
	}
	if (cur_error & ERR_PATH0_STORE_BUF_OVERFLOW) {
		capture_clr_error(ERR_PATH0_STORE_BUF_OVERFLOW);
		css_warn("zsl[0] bof\n");
	}
	if (cur_error & ERR_PATH1_STORE_BUF_OVERFLOW) {
		capture_clr_error(ERR_PATH1_STORE_BUF_OVERFLOW);
		css_warn("zsl[1] bof\n");
	}
	if (cur_error & ERR_FD_BUF_OVERFLOW) {
		capture_clr_error(ERR_FD_BUF_OVERFLOW);
		css_warn("fd-bof\n");
	}
	if (cur_error & ERR_JPEG_STREAM_BUF_OVERFLOW) {
		capture_clr_error(ERR_JPEG_STREAM_BUF_OVERFLOW);
		css_warn("jpeg-bof\n");
	}
	if (cur_error & ERR_JPEG_INTERFACE_ERR) {
		capture_clr_error(ERR_JPEG_INTERFACE_ERR);
		css_warn("jpeg-interface err\n");
	}
	if (cur_error & ERR_ZSL_PATH0_ROOP) {
		capture_clr_error(ERR_ZSL_PATH0_ROOP);
		css_warn("path0-roop(zsl)\n");
	}
	if (cur_error & ERR_ZSL_PATH1_ROOP) {
		capture_clr_error(ERR_ZSL_PATH1_ROOP);
		css_warn("path1-roop(zsl)\n");
	}
	if (cur_error & ERR_ZSL_PATH0_BOF) {
		capture_clr_error(ERR_ZSL_PATH0_BOF);
		css_warn("path0-bof(zsl)\n");
	}
	if (cur_error & ERR_ZSL_PATH1_BOF) {
		capture_clr_error(ERR_ZSL_PATH1_BOF);
		css_warn("path1-bof(zsl)\n");
	}
	if (cur_error & ERR_ZSL_PATH0_ROOTOUT) {
		capture_clr_error(ERR_ZSL_PATH0_ROOTOUT);
		css_warn("path0-roop out\n");
		odin_isp_warp_reg_dump(0);
		odin_isp_warp_reg_dump(1);
		odin_mipicsi_reg_dump();
		odin_isp_warp_reg_dump(0);
		odin_isp_warp_reg_dump(1);
	}
	if (cur_error & ERR_ZSL_PATH1_ROOTOUT) {
		capture_clr_error(ERR_ZSL_PATH1_ROOTOUT);
		css_warn("path1-roop out\n");
		odin_isp_warp_reg_dump(0);
		odin_isp_warp_reg_dump(1);
		odin_mipicsi_reg_dump();
		odin_isp_warp_reg_dump(0);
		odin_isp_warp_reg_dump(1);
	}
}


static void capture_err_log(const char *fmt, ...)
{
	fmt = fmt;
	schedule_work(&err_report);
}
#define css_int_err(fmt, ...) capture_err_log(fmt, ##__VA_ARGS__)
#else
static void capture_set_error(unsigned int err)
{
	err = err;
	return;
}

static void capture_clr_error(unsigned int err)
{
	err = err;
	return;
}
#define css_int_err(fmt, ...) css_warn(fmt, ##__VA_ARGS__)
#endif

struct timer_list cam_report_timer;
struct work_struct reportwq;
struct mutex report_mutex;

static unsigned int css_suspend = 0;
unsigned int report_init = 0;
unsigned int video0_captcnt = 0;
unsigned int video0_dropcnt = 0;
unsigned int video0_skipcnt = 0;
unsigned int video0_zslcnt	= 0;
unsigned int frgb0_cnt      = 0;
unsigned int isp0_drop3a	= 0;
signed	 int isp0_bad_stat	= 0;
unsigned int video1_captcnt = 0;
unsigned int video1_dropcnt = 0;
unsigned int video1_skipcnt = 0;
unsigned int video1_zslcnt	= 0;
unsigned int frgb1_cnt      = 0;
unsigned int isp1_drop3a	= 0;
signed	 int isp1_bad_stat	= 0;
unsigned int report_enabled = 0;

static void capture_fps_log_timer(unsigned long data);

void capture_fps_log(void *data)
{
	mutex_lock(&report_mutex);
	if (report_init) {
		del_timer(&cam_report_timer);
		init_timer(&cam_report_timer);
		cam_report_timer.expires = jiffies + (1000 * HZ) / 1000;
		cam_report_timer.data = (unsigned long) 0;
		cam_report_timer.function = capture_fps_log_timer;
		add_timer(&cam_report_timer);
	}
	mutex_unlock(&report_mutex);

	printk("[report] v0[%2d %2d %2d %2d] v1[%2d %2d %2d %2d]" \
			" i[%2d %2d %2d %2d] s[%04x %04x]\n",
			video0_captcnt, video0_dropcnt, video0_skipcnt, video0_zslcnt,
			video1_captcnt, video1_dropcnt, video1_skipcnt, video1_zslcnt,
			isp0_drop3a, isp0_bad_stat,
			isp1_drop3a, isp1_bad_stat,
			frgb0_cnt, frgb1_cnt);

	/* bayer_load_test_only(g_capfh[0]); */

	return;
}

int capture_fps_log_init(void)
{
	mutex_lock(&report_mutex);
	if (report_init == 0) {
		report_init = 1;
		init_timer(&cam_report_timer);
		cam_report_timer.expires = jiffies + (1000 * HZ) / 1000;
		cam_report_timer.data = (unsigned long) 0;
		cam_report_timer.function = capture_fps_log_timer;
		add_timer(&cam_report_timer);
	}
	mutex_unlock(&report_mutex);

	return CSS_SUCCESS;
}

int capture_fps_log_deinit(void)
{
	mutex_lock(&report_mutex);
	if (report_init) {
		report_init = 0;
		del_timer(&cam_report_timer);
	}
	mutex_unlock(&report_mutex);
	return CSS_SUCCESS;
}

static inline int capture_fps_drop_count(struct css_bufq *bufq)
{
	int count = 0;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	count = bufq->drop_cnt;
	bufq->drop_cnt = 0;
	spin_unlock_irqrestore(&bufq->lock, flags);

	return count;
}

static inline int capture_fps_capture_count(struct css_bufq *bufq)
{
	int count = 0;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	count = bufq->capt_cnt;
	bufq->capt_cnt = 0;
	spin_unlock_irqrestore(&bufq->lock, flags);

	return count;
}

static inline int capture_fps_skip_count(struct css_bufq *bufq)
{
	int count = 0;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	count = bufq->skip_cnt;
	bufq->skip_cnt = 0;
	spin_unlock_irqrestore(&bufq->lock, flags);

	return count;
}

void capture_get_fps_info(void)
{
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	struct css_bufq *bufq = NULL;
	struct capture_fh *capfh = NULL;

	if (css_suspend == 1) {
		css_info("css in suspend\n");
	}

	if (report_init == 0 || css_suspend == 1) {
		video0_captcnt = 0;
		video0_dropcnt = 0;
		video0_skipcnt = 0;
		video0_zslcnt = 0;
		frgb0_cnt = 0;
		isp0_drop3a	= 0;
		isp0_bad_stat = 0;
		video1_captcnt = 0;
		video1_dropcnt = 0;
		video1_skipcnt = 0;
		video1_zslcnt = 0;
		frgb1_cnt = 0;
		isp1_drop3a = 0;
		isp1_bad_stat = 0;
		return;
	}

	capfh = g_capfh[0];
	if (capfh) {
		zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
		zsl_config = &zsl_dev->config;

		if (zsl_config->enable) {
			bufq = &zsl_dev->raw_buf;
			video0_zslcnt = capture_fps_capture_count(bufq);
		} else {
			video0_zslcnt = 0;
		}

		if (capture_get_stream_mode(capfh) == 1) {
			bufq = capture_buffer_get_pointer(capfh->fb,
						capfh->buf_attrb.buf_type);

			if (bufq == NULL) {
				video0_captcnt = 0;
				video0_dropcnt = 0;
				video0_skipcnt = 0;
				frgb0_cnt = 0;
			} else {
				video0_captcnt = capture_fps_capture_count(bufq);
				video0_dropcnt = capture_fps_drop_count(bufq);
				video0_skipcnt = capture_fps_skip_count(bufq);
				frgb0_cnt = scaler_get_frame_num(0);
			}
		} else {
			video0_captcnt = 0;
			video0_dropcnt = 0;
			video0_skipcnt = 0;
			frgb0_cnt = 0;
		}
	} else {
		video0_captcnt = 0;
		video0_dropcnt = 0;
		video0_skipcnt = 0;
		video0_zslcnt = 0;
		frgb0_cnt = 0;
	}

	capfh = g_capfh[1];
	if (capfh) {
		zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
		zsl_config = &zsl_dev->config;

		if (zsl_config->enable) {
			bufq = &zsl_dev->raw_buf;
			video1_zslcnt = capture_fps_capture_count(bufq);
		} else {
			video1_zslcnt = 0;
		}

		if (capture_get_stream_mode(capfh) == 1) {
			if (capfh->fb) {
				bufq = capture_buffer_get_pointer(capfh->fb,
							capfh->buf_attrb.buf_type);

				if (bufq == NULL) {
					video1_captcnt = 0;
					video1_dropcnt = 0;
					video1_skipcnt = 0;
					frgb1_cnt = 0;

				} else {
					video1_captcnt = capture_fps_capture_count(bufq);
					video1_dropcnt = capture_fps_drop_count(bufq);
					video1_skipcnt = capture_fps_skip_count(bufq);
					frgb1_cnt = scaler_get_frame_num(1);
				}
			}
		} else {
			video1_captcnt = 0;
			video1_dropcnt = 0;
			video1_skipcnt = 0;
			frgb1_cnt = 0;
		}
	} else {
		video1_captcnt = 0;
		video1_dropcnt = 0;
		video1_skipcnt = 0;
		video1_zslcnt = 0;
		frgb1_cnt = 0;
	}

	isp0_drop3a = odin_isp_get_dropped_3a(ISP0);
	isp0_bad_stat = odin_isp_get_bad_statistic(ISP0);
	isp1_drop3a = odin_isp_get_dropped_3a(ISP1);
	isp1_bad_stat = odin_isp_get_bad_statistic(ISP1);

}

static void capture_fps_log_timer(unsigned long data)
{
	capture_get_fps_info();
	schedule_work(&reportwq);
}

#ifdef CAM_USE_SYSFS_LOG_LEVEL

#define chk_add_log_info(str, label)	\
	if (strstr(msg, "+"str) != NULL) {	\
		css_log |= label;				\
		match = 1;						\
		printk(KERN_INFO "log:%s enabled\n", str); \
	}

#define chk_remove_log_info(str, label)	\
	if (strstr(msg, "-"str) != NULL) {	\
		css_log &= ~label;				\
		match = 1;						\
		printk(KERN_INFO "log:%s disabled\n", str); \
	}


static int css_store_log_level(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t len)
{
	char msg[256] = {0, };
	const char *p = buf;
	int match = 0;

	while (*p != '\0') {
		if (!isspace(*p))
			strncat(msg, p, 1);
		p++;
	}

	printk(KERN_INFO "CSS log level is..\n");

	chk_add_log_info("all",			0xFFFFFFFF);
	chk_add_log_info("err",			LOG_ERR);
	chk_add_log_info("warn",		LOG_WARN);
	chk_add_log_info("low",			LOG_LOW);
	chk_add_log_info("info",		LOG_INFO);
	chk_add_log_info("cam_int",		CAM_INT);
	chk_add_log_info("cam_v4l2",	CAM_V4L2);
	chk_add_log_info("zsl",			ZSL);
	chk_add_log_info("zsl_low",		ZSL_LOW);
	chk_add_log_info("3dnr",		VNR);
	chk_add_log_info("afifo",		AFIFO);
	chk_add_log_info("fd",			FD);
	chk_add_log_info("fpd",			FPD);
	chk_add_log_info("frgb",		FRGB);
	chk_add_log_info("isp",			ISP);
	chk_add_log_info("mdis",		MDIS);

	if (strstr(msg, "+mipicsi0") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH0)) {
			css_log |= MIPICSI0;
			printk(KERN_INFO "log:mipicsi0 enabled\n");
			odin_mipicsi_interrupt_enable(CSI_CH0, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi0 not init\n");
		}
		match = 1;
	}

	if (strstr(msg, "+mipicsi1") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH1)) {
			css_log |= MIPICSI1;
			printk(KERN_INFO "log:mipicsi1 enabled\n");
			odin_mipicsi_interrupt_enable(CSI_CH1, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi1 not init\n");
		}
		match = 1;
	}

	if (strstr(msg, "+mipicsi2") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH2)) {
			css_log |= MIPICSI2;
			printk(KERN_INFO "log:mipicsi2 enabled\n");
			odin_mipicsi_interrupt_enable(CSI_CH2, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi2 not init\n");
		}
		match = 1;
	}

#ifdef CONFIG_VIDEO_ODIN_S3DC
	chk_add_log_info("s3dc",	 S3DC);
#endif
	chk_add_log_info("sys", 	 CSS_SYS);
	chk_add_log_info("jpeg_enc", JPEG_ENC);
	chk_add_log_info("jpeg_v4l2",JPEG_V4L2);
	chk_add_log_info("sensor",	 SENSOR);

	if (strstr(msg, "+report") != NULL) {
		if (report_enabled == 0)	{
			printk(KERN_INFO "report enabled\n");
			report_enabled = 1;
			match = 1;
			capture_fps_log_init();
		}
	}

	chk_remove_log_info("all",		0xFFFFFFFF);
	chk_remove_log_info("err",		LOG_ERR);
	chk_remove_log_info("warn",		LOG_WARN);
	chk_remove_log_info("low",		LOG_LOW);
	chk_remove_log_info("info",		LOG_INFO);
	chk_remove_log_info("cam_int",	CAM_INT);
	chk_remove_log_info("cam_v4l2", CAM_V4L2);
	chk_remove_log_info("zsl",		ZSL);
	chk_remove_log_info("zsl_low",	ZSL_LOW);
	chk_remove_log_info("3dnr",		VNR);
	chk_remove_log_info("afifo",	AFIFO);
	chk_remove_log_info("fd",		FD);
	chk_remove_log_info("fpd",		FPD);
	chk_remove_log_info("frgb",		FRGB);
	chk_remove_log_info("isp",		ISP);
	chk_remove_log_info("mdis",		MDIS);

	if (strstr(msg, "-mipicsi0") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH0)) {
			css_log &= ~MIPICSI0;
			printk(KERN_INFO "log:mipicsi0 disable\n");
			odin_mipicsi_interrupt_disable(CSI_CH0, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi0 not init\n");
		}
		match = 1;
	}

	if (strstr(msg, "-mipicsi1") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH1)) {
			css_log &= ~MIPICSI1;
			printk(KERN_INFO "log:mipicsi1 disable\n");
			odin_mipicsi_interrupt_disable(CSI_CH1, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi1 not init\n");
		}
		match = 1;
	}

	if (strstr(msg, "-mipicsi2") != NULL) {
		if (odin_mipicsi_get_hw_init_state(CSI_CH2)) {
			css_log &= ~MIPICSI2;
			printk(KERN_INFO "log:mipicsi2 disable\n");
			odin_mipicsi_interrupt_disable(CSI_CH2, INT_CSI_ERROR);
		} else {
			printk(KERN_INFO "log:mipicsi2 not init\n");
		}
		match = 1;
	}

#ifdef CONFIG_VIDEO_ODIN_S3DC
	chk_remove_log_info("s3dc", 	S3DC);
#endif
	chk_remove_log_info("sys",		CSS_SYS);
	chk_remove_log_info("jpeg_enc", JPEG_ENC);
	chk_remove_log_info("jpeg_v4l2",JPEG_V4L2);
	chk_remove_log_info("sensor",	SENSOR);

	if (strstr(msg, "-report") != NULL) {
		if (report_enabled) {
			capture_fps_log_deinit();
			report_enabled = 0;
			match = 1;
			printk(KERN_INFO "report disabled\n");
		}
	}

	if (match == 0) {
		printk(KERN_INFO "usage : echo [+/-][option] > log_level\n");
		printk(KERN_INFO "   + : enable  log\n");
		printk(KERN_INFO "   - : disable log\n");
		printk(KERN_INFO "   [option]\n");
		printk(KERN_INFO "   all       : all log\n");
		printk(KERN_INFO "   err       : error log\n");
		printk(KERN_INFO "   warn      : warning log\n");
		printk(KERN_INFO "   low       : low level log\n");
		printk(KERN_INFO "   info      : normal log\n");
		printk(KERN_INFO "   cam_int   : camera interrupt\n");
		printk(KERN_INFO "   cam_v4l2  : camera v4l2 log\n");
		printk(KERN_INFO "   zsl       : zero shutter lag log\n");
		printk(KERN_INFO "   zsl_low   : zero shutter lag low log\n");
		printk(KERN_INFO "   3dnr      : 3dnr log\n");
		printk(KERN_INFO "   afifo     : async fifo log\n");
		printk(KERN_INFO "   fd        : face-detection log\n");
		printk(KERN_INFO "   fpd       : fpd log\n");
		printk(KERN_INFO "   frgb      : frame-grabber log\n");
		printk(KERN_INFO "   isp       : isp i/f log\n");
		printk(KERN_INFO "   mdis      : mdis log\n");
		printk(KERN_INFO "   mipicsi0  : mipi-csi2 ch0 log\n");
		printk(KERN_INFO "   mipicsi1  : mipi-csi2 ch1 log\n");
		printk(KERN_INFO "   mipicsi2  : mipi-csi2 ch2 log\n");
#ifdef CONFIG_VIDEO_ODIN_S3DC
		printk(KERN_INFO "   s3dc      : s3dc log\n");
#endif
		printk(KERN_INFO "   sys       : css sys log\n");
		printk(KERN_INFO "   jpeg_enc  : jpeg encoder log\n");
		printk(KERN_INFO "   jpeg_v4l2 : jpeg v4l2 log\n");
		printk(KERN_INFO "   sensor    : sensor log\n");
	}
	return len;
}

static int css_show_log_level(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	char msg[256] = {0, };

	sprintf(msg, "[enabled log]\n");
	strcat(buf, msg);

	if (css_log & LOG_ERR) {
		sprintf(msg, " ERR\n");
		strcat(buf, msg);
	}
	if (css_log & LOG_WARN) {
		sprintf(msg, " WARN\n");
		strcat(buf, msg);
	}
	if (css_log & LOG_LOW) {
		sprintf(msg, " LOW\n");
		strcat(buf, msg);
	}
	if (css_log & LOG_INFO) {
		sprintf(msg, " INFO\n");
		strcat(buf, msg);
	}
	if (css_log & CAM_INT) {
		sprintf(msg, " CAM_INT\n");
		strcat(buf, msg);
	}
	if (css_log & CAM_V4L2) {
		sprintf(msg, " CAM_V4L2\n");
		strcat(buf, msg);
	}
	if (css_log & ZSL) {
		sprintf(msg, " ZSL\n");
		strcat(buf, msg);
	}
	if (css_log & ZSL_LOW) {
		sprintf(msg, " ZSL LOW\n");
		strcat(buf, msg);
	}
	if (css_log & VNR) {
		sprintf(msg, " VNR\n");
		strcat(buf, msg);
	}
	if (css_log & AFIFO) {
		sprintf(msg, " AFIFO\n");
		strcat(buf, msg);
	}
	if (css_log & FD) {
		sprintf(msg, " FD\n");
		strcat(buf, msg);
	}
	if (css_log & FPD) {
		sprintf(msg, " FPD\n");
		strcat(buf, msg);
	}
	if (css_log & FRGB) {
		sprintf(msg, " FRGB\n");
		strcat(buf, msg);
	}
	if (css_log & ISP) {
		sprintf(msg, " ISP\n");
		strcat(buf, msg);
	}
	if (css_log & MDIS) {
		sprintf(msg, " MDIS\n");
		strcat(buf, msg);
	}
	if (css_log & MIPICSI0) {
		sprintf(msg, " MIPICSI2 CH0\n");
		strcat(buf, msg);
	}
	if (css_log & MIPICSI1) {
		sprintf(msg, " MIPICSI2 CH1\n");
		strcat(buf, msg);
	}
	if (css_log & MIPICSI2) {
		sprintf(msg, " MIPICSI2 CH2\n");
		strcat(buf, msg);
	}
#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (css_log & S3DC) {
		sprintf(msg, " S3DC\n");
		strcat(buf, msg);
	}
#endif
	if (css_log & CSS_SYS) {
		sprintf(msg, " SYS\n");
		strcat(buf, msg);
	}
	if (css_log & JPEG_ENC) {
		sprintf(msg, " JPEG_ENC\n");
		strcat(buf, msg);
	}
	if (css_log & JPEG_V4L2) {
		sprintf(msg, " JPEG_V4L2\n");
		strcat(buf, msg);
	}
	if (css_log & SENSOR) {
		sprintf(msg, " SENSOR\n");
		strcat(buf, msg);
	}
	return strlen(buf);
}

static DEVICE_ATTR(log_level, 0644, css_show_log_level, css_store_log_level);
#endif

u32 capture_get_frame_size(u32 pixelformat, u32 w, u32 h)
{
	u32 ret = 0;
	u32 img_size = (w * h * 2);

	switch (pixelformat)
	{
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
		ret = img_size;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		ret = img_size * 3/4;
		break;
	case V4L2_PIX_FMT_JPEG:
		ret = img_size + img_size/4;
		break;
	default:
		ret = img_size;
		break;
	}

	return ALIGN(ret, 4);
}

int capture_set_stereo_status(struct css_device *cssdev, int status)
{
	cssdev->stereo_enable = status;
	return CSS_SUCCESS;
}

int capture_get_stereo_status(struct css_device *cssdev)
{
#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (cssdev != NULL)
		return cssdev->stereo_enable;
	else
		return 0;
#else
	return 0;
#endif
}

int capture_set_postview_scaler(struct capture_fh *capfh,
			css_scaler_select scaler)
{
	if (capfh && capfh->cssdev) {
		capfh->cssdev->postview.scaler = scaler;
		return CSS_SUCCESS;
	}

	return CSS_FAILURE;
}

int capture_set_postview_config(struct css_postview *postview,
		css_scaler_select postview_scl,
		struct css_rect *crop_rect, u32 phy_addr)
{
	int ret = CSS_SUCCESS;
	struct css_scaler_device postview_scl_dev;
	struct css_scaler_device *preview_scl_dev;

	if (g_capfh[postview_scl] == NULL) {
		css_err("g_capfh[%d] is NULL\n", postview_scl);
		return CSS_FAILURE;
	}
	preview_scl_dev = g_capfh[postview_scl]->scl_dev;

	memcpy(&postview_scl_dev, preview_scl_dev, sizeof(struct css_scaler_device));

	postview_scl_dev.config.dst_width	= postview->width;
	postview_scl_dev.config.dst_height	= postview->height;
	postview_scl_dev.config.action		= CSS_SCALER_ACT_CAPTURE;
	postview_scl_dev.config.crop.start_x	= crop_rect->start_x;
	postview_scl_dev.config.crop.start_y	= crop_rect->start_y;
	postview_scl_dev.config.crop.width		= crop_rect->width;
	postview_scl_dev.config.crop.height		= crop_rect->height;
	postview_scl_dev.config.hw_buf_index			= 0;

	switch (postview->fmt) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
		postview_scl_dev.config.pixel_fmt = CSS_COLOR_YUV422;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		postview_scl_dev.config.pixel_fmt = CSS_COLOR_YUV420;
		break;
	case V4L2_PIX_FMT_RGB565:
		postview_scl_dev.config.pixel_fmt = CSS_COLOR_RGB565;
		break;
	default:
		css_err("postview invalid pixel format %d!\n", postview->fmt);
		return CSS_ERROR_INVALID_PIXEL_FORMAT;
	}

	ret = scaler_configure(postview_scl, &postview_scl_dev.config,
			&postview_scl_dev.data);

	scaler_set_buffers(postview_scl, &postview_scl_dev.config, phy_addr);
	scaler_action_configure(postview_scl, &postview_scl_dev.data,
			postview_scl_dev.config.action);

	return ret;
}

static int capture_find_pixel_format(struct v4l2_format *fmt)
{
	int pixel_format;

	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
		pixel_format = CSS_COLOR_YUV422;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		pixel_format = CSS_COLOR_YUV420;
		break;
	case V4L2_PIX_FMT_RGB565:
		pixel_format = CSS_COLOR_RGB565;
		break;
	default:
		css_err("invalid pixel format %d!\n", fmt->fmt.pix.pixelformat);
		return -EINVAL;
	}

	return pixel_format;
}

int capture_set_try_format(struct capture_fh *capfh, struct v4l2_format *fmt)
{
	int ret = CSS_SUCCESS;
	struct css_device *cssdev = capfh->cssdev;
	css_behavior behavior;
	char fmt_str[10];

	behavior = (css_behavior)fmt->fmt.pix.priv;
	if (behavior == CSS_BEHAVIOR_NONE) {
		css_err("scaler_%d invalid behavior!\n", capfh->main_scaler);
		return -EINVAL;
	}

	fmt->fmt.pix.colorspace	= 0;
	fmt->fmt.pix.field		= V4L2_FIELD_NONE;

	/*
	 * if (cam_dev->cur_sensor) {
	 *	  if (cam_dev->cur_sensor->init_width < fmt->fmt.pix.width ||
	 *		  cam_dev->cur_sensor->init_height < fmt->fmt.pix.height)
	 *		  return -EINVAL;
	 * }
	 */

	memset(fmt_str, 0x0, 10);
	memcpy(fmt_str, &fmt->fmt.pix.pixelformat, 4);
	css_cam_v4l2("pix fmt : v4l2_fourcc %s\n", fmt_str);

	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
		/* must out width  multiple of 4 */
		/* must out height multiple of 2 */
		fmt->fmt.pix.width = (fmt->fmt.pix.width >> 2) << 2;
		fmt->fmt.pix.height = (fmt->fmt.pix.height >> 1) << 1;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		/* must out width  multiple of 16 */
		/* must out height multiple of 2 */
		fmt->fmt.pix.width = (fmt->fmt.pix.width >> 4) << 4;
		fmt->fmt.pix.height = (fmt->fmt.pix.height >> 1) << 1;
		break;
	case V4L2_PIX_FMT_RGB565:
		/* must out width  multiple of 4 */
		/* must out height multiple of 2 */
		fmt->fmt.pix.width = (fmt->fmt.pix.width >> 2) << 2;
		fmt->fmt.pix.height = (fmt->fmt.pix.height >> 1) << 1;
		break;
	default:
		css_err("scaler_%d invalid pixel format %d!\n",
				capfh->main_scaler, fmt->fmt.pix.pixelformat);
		return -EINVAL;
	}

	fmt->fmt.pix.sizeimage = capture_get_frame_size(fmt->fmt.pix.pixelformat,
								 fmt->fmt.pix.width, fmt->fmt.pix.height);

	if (behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW ||
		behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW)
		fmt->fmt.pix.sizeimage += cssdev->postview.size;

	return ret;
}

static inline int capture_check_preview_behavior(css_behavior behavior)
{
	if (behavior == CSS_BEHAVIOR_RECORDING ||
		behavior == CSS_BEHAVIOR_CAPTURE_ZSL ||
		behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW ||
#ifdef CONFIG_VIDEO_ODIN_S3DC
		behavior == CSS_BEHAVIOR_CAPTURE_ZSL_3D ||
#endif
		behavior == CSS_BEHAVIOR_SNAPSHOT ||
		behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW)
		return 0;

	return 1;
}

int capture_set_format(struct capture_fh *capfh, struct v4l2_format *fmt)
{
	css_behavior behavior;
	css_pixel_format pixel_format;
	int ret = CSS_SUCCESS;
	unsigned int sensor_width = 0;
	unsigned int sensor_height = 0;
	unsigned int ispwrap_path;
	struct v4l2_mbus_framefmt m_pix = {0, };
	struct css_image_size ispwrap_img_size;
	struct css_scaler_config *scl_config;
	struct css_scaler_data *scl_data;
	struct css_sensor_device *cam_dev = capfh->cam_dev;
	struct css_scaler_device *scl_dev = capfh->scl_dev;
	struct css_device *cssdev = capfh->cssdev;
	struct css_bayer_scaler *bayer_scl = &cssdev->afifo.bayer_scl[0];

	behavior = (css_behavior)fmt->fmt.pix.priv;

	scl_config = &scl_dev->config;
	scl_data = &scl_dev->data;

	css_afifo("Bayer[%d]: Src W(%d) (%d)\n", cssdev->afifo.bscl_index,
		bayer_scl[cssdev->afifo.bscl_index].src_w,
		bayer_scl[cssdev->afifo.bscl_index].src_h);

	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video, g_mbus_fmt,
				&m_pix);

		sensor_width = bayer_scl[cssdev->afifo.bscl_index].src_w;
		sensor_height = bayer_scl[cssdev->afifo.bscl_index].src_h;
		if (sensor_width <= 0 || sensor_height <= 0) {
			sensor_width = m_pix.width;
			sensor_height = m_pix.height;
		}

		if (sensor_width < fmt->fmt.pix.width) {
			css_err("sensor_width(%d) > f->fmt.pix.width(%d)\n",
					sensor_width, fmt->fmt.pix.width);
			return -EINVAL;
		}
	} else {
		css_err("current sensor is not found\n");
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	if (scl_config->crop.height > sensor_height) {
		css_err("crop height(%d) > sensor height(%d)\n",
				scl_config->crop.height, sensor_height);
		return -EINVAL;
	}

#if 1
	scl_config->y_padding = fmt->fmt.pix.bytesperline;
#else
	if (fmt->fmt.pix.height > scl_config->crop.height)
		scl_config->y_padding = fmt->fmt.pix.height
								- scl_config->crop.height;
	else
		scl_config->y_padding = 0;
#endif

	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_RGB565:
		/* must out width  multiple of 4 */
		/* must out height multiple of 2 */
		if (fmt->fmt.pix.width % 4 != 0) {
			css_err("width must be multiple of 4\n");
			return -EINVAL;
		}
		if (fmt->fmt.pix.height % 2 != 0) {
			css_err("height must be multiple of 2\n");
			return -EINVAL;
		}
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		/* must out width  multiple of 16 */
		/* must out height multiple of 2 */
		if (fmt->fmt.pix.width % 16 != 0) {
			css_err("width must be multiple of 16\n");
			return -EINVAL;
		}
		if (fmt->fmt.pix.height % 2 != 0) {
			css_err("height must be multiple of 2\n");
			return -EINVAL;
		}
		break;
	default:
		css_err("scaler_%d invalid pixel format %d!\n",
				capfh->main_scaler, fmt->fmt.pix.pixelformat);
		return -EINVAL;
	}

	if ((ret = capture_set_try_format(capfh, fmt)) != CSS_SUCCESS)
		return ret;

	pixel_format = capture_find_pixel_format(fmt);
	if (pixel_format < 0)
		return CSS_ERROR_INVALID_PIXEL_FORMAT;


	/*
	 * input of scaler is output of Bayer Scaler.
	 *
	 * SENSOR => MIPICSI => Bayer Scaler => ISP => FrameGrabber
	 *	 |						 |						|
	 * resolution			crop & resizing 	   crop & resizing
	 *						  (if needed)			  (if needed)
	 */

	if (!capture_check_preview_behavior(behavior)) {
		if (behavior == CSS_BEHAVIOR_CAPTURE_ZSL_3D)
			return CSS_ERROR_INVALID_BEHAVIOR;
	}

	capfh->behavior = behavior;
	switch (behavior) {
	case CSS_BEHAVIOR_PREVIEW:
		capfh->buf_attrb.buf_type = CSS_BUF_PREVIEW;
		scl_config->action = CSS_SCALER_ACT_CAPTURE;
		break;
	case CSS_BEHAVIOR_SNAPSHOT:
	case CSS_BEHAVIOR_CAPTURE_ZSL:
		capfh->buf_attrb.buf_type = CSS_BUF_CAPTURE;
		scl_config->action = CSS_SCALER_ACT_CAPTURE;
		break;
	case CSS_BEHAVIOR_SNAPSHOT_POSTVIEW:
	case CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW:
		capfh->buf_attrb.buf_type = CSS_BUF_CAPTURE_WITH_POSTVIEW;
		scl_config->action = CSS_SCALER_ACT_CAPTURE;
		break;
#ifdef CONFIG_VIDEO_ODIN_S3DC
	case CSS_BEHAVIOR_PREVIEW_3D:
		capfh->buf_attrb.buf_type = CSS_BUF_PREVIEW_3D;
		scl_config->action = CSS_SCALER_ACT_CAPTURE_3D;
		s3dc_set_v4l2_preview_cam_fh(capfh);
		break;
	case CSS_BEHAVIOR_CAPTURE_ZSL_3D:
		capfh->buf_attrb.buf_type = CSS_BUF_CAPTURE_3D;
		scl_config->action = CSS_SCALER_ACT_CAPTURE_3D;
		s3dc_set_v4l2_capture_cam_fh(capfh);
		break;
#endif
	case CSS_BEHAVIOR_RECORDING:
		capfh->buf_attrb.buf_type = CSS_BUF_PREVIEW;
		scl_config->action = CSS_SCALER_ACT_CAPTURE;
		break;
	default:
		css_err("unsupported behavior\n");
		return -EINVAL;
		break;
	}

	/*
	 *------------------------------------------------------------------
	 *		   |----|----------|
	 *		   |	|		   |
	 *		---|crop|  scaler  |--dst_width
	 *		 | |	|		   |  dst_height
	 *		 | |----|----------|
	 * src_width 	|
	 * src_height	crop
	 *------------------------------------------------------------------
	 */

	/*
	 * now scaler source is a sensor width and height.
	 * It just be temporary solution.
	 * we need to change this to bayer scaler output.
	 * if proper bayer scaler function is ready, we will
	 * use it.
	*/

	scl_config->dst_width	= fmt->fmt.pix.width;
#if 0
	scl_config->dst_height	= fmt->fmt.pix.height;
#else
	scl_config->dst_height	= fmt->fmt.pix.height - scl_config->y_padding;
#endif
	scl_config->pixel_fmt	= pixel_format;
	scl_config->frame_size	= fmt->fmt.pix.sizeimage;

	if (scl_config->crop.width <= 0 ||
		scl_config->crop.height <= 0) {
		/* crop info be filled by V4L2 s_crop command */
		scl_config->crop.start_x = 0;
		scl_config->crop.start_y = 0;
	}

	/* AsyncFIFO Input Sensor Selection */
	odin_isp_wrap_set_sensor_sel(cam_dev->cur_sensor->id);

	switch (behavior) {
	case CSS_BEHAVIOR_PREVIEW:
	{
		int capture_scaler;

		if (capfh->main_scaler > CSS_SCALER_1) {
			css_err("invalid capture scaler %d!\n", capfh->main_scaler);
			return -EINVAL;
		}

#ifndef CSS_VT_MEM_CTRL_SUPPORT
		if (capture_get_path_mode(cssdev) == CSS_NORMAL_PATH_MODE)
			css_bus_control(MEM_BUS, CSS_DEFAULT_MEM_BUS_FREQ_KHZ,
						CSS_BUS_TIME_SCHED_NONE);
#endif

		/* AsyncFIFO Path Selection */
		if (capfh->main_scaler == CSS_SCALER_0) {
			css_zsl_path rpath;

			rpath = (capfh->main_scaler == CSS_SCALER_0) ? \
						ZSL_READ_PATH0 : ZSL_READ_PATH1;

			if (capture_get_path_mode(cssdev) == CSS_LIVE_PATH_MODE) {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
				/* odin_isp_wrap_set_read_path_enable(rpath); */
				ispwrap_path = ISP_WRAP1;
			} else {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
				/* odin_isp_wrap_set_read_path_disable(rpath); */
				ispwrap_path = ISP_WRAP0;
			}
		} else {
			odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
			ispwrap_path = ISP_WRAP1;
		}

		css_bus_control(CCI_BUS, CSS_DEFAULT_CCI_BUS_FREQ_KHZ,
					CSS_BUS_TIME_SCHED_NONE);

		odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);

		capture_set_stereo_status(cssdev, false);

		capture_scaler = capfh->main_scaler;

		odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);
		scl_config->src_width = ispwrap_img_size.width;
		scl_config->src_height = ispwrap_img_size.height;
		if (scl_config->crop.width > scl_config->src_width ||
			scl_config->crop.height > scl_config->src_height) {
			scl_config->crop.width = scl_config->src_width;
			scl_config->crop.height = scl_config->src_height;
		}

		ret = scaler_configure(capture_scaler, scl_config, scl_data);
		if (ret != CSS_SUCCESS)
			css_err("scaler_%d configure err=%d\n", capture_scaler, ret);

		break;
	}

	case CSS_BEHAVIOR_CAPTURE_ZSL:
	case CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW:
	{
		if (capfh->main_scaler > CSS_SCALER_1) {
			css_err("invalid capture scaler %d!\n", capfh->main_scaler);
			return -EINVAL;
		}

		if (capfh->main_scaler == CSS_SCALER_0) {
			/*
			 * current front zsl capture path is zsl1->isp1->scaler0.
			 * if we wanna use zsl0->isp0->scaler0, use below configure.
			 * odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
			 * ispwrap_path = ISP_WRAP0;
			*/
			odin_isp_wrap_set_path_sel(ISP1_LINKTO_SC0_ISP1_LINKTO_SC1);
			ispwrap_path = ISP_WRAP0;
		} else {
			if (capture_get_path_mode(cssdev) == CSS_LIVE_PATH_MODE) {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
				ispwrap_path = ISP_WRAP0;
			} else {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP0_LINKTO_SC1);
				ispwrap_path = ISP_WRAP0;
			}
		}

		odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);

		if (behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW) {
			css_scaler_select postview_scaler;
			/* twice load needed so, same scaler needed. */
			if (capfh->main_scaler == CSS_SCALER_0)
				postview_scaler = CSS_SCALER_0;
			else
				postview_scaler = CSS_SCALER_1;

			capture_set_postview_scaler(capfh, postview_scaler);
		} else {

			odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);

			scl_config->src_width = ispwrap_img_size.width;
			scl_config->src_height = ispwrap_img_size.height;
		}

		break;
	}
	case CSS_BEHAVIOR_SNAPSHOT:
	{
		if (capfh->main_scaler < CSS_SCALER_FD) {
			css_scaler_select preview_scaler;
			css_scaler_select snapshot_scaler;

			if (capfh->main_scaler == CSS_SCALER_0)
				preview_scaler = CSS_SCALER_1;
			else
				preview_scaler = CSS_SCALER_0;

			snapshot_scaler = capfh->main_scaler;

			if (CSS_SCALER_0 == preview_scaler) {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
				ispwrap_path = ISP_WRAP0;
			} else {
				odin_isp_wrap_set_path_sel(ISP1_LINKTO_SC0_ISP1_LINKTO_SC1);
				ispwrap_path = ISP_WRAP1;
			}

			odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);

			odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);

			scl_config->src_width = ispwrap_img_size.width;
			scl_config->src_height = ispwrap_img_size.height;

			if (scl_config->crop.width > scl_config->src_width ||
				scl_config->crop.height > scl_config->src_height) {
				scl_config->crop.width = scl_config->src_width;
				scl_config->crop.height = scl_config->src_height;
			}

			ret = scaler_configure(snapshot_scaler, scl_config, scl_data);
			if (ret != CSS_SUCCESS)
				css_err("scaler%d configure err=%d\n",snapshot_scaler, ret);
		} else {
			css_err("unsupported scaler for snapshot behavior!\n");
			ret = -EINVAL;
		}
		break;
	}
	case CSS_BEHAVIOR_SNAPSHOT_POSTVIEW:
	{
		if (capfh->main_scaler < CSS_SCALER_FD) {
			css_scaler_select snapshot_scaler;
			css_scaler_select postview_scaler;

			snapshot_scaler = capfh->main_scaler;

			if (capfh->main_scaler == CSS_SCALER_0)
				postview_scaler = CSS_SCALER_1;
			else
				postview_scaler = CSS_SCALER_0;

			if (CSS_SCALER_0 == postview_scaler) {
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP0_LINKTO_SC1);
				ispwrap_path = ISP_WRAP0;
			} else {
				odin_isp_wrap_set_path_sel(ISP1_LINKTO_SC0_ISP1_LINKTO_SC1);
				ispwrap_path = ISP_WRAP1;
			}

			odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);

			odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);

			scl_config->src_width = ispwrap_img_size.width;
			scl_config->src_height = ispwrap_img_size.height;

			if (scl_config->crop.width > scl_config->src_width ||
				scl_config->crop.height > scl_config->src_height) {
				scl_config->crop.width = scl_config->src_width;
				scl_config->crop.height = scl_config->src_height;
			}

			ret = scaler_configure(snapshot_scaler, scl_config, scl_data);
			if (ret != CSS_SUCCESS)
				css_err("scaler%d configure err=%d\n",snapshot_scaler, ret);

			capture_set_postview_scaler(capfh, postview_scaler);
		} else {
			css_err("unsupported scaler for snapshot behavior!\n");
			ret = -EINVAL;
		}
		break;
	}
	case CSS_BEHAVIOR_RECORDING:
	{
		if (capfh->main_scaler < CSS_SCALER_FD) {
			css_scaler_select preview_scaler;
			css_scaler_select record_scaler;

			if (capfh->main_scaler == CSS_SCALER_0)
				preview_scaler = CSS_SCALER_1;
			else
				preview_scaler = CSS_SCALER_0;

			record_scaler = capfh->main_scaler;

			if (CSS_SCALER_0 == preview_scaler) {
				/* REAR RECORD */
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
				ispwrap_path = ISP_WRAP0;
			} else {
				/* FRONT RECORD */
				odin_isp_wrap_set_path_sel(ISP1_LINKTO_SC0_ISP1_LINKTO_SC1);
				ispwrap_path = ISP_WRAP1;
			}

			odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);
			odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);

			scl_config->src_width = ispwrap_img_size.width;
			scl_config->src_height = ispwrap_img_size.height;

			if (scl_config->crop.width > scl_config->src_width ||
				scl_config->crop.height > scl_config->src_height) {
				scl_config->crop.width = scl_config->src_width;
				scl_config->crop.height = scl_config->src_height;
			}

			ret = scaler_configure(record_scaler, scl_config, scl_data);
			if (ret != CSS_SUCCESS)
				css_err("scaler%d configure err=%d\n",record_scaler, ret);
		} else {
			css_err("unsupported scaler for record behavior!\n");
			ret = -EINVAL;
		}
		break;
	}
#ifdef CONFIG_VIDEO_ODIN_S3DC
	case CSS_BEHAVIOR_PREVIEW_3D:
	{
		struct css_scaler_data scl_data = {0, };

		if (capfh->main_scaler > CSS_SCALER_1) {
			css_err("invalid capture scaler %d!\n", capfh->main_scaler);
			return -EINVAL;
		}

		capture_set_stereo_status(cssdev, true);

		/* For Single rear sensor */
		odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP0_LINKTO_SC1);

		/*
		 * If Dual rear sensor
		 * odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
		 */

		odin_isp_wrap_set_mem_ctrl(S3DC_SRAM_ENABLE);
		odin_isp_wrap_get_resolution(ISP_WRAP0, &ispwrap_img_size);

		scl_config->dst_width = scl_config->src_width;
		scl_config->dst_height = scl_config->src_height;

		/*
		 * if (scl_config->crop.width > scl_config->src_width ||
		 *	scl_config->crop.height > scl_config->src_width) {
		 *	scl_config->crop.width	= scl_config->src_width;
		 *	scl_config->crop.height = scl_config->src_height;
		 * }
		 */

		ret = scaler_configure(CSS_SCALER_0, scl_config, &scl_data);
		if (ret != CSS_SUCCESS) {
			css_err("scaler_0 configure err=%d\n", ret);
			return ret;
		}

		/*
		 * odin_isp_wrap_get_resolution(ISP_WRAP0, &ispwrap_img_size);
		 * scl_config->dst_width = scl_config->src_width;
		 * scl_config->dst_height = scl_config->src_height;
		 *
		 * if (scl_config->crop.width > scl_config->src_width ||
		 *	scl_config->crop.height > scl_config->src_width) {
		 * 	scl_config->crop.width = scl_config->src_width;
		 * 	scl_config->crop.height = scl_config->src_height;
		 * }
		 */

		ret = scaler_configure(CSS_SCALER_1, scl_config, &scl_data);
		if (ret != CSS_SUCCESS) {
			css_err("scaler_1 configure err=%d\n", ret);
			return ret;
		}
		break;
	}
	case CSS_BEHAVIOR_CAPTURE_ZSL_3D:
	{
		if (capfh->main_scaler > CSS_SCALER_1) {
			css_err("invalid capture scaler %d!\n", capfh->main_scaler);
			return -EINVAL;
		}

		/* For Single rear sensor */
		odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP0_LINKTO_SC1);
		/* If Dual rear sensor */
		/* odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1); */

		odin_isp_wrap_set_mem_ctrl(S3DC_SRAM_ENABLE);
		odin_isp_wrap_get_resolution(ispwrap_path, &ispwrap_img_size);

		scl_config->src_width = ispwrap_img_size.width;
		scl_config->src_height = ispwrap_img_size.height;
		break;
	}
#endif
	}

	css_info("%s scaler_%d bh=%d src_w=%d, h=%d, dst w=%d, h=%d +[p%d]\n",
			__func__, capfh->main_scaler, behavior,
			scl_config->src_width, scl_config->src_height,
			scl_config->dst_width, scl_config->dst_height,
			scl_config->y_padding);

#if 0
	css_info("%s scaler_%d frame size = %d\n",	__func__,
			capfh->main_scaler, fmt->fmt.pix.sizeimage);

	css_info("%s scaler_%d behavior = %d\n",	__func__,
			capfh->main_scaler, behavior);
#endif
	css_info("current sensor : %d\n", cam_dev->cur_sensor->id);

	return ret;
}

static inline int capture_buffer_get_capture_index(struct css_bufq *bufq)
{
	int i = 0;
	int buf_index = -1;

	for (i = 0; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_CAPTURING) {
			buf_index = i;
			break;
		}
	}

	return buf_index;
}

int capture_buffer_get_queued_index(struct css_bufq *bufq)
{
	int i = 0;
	int buf_index = 0;
	int queuedcnt = 0;

	/* search empty stream buf */
	for (i = 0; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_QUEUED) {
			queuedcnt++;
		}
	}

	/* find queued buffer index */
	if (queuedcnt > 0) {
		for (i = 0; i < bufq->count; i++) {
			if (bufq->bufs[i].state == CSS_BUF_QUEUED) {
				buf_index = i;
				break;
			}
		}
	}

	return buf_index;
}

int capture_buffer_get_next_index(struct css_bufq *bufq)
{
	int i = 0;
	int buf_index = 0;
	int buf_is_full = 1;

	if (bufq->count < 2){
		if (bufq->bufs[0].state == CSS_BUF_QUEUED)
			return buf_index;
		else
			return -1;
	}

	if (bufq->frminterval > 1) {
		if (bufq->capt_tot != 0 &&
			((bufq->capt_tot % bufq->frminterval) != 0)) {
			/* drop captured frame */
			return -2;
		}
	}

	/*search empty stream buf*/
	for (i = bufq->q_pos; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_QUEUED &&
			i != bufq->q_pos) {
			buf_index = i;
			buf_is_full = 0;
			break;
		}
	}

	if (buf_is_full) {
		for (i = 0; i < bufq->q_pos; i++) {
			if (bufq->bufs[i].state == CSS_BUF_QUEUED &&
				i != bufq->q_pos) {
				buf_index = i;
				buf_is_full = 0;
				break;
			}
		}
	}

	if (buf_is_full)
		return -1;
	else
		return buf_index;
}

struct css_bufq *capture_buffer_get_pointer(struct css_framebuffer *cap_fb,
					css_buffer_type buf_type)
{
	struct css_bufq *bufq = NULL;

	switch (buf_type) {
	case CSS_BUF_PREVIEW:
		bufq = &cap_fb->preview_bufq;
		break;
	case CSS_BUF_CAPTURE:
	case CSS_BUF_CAPTURE_WITH_POSTVIEW:
		bufq = &cap_fb->capture_bufq;
		break;
#ifdef CONFIG_VIDEO_ODIN_S3DC
	case CSS_BUF_CAPTURE_3D:
		bufq = &cap_fb->capture_bufq;
		break;
	case CSS_BUF_PREVIEW_3D:
		bufq = &cap_fb->stereo_bufq;
		break;
#endif
	default:
		css_err("unknown buffer type %d\n", buf_type);
		bufq = NULL;
		break;
	}

	return bufq;
}

int capture_buffer_init(struct capture_fh *capfh, int bufs_cnt)
{
	int i;
	int ret = CSS_SUCCESS;
	int bufs_size = 0;
	struct css_device *cssdev = capfh->cssdev;
	struct css_framebuffer *cap_fb = capfh->fb;
	struct css_scaler_device *scl_dev = capfh->scl_dev;
	struct css_bufq *bufq = NULL;
	struct css_bufq *drop_bufq = NULL;

	switch (capfh->buf_attrb.buf_type) {
	case CSS_BUF_PREVIEW:
		bufq = &cap_fb->preview_bufq;
		bufs_size = scl_dev->config.frame_size;
		break;
	case CSS_BUF_CAPTURE:
		bufq = &cap_fb->capture_bufq;
		bufs_size = scl_dev->config.frame_size;
		break;
	case CSS_BUF_CAPTURE_WITH_POSTVIEW:
		bufq = &cap_fb->capture_bufq;
		bufs_size = scl_dev->config.frame_size;
		break;
#ifdef CONFIG_VIDEO_ODIN_S3DC
	case CSS_BUF_CAPTURE_3D:
		bufq = &cap_fb->capture_bufq;
		bufs_size = scl_dev->config.frame_size;
		ret = s3dc_set_v4l2_stereo_buf_set(CSS_BUF_CAPTURE_3D, bufq);
		if (ret != CSS_SUCCESS)
			return ret;
		break;
	case CSS_BUF_PREVIEW_3D:
		bufq = &cap_fb->stereo_bufq;
		bufs_size = scl_dev->config.frame_size;
		ret = s3dc_set_v4l2_stereo_buf_set(CSS_BUF_PREVIEW_3D, bufq);
		if (ret != CSS_SUCCESS)
			return ret;
		break;
#endif
	default:
		css_err("unknown buffer type %d\n", capfh->buf_attrb.buf_type);
		return CSS_FAILURE;
		break;
	}

	if (bufq->ready == false) {
		if (bufs_size) {
			bufq->bufs = kzalloc(sizeof(struct css_buffer)
								* CSS_MAX_FRAME_BUFFER_COUNT, GFP_KERNEL);

			if (bufq->bufs == NULL) {
				css_err("stream buffer alloc fail!\n")
				return CSS_FAILURE;
			}

			if (cssdev->rsvd_mem) {
				for (i=0; i < bufs_cnt; i++) {
					ret = odin_css_physical_buffer_alloc(cssdev->rsvd_mem,
										&bufq->bufs[i], bufs_size);
					if (ret != CSS_SUCCESS) {
						odin_css_physical_buffer_free(cssdev->rsvd_mem,
										&bufq->bufs[i]);
						css_err("stream buffer alloc fail index is %d!\n", i);
						return CSS_FAILURE;
					}
				}
			} else {
				for (i=0; i < bufs_cnt; i++) {
					bufq->bufs[i].state = CSS_BUF_UNUSED;
					bufq->bufs[i].dma_addr = 0;
					bufq->bufs[i].ion_fd = -1;
					bufq->bufs[i].imported = 0;
					INIT_LIST_HEAD(&bufq->bufs[i].list);
				}
			}
			bufq->count = bufs_cnt;

			if (bufs_cnt < 2)
				bufq->mode = ONE_SHOT_MODE;
			else
				bufq->mode = STREAM_MODE;

			css_info("bufmode: %d [cnt:%d]\n", bufq->mode, bufs_cnt);

			init_waitqueue_head(&bufq->complete.wait);
			spin_lock_init(&bufq->lock);
			INIT_LIST_HEAD(&bufq->head);
		}

		bufq->cam_client = odin_ion_client_create("cam_ion");
		if (bufq->cam_client == NULL) {
			css_err("cam_client[%d] create fail\n",
					capfh->buf_attrb.buf_type);
			return CSS_FAILURE;
		} else {
			css_sys("cam_client[%d] create success\n",
					capfh->buf_attrb.buf_type);
		}

		bufq->ready = true;
	}

	drop_bufq = &cap_fb->drop_bufq;

	if (drop_bufq->ready == false) {
		unsigned int dropbuf_size = bufs_size + cssdev->postview.size;
		if (dropbuf_size) {
			drop_bufq->bufs = kzalloc(sizeof(struct css_buffer)	* 1,
								GFP_KERNEL);

			if (drop_bufq->bufs == NULL) {
				css_err("drop buffer alloc fail!\n")
				return CSS_FAILURE;
			}

			if (cssdev->rsvd_mem) {
				ret = odin_css_physical_buffer_alloc(cssdev->rsvd_mem,
									drop_bufq->bufs, dropbuf_size);
				if (ret != CSS_SUCCESS) {
					odin_css_physical_buffer_free(cssdev->rsvd_mem,
									drop_bufq->bufs);
					css_err("drop_bufq alloc fail!\n");
					return CSS_FAILURE;
				}
			} else {
#ifdef CONFIG_ODIN_ION_SMMU
				unsigned long iova = 0;
				unsigned long aligned_size = 0;
				struct ion_client *i_client;

				i_client = odin_ion_client_create("cam_drop_buf");
				if (i_client) {
					cap_fb->drop_bufq_client = i_client;

					aligned_size = ion_align_size(dropbuf_size);
					drop_bufq->bufs->ion_hdl.handle = ion_alloc(i_client,
													aligned_size,
													0x1000,
													(1<<ODIN_ION_SYSTEM_HEAP1),
													0,
													ODIN_SUBSYS_CSS);
					if (IS_ERR_OR_NULL(drop_bufq->bufs->ion_hdl.handle)) {
						odin_ion_client_destroy(i_client);
						cap_fb->drop_bufq_client = NULL;
						if (drop_bufq->bufs) {
							kzfree(drop_bufq->bufs);
							drop_bufq->bufs = NULL;
						}
						css_err("drop_bufq ion get handle fail\n");
						return CSS_FAILURE;
					}

					iova = odin_ion_get_iova_of_buffer(
							drop_bufq->bufs->ion_hdl.handle, ODIN_SUBSYS_CSS);
					if (!iova) {
						if (drop_bufq->bufs->ion_hdl.handle)
							drop_bufq->bufs->ion_hdl.handle = NULL;

						odin_ion_client_destroy(i_client);
						cap_fb->drop_bufq_client = NULL;
						if (drop_bufq->bufs) {
							kzfree(drop_bufq->bufs);
							drop_bufq->bufs = NULL;
						}
						css_err("drop_bufq ion get iova fail\n");
						return CSS_FAILURE;
					}

					drop_bufq->bufs->id		= 0;
					drop_bufq->bufs->state	= CSS_BUF_UNUSED;
					drop_bufq->bufs->offset	= 0;
					drop_bufq->bufs->byteused	= 0;
					drop_bufq->bufs->dma_addr	= iova;
					drop_bufq->bufs->cpu_addr	= NULL;
					drop_bufq->bufs->size		= dropbuf_size;
				} else {
					css_err("drop_bufq ion client create fail\n");
					return CSS_FAILURE;
				}
#else
				css_err("not supported ion\n");
				return CSS_FAILURE;
#endif
				drop_bufq->bufs->state = CSS_BUF_UNUSED;
			}
			drop_bufq->count = 1;
			drop_bufq->mode = ONE_SHOT_MODE;

			spin_lock_init(&drop_bufq->lock);
			INIT_LIST_HEAD(&drop_bufq->head);
		}
		drop_bufq->ready = true;
	}

	return CSS_SUCCESS;
}

int capture_buffer_deinit(struct capture_fh *capfh)
{
	int i;
	struct css_device *cssdev = capfh->cssdev;
	struct css_framebuffer *cap_fb = capfh->fb;
	struct css_bufq *bufq = NULL;
	struct css_bufq *drop_bufq = NULL;

	switch (capfh->buf_attrb.buf_type) {
	case CSS_BUF_PREVIEW:
		bufq = &cap_fb->preview_bufq;
		break;
	case CSS_BUF_CAPTURE:
	case CSS_BUF_CAPTURE_WITH_POSTVIEW:
		bufq = &cap_fb->capture_bufq;
		break;
#ifdef CONFIG_VIDEO_ODIN_S3DC
	case CSS_BUF_CAPTURE_3D:
		bufq = &cap_fb->capture_bufq;
		s3dc_set_v4l2_stereo_buf_set(CSS_BUF_CAPTURE_3D, NULL);
		break;
	case CSS_BUF_PREVIEW_3D:
		bufq = &cap_fb->stereo_bufq;
		s3dc_set_v4l2_stereo_buf_set(CSS_BUF_PREVIEW_3D, NULL);
		break;
#endif
	default:
		css_err("unknown buffer type %d\n", capfh->buf_attrb.buf_type);
		return CSS_FAILURE;
		break;
	}

	if (bufq->ready) {
		if (cssdev->rsvd_mem) {
			for (i=0; i < bufq->count; i++) {
				odin_css_physical_buffer_free(cssdev->rsvd_mem,
								&bufq->bufs[i]);
			}
		}

		if (bufq->bufs) {
			kzfree(bufq->bufs);
			bufq->bufs = NULL;
		}

		bufq->count = 0;
		bufq->ready = false;
		if (bufq->cam_client) {
			css_sys("cam_client[%d] destroy\n", capfh->buf_attrb.buf_type);
			odin_ion_client_destroy(bufq->cam_client);
			bufq->cam_client = NULL;
		}
	}

	drop_bufq = &cap_fb->drop_bufq;

	if (drop_bufq->ready && drop_bufq->bufs) {
		if (cssdev->rsvd_mem) {
			odin_css_physical_buffer_free(cssdev->rsvd_mem, drop_bufq->bufs);
		} else {
#ifdef CONFIG_ODIN_ION_SMMU
			struct ion_client *i_client;
			i_client = cap_fb->drop_bufq_client;

			if (drop_bufq->bufs->ion_hdl.handle) {
				drop_bufq->bufs->id		= -1;
				drop_bufq->bufs->state	= CSS_BUF_UNUSED;
				drop_bufq->bufs->offset	= 0;
				drop_bufq->bufs->byteused	= 0;
				drop_bufq->bufs->dma_addr	= 0;
				drop_bufq->bufs->cpu_addr	= NULL;
				drop_bufq->bufs->size		= 0;
				drop_bufq->bufs->ion_hdl.handle = NULL;
			}
			if (i_client) {
				odin_ion_client_destroy(i_client);
				cap_fb->drop_bufq_client = NULL;
			}
#else
			css_err("not supported ion\n");
			return CSS_FAILURE;
#endif
		}

		if (drop_bufq->bufs) {
			kzfree(drop_bufq->bufs);
			drop_bufq->bufs = NULL;
		}
		drop_bufq->count = 0;
		drop_bufq->ready = false;
	}

	return CSS_SUCCESS;
}

int capture_buffer_close(struct capture_fh *capfh)
{
	int i;
	struct css_device *cssdev = capfh->cssdev;
	struct css_framebuffer *cap_fb = capfh->fb;
	struct css_bufq *bufq = NULL;
	struct css_bufq *drop_bufq = NULL;

	bufq = &cap_fb->preview_bufq;
	if (bufq->ready) {
		if (cssdev->rsvd_mem) {
			for (i=0; i < bufq->count; i++)
				odin_css_physical_buffer_free(cssdev->rsvd_mem, &bufq->bufs[i]);
		}
		if (bufq->bufs) {
			kzfree(bufq->bufs);
			bufq->bufs = NULL;
		}
		bufq->count = 0;
		bufq->ready = false;

		if (bufq->cam_client) {
			css_sys("cam_client[preview] destroy\n");
			odin_ion_client_destroy(bufq->cam_client);
			bufq->cam_client = NULL;
		}
	}

	bufq = &cap_fb->capture_bufq;
	if (bufq->ready) {
		if (cssdev->rsvd_mem) {
			for (i=0; i < bufq->count; i++)
				odin_css_physical_buffer_free(cssdev->rsvd_mem, &bufq->bufs[i]);
		}
		if (bufq->bufs) {
			kzfree(bufq->bufs);
			bufq->bufs = NULL;
		}
		bufq->count = 0;
		bufq->ready = false;

		if (bufq->cam_client) {
			css_sys("cam_client[capture] destroy\n");
			odin_ion_client_destroy(bufq->cam_client);
			bufq->cam_client = NULL;
		}
	}

	bufq = &cap_fb->stereo_bufq;
	if (bufq->ready) {
		if (cssdev->rsvd_mem) {
			for (i=0; i < bufq->count; i++)
				odin_css_physical_buffer_free(cssdev->rsvd_mem, &bufq->bufs[i]);
		}
		if (bufq->bufs) {
			kzfree(bufq->bufs);
			bufq->bufs = NULL;
		}
		bufq->count = 0;
		bufq->ready = false;

		if (bufq->cam_client) {
			css_sys("cam_client[stereo] destroy\n");
			odin_ion_client_destroy(bufq->cam_client);
			bufq->cam_client = NULL;
		}
	}

	drop_bufq = &cap_fb->drop_bufq;

	if (drop_bufq->ready && drop_bufq->bufs) {
		if (cssdev->rsvd_mem) {
			odin_css_physical_buffer_free(cssdev->rsvd_mem, drop_bufq->bufs);
		} else {
#ifdef CONFIG_ODIN_ION_SMMU
			struct ion_client *i_client;
			i_client = cap_fb->drop_bufq_client;

			if (drop_bufq->bufs->ion_hdl.handle) {
				drop_bufq->bufs->id		= -1;
				drop_bufq->bufs->state	= CSS_BUF_UNUSED;
				drop_bufq->bufs->offset	= 0;
				drop_bufq->bufs->byteused	= 0;
				drop_bufq->bufs->dma_addr	= 0;
				drop_bufq->bufs->cpu_addr	= NULL;
				drop_bufq->bufs->size		= 0;
				drop_bufq->bufs->ion_hdl.handle = NULL;
			}
			if (i_client) {
				odin_ion_client_destroy(i_client);
				cap_fb->drop_bufq_client = NULL;
			}
#else
			css_err("not supported ion\n");
			return CSS_FAILURE;
#endif
		}

		if (drop_bufq->bufs) {
			kzfree(drop_bufq->bufs);
			drop_bufq->bufs = NULL;
		}
		drop_bufq->count = 0;
		drop_bufq->ready = false;
	}

	return CSS_SUCCESS;
}

void capture_buffer_flush(struct capture_fh *capfh)
{
	int i, buf_cnt = 0;
	unsigned long flags;
	struct css_bufq *bufq = NULL;
	struct css_buffer *cssbuf = NULL;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n", capfh->main_scaler);
		return;
	}

	buf_cnt = bufq->count;

	cssbuf = capture_buffer_pop(bufq);
	buf_cnt--;

	if (cssbuf != NULL) {
		css_sys("video%d flush buf[%d]-0x%08X!\n", capfh->main_scaler,
				cssbuf->id, (unsigned int)cssbuf->dma_addr);
	}

	while (cssbuf != NULL && buf_cnt > 0) {
		cssbuf = capture_buffer_pop(bufq);
		if (cssbuf != NULL) {
			css_sys("video%d flush buf[%d]-0x%08X!\n", capfh->main_scaler,
					cssbuf->id, (unsigned int)cssbuf->dma_addr);
		}
		buf_cnt--;
	}

	spin_lock_irqsave(&bufq->lock, flags);
	bufq->index = 0;
	bufq->q_pos = 0;
	for (i = 0; i < bufq->count; i++)
		bufq->bufs[i].state = CSS_BUF_UNUSED;
	spin_unlock_irqrestore(&bufq->lock, flags);

	return;
}

int capture_buffer_push(struct css_bufq *bufq, int i)
{
	struct css_buffer *tmp_buffer;
	struct list_head *count, *n;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	list_for_each_safe(count, n, &bufq->head) {
		tmp_buffer = list_entry(count, struct css_buffer, list);
		if (tmp_buffer->id == i) {
			spin_unlock_irqrestore(&bufq->lock, flags);
			css_warn("Exist id(%d) in queue\n", i);
			return 0;
		}
	}
	bufq->bufs[i].id = i;
	list_add_tail(&bufq->bufs[i].list, &bufq->head);
	spin_unlock_irqrestore(&bufq->lock, flags);
	return 0;
}

struct css_buffer *capture_buffer_pop(struct css_bufq *bufq)
{
	struct css_buffer *tmp_buffer;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	if (list_empty(&bufq->head)) {
		spin_unlock_irqrestore(&bufq->lock, flags);
		return NULL;
	} else {
		tmp_buffer = list_first_entry(&bufq->head, struct css_buffer, list);
		list_del(&tmp_buffer->list);
	}
	spin_unlock_irqrestore(&bufq->lock, flags);
	return tmp_buffer;
}

int capture_start(struct capture_fh *capfh, struct css_bufq *bufq,
			int reset_frame_cnt)
{
	int ret = CSS_SUCCESS;
	int buf_index = 0;
	unsigned int phy_addr = 0;
	unsigned int drop_mode = 0;
	struct css_scaler_device *scl_dev = NULL;
	struct css_scaler_config tmp_config;
	struct css_scaler_data tmp_data;

	if (bufq == NULL) {
		bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	} else {
		drop_mode = 1;
	}

	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("stream[%d] buffer is null!!\n", capfh->main_scaler);
		return CSS_FAILURE;
	}

	scl_dev = capfh->scl_dev;
	buf_index = bufq->index;

	if (buf_index < 0 || buf_index >= bufq->count) {
		css_err("stream[%d] buf index error %d\n", capfh->main_scaler,
				buf_index);
		return CSS_ERROR_BUFFER_INDEX;
	}

	phy_addr = (unsigned int)bufq->bufs[buf_index].dma_addr;

	if (phy_addr != 0) {

		bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;

#ifdef CONFIG_VIDEO_ODIN_S3DC
		if (capfh->buf_attrb.buf_type & CSS_BUF_3D_TYPE) {
			if (reset_frame_cnt == 1) {
				scaler_reset_frame_num(0);
				scaler_reset_frame_num(1);
			}
			s3dc_set_framebuffer_address(phy_addr);
			scaler_int_enable(INT_PATH_3D_0|INT_PATH_3D_1);
			s3dc_int_clear(INT_S3DC_FRAME_END);
			s3dc_int_enable(INT_S3DC_FRAME_END);
			scaler_set_capture_stereo_frame();
		} else
#endif
		{
			if (reset_frame_cnt == 1) {
				scaler_reset_frame_num(capfh->main_scaler);
			}

			scaler_set_buffers(capfh->main_scaler,
						&scl_dev->config, phy_addr);
			scaler_action_configure(capfh->main_scaler,
						&scl_dev->data, scl_dev->config.action);

			if (capfh->buf_attrb.buf_type == CSS_BUF_CAPTURE_WITH_POSTVIEW) {
				struct css_postview *postview = &capfh->cssdev->postview;

				phy_addr = (unsigned int)bufq->bufs[buf_index].dma_addr + \
								bufq->bufs[buf_index].offset;

				ret = capture_set_postview_config(postview, postview->scaler,
								&scl_dev->config.crop, phy_addr);

				if (ret == CSS_SUCCESS) {
					scaler_set_capture_frame_with_postview(capfh->main_scaler,
								postview->scaler);
				}

			} else {
				struct css_scaler_device *scl_dev = capfh->scl_dev;
				struct css_scaler_config *scl_config = NULL;
				struct css_scaler_data *scl_data = NULL;

				if (drop_mode) {
					tmp_config = scl_dev->config;
					tmp_config.dst_height = 16;
					tmp_data = scl_dev->data;
					scl_config = &tmp_config;
					scl_data = &tmp_data;
				} else {
					scl_config = &scl_dev->config;
					scl_data = &scl_dev->data;
				}
				ret = scaler_configure(capfh->main_scaler, scl_config, scl_data);
				if (ret == CSS_SUCCESS) {
					scaler_set_capture_frame(capfh->main_scaler);
				} else {
					css_err("capfh[%d] scl config fail %d\n",
						capfh->main_scaler, ret);
				}
			}
		}
	} else {
		css_err("capfh[%d] idx(%d) phy_addr = 0\n", capfh->main_scaler,
				buf_index);
	}

	return ret;
}

unsigned int capture_get_done_mask(struct capture_fh *capfh)
{
	int mask = 0;

	switch (capfh->main_scaler) {
	case CSS_SCALER_0:
		mask = PATH_0_BUSI_CAPTURE_DONE;
		break;
	case CSS_SCALER_1:
		mask = PATH_1_BUSI_CAPTURE_DONE;
		break;
	default:
		css_err("invalid scaler %d\n", capfh->main_scaler);
		break;
	}

	return mask;
}

int capture_wait_scaler_vsync(struct css_device *cssdev,
			css_scaler_select scaler, unsigned int msec)
{
	struct completion *wait = NULL;
	int timeout = 0;
	int intrpt = 0;

	switch (scaler) {
	case CSS_SCALER_0:
		wait = &cssdev->vsync0_done;
		INIT_COMPLETION(cssdev->vsync0_done);
		intrpt = INT_PATH0_VSYNC;
		break;
	case CSS_SCALER_1:
		wait = &cssdev->vsync1_done;
		INIT_COMPLETION(cssdev->vsync1_done);
		intrpt = INT_PATH1_VSYNC;
		break;
	case CSS_SCALER_FD:
		wait = &cssdev->vsync_fd_done;
		INIT_COMPLETION(cssdev->vsync_fd_done);
		intrpt = INT_FD_VSYNC;
		break;
	default:
		return CSS_FAILURE;
		break;
	}

	scaler_int_enable(intrpt);
	timeout = wait_for_completion_timeout(wait, msecs_to_jiffies(msec));
	scaler_int_disable(intrpt);
	if (timeout == 0) {
		css_sys("vsync%02d timeout!!\n", scaler);
	} else {
		css_sys("vsync%02d waitdone!!\n", scaler);
	}

	return timeout;
}

void capture_wait_any_scaler_vsync(unsigned int msec)
{
	unsigned int cur_cnt_0, frame_cnt_0, count;
	unsigned int cur_cnt_1, frame_cnt_1;

	count = msec * 100;
	cur_cnt_0 = scaler_get_frame_num(0);
	cur_cnt_1 = scaler_get_frame_num(1);

	if (cur_cnt_0 == 0 && cur_cnt_1 == 0)
		return;

	while (1) {
		frame_cnt_0 = scaler_get_frame_num(0);
		frame_cnt_1 = scaler_get_frame_num(1);
		if ((frame_cnt_0 != cur_cnt_0) || (frame_cnt_1 != cur_cnt_1))
			break;

		if (!count--) {
			css_info("-----> timeout");
			break;
		}
		usleep_range(10,10);
	}
}

int capture_skip_frame(struct css_device *cssdev, css_scaler_select scaler,
			unsigned int framecnt)
{
	unsigned int i = 0;

	css_sys("skip %d frame\n", framecnt);

	for (i = 0; i < framecnt; i++)
		capture_wait_scaler_vsync(cssdev, scaler, 100);

	return CSS_SUCCESS;
}

int capture_wait_done(unsigned int mask, unsigned int msec)
{
	int timeout = 0;
	int done_st;

	mask &= 0x7FFF;

	if (mask != 0) {
		timeout = msec;
		do {
			done_st = scaler_get_done_status();
			if (done_st & mask)	{
				return timeout;
			} else {
				usleep_range(1000, 1000);
			}

		} while (timeout-- > 0);

		timeout = 0;
	}

	return timeout;
}

int capture_stop(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_bufq *bufq = NULL;
	unsigned int done_mask;
	int timeout;

	if (capfh == NULL)
		return CSS_ERROR_CAPTURE_HANDLE;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);

#ifdef CONFIG_VIDEO_ODIN_S3DC
	 if (capfh->buf_attrb.buf_type & CSS_BUF_3D_TYPE) {
		int timeout;
		timeout = s3dc_wait_capture_done(1000);
		if (timeout == 0) {
			css_info("s3dc capture timeout!!\n");
		}
		scaler_int_disable(INT_PATH_3D_0|INT_PATH_3D_1);
		scaler_int_clear(INT_PATH_3D_0|INT_PATH_3D_1);
		s3dc_int_disable(INT_S3DC_FRAME_END);
		s3dc_int_clear(INT_S3DC_FRAME_END);
	 } else
#endif
	 {
#if 0
	 	if (bufq && bufq->bufs) {
			if (bufq->mode == STREAM_MODE && !bufq->error) {
				done_mask = capture_get_done_mask(capfh);
				timeout = capture_wait_done(done_mask, 1000);
				if (timeout == 0) {
					css_info("video%d capture timeout!!\n", capfh->main_scaler);
				}
			}
	 	}
#endif
		if (bufq) {
			bufq->capt_tot = 0;
		}
		switch (capfh->main_scaler) {
		case CSS_SCALER_0:
			scaler_int_disable(INT_PATH0_BUSI);
			scaler_int_clear(INT_PATH0_BUSI);
			scaler_path_reset(PATH_0_RESET);
			break;
		case CSS_SCALER_1:
			scaler_int_disable(INT_PATH1_BUSI);
			scaler_int_clear(INT_PATH1_BUSI);
			scaler_path_reset(PATH_1_RESET);
			break;
		case CSS_SCALER_FD:
			scaler_int_disable(INT_PATH_FD);
			scaler_int_clear(INT_PATH_FD);
			scaler_path_reset(PATH_FD_RESET);
			break;
		default:
			css_warn("undefined scaler!!\n");
			break;
		}
	 }

	return ret;
}

static int capture_intr_handler_fd_path_done(struct css_device *cssdev)
{
	int result = CSS_SUCCESS;
	struct css_fd_device *fd_dev = cssdev->fd_device;
	struct face_detect_config *config_data = NULL;
	struct css_scaler_data *fd_scaler_config = NULL;
	struct css_crop_data crop_data;
	unsigned int i, buf_index = 0;
	unsigned int old_buf = 0;

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

	buf_index = fd_dev->buf_index;

	if (buf_index >= FACE_BUF_MAX) {
		css_err("fd buf index error %d\n", buf_index);
		return CSS_ERROR_FD_BUF_INDEX;
	}

	fd_dev->bufs[buf_index].state = CSS_BUF_CAPTURE_DONE;

	scaler_int_disable(INT_PATH_FD);
	scaler_int_clear(INT_PATH_FD);

	if (fd_dev->fd_run != 1) {
		css_fd("**********DEBUG FD INT : buf =%d \n", buf_index);
		result = odin_face_detection_run(buf_index);
		if (result == CSS_SUCCESS) {

#ifdef USE_FPD_HW_IP
			for (i = 0; i < FACE_BUF_MAX; i++) {
				buf_index++;
				buf_index = buf_index % FACE_BUF_MAX;
				if (fd_dev->fd_info[buf_index].state == FD_READY)
					break;
			}
#else
			for (i = 0; i < FACE_BUF_MAX; i++)
				css_fd("DEBUG FD INT: buf = %d, state = %d,new =%d\n", i,
					fd_dev->fd_info[i].state, fd_dev->fd_info[i].buf_newest);

			buf_index = 0;
			for (i = 0; i < FACE_BUF_MAX; i++) {
				/* find READY & the biggest(old) one */
				if (fd_dev->fd_info[i].state == FD_READY) {
					if (old_buf < fd_dev->fd_info[i].buf_newest) {
						old_buf = fd_dev->fd_info[i].buf_newest;
						buf_index = i;
					}
				}
			}

			for (i = 0; i < FACE_BUF_MAX; i++) {
				if (fd_dev->fd_info[i].state == FD_READY) {
					break;
				}
			}
#endif
			if (i == FACE_BUF_MAX) {
				/* must not happen!! */
				css_err("FD info is FULL\n");
				for (i = 0; i < FACE_BUF_MAX; i++) {
					css_err("ERR: buf = %d, state = %d,new =%d\n", i,
							fd_dev->fd_info[i].state,
							fd_dev->fd_info[i].buf_newest);
				}
				result = CSS_ERROR_FD_BUF_FULL;
			}
		}
	} else {
		css_fd("FD Already RUN\n");
	}

	/* printk("FD NEXT = %d\n", buf_index); */

	if (result == CSS_SUCCESS) {
		css_fd("**********DEBUG FD CAPTURE : buf =%d \n", buf_index);

		/* 4) scaler set buffers */
		if (fd_dev->bufs[buf_index].dma_addr != 0) {

			fd_dev->buf_index = buf_index;

			fd_dev->bufs[buf_index].state = CSS_BUF_CAPTURING;

			result = fd_set_buffer(CSS_SCALER_FD,
								fd_dev->bufs[buf_index].dma_addr);
#ifdef CONFIG_ODIN_ION_SMMU
			fd_dev->fd_info[buf_index].ion_hdl.handle
									= fd_dev->bufs[buf_index].ion_hdl.handle;
			fd_dev->fd_info[buf_index].ion_cpu_addr
									= fd_dev->bufs[buf_index].ion_cpu_addr;
#endif

			/* 5) scaler set capture frame */
			config_data = &fd_dev->config_data;
			fd_scaler_config = &fd_dev->fd_scaler_config;

			crop_data.src.width = config_data->fd_scaler_src_width;
			crop_data.src.height = config_data->fd_scaler_src_height;

			crop_data.crop.start_x = config_data->crop.start_x;
			crop_data.crop.start_y = config_data->crop.start_y;
			crop_data.crop.width = config_data->crop.width;
			crop_data.crop.height = config_data->crop.height;

			crop_data.dst.width = config_data->width;
			crop_data.dst.height = config_data->height;

			result = scaler_set_crop_n_masksize(CSS_SCALER_FD,
							crop_data, fd_scaler_config);
			if (result == CSS_SUCCESS) {
				fd_dev->fd_info[buf_index].crop.start_x =
														crop_data.crop.start_x;
				fd_dev->fd_info[buf_index].crop.start_y =
														crop_data.crop.start_y;
				fd_dev->fd_info[buf_index].crop.width = crop_data.crop.width;
				fd_dev->fd_info[buf_index].crop.height = crop_data.crop.height;

				/* 6) scaler set capture frame */
				result = scaler_action_configure(CSS_SCALER_FD,
								fd_scaler_config, config_data->action);
				result = scaler_set_capture_frame(CSS_SCALER_FD);
			} else {
				fd_dev->bufs[buf_index].state = CSS_BUF_UNUSED;
				css_err("invalid FD crop size\n");
				result = CSS_ERROR_FD_BUF_INVALID_SIZE;
			}
		} else {
			css_err("FD buffer is NULL\n");
			result = CSS_ERROR_FD_INVALID_BUF;
			/* BUG_ON(fd_dev->bufs[buf_index].dma_addr == 0); */
		}
	}

	return result;
}

static int capture_intr_handler_scaler_path_done(struct capture_fh *capfh,
			struct css_device *cssdev)
{
	int ret = CSS_SUCCESS;
	int capture_index = 0;
	int buf_index = 0;
	int scl_num;
	unsigned long flags;
	struct css_bufq *bufq = NULL;
	struct css_bufq *drop_bufq = NULL;
	struct timespec ts;

	scl_num = capfh->main_scaler;

	switch (scl_num) {
	case CSS_SCALER_0:
		scaler_int_disable(INT_PATH0_BUSI);
		scaler_int_clear(INT_PATH0_BUSI);
		break;
	case CSS_SCALER_1:
		scaler_int_disable(INT_PATH1_BUSI);
		scaler_int_clear(INT_PATH1_BUSI);
		break;
	default:
		break;
	}

	if (capfh->scl_dev == NULL) {
		css_err("scl_dev is NULL (capfh[%d])!\n", capfh->main_scaler);
		return CSS_FAILURE;
	}

	if (capture_get_stream_mode(capfh) == 0) {
		css_err("stream is off (capfh[%d])!\n", capfh->main_scaler);
		return CSS_FAILURE;
	}

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n",
				capfh->main_scaler);
		return CSS_FAILURE;
	}

	spin_lock_irqsave(&bufq->lock, flags);
	bufq->capt_tot++;
	spin_unlock_irqrestore(&bufq->lock, flags);

	capture_index = capture_buffer_get_capture_index(bufq);
	if (capture_index >= 0) {
		struct css_scaler_config *scl_config = NULL;

		scl_config = &capfh->scl_dev->config;

		spin_lock_irqsave(&bufq->lock, flags);
		bufq->q_pos = capture_index;
		spin_unlock_irqrestore(&bufq->lock, flags);

		if (capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL) {
		#if 1
			int ispidx = 0;
			ispidx = odin_isp_wrap_get_isp_idx(scl_num);
			bufq->bufs[bufq->q_pos].frame_cnt
				= odin_isp_get_vsync_count(ispidx);
			css_low("frm[%d:%d]:%d\n", scl_num, ispidx,
				bufq->bufs[bufq->q_pos].frame_cnt);
		#else
			bufq->bufs[bufq->q_pos].frame_cnt
				= scaler_get_frame_num(scl_num);
		#endif
		}

		bufq->bufs[bufq->q_pos].byteused = scaler_get_frame_size(scl_num);
		bufq->bufs[bufq->q_pos].width = scaler_get_frame_width(scl_num);
		bufq->bufs[bufq->q_pos].height = scaler_get_frame_height(scl_num);
		bufq->bufs[bufq->q_pos].state = CSS_BUF_CAPTURE_DONE;

		ktime_get_ts(&ts);
		bufq->bufs[bufq->q_pos].timestamp.tv_sec = (long)ts.tv_sec;
		bufq->bufs[bufq->q_pos].timestamp.tv_usec = ts.tv_nsec;
		capture_buffer_push(bufq, bufq->q_pos);
		wake_up_interruptible(&bufq->complete.wait);

		capfh->scl_dev->roop_err_cnt = 0;

		buf_index = capture_buffer_get_next_index(bufq);
		if (buf_index >= 0) {
			css_low("scl%d next fr=%d\n", scl_num, buf_index);
			spin_lock_irqsave(&bufq->lock, flags);
			bufq->index = buf_index;
			spin_unlock_irqrestore(&bufq->lock, flags);
		} else {
			drop_bufq = &(capfh->fb->drop_bufq);
		}
	} else
		drop_bufq = &(capfh->fb->drop_bufq);

	if (capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL) {
		if (bufq->mode == STREAM_MODE) {
			if (drop_bufq) {
				buf_index = capture_buffer_get_next_index(bufq);
				if (buf_index >= 0) {
					css_low("scl%d next fr=%d\n", scl_num, buf_index);
					spin_lock_irqsave(&bufq->lock, flags);
					bufq->index = buf_index;
					spin_unlock_irqrestore(&bufq->lock, flags);
					drop_bufq = NULL;
				}
			}
			if (drop_bufq) {
				if (buf_index == -2) {
					spin_lock_irqsave(&bufq->lock, flags);
					bufq->skip_cnt++;
					spin_unlock_irqrestore(&bufq->lock, flags);
					css_low("scl_%d skip : drop frame!! capture_index=%d\n",
						scl_num, capture_index);
				} else {
					css_low("scl_%d queue full : drop frame!! capture_index=%d\n",
						scl_num, capture_index);
				}
				spin_lock_irqsave(&bufq->lock, flags);
				bufq->drop_cnt++;
				spin_unlock_irqrestore(&bufq->lock, flags);
				ret = capture_start(capfh, drop_bufq, 0);
			} else {
				spin_lock_irqsave(&bufq->lock, flags);
				bufq->capt_cnt++;
				spin_unlock_irqrestore(&bufq->lock, flags);
				ret = capture_start(capfh, NULL, 0);
			}
		}
	}
	return ret;
}

static void capture_intr_handler_roop_error(struct capture_fh *capfh)
{
	int capture_scaler = 0;
	css_zsl_path r_path = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_scaler_device *scl_dev = NULL;
	struct css_bufq *bufq = NULL;
	unsigned int resetsig;

	if (capfh == NULL)
		return;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL)
		return;

	scl_dev = capfh->scl_dev;
	if (scl_dev == NULL)
		return;

	capture_scaler = capfh->main_scaler;
	if (capture_scaler == 0) {
		r_path = ZSL_READ_PATH1;
		resetsig = PATH_0_RESET;
	} else {
		r_path = ZSL_READ_PATH0;
		resetsig = PATH_1_RESET;
	}

	zsl_dev = capture_get_zsl_dev(capfh, r_path);

	if (zsl_dev == NULL)
		return;

	if (zsl_dev->load_running) {
		scaler_path_reset(resetsig);
		zsl_dev->error = 1;
		if (capture_scaler == 0) {
			capture_set_error(ERR_ZSL_PATH0_ROOP);
			css_int_err("path0-roop(zsl)\n");
		} else {
			capture_set_error(ERR_ZSL_PATH1_ROOP);
			css_int_err("path1-roop(zsl)\n");
		}
		return;
	}

	css_int_err("path%d-roop\n", capture_scaler);

	scl_dev->roop_err_cnt++;

	if (scl_dev->roop_err_cnt > 100) {
		scaler_path_reset(resetsig);
		bufq->error = 1;
		if (capture_scaler == 0) {
			capture_set_error(ERR_ZSL_PATH0_ROOTOUT);
			css_int_err("path0-roop out\n");
		} else {
			capture_set_error(ERR_ZSL_PATH1_ROOTOUT);
			css_int_err("path1-roop out\n");
		}
		wake_up_interruptible(&bufq->complete.wait);
		return;
	}

	scaler_path_reset(resetsig);
	if (capture_get_stream_mode(capfh))
		capture_start(capfh, NULL, 0);
}

static void capture_intr_handler_bof_error(struct capture_fh *capfh)
{
	int capture_scaler	= 0;
	css_zsl_path r_path = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	unsigned int resetsig;

	if (capfh == NULL)
		return;

	capture_scaler = capfh->main_scaler;
	if (capture_scaler == 0) {
		r_path = ZSL_READ_PATH1;
		resetsig = PATH_0_RESET;
	} else {
		r_path = ZSL_READ_PATH0;
		resetsig = PATH_1_RESET;
	}

	zsl_dev = capture_get_zsl_dev(capfh, r_path);
	if (zsl_dev == NULL)
		return;

	if (zsl_dev->load_running) {
		scaler_path_reset(resetsig);
		zsl_dev->error = 1;
		if (capture_scaler == 0) {
			capture_set_error(ERR_ZSL_PATH0_BOF);
			css_int_err("path0-bof(zsl)\n");
		} else {
			capture_set_error(ERR_ZSL_PATH1_BOF);
			css_int_err("path1-bof(zsl)\n");
		}
		return;
	}

	css_int_err("path%d-bof\n", capture_scaler);

	scaler_path_reset(resetsig);
	if (capture_get_stream_mode(capfh))
		capture_start(capfh, NULL, 0);
}

static int capture_intr_handler_scaler_error(unsigned int err_state)
{
	int ret = CSS_SUCCESS;
#ifdef ENABLE_ERR_INT_CODE
	struct capture_fh *capfh = NULL;

	capture_set_error(err_state);

	if (ERR_PATH0_BUF_OVERFLOW & err_state) {
		capfh = g_capfh[CSS_SCALER_0];
		if (capfh)
			capture_intr_handler_bof_error(capfh);
	}

	if (ERR_PATH0_ROOP & err_state) {
		capfh = g_capfh[CSS_SCALER_0];
		if (capfh)
			capture_intr_handler_roop_error(capfh);
	}

	if (ERR_PATH1_BUF_OVERFLOW & err_state) {
		capfh = g_capfh[CSS_SCALER_1];
		if (capfh)
			capture_intr_handler_bof_error(capfh);
	}

	if (ERR_PATH1_ROOP & err_state) {
		capfh = g_capfh[CSS_SCALER_1];
		if (capfh)
			capture_intr_handler_roop_error(capfh);
	}

	if (ERR_FD_BUF_OVERFLOW & err_state) {
		css_int_err("fd-bof\n");
	}

	if (capture_get_stereo_status((capfh = g_capfh[CSS_SCALER_0]) \
		== NULL ? NULL : g_capfh[CSS_SCALER_0]->cssdev)) {
		struct css_zsl_device *zsl_dev = NULL;
		struct css_zsl_config *zsl_config = NULL;

		zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
		zsl_config = &zsl_dev->config;

		if ((ERR_PATH0_STORE_BUF_OVERFLOW | ERR_PATH1_STORE_BUF_OVERFLOW)
			& err_state) {
			if (zsl_config->enable) {
				css_int_err("zsl[%d][0/1] bof\n", zsl_dev->path);
				css_zsl_low("zsl[%d] frame drop!!\n", zsl_dev->path);
				zsl_config->enable = 0;
				scaler_path_reset(PATH_RAW0_ST_RESET | RAW0_ST_CTRL_RESET| \
									PATH_RAW1_ST_RESET | RAW1_ST_CTRL_RESET);
				scaler_bayer_store_enable(zsl_dev->path, zsl_config);
			} else {
				complete(&zsl_dev->store_done);
			}
		}
	} else {
		if (ERR_PATH0_STORE_BUF_OVERFLOW & err_state) {
			struct css_zsl_device *zsl_dev = NULL;
			struct css_zsl_config *zsl_config = NULL;

			capfh = g_capfh[CSS_SCALER_0];
			if (capfh) {
				zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);

				zsl_config = &zsl_dev->config;
				if (zsl_config->enable) {
					css_int_err("zsl[0] bof\n");
					css_zsl_low("zsl[0] frame drop!!\n");
					zsl_config->enable = 0;
					scaler_path_reset(PATH_RAW0_ST_RESET|RAW0_ST_CTRL_RESET);
					if (zsl_config->next_capture_index == 0) {
						struct css_zsl_config zconfig;
						memcpy(&zconfig, zsl_config,
								sizeof(struct css_zsl_config));
						zconfig.img_size.height = ZSL_STORE_DROP_CAPTURE_LINE;
						scaler_bayer_store_enable(zsl_dev->path, &zconfig);
					} else {
						scaler_bayer_store_enable(zsl_dev->path, zsl_config);
					}
				}
				complete(&zsl_dev->store_done);
			}
		}
		if (ERR_PATH1_STORE_BUF_OVERFLOW & err_state) {
			struct css_zsl_device *zsl_dev = NULL;
			struct css_zsl_config *zsl_config = NULL;

			capfh = g_capfh[CSS_SCALER_1];
			if (capfh) {
				zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_1);

				zsl_config = &zsl_dev->config;
				if (zsl_config->enable) {
					css_int_err("zsl[1] bof\n");
					css_zsl_low("zsl[1] frame drop!!\n");
					zsl_config->enable = 0;
					scaler_path_reset(PATH_RAW1_ST_RESET|RAW1_ST_CTRL_RESET);
					if (zsl_config->next_capture_index == 0) {
						struct css_zsl_config zconfig;
						memcpy(&zconfig, zsl_config,
								sizeof(struct css_zsl_config));
						zconfig.img_size.height = ZSL_STORE_DROP_CAPTURE_LINE;
						scaler_bayer_store_enable(zsl_dev->path, &zconfig);
					} else {
						scaler_bayer_store_enable(zsl_dev->path, zsl_config);
					}
				}
				complete(&zsl_dev->store_done);
			}
		}
	}

	if (ERR_JPEG_STREAM_BUF_OVERFLOW & err_state) {
		css_int_err("jpeg-bof\n");
	}

	if (ERR_JPEG_INTERFACE_ERR & err_state) {
		css_int_err("jpeg-interface err\n");
	}
#endif
	return ret;
}

irqreturn_t capture_irq(int irq, void *dev_id)
{
	unsigned int int_status = 0;
	struct css_device *cssdev = dev_id;
	struct capture_fh *capfh = NULL;

	CSS_TIME_DEFINE(int);

	if (scaler_get_ready_state() == 0)
		return IRQ_HANDLED;
	/*
	 * get scaler int status and
	 */
	int_status = scaler_get_int_status();
	/* int_status &= ~(scaler_get_int_mask()); */

	if (int_status & INT_ERROR) {
		scaler_int_disable(INT_ERROR);
		css_cam_int("cam int = 0x%08x %x\n", (unsigned int)int_status,
					scaler_get_err_status());
	} else {
		css_cam_int("cam int = 0x%08x\n", (unsigned int)int_status);
	}

	CSS_TIME_START(int);

	if (int_status & INT_ERROR) {
		unsigned int err_state;

		err_state = scaler_get_err_status();
		scaler_int_clear(INT_ERROR);
		capture_intr_handler_scaler_error(err_state);
	}

	if (capture_get_stereo_status(cssdev) &&
		int_status & (INT_PATH_RAW0_STORE | INT_PATH_RAW1_STORE)) {
		bayer_stereo_store_intr_handler(cssdev, int_status);
	} else {
		if (int_status & INT_PATH_RAW0_STORE) {
			capfh = g_capfh[CSS_SCALER_0];
			if (capfh)
				bayer_store_int_handler(capfh);
		}

		if (int_status & INT_PATH_RAW1_STORE) {
			capfh = g_capfh[CSS_SCALER_1];
			if (capfh)
				bayer_store_int_handler(capfh);
		}
	}

	if (int_status & INT_PATH0_BUSI) {
		capfh = g_capfh[CSS_SCALER_0];
		if (capfh)
			capture_intr_handler_scaler_path_done(capfh, cssdev);
	}

	if (int_status & INT_PATH1_BUSI) {
		capfh = g_capfh[CSS_SCALER_1];
		if (capfh)
			capture_intr_handler_scaler_path_done(capfh, cssdev);
	}

	if (int_status & INT_PATH_FD)
		capture_intr_handler_fd_path_done(cssdev);

	if (int_status & INT_PATH_RAW0_LOAD) {
		scaler_int_disable(INT_PATH_RAW0_LOAD);
		scaler_int_clear(INT_PATH_RAW0_LOAD);
		css_cam_int("raw0 load int!!\n");
	}

	if (int_status & INT_PATH_RAW1_LOAD) {
		scaler_int_disable(INT_PATH_RAW1_LOAD);
		scaler_int_clear(INT_PATH_RAW1_LOAD);
		css_cam_int("raw1 load int!!\n");
	}

	if (int_status & INT_PATH_3D_0) {
		scaler_int_disable(INT_PATH_3D_0);
#ifdef CONFIG_VIDEO_ODIN_S3DC
		s3dc_set_frame_count(scaler_get_frame_num(CSS_SCALER_0));
#endif
		scaler_int_clear(INT_PATH_3D_0);
		css_cam_int("3d_0 int!!\n");
	}

	if (int_status & INT_PATH_3D_1) {
		scaler_int_disable(INT_PATH_3D_1);
		scaler_int_clear(INT_PATH_3D_1);
		css_cam_int("3d_1 int!!\n");
	}

	if (int_status & INT_PATH0_VSYNC) {
		scaler_int_disable(INT_PATH0_VSYNC);
		scaler_int_clear(INT_PATH0_VSYNC);
		complete(&cssdev->vsync0_done);
		css_cam_int("vsync0 int!!\n");
	}

	if (int_status & INT_PATH1_VSYNC) {
		scaler_int_disable(INT_PATH1_VSYNC);
		scaler_int_clear(INT_PATH1_VSYNC);
		complete(&cssdev->vsync1_done);
		css_cam_int("vsync1 int!!\n");
	}

	if (int_status & INT_FD_VSYNC) {
		scaler_int_disable(INT_FD_VSYNC);
		scaler_int_clear(INT_FD_VSYNC);
		complete(&cssdev->vsync_fd_done);
		css_cam_int("vsyncFD int!!\n");
	}

	if (int_status & INT_ERROR)
		scaler_int_enable(INT_ERROR);

	CSS_TIME_END(int);
	CSS_TIME_RESULT(cam_int, "int total(%ld) us\n", CSS_TIME(int));

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static struct of_device_id capture_match[] = {
	{
		.compatible = CSS_OF_CAPTURE_NAME,
		.data		= &sensor_group,
	},
	{},
};
#endif /* CONFIG_OF */

static struct css_device *capture_global_init(void)
{
	struct css_device *cssdev = NULL;

	cssdev = (struct css_device*)kzalloc(sizeof(struct css_device), GFP_KERNEL);

	if (!cssdev) {
		css_err("cssdev memory alloc fail\n");
		return NULL;
	}

	cssdev->zsl_device[0].path = ZSL_STORE_PATH0;
	cssdev->zsl_device[1].path = ZSL_STORE_PATH1;

	cssdev->postview.width	= CSS_POSTVIEW_DEFAULT_W;
	cssdev->postview.height	= CSS_POSTVIEW_DEFAULT_H;
	cssdev->postview.fmt	= V4L2_PIX_FMT_UYVY;
	cssdev->postview.size	= capture_get_frame_size(cssdev->postview.fmt,
													cssdev->postview.width,
													cssdev->postview.height);

	cssdev->ispclk.clk_hz[0]	= MINIMUM_ISP_PERFORMANCE;
	cssdev->ispclk.clk_hz[1]	= MINIMUM_ISP_PERFORMANCE;
	cssdev->ispclk.set_clk_hz	= 0;
	cssdev->ispclk.cur_clk_hz	= 0;

	if (css_get_chip_revision() >= 0x2)
		cssdev->livepath_support = true;
	else
		cssdev->livepath_support = false;

	cssdev->path_mode = CSS_NORMAL_PATH_MODE;

	g_cssdev = cssdev;

	return cssdev;
}

static void capture_global_deinit(struct css_device *cssdev)
{
	if (cssdev)
		kzfree(cssdev);

	return;
}

static int capture_framebuffer_init(struct css_device *cssdev,
				struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res)
		return CSS_ERROR_GET_RESOURCE;

	cssdev->rsvd_mem = odin_css_create_mem_pool(res->start,
								resource_size(res), pdev);
	if (!cssdev->rsvd_mem)
		return CSS_ERROR_GET_RESOURCE;

	css_info("memory(cam): 0x%08x 0x%08x\n",
			(u32)res->start, (u32)resource_size(res));

	return ret;
}

static int capture_framebuffer_deinit(struct css_device *cssdev)
{
	int ret = CSS_SUCCESS;

	if (cssdev->rsvd_mem) {
		ret = odin_css_destroy_mem_pool(cssdev->rsvd_mem);
		cssdev->rsvd_mem = NULL;
	}

	return ret;
}

extern void bayer_work_loadpath0(struct work_struct *work);
extern void bayer_work_loadpath1(struct work_struct *work);

int capture_probe(struct platform_device *pdev)
{
	int ret = -EBUSY;
	int i;
	struct css_device *cssdev = NULL;
	struct css_platform_sensor_group *pdata = NULL;

#if CONFIG_OF
	const struct of_device_id *of_id;
	of_id = of_match_device(capture_match, &pdev->dev);
	pdev->dev.platform_data = (void*)of_id->data;
#endif

	pdata = pdev->dev.platform_data;

	cssdev = capture_global_init();
	if (!cssdev) {
		css_err("css_device alloc fail\n");
		return -ENOMEM;
	}

	cssdev->pdev = pdev;

	platform_set_drvdata(pdev, cssdev);

	css_power_domain_init_on(POWER_ALL);

	scaler_register_isr(&cssdev->scl_hw, capture_irq);

	if (platform_get_resource(pdev, IORESOURCE_MEM, 0) != NULL) {
		ret = capture_framebuffer_init(cssdev, pdev);
		if (ret != CSS_SUCCESS)
			goto error_video_register;
	}

	/* initialize sync object and bottom half */
	INIT_WORK(&cssdev->work_bayer_loadpath0, bayer_work_loadpath0);
	INIT_WORK(&cssdev->work_bayer_loadpath1, bayer_work_loadpath1);

	INIT_WORK(&reportwq, capture_fps_log);
	mutex_init(&report_mutex);
	report_init = 0;
	css_suspend = 0;

#ifdef ODIN_CAMERA_ERR_REPORT
	INIT_WORK(&err_report, capture_err_log_wq);
#endif

	mutex_init(&cssdev->bayer_mutex);

	init_completion(&cssdev->vsync0_done);
	init_completion(&cssdev->vsync1_done);
	init_completion(&cssdev->vsync_fd_done);

	ret = camera_v4l2_init(cssdev);
	if (ret != CSS_SUCCESS)
		goto error_video_register;

#if 0
	int index;

	index = camera_detect_sensor(pdata,0);
	cssdev->sensor[SENSOR_REAR] = pdata->sensor[index];
	for (i = 2; i < SENSOR_MAX; i++) {
		cssdev->sensor[i-1] = pdata->sensor[i];
	}
#else
	for (i = 0; i < SENSOR_MAX; i++)
		cssdev->sensor[i] = pdata->sensor[i];
#endif

#ifdef CAM_USE_SYSFS_LOG_LEVEL
	ret = device_create_file(&(pdev->dev), &dev_attr_log_level);
	if (ret < 0) {
		css_err("failed to add sysfs entries for log level\n");
		goto error_video_register;
	}
#endif

#ifdef CONFIG_ARCH_MVVICTORIA
	odin_css_init();
#endif

	cssdev->capfh = (unsigned int*)&g_capfh[0];

	css_power_domain_init_off(POWER_ALL);

	css_log_init(LOG_ERR | LOG_WARN | LOG_INFO);

	css_info("odin-capture driver probe(%d) success!\n",
			css_get_chip_revision());

	return CSS_SUCCESS;

error_video_register:
	if (cssdev && cssdev->rsvd_mem)
		capture_framebuffer_deinit(cssdev);
	if (cssdev)
		capture_global_deinit(cssdev);

	return ret;
}

int capture_remove(struct platform_device *pdev)
{
	struct css_device *cssdev = platform_get_drvdata(pdev);

	BUG_ON(cssdev == NULL);

#ifdef CAM_USE_SYSFS_LOG_LEVEL
	device_remove_file(&(pdev->dev), &dev_attr_log_level);
#endif
	css_suspend = 1;
	camera_v4l2_release(cssdev);
	if (cssdev && cssdev->rsvd_mem)
		capture_framebuffer_deinit(cssdev);

	if (cssdev) {
		capture_global_deinit(cssdev);
		cssdev = NULL;
	}

	css_info("camera driver remove success!");

	return 0;
}

int capture_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	css_suspend = 1;
	css_power_domain_suspend();
	return ret;
}

int capture_resume(struct platform_device *pdev)
{
	int ret = 0;
	css_power_domain_resume();
	css_suspend = 0;
	return ret;
}

struct platform_driver capture_driver =
{
	.probe		= capture_probe,
	.remove 	= capture_remove,
	.suspend	= capture_suspend,
	.resume 	= capture_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_CAPTURE,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(capture_match),
#endif
	},
};

static int __init capture_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&capture_driver);

	return ret;
}

static void __exit capture_exit(void)
{
	platform_driver_unregister(&capture_driver);
	return;
}

MODULE_DESCRIPTION("odin camera driver for V4L2");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(capture_init);
module_exit(capture_exit);

MODULE_LICENSE("GPL");

