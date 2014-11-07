
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/delay.h>

#include "../odin-css.h"
#include "../odin-mipicsi.h"
#include "../odin-capture.h"
#include "odin-sensor-imx091.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#define IMX091_DRIVER_NAME "IMX091"
#define STREAM_ON_OFF_ADDR	0x0100

int imx091_i2c_write(const struct i2c_client *client, const char *buf,
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

static inline struct imx091_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx091_state, sd);
}

int imx091_stream_on_off(struct v4l2_subdev *sd, unsigned int on_off)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx091_state *state = to_state(sd);
	int result = CSS_SUCCESS;
	unsigned char reg_data[3] = {0, };

	css_info("imx091_stream_on_off :%d\n", on_off);

	reg_data[0] = STREAM_ON_OFF_ADDR >> 8;
	reg_data[1] = STREAM_ON_OFF_ADDR & 0xff;
	reg_data[2] = on_off;

	result = imx091_i2c_write(client, (unsigned char *)reg_data, 3);
	state->sensor_stream_on = on_off;
}

int imx091_sensor_power(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx091_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	css_info("imx091_sensor_power : %d \n", on);

	if (on == 1) {
		/* power on */
		if (power->cam_2_8v_avdd)
			regulator_enable(power->cam_2_8v_avdd);
		if (power->cam_1_2v_avdd)
			regulator_enable(power->cam_1_2v_avdd);
		if (power->cam_vcm_1_8v_avdd)
			regulator_enable(power->cam_vcm_1_8v_avdd);
	} else {
		/* power off */
		if (power->cam_2_8v_avdd)
			regulator_disable(power->cam_2_8v_avdd);
		if (power->cam_1_2v_avdd)
			regulator_disable(power->cam_1_2v_avdd);
		if (power->cam_vcm_1_8v_avdd)
			regulator_disable(power->cam_vcm_1_8v_avdd);
	}
	return result;
}

int imx091_sensor_reset(struct v4l2_subdev *sd, int active)
{
	struct imx091_state *state = to_state(sd);
	/* state->gpio_base - 8 + state->pdata.reset_gpio.num; */
	unsigned int reset_num = state->pdata.reset_gpio.num;

	css_info("imx091_sensor_reset : %d, %d\n", active, reset_num);

	if (gpio_request(reset_num, "imx091_reset") < 0) {
		css_err("gpio_request fail : imx091_reset\n");
		return -1;
	}
	gpio_direction_output(reset_num, 1);

	/* Set enable Sensor's reset GPIO */
	if (active) {
		gpio_set_value(reset_num, 1);
	} else {
		gpio_set_value(reset_num, 0);
	}

	gpio_free(reset_num);
	return 0;
}

static int imx091_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx091_state *state = to_state(sd);

	css_sensor("imx091_sensor_power_sequence\n");

	if (!state) {
		css_err("imx091 state is NULL\n");
		return CSS_ERROR_INVALID_SENSOR_STATE;
	}

	if (on) {
		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_enable(CSI_CH0,4,250,1,1,2048,0);
		}
		/* 0. set reset - low*/
		imx091_sensor_reset(sd, 0);

		/* 1. set power up*/
		imx091_sensor_power(sd, 1);

		udelay(1); /* 500ns */

		/* 2. set reset - XSHUTDOWN high*/
		imx091_sensor_reset(sd, 1);
		udelay(300); //300us

		/* 3. set pll */
		/* mclk on */
		udelay(5); /* 76 cycle */
	} else {
		/* 1. set pll */
		/* mclk off */
		udelay(10); /* 128 cycle */

		/* 2. set reset - XSHUTDOWN low*/
		imx091_sensor_reset(sd, 0);
		udelay(1); /* 500ns */

		/* 3. set power down */
		imx091_sensor_power(sd, 0);

		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_disable(CSI_CH0);
		}
	}

	return result;
}

