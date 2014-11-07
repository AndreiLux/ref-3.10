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

#include <asm/system_info.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>

#include "odin-css.h"
#include "odin-css-utils.h"
#include "odin-css-platdrv.h"
#include "odin-css-clk.h"
#include "odin-css-system.h"
#include "odin-facedetection.h"
#include "odin-framegrabber.h"
#include "odin-mdis.h"
#include "odin-mipicsi.h"
#include "odin-s3dc.h"
#include "odin-isp.h"
#include "odin-isp-wrap.h"
#include "odin-capture.h"
#include "odin-bayercontrol.h"
#include "odin-camera.h"
#ifdef CONFIG_VIDEO_ODIN_FLASH
#include "odin-flash.h"
#endif

#define ODIN_CAMERA_MINOR 0

typedef struct {
	int  index;
	char dev_name[256];
} ODIN_VIDEO_DEV_INFO;


static DEFINE_MUTEX(devinit);

/*
 * Rear  : video0 - Preview, video1 - Capture or Video
 * Front : video0 - Capture or Video, video1 - Preview
 */
static const ODIN_VIDEO_DEV_INFO video_dev_name[] = {
	{0, "video0"},
	{1, "video1"},
};

int camera_fd_init(struct capture_fh *capfh)
{
	int result = CSS_SUCCESS;

	result = odin_fd_init_resource(capfh->cssdev);

	return result;
}

int camera_fd_deinit(struct capture_fh *capfh)
{
	int result = CSS_SUCCESS;

	result = odin_fd_deinit_resource(capfh->cssdev);

	return result;
}

int camera_fd_start(struct capture_fh *capfh, int select)
{
	int result = CSS_SUCCESS;
	unsigned int buf_index = 0;
#ifndef CONFIG_ODIN_ION_SMMU
	int i;
#endif

	struct css_fd_device *fd_dev = capfh->cssdev->fd_device;
	struct face_detect_config *config_data = NULL;

	struct css_scaler_config *scaler_config = &capfh->scl_dev->config;
	struct css_scaler_data *fd_scaler_config = NULL;

	css_info("camera_fd_start\n");

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

	fd_dev->start = 1;

	odin_fd_hw_init(capfh->cssdev);
#ifdef USE_FPD_HW_IP
	odin_fpd_hw_init(capfh->cssdev);
#endif

	/* 1) fd scaler power on & clk on			*/
	scaler_init_device(CSS_SCALER_FD);

	/* 2) source path							*/
	config_data = &fd_dev->config_data;
	fd_scaler_config = &fd_dev->fd_scaler_config;

	if (select == 0) {
		config_data->fd_source = FD_SOURCE_PATH0;
	} else {
		config_data->fd_source = FD_SOURCE_PATH1;
	}

	/*
	 * 3) scaler config
	 *  cf. scaler config must set previously.
	 *  v4l2 set scaler config.
	 */
	config_data->fd_scaler_src_width = scaler_config->src_width;
	config_data->fd_scaler_src_height = scaler_config->src_height;
	config_data->action = CSS_SCALER_ACT_CAPTURE_FD;
	css_fd("config_data->fd_scaler_src_width: %d\n",
			config_data->fd_scaler_src_width);
	css_fd("config_data->fd_scaler_src_height: %d\n",
			config_data->fd_scaler_src_height);

	result = fd_scaler_configure(CSS_SCALER_FD, config_data, fd_scaler_config);
	if (result != CSS_SUCCESS) {
		css_err("scaler_fd configure err=%d\n", result);
	}

	/* 4) alloc buffers						*/
#ifndef CONFIG_ODIN_ION_SMMU
	for (i = 0; i< FACE_BUF_MAX; i++)
	{
#ifdef USE_FPD_HW_IP	/* work area = H/W fixed , don't use dram */
		fd_dev->bufs[i].size = fd_dev->config_data.width
								* fd_dev->config_data.height
								+ 512	/*input face information area*/
								+ 4096	/*output result area*/;
#else
		fd_dev->bufs[i].size = fd_dev->config_data.width
								* fd_dev->config_data.height;
#endif
		fd_dev->bufs[i].cpu_addr = dma_alloc_coherent(NULL,
								fd_dev->bufs[i].size,
								&fd_dev->bufs[i].dma_addr, GFP_KERNEL);
	}
#endif

	/* 5) scaler set buffers					*/
	fd_dev->buf_index = 0;
	buf_index = fd_dev->buf_index;

	css_fd("odin_camera_fd : scaler set buffers\n");
	if (fd_dev->bufs[buf_index].dma_addr != 0) {

		css_fd("odin_camera_fd : scaler set buffers 0x%x\n",
								fd_dev->bufs[buf_index].dma_addr);

		fd_dev->bufs[buf_index].state = CSS_BUF_CAPTURING;

		result = fd_set_buffer(CSS_SCALER_FD, fd_dev->bufs[buf_index].dma_addr);
#ifdef CONFIG_ODIN_ION_SMMU
		fd_dev->fd_info[buf_index].ion_hdl.handle
								= fd_dev->bufs[buf_index].ion_hdl.handle;
		fd_dev->fd_info[buf_index].ion_cpu_addr
								= fd_dev->bufs[buf_index].ion_cpu_addr;
#endif

		/* 6) set crop size					*/
		fd_dev->fd_info[buf_index].crop.start_x =
											fd_dev->config_data.crop.start_x;
		fd_dev->fd_info[buf_index].crop.start_y =
											fd_dev->config_data.crop.start_y;
		fd_dev->fd_info[buf_index].crop.width =
											fd_dev->config_data.crop.width;
		fd_dev->fd_info[buf_index].crop.height =
											fd_dev->config_data.crop.height;

	/* 7) scaler set capture frame				*/
		result = scaler_action_configure(CSS_SCALER_FD,
					fd_scaler_config, config_data->action);
		result = scaler_set_capture_frame(CSS_SCALER_FD);

	} else {
		result = CSS_ERROR_FD_INVALID_BUF;
	}

	return result;
}

int camera_fd_set_resolution(unsigned int res)
{
	int result = CSS_SUCCESS;
	odin_fd_set_resolution(res);
	return result;
}

int camera_fd_set_direction(unsigned int dir)
{
	int result = CSS_SUCCESS;
	odin_fd_set_direction(dir);
	return result;
}

int camera_fd_set_threshold(unsigned int threshold)
{
	int result = CSS_SUCCESS;
	odin_fd_set_threshold(threshold);
	return result;
}

int camera_fd_get_data(void)
{
	int result = CSS_SUCCESS;
	int count;
	count = odin_fd_get_face_count();
	/*
	 *	fd_face_info
	 *	fd_get_face_info();
	 */
	return result;
}

int camera_fd_buf_destory(struct css_fd_device *fd_dev)
{
	int i = 0;

	css_fd("camera_fd_buf_destory\n");

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

	for (i = 0; i < FACE_BUF_MAX; i++) {
		if (fd_dev->bufs[i].ion_cpu_addr && fd_dev->bufs[i].ion_hdl.handle)
			ion_unmap_kernel(fd_dev->client, fd_dev->bufs[i].ion_hdl.handle);
			fd_dev->bufs[i].ion_cpu_addr = 0;
	}

	if (fd_dev->client) {
		odin_ion_client_destroy(fd_dev->client);
		fd_dev->client = NULL;
	}

	return CSS_SUCCESS;
}

int camera_fd_stop(struct capture_fh *capfh)
{
	int result = CSS_SUCCESS;
	int i;
	struct css_fd_device *fd_dev= capfh->cssdev->fd_device;

	css_info("camera_fd_stop\n");

	/* 1. capture frame stop			*/
	scaler_int_disable(INT_PATH_FD);
	scaler_int_clear(INT_PATH_FD);

	/* 2. fd scaler power off & clk off	*/
	scaler_deinit_device(CSS_SCALER_FD);
	odin_fd_hw_deinit(capfh->cssdev);
#ifdef USE_FPD_HW_IP
	odin_fpd_hw_deinit(capfh->cssdev);
#endif

	/* 3. dealloc or init buffer		*/
#ifdef CONFIG_ODIN_ION_SMMU
	result = camera_fd_buf_destory(fd_dev);
	if (result != CSS_SUCCESS) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}
#endif

	camera_fd_deinit(capfh);

	fd_dev->start = 0;

	return result;
}

int camera_i2c_write(const struct i2c_client *client, const char *buf,
					int count)
{
	int ret;
	struct i2c_msg msg;
	struct i2c_adapter *adap = client->adapter;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;
	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}

int camera_i2c_read(const struct i2c_client *client, const char *buf)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		}, {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf,
		},
	};

	if (i2c_transfer(adap, msgs, ARRAY_SIZE(msgs)) != 2) {
		css_err("camera_i2c_read error!!\n");
		return -EREMOTEIO;
	}

	return CSS_SUCCESS;
}

int camera_detect_sensor(struct css_platform_sensor_group *pdata,
				unsigned int cam_index)
{
	struct i2c_adapter *i2c_adapter;
	struct i2c_board_info *i2c_info;
	unsigned short addr;
	u8 data[2];
	struct i2c_client *client;
	int ret;
	int index;
	struct regulator *cam_1_2v_avdd		= NULL;
	struct regulator *cam_vcm_1_8v_avdd = NULL;
	struct regulator *cam_2_8v_avdd		= NULL;

	cam_1_2v_avdd = regulator_get(NULL, "+1.05V_CAM_DVDD"); /*LDO4*/
	if (!IS_ERR(cam_1_2v_avdd))
		regulator_enable(cam_1_2v_avdd);
	else
		css_err("+1.05V_CAM_DVDD get error\n");

	cam_vcm_1_8v_avdd = regulator_get(NULL, "+1.8V_13M_VCM"); /*LDO18*/
	if (!IS_ERR(cam_vcm_1_8v_avdd))
		regulator_enable(cam_vcm_1_8v_avdd);
	else
		css_err("+1.8V_13M_VCM get error\n");

	cam_2_8v_avdd = regulator_get(NULL, "+2.7V_13M_ANA"); /*LDO23*/
	if (!IS_ERR(cam_2_8v_avdd))
		regulator_enable(cam_2_8v_avdd);
	else
		css_err("+2.7V_13M_ANA get error\n");

	mdelay(3);

	i2c_adapter = i2c_get_adapter(pdata->sensor[cam_index]->i2c_busnum);
	i2c_info = pdata->sensor[cam_index]->info;
	client = i2c_new_device(i2c_adapter, i2c_info);

	addr = 0x0001;
	data[0] = (addr >> 8) & 0xff;;
	data[1] = (addr >> 0) & 0xff;
	ret = camera_i2c_read(client, data);

	i2c_unregister_device(client);

	if (data[0] == 0x91)
		index = 0;
	else
		index = 1;

	if (!IS_ERR(cam_2_8v_avdd)) {
		regulator_disable(cam_2_8v_avdd);
		regulator_put(cam_2_8v_avdd);
		cam_2_8v_avdd = NULL;
	}
	if (!IS_ERR(cam_1_2v_avdd)) {
		regulator_disable(cam_1_2v_avdd);
		regulator_put(cam_1_2v_avdd);
		cam_1_2v_avdd = NULL;
	}
	if (!IS_ERR(cam_vcm_1_8v_avdd)) {
		regulator_disable(cam_vcm_1_8v_avdd);
		regulator_put(cam_vcm_1_8v_avdd);
		cam_vcm_1_8v_avdd = NULL;
	}

	css_info("camera_detect_sensor %x ,%d\n ",data[0],index);

	return index;
}

