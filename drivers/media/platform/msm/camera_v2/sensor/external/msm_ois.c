/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[%s::%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"

DEFINE_MSM_MUTEX(msm_ois_mutex);

//#define CONFIG_MSM_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSM_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define CONFIG_MSM_CAMERA_DEBUG_INFO
#undef CDBG_I
#ifdef CONFIG_MSM_CAMERA_DEBUG_INFO
#define CDBG_I(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG_I(fmt, args...) do { } while (0)
#endif

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *a_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *a_ctrl);

static struct i2c_driver msm_ois_i2c_driver;


static int32_t msm_ois_set_mode(struct msm_ois_ctrl_t *a_ctrl,
							uint16_t mode)
{
	uint16_t read_value;

	CDBG_I("Enter\n");
	a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
		&a_ctrl->i2c_client, 0xF8, &read_value,
		MSM_CAMERA_I2C_BYTE_DATA);
		CDBG("[read_data_0xF8::%x]\n", read_value);
	if ( read_value == 0x33 ) {
		CDBG("OIS module is GOOD!!\n");
	} else {
		pr_err("OIS module is BAD - CHIP ID is wrong !!\n");
		return 0;
	}

	CDBG("[mode :: %d] \n", mode);
	switch(mode) {
		case OIS_MODE_OFF:
			CDBG_I("SET :: OIS_MODE_OFF\n");
			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - OFF*/
				&a_ctrl->i2c_client, 0x00, 0x00,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS mode reg set - centering*/
				&a_ctrl->i2c_client, 0x02, 0x05,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - ON*/
				&a_ctrl->i2c_client, 0x00, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);
			break;
		case OIS_MODE_ON_STILL:
			CDBG_I("SET :: OIS_MODE_ON_STILL\n");
			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - OFF*/
				&a_ctrl->i2c_client, 0x00, 0x00,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS mode reg set - still*/
				&a_ctrl->i2c_client, 0x02, 0x00,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - ON*/
				&a_ctrl->i2c_client, 0x00, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);
			break;
		case OIS_MODE_ON_VIDEO:
			CDBG_I("SET :: OIS_MODE_ON_VIDEO\n");
			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - OFF*/
				&a_ctrl->i2c_client, 0x00, 0x00,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS mode reg set - video*/
				&a_ctrl->i2c_client, 0x02, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);

			a_ctrl->i2c_client.i2c_func_tbl->i2c_write(	/* OIS ctrl reg set - ON*/
				&a_ctrl->i2c_client, 0x00, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);
			break;
		default:
			break;
	}
	return 0;
}

