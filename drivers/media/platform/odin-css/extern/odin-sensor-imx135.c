
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
#include "ois/odin-ois-i2c.h"
#include "odin-sensor-imx135.h"
#include <linux/gpio.h>
#include <linux/odin_pmic.h>
#include <linux/regulator/consumer.h>
#include <linux/board_lge.h>

#define IMX135_DRIVER_NAME	"IMX135"
#define OIS_MAKER_ID_ADDR	(0x700)
#define OIS_SLAVE_ID		0xA0
#define STREAM_ON_OFF_ADDR	0x0100

extern void lgit_ois_init(struct i2c_client *client,
				struct odin_ois_ctrl *ois_ctrl);
extern void lgit2_ois_init(struct i2c_client *client,
				struct odin_ois_ctrl *ois_ctrl);
extern void fuji_ois_init(struct i2c_client *client,
				struct odin_ois_ctrl *ois_ctrl);

int imx135_i2c_write(const struct i2c_client *client, const char *buf,
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

int imx135_ois_i2c_read(const struct i2c_client *client, const char *buf,
					int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[] = {
		{
			.addr	= OIS_SLAVE_ID, /* client->addr, 0x50<<1 */
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		}, {
			.addr	= OIS_SLAVE_ID, /* client->addr, */
			.flags	= I2C_M_RD,
			.len	= count,
			.buf	= buf,
		},
	};

	if (i2c_transfer(adap, msgs, ARRAY_SIZE(msgs)) != count) {
		css_err("imx135_ois_i2c_read error!!\n");
		return -EREMOTEIO;
	}

	return CSS_SUCCESS;
}


static inline struct imx135_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx135_state, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct imx135_state, hdl)->sd;
}

int imx135_stream_on_off(struct v4l2_subdev *sd, unsigned int on_off)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx135_state *state = to_state(sd);
	int result = CSS_SUCCESS;
	unsigned char reg_data[3] = {0, };

	css_info("imx135_stream_on_off :%d\n", on_off);

	reg_data[0] = STREAM_ON_OFF_ADDR >> 8;
	reg_data[1] = STREAM_ON_OFF_ADDR & 0xff;
	reg_data[2] = on_off;

	result = imx135_i2c_write(client, (unsigned char *)reg_data, 3);
	state->sensor_stream_on = on_off;

	return result;
}

int imx135_ois_reset(struct v4l2_subdev *sd, int active)
{
	struct imx135_state *state = to_state(sd);
	unsigned int ois_reset_num = 0;

	ois_reset_num = state->pdata.ois_reset_gpio.num;

	css_info("imx135_ois_reset : %d ,%d\n", active , ois_reset_num);

	if (gpio_request(ois_reset_num, "imx135_ois_reset") < 0) {
		css_err("gpio_request fail\n");
		return CSS_FAILURE;
	}
	gpio_direction_output(ois_reset_num, 1);

	/* Set enable Sensor's reset GPIO */
	if (active) {
		gpio_set_value(ois_reset_num, 1);
	} else {
		gpio_set_value(ois_reset_num, 0);
	}

	gpio_free(ois_reset_num);

	return CSS_SUCCESS;
}

int imx135_ois_power(struct v4l2_subdev *sd, int active)
{
	struct imx135_state *state = to_state(sd);
	unsigned int ois_power_num = 0;

	ois_power_num = state->pdata.ois_power_gpio.num;

	css_info("imx135_ois_power : %d , %d \n", active, ois_power_num);

	if (gpio_request(ois_power_num, "imx135_ois_power") < 0) {
		css_err("gpio_request fail\n");
		return CSS_FAILURE;
	}
	gpio_direction_output(ois_power_num, 1);

	/* Set enable Sensor's reset GPIO */
	if (active) {
		gpio_set_value(ois_power_num, 1);
	} else {
		gpio_set_value(ois_power_num, 0);
	}

	gpio_free(ois_power_num);

	return CSS_SUCCESS;
}

static int imx135_flash_power(struct v4l2_subdev *sd, int on)
{
	/* struct imx135_state *state = to_state(sd); */
	unsigned int flash_power_num = 79; /* MADK 2.1 */

	css_info("imx135_flash : %d , %d \n", on, flash_power_num);

	if (gpio_request(flash_power_num, "imx135_flash_power") < 0) {
		css_err("gpio_request fail\n");
		return CSS_FAILURE;
	}
	gpio_direction_output(flash_power_num, 1);

	/* Set enable Sensor's reset GPIO */
	if (on)
		gpio_set_value(flash_power_num, 1);
	else
		gpio_set_value(flash_power_num, 0);

	gpio_free(flash_power_num);

	return CSS_SUCCESS;
}

