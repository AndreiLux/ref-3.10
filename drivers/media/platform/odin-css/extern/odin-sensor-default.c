#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include "../odin-css.h"
#include "../odin-mipicsi.h"
#include "../odin-capture.h"
#include "odin-sensor-default.h"

#define DEFAULT_DRIVER_NAME "odin_default"


static inline struct default_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct default_state, sd);
}

/*1. i2c_data : 0x12345678*/
static int default_i2c_write(struct v4l2_subdev *sd, unsigned int i2c_data[],
				unsigned int length)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int i;
	int reg_count = 4; /* sensor dependency */
	unsigned char buf[reg_count];

	for (i = 0; i < length; i++) {
		buf[0] = i2c_data[i] >> 24;
		buf[1] = i2c_data[i] >> 16;
		buf[2] = i2c_data[i] >> 8;
		buf[3] = i2c_data[i] & 0xff;

		if (i2c_master_send(p_client, buf, reg_count) != reg_count)
			ret = -1;
	}
	return ret;
}

/*2. i2c_data : 0x1234*/
static int default_i2c_write_A(struct v4l2_subdev *sd,
				unsigned short i2c_data[], unsigned int length)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int i;
	int reg_count = 2; /* sensor dependency */
	unsigned char buf[reg_count];

	for (i = 0; i < length; i++) {
		buf[0] = i2c_data[i] >> 8;
		buf[1] = i2c_data[i] & 0xff;

		if (i2c_master_send(p_client, buf, reg_count) != reg_count)
			ret = -1;
	}
	return ret;
}

/*3. cmd : 0x1234, data : 0x5678*/
static int default_i2c_write_reg(struct v4l2_subdev *sd, unsigned short cmd,
				unsigned short data)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	int ret = 0;
	unsigned int reg_count = 4; /* sensor dependency */
	unsigned char buf[reg_count];

	buf[0] = cmd >> 8;
	buf[1] = cmd;
	buf[2] = data >> 8;
	buf[3] = data & 0xff;

	msg.addr = p_client->addr;
	msg.flags = 0;
	msg.len = reg_count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(p_client->adapter, &msg, 1);

	return ret;
}

/*4. cmd : 0x12, data : {0x56}*/
static int default_i2c_write_reg_A(struct v4l2_subdev *sd, unsigned char cmd,
				unsigned char data)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	int ret = 0;
	unsigned int reg_count = 2; /* sensor dependency */
	unsigned char buf[reg_count];

	buf[0] = cmd;
	buf[1] = data;

	msg.addr = p_client->addr;
	msg.flags = 0;
	msg.len = reg_count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(p_client->adapter, &msg, 1);

	return ret;
}

/*5. cmd : {0x12,0x34, data : 0x56,0x78}*/
/*
mvUint8 sensorCaptureOffData[][4] = {
	{0x12, 0x34, 0x56, 0x78},
};
*/
static int default_i2c_write_B(struct v4l2_subdev *sd, unsigned char *i2c_data,
				unsigned int length)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int i;
	int reg_count = 4; /* sensor dependency */
	unsigned char buf[reg_count];

	for (i = 0; i < length; i++) {
		buf[0] = *i2c_data;
		buf[1] = *(i2c_data+1);
		buf[2] = *(i2c_data+2);
		buf[3] = *(i2c_data+3);
		if (i2c_master_send(p_client, i2c_data, reg_count) != reg_count)
			ret = -1;
	}
	return ret;
}

/*6. cmd : {0x12,0x34, data : 0x56}*/
static int default_i2c_write_C(struct v4l2_subdev *sd, unsigned char *i2c_data,
				unsigned int length)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int i;
	int reg_count = 3; /* sensor dependency */
	unsigned char buf[reg_count];

	for (i = 0; i < length; i++) {
		buf[0] = *i2c_data;
		buf[1] = *(i2c_data+1);
		buf[2] = *(i2c_data+2);
		if (i2c_master_send(p_client, i2c_data, reg_count) != reg_count)
			ret = -1;
	}
	return ret;
}

/* 7. cmd : {0x12 data : 0x34,0x56}*/
/* 8. cmd : {0x12,0x34, data : 0x56,0x78}
	  cmd : {0x12,0x34, data : 0x56}          */
/* 9. cmd : {0x12,0x34, data : 0x56,0x78}
	  cmd : {0x12, data : 0x56}          */
/*10. cmd : {0x12,0x34, data : 0x56,0x78}
	  cmd : {0x12, data : 0x56,0x78}          */

static int default_i2c_read(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned int length)
{
	struct i2c_client *p_client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int i;
	int reg_count = 4; /* sensor dependency */

	return ret;
}

static int default_set_brightness(struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->value) {
	case EV_MINUS_1:
		/* TODO */
		break;
	case EV_DEFAULT:
		break;
	case EV_PLUS_1:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int default_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	/*
	case A:
		to do
		break;
	*/
	default:
		/* to do */
		break;
	}

	return ret;
}

static int default_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = 0;
	unsigned int id = ctrl->id;

	switch (id) {
	case V4L2_CID_CAMERA_SCENE_MODE:
		/* TODO */
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		/* TODO */
		break;
	default:
		/* TODO */
		break;
	}

	return ret;
}

static int default_SensorSetResolution(struct v4l2_subdev *sd)
{
	int ret = 0;

	return ret;
}

static int default_SensorGetResolution(struct v4l2_subdev *sd)
{
	int ret = 0;

	return ret;
}

static int default_SensorReset(int enable)
{
	int ret = 0;

	return ret;
}

static int default_SensorPower(int enable)
{
	int ret = 0;

	return ret;
}

static int default_sensor_power_sequence(struct v4l2_subdev *sd ,int on)
{
	int ret = 0;

	if (on) {
		/*power up*/

		/*reset */

		/*mclk*/

		/*reset*/
	} else {
		/*reset*/

		/*mclk off*/

		/*reset*/
	}

	return ret;
}

static int default_init(struct v4l2_subdev *sd)
{
	int ret = 0;

	struct i2c_client *p_client = v4l2_get_subdevdata(sd);

	return ret;
}

static const struct v4l2_subdev_core_ops default_core_ops = {
	.init = default_init,
	.g_ctrl = default_g_ctrl,
	.s_ctrl = default_s_ctrl,
	.reset = default_SensorReset,
	.s_power = default_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops default_video_ops = {
	.s_mbus_fmt = default_SensorSetResolution,
	.g_mbus_fmt = default_SensorGetResolution,
};


static const struct v4l2_subdev_ops default_ops = {
	.core = &default_core_ops,
	.video = &default_video_ops,
};

static int default_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct default_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;

	state = kzalloc(sizeof(struct default_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	subdev = &state->sd;
	strcpy(subdev->name, DEFAULT_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &default_ops);

	return 0;
}

static int default_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id default_id[] = {
	{DEFAULT_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, defualt_id);

static struct i2c_driver default_i2c_driver = {
	.driver = {
		.name = DEFAULT_DRIVER_NAME,
	},
	.probe = default_probe,
	.remove = default_remove,
	.id_table = default_id,
};

static int __init default_mod_init(void)
{
	return i2c_add_driver(&default_i2c_driver);
}

static void __exit default_mod_exit(void)
{
	i2c_del_driver(&default_i2c_driver);
}

late_initcall(default_mod_init);
module_exit(default_mod_exit);

