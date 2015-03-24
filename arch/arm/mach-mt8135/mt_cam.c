#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/mt_cam.h>
#include <mach/mt_gpio_def.h>
#include "linux/camera/kd_imgsensor.h"
#include "linux/camera/kd_camera_feature.h"
#include "kd_camera_hw.h"

#include <linux/clkdev.h>
#include <linux/clk.h>

static unsigned long IsPowerOn;

struct mt_cam {
	char *name;
	int type;
	struct mt_camera_sensor_config conf;
	struct clk *cmmclk;
	struct clk *auxclk;
	void (*pwr_enable)(struct mt_cam *);
	void (*pwr_disable)(struct mt_cam *);
};

static inline void cam_clk_enable(struct mt_cam *cam)
{
	if (cam->cmmclk) {
		clk_enable(cam->cmmclk);
		mdelay(1);
	}
	if (cam->auxclk) {
		clk_enable(cam->auxclk);
		mdelay(1);
	}
}

static inline void cam_clk_disable(struct mt_cam *cam)
{
	if (cam->cmmclk) {
		clk_disable(cam->cmmclk);
		mdelay(1);
	}
	if (cam->auxclk) {
		clk_disable(cam->auxclk);
		mdelay(1);
	}
}

static inline void cam_reset_enable(struct mt_cam *cam)
{
	gpio_set_value(cam->conf.pdn_pin, cam->conf.pdn_on);
	mt_pin_set_mode_gpio(cam->conf.rst_pin);
	gpio_set_value(cam->conf.rst_pin, cam->conf.rst_on);
}

static inline void cam_reset_disable(struct mt_cam *cam)
{
	gpio_set_value(cam->conf.pdn_pin, !cam->conf.pdn_on);
	mdelay(11);
	mt_pin_set_mode_gpio(cam->conf.rst_pin);
	gpio_set_value(cam->conf.rst_pin, !cam->conf.rst_on);
}

static inline void cam_reset_shutdown(struct mt_cam *cam)
{
	mt_pin_set_mode_gpio(cam->conf.pdn_pin);
	gpio_set_value(cam->conf.pdn_pin, 0);
	mt_pin_set_mode_gpio(cam->conf.rst_pin);
	gpio_set_value(cam->conf.rst_pin, 0);
}

static void general_power_off(struct mt_cam *cam)
{
	struct mt_camera_sensor_config *conf = &cam->conf;

	pr_debug("[%s] +++ Camera Sensor name:%s, IsPowerOn(%d) +++\n",
		__func__, cam->name, (int)IsPowerOn);

	/* Check for current power state and clear to avoid double disable */
	if (!test_and_clear_bit(0, &IsPowerOn))
		return;

	cam_reset_enable(cam);
	cam_clk_disable(cam);

	if (conf->vcam_d >= 0)
		hwPowerDown(conf->vcam_d, cam->name);
	if (conf->vcam_io >= 0)
		hwPowerDown(conf->vcam_io, cam->name);
	if (conf->vcam_a >= 0)
		hwPowerDown(conf->vcam_a, cam->name);
	mdelay(10);

	/* For Power Saving */
	cam_reset_shutdown(cam);
}

/* OV2724 Power Control */
static void ov2724_power_on(struct mt_cam *cam)
{
	struct mt_camera_sensor_config *conf = &cam->conf;

	/* Check for current power state and set to avoid double enable */
	if (test_and_set_bit(0, &IsPowerOn))
		return;

	cam_reset_enable(cam);

	/* DOVDD */
	if (conf->vcam_io >= 0) {
		hwPowerOn(conf->vcam_io, VOL_1800, cam->name);
		mdelay(1);
	}

	/* AVDD */
	if (conf->vcam_a >= 0)
		hwPowerOn(conf->vcam_a, VOL_2800, cam->name);


	/* DVDD */
	if (conf->vcam_d >= 0) {
		hwPowerOn(conf->vcam_d, VOL_1800, cam->name);
		mdelay(5);
	}

	cam_clk_enable(cam);
	cam_reset_disable(cam);
}

/* HI704 Power Control */
static void hi704_power_on(struct mt_cam *cam)
{
	struct mt_camera_sensor_config *conf = &cam->conf;

	/* Check for current power state and clear to avoid double disable */
	if (test_and_set_bit(0, &IsPowerOn))
		return;

	/* PDN (CHIP_ENB) : init to Low */
	gpio_set_value(cam->conf.pdn_pin, !cam->conf.pdn_on);

	mdelay(2);

	/* AVDD */
	if (conf->vcam_a >= 0) {
		hwPowerOn(conf->vcam_a, VOL_2800, cam->name);
		mdelay(1);
	}

	/* PDN (CHIP_ENB) : Low -> High */
	gpio_set_value(cam->conf.pdn_pin, cam->conf.pdn_on);
	mdelay(1);

	/* DOVDD */
	if (conf->vcam_io >= 0)
		hwPowerOn(conf->vcam_io, VOL_1800, cam->name);

	/* DVDD */
	if (conf->vcam_d >= 0)
		hwPowerOn(conf->vcam_d, VOL_1800, cam->name);

	mdelay(2);

	cam_clk_enable(cam);
	mdelay(4);

	/* PDN (CHIP_ENB) : High -> Low */
	cam_reset_disable(cam);
}

