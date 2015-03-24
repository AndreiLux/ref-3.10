/*****************************************************************************
 *
 * Filename:
 * ---------
 *   Sensor.c
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov2724mipiraw_Sensor.h"
#include "ov2724mipiraw_Camera_Sensor_para.h"
#include "ov2724mipiraw_CameraCustomized.h"

#ifdef DEBUG
#define SENSORDB printk
#else
#define SENSORDB pr_debug
#endif

static OV2724MIPI_sensor_struct OV2724MIPI_sensor = {
	.eng = {
		.reg = CAMERA_SENSOR_REG_DEFAULT_VALUE,
		.cct = CAMERA_SENSOR_CCT_DEFAULT_VALUE,
		},
	.eng_info = {
		     .SensorId = 128,
		     .SensorType = CMOS_SENSOR,
		     .SensorOutputDataFormat = OV2724MIPI_COLOR_FORMAT,
		     },
	.normal_fps = 10,
	.night_fps = 5,
	.shutter = 0x20,
	.gain = 0x20,
	.pclk = OV2724MIPI_PREVIEW_CLK,
	.frame_height = OV2724MIPI_PV_PERIOD_LINE_NUMS + OV2724MIPI_DEFAULT_DUMMY_LINES,
	.line_length = OV2724MIPI_PV_PERIOD_PIXEL_NUMS + OV2724MIPI_DEFAULT_DUMMY_PIXELS,
	.default_height = OV2724MIPI_PV_PERIOD_LINE_NUMS + OV2724MIPI_DEFAULT_DUMMY_LINES,
};

static DEFINE_SPINLOCK(OV2724mipiraw_drv_lock);
kal_bool OV2724MIPI_AutoFlicker_Mode = KAL_FALSE;

#define OV2724_TEST_PATTERN_CHECKSUM (0x8b04298)

static MSDK_SCENARIO_ID_ENUM CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

kal_uint16 OV2724MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char puSendCmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(puSendCmd, 3, OV2724MIPI_WRITE_ID);
	return TRUE;
}

kal_uint16 OV2724MIPI_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char puSendCmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd, 2, (u8 *) &get_byte, 1, OV2724MIPI_WRITE_ID);

	return get_byte;
}

static void OV2724MIPI_Write_Shutter(kal_uint16 iShutter)
{
	kal_uint16 frame_height = 0, current_fps = 0;
	unsigned long flags;
	/* SENSORDB("[OV2724MIPI]enter OV2724MIPI_Write_Shutter function iShutter=%d\n", iShutter); */

	if (main_sensor_init_setting_switch != 0)	/* FATP bypass */
		return;

	if (!iShutter)
		iShutter = 1;	/* avoid 0 */
	else if (iShutter >= 0x7FFF - 1) {
		iShutter = 0x7FFF - 0xF;
		pr_err("Error: The Shutter is too big, The Sensor is dead!!!!!!!!\n");
	}
	if (iShutter > OV2724MIPI_sensor.frame_height - 4)
		frame_height = iShutter + 4;
	else
		frame_height = OV2724MIPI_sensor.frame_height;

	/* SENSORDB("[OV2724MIPI_Write_Shutter]iShutter:%x; frame_height:%x\n", iShutter,
		 frame_height); */

	current_fps = OV2724MIPI_sensor.pclk / OV2724MIPI_sensor.line_length / frame_height;
	/* SENSORDB("CURRENT FPS:%d,OV2724MIPI_sensor.default_height=%d",
		 current_fps, OV2724MIPI_sensor.frame_height); */
	if (current_fps == 30 || current_fps == 15) {
		if (OV2724MIPI_AutoFlicker_Mode == TRUE)
			frame_height = frame_height + (frame_height >> 7);
	}
	OV2724MIPI_write_cmos_sensor(0x0340, frame_height >> 8);
	OV2724MIPI_write_cmos_sensor(0x0341, frame_height & 0xFF);
	spin_lock_irqsave(&OV2724mipiraw_drv_lock, flags);
	/* OV2724MIPI_sensor.frame_height=frame_height; */
	spin_unlock_irqrestore(&OV2724mipiraw_drv_lock, flags);
	/* OV2724MIPI_write_cmos_sensor(0x3502, (iShutter & 0x0f)<<4); */
	/* OV2724MIPI_write_cmos_sensor(0x3501, (iShutter>>4) & 0xFF); */
	/* OV2724MIPI_write_cmos_sensor(0x3500, (iShutter>>12) & 0x0F); */
	OV2724MIPI_write_cmos_sensor(0x3500, iShutter >> 8);
	OV2724MIPI_write_cmos_sensor(0x3501, iShutter & 0xFF);
	SENSORDB("[%s] frame_height = 0x%x , iShutter = 0x%x\n", __func__, frame_height, iShutter);
}				/*  OV2724MIPI_Write_Shutter    */

static void OV2724MIPI_Set_Dummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
	kal_uint16 line_length, frame_height;
	SENSORDB("[%s] iPixels:%x; iLines:%x\n", __func__, iPixels, iLines);

	if (main_sensor_init_setting_switch != 0)	/* FATP bypass */
		return;

	line_length = OV2724MIPI_FULL_PERIOD_PIXEL_NUMS + iPixels + OV2724MIPI_DEFAULT_DUMMY_PIXELS;
	frame_height = OV2724MIPI_FULL_PERIOD_LINE_NUMS + iLines + OV2724MIPI_DEFAULT_DUMMY_LINES;
	if ((line_length >= 0xFFFF) || (frame_height >= 0xFFFF)) {
		pr_warn("Warnning: line length or frame height is overflow!!!!!!!!\n");
		return;
	}
#if 1				/* add by chenqiang */
	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_sensor.line_length = line_length;
	OV2724MIPI_sensor.frame_height = frame_height;
	/* OV2724MIPI_sensor.default_height=frame_height; */
	spin_unlock(&OV2724mipiraw_drv_lock);
	SENSORDB("[%s] line_length:%x; frame_height:%x\n", __func__, line_length, frame_height);
	OV2724MIPI_write_cmos_sensor(0x0342, line_length >> 8);
	OV2724MIPI_write_cmos_sensor(0x0343, line_length & 0xFF);
	OV2724MIPI_write_cmos_sensor(0x0340, frame_height >> 8);
	OV2724MIPI_write_cmos_sensor(0x0341, frame_height & 0xFF);
#endif
}				/*  OV2724MIPI_Set_Dummy    */

/*************************************************************************
* FUNCTION
*	OV2724MIPI_SetShutter
*
* DESCRIPTION
*	This function set e-shutter of OV2724MIPI to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void set_OV2724MIPI_shutter(kal_uint16 iShutter)
{
#if 1
	unsigned long flags;
	/*SENSORDB("[OV2724MIPI]enter set_OV2724MIPI_shutter function iShutter=%x\n", iShutter);*/
	if (OV2724MIPI_sensor.shutter == iShutter)	/* No Change */
		return;
	spin_lock_irqsave(&OV2724mipiraw_drv_lock, flags);
	OV2724MIPI_sensor.shutter = iShutter;
	spin_unlock_irqrestore(&OV2724mipiraw_drv_lock, flags);
	OV2724MIPI_Write_Shutter(iShutter);
	/*SENSORDB("[OV2724MIPI]exit set_OV2724MIPI_shutter function\n");*/
