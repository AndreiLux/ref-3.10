
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <linux/delay.h>

#include "../odin-css.h"
#include "../odin-mipicsi.h"
#include "../odin-capture.h"
#include "odin-sensor-imx132.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#define IMX132_DRIVER_NAME "IMX132"
#define STREAM_ON_OFF_ADDR	0x0100

int imx132_i2c_write(const struct i2c_client *client, const char *buf,
					int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}

static inline struct imx132_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx132_state, sd);
}

int imx132_stream_on_off(struct v4l2_subdev *sd, unsigned int on_off)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx132_state *state = to_state(sd);
	int result = CSS_SUCCESS;
	unsigned char reg_data[3] = {0, };

	css_info("imx132_stream_on_off :%d\n", on_off);

	reg_data[0] = STREAM_ON_OFF_ADDR >> 8;
	reg_data[1] = STREAM_ON_OFF_ADDR & 0xff;
	reg_data[2] = on_off;

	result = imx132_i2c_write(client, (unsigned char *)reg_data, 3);
	state->sensor_stream_on = on_off;

	return result;
}

int imx132_sensor_power(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx132_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	css_info("imx132_sensor_power : %d \n", on);

	if (on == 1) {
		/* power on */
		if (power->vtcam_1_8v_io)
			regulator_enable(power->vtcam_1_8v_io);
		if (power->vtcam_2_8v_avdd)
			regulator_enable(power->vtcam_2_8v_avdd);
		if (power->vtcam_1_2v_avdd)
			regulator_enable(power->vtcam_1_2v_avdd);
	} else {
		/* power off */
		if (power->vtcam_2_8v_avdd)
			regulator_disable(power->vtcam_2_8v_avdd);
		if (power->vtcam_1_2v_avdd)
			regulator_disable(power->vtcam_1_2v_avdd);
		if (power->vtcam_1_8v_io)
			regulator_disable(power->vtcam_1_8v_io);
	}
	return result;
}

int imx132_sensor_reset(struct v4l2_subdev *sd, int active)
{
	struct imx132_state *state = to_state(sd);
	unsigned int reset_num = state->pdata.reset_gpio.num;

	css_info("imx132_sensor_reset : %d, %d\n", active, reset_num);

	if (gpio_request(reset_num, "imx132_reset") < 0) {
		css_err("gpio_request fail : imx132_reset\n");
		return CSS_FAILURE;
	}
	gpio_direction_output(reset_num, 1);

	/* Set enable Sensor's reset GPIO */
	if (active) {
		gpio_set_value(reset_num, 1);
	} else {
		gpio_set_value(reset_num, 0);
	}

	gpio_free(reset_num);
	return CSS_SUCCESS;
}

static int imx132_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx132_state *state = to_state(sd);

	css_sensor("imx132_sensor_power_sequence\n");

	if (!state) {
		css_err("imx132 state is NULL\n");
		return CSS_ERROR_INVALID_SENSOR_STATE;
	}

	if (on) {
		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_enable(CSI_CH2,1,400,1,1,2048,0);
		}
		/* 0. set reset - low*/
		imx132_sensor_reset(sd, 0);

		/* 1. set power up*/
		imx132_sensor_power(sd, 1);

		udelay(1); /* 500ns */

		/* 2. set reset - XSHUTDOWN high*/
		imx132_sensor_reset(sd, 1);
		udelay(300); /* 300us */

		/* 3. set pll */
		/* mclk on */
		udelay(5); /*76 cycle*/
	} else {
		/* 1. set pll */
		/* mclk off */
		udelay(10); /*128 cycle*/

		/* 2. set reset - XSHUTDOWN low*/
		imx132_sensor_reset(sd, 0);
		udelay(1); /*500ns*/

		/* 3. set power down */
		imx132_sensor_power(sd, 0);

		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_disable(CSI_CH2);
		}
	}

	return result;
}

