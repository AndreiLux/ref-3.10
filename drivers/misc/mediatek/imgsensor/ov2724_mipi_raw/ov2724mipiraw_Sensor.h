/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.h
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _OV2724MIPIRAW_SENSOR_H
#define _OV2724MIPIRAW_SENSOR_H

	 /* #define OV2724MIPI_DEBUG */
	 /* #define OV2724MIPI_DRIVER_TRACE */
	 /* #define OV2724MIPI_TEST_PATTEM */
#ifdef OV2724MIPI_DEBUG
	 /* #define SENSORDB printk */
#else
	 /* #define SENSORDB(x,...) */
#endif

#define OV2724MIPI_FACTORY_START_ADDR 0
#define OV2724MIPI_ENGINEER_START_ADDR 10

typedef enum OV2724MIPI_group_enum {
	OV2724MIPI_PRE_GAIN = 0,
	OV2724MIPI_CMMCLK_CURRENT,
	OV2724MIPI_FRAME_RATE_LIMITATION,
	OV2724MIPI_REGISTER_EDITOR,
	OV2724MIPI_GROUP_TOTAL_NUMS
} OV2724MIPI_FACTORY_GROUP_ENUM;

typedef enum OV2724MIPI_register_index {
	OV2724MIPI_SENSOR_BASEGAIN = OV2724MIPI_FACTORY_START_ADDR,
	OV2724MIPI_PRE_GAIN_R_INDEX,
	OV2724MIPI_PRE_GAIN_Gr_INDEX,
	OV2724MIPI_PRE_GAIN_Gb_INDEX,
	OV2724MIPI_PRE_GAIN_B_INDEX,
	OV2724MIPI_FACTORY_END_ADDR
} OV2724MIPI_FACTORY_REGISTER_INDEX;

typedef enum OV2724MIPI_engineer_index {
	OV2724MIPI_CMMCLK_CURRENT_INDEX = OV2724MIPI_ENGINEER_START_ADDR,
	OV2724MIPI_ENGINEER_END
} OV2724MIPI_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct {
	SENSOR_REG_STRUCT reg[OV2724MIPI_ENGINEER_END];
	SENSOR_REG_STRUCT cct[OV2724MIPI_FACTORY_END_ADDR];
} sensor_data_struct;

	 /* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define OV2724MIPI_PREVIEW_CLK                     160000000	/* 42000000 */
#define OV2724MIPI_CAPTURE_CLK                     160000000	/* 42000000 */

#define OV2724MIPI_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_R

#define OV2724MIPI_MIN_ANALOG_GAIN				1	/* 1x */
#define OV2724MIPI_MAX_ANALOG_GAIN				16	/* 32x */


	 /* FRAME RATE UNIT */
#define OV2724MIPI_FPS(x)                          (10 * (x))

	 /* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
	 /* #define OV2724MIPI_FULL_PERIOD_PIXEL_NUMS              2700  9 fps */
#define OV2724MIPI_FULL_PERIOD_PIXEL_NUMS          1920	/* 8 fps */
#define OV2724MIPI_FULL_PERIOD_LINE_NUMS           1080
#define OV2724MIPI_PV_PERIOD_PIXEL_NUMS            1920	/* 30 fps */
#define OV2724MIPI_PV_PERIOD_LINE_NUMS             1080

	 /* SENSOR START/END POSITION */
#define OV2724MIPI_FULL_X_START                    0	/* 1 */
#define OV2724MIPI_FULL_Y_START                    0	/* 1 */
#define OV2724MIPI_IMAGE_SENSOR_FULL_WIDTH         (1920)	/* (1920-16) */
#define OV2724MIPI_IMAGE_SENSOR_FULL_HEIGHT        (1080)	/* (1080-9) */
#define OV2724MIPI_PV_X_START                      0	/* 1 */
#define OV2724MIPI_PV_Y_START                      0	/* 1 */
#define OV2724MIPI_IMAGE_SENSOR_PV_WIDTH           (1920)	/* (1920-16) */
#define OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT          (1080)	/* (1080-9) */

#define OV2724MIPI_DEFAULT_DUMMY_PIXELS			(362)	/* LineLength - OV2724MIPI_FULL_PERIOD_PIXEL_NUMS */
#define OV2724MIPI_DEFAULT_DUMMY_LINES			(1256)	/* FrameLength - OV2724MIPI_FULL_PERIOD_LINE_NUMS */

	 /* SENSOR READ/WRITE ID */
#define OV2724MIPI_WRITE_ID_1 (0x20)	/* (0x6c) */
#define OV2724MIPI_READ_ID_1  (0x21)	/* (0x6d) */
#define OV2724MIPI_WRITE_ID_2 (0x6c)	/* (0x20)//(0x6c) */
#define OV2724MIPI_READ_ID_2  (0x6d)	/* (0x21)//(0x6d) */



	 /* SENSOR ID */
	 /* #define OV2724MIPI_SENSOR_ID                                          (0x2720) */

	 /* SENSOR PRIVATE STRUCT */
typedef struct OV2724MIPI_sensor_STRUCT {
	MSDK_SENSOR_CONFIG_STRUCT cfg_data;
	sensor_data_struct eng;	/* engineer mode */
	MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
	kal_uint8 mirror;
	kal_bool pv_mode;
	kal_bool video_mode;
	kal_bool NightMode;
	kal_uint16 normal_fps;	/* video normal mode max fps */
	kal_uint16 night_fps;	/* video night mode max fps */
	kal_uint16 FixedFps;
	kal_uint16 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint16 frame_height;
	kal_uint16 default_height;
	kal_uint16 line_length;
} OV2724MIPI_sensor_struct;

kal_uint16 OV2724MIPI_sensor_id = 0;
kal_uint16 OV2724MIPI_state = 1;
kal_uint8 OV2724MIPI_WRITE_ID = 0;
kal_uint8 OV2724test_pattern_flag = 0;

/* Extern Function / Variables */
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int main_sensor_init_setting_switch;

	 /* export functions */
UINT32 OV2724MIPIOpen(void);
UINT32 OV2724MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId,
			 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV2724MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,
				UINT32 *pFeatureParaLen);
UINT32 OV2724MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV2724MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV2724MIPIClose(void);

#define Sleep(ms) mdelay(ms)

#endif