static int imx135_enable_io(struct v4l2_subdev *sd, int active)
{
	struct imx135_state *state = to_state(sd);
	unsigned int gpio = 0;

	gpio = state->pdata.io_enable_gpio.num;

	if(active){
		if (gpio_request(gpio, "imx135_enable_io") < 0) {
			css_err("gpio_request fail\n");
			return CSS_FAILURE;
		}
		int ret = gpio_direction_output(gpio, 1);
		css_err("ret = %d\n", ret);
	} else {
		if(gpio_is_valid(gpio)) {
			gpio_direction_output(gpio, 0);
			gpio_free(gpio);
		}
	}
}

static int imx135_sensor_power(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx135_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;
	int cam_volt = 0;

	css_info("imx135_sensor_power : %d \n", on);

	if (on == 1) {
		/* power on */
#if 1
		if(lge_get_board_revno() <= HW_REV_B) {
			if (power->cam_1_8v_io)
				regulator_enable(power->cam_1_8v_io);
		} else {
			imx135_enable_io(sd, on);
		}
		if (power->cam_1_2v_avdd) {
			regulator_enable(power->cam_1_2v_avdd);
			result = regulator_set_voltage(power->cam_1_2v_avdd,
											1050000, 1050000);
			cam_volt = regulator_get_voltage(power->cam_1_2v_avdd);
			if (result < 0)
				css_err("regulator_set_voltage error, %d \n",result);

			if (state->pdata.is_ois)
				imx135_ois_power(sd, 1);
			/* mdelay(1); */
		}
		if (power->cam_2_8v_avdd)
			regulator_enable(power->cam_2_8v_avdd);
		if (power->cam_vcm_1_8v_avdd)
			regulator_enable(power->cam_vcm_1_8v_avdd);
#else
		result = odin_pmic_source_enable(_2V8_CAM_AVDD);
		result = odin_pmic_source_enable(_1V2_CAM_DVDD);
		if (result < 0)	{
			printk("odin_pmic_source_enable error \n");
		} else {
			result = odin_pmic_source_set(_1V2_CAM_DVDD,1080);
			cam_volt = odin_pmic_source_get(_1V2_CAM_DVDD);
		}
		if (state->pdata.is_ois)
			imx135_ois_power(sd, 1);
		mdelay(1);
		result = odin_pmic_source_enable(_1V8_CAM_VDDIO);
#endif

		css_info("imx135 _1V2_CAM_DVDD : %d \n", cam_volt);

	} else {
		/* power off */
#if 0
		result = odin_pmic_source_disable(_2V8_CAM_AVDD);
		result = odin_pmic_source_disable(_1V2_CAM_DVDD);
		result = odin_pmic_source_disable(_1V8_CAM_VDDIO);
#else
		if (power->cam_1_2v_avdd)
			regulator_disable(power->cam_1_2v_avdd);

		if(lge_get_board_revno() <= HW_REV_B) {
			if (power->cam_1_8v_io)
				regulator_disable(power->cam_1_8v_io);
		} else {
			imx135_enable_io(sd, on);
		}

		if (power->cam_2_8v_avdd)
			regulator_disable(power->cam_2_8v_avdd);
		if (power->cam_vcm_1_8v_avdd)
			regulator_disable(power->cam_vcm_1_8v_avdd);
#endif
		if (state->pdata.is_ois)
			imx135_ois_power(sd, 0);

	}
	return result;
}