static int imx132_init(struct v4l2_subdev *sd, unsigned int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = NULL;
	struct imx132_state *state = to_state(sd);

	int i = 0;
	int result = CSS_SUCCESS;

	struct css_setting_table {
		unsigned short addr;
		unsigned char data;
	};

	struct css_setting_table *table;

	unsigned char *reg_data;
	unsigned char regs[3] = {0, };
	unsigned char *reg_table;

#ifdef TEST_INIT_VALUE
	int test_mode = 0;
#endif

	css_sensor("imx132_init() S \r\n");

	if (!client) {
		css_err("imx132_init() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}

	pdata = client->dev.platform_data;
#ifdef TEST_INIT_VALUE
	if (!pdata->sensor_init.value) {
		pdata->sensor_init.value = imx132_reg;
		pdata->sensor_init.init_size = sizeof(imx132_init_reg) / 3;
		pdata->sensor_init.mode_size = sizeof(imx132_mode_reg) / 3;
		test_mode = 1; /* not mmap */
	}
#endif

	if (val == CSS_SENSOR_INIT) {
		css_info("imx132 init : %d \n",pdata->sensor_init.init_size);

		/* write init value */
		if (pdata->sensor_init.value) {
#ifdef TEST_INIT_VALUE
			reg_table = pdata->sensor_init.value;
#else
			table = pdata->sensor_init.value;
#endif

			for (i = 0; i < pdata->sensor_init.init_size; i++) {
#ifdef TEST_INIT_VALUE
				regs[0] = *reg_table;
				regs[1] = *(reg_table + 1);
				regs[2] = *(reg_table + 2);
#else
				regs[0] = table->addr >> 8;
				regs[1] = table->addr & 0xff;
				regs[2] = table->data & 0xff;
#endif
				reg_data = regs;

				result = imx132_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
						" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
						client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx132_i2c_write(client, reg_data, 3);
					css_err("Retry => Dev ID = %02x,"\
						" addr = %04x data = %02x, ret = %d\n",
						client->addr, regs[0] << 8 | regs[1], regs[2], result);
					if (result != 3) {
						css_err("Retry Fail: Dev ID = %02x,"\
								" addr = %04x data = %02x, ret = %d\n",
								client->addr, regs[0] << 8 | regs[1], regs[2],
								result);
						return CSS_ERROR_SENSOR_I2C_FAIL;
					}
				}

#ifdef TEST_INIT_VALUE
				reg_table = reg_table + 3;
#else
				table = table + 1;
#endif
			}
		} else {
			css_err("imx132 init reg is NULL\n");
			return CSS_ERROR_SENSOR_INIT_FAIL;
		}
	} else if (val == CSS_SENSOR_MODE_CHANGE) {
		css_info("imx132 mode change %d\n",pdata->sensor_init.mode_size);

		if (state->sensor_stream_on == 1) {
			imx132_stream_on_off(sd, 0);
		}

		/* write mode change value */
		if (pdata->sensor_init.value) {
#ifdef TEST_INIT_VALUE
			reg_table = pdata->sensor_init.value
						+ pdata->sensor_init.init_size * 3;
#else
			table = pdata->sensor_init.value + pdata->sensor_init.init_size
					* sizeof(struct css_setting_table);
#endif
			for (i = 0; i < pdata->sensor_init.mode_size; i++) {
#ifdef TEST_INIT_VALUE
				regs[0] = *reg_table;
				regs[1] = *(reg_table + 1);
				regs[2] = *(reg_table + 2);
#else
				regs[0] = table->addr >> 8;
				regs[1] = table->addr & 0xff;
				regs[2] = table->data & 0xff;
#endif
				reg_data = regs;

				result = imx132_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
						" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
						client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx132_i2c_write(client, reg_data, 3);
					css_err("Retry => Dev ID = %02x,"\
							" addr = %04x data = %02x, ret = %d\n",
							client->addr, regs[0] << 8 | regs[1], regs[2],
							result);
					if (result != 3) {
						css_err("Retry Fail: Dev ID = %02x,"\
								" addr = %04x data = %02x, ret = %d\n",
								client->addr, regs[0] << 8 | regs[1], regs[2],
								result);
						return CSS_ERROR_SENSOR_I2C_FAIL;
					}
				}

#ifdef TEST_INIT_VALUE
				reg_table = reg_table + 3;
#else
				table = table + 1;
#endif
			}
		} else {
			css_err("imx132 mode change reg is NULL\n");
			return CSS_ERROR_SENSOR_MODE_CHANGE_FAIL;
		}

		state->sensor_stream_on = 1;

		if (pdata->is_mipi) {
			/* only for sony IMX132
			 * from V1 verification code.
			 * D-phy reset sequence must be checked in Gate-Clock Mode
			 * need to verify that below codes are available in Continuous Mode
			*/
			odin_mipicsi_d_phy_reset_on(CSI_CH2);
			mdelay(1);
			odin_mipicsi_d_phy_reset_off(CSI_CH2);
		}
	} else if (val == CSS_SENSOR_DISABLE) {
		css_info("imx132 Disable\n");
		if (state->sensor_stream_on == 1) {
			imx132_stream_on_off(sd, 0);
		}
	}

#ifdef TEST_INIT_VALUE
	if (test_mode) {
		pdata->sensor_init.value = NULL;
		pdata->sensor_init.init_size = 0;
	}
#endif
	return CSS_SUCCESS;
}

