/* CwMcuSensor.c - driver file for HTC SensorHUB
 *
 * Copyright (C) 2014 HTC Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/CwMcuSensor.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/workqueue.h>

#include <linux/regulator/consumer.h>

/*#include <mach/gpiomux.h>*/
#define D(x...) pr_debug("[S_HUB][CW_MCU] " x)
#define I(x...) pr_info("[S_HUB][CW_MCU] " x)
#define E(x...) pr_err("[S_HUB][CW_MCU] " x)

#define RETRY_TIMES 20
#define ACTIVE_RETRY_TIMES 10
#define DPS_MAX			(1 << (16 - 1))
/* ========================================================================= */

/* Input poll interval in milliseconds */
#define CWMCU_POLL_INTERVAL	10
#define CWMCU_POLL_MAX		200
#define CWMCU_POLL_MIN		10
#define TOUCH_LOG_DELAY		5000

/* ========================================================================= */
#define htc_gesture_motion 0x07
#define rel_significant_motion REL_WHEEL
#define htc_any_motion 0x09

#define ACC_CALIBRATOR_LEN 3
#define ACC_CALIBRATOR_RL_LEN 12
#define MAG_CALIBRATOR_LEN 26
#define GYRO_CALIBRATOR_LEN 3
#define LIGHT_CALIBRATOR_LEN 4
#define PRESSURE_CALIBRATOR_LEN 4

#define ENABLE_LIST_GROUP_NUM 4

#define REACTIVATE_PERIOD (10*HZ)

#define USE_WAKE_MCU 1
#define CALIBRATION_DATA_PATH "/calibration_data"
#define G_SENSOR_FLASH_DATA "gs_flash"
#define GYRO_SENSOR_FLASH_DATA "gyro_flash"
#define LIGHT_SENSOR_FLASH_DATA "als_flash"
#define BARO_SENSOR_FLASH_DATA "bs_flash"

#if USE_WAKE_MCU
static int use_wake_mcu;
#endif

static int probe_success;
static int DEBUG_FLAG_GSENSOR;
module_param(DEBUG_FLAG_GSENSOR, int, 0600);
static int DEBUG_FLAG_COMPASS;
module_param(DEBUG_FLAG_COMPASS, int, 0600);
static int DEBUG_FLAG_GYRO;
module_param(DEBUG_FLAG_GYRO, int, 0600);
static int DEBUG_FLAG_PRESSURE;
module_param(DEBUG_FLAG_PRESSURE, int, 0600);
static int DEBUG_FLAG_FUSION;
module_param(DEBUG_FLAG_FUSION, int, 0600);
static int DEBUG_FLAG_MAGNETIC_UNCALIBRATED;
module_param(DEBUG_FLAG_MAGNETIC_UNCALIBRATED, int, 0600);
static int DEBUG_FLAG_GEOMAGNETIC_ROTATION_VECTOR;
module_param(DEBUG_FLAG_GEOMAGNETIC_ROTATION_VECTOR, int, 0600);

static void polling_do_work(struct work_struct *w);
static DECLARE_DELAYED_WORK(polling_work, polling_do_work);

static void resume_do_work(struct work_struct *w);
static DECLARE_WORK(resume_work, resume_do_work);

static void activated_i2c_do_work(struct work_struct *w);
static DECLARE_WORK(activated_i2c_work, activated_i2c_do_work);

struct workqueue_struct *mcu_wq;

struct wake_lock ges_wake_lock;
struct wake_lock significant_wake_lock;

static int power_key_pressed;
struct cwmcu_data {
	struct i2c_client *client;
	atomic_t delay;
	int resume_done;
	struct mutex mutex_lock;
	struct mutex group_i2c_lock;
	struct mutex activated_i2c_lock;
	struct input_polled_dev *input_polled;
	struct input_dev *input;
	struct timeval previous;
	struct timeval now;
	int time_diff;
	struct class *sensor_class;
	struct device *sensor_dev;
	u8	acceleration_axes;
	u8	magnetic_axes;
	u8	gyro_axes;

	u32	enabled_list;

	int	sensors_time[num_sensors];
	int	report_period[num_sensors];
	int IRQ;
	struct delayed_work work;
	struct work_struct irq_work;
	struct input_dev *input_htc_gesture;
	uint32_t gpio_wake_mcu;
	uint32_t gpio_reset;
	uint32_t gpio_chip_mode;
	uint32_t gpio_mcu_irq;
	int gs_chip_layout;
	int gs_kvalue;
	int gs_kvalue_R1;
	int gs_kvalue_R2;
	int gs_kvalue_R3;
	int gs_kvalue_L1;
	int gs_kvalue_L2;
	int gs_kvalue_L3;
	int gy_kvalue;
	int als_kvalue;
	int bs_kvalue;
	int bs_kheader;
	u8  gs_calibrated;
	u8  ls_calibrated;
	u8  bs_calibrated;
	u8  gy_calibrated;

	u8 filter_first_zeros[num_sensors];

	int i2c_total_retry;
	unsigned long i2c_jiffies;
};
static struct cwmcu_data *mcu_data;

static s32 pressure_prior_good_value;

static int CWMCU_i2c_read(struct cwmcu_data *sensor,
			u8 reg_addr, u8 *data, u8 len);
static int CWMCU_i2c_write(struct cwmcu_data *sensor,
			u8 reg_addr, u8 *data, u8 len);

static int cwmcu_get_calibrator_status(u8 sensor_id, u8 *data)
{
	int error_msg = 0;
	if (sensor_id == CW_ACCELERATION)
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_ACC,
				data, 1);
	else if (sensor_id == CW_MAGNETIC)
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_MAG,
				data, 1);
	else if (sensor_id == CW_GYRO)
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_GYRO,
				data, 1);

	return error_msg;
}

static int cwmcu_get_calibrator(u8 sensor_id, s8 *data, u8 len)
{
	int error_msg = 0;

	if ((sensor_id == CW_ACCELERATION) && (len == ACC_CALIBRATOR_LEN))
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_ACC,
				data, len);
	else if ((sensor_id == CW_MAGNETIC) && (len == MAG_CALIBRATOR_LEN))
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_MAG,
				data, len);
	else if ((sensor_id == CW_GYRO) && (len == GYRO_CALIBRATOR_LEN))
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_GYRO,
				data, len);
	else if ((sensor_id == CW_LIGHT) && (len == LIGHT_CALIBRATOR_LEN))
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_LIGHT,
				data, len);
	else if ((sensor_id == CW_PRESSURE) && (len == PRESSURE_CALIBRATOR_LEN))
		error_msg = CWMCU_i2c_read(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_PRESSURE,
				data, len);
	else
		E("%s: invalid arguments, sensor_id = %u, len = %u\n",
		  __func__, sensor_id, len);

	I("sensors_id = %u, calibrator data = (%d, %d, %d)\n", sensor_id,
	  data[0], data[1], data[2]);
	return error_msg;
}