#endif
}				/*  Set_OV2724MIPI_Shutter */


#if 0				/* Remove for compile warning */
static kal_uint16 OV2724MIPIReg2Gain(const kal_uint8 iReg)
{
#if 1
	kal_uint8 iI;
	kal_uint16 iGain = BASEGAIN;	/* 1x-gain base */
	/* Range: 1x to 32x */
	/* Gain = (GAIN[7] + 1) * (GAIN[6] + 1) * (GAIN[5] + 1) * (GAIN[4] + 1) * (1 + GAIN[3:0] / 16) */
	for (iI = 7; iI >= 4; iI--)
		iGain *= (((iReg >> iI) & 0x01) + 1);

	return iGain + iGain * (iReg & 0x0F) / 16;
#endif
}
#endif

static kal_uint8 OV2724MIPIGain2Reg(const kal_uint16 iGain)
{
#if 1
	kal_uint16 iReg = 0x00;
	/*SENSORDB("[%s] iGain = %d\n", __func__, iGain);*/
	iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
	iReg = iReg & 0xFF;
	/*SENSORDB("[%s] iGain2Reg = %d\n", __func__, iReg);*/
	return (kal_uint8) iReg;
#endif
}





/*************************************************************************
* FUNCTION
*	OV2724MIPI_SetGain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV2724MIPI_SetGain(UINT16 iGain)
{
#if 1
	kal_uint8 iReg;
	/* V5647_sensor.gain = iGain; */
	/* 0x350a[0:1], 0x350b AGC real gain */
	/* [0:3] = N meams N /16 X      */
	/* [4:9] = M meams M X  */
	/* Total gain = M + N /16 X */
	switch (main_sensor_init_setting_switch) {
	case 1:
	case 2:
		iReg = 0x10;
		break;
	case 3:
		iReg = 0xF8;
		break;
	default:
		{
			iReg = OV2724MIPIGain2Reg(iGain);
			if (iReg < 0x10)
				iReg = 0x10;
		}
		break;
	}
	OV2724MIPI_write_cmos_sensor(0x3509, iReg);
	/*SENSORDB("[OV2724MIPI_SetGain] FATP mode (%d) Analog Gain = %d\n",
		 main_sensor_init_setting_switch, iReg);*/
#endif
}				/*      OV2724MIPI_SetGain      */



/*************************************************************************
* FUNCTION
*	OV2724MIPI_NightMode
*
* DESCRIPTION
*	This function night mode of OV2724MIPI.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV2724MIPI_night_mode(kal_bool enable)
{
}				/*  OV2724MIPI_NightMode    */


/* write camera_para to sensor register */
static void OV2724MIPI_camera_para_to_sensor(void)
{
	kal_uint32 i;
	/*SENSORDB("[OV2724MIPI]enter OV2724MIPI_camera_para_to_sensor function\n");*/
	for (i = 0; 0xFFFFFFFF != OV2724MIPI_sensor.eng.reg[i].Addr; i++) {
		OV2724MIPI_write_cmos_sensor(OV2724MIPI_sensor.eng.reg[i].Addr,
					     OV2724MIPI_sensor.eng.reg[i].Para);
	}
	for (i = OV2724MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV2724MIPI_sensor.eng.reg[i].Addr;
	     i++) {
		OV2724MIPI_write_cmos_sensor(OV2724MIPI_sensor.eng.reg[i].Addr,
					     OV2724MIPI_sensor.eng.reg[i].Para);
	}
	OV2724MIPI_SetGain(OV2724MIPI_sensor.gain);	/* update gain */
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPI_camera_para_to_sensor function\n");*/
}

