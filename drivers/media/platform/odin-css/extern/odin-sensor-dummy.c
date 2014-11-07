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
#include "odin-sensor-dummy.h"

#define DUMMY_DRIVER_NAME "DUMMY"


static inline struct dummy_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct dummy_state, sd);
}

int dummy_sensor_reset(struct v4l2_subdev *sd, int active)
{
	return 0;
}

static int dummy_sensor_power_sequence(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static int dummy_init(struct v4l2_subdev *sd)
{
	return 0;
}

static int dummy_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct dummy_state *state = to_state(sd);

	state->pdata.current_width = mf->width;
	state->pdata.current_height = mf->height;

	return 0;
}

static int dummy_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct dummy_state *state = to_state(sd);

	mf->width	= state->pdata.current_width;
	mf->height	= state->pdata.current_height;

	return 0;
}

static const struct v4l2_subdev_core_ops dummy_core_ops = {
	.init = dummy_init,
	.reset = dummy_sensor_reset,
	.s_power = dummy_sensor_power_sequence,
};

static const struct v4l2_subdev_video_ops dummy_video_ops = {
	.s_mbus_fmt = dummy_s_fmt,
	.g_mbus_fmt = dummy_g_fmt,
};

static const struct v4l2_subdev_ops dummy_ops = {
	.core = &dummy_core_ops,
	.video = &dummy_video_ops,
};

static int dummy_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct dummy_state *state = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	css_info(">>>>>>>>>>>>dummy_probe<<<<<<<<<<<<<<<<\n");

	state = kzalloc(sizeof(struct dummy_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	memcpy(&state->pdata, pdata, sizeof(state->pdata));

	subdev = &state->sd;
	strcpy(subdev->name, DUMMY_DRIVER_NAME);

	/* registering subdev */
	v4l2_i2c_subdev_init(subdev, client, &dummy_ops);

	return 0;
}

static int dummy_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id dummy_id[] = {
	{DUMMY_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, dummy_id);

static struct i2c_driver dummy_i2c_driver = {
	.driver = {
		.name = DUMMY_DRIVER_NAME,
	},
	.probe = dummy_probe,
	.remove = dummy_remove,
	.id_table = dummy_id,
};

static int __init dummy_mod_init(void)
{
	return i2c_add_driver(&dummy_i2c_driver);
}

static void __exit dummy_mod_exit(void)
{
	i2c_del_driver(&dummy_i2c_driver);
}

late_initcall(dummy_mod_init);
module_exit(dummy_mod_exit);
