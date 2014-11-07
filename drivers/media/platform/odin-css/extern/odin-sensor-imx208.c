
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
#include "odin-sensor-imx208.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <linux/board_lge.h>

#define IMX208_DRIVER_NAME	"IMX208"
#define STREAM_ON_OFF_ADDR	0x0100

int imx208_i2c_write(const struct i2c_client *client, const char *buf,
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

static inline struct imx208_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx208_state, sd);
}

int imx208_stream_on_off(struct v4l2_subdev *sd, unsigned int on_off)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx208_state *state = to_state(sd);
	int result = CSS_SUCCESS;
	unsigned char reg_data[3] = {0, };

	css_info("imx208_stream_on_off :%d\n", on_off);

	reg_data[0] = STREAM_ON_OFF_ADDR >> 8;
	reg_data[1] = STREAM_ON_OFF_ADDR & 0xff;
	reg_data[2] = on_off;

	result = imx208_i2c_write(client, (unsigned char *)reg_data, 3);
	state->sensor_stream_on = on_off;

	return result;
}

int imx208_sensor_power(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx208_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	css_info("imx208_sensor_power : %d \n", on);

	if (on == 1) {
		/* power on */
		if(lge_get_board_revno() <= HW_REV_B) {
			if (power->cam_1_8v_io)
				regulator_enable(power->cam_1_8v_io);
		}
		if (power->vtcam_2_8v_avdd)
			regulator_enable(power->vtcam_2_8v_avdd);
		if (power->vtcam_1_8v_io)
			regulator_enable(power->vtcam_1_8v_io);
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

		if(lge_get_board_revno() <= HW_REV_B) {
			if (power->cam_1_8v_io)
				regulator_disable(power->cam_1_8v_io);
		}
	}
	return result;
}

int imx208_sensor_reset(struct v4l2_subdev *sd, int active)
{
	struct imx208_state *state = to_state(sd);
	unsigned int reset_num = state->pdata.reset_gpio.num;
	/* state->gpio_base - 8 + state->pdata.reset_gpio.num; */

	css_info("imx208_sensor_reset : %d, %d\n", active, reset_num);

	if (gpio_request(reset_num, "imx208_reset") < 0) {
		css_err("gpio_request fail : imx208_reset\n");
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

static int imx208_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx208_state *state = to_state(sd);

	css_sensor("imx208_sensor_power_sequence\n");

	if (!state) {
		css_err("imx208 state is NULL\n");
		return CSS_ERROR_INVALID_SENSOR_STATE;
	}

	if (on) {
		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_enable(CSI_CH2,1,400,1,1,2048,0);
		}
		/* 0. set reset - low	*/
		imx208_sensor_reset(sd, 0);

		/* 1. set power up		*/
		imx208_sensor_power(sd, 1);

		/* udelay(1); */	/* 500ns */
		mdelay(3); 		/* 500ns */

		/* 2. set reset - XSHUTDOWN high*/
		imx208_sensor_reset(sd, 1);
		udelay(300);	/* 300us */

		/* 3. set pll			*/
		/* mclk on
		udelay(5); /* 76 cycle */
	} else {
		/* 1. set pll			*/
		/* mclk off				*/
		udelay(10); /* 128 cycle */

		/* 2. set reset - XSHUTDOWN low*/
		imx208_sensor_reset(sd, 0);
		udelay(1); /* 500ns */

		/* 3. set power down */
		imx208_sensor_power(sd, 0);

		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_disable(CSI_CH2);
		}
	}

	return result;
}

