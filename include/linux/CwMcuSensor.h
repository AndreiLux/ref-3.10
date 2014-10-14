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
	CW_SCAN_ID = 0,
	CW_SCAN_X,
	CW_SCAN_Y,
	CW_SCAN_Z,
	CW_SCAN_XX,
	CW_SCAN_YY,
	CW_SCAN_ZZ,
	CW_SCAN_TIMESTAMP,
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
	HTC_MAGIC_COVER                = 12,
	CW_MAGNETIC_UNCALIBRATED       = 16,
	CW_GYROSCOPE_UNCALIBRATED      = 17,
	CW_GAME_ROTATION_VECTOR        = 18,
	CW_GEOMAGNETIC_ROTATION_VECTOR = 19,
	CW_SIGNIFICANT_MOTION          = 20,
	CW_STEP_DETECTOR               = 21,
	CW_STEP_COUNTER                = 22,
	HTC_FACEDOWN_DETECTION         = 23,
	CW_SENSORS_ID_FW /* Be careful, do not exceed 31, Firmware ID limit */,
	CW_ACCELERATION_W                = 32,
	CW_MAGNETIC_W                    = 33,
	CW_GYRO_W                        = 34,
	CW_PRESSURE_W                    = 37,
	CW_ORIENTATION_W                 = 38,
	CW_ROTATIONVECTOR_W              = 39,
	CW_LINEARACCELERATION_W          = 40,
	CW_GRAVITY_W                     = 41,
	CW_MAGNETIC_UNCALIBRATED_W       = 48,
	CW_GYROSCOPE_UNCALIBRATED_W      = 49,
	CW_GAME_ROTATION_VECTOR_W        = 50,
	CW_GEOMAGNETIC_ROTATION_VECTOR_W = 51,
	CW_STEP_DETECTOR_W               = 53,
	CW_STEP_COUNTER_W                = 54,
	CW_SENSORS_ID_TOTAL              = 55, /* Includes Wake up version */
	TIME_DIFF_EXHAUSTED            = 97,
	CW_TIME_BASE                   = 98,
	CW_META_DATA                   = 99,
	CW_MAGNETIC_UNCALIBRATED_BIAS  = 100,
	CW_GYROSCOPE_UNCALIBRATED_BIAS = 101
} CW_SENSORS_ID;

#define NS_PER_US 1000000LL

#define FIRMWARE_VERSION 0x10

#define HTC_SYSTEM_STATUS_REG                      0x1E

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
#define PRESSURE_UPDATE_RATE                                    0xB6
#define PRESSURE_WAKE_UPDATE_RATE                               0xB7

#define CWMCU_MAX_DELAY		200
#define CWMCU_NO_POLLING_DELAY		10000

#define G_SENSORS_STATUS                                        0x60
#define ACCE_UPDATE_RATE                                        0x66
#define ACCE_WAKE_UPDATE_RATE                                   0x67
#define ECOMPASS_SENSORS_STATUS                                 0x70
#define MAGN_UPDATE_RATE                                        0x76
#define MAGN_WAKE_UPDATE_RATE                                   0x77
#define GYRO_SENSORS_STATUS                                     0x80
#define GYRO_UPDATE_RATE                                        0x86
#define GYRO_WAKE_UPDATE_RATE                                   0x87
#define LIGHT_SENSORS_STATUS                                    0x90
#define LIGHT_UPDATE_PERIOD                                     0x96
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

#define ORIE_WAKE_UPDATE_RATE                                   0xC9
#define ROTA_WAKE_UPDATE_RATE                                   0xCA
#define LINE_WAKE_UPDATE_RATE                                   0xCB
#define GRAV_WAKE_UPDATE_RATE                                   0xCC
#define MAGN_UNCA_WAKE_UPDATE_RATE                              0xCD
#define GYRO_UNCA_WAKE_UPDATE_RATE                              0xCE
#define GAME_ROTA_WAKE_UPDATE_RATE                              0xCF
#define GEOM_ROTA_WAKE_UPDATE_RATE                              0xD2
#define STEP_COUNTER_UPDATE_PERIOD                              0xD3