static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *a_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_ois_vreg *vreg_cfg;

	CDBG_I("Enter\n");
	vreg_cfg = &a_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt){
		pr_err("failed\n");
		return 0;
	}
	CDBG("[num_vreg::%d]", cnt);

	if (cnt >= MSM_OIS_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(a_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	CDBG_I("Enter\n");
	if (a_ctrl->ois_state != OIS_POWER_DOWN) {
		rc = msm_ois_vreg_control(a_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}
		a_ctrl->ois_state = OIS_POWER_DOWN;
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_config(struct msm_ois_ctrl_t *a_ctrl,
	void __user *argp)
{
	struct msm_ois_cfg_data *cdata =
		(struct msm_ois_cfg_data *)argp;
	int32_t rc = 0;

	mutex_lock(a_ctrl->ois_mutex);
	CDBG_I("Enter\n");
	CDBG_I("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_OIS_SET_MODE:
		CDBG("CFG_OIS_SET_MODE enter \n");
		CDBG("CFG_OIS_SET_MODE value :: %d\n", cdata->set_mode_value);
		rc = msm_ois_set_mode(a_ctrl, cdata->set_mode_value);
		if (rc < 0)
			pr_err("init table failed %d\n", rc);
		break;


	case CFG_OIS_POWERDOWN:
		rc = msm_ois_power_down(a_ctrl);
		if (rc < 0)
			pr_err("msm_ois_power_down failed %d\n", rc);
		break;

	case CFG_OIS_POWERUP:
		rc = msm_ois_power_up(a_ctrl);
		if (rc < 0)
			pr_err("Failed ois power up%d\n", rc);
		break;

	default:
		break;
	}
	mutex_unlock(a_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_get_subdev_id(struct msm_ois_ctrl_t *a_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG_I("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = a_ctrl->pdev->id;
	else
		*subdev_id = a_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_ois_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_ois_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG_I("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
		CDBG("%s:%d request gpio\n", __func__, __LINE__);
		rc = msm_camera_request_gpio_table(
			a_ctrl->gpio_conf->cam_gpio_req_tbl,
			a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
		if (rc < 0) {
			pr_err("%s: request gpio failed\n", __func__);
			return rc;
		}
	}
	CDBG("Exit\n");
	return rc;
}

static int msm_ois_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_ois_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG_I("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->gpio_conf && a_ctrl->gpio_conf->cam_gpio_req_tbl) {
		CDBG("%s:%d release gpio\n", __func__, __LINE__);
		msm_camera_request_gpio_table(
			a_ctrl->gpio_conf->cam_gpio_req_tbl,
			a_ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
	}
	if (a_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}

	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_ois_internal_ops = {
	.open = msm_ois_open,
	.close = msm_ois_close,
};

static long msm_ois_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_ois_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG_I("Enter\n");
	CDBG_I("%s:%d a_ctrl %p argp %p\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ois_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_OIS_IO_CFG:
		return msm_ois_config(a_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		msm_ois_close(sd, NULL);
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG_I("Enter\n");

	rc = msm_ois_vreg_control(a_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_ois_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	CDBG_I("Enter\n");
	mutex_lock(a_ctrl->ois_mutex);
	if (on)
		rc = msm_ois_power_up(a_ctrl);
	else
		rc = msm_ois_power_down(a_ctrl);
	mutex_unlock(a_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
	.ioctl = msm_ois_subdev_ioctl,
	.s_power = msm_ois_power,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
	.core = &msm_ois_subdev_core_ops,
};

static int32_t msm_ois_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	CDBG_I("Enter\n");
	if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio req tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_req_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;
	return rc;
}

static int32_t msm_ois_get_gpio_data(struct msm_ois_ctrl_t *a_ctrl,
	struct device_node *of_node)
{
	int32_t rc = 0;
	uint32_t i = 0;
	uint16_t gpio_array_size = 0;
	uint16_t *gpio_array = NULL;

	CDBG_I("Enter\n");

	a_ctrl->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
		GFP_KERNEL);
	if (!a_ctrl->gpio_conf) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size) {
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
			GFP_KERNEL);
		if (!gpio_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR1;
		}
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			CDBG("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_ois_get_dt_gpio_req_tbl(of_node,
			a_ctrl->gpio_conf, gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
		kfree(gpio_array);
	}
	return 0;

ERROR2:
	kfree(gpio_array);
ERROR1:
	kfree(a_ctrl->gpio_conf);
	return rc;
}

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_ois_ctrl_t *ois_ctrl_t = NULL;
	struct msm_ois_vreg *vreg_cfg;
	bool check_use_gpios;

	CDBG_I("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	if (!client->dev.of_node) {
		ois_ctrl_t = (struct msm_ois_ctrl_t *)(id->driver_data);
	} else {
		ois_ctrl_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
			GFP_KERNEL);
		if (!ois_ctrl_t) {
			pr_err("%s:%d no memory\n", __func__, __LINE__);
			return -ENOMEM;
		}
		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			pr_err("i2c_check_functionality failed\n");
			goto probe_failure;
		}

		CDBG("client = 0x%p\n",  client);

		rc = of_property_read_u32(client->dev.of_node, "cell-index",
			&ois_ctrl_t->subdev_id);
		CDBG("cell-index %d, rc %d\n", ois_ctrl_t->subdev_id, rc);
		ois_ctrl_t->cam_name = ois_ctrl_t->subdev_id;
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			kfree(ois_ctrl_t);//prevent
			return rc;
		}
		check_use_gpios = of_property_read_bool(client->dev.of_node, "unuse-gpios");
		CDBG("%s: check unuse-gpio flag(%d)\n",
			__FUNCTION__, check_use_gpios);
		if (!check_use_gpios) {
			rc = msm_ois_get_gpio_data(ois_ctrl_t,
				client->dev.of_node);
		}
	}

	if (of_find_property(client->dev.of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &ois_ctrl_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data(client->dev.of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(ois_ctrl_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	ois_ctrl_t->ois_mutex = &msm_ois_mutex;
	ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;

	CDBG("client = %x\n", (unsigned int) client);
	ois_ctrl_t->i2c_client.client = client;
	ois_ctrl_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	/* Set device type as I2C */
	ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
	ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	ois_ctrl_t->ois_mutex = &msm_ois_mutex;

	ois_ctrl_t->cam_name = ois_ctrl_t->subdev_id;
	CDBG("ois_ctrl_t->cam_name: %d", ois_ctrl_t->cam_name);
	/* Assign name for sub device */
	snprintf(ois_ctrl_t->msm_sd.sd.name, sizeof(ois_ctrl_t->msm_sd.sd.name),
		"%s", ois_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&ois_ctrl_t->msm_sd.sd,
		ois_ctrl_t->i2c_client.client,
		ois_ctrl_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&ois_ctrl_t->msm_sd.sd, ois_ctrl_t);
	ois_ctrl_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	ois_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&ois_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	ois_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ois_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	ois_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0xB;
	msm_sd_register(&ois_ctrl_t->msm_sd);
	//g_ois_i2c_client.client = ois_ctrl_t->i2c_client.client;

	CDBG("Succeded Exit\n");
	return rc;
probe_failure:
	if (ois_ctrl_t)
		kfree(ois_ctrl_t);
	return rc;
}

static int32_t msm_ois_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_ois_ctrl_t *msm_ois_t = NULL;
	struct msm_ois_vreg *vreg_cfg;

	CDBG_I("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_ois_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!msm_ois_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}
	msm_ois_t->subdev_id = pdev->id;
	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_ois_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_ois_t->cci_master, rc);
	if (rc < 0) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}
	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_ois_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_ois_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	msm_ois_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	msm_ois_t->ois_mutex = &msm_ois_mutex;
	msm_ois_t->cam_name = pdev->id;

	/* Set platform device handle */
	msm_ois_t->pdev = pdev;
	/* Set device type as platform device */
	msm_ois_t->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_ois_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_ois_t->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	msm_ois_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_ois_t->i2c_client.cci_client) {
		kfree(msm_ois_t->vreg_cfg.cam_vreg);
		kfree(msm_ois_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_ois_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = MASTER_MAX;
	v4l2_subdev_init(&msm_ois_t->msm_sd.sd,
		msm_ois_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_ois_t->msm_sd.sd, msm_ois_t);
	msm_ois_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	msm_ois_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_ois_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_ois_t->msm_sd.sd.name), "msm_ois");
	media_entity_init(&msm_ois_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_ois_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_ois_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	msm_ois_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0xB;
	rc = msm_sd_register(&msm_ois_t->msm_sd);
	//g_ois_i2c_client.cci_client = msm_ois_t->i2c_client.cci_client;

	CDBG("Exit[rc::%d]\n", rc);
	return rc;
}

static const struct i2c_device_id msm_ois_i2c_id[] = {
	{ "msm_ois", (kernel_ulong_t)NULL },
	{ }
};
static const struct of_device_id msm_ois_dt_match[] = {
	{.compatible = "qcom,ois", .data = NULL},
	{}
};

static struct i2c_driver msm_ois_i2c_driver = {
	.id_table = msm_ois_i2c_id,
	.probe  = msm_ois_i2c_probe,
	.remove = __exit_p(msm_ois_i2c_remove),
	.driver = {
		.name = "msm_ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_dt_match,
	},
};

MODULE_DEVICE_TABLE(of, msm_ois_dt_match);

static struct platform_driver msm_ois_platform_driver = {
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_dt_match,
	},
};

static int __init msm_ois_init_module(void)
{
	int32_t rc = 0;
	CDBG_I("Enter\n");
	rc = platform_driver_probe(&msm_ois_platform_driver,
		msm_ois_platform_probe);
	if (rc < 0) {
	    pr_err("%s:%d failed platform driver probe rc %d\n",
		   __func__, __LINE__, rc);
	} else {
		CDBG("%s:%d platform_driver_probe rc %d\n", __func__, __LINE__, rc);
	}
	rc = i2c_add_driver(&msm_ois_i2c_driver);
	if (rc < 0)
	    pr_err("%s:%d failed i2c driver probe rc %d\n",
		   __func__, __LINE__, rc);
	else
            CDBG("%s:%d i2c_add_driver rc %d\n", __func__, __LINE__, rc);

	return rc;
}

module_init(msm_ois_init_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");
