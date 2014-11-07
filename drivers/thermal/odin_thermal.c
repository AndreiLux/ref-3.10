/* drivers/thermal/odin_thermal.c
 *
 * ODIN thermal sensor driver
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/odin_mailbox.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <trace/events/power.h>

#include <linux/board_lge.h>
#include <linux/mfd/d2260/pmic.h>
#include <linux/wakelock.h>

#define TSENSOR_GET_TEMP	0x50000001
#define TSENSOR_GET_FUSING_DATA 0x50000002

#define DEFAULT_PCB_THERM_TEMP 200
#define VOLTAGE_PCB_THERM_MAX 4002
#define VOLTAGE_PCB_THERM_MIN 102
#define DEGREE_PCB_THERM_MAX 1250
#define DEGREE_PCB_THERM_MIN -400
#define GAP_OF_PCB_THERM_TEMP_STEP 50
#define NUMBER_OF_ROW_VOLTAGE_TO_TEMP_TABLE 34

struct odin_tsens_sensor_data {
	int id;
	enum thermal_device_mode mode;
	struct thermal_zone_device *tz;
};

struct odin_tsens_drvdata {
	int num_sensors;
	struct odin_tsens_sensor_data *sensors;
};

struct tsens_code_pair {
	u32 code;
	u32 temp;
};
/* NOTE: ADC codes shall be increased by 1. */
static const struct tsens_code_pair tsens_code_tbl[] = {
	{215,    0}, {216,   12}, {217,   25}, {218,   37},
	{219,   50}, {220,   63}, {221,   75}, {222,   88}, {223,  100},
	{224,  113}, {225,  125}, {226,  138}, {227,  150}, {228,  163},
	{229,  175}, {230,  188}, {231,  200}, {232,  213}, {233,  225},
	{234,  238}, {235,  250}, {236,  263}, {237,  275}, {238,  288},
	{239,  300}, {240,  313}, {241,  325}, {242,  338}, {243,  350},
	{244,  363}, {245,  375}, {246,  388}, {247,  400}, {248,  413},
	{249,  425}, {250,  438}, {251,  450}, {252,  467}, {253,  483},
	{254,  500}, {255,  517}, {256,  533}, {257,  550}, {258,  563},
	{259,  575}, {260,  588}, {261,  600}, {262,  613}, {263,  625},
	{264,  638}, {265,  650}, {266,  664}, {267,  679}, {268,  693},
	{269,  707}, {270,  721}, {271,  736}, {272,  750}, {273,  764},
	{274,  779}, {275,  793}, {276,  807}, {277,  821}, {278,  836},
	{279,  850}, {280,  864}, {281,  879}, {282,  893}, {283,  907},
	{284,  921}, {285,  936}, {286,  950}, {287,  964}, {288,  979},
	{289,  993}, {290, 1007}, {291, 1021}, {292, 1036}, {293, 1050},
	{294, 1064}, {295, 1079}, {296, 1093}, {297, 1107}, {298, 1121},
	{299, 1136}, {300, 1150}, {301, 1164}, {302, 1179}, {303, 1193},
	{304, 1207}, {305, 1221}, {306, 1236}, {307, 1250},
};
static const u32 tsens_code_tbl_size = ARRAY_SIZE(tsens_code_tbl);

static struct odin_tsens_drvdata *tsensdev;

struct vadc_map_pcb_therm_temp {
	int celsius;
	int voltage;
};

static unsigned long ex_temp = 25000;
static int temp_flag = 1;
static int temp_count = 0;

static const struct vadc_map_pcb_therm_temp adcmap_pcb_therm_threshold[NUMBER_OF_ROW_VOLTAGE_TO_TEMP_TABLE] = {0};

static const struct vadc_map_pcb_therm_temp adbmap_pcb_therm_threshold_2_5[] = {
	{-400,	2443},
	{-350,	2419},
	{-300,	2389},
	{-250,	2349},
	{-200,	2298},
	{-150,	2233},
	{-100,	2155},
	{-50,	2061},
	{0, 	1951},
	{50,	1828},
	{100,	1692},
	{150,	1548},
	{200,	1399},
	{250,	1250},
	{300,	1105},
	{350,	 968},
	{400,	 841},
	{450,	 726},
	{500,	 623},
	{550,	 533},
	{600,	 455},
	{650,	 387},
	{700,	 329},
	{750,	 281},
	{800,	 239},
	{850,	 204},
	{900,	 174},
	{950,	 149},
	{1000,	 128},
	{1050,	 110},
	{1100,	  95},
	{1150,	  83},
	{1200,	  72},
	{1250,	  62},
};