#define STEP_COUNTER_MASK ((1ULL << CW_STEP_COUNTER) | \
			   (1ULL << CW_STEP_COUNTER_W))

#define IIO_CONTINUOUS_MASK ((1ULL << CW_ACCELERATION) | \
			     (1ULL << CW_MAGNETIC) | \
			     (1ULL << CW_GYRO) | \
			     (1ULL << CW_PRESSURE) | \
			     (1ULL << CW_ORIENTATION) | \
			     (1ULL << CW_ROTATIONVECTOR) | \
			     (1ULL << CW_LINEARACCELERATION) | \
			     (1ULL << CW_GRAVITY) | \
			     (1ULL << CW_MAGNETIC_UNCALIBRATED) | \
			     (1ULL << CW_GYROSCOPE_UNCALIBRATED) | \
			     (1ULL << CW_GAME_ROTATION_VECTOR) | \
			     (1ULL << CW_GEOMAGNETIC_ROTATION_VECTOR) | \
			     (1ULL << CW_STEP_DETECTOR) | \
			     (1ULL << CW_STEP_COUNTER) | \
			     (1ULL << CW_ACCELERATION_W) | \
			     (1ULL << CW_MAGNETIC_W) | \
			     (1ULL << CW_GYRO_W) | \
			     (1ULL << CW_PRESSURE_W) | \
			     (1ULL << CW_ORIENTATION_W) | \
			     (1ULL << CW_ROTATIONVECTOR_W) | \
			     (1ULL << CW_LINEARACCELERATION_W) | \
			     (1ULL << CW_GRAVITY_W) | \
			     (1ULL << CW_MAGNETIC_UNCALIBRATED_W) | \
			     (1ULL << CW_GYROSCOPE_UNCALIBRATED_W) | \
			     (1ULL << CW_GAME_ROTATION_VECTOR_W) | \
			     (1ULL << CW_GEOMAGNETIC_ROTATION_VECTOR_W) | \
			     (1ULL << CW_STEP_DETECTOR_W) | \
			     (1ULL << CW_STEP_COUNTER_W))

#define CW_I2C_REG_WATCHDOG_STATUS                              0xE6
#define WATCHDOG_STATUS_LEN                                     12

#define CW_I2C_REG_EXCEPTION_BUFFER_LEN                   0xFD
#define EXCEPTION_BUFFER_LEN_SIZE                         4
#define CW_I2C_REG_EXCEPTION_BUFFER                       0xFE
#define EXCEPTION_BLOCK_LEN                               16

#define CW_I2C_REG_WARN_MSG_ENABLE                        0xFA
#define CW_I2C_REG_WARN_MSG_BUFFER_LEN                    0xFB
#define WARN_MSG_BUFFER_LEN_SIZE                          8
#define CW_I2C_REG_WARN_MSG_BUFFER                        0xFC
#define WARN_MSG_BLOCK_LEN                                16
#define WARN_MSG_PER_ITEM_LEN                             120

#define CW_I2C_REG_WATCH_DOG_ENABLE                       0xF9

#define UPDATE_RATE_NORMAL              1
#define UPDATE_RATE_UI                  2
#define UPDATE_RATE_GAME                3
#define UPDATE_RATE_FASTEST             4
#define UPDATE_RATE_RATE_10Hz           5
#define UPDATE_RATE_RATE_25Hz           6

#define GENSOR_POSITION			0x65
#define COMPASS_POSITION		0x75
#define GYRO_POSITION			0x85

#define num_sensors			CW_SENSORS_ID_TOTAL

#define CWSTM32_BATCH_MODE_COMMAND	0x40  /* R/W         1 Byte
					       * Bit 2: Timeout flag (R/WC)
					       * Bit 4: Buffer full flag (R/WC)
					       */