/* update camera_para from sensor register */
static void OV2724MIPI_sensor_to_camera_para(void)
{
	kal_uint32 i, temp_data;
	/*SENSORDB("[OV2724MIPI]enter OV2724MIPI_sensor_to_camera_para function\n");*/
	for (i = 0; 0xFFFFFFFF != OV2724MIPI_sensor.eng.reg[i].Addr; i++) {

		temp_data = OV2724MIPI_read_cmos_sensor(OV2724MIPI_sensor.eng.reg[i].Addr);
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_sensor.eng.reg[i].Para = temp_data;
		spin_unlock(&OV2724mipiraw_drv_lock);
	}
	for (i = OV2724MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV2724MIPI_sensor.eng.reg[i].Addr;
	     i++) {
		temp_data = OV2724MIPI_read_cmos_sensor(OV2724MIPI_sensor.eng.reg[i].Addr);
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_sensor.eng.reg[i].Para = temp_data;
		spin_unlock(&OV2724mipiraw_drv_lock);
	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPI_sensor_to_camera_para function\n");*/
}

/* ------------------------ Engineer mode ------------------------ */
static inline void OV2724MIPI_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{
	/*SENSORDB("[OV2724MIPI]enter OV2724MIPI_get_sensor_group_count function\n");*/
	*sensor_count_ptr = OV2724MIPI_GROUP_TOTAL_NUMS;
}

static inline void OV2724MIPI_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{
	/*SENSORDB("[OV2724MIPI]enter OV2724MIPI_get_sensor_group_info function\n");*/
	switch (para->GroupIdx) {
	case OV2724MIPI_PRE_GAIN:
		sprintf(para->GroupNamePtr, "CCT");
		para->ItemCount = 5;
		break;
	case OV2724MIPI_CMMCLK_CURRENT:
		sprintf(para->GroupNamePtr, "CMMCLK Current");
		para->ItemCount = 1;
		break;
	case OV2724MIPI_FRAME_RATE_LIMITATION:
		sprintf(para->GroupNamePtr, "Frame Rate Limitation");
		para->ItemCount = 2;
		break;
	case OV2724MIPI_REGISTER_EDITOR:
		sprintf(para->GroupNamePtr, "Register Editor");
		para->ItemCount = 2;
		break;
	default:
		ASSERT(0);
	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPI_get_sensor_group_info function\n");*/
}

static inline void OV2724MIPI_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
	static const kal_char * cct_item_name[] = {
		"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B" };
	static const kal_char * editer_item_name[] = { "REG addr", "REG value" };
	SENSORDB("[OV2724MIPI]enter OV2724MIPI_get_sensor_item_info function\n");
	switch (para->GroupIdx) {
	case OV2724MIPI_PRE_GAIN:
		switch (para->ItemIdx) {
		case OV2724MIPI_SENSOR_BASEGAIN:
		case OV2724MIPI_PRE_GAIN_R_INDEX:
		case OV2724MIPI_PRE_GAIN_Gr_INDEX:
		case OV2724MIPI_PRE_GAIN_Gb_INDEX:
		case OV2724MIPI_PRE_GAIN_B_INDEX:
			break;
		default:
			ASSERT(0);
		}
		sprintf(para->ItemNamePtr,
			cct_item_name[para->ItemIdx - OV2724MIPI_SENSOR_BASEGAIN]);
		para->ItemValue = OV2724MIPI_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
		para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
		para->Min = OV2724MIPI_MIN_ANALOG_GAIN * 1000;
		para->Max = OV2724MIPI_MAX_ANALOG_GAIN * 1000;
		break;
	case OV2724MIPI_CMMCLK_CURRENT:
		switch (para->ItemIdx) {
		case 0:
			sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
			switch (OV2724MIPI_sensor.eng.reg[OV2724MIPI_CMMCLK_CURRENT_INDEX].Para) {
			case ISP_DRIVING_2MA:
				para->ItemValue = 2;
				break;
			case ISP_DRIVING_4MA:
				para->ItemValue = 4;
				break;
			case ISP_DRIVING_6MA:
				para->ItemValue = 6;
				break;
			case ISP_DRIVING_8MA:
				para->ItemValue = 8;
				break;
			default:
				ASSERT(0);
			}
			para->IsTrueFalse = para->IsReadOnly = KAL_FALSE;
			para->IsNeedRestart = KAL_TRUE;
			para->Min = 2;
			para->Max = 8;
			break;
		default:
			ASSERT(0);
		}
		break;
	case OV2724MIPI_FRAME_RATE_LIMITATION:
		switch (para->ItemIdx) {
		case 0:
			sprintf(para->ItemNamePtr, "Max Exposure Lines");
			para->ItemValue = 5998;
			break;
		case 1:
			sprintf(para->ItemNamePtr, "Min Frame Rate");
			para->ItemValue = 5;
			break;
		default:
			ASSERT(0);
		}
		para->IsTrueFalse = para->IsNeedRestart = KAL_FALSE;
		para->IsReadOnly = KAL_TRUE;
		para->Min = para->Max = 0;
		break;
	case OV2724MIPI_REGISTER_EDITOR:
		switch (para->ItemIdx) {
		case 0:
		case 1:
			sprintf(para->ItemNamePtr, editer_item_name[para->ItemIdx]);
			para->ItemValue = 0;
			para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
			para->Min = 0;
			para->Max = (para->ItemIdx == 0 ? 0xFFFF : 0xFF);
			break;
		default:
			ASSERT(0);
		}
		break;
	default:
		ASSERT(0);
	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPI_get_sensor_item_info function\n");*/
}

static inline kal_bool OV2724MIPI_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
	kal_uint16 temp_para;
	/*SENSORDB("[OV2724MIPI]enter OV2724MIPI_set_sensor_item_info function\n");*/
	switch (para->GroupIdx) {
	case OV2724MIPI_PRE_GAIN:
		switch (para->ItemIdx) {
		case OV2724MIPI_SENSOR_BASEGAIN:
		case OV2724MIPI_PRE_GAIN_R_INDEX:
		case OV2724MIPI_PRE_GAIN_Gr_INDEX:
		case OV2724MIPI_PRE_GAIN_Gb_INDEX:
		case OV2724MIPI_PRE_GAIN_B_INDEX:
			spin_lock(&OV2724mipiraw_drv_lock);
			OV2724MIPI_sensor.eng.cct[para->ItemIdx].Para =
			    para->ItemValue * BASEGAIN / 1000;
			spin_unlock(&OV2724mipiraw_drv_lock);
			OV2724MIPI_SetGain(OV2724MIPI_sensor.gain);	/* update gain */
			break;
		default:
			ASSERT(0);
		}
		break;
	case OV2724MIPI_CMMCLK_CURRENT:
		switch (para->ItemIdx) {
		case 0:
			switch (para->ItemValue) {
			case 2:
				temp_para = ISP_DRIVING_2MA;
				break;
			case 3:
			case 4:
				temp_para = ISP_DRIVING_4MA;
				break;
			case 5:
			case 6:
				temp_para = ISP_DRIVING_6MA;
				break;
			default:
				temp_para = ISP_DRIVING_8MA;
				break;
			}
			/* OV2724MIPI_set_isp_driving_current(temp_para); */
			spin_lock(&OV2724mipiraw_drv_lock);
			OV2724MIPI_sensor.eng.reg[OV2724MIPI_CMMCLK_CURRENT_INDEX].Para = temp_para;
			spin_unlock(&OV2724mipiraw_drv_lock);
			break;
		default:
			ASSERT(0);
		}
		break;
	case OV2724MIPI_FRAME_RATE_LIMITATION:
		ASSERT(0);
		break;
	case OV2724MIPI_REGISTER_EDITOR:
		switch (para->ItemIdx) {
			static kal_uint32 fac_sensor_reg;
		case 0:
			if (para->ItemValue < 0 || para->ItemValue > 0xFFFF)
				return KAL_FALSE;
			fac_sensor_reg = para->ItemValue;
			break;
		case 1:
			if (para->ItemValue < 0 || para->ItemValue > 0xFF)
				return KAL_FALSE;
			OV2724MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
			break;
		default:
			ASSERT(0);
		}
		break;
	default:
		ASSERT(0);
	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPI_set_sensor_item_info function\n");*/
	return KAL_TRUE;
}

static void OV2724MIPI_init(void)
{
	SENSORDB("[OV2724MIPI]enter OV2724MIPI_init function\n");
	SENSORDB("[OV2724MIPI]init config node = %d\n", main_sensor_init_setting_switch);
	OV2724MIPI_write_cmos_sensor(0x0103, 0x01);	/* software reset */
	mdelay(5);	/* delay of 5ms for SW reset to complete */

	OV2724MIPI_write_cmos_sensor(0x0101, 0x02);
	OV2724MIPI_write_cmos_sensor(0x0301, 0x0a);
	OV2724MIPI_write_cmos_sensor(0x0303, 0x05);
	OV2724MIPI_write_cmos_sensor(0x0307, 0x64);

	switch (main_sensor_init_setting_switch) {
	case 1:
	case 2:
		{	/* exposure time : 1/30s */
			OV2724MIPI_write_cmos_sensor(0x0340, 0x09);
			OV2724MIPI_write_cmos_sensor(0x0341, 0x20);
			OV2724MIPI_write_cmos_sensor(0x3500, 0x09);
			OV2724MIPI_write_cmos_sensor(0x3501, 0x1c);
		}
		break;
	case 3:
		{	/* exposure time : 1/15s */
			OV2724MIPI_write_cmos_sensor(0x0340, 0x12);
			OV2724MIPI_write_cmos_sensor(0x0341, 0x40);
			OV2724MIPI_write_cmos_sensor(0x3500, 0x12);
			OV2724MIPI_write_cmos_sensor(0x3501, 0x3c);
		}
		break;
	case 0:	/* Shipping mdoe */
	default:
		{	/* 30fps */
			OV2724MIPI_write_cmos_sensor(0x0340, 0x09);
			OV2724MIPI_write_cmos_sensor(0x0341, 0x20);	/* ;v05 */
			OV2724MIPI_write_cmos_sensor(0x3500, 0x04);	/* 0x460 */
			OV2724MIPI_write_cmos_sensor(0x3501, 0x66);
		}
		break;
	}

	OV2724MIPI_write_cmos_sensor(0x0342, 0x08);
	OV2724MIPI_write_cmos_sensor(0x0343, 0xea);
	OV2724MIPI_write_cmos_sensor(0x0344, 0x00);
	OV2724MIPI_write_cmos_sensor(0x0345, 0x00);

	OV2724MIPI_write_cmos_sensor(0x0346, 0x00);
	OV2724MIPI_write_cmos_sensor(0x0347, 0x00);
	OV2724MIPI_write_cmos_sensor(0x0348, 0x07);
	OV2724MIPI_write_cmos_sensor(0x0349, 0x9f);
	OV2724MIPI_write_cmos_sensor(0x034a, 0x04);
	OV2724MIPI_write_cmos_sensor(0x034b, 0x47);
	OV2724MIPI_write_cmos_sensor(0x034c, 0x07);	/* 08 */
	OV2724MIPI_write_cmos_sensor(0x034d, 0x80);
	OV2724MIPI_write_cmos_sensor(0x034e, 0x04);	/* 02 */
	OV2724MIPI_write_cmos_sensor(0x034f, 0x38);	/* 9b */

	OV2724MIPI_write_cmos_sensor(0x0381, 0x01);
	OV2724MIPI_write_cmos_sensor(0x0383, 0x01);
	OV2724MIPI_write_cmos_sensor(0x0385, 0x01);	/* 45 */
	OV2724MIPI_write_cmos_sensor(0x0387, 0x01);
	OV2724MIPI_write_cmos_sensor(0x3014, 0x28);	/* 80 */
	OV2724MIPI_write_cmos_sensor(0x3019, 0xf0);	/* d2 *//* sensor temperature default: ON */
	OV2724MIPI_write_cmos_sensor(0x301f, 0x63);	/* 38 */
	OV2724MIPI_write_cmos_sensor(0x3020, 0x09);
	OV2724MIPI_write_cmos_sensor(0x3103, 0x02);	/* 0x85c */
	OV2724MIPI_write_cmos_sensor(0x3106, 0x10);

	OV2724MIPI_write_cmos_sensor(0x3502, 0x01);
	OV2724MIPI_write_cmos_sensor(0x3503, 0x20);
	OV2724MIPI_write_cmos_sensor(0x3504, 0x02);
	OV2724MIPI_write_cmos_sensor(0x3505, 0x20);	/* add by chenqiang */
	OV2724MIPI_write_cmos_sensor(0x3508, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3509, 0x7f);
	OV2724MIPI_write_cmos_sensor(0x350a, 0x00);
	OV2724MIPI_write_cmos_sensor(0x350b, 0x7f);

	OV2724MIPI_write_cmos_sensor(0x350c, 0x00);
	OV2724MIPI_write_cmos_sensor(0x350d, 0x7f);	/* ;BA_V03 */
	OV2724MIPI_write_cmos_sensor(0x350f, 0x83); /* Sync Gain ecffective time with Exposure */
	OV2724MIPI_write_cmos_sensor(0x3510, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3511, 0x20);
	OV2724MIPI_write_cmos_sensor(0x3512, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3513, 0x20);
	OV2724MIPI_write_cmos_sensor(0x3514, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3515, 0x20);	/* 03 */
	OV2724MIPI_write_cmos_sensor(0x3518, 0x00);

	OV2724MIPI_write_cmos_sensor(0x3519, 0x7f);
	OV2724MIPI_write_cmos_sensor(0x351a, 0x00);
	OV2724MIPI_write_cmos_sensor(0x351b, 0x10);
	OV2724MIPI_write_cmos_sensor(0x351c, 0x00);
	OV2724MIPI_write_cmos_sensor(0x351d, 0x10);
	OV2724MIPI_write_cmos_sensor(0x3602, 0x7c);
	OV2724MIPI_write_cmos_sensor(0x3603, 0x22);	/* ;v06 */
	OV2724MIPI_write_cmos_sensor(0x3620, 0x80);	/* ;v06 */
	OV2724MIPI_write_cmos_sensor(0x3622, 0x0b);	/* ;R1A_AM02 */
	OV2724MIPI_write_cmos_sensor(0x3623, 0x48);

	OV2724MIPI_write_cmos_sensor(0x3632, 0xa0);
	OV2724MIPI_write_cmos_sensor(0x3703, 0x23);
	OV2724MIPI_write_cmos_sensor(0x3707, 0x93);
	OV2724MIPI_write_cmos_sensor(0x3708, 0x46);
	OV2724MIPI_write_cmos_sensor(0x370a, 0x33);
	OV2724MIPI_write_cmos_sensor(0x3716, 0x50);
	OV2724MIPI_write_cmos_sensor(0x3717, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3718, 0x10);
	OV2724MIPI_write_cmos_sensor(0x371c, 0xfe);
	OV2724MIPI_write_cmos_sensor(0x371d, 0x44);

	OV2724MIPI_write_cmos_sensor(0x371e, 0x61);
	OV2724MIPI_write_cmos_sensor(0x3721, 0x10);
	OV2724MIPI_write_cmos_sensor(0x3725, 0xd1);
	OV2724MIPI_write_cmos_sensor(0x3730, 0x01);	/* 00 */
	OV2724MIPI_write_cmos_sensor(0x3731, 0xd0);
	OV2724MIPI_write_cmos_sensor(0x3732, 0x02);
	OV2724MIPI_write_cmos_sensor(0x3733, 0x60);
	OV2724MIPI_write_cmos_sensor(0x3734, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3735, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3736, 0x00);

	OV2724MIPI_write_cmos_sensor(0x3737, 0x00);	/* 00 */
	OV2724MIPI_write_cmos_sensor(0x3738, 0x02);
	OV2724MIPI_write_cmos_sensor(0x3739, 0x20);	/* a1 */
	OV2724MIPI_write_cmos_sensor(0x373a, 0x01);
	OV2724MIPI_write_cmos_sensor(0x373b, 0xb0);
	OV2724MIPI_write_cmos_sensor(0x3748, 0x0b);
	OV2724MIPI_write_cmos_sensor(0x3749, 0x9c);
	OV2724MIPI_write_cmos_sensor(0x3759, 0x50);
	OV2724MIPI_write_cmos_sensor(0x3810, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3811, 0x0f);

	OV2724MIPI_write_cmos_sensor(0x3812, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3813, 0x08);
	OV2724MIPI_write_cmos_sensor(0x3820, 0x80);
	OV2724MIPI_write_cmos_sensor(0x3821, 0x00);
	OV2724MIPI_write_cmos_sensor(0x382d, 0x00);
	OV2724MIPI_write_cmos_sensor(0x3831, 0x00);	/* add by chenqiang for r4800 bit[3]  Line sync enable. */
	OV2724MIPI_write_cmos_sensor(0x3b00, 0x50);	/* len start/end */
	OV2724MIPI_write_cmos_sensor(0x3b01, 0x24);
	OV2724MIPI_write_cmos_sensor(0x3b02, 0x34);
	OV2724MIPI_write_cmos_sensor(0x3b04, 0xdc);

	OV2724MIPI_write_cmos_sensor(0x3b09, 0x62);
	OV2724MIPI_write_cmos_sensor(0x4001, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4008, 0x04);
	OV2724MIPI_write_cmos_sensor(0x4009, 0x0d);
	OV2724MIPI_write_cmos_sensor(0x400a, 0x01);
	OV2724MIPI_write_cmos_sensor(0x400b, 0x80);
	OV2724MIPI_write_cmos_sensor(0x400c, 0x00);
	OV2724MIPI_write_cmos_sensor(0x400d, 0x01);
	OV2724MIPI_write_cmos_sensor(0x4010, 0xd0);
	OV2724MIPI_write_cmos_sensor(0x4017, 0x08);

	OV2724MIPI_write_cmos_sensor(0x4042, 0x12);
	OV2724MIPI_write_cmos_sensor(0x4303, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4307, 0x3a);
	OV2724MIPI_write_cmos_sensor(0x4320, 0x80);
	OV2724MIPI_write_cmos_sensor(0x4322, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4323, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4324, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4325, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4326, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4327, 0x00);

	OV2724MIPI_write_cmos_sensor(0x4328, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4329, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4501, 0x08);
	OV2724MIPI_write_cmos_sensor(0x4505, 0x05);
	OV2724MIPI_write_cmos_sensor(0x4601, 0x0a);
	OV2724MIPI_write_cmos_sensor(0x4800, 0x04);
	OV2724MIPI_write_cmos_sensor(0x4816, 0x52);
	OV2724MIPI_write_cmos_sensor(0x481f, 0x32);
	OV2724MIPI_write_cmos_sensor(0x4837, 0x14);
	OV2724MIPI_write_cmos_sensor(0x4838, 0x00);

	OV2724MIPI_write_cmos_sensor(0x490b, 0x00);
	OV2724MIPI_write_cmos_sensor(0x4a00, 0x01);
	OV2724MIPI_write_cmos_sensor(0x4a01, 0xff);
	OV2724MIPI_write_cmos_sensor(0x4a02, 0x59);
	OV2724MIPI_write_cmos_sensor(0x4a03, 0xd7);
	OV2724MIPI_write_cmos_sensor(0x4a04, 0xff);
	OV2724MIPI_write_cmos_sensor(0x4a05, 0x30);
	OV2724MIPI_write_cmos_sensor(0x4a07, 0xff);
	OV2724MIPI_write_cmos_sensor(0x4d00, 0x04);
	OV2724MIPI_write_cmos_sensor(0x4d01, 0x51);

	OV2724MIPI_write_cmos_sensor(0x4d02, 0xd0);
	OV2724MIPI_write_cmos_sensor(0x4d03, 0x7f);
	OV2724MIPI_write_cmos_sensor(0x4d04, 0x92);
	OV2724MIPI_write_cmos_sensor(0x4d05, 0xcf);
	OV2724MIPI_write_cmos_sensor(0x4d0b, 0x01);
	OV2724MIPI_write_cmos_sensor(0x5000, 0x1f);
	OV2724MIPI_write_cmos_sensor(0x5080, 0x00);
	OV2724MIPI_write_cmos_sensor(0x5101, 0x0a);
	OV2724MIPI_write_cmos_sensor(0x5103, 0x69);
	OV2724MIPI_write_cmos_sensor(0x3021, 0x00);

	OV2724MIPI_write_cmos_sensor(0x3022, 0x00);
	OV2724MIPI_write_cmos_sensor(0x0100, 0x01);	/* 1mS delay required */
	mdelay(2);	/* after cmd STREAMING_ON, set to 2mS for stability */
	OV2724MIPI_AutoFlicker_Mode = KAL_FALSE;
	SENSORDB("[OV2724MIPI]exit OV2724MIPI_init function\n");
}

static void OV2724MIPI_HVMirror(kal_uint8 image_mirror)
{
#if 0
	kal_int16 mirror = 0, flip = 0;
	mirror = OV2724MIPI_read_cmos_sensor(0x3820);
	flip = OV2724MIPI_read_cmos_sensor(0x3821);
	switch (imgMirror) {
	case IMAGE_H_MIRROR:	/* IMAGE_NORMAL: */
		OV2724MIPI_write_cmos_sensor(0x3820, (mirror & (0xF9)));	/* Set normal */
		OV2724MIPI_write_cmos_sensor(0x3821, (flip & (0xF9)));	/* Set normal */
		break;
	case IMAGE_NORMAL:	/* IMAGE_V_MIRROR: */
		OV2724MIPI_write_cmos_sensor(0x3820, (mirror & (0xF9)));	/* Set flip */
		OV2724MIPI_write_cmos_sensorr(0x3821, (flip | (0x06)));	/* Set flip */
		break;
	case IMAGE_HV_MIRROR:	/* IMAGE_H_MIRROR: */
		OV2724MIPI_write_cmos_sensor(0x3820, (mirror | (0x06)));	/* Set mirror */
		OV2724MIPI_write_cmos_sensor(0x3821, (flip & (0xF9)));	/* Set mirror */
		break;
	case IMAGE_V_MIRROR:	/* IMAGE_HV_MIRROR: */
		OV2724MIPI_write_cmos_sensor(0x3820, (mirror | (0x06)));	/* Set mirror & flip */
		OV2724MIPI_write_cmos_sensor(0x3821, (flip | (0x06)));	/* Set mirror & flip */
		break;
	}

#endif
}



/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*	OV2724MIPIOpen
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/

kal_uint16 OV2724MIPIGetSensorID(UINT32 *sensorID)
{

	const kal_uint8 i2c_addr[] = { OV2724MIPI_WRITE_ID_2, OV2724MIPI_WRITE_ID_1 };
	kal_uint16 i;

	/* OV2724 may have different i2c address */
	for (i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++) {
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_WRITE_ID = i2c_addr[i];
		spin_unlock(&OV2724mipiraw_drv_lock);

		/*SENSORDB("i2c address is %x ", OV2724MIPI_WRITE_ID);*/

		*sensorID =
		    ((OV2724MIPI_read_cmos_sensor(0x300A) << 8) |
		     OV2724MIPI_read_cmos_sensor(0x300B));
		pr_debug("[OV2724MIPI]exit OV2724MIPIGetSensorID function sensorID = %x\n",
			 *sensorID);

		SENSORDB("i2c address i=%x, sensorID=%x ", i, *sensorID);

		if (*sensorID == OV2724MIPI_SENSOR_ID)
			break;

	}

	if (*sensorID != OV2724MIPI_SENSOR_ID) {
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	/*pr_debug("[OV2724MIPI]exit OV2724MIPIGetSensorID function\n");*/
	return ERROR_NONE;

}

static void OV2724MIPIGetSensorTemperature(UINT32 *temperature)
{
	/* turn on sensor temperature */
	OV2724MIPI_write_cmos_sensor(0x3019, 0xf0);

	/* read sensor temperature (actual temp is need to minus 64) */
	*temperature = (OV2724MIPI_read_cmos_sensor(0x4D13) - 64);
	SENSORDB("[%s] sensor temperature : %d degrees\n", __func__, *temperature);
}


UINT32 OV2724MIPIOpen(void)
{
	kal_uint32 sensor_id = 0;
	SENSORDB("[OV2724MIPI]enter OV2724MIPIOpen function\n");

	OV2724MIPIGetSensorID((UINT32 *) (&sensor_id));

	SENSORDB("sensor_id is %x ", sensor_id);

	if (sensor_id != OV2724MIPI_SENSOR_ID)
		pr_warn("[%s] Get sensor id = 0x%x (!= 0x%x)", __func__, sensor_id, OV2724MIPI_SENSOR_ID);

	/*SENSORDB("[ydb]OV2724MIPI Sensor Read ID:0x%04x OK\n", OV2724MIPI_sensor_id);*/
	OV2724MIPI_init();
	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_state = 1;
	OV2724MIPI_sensor.pv_mode = KAL_TRUE;
	OV2724MIPI_sensor.shutter = 0x100;
	spin_unlock(&OV2724mipiraw_drv_lock);
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPIOpen function\n");*/
	return ERROR_NONE;
}



/*************************************************************************
* FUNCTION
*	OV2724MIPIClose
*
* DESCRIPTION
*	This function is to turn off sensor module power.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV2724MIPIClose(void)
{
	pr_debug("[OV2724MIPI]enter OV2724MIPIClose function\n");
	/* CISModulePowerOn(FALSE); */
/* DRV_I2CClose(OV2724MIPIhDrvI2C); */
	return ERROR_NONE;
}				/* OV2724MIPIClose */

#if 0

UINT32 OV2724MIPISetTestPatternMode(kal_bool bEnable)
{
	SENSORDB("[OV2724MIPI]enter OV2724MIPISetTestPatternMode function bEnable=%d\n", bEnable);
	if (bEnable) {		/* enable color bar */
		OV2724test_pattern_flag = TRUE;
		OV2724MIPI_write_cmos_sensor(0x5040, 0x80);
	} else {
		OV2724test_pattern_flag = FALSE;
		OV2724MIPI_write_cmos_sensor(0x5040, 0x00);
	}
	mdelay(50);
	return ERROR_NONE;
}
#endif

/*************************************************************************
* FUNCTION
* OV2724MIPIPreview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV2724MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 dummy_line = 0;
	SENSORDB("[%s] +\n", __func__);
	if (OV2724MIPI_state == 1) {
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_state = 0;
		spin_unlock(&OV2724mipiraw_drv_lock);
	}
	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&OV2724mipiraw_drv_lock);
	switch (sensor_config_data->SensorOperationMode) {
	case MSDK_SENSOR_OPERATION_MODE_VIDEO:
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_sensor.video_mode = KAL_TRUE;
		spin_unlock(&OV2724mipiraw_drv_lock);
		SENSORDB("[]Video mode\n");
		break;
	default:		/* ISP_PREVIEW_MODE */
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_sensor.video_mode = KAL_FALSE;
		spin_unlock(&OV2724mipiraw_drv_lock);
		SENSORDB("[]Camera preview mode\n");
		break;
	}
	/* OV2724MIPI_HVMirror(sensor_config_data->SensorImageMirror); */
	OV2724MIPI_Set_Dummy(0, dummy_line);	/* modify dummy_pixel must gen AE table again */
	SENSORDB("[%s] -\n", __func__);
	return ERROR_NONE;
}				/*  OV2724MIPIPreview   */