static int imx091_init(struct v4l2_subdev *sd, unsigned int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	struct imx091_state *state = to_state(sd);

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

	css_sensor("imx091_init() S \r\n");

	if (!client) {
		css_err("imx091_init() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}

#ifdef TEST_INIT_VALUE
	if (!pdata->sensor_init.value) {
		pdata->sensor_init.value = imx091_reg;
		pdata->sensor_init.init_size = sizeof(imx091_reg) / 3;
		pdata->sensor_init.mode_size = (sizeof(imx091_reg) / 3)
									- pdata->sensor_init.init_size;
		test_mode = 1; /* not mmap */
	}
#endif

	if (val == CSS_SENSOR_INIT)
	{
		css_info("imx091 init : %d \n",pdata->sensor_init.init_size);

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

				result = imx091_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x, addr1 = %02x"\
							" , addr2 = %02x data = %02x, ret = %d\n",
							client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx091_i2c_write(client, reg_data, 3);
					css_err("Retry => Dev ID = %02x,"\
						" addr = %04x data = %02x, ret = %d\n",
						client->addr, regs[0] << 8 | regs[1], regs[2], result);
					if (result != 3) {
						css_err("Retry Fail: Dev ID = %02x,"\
								" addr = %04x data = %02x, ret = %d\n",
								client->addr, regs[0] << 8 | regs[1],
								regs[2], result);
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
			css_err("imx091 init reg is NULL\n");
			return CSS_ERROR_SENSOR_INIT_FAIL;
		}
	} else if (val == CSS_SENSOR_MODE_CHANGE) {
		css_info("imx091 mode change %d\n", pdata->sensor_init.mode_size);

		if (state->sensor_stream_on == 1) {
			imx091_stream_on_off(sd, 0);
		}

		/* write mode change value */
		if (pdata->sensor_init.value) {
#ifdef TEST_INIT_VALUE
			reg_table = pdata->sensor_init.value
						+ pdata->sensor_init.init_size * 3;
#else
			table = pdata->sensor_init.value
				+ pdata->sensor_init.init_size*sizeof(struct css_setting_table);
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

				result = imx091_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x, addr1 = %02x"\
							" , addr2 = %02x data = %02x, ret = %d\n",
							client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx091_i2c_write(client, reg_data, 3);
					css_err("Retry => Dev ID = %02x,"\
							" addr = %04x data = %02x, ret = %d\n",
								client->addr, regs[0] << 8 | regs[1],
								regs[2], result);
					if (result != 3) {
						css_err("Retry Fail: Dev ID = %02x,"\
							" addr = %04x data = %02x, ret = %d\n",
								client->addr, regs[0] << 8 | regs[1],
								regs[2], result);
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
			css_err("imx091 mode change reg is NULL\n");
			return CSS_ERROR_SENSOR_MODE_CHANGE_FAIL;
		}

		state->sensor_stream_on = 1;

		if (pdata->is_mipi) {
			/* only for sony IMX091
			 * from V1 verification code.
			 * D-phy reset sequence must be checked in Gate-Clock Mode
			 * need to verify that below codes are available in Continuous Mode
			*/
			odin_mipicsi_d_phy_reset_on(CSI_CH0);
			mdelay(1);
			odin_mipicsi_d_phy_reset_off(CSI_CH0);
		}
	} else if (val == CSS_SENSOR_DISABLE) {
		css_info("imx091 Disable\n");

		if (state->sensor_stream_on == 1) {
			imx091_stream_on_off(sd, 0);
		}
	}

#ifdef TEST_INIT_VALUE
	if (test_mode) {
		pdata->sensor_init.value = NULL;
		pdata->sensor_init.init_size = 0;
	}
#endif
	return 0;
}

static int imx091_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	/*
	 * struct i2c_client *client = v4l2_get_subdevdata(sd);
	 * struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	 */
	struct imx091_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	/*
	if (width == 640)
		i2c_write();
	*/

	return 0;
}

static int imx091_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx091_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;
	/*
	mf->code	= state->fmt->code;
	mf->colorspace	= state->fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
	*/

	return 0;
}

static const struct v4l2_subdev_core_ops imx091_core_ops = {
	.init = imx091_init,
	.reset = imx091_sensor_reset,
	.s_power = imx091_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops imx091_video_ops = {
	.s_mbus_fmt = imx091_s_fmt,
	.g_mbus_fmt = imx091_g_fmt,
};

static const struct v4l2_subdev_ops imx091_ops = {
	.core = &imx091_core_ops,
	.video = &imx091_video_ops,
};

static int imx091_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx091_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	struct css_sensor_power *power = &pdata->power;

#if 0 /* CONFIG_OF */
	struct device_node *np;
#endif

	css_info("imx091_probe\n");

	if (!client) {
		css_err("imx091_probe() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}

	power->cam_1_2v_avdd = regulator_get(NULL, "+1.05V_CAM_DVDD"); /*LDO4*/
	if (IS_ERR(power->cam_1_2v_avdd)) {
		css_err("cam_1_2v_avdd regulator_get is failed %d\n",
			PTR_ERR(power->cam_1_2v_avdd));
		power->cam_1_2v_avdd = NULL;
	}

	power->cam_vcm_1_8v_avdd = regulator_get(NULL, "+1.8V_13M_VCM"); /*LDO18*/
	if (IS_ERR(power->cam_vcm_1_8v_avdd)) {
		css_err("cam_vcm_1_8v_avdd regulator_get is failed %d\n",
			PTR_ERR(power->cam_vcm_1_8v_avdd));
		power->cam_vcm_1_8v_avdd = NULL;
	}

	power->cam_2_8v_avdd = regulator_get(NULL, "+2.7V_13M_ANA"); /*LDO23*/
	if (IS_ERR(power->cam_2_8v_avdd)) {
		css_err("cam_2_8v_avdd regulator_get is failed %d\n",
			PTR_ERR(power->cam_2_8v_avdd));
		power->cam_2_8v_avdd = NULL;
	}

	state = kzalloc(sizeof(struct imx091_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, IMX091_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &imx091_ops);


#if 0 /* CONFIG_OF */
	np = of_find_compatible_node(NULL, NULL, "LG,odin-css-gpio1");
	if (!np) {
		printk("	odin-css-gpio1 error \n");
	} else {
		result = of_property_read_u32(np, "bank_num", &state->gpio_base);
		if (result < 0)
			printk(" np of_property_read_u32 error \n");

		printk("	css_base = %d \n",state->gpio_base);
	}
#endif

#if 0
	if (client->dev.of_node) {
		result = of_property_read_u32(client->dev.of_node, "bank_num",
									&gpio_base);
		if (result < 0)
			printk("of_property_read_u32 error \n");

		printk("	css_base = %d\n",gpio_base);
	}
#endif

	return 0;

}

static int imx091_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx091_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	if (power->cam_vcm_1_8v_avdd)
		regulator_put(power->cam_vcm_1_8v_avdd);

	if (power->cam_1_2v_avdd)
		regulator_put(power->cam_1_2v_avdd);

	if (power->cam_2_8v_avdd)
		regulator_put(power->cam_2_8v_avdd);

	v4l2_device_unregister_subdev(sd);
	kfree(state);
	state = NULL;

	return 0;
}

static const struct i2c_device_id imx091_id[] = {
	{IMX091_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx091_id);

static struct i2c_driver imx091_i2c_driver = {
	.driver = {
		.name = IMX091_DRIVER_NAME,
	},
	.probe = imx091_probe,
	.remove = imx091_remove,
	.id_table = imx091_id,
};

static int __init imx091_mod_init(void)
{
	return i2c_add_driver(&imx091_i2c_driver);
}

static void __exit imx091_mod_exit(void)
{
	i2c_del_driver(&imx091_i2c_driver);
}

late_initcall(imx091_mod_init);
module_exit(imx091_mod_exit);

