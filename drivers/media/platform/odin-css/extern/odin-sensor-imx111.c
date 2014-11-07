
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
#include "odin-sensor-imx111.h"

#define IMX111_DRIVER_NAME "IMX111"

int imx111_i2c_write(const struct i2c_client *client, const char *buf, int count)
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


static inline struct imx111_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx111_state, sd);
}

static int imx111_sensor_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;

	printk("imx111_sensor_power : %d \n", on);

	/* power gpio enable */

	if (on == 1) {
		/* power on */
		/* gpio setting */
	} else {
		/* power off */
		/* gpio setting */
	}
	return 0;
}

int imx111_sensor_reset(struct v4l2_subdev *sd, int active)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;

	printk("imx111_sensor_reset : %d \n", active);

	/* Set enable Sensor's reset GPIO */
	if (active) {

	} else {

	}

	return 0;
}

static int imx111_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	int result = CSS_SUCCESS;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;

	printk("imx111_sensor_power_sequence\n");

	if (on) {
		if (pdata->is_mipi) {
			odin_mipicsi_channel_rx_enable(CSI_CH0,2,400,1,1,2048, 0);
		}
		/* 1. set power up*/
		imx111_sensor_power(sd, 1);

		udelay(1); /*500ns*/

		/* 2. set reset - XSHUTDOWN high*/
		imx111_sensor_reset(sd, 1);
		udelay(300); /*300us*/

		/* 3. set pll */
		/*mclk on*/
		udelay(5); /*76 cycle*/
	} else {
		/* 1. set pll */
		/*mclk off*/
		udelay(10); /*128 cycle*/

		/* 2. set reset - XSHUTDOWN low*/
		imx111_sensor_reset(sd, 0);
		udelay(1); /*500ns*/

		/* 3. set power down */
		imx111_sensor_power(sd, 0);
	}

	return 0;
}

static int imx111_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	struct imx111_state *state = to_state(sd);

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

	printk("imx111_init() S \r\n");

	if (!client) {
		printk("imx111_init() : Do not ready to use I2C \r\n");
	}
	printk("sensor_reg_size() : %d \n",pdata->sensor_init.size);

#ifdef TEST_INIT_VALUE
#if 0
	if (!pdata->init_value) {
		pdata->init_value = imx111_reg;
	}

	if (pdata->init_value) {
		table = pdata->init_value;
		reg_table = pdata->init_value;
#else
	if (!pdata->sensor_init.value) {
			pdata->sensor_init.value = imx111_reg;
			pdata->sensor_init.size = sizeof(imx111_reg) /3;
			test_mode = 1; //not mmap
		}

	if (pdata->sensor_init.value) {
		table = pdata->sensor_init.value;
		reg_table = pdata->sensor_init.value;
#endif
#endif

		for (i = 0; i < pdata->sensor_init.size; i++) {
			/*
			regs[0] = (unsigned char)table->addr;
			regs[1] = (unsigned char)(table->addr >> 8);
			regs[2] = (unsigned char)table->data;
			*/

			regs[0] = *reg_table;
			regs[1] = *(reg_table + 1);
			regs[2] = *(reg_table + 2);

			reg_data = regs;

			result = imx111_i2c_write(client, reg_data, 3);
			printk(" Dev ID = %02x,"\
					" addr1 = %02x , addr2 = %02x data = %02x, ret = %d\n",
					client->addr, regs[0] , regs[1], regs[2], result);


			if (result != 3) {
				result = imx111_i2c_write(client, reg_data, 3);
				printk("Retry => Dev ID = %02x,"\
						" addr = %04x data = %02x, ret = %d\n",
						client->addr, regs[0] << 8 | regs[1], regs[2], result);
			}
			reg_table = reg_table + 3;

			/* table = table + 1; */
		}
	}

#ifdef TEST_INIT_VALUE
	if (test_mode) {
		pdata->sensor_init.value = NULL;
		pdata->sensor_init.size = 0;
	}
#endif

	return 0;

}

static int imx111_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	/*
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	*/
	struct imx111_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	/*
	if (width == 640)
		i2c_write();
	*/

	return 0;
}

static int imx111_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct imx111_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;
	/*
	mf->code	= state->fmt->code;
	mf->colorspace	= state->fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;
	*/

	return 0;
}


static const struct v4l2_subdev_core_ops imx111_core_ops = {
	.init = imx111_init,
/*	.g_ctrl = imx111_g_ctrl, */
/*	.s_ctrl = imx111_s_ctrl, */
	.reset = imx111_sensor_reset,
	.s_power = imx111_sensor_power_sequence,
/*	.g_chip_ident = imx111_SensorDeviceCheck, */
};

static const struct v4l2_subdev_video_ops imx111_video_ops = {
	.s_mbus_fmt = imx111_s_fmt,
	.g_mbus_fmt = imx111_g_fmt,
};

static const struct v4l2_subdev_ops imx111_ops = {
	.core = &imx111_core_ops,
	.video = &imx111_video_ops,
};

static int imx111_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx111_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	printk(">>>>>>>>>>>>imx111_probe<<<<<<<<<<<<<<<<\n");


	state = kzalloc(sizeof(struct imx111_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, IMX111_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &imx111_ops);

	return 0;

}

static int imx111_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id imx111_id[] = {
	{IMX111_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, imx111_id);

static struct i2c_driver imx111_i2c_driver = {
	.driver = {
		.name = IMX111_DRIVER_NAME,
	},
	.probe = imx111_probe,
	.remove = imx111_remove,
	.id_table = imx111_id,
};

static int __init imx111_mod_init(void)
{
	return i2c_add_driver(&imx111_i2c_driver);
}

static void __exit imx111_mod_exit(void)
{
	i2c_del_driver(&imx111_i2c_driver);
}


late_initcall(imx111_mod_init);
module_exit(imx111_mod_exit);