int camera_set_isp_clock(struct css_device *cssdev)
{
	int ret = CSS_SUCCESS;
	struct css_isp_clock *ispclk = NULL;
	unsigned long round_clk_khz = 0;
	ktime_t delta, stime, etime;
	unsigned long long duration;

	if (cssdev) {
		ispclk = &cssdev->ispclk;
		ispclk->set_clk_hz = CSS_MAX(ispclk->clk_hz[0], ispclk->clk_hz[1]);
		if (ispclk->cur_clk_hz != ispclk->set_clk_hz) {
			unsigned int khz;
			khz = ispclk->set_clk_hz/1000;
			round_clk_khz = css_isp_clk_round_khz(khz);
			stime = ktime_get();
			ret = css_set_isp_divider(round_clk_khz);
			etime = ktime_get();
			delta = ktime_sub(etime, stime);
			duration = (unsigned long long) ktime_to_ns(delta) >> 10;
			css_info("[ISP] %ld:%ld(MHz): %llu %dMHz -> %dMHz\n",
					round_clk_khz/1000, css_get_isp_clock_khz_by_div()/1000,
					duration, ispclk->cur_clk_hz/1000000,
					ispclk->set_clk_hz/1000000);
			ispclk->cur_clk_hz = ispclk->set_clk_hz;
		}
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

void camera_update_isp_clock(struct css_device *cssdev)
{
	if (cssdev) {
		struct css_isp_clock *ispclk = NULL;
		ispclk = &cssdev->ispclk;
		ispclk->cur_clk_hz = css_get_isp_clock_khz_by_div() * 1000;
	}
}

static int camera_v4l2_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	memset(cap, 0, sizeof(*cap));
	strcpy(cap->driver, "odin-css");
	cap->version = KERNEL_VERSION(3, 3, 0);

	switch (capfh->main_scaler) {
	case CSS_SCALER_0:
	case CSS_SCALER_1:
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
		break;
	default:
		css_err("invalid capture scaler num!\n");
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&capfh->v4l2_mutex);

	css_cam_v4l2("querycap [0x%08X]\n", cap->capabilities);
	return ret;
}

static
int camera_v4l2_g_ctrl(struct file *file, void *fh, struct v4l2_control *c)
{
	int ret = 0;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	css_cam_v4l2("g_ctrl ret = %d\n", ret);

	mutex_lock(&capfh->v4l2_mutex);

	return ret;
}

static
int camera_v4l2_s_ctrl(struct file *file, void *fh, struct v4l2_control *c)
{
	int ret = 0;
	struct capture_fh *capfh = file->private_data;
	struct css_fd_device *fd_dev = NULL;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	switch (c->id) {
	case V4L2_CID_CSS_FD_SRC:
	{
		struct css_scaler_config *scl_config = NULL;
		scl_config = &capfh->scl_dev->config;

#if 0
		if (c->value == 0) {
			scl_config->mFDsource = FD_SOURCE_PATH0;
		} else {
			scl_config->mFDsource = FD_SOURCE_PATH1;
		}
#endif
		css_cam_v4l2("set FD-source %d\n", c->value);
		break;
	}
	case V4L2_CID_CSS_FD_CMD:
		if (c->value == FD_COMMAND_STOP)
			camera_fd_stop(capfh);
		else if (c->value == FD_COMMAND_START)
			camera_fd_start(capfh,0);
		break;
	case V4L2_CID_CSS_FD_START:
		camera_fd_start(capfh, c->value);
		break;
	case V4L2_CID_CSS_FD_STOP:
		camera_fd_stop(capfh);
		break;
	case V4L2_CID_CSS_FD_SET_DIR:
		fd_dev = capfh->cssdev->fd_device;
		if (!fd_dev) {
			css_err("fd dev is null\n");
			mutex_unlock(&capfh->v4l2_mutex);
			return CSS_ERROR_FD_DEV_IS_NULL;
		}
		fd_dev->config_data.direct = c->value;
		camera_fd_set_direction(c->value);
		break;
	case V4L2_CID_CSS_FD_SET_THRESHOLD:
		fd_dev = capfh->cssdev->fd_device;
		if (!fd_dev) {
			css_err("fd dev is null\n");
			mutex_unlock(&capfh->v4l2_mutex);
			return CSS_ERROR_FD_DEV_IS_NULL;
		}
		fd_dev->config_data.threshold = c->value;
		camera_fd_set_threshold(c->value);
		break;
	case V4L2_CID_CSS_FD_SET_RESOLUTION:
		fd_dev = capfh->cssdev->fd_device;
		if (!fd_dev) {
			css_err("fd dev is null\n");
			mutex_unlock(&capfh->v4l2_mutex);
			return CSS_ERROR_FD_DEV_IS_NULL;
		}
		fd_dev->config_data.res = c->value;
		camera_fd_set_resolution(c->value);
		break;
	case V4L2_CID_CSS_FD_SET_BUF:
	{
		int buf_index = c->value;
#if 0
		int i =0;
#endif
		fd_dev = capfh->cssdev->fd_device;
		if (!fd_dev) {
			css_err("fd dev is null\n");
			mutex_unlock(&capfh->v4l2_mutex);
			return CSS_ERROR_FD_DEV_IS_NULL;
		}

		css_fd("FD_SET_BUF buf = %d FD DONE\n",buf_index);
		css_fd("DEBUG FD_SET_BUF BEFORE: buf = %d, state = %d,new =%d\n",
			buf_index, fd_dev->fd_info[buf_index].state,
			fd_dev->fd_info[buf_index].buf_newest);

		fd_dev->fd_info[buf_index].state = FD_DONE; /* FD_READY? */
#if 0
		for (i = 0; i < FACE_BUF_MAX; i++) {
			css_fd("DEBUG FD_SET_BUF: buf = %d, state = %d,new =%d\n",
				i,capfh->cssdev->fd_device->fd_info[i].state,
				capfh->cssdev->fd_device->fd_info[i].buf_newest);
		}
#endif

		break;
	}
	default:
		ret = -1;
		break;
	}

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_g_ext_ctrls_handler(struct capture_fh *capfh,
				struct v4l2_ext_control *ctrl, int index)
{
	int ret = CSS_SUCCESS;

#ifdef USE_FPD_HW_IP
	struct css_device *cssdev = capfh->cssdev;
	struct css_fd_device *fd_dev = NULL;

	fd_dev = cssdev->fd_device;
	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}
#endif

	switch (ctrl->id) {
	/* Face Detection CID handler */
	/* 1. Overall information */
	case V4L2_CID_CSS_FD_GET_FACE_COUNT:
		ctrl->value = odin_fd_get_face_count();
		if (ctrl->value == 0)
			return -255;
		break;
	/* 2. Face information */
#ifdef USE_FPD_HW_IP
	case V4L2_CID_CSS_FD_GET_FACE_CENTER_X:
		ctrl->value = fd_dev->result_data.fd_face_info[index].center_x;
		break;
	case V4L2_CID_CSS_FD_GET_FACE_CENTER_Y:
		ctrl->value = fd_dev->result_data.fd_face_info[index].center_y;
		break;
	case V4L2_CID_CSS_FD_GET_FACE_SIZE:
		ctrl->value = fd_dev->result_data.fd_face_info[index].size;
		break;
	case V4L2_CID_CSS_FD_GET_FACE_CONFIDENCE:
		ctrl->value = fd_dev->result_data.fd_face_info[index].confidence;
		break;
	case V4L2_CID_CSS_FD_GET_ANGLE:
		ctrl->value = fd_dev->result_data.fd_face_info[index].angle;
		break;
	case V4L2_CID_CSS_FD_GET_POSE:
		ctrl->value = fd_dev->result_data.fd_face_info[index].pose;
		break;
	/* 3. Facial Detection information */
	case V4L2_CID_CSS_FPD_GET_EXEC_STATUS:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].execution_status;
		break;
	case V4L2_CID_CSS_FPD_GET_MOUTH_CONFIDENCE:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth_conf;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CONFIDENCE:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].right_eye_conf;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_CONFIDENCE:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye_conf;
		break;
	/* 4. Angle information */
	case V4L2_CID_CSS_FPD_GET_ROLL:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].roll;
		break;
	case V4L2_CID_CSS_FPD_GET_PITCH:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].pitch;
		break;
	case V4L2_CID_CSS_FPD_GET_YAW:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].yaw;
		break;
	/* 5. Left eye information */
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_INNER_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.inner.x;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_INNER_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.inner.y;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_OUTER_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.outer.x;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_OUTER_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.outer.y;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_CENTER_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.outer.x;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_EYE_CENTER_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].left_eye.outer.y;
		break;
	/* 6. Right eye information */
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_INNER_X:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.inner.x;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_INNER_Y:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.inner.y;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_OUTER_X:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.outer.x;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_OUTER_Y:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.outer.y;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CENTER_X:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.outer.x;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_EYE_CENTER_Y:
		ctrl->value
			= fd_dev->result_data.fpd_face_info[index].right_eye.outer.y;
		break;
	/* 7. Nostril information */
	case V4L2_CID_CSS_FPD_GET_LEFT_NOSTRIL_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].nostril.left.x;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_NOSTRIL_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].nostril.left.y;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_NOSTRIL_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].nostril.left.x;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_NOSTRIL_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].nostril.left.y;
		break;
	/* 8. Mouth information */
	case V4L2_CID_CSS_FPD_GET_LEFT_MOUTH_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.left.x;
		break;
	case V4L2_CID_CSS_FPD_GET_LEFT_MOUTH_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.left.y;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_MOUTH_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.right.x;
		break;
	case V4L2_CID_CSS_FPD_GET_RIGHT_MOUTH_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.right.y;
		break;
	case V4L2_CID_CSS_FPD_GET_UPPER_MOUTH_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.upper.x;
		break;
	case V4L2_CID_CSS_FPD_GET_UPPER_MOUTH_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.upper.y;
		break;
	case V4L2_CID_CSS_FPD_GET_CENTER_MOUTH_X:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.center.x;
		break;
	case V4L2_CID_CSS_FPD_GET_CENTER_MOUTH_Y:
		ctrl->value = fd_dev->result_data.fpd_face_info[index].mouth.center.y;
		break;
#endif
	/* 9. Update next face information */
	case V4L2_CID_CSS_FD_GET_NEXT:
		break;
	default:
		return 255;
		break;
	}
	return ret;
}

static int camera_v4l2_g_ext_ctrls(struct file *file, void *fh,
				struct v4l2_ext_controls *e)
{
	int i,j, ret = 0;
	struct v4l2_ext_control *ctrl;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (!e || e->ctrl_class != V4L2_CTRL_CLASS_CAMERA)
		return -EINVAL;

	css_cam_v4l2("g_ext_ctrls S \n");

	mutex_lock(&capfh->v4l2_mutex);

	ctrl = e->controls;

	for (i = 0; i < e->count; i++) {
		ctrl = e->controls + i;
		if (i == 0) {
			j = 0;
		} else {
			/* i--; */ /* first command = face count */
			j = i / 37;
			if ((i % 37) == 0)
				j--;
		}

		ret = camera_g_ext_ctrls_handler(capfh, ctrl, j);
		if (ret > 0) {
			e->error_idx = i;
			break;
		} else if (ret < 0) {
			ret = 0;
			break;
		}
	}

	css_cam_v4l2("g_ext_ctrls ret = %d\n", ret);

	mutex_unlock(&capfh->v4l2_mutex);
	return ret;
}

static
int camera_v4l2_enum_input(struct file *file, void *fh, struct v4l2_input *i)
{
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = video_drvdata(file);

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	css_cam_v4l2("camera_v4l2_enum_input : index = %d\n", i->index);

	if (i->index > SENSOR_DUMMY) {
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	if (strlen(cssdev->sensor[i->index]->name) < 32)
		strcpy(i->name, cssdev->sensor[i->index]->name);
	else {
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	i->type = V4L2_INPUT_TYPE_CAMERA;

	mutex_unlock(&capfh->v4l2_mutex);

	return CSS_SUCCESS;
}

static
int camera_v4l2_g_input(struct file *file, void *fh, unsigned int *i)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev = NULL;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	cam_dev = capfh->cam_dev;

	mutex_lock(&capfh->v4l2_mutex);

	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		*i = cam_dev->cur_sensor->id;
	} else {
		css_err("current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	mutex_unlock(&capfh->v4l2_mutex);

	css_cam_v4l2("g_input ret = %d\n", ret);
	return ret;
}

int camera_subdev_configure(struct css_device *cssdev, unsigned int i)
{
	struct i2c_adapter *i2c_adapter;
	struct i2c_board_info *i2c_info;
	struct v4l2_subdev *subdev;

	i2c_adapter = i2c_get_adapter(cssdev->sensor[i]->i2c_busnum);
	i2c_info = cssdev->sensor[i]->info;

	subdev = v4l2_i2c_new_subdev_board(&cssdev->v4l2_dev,
				i2c_adapter, i2c_info, NULL);

	if (!subdev) {
		i2c_put_adapter(i2c_adapter);
		css_err("v4l2_i2c_new_subdev_board() :failed!\r\n");
		return CSS_ERROR_SENSOR_CONFIG_SUBDEV_FAIL;
	}

	cssdev->sensor[i]->sd = subdev;

	return CSS_SUCCESS;
}

int camera_subdev_release(struct capture_fh *capfh)
{
	struct css_sensor_device *cam_dev = capfh->cam_dev;
	struct i2c_client *client;
	struct css_i2cboard_platform_data *pdata;
	int ret = CSS_SUCCESS;

	if (cam_dev && cam_dev->cur_sensor) {
		if (cam_dev->cur_sensor->initialize) {
			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
										core, init, CSS_SENSOR_DISABLE);
			if (ret < 0)
				css_err("sensor disable fail ret=%d\n", ret);
			cam_dev->cur_sensor->initialize = 0;
		}
		if (cam_dev->cur_sensor->power) {
			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_power, 0);
			if (ret < 0)
				css_err("sensor s_power fail ret=%d\n", ret);
			cam_dev->cur_sensor->power = 0;
		}

		if (cam_dev->cur_sensor->sd) {
			client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);
			if (client) {
				pdata = client->dev.platform_data;

				if (pdata->sensor_init.cpu_addr) {
					css_sensor("1 0x%x\n",pdata->sensor_init.cpu_addr);
#if 0
					dma_free_coherent(NULL, pdata->sensor_init.mmap_size,
										pdata->sensor_init.cpu_addr,
										pdata->sensor_init.dma_addr);
					/* pdata->sensor_init.cpu_addr = NULL; */
#endif
					memset(&pdata->sensor_init, 0,
							sizeof(struct css_sensor_init));
				}

				if (client->adapter)
					i2c_put_adapter(client->adapter);

				i2c_unregister_device(client);
			}

			cam_dev->cur_sensor->sd = NULL;
		}
		cam_dev->cur_sensor = NULL;
	}

	if (cam_dev && cam_dev->s3d_sensor) {
		if (cam_dev->s3d_sensor->initialize) {
			ret = v4l2_subdev_call(cam_dev->s3d_sensor->sd,
										core, init, CSS_SENSOR_DISABLE);
			if (ret < 0)
				css_err("sensor disable fail ret=%d\n", ret);
			cam_dev->s3d_sensor->initialize = 0;
		}
		if (cam_dev->s3d_sensor->power) {
			ret = v4l2_subdev_call(cam_dev->s3d_sensor->sd, core, s_power, 0);
			if (ret < 0)
				css_err("sensor s_power fail ret=%d\n", ret);
			cam_dev->s3d_sensor->power = 0;
		}

		if (cam_dev->s3d_sensor->sd) {
			client = v4l2_get_subdevdata(cam_dev->s3d_sensor->sd);
			if (client) {
				pdata = client->dev.platform_data;

				if (pdata->sensor_init.cpu_addr) {
					css_sensor("2 0x%x\n",pdata->sensor_init.cpu_addr);
#if 0
					dma_free_coherent(NULL, pdata->sensor_init.mmap_size,
										pdata->sensor_init.cpu_addr,
										pdata->sensor_init.dma_addr);
					/* pdata->sensor_init.cpu_addr = NULL; */
#endif
					memset(&pdata->sensor_init, 0,
							sizeof(struct css_sensor_init));
				}

				if (client->adapter)
					i2c_put_adapter(client->adapter);
				i2c_unregister_device(client);
			}
			cam_dev->s3d_sensor->sd = NULL;
		}
		cam_dev->s3d_sensor = NULL;
	}

	if (cam_dev && cam_dev->mmap_sensor_init.cpu_addr) {
		css_sensor("free : 3 0x%x\n",cam_dev->mmap_sensor_init.cpu_addr);
		dma_free_coherent(NULL,cam_dev->mmap_sensor_init.mmap_size,
				cam_dev->mmap_sensor_init.cpu_addr,
				cam_dev->mmap_sensor_init.dma_addr);
		memset(&cam_dev->mmap_sensor_init, 0, sizeof(struct css_sensor_init));
	}

	if (cam_dev)
		kzfree(cam_dev);

	return ret;
}

