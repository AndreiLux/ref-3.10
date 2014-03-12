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

#ifndef __CWMCUSENSOR_H__
#define __CWMCUSENSOR_H__
#include <linux/ioctl.h>

#define CWMCU_I2C_NAME "CwMcuSensor"

enum ABS_status {
	ABS_ACC_X = 0x01,
	ABS_ACC_Y = 0x02,
	ABS_ACC_Z = 0x03,
	ABS_MAG_X = 0x04,
	ABS_MAG_Y = 0x05,
	ABS_MAG_Z = 0x06,
	ABS_GYRO_X = 0x07,
	ABS_GYRO_Y = 0x08,
	ABS_GYRO_Z = 0x09,
	/*ABS_LIGHT_X = 0x0A,*/
	ABS_MAG_ACCURACY = 0x0A,
	/*ABS_LIGHT_Y = 0x0B,*/
	ABS_ORI_ACCURACY = 0x0B,
	ABS_LIGHT_Z = 0x0C,
	ABS_GEOMAGNETIC_ROTATION_VECTOR_X = 0x0D,
	ABS_GEOMAGNETIC_ROTATION_VECTOR_Y = 0x0E,
	ABS_GEOMAGNETIC_ROTATION_VECTOR_Z = 0x0F,
	ABS_PRESSURE_X = 0x10,
	ABS_PRESSURE_Y = 0x11,
	ABS_PRESSURE_Z = 0x12,
	ABS_ORI_X = 0x13,
	ABS_ORI_Y = 0x14,
	ABS_ORI_Z = 0x15,
	ABS_ROT_X = 0x16,
	ABS_ROT_Y = 0x17,
	ABS_ROT_Z = 0x18,
	ABS_LIN_X = 0x1A, /* Jump over ABS_DISTANCE */
	ABS_LIN_Y = 0x1B,
	ABS_LIN_Z = 0x1C,
	ABS_GRA_X = 0x1D,
	ABS_GRA_Y = 0x1E,
	ABS_GRA_Z = 0x1F,
	ABS_STEP_DETECTOR = 0x23,
	ABS_STEP_COUNTER = 0x24,
	ABS_GESTURE_MOTION = 0x26,
	ABS_MAGNETIC_UNCALIBRATED_X = 0x30, /* Jump oveer ABS_MT_SLOT, Sensor HAL cannot receive */
	ABS_MAGNETIC_UNCALIBRATED_Y = 0x31,
	ABS_MAGNETIC_UNCALIBRATED_Z = 0x32,
	ABS_MAGNETIC_UNCALIBRATED_BIAS_X = 0x3F, /* Jump over ABS_MT_WIDTH_MINOR, this cause little white point, and touch fails*/
	ABS_MAGNETIC_UNCALIBRATED_BIAS_Y = 0x34,
	ABS_MAGNETIC_UNCALIBRATED_BIAS_Z = 0x35,
	ABS_GYROSCOPE_UNCALIBRATED_X = 0x36,
	ABS_GYROSCOPE_UNCALIBRATED_Y = 0x37,
	ABS_GYROSCOPE_UNCALIBRATED_Z = 0x38,
	ABS_GYROSCOPE_UNCALIBRATED_BIAS_X = 0x39,
	ABS_GYROSCOPE_UNCALIBRATED_BIAS_Y = 0x3A,
	ABS_GYROSCOPE_UNCALIBRATED_BIAS_Z = 0x3B,
	ABS_GAME_ROTATION_VECTOR_X = 0x3C,
	ABS_GAME_ROTATION_VECTOR_Y = 0x3D,
	ABS_GAME_ROTATION_VECTOR_Z = 0x3E,
};


typedef enum {
	CW_ACCELERATION                = 0,
	CW_MAGNETIC                    = 1,
	CW_GYRO                        = 2,
	CW_LIGHT                       = 3,
	CW_PRESSURE                    = 5,
	CW_ORIENTATION                 = 6,
	CW_ROTATIONVECTOR              = 7,
	CW_LINEARACCELERATION          = 8,
	CW_GRAVITY                     = 9,
	CW_MAGNETIC_UNCALIBRATED       = 16,
	CW_GYROSCOPE_UNCALIBRATED      = 17,
	CW_GAME_ROTATION_VECTOR        = 18,
	CW_GEOMAGNETIC_ROTATION_VECTOR = 19,
	CW_SIGNIFICANT_MOTION          = 20,
	CW_STEP_DETECTOR               = 21,
	CW_STEP_COUNTER                = 22,
	HTC_GESTURE_MOTION             = 24,
	HTC_ANY_MOTION                 = 28,
	CW_SENSORS_ID_END /* Be careful, do not exceed 31 */
} CW_SENSORS_ID;