static ssize_t set_calibrator_en(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u8 data;
	u8 data2;
	unsigned long sensors_id;
	int error;

	error = kstrtoul(buf, 10, &sensors_id);
	if (error) {
		E("%s: kstrtoul fails, error = %d\n", __func__, error);
		return error;
	}

	/* sensor_id at least should between 0 ~ 31 */
	data = (u8)sensors_id;
	I("%s: data(sensors_id) = %u\n", __func__, data);

	mutex_lock(&mcu_data->group_i2c_lock);

	switch (data) {
	case 1:
		error = CWMCU_i2c_read(mcu_data, G_SENSORS_STATUS, &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, G_SENSORS_STATUS, &data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 2:
		error = CWMCU_i2c_read(mcu_data, ECOMPASS_SENSORS_STATUS,
				       &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, ECOMPASS_SENSORS_STATUS,
					&data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 4:
		error = CWMCU_i2c_read(mcu_data, GYRO_SENSORS_STATUS,
				       &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, GYRO_SENSORS_STATUS,
					&data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 7:
		error = CWMCU_i2c_read(mcu_data, LIGHT_SENSORS_STATUS,
				       &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, LIGHT_SENSORS_STATUS,
					&data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 9:
		data = 2; /* X- R calibration */
		error = CWMCU_i2c_write(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_TARGET_ACC,
				&data, 1);
		error = CWMCU_i2c_read(mcu_data, G_SENSORS_STATUS, &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, G_SENSORS_STATUS, &data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 10:
		data = 1; /* X+ L calibration */
		error = CWMCU_i2c_write(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_TARGET_ACC,
				&data, 1);
		error = CWMCU_i2c_read(mcu_data, G_SENSORS_STATUS, &data2, 1);
		if (error < 0)
			goto i2c_fail;
		data = data2 | 16;
		error = CWMCU_i2c_write(mcu_data, G_SENSORS_STATUS, &data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	case 11:
		data = 0; /* Z+ */
		error = CWMCU_i2c_write(mcu_data,
				CW_I2C_REG_SENSORS_CALIBRATOR_TARGET_ACC,
				&data, 1);
		if (error < 0)
			goto i2c_fail;
		break;
	default:
		mutex_unlock(&mcu_data->group_i2c_lock);
		E("%s: Improper sensor_id = %u\n", __func__, data);
		return -EINVAL;
	}
	error = count;

i2c_fail:
	mutex_unlock(&mcu_data->group_i2c_lock);
	I("%s--: data2 = 0x%x, error = %d\n", __func__, data2, error);
	return error;
}

static ssize_t sprint_data(char *buf, s8 *data, ssize_t len)
{
	int i;
	char *str_buf;
	char *pstr_buf;
	int rc;

	str_buf = kstrndup(buf, PAGE_SIZE, GFP_KERNEL);
	if (str_buf == NULL) {
		E("%s: cannot allocate buffer\n", __func__);
		return -ENOMEM;
	}
	pstr_buf = str_buf;

	for (i = 0;
	     (pstr_buf < (str_buf + PAGE_SIZE)) && (i < len);
	     i++) {
		rc = scnprintf(pstr_buf,
			       (PAGE_SIZE - (pstr_buf - str_buf)),
			       (i >= (len - 1)) ? "%d\n" : "%d ", data[i]);
		pstr_buf = pstr_buf + rc;
	}
	rc = scnprintf(buf, PAGE_SIZE, "%s", str_buf);
	kfree(str_buf);
	return rc;
}

static ssize_t show_calibrator_status_acc(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	u8 data[6] = {0};

	if (cwmcu_get_calibrator_status(CW_ACCELERATION, data) >= 0)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n", data[0]);

	return scnprintf(buf, PAGE_SIZE, "0x1\n");
}

static ssize_t show_calibrator_status_mag(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	u8 data[6] = {0};

	if (cwmcu_get_calibrator_status(CW_MAGNETIC, data) >= 0)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n", data[0]);

	return scnprintf(buf, PAGE_SIZE, "0x1\n");
}

static ssize_t show_calibrator_status_gyro(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	u8 data[6] = {0};

	if (cwmcu_get_calibrator_status(CW_GYRO, data) >= 0)
		return scnprintf(buf, PAGE_SIZE, "0x%x\n", data[0]);

	return scnprintf(buf, PAGE_SIZE, "0x1\n");
}

static ssize_t set_k_value(const char *buf, size_t count, u8 reg_addr, u8 len)
{
	int i;
	long data_temp[len];
	char *str_buf;
	char *running;
	int error;

	I("%s: count = %lu, strlen(buf) = %lu, PAGE_SIZE = %lu\n",
	  __func__, count, strlen(buf), PAGE_SIZE);

	str_buf = kstrndup(buf, count, GFP_KERNEL);
	if (str_buf == NULL) {
		E("%s: cannot allocate buffer\n", __func__);
		return -ENOMEM;
	}
	running = str_buf;

	for (i = 0; i < len; i++) {
		int error;
		char *token;

		token = strsep(&running, " ");

		if (token == NULL) {
			I("%s: i = %d\n", __func__, i);
			break;
		} else {
			error = kstrtol(token, 10, &data_temp[i]);
			if (error) {
				E("%s: kstrtol fails, error = %d, i = %d\n",
				  __func__, error, i);
				kfree(str_buf);
				return error;
			}
		}
	}
	kfree(str_buf);

	I("Set calibration by attr (%ld, %ld, %ld), len = %u, reg_addr = 0x%x\n"
	  , data_temp[0], data_temp[1], data_temp[2], len, reg_addr);

	mutex_lock(&mcu_data->group_i2c_lock);
	for (i = 0; i < len; i++) {
		u8 data = (u8)(data_temp[i]);
		/* Firmware can't write multi bytes */
		error = CWMCU_i2c_write(mcu_data, reg_addr, &data, 1);
		if (error < 0) {
			E("%s: error = %d, i = %d\n", __func__, error, i);
			mutex_unlock(&mcu_data->group_i2c_lock);
			return -EIO;
		}
	}
	mutex_unlock(&mcu_data->group_i2c_lock);

	return count;
}

static ssize_t set_k_value_acc_f(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return set_k_value(buf, count,
			   CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_ACC,
			   ACC_CALIBRATOR_LEN);
}


static ssize_t set_k_value_mag_f(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return set_k_value(buf, count,
			   CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_MAG,
			   MAG_CALIBRATOR_LEN);
}

static ssize_t set_k_value_gyro_f(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return set_k_value(buf, count,
			   CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_GYRO,
			   GYRO_CALIBRATOR_LEN);
}

static ssize_t set_k_value_barometer_f(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	return set_k_value(buf, count,
			   CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE,
			   PRESSURE_CALIBRATOR_LEN);
}

static ssize_t get_k_value(int type, char *buf, char *data, unsigned len)
{
	if (cwmcu_get_calibrator(type, data, len) < 0)
		memset(data, len, 0);

	return sprint_data(buf, data, len);
}

static ssize_t get_k_value_acc_f(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	u8 data[ACC_CALIBRATOR_LEN];

	return get_k_value(CW_ACCELERATION, buf, data, sizeof(data));
}

static ssize_t get_k_value_acc_rl_f(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int i;
	u8 data[ACC_CALIBRATOR_RL_LEN] = {0};

	if (CWMCU_i2c_read(mcu_data, CW_I2C_REG_SENSORS_CALIBRATOR_RESULT_RL_ACC
			   , data, sizeof(data)) >= 0) {
		for (i = 0; i < sizeof(data); i++) {
			if (DEBUG_FLAG_GSENSOR == 1)
				I("data[%d]: %u\n", i, data[i]);
		}

		mcu_data->gs_kvalue_L1 = ((s8)data[1] << 8) | data[0];
		mcu_data->gs_kvalue_L2 = ((s8)data[3] << 8) | data[2];
		mcu_data->gs_kvalue_L3 = ((s8)data[5] << 8) | data[4];
		mcu_data->gs_kvalue_R1 = ((s8)data[7] << 8) | data[6];
		mcu_data->gs_kvalue_R2 = ((s8)data[9] << 8) | data[8];
		mcu_data->gs_kvalue_R3 = ((s8)data[11] << 8) | data[10];
	}

	return sprint_data(buf, data, sizeof(data));
}

static ssize_t ap_get_k_value_acc_rl_f(struct device *dev,
				      struct device_attribute *attr, char *buf)
{

	return scnprintf(buf, PAGE_SIZE, "%d %d %d %d %d %d\n",
			 (s16)mcu_data->gs_kvalue_L1,
			 (s16)mcu_data->gs_kvalue_L2,
			 (s16)mcu_data->gs_kvalue_L3,
			 (s16)mcu_data->gs_kvalue_R1,
			 (s16)mcu_data->gs_kvalue_R2,
			 (s16)mcu_data->gs_kvalue_R3);
}

static ssize_t get_k_value_mag_f(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	u8 data[MAG_CALIBRATOR_LEN];

	return get_k_value(CW_MAGNETIC, buf, data, sizeof(data));
}

static ssize_t get_k_value_gyro_f(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	u8 data[GYRO_CALIBRATOR_LEN];

	return get_k_value(CW_GYRO, buf, data, sizeof(data));
}

static ssize_t get_k_value_light_f(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	u8 data[LIGHT_CALIBRATOR_LEN] = {0};

	if (cwmcu_get_calibrator(CW_LIGHT, data, sizeof(data)) < 0) {
		E("%s: Get LIGHT Calibrator fails\n", __func__);
		return -EIO;
	}
	return scnprintf(buf, PAGE_SIZE, "%x %x %x %x\n", data[0], data[1],
			 data[2], data[3]);
}

static ssize_t get_k_value_barometer_f(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	u8 data[PRESSURE_CALIBRATOR_LEN];

	return get_k_value(CW_PRESSURE, buf, data, sizeof(data));
}

static ssize_t get_light_kadc(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	u8 data[4] = {0};
	u16 light_gadc;
	u16 light_kadc;

	CWMCU_i2c_read(mcu_data, LIGHT_SENSORS_CALIBRATION_DATA, data,
		       sizeof(data));

	light_gadc = (data[1] << 8) | data[0];
	light_kadc = (data[3] << 8) | data[2];
	return scnprintf(buf, PAGE_SIZE, "gadc = 0x%x, kadc = 0x%x", light_gadc,
			 light_kadc);
}

static ssize_t get_firmware_version(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	u8 firmware_version[6] = {0};

	CWMCU_i2c_read(mcu_data, FIRMWARE_VERSION, firmware_version,
		       sizeof(firmware_version));
	return scnprintf(buf, PAGE_SIZE,
			 "Firmware Architecture version %u, "
			 "Sense version %u, Cywee lib version %u,"
			 " Water number %u"
			 ", Active Engine %u, Project Mapping %u\n",
			 firmware_version[0], firmware_version[1],
			 firmware_version[2], firmware_version[3],
			 firmware_version[4], firmware_version[5]);
}

static ssize_t get_hall_sensor(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	/* TESTME, array 2 change to variable */
	u8 hall_sensor = 0;

	CWMCU_i2c_read(mcu_data, CWSTM32_READ_Hall_Sensor, &hall_sensor, 1);

	return scnprintf(buf, PAGE_SIZE,
			 "Hall_1(S, N) = (%u, %u), Hall_2(S, N)"
			 " = (%u, %u), Hall_3(S, N) = (%u, %u)\n",
			 !!(hall_sensor & 0x1), !!(hall_sensor & 0x2),
			 !!(hall_sensor & 0x4), !!(hall_sensor & 0x8),
			 !!(hall_sensor & 0x10), !!(hall_sensor & 0x20));
}

static ssize_t get_barometer(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	u8 data[6] = {0};

	CWMCU_i2c_read(mcu_data, CWSTM32_READ_Pressure, data, sizeof(data));
	return scnprintf(buf, PAGE_SIZE, "%x %x %x %x\n", data[0], data[1],
					 data[2], data[3]);
}

static ssize_t get_light_polling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	u8 data[3] = {0};
	u8 data_polling_enable;
	u16 light_adc;
	int rc;

	data_polling_enable = CW_MCU_BIT_LIGHT_POLLING;

	mutex_lock(&mcu_data->group_i2c_lock);
	rc = CWMCU_i2c_write(mcu_data, LIGHT_SENSORS_STATUS,
			&data_polling_enable, 1);
	if (rc < 0) {
		E("%s: write fail, rc = %d\n", __func__, rc);
		mutex_unlock(&mcu_data->group_i2c_lock);
		return rc;
	}
	CWMCU_i2c_read(mcu_data, CWSTM32_READ_Light, data, sizeof(data));
	if (rc < 0) {
		E("%s: read fail, rc = %d\n", __func__, rc);
		mutex_unlock(&mcu_data->group_i2c_lock);
		return rc;
	}
	mutex_unlock(&mcu_data->group_i2c_lock);

	/* TESTME for light_adc is changed from array to variable */
	light_adc = (data[2] << 8) | data[1];

	return scnprintf(buf, PAGE_SIZE, "ADC[0x%04X] => level %u\n", light_adc,
					 data[0]);
}


static ssize_t read_mcu_data(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int i;
	u8 reg_addr;
	u8 len;
	long data_temp[2] = {0};
	u8 mcu_rdata[128] = {0};
	char *str_buf;
	char *running;

	str_buf = kstrndup(buf, count, GFP_KERNEL);
	if (str_buf == NULL) {
		E("%s: cannot allocate buffer\n", __func__);
		return -ENOMEM;
	}
	running = str_buf;

	for (i = 0; i < ARRAY_SIZE(data_temp); i++) {
		int error;
		char *token;

		token = strsep(&running, " ");

		if (i == 0)
			error = kstrtol(token, 16, &data_temp[i]);
		else {
			if (token == NULL) {
				data_temp[i] = 1;
				I("%s: token 2 missing\n", __func__);
				break;
			} else
				error = kstrtol(token, 10, &data_temp[i]);
		}
		if (error) {
			E("%s: kstrtol fails, error = %d, i = %d\n",
			  __func__, error, i);
			kfree(str_buf);
			return error;
		}
	}
	kfree(str_buf);

	/* TESTME for changing array to variable */
	reg_addr = (u8)(data_temp[0]);
	len = (u8)(data_temp[1]);

	if (len < sizeof(mcu_rdata)) {
		CWMCU_i2c_read(mcu_data, reg_addr, mcu_rdata, len);
		for (i = 0; i < len; i++)
			I("read mcu reg_addr = 0x%x, reg[%u] = 0x%x\n",
				reg_addr, (reg_addr + i), mcu_rdata[i]);
	} else
		E("%s: len = %u, out of range\n", __func__, len);

	return count;
}


static int CWMCU_i2c_write(struct cwmcu_data *sensor,
			  u8 reg_addr, u8 *data, u8 len)
{
	s32 write_res;
	int i;

	mutex_lock(&mcu_data->activated_i2c_lock);
	if (sensor->i2c_total_retry > RETRY_TIMES) {
		mutex_unlock(&mcu_data->activated_i2c_lock);
		return -EIO;
	}

	#if USE_WAKE_MCU
	if (gpio_get_value(mcu_data->gpio_wake_mcu)) {
		gpio_set_value(mcu_data->gpio_wake_mcu, 0);
		/*E("%s: WAKE_MCU was not LOW, after retry, it's %d\n",
			__func__, gpio_get_value(mcu_data->gpio_wake_mcu));*/
	}
	gpio_set_value(mcu_data->gpio_wake_mcu, 1);
	#endif

	for (i = 0; i < len; i++) {
		for (; sensor->i2c_total_retry <= RETRY_TIMES;) {
			write_res = i2c_smbus_write_byte_data(sensor->client,
						  reg_addr++, data[i]);
			if (write_res < 0) {
				sensor->i2c_total_retry++;
				E(
				  "%s: i2c write error,"
				  " write_res = %d, total_retry = %d\n",
					__func__, write_res,
					sensor->i2c_total_retry);
				continue;
			} else {
				sensor->i2c_total_retry = 0;
				break;
			}
		}

		if (sensor->i2c_total_retry > RETRY_TIMES) {
			E("%s: retry over %d\n", __func__, RETRY_TIMES);
			#if USE_WAKE_MCU
			gpio_set_value(mcu_data->gpio_wake_mcu, 0);
			#endif
			mutex_unlock(&mcu_data->activated_i2c_lock);
			return -EIO;
		}
	}

	#if USE_WAKE_MCU
	gpio_set_value(mcu_data->gpio_wake_mcu, 0);
	#endif

	mutex_unlock(&mcu_data->activated_i2c_lock);

	return 0;
}

static int cwmcu_set_sensor_kvalue(struct cwmcu_data *sensor)
{
	/* Write single Byte because firmware can't write multi bytes now */
	u8 gs_datax = 0, gs_datay = 0, gs_dataz = 0;
	u8 gy_datax = 0, gy_datay = 0, gy_dataz = 0;
	u8 als_goldh = 0x0A;
	u8 als_goldl = 0x38;
	u8 als_datal = 0, als_datah = 0;
	u8 bs_dataa = 0, bs_datab = 0, bs_datac = 0, bs_datad = 0;
	u8 firmware_version[6] = {0};

	sensor->gs_calibrated = 0;
	sensor->gy_calibrated = 0;
	sensor->ls_calibrated = 0;
	sensor->bs_calibrated = 0;

	CWMCU_i2c_read(mcu_data, FIRMWARE_VERSION, firmware_version,
		       sizeof(firmware_version));
	I(
	  "Firmware Architecture version %u, Sense version %u,"
	  " Cywee lib version %u, Water number %u"
	  ", Active Engine %u, Project Mapping %u\n",
		firmware_version[0], firmware_version[1], firmware_version[2],
		firmware_version[3], firmware_version[4], firmware_version[5]);

	if ((sensor->gs_kvalue & (0x67 << 24)) == (0x67 << 24)) {
		gs_datax = (sensor->gs_kvalue >> 16) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_ACC,
				&gs_datax, 1);
		gs_datay = (sensor->gs_kvalue >>  8) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_ACC,
				&gs_datay, 1);
		gs_dataz = (sensor->gs_kvalue) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_ACC,
				&gs_dataz, 1);
		sensor->gs_calibrated = 1;
		I("Set g-sensor kvalue x is %x y is %x z is %x\n",
			gs_datax, gs_datay, gs_dataz);
	}

	if ((sensor->gy_kvalue & (0x67 << 24)) == (0x67 << 24)) {
		gy_datax = (sensor->gy_kvalue >> 16) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_GYRO,
				&gy_datax, 1);
		gy_datay = (sensor->gy_kvalue >>  8) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_GYRO,
				&gy_datay, 1);
		gy_dataz = (sensor->gy_kvalue) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_GYRO,
				&gy_dataz, 1);
		sensor->gy_calibrated = 1;
		I("Set gyro-sensor kvalue x is %x y is %x z is %x\n",
			gy_datax, gy_datay, gy_dataz);
	}

	if ((sensor->als_kvalue & 0x6DA50000) == 0x6DA50000) {
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_LIGHT,
				&als_goldl, 1);
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_LIGHT,
				&als_goldh, 1);
		als_datal = (sensor->als_kvalue) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_LIGHT,
				&als_datal, 1);
		als_datah = (sensor->als_kvalue >>  8) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_LIGHT,
				&als_datah, 1);
		sensor->ls_calibrated = 1;
		I("Set light-sensor kvalue is %x %x\n", als_datah, als_datal);
	}

	if (sensor->bs_kheader == 0x67) {
		bs_dataa = (sensor->bs_kvalue >> 24) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE,
				&bs_dataa, 1);
		bs_datab = (sensor->bs_kvalue >> 16) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE,
				&bs_datab, 1);
		bs_datac = (sensor->bs_kvalue >> 8) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE,
				&bs_datac, 1);
		bs_datad = (sensor->bs_kvalue) & 0xFF;
		CWMCU_i2c_write(sensor,
				CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE,
				&bs_datad, 1);
		sensor->bs_calibrated = 1;
		I("Set baro-sensor kvalue a is %x b is %x c is %x d is %x\n",
			bs_dataa, bs_datab, bs_datac, bs_datad);
	}
	I("Sensor calibration matrix is (gs %u gy %u ls %u bs %u)\n",
		sensor->gs_calibrated, sensor->gy_calibrated,
		sensor->ls_calibrated, sensor->bs_calibrated);
	return 0;
}


