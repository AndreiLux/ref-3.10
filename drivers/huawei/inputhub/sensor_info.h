/*
 *  drivers/misc/inputhub/inputhub_route.h
 *  Sensor Hub Channel driver
 *
 *  Copyright (C) 2012 Huawei, Inc.
 *  Author: qindiwen <inputhub@huawei.com>
 *
 */
#ifndef	__SENSORS_H__
#define	__SENSORS_H__

#define PDC_SIZE		27

enum sensor_name {
	ACC,
	MAG,
	GYRO,
	ALS,
	PS,
	SENSOR_MAX
};

typedef enum
{
	SUC=1,
	EXEC_FAIL,
	NV_FAIL,
	COMMU_FAIL
}RET_TYPE;

struct i2c_data{
	uint8_t bus_num;
	uint8_t i2c_address;
	uint16_t reg_address;
	uint8_t reg_type;
	uint8_t reg_len;
	uint8_t data[MAX_I2C_DATA_LENGTH];
};

struct sensor_status
{
	int status[TAG_SENSOR_END];//record whether sensor is in activate status, already opened and setdelay
	int delay[TAG_SENSOR_END];//record sensor delay time
	int opened[TAG_SENSOR_END];//record whether sensor was opened
	char gyro_selfTest_result[5];
	char mag_selfTest_result[5];
};
struct g_sensor_platform_data{
    uint8_t i2c_address;
    uint8_t axis_map_x;
    uint8_t axis_map_y;
    uint8_t axis_map_z;
    uint8_t negate_x;
    uint8_t negate_y;
    uint8_t negate_z;
    uint8_t gpio_int1;
    uint8_t gpio_int2;
    uint16_t poll_interval;
    int offset_x;
    int offset_y;
    int offset_z;
};
struct gyro_platform_data{
    uint8_t exist;
    uint8_t i2c_address;
    uint8_t axis_map_x;
    uint8_t axis_map_y;
    uint8_t axis_map_z;
    uint8_t negate_x;
    uint8_t negate_y;
    uint8_t negate_z;
    uint8_t gpio_int1;
    uint8_t gpio_int2;
    uint16_t poll_interval;
};
struct compass_platform_data {
	uint8_t i2c_address;
	uint8_t axis_map_x;
	uint8_t axis_map_y;
	uint8_t axis_map_z;
	uint8_t negate_x;
	uint8_t negate_y;
	uint8_t negate_z;
	uint8_t outbit;
	uint8_t gpio_drdy;
	uint8_t gpio_rst;
	uint8_t pdc_data[PDC_SIZE];
	uint16_t poll_interval;
};
struct als_platform_data{
	uint8_t i2c_address;
	uint8_t gpio_int1;
	uint8_t atime;
	uint8_t again;
	uint16_t poll_interval;
	uint16_t init_time;
	int threshold_value;
	int GA1;
	int GA2;
	int GA3;
};

struct ps_platform_data{
	uint8_t i2c_address;
	uint8_t ps_pulse_count;
	uint8_t gpio_int1;
	uint8_t persistent;
	uint8_t ptime;
	uint8_t p_on;  //need to close oscillator
	uint16_t poll_interval;
	uint16_t init_time;
	int min_proximity_value;
	int pwindows_value;
	int pwave_value;
	int threshold_value;
	int rdata_under_sun;
};

extern void update_sensor_info(const pkt_header_t *pkt);
extern void disable_sensors_when_suspend(void);
extern void enable_sensors_when_resume(void);
#endif	/* __SENSORS_H__ */
