/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/i2c.h>

#include "odin-css.h"
#include "odin-css-platdrv.h"

static struct css_i2cboard_platform_data dummy_pdata = {
	.power_gpio = {-1, -1},
	.pwdn_gpio = {-1, -1},
	.reset_gpio = {-1, -1},
	.detect_gpio = {-1, -1},
	.is_mipi = 0,
	.sensor_init.value = NULL,
};

static struct i2c_board_info dummyi2c_info = {
	 I2C_BOARD_INFO("DUMMY", 0x10),
	 .platform_data = &dummy_pdata,
};

static struct css_platform_sensor dummy_sensor = {
	.id = SENSOR_DUMMY,
	.name = "DUMMY",
	.info = &dummyi2c_info,
	.init_width = 0,
	.init_height = 0,
	.i2c_busnum = 10,
};

static struct css_i2cboard_platform_data imx091_pdata = {
	.power_gpio = {-1, -1},
	.pwdn_gpio = {-1, CSS_POLARITY_ACTIVE_HIGH},
	.reset_gpio = {179/*19*/, CSS_POLARITY_ACTIVE_LOW},
	.detect_gpio = {-1, -1},
	.is_mipi = 1,
	.sensor_init.value = NULL, /* imx091_initalize_value[], */
};

static struct i2c_board_info imx091_i2c_info = {
	 I2C_BOARD_INFO("IMX091",0x34),
	 .platform_data = &imx091_pdata,
};

static struct css_platform_sensor imx091_sensor = {
	.id = SENSOR_REAR,
	.name = "imx091",
	.info = &imx091_i2c_info,
	.init_width = 2104,  /* QXGA@30fps 13M15fps W:4208 W:1280 */
	.init_height = 1560, /* QXGA@30fps 13M15fps H:3120 H:960  */
	.i2c_busnum = 10,
};

static struct css_i2cboard_platform_data imx132_pdata = {
	.power_gpio = {-1, -1},
	.pwdn_gpio = {-1, CSS_POLARITY_ACTIVE_HIGH},
	.reset_gpio = {181/*21*/, CSS_POLARITY_ACTIVE_LOW},
	.detect_gpio = {-1, -1},
	.is_mipi = 1,
	.sensor_init.value = NULL,
};

static struct i2c_board_info imx132_i2c_info = {
	I2C_BOARD_INFO("IMX132",0x6C),
	.platform_data = &imx132_pdata,
};

static struct css_platform_sensor imx132_sensor = {
	.id = SENSOR_FRONT,
	.name = "imx132",
	.info = &imx132_i2c_info,
	.init_width = 1976,
	.init_height = 1200,
	.i2c_busnum = 12,
};

static struct css_i2cboard_platform_data imx135_pdata = {
	.power_gpio = {-1, -1},
	.pwdn_gpio = {-1, CSS_POLARITY_ACTIVE_HIGH},
	.reset_gpio = {179/*19*/, CSS_POLARITY_ACTIVE_LOW},
	.detect_gpio = {-1, -1},
	.ois_reset_gpio = {174/*14*/, CSS_POLARITY_ACTIVE_LOW},
	.ois_power_gpio = {175/*15*/, CSS_POLARITY_ACTIVE_LOW},
	.is_ois = 1,
	.is_mipi = 1,
	.sensor_init.value = NULL, /* imx135_initalize_value[], */
	.io_enable_gpio = {172 /*12*/, CSS_POLARITY_ACTIVE_LOW},
};

static struct i2c_board_info imx135_i2c_info = {
	I2C_BOARD_INFO("IMX135",0x20),
	.platform_data = &imx135_pdata,
};

static struct css_platform_sensor imx135_sensor = {
	.id = SENSOR_REAR,
	.name = "imx135",
	.info = &imx135_i2c_info,
	.init_width = 1920,
	.init_height = 1080,
	.ois_support = 1,
	.i2c_busnum = 10,
};

static struct css_i2cboard_platform_data imx208_pdata = {
	.power_gpio = {-1, -1},
	.pwdn_gpio = {-1, CSS_POLARITY_ACTIVE_HIGH},
	.reset_gpio = {181/*21*/, CSS_POLARITY_ACTIVE_LOW},
	.detect_gpio = {-1, -1},
	.is_mipi = 1,
	.sensor_init.value = NULL,
};

static struct i2c_board_info imx208_i2c_info = {
	I2C_BOARD_INFO("IMX208",0x6E),
	.platform_data = &imx208_pdata,
};

static struct css_platform_sensor imx208_sensor = {
	.id = SENSOR_FRONT,
	.name = "imx208",
	.info = &imx208_i2c_info,
	.init_width = 1936,
	.init_height = 1096,
	.i2c_busnum = 12,
};

struct css_platform_sensor_group sensor_group = {
	.default_cam = SENSOR_REAR,
	.sensor = {
		/* Rear */
		&imx135_sensor,
		/* Front */
#if defined(CONFIG_VIDEO_ODIN_SENSOR_IMX132)
		&imx132_sensor,
#endif
#if defined(CONFIG_VIDEO_ODIN_SENSOR_IMX208)
		&imx208_sensor,
#endif
		/* Dummy */
		&dummy_sensor,
	},
};