static struct wake_lock odin_thermal_wakelock;

static int get_temp(u32 id, unsigned long *temp)
{
	u32 mb_data[7] = {0, };
	u32 code;
	u32 tsens_code_min = tsens_code_tbl[0].code;
	u32 tsens_code_max = tsens_code_tbl[tsens_code_tbl_size - 1].code;
	int rc;

	if (!tsensdev || !tsensdev->sensors)
		return -ENODEV;

	if (id >= tsensdev->num_sensors || !temp)
		return -EINVAL;

	mb_data[0] = SENSOR_CMD_TYPE;
	mb_data[1] = TSENSOR_GET_TEMP;
	mb_data[2] = id;
	rc = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_THERMAL, mb_data, 1000);
	if (rc < 0) {
		pr_err("%s: mailbox call failed (%d)\n", __func__, rc);
		return rc;
	}

	code = mb_data[0];

	if (code == 0 || code > tsens_code_max) {
		pr_err("%s: invalid code (%d)(%d)\n", __func__, id, code);
		return -EIO;
	}

	/* Convert ADC code to milli-degrees Celsius */
	if (code < tsens_code_min)
		*temp = tsens_code_tbl[0].temp * 100;
	else
		*temp = tsens_code_tbl[code - tsens_code_min].temp * 100;

	pr_debug("%s: TS%u=%lu (code=%u)\n", __func__, id, *temp, code);
	return 0;
}

int get_pcb_therm_top_temp(unsigned long *temp)
{
	unsigned long diff;

	*temp = (voltage_to_degree(d2260_get_pcb_therm_top_voltage())) * 100;

	if (temp_flag == 0 && temp_count < 5)
	{
		diff = (*temp > ex_temp) ? (*temp - ex_temp) : (ex_temp - *temp);
		if(diff > 30000)
		{
			pr_err("%s: invalid temp (%d)(%d)(%d)\n", __func__, (int)*temp/100, (int)ex_temp/100, temp_count);
			*temp = ex_temp;
			temp_count++;
			return 0;
		}
	}
	ex_temp = *temp;
	temp_flag = 0;
	temp_count = 0;

	return 0;
}

int get_pcb_therm_bot_temp(unsigned long *temp)
{
	*temp = (voltage_to_degree(d2260_get_pcb_therm_bot_voltage())) * 100;

	return 0;
}

int voltage_to_degree(int voltage)
{
	int count = 0;
	int current_degree = DEFAULT_PCB_THERM_TEMP;
	static int ex_degree = DEFAULT_PCB_THERM_TEMP;

	if (voltage >= VOLTAGE_PCB_THERM_MAX)
			return DEGREE_PCB_THERM_MAX;

	if (voltage < VOLTAGE_PCB_THERM_MIN)
			return DEGREE_PCB_THERM_MIN;

	for (count = 0; count < NUMBER_OF_ROW_VOLTAGE_TO_TEMP_TABLE; count++) {
		if ((voltage < adcmap_pcb_therm_threshold[count].voltage) &&
			(voltage >= adcmap_pcb_therm_threshold[count + 1].voltage)) {

				current_degree = adcmap_pcb_therm_threshold[count].celsius +
					(((adcmap_pcb_therm_threshold[count].voltage - voltage) *
					GAP_OF_PCB_THERM_TEMP_STEP)/
					(adcmap_pcb_therm_threshold[count].voltage -
					adcmap_pcb_therm_threshold[count + 1].voltage));

				ex_degree = current_degree;
				return current_degree;
		}
	}
	return ex_degree;
}