static int camera_v4l2_s_input(struct file *file, void *fh, unsigned int i)
{
	int ret = CSS_SUCCESS;
#ifndef TEST_INIT_VALUE
	unsigned int *temp;
#endif
	struct i2c_client *client;
	struct v4l2_mbus_framefmt m_pix;
	struct css_i2cboard_platform_data *pdata;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = video_drvdata(file);
	struct css_sensor_device *cam_dev;
	struct css_platform_sensor *sensor;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (i > SENSOR_S3D)
		return -EINVAL;
#else
	if (i > SENSOR_DUMMY)
		return -EINVAL;
#endif

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev) {
		css_err("cam_dev is not allocated");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	/* mutex_lock(&capfh->v4l2_mutex); */

#ifndef TEST_INIT_VALUE
	if (i != SENSOR_DUMMY) {
		css_info("s_input : 0x%x\n",cam_dev->mmap_sensor_init.cpu_addr);
		if (cam_dev->mmap_sensor_init.cpu_addr) {
			temp = cam_dev->mmap_sensor_init.value;
			if (!*temp) {
				/* no memcpy */
				css_err("sensor init value is empty\n");
				/* mutex_unlock(&capfh->v4l2_mutex); */
				return CSS_ERROR_SENSOR_INIT_VALUE_EMPTY;
			}
		} else {
			/* no mmap */
			css_err("sensor init memory is not exist\n");
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return CSS_ERROR_SENSOR_INIT_MEMORY_IS_NOT_FOUND;
		}
	}
#endif

	if (!cssdev->sensor[i]->sd) {
		ret = camera_subdev_configure(cssdev,i);
		if (ret < 0) {
			css_err("camera_subdev_configure fail ret=%d\n", ret);
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return CSS_ERROR_SENSOR_CONFIG_SUBDEV_FAIL;
		}
	} else {
		css_err("current sensor exists already\n");
	}

	sensor = cssdev->sensor[i];
	sensor->id = i;

	client = v4l2_get_subdevdata(sensor->sd);
	pdata = client->dev.platform_data;

#ifndef TEST_INIT_VALUE
	if (i != SENSOR_DUMMY){
		client = v4l2_get_subdevdata(sensor->sd);
		if (!client || !client->dev.platform_data) {
			css_err("Do not ready to use I2C \r\n");
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
		}

		pdata = client->dev.platform_data;
		pdata->sensor_init = cam_dev->mmap_sensor_init;
		pdata->sensor_init.init_size = *temp;
		pdata->sensor_init.mode_size = *(temp + 1);
		pdata->sensor_init.value = (void *)(temp + 2);
		css_info("*temp: %d\n", *temp);
		css_sensor("*(temp + 1): %d\n", *(temp + 1));
		css_info("*(temp + 2): %d\n", *(temp + 2));

	}
#endif

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (i == SENSOR_S3D)
		cam_dev->s3d_sensor = sensor;
	else
#endif
		cam_dev->cur_sensor = sensor;

	if (sensor->initialize != 1) {
		sensor->power = 1;
		ret = v4l2_subdev_call(sensor->sd, core, s_power, 1);
		if (ret < 0) {
			css_err("odin_sensor power fail ret=%d\n", ret);
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return ret;
		}

		ret = v4l2_subdev_call(sensor->sd, core, init, CSS_SENSOR_INIT);
		if (ret < 0) {
			css_err("odin_sensor init fail ret=%d\n", ret);
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return ret;
		}

		sensor->initialize = 1;

		if (i != SENSOR_DUMMY) {
			m_pix.width = sensor->init_width;
			m_pix.height = sensor->init_height;
			ret = v4l2_subdev_call(sensor->sd, video, s_mbus_fmt, &m_pix);
		}
	}

	css_cam_v4l2("s_input idx = %d\n", i);

	/* mutex_unlock(&capfh->v4l2_mutex); */

	return ret;
}

static int camera_v4l2_cropcap(struct file *file, void *fh,
				struct v4l2_cropcap *cropcap)
{
	int ret = 0;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	css_cam_v4l2("cropcap ret = %d\n", ret);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_g_crop(struct file *file, void *fh,
				struct v4l2_crop *crop)
{
	int ret = 0;
	struct capture_fh *capfh = file->private_data;
	struct css_scaler_config *scl_config = NULL;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	scl_config = &capfh->scl_dev->config;
	crop->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop->c.top		= scl_config->crop.start_y;
	crop->c.left	= scl_config->crop.start_x;
	crop->c.width	= scl_config->crop.width;
	crop->c.height	= scl_config->crop.height;

	css_cam_v4l2("g_crop [%d %d %d %d]\n", crop->c.top, crop->c.left,
					crop->c.width, crop->c.height);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_s_crop(struct file *file, void *fh,
				const struct v4l2_crop *crop)
{
	int ret = 0;
	struct capture_fh *capfh = file->private_data;
	struct css_scaler_config *scl_config = NULL;

	css_info("croprect [%d %d %d %d type %d]\n", crop->c.left, crop->c.top,
				crop->c.width, crop->c.height, crop->type);

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	scl_config = &capfh->scl_dev->config;
	scl_config->crop.start_y = crop->c.top;
	scl_config->crop.start_x = crop->c.left;
	scl_config->crop.width = crop->c.width;
	scl_config->crop.height = crop->c.height;

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static const struct v4l2_fmtdesc capture_fmts[] = {
	{
		.index	= 0,
		.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags	= 0,
		.description	= "YUV422 UYVY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
	},
	{
		.index	= 1,
		.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags	= 0,
		.description	= "YUV420",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
	},
	{
		.index	= 2,
		.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags	= 0,
		.description	= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
	},
};


static int camera_v4l2_enum_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_fmtdesc *fmt)
{
	int ret = CSS_SUCCESS;
	int i = fmt->index;

	if (i >= ARRAY_SIZE(capture_fmts)) {
		return -EINVAL;
	}

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		css_err("invalid format type %d!\n", fmt->type);
		return -EINVAL;
	}

	memset(fmt, 0, sizeof(*fmt));
	memcpy(fmt, &capture_fmts[i], sizeof(*fmt));

	css_cam_v4l2("enum_fmt_vid_cap = %d\n", ret);

	return ret;
}

static int camera_v4l2_try_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	int ret = CSS_SUCCESS;
	unsigned int sensor_width = 0;
	unsigned int sensor_height = 0;
	struct v4l2_mbus_framefmt m_pix = {0,};
	struct css_sensor_device *cam_dev = NULL;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	mutex_lock(&capfh->v4l2_mutex);

	cam_dev = capfh->cam_dev;

	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video, g_mbus_fmt,
					&m_pix);

		sensor_width = m_pix.width;
		sensor_height = m_pix.height;
		if (sensor_width < fmt->fmt.pix.width ||
			sensor_height < fmt->fmt.pix.height) {
			mutex_unlock(&capfh->v4l2_mutex);
			return -EINVAL;
		}
	} else {
		css_err("try_fmt_vid_cap : current sensor is not found");
		mutex_unlock(&capfh->v4l2_mutex);
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	ret = capture_set_try_format(capfh, fmt);

	css_cam_v4l2("try_fmt_vid_cap = %d\n", ret);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_g_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_device *cam_dev = NULL;
	struct capture_fh *capfh = file->private_data;
	struct v4l2_mbus_framefmt m_pix = {0,};

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	cam_dev = capfh->cam_dev;

	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video, g_mbus_fmt,
					&m_pix);

		fmt->fmt.pix.width = m_pix.width;
		fmt->fmt.pix.height = m_pix.height;

		css_cam_v4l2("g_fmt_vid_cap : ret = %d, w = %d ,h = %d\n",
			ret, fmt->fmt.pix.width, fmt->fmt.pix.height);
	} else {
		css_err("current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_s_fmt_vid_cap(struct file *file, void *fh,
				struct v4l2_format *fmt)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;
	struct css_scaler_config *scl_config;
	struct css_sensor_device *cam_dev = NULL;
	struct v4l2_mbus_framefmt m_pix = {0,};

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	/* Check type */
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (fmt->fmt.pix.field != V4L2_FIELD_ANY &&
		fmt->fmt.pix.field != V4L2_FIELD_NONE &&
		fmt->fmt.pix.field != V4L2_FIELD_INTERLACED) {
		css_err("invalid image field %d!\n", fmt->fmt.pix.field);
		return -EINVAL;
	}

	if (fmt->fmt.pix.width == 0 || fmt->fmt.pix.height == 0) {
		css_err("invalid image size!\n");
		return -EINVAL;
	}

	css_cam_v4l2("fmt->fmt.pix.priv = %d\n", (css_behavior)fmt->fmt.pix.priv);

	mutex_lock(&capfh->v4l2_mutex);

	cam_dev = capfh->cam_dev;

	/*Sensor Resolution change*/
	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		if ((css_behavior)fmt->fmt.pix.priv == CSS_BEHAVIOR_SET_DUMMI_SENSOR) {
			m_pix.width = fmt->fmt.pix.width;
			m_pix.height = fmt->fmt.pix.height;
			m_pix.reserved[0] = (css_behavior)fmt->fmt.pix.priv;

			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video,
						s_mbus_fmt, &m_pix);
			mutex_unlock(&capfh->v4l2_mutex);
			return CSS_SUCCESS;
		}
	} else {
		css_err("current sensor is not found\n");
		mutex_unlock(&capfh->v4l2_mutex);
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	scl_config = &capfh->scl_dev->config;

	ret = capture_set_format(capfh, fmt);

	memcpy(&scl_config->v4l2_fmt, fmt, sizeof(struct v4l2_format));

	css_cam_v4l2("s_fmt_vid_cap = %d\n", ret);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *req)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	if (capfh->main_scaler > CSS_SCALER_1) {
		req->count = 1;
	} else {
		if (req->count > CSS_MAX_FRAME_BUFFER_COUNT) {
			css_warn("over buffer count requested, fix to max 6!")
			req->count = CSS_MAX_FRAME_BUFFER_COUNT;
		}
	}

	if (ret == CSS_SUCCESS) {
		if (req->count == 0)
			ret = capture_buffer_deinit(capfh);
		else
			ret = capture_buffer_init(capfh, req->count);
	}

	css_cam_v4l2("reqbufs[%d] = %d\n", req->count, ret);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static