/*************************************************************************
* FUNCTION
*	OV2724MIPICapture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV2724MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/* const kal_uint32 pv_pclk = OV2724MIPI_sensor.pclk; */
	kal_uint16 dummy_line = 0;
	kal_uint16 startx = 0, starty = 0;
	SENSORDB("[%s] +\n", __func__);
	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_state = 1;
	spin_unlock(&OV2724mipiraw_drv_lock);

	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_sensor.video_mode = KAL_FALSE;
	OV2724MIPI_AutoFlicker_Mode = KAL_FALSE;
	spin_unlock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_HVMirror(sensor_config_data->SensorImageMirror);
	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_sensor.pv_mode = KAL_FALSE;
	spin_unlock(&OV2724mipiraw_drv_lock);
	startx += OV2724MIPI_FULL_X_START;
	starty += OV2724MIPI_FULL_Y_START;
	image_window->GrabStartX = startx;
	image_window->GrabStartY = starty;
	image_window->ExposureWindowWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH - 2 * startx;
	image_window->ExposureWindowHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT - 2 * starty;
	OV2724MIPI_Set_Dummy(0, dummy_line);	/* modify dummy_pixel must gen AE table again */
	SENSORDB("[%s] -\n", __func__);
	return ERROR_NONE;
}				/* OV2724MIPI_Capture() */