#define NS_PER_US 1000000

#define FIRMWARE_VERSION 0x10

#define CWSTM32_READ_Touch_Log                     0xF1
#define CWSTM32_READ_Dump_Backup_Call_Stack_Buffer 0xFB
#define CWSTM32_READ_Dump_Call_Stack_Buffer        0xFD
#define CWSTM32_READ_Dump_Debug_Buffer             0xFE
#define CWSTM32_READ_Debug_Status                  0xFF
#define HTC_SYSTEM_STATUS_REG                      0x1E

#if defined(CONFIG_SYNC_TOUCH_STATUS)
int touch_status(u8 status);
#define TOUCH_STATUS_REGISTER                      0xF2
#endif

#define CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_ACC                0x60
#define CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_ACC              0x68
#define CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_ACC              0x68
#define CW_I2C_REG_SENSORS_CALIBRATOR_TARGET_ACC	        0x69
#define CW_I2C_REG_SENSORS_CALIBRATOR_RESULT_RL_ACC             0x6A

#define CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_MAG                0x70
#define CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_MAG              0x78
#define CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_MAG              0x78
#define CW_I2C_REG_SENSORS_ACCURACY_MAG                         0x79


#define CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_GYRO               0x80
#define CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_GYRO             0x88
#define CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_GYRO             0x88

#define CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_LIGHT              0x90
#define CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_LIGHT            0x98
#define CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_LIGHT            0x98

#define CW_I2C_REG_SENSORS_CALIBRATOR_STATUS_PRESSURE           0xB0
#define CW_I2C_REG_SENSORS_CALIBRATOR_GET_DATA_PRESSURE         0xB8
#define CW_I2C_REG_SENSORS_CALIBRATOR_SET_DATA_PRESSURE         0xB8

#define CWMCU_MAX_DELAY		2000

#define G_SENSORS_STATUS                                        0x60
#define ACCE_UPDATE_RATE                                        0x66
#define ECOMPASS_SENSORS_STATUS                                 0x70
#define MAGN_UPDATE_RATE                                        0x76
#define GYRO_SENSORS_STATUS                                     0x80
#define GYRO_UPDATE_RATE                                        0x86
#define LIGHT_SENSORS_STATUS                                    0x90
#define LIGHT_SENSORS_CALIBRATION_DATA				0x98

#define ORIE_UPDATE_RATE                                        0xC0
#define ROTA_UPDATE_RATE                                        0xC1
#define LINE_UPDATE_RATE                                        0xC2
#define GRAV_UPDATE_RATE                                        0xC3
#define MAGN_UNCA_UPDATE_RATE                                   0xC4
#define GYRO_UNCA_UPDATE_RATE                                   0xC5
#define GAME_ROTA_UPDATE_RATE                                   0xC6
#define GEOM_ROTA_UPDATE_RATE                                   0xC7
#define SIGN_UPDATE_RATE                                        0xC8

#define UPDATE_RATE_NORMAL              1
#define UPDATE_RATE_UI                  2
#define UPDATE_RATE_GAME                3
#define UPDATE_RATE_FASTEST             4

#define GENSOR_POSITION			0x65
#define COMPASS_POSITION		0x75
#define GYRO_POSITION			0x85
#define standardbase			0
#define	acceleration			CW_ACCELERATION
#define	magnetic			CW_MAGNETIC
#define	gyro				CW_GYRO
#define	light				CW_LIGHT
#define	pressure			CW_PRESSURE
#define	orientation			CW_ORIENTATION
#define	rotation_vector			CW_ROTATIONVECTOR
#define	linear_acceleration		CW_LINEARACCELERATION
#define	gravity				CW_GRAVITY

#define	gesture_motion			HTC_GESTURE_MOTION
#define	any_motion			HTC_ANY_MOTION

#define magnetic_uncalibrated		CW_MAGNETIC_UNCALIBRATED
#define gyroscope_uncalibrated		CW_GYROSCOPE_UNCALIBRATED
#define game_rotation_vector		CW_GAME_ROTATION_VECTOR
#define geomagnetic_rotation_vector	CW_GEOMAGNETIC_ROTATION_VECTOR