static  int odin_tz_get_temp(struct thermal_zone_device *tz,
	unsigned long *temp)
{
	struct odin_tsens_sensor_data *sensor;
	int rc;

	if (!tz || !tz->devdata || !temp)
		return -EINVAL;

	sensor = (struct odin_tsens_sensor_data *)tz->devdata;

	if (sensor->mode == THERMAL_DEVICE_DISABLED)
		return -EIO;

	if (sensor->id == tsensdev->num_sensors - 1) {
		rc = get_pcb_therm_bot_temp(temp);
	} else if (sensor->id == tsensdev->num_sensors - 2) {
		rc = get_pcb_therm_top_temp(temp);
	} else {
		rc = get_temp(sensor->id, temp);
	}

	if (rc) {
		dev_err(&tz->device, "failed to get temp (%d)\n", rc);
		return rc;
	}

	/* trace event of Thermal Sensor Temperature */
	trace_odin_ts_temp("odin-thermal temp", sensor->id, *temp);

	return 0;
}

static int odin_tz_get_mode(struct thermal_zone_device *tz,
	enum thermal_device_mode *mode)
{
	struct odin_tsens_sensor_data *sensor;

	if (!tz || !tz->devdata || !mode)
		return -EINVAL;

	sensor = (struct odin_tsens_sensor_data *)tz->devdata;
	*mode = sensor->mode;

	return 0;
}

static int odin_tz_set_mode(struct thermal_zone_device *tz,
	enum thermal_device_mode mode)
{
	struct odin_tsens_sensor_data *sensor;

	if (!tz || !tz->devdata)
		return -EINVAL;

	if (mode != THERMAL_DEVICE_DISABLED && mode != THERMAL_DEVICE_ENABLED)
		return -EINVAL;

	sensor = (struct odin_tsens_sensor_data *)tz->devdata;

	/* FIXME: sensor ENABLE/DISABLE */

	sensor->mode = mode;
	return 0;
}

static unsigned long thermal_mitigation_status = 0;
static int odin_get_therm_miti(struct thermal_zone_device *tz,
	int *therm_miti_status)
{
	/* FIXME: need lock? */
	*therm_miti_status = thermal_mitigation_status;

	return 0;
}

static int odin_set_therm_miti(struct thermal_zone_device *tz,
	int *therm_miti_status)
{
	/* FIXME: need lock? */
	thermal_mitigation_status = *therm_miti_status;

	return 0;
}

static int odin_set_shutdown(struct thermal_zone_device *tz)
{
	/* FIXME: need lock? */
	wake_lock(&odin_thermal_wakelock);
	pr_err("%s: shutdown by thermal daemon~!\n", __func__);

	return 0;
}

static struct thermal_zone_device_ops odin_tz_dev_ops = {
	.get_temp = odin_tz_get_temp,
	.get_mode = odin_tz_get_mode,
	.set_mode = odin_tz_set_mode,
	.get_therm_miti = odin_get_therm_miti,
	.set_therm_miti = odin_set_therm_miti,
	.set_shutdown = odin_set_shutdown,
};

static int register_tz_devices(void)
{
	int rc, i;

	if (!tsensdev || !tsensdev->sensors)
		return -ENODEV;

	for (i = 0; i < tsensdev->num_sensors; i++) {
		char type[THERMAL_NAME_LENGTH];
		/* register pcb_thermistor for Rev.J to tz devices */
		if (i == tsensdev->num_sensors - 1) {
			snprintf(type, THERMAL_NAME_LENGTH, "pcb_bot_thermistor");
		} else if (i == tsensdev->num_sensors - 2) {
			snprintf(type, THERMAL_NAME_LENGTH, "pcb_top_thermistor");
		} else {
			snprintf(type, THERMAL_NAME_LENGTH, "tsensor_core%d", i);
		}
		tsensdev->sensors[i].mode = THERMAL_DEVICE_ENABLED;
		tsensdev->sensors[i].id = i;
		tsensdev->sensors[i].tz =
			thermal_zone_device_register(type, 0, 0,
				&tsensdev->sensors[i], &odin_tz_dev_ops,
				NULL, 0, 0);
		if (IS_ERR(tsensdev->sensors[i].tz)) {
			pr_err("%s: can't register tz dev (%s)\n",
				__func__, type);
			rc = -ENODEV;
			goto err_register_tz;
		}
		/* drvdata->sensors[i].mode = THERMAL_DEVICE_ENABLED; */
	}

	return 0;

err_register_tz:

	for (i -= 1; i > 0; i--)
		thermal_zone_device_unregister(tsensdev->sensors[i].tz);

	return rc;
}