UINT32 OV2724MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	/*pr_debug("[OV2724MIPI]enter OV2724MIPIGetResolution function\n");*/
	pSensorResolution->SensorPreviewWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorFullWidth = OV2724MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight = OV2724MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	/* Gionee yanggy 2012-12-10 add for rear camera begin */
	pSensorResolution->SensorVideoWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorVideoHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->Sensor3DPreviewWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->Sensor3DPreviewHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->Sensor3DFullWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->Sensor3DFullHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->Sensor3DVideoWidth = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->Sensor3DVideoHeight = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
	/* Gionee yanggy 2012-12-10 add for rear camera end */
	pr_debug
	    ("[%s] PrvWidth=%d,FullWidth=%d\n", __func__,
	     pSensorResolution->SensorPreviewWidth, pSensorResolution->SensorFullWidth);
	return ERROR_NONE;
}				/* OV2724MIPIGetResolution() */

UINT32 OV2724MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
			 MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pr_debug("[OV2724MIPI]enter OV2724MIPIGetInfo function FeatureId:%d\n", ScenarioId);
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pSensorInfo->SensorPreviewResolutionX = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorInfo->SensorPreviewResolutionY = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
		pSensorInfo->SensorCameraPreviewFrameRate = 15;
		break;
	default:
		pSensorInfo->SensorPreviewResolutionX = OV2724MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorInfo->SensorPreviewResolutionY = OV2724MIPI_IMAGE_SENSOR_PV_HEIGHT;
		pSensorInfo->SensorCameraPreviewFrameRate = 30;
		break;

	}
	pSensorInfo->SensorFullResolutionX = OV2724MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = OV2724MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 10;
	pSensorInfo->SensorWebCamCaptureFrameRate = 15;
	pSensorInfo->SensorResetActiveHigh = TRUE;	/* low active */
	pSensorInfo->SensorResetDelayCount = 5;

	pSensorInfo->SensorOutputDataFormat = OV2724MIPI_COLOR_FORMAT;
	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	pSensorInfo->SensorInterruptDelayLines = 4;
	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
	/* pSensorInfo->MIPIsensorType=MIPI_OPHY_CSI2; */

	pSensorInfo->CaptureDelayFrame = 2;
	pSensorInfo->PreviewDelayFrame = 2;
	pSensorInfo->VideoDelayFrame = 5;

	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_2MA;
	pSensorInfo->AEShutDelayFrame = 0;	/* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 1;	/* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2;
	/* pSensorInfo->AEISPGainDelayFrame = 1; */
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pSensorInfo->SensorClockFreq = 24;	/* 26  hehe@0327 for flicker */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV2724MIPI_FULL_X_START;
		pSensorInfo->SensorGrabStartY = OV2724MIPI_FULL_Y_START;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorHightSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorPacketECCOrder = 1;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:

		pSensorInfo->SensorClockFreq = 24;	/* 26  hehe@0327 for flicker */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV2724MIPI_FULL_X_START;
		pSensorInfo->SensorGrabStartY = OV2724MIPI_FULL_Y_START;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorHightSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	default:
		pSensorInfo->SensorClockFreq = 24;	/* 26 hehe@0327 for flicker */
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = OV2724MIPI_FULL_Y_START;
		pSensorInfo->SensorGrabStartY = OV2724MIPI_FULL_Y_START;
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorHightSampling = 0;	/* 0 is default 1x */
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}
#if 0
	OV2724MIPIPixelClockDivider = pSensorInfo->SensorPixelClockCount;
	memcpy(pSensorConfigData, &OV2724MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
#endif
	/*pr_debug("[OV2724MIPI]exit OV2724MIPIGetInfo function\n");*/
	return ERROR_NONE;
}				/* OV2724MIPIGetInfo() */