#define significant_motion		CW_SIGNIFICANT_MOTION
#define step_detector			CW_STEP_DETECTOR
#define step_counter			CW_STEP_COUNTER

#define num_sensors			CW_SENSORS_ID_END

#define CWSTM32_ENABLE_REG			0x01
#define CWSTM32_READ_SEQUENCE_DATA_REG  	0x0F

#define CWSTM32_WRITE_POSITION_Acceleration    	0x20
#define CWSTM32_WRITE_POSITION_Magnetic		0x21
#define CWSTM32_WRITE_POSITION_Gyro		0x22

#define CWSTM32_WRITE_CLEAN_COUNT_Pedometer	0x30

#define CWSTM32_INT_ST1                         0x08
#define CWSTM32_INT_ST2                         0x09
#define CWSTM32_INT_ST3                         0x0A
#define CWSTM32_INT_ST4                         0x0B
#define CWSTM32_ERR_ST                          0x1F

/* INT_ST1 */
#define CW_MCU_INT_BIT_LIGHT                    (1 << 3)

/* INT_ST4 */
#define CW_MCU_INT_BIT_HTC_GESTURE_MOTION       (1 << 0)
#define CW_MCU_INT_BIT_ANY_MOTION               (1 << 4)

/* INT_ST3 */
#define CW_MCU_INT_BIT_SIGNIFICANT_MOTION       (1 << 4)
#define CW_MCU_INT_BIT_STEP_DETECTOR            (1 << 5)
#define CW_MCU_INT_BIT_STEP_COUNTER             (1 << 6)

/* ERR_ST */
#define CW_MCU_INT_BIT_ERROR_MCU_EXCEPTION      (1 << 6)
#define CW_MCU_INT_BIT_ERROR_WATCHDOG_RESET     (1 << 7)

#define CW_MCU_BIT_LIGHT_POLLING		(1 << 5)

#define	CW_MCU_I2C_SENSORS_REG_START		(0x20)

#define CWSTM32_READ_Gesture_Flip                               (CW_MCU_I2C_SENSORS_REG_START + HTC_GESTURE_FLIP)
#define CWSTM32_READ_Acceleration    		(CW_MCU_I2C_SENSORS_REG_START + CW_ACCELERATION)
#define CWSTM32_READ_Magnetic						(CW_MCU_I2C_SENSORS_REG_START + CW_MAGNETIC)
#define CWSTM32_READ_Gyro    						(CW_MCU_I2C_SENSORS_REG_START + CW_GYRO)
#define CWSTM32_READ_Light    					(CW_MCU_I2C_SENSORS_REG_START + CW_LIGHT)
#define CWSTM32_READ_Pressure	    			(CW_MCU_I2C_SENSORS_REG_START + CW_PRESSURE)
#define CWSTM32_READ_Orientation    		(CW_MCU_I2C_SENSORS_REG_START + CW_ORIENTATION)
#define CWSTM32_READ_RotationVector    	(CW_MCU_I2C_SENSORS_REG_START + CW_ROTATIONVECTOR)
#define CWSTM32_READ_LinearAcceleration (CW_MCU_I2C_SENSORS_REG_START + CW_LINEARACCELERATION)
#define CWSTM32_READ_Gravity    				(CW_MCU_I2C_SENSORS_REG_START + CW_GRAVITY)
#define CWSTM32_READ_Hall_Sensor                 	        0x2C
#define CWSTM32_READ_MAGNETIC_UNCALIBRATED			0x30
#define CWSTM32_READ_GYROSCOPE_UNCALIBRATED			0x31
#define CWSTM32_READ_GAME_ROTATION_VECTOR			0x32
#define CWSTM32_READ_GEOMAGNETIC_ROTATION_VECTOR		0x33
#define CWSTM32_READ_SIGNIFICANT_MOTION				0x34
#define CWSTM32_READ_STEP_DETECTOR				0x35
#define CWSTM32_READ_STEP_COUNTER				0x36
#define CWSTM32_READ_Gesture_Motion                             0x38
#define CWSTM32_READ_Any_Motion                 	        0x3F

#ifdef __KERNEL__
struct cwmcu_platform_data {
	unsigned char acceleration_axes;
	unsigned char magnetic_axes;
	unsigned char gyro_axes;
	uint32_t gpio_wake_mcu;
	uint32_t gpio_reset;
	uint32_t gpio_chip_mode;
	uint32_t gpio_mcu_irq;
	int gs_chip_layout;

};
#endif /* __KERNEL */

#endif /* __CWMCUSENSOR_H__ */