int camera_v4l2_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = video_drvdata(file);
	struct css_buffer *cssbuf = NULL;
	struct css_bufq *bufq = NULL;
	int ret = CSS_SUCCESS;
	int index = 0;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (buf == NULL) {
		css_err("buf is null!\n");
		return -EINVAL;
	}

	if (buf->memory == V4L2_MEMORY_USERPTR) {
		css_err("querybuf don't support v4l2 userptr!\n");
		return -EINVAL;
	}

	mutex_lock(&capfh->v4l2_mutex);

	index = buf->index;

	memset(buf, 0, sizeof(*buf));

	if (cssdev->rsvd_mem == NULL) {
		css_err("no reserved memory!\n");
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	if (index < 0 || index > CSS_MAX_FRAME_BUFFER_COUNT) {
		css_err("invalid index %d!\n", index);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n", capfh->main_scaler);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	cssbuf = &bufq->bufs[index];
	buf->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf->index		= index;
	buf->bytesused	= 0;
	buf->field		= V4L2_FIELD_NONE;
	buf->memory		= V4L2_MEMORY_MMAP;
	buf->m.offset	= (u32)cssbuf->dma_addr;
	buf->length		= (u32)cssbuf->size;
	buf->timestamp.tv_sec	= 0;
	buf->timestamp.tv_usec	= 0;

	css_cam_v4l2("querybuf [%d]0x%08X : %d\n", index, buf->m.offset,
						buf->length);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

/* request a new frame to driver */
static
int camera_v4l2_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret = CSS_SUCCESS;
	int buf_cnt = 0;
	unsigned long flags = 0;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = video_drvdata(file);
	struct css_buffer *cssbuf = NULL;
	struct css_bufq *bufq = NULL;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (buf == NULL) {
		css_err("buf is null\n");
		return -EINVAL;
	}

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		css_err("invalid buf type %d!\n", buf->type);
		return -EINVAL;
	}

	if (buf->memory == V4L2_MEMORY_OVERLAY) {
		css_err("invalid memory type %d!\n", buf->memory);
		return -EINVAL;
	}

	mutex_lock(&capfh->v4l2_mutex);

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n", capfh->main_scaler);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	buf_cnt = bufq->count;
	if (buf->index > buf_cnt) {
		css_err("invalid buf index %d!\n", buf->index);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	cssbuf = &bufq->bufs[buf->index];
	if (!cssbuf) {
		css_err("css buf null %d!\n", buf->index);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	if (cssbuf->state != CSS_BUF_UNUSED) {
		css_warn("video%d css buf[%d] state %d!\n", capfh->main_scaler,
					buf->index, cssbuf->state);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	if (buf->memory == V4L2_MEMORY_USERPTR) {
#ifdef CONFIG_ODIN_ION_SMMU

		int fd = -1;
		fd = (int)buf->m.fd;
		if (fd < 0) {
			css_err("invalid fd\n");
			mutex_unlock(&capfh->v4l2_mutex);
			return -EINVAL;
		}

		if (cssbuf->imported == 0) {
			struct ion_handle *handle = NULL;
			if (bufq->cam_client == NULL) {
				css_err("cam_client NULL\n");
				mutex_unlock(&capfh->v4l2_mutex);
				return -EINVAL;
			}
			cssbuf->ion_fd = fd;
			handle = ion_import_dma_buf(bufq->cam_client, fd);

			if (IS_ERR_OR_NULL(handle)) {
				css_err("can't get import_dma_buf handle(%p) %p (%d)\n",
							handle, bufq->cam_client, fd);
				css_get_fd_info(fd);
				mutex_unlock(&capfh->v4l2_mutex);
				return -EINVAL;
			}
			css_sys("import fd[%d]: %d\n", buf->index, fd);

			cssbuf->dma_addr = (dma_addr_t)odin_ion_get_iova_of_buffer(handle,
										ODIN_SUBSYS_CSS);
			cssbuf->imported = true;
		}

		if (fd != cssbuf->ion_fd) {
			css_warn("fd not match: idx=%d sfd[%d] vs infd[%d]\n",
				buf->index, cssbuf->ion_fd, fd);
		}
#else
		cssbuf->dma_addr = (dma_addr_t)buf->m.userptr;
#endif
		cssbuf->id = -1;
		cssbuf->size = buf->length;
		cssbuf->allocated = true;

		css_cam_v4l2("video%d qbuf idx[%d]: fd=%d phy=0x%x\n",
			capfh->main_scaler, buf->index, fd, (u32)cssbuf->dma_addr);
	} else {
		css_cam_v4l2("video%d qbuf idx[%d]: phy=0x%x\n",
			capfh->main_scaler, buf->index, (u32)cssbuf->dma_addr);
	}

	cssbuf->byteused	= 0;

	if (capfh->buf_attrb.buf_type == CSS_BUF_CAPTURE_WITH_POSTVIEW) {
		size_t postview_size = cssdev->postview.size;
		cssbuf->offset = cssbuf->size - postview_size;
	} else {
		cssbuf->offset = 0;
	}

	spin_lock_irqsave(&bufq->lock, flags);
	cssbuf->state = CSS_BUF_QUEUED;
	spin_unlock_irqrestore(&bufq->lock, flags);

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

/* get new frame buffer from driver and user */
static int camera_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;
	struct css_bufq *bufq = NULL;
	struct css_buffer *cssbuf = NULL;
	int buf_index = 0;
	int stream_type = 0;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (buf == NULL) {
		css_err("buf is null!\n");
		return -EINVAL;
	}

	mutex_lock(&capfh->v4l2_mutex);

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n", capfh->main_scaler);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	cssbuf = capture_buffer_pop(bufq);
	if (cssbuf == NULL) {
		css_err("buf empty!\n");
		mutex_unlock(&capfh->v4l2_mutex);
		return CSS_ERROR_BUFFER_EMPTY;
	}

	if (cssbuf->state != CSS_BUF_CAPTURE_DONE) {
		css_err("invalid buf state %d!\n", cssbuf->state);
		mutex_unlock(&capfh->v4l2_mutex);
		return -EINVAL;
	}

	cssbuf->state = CSS_BUF_UNUSED;

	stream_type = (buf->reserved == BAYER_LOAD_STREAM_TYPE);

	buf->index		= cssbuf->id;
	buf->bytesused	= cssbuf->byteused;
	buf->timestamp	= cssbuf->timestamp;
	if (buf->memory == V4L2_MEMORY_USERPTR)
		buf->m.fd = (u32)cssbuf->ion_fd;
	else
		buf->m.offset = (u32)cssbuf->dma_addr;

	buf->length		= cssbuf->size;
	buf->sequence	= cssbuf->frame_cnt;
	buf->reserved	= cssbuf->offset;

	css_cam_v4l2("video%d dqbuf idx[%d]: %d phy=0x%x\n",
					capfh->main_scaler,
					buf->index,
					cssbuf->frame_cnt,
					(u32)cssbuf->dma_addr);

	if (capture_get_stream_mode(capfh)) {
		buf_index = capture_buffer_get_next_index(bufq);
		if (buf_index < 0)
			goto out;

		if (capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL ||
			capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW ||
			capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_3D) {
			css_zsl("bayer load stream type: %d\n", stream_type);
			if (stream_type) {
				bayer_load_set_type(capfh, ZSL_LOAD_STREAM_TYPE);
				ret = bayer_load_schedule(capfh);
			} else {
				bayer_load_set_type(capfh, ZSL_LOAD_ONESHOT_TYPE);
			}
		} else {
			if (bufq->mode != STREAM_MODE)
				capture_start(capfh, NULL, 0);
		}
	}

out:
	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;
	struct css_bufq *bufq = NULL;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}


	if (capfh->scl_dev == NULL) {
		css_err("scl_dev null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	if (capture_get_stream_mode(capfh)) {
		css_warn("video%d already stream on state!\n", capfh->main_scaler);
		mutex_unlock(&capfh->v4l2_mutex);
		return CSS_SUCCESS;
	}

	/*
	 * capfh->stream_on = 1
	 * if buf_cnt is bigger than 1,
	 * successive capture will be done by interrupt handler.
	 */
	capture_stream_mode_on(capfh);

	if (capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL ||
		capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW ||
		capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_3D) {
		bayer_load_init_count(capfh);
		ret = bayer_load_schedule(capfh);
	} else {
		bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
		if (bufq == NULL || bufq->bufs == NULL) {
			css_err("bufq (or bufs) is NULL (capfh[%d])!\n",
					capfh->main_scaler);
			mutex_unlock(&capfh->v4l2_mutex);
			return -EINVAL;
		}
		bufq->capt_tot = 0;
		bufq->index = capture_buffer_get_queued_index(bufq);

		if (capfh->behavior == CSS_BEHAVIOR_PREVIEW ||
			capfh->behavior == CSS_BEHAVIOR_PREVIEW_3D) {
			capture_skip_frame(capfh->cssdev, capfh->main_scaler, 1);
		} else if (capfh->behavior == CSS_BEHAVIOR_SNAPSHOT ||
					capfh->behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW) {
			css_bus_control(MEM_BUS, CSS_CAPTURE_MEM_BUS_FREQ_KHZ,
							CSS_BUS_TIME_SCHED_NONE);
			capture_skip_frame(capfh->cssdev, capfh->main_scaler, 3);
		}

		capfh->scl_dev->roop_err_cnt = 0;

		if (capfh->main_scaler == CSS_SCALER_0)
			scaler_path_reset(PATH_0_RESET);
		else
			scaler_path_reset(PATH_1_RESET);

		ret = capture_start(capfh, NULL, 1);
	}

	if (ret != CSS_SUCCESS)
		capture_stream_mode_off(capfh);

	css_info("streamon[scaler%02d]: 0x%08x(0x%08x)\n", capfh->main_scaler,
				css_crg_clk_info(), scaler_get_int_mask());

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	int ret = CSS_SUCCESS;
	static struct css_device *cssdev = NULL;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	mutex_lock(&capfh->v4l2_mutex);

	if (capture_get_stream_mode(capfh)) {
		capture_stream_mode_off(capfh);
		if (capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL &&
			capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW &&
			capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL_3D)
			capture_stop(capfh);
		else
			bayer_load_wait_done(capfh);

		if (capfh->behavior == CSS_BEHAVIOR_SNAPSHOT ||
			capfh->behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW)
			css_bus_control(MEM_BUS, CSS_DEFAULT_MEM_BUS_FREQ_KHZ,
				CSS_BUS_TIME_SCHED_NONE);

		capture_buffer_flush(capfh);
		css_info("streamoff[scaler%02d]: 0x%08x(0x%08x)\n", capfh->main_scaler,
					css_crg_clk_info(), scaler_get_int_mask());
		if (capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL ||
			capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW ||
			capfh->behavior == CSS_BEHAVIOR_SNAPSHOT ||
			capfh->behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW ||
			capfh->behavior == CSS_BEHAVIOR_RECORDING) {
			cssdev = capfh->cssdev;

			if (capture_get_path_mode(cssdev) == CSS_NORMAL_PATH_MODE) {
				if (capfh->behavior == CSS_BEHAVIOR_RECORDING &&
						capfh->main_scaler == CSS_SCALER_1) {
					/* REAR RECORD */
					odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
				}
				else
					odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
			}
		}
	} else {
		css_warn("video%d already stream off state!\n", capfh->main_scaler);
	}

	mutex_unlock(&capfh->v4l2_mutex);

	return ret;
}

static int camera_default_isp_control(struct file *file, void *arg)
{
#ifndef CONFIG_VIDEO_ISP_MISC_DRIVER
	struct css_isp_control_args *args = (struct css_isp_control_args *)arg;
	unsigned int cmd = 0;
	unsigned int arg = 0;

	cmd = args->c.left;
	arg = (unsigned int)&args->c.top;

	if (CSS_FAILURE == odin_isp_control(file, cmd, arg))
		return -EINVAL;

#endif
	return CSS_SUCCESS;
}

static int camera_default_mdis_control(struct file *file, void *arg)
{
	struct css_mdis_control_args *args = (struct css_mdis_control_args *)arg;

	if (odin_mdis_control(file, args) != CSS_SUCCESS)
		return -EINVAL;

	return CSS_SUCCESS;
}

static int camera_default_set_async_ctrl_params(struct file *file, void *arg)
{
	s32 i = 0;
	struct css_device *cssdev = video_drvdata(file);
	struct css_afifo_param_args *param = (struct css_afifo_param_args *)arg;

	if (arg == NULL)
		return -EINVAL;

	if (cssdev == NULL)
		return -EINVAL;

	switch (param->cmd) {
	case AFIFO_CTRL:
	{
		struct css_afifo_ctrl *ctrl = &cssdev->afifo.ctrl[0];
		struct css_afifo_ctrl_args *ctrl_args
							= (struct css_afifo_ctrl_args *)&param->ctrl;

		i = cssdev->afifo.ctrl_index = ctrl_args->index;

		ctrl[i].index = i;
		ctrl[i].format = ctrl_args->format;

		css_afifo("AFIFO Path: %s\n", (i == 1) ? "PATH_1" : "PATH_0");
		css_afifo("AFIFO Sensor Format: %x\n", ctrl_args->format);

		odin_isp_wrap_control_param(&ctrl[i]);

		break;
	}
	case AFIFO_PRE:
	{
		struct css_afifo_pre *pre = &cssdev->afifo.pre[0];
		struct css_afifo_pre_ctrl_args *ctrl_args = \
			(struct css_afifo_pre_ctrl_args *)&param->pre;

		i = cssdev->afifo.pre_index = ctrl_args->index;

		pre[i].index	= i;
		pre[i].width	= ctrl_args->width;
		pre[i].height	= ctrl_args->height;
		pre[i].format	= ctrl_args->format;

		css_afifo("AFIFO Pre Path: %s\n", (i == 1) ? "PRE_1" : "PRE_0");
		css_afifo("AFIFO Pre Input Size: [%d, %d]\n",
			pre[i].width, pre[i].height);
		css_afifo("AFIFO Sensor Format: %x\n", ctrl_args->format);

		odin_isp_wrap_pre_param(&pre[i]);

		break;
	}
	case AFIFO_POST:
	{
		struct css_afifo_post			*post = &cssdev->afifo.post[0];
		struct css_afifo_post_ctrl_args *ctrl_args = \
			(struct css_afifo_post_ctrl_args *)&param->post;

		i = cssdev->afifo.post_index = ctrl_args->index;

		post[i].index	 = i;
		post[i].width	= ctrl_args->width;
		post[i].height = ctrl_args->height;

		css_afifo("AFIFO Post Path: %s\n", (i == 1) ? "POST_1" : "POST_0");
		css_afifo("AFIFO Post Output Size: [%d, %d]\n",
			post[i].width, post[i].height);

		odin_isp_wrap_post_param(&post[i]);

		break;
	}
	case AFIFO_BAYER:
	{
		struct css_bayer_scaler *bayer_scl = &cssdev->afifo.bayer_scl[0];
		struct css_bayer_ctrl_args *ctrl_args = \
			(struct css_bayer_ctrl_args *)&param->bayer;

		i = cssdev->afifo.bscl_index = ctrl_args->index;

		bayer_scl[i].index	= i;
		bayer_scl[i].src_w	= ctrl_args->src_w;
		bayer_scl[i].src_h	= ctrl_args->src_h;
		bayer_scl[i].dest_w	= ctrl_args->dest_w;
		bayer_scl[i].dest_h	= ctrl_args->dest_h;
		bayer_scl[i].margin_x	= ctrl_args->margin_x;
		bayer_scl[i].margin_y	= ctrl_args->margin_y;
		bayer_scl[i].offset_x	= ctrl_args->offset_x;
		bayer_scl[i].offset_y	= ctrl_args->offset_y;
		bayer_scl[i].crop_en	= ctrl_args->crop_en;
		bayer_scl[i].scl_en		= ctrl_args->scl_en;

		css_afifo("Bayer[%d].index: %d\n", i, bayer_scl[i].index);
		css_afifo("Bayer[%d].src_w: %d\n", i, bayer_scl[i].src_w);
		css_afifo("Bayer[%d].src_h: %d\n", i, bayer_scl[i].src_h);
		css_afifo("Bayer[%d].dest_w: %d\n", i, bayer_scl[i].dest_w);
		css_afifo("Bayer[%d].dest_h: %d\n", i, bayer_scl[i].dest_h);
		css_afifo("Bayer[%d].margin_x: %d\n", i, bayer_scl[i].margin_x);
		css_afifo("Bayer[%d].margin_y: %d\n", i, bayer_scl[i].margin_y);
		css_afifo("Bayer[%d].offset_x: %d\n", i, bayer_scl[i].offset_x);
		css_afifo("Bayer[%d].offset_y: %d\n", i, bayer_scl[i].offset_y);
		css_afifo("Bayer[%d].crop_en: %d\n", i, bayer_scl[i].crop_en);
		css_afifo("Bayer[%d].scl_en: %d\n", i, bayer_scl[i].scl_en);

		odin_isp_wrap_bayer_param(&bayer_scl[i]);

		break;
	}
	default:
		return -EINVAL;
	}

	return CSS_SUCCESS;
}

static int camera_default_set_postview_config(struct file *file, void *arg)
{
	struct css_postview_info_args *info = (struct css_postview_info_args *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = capfh->cssdev;

	cssdev->postview.width = info->width;
	cssdev->postview.height = info->height;
	cssdev->postview.fmt = info->pix_fmt;
	cssdev->postview.size
			= capture_get_frame_size(info->pix_fmt, info->width, info->height);

	return CSS_SUCCESS;
}

static int camera_default_get_postview_info(struct file *file, void *arg)
{
	struct css_postview_info_args *info = (struct css_postview_info_args *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = capfh->cssdev;

	info->width = cssdev->postview.width;
	info->height = cssdev->postview.height;
	info->pix_fmt = cssdev->postview.fmt;
	info->size = cssdev->postview.size;

	return CSS_SUCCESS;
}

static int camera_default_get_frame_size_info(struct file *file, void *arg)
{
	struct css_frame_size_info_args *info
						= (struct css_frame_size_info_args*)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_bufq *bufq = NULL;
	struct css_buffer *cssbuf = NULL;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n", capfh->main_scaler);
		info->width = 0;
		info->height = 0;
		return -EINVAL;
	}

	if (info->index > bufq->count) {
		info->width = 0;
		info->height = 0;
		return -EINVAL;
	}

	cssbuf = &bufq->bufs[info->index];

#if 0
	if (cssbuf->state != CSS_BUF_CAPTURE_DONE) {
		info->width = 0;
		info->height = 0;
		return -EINVAL;
	}
#endif

	info->width = cssbuf->width;
	info->height = cssbuf->height;

	return CSS_SUCCESS;
}

static int camera_default_zsl_alloc_buffer(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_buffer_args *buf_args = (struct css_zsl_buffer_args *)arg;
	struct css_zsl_device *zsl_dev = NULL;
	struct capture_fh *capfh = file->private_data;
	unsigned int buf_cnt = 0;
	unsigned int buf_size = 0;

	if (capture_get_stereo_status(capfh->cssdev)) {
		if (capfh->main_scaler != CSS_SCALER_0) {
			css_err("zsl invalid scaler number!!\n");
			return -EINVAL;
		}
	} else {
		if (capfh->main_scaler > CSS_SCALER_1) {
			css_err("zsl invalid scaler number!!\n");
			return -EINVAL;
		}
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return -EINVAL;
	}

	buf_cnt = (buf_args->count + 1);
	if (buf_args->width == 0 || buf_args->height == 0) {
		ret = CSS_FAILURE;
		return -EINVAL;
	}

	buf_size = (buf_args->width * buf_args->height * 2) * buf_cnt;

	zsl_dev->buf_width	= buf_args->width;
	zsl_dev->buf_height	= buf_args->height;
	zsl_dev->buf_count	= buf_cnt;
	zsl_dev->buf_size	= buf_size;

	css_info("zsl[%d] set buffer count = %d!!\n",
				zsl_dev->path, zsl_dev->buf_count);
	css_zsl("zsl[%d] buffer width:  %d\n",
				zsl_dev->path, zsl_dev->buf_width);
	css_zsl("zsl[%d] buffer height: %d\n",
				zsl_dev->path, zsl_dev->buf_height);
	css_zsl("zsl[%d] set buffer size  = %d(KB)!!\n",
				zsl_dev->path, zsl_dev->buf_size/1024);

	ret = bayer_buffer_alloc(capfh);
	if (ret != CSS_SUCCESS)
		return -EINVAL;

	return ret;
}

static int camera_default_zsl_free_buffer(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;

	if (capture_get_stereo_status(capfh->cssdev)) {
		if (capfh->main_scaler != CSS_SCALER_0) {
			css_err("zsl invalid scaler number!!\n");
			return -EINVAL;
		}
	}

	ret = bayer_buffer_free(capfh);
	if (ret != CSS_SUCCESS) {
		return -EINVAL;
	}

	return ret;
}

static int camera_default_zsl_control(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_control_args *ctrl_args = (struct css_zsl_control_args *)arg;
	struct capture_fh *capfh = file->private_data;

	int cmd_mode = (int)ctrl_args->cmode;

	if (capture_get_stereo_status(capfh->cssdev)) {
		if (capfh->main_scaler != CSS_SCALER_0) {
			css_err("zsl invalid scaler number!!\n");
			return -EINVAL;
		}
	}

	switch (cmd_mode) {
	case ZSL_START:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_store_enable(capfh);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case ZSL_PAUSE:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_store_pause(capfh);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case ZSL_RESUME:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_store_resume(capfh);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case ZSL_STOP:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_store_disable(capfh);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case ZSL_STORE_MODE:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_store_mode(capfh, ctrl_args->storemode);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_STORE_BIT_FORMAT:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_store_bit_format(capfh, ctrl_args->bitformat);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_STORE_FRAME_INTERVAL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_store_frame_interval(capfh, ctrl_args->frame_interval);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_STORE_WAIT_BUFFER_FULL:
		ret = bayer_store_wait_buffer_full(capfh);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_STORE_STATISTIC:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_store_statistic(capfh,
					(struct zsl_store_statistic *)&ctrl_args->statistic);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_STORE_GET_QUEUED_BUF_COUNT:
		ctrl_args->queued_count = 0;
		ret = bayer_store_get_queued_count(capfh, &ctrl_args->queued_count);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_LOAD_BUFFER_OFFSET:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_buf_index(capfh, ctrl_args->buf_offset);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_LOAD_ORDER:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_buf_order(capfh, (unsigned int)ctrl_args->order);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_LOAD_FRAME_NUMBER:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_frame_number(capfh, (int)ctrl_args->frame_number);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_LOAD_MODE:
		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_mode(capfh, ctrl_args->loadmode);
		mutex_unlock(&capfh->v4l2_mutex);
		if (ret != CSS_SUCCESS) ret = -EINVAL;
		break;
	case ZSL_OPEN_BAYER_FD:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_fd_open(&ctrl_args->bayer_fd, &ctrl_args->bayer_size,
						&ctrl_args->bayer_align_size);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case ZSL_RELEASE_BAYER_FD:
		mutex_lock(&capfh->v4l2_mutex);
		bayer_fd_release();
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	default:
		return -EINVAL;
		break;
	}
	return ret;
}

static int camera_default_fd_control(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_fd_contrl_args *fd_args;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = capfh->cssdev;
	struct css_fd_device *fd_dev = cssdev->fd_device;
	int i;
#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_handle *handle = NULL;
	int file_desc;
#endif

	css_fd("CSS_IOC_FD_CONTROL S \n");

	fd_args = (struct css_fd_contrl_args *)arg;

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

	if (fd_args->cmd == FD_SET_CROP_SIZE) {
		fd_dev->config_data.crop.start_x= fd_args->crop.start_x;
		fd_dev->config_data.crop.start_y = fd_args->crop.start_y;
		fd_dev->config_data.crop.width = fd_args->crop.width;
		fd_dev->config_data.crop.height = fd_args->crop.height;

		css_fd("fd crop x = %d, y = %d, w = %d, h = %d\n",
						fd_dev->config_data.crop.start_x,
						fd_dev->config_data.crop.start_y,
						fd_dev->config_data.crop.width,
						fd_dev->config_data.crop.height);
	} else if (fd_args->cmd == FD_SET_CONFIG) {
		if (fd_dev->start == 1) {
			css_err("fd is running already\n");
			return CSS_ERROR_FD_IS_RUN_ALREADY;
		}

	camera_fd_init(capfh);

#ifdef CONFIG_ODIN_ION_SMMU
	if (fd_dev->client == NULL) {
		fd_dev->client = odin_ion_client_create("fd_ion");
		if (IS_ERR_OR_NULL(fd_dev->client)) {
			return CSS_ERROR_FD_BUF_CLIENT;
		}
	} else {
		return CSS_ERROR_FD_BUF_CLIENT_EXIST;
	}

	for (i = 0; i < FACE_BUF_MAX; i++) {
		file_desc = fd_args->fd_buf_addr[i];
		handle = ion_import_dma_buf(fd_dev->client, file_desc);
		if (IS_ERR_OR_NULL(handle)) {
			css_err("can't get import_dma_buf fd handle(%p) %p (%d)\n",
						handle, fd_dev->client,	file_desc);
			css_get_fd_info(file_desc);
			ret = -EINVAL;
			goto err;
		}
		fd_dev->bufs[i].ion_hdl.handle = handle;
		fd_dev->bufs[i].dma_addr = (dma_addr_t)odin_ion_get_iova_of_buffer(
											handle, ODIN_SUBSYS_CSS);

		fd_dev->bufs[i].ion_cpu_addr
						= (unsigned long)ion_map_kernel(fd_dev->client, handle);
		if (fd_dev->bufs[i].ion_cpu_addr) {
			css_fd("fd ion_map_kernel = %lx\n\n",
					fd_dev->bufs[i].ion_cpu_addr);
		} else {
			css_err("fd ion_cpu_addr is NULL\n");
			ret = CSS_ERROR_FD_BUF_ALLOC_FAIL;
			goto err;
		}
	}
#endif

	/* fd_dev->config_data.direct = fd_args->dir; */
	fd_dev->config_data.threshold = fd_args->threshold;
	fd_dev->config_data.res = fd_args->resolution;

	css_fd("direct : %d\n",fd_dev->config_data.direct);
	css_fd("threshold : %d\n",fd_dev->config_data.threshold);

		camera_fd_set_resolution(fd_dev->config_data.res);
	} else {
		css_err("invalid fd control commmad");
		return CSS_ERROR_FD_BUF_INVALID_COMMAND;
	}

	return CSS_SUCCESS;

err:
	camera_fd_buf_destory(fd_dev);
	return ret;
}

static int camera_default_get_fd_data(struct file *file, void *arg)
{
#ifdef FD_PAIR_GET_DATA
	struct css_fd_pair_result_data *fd_args =
				(struct css_fd_pair_result_data *)arg;
#else
	struct css_fd_result_data *fd_args = (struct css_fd_result_data *)arg;
#endif
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = capfh->cssdev;
	struct css_fd_device *fd_dev = cssdev->fd_device;
	int face_num = 0;
	int buf_index = 0;
	int i;

	css_fd("CSS_IOC_GET_FD_DATA S \n");

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

#ifdef FD_PAIR_GET_DATA
	for (i = 0 ; i < 2 ; i++) {
		memcpy(&fd_args->fd_data[i] ,
			&fd_dev->last_result_data[i],
			sizeof(struct css_fd_result_data));
	}
#else
	face_num = odin_fd_get_face_count();
	fd_args->face_num = face_num;

	fd_args->crop.start_x = fd_dev->result_data.crop.start_x;
    fd_args->crop.start_y = fd_dev->result_data.crop.start_y;
    fd_args->crop.width = fd_dev->result_data.crop.width;
    fd_args->crop.height = fd_dev->result_data.crop.height;
    fd_args->direct = fd_dev->result_data.direct;

	if (face_num) {
		memcpy(fd_args->fd_face_info, fd_dev->result_data.fd_face_info,
				sizeof(struct css_face_detect_info) * face_num);
#ifdef USE_FPD_HW_IP
			/* if (fd_args->fpd_enable) { */ /* fpd ready */
			memcpy(fd_args->fpd_face_info, fd_dev->result_data.fpd_face_info,
					sizeof(struct css_facial_parts_info) * face_num);
			/* } */
#else
		if (fd_args->fpd_enable) {
			if (fd_args->get_frame) {
				buf_index = fd_dev->result_data.result_buf_index;
				css_fd("kernel : Get frame buf = %d\n", buf_index);

				fd_args->result_buf_index = buf_index;
				fd_dev->fd_info[buf_index].state = FPD_RUN;
				for (i = 0; i < FACE_BUF_MAX; i++) {
					css_fd("FD GET : buf = %d, state = %d\n",
							i, fd_dev->fd_info[i].state);
				}
			} else {
				fd_args->result_buf_index = -1;
			}
		}
#endif
	}
#endif

	return CSS_SUCCESS;
}

static int camera_default_set_sensor_size(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_info *sensor_args = (struct css_sensor_info *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev = capfh->cam_dev;
	struct v4l2_mbus_framefmt m_pix = {0,};

	css_info("sensor_args->width(%d) sensor_args->height(%d)\n",
		sensor_args->width, sensor_args->height);

	m_pix.width = sensor_args->width;
	m_pix.height = sensor_args->height;
	/* m_pix.reserved[0] = (css_behavior)fmt->fmt.pix.priv; */

	if (cam_dev && cam_dev->cur_sensor && cam_dev->cur_sensor->sd) {
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
											video, s_mbus_fmt, &m_pix);
	} else {
		css_err("set sensor size : current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	return ret;
}

int camera_default_get_sensor_init_size(struct file *file, void *arg)
{
	struct css_sensor_info *sensor_args = (struct css_sensor_info *)arg;
	struct css_device *cssdev = video_drvdata(file);
	unsigned int index = sensor_args->index;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (index > SENSOR_S3D) {
		css_err("invaild sensor index\n");
		return -EINVAL;
	}
#else
	if (index > SENSOR_DUMMY) {
		css_err("invaild sensor index\n");
		return -EINVAL;
	}
#endif

	sensor_args->width = cssdev->sensor[index]->init_width;
	sensor_args->height = cssdev->sensor[index]->init_height;

	css_info("sensor init width(%d) sensor init height(%d)\n",
			sensor_args->width, sensor_args->height);

	return CSS_SUCCESS;
}

static int camera_default_sensor_mode_change(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_info *sensor_args = (struct css_sensor_info *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev;
	struct v4l2_mbus_framefmt m_pix = {0,};
	struct v4l2_control vctrl;
#ifndef TEST_INIT_VALUE
	unsigned int *temp;
#endif
	struct i2c_client *client;
	struct css_i2cboard_platform_data *pdata;

	css_info("mode change : w(%d) h(%d)\n",
				sensor_args->width, sensor_args->height);

	if (!capfh || !capfh->cam_dev) {
		css_err("mode change : cam_dev is not found\n");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("mode change : current sensor is not found\n");
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	if (cam_dev->cur_sensor->initialize) {
#ifndef TEST_INIT_VALUE
		if (cam_dev->mmap_sensor_init.cpu_addr) {
			temp = cam_dev->mmap_sensor_init.value;
			if (!*temp) {
				/* no memcpy */
				css_err("sensor init value is empty\n");
				/* mutex_unlock(&capfh->v4l2_mutex); */
				return CSS_ERROR_SENSOR_INIT_VALUE_EMPTY;
			}
		} else {
			/* no mmap */
			css_err("sensor init memory is not exist\n");
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return CSS_ERROR_SENSOR_INIT_MEMORY_IS_NOT_FOUND;
		}

		client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);
		pdata = client->dev.platform_data;


		if (!client || !client->dev.platform_data) {
			css_err("Do not ready to use I2C \r\n");
			/* mutex_unlock(&capfh->v4l2_mutex); */
			return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
		}

		pdata = client->dev.platform_data;
		pdata->sensor_init = cam_dev->mmap_sensor_init;
		pdata->sensor_init.init_size = *temp;
		pdata->sensor_init.mode_size = *(temp + 1);
		pdata->sensor_init.value = (void *)(temp + 2);
		css_info("*temp: %d\n", *temp);
		css_sensor("*(temp + 1): %d\n", *(temp + 1));
		css_info("*(temp + 2): %d\n", *(temp + 2));

#endif
		m_pix.width = sensor_args->width;
		m_pix.height = sensor_args->height;

		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
								video, s_mbus_fmt, &m_pix);
		if (ret < 0) {
			css_err("odin_sensor s_mbus_fmt fail ret=%d\n", ret);
			return ret;
		}

		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
										core, init, CSS_SENSOR_MODE_CHANGE);
		if (ret < 0) {
			css_err("odin_sensor mode change fail ret=%d\n", ret);
			return ret;
		}

		if (cam_dev->cur_sensor->ois_support) {
			vctrl.id = V4L2_CID_CSS_SENSOR_SET_OIS_MODE;

/*                                                                                         */
#if 1  /* TBD... */
			switch (sensor_args->mode) {
			case SENSOR_PREVIEW:
			case SENSOR_CAPTURE:
				vctrl.value = OIS_MODE_CAPTURE;
				break;
			case SENSOR_VIDEO:
				vctrl.value = OIS_MODE_VIDEO;
				break;
			case SENSOR_ZERO_SHUTTER_LAG_PREVIEW:
				vctrl.value = OIS_MODE_PREVIEW_CAPTURE;
				break;
			/*
			case SENSOR_OIS_TEST:
				vctrl.value = OIS_MODE_CENTERING_ONLY;
				break;
			*/
			default:
				vctrl.value = OIS_MODE_VIDEO;
				break;
			}
#else	/* FIX : Center only mode 2013.11.28 */
			vctrl.value = OIS_MODE_CENTERING_ONLY;
#endif
/*                                                                                 */

			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
												core, s_ctrl, &vctrl);
			css_cam_v4l2("s_ctrl %x, mode : %d\n", vctrl.id,vctrl.value);
		}

		m_pix.width = sensor_args->width;
		m_pix.height = sensor_args->height;

		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
								video, s_mbus_fmt, &m_pix);

	} else {
		css_err("odin_sensor doesn't initialize, ret=%d\n", ret);
		ret = CSS_ERROR_SENSOR_DOES_NOT_READY;
	}

	return ret;
}

static int camera_default_sensor_mode_change_aat(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_info *sensor_args = (struct css_sensor_info *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev;
	struct v4l2_mbus_framefmt m_pix = {0,};
	struct v4l2_control vctrl;

	css_info("mode change : w(%d) h(%d)\n",
				sensor_args->width, sensor_args->height);

	if (!capfh || !capfh->cam_dev) {
		css_err("mode change : cam_dev is not found\n");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("mode change : current sensor is not found\n");
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	if (cam_dev->cur_sensor->initialize) {
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
								core, init, CSS_SENSOR_MODE_CHANGE_FOR_AAT);
		if (ret < 0) {
			css_err("odin_sensor mode change fail ret=%d\n", ret);
			return ret;
		}

		m_pix.width = sensor_args->width;
		m_pix.height = sensor_args->height;

		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
								video, s_mbus_fmt, &m_pix);

	} else {
		css_err("odin_sensor doesn't initialize, ret=%d\n", ret);
		ret = CSS_ERROR_SENSOR_DOES_NOT_READY;
	}

	return ret;
}

static int camera_default_ois_control(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_ois_control_args *ois_args = (struct css_ois_control_args *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev;
	struct i2c_client *client;
	struct css_i2cboard_platform_data *pdata;
	struct v4l2_control vctrl;

	css_info("CSS_IOC_OIS_CONTROL\n");

	if (!capfh || !capfh->cam_dev) {
		css_err("ois control : cam_dev is not found\n");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("ois control : current sensor is not found\n");
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);
	if (cam_dev->cur_sensor->ois_support) {
		pdata = client->dev.platform_data;

		if (ois_args->cmd == OIS_MOVE_LENS) {
			css_info("lens_x(%d) lens_y(%d)\n",
				ois_args->info.x, ois_args->info.y);

			pdata->ois_info.lens.x = ois_args->info.x;
			pdata->ois_info.lens.y = ois_args->info.y;

			vctrl.id = V4L2_CID_CSS_SENSOR_MOVE_OIS_LENS;
			/*
			 * vctrl.value = OIS_MODE_LENS_MOVE_SET;
			 * ret = v4l2_subdev_call(cam_dev->cur_sensor->sd,
			 *							core, s_ctrl, &vctrl);
			 */
			vctrl.value = OIS_MODE_LENS_MOVE_START;
#if 1
			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, g_ctrl, &vctrl);
			css_info("subdev call result %d\n", ret);
			return ret;
#else
			ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);

#endif
		} else if (ois_args->cmd == OIS_ON) {
			vctrl.id = V4L2_CID_CSS_SENSOR_OIS_ON;
			vctrl.value = ois_args->version;
		} else if (ois_args->cmd == SET_VERSION) {
			vctrl.id = V4L2_CID_CSS_SENSOR_SET_OIS_VERSION;
			vctrl.value = ois_args->version;
		} else if (ois_args->cmd == OIS_MODE) {
			switch (ois_args->mode) {
			case SENSOR_PREVIEW:
			case SENSOR_VIDEO:
				vctrl.value = OIS_MODE_VIDEO;
				break;
			case SENSOR_CAPTURE:
				vctrl.value = OIS_MODE_CAPTURE;
				break;
			case SENSOR_ZERO_SHUTTER_LAG_PREVIEW:
				vctrl.value = OIS_MODE_PREVIEW_CAPTURE;
				break;
			case SENSOR_OIS_TEST:
				vctrl.value = OIS_MODE_CENTERING_ONLY;
				break;
				/*                                                            */
			case SENSOR_OIS_OFF:
				vctrl.value = OIS_MODE_CENTERING_OFF;
				break;
				/*                                                            */
			default:
				vctrl.value = OIS_MODE_VIDEO;
				break;
			}
			vctrl.id = V4L2_CID_CSS_SENSOR_SET_OIS_MODE;
		}
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);
	}
	return ret;
}

static int camera_default_get_ois_data(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_ois_status_data *ois_args = (struct css_ois_status_data *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev;
	struct i2c_client *client;
	struct css_i2cboard_platform_data *pdata;
	struct v4l2_control vctrl;

	css_info("CSS_IOC_GET_OIS_DATA\n");

	if (!capfh || !capfh->cam_dev) {
		css_err("git ois data : cam_dev is not found\n");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("get ois data : current sensor is not found\n");
		return CSS_ERROR_SENSOR_IS_NOT_FOUND;
	}

	if (cam_dev->cur_sensor->ois_support) {
		vctrl.id = V4L2_CID_CSS_SENSOR_GET_OIS_STATUS;
		vctrl.value = 0;
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, g_ctrl, &vctrl);

		client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);
		pdata = client->dev.platform_data;
		memcpy(ois_args, &pdata->ois_info.ois_status,
					sizeof(struct css_ois_status_data));
	}

	return ret;
}