UINT32 OV2724MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId,
			 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
			 MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	SENSORDB("[OV2724MIPI]enter OV2724MIPIControl function FeatureId:%d\n", ScenarioId);
	CurrentScenarioId = ScenarioId;
	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		OV2724MIPIPreview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		OV2724MIPICapture(pImageWindow, pSensorConfigData);
		break;
	default:
		return ERROR_INVALID_SCENARIO_ID;
	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPIControl function\n");*/
	return ERROR_NONE;
}				/* OV2724MIPIControl() */

UINT32 OV2724SetMaxFrameRate(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
	kal_uint16 FrameHeight = OV2724MIPI_sensor.frame_height;
	/* unsigned long flags; */

	SENSORDB("[OV2724MIPI][OV2724SetMaxFrameRate]u2FrameRate=%d", u2FrameRate);

	FrameHeight =
	    (10 * OV2724MIPI_sensor.pclk) / u2FrameRate / (OV2724MIPI_PV_PERIOD_PIXEL_NUMS +
							   OV2724MIPI_DEFAULT_DUMMY_PIXELS);

	spin_lock(&OV2724mipiraw_drv_lock);
	OV2724MIPI_sensor.frame_height = FrameHeight;
	spin_unlock(&OV2724mipiraw_drv_lock);
	dummy_line =
	    FrameHeight - (OV2724MIPI_PV_PERIOD_LINE_NUMS + OV2724MIPI_DEFAULT_DUMMY_LINES);
	if (dummy_line < 0)
		dummy_line = 0;

	/* to fix VSYNC, to fix frame rate */
	OV2724MIPI_Set_Dummy(0, dummy_line);	/* modify dummy_pixel must gen AE table again */
	return (UINT32) u2FrameRate;
}