static int imx208_init(struct v4l2_subdev *sd, unsigned int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = NULL;
	struct imx208_state *state = to_state(sd);

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

	css_sensor("imx208_init() S \r\n");

	if (!client) {
		css_err("imx208_init() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}

	pdata = client->dev.platform_data;
#ifdef TEST_INIT_VALUE
	if (!pdata->sensor_init.value) {
		pdata->sensor_init.value = imx208_reg;
		pdata->sensor_init.init_size = sizeof(imx208_init_reg) / 3;
		/* (sizeof(imx208_reg) / 3) - pdata->sensor_init.init_size; */
		pdata->sensor_init.mode_size = sizeof(imx208_mode_reg) / 3;
		test_mode = 1; /* not mmap */
	}
#endif

	if (val == CSS_SENSOR_INIT) {
		css_info("imx208 init : %d \n",pdata->sensor_init.init_size);

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

				result = imx208_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
					" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
					client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx208_i2c_write(client, reg_data, 3);
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
			css_err("imx208 init reg is NULL\n");
			return CSS_ERROR_SENSOR_INIT_FAIL;
		}
	} else if (val == CSS_SENSOR_MODE_CHANGE) {
		struct imx208_state *state = to_state(sd);

		css_info("imx208 mode change %d\n", pdata->sensor_init.mode_size);
		if (state) {
			if (state->pdata.current_width == 968 &&
				state->pdata.current_height == 548) {
				odin_mipicsi_set_speed(CSI_CH2, 200);
				css_info("chg mipi speed: 200\n");
			}
		}

		if (state->sensor_stream_on == 1) {
			imx208_stream_on_off(sd, 0);
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

				result = imx208_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
					" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
					client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx208_i2c_write(client, reg_data, 3);
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
			css_err("imx208 mode change reg is NULL\n");
			return CSS_ERROR_SENSOR_MODE_CHANGE_FAIL;
		}

		state->sensor_stream_on = 1;

		if (pdata->is_mipi) {
			/* only for sony IMX208
			 * from V1 verification code.
			 * D-phy reset sequence must be checked in Gate-Clock Mode
			 * need to verify that below codes are available in Continuous Mode
			 */
			odin_mipicsi_d_phy_reset_on(CSI_CH2);
			mdelay(1);
			odin_mipicsi_d_phy_reset_off(CSI_CH2);
		}
	} else if (val == CSS_SENSOR_DISABLE) {
		css_info("imx208 Disable\n");
		if (state->sensor_stream_on == 1) {
			imx208_stream_on_off(sd, 0);
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

static int imx208_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	/*
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	*/
	struct imx208_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	/*
	if (width == 640)
		i2c_write();
	*/

	return CSS_SUCCESS;
}

static int imx208_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx208_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;
	/*
	mf->code	= state->fmt->code;
	mf->colorspace	= state->fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
	*/

	return CSS_SUCCESS;
}

static const struct v4l2_subdev_core_ops imx208_core_ops = {
	.init = imx208_init,
	.reset = imx208_sensor_reset,
	.s_power = imx208_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops imx208_video_ops = {
	.s_mbus_fmt = imx208_s_fmt,
	.g_mbus_fmt = imx208_g_fmt,
};

static const struct v4l2_subdev_ops imx208_ops = {
	.core = &imx208_core_ops,
	.video = &imx208_video_ops,
};

static int imx208_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx208_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = NULL;
	struct css_sensor_power *power = NULL;

	css_info("imx208_probe\n");

	if (!client) {
		css_err("imx208_probe() : Do not ready to use I2C \r\n");
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

	if(lge_get_board_revno() <= HW_REV_B) {
		power->cam_1_8v_io = regulator_get(NULL, "+1.8V_VDD_CSS_MAIN_CAM_IO");
		if (IS_ERR(power->cam_1_8v_io)) {/*LDO25*/
			css_err("cam_1_8v_io regulator_get is failed %d\n",
				PTR_ERR(power->cam_1_8v_io));
			power->cam_1_8v_io = NULL;
		}
	}

	state = kzalloc(sizeof(struct imx208_state), GFP_KERNEL);
	if (state == NULL) {
		css_err("state alloc fail\n");
		return -ENOMEM;
	}
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, IMX208_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &imx208_ops);

	return CSS_SUCCESS;
}

static int imx208_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx208_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	if (power->vtcam_1_2v_avdd)
		regulator_put(power->vtcam_1_2v_avdd);
	if (power->vtcam_2_8v_avdd)
		regulator_put(power->vtcam_2_8v_avdd);
	if (power->vtcam_1_8v_io)
		regulator_put(power->vtcam_1_8v_io);

	if(lge_get_board_revno() <= HW_REV_B) {
		if (power->cam_1_8v_io)
			regulator_put(power->cam_1_8v_io);
	}

	v4l2_device_unregister_subdev(sd);
	kfree(state);
	state = NULL;

	return CSS_SUCCESS;
}

static const struct i2c_device_id imx208_id[] = {
	{IMX208_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx208_id);

static struct i2c_driver imx208_i2c_driver = {
	.driver = {
		.name = IMX208_DRIVER_NAME,
	},
	.probe = imx208_probe,
	.remove = imx208_remove,
	.id_table = imx208_id,
};

static int __init imx208_mod_init(void)
{
	return i2c_add_driver(&imx208_i2c_driver);
}

static void __exit imx208_mod_exit(void)
{
	i2c_del_driver(&imx208_i2c_driver);
}

late_initcall(imx208_mod_init);
module_exit(imx208_mod_exit);