static int camera_default_i2c_write(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_i2c_data *sensor_args = (struct css_i2c_data *)arg;
	struct css_sensor_table *table = &sensor_args->table;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev = NULL;
	struct i2c_client *client = NULL;
	int i;
	unsigned char *reg_table;
	unsigned char regs[3] = {0, };

	css_cam_v4l2("CSS_IOC_I2C_WRITE SS =%d \n",sensor_args->size);

	if (!capfh || !capfh->cam_dev) {
		css_err("i2c write : cam_dev is not found\n");
		ret = CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
		goto invalid_ioctl;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("i2c write : current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
		goto invalid_ioctl;
	}

	client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);
	if (!client) {
		css_err("Do not ready to use I2C \r\n");
		ret = CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
		goto invalid_ioctl;
	}

	for (i = 0; i < sensor_args->size; i++) {
		regs[0] = table->addr >> 8;
		regs[1] = table->addr & 0xff;
		regs[2] = table->data & 0xff;

		reg_table = regs;
		ret = camera_i2c_write(client, reg_table, 3);

		css_cam_v4l2(" Dev ID = %02x, addr1 = %02x ,",
						client->addr, *reg_table);
		css_cam_v4l2(" addr2 = %02x data = %02x, ret = %d\n",
						*(reg_table+1), *(reg_table+2), ret);

		table = table + 1;
	}

invalid_ioctl:
	return ret;
}