int imx135_sensor_reset(struct v4l2_subdev *sd, int active)
{
	struct imx135_state *state = to_state(sd);
	unsigned int reset_num = state->pdata.reset_gpio.num;

	css_info("imx135_sensor_reset : %d , %d\n", active, reset_num);

	if (gpio_request(reset_num, "imx135_reset") < 0) {
		css_err("gpio_request fail : imx135_reset\n");
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

static int imx135_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct imx135_state *state = to_state(sd);

	css_sensor("imx135_sensor_power_sequence\n");

	if (!state) {
		css_err("imx135 state is NULL\n");
		return CSS_ERROR_INVALID_SENSOR_STATE;
	}

	if (on) {
		if (state->pdata.is_mipi)
			odin_mipicsi_channel_rx_enable(CSI_CH0,4,250,1,1,2048,0);

		/* 0. set reset - low*/
		imx135_sensor_reset(sd, 0);
		if (state->pdata.is_ois)
			imx135_ois_reset(sd, 0);

		/* 1. set power up*/
		imx135_sensor_power(sd, 1);

		mdelay(2);

		/* 2. set pll */
		/* mclk on */

		/* 3. set reset - XSHUTDOWN high*/
		if (state->pdata.is_ois)
			imx135_ois_reset(sd, 1);
		imx135_sensor_reset(sd, 1);

		mdelay(2);

	} else {
		/* 1. set pll */
		/* mclk off */

		/* 2. set reset - XSHUTDOWN low*/
		imx135_sensor_reset(sd, 0);
		if (state->pdata.is_ois)
			imx135_ois_reset(sd, 0);

		/* 3. set power down */
		imx135_sensor_power(sd, 0);

		if (state->pdata.is_mipi) {
			odin_mipicsi_channel_rx_disable(CSI_CH0);
		}
	}

	return result;
}

static int imx135_ois_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	struct imx135_state *state = to_state(sd);
	unsigned char chipid[2] = {0, };
	int ret = 0;
	int retry = 0;

	chipid[0] = OIS_MAKER_ID_ADDR >> 8;
	chipid[1] = OIS_MAKER_ID_ADDR & 0xff;

	while (1) {
		ret = imx135_ois_i2c_read(client,(unsigned char *)chipid, 1);
		if (ret == 0) {
			/* printk(" 0x700 read %d\n", chipid[0]); */
			switch (chipid[0]) {
			case 0x01:
				lgit_ois_init(client,&state->ois);
				css_info("%s : LGIT OIS module type #1!\n", __func__);
				break;
			case 0x02:
			case 0x05:
				lgit2_ois_init(client,&state->ois);
				css_info("%s : LGIT OIS module type #2!\n", __func__);
				break;
			case 0x03:
				fuji_ois_init(client,&state->ois);
				css_info("%s : FujiFilm OIS module!\n", __func__);
				break;
			default:
				css_err("%s : unknown module! maker id = %d\n", __func__,
						chipid);
				state->ois.ois_func_tbl = NULL;
				return OIS_INIT_NOT_SUPPORTED;
			}
			break;

		}
		if (ret < 0 && retry > 3) {
			css_err(" imx135_ois_i2c_read error %d\n", ret);
			state->ois.ois_func_tbl = NULL;
			return OIS_INIT_NOT_SUPPORTED;
		}
		retry++;
	}
	state->ois_mode = OIS_MODE_CENTERING_ONLY;
	state->ois_version = OIS_VER_RELEASE;

	return OIS_SUCCESS;

}

