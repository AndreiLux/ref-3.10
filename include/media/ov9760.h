/**
 * Copyright (c) 2012-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV9760_H__
#define __OV9760_H__

#include <linux/ioctl.h>

#define OV9760_IOCTL_SET_MODE		_IOW('o', 1, struct ov9760_mode)
#define OV9760_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV9760_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV9760_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define OV9760_IOCTL_GET_STATUS		_IOR('o', 5, __u8)
#define OV9760_IOCTL_SET_GROUP_HOLD	_IOW('o', 6, struct ov9760_ae)
#define OV9760_IOCTL_GET_FUSEID	_IOR('o', 7, struct ov9760_sensordata)

struct ov9760_sensordata {
	__u32 fuse_id_size;
	__u8 fuse_id[16];
};

struct ov9760_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

struct ov9760_ae {
	__u32 frame_length;
	__u8 frame_length_enable;
	__u32 coarse_time;
	__u8 coarse_time_enable;
	__s32 gain;
	__u8 gain_enable;
};

#ifdef __KERNEL__
struct ov9760_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct ov9760_platform_data {
	int (*power_on)(struct ov9760_power_rail *pw);
	int (*power_off)(struct ov9760_power_rail *pw);
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif  /* __OV9760_H__ */