static int camera_default_i2c_read(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_i2c_data *sensor_args = (struct css_i2c_data *)arg;
	struct css_sensor_table *table = &sensor_args->table;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev = NULL;
	unsigned char *reg_table;
	struct i2c_client *client = NULL;
	unsigned char regs[3] = {0, };

	css_cam_v4l2("CSS_IOC_I2C_READ\n");

	if (!capfh || !capfh->cam_dev) {
		css_err("i2c read : cam_dev is not found\n");
		ret = CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
		goto invalid_ioctl;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("i2c read : current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
		goto invalid_ioctl;
	}

	client = v4l2_get_subdevdata(cam_dev->cur_sensor->sd);

	if (!client) {
		css_err("Do not ready to use I2C \r\n");
		ret = CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
		goto invalid_ioctl;
	}
#if 1
	regs[0] = table->addr >> 8;
	regs[1] = table->addr & 0xff;

	reg_table = regs;

	ret = camera_i2c_read(client, reg_table);
	table->data = *reg_table;
#else
	reg_table = &sensor_args->value;

	ret = camera_i2c_read(client, reg_table);
#endif

invalid_ioctl:
	return ret;
}

static int camera_default_flash_control(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	struct css_flash_control_args *flash_args =
		(struct css_flash_control_args *)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_sensor_device *cam_dev = NULL;
	unsigned int cmd_mode = (unsigned int)flash_args->mode;
	struct v4l2_control vctrl;
	struct css_flash_control_args flash;

	css_sensor("CSS_IOC_FLASH_CONTROL\n");
	if (!capfh || !capfh->cam_dev) {
		css_err("i2c read : cam_dev is not found\n");
		ret = CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
		goto invalid_ioctl;
	}

	cam_dev = capfh->cam_dev;
	if (!cam_dev->cur_sensor || !cam_dev->cur_sensor->sd) {
		css_err("i2c read : current sensor is not found\n");
		ret = CSS_ERROR_SENSOR_IS_NOT_FOUND;
		goto invalid_ioctl;
	}
#ifdef CONFIG_VIDEO_ODIN_FLASH
	if (cmd_mode == CSS_TORCH_ON) {
			css_info(" TORCH_ON \n");
			/*flash_hw_init();*/
			flash.mode = CSS_TORCH_ON;
			flash.flash_current[0] = 0x64;
			flash.flash_current[1] = 0x7F;
			flash_control(&flash);
		} else if (cmd_mode == CSS_TORCH_OFF) {
			css_info(" TORCH_OFF \n");
			flash.mode = CSS_TORCH_OFF;
			flash_control(&flash);
			/*flash_hw_deinit();*/
		} else if (cmd_mode == CSS_LED_HIGH) {
			flash_hw_init();
		} else if (cmd_mode == CSS_LED_OFF) {
			flash_hw_deinit();
		} else { /* STROBE_ON */
			css_info(" STROBE_ON \n");
			/*flash_hw_init();*/
			flash.mode = CSS_LED_LOW;
			flash.flash_current[0] = 0x64;
			flash.flash_current[1] = 0x7F;
			flash_control(&flash);
			/*flash.mode = CSS_LED_OFF;
			flash_control(&flash);
			flash_hw_deinit();*/
		}
#else
	if (cmd_mode == CSS_TORCH_ON) {
		css_sensor(" TORCH_ON \n");
		/*
		vctrl.id = V4L2_CID_CSS_SENSOR_TORCH;
		vctrl.value = TORCH_ON;
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);
		*/
	} else if (cmd_mode == CSS_TORCH_OFF) {
		css_sensor(" TORCH_OFF \n");
		/*
		vctrl.id = V4L2_CID_CSS_SENSOR_TORCH;
		vctrl.value = TORCH_OFF;
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);
		*/
	} else if ( cmd_mode == CSS_STROBE_ON) { /* STROBE_ON */
		css_sensor(" STROBE_ON \n");
		/*
		vctrl.id = V4L2_CID_CSS_SENSOR_STROBE;
		vctrl.value = STROBE_ON;
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);
		vctrl.value = STROBE_DEFAULT;
		ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, core, s_ctrl, &vctrl);
		*/
	} else {
		css_sensor("flash %d\n", cmd_mode);
	}
#endif

invalid_ioctl:
	return ret;
}

static int camera_default_sensor_reschg_ind(struct file *file, void *arg)
{
	struct css_sensor_resolution_change_ind *ind = NULL;
	/* struct capture_fh *capfh = file->private_data; */

	ind = (struct css_sensor_resolution_change_ind *)arg;
	if (ind != NULL) {
		if (ind->cmd == RESCHG_PREPARE) {
			/* capture_skip_frame(capfh->cssdev, capfh->main_scaler, 1); */
			switch (ind->camid) {
			case 0: /*REAR*/
				break;
			case 1: /*FRONT*/
				break;
			case 2: /*STEREO*/
				break;
			default:
				break;
			}
		} else if (ind->cmd == RESCHG_DONE) {
#ifdef ENABLE_ERR_INT_CODE
			scaler_int_disable(INT_ERROR);
#endif

			switch (ind->camid) {
			case 0: /*REAR*/
				odin_mipicsi_wait_stable(CSI_CH0,
							ind->width,
							ind->height,
							7);
				break;
			case 1: /*FRONT*/
				odin_mipicsi_wait_stable(CSI_CH2,
							ind->width,
							ind->height,
							7);

				break;
			case 2: /*STEREO*/
				odin_mipicsi_wait_stable(CSI_CH0,
							ind->width,
							ind->height,
							7);
				odin_mipicsi_wait_stable(CSI_CH1,
							ind->width,
							ind->height,
							7);
				break;
			default:
				break;
			}
#ifdef ENABLE_ERR_INT_CODE
			scaler_int_enable(INT_ERROR);
#endif
		}
	}

	return CSS_SUCCESS;
}

static int camera_default_set_isp_performance(struct file *file, void *arg)
{
	unsigned int performance = (unsigned int)*(unsigned int*)arg;
	struct capture_fh *capfh = file->private_data;
	struct css_device *cssdev = video_drvdata(file);
	int idx = 0;

	if (capfh && cssdev) {
		idx = capfh->main_scaler;

		if (idx < 0 || idx >= ISP_MAX) {
			css_err("invalid isp index: %d\n", idx);
			return -EINVAL;
		}

		if (performance == VT_ISP_PERFORMANCE) {
			performance = MINIMUM_ISP_PERFORMANCE;
#ifdef CSS_VT_MEM_CTRL_SUPPORT
			css_bus_control(MEM_BUS, CSS_VT_MEM_BUS_FREQ_KHZ,
							CSS_BUS_TIME_SCHED_NONE);
#endif
		} else if (performance < DEFAULT_ISP_PERFORMANCE)
			performance = DEFAULT_ISP_PERFORMANCE;
		else if (performance > DEFAULT_ISP_PERFORMANCE &&
			performance < MAXIMUM_ISP_PERFORMANCE)
			performance = MAXIMUM_ISP_PERFORMANCE;
		else if (performance > MAXIMUM_ISP_PERFORMANCE)
			performance = MAXIMUM_ISP_PERFORMANCE;

		cssdev->ispclk.clk_hz[idx] = performance;
		capture_wait_any_scaler_vsync(150);
		camera_set_isp_clock(cssdev);
	}

	return CSS_SUCCESS;
}

static int camera_default_get_live_path_support(struct file *file, void *arg)
{
	unsigned int *support = (unsigned int*)arg;
	struct css_device *cssdev = video_drvdata(file);

	if (support == NULL)
		return -EINVAL;

	if (cssdev == NULL)
		return -EINVAL;

	*support = cssdev->livepath_support;

	return CSS_SUCCESS;
}

static int camera_default_set_path_mode(struct file *file, void *arg)
{
	unsigned int mode = *(unsigned int*)arg;
	struct css_device *cssdev = video_drvdata(file);

	if (cssdev == NULL)
		return -EINVAL;

	if (mode > CSS_LIVE_PATH_MODE) {
		css_err("invalid path mode %d\n", mode);
		return -EINVAL;
	}

	if (!cssdev->livepath_support && mode == CSS_LIVE_PATH_MODE) {
		css_err("NOT support live path mode\n");
		return -EINVAL;
	}

	cssdev->path_mode = (css_path_mode)mode;

#ifndef CSS_VT_MEM_CTRL_SUPPORT
	if (cssdev->path_mode == CSS_LIVE_PATH_MODE)
		css_bus_control(MEM_BUS, CSS_CAPTURE_MEM_BUS_FREQ_KHZ,
					CSS_BUS_TIME_SCHED_NONE);
	else
		css_bus_control(MEM_BUS, CSS_DEFAULT_MEM_BUS_FREQ_KHZ,
					CSS_BUS_TIME_SCHED_NONE);
#endif

	css_info("path_mode: %d\n", mode);

	return CSS_SUCCESS;
}

static int camera_default_set_zsl_read_path(struct file *file, void *arg)
{
	struct css_afifo_param_args *param = (struct css_afifo_param_args *)arg;
	struct css_afifo_pre_ctrl_args *ctrl_args = \
		(struct css_afifo_pre_ctrl_args *)&param->pre;

	if (ctrl_args->read_path_en)
		odin_isp_wrap_set_read_path_enable(ctrl_args->read_path_sel);
	else
		odin_isp_wrap_set_read_path_disable(ctrl_args->read_path_sel);

	return CSS_SUCCESS;
}

static int camera_default_set_live_path(struct file *file, void *arg)
{
	s32 i = 0;
	unsigned int enable = *(unsigned int*)arg;
	struct css_device *cssdev = video_drvdata(file);
	struct css_afifo_pre *pre = &cssdev->afifo.pre[0];

	i = cssdev->afifo.pre_index;

	odin_isp_wrap_set_live_path(&pre[i], enable);
	return CSS_SUCCESS;
}


static int camera_default_set_capture_frame_interval(struct file *file,
		void *arg)
{
	struct capture_fh *capfh = file->private_data;
	struct css_bufq *bufq = NULL;
	unsigned int interval = 0;

	if (capfh == NULL)
		return CSS_FAILURE;

	if (capfh->fb == NULL)
		return CSS_FAILURE;

	if (arg == NULL)
		return CSS_FAILURE;

	interval = *(unsigned int*)arg;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
	if (bufq == NULL || bufq->bufs == NULL) {
		css_err("bufq (or bufs) is NULL (capfh[%d])!\n",
				capfh->main_scaler);
		return CSS_FAILURE;
	}

	bufq->frminterval = interval;

	css_info("capture interval: %d\n", interval);
	return CSS_SUCCESS;
}

static int camera_default_set_sensor_rom_power(struct file *file, void *arg)
{
	int ret = CSS_SUCCESS;
	unsigned int on = 0;
	struct css_device *cssdev;
	struct css_platform_sensor *sensor;
	struct i2c_client *client = NULL;
	struct capture_fh *capfh;
	struct css_sensor_device *cam_dev;
	sensor_index sen_idx = 0; /* only for REAR sensor */

	capfh = file->private_data;
	if (capfh == NULL)
		return CSS_FAILURE;

	cam_dev = capfh->cam_dev;
	if (!cam_dev) {
		css_err("cam_dev is not allocated");
		return CSS_ERROR_CAM_DEV_IS_NOT_FOUND;
	}

	cssdev = video_drvdata(file);
	if (cssdev == NULL)
		return -EINVAL;

	if (arg == NULL)
		return CSS_FAILURE;

	on = *(unsigned int*)arg;

	if (on) {
		/* subdev init */
		if (!cssdev->sensor[sen_idx]->sd) {
			ret = camera_subdev_configure(cssdev, sen_idx);
			if (ret < 0) {
				css_err("camera_subdev_configure fail ret=%d\n", ret);
				return CSS_ERROR_SENSOR_CONFIG_SUBDEV_FAIL;
			}
		} else {
			css_err("current sensor exists already\n");
		}

		sensor = cssdev->sensor[sen_idx];
		sensor->id = sen_idx;
		sensor->initialize = 0;
		cam_dev->cur_sensor = sensor;

		/* sensor poewr on */
		if (sensor->power != 1) {
			ret = v4l2_subdev_call(sensor->sd, core, s_power, 1);
			if (ret < 0) {
				css_err("odin_sensor power fail ret=%d\n", ret);
				return ret;
			}
			sensor->power = 1;
		}
	}

	return CSS_SUCCESS;
}

static long camera_v4l2_default(struct file *file, void *fh,
				bool valid_prio, int cmd, void *arg)
{
	long ret = CSS_SUCCESS;
	struct capture_fh *capfh = file->private_data;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}


	switch (cmd) {
	case CSS_IOC_ISP_CONTROL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_isp_control(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_MDIS_CONTROL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_mdis_control(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_ASYNC_CTRL_PARAMS:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_async_ctrl_params(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_POSTVIEW_CONFIG:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_postview_config(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_GET_POSTVIEW_INFO:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_get_postview_info(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_GET_FRAME_SIZE_INFO:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_get_frame_size_info(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_ZSL_ALLOC_BUFFER:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_zsl_alloc_buffer(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_ZSL_FREE_BUFFER:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_zsl_free_buffer(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_ZSL_CONTROL:
		ret = camera_default_zsl_control(file, arg);
		break;
	case CSS_IOC_FD_CONTROL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_fd_control(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_GET_FD_DATA:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_get_fd_data(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_SENSOR_SIZE:
		ret = camera_default_set_sensor_size(file, arg);
		break;
	case CSS_IOC_GET_SENSOR_INIT_SIZE:
		ret = camera_default_get_sensor_init_size(file, arg);
		break;
	case CSS_IOC_SET_SENSOR_MODE_CHANGE:
		ret = camera_default_sensor_mode_change(file, arg);
		break;
	case CSS_IOC_SENSOR_MODE_CHANGE_AAT:
		ret = camera_default_sensor_mode_change_aat(file, arg);
		break;
	case CSS_IOC_OIS_CONTROL:
		ret = camera_default_ois_control(file, arg);
		break;
	case CSS_IOC_GET_OIS_DATA:
		ret = camera_default_get_ois_data(file, arg);
		break;
	case CSS_IOC_I2C_WRITE:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_i2c_write(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_I2C_READ:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_i2c_read(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_FLASH_CONTROL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_flash_control(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SENSOR_RESCHG_IND:
		ret = camera_default_sensor_reschg_ind(file, arg);
		break;
	case CSS_IOC_SET_ISP_PERFORMANCE:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_isp_performance(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_GET_LIVE_PATH_SUPPORT:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_get_live_path_support(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_PATH_MODE:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_path_mode(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_LIVE_PATH_SET:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_live_path(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_ZSL_READ_PATH_SET :
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_zsl_read_path(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_CAPTURE_FRAME_INTERVAL:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_capture_frame_interval(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	case CSS_IOC_SET_SENSOR_ROM_POWER:
		mutex_lock(&capfh->v4l2_mutex);
		ret = camera_default_set_sensor_rom_power(file, arg);
		mutex_unlock(&capfh->v4l2_mutex);
		break;
	default:
		css_err("Invaild ioctl command %d\n", cmd & 0x1F);
		ret = -EINVAL;
		break;
	}

	return ret;
}

extern struct capture_fh *g_capfh[CSS_MAX_FH];

int camera_scaler_data_init(struct css_scaler_device *scl_dev)
{
	int ret = CSS_SUCCESS;
	struct css_scaler_data *scl_data;

	scl_data = &scl_dev->data;

	scl_data->vsync_delay = 0;

	scl_data->crop.m_crop_h_offset	= 0;
	scl_data->crop.m_crop_h_size	= 0;
	scl_data->crop.m_crop_v_offset	= 0;
	scl_data->crop.m_crop_v_size	= 0;

	scl_data->strobe.enable			= CSS_MODULE_DISABLE;
	scl_data->strobe.time			= 0;
	scl_data->strobe.high			= 0;

	scl_data->effect.command		= CSS_EFFECT_NO_OPERATION;
	scl_data->effect.strength		= CSS_EFFECT_STRENGTH_ONE_EIGHTH;
	scl_data->effect.cb_value		= 0;
	scl_data->effect.cr_value		= 0;

	scl_data->average_mode			= CSS_AVERAGE_NO_OPERATION;
	scl_data->output_pixel_fmt		= CSS_COLOR_YUV422;
	scl_data->instruction_mode		= CSS_INSTRUCTION_NO_OPERATION;
	scl_data->output_image_size.width	= 0;
	scl_data->output_image_size.height	= 0;
	scl_data->scaling_factor.m_scl_h_factor	= 0x400;
	scl_data->scaling_factor.m_scl_v_factor	= 0x400;

	return ret;
}

int camera_v4l2_open(struct file *file)
{
	int ret = CSS_FAILURE;
	struct capture_fh *capfh = NULL;
	struct css_framebuffer *cap_fb = NULL;
	struct css_scaler_device *scl_dev = NULL;
	struct css_sensor_device *cam_dev = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_device *cssdev = NULL;

	int i;
	int open_idx = -1;

	css_info("odin camera open %s\n", file->f_path.dentry->d_iname);

	for (i = 0; i < ARRAY_SIZE(video_dev_name); i++) {
		if (strstr(file->f_path.dentry->d_iname, video_dev_name[i].dev_name)) {
			open_idx = video_dev_name[i].index;
		}
	}

	if (open_idx < 0) {
		css_err("please check open video file number!!\n");
		return CSS_FAILURE;
	}

	cssdev = video_drvdata(file);
	if (cssdev == NULL) {
		css_err("css_dev is NULL!!\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	mutex_lock(&devinit);

	if (g_capfh[open_idx] != NULL) {
		mutex_unlock(&devinit);
		css_warn("already opened video%d file\n", open_idx);
		return -EBUSY;
	}

	capfh = kzalloc(sizeof(struct capture_fh), GFP_KERNEL);
	if (!capfh) {
		mutex_unlock(&devinit);
		return -ENOMEM;
	}
	mutex_init(&capfh->v4l2_mutex);

	capfh->cssdev = cssdev;

	if (g_capfh[0] == NULL && g_capfh[1] == NULL) {
		memset(&capfh->cssdev->zsl_device[0], 0x0,
				sizeof(struct css_zsl_device));
		memset(&capfh->cssdev->zsl_device[1], 0x0,
				sizeof(struct css_zsl_device));
		capture_fps_log_init();
	}

	mutex_lock(&capfh->v4l2_mutex);

	cap_fb = kzalloc(sizeof(struct css_framebuffer), GFP_KERNEL);
	if (!cap_fb) {
		css_err("bufset alloc fail\n");
		goto cam_css_fb_alloc_fail;
	}

	scl_dev = kzalloc(sizeof(struct css_scaler_device), GFP_KERNEL);
	if (!scl_dev) {
		css_err("scl_dev alloc fail\n");
		goto cam_scl_alloc_fail;
	}

	cam_dev = kzalloc(sizeof(struct css_sensor_device), GFP_KERNEL);
	if (!cam_dev) {
		ret = -ENOMEM;
		goto cam_open_fail;
	}

	capfh->fb = cap_fb;
	capfh->scl_dev = scl_dev;
	capfh->cam_dev = cam_dev;
	capfh->main_scaler = open_idx;
	capfh->zslstorefh = 0;

	g_capfh[open_idx] = capfh;

	camera_scaler_data_init(capfh->scl_dev);

	css_power_domain_on(POWER_SCALER);

	scaler_init_device(CSS_SCALER_0);
	scaler_init_device(CSS_SCALER_1);

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);

	if (zsl_dev) {
		zsl_dev->load_running	= 0;
		zsl_dev->frame_interval	= 0;
		zsl_dev->frame_pos		= 1;
		zsl_dev->frame_per_ms	= 0;
		zsl_dev->frame_cnt		= 0;
		zsl_dev->buf_count		= 0;
		zsl_dev->buf_size		= 0;
		zsl_dev->loadbuf_idx	= 0;
		zsl_dev->loadbuf_order	= 0;
		zsl_dev->setloadframe	= 0;
		zsl_dev->bitfmt			= RAWDATA_BIT_MODE_12BIT;
		zsl_dev->storemode		= ZSL_STORE_STREAM_MODE;
		zsl_dev->loadmode		= ZSL_LOAD_DISCONTINUOUS_FRAME_MODE;
		zsl_dev->loadtype		= ZSL_LOAD_ONESHOT_TYPE;
		zsl_dev->loadcnt		= 0;
		memset(&zsl_dev->statistic, 0x0, sizeof(struct zsl_statistic));
		init_completion(&zsl_dev->store_done);
		init_completion(&zsl_dev->store_full);
		init_completion(&zsl_dev->load_done);
	}

	camera_update_isp_clock(capfh->cssdev);

	file->private_data = capfh;

	mutex_unlock(&capfh->v4l2_mutex);
	mutex_unlock(&devinit);

	return CSS_SUCCESS;

cam_open_fail:
	kzfree(scl_dev);

cam_scl_alloc_fail:
	kzfree(cap_fb);

cam_css_fb_alloc_fail:
	kzfree(capfh);

	mutex_unlock(&capfh->v4l2_mutex);
	mutex_unlock(&devinit);

	return ret;
}

int camera_v4l2_close(struct file *file)
{
	int ret = 0;
	int close_idx = 0;
	struct capture_fh *capfh = NULL;
	struct css_device *cssdev = NULL;
	struct css_fd_device *fd_dev = NULL;

	css_info("camera release %s\n", file->f_path.dentry->d_iname);

	capfh = file->private_data;
	if (!capfh) {
		css_err("capfh is NULL\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	cssdev = capfh->cssdev;
	if (!cssdev) {
		css_err("cssdev is NULL\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	close_idx = capfh->main_scaler;

	mutex_lock(&devinit);
	mutex_lock(&capfh->v4l2_mutex);

	if (capture_get_stream_mode(capfh)) {
		capture_stream_mode_off(capfh);
		if (capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL &&
			capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW &&
			capfh->behavior != CSS_BEHAVIOR_CAPTURE_ZSL_3D)
			capture_stop(capfh);
		else
			bayer_load_wait_done(capfh);

		capture_buffer_flush(capfh);
	}

	if (bayer_store_get_status(capfh)) {
		bayer_store_disable(capfh);
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);

	capfh->zslstorefh = 0;

	if (capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL ||
		capfh->behavior == CSS_BEHAVIOR_CAPTURE_ZSL_POSTVIEW ||
		capfh->behavior == CSS_BEHAVIOR_SNAPSHOT ||
		capfh->behavior == CSS_BEHAVIOR_SNAPSHOT_POSTVIEW ||
		capfh->behavior == CSS_BEHAVIOR_RECORDING) {
		if (capture_get_path_mode(capfh->cssdev) == CSS_NORMAL_PATH_MODE) {
			if (capfh->behavior == CSS_BEHAVIOR_RECORDING &&
					capfh->main_scaler == CSS_SCALER_1) {
				/* REAR RECORD */
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
			}
			else
				odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC0_ISP1_LINKTO_SC1);
		}
	}

	bayer_buffer_free(capfh);

	scaler_deinit_device(CSS_SCALER_1);
	scaler_deinit_device(CSS_SCALER_0);

	/* odin_mdis_deinit_device(); */

#ifdef CONFIG_VIDEO_ODIN_S3DC
	switch (capfh->behavior) {
		case CSS_BEHAVIOR_PREVIEW_3D:
			s3dc_set_v4l2_preview_cam_fh(NULL);
			break;
		case CSS_BEHAVIOR_CAPTURE_ZSL_3D:
			s3dc_set_v4l2_capture_cam_fh(NULL);
			break;
		default:
			break;
	}
#endif

	capture_buffer_close(capfh);

	if (g_capfh[close_idx] == capfh) {
		g_capfh[close_idx] = NULL;
	}

	if (g_capfh[0] == NULL && g_capfh[1] == NULL) {
		fd_dev = capfh->cssdev->fd_device;
		if (fd_dev) {
			if (fd_dev->start)
				camera_fd_stop(capfh);
		}
		scaler_device_close();
		capture_fps_log_deinit();
	}

	if (capfh->scl_dev) {
		kzfree(capfh->scl_dev);
		capfh->scl_dev = NULL;
	}
	if (capfh->fb) {
		kzfree(capfh->fb);
		capfh->fb = NULL;
	}
	if (capfh->cam_dev) {
		camera_subdev_release(capfh);
		capfh->cam_dev = NULL;
	}

	css_power_domain_off(POWER_SCALER);

	mutex_unlock(&capfh->cssdev->bayer_mutex);
	mutex_unlock(&capfh->v4l2_mutex);

	if (capfh) {
		kzfree(capfh);
		file->private_data = NULL;
	}

	mutex_unlock(&devinit);

	return ret;
}

ssize_t camera_v4l2_read(struct file *file, char __user *data, size_t count,
			loff_t *ppos)
{
	int ret = 0;

	/* TODO : READ FOR FACE-DETECT DATAS... */
	return ret;
}

ssize_t camera_v4l2_write(struct file *file, const char __user *data,
			size_t count, loff_t *ppos)
{
	int ret = 0;

	/* UNSUPPORTED */
	return ret;
}

int camera_v4l2_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct capture_fh *capfh = file->private_data;
	int ret = CSS_SUCCESS;
	unsigned long start  = vma->vm_start;
	unsigned long size	 = PAGE_ALIGN(vma->vm_end - vma->vm_start);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn	 = __phys_to_pfn(offset); /* same as vma->vm_pgoff */
	unsigned long prot	 = vma->vm_page_prot;

	if (capfh == NULL) {
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

#ifndef CONFIG_ODIN_ION_SMMU
#ifndef USE_FPD_HW_IP
	struct css_fd_device *fd_dev = capfh->cssdev->fd_device;
	int buf_size;

	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}
#endif
#endif

	switch (offset) {
	case MMAP_OFFSET_SENSOR:
	{
		struct capture_fh *capfh = file->private_data;
		struct css_sensor_init *sensor_init = &capfh->cam_dev->mmap_sensor_init;
		dma_addr_t daddr;

#ifdef TEST_INIT_VALUE
		css_info("TEST_INIT_VALUE\n");
		return 0;
#endif

		if (sensor_init->cpu_addr) {
			css_err("mmap is called already\n");
			return CSS_ERROR_SENSOR_INIT_MEMORY_EXIST_ALREADY;
		}

		sensor_init->cpu_addr = dma_alloc_coherent(NULL, size, &daddr,
									GFP_KERNEL);
		sensor_init->value = sensor_init->cpu_addr;
		sensor_init->mmap_size = size;
		sensor_init->dma_addr = daddr;
		css_sensor("MMAP_OFFSET_SENSOR : 0x%x\n",sensor_init->cpu_addr);

		if (dma_mmap_coherent(0, vma, sensor_init->cpu_addr, daddr, size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}

		break;
	}
#ifndef CONFIG_ODIN_ION_SMMU
#ifndef USE_FPD_HW_IP
	case 0xD1070000:
	{
		vma->vm_pgoff = 0;
		buf_size = fd_dev->config_data.width * fd_dev->config_data.height;

#ifdef FD_USE_TEST_IMAGE
		if (dma_mmap_coherent(0, vma, fd_image_cpu_addr[0],
			fd_image_dma_addr[0], buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#else
		if (dma_mmap_coherent(0, vma, fd_dev->bufs[0].cpu_addr,
			fd_dev->bufs[0].dma_addr, buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#endif
		break;
	}
	case 0xD1071000:
	{
		vma->vm_pgoff = 0;
		buf_size = fd_dev->config_data.width * fd_dev->config_data.height;
#ifdef FD_USE_TEST_IMAGE
		if (dma_mmap_coherent(0, vma, fd_image_cpu_addr[1],
			fd_image_dma_addr[1], buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#else
		if (dma_mmap_coherent(0, vma, fd_dev->bufs[1].cpu_addr,
			fd_dev->bufs[1].dma_addr, buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#endif
		break;
	}
	case 0xD1072000:
	{
		vma->vm_pgoff = 0;
		buf_size = fd_dev->config_data.width * fd_dev->config_data.height;
#ifdef FD_USE_TEST_IMAGE
		if (dma_mmap_coherent(0, vma, fd_image_cpu_addr[2],
			fd_image_dma_addr[2], buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#else
		if (dma_mmap_coherent(0, vma, fd_dev->bufs[2].cpu_addr,
			fd_dev->bufs[2].dma_addr, buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#endif
		break;
	}
	case 0xD1073000:
	{
		vma->vm_pgoff = 0;
		buf_size = fd_dev->config_data.width * fd_dev->config_data.height;
#ifdef FD_USE_TEST_IMAGE
		if (dma_mmap_coherent(0, vma, fd_image_cpu_addr[3],
			fd_image_dma_addr[3], buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#else
		if (dma_mmap_coherent(0, vma, fd_dev->bufs[3].cpu_addr,
			fd_dev->bufs[3].dma_addr, buf_size)) {
			css_err("failed to map DMA memory to user space \n");
			return -EAGAIN;
		}
#endif
		break;
	}
#endif
#endif
#ifndef CONFIG_VIDEO_ISP_MISC_DRIVER
	case MMAP_OFFSET_ISP1:
	case MMAP_OFFSET_ISP2:
#endif
	case MMAP_OFFSET_MDIS:
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,6,0))
		vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#else
		vma->vm_flags |= (VM_IO | VM_RESERVED);
#endif
		prot = pgprot_noncached(prot);

		if (remap_pfn_range(vma, start, pfn, size, prot)) {
			css_err("failed to remap kernel memory for ISP to user space \n");
			return -EAGAIN;
		}
		break;

	default:
		if (remap_pfn_range(vma, start, pfn, size, prot)) {
			css_err("failed to remap kernel memory for");
			css_err("frame buffer to user space \n");
			return -EAGAIN;
		}
		break;
	}

#if 0
	if (vma->vm_pgoff == 0) {
		dma_addr_t daddr;

		sensor_reg = dma_alloc_coherent(NULL, size, &daddr, GFP_KERNEL);

		if (dma_mmap_coherent(0, vma, sensor_reg, daddr/*vma->vm_pgoff*/, size))
		{
			printk("ERR:mmap() dma_mmap_coherent \n");
			return -1;
		}
	} else {
		if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
			vma->vm_page_prot))
		{
			css_err("failed to remap kernel memory to user space \n");
			return -1;
		}
	}
#endif

	return ret;
}

unsigned int camera_v4l2_poll(struct file *file, poll_table *wait)
{
	int poll_mask = 0;
	struct css_bufq *bufq = NULL;
	struct capture_fh *capfh = file->private_data;

	bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);

	if (bufq) {
		if (!list_empty(&bufq->head)) {
			poll_mask |= POLLIN | POLLRDNORM;
		} else if (bufq->error) {
			bufq->error = 0;
			poll_mask |= POLLERR;
		} else {
			poll_wait(file, &bufq->complete.wait, wait);
		}
	} else {
		poll_mask |= POLLERR;
		schedule();
	}

	return poll_mask;
}

const struct v4l2_ioctl_ops camera_v4l2_ioctl_ops =
{
	.vidioc_querycap		= camera_v4l2_querycap,
	.vidioc_enum_input		= camera_v4l2_enum_input,
	.vidioc_cropcap			= camera_v4l2_cropcap,
	.vidioc_g_crop			= camera_v4l2_g_crop,
	.vidioc_s_crop			= camera_v4l2_s_crop,
	.vidioc_g_input			= camera_v4l2_g_input,
	.vidioc_s_input			= camera_v4l2_s_input,
	.vidioc_s_ctrl			= camera_v4l2_s_ctrl,
	.vidioc_g_ctrl			= camera_v4l2_g_ctrl,
	.vidioc_g_ext_ctrls	= camera_v4l2_g_ext_ctrls,
	.vidioc_enum_fmt_vid_cap= camera_v4l2_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap	= camera_v4l2_try_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= camera_v4l2_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= camera_v4l2_s_fmt_vid_cap,
	.vidioc_reqbufs			= camera_v4l2_reqbufs,
	.vidioc_querybuf		= camera_v4l2_querybuf,
	.vidioc_qbuf			= camera_v4l2_qbuf,
	.vidioc_dqbuf			= camera_v4l2_dqbuf,
	.vidioc_streamon		= camera_v4l2_streamon,
	.vidioc_streamoff		= camera_v4l2_streamoff,
	.vidioc_default			= camera_v4l2_default,
};

const struct v4l2_file_operations camera_v4l2_fops =
{
	.owner					= THIS_MODULE,
	.open					= camera_v4l2_open,
	.release				= camera_v4l2_close,
	.unlocked_ioctl			= video_ioctl2,
	.mmap					= camera_v4l2_mmap,
	.read					= camera_v4l2_read,
	.write					= camera_v4l2_write,
	.poll					= camera_v4l2_poll,
};

struct video_device camera_v4l2_dev =
{
	.name = ODIN_V4L2_DEV_NAME,
	.fops = &camera_v4l2_fops,
	.ioctl_ops = &camera_v4l2_ioctl_ops,
	.release = video_device_release,
};

int camera_v4l2_init(struct css_device *cssdev)
{
	int ret = -EBUSY;
	struct video_device *vid_dev = NULL;

	BUG_ON(cssdev == NULL);

	/* V4L2 device-subdev registration */
	ret = v4l2_device_register(&cssdev->pdev->dev, &cssdev->v4l2_dev);
	if (ret != CSS_SUCCESS) {
		goto error_video_register;
	}

	/* allocate v4l2 device */
	vid_dev = video_device_alloc();
	if (!vid_dev) {
		css_err("video_dev was failed to allocate!\n");
		goto error_video_alloc;
	}

	/* initialize v4l2 device */
	cssdev->vid_dev = vid_dev;
	memcpy(vid_dev, &camera_v4l2_dev, sizeof(camera_v4l2_dev));

	video_set_drvdata(vid_dev, cssdev);

	/* registers v4l2 device for odin-css to v4l2 layer of kernel	*/
	/* if videoDevNum == -1, first v4l2 device number is 0.			*/
	ret = video_register_device(vid_dev, VFL_TYPE_GRABBER, ODIN_CAMERA_MINOR);
	if (ret) {
		css_err("failed to register video device %d\n", ret);
		goto error_video_reg_dev;
	}

	css_info("odin-capture driver init(%d) success!\n");

	return CSS_SUCCESS;

error_video_reg_dev:
	if (vid_dev)
		video_device_release(vid_dev);

error_video_alloc:
	if (&cssdev->v4l2_dev)
		v4l2_device_unregister(&cssdev->v4l2_dev);

error_video_register:
	return ret;
}

int camera_v4l2_release(struct css_device *cssdev)
{
	struct video_device *vid_dev = cssdev->vid_dev;

	BUG_ON(cssdev == NULL);
	BUG_ON(vid_dev == NULL);

	if (&cssdev->v4l2_dev)
		v4l2_device_unregister(&cssdev->v4l2_dev);

	if (vid_dev)
		video_unregister_device(vid_dev);

	if (vid_dev) {
		video_device_release(vid_dev);
		vid_dev = NULL;
	}

	css_info("camera driver remove success!");

	return CSS_SUCCESS;
}