static int odin_tsens_probe(struct platform_device *pdev)
{
	const struct device_node *np = pdev->dev.of_node;
	char *prop;
	int rc;
	int count;

	if (tsensdev) {
		dev_err(&pdev->dev, "device is busy\n");
		return -EBUSY;
	}

	if (!np) {
		dev_err(&pdev->dev, "can't find device node\n");
		return -ENODEV;
	}

	tsensdev = kzalloc(sizeof(struct odin_tsens_drvdata), GFP_KERNEL);
	if (!tsensdev) {
		dev_err(&pdev->dev, "can't alloc drvdata\n");
		return -ENOMEM;
	}

	prop = "num-sensors";
	rc = of_property_read_u32(np, prop, &tsensdev->num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "can't read prop %s (%d)\n", prop, rc);
		goto err_tsens_probe;
	}

	tsensdev->sensors = kzalloc(sizeof(struct odin_tsens_sensor_data) *
				tsensdev->num_sensors, GFP_KERNEL);
	if (!tsensdev->sensors) {
		dev_err(&pdev->dev, "can't alloc sensor data (%d)\n", rc);
		rc = -ENOMEM;
		goto err_tsens_probe;
	}

	/* Initial adbmap_pcb_therm_threshold */
	for (count = 0; count < NUMBER_OF_ROW_VOLTAGE_TO_TEMP_TABLE; count++) {
		memcpy(&adcmap_pcb_therm_threshold[count],
				&adbmap_pcb_therm_threshold_2_5[count],
				sizeof(struct vadc_map_pcb_therm_temp));
	}

	wake_lock_init(&odin_thermal_wakelock, WAKE_LOCK_SUSPEND,"odin_thermal_Lock");

	platform_set_drvdata(pdev, tsensdev);
	return 0;

err_tsens_probe:

	if (tsensdev->sensors)
		kfree(tsensdev->sensors);

	if (tsensdev) {
		kfree(tsensdev);
		tsensdev = NULL;
	}

	platform_set_drvdata(pdev, NULL);
	return rc;
}

static int odin_tsens_remove(struct platform_device *pdev)
{
	int i;

	if (!tsensdev || !tsensdev->sensors)
		return -ENODEV;

	for (i = 0; i < tsensdev->num_sensors; i++)
		thermal_zone_device_unregister(tsensdev->sensors[i].tz);

	kfree(tsensdev->sensors);
	kfree(tsensdev);
	tsensdev = NULL;
	platform_set_drvdata(pdev, NULL);

	return 0;
}

int odin_tsens_get_temp(u32 id, unsigned long *temp)
{
	if (!tsensdev)
		return -ENODEV;

	return get_temp(id, temp);
}
EXPORT_SYMBOL_GPL(odin_tsens_get_temp);

static int odin_tsens_suspend(struct device *dev)
{
	temp_flag = 0;
	return 0;
}

static int odin_tsens_resume(struct device *dev)
{
	temp_flag = 1;
	return 0;
}

static struct of_device_id odin_tsens_of_match[] = {
	{ .compatible = "lge,odin-thermal", },
	{ /* end of table */ },
};

static SIMPLE_DEV_PM_OPS(odin_tsens_dev_pm_ops,
	odin_tsens_suspend, odin_tsens_resume);

static struct platform_driver odin_tsens_driver = {
	.probe = odin_tsens_probe,
	.remove	= odin_tsens_remove,
	.driver = {
		.name = "odin-thermal",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_tsens_of_match),
		.pm = &odin_tsens_dev_pm_ops,
	},
};

int __init odin_tsens_driver_arch_init(void)
{
	return platform_driver_register(&odin_tsens_driver);
}

static int __init odin_tsens_driver_init(void)
{
	return register_tz_devices();
}
module_init(odin_tsens_driver_init);

static void __exit odin_tsens_driver_exit(void)
{
	platform_driver_unregister(&odin_tsens_driver);
}
module_exit(odin_tsens_driver_exit);

MODULE_DESCRIPTION("ODIN Thermal Sensor Driver");
MODULE_AUTHOR("Changwon Lee <changwon.lee@lge.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:odin-thermal");
