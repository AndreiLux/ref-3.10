
#ifndef __ODIN_SENSOR_DUMMY_H__
#define __ODIN_SENSOR_DUMMY_H__

struct dummy_state {
	/* struct imx111_platform_data *pdata; */
	struct css_i2cboard_platform_data pdata;

	struct v4l2_subdev sd;
};

#endif /* __ODIN_SENSOR_DUMMY_H__ */