static int cwmcu_sensor_placement(struct cwmcu_data *sensor)
{
	I("Set Sensor Placement\n");
	CWMCU_i2c_write(sensor, GENSOR_POSITION, &sensor->acceleration_axes, 1);
	CWMCU_i2c_write(sensor, COMPASS_POSITION, &sensor->magnetic_axes, 1);
	CWMCU_i2c_write(sensor, GYRO_POSITION, &sensor->gyro_axes, 1);

	return 0;
}

static int cwmcu_restore_status(struct cwmcu_data *sensor)
{
	int i, rc;
	u8 data;
	u8 reg_addr = 0;
	u8 reg_value = 0;

	I("Restore status\n");

	for (i = 0; i < ENABLE_LIST_GROUP_NUM; i++) {
		data = (u8)(sensor->enabled_list>>(i*8));
		CWMCU_i2c_write(mcu_data, CWSTM32_ENABLE_REG+i, &data, 1);
		I("%s: write_addr = 0x%x, write_val = 0x%x\n",
		  __func__, (CWSTM32_ENABLE_REG+i), data);
	}

	I("%s: enable_list = 0x%x\n", __func__, sensor->enabled_list);

	for (i = 0; i <= CW_GEOMAGNETIC_ROTATION_VECTOR; i++) {
		if (i <= CW_GYRO)
			reg_addr = (ACCE_UPDATE_RATE + (i * 0x10));
		else if ((i >= CW_ORIENTATION) && (i <= CW_GRAVITY))
			reg_addr = (ORIE_UPDATE_RATE + (i - CW_ORIENTATION));
		else if ((i >= CW_MAGNETIC_UNCALIBRATED) &&
			 (i <= CW_GEOMAGNETIC_ROTATION_VECTOR))
			reg_addr = (MAGN_UNCA_UPDATE_RATE +
				   (i - CW_MAGNETIC_UNCALIBRATED));
		else {
			I("%s: i = %d, no need to restore sample rate\n",
			  __func__, i);
			continue;
		}

		switch (sensor->report_period[i]/1000) {
		case 200:
			reg_value = UPDATE_RATE_NORMAL;
			break;
		case 66:
			reg_value = UPDATE_RATE_UI;
			break;
		case 20:
			reg_value = UPDATE_RATE_GAME;
			break;
		case 10:
		case 16:
			reg_value = UPDATE_RATE_FASTEST;
			break;
		default:
			I(
			  "%s: No need to restore, i = %d,"
			  " report_period = %d\n",
			  __func__, i, sensor->report_period[i]);
			continue;
		}

		I("%s: reg_addr = 0x%x, reg_value = 0x%x\n",
		  __func__, reg_addr, reg_value);
		rc = CWMCU_i2c_write(mcu_data, reg_addr, &reg_value, 1);
		if (rc) {
			E("%s: CWMCU_i2c_write fails, rc = %d, i = %d\n",
			  __func__, rc, i);
			return -EIO;
		}

		I("%s: sensors_id = %ld, delay_us = %6d\n",
		  __func__, i, mcu_data->report_period[i]);
	}

	return 0;
}

static void polling_do_work(struct work_struct *w)
{
	cwmcu_sensor_placement(mcu_data);
	cwmcu_set_sensor_kvalue(mcu_data);
	cwmcu_restore_status(mcu_data);
	cancel_delayed_work(&polling_work);
}


/* Returns the number of read bytes on success */
static int CWMCU_i2c_read(struct cwmcu_data *sensor,
			 u8 reg_addr, u8 *data, u8 len)
{
	s32 rc = 0;

	mutex_lock(&mcu_data->activated_i2c_lock);
	if (sensor->i2c_total_retry > RETRY_TIMES) {
		for (rc = 0; rc < len; rc++)
			data[rc] = 0; /* Assign data to 0 when chip NACK */
		mutex_unlock(&mcu_data->activated_i2c_lock);
		return len;
	}

#if USE_WAKE_MCU
	if (gpio_get_value(mcu_data->gpio_wake_mcu))
		gpio_set_value(mcu_data->gpio_wake_mcu, 0);
	gpio_set_value(mcu_data->gpio_wake_mcu, 1);
#endif
	for (; sensor->i2c_total_retry <= RETRY_TIMES;) {
		rc = i2c_smbus_read_i2c_block_data(sensor->client, reg_addr,
						   len, data);
		if (rc == len) {
			sensor->i2c_total_retry = 0;
			break;
		} else {
			sensor->i2c_total_retry++;
			E("%s: i2c read, rc = %d, total_retry = %d\n", __func__,
			  rc, sensor->i2c_total_retry);
		}
	}

#if USE_WAKE_MCU
	gpio_set_value(mcu_data->gpio_wake_mcu, 0);
#endif

	if (sensor->i2c_total_retry > RETRY_TIMES)
		E("%s: retry over %d\n", __func__, RETRY_TIMES);

	mutex_unlock(&mcu_data->activated_i2c_lock);

	return rc;
}

#if defined(CONFIG_SYNC_TOUCH_STATUS)
int touch_status(u8 status)
{
	int ret = -1;
	if (status == 1 || status == 0) {
		ret = CWMCU_i2c_write(mcu_data, TOUCH_STATUS_REGISTER,
				      &status, 1);
		I("[TP][SensorHub] touch_status = %u\n", status);
	}
	return ret;
}
#endif

static void reset_hub(void)
{
	gpio_direction_output(mcu_data->gpio_reset, 0);
	I("%s: gpio_reset = %d\n", __func__,
	  gpio_get_value_cansleep(mcu_data->gpio_reset));
	usleep_range(10000, 15000);
	gpio_direction_output(mcu_data->gpio_reset, 1);
	I("%s: gpio_reset = %d\n", __func__,
	  gpio_get_value_cansleep(mcu_data->gpio_reset));

	mcu_data->i2c_total_retry = 0;
	mcu_data->i2c_jiffies = jiffies;

	queue_delayed_work(mcu_wq, &polling_work,
			msecs_to_jiffies(5000));
}

static void activated_i2c_do_work(struct work_struct *w)
{
	mutex_lock(&mcu_data->activated_i2c_lock);
	if ((mcu_data->i2c_total_retry > RETRY_TIMES) &&
	    (time_after(jiffies, mcu_data->i2c_jiffies + REACTIVATE_PERIOD))) {
		reset_hub();
	}

	if (mcu_data->i2c_total_retry > RETRY_TIMES) {
		E("%s: i2c_total_retry = %d\n", __func__,
		  mcu_data->i2c_total_retry);
		mutex_unlock(&mcu_data->activated_i2c_lock);
		return;
	}

	/* record the failure */
	mcu_data->i2c_total_retry++;
	mcu_data->i2c_jiffies = jiffies;

	mutex_unlock(&mcu_data->activated_i2c_lock);
	I("%s--: mcu_data->i2c_total_retry = %d\n", __func__,
	  mcu_data->i2c_total_retry);
}

static ssize_t active_set(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	long enabled = 0;
	long sensors_id = 0;
	u8 data;
	u8 i;
	int retry;
	char *str_buf;
	char *running;
	u32 sensors_bit;

	/* Try to recover HUB in low CPU utilization */
	mutex_lock(&mcu_data->activated_i2c_lock);
	if (mcu_data->i2c_total_retry > RETRY_TIMES) {
		I("%s: mcu_data->i2c_total_retry = %d\n", __func__,
		  mcu_data->i2c_total_retry);
		queue_work(mcu_wq, &activated_i2c_work);
	}
	mutex_unlock(&mcu_data->activated_i2c_lock);

	for (retry = 0; retry < ACTIVE_RETRY_TIMES; retry++) {
		mutex_lock(&mcu_data->mutex_lock);
		if (mcu_data->resume_done != 1) {
			mutex_unlock(&mcu_data->mutex_lock);
			I("%s: resume not completed, retry = %d\n",
				__func__, retry);
			usleep_range(5000, 10000);
		} else {
			mutex_unlock(&mcu_data->mutex_lock);
			break;
		}
	}
	if (retry >= ACTIVE_RETRY_TIMES) {
		I("%s: resume not completed, retry = %d, retry fails!\n",
			__func__, retry);
		return -ETIMEDOUT;
	}

	str_buf = kstrndup(buf, count, GFP_KERNEL);
	if (str_buf == NULL) {
		E("%s: cannot allocate buffer\n", __func__);
		return -ENOMEM;
	}
	running = str_buf;

	for (i = 0; i < 2; i++) {
		int error;
		char *token;

		token = strsep(&running, " ");

		if (i == 0)
			error = kstrtol(token, 10, &sensors_id);
		else {
			if (token == NULL) {
				enabled = sensors_id;
				sensors_id = 0;
			} else
				error = kstrtol(token, 10, &enabled);
		}
		if (error) {
			E("%s: kstrtol fails, error = %d, i = %d\n",
				__func__, error, i);
			kfree(str_buf);
			return error;
		}
	}
	kfree(str_buf);

	if (probe_success != 1)
		return -EBUSY;

	if (sensors_id == any_motion && power_key_pressed)
		return count;

	if ((enabled == 1) &&
	    (sensors_id < CW_SENSORS_ID_END) &&
	    (sensors_id >= 0)
	   ) {
		I("%s: Filter first ZEROs, sensors_id = %ld\n",
			__func__, sensors_id);
		mcu_data->filter_first_zeros[sensors_id] = 1;
	}

	if ((sensors_id >= CW_SENSORS_ID_END) ||
	    (sensors_id < 0)
	   ) {
		E("%s: Invalid sensors_id = %ld\n", __func__, sensors_id);
		return -EINVAL;
	}

	sensors_bit = (1 << sensors_id);
	mcu_data->enabled_list &= ~sensors_bit;
	mcu_data->enabled_list |= enabled ? sensors_bit : 0;

	i = sensors_id / 8;
	data = (u8)(mcu_data->enabled_list>>(i*8));
	CWMCU_i2c_write(mcu_data, CWSTM32_ENABLE_REG+i, &data, 1);

	I("%s: sensors_id = %ld, enable = %ld, enable_list = 0x%x\n",
		__func__, sensors_id, enabled, mcu_data->enabled_list);

	return count;
}

static ssize_t active_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", mcu_data->enabled_list);
}

static ssize_t interval_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", CWMCU_POLL_INTERVAL);
}