#define CWSTM32_BATCH_MODE_DATA_QUEUE	0x45  /* R/W         9 Bytes */
#define CWSTM32_BATCH_MODE_TIMEOUT	0x46  /* R/W         4 Bytes    (ms) */
#define CWSTM32_BATCH_MODE_DATA_COUNTER	0x47  /* R/W         4 Bytes
					       * (4 bytes, from low byte to
					       *  high byte */
#define CWSTM32_BATCH_FLUSH		0x48  /* W        1 Byte (sensors_id) */

#define CWSTM32_WAKE_UP_BATCH_MODE_DATA_QUEUE	0x55  /* R/W         9 Bytes */
#define CWSTM32_WAKE_UP_BATCH_MODE_TIMEOUT      0x56  /* R/W         4 Bytes
						       * (ms) */
#define CWSTM32_WAKE_UP_BATCH_MODE_DATA_COUNTER	0x57  /* R/W         4 Bytes
						       * (4 bytes, from low byte
						       *  to high byte) */

#define SYNC_TIMESTAMP_BIT		(1 << 1)
#define TIMESTAMP_SYNC_CODE		(98)

#define CW_I2C_REG_MCU_TIME                     0x11

#define MAX_EVENT_COUNT                         2500

/* If queue is empty */
#define CWMCU_NODATA	0xFF

#define CWSTM32_ENABLE_REG			0x01
#define CWSTM32_READ_SEQUENCE_DATA_REG  	0x0F

#define CWSTM32_WRITE_POSITION_Acceleration	0x20
#define CWSTM32_WRITE_POSITION_Magnetic		0x21
#define CWSTM32_WRITE_POSITION_Gyro		0x22

#define CWSTM32_WRITE_CLEAN_COUNT_Pedometer	0x30

#define CWSTM32_INT_ST1                         0x08
#define CWSTM32_INT_ST2                         0x09
#define CWSTM32_INT_ST3                         0x0A
#define CWSTM32_INT_ST4                         0x0B
#define CWSTM32_ERR_ST                          0x1F

#define CW_BATCH_ENABLE_REG                     0x41
#define CW_WAKE_UP_BATCH_ENABLE_REG             0x51

#define CW_CPU_STATUS_REG                       0xD1

/* INT_ST1 */
#define CW_MCU_INT_BIT_LIGHT                    (1 << 3)

/* INT_ST2 */
#define CW_MCU_INT_BIT_MAGIC_COVER              (1 << 4)

/* INT_ST3 */
#define CW_MCU_INT_BIT_SIGNIFICANT_MOTION       (1 << 4)
#define CW_MCU_INT_BIT_STEP_DETECTOR            (1 << 5)
#define CW_MCU_INT_BIT_STEP_COUNTER             (1 << 6)
#define CW_MCU_INT_BIT_FACEDOWN_DETECTION       (1 << 7)

/* ERR_ST */
#define CW_MCU_INT_BIT_ERROR_WARN_MSG           (1 << 5)
#define CW_MCU_INT_BIT_ERROR_MCU_EXCEPTION      (1 << 6)
#define CW_MCU_INT_BIT_ERROR_WATCHDOG_RESET     (1 << 7)

/* batch_st */
#define CW_MCU_INT_BIT_BATCH_TIMEOUT      (1 << 2)
#define CW_MCU_INT_BIT_BATCH_BUFFER_FULL  (1 << 4)
#define CW_MCU_INT_BIT_BATCH_TRIGGER_READ (CW_MCU_INT_BIT_BATCH_TIMEOUT |\
					   CW_MCU_INT_BIT_BATCH_BUFFER_FULL)
#define CW_MCU_INT_BIT_BATCH_INT_MASK     CW_MCU_INT_BIT_BATCH_TRIGGER_READ

#define IIO_SENSORS_MASK (((u64)(~0ULL)) & ~(1ULL << HTC_MAGIC_COVER) & \
					   ~(1ULL << (HTC_MAGIC_COVER+32)))

#define CW_MCU_BIT_LIGHT_POLLING		(1 << 5)

#define FW_DOES_NOT_EXIST                       (1 << 0)
#define FW_UPDATE_QUEUED                        (1 << 1)
#define FW_ERASE_FAILED                         (1 << 2)
#define FW_FLASH_FAILED                         (1 << 3)

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
#define CWSTM32_READ_FACEDOWN_DETECTION				0x3A

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
