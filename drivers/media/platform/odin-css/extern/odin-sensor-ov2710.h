
#ifndef __ODIN_SENSOR_OV2710_H__
#define __ODIN_SENSOR_OV2710_H__


#define OV2710_I2C	(0x6c>>1)	/* I2C address */

struct ov2710_state {
	/* struct ov2710_platform_data *pdata; */
	struct v4l2_subdev sd;
};

#endif /* __ODIN_SENSOR_OV2710_H__ */