static int imx135_init(struct v4l2_subdev *sd, unsigned int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = NULL;
	struct imx135_state *state = to_state(sd);

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

	css_sensor("imx135_init() S \r\n");

	if (!client) {
		css_err("imx135_init() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}

	pdata = client->dev.platform_data;
#ifdef TEST_INIT_VALUE
	if (!pdata->sensor_init.value) {
		pdata->sensor_init.value = imx135_reg; /* imx135_reg_fhd_60; */
		pdata->sensor_init.init_size = sizeof(imx135_init_reg) / 3;
		pdata->sensor_init.mode_size = sizeof(imx135_mode_reg) / 3;
		test_mode = 1; /* not mmap */
	}
#endif

	if (val == CSS_SENSOR_INIT) {
		css_info("imx135 init : %d \n",pdata->sensor_init.init_size);

		if (pdata->is_ois) {
			result = imx135_ois_init(sd);
			/*
			if (result != OIS_SUCCESS)
				pdata->is_ois = 0;
			*/
		}
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

				result = imx135_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
					" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
					client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx135_i2c_write(client, reg_data, 3);
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
			css_err("imx135 init reg is NULL\n");
			return CSS_ERROR_SENSOR_INIT_FAIL;
		}
	} else if (val == CSS_SENSOR_MODE_CHANGE) {
		css_info("imx135 mode change %d\n",pdata->sensor_init.mode_size);

		if (state->sensor_stream_on == 1) {
			if (pdata->is_ois) {
				if (state->ois.ois_func_tbl)
					state->ois.ois_func_tbl->ois_mode(OIS_MODE_CENTERING_ONLY);
			}
			imx135_stream_on_off(sd, 0);
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

				result = imx135_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
						" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
						client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx135_i2c_write(client, reg_data, 3);
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
			css_err("imx135 mode change reg is NULL\n");
			return CSS_ERROR_SENSOR_MODE_CHANGE_FAIL;
		}

		state->sensor_stream_on = 1;
		/*
		if (pdata->is_ois) {
			if (state->ois.ois_func_tbl)
				state->ois.ois_func_tbl->ois_mode(state->ois_mode);

		}
		*/
		if (pdata->is_mipi) {
			/* only for sony IMX135
			 * from V1 verification code.
			 * D-phy reset sequence must be checked in Gate-Clock Mode
			 * need to verify that below codes are available in Continuous Mode
			 */
			odin_mipicsi_d_phy_reset_on(CSI_CH0);
			mdelay(1);
			odin_mipicsi_d_phy_reset_off(CSI_CH0);
		}
		/*
		if (pdata->is_ois) {
			if (state->ois.ois_func_tbl)
				state->ois.ois_func_tbl->ois_stat(&state->ois.ois_info);
		}
		*/
	} else if (val == CSS_SENSOR_MODE_CHANGE_FOR_AAT) {
		css_info("imx135 mode change for aat\n");
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

				result = imx135_i2c_write(client, reg_data, 3);
				css_sensor(" Dev ID = %02x,"\
						" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
						client->addr, regs[0] , regs[1], regs[2], result);

				if (result != 3) {
					result = imx135_i2c_write(client, reg_data, 3);
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
			css_err("imx135 mode change reg is NULL\n");
			return CSS_ERROR_SENSOR_MODE_CHANGE_FAIL;
		}

		state->sensor_stream_on = 1;

		if (pdata->is_mipi) {
			/* only for sony IMX135
			 * from V1 verification code.
			 * D-phy reset sequence must be checked in Gate-Clock Mode
			 * need to verify that below codes are available in Continuous Mode
			 */
			odin_mipicsi_d_phy_reset_on(CSI_CH0);
			mdelay(1);
			odin_mipicsi_d_phy_reset_off(CSI_CH0);
		}
	} else if (val == CSS_SENSOR_DISABLE) {
		css_info("imx135 disable\n");
		if (pdata->is_ois) {
			if (state->ois.ois_func_tbl) {
				state->ois.ois_func_tbl->ois_mode(OIS_MODE_CENTERING_ONLY);
				state->ois.ois_func_tbl->ois_off();
				state->ois.ois_func_tbl = NULL;
			}
		}

		if (state->sensor_stream_on == 1) {
			imx135_stream_on_off(sd, 0);
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

static int imx135_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx135_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	/*
	if (width == 640)
		i2c_write();
	*/

	return CSS_SUCCESS;
}

static int imx135_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx135_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;
	/*
	mf->code	= state->fmt->code;
	mf->colorspace	= state->fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
	*/

	return CSS_SUCCESS;
}

static int imx135_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct imx135_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	int result = CSS_SUCCESS;
	int x, y;

	switch (ctrl->id) {
	case V4L2_CID_CSS_SENSOR_GET_OIS_STATUS:
		css_sensor("get ois status\n");
		if (state->pdata.is_ois) {
			if (state->ois.ois_func_tbl)
				state->ois.ois_func_tbl->ois_stat(&state->ois.ois_info);
			memcpy(&pdata->ois_info.ois_status, &state->ois.ois_info,
					sizeof(struct odin_ois_info));
		}
		break;
	case V4L2_CID_CSS_SENSOR_MOVE_OIS_LENS:
		if (state->pdata.is_ois) {
			state->pdata.ois_info.lens.x = pdata->ois_info.lens.x;
			state->pdata.ois_info.lens.y = pdata->ois_info.lens.y;
			x = state->pdata.ois_info.lens.x;
			y = state->pdata.ois_info.lens.y;
			if (state->ois.ois_func_tbl)
			result = state->ois.ois_func_tbl->ois_move_lens(x, y);
			css_info("%s move lens result %d\n", __func__,result);
		}
		break;
	default:
		css_err("imx135_g_ctrl : Unknown IOCTL\n");
		result = -EINVAL;
		break;
	}

	return result;
}

static int imx135_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct imx135_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;

	int result = CSS_SUCCESS;
	int x, y;

	css_sensor("imx135_s_ctrl() S %d\n",ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_CSS_SENSOR_SET_OIS_VERSION:
		/* AAT + debugging : set version -> mode */
		css_info("set ois version %d\n", ctrl->val);
		if (state->pdata.is_ois) {
			state->ois_version = ctrl->val;
		}
		break;
	case V4L2_CID_CSS_SENSOR_OIS_ON:
		css_info("set ois on ver : %d\n", ctrl->val);
		if (state->pdata.is_ois) {
			state->ois_version = ctrl->val;
			if (state->ois.ois_func_tbl)
				result = state->ois.ois_func_tbl->ois_on(state->ois_version);
		}
		break;
	case V4L2_CID_CSS_SENSOR_SET_OIS_MODE:
		css_info("set ois mode %d\n", ctrl->val);
		if (state->pdata.is_ois) {
			state->ois_mode = ctrl->val;
			/* state->ois_version = OIS_VER_RELEASE; */
			if (state->ois.ois_func_tbl)
				state->ois.ois_func_tbl->ois_mode(ctrl->val);
		}
		break;
	case V4L2_CID_CSS_SENSOR_MOVE_OIS_LENS:
		css_info("move ois lens %d\n", ctrl->val);
		if (state->pdata.is_ois) {
			if (ctrl->val == OIS_MODE_LENS_MOVE_START) {
				state->pdata.ois_info.lens.x = pdata->ois_info.lens.x;
				state->pdata.ois_info.lens.y = pdata->ois_info.lens.y;
				x = state->pdata.ois_info.lens.x;
				y = state->pdata.ois_info.lens.y;
				if (state->ois.ois_func_tbl)
					state->ois.ois_func_tbl->ois_move_lens(x,y);
			}
			/*
			else if (ctrl->val == OIS_MODE_LENS_MOVE_SET) {
				state->pdata.ois_info.lens.x = pdata->ois_info.lens.x;
				state->pdata.ois_info.lens.y = pdata->ois_info.lens.y;
			}
			*/
		}
		break;
	case V4L2_CID_CSS_SENSOR_STROBE:
		css_info("flash on %d\n", ctrl->val);
		if (ctrl->val == STROBE_ON)
			imx135_flash_power(sd, 1);
		else
			imx135_flash_power(sd, 0);
		break;
	case V4L2_CID_CSS_SENSOR_TORCH:
		css_info("torch on%d\n", ctrl->val);
		/*
		if (ctrl->val == TORCH_OFF)
		else
		*/
		break;
	default:
		css_err("imx135_s_ctrl : Unknown IOCTL\n");
		result = -EINVAL;
		break;
	}

	return result;
}