UINT32 OV2724MIPISetVideoMode(UINT16 u2FrameRate)
{
	/* kal_int16 dummy_line; */
	/* to fix VSYNC, to fix frame rate */
	SENSORDB("[OV2724MIPI]enter OV2724MIPISetVideoMode functionu 2FrameRate=%d\n", u2FrameRate);

	spin_lock(&OV2724mipiraw_drv_lock);
	if (u2FrameRate == 30) {
		OV2724MIPI_sensor.NightMode = KAL_FALSE;
	} else if (u2FrameRate == 15) {
		OV2724MIPI_sensor.NightMode = KAL_TRUE;
	} else if (u2FrameRate == 0) {
		/* For Dynamic frame rate,Nothing to do */
		OV2724MIPI_sensor.video_mode = KAL_FALSE;
		spin_unlock(&OV2724mipiraw_drv_lock);
		SENSORDB("Do not fix framerate\n");
		return KAL_TRUE;
	} else {
		/* TODO: */
		/* return TRUE; */
	}

	OV2724MIPI_sensor.video_mode = KAL_TRUE;
	OV2724MIPI_sensor.FixedFps = u2FrameRate;
	spin_unlock(&OV2724mipiraw_drv_lock);

	if ((u2FrameRate == 30) && (OV2724MIPI_AutoFlicker_Mode == KAL_TRUE))
		u2FrameRate = 303;
	else if ((u2FrameRate == 15) && (OV2724MIPI_AutoFlicker_Mode == KAL_TRUE))
		u2FrameRate = 148;	/* 148; */
	else
		u2FrameRate = 10 * u2FrameRate;

	OV2724SetMaxFrameRate(u2FrameRate);

	/*SENSORDB("[OV2724MIPI]exit OV2724MIPISetVideoMode functionu\n");*/
	return TRUE;
}

UINT32 OV2724MIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	SENSORDB
	    ("[OV2724MIPI]enter OV2724MIPISetAutoFlickerMode functionu bEnable=%d,u2FrameRate=%d\n",
	     bEnable, u2FrameRate);
	if (bEnable) {
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_AutoFlicker_Mode = KAL_TRUE;
		spin_unlock(&OV2724mipiraw_drv_lock);
		if ((OV2724MIPI_sensor.FixedFps == 30)
		    && (OV2724MIPI_sensor.video_mode == KAL_TRUE))
			OV2724SetMaxFrameRate(303);
		if ((OV2724MIPI_sensor.FixedFps == 15)
		    && (OV2724MIPI_sensor.video_mode == KAL_TRUE))
			OV2724SetMaxFrameRate(148);
	} else {
		spin_lock(&OV2724mipiraw_drv_lock);
		OV2724MIPI_AutoFlicker_Mode = KAL_FALSE;
		spin_unlock(&OV2724mipiraw_drv_lock);
		if ((OV2724MIPI_sensor.FixedFps == 30)
		    && (OV2724MIPI_sensor.video_mode == KAL_TRUE))
			OV2724SetMaxFrameRate(300);
		if ((OV2724MIPI_sensor.FixedFps == 15)
		    && (OV2724MIPI_sensor.video_mode == KAL_TRUE))
			OV2724SetMaxFrameRate(150);

	}
	/*SENSORDB("[OV2724MIPI]exit OV2724MIPISetAutoFlickerMode functionu\n");*/
	return TRUE;
}

UINT32 OV2724MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength, frameHeight;

	SENSORDB("[%s] scenarioId = %d, frame rate = %d\n", __func__, scenarioId, frameRate);

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = OV2724MIPI_PREVIEW_CLK;
		lineLength = OV2724MIPI_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - OV2724MIPI_PV_PERIOD_LINE_NUMS;
		OV2724MIPI_Set_Dummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = OV2724MIPI_PREVIEW_CLK;
		lineLength = OV2724MIPI_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - OV2724MIPI_PV_PERIOD_LINE_NUMS;
		OV2724MIPI_Set_Dummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pclk = OV2724MIPI_CAPTURE_CLK;
		lineLength = OV2724MIPI_FULL_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - OV2724MIPI_FULL_PERIOD_LINE_NUMS;
		OV2724MIPI_Set_Dummy(0, dummyLine);
		break;
	default:
		pr_warn("[%s]No such scenario id:%d\n", __func__, scenarioId);
		break;
	}
	return ERROR_NONE;
}