static ssize_t interval_set(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	long val = 0;
	long sensors_id = 0;
	int rc = 0;
	u8 reg_addr = 0;
	u8 reg_value = 0;
	int i;
	char *str_buf;
	char *running;

	str_buf = kstrndup(buf, count, GFP_KERNEL);
	if (str_buf == NULL) {
		E("%s: cannot allocate buffer\n", __func__);
		return -ENOMEM;
	}
	running = str_buf;

	for (i = 0; i < 2; i++) {
		int error;
		char *token;

		token = strsep(&running, " ");

		if (i == 0)
			error = kstrtol(token, 10, &sensors_id);
		else {
			if (token == NULL) {
				val = 66;
				I("%s: delay set to 66\n", __func__);
			} else
				error = kstrtol(token, 10, &val);
		}
		if (error) {
			E("%s: kstrtol fails, error = %d, i = %d\n",
				__func__, error, i);
			kfree(str_buf);
			return error;
		}
	}
	kfree(str_buf);

	if ((sensors_id < 0) || (sensors_id >= num_sensors)) {
		I("%s: Invalid sensors_id = %ld\n", __func__, sensors_id);
		return -EINVAL;
	}

	if (val < CWMCU_POLL_MIN)
		val = CWMCU_POLL_MIN;

	mcu_data->sensors_time[sensors_id] = 0;
	mcu_data->report_period[sensors_id] = val*1000;

	switch (sensors_id) {
	case CW_ACCELERATION:
		reg_addr = ACCE_UPDATE_RATE;
		break;
	case CW_MAGNETIC:
		reg_addr = MAGN_UPDATE_RATE;
		break;
	case CW_GYRO:
		reg_addr = GYRO_UPDATE_RATE;
		break;
	case CW_ORIENTATION:
		reg_addr = ORIE_UPDATE_RATE;
		break;
	case CW_ROTATIONVECTOR:
		reg_addr = ROTA_UPDATE_RATE;
		break;
	case CW_LINEARACCELERATION:
		reg_addr = LINE_UPDATE_RATE;
		break;
	case CW_GRAVITY:
		reg_addr = GRAV_UPDATE_RATE;
		break;
	case CW_MAGNETIC_UNCALIBRATED:
		reg_addr = MAGN_UNCA_UPDATE_RATE;
		break;
	case CW_GYROSCOPE_UNCALIBRATED:
		reg_addr = GYRO_UNCA_UPDATE_RATE;
		break;
	case CW_GAME_ROTATION_VECTOR:
		reg_addr = GAME_ROTA_UPDATE_RATE;
		break;
	case CW_GEOMAGNETIC_ROTATION_VECTOR:
		reg_addr = GEOM_ROTA_UPDATE_RATE;
		break;
	case CW_SIGNIFICANT_MOTION:
		reg_addr = SIGN_UPDATE_RATE;
		break;
	default:
		reg_addr = 0;
		I(
		  "%s: Only reoprt_period changed, sensors_id = %ld,"
		  " delay_us = %6d\n",
		  __func__, sensors_id,
		  mcu_data->report_period[sensors_id]);
		return count;
	}
	switch (val) {
	case 200:
		reg_value = UPDATE_RATE_NORMAL;
		break;
	case 66:
		reg_value = UPDATE_RATE_UI;
		break;
	case 20:
		reg_value = UPDATE_RATE_GAME;
		break;
	case 10:
	case 16:
		reg_value = UPDATE_RATE_FASTEST;
		break;
	default:
		I(
		  "%s: switch(val): Only reoprt_period changed,"
		  " sensors_id = %ld, delay_us = %6d\n",
		  __func__, sensors_id,
		  mcu_data->report_period[sensors_id]);
		return count;
	}

	D("%s: reg_addr = 0x%x, reg_value = 0x%x\n",
			__func__, reg_addr, reg_value);
	rc = CWMCU_i2c_write(mcu_data, reg_addr, &reg_value, 1);
	if (rc)
		E("%s: CWMCU_i2c_write fails, rc = %d\n", __func__, rc);
	else
		rc = count;

	I("%s: sensors_id = %ld, delay_us = %6d\n",
		__func__, sensors_id, mcu_data->report_period[sensors_id]);

	return rc;
}

static void cwmcu_init_input_device(struct cwmcu_data *sensor,
				    struct input_dev *idev)
{
	idev->name = CWMCU_I2C_NAME;
	idev->id.bustype = BUS_I2C;
	idev->dev.parent = &sensor->client->dev;

	idev->evbit[0] = BIT_MASK(EV_ABS);
	set_bit(EV_ABS, idev->evbit);
	input_set_capability(idev, EV_REL, htc_gesture_motion);
	input_set_capability(idev, EV_REL, rel_significant_motion);
	input_set_capability(idev, EV_REL, htc_any_motion);

	input_set_abs_params(idev, ABS_DISTANCE, 0, 1, 0, 0);

	input_set_abs_params(idev, ABS_ACC_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ACC_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ACC_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAG_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAG_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAG_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAG_ACCURACY, -3, 3, 0, 0);
	input_set_abs_params(idev, ABS_GYRO_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYRO_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYRO_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_LIGHT_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ORI_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ORI_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ORI_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ORI_ACCURACY, -3, 3, 0, 0);
	input_set_abs_params(idev, ABS_ROT_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ROT_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_ROT_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_LIN_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_LIN_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_LIN_Z, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GRA_X, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GRA_Y, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GRA_Z, -DPS_MAX, DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_BIAS_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_BIAS_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_MAGNETIC_UNCALIBRATED_BIAS_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_BIAS_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_BIAS_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GYROSCOPE_UNCALIBRATED_BIAS_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_GAME_ROTATION_VECTOR_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GAME_ROTATION_VECTOR_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GAME_ROTATION_VECTOR_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_GEOMAGNETIC_ROTATION_VECTOR_X, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GEOMAGNETIC_ROTATION_VECTOR_Y, -DPS_MAX,
			     DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_GEOMAGNETIC_ROTATION_VECTOR_Z, -DPS_MAX,
			     DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_STEP_DETECTOR, -DPS_MAX, DPS_MAX, 0, 0);
	input_set_abs_params(idev, ABS_STEP_COUNTER, -DPS_MAX, DPS_MAX, 0, 0);

	input_set_abs_params(idev, ABS_MISC, 0, 9, 0, 0);
}