static const struct v4l2_ctrl_ops imx135_ctrl_ops = {
	.s_ctrl = imx135_s_ctrl,
	.g_volatile_ctrl = imx135_g_ctrl,
};

static const struct v4l2_subdev_core_ops imx135_core_ops = {
	.init = imx135_init,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.reset = imx135_sensor_reset,
	.s_power = imx135_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops imx135_video_ops = {
	.s_mbus_fmt = imx135_s_fmt,
	.g_mbus_fmt = imx135_g_fmt,
};

static const struct v4l2_subdev_ops imx135_ops = {
	.core = &imx135_core_ops,
	.video = &imx135_video_ops,
};

static const struct v4l2_ctrl_config imx135_ctrls[] = {
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_STROBE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Set STROBE",
		.min		= STROBE_DEFAULT,
		.max		= STROBE_ON,
		.step		= 1,
		.def		= STROBE_DEFAULT,
		.flags		= 0,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_TORCH,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Set TORCH",
		.min		= TORCH_OFF,
		.max		= TORCH_DEFAULT,
		.step		= 1,
		.def		= TORCH_DEFAULT,
		.flags		= 0,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_SET_OIS_VERSION,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Set OIS Version",
		.min		= OIS_VER_RELEASE,
		.max		= OIS_VER_DEFAULT,
		.step		= 1,
		.def		= OIS_VER_DEFAULT,
		.flags		= 0,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_OIS_ON,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "OIS on",
		.min		= OIS_VER_RELEASE,
		.max		= OIS_VER_DEFAULT,
		.step		= 1,
		.def		= OIS_VER_DEFAULT,
		.flags		= 0,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_SET_OIS_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Set OIS Mode",
		.min		= OIS_MODE_PREVIEW_CAPTURE,
		.max		= OIS_MODE_END,
		.step		= 1,
		.def		= OIS_MODE_END,
		.flags		= 0,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_MOVE_OIS_LENS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Move OIS lens",
		.min		= OIS_MODE_LENS_MOVE_START,
		.max		= OIS_MODE_LENS_MOVE_END,
		.step		= 1,
		.def		= OIS_MODE_LENS_MOVE_END,
		.flags		= V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops		= &imx135_ctrl_ops,
		.id			= V4L2_CID_CSS_SENSOR_GET_OIS_STATUS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Get OIS status",
		.min		= 0,
		.max		= 100,
		.step		= 1,
		.def		= 100,
		.flags		= V4L2_CTRL_FLAG_VOLATILE,
	}
};