static struct mt_cam default_cam[] = {
	{
		.name = "ov2724mipiraw",
		.type = DUAL_CAMERA_MAIN_SENSOR,
		.pwr_enable = ov2724_power_on,
		.pwr_disable = general_power_off,
	},
	{
		.name = "hi704yuv",
		.type = DUAL_CAMERA_SUB_SENSOR,
		.pwr_enable = hi704_power_on,
		.pwr_disable = general_power_off,
	},
};

/* the line below must be excused from camelcase rule
 * until kd_sensorlist.c is reworked */
int kdCISModulePowerOn(
	CAMERA_DUAL_CAMERA_SENSOR_ENUM sensor_type,
	char *sensor_name, bool on, char *mode_name)
{
	int i;
	struct mt_cam *selected_cam = NULL;

	for (i = 0; i < ARRAY_SIZE(default_cam); ++i) {
		struct mt_cam *cam = &default_cam[i];
		if (cam->type != sensor_type || strcmp(sensor_name, cam->name))
			continue;
		selected_cam = cam;
	}

	if (!selected_cam || selected_cam->conf.disabled) {
		pr_debug("%s: sensor not found: name=%s; type=%d\n",
			__func__, sensor_name, sensor_type);
		return -ENODEV;
	}

	pr_debug("%s: sensor found: name=%s; type=%d; Power=%d\n",
		__func__, sensor_name, sensor_type, on);

	/*
	 * Here, assumption is, that only one censor is enabled at any given time.
	 * in other words, ISP data pipeline is shared for all sensors,
	 * and there is only one such pipeline.
	 * If ISP has support for multiple sensors concurrently, we need to
	 * rework the code, and only reset the sensors that are sharing the same
	 * data pipeline.
	 */
	if (on) {
		/* bring all cams into reset state */
		for (i = 0; i < ARRAY_SIZE(default_cam); ++i) {
			struct mt_cam *cam = &default_cam[i];
			if (!cam->conf.disabled) {
				cam_reset_enable(cam);
				selected_cam->pwr_disable(selected_cam);
			}
		}
		selected_cam->pwr_enable(selected_cam);
	} else {
		selected_cam->pwr_disable(selected_cam);
		/* bring all cams into low-power state */
		for (i = 0; i < ARRAY_SIZE(default_cam); ++i) {
			struct mt_cam *cam = &default_cam[i];
			if (!cam->conf.disabled)
				cam_reset_shutdown(cam);
		}
	}
	return 0;
}
EXPORT_SYMBOL(kdCISModulePowerOn);

void mt_cam_init(struct mt_camera_sensor_config *config, int sensor_num)
{
	struct mt_cam *cam;
	struct mt_camera_sensor_config *conf;
	int i, j;

	for (i = 0, cam = default_cam; i < ARRAY_SIZE(default_cam); ++i, ++cam) {
		for (j = 0, conf = 0; j < sensor_num; ++j)
			if (config && !strcmp(cam->name, config[i].name)) {
				conf = &config[i];
				break;
			}

		if (!conf) {
			pr_notice("%s: sensor: %s; no board config found. Disabled\n",
				__func__, cam->name);
			cam->conf.disabled = true;
			continue;
		}
		cam->conf = *conf;

		if (cam->conf.disabled) {
			pr_err("%s: %s is disabled by board config\n",
				__func__, cam->name);
			continue;
		}

		if (cam->conf.mclk_name)
			cam->cmmclk = clk_get(NULL, cam->conf.mclk_name);
		if (cam->conf.auxclk_name)
			cam->auxclk = clk_get(NULL, cam->conf.auxclk_name);
		if (IS_ERR(cam->cmmclk) || IS_ERR(cam->auxclk)) {
			cam->conf.disabled = true;
			pr_err("%s: %s failed to request sensor clocks\n",
				__func__, cam->name);
			continue;
		}
		if (gpio_request(cam->conf.pdn_pin, cam->name) ||
		    gpio_request(cam->conf.rst_pin, cam->name)) {
			pr_err("%s: %s failed to request GPIOs\n", __func__, cam->name);
			cam->conf.disabled = true;
		}
		if (!cam->conf.disabled)
			cam_reset_shutdown(cam);
	}
}