static void cwmcu_read(struct cwmcu_data *sensor)
{
	if (probe_success != 1) {
		E("%s: probe_success = %d\n", __func__, probe_success);
		return;
	}

	do_gettimeofday(&sensor->now);
	sensor->time_diff = (sensor->now.tv_sec - sensor->previous.tv_sec) *
			    NS_PER_US + (sensor->now.tv_usec -
					 sensor->previous.tv_usec);
	sensor->previous = sensor->now;

	if (DEBUG_FLAG_GSENSOR == 1)
		I("%s: time_diff = %d, enabled_list = 0x%x\n", __func__,
		  sensor->time_diff, sensor->enabled_list);

	if (sensor->enabled_list & (1<<acceleration)) {
		if (DEBUG_FLAG_GSENSOR == 1) {
			I(
			  "%s: sensor->sensors_time[acceleration] = %d,"
			  " sensor->report_period[acceleration] = %d\n",
				__func__, sensor->sensors_time[acceleration],
				sensor->report_period[acceleration]);
		}

		if (sensor->sensors_time[acceleration] >=
		    sensor->report_period[acceleration]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_Acceleration, data,
				       sizeof(data));

			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[acceleration] =
				sensor->sensors_time[acceleration] -
				sensor->report_period[acceleration];

			if ((sensor->filter_first_zeros[acceleration] == 1) &&
			    ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			     (data_buff[2] == -1))
			   ) {
				if (DEBUG_FLAG_GSENSOR == 1)
					I(
					  "%s: Accelerometer(x, y, z) = "
					  "(%d, %d, %d), Filtered\n",
					  __func__, data_buff[0], data_buff[1],
					  data_buff[2]);
			} else {
				input_report_abs(sensor->input, ABS_ACC_X, 1);
				input_report_abs(sensor->input, ABS_ACC_X,
						 data_buff[0]);
				input_report_abs(sensor->input, ABS_ACC_Y,
						 data_buff[1]);
				input_report_abs(sensor->input, ABS_ACC_Z,
						 data_buff[2]);

				if (DEBUG_FLAG_GSENSOR == 1)
					I(
					  "g-sensor datax is %d datay is %d"
					  " dataz is %d\n", data_buff[0],
					  data_buff[1], data_buff[2]);

				input_sync(sensor->input);
				sensor->filter_first_zeros[acceleration] = 0;
			}
		}
		if ((sensor->sensors_time[acceleration] >= 0) &&
		    (sensor->sensors_time[acceleration] <
		     (2 * sensor->report_period[acceleration]))
		   )
			sensor->sensors_time[acceleration] += sensor->time_diff;
		else
			sensor->sensors_time[acceleration] = 0;
	}

	if (sensor->enabled_list & (1<<magnetic)) {
		if (sensor->sensors_time[magnetic] >=
		    sensor->report_period[magnetic]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_Magnetic, data,
				       sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			CWMCU_i2c_read(sensor, CW_I2C_REG_SENSORS_ACCURACY_MAG,
				       data, 1);
			data_buff[3] = (s16)data[0];

			sensor->sensors_time[magnetic] =
				sensor->sensors_time[magnetic] -
				sensor->report_period[magnetic];

			if ((sensor->filter_first_zeros[magnetic] == 1) &&
			    ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			     (data_buff[2] == -1))
			   ) {
				if (DEBUG_FLAG_COMPASS == 1) {
					I(
					  "%s: magnetic(x, y, z) = "
					  "(%d, %d, %d), Filtered\n",
					__func__, data_buff[0],
					data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_MAG_X, 1);
				input_report_abs(sensor->input,
						ABS_MAG_X, data_buff[0]);
				input_report_abs(sensor->input,
						ABS_MAG_Y, data_buff[1]);
				input_report_abs(sensor->input,
						ABS_MAG_Z, data_buff[2]);
				input_report_abs(sensor->input,
						ABS_MAG_ACCURACY, data_buff[3]);

				if (DEBUG_FLAG_COMPASS == 1) {
					I(
					  "compass datax is %d datay is %d "
					  "dataz is %d, Accuracy = %d\n",
						data_buff[0], data_buff[1],
						data_buff[2], data_buff[3]);
				}

				input_sync(sensor->input);
				sensor->filter_first_zeros[magnetic] = 0;
			}
		}
		if ((sensor->sensors_time[magnetic] >= 0) &&
		    (sensor->sensors_time[magnetic] <
		     (2 * sensor->report_period[magnetic]))
		   )
			sensor->sensors_time[magnetic] += sensor->time_diff;
		else
			sensor->sensors_time[magnetic] = 0;
	}

	if (sensor->enabled_list & (1<<gyro)) {
		if (sensor->sensors_time[gyro] >= sensor->report_period[gyro]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_Gyro, data,
				       sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[gyro] =
				sensor->sensors_time[gyro] -
				sensor->report_period[gyro];

			if ((sensor->filter_first_zeros[gyro] == 1) &&
			    ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			     (data_buff[2] == -1))
			   ) {
				if (DEBUG_FLAG_GYRO == 1) {
					I(
					  "%s: gyro(x, y, z) = (%d, %d, %d),"
					  " Filtered\n",
					  __func__, data_buff[0], data_buff[1],
					  data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_GYRO_X, 1);
				input_report_abs(sensor->input, ABS_GYRO_X,
						 data_buff[0]);
				input_report_abs(sensor->input, ABS_GYRO_Y,
						 data_buff[1]);
				input_report_abs(sensor->input, ABS_GYRO_Z,
						 data_buff[2]);

				if (DEBUG_FLAG_GYRO == 1)
					I(
					  "gyro datax is %d datay is %d "
					  "dataz is %d\n", data_buff[0],
					  data_buff[1], data_buff[2]);

				input_sync(sensor->input);
				sensor->filter_first_zeros[gyro] = 0;
			}
		}
		if ((sensor->sensors_time[gyro] >= 0) &&
		    (sensor->sensors_time[gyro] <
		     (2 * sensor->report_period[gyro]))
		   )
			sensor->sensors_time[gyro] += sensor->time_diff;
		else
			sensor->sensors_time[gyro] = 0;
	}

	if (sensor->enabled_list & (1<<pressure)) {
		if (sensor->sensors_time[pressure] >=
		    sensor->report_period[pressure]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};
			s32 data_buff_ps;

			CWMCU_i2c_read(sensor, CWSTM32_READ_Pressure, data,
				       sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			/* Workaround for missing third byte */
			data_buff_ps = (s32)(((u32)data[3] << 24) |
					     ((u32)data[2] << 16) |
					     ((u32)data[1] << 8) |
					     ((u32)data[0]));
			if (abs(data_buff_ps - pressure_prior_good_value) >
			    10000) {
				CWMCU_i2c_read(sensor, CWSTM32_READ_Pressure,
					       data, 6);
				data_buff[0] = ((s8)data[1] << 8) | data[0];
				data_buff[1] = ((s8)data[3] << 8) | data[2];
				data_buff[2] = ((s8)data[5] << 8) | data[4];
				pressure_prior_good_value = (s32)(((u32)data[3]
								   << 24) |
								  ((u32)data[2]
								   << 16) |
								  ((u32)data[1]
								   << 8) |
								  ((u32)data[0])
								 );
				if (DEBUG_FLAG_PRESSURE == 1)
					I(
					  "[S_HUB][CW_MCU] Wrong barometer "
					  "value is found! Last = %d,"
					  " Retry = %d\n", data_buff_ps,
					  pressure_prior_good_value);
			} else
				pressure_prior_good_value = data_buff_ps;

			sensor->sensors_time[pressure] =
				sensor->sensors_time[pressure] -
				sensor->report_period[pressure];
			input_report_abs(sensor->input, ABS_PRESSURE_Z, 1);
			input_report_abs(sensor->input, ABS_PRESSURE_X,
					 pressure_prior_good_value);
			input_report_abs(sensor->input, ABS_PRESSURE_Y,
					 data_buff[1]);
			input_report_abs(sensor->input, ABS_PRESSURE_Z,
					 data_buff[2]);

			if (DEBUG_FLAG_PRESSURE == 1)
				I(
				  "pressure datax = %x, datay = %x, dataz = %x,"
				  " pressure_prior_good_value = %x\n",
				  data_buff[0], data_buff[1], data_buff[2],
				  pressure_prior_good_value);

			input_sync(sensor->input);
		}
		if ((sensor->sensors_time[pressure] >= 0) &&
		    (sensor->sensors_time[pressure] <
		     (2 * sensor->report_period[pressure]))
		   )
			sensor->sensors_time[pressure] += sensor->time_diff;
		else
			sensor->sensors_time[pressure] = 0;
	}

	if (sensor->enabled_list & (1<<orientation)) {
		if (sensor->sensors_time[orientation] >=
		    sensor->report_period[orientation]) {
			u8 data[6] = {0};
			s16 data_buff[4] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_Orientation, data,
				       sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			CWMCU_i2c_read(sensor, CW_I2C_REG_SENSORS_ACCURACY_MAG,
				       data, 1);
			data_buff[3] = (s16)data[0];

			sensor->sensors_time[orientation] =
				sensor->sensors_time[orientation] -
				sensor->report_period[orientation];

			if ((sensor->filter_first_zeros[orientation] == 1) &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0) &&
			      (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			      (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: orientation(0, 1, 2) = "
					  "(%d, %d, %d), Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_ORI_X, 1);
				input_report_abs(sensor->input,
					ABS_ORI_X, data_buff[0]);
				input_report_abs(sensor->input,
					ABS_ORI_Y, data_buff[1]);
				input_report_abs(sensor->input,
					ABS_ORI_Z, data_buff[2]);
				input_report_abs(sensor->input,
					ABS_ORI_ACCURACY, data_buff[3]);
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "orientation datax is %d datay is "
					  "%d dataz is %d, Accuracy = %d\n",
						data_buff[0], data_buff[1],
						data_buff[2], data_buff[3]);
				}

				input_sync(sensor->input);
				sensor->filter_first_zeros[orientation] = 0;
			}
		}
		if ((sensor->sensors_time[orientation] >= 0) &&
		    (sensor->sensors_time[orientation] <
		     (2 * sensor->report_period[orientation]))
		   )
			sensor->sensors_time[orientation] += sensor->time_diff;
		else
			sensor->sensors_time[orientation] = 0;
	}

	if (sensor->enabled_list & (1<<rotation_vector)) {
		if (sensor->sensors_time[rotation_vector] >=
		    sensor->report_period[rotation_vector]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_RotationVector,
				       data, sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[rotation_vector] =
				sensor->sensors_time[rotation_vector] -
				sensor->report_period[rotation_vector];

			if ((sensor->filter_first_zeros[rotation_vector] == 1)
			    &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0) &&
			      (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			      (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: rotation_vector(0, 1, 2) = "
					  "(%d, %d, %d), Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_ROT_X, 1);
				input_report_abs(sensor->input, ABS_ROT_X,
						 data_buff[0]);
				input_report_abs(sensor->input, ABS_ROT_Y,
						 data_buff[1]);
				input_report_abs(sensor->input, ABS_ROT_Z,
						 data_buff[2]);
				if (DEBUG_FLAG_FUSION == 1)
					I(
					  "rotationvector datax is %d datay is"
					  " %d dataz is %d\n", data_buff[0],
					  data_buff[1], data_buff[2]);

				input_sync(sensor->input);
				sensor->filter_first_zeros[rotation_vector] = 0;
			}
		}
		if ((sensor->sensors_time[rotation_vector] >= 0) &&
		    (sensor->sensors_time[rotation_vector] <
		     (2 * sensor->report_period[rotation_vector]))
		   )
			sensor->sensors_time[rotation_vector] +=
						sensor->time_diff;
		else
			sensor->sensors_time[rotation_vector] = 0;
	}

	if (sensor->enabled_list & (1<<linear_acceleration)) {
		if (sensor->sensors_time[linear_acceleration] >=
		    sensor->report_period[linear_acceleration]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_LinearAcceleration,
				       data, sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[linear_acceleration] =
				sensor->sensors_time[linear_acceleration] -
				sensor->report_period[linear_acceleration];

			if ((sensor->filter_first_zeros[linear_acceleration] ==
			     1) &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0) &&
			      (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			      (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: linear_acceleration(0, 1, 2) = "
					  "(%d, %d, %d), Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_LIN_X, 1);
				input_report_abs(sensor->input, ABS_LIN_X,
						 data_buff[0]);
				input_report_abs(sensor->input, ABS_LIN_Y,
						 data_buff[1]);
				input_report_abs(sensor->input, ABS_LIN_Z,
						 data_buff[2]);

				if (DEBUG_FLAG_FUSION == 1)
					I(
					  "linearacceleration datax is %d"
					  " datay is %d dataz is %d\n",
					  data_buff[0], data_buff[1],
					  data_buff[2]);

				input_sync(sensor->input);
				sensor->filter_first_zeros[linear_acceleration]
						= 0;
			}
		}
		if ((sensor->sensors_time[linear_acceleration] >= 0) &&
		    (sensor->sensors_time[linear_acceleration] <
		     (2 * sensor->report_period[linear_acceleration]))
		   )
			sensor->sensors_time[linear_acceleration] +=
				sensor->time_diff;
		else
			sensor->sensors_time[linear_acceleration] = 0;
	}

	if (sensor->enabled_list & (1<<gravity)) {
		if (sensor->sensors_time[gravity] >=
		    sensor->report_period[gravity]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_Gravity, data,
				       sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[gravity] =
				sensor->sensors_time[gravity] -
				sensor->report_period[gravity];

			if ((sensor->filter_first_zeros[gravity] == 1) &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0) &&
			      (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			      (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: gravity(0, 1, 2) = (%d, %d, %d),"
					  " Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input, ABS_GRA_X, 1);
				input_report_abs(sensor->input, ABS_GRA_X,
						 data_buff[0]);
				input_report_abs(sensor->input, ABS_GRA_Y,
						 data_buff[1]);
				input_report_abs(sensor->input, ABS_GRA_Z,
						 data_buff[2]);

				if (DEBUG_FLAG_FUSION == 1)
					I(
					  "gravity datax is %d datay is %d"
					  " dataz is %d\n", data_buff[0],
					  data_buff[1], data_buff[2]);

				input_sync(sensor->input);
				sensor->filter_first_zeros[gravity] = 0;
			}
		}
		if ((sensor->sensors_time[gravity] >= 0) &&
		    (sensor->sensors_time[gravity] <
		     (2 * sensor->report_period[gravity]))
		   )
			sensor->sensors_time[gravity] += sensor->time_diff;
		else
			sensor->sensors_time[gravity] = 0;
	}

	if (sensor->enabled_list & (1 << magnetic_uncalibrated)) {
		if (sensor->sensors_time[magnetic_uncalibrated] >=
		    sensor->report_period[magnetic_uncalibrated]) {
			u8 data[12] = {0};
			s16 data_buff[6] = {0};

			CWMCU_i2c_read(sensor,
				CWSTM32_READ_MAGNETIC_UNCALIBRATED, data,
				sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];
			data_buff[3] = ((s8)data[7] << 8) | data[6];
			data_buff[4] = ((s8)data[9] << 8) | data[8];
			data_buff[5] = ((s8)data[11] << 8) | data[10];
			sensor->sensors_time[magnetic_uncalibrated] =
				sensor->sensors_time[magnetic_uncalibrated] -
				sensor->report_period[magnetic_uncalibrated];

			if ((sensor->filter_first_zeros[magnetic_uncalibrated]
			     == 1) &&
			    (data_buff[0] == 0) &&
			    (data_buff[1] == 0) &&
			    (data_buff[2] == 0) &&
			    (data_buff[3] == 0) &&
			    (data_buff[4] == 0) &&
			    (data_buff[5] == 0)) {
				if (DEBUG_FLAG_MAGNETIC_UNCALIBRATED == 1) {
					I(
					  "%s: Magn_Uncalib(0, 1, 2, 3, 4, 5)"
					  " = (%d, %d, %d, %d, %d, %d),"
					  " Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2],
					  data_buff[3], data_buff[4],
					  data_buff[5]);
				}
			} else {
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_X, 1);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_X,
					data_buff[0]);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_Y,
					data_buff[1]);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_Z,
					data_buff[2]);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_BIAS_X,
					data_buff[3]);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_BIAS_Y,
					data_buff[4]);
				input_report_abs(sensor->input,
					ABS_MAGNETIC_UNCALIBRATED_BIAS_Z,
					data_buff[5]);
				input_sync(sensor->input);
				sensor->filter_first_zeros
					[magnetic_uncalibrated] = 0;

				if (DEBUG_FLAG_MAGNETIC_UNCALIBRATED == 1) {
					I(
					  "%s: data_buff(0, 1, 2, 3, 4, 5) = "
					  "(%d, %d, %d, %d, %d, %d), "
					  "read address = 0x%X\n",
						__func__, data_buff[0],
						data_buff[1], data_buff[2],
						data_buff[3], data_buff[4],
						data_buff[5],
					CWSTM32_READ_MAGNETIC_UNCALIBRATED);
				}
			}
		}
		if ((sensor->sensors_time[magnetic_uncalibrated] >= 0) &&
		    (sensor->sensors_time[magnetic_uncalibrated] <
		     (2 * sensor->report_period[magnetic_uncalibrated]))
		   )
			sensor->sensors_time[magnetic_uncalibrated] +=
				sensor->time_diff;
		else
			sensor->sensors_time[magnetic_uncalibrated] = 0;
	}

	if (sensor->enabled_list & (1 << gyroscope_uncalibrated)) {
		if (sensor->sensors_time[gyroscope_uncalibrated] >=
		    sensor->report_period[gyroscope_uncalibrated]) {
			u8 data[12] = {0};
			s16 data_buff[6] = {0};

			CWMCU_i2c_read(sensor,
				CWSTM32_READ_GYROSCOPE_UNCALIBRATED,
				data, sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];
			data_buff[3] = ((s8)data[7] << 8) | data[6];
			data_buff[4] = ((s8)data[9] << 8) | data[8];
			data_buff[5] = ((s8)data[11] << 8) | data[10];
			sensor->sensors_time[gyroscope_uncalibrated] =
				sensor->sensors_time[gyroscope_uncalibrated] -
				sensor->report_period[gyroscope_uncalibrated];

			if ((sensor->filter_first_zeros[gyroscope_uncalibrated]
			     == 1) &&
			    (data_buff[0] == 0) &&
			    (data_buff[1] == 0) &&
			    (data_buff[2] == 0) &&
			    (data_buff[3] == 0) &&
			    (data_buff[4] == 0) &&
			    (data_buff[5] == 0)) {
				D(
				  "%s: Gyro_Uncalib(0, 1, 2, 3, 4, 5) ="
				  " (%d, %d, %d, %d, %d, %d), Filtered\n",
						__func__, data_buff[0],
						data_buff[1], data_buff[2],
						data_buff[3], data_buff[4],
						data_buff[5]);
			} else {
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_X, 1);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_X,
					data_buff[0]);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_Y,
					data_buff[1]);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_Z,
					data_buff[2]);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_BIAS_X,
					data_buff[3]);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_BIAS_Y,
					data_buff[4]);
				input_report_abs(sensor->input,
					ABS_GYROSCOPE_UNCALIBRATED_BIAS_Z,
					data_buff[5]);
				input_sync(sensor->input);
				sensor->filter_first_zeros
					[gyroscope_uncalibrated] = 0;
			}
		}
		if ((sensor->sensors_time[gyroscope_uncalibrated] >= 0) &&
		    (sensor->sensors_time[gyroscope_uncalibrated] <
		     (2 * sensor->report_period[gyroscope_uncalibrated]))
		   )
			sensor->sensors_time[gyroscope_uncalibrated] +=
				sensor->time_diff;
		else
			sensor->sensors_time[gyroscope_uncalibrated] = 0;
	}

	if (sensor->enabled_list & (1<<game_rotation_vector)) {
		if (sensor->sensors_time[game_rotation_vector] >=
		    sensor->report_period[game_rotation_vector]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor, CWSTM32_READ_GAME_ROTATION_VECTOR
				       , data, sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];
			sensor->sensors_time[game_rotation_vector] =
				sensor->sensors_time[game_rotation_vector] -
				sensor->report_period[game_rotation_vector];

			if ((sensor->filter_first_zeros[game_rotation_vector]
			     == 1) &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0) &&
			      (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1) &&
			      (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: Game_RV(0, 1, 2) = (%d, %d, %d),"
					  " Filtered\n",
					  __func__, data_buff[0], data_buff[1],
					  data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input,
					ABS_GAME_ROTATION_VECTOR_X, 1);
				input_report_abs(sensor->input,
					ABS_GAME_ROTATION_VECTOR_X,
					data_buff[0]);
				input_report_abs(sensor->input,
					ABS_GAME_ROTATION_VECTOR_Y,
					data_buff[1]);
				input_report_abs(sensor->input,
					ABS_GAME_ROTATION_VECTOR_Z,
					data_buff[2]);
				input_sync(sensor->input);
				sensor->filter_first_zeros[game_rotation_vector]
							= 0;
			}
		}
		if ((sensor->sensors_time[game_rotation_vector] >= 0) &&
		    (sensor->sensors_time[game_rotation_vector] <
		     (2 * sensor->report_period[game_rotation_vector]))
		   )
			sensor->sensors_time[game_rotation_vector] +=
				sensor->time_diff;
		else
			sensor->sensors_time[game_rotation_vector] = 0;
	}

	if (sensor->enabled_list & (1<<geomagnetic_rotation_vector)) {
		if (sensor->sensors_time
			    [geomagnetic_rotation_vector] >=
			    sensor->report_period
				    [geomagnetic_rotation_vector]) {
			u8 data[6] = {0};
			s16 data_buff[3] = {0};

			CWMCU_i2c_read(sensor,
				CWSTM32_READ_GEOMAGNETIC_ROTATION_VECTOR,
				data, sizeof(data));
			data_buff[0] = ((s8)data[1] << 8) | data[0];
			data_buff[1] = ((s8)data[3] << 8) | data[2];
			data_buff[2] = ((s8)data[5] << 8) | data[4];

			sensor->sensors_time[geomagnetic_rotation_vector] =
				sensor->sensors_time
					[geomagnetic_rotation_vector]
					- sensor->report_period
						  [geomagnetic_rotation_vector];

			if ((sensor->filter_first_zeros
				     [geomagnetic_rotation_vector] == 1) &&
			    (
			     ((data_buff[0] == 0) && (data_buff[1] == 0)
			      && (data_buff[2] == 0))
			     ||
			     ((data_buff[0] == -1) && (data_buff[1] == -1)
			      && (data_buff[2] == -1))
			    )
			   ) {
				if (DEBUG_FLAG_FUSION == 1) {
					I(
					  "%s: Geo_RV(0, 1, 2) = (%d, %d, %d),"
					  " Filtered\n",
					  __func__, data_buff[0],
					  data_buff[1], data_buff[2]);
				}
			} else {
				input_report_abs(sensor->input,
					ABS_GEOMAGNETIC_ROTATION_VECTOR_X, 1);
				input_report_abs(sensor->input,
					ABS_GEOMAGNETIC_ROTATION_VECTOR_X,
					data_buff[0]);
				input_report_abs(sensor->input,
					ABS_GEOMAGNETIC_ROTATION_VECTOR_Y,
					data_buff[1]);
				input_report_abs(sensor->input,
					ABS_GEOMAGNETIC_ROTATION_VECTOR_Z,
					data_buff[2]);
				input_sync(sensor->input);
				sensor->filter_first_zeros
					[geomagnetic_rotation_vector] = 0;
				if (DEBUG_FLAG_GEOMAGNETIC_ROTATION_VECTOR
				    == 1) {
					I(
					  "%s: Geo_RV(0, 1, 2) = (%d, %d, %d),"
					  " read address = 0x%X\n",
				      __func__, data_buff[0],
				      data_buff[1], data_buff[2],
				      CWSTM32_READ_GEOMAGNETIC_ROTATION_VECTOR);
				}
			}

		}
		if ((sensor->sensors_time[geomagnetic_rotation_vector] >= 0) &&
		    (sensor->sensors_time[geomagnetic_rotation_vector] <
		     (2 * sensor->report_period[geomagnetic_rotation_vector]))
		   )
			sensor->sensors_time[geomagnetic_rotation_vector] +=
						sensor->time_diff;
		else
			sensor->sensors_time[geomagnetic_rotation_vector] = 0;
	}

}