static int imx135_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx135_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = NULL;
	struct css_sensor_power *power = NULL;
	int i;

	css_info("imx135_probe\n");

	if (!client) {
		css_err("imx135_probe() : Do not ready to use I2C \r\n");
		return CSS_ERROR_SENSOR_I2C_DOES_NOT_READY;
	}
	pdata = client->dev.platform_data;
	power = &pdata->power;

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

	if(lge_get_board_revno() <= HW_REV_B) {
		power->cam_1_8v_io = regulator_get(NULL, "+1.8V_VDD_CSS_MAIN_CAM_IO"); /*LDO25*/
		if (IS_ERR(power->cam_1_8v_io)) {
			css_err("cam_1_8v_io regulator_get is failed %d\n",
				PTR_ERR(power->cam_1_8v_io));
			power->cam_1_8v_io = NULL;
		}
	} else {
		power->cam_1_8v_io = NULL;
	}

	state = kzalloc(sizeof(struct imx135_state), GFP_KERNEL);
	if (state == NULL) {
		css_err("state alloc fail\n");
		return -ENOMEM;
	}
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, IMX135_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &imx135_ops);

	/* init_controls*/
	v4l2_ctrl_handler_init(&state->hdl, ARRAY_SIZE(imx135_ctrls)/*1*/);

	for (i = 0; i < ARRAY_SIZE(imx135_ctrls); ++i)
		v4l2_ctrl_new_custom(&state->hdl, &imx135_ctrls[i], NULL);

	subdev->ctrl_handler = &state->hdl;
	if (state->hdl.error) {
		css_err("v4l2_ctrl_handler_init is failed %d\n",state->hdl.error);
		goto err_hdl;
	}

	/* v4l2_ctrl_handler_setup(&state->hdl); */
	css_sensor("imx135_probe v4l2_ctrl_handler_init\n");

	return CSS_SUCCESS;

err_hdl:
	v4l2_ctrl_handler_free(&state->hdl);
	kfree(state);
	state = NULL;
	return CSS_FAILURE;
}

static int imx135_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx135_state *state = to_state(sd);
	struct css_sensor_power *power = &state->pdata.power;

	if (power->cam_vcm_1_8v_avdd)
		regulator_put(power->cam_vcm_1_8v_avdd);
	if (power->cam_1_2v_avdd)
		regulator_put(power->cam_1_2v_avdd);
	if (power->cam_2_8v_avdd)
		regulator_put(power->cam_2_8v_avdd);
	if (power->cam_1_8v_io)
		regulator_put(power->cam_1_8v_io);

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&state->hdl);
	kfree(state);
	state = NULL;

	return CSS_SUCCESS;
}

static const struct i2c_device_id imx135_id[] = {
	{IMX135_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx135_id);

static struct i2c_driver imx135_i2c_driver = {
	.driver = {
		.name = IMX135_DRIVER_NAME,
	},
	.probe = imx135_probe,
	.remove = imx135_remove,
	.id_table = imx135_id,
};

static int __init imx135_mod_init(void)
{
	return i2c_add_driver(&imx135_i2c_driver);
}

static void __exit imx135_mod_exit(void)
{
	i2c_del_driver(&imx135_i2c_driver);
}

late_initcall(imx135_mod_init);
module_exit(imx135_mod_exit);