UINT32 OV2724MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate)
{
	SENSORDB("[%s] scenarioId = %d\n", __func__, scenarioId);

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		 *pframeRate = 300;
		 break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		 *pframeRate = 150;
		break;
	default:
		pr_warn("[%s] No such scenario id:%d\n", __func__, scenarioId);
		break;
	}

	return ERROR_NONE;
}


UINT32 OV2724MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
				UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData = (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	/*pr_debug("[OV2724MIPI]enter OV2724MIPIFeatureControl functionu FeatureId:%d\n", FeatureId);*/
	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = OV2724MIPI_IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16 = OV2724MIPI_IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:	/* 3 */

		switch (CurrentScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara16++ = OV2724MIPI_sensor.line_length;
			*pFeatureReturnPara16 = OV2724MIPI_sensor.frame_height;
			*pFeatureParaLen = 4;
			break;
		default:
			*pFeatureReturnPara16++ = OV2724MIPI_sensor.line_length;
			*pFeatureReturnPara16 = OV2724MIPI_sensor.frame_height;
			*pFeatureParaLen = 4;
			break;

		}

		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	/* 3 */

		switch (CurrentScenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara32 = OV2724MIPI_sensor.pclk;
			*pFeatureParaLen = 4;
			break;
		default:
			*pFeatureReturnPara32 = OV2724MIPI_sensor.pclk;
			*pFeatureParaLen = 4;
			break;
		}

		break;
	case SENSOR_FEATURE_SET_ESHUTTER:	/* 4 */
		set_OV2724MIPI_shutter(*pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		OV2724MIPI_night_mode((BOOL) * pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_GAIN:	/* 6 */
		OV2724MIPI_SetGain((UINT16) *pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
#if 1
	case SENSOR_FEATURE_SET_REGISTER:
		OV2724MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		pSensorRegData->RegData = OV2724MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
		memcpy(&OV2724MIPI_sensor.eng.cct, pFeaturePara, sizeof(OV2724MIPI_sensor.eng.cct));
		break;
		break;
	case SENSOR_FEATURE_GET_CCT_REGISTER:	/* 12 */
		if (*pFeatureParaLen >= sizeof(OV2724MIPI_sensor.eng.cct) + sizeof(kal_uint32)) {
			*((kal_uint32 *) pFeaturePara++) = sizeof(OV2724MIPI_sensor.eng.cct);
			memcpy(pFeaturePara, &OV2724MIPI_sensor.eng.cct,
			       sizeof(OV2724MIPI_sensor.eng.cct));
		}
		break;
	case SENSOR_FEATURE_SET_ENG_REGISTER:
		memcpy(&OV2724MIPI_sensor.eng.reg, pFeaturePara, sizeof(OV2724MIPI_sensor.eng.reg));
		break;
	case SENSOR_FEATURE_GET_ENG_REGISTER:	/* 14 */
		if (*pFeatureParaLen >= sizeof(OV2724MIPI_sensor.eng.reg) + sizeof(kal_uint32)) {
			*((kal_uint32 *) pFeaturePara++) = sizeof(OV2724MIPI_sensor.eng.reg);
			memcpy(pFeaturePara, &OV2724MIPI_sensor.eng.reg,
			       sizeof(OV2724MIPI_sensor.eng.reg));
		}
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
		((PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara)->Version =
		    NVRAM_CAMERA_SENSOR_FILE_VERSION;
		((PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara)->SensorId = OV2724MIPI_SENSOR_ID;
		memcpy(((PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara)->SensorEngReg,
		       &OV2724MIPI_sensor.eng.reg, sizeof(OV2724MIPI_sensor.eng.reg));
		memcpy(((PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara)->SensorCCTReg,
		       &OV2724MIPI_sensor.eng.cct, sizeof(OV2724MIPI_sensor.eng.cct));
		*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		memcpy(pFeaturePara, &OV2724MIPI_sensor.cfg_data,
		       sizeof(OV2724MIPI_sensor.cfg_data));
		*pFeatureParaLen = sizeof(OV2724MIPI_sensor.cfg_data);
		break;
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		OV2724MIPI_camera_para_to_sensor();
		break;
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		OV2724MIPI_sensor_to_camera_para();
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		OV2724MIPI_get_sensor_group_count((kal_uint32 *) pFeaturePara);
		*pFeatureParaLen = 4;
		break;
		OV2724MIPI_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_GET_ITEM_INFO:
		OV2724MIPI_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_SET_ITEM_INFO:
		OV2724MIPI_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara);
		*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		break;
	case SENSOR_FEATURE_GET_ENG_INFO:
		memcpy(pFeaturePara, &OV2724MIPI_sensor.eng_info,
		       sizeof(OV2724MIPI_sensor.eng_info));
		*pFeatureParaLen = sizeof(OV2724MIPI_sensor.eng_info);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;

	case SENSOR_FEATURE_GET_SENSOR_CURRENT_TEMPERATURE:
		OV2724MIPIGetSensorTemperature(pFeatureReturnPara32);
		/*SENSORDB
		    ("[%s] == SENSOR_FEATURE_GET_SENSOR_CURRENT_TEMPERATURE == temperature = %d\n",
		     __func__, *pFeatureReturnPara32);*/
		break;
#endif
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		OV2724MIPISetVideoMode(*pFeatureData16);
		break;

	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		OV2724MIPISetAutoFlickerMode((BOOL) * pFeatureData16, *(pFeatureData16 + 1));
		break;

	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		OV2724MIPIGetSensorID(pFeatureReturnPara32);
		break;

	case SENSOR_FEATURE_GET_FACTORY_MODE:	/* 3 */
		*pFeatureReturnPara32 = main_sensor_init_setting_switch;
		*pFeatureParaLen = 4;
		/*SENSORDB("[%s] == SENSOR_FEATURE_GET_FACTORY_MODE == Factory mode = %d\n",
			 __func__, *pFeatureReturnPara32);*/
		break;

	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		OV2724MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		OV2724MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32,
		(MUINT32 *)(*(pFeatureData32+1)));
		break;

#if 0

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		OV2724MIPISetTestPatternMode((BOOL) * pFeatureData16);
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*pFeatureReturnPara32 = OV2724_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
#endif
	default:
		pr_warn("[%s] Unsupport Feature cmd id: %d\n", __func__, FeatureId);
		break;
	}
	/*pr_debug("[OV2724MIPI]exit OV2724MIPIFeatureControl functionu FeatureId:%d\n", FeatureId);*/
	return ERROR_NONE;
}				/* OV2724MIPIFeatureControl() */

SENSOR_FUNCTION_STRUCT SensorFuncOV2724MIPI = {
	OV2724MIPIOpen,
	OV2724MIPIGetInfo,
	OV2724MIPIGetResolution,
	OV2724MIPIFeatureControl,
	OV2724MIPIControl,
	OV2724MIPIClose
};

UINT32 OV2724MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{

	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncOV2724MIPI;

	return ERROR_NONE;
}				/* SensorInit() */