static void cwmcu_poll(struct input_polled_dev *dev)
{
	cwmcu_read(dev->private);
}

static int cwmcu_open(struct cwmcu_data *sensor)
{
	int error;

	error = pm_runtime_get_sync(&sensor->client->dev);
	if (error && error != -ENOSYS)
		return error;

	return 0;
}

static void cwmcu_close(struct cwmcu_data *sensor)
{
	pm_runtime_put_sync(&sensor->client->dev);
}

static void cwmcu_poll_open(struct input_polled_dev *ipoll_dev)
{
	struct cwmcu_data *sensor = ipoll_dev->private;

	cwmcu_open(sensor);
}

static void cwmcu_poll_close(struct input_polled_dev *ipoll_dev)
{
	struct cwmcu_data *sensor = ipoll_dev->private;

	cwmcu_close(sensor);
}

static int cwmcu_register_polled_device(struct cwmcu_data *sensor)
{
	struct input_polled_dev *ipoll_dev;
	int error;

	ipoll_dev = input_allocate_polled_device();
	if (!ipoll_dev)
		return -ENOMEM;

	ipoll_dev->private = sensor;
	ipoll_dev->open = cwmcu_poll_open;
	ipoll_dev->close = cwmcu_poll_close;
	ipoll_dev->poll = cwmcu_poll;
	ipoll_dev->poll_interval = CWMCU_POLL_INTERVAL;
	ipoll_dev->poll_interval_min = CWMCU_POLL_MIN;
	ipoll_dev->poll_interval_max = CWMCU_POLL_MAX;

	cwmcu_init_input_device(sensor, ipoll_dev->input);

	error = input_register_polled_device(ipoll_dev);
	if (error) {
		input_free_polled_device(ipoll_dev);
		return error;
	}

	sensor->input_polled = ipoll_dev;
	sensor->input = ipoll_dev->input;

	return 0;
}


static int cwmcu_suspend(struct device *dev)
{
#if USE_WAKE_MCU
	u8 data = 0x00;
#endif

	I("[CWMCU] %s\n", __func__);

#if USE_WAKE_MCU
	use_wake_mcu = 1;
	CWMCU_i2c_write(mcu_data, HTC_SYSTEM_STATUS_REG, &data, 1);
	gpio_set_value(mcu_data->gpio_wake_mcu, 0);
#endif
	mutex_lock(&mcu_data->mutex_lock);
	mcu_data->resume_done = 0;
	mutex_unlock(&mcu_data->mutex_lock);

	return 0;
}

static void resume_do_work(struct work_struct *w)
{
	u8 data = 0x01;

	I("[CWMCU] %s++\n", __func__);

	/*gpio_set_value(mcu_data->gpio_wake_mcu, 1);*/
	CWMCU_i2c_write(mcu_data, HTC_SYSTEM_STATUS_REG, &data, 1);

#if USE_WAKE_MCU
	use_wake_mcu = 0;
#endif
	mutex_lock(&mcu_data->mutex_lock);
	mcu_data->resume_done = 1;
	mutex_unlock(&mcu_data->mutex_lock);

	I("[CWMCU] %s--\n", __func__);
}

static int cwmcu_resume(struct device *dev)
{
	I("[CWMCU] %s++\n", __func__);

#if USE_WAKE_MCU
	queue_work(mcu_wq, &resume_work);
	/*CWMCU_i2c_write(mcu_data, HTC_SYSTEM_STATUS_REG, &data, 1);
	use_wake_mcu = 0;
	gpio_set_value(mcu_data->gpio_wake_mcu, 1);*/
#endif
	I("[CWMCU] %s--\n", __func__);
	return 0;
}