static int imx132_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx132_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	/*
	if (width == 640)
		i2c_write();
	*/

	return CSS_SUCCESS;
}

static int imx132_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx132_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;
	/*
	mf->code	= state->fmt->code;
	mf->colorspace	= state->fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
	*/

	return CSS_SUCCESS;
}

static const struct v4l2_subdev_core_ops imx132_core_ops = {
	.init = imx132_init,
	.reset = imx132_sensor_reset,
	.s_power = imx132_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops imx132_video_ops = {
	.s_mbus_fmt = imx132_s_fmt,
	.g_mbus_fmt = imx132_g_fmt,
};

static const struct v4l2_subdev_ops imx132_ops = {
	.core = &imx132_core_ops,
	.video = &imx132_video_ops,
};

static int imx132_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx132_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = NULL;
	struct css_sensor_power *power = NULL;

	css_info("imx132_probe\n");

	if (!client) {
		css_err("imx132_probe() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}
	pdata = client->dev.platform_data;
	power = &pdata->power;

	power->vtcam_1_2v_avdd = regulator_get(NULL, "+1.2V_VT_CAM_DVDD"); /*LDO1*/
	if (IS_ERR(power->vtcam_1_2v_avdd)) {
		css_err("vtcam_1_2v_avdd regulator_get is failed %d\n",
			PTR_ERR(power->vtcam_1_2v_avdd));
		power->vtcam_1_2v_avdd = NULL;
	}

	power->vtcam_2_8v_avdd = regulator_get(NULL, "+2.8V_VT_CAM_AVDD"); /*LDO13*/
	if (IS_ERR(power->vtcam_2_8v_avdd)) {
		css_err("vtcam_2_8v_avdd regulator_get is failed %d\n",
			PTR_ERR(power->vtcam_2_8v_avdd));
		power->vtcam_2_8v_avdd = NULL;
	}

	power->vtcam_1_8v_io = regulator_get(NULL, "+1.8V_VT_CAM_IO");
	if (IS_ERR(power->vtcam_1_8v_io)) {
		css_err("vtcam_1_8v_io regulator_get is failed %d\n",
			PTR_ERR(power->vtcam_1_8v_io));
		power->vtcam_1_8v_io = NULL;
	}

	state = kzalloc(sizeof(struct imx132_state), GFP_KERNEL);
	if (state == NULL) {
		css_err("state alloc fail\n");
		return -ENOMEM;
	}
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, IMX132_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &imx132_ops);

	return CSS_SUCCESS;
}

static int imx132_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx132_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	if (power->vtcam_1_2v_avdd)
		regulator_put(power->vtcam_1_2v_avdd);
	if (power->vtcam_2_8v_avdd)
		regulator_put(power->vtcam_2_8v_avdd);
	if (power->vtcam_1_8v_io)
		regulator_put(power->vtcam_1_8v_io);

	v4l2_device_unregister_subdev(sd);
	kfree(state);
	state = NULL;

	return CSS_SUCCESS;
}

static const struct i2c_device_id imx132_id[] = {
	{IMX132_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx132_id);

static struct i2c_driver imx132_i2c_driver = {
	.driver = {
		.name = IMX132_DRIVER_NAME,
	},
	.probe = imx132_probe,
	.remove = imx132_remove,
	.id_table = imx132_id,
};

static int __init imx132_mod_init(void)
{
	return i2c_add_driver(&imx132_i2c_driver);
}

static void __exit imx132_mod_exit(void)
{
	i2c_del_driver(&imx132_i2c_driver);
}

late_initcall(imx132_mod_init);
module_exit(imx132_mod_exit);