static void cwmcu_irq_work_func(struct work_struct *work)
{
	struct cwmcu_data *sensor = container_of((struct work_struct *)work,
						 struct cwmcu_data, irq_work);
	s32 ret;
	u8 data[127] = {0};
	s16 data_buff[3] = {0};
	int retry;
	s32 data_event = 0;
	u8 INT_st1, INT_st2, INT_st3, INT_st4, ERR_st;
	u8 clear_intr = 0xFF;
	u16 light_adc = 0;

	D("[CWMCU] %s\n", __func__);

	/*gpio_set_value(mcu_data->gpio_wake_mcu, 1); */ /* wake MCU up */

	if (sensor->input == NULL) {
		E("[CWMCU] sensor->input == NULL\n");
		return;
	}

	CWMCU_i2c_read(sensor, CWSTM32_INT_ST1, &INT_st1, 1);
	CWMCU_i2c_read(sensor, CWSTM32_INT_ST2, &INT_st2, 1);
	CWMCU_i2c_read(sensor, CWSTM32_INT_ST3, &INT_st3, 1);
	CWMCU_i2c_read(sensor, CWSTM32_INT_ST4, &INT_st4, 1);
	CWMCU_i2c_read(sensor, CWSTM32_ERR_ST, &ERR_st, 1);

	D("%s: INT_st(1, 2, 3, 4) = (0x%x, 0x%x, 0x%x, 0x%x), ERR_st = 0x%x\n",
		__func__, INT_st1, INT_st2, INT_st3, INT_st4, ERR_st);

	/* INT_st1: bit 3 */
	if (INT_st1 & CW_MCU_INT_BIT_LIGHT) {
		if (sensor->enabled_list & (1<<light)) {
			CWMCU_i2c_read(sensor, CWSTM32_READ_Light, data, 3);
			if (data[0] < 11) {
				sensor->sensors_time[light] =
						sensor->sensors_time[light] -
						sensor->report_period[light];
				light_adc = (data[2] << 8) | data[1];

				input_report_abs(sensor->input, ABS_MISC,
						 data[0]);
				input_sync(sensor->input);

				I(
				  "light interrupt occur value is %u, adc "
				  "is %x ls_calibration is %u\n",
					data[0], light_adc,
					sensor->ls_calibrated);
			} else {
				I(
				  "light interrupt occur value is %u, adc is"
				  " %x ls_calibration is %u (message only)\n",
					data[0], light_adc,
					sensor->ls_calibrated);
			}
		}
		if (data[0] < 11) {
			clear_intr = CW_MCU_INT_BIT_LIGHT;
			CWMCU_i2c_write(sensor, CWSTM32_INT_ST1, &clear_intr,
					1);
		}
	}

	/* INT_st3: bit 4 */
	if (INT_st3 & CW_MCU_INT_BIT_SIGNIFICANT_MOTION) {
		if (sensor->enabled_list & (1<<significant_motion)) {
			sensor->sensors_time[significant_motion] = 0;

			wake_lock_timeout(&significant_wake_lock, 1 * HZ);
			input_report_rel(sensor->input,	rel_significant_motion,
					 1);
			input_sync(sensor->input);

			I("%s: Significant Motion interrupt occurs!!\n",
					__func__);
		}
		clear_intr = CW_MCU_INT_BIT_SIGNIFICANT_MOTION;
		CWMCU_i2c_write(sensor, CWSTM32_INT_ST3, &clear_intr, 1);
	}

	/* INT_st3: bit 5 */
	if (INT_st3 & CW_MCU_INT_BIT_STEP_DETECTOR) {
		if (sensor->enabled_list & (1<<step_detector)) {
			ret = CWMCU_i2c_read(sensor, CWSTM32_READ_STEP_DETECTOR,
					     data, 1);
			if (ret >= 0) {
				sensor->sensors_time[step_detector] = 0;

				input_report_abs(sensor->input,
						ABS_STEP_DETECTOR, -1);
				input_report_abs(sensor->input,
						ABS_STEP_DETECTOR, data[0]);
				input_sync(sensor->input);

				I("%s: Step Detector INT, timestamp = %u\n",
						__func__, data[0]);
			} else
				I("%s: Step Detector i2c read fail, ret = %d\n",
						__func__, ret);

		}
		clear_intr = CW_MCU_INT_BIT_STEP_DETECTOR;
		CWMCU_i2c_write(sensor, CWSTM32_INT_ST3, &clear_intr, 1);
	}

	/* INT_st3: bit 6 */
	if (INT_st3 & CW_MCU_INT_BIT_STEP_COUNTER) {
		if (sensor->enabled_list & (1<<step_counter)) {
			ret = CWMCU_i2c_read(sensor, CWSTM32_READ_STEP_COUNTER,
					     data, 1);
			if (ret >= 0) {
				sensor->sensors_time[step_counter] = 0;

				input_report_abs(sensor->input,
						ABS_STEP_COUNTER, -1);
				input_report_abs(sensor->input,
						ABS_STEP_COUNTER, data[0]);
				input_sync(sensor->input);

				I("%s: Step Counter interrupt, step = %u\n",
						__func__, data[0]);
			} else
				I("%s: Step Counter i2c read fails, ret = %d\n",
						__func__, ret);

		}
		clear_intr = CW_MCU_INT_BIT_STEP_COUNTER;
		CWMCU_i2c_write(sensor, CWSTM32_INT_ST3, &clear_intr, 1);
	}

	/* INT_st4: bit 0 */
	if (INT_st4 & CW_MCU_INT_BIT_HTC_GESTURE_MOTION) {
		for (retry = 0; retry < RETRY_TIMES; retry++) {
			ret = i2c_smbus_read_i2c_block_data(sensor->client,
					CWSTM32_READ_Gesture_Motion, 6, data);
			if (ret == 6)
				break;

			mdelay(5);
		}
		I("[CWMCU] i2c bus read %d bytes\n", ret);
		data_event = (s32)((data[0] & 0x1F) | (((data[1] |
						(data[2] << 8)) & 0x3FF) << 5) |
						(data[3] << 15) |
						(data[4] << 23));

		sensor->sensors_time[gesture_motion] = 0;
		input_report_rel(sensor->input, htc_gesture_motion, data_event);
		input_sync(sensor->input);
		I("gesture_motion: df0: %d, d0: %u, d1: %u\n", data_buff[0],
				data[0], data[1]);
		I("gesture_motion: data_buff: %d, data_event: %d\n",
				data_buff[1], data_event);

		clear_intr = CW_MCU_INT_BIT_HTC_GESTURE_MOTION;
		CWMCU_i2c_write(sensor, CWSTM32_INT_ST4, &clear_intr, 1);
	}

	/* INT_st4: bit 7 */
	if (INT_st4 & CW_MCU_INT_BIT_ANY_MOTION) {
		if (sensor->enabled_list & (1<<any_motion)) {
			for (retry = 0; retry < RETRY_TIMES; retry++) {
				ret = i2c_smbus_read_i2c_block_data(
						sensor->client,
						CWSTM32_READ_Any_Motion, 1,
						data);
				if (ret == 1)
					break;

				mdelay(5);
			}
			sensor->sensors_time[any_motion] = 0;
			input_report_rel(sensor->input,	htc_any_motion,
					 data[0]);
			input_sync(sensor->input);
			I("[CWMCU] any_motion: %u\n", data[0]);
		}
		clear_intr = CW_MCU_INT_BIT_ANY_MOTION;
		CWMCU_i2c_write(sensor, CWSTM32_INT_ST4, &clear_intr, 1);
	}

	/* ERR_st: bit 7 */
	if (ERR_st & CW_MCU_INT_BIT_ERROR_WATCHDOG_RESET) {
		D("[CWMCU] Watch Dog Reset \n");
		msleep(5);

		mutex_lock(&mcu_data->activated_i2c_lock);
		mcu_data->i2c_total_retry = RETRY_TIMES + 1;
		mutex_unlock(&mcu_data->activated_i2c_lock);

		queue_work(mcu_wq, &activated_i2c_work);

		clear_intr = CW_MCU_INT_BIT_ERROR_WATCHDOG_RESET;
		ret = CWMCU_i2c_write(sensor, CWSTM32_ERR_ST, &clear_intr, 1);
	}
	enable_irq(sensor->IRQ);
}

static irqreturn_t cwmcu_irq_handler(int irq, void *handle)
{
	struct cwmcu_data *data = handle;

	if (probe_success != 1) {
		I("%s: probe not completed\n", __func__);
		return IRQ_HANDLED;
	}

	if ((data == NULL) || (data->client == NULL))
		return IRQ_HANDLED;

	disable_irq_nosync(data->IRQ);

	wake_lock_timeout(&ges_wake_lock, 2 * HZ);
	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

static int mcu_parse_dt(struct device *dev, struct cwmcu_data *pdata)
{
	struct property *prop;
	struct device_node *dt = dev->of_node;
	u32 buf = 0;
	struct device_node *g_sensor_offset;
	int g_sensor_cali_size = 0;
	unsigned char *g_sensor_cali_data = NULL;
	struct device_node *gyro_sensor_offset;
	int gyro_sensor_cali_size = 0;
	unsigned char *gyro_sensor_cali_data = NULL;
	struct device_node *light_sensor_offset = NULL;
	int light_sensor_cali_size = 0;
	unsigned char *light_sensor_cali_data = NULL;
	struct device_node *baro_sensor_offset;
	int baro_sensor_cali_size = 0;
	unsigned char *baro_sensor_cali_data = NULL;

	int i;

	if (pdata == NULL) {
		I("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}
	pdata->gs_kvalue = 0;
	g_sensor_offset = of_find_node_by_path(CALIBRATION_DATA_PATH);
	if (g_sensor_offset) {
		g_sensor_cali_data = (unsigned char *)
				     of_get_property(g_sensor_offset,
						     G_SENSOR_FLASH_DATA,
						     &g_sensor_cali_size);
		I("%s: cali_size = %d\n", __func__, g_sensor_cali_size);
		if (g_sensor_cali_data) {
			for (i = 0; (i < g_sensor_cali_size) && (i < 4); i++) {
				I("g sensor cali_data[%d] = %02x\n", i,
						g_sensor_cali_data[i]);
				pdata->gs_kvalue |= (g_sensor_cali_data[i] <<
						    (i * 8));
			}
		}

	} else
		I("%s: G-sensor Calibration data offset not found\n", __func__);

	pdata->gs_kvalue_L1 = 0;
	pdata->gs_kvalue_L2 = 0;
	pdata->gs_kvalue_L3 = 0;
	pdata->gs_kvalue_R1 = 0;
	pdata->gs_kvalue_R2 = 0;
	pdata->gs_kvalue_R3 = 0;
	pdata->gy_kvalue = 0;

	gyro_sensor_offset = of_find_node_by_path(CALIBRATION_DATA_PATH);
	if (gyro_sensor_offset) {
		gyro_sensor_cali_data = (unsigned char *)
					of_get_property(gyro_sensor_offset,
							GYRO_SENSOR_FLASH_DATA,
							&gyro_sensor_cali_size);
		I("%s:gyro cali_size = %d\n", __func__, gyro_sensor_cali_size);
		if (gyro_sensor_cali_data) {
			for (i = 0; (i < gyro_sensor_cali_size) && (i < 4);
				     i++) {
				I("gyro sensor cali_data[%d] = %02x\n", i,
					gyro_sensor_cali_data[i]);
				pdata->gy_kvalue |= (gyro_sensor_cali_data[i] <<
						     (i * 8));
			}
			pdata->gs_kvalue_L1 = (gyro_sensor_cali_data[5] << 8) |
						gyro_sensor_cali_data[4];
			I("g sensor cali_data L1 = %x\n", pdata->gs_kvalue_L1);
			pdata->gs_kvalue_L2 = (gyro_sensor_cali_data[7] << 8) |
						gyro_sensor_cali_data[6];
			I("g sensor cali_data L2 = %x\n", pdata->gs_kvalue_L2);
			pdata->gs_kvalue_L3 = (gyro_sensor_cali_data[9] << 8) |
						gyro_sensor_cali_data[8];
			I("g sensor cali_data L3 = %x\n", pdata->gs_kvalue_L3);
			pdata->gs_kvalue_R1 = (gyro_sensor_cali_data[11] << 8) |
						gyro_sensor_cali_data[10];
			I("g sensor cali_data R1 = %x\n", pdata->gs_kvalue_R1);
			pdata->gs_kvalue_R2 = (gyro_sensor_cali_data[13] << 8) |
						gyro_sensor_cali_data[12];
			I("g sensor cali_data R2 = %x\n", pdata->gs_kvalue_R2);
			pdata->gs_kvalue_R3 = (gyro_sensor_cali_data[15] << 8) |
						gyro_sensor_cali_data[14];
			I("g sensor cali_data R3 = %x\n", pdata->gs_kvalue_R3);
		}

	} else
		I("%s: GYRO-sensor Calibration data offset not found\n",
				__func__);

	pdata->als_kvalue = 0;
	light_sensor_offset = of_find_node_by_path(CALIBRATION_DATA_PATH);
	if (light_sensor_offset) {
		light_sensor_cali_data = (unsigned char *)
					 of_get_property(light_sensor_offset,
						       LIGHT_SENSOR_FLASH_DATA,
						       &light_sensor_cali_size);
		I("%s:light cali_size = %d\n", __func__,
				light_sensor_cali_size);
		if (light_sensor_cali_data) {
			for (i = 0; (i < light_sensor_cali_size) && (i < 4);
			     i++) {
				I("light sensor cali_data[%d] = %02x\n", i,
					light_sensor_cali_data[i]);
				pdata->als_kvalue |=
					(light_sensor_cali_data[i] << (i * 8));
			}
		}
	} else
		I("%s: LIGHT-sensor Calibration data offset not found\n",
			__func__);

	pdata->bs_kvalue = 0;
	baro_sensor_offset = of_find_node_by_path(CALIBRATION_DATA_PATH);
	if (baro_sensor_offset) {
		baro_sensor_cali_data = (unsigned char *)
					of_get_property(baro_sensor_offset,
							BARO_SENSOR_FLASH_DATA,
							&baro_sensor_cali_size);
		I("%s: cali_size = %d\n", __func__, baro_sensor_cali_size);
		if (baro_sensor_cali_data) {
			for (i = 0; (i < baro_sensor_cali_size) &&
			     (i < 5); i++) {
				I("baro sensor cali_data[%d] = %02x\n", i,
					baro_sensor_cali_data[i]);
				if (i == baro_sensor_cali_size - 1)
					pdata->bs_kheader =
						baro_sensor_cali_data[i];
				else
					pdata->bs_kvalue |=
						(baro_sensor_cali_data[i] <<
						(i * 8));
			}
		}
	} else
		I("%s: Barometer-sensor Calibration data offset not found\n",
			__func__);

	pdata->gpio_wake_mcu = of_get_named_gpio(dt, "mcu,Cpu_wake_mcu-gpio",
					0);
	if (!gpio_is_valid(pdata->gpio_wake_mcu))
		I("DT:gpio_wake_mcu value is not valid\n");
	else
		I("DT:gpio_wake_mcu=%d\n", pdata->gpio_wake_mcu);

	pdata->gpio_mcu_irq = of_get_named_gpio(dt, "mcu,intr-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_mcu_irq))
		I("DT:gpio_mcu_irq value is not valid\n");
	else
		I("DT:gpio_mcu_irq=%d\n", pdata->gpio_mcu_irq);

	pdata->gpio_reset = of_get_named_gpio(dt, "mcu,Reset-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset))
		I("DT:gpio_reset value is not valid\n");
	else
		I("DT:gpio_reset=%d\n", pdata->gpio_reset);

	pdata->gpio_chip_mode = of_get_named_gpio(dt, "mcu,Chip_mode-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_chip_mode))
		I("DT:gpio_chip_mode value is not valid\n");
	else
		I("DT:gpio_chip_mode=%d\n", pdata->gpio_chip_mode);

	prop = of_find_property(dt, "mcu,gs_chip_layout", NULL);
	if (prop) {
		of_property_read_u32(dt, "mcu,gs_chip_layout", &buf);
		pdata->gs_chip_layout = buf;
		I("%s: chip_layout = %d\n", __func__, pdata->gs_chip_layout);
	} else
		I("%s: g_sensor,chip_layout not found", __func__);

	prop = of_find_property(dt, "mcu,acceleration_axes", NULL);
	if (prop) {
		of_property_read_u32(dt, "mcu,acceleration_axes", &buf);
		pdata->acceleration_axes = buf;
		I("%s: acceleration axes = %u\n", __func__,
			pdata->acceleration_axes);
	} else
		I("%s: g_sensor axes not found", __func__);

	prop = of_find_property(dt, "mcu,magnetic_axes", NULL);
	if (prop) {
		of_property_read_u32(dt, "mcu,magnetic_axes", &buf);
		pdata->magnetic_axes = buf;
		I("%s: Compass axes = %u\n", __func__, pdata->magnetic_axes);
	} else
		I("%s: Compass axes not found", __func__);

	prop = of_find_property(dt, "mcu,gyro_axes", NULL);
	if (prop) {
		of_property_read_u32(dt, "mcu,gyro_axes", &buf);
		pdata->gyro_axes = buf;
		I("%s: gyro axes = %u\n", __func__, pdata->gyro_axes);
	} else
		I("%s: gyro axes not found", __func__);

	return 0;
}

static ssize_t bma250_clear_powerkey_pressed(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val = 0;
	int error;

	error = kstrtoul(buf, 10, &val);
	if (error)
		return error;

	power_key_pressed = !!val;

	return count;
}

static ssize_t bma250_get_powerkry_pressed(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", power_key_pressed);
}
static DEVICE_ATTR(clear_powerkey_flag, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP,
		bma250_get_powerkry_pressed, bma250_clear_powerkey_pressed);

static struct device_attribute attributes[] = {

	__ATTR(enable, 0666, active_show,
			active_set),
	__ATTR(delay_ms, 0666, interval_show,
			interval_set),
	__ATTR(calibrator_en, 0220, NULL, set_calibrator_en),
	__ATTR(calibrator_status_acc, 0440, show_calibrator_status_acc, NULL),
	__ATTR(calibrator_status_mag, 0440, show_calibrator_status_mag, NULL),
	__ATTR(calibrator_status_gyro, 0440, show_calibrator_status_gyro, NULL),
	__ATTR(calibrator_data_acc, 0666, get_k_value_acc_f, set_k_value_acc_f),
	__ATTR(calibrator_data_acc_rl, 0440, get_k_value_acc_rl_f, NULL),
	__ATTR(ap_calibrator_data_acc_rl, 0440, ap_get_k_value_acc_rl_f, NULL),
	__ATTR(calibrator_data_mag, 0666, get_k_value_mag_f, set_k_value_mag_f),
	__ATTR(calibrator_data_gyro, 0666, get_k_value_gyro_f,
			set_k_value_gyro_f),
	__ATTR(calibrator_data_light, 0440, get_k_value_light_f, NULL),
	__ATTR(calibrator_data_barometer, 0666, get_k_value_barometer_f,
			set_k_value_barometer_f),
	__ATTR(data_barometer, 0440, get_barometer, NULL),
	__ATTR(data_light_polling, 0440, get_light_polling, NULL),
	__ATTR(sensor_hub_rdata, 0220, NULL, read_mcu_data),
	__ATTR(data_light_kadc, 0440, get_light_kadc, NULL),
	__ATTR(firmware_version, 0440, get_firmware_version, NULL),
	__ATTR(hall_sensor, 0440, get_hall_sensor, NULL),
};


static int create_sysfs_interfaces(struct cwmcu_data *sensor)
{
	int i;
	struct class *bma250_powerkey_class;
	struct device *bma250_powerkey_dev;
	int res;

	sensor->sensor_class = class_create(THIS_MODULE, "htc_sensorhub");
	if (sensor->sensor_class == NULL)
		goto custom_class_error;

	sensor->sensor_dev = device_create(sensor->sensor_class, NULL, 0, "%s",
					   "sensor_hub");
	if (sensor->sensor_dev == NULL)
		goto custom_class_error;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(sensor->sensor_dev, attributes + i))
			goto error;

	bma250_powerkey_class = class_create(THIS_MODULE, "bma250_powerkey");
	if (IS_ERR(bma250_powerkey_class)) {
		res = PTR_ERR(bma250_powerkey_class);
		bma250_powerkey_class = NULL;
		E("%s: could not allocate bma250_powerkey_class, res = %d\n",
			__func__, res);
		goto error;
	}

	bma250_powerkey_dev = device_create(bma250_powerkey_class,
				NULL, 0, "%s", "bma250");
	res = device_create_file(bma250_powerkey_dev,
				 &dev_attr_clear_powerkey_flag);
	if (res) {
		E("%s, create bma250_device_create_file fail!\n", __func__);
		goto err_create_bma250_device_file;
	}

	return 0;

err_create_bma250_device_file:
	class_destroy(bma250_powerkey_class);
error:
	for (; i >= 0; i--)
		device_remove_file(sensor->sensor_dev, attributes + i);

custom_class_error:
	dev_err(&sensor->client->dev, "%s:Unable to create interface\n",
		__func__);
	return -1;
}

static int CWMCU_i2c_probe(struct i2c_client *client,
				       const struct i2c_device_id *id)
{
	struct cwmcu_data *sensor;
	int error;
	int i;

	I("%s++:\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		return -EIO;
	}

	sensor = kzalloc(sizeof(struct cwmcu_data), GFP_KERNEL);
	if (!sensor) {
		I("castor: kzalloc error\n");
		return -ENOMEM;
	}
	mcu_data = sensor;

	mutex_init(&sensor->mutex_lock);
	mutex_init(&sensor->group_i2c_lock);
	mutex_init(&sensor->activated_i2c_lock);

	sensor->client = client;
	i2c_set_clientdata(client, sensor);

	error = cwmcu_register_polled_device(sensor);
	if (error) {
		I("castor: cwmcu_register_polled_device error\n");
		goto err_free_mem;
	}

	error = create_sysfs_interfaces(sensor);

	if (error)
		goto exit_free_input;

	for (i = 0; i < num_sensors; i++) {
		sensor->sensors_time[i] = 0;
		sensor->report_period[i] = 200000;
	}

	if (client->dev.of_node) {
		D("Device Tree parsing.");

		error = mcu_parse_dt(&client->dev, sensor);
		if (error) {
			dev_err(&client->dev,
				"%s: mcu_parse_dt for pdata failed. err = %d\n"
					, __func__, error);
			goto exit_mcu_parse_dt_fail;
		}
	} else {
		if (client->dev.platform_data != NULL) {
			sensor->acceleration_axes =
				((struct cwmcu_platform_data *)
				 sensor->client->dev.platform_data)
				->acceleration_axes;
			sensor->magnetic_axes =
				((struct cwmcu_platform_data *)
				 sensor->client->dev.platform_data)
				->magnetic_axes;
			sensor->gyro_axes =
				((struct cwmcu_platform_data *)
				 sensor->client->dev.platform_data)
				->gyro_axes;
			sensor->gpio_wake_mcu =
				((struct cwmcu_platform_data *)
				 sensor->client->dev.platform_data)
				->gpio_wake_mcu;
		}
	}

	wake_lock_init(&ges_wake_lock, WAKE_LOCK_SUSPEND, "ges_wake_lock");
	wake_lock_init(&significant_wake_lock, WAKE_LOCK_SUSPEND,
		       "significant_wake_lock");

	INIT_WORK(&sensor->irq_work, cwmcu_irq_work_func);

	atomic_set(&sensor->delay, CWMCU_MAX_DELAY);

	mcu_wq = create_singlethread_workqueue("htc_mcu");
	i2c_set_clientdata(client, sensor);
	pm_runtime_enable(&client->dev);

	error = gpio_request(sensor->gpio_reset, "cwmcu_reset");
	if (error)
		I("%s : request reset gpio fail\n", __func__);

	error = gpio_request(sensor->gpio_wake_mcu, "cwmcu_CPU2MCU");
	if (error)
		I("%s : request gpio_wake_mcu gpio fail\n", __func__);

	error = gpio_request(sensor->gpio_chip_mode, "cwmcu_hub_boot_mode");
	if (error)
		I("%s : request ghip mode gpio fail\n", __func__);

	gpio_direction_output(mcu_data->gpio_reset, 1);
	gpio_direction_output(mcu_data->gpio_wake_mcu, 1);
	gpio_direction_output(mcu_data->gpio_chip_mode, 0);

	error = gpio_request(sensor->gpio_mcu_irq, "cwmcu_int");
	if (error) {
		I("%s : request irq gpio fail\n",
			__func__);
	}
	client->irq = gpio_to_irq(sensor->gpio_mcu_irq);

	sensor->IRQ = client->irq;
	I("Requesting irq = %d\n", sensor->IRQ);
	error = request_irq(sensor->IRQ, cwmcu_irq_handler, IRQF_TRIGGER_RISING,
				"cwmcu", sensor);
	if (error)
		I("[CWMCU] could not request irq %d\n", error);
	error = enable_irq_wake(sensor->IRQ);
	if (error < 0)
		I("[CWMCU] could not enable irq as wakeup source %d\n", error);

	mutex_lock(&sensor->mutex_lock);
	sensor->resume_done = 1;
	mutex_unlock(&sensor->mutex_lock);

	for (i = 0; i < num_sensors; i++)
		sensor->filter_first_zeros[i] = 0;

	queue_delayed_work(mcu_wq, &polling_work,
			msecs_to_jiffies(5000));

	/*vib_trigger_register_simple("vibrator", &vib_trigger);*/
	probe_success = 1;
	I("CWMCU_i2c_probe success!\n");

	return 0;

exit_free_input:
	if (sensor && (sensor->input))
		input_free_device(sensor->input);
	if (sensor && (sensor->input_polled)) {
		input_unregister_polled_device(sensor->input_polled);
		input_free_polled_device(sensor->input_polled);
	}
exit_mcu_parse_dt_fail:
	if (client->dev.of_node &&
	    ((struct cwmcu_platform_data *)sensor->client->dev.platform_data))
		kfree(sensor->client->dev.platform_data);
err_free_mem:
	kfree(sensor);
	return error;
}


static int CWMCU_i2c_remove(struct i2c_client *client)
{
	struct cwmcu_data *sensor = i2c_get_clientdata(client);

	if (sensor && (sensor->input_polled)) {
		input_unregister_polled_device(sensor->input_polled);
		input_free_polled_device(sensor->input_polled);
	}

	wake_lock_destroy(&ges_wake_lock);
	wake_lock_destroy(&significant_wake_lock);
	kfree(sensor);
	return 0;
}

static const struct dev_pm_ops cwmcu_pm_ops = {
	.suspend = cwmcu_suspend,
	.resume = cwmcu_resume
};


static const struct i2c_device_id cwmcu_id[] = {
	{CWMCU_I2C_NAME, 0},
	{ }
};
#ifdef CONFIG_OF
static struct of_device_id mcu_match_table[] = {
	{.compatible = "htc_mcu" },
	{},
};
#else
#define mcu_match_table NULL
#endif

MODULE_DEVICE_TABLE(i2c, cwmcu_id);

static struct i2c_driver cwmcu_driver = {
	.driver = {
		.name = CWMCU_I2C_NAME,
		   .owner = THIS_MODULE,
		.pm = &cwmcu_pm_ops,
		.of_match_table = mcu_match_table,
	},
	.probe    = CWMCU_i2c_probe,
	.remove   = /*__devexit_p(*/CWMCU_i2c_remove/*)*/,
	.id_table = cwmcu_id,
};

static int __init CWMCU_i2c_init(void)
{
	return i2c_add_driver(&cwmcu_driver);
}
module_init(CWMCU_i2c_init);

static void __exit CWMCU_i2c_exit(void)
{
	i2c_del_driver(&cwmcu_driver);
}
module_exit(CWMCU_i2c_exit);

MODULE_DESCRIPTION("CWMCU I2C Bus Driver V1.6");
MODULE_AUTHOR("CyWee Group Ltd.");
MODULE_LICENSE("GPL");
