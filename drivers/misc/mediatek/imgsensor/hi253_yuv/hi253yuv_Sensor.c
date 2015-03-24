/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   Anyuan Huang (MTK70663)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
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
#include <asm/io.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "hi253yuv_Sensor.h"
#include "hi253yuv_Camera_Sensor_para.h"
#include "hi253yuv_CameraCustomized.h"

#define HI253YUV_DEBUG
#ifdef HI253YUV_DEBUG
#define SENSORDB printk
/* #define SENSORDB(fmt, arg...)  pr_err(fmt, ##arg) */
/* #define SENSORDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "4EC", fmt, ##arg) */
#else
#define SENSORDB(x, ...)
#endif

struct {
	kal_bool NightMode;
	kal_uint8 ZoomFactor;	/* Zoom Index */
	kal_uint16 Banding;
	kal_uint32 PvShutter;
	kal_uint32 PvDummyPixels;
	kal_uint32 PvDummyLines;
	kal_uint32 CapDummyPixels;
	kal_uint32 CapDummyLines;
	kal_uint32 PvOpClk;
	kal_uint32 CapOpClk;

	/* Video frame rate 300 means 30.0fps. Unit Multiple 10. */
	kal_uint32 MaxFrameRate;
	kal_uint32 MiniFrameRate;
	/* Sensor Register backup. */
	kal_uint8 VDOCTL2;	/* P0.0x11. */
	kal_uint8 ISPCTL3;	/* P10.0x12. */
	kal_uint8 ISPCTL4;	/* P10.0x13. */
	kal_uint8 AECTL1;	/* P20.0x10. */
	kal_uint8 AWBCTL1;	/* P22.0x10. */
	kal_uint8 SATCTL;	/* P10.0x60. */

	kal_uint8 wb;		/* P22.0x10. */
	kal_uint8 Effect;
	kal_uint8 Brightness;
} HI253Status;

kal_uint8 HI253sensor_socket = DUAL_CAMERA_NONE_SENSOR;
MSDK_SCENARIO_ID_ENUM HI253_CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

kal_bool HI253_MPEG4_encode_mode = KAL_FALSE;
UINT32 last_central_p_x, last_central_p_y;

static DEFINE_SPINLOCK(hi253_drv_lock);


#define Sleep(ms) mdelay(ms)

kal_uint16 HI253WriteCmosSensor(kal_uint32 Addr, kal_uint32 Para)
{
	char pSendCmd[2] = { (char)(Addr & 0xFF), (char)(Para & 0xFF) };

	iWriteRegI2C(pSendCmd, 2, HI253_WRITE_ID);

	return 0;
}

/* #define USE_I2C_BURST_WRITE */
#ifdef USE_I2C_BURST_WRITE
#define I2C_BUFFER_LEN 254	/* MAX data to send by MT6572 i2c dma mode is 255 bytes */
#define BLOCK_I2C_DATA_WRITE iBurstWriteReg
#else
#define I2C_BUFFER_LEN 8	/* MT6572 i2s bus master fifo length is 8 bytes */
#define BLOCK_I2C_DATA_WRITE iWriteRegI2C
#endif

/* {addr, data} pair in para */
/* len is the total length of addr+data */
/* Using I2C multiple/burst write if the addr increase by 1 */
static kal_uint16 HI253_table_write_cmos_sensor(const kal_uint8 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];	/* at most 2 bytes address and 6 bytes data for multiple write. MTK i2c master has only 8bytes fifo. */
	kal_uint32 tosend, IDX;
	kal_uint8 addr, addr_last, data;

	tosend = 0;
	IDX = 0;
	while (IDX < len) {
		addr = para[IDX];

		if (tosend == 0) {	/* new (addr, data) to send */
			puSendCmd[tosend++] = (char)addr;
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)data;
			IDX += 2;
			addr_last = addr;
		} else if (addr == addr_last + 1) {	/* to multiple write the data to the incremental address */
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)data;
			IDX += 2;
			addr_last++;
		}
		/* to send out the data if the sen buffer is full or last data or to program to the address not incremental. */
		if (tosend == I2C_BUFFER_LEN || IDX == len || addr != addr_last) {
			BLOCK_I2C_DATA_WRITE(puSendCmd, tosend, HI253_WRITE_ID);
			tosend = 0;
		}
	}
	return 0;
}

kal_uint16 HI253ReadCmosSensor(kal_uint32 Addr)
{
	char pGetByte = 0;
	char pSendCmd = (char)(Addr & 0xFF);

	iReadRegI2C(&pSendCmd, 1, &pGetByte, 1, HI253_WRITE_ID);

	return pGetByte;
}

void HI253SetPage(kal_uint8 Page)
{
	HI253WriteCmosSensor(0x03, Page);
}

int HI253_SEN_RUN_TEST_PATTERN = 0;
#define HI253_TEST_PATTERN_CHECKSUM 0x825a0448
UINT32 HI253SetTestPatternMode(kal_bool bEnable)
{
	/* SENSORDB("[HI253SetTestPatternMode] Fail: Not Support. Test pattern enable:%d\n", bEnable); */
	if (bEnable) {
		/* output color bar */
		HI253SetPage(0x00);
		HI253WriteCmosSensor(0x50, 0x04);	/* Sleep ON */
	} else {
		HI253SetPage(0x00);
		HI253WriteCmosSensor(0x50, 0x00);	/* Sleep ON */
	}
	return ERROR_NONE;
}

void HI253InitSetting(void)
{
	/*[SENSOR_INITIALIZATION]
	   DISP_DATE = "2010-11-24 "
	   DISP_WIDTH = 800
	   DISP_HEIGHT = 600
	   DISP_FORMAT = YUV422
	   DISP_DATAORDER = YUYV
	   MCLK = 26.00
	   PLL = 2.00
	   BEGIN */
	SENSORDB("[HI253] Sensor Init...\n");

	HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */
	HI253WriteCmosSensor(0x08, 0x0f);	/* Hi-Z ON */
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

	HI253SetPage(0x00);	/* Dummy 750us START */
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);	/* Dummy 750us END */

	HI253WriteCmosSensor(0x0e, 0x00);	/* PLL */

	HI253SetPage(0x00);	/* Dummy 750us START */
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);	/* Dummy 750us END */

	HI253WriteCmosSensor(0x0e, 0x00);	/* PLL OFF */
	HI253WriteCmosSensor(0x01, 0xf1);	/* Sleep ON */
	HI253WriteCmosSensor(0x08, 0x00);	/* Hi-Z OFF */
	HI253WriteCmosSensor(0x01, 0xf3);
	HI253WriteCmosSensor(0x01, 0xf1);

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x10, 0x0c);	/* AE OFF */
	HI253SetPage(0x22);
	HI253WriteCmosSensor(0x10, 0x69);	/* AWB OFF */

	HI253SetPage(0x00);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x13,	/* Sub1/2_Preview2 */
			0x11, 0x90,	/* Windowing ON, 1Frame Skip */
			0x12, 0x04,	/* 00:Rinsing edge 04:fall edge */
			0x0b, 0xaa,	/* ESD Check Register */
			0x0c, 0xaa,	/* ESD Check Register */
			0x0d, 0xaa,	/* ESD Check Register */
			0x20, 0x00,	/* WINROWH */
			0x21, 0x04,	/* WINROWL */
			0x22, 0x00,	/* WINCOLH */
			0x23, 0x07,	/* WINCOLL */
			0x24, 0x04,	/* WINHGTH */
			0x25, 0xb0,	/* WINHGTL */
			0x26, 0x06,	/* WINWIDH */
			0x27, 0x40,	/* WINWIDL */

			0x40, 0x01,	/* HBLANKH 424 */
			0x41, 0xa8,	/* HBLANKL */
			0x42, 0x00,	/* VSYNCH 62 */
			0x43, 0x3e,	/* VSYNCL */

			0x45, 0x04,	/* VSCTL1 */
			0x46, 0x18,	/* VSCTL2 */
			0x47, 0xd8,	/* VSCTL3 */

			0xe1, 0x0f,

			/* BLC */
			0x80, 0x2e,	/* BLCCTL */
			0x81, 0x7e,
			0x82, 0x90,
			0x83, 0x00,
			0x84, 0x0c,
			0x85, 0x00,
			0x90, 0x0a,	/* BLCTIMETH ON */
			0x91, 0x0a,	/* BLCTIMETH OFF */
			0x92, 0x78,	/* BLCAGTH ON */
			0x93, 0x70,	/* BLCAGTH OFF */
			/* 0x92, 0xd8, // BLCAGTH ON */
			/* 0x93, 0xd0, // BLCAGTH OFF */
			0x94, 0x75,	/* BLCDGTH ON */
			0x95, 0x70,	/* BLCDGTH OFF */
			0x96, 0xdc,
			0x97, 0xfe,
			0x98, 0x20,

			/* OutDoor BLC */
			0x99, 0x42,	/* B */
			0x9a, 0x42,	/* Gb */
			0x9b, 0x42,	/* R */
			0x9c, 0x42,	/* Gr */

			/* Dark BLC */
			0xa0, 0x00,	/* BLCOFS DB */
			0xa2, 0x00,	/* BLCOFS DGb */
			0xa4, 0x00,	/* BLCOFS DR */
			0xa6, 0x00,	/* BLCOFS DGr */

			/* Normal BLC */
			0xa8, 0x43,
			0xaa, 0x43,
			0xac, 0x43,
			0xae, 0x43,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x02);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x12, 0x03,
			0x13, 0x03,
			0x16, 0x00,
			0x17, 0x8C,
			0x18, 0x4c,	/* 0x28->0x2c->4c [20100919 update] */
			0x19, 0x00,	/* 0x40->0x00 [20100912 update] */
			0x1a, 0x39,
			0x1c, 0x09,
			0x1d, 0x40,
			0x1e, 0x30,
			0x1f, 0x10,
			0x20, 0x77,
			0x21, 0xde,	/* 0xdd->0xde [20100919 update] */
			0x22, 0xa7,
			0x23, 0x30,	/* 0xb0->0x30 [20100912 update] */
			0x27, 0x3c,
			0x2b, 0x80,
			0x2e, 0x00,	/* 100913 power saving Hy gou 11}, */
			0x2f, 0x00,	/* 100913 power saving Hy gou a1}, */
			0x30, 0x05,
			0x50, 0x20,
			0x52, 0x01,
			0x53, 0xc1,
			0x55, 0x1c,
			0x56, 0x11,	/* 0x00->0x11 [20100912 update] */
			0x5d, 0xA2,
			0x5e, 0x5a,
			0x60, 0x87,
			0x61, 0x99,
			0x62, 0x88,
			0x63, 0x97,
			0x64, 0x88,
			0x65, 0x97,
			0x67, 0x0c,
			0x68, 0x0c,
			0x69, 0x0c,
			0x72, 0x89,
			0x73, 0x96,	/* 0x97->0x96 [20100919 update] */
			0x74, 0x89,
			0x75, 0x96,	/* 0x97->0x96 [20100919 update] */
			0x76, 0x89,
			0x77, 0x96,	/* 0x97->0x96 [20100912 update] */
			0x7C, 0x85,
			0x7d, 0xaf,
			0x80, 0x01,
			0x81, 0x7f,	/* 0x81->0x7f [20100919 update] */
			0x82, 0x13,	/* 0x23->0x13 [20100912 update] */
			0x83, 0x24,	/* 0x2b->0x24 [20100912 update] */
			0x84, 0x7d,
			0x85, 0x81,
			0x86, 0x7d,
			0x87, 0x81,
			0x92, 0x48,	/* 0x53->0x48 [20100912 update] */
			0x93, 0x54,	/* 0x5e->0x54 [20100912 update] */
			0x94, 0x7d,
			0x95, 0x81,
			0x96, 0x7d,
			0x97, 0x81,
			0xa0, 0x02,
			0xa1, 0x7b,
			0xa2, 0x02,
			0xa3, 0x7b,
			0xa4, 0x7b,
			0xa5, 0x02,
			0xa6, 0x7b,
			0xa7, 0x02,
			0xa8, 0x85,
			0xa9, 0x8c,
			0xaa, 0x85,
			0xab, 0x8c,
			0xac, 0x10,	/* 0x20->0x10 [20100912 update] */
			0xad, 0x16,	/* 0x26->0x16 [20100912 update] */
			0xae, 0x10,	/* 0x20->0x10 [20100912 update] */
			0xaf, 0x16,	/* 0x26->0x16 [20100912 update] */
			0xb0, 0x99,
			0xb1, 0xa3,
			0xb2, 0xa4,
			0xb3, 0xae,
			0xb4, 0x9b,
			0xb5, 0xa2,
			0xb6, 0xa6,
			0xb7, 0xac,
			0xb8, 0x9b,
			0xb9, 0x9f,
			0xba, 0xa6,
			0xbb, 0xaa,
			0xbc, 0x9b,
			0xbd, 0x9f,
			0xbe, 0xa6,
			0xbf, 0xaa,
			0xc4, 0x2c,	/* 0x36->0x2c [20100912 update] */
			0xc5, 0x43,	/* 0x4e->0x43 [20100912 update] */
			0xc6, 0x63,	/* 0x61->0x63 [20100912 update] */
			0xc7, 0x79,	/* 0x78->0x79 [20100919 update] */
			0xc8, 0x2d,	/* 0x36->0x2d [20100912 update] */
			0xc9, 0x42,	/* 0x4d->0x42 [20100912 update] */
			0xca, 0x2d,	/* 0x36->0x2d [20100912 update] */
			0xcb, 0x42,	/* 0x4d->0x42 [20100912 update] */
			0xcc, 0x64,	/* 0x62->0x64 [20100912 update] */
			0xcd, 0x78,
			0xce, 0x64,	/* 0x62->0x64 [20100912 update] */
			0xcf, 0x78,
			0xd0, 0x0a,
			0xd1, 0x09,
			0xd4, 0x0a,	/* DCDCTIMETHON */
			0xd5, 0x0a,	/* DCDCTIMETHOFF */
			/* 0xd6, 0x78, // DCDCAGTHON */
			/* 0xd7, 0x70, // DCDCAGTHOFF */
			0xd6, 0xd8,	/* DCDCAGTHON */
			0xd7, 0xd0,	/* DCDCAGTHOFF */
			0xe0, 0xc4,
			0xe1, 0xc4,
			0xe2, 0xc4,
			0xe3, 0xc4,
			0xe4, 0x00,
			0xe8, 0x80,	/* 0x00->0x80 [20100919 update] */
			0xe9, 0x40,
			0xea, 0x7f,	/* 0x82->0x7f [20100919 update] */
			0xf0, 0x01,	/* 100810 memory delay */
			0xf1, 0x01,	/* 100810 memory delay */
			0xf2, 0x01,	/* 100810 memory delay */
			0xf3, 0x01,	/* 100810 memory delay */
			0xf4, 0x01,	/* 100810 memory delay */
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x03);
	HI253WriteCmosSensor(0x10, 0x10);

	HI253SetPage(0x10);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x03,	/* YUYV */
			0x12, 0x30,	/* ISPCTL3 */
			0x13, 0x0a,
			0x20, 0x00,	/* ITUCTL */
			0x30, 0x00,
			0x31, 0x00,
			0x32, 0x00,
			0x33, 0x00,
			0x34, 0x30,
			0x35, 0x00,
			0x36, 0x00,
			0x38, 0x00,
			0x3e, 0x58,
			0x3f, 0x02,	/* 0x02 for preview and 0x00 for capture */
			0x40, 0x85,	/* YOFS modify brightness */
			0x41, 0x00,	/* DYOFS */
			0x48, 0x88,

			/* Saturation; */
			0x60, 0x6f,	/* SATCTL */
			0x61, 0x76,	/* SATB */
			0x62, 0x76,	/* SATR */
			0x63, 0x30,	/* AGSAT */
			0x64, 0x41,
			0x66, 0x33,	/* SATTIMETH */
			0x67, 0x00,	/* SATOUTDEL */
			0x6a, 0x90,	/* UPOSSAT */
			0x6b, 0x80,	/* UNEGSAT */
			0x6c, 0x80,	/* VPOSSAT */
			0x6d, 0xa0,	/* VNEGSAT */
			0x76, 0x01,	/* white protection ON */
			0x74, 0x66,
			0x79, 0x06,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x11);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x3f,	/* DLPFCTL1 */
			0x11, 0x40,
			0x12, 0xba,
			0x13, 0xcb,
			0x26, 0x20,	/* LPFAGTHL */
			0x27, 0x22,	/* LPFAGTHH */
			0x28, 0x0f,	/* LPFOUTTHL */
			0x29, 0x10,	/* LPFOUTTHH */
			0x2b, 0x30,	/* LPFYMEANTHL */
			0x2c, 0x32,	/* LPFYMEANTHH */

			/* Out2 D-LPF th */
			0x30, 0x70,	/* OUT2YBOUNDH */
			0x31, 0x10,	/* OUT2YBOUNDL */
			0x32, 0x65,	/* OUT2RATIO */
			0x33, 0x09,	/* OUT2THH */
			0x34, 0x06,	/* OUT2THM */
			0x35, 0x04,	/* OUT2THL */

			/* Out1 D-LPF th */
			0x36, 0x70,	/* OUT1YBOUNDH */
			0x37, 0x18,	/* OUT1YBOUNDL */
			0x38, 0x65,	/* OUT1RATIO */
			0x39, 0x09,	/* OUT1THH */
			0x3a, 0x06,	/* OUT1THM */
			0x3b, 0x04,	/* OUT1THL */

			/* Indoor D-LPF th */
			0x3c, 0x80,	/* INYBOUNDH */
			0x3d, 0x18,	/* INYBOUNDL */
			0x3e, 0x80,	/* INRATIO */
			0x3f, 0x0c,	/* INTHH */
			0x40, 0x09,	/* INTHM */
			0x41, 0x06,	/* INTHL */

			0x42, 0x80,	/* DARK1YBOUNDH */
			0x43, 0x18,	/* DARK1YBOUNDL */
			0x44, 0x80,	/* DARK1RATIO */
			0x45, 0x12,	/* DARK1THH */
			0x46, 0x10,	/* DARK1THM */
			0x47, 0x10,	/* DARK1THL */
			0x48, 0x90,	/* DARK2YBOUNDH */
			0x49, 0x40,	/* DARK2YBOUNDL */
			0x4a, 0x80,	/* DARK2RATIO */
			0x4b, 0x13,	/* DARK2THH */
			0x4c, 0x10,	/* DARK2THM */
			0x4d, 0x11,	/* DARK2THL */
			0x4e, 0x80,	/* DARK3YBOUNDH */
			0x4f, 0x30,	/* DARK3YBOUNDL */
			0x50, 0x80,	/* DARK3RATIO */
			0x51, 0x13,	/* DARK3THH */
			0x52, 0x10,	/* DARK3THM */
			0x53, 0x13,	/* DARK3THL */
			0x54, 0x11,
			0x55, 0x17,
			0x56, 0x20,
			0x57, 0x20,
			0x58, 0x20,
			0x59, 0x30,
			0x5a, 0x18,
			0x5b, 0x00,
			0x5c, 0x00,
			0x60, 0x3f,
			0x62, 0x50,
			0x70, 0x06,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x12);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x20, 0x00,	/* YCLPFCTL1 0x00 for preview and 0x0f for capture */
			0x21, 0x00,	/* YCLPFCTL2 0x00 for preview and 0x0f for capture */
			0x25, 0x30,
			0x28, 0x00,	/* Out1 BP t */
			0x29, 0x00,	/* Out2 BP t */
			0x2a, 0x00,
			0x30, 0x50,
			0x31, 0x18,	/* YCUNI1TH */
			0x32, 0x32,	/* YCUNI2TH */
			0x33, 0x40,	/* YCUNI3TH */
			0x34, 0x50,	/* YCNOR1TH */
			0x35, 0x70,	/* YCNOR2TH */
			0x36, 0xa0,	/* YCNOR3TH */
			0x3b, 0x06,
			0x3c, 0x06,

			/* Out2 th */
			0x40, 0xa0,	/* YCOUT2THH */
			0x41, 0x40,	/* YCOUT2THL */
			0x42, 0xa0,	/* YCOUT2STDH */
			0x43, 0x90,	/* YCOUT2STDM */
			0x44, 0x90,	/* YCOUT2STDL */
			0x45, 0x80,	/* YCOUT2RAT */

			/* Out1 th */
			0x46, 0xb0,	/* YCOUT1THH */
			0x47, 0x55,	/* YCOUT1THL */
			0x48, 0xa0,	/* YCOUT1STDH */
			0x49, 0x90,	/* YCOUT1STDM */
			0x4a, 0x90,	/* YCOUT1STDL */
			0x4b, 0x80,	/* YCOUT1RAT */

			/* In door th */
			0x4c, 0xb0,	/* YCINTHH */
			0x4d, 0x40,	/* YCINTHL */
			0x4e, 0x90,	/* YCINSTDH */
			0x4f, 0x90,	/* YCINSTDM */
			0x50, 0xe6,	/* YCINSTDL */
			0x51, 0x80,	/* YCINRAT */

			/* Dark1 th */
			0x52, 0xb0,	/* YCDARK1THH */
			0x53, 0x60,	/* YCDARK1THL */
			0x54, 0xc0,	/* YCDARK1STDH */
			0x55, 0xc0,	/* YCDARK1STDM */
			0x56, 0xc0,	/* YCDARK1STDL */
			0x57, 0x80,	/* YCDARK1RAT */

			/* Dark2 th */
			0x58, 0x90,	/* YCDARK2THH */
			0x59, 0x40,	/* YCDARK2THL */
			0x5a, 0xd0,	/* YCDARK2STDH */
			0x5b, 0xd0,	/* YCDARK2STDM */
			0x5c, 0xe0,	/* YCDARK2STDL */
			0x5d, 0x80,	/* YCDARK2RAT */

			/* Dark3 th */
			0x5e, 0x88,	/* YCDARK3THH */
			0x5f, 0x40,	/* YCDARK3THL */
			0x60, 0xe0,	/* YCDARK3STDH */
			0x61, 0xe6,	/* YCDARK3STDM */
			0x62, 0xe6,	/* YCDARK3STDL */
			0x63, 0x80,	/* YCDARK3RAT */

			0x70, 0x15,
			0x71, 0x01,	/* Don't Touch register */

			0x72, 0x18,
			0x73, 0x01,	/* Don't Touch register */

			0x74, 0x25,
			0x75, 0x15,
			0x80, 0x30,
			0x81, 0x50,
			0x82, 0x80,
			0x85, 0x1a,
			0x88, 0x00,
			0x89, 0x00,
			0x90, 0x00,	/* DPCCTL 0x00 For Preview and 0x5d for capture */

			0xc5, 0x30,
			0xc6, 0x2a,

			/* Dont Touch register */
			0xD0, 0x0c,
			0xD1, 0x80,
			0xD2, 0x67,
			0xD3, 0x00,
			0xD4, 0x00,
			0xD5, 0x02,
			0xD6, 0xff,
			0xD7, 0x18,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x13);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0xcb,	/* EDGECTL1 */
			0x11, 0x7b,	/* EDGECTL2 */
			0x12, 0x07,	/* EDGECTL3 */
			0x14, 0x00,	/* EDGECTL5 */

			0x20, 0x15,	/* EDGENGAIN */
			0x21, 0x13,	/* EDGEPGAIN */
			0x22, 0x33,
			0x23, 0x04,	/* EDGEHCLIP1TH */
			0x24, 0x09,	/* EDGEHCLIP2TH */
			0x25, 0x08,	/* EDGELCLIPTH */
			0x26, 0x18,	/* EDGELCLIPLMT */
			0x27, 0x30,
			0x29, 0x12,	/* EDGETIMETH */
			0x2a, 0x50,	/* EDGEAGTH */

			/* Low clip th */
			0x2b, 0x06,
			0x2c, 0x06,
			0x25, 0x08,
			0x2d, 0x0c,
			0x2e, 0x12,
			0x2f, 0x12,

			/* Out2 Edge */
			0x50, 0x10,
			0x51, 0x14,
			0x52, 0x10,
			0x53, 0x0c,
			0x54, 0x0f,
			0x55, 0x0c,

			/* Out1 Edge */
			0x56, 0x10,
			0x57, 0x13,
			0x58, 0x10,
			0x59, 0x0c,
			0x5a, 0x0f,
			0x5b, 0x0c,

			/* Indoor Edg */
			0x5c, 0x0a,
			0x5d, 0x0b,
			0x5e, 0x0a,
			0x5f, 0x08,
			0x60, 0x09,
			0x61, 0x08,

			/* Dark1 Edge */
			0x62, 0x08,
			0x63, 0x08,
			0x64, 0x08,
			0x65, 0x06,
			0x66, 0x06,
			0x67, 0x06,

			/* Dark2 Edge */
			0x68, 0x07,
			0x69, 0x07,
			0x6a, 0x07,
			0x6b, 0x05,
			0x6c, 0x05,
			0x6d, 0x05,

			/* Dark3 Edge */
			0x6e, 0x07,
			0x6f, 0x07,
			0x70, 0x07,
			0x71, 0x05,
			0x72, 0x05,
			0x73, 0x05,

			/* 2DY */
			0x80, 0x00,	/* EDGE2DCTL1 00 for preview, must turn on 2DY 0xfd when capture */
			0x81, 0x1f,	/* EDGE2DCTL2 */
			0x82, 0x05,	/* EDGE2DCTL3 */
			0x83, 0x01,	/* EDGE2DCTL4 */

			0x90, 0x05,	/* EDGE2DNGAIN */
			0x91, 0x05,	/* EDGE2DPGAIN */
			0x92, 0x33,	/* EDGE2DLCLIPLMT */
			0x93, 0x30,
			0x94, 0x03,	/* EDGE2DHCLIP1TH */
			0x95, 0x14,	/* EDGE2DHCLIP2TH */
			0x97, 0x30,
			0x99, 0x30,

			0xa0, 0x04,	/* EDGE2DLCOUT2N */
			0xa1, 0x05,	/* EDGE2DLCOUT2P */
			0xa2, 0x04,	/* EDGE2DLCOUT1N */
			0xa3, 0x05,	/* EDGE2DLCOUT1P */
			0xa4, 0x07,	/* EDGE2DLCINN */
			0xa5, 0x08,	/* EDGE2DLCINP */
			0xa6, 0x07,	/* EDGE2DLCD1N */
			0xa7, 0x08,	/* EDGE2DLCD1P */
			0xa8, 0x07,	/* EDGE2DLCD2N */
			0xa9, 0x08,	/* EDGE2DLCD2P */
			0xaa, 0x07,	/* EDGE2DLCD3N */
			0xab, 0x08,	/* EDGE2DLCD3P */

			/* Out2 */
			0xb0, 0x22,
			0xb1, 0x2a,
			0xb2, 0x28,
			0xb3, 0x22,
			0xb4, 0x2a,
			0xb5, 0x28,

			/* Out1 */
			0xb6, 0x22,
			0xb7, 0x2a,
			0xb8, 0x28,
			0xb9, 0x22,
			0xba, 0x2a,
			0xbb, 0x28,

			0xbc, 0x17,
			0xbd, 0x17,
			0xbe, 0x17,
			0xbf, 0x17,
			0xc0, 0x17,
			0xc1, 0x17,

			/* Dark1 */
			0xc2, 0x1e,
			0xc3, 0x12,
			0xc4, 0x10,
			0xc5, 0x1e,
			0xc6, 0x12,
			0xc7, 0x10,

			/* Dark2 */
			0xc8, 0x18,
			0xc9, 0x05,
			0xca, 0x05,
			0xcb, 0x18,
			0xcc, 0x05,
			0xcd, 0x05,

			/* Dark3 */
			0xce, 0x18,
			0xcf, 0x05,
			0xd0, 0x05,
			0xd1, 0x18,
			0xd2, 0x05,
			0xd3, 0x05,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x14);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x11,	/* LENSCTL1 */
			0x20, 0x40,	/* XCEN */
			0x21, 0x80,	/* YCEN */
			0x22, 0x80,	/* LENSRGAIN */
			0x23, 0x80,	/* LENSGGAIN */
			0x24, 0x80,	/* LENSBGAIN */

			0x30, 0xc8,
			0x31, 0x2b,
			0x32, 0x00,
			0x33, 0x00,
			0x34, 0x90,

			0x40, 0x65,	/* LENSRP0 */
			0x50, 0x42,	/* LENSGrP0 */
			0x60, 0x3a,	/* LENSBP0 */
			0x70, 0x42,	/* LENSGbP0 */
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x15);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x0f,	/* CMCCTL */
			0x14, 0x52,	/* CMCOFSGH */
			0x15, 0x42,	/* CMCOFSGM */
			0x16, 0x32,	/* CMCOFSGL */
			0x17, 0x2f,	/* CMCSIGN */

			/* CMC */
			0x30, 0x8f,	/* CMC11 */
			0x31, 0x59,	/* CMC12 */
			0x32, 0x0a,	/* CMC13 */
			0x33, 0x15,	/* CMC21 */
			0x34, 0x5b,	/* CMC22 */
			0x35, 0x06,	/* CMC23 */
			0x36, 0x07,	/* CMC31 */
			0x37, 0x40,	/* CMC32 */
			0x38, 0x86,	/* CMC33 */

			/* CMC OFS */
			0x40, 0x95,	/* CMCOFSL11 */
			0x41, 0x1f,	/* CMCOFSL12 */
			0x42, 0x8a,	/* CMCOFSL13 */
			0x43, 0x86,	/* CMCOFSL21 */
			0x44, 0x0a,	/* CMCOFSL22 */
			0x45, 0x84,	/* CMCOFSL23 */
			0x46, 0x87,	/* CMCOFSL31 */
			0x47, 0x9b,	/* CMCOFSL32 */
			0x48, 0x23,	/* CMCOFSL33 */

			/* CMC POFS */
			0x50, 0x8c,	/* CMCOFSH11 */
			0x51, 0x0c,	/* CMCOFSH12 */
			0x52, 0x00,	/* CMCOFSH13 */
			0x53, 0x07,	/* CMCOFSH21 */
			0x54, 0x17,	/* CMCOFSH22 */
			0x55, 0x9d,	/* CMCOFSH23 */
			0x56, 0x00,	/* CMCOFSH31 */
			0x57, 0x0b,	/* CMCOFSH32 */
			0x58, 0x89,	/* CMCOFSH33 */

			0x80, 0x03,
			0x85, 0x40,
			0x87, 0x02,
			0x88, 0x00,
			0x89, 0x00,
			0x8a, 0x00,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x16);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x31,	/* GMACTL */
			0x18, 0x37,
			0x19, 0x36,
			0x1a, 0x0e,
			0x1b, 0x01,
			0x1c, 0xdc,
			0x1d, 0xfe,

			0x30, 0x00,	/* GGMA0 */
			0x31, 0x06,	/* GGMA1 */
			0x32, 0x1d,	/* GGMA2 */
			0x33, 0x33,	/* GGMA3 */
			0x34, 0x53,	/* GGMA4 */
			0x35, 0x6c,	/* GGMA5 */
			0x36, 0x81,	/* GGMA6 */
			0x37, 0x94,	/* GGMA7 */
			0x38, 0xa4,	/* GGMA8 */
			0x39, 0xb3,	/* GGMA9 */
			0x3a, 0xc0,	/* GGMA10 */
			0x3b, 0xcb,	/* GGMA11 */
			0x3c, 0xd5,	/* GGMA12 */
			0x3d, 0xde,	/* GGMA13 */
			0x3e, 0xe6,	/* GGMA14 */
			0x3f, 0xee,	/* GGMA15 */
			0x40, 0xf5,	/* GGMA16 */
			0x41, 0xfc,	/* GGMA17 */
			0x42, 0xff,	/* GGMA18 */

			0x50, 0x00,	/* RGMA0 */
			0x51, 0x03,	/* RGMA1 */
			0x52, 0x19,	/* RGMA2 */
			0x53, 0x34,	/* RGMA3 */
			0x54, 0x58,	/* RGMA4 */
			0x55, 0x75,	/* RGMA5 */
			0x56, 0x8d,	/* RGMA6 */
			0x57, 0xa1,	/* RGMA7 */
			0x58, 0xb2,	/* RGMA8 */
			0x59, 0xbe,	/* RGMA9 */
			0x5a, 0xc9,	/* RGMA10 */
			0x5b, 0xd2,	/* RGMA11 */
			0x5c, 0xdb,	/* RGMA12 */
			0x5d, 0xe3,	/* RGMA13 */
			0x5e, 0xeb,	/* RGMA14 */
			0x5f, 0xf0,	/* RGMA15 */
			0x60, 0xf5,	/* RGMA16 */
			0x61, 0xf7,	/* RGMA17 */
			0x62, 0xf8,	/* RGMA18 */

			0x70, 0x00,	/* BGMA0 */
			0x71, 0x08,	/* BGMA1 */
			0x72, 0x17,	/* BGMA2 */
			0x73, 0x2f,	/* BGMA3 */
			0x74, 0x53,	/* BGMA4 */
			0x75, 0x6c,	/* BGMA5 */
			0x76, 0x81,	/* BGMA6 */
			0x77, 0x94,	/* BGMA7 */
			0x78, 0xa4,	/* BGMA8 */
			0x79, 0xb3,	/* BGMA9 */
			0x7a, 0xc0,	/* BGMA10 */
			0x7b, 0xcb,	/* BGMA11 */
			0x7c, 0xd5,	/* BGMA12 */
			0x7d, 0xde,	/* BGMA13 */
			0x7e, 0xe6,	/* BGMA14 */
			0x7f, 0xee,	/* BGMA15 */
			0x80, 0xf4,	/* BGMA16 */
			0x81, 0xfa,	/* BGMA17 */
			0x82, 0xff,	/* BGMA18 */
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x17);
	{
		static const kal_uint8 addr_data_pair[] = {
			0xc4, 0x68,	/* FLK200 */
			0xc5, 0x56,	/* FLK240 */
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x20);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x11, 0x1c,
			0x20, 0x01,	/* AEFRAMECTL lowtemp off */
			0x21, 0x30,
			0x22, 0x10,
			0x23, 0x00,
			0x24, 0x04,

			0x28, 0xff,
			0x29, 0xad,

			/* MTK set up anti banding -- > 1/100s */
			0x2a, 0xf0,
			0x2b, 0x34,
			0x2c, 0xc3,
			0x2d, 0x5f,
			0x2e, 0x33,
			0x30, 0x78,
			0x32, 0x03,
			0x33, 0x2e,
			0x34, 0x30,
			0x35, 0xd4,
			0x36, 0xfe,
			0x37, 0x32,
			0x38, 0x04,
			0x3b, 0x22,
			0x3c, 0xef,
			0x47, 0xf0,

			/* Y_Frame TH */
			0x50, 0x45,
			0x51, 0x88,

			0x56, 0x10,
			0x57, 0xb7,
			0x58, 0x14,
			0x59, 0x88,
			0x5a, 0x04,

			0x60, 0x55,	/* AEWGT1 */
			0x61, 0x55,	/* AEWGT2 */
			0x62, 0x6a,	/* AEWGT3 */
			0x63, 0xa9,	/* AEWGT4 */
			0x64, 0x6a,	/* AEWGT5 */
			0x65, 0xa9,	/* AEWGT6 */
			0x66, 0x6a,	/* AEWGT7 */
			0x67, 0xa9,	/* AEWGT8 */
			0x68, 0x6b,	/* AEWGT9 */
			0x69, 0xe9,	/* AEWGT10 */
			0x6a, 0x6a,	/* AEWGT11 */
			0x6b, 0xa9,	/* AEWGT12 */
			0x6c, 0x6a,	/* AEWGT13 */
			0x6d, 0xa9,	/* AEWGT14 */
			0x6e, 0x55,	/* AEWGT15 */
			0x6f, 0x55,	/* AEWGT16 */
			0x70, 0x46,	/* YLVL */
			0x71, 0xBb,

			/* haunting control */
			0x76, 0x21,
			0x77, 0xBC,	/* 02}, */
			0x78, 0x34,	/* 22}, // YTH1 */
			0x79, 0x3a,	/* 2a}, // YTH2HI */


			0x7a, 0x23,
			0x7b, 0x22,
			0x7d, 0x23,
			0x83, 0x01,	/* EXP Normal 33.33 fps */
			0x84, 0x7c,
			0x85, 0x40,
			0x86, 0x01,	/* EXPMin 10416.67 fps */
			0x87, 0x38,
			0x88, 0x04,	/* EXP Max 10.00 fps */
			0x89, 0xf3,
			0x8a, 0x80,
			0x8B, 0x7e,	/* EXP100 */
			0x8C, 0xc0,
			0x8D, 0x69,	/* EXP120 */
			0x8E, 0x6c,
			0x91, 0x05,
			0x92, 0xe9,
			0x93, 0xac,
			0x94, 0x04,
			0x95, 0x32,
			0x96, 0x38,
			0x98, 0xdc,	/* EXPOUT1 DC 9d out target th */
			0x99, 0x45,	/* EXPOUT2 */
			0x9a, 0x0d,
			0x9b, 0xde,

			0x9c, 0x07,	/* EXP Limit 1736.11 fps */
			0x9d, 0x50,
			0x9e, 0x01,	/* EXP Unit */
			0x9f, 0x38,
			0xa0, 0x03,
			0xa1, 0xa9,
			0xa2, 0x80,
			0xb0, 0x1d,	/* AG */
			0xb1, 0x1a,	/* AGMIN */
			0xb2, 0x80,	/* AGMAX */
			0xb3, 0x20,	/* AGLVLH //1a */
			0xb4, 0x1a,	/* AGTH1 */
			0xb5, 0x44,	/* AGTH2 */
			0xb6, 0x2f,	/* AGBTH1 */
			0xb7, 0x28,	/* AGBTH2 */
			0xb8, 0x25,	/* AGBTH3 */
			0xb9, 0x22,	/* AGBTH4 */
			0xba, 0x21,	/* AGBTH5 */
			0xbb, 0x20,	/* AGBTH6 */
			0xbc, 0x1f,	/* AGBTH7 */
			0xbd, 0x1f,	/* AGBTH8 */
			0xc0, 0x30,	/* AGSKY */
			0xc1, 0x20,
			0xc2, 0x20,
			0xc3, 0x20,
			0xc4, 0x08,	/* AGTIMETH */
			0xc8, 0x80,	/* DGMAX */
			0xc9, 0x40,	/* DGMIN */
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x22);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0xfd,	/* AWBCTL1 */
			0x11, 0x2e,	/* AWBCTL2 */
			0x19, 0x01,
			0x20, 0x30,
			0x21, 0x80,
			0x23, 0x08,
			0x24, 0x01,

			0x30, 0x80,	/* ULVL */
			0x31, 0x80,	/* VLVL */
			0x38, 0x11,	/* UVTH1 */
			0x39, 0x34,	/* UVTH2 */
			0x40, 0xf7,	/* YRANGE */

			0x41, 0x77,	/* CDIFF */
			0x42, 0x55,	/* CSUM2 */
			0x43, 0xf0,
			0x44, 0x66,
			0x45, 0x33,
			0x46, 0x01,	/* WHTPXLTH1 */
			0x47, 0x94,

			0x50, 0xb2,
			0x51, 0x81,
			0x52, 0x98,

			0x80, 0x38,	/* RGAIN */
			0x81, 0x20,	/* GGAIN */
			0x82, 0x38,	/* BGAIN */

			0x83, 0x5e,	/* RMAX */
			0x84, 0x20,	/* RMIN */
			0x85, 0x4e,	/* BMAX */
			0x86, 0x15,	/* BMIN */

			0x87, 0x54,	/* RMAXM */
			0x88, 0x20,	/* RMINM */
			0x89, 0x3f,	/* BMAXM */
			0x8a, 0x1c,	/* BMINM */

			0x8b, 0x54,	/* RMAXB */
			0x8c, 0x3f,	/* RMINB */
			0x8d, 0x24,	/* BMAXB */
			0x8e, 0x1c,	/* BMINB */

			0x8f, 0x60,	/* BGAINPARA1 */
			0x90, 0x5f,	/* BGAINPARA2 */
			0x91, 0x5c,	/* BGAINPARA3 */
			0x92, 0x4C,	/* BGAINPARA4 */
			0x93, 0x41,	/* BGAINPARA5 */
			0x94, 0x3b,	/* BGAINPARA6 */
			0x95, 0x36,	/* BGAINPARA7 */
			0x96, 0x30,	/* BGAINPARA8 */
			0x97, 0x27,	/* BGAINPARA9 */
			0x98, 0x20,	/* BGAINPARA10 */
			0x99, 0x1C,	/* BGAINPARA11 */
			0x9a, 0x19,	/* BGAINPARA12 */

			0x9b, 0x88,	/* BGAINBND1 */
			0x9c, 0x88,	/* BGAINBND2 */
			0x9d, 0x48,	/* RGAINTH1 */
			0x9e, 0x38,	/* RGAINTH2 */
			0x9f, 0x30,	/* RGAINTH3 */

			0xa0, 0x74,	/* RDELTA1 */
			0xa1, 0x35,	/* BDELTA1 */
			0xa2, 0xaf,	/* RDELTA2 */
			0xa3, 0xf7,	/* BDELTA2 */

			0xa4, 0x10,	/* AWBEXPLMT1 */
			0xa5, 0x50,	/* AWBEXPLMT2 */
			0xa6, 0xc4,	/* AWBEXPLMT3 */

			0xad, 0x40,
			0xae, 0x4a,

			0xaf, 0x2a,
			0xb0, 0x29,

			0xb1, 0x20,
			0xb4, 0xff,
			0xb8, 0x6b,
			0xb9, 0x00,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x24);
	{
		static const kal_uint8 addr_data_pair[] = {
			0x10, 0x01,	/* AFCTL1 */
			0x18, 0x06,
			0x30, 0x06,
			0x31, 0x90,
			0x32, 0x25,
			0x33, 0xa2,
			0x34, 0x26,
			0x35, 0x58,
			0x36, 0x60,
			0x37, 0x00,
			0x38, 0x50,
			0x39, 0x00,
		};
		HI253_table_write_cmos_sensor(addr_data_pair,
					      sizeof(addr_data_pair) / sizeof(kal_uint8));
	}

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON 50Hz */
	HI253SetPage(0x22);
	HI253WriteCmosSensor(0x10, 0xe9);	/* AWB ON */

	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x0e, 0x03);	/* PLL */
	HI253WriteCmosSensor(0x0e, 0x73);	/* PLL ON x2 */

	HI253SetPage(0x00);	/* Dummy 750us START */
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);
	HI253SetPage(0x00);	/* Dummy 750us END */

	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */
	SENSORDB("[HI253] Sensor Init Done\n");

	/*[END] */
}

void HI253InitPara(void)
{
	spin_lock(&hi253_drv_lock);
	HI253Status.NightMode = KAL_FALSE;
	HI253Status.ZoomFactor = 0;
	HI253Status.Banding = AE_FLICKER_MODE_50HZ;
	HI253Status.PvShutter = 0x17c40;
	HI253Status.MaxFrameRate = HI253_MAX_FPS;
	HI253Status.MiniFrameRate = HI253_FPS(10);
	HI253Status.PvDummyPixels = 424;
	HI253Status.PvDummyLines = 62;
	HI253Status.CapDummyPixels = 360;
	HI253Status.CapDummyLines = 52;	/* 10 FPS, 104 for 9.6 FPS */
	HI253Status.PvOpClk = 26;
	HI253Status.CapOpClk = 26;
	HI253Status.VDOCTL2 = 0x90;
	HI253Status.ISPCTL3 = 0x30;
	HI253Status.ISPCTL4 = 0x00;	/* SATCTL */
	HI253Status.SATCTL = 0x6f;	/* SATCTL */

	HI253Status.AECTL1 = 0x9c;
	HI253Status.AWBCTL1 = 0xe9;
	HI253Status.Effect = MEFFECT_OFF;
	HI253Status.Brightness = 0;



	HI253_MPEG4_encode_mode = KAL_FALSE;
	HI253_CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

	last_central_p_x = 2;
	last_central_p_y = 2;

	spin_unlock(&hi253_drv_lock);
}

/*************************************************************************
* FUNCTION
*  HI253SetMirror
*
* DESCRIPTION
*  This function mirror, flip or mirror & flip the sensor output image.
*
*  IMPORTANT NOTICE: For some sensor, it need re-set the output order Y1CbY2Cr after
*  mirror or flip.
*
* PARAMETERS
*  1. kal_uint16 : horizontal mirror or vertical flip direction.
*
* RETURNS
*  None
*
*************************************************************************/
static void HI253SetMirror(kal_uint16 ImageMirror)
{
	spin_lock(&hi253_drv_lock);
	HI253Status.VDOCTL2 &= 0xfc;
	spin_unlock(&hi253_drv_lock);
	switch (ImageMirror) {
	case IMAGE_H_MIRROR:
		spin_lock(&hi253_drv_lock);
		HI253Status.VDOCTL2 |= 0x01;
		spin_unlock(&hi253_drv_lock);
		break;
	case IMAGE_V_MIRROR:
		spin_lock(&hi253_drv_lock);
		HI253Status.VDOCTL2 |= 0x02;
		spin_unlock(&hi253_drv_lock);
		break;
	case IMAGE_HV_MIRROR:
		spin_lock(&hi253_drv_lock);
		HI253Status.VDOCTL2 |= 0x03;
		spin_unlock(&hi253_drv_lock);
		break;
	case IMAGE_NORMAL:
	default:
		spin_lock(&hi253_drv_lock);
		HI253Status.VDOCTL2 |= 0x00;
		spin_unlock(&hi253_drv_lock);
	}
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x11, HI253Status.VDOCTL2);
}

static void HI253SetAeMode(kal_bool AeEnable)
{
	SENSORDB("[HI253]HI253SetAeMode AeEnable:%d;\n", AeEnable);

	if (AeEnable == KAL_TRUE) {
		spin_lock(&hi253_drv_lock);
		HI253Status.AECTL1 |= 0x80;
		spin_unlock(&hi253_drv_lock);
	} else {
		spin_lock(&hi253_drv_lock);
		HI253Status.AECTL1 &= (~0x80);
		spin_unlock(&hi253_drv_lock);
	}
	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x10, HI253Status.AECTL1);
}


static void HI253SetAwbMode(kal_bool AwbEnable)
{
	SENSORDB("[HI253]HI253SetAwbMode AwbEnable:%d;\n", AwbEnable);
	if (AwbEnable == KAL_TRUE) {
		spin_lock(&hi253_drv_lock);
		HI253Status.AWBCTL1 |= 0x80;
		spin_unlock(&hi253_drv_lock);
	} else {
		spin_lock(&hi253_drv_lock);
		HI253Status.AWBCTL1 &= (~0x80);
		spin_unlock(&hi253_drv_lock);
	}
	HI253SetPage(0x22);
	HI253WriteCmosSensor(0x10, HI253Status.AWBCTL1);
}

/*************************************************************************
* FUNCTION
* HI253NightMode
*
* DESCRIPTION
* This function night mode of HI253.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HI253NightMode(kal_bool Enable)
{
	kal_uint32 EXPMAX, EXPTIME, BLC_TIME_TH_ONOFF;
	kal_uint32 LineLength, BandingValue;
	SENSORDB("[HI253]CONTROLFLOW HI253NightMode Enable:%d;\n", Enable);
	/* Night mode only for camera preview */
	/* if (HI253Status.MaxFrameRate == HI253Status.MiniFrameRate)  return ; */
	/* if (HI253Status.MaxFrameRate == HI253Status.MiniFrameRate) */

	/* if(HI253_CurrentScenarioId == MSDK_SCENARIO_ID_VIDEO_PREVIEW ) */
	spin_lock(&hi253_drv_lock);
	HI253Status.NightMode = Enable;
	spin_unlock(&hi253_drv_lock);
	if (HI253_MPEG4_encode_mode == KAL_TRUE) {
		HI253YUVSetVideoMode(30);
		return;
	}
	if (HI253_CurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD) {

		if (HI253Status.NightMode) {
			HI253WriteCmosSensor(0x01, 0xf9);
			HI253WriteCmosSensor(0x03, 0x00);
			HI253WriteCmosSensor(0x10, 0x00);

			HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
			HI253WriteCmosSensor(0x41, 0x68);
			HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
			HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */

			HI253WriteCmosSensor(0x83, 0x09);	/* EXP Max 8.33 fps */
			HI253WriteCmosSensor(0x84, 0xeb);
			HI253WriteCmosSensor(0x85, 0x10);

			HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 8.33 fps */
			HI253WriteCmosSensor(0x89, 0xeb);
			HI253WriteCmosSensor(0x8a, 0x10);

			HI253WriteCmosSensor(0x01, 0xf8);

		} else {
			HI253WriteCmosSensor(0x01, 0xf9);
			HI253WriteCmosSensor(0x03, 0x00);
			HI253WriteCmosSensor(0x10, 0x00);

			HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
			HI253WriteCmosSensor(0x41, 0x68);
			HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
			HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */

			HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 8.33 fps */
			HI253WriteCmosSensor(0x89, 0xf3);
			HI253WriteCmosSensor(0x8a, 0x70);

			HI253WriteCmosSensor(0x01, 0xf8);


		}

		return;

	}
/* return; */
	spin_lock(&hi253_drv_lock);
	HI253Status.MiniFrameRate = Enable ? HI253_FPS(5) : HI253_FPS(10);
	spin_unlock(&hi253_drv_lock);
	LineLength = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
	/* BandingValue = (HI253Status.Banding == AE_FLICKER_MODE_50HZ) ? 100 : 120; */
	BandingValue = (HI253Status.Banding == AE_FLICKER_MODE_60HZ) ? 120 : 100;

	EXPTIME =
	    (HI253Status.PvOpClk * 1000000 / LineLength / BandingValue) * BandingValue *
	    LineLength * HI253_FRAME_RATE_UNIT / 8 / HI253Status.MaxFrameRate;
	EXPMAX =
	    (HI253Status.PvOpClk * 1000000 / LineLength / BandingValue) * BandingValue *
	    LineLength * HI253_FRAME_RATE_UNIT / 8 / HI253Status.MiniFrameRate;
	BLC_TIME_TH_ONOFF = BandingValue * HI253_FRAME_RATE_UNIT / HI253Status.MiniFrameRate;

	SENSORDB("[HI253]LineLenght:%d,BandingValue:%d;MiniFrameRaet:%d\n", LineLength,
		 BandingValue, HI253Status.MiniFrameRate);
	SENSORDB("[HI253]EXPTIME:%d; EXPMAX:%d; BLC_TIME_TH_ONOFF:%d;\n", EXPTIME, EXPMAX,
		 BLC_TIME_TH_ONOFF);
	SENSORDB("[HI253]VDOCTL2:%x; AECTL1:%x;\n", HI253Status.VDOCTL2, HI253Status.AECTL1);

/* /add */
	HI253WriteCmosSensor(0x03, 0x00);
	HI253WriteCmosSensor(0x10, 0x13);
/* / */

	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */
	spin_lock(&hi253_drv_lock);
	HI253Status.VDOCTL2 &= 0xfb;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x11, HI253Status.VDOCTL2);	/* Fixed frame rate OFF */
	HI253WriteCmosSensor(0x90, BLC_TIME_TH_ONOFF);	/* BLC_TIME_TH_ON */
	HI253WriteCmosSensor(0x91, BLC_TIME_TH_ONOFF);	/* BLC_TIME_TH_OFF */
	/* HI253WriteCmosSensor(0x92, 0x78); // BLCAGTH ON */
	/* HI253WriteCmosSensor(0x93, 0x70); // BLCAGTH OFF */
	/*if(Enable==KAL_FALSE) */
	{
		HI253WriteCmosSensor(0x92, 0x78);	/* BLCAGTH ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLCAGTH OFF */
	}
	/*else
	   {
	   HI253WriteCmosSensor(0x92, 0xd8); // BLCAGTH ON
	   HI253WriteCmosSensor(0x93, 0xd0); // BLCAGTH OFF
	   } */
	HI253SetPage(0x02);
	HI253WriteCmosSensor(0xd4, BLC_TIME_TH_ONOFF);	/* DCDC_TIME_TH_ON */
	HI253WriteCmosSensor(0xd5, BLC_TIME_TH_ONOFF);	/* DCDC_TIME_TH_OFF */
	/* HI253WriteCmosSensor(0xd6, 0x78); // DCDC_AG_TH_ON */
	/* HI253WriteCmosSensor(0xd7, 0x70); // DCDC_AG_TH_OFF */

	/*if(Enable==KAL_FALSE) */
	{
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */
	}
	/*else
	   {
	   HI253WriteCmosSensor(0xd6, 0xd8); // DCDC_AG_TH_ON
	   HI253WriteCmosSensor(0xd7, 0xd0); // DCDC_AG_TH_OFF
	   } */
	HI253SetPage(0x20);
	spin_lock(&hi253_drv_lock);
	HI253Status.AECTL1 &= (~0x80);
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x10, HI253Status.AECTL1);	/* AE ON BIT 7 */
	HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
	HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
	HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
	HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */
	/* HI253WriteCmosSensor(0x83, (EXPTIME>>16)&(0xff)); // EXPTIMEH max fps */
	/* HI253WriteCmosSensor(0x84, (EXPTIME>>8)&(0xff)); // EXPTIMEM */
	/* HI253WriteCmosSensor(0x85, (EXPTIME>>0)&(0xff)); // EXPTIMEL */
	HI253WriteCmosSensor(0x88, (EXPMAX >> 16) & (0xff));	/* EXPMAXH min fps init 0x04f380 */
	HI253WriteCmosSensor(0x89, (EXPMAX >> 8) & (0xff));	/* EXPMAXM */
	HI253WriteCmosSensor(0x8a, (EXPMAX >> 0) & (0xff));	/* EXPMAXL */
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */
	spin_lock(&hi253_drv_lock);
	HI253Status.AECTL1 |= 0x80;
	HI253Status.NightMode = Enable;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x10, HI253Status.AECTL1);	/* AE ON BIT 7 */
	HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
}				/* HI253NightMode */


/*************************************************************************
* FUNCTION
* HI253Open
*
* DESCRIPTION
* this function initialize the registers of CMOS sensor
*
* PARAMETERS
* none
*
* RETURNS
*  none
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Open(void)
{
	kal_uint16 SensorId = 0;
	/* 1 software reset sensor and wait (to sensor) */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01, 0xf1);
	HI253WriteCmosSensor(0x01, 0xf3);
	HI253WriteCmosSensor(0x01, 0xf1);

	SensorId = HI253ReadCmosSensor(0x04);
	Sleep(3);
	SENSORDB("[HI253]HI253Open: Sensor ID %x\n", SensorId);

	if (SensorId != HI253_SENSOR_ID) {
		return ERROR_SENSOR_CONNECT_FAIL;
		/* SensorId = HI253_SENSOR_ID; */
	}
	HI253InitSetting();
	HI253InitPara();
	return ERROR_NONE;

}

/* HI253Open() */

/*************************************************************************
* FUNCTION
*   HI253GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   *sensorID : return the sensor ID
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253GetSensorID(UINT32 *sensorID)
{
	/* 1 software reset sensor and wait (to sensor) */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01, 0xf1);
	HI253WriteCmosSensor(0x01, 0xf3);
	HI253WriteCmosSensor(0x01, 0xf1);

	*sensorID = HI253ReadCmosSensor(0x04);
	Sleep(3);
	SENSORDB("[HI253]HI253GetSensorID: Sensor ID %x\n", *sensorID);

	if (*sensorID != HI253_SENSOR_ID) {
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
		/* *sensorID = HI253_SENSOR_ID; */
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
* HI253Close
*
* DESCRIPTION
* This function is to turn off sensor module power.
*
* PARAMETERS
* None
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Close(void)
{
	return ERROR_NONE;
}				/* HI253Close() */

/*************************************************************************
* FUNCTION
* HI253Preview
*
* DESCRIPTION
* This function start the sensor preview.
*
* PARAMETERS
* *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 LineLength, EXP100, EXP120, EXPMIN, EXPUNIT;

	SENSORDB("\n\n\n\n\n\n");
	SENSORDB("[HI253]CONTROLFLOW HI253Preview\n");
	/* For change max frame rate only need modify HI253Status.MaxFrameRate */
	spin_lock(&hi253_drv_lock);
	HI253Status.MaxFrameRate = HI253_MAX_FPS;
	HI253_MPEG4_encode_mode = KAL_FALSE;
	spin_unlock(&hi253_drv_lock);
	HI253SetMirror(IMAGE_NORMAL);

	spin_lock(&hi253_drv_lock);
	HI253Status.PvDummyPixels = 424;
	last_central_p_x = 2;
	last_central_p_y = 2;
	spin_unlock(&hi253_drv_lock);
	LineLength = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
	spin_lock(&hi253_drv_lock);
	HI253Status.MiniFrameRate = HI253_FPS(10);
	HI253Status.PvDummyLines =
	    HI253Status.PvOpClk * 1000000 * HI253_FRAME_RATE_UNIT / LineLength /
	    HI253Status.MaxFrameRate - HI253_PV_PERIOD_LINE_NUMS;
	spin_unlock(&hi253_drv_lock);

	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x10, 0x13);
	HI253WriteCmosSensor(0x12, 0x04);
	HI253WriteCmosSensor(0x20, 0x00);	/* WINROWH */
	HI253WriteCmosSensor(0x21, 0x04);	/* WINROWL */
	HI253WriteCmosSensor(0x22, 0x00);	/* WINCOLH */
	HI253WriteCmosSensor(0x23, 0x07);	/* WINCOLL */

	HI253WriteCmosSensor(0x3f, 0x00);
	HI253WriteCmosSensor(0x40, (HI253Status.PvDummyPixels >> 8) & 0xff);
	HI253WriteCmosSensor(0x41, (HI253Status.PvDummyPixels >> 0) & 0xff);
	HI253WriteCmosSensor(0x42, (HI253Status.PvDummyLines >> 8) & 0xff);
	HI253WriteCmosSensor(0x43, (HI253Status.PvDummyLines >> 0) & 0xff);
	HI253WriteCmosSensor(0x3f, 0x02);

	HI253SetPage(0x12);
	HI253WriteCmosSensor(0x20, 0x00);
	HI253WriteCmosSensor(0x21, 0x00);
	HI253WriteCmosSensor(0x90, 0x00);
	HI253SetPage(0x13);
	HI253WriteCmosSensor(0x80, 0x00);

	EXP100 = (HI253Status.PvOpClk * 1000000 / LineLength) * LineLength / 100 / 8;
	EXP120 = (HI253Status.PvOpClk * 1000000 / LineLength) * LineLength / 120 / 8;
	EXPMIN = EXPUNIT = LineLength / 4;

	SENSORDB("[HI253]DummyPixel:%d DummyLine:%d; LineLenght:%d,Plck:%d\n",
		 HI253Status.PvDummyPixels, HI253Status.PvDummyLines, LineLength,
		 HI253Status.PvOpClk);
	SENSORDB("[HI253]EXP100:%d EXP120:%d;\n", EXP100, EXP120);

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x83, (HI253Status.PvShutter >> 16) & 0xFF);
	HI253WriteCmosSensor(0x84, (HI253Status.PvShutter >> 8) & 0xFF);
	HI253WriteCmosSensor(0x85, HI253Status.PvShutter & 0xFF);
	HI253WriteCmosSensor(0x86, (EXPMIN >> 8) & 0xff);	/* EXPMIN */
	HI253WriteCmosSensor(0x87, (EXPMIN >> 0) & 0xff);
	HI253WriteCmosSensor(0x8b, (EXP100 >> 8) & 0xff);	/* EXP100 */
	HI253WriteCmosSensor(0x8c, (EXP100 >> 0) & 0xff);
	HI253WriteCmosSensor(0x8d, (EXP120 >> 8) & 0xff);	/* EXP120 */
	HI253WriteCmosSensor(0x8e, (EXP120 >> 0) & 0xff);
	HI253WriteCmosSensor(0x9c, (HI253_PV_EXPOSURE_LIMITATION >> 8) & 0xff);
	HI253WriteCmosSensor(0x9d, (HI253_PV_EXPOSURE_LIMITATION >> 0) & 0xff);
	HI253WriteCmosSensor(0x9e, (EXPUNIT >> 8) & 0xff);	/* EXP Unit */
	HI253WriteCmosSensor(0x9f, (EXPUNIT >> 0) & 0xff);

	HI253SetAeMode(KAL_TRUE);
	HI253SetAwbMode(KAL_TRUE);
	HI253NightMode(HI253Status.NightMode);

	/* restore AE weight */
	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x60, 0x55);	/* AEWGT1 */
	HI253WriteCmosSensor(0x61, 0x55);	/* AEWGT2 */
	HI253WriteCmosSensor(0x62, 0x6a);	/* AEWGT3 */
	HI253WriteCmosSensor(0x63, 0xa9);	/* AEWGT4 */
	HI253WriteCmosSensor(0x64, 0x6a);	/* AEWGT5 */
	HI253WriteCmosSensor(0x65, 0xa9);	/* AEWGT6 */
	HI253WriteCmosSensor(0x66, 0x6a);	/* AEWGT7 */
	HI253WriteCmosSensor(0x67, 0xa9);	/* AEWGT8 */
	HI253WriteCmosSensor(0x68, 0x6b);	/* AEWGT9 */
	HI253WriteCmosSensor(0x69, 0xe9);	/* AEWGT10 */
	HI253WriteCmosSensor(0x6a, 0x6a);	/* AEWGT11 */
	HI253WriteCmosSensor(0x6b, 0xa9);	/* AEWGT12 */
	HI253WriteCmosSensor(0x6c, 0x6a);	/* AEWGT13 */
	HI253WriteCmosSensor(0x6d, 0xa9);	/* AEWGT14 */
	HI253WriteCmosSensor(0x6e, 0x55);	/* AEWGT15 */
	HI253WriteCmosSensor(0x6f, 0x55);	/* AEWGT16 */


	/* mDELAY(50); */
	return ERROR_NONE;
}				/* HI253Preview() */

UINT32 HI253Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 LineLength, EXP100, EXP120, EXPMIN, EXPUNIT, CapShutter;
	kal_uint8 ClockDivider;
	kal_uint32 temp;
	SENSORDB("\n\n\n\n\n\n");
	SENSORDB("[HI253]CONTROLFLOW HI253Capture!!!!!!!!!!!!!\n");
	SENSORDB("[HI253]Image Target Width: %d; Height: %d\n", image_window->ImageTargetWidth,
		 image_window->ImageTargetHeight);
	if ((image_window->ImageTargetWidth <= HI253_PV_WIDTH)
	    && (image_window->ImageTargetHeight <= HI253_PV_HEIGHT))
		return ERROR_NONE;	/* Less than PV Mode */

	HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */
	HI253SetAeMode(KAL_FALSE);
	HI253SetAwbMode(KAL_FALSE);

	HI253SetPage(0x20);
	temp =
	    (HI253ReadCmosSensor(0x80) << 16) | (HI253ReadCmosSensor(0x81) << 8) |
	    HI253ReadCmosSensor(0x82);
	spin_lock(&hi253_drv_lock);
	HI253Status.PvShutter = temp;
	spin_unlock(&hi253_drv_lock);

	/* 1600*1200 */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x10, 0x00);
	HI253WriteCmosSensor(0x3f, 0x00);
	HI253SetPage(0x12);
	HI253WriteCmosSensor(0x20, 0x0f);
	HI253WriteCmosSensor(0x21, 0x0f);
	HI253WriteCmosSensor(0x90, 0x5d);
	HI253SetPage(0x13);
	HI253WriteCmosSensor(0x80, 0xfd);
	/*capture 1600*1200 start x, y */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x20, 0x00);	/* WINROWH */
	HI253WriteCmosSensor(0x21, 0x0f + 2);	/* WINROWL */
	HI253WriteCmosSensor(0x22, 0x00);	/* WINCOLH */
	HI253WriteCmosSensor(0x23, 0x19);	/* WINCOLL */
	spin_lock(&hi253_drv_lock);
	HI253Status.CapDummyPixels = 360;
	HI253Status.CapDummyLines = 52;	/* 10 FPS, 104 for 9.6 FPS */
	spin_unlock(&hi253_drv_lock);
	LineLength = HI253_FULL_PERIOD_PIXEL_NUMS + HI253Status.CapDummyPixels;
	spin_lock(&hi253_drv_lock);
	HI253Status.CapOpClk = 26;
	spin_unlock(&hi253_drv_lock);
	EXP100 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 100 / 8;
	EXP120 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 120 / 8;
	EXPMIN = EXPUNIT = LineLength / 4;

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x86, (EXPMIN >> 8) & 0xff);	/* EXPMIN */
	HI253WriteCmosSensor(0x87, (EXPMIN >> 0) & 0xff);
	HI253WriteCmosSensor(0x8b, (EXP100 >> 8) & 0xff);	/* EXP100 */
	HI253WriteCmosSensor(0x8c, (EXP100 >> 0) & 0xff);
	HI253WriteCmosSensor(0x8d, (EXP120 >> 8) & 0xff);	/* EXP120 */
	HI253WriteCmosSensor(0x8e, (EXP120 >> 0) & 0xff);
	HI253WriteCmosSensor(0x9c, (HI253_FULL_EXPOSURE_LIMITATION >> 8) & 0xff);
	HI253WriteCmosSensor(0x9d, (HI253_FULL_EXPOSURE_LIMITATION >> 0) & 0xff);
	HI253WriteCmosSensor(0x9e, (EXPUNIT >> 8) & 0xff);	/* EXP Unit */
	HI253WriteCmosSensor(0x9f, (EXPUNIT >> 0) & 0xff);

	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x40, (HI253Status.CapDummyPixels >> 8) & 0xff);
	HI253WriteCmosSensor(0x41, (HI253Status.CapDummyPixels >> 0) & 0xff);
	HI253WriteCmosSensor(0x42, (HI253Status.CapDummyLines >> 8) & 0xff);
	HI253WriteCmosSensor(0x43, (HI253Status.CapDummyLines >> 0) & 0xff);

	if (HI253Status.ZoomFactor > 8) {
		ClockDivider = 1;	/* Op CLock 13M */
	} else {
		ClockDivider = 0;	/* OpCLock 26M */
	}
	SENSORDB("[HI253]ClockDivider: %d\n", ClockDivider);
	HI253WriteCmosSensor(0x12, 0x04 | ClockDivider);
	CapShutter = HI253Status.PvShutter >> ClockDivider;
	if (CapShutter < 1)
		CapShutter = 1;

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x83, (CapShutter >> 16) & 0xFF);
	HI253WriteCmosSensor(0x84, (CapShutter >> 8) & 0xFF);
	HI253WriteCmosSensor(0x85, CapShutter & 0xFF);
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */
	SENSORDB("[HI253]CapShutter: %d\n", CapShutter);
	return ERROR_NONE;
}				/* HI253Capture() */

UINT32 HI253GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	pSensorResolution->SensorFullWidth = HI253_FULL_WIDTH;
	pSensorResolution->SensorFullHeight = HI253_FULL_HEIGHT;
	pSensorResolution->SensorPreviewWidth = HI253_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight = HI253_PV_HEIGHT;



	pSensorResolution->SensorVideoWidth = HI253_PV_WIDTH;
	pSensorResolution->SensorVideoHeight = HI253_PV_HEIGHT;


	return ERROR_NONE;
}				/* HI253GetResolution() */

UINT32 HI253GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pSensorInfo->SensorPreviewResolutionX = HI253_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY = HI253_PV_HEIGHT;
	pSensorInfo->SensorFullResolutionX = HI253_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY = HI253_FULL_HEIGHT;

	pSensorInfo->SensorCameraPreviewFrameRate = 30;
/* // */
	switch (ScenarioId) {

	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pSensorInfo->SensorPreviewResolutionX = HI253_FULL_WIDTH;
		pSensorInfo->SensorPreviewResolutionY = HI253_FULL_HEIGHT;

		pSensorInfo->SensorCameraPreviewFrameRate = 15;
		break;
	default:

		pSensorInfo->SensorPreviewResolutionX = HI253_PV_WIDTH;
		pSensorInfo->SensorPreviewResolutionY = HI253_PV_HEIGHT;
		pSensorInfo->SensorCameraPreviewFrameRate = 30;
		break;

	}


/* // */


	pSensorInfo->SensorVideoFrameRate = 30;
	pSensorInfo->SensorStillCaptureFrameRate = 10;
	pSensorInfo->SensorWebCamCaptureFrameRate = 15;
	pSensorInfo->SensorResetActiveHigh = FALSE;
	pSensorInfo->SensorResetDelayCount = 1;
	pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_YUYV;	/* back for 16 SENSOR_OUTPUT_FORMAT_UYVY; */
	pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;

	/*pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].ISOSupported=TRUE;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].BinningEnable=FALSE;

	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].ISOSupported=TRUE;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].BinningEnable=FALSE;

	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].ISOSupported=TRUE;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].BinningEnable=FALSE;

	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxWidth=CAM_SIZE_1M_WIDTH;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxHeight=CAM_SIZE_1M_HEIGHT;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].ISOSupported=TRUE;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].BinningEnable=TRUE;

	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxWidth=CAM_SIZE_1M_WIDTH;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxHeight=CAM_SIZE_1M_HEIGHT;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].ISOSupported=TRUE;
	   pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].BinningEnable=TRUE; */
	pSensorInfo->CaptureDelayFrame = 1;
	pSensorInfo->PreviewDelayFrame = 3;
	pSensorInfo->VideoDelayFrame = 4;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;

/* pSensorInfo->YUVAwbDelayFrame = 3; */
/* pSensorInfo->YUVEffectDelayFrame = 2; */

	/* Sophie Add for 72 */
	pSensorInfo->YUVAwbDelayFrame = 5;
	pSensorInfo->YUVEffectDelayFrame = 3;

	/* Samuel Add for 72 HDR Ev capture */
	pSensorInfo->AEShutDelayFrame = 4;



	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pSensorInfo->SensorClockFreq = 26;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = HI253_GRAB_START_X + 3;
		pSensorInfo->SensorGrabStartY = HI253_GRAB_START_Y;
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		/* case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4: */
		/* case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: */
		/* case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM: */
	default:
		pSensorInfo->SensorClockFreq = 26;
		pSensorInfo->SensorClockDividCount = 3;
		pSensorInfo->SensorClockRisingCount = 0;
		pSensorInfo->SensorClockFallingCount = 2;
		pSensorInfo->SensorPixelClockCount = 3;
		pSensorInfo->SensorDataLatchCount = 2;
		pSensorInfo->SensorGrabStartX = HI253_GRAB_START_X + 3;
		pSensorInfo->SensorGrabStartY = HI253_GRAB_START_Y + 1;
		break;
	}
	return ERROR_NONE;
}				/* HI253GetInfo() */

UINT32 HI253Capture_ZSD(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 LineLength, EXP100, EXP120, EXPMIN, EXPUNIT;	/* , CapShutter; */
	/* kal_uint8 ClockDivider; */
	/* kal_uint32 temp; */
	SENSORDB("\n\n\n\n\n\n");
	SENSORDB("[HI253]CONTROLFLOW HI253Capture_ZSD!!!!!!!!!!!!!\n");
	SENSORDB("[HI253]Image Target Width: %d; Height: %d\n", image_window->ImageTargetWidth,
		 image_window->ImageTargetHeight);
	spin_lock(&hi253_drv_lock);

	HI253_MPEG4_encode_mode = KAL_FALSE;
	spin_unlock(&hi253_drv_lock);


#if 1
	HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */
/* HI253SetAeMode(KAL_FALSE); */
/* HI253SetAwbMode(KAL_FALSE); */

	/*I253SetPage(0x20);
	   temp=(HI253ReadCmosSensor(0x80) << 16)|(HI253ReadCmosSensor(0x81) << 8)|HI253ReadCmosSensor(0x82);
	   spin_lock(&hi253_drv_lock);
	   HI253Status.PvShutter = temp;
	   spin_unlock(&hi253_drv_lock); */

	/* 1600*1200 */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x10, 0x00);
	HI253WriteCmosSensor(0x3f, 0x00);
	HI253SetPage(0x12);
	HI253WriteCmosSensor(0x20, 0x0f);
	HI253WriteCmosSensor(0x21, 0x0f);
	HI253WriteCmosSensor(0x90, 0x5d);
	HI253SetPage(0x13);
	HI253WriteCmosSensor(0x80, 0xfd);
	/*capture 1600*1200 start x, y */
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x20, 0x00);	/* WINROWH */
	HI253WriteCmosSensor(0x21, 0x0f + 2);	/* WINROWL */
	HI253WriteCmosSensor(0x22, 0x00);	/* WINCOLH */
	HI253WriteCmosSensor(0x23, 0x19);	/* WINCOLL */
	spin_lock(&hi253_drv_lock);
	HI253Status.CapDummyPixels = 360;
	HI253Status.CapDummyLines = 52;	/* 10 FPS, 104 for 9.6 FPS */
	spin_unlock(&hi253_drv_lock);
	LineLength = HI253_FULL_PERIOD_PIXEL_NUMS + HI253Status.CapDummyPixels;
	spin_lock(&hi253_drv_lock);
	HI253Status.CapOpClk = 26;
	spin_unlock(&hi253_drv_lock);
	EXP100 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 100 / 8;
	EXP120 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 120 / 8;
	EXPMIN = EXPUNIT = LineLength / 4;

	HI253SetPage(0x20);
	HI253WriteCmosSensor(0x86, (EXPMIN >> 8) & 0xff);	/* EXPMIN */
	HI253WriteCmosSensor(0x87, (EXPMIN >> 0) & 0xff);
	HI253WriteCmosSensor(0x8b, (EXP100 >> 8) & 0xff);	/* EXP100 */
	HI253WriteCmosSensor(0x8c, (EXP100 >> 0) & 0xff);
	HI253WriteCmosSensor(0x8d, (EXP120 >> 8) & 0xff);	/* EXP120 */
	HI253WriteCmosSensor(0x8e, (EXP120 >> 0) & 0xff);
	HI253WriteCmosSensor(0x9c, (HI253_FULL_EXPOSURE_LIMITATION >> 8) & 0xff);
	HI253WriteCmosSensor(0x9d, (HI253_FULL_EXPOSURE_LIMITATION >> 0) & 0xff);
	HI253WriteCmosSensor(0x9e, (EXPUNIT >> 8) & 0xff);	/* EXP Unit */
	HI253WriteCmosSensor(0x9f, (EXPUNIT >> 0) & 0xff);

	HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 8.33 fps */
	HI253WriteCmosSensor(0x89, 0xf3);
	HI253WriteCmosSensor(0x8a, 0x70);


	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x40, (HI253Status.CapDummyPixels >> 8) & 0xff);
	HI253WriteCmosSensor(0x41, (HI253Status.CapDummyPixels >> 0) & 0xff);
	HI253WriteCmosSensor(0x42, (HI253Status.CapDummyLines >> 8) & 0xff);
	HI253WriteCmosSensor(0x43, (HI253Status.CapDummyLines >> 0) & 0xff);

/*  if(HI253Status.ZoomFactor > 8)
  {
    ClockDivider = 1; //Op CLock 13M
  }
  else
  {
    ClockDivider = 0; //OpCLock 26M
  }
  SENSORDB("[HI253]ClockDivider: %d\n",ClockDivider);
  HI253WriteCmosSensor(0x12, 0x04|ClockDivider);
  CapShutter = HI253Status.PvShutter >> ClockDivider;
  if(CapShutter<1)      CapShutter = 1;

  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x83, (CapShutter >> 16) & 0xFF);
  HI253WriteCmosSensor(0x84, (CapShutter >> 8) & 0xFF);
  HI253WriteCmosSensor(0x85, CapShutter & 0xFF);  */

	HI253WriteCmosSensor(0x12, 0x04 | 0);
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */
	/* SENSORDB("[HI253]CapShutter: %d\n",CapShutter); */
#else

	HI253WriteCmosSensor(0x03, 0x00);
	HI253WriteCmosSensor(0x01, 0x01);
	HI253WriteCmosSensor(0x03, 0x22);
	HI253WriteCmosSensor(0x10, 0x69);
	HI253WriteCmosSensor(0x03, 0x00);
	HI253WriteCmosSensor(0x10, 0x00);
	HI253WriteCmosSensor(0x20, 0x00);
	HI253WriteCmosSensor(0x21, 0x11);
	HI253WriteCmosSensor(0x22, 0x00);
	HI253WriteCmosSensor(0x23, 0x19);
	HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
	HI253WriteCmosSensor(0x41, 0x68);
	HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
	HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */
	HI253SetPage(0x12);
	HI253WriteCmosSensor(0x20, 0x00);
	HI253WriteCmosSensor(0x21, 0x00);
	HI253WriteCmosSensor(0x90, 0x00);
	HI253SetPage(0x13);
	HI253WriteCmosSensor(0x80, 0x00);

	HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
	HI253WriteCmosSensor(0x86, 0x01);	/* EXPMin 6500.00 fps */
	HI253WriteCmosSensor(0x87, 0xf4);

	HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 8.33 fps */
	HI253WriteCmosSensor(0x89, 0xf3);
	HI253WriteCmosSensor(0x8a, 0x70);

	HI253WriteCmosSensor(0x8B, 0x7e);	/* EXP100 */
	HI253WriteCmosSensor(0x8C, 0xf4);
	HI253WriteCmosSensor(0x8D, 0x69);	/* EXP120 */
	HI253WriteCmosSensor(0x8E, 0x78);
	HI253WriteCmosSensor(0x9c, 0x0d);	/* EXP Limit 928.57 fps */
	HI253WriteCmosSensor(0x9d, 0xac);
	HI253WriteCmosSensor(0x9e, 0x01);	/* EXP Unit */
	HI253WriteCmosSensor(0x9f, 0xf4);

	HI253WriteCmosSensor(0x03, 0x00);
	HI253WriteCmosSensor(0x01, 0x00);

	HI253WriteCmosSensor(0x03, 0x22);
	HI253WriteCmosSensor(0x10, 0xe9);
	HI253WriteCmosSensor(0x03, 0x20);
	HI253WriteCmosSensor(0x10, 0x9c);

#endif

	HI253SetAeMode(KAL_TRUE);
	HI253SetAwbMode(KAL_TRUE);
	return ERROR_NONE;
}				/* HI253Capture() */

UINT32 HI253Control(MSDK_SCENARIO_ID_ENUM ScenarioId,
		    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	SENSORDB("[HI253]CONTROLFLOW HI253Control\n");

	HI253_CurrentScenarioId = ScenarioId;

	switch (ScenarioId) {
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		SENSORDB("[HI253]CONTROLFLOW MSDK_SCENARIO_ID_VIDEO_PREVIEW\n");
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		SENSORDB("[HI253]CONTROLFLOW MSDK_SCENARIO_ID_CAMERA_PREVIEW\n");
		/* case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4: */
		HI253Preview(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		SENSORDB("[HI253]CONTROLFLOW MSDK_SCENARIO_ID_CAMERA_ZSD\n");
		HI253Capture_ZSD(pImageWindow, pSensorConfigData);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		SENSORDB("[HI253]CONTROLFLOW MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG\n");
		/* case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM: */
		HI253Capture(pImageWindow, pSensorConfigData);
		break;
	default:
		break;
	}
	return ERROR_NONE;
}				/* HI253Control() */


/* kal_uint8  HI253ReadGain() */
kal_uint16 HI253ReadGain(void)
{
	kal_uint16 temp;
	HI253WriteCmosSensor(0x03, 0x20);
	temp = HI253ReadCmosSensor(0xb0);
	/* temp=64+(temp/32)*128; */
	/* temp=64+(temp/32)*128; */
	temp = 64 + temp * 4;
	pr_info("HI253ReadGain:%d\n\r", temp);
	return temp;
}

kal_uint32 HI253ReadShutter(void)
{
	kal_uint32 temp, LineLength;
	HI253WriteCmosSensor(0x03, 0x20);
	temp =
	    (HI253ReadCmosSensor(0x80) << 16) | (HI253ReadCmosSensor(0x81) << 8) |
	    (HI253ReadCmosSensor(0x82));
	HI253WriteCmosSensor(0x03, 0x00);
	/* LineLength = ((HI253ReadCmosSensor(0x40) << 8)|HI253ReadCmosSensor(0x41)) + 824; */
	/* temp = ((temp * 8) / LineLength); */
	LineLength = ((HI253ReadCmosSensor(0x40) << 8) | HI253ReadCmosSensor(0x41)) + 632;	/* pre1 */
	temp = temp * 8 / LineLength;
	pr_info("HI253ReadShutter:%d\n\r", temp);
	return temp;
}

/* kal_uint8 HI253ReadAwbRGain() */
kal_uint16 HI253ReadAwbRGain(void)
{
	kal_uint16 temp;
	HI253WriteCmosSensor(0x03, 0x22);
	temp = HI253ReadCmosSensor(0x80);	/* RGain */
	/* temp=64+(temp/64)*128; */
	/* temp=64+temp*4; */
	temp = 64 + temp * 2;
	pr_info("HI253ReadAwbRGain:%d\n\r", temp);
	return temp;
}

kal_uint16 HI253ReadAwbBGain(void)
{
	kal_uint16 temp;
	HI253WriteCmosSensor(0x03, 0x22);
	temp = HI253ReadCmosSensor(0x82);	/* BGain */
	/* temp=64+(temp/64)*128; */
	/* temp=64+temp*4; */
	temp = 64 + temp * 2;
	pr_info("HI253ReadAwbBGain:%d\n\r", temp);
	return temp;
}

static void HI253GetEvAwbRef(UINT32 pSensorAEAWBRefStruct /*PSENSOR_AE_AWB_REF_STRUCT Ref */)
{
	PSENSOR_AE_AWB_REF_STRUCT Ref = (PSENSOR_AE_AWB_REF_STRUCT) pSensorAEAWBRefStruct;
	SENSORDB("HI253GetEvAwbRef ref = 0x%x\n", (unsigned int)Ref);

	Ref->SensorAERef.AeRefLV05Shutter = 2620;	/* 338;//83000;//83870 1503;//5000;0x04f588      2195 992 */
	Ref->SensorAERef.AeRefLV05Gain = 640;	/* 704;//496*7;//2; // 7.75x, 128 base   0xa0 5.51X */
	Ref->SensorAERef.AeRefLV13Shutter = 655;	/* 1310;//62;//2801;// 49;//49 0x027ac4  404 */
	Ref->SensorAERef.AeRefLV13Gain = 180;	/* 64*2; // 1x, 128 base 0x1d */

	Ref->SensorAwbGainRef.AwbRefD65Rgain = 206;	/* 188; // 1.46875x, 128 base  0x47 */
	Ref->SensorAwbGainRef.AwbRefD65Bgain = 138;	/* 128; // 1x, 128 base 0x25 */
	Ref->SensorAwbGainRef.AwbRefCWFRgain = 170;	/* 160; // 1.25x, 128 base 0x35 */
	Ref->SensorAwbGainRef.AwbRefCWFBgain = 204;	/* 164; // 1.28125x, 128 base 0x46 */

}

static void HI253GetCurAeAwbInfo(UINT32 pSensorAEAWBCurStruct /*PSENSOR_AE_AWB_CUR_STRUCT Info */)
{
	PSENSOR_AE_AWB_CUR_STRUCT Info = (PSENSOR_AE_AWB_CUR_STRUCT) pSensorAEAWBCurStruct;
	SENSORDB("HI253GetCurAeAwbInfo Info = 0x%x\n", (unsigned int)Info);

	Info->SensorAECur.AeCurShutter = HI253ReadShutter();
	Info->SensorAECur.AeCurGain = HI253ReadGain();	/* *2; // 128 base */
	Info->SensorAwbGainCur.AwbCurRgain = HI253ReadAwbRGain();	/* <<1;// 128 base */
	Info->SensorAwbGainCur.AwbCurBgain = HI253ReadAwbBGain();	/* <<1; // 128 base */
	SENSORDB("HI253GetCurAeAwbInfo Info->SensorAECur.AeCurShutter  = 0x%x\n",
		 Info->SensorAECur.AeCurShutter);
	SENSORDB("HI253GetCurAeAwbInfo Info->SensorAECur.AeCurGain  = 0x%x\n",
		 Info->SensorAECur.AeCurGain);
	SENSORDB("HI253GetCurAeAwbInfo Info->SensorAwbGainCur.AwbCurRgain  = 0x%x\n",
		 Info->SensorAwbGainCur.AwbCurRgain);
	SENSORDB("HI253GetCurAeAwbInfo Info->SensorAwbGainCur.AwbCurBgain  = 0x%x\n",
		 Info->SensorAwbGainCur.AwbCurBgain);

}

void HI253set_scene_mode(UINT16 para)
{
	/* unsigned int activeConfigNum = 0; */

	/* HI253GetActiveConfigNum(&activeConfigNum); */

	switch (para) {
	case SCENE_MODE_PORTRAIT:
		SENSORDB("[HI253]SCENE_MODE_PORTRAIT\n");
		/* Portrait */
		HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */

		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x10, 0x11);
		HI253WriteCmosSensor(0x40, 0x01);
		HI253WriteCmosSensor(0x41, 0x68);
		HI253WriteCmosSensor(0x42, 0x00);
		HI253WriteCmosSensor(0x43, 0x14);

		/* HI253WriteCmosSensor(0x03, 0x02); */
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x11, 0x90);	/* Fixed frame rate OFF */
		HI253WriteCmosSensor(0x90, 0x0c);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x0c);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x02);
		HI253WriteCmosSensor(0xd4, 0x0c);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x0c);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x8c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */

		HI253WriteCmosSensor(0x83, 0x01);	/* EXPTIMEH max fps */
		HI253WriteCmosSensor(0x84, 0x7c);	/* EXPTIMEM */
		HI253WriteCmosSensor(0x85, 0xdc);	/* EXPTIMEL */

		HI253WriteCmosSensor(0x88, 0x05);	/* EXPMAXH 8fps */
		HI253WriteCmosSensor(0x89, 0xf3);	/* EXPMAXM */
		HI253WriteCmosSensor(0x8a, 0x70);	/* EXPMAXL */

		HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */

		break;
	case SCENE_MODE_LANDSCAPE:
		SENSORDB("[HI253]SCENE_MODE_LANDSCAPE\n");
		/* Landscape ?? */

		HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */

		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x10, 0x11);
		HI253WriteCmosSensor(0x40, 0x01);
		HI253WriteCmosSensor(0x41, 0x68);
		HI253WriteCmosSensor(0x42, 0x00);
		HI253WriteCmosSensor(0x43, 0x14);

		/* HI253WriteCmosSensor(0x03, 0x02); */
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x11, 0x90);	/* Fixed frame rate OFF */
		HI253WriteCmosSensor(0x90, 0x0c);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x0c);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x02);
		HI253WriteCmosSensor(0xd4, 0x0c);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x0c);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x8c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */

		HI253WriteCmosSensor(0x83, 0x01);	/* EXPTIMEH max fps */
		HI253WriteCmosSensor(0x84, 0x7c);	/* EXPTIMEM */
		HI253WriteCmosSensor(0x85, 0xdc);	/* EXPTIMEL */

		HI253WriteCmosSensor(0x88, 0x03);	/* EXPMAXH 14fps */
		HI253WriteCmosSensor(0x89, 0x78);	/* EXPMAXM */
		HI253WriteCmosSensor(0x8a, 0xac);	/* EXPMAXL */

		HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;
		/* case SCENE_MODE_NIGHT: */
	case SCENE_MODE_NIGHTSCENE:
		SENSORDB("[HI253]SCENE_MODE_NIGHTSCENE\n");
		HI253NightMode(KAL_TRUE);	/* //MODE0=5-30FPS */
		break;
	case SCENE_MODE_FIREWORKS:	/* to do */
		SENSORDB("[HI253]SCENE_MODE_FIREWORKS\n");
		break;

	case SCENE_MODE_BEACH:
	case SCENE_MODE_SNOW:
		SENSORDB("[HI253]SCENE_MODE_SNOW\n");
		/* Beach ?? */
		HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */

		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x10, 0x11);
		HI253WriteCmosSensor(0x40, 0x01);
		HI253WriteCmosSensor(0x41, 0x68);
		HI253WriteCmosSensor(0x42, 0x00);
		HI253WriteCmosSensor(0x43, 0x14);

		/* HI253WriteCmosSensor(0x03, 0x02); */
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x11, 0x90);	/* Fixed frame rate OFF */
		HI253WriteCmosSensor(0x90, 0x0c);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x0c);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x02);
		HI253WriteCmosSensor(0xd4, 0x0c);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x0c);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x8c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */

		HI253WriteCmosSensor(0x83, 0x01);	/* EXPTIMEH max fps */
		HI253WriteCmosSensor(0x84, 0x7c);	/* EXPTIMEM */
		HI253WriteCmosSensor(0x85, 0xdc);	/* EXPTIMEL */

		HI253WriteCmosSensor(0x88, 0x03);	/* EXPMAXH 8fps */
		HI253WriteCmosSensor(0x89, 0xf7);	/* EXPMAXM */
		HI253WriteCmosSensor(0x8a, 0xa0);	/* EXPMAXL */

		HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;

	case SCENE_MODE_PARTY:	/* TO do */
		SENSORDB("[HI253]SCENE_MODE_SNOW\n");

		break;
	case SCENE_MODE_SUNSET:
		SENSORDB("[HI253]SCENE_MODE_SUNSET\n");
		/* Sunset ?? */
		HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */

		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x10, 0x11);
		HI253WriteCmosSensor(0x40, 0x01);
		HI253WriteCmosSensor(0x41, 0x68);
		HI253WriteCmosSensor(0x42, 0x00);
		HI253WriteCmosSensor(0x43, 0x14);

		/* HI253WriteCmosSensor(0x03, 0x02); */
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x11, 0x90);	/* Fixed frame rate OFF */
		HI253WriteCmosSensor(0x90, 0x0c);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x0c);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x02);
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0xd4, 0x0c);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x0c);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x8c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */

		HI253WriteCmosSensor(0x83, 0x01);	/* EXPTIMEH max fps */
		HI253WriteCmosSensor(0x84, 0x7c);	/* EXPTIMEM */
		HI253WriteCmosSensor(0x85, 0xdc);	/* EXPTIMEL */

		HI253WriteCmosSensor(0x88, 0x04);	/* EXPMAXH 10fps */
		HI253WriteCmosSensor(0x89, 0xf5);	/* EXPMAXM */
		HI253WriteCmosSensor(0x8a, 0x88);	/* EXPMAXL */

		HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;
	case SCENE_MODE_SPORTS:
		SENSORDB("[HI253]SCENE_MODE_SPORTS\n");
		/* Sports ?? */

		HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */

		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x10, 0x11);
		HI253WriteCmosSensor(0x40, 0x01);
		HI253WriteCmosSensor(0x41, 0x68);
		HI253WriteCmosSensor(0x42, 0x00);
		HI253WriteCmosSensor(0x43, 0x14);


		/* HI253WriteCmosSensor(0x03, 0x02); */
		HI253WriteCmosSensor(0x03, 0x00);
		HI253WriteCmosSensor(0x11, 0x90);	/* Fixed frame rate OFF */
		HI253WriteCmosSensor(0x90, 0x0c);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x0c);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x02);
		HI253WriteCmosSensor(0xd4, 0x0c);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x0c);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x8c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		HI253WriteCmosSensor(0x11, 0x1c);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2a, 0xf0);	/* 0x35 for fixed frame rate */
		HI253WriteCmosSensor(0x2b, 0x34);	/* 0x35 for fixed frame rate, 0x34 for dynamic frame rate */

		HI253WriteCmosSensor(0x83, 0x01);	/* EXPTIMEH max fps */
		HI253WriteCmosSensor(0x84, 0x7c);	/* EXPTIMEM */
		HI253WriteCmosSensor(0x85, 0xdc);	/* EXPTIMEL */

		HI253WriteCmosSensor(0x88, 0x02);	/* EXPMAXH 20fps */
		HI253WriteCmosSensor(0x89, 0x7a);	/* EXPMAXM */
		HI253WriteCmosSensor(0x8a, 0xc4);	/* EXPMAXL */

		HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */

		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x9c);	/* AE ON BIT 7 */
		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		/* */

		break;
	case SCENE_MODE_OFF:
		SENSORDB("[HI253]SCENE_MODE_OFF\n");
	default:
		SENSORDB("[HI253]SCENE_MODE default: %d\n", para);
		HI253NightMode(KAL_FALSE);	/* //MODE0=10-30FPS */

		break;
	}
}

BOOL HI253SetWb(UINT16 Para)
{
	SENSORDB("[HI253]CONTROLFLOW HI253SetWb Para:%d;\n", Para);
	spin_lock(&hi253_drv_lock);
	HI253Status.wb = Para;
	spin_unlock(&hi253_drv_lock);
	switch (Para) {
	case AWB_MODE_OFF:
		HI253SetAwbMode(KAL_FALSE);
		/* HI253SetPage(0x22); */
		HI253WriteCmosSensor(0x80, 0x38);	/* RGAIN */
		HI253WriteCmosSensor(0x81, 0x20);	/* GGAIN */
		HI253WriteCmosSensor(0x82, 0x38);	/* BGAIN */

		HI253WriteCmosSensor(0x83, 0x5e);	/* RMAX */
		HI253WriteCmosSensor(0x84, 0x20);	/* RMIN */
		HI253WriteCmosSensor(0x85, 0x4e);	/* BMAX */
		HI253WriteCmosSensor(0x86, 0x15);	/* BMIN */
		break;
	case AWB_MODE_AUTO:
		HI253SetPage(0x22);
		/* HI253WriteCmosSensor(0x80, 0x38); // RGAIN */
		/* HI253WriteCmosSensor(0x81, 0x20); // GGAIN */
		/* HI253WriteCmosSensor(0x82, 0x38); // BGAIN */

		HI253WriteCmosSensor(0x83, 0x5e);	/* RMAX */
		HI253WriteCmosSensor(0x84, 0x20);	/* RMIN */
		HI253WriteCmosSensor(0x85, 0x4e);	/* BMAX */
		HI253WriteCmosSensor(0x86, 0x15);	/* BMIN */
		HI253SetAwbMode(KAL_TRUE);
		break;
	case AWB_MODE_CLOUDY_DAYLIGHT:	/* cloudy */
		HI253SetAwbMode(KAL_TRUE);
		HI253WriteCmosSensor(0x80, 0x49);
		HI253WriteCmosSensor(0x81, 0x20);
		HI253WriteCmosSensor(0x82, 0x24);
		HI253WriteCmosSensor(0x83, 0x50);
		HI253WriteCmosSensor(0x84, 0x45);
		HI253WriteCmosSensor(0x85, 0x24);
		HI253WriteCmosSensor(0x86, 0x1e);
		break;
	case AWB_MODE_DAYLIGHT:	/* sunny */
		HI253SetAwbMode(KAL_TRUE);
		HI253WriteCmosSensor(0x80, 0x4b);
		HI253WriteCmosSensor(0x81, 0x20);
		HI253WriteCmosSensor(0x82, 0x27);
		HI253WriteCmosSensor(0x83, 0x4b);
		HI253WriteCmosSensor(0x84, 0x3f);
		HI253WriteCmosSensor(0x85, 0x29);
		HI253WriteCmosSensor(0x86, 0x23);
		break;
	case AWB_MODE_INCANDESCENT:	/* office */
		HI253SetAwbMode(KAL_TRUE);
		HI253WriteCmosSensor(0x80, 0x24);
		HI253WriteCmosSensor(0x81, 0x20);
		HI253WriteCmosSensor(0x82, 0x48);
		HI253WriteCmosSensor(0x83, 0x2e);
		HI253WriteCmosSensor(0x84, 0x24);
		HI253WriteCmosSensor(0x85, 0x48);
		HI253WriteCmosSensor(0x86, 0x42);
		break;
	case AWB_MODE_TUNGSTEN:	/* home */
		HI253SetAwbMode(KAL_TRUE);
		HI253WriteCmosSensor(0x80, 0x21);
		HI253WriteCmosSensor(0x81, 0x20);
		HI253WriteCmosSensor(0x82, 0x4c);
		HI253WriteCmosSensor(0x83, 0x22);
		HI253WriteCmosSensor(0x84, 0x1e);
		HI253WriteCmosSensor(0x85, 0x4c);
		HI253WriteCmosSensor(0x86, 0x45);
		break;
	case AWB_MODE_FLUORESCENT:
		HI253SetAwbMode(KAL_TRUE);
		HI253WriteCmosSensor(0x80, 0x38);
		HI253WriteCmosSensor(0x81, 0x20);
		HI253WriteCmosSensor(0x82, 0x47);
		HI253WriteCmosSensor(0x83, 0x38);
		HI253WriteCmosSensor(0x84, 0x32);
		HI253WriteCmosSensor(0x85, 0x48);
		HI253WriteCmosSensor(0x86, 0x46);
	default:
		return KAL_FALSE;
	}
	return KAL_TRUE;
}				/* HI253SetWb */

BOOL HI253SetEffect(UINT16 Para)
{
	SENSORDB("[HI253]CONTROLFLOW HI253SetEffect Para:%d;\n", Para);
	HI253Status.Effect = Para;
	switch (Para) {
	case MEFFECT_OFF:
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x30);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x30;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x40, HI253Status.Brightness);
		break;
	case MEFFECT_SEPIA:
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x23);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x23;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x44, 0x70);
		HI253WriteCmosSensor(0x45, 0x98);
		HI253WriteCmosSensor(0x47, 0x7f);
		HI253WriteCmosSensor(0x03, 0x13);
		HI253WriteCmosSensor(0x20, 0x07);
		HI253WriteCmosSensor(0x21, 0x07);
		break;
	case MEFFECT_NEGATIVE:	/* ----datasheet */
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x28);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x28;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x44, 0x80);
		HI253WriteCmosSensor(0x45, 0x80);
		HI253WriteCmosSensor(0x47, 0x7f);
		HI253WriteCmosSensor(0x03, 0x13);
		HI253WriteCmosSensor(0x20, 0x07);
		HI253WriteCmosSensor(0x21, 0x07);
		break;
	case MEFFECT_SEPIAGREEN:	/* ----datasheet aqua */
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x23);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x23;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x44, 0x80);
		HI253WriteCmosSensor(0x45, 0x04);
		HI253WriteCmosSensor(0x47, 0x7f);
		HI253WriteCmosSensor(0x03, 0x13);
		HI253WriteCmosSensor(0x20, 0x07);
		HI253WriteCmosSensor(0x21, 0x07);
		break;
	case MEFFECT_SEPIABLUE:
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x23);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x23;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x44, 0xb0);
		HI253WriteCmosSensor(0x45, 0x40);
		HI253WriteCmosSensor(0x47, 0x7f);
		HI253WriteCmosSensor(0x03, 0x13);
		HI253WriteCmosSensor(0x20, 0x07);
		HI253WriteCmosSensor(0x21, 0x07);
		break;
	case MEFFECT_MONO:	/* ----datasheet black & white */
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x11, 0x03);
		HI253WriteCmosSensor(0x12, 0x23);
		spin_lock(&hi253_drv_lock);
		HI253Status.ISPCTL3 = 0x23;
		spin_unlock(&hi253_drv_lock);
		HI253WriteCmosSensor(0x13, 0x00);
		/* HI253WriteCmosSensor(0x40, 0x00); */
		HI253WriteCmosSensor(0x44, 0x80);
		HI253WriteCmosSensor(0x45, 0x80);
		HI253WriteCmosSensor(0x47, 0x7f);
		HI253WriteCmosSensor(0x03, 0x13);
		HI253WriteCmosSensor(0x20, 0x07);
		HI253WriteCmosSensor(0x21, 0x07);
		break;
	default:
		return KAL_FALSE;
	}
	if (HI253Status.Effect != MEFFECT_OFF) {
		HI253SetPage(0x10);
		HI253WriteCmosSensor(0x12, HI253ReadCmosSensor(0x12) | 0x10);
		HI253WriteCmosSensor(0x40, HI253Status.Brightness);
	}
	return KAL_TRUE;

}				/* HI253SetEffect */

BOOL HI253SetBanding(UINT16 Para)
{

	SENSORDB("[HI253]CONTROLFLOW HI253SetBanding Para:%d;\n", Para);
	switch (Para) {
	case AE_FLICKER_MODE_50HZ:
		{
			SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_50HZ\n");
		}
		break;

	case AE_FLICKER_MODE_60HZ:
		{
			SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_60HZ\n");
		}
		break;

	case AE_FLICKER_MODE_OFF:
		{
			SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_OFF\n");
		}
		break;

	case AE_FLICKER_MODE_AUTO:
	default:
		{
			SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_AUTO\n");
			/* Auto Flicker */
			break;
		}
	}


	spin_lock(&hi253_drv_lock);
	HI253Status.Banding = Para;
	spin_unlock(&hi253_drv_lock);
	SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_OFF\n");

/*		if(Para == AE_FLICKER_MODE_OFF)
		{
		//	HI253SetAeMode(KAL_FALSE);
			SENSORDB("[Enter]HI253SetBanding AE_FLICKER_MODE_OFF\n");
			//off
			HI253WriteCmosSensor(0x03, 0x20); //Page 20
			HI253WriteCmosSensor(0x10, 0x1c);//AE off
			spin_lock(&hi253_drv_lock);
			HI253Status.AECTL1 = 0x1c;
			spin_unlock(&hi253_drv_lock);
			return;
		}*/

	/* HI253SetAeMode(KAL_FALSE); */

/* //////////////////for zsd banding */
	if (HI253_CurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD) {
		return KAL_TRUE;
		{
			if (HI253Status.NightMode == KAL_FALSE) {
				SENSORDB("[Enter]HI253SetBanding preview Normal\n");
				switch (Para) {
				case AE_FLICKER_MODE_50HZ:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x1c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x1c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
					/* HI253WriteCmosSensor(0x84, 0x7c); */
					/* HI253WriteCmosSensor(0x85, 0x40); */
					HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 33.33 fps */
					HI253WriteCmosSensor(0x89, 0xf3);
					HI253WriteCmosSensor(0x8a, 0x70);

					break;
				case AE_FLICKER_MODE_60HZ:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x0c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x0c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
					/* HI253WriteCmosSensor(0x84, 0xa5); */
					/* HI253WriteCmosSensor(0x85, 0xb0); */

					HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 40.00 fps */
					HI253WriteCmosSensor(0x89, 0xf3);
					HI253WriteCmosSensor(0x8a, 0x70);

					break;
				case AE_FLICKER_MODE_AUTO:
				case AE_FLICKER_MODE_OFF:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x5c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x5c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
					/* HI253WriteCmosSensor(0x84, 0x7c); */
					/* HI253WriteCmosSensor(0x85, 0xbe); */
					HI253WriteCmosSensor(0x88, 0x05);	/* EXP Max 33.33 fps */
					HI253WriteCmosSensor(0x89, 0xf3);
					HI253WriteCmosSensor(0x8a, 0x70);


					HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
					HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
					HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
					break;
				default:
					break;
				}
			} else {
				SENSORDB("[Enter]HI253SetBanding preview night\n");
				switch (Para) {
				case AE_FLICKER_MODE_50HZ:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x1c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x1c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
					/* HI253WriteCmosSensor(0x84, 0x7c); */
					/* HI253WriteCmosSensor(0x85, 0x40); */
					HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 33.33 fps */
					HI253WriteCmosSensor(0x89, 0xeb);
					HI253WriteCmosSensor(0x8a, 0x10);

					break;
				case AE_FLICKER_MODE_60HZ:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x0c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x0c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
					/* HI253WriteCmosSensor(0x84, 0xa5); */
					/* HI253WriteCmosSensor(0x85, 0xb0); */

					HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 40.00 fps */
					HI253WriteCmosSensor(0x89, 0xeb);
					HI253WriteCmosSensor(0x8a, 0x10);

					break;
				case AE_FLICKER_MODE_AUTO:
				case AE_FLICKER_MODE_OFF:
					HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
					HI253WriteCmosSensor(0x40, 0x01);	/* Hblank_360 */
					HI253WriteCmosSensor(0x41, 0x68);
					HI253WriteCmosSensor(0x42, 0x00);	/* Vblank */
					HI253WriteCmosSensor(0x43, 0x34);	/* Flick Stop */

					HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
					HI253WriteCmosSensor(0x10, 0x5c);
					spin_lock(&hi253_drv_lock);
					HI253Status.AECTL1 = 0x5c;
					spin_unlock(&hi253_drv_lock);
					/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
					/* HI253WriteCmosSensor(0x84, 0x7c); */
					/* HI253WriteCmosSensor(0x85, 0xbe); */
					HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 33.33 fps */
					HI253WriteCmosSensor(0x89, 0xeb);
					HI253WriteCmosSensor(0x8a, 0x10);


					HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
					HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
					HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
					break;
				default:
					break;
				}
			}
		}

		HI253SetAeMode(KAL_TRUE);

		return KAL_TRUE;
	}

/* //////////////////// */

	if (HI253_MPEG4_encode_mode == KAL_FALSE) {	/* preview */
		if (HI253Status.NightMode == KAL_FALSE) {
			SENSORDB("[Enter]HI253SetBanding preview Normal\n");
			switch (Para) {
			case AE_FLICKER_MODE_50HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0xa8);	/* hblank424 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x1c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x1c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0x40); */
				HI253WriteCmosSensor(0x88, 0x04);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0xf3);
				HI253WriteCmosSensor(0x8a, 0x80);

				break;
			case AE_FLICKER_MODE_60HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0xa8);	/* hblank424 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x0c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x0c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
				/* HI253WriteCmosSensor(0x84, 0xa5); */
				/* HI253WriteCmosSensor(0x85, 0xb0); */

				HI253WriteCmosSensor(0x88, 0x04);	/* EXP Max 40.00 fps */
				HI253WriteCmosSensor(0x89, 0xf1);
				HI253WriteCmosSensor(0x8a, 0x10);

				break;
			case AE_FLICKER_MODE_AUTO:
			case AE_FLICKER_MODE_OFF:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x5c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x5c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */
				HI253WriteCmosSensor(0x88, 0x04);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0xf5);
				HI253WriteCmosSensor(0x8a, 0x24);


				HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
				HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
				HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
				break;
			default:
				break;
			}
		} else {
			SENSORDB("[Enter]HI253SetBanding preview night\n");
			switch (Para) {
			case AE_FLICKER_MODE_50HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0xa8);	/* hblank424 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x1c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x1c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0x40); */
				HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0xe7);
				HI253WriteCmosSensor(0x8a, 0x00);

				break;
			case AE_FLICKER_MODE_60HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0xa8);	/* hblank424 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x0c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x0c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
				/* HI253WriteCmosSensor(0x84, 0xa5); */
				/* HI253WriteCmosSensor(0x85, 0xb0); */

				HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 40.00 fps */
				HI253WriteCmosSensor(0x89, 0xe2);
				HI253WriteCmosSensor(0x8a, 0x20);

				break;
			case AE_FLICKER_MODE_AUTO:
			case AE_FLICKER_MODE_OFF:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x5c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x5c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */
				HI253WriteCmosSensor(0x88, 0x09);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0xea);
				HI253WriteCmosSensor(0x8a, 0x48);


				HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
				HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
				HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
				break;
			default:
				break;
			}
		}
	} else {		/* video */
		if (HI253Status.NightMode == KAL_FALSE) {
			SENSORDB("[Enter]HI253SetBanding video normal\n");
			switch (Para) {
			case AE_FLICKER_MODE_50HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x1c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x1c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */
				HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0x7c);
				HI253WriteCmosSensor(0x8a, 0xbe);
				HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
				HI253WriteCmosSensor(0x92, 0xa7);
				HI253WriteCmosSensor(0x93, 0x0c);
				break;
			case AE_FLICKER_MODE_60HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x0c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x0c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
				/* HI253WriteCmosSensor(0x84, 0xa7); */
				/* HI253WriteCmosSensor(0x85, 0x0c); */

				HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 40.00 fps */
				HI253WriteCmosSensor(0x89, 0x3d);
				HI253WriteCmosSensor(0x8a, 0x49);
				HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
				HI253WriteCmosSensor(0x92, 0xa7);
				HI253WriteCmosSensor(0x93, 0x0c);
				break;
			case AE_FLICKER_MODE_AUTO:
			case AE_FLICKER_MODE_OFF:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x5c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x5c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */
				HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 33.33 fps */
				HI253WriteCmosSensor(0x89, 0x7c);
				HI253WriteCmosSensor(0x8a, 0xbe);
				HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
				HI253WriteCmosSensor(0x92, 0xa7);
				HI253WriteCmosSensor(0x93, 0x0c);

				HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
				HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
				HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
				break;
			default:
				break;
			}
		} else {
			SENSORDB("[Enter]HI253SetBanding video night\n");
			switch (Para) {
			case AE_FLICKER_MODE_50HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x1c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x1c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */

				HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 16.67 fps */
				HI253WriteCmosSensor(0x89, 0xf9);
				HI253WriteCmosSensor(0x8a, 0x7c);

				HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
				HI253WriteCmosSensor(0x92, 0x4e);
				HI253WriteCmosSensor(0x93, 0x18);
				break;
			case AE_FLICKER_MODE_60HZ:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x0c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x0c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
				/* HI253WriteCmosSensor(0x84, 0xa7); */
				/* HI253WriteCmosSensor(0x85, 0x0c); */

				HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 17.14 fps */
				HI253WriteCmosSensor(0x89, 0xe4);
				HI253WriteCmosSensor(0x8a, 0x55);
				HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
				HI253WriteCmosSensor(0x92, 0x4e);
				HI253WriteCmosSensor(0x93, 0x18);
				break;
			case AE_FLICKER_MODE_AUTO:
			case AE_FLICKER_MODE_OFF:
				HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
				HI253WriteCmosSensor(0x40, 0x01);
				HI253WriteCmosSensor(0x41, 0x3c);	/* hblank316 */
				HI253WriteCmosSensor(0x42, 0x00);
				HI253WriteCmosSensor(0x43, 0x3e);
				HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
				HI253WriteCmosSensor(0x10, 0x5c);
				spin_lock(&hi253_drv_lock);
				HI253Status.AECTL1 = 0x5c;
				spin_unlock(&hi253_drv_lock);
				/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
				/* HI253WriteCmosSensor(0x84, 0x7c); */
				/* HI253WriteCmosSensor(0x85, 0xbe); */
				HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 16.67 fps */
				HI253WriteCmosSensor(0x89, 0xf9);
				HI253WriteCmosSensor(0x8a, 0x7c);
				HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
				HI253WriteCmosSensor(0x92, 0x4e);
				HI253WriteCmosSensor(0x93, 0x18);
				HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
				HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
				HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
				break;
			default:
				break;
			}
		}
	}

	HI253SetAeMode(KAL_TRUE);

	return TRUE;
}

BOOL HI253SetExposure(UINT16 Para)
{
	SENSORDB("[HI253]CONTROLFLOW HI253SetExposure Para:%d;\n", Para);
	HI253SetPage(0x10);
	spin_lock(&hi253_drv_lock);
	HI253Status.ISPCTL3 |= 0x10;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x12, HI253Status.ISPCTL3);	/* make sure the Yoffset control is opened. */

	switch (Para) {
	case AE_EV_COMP_n20:	/* -2 EV */
		SENSORDB("[HI253]HI253SetExposure AE_EV_COMP_n20\n");
		HI253WriteCmosSensor(0x40, 0xe0);
		spin_lock(&hi253_drv_lock);
		HI253Status.Brightness = 0xe0;
		spin_unlock(&hi253_drv_lock);
		break;
	case AE_EV_COMP_n10:	/* -1 EV */
		SENSORDB("[HI253]HI253SetExposure AE_EV_COMP_n10\n");
		HI253WriteCmosSensor(0x40, 0xb0);
		spin_lock(&hi253_drv_lock);
		HI253Status.Brightness = 0xb0;
		spin_unlock(&hi253_drv_lock);
		break;
	case AE_EV_COMP_00:	/* EV 0 */
		SENSORDB("[HI253]HI253SetExposure AE_EV_COMP_00\n");
		HI253WriteCmosSensor(0x40, 0x85);
		spin_lock(&hi253_drv_lock);
		HI253Status.Brightness = 0x85;
		spin_unlock(&hi253_drv_lock);
		break;
	case AE_EV_COMP_10:	/* +1 EV */
		SENSORDB("[HI253]HI253SetExposure AE_EV_COMP_10\n");
		HI253WriteCmosSensor(0x40, 0x30);
		spin_lock(&hi253_drv_lock);
		HI253Status.Brightness = 0x30;
		spin_unlock(&hi253_drv_lock);
		break;
	case AE_EV_COMP_20:	/* +2 EV */
		SENSORDB("[HI253]HI253SetExposure AE_EV_COMP_20\n");

		HI253WriteCmosSensor(0x40, 0x60);
		spin_lock(&hi253_drv_lock);
		HI253Status.Brightness = 0x60;
		spin_unlock(&hi253_drv_lock);
		break;
	default:
		return KAL_FALSE;
	}
	SENSORDB("[HI253]HI253SetExposure HI253Status.Brightness:%d;\n", HI253Status.Brightness);
	return KAL_TRUE;
}				/* HI253SetExposure */

void HI253get_AEAWB_lock(UINT32 *pAElockRet32, UINT32 *pAWBlockRet32)
{
	*pAElockRet32 = 1;
	*pAWBlockRet32 = 1;
	SENSORDB("[HI253]GetAEAWBLock,AE=%d ,AWB=%d\n,", *pAElockRet32, *pAWBlockRet32);
}



void HI253set_brightness(UINT16 para)
{
	SENSORDB("[HI253]CONTROLFLOW HI253set_brightness Para:%d;\n", para);
	HI253SetPage(0x10);
	spin_lock(&hi253_drv_lock);
	HI253Status.ISPCTL3 |= 0x10;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x12, HI253Status.ISPCTL3);	/* make sure the Yoffset control is opened. */


	switch (para) {
	case ISP_BRIGHT_LOW:
		/* Low */
		/* 253WriteCmosSensor(0x03, 0x10); */
		/* HI253WriteCmosSensor(0x40, 0xa8); */
		HI253SetPage(0x20);
		HI253WriteCmosSensor(0x70, 0x30);
		spin_lock(&hi253_drv_lock);
		/* HI253Status.Brightness = 0xa0; */
		spin_unlock(&hi253_drv_lock);
		break;
	case ISP_BRIGHT_HIGH:
		/* Hig */
		/* 253WriteCmosSensor(0x03, 0x10); */
		HI253SetPage(0x20);
		HI253WriteCmosSensor(0x70, 0x50);
		spin_lock(&hi253_drv_lock);
		/* HI253Status.Brightness = 0x20; */
		spin_unlock(&hi253_drv_lock);
		break;
	case ISP_BRIGHT_MIDDLE:
	default:
		/* Med */
		/* 253WriteCmosSensor(0x03, 0x10); */
		HI253SetPage(0x20);
		HI253WriteCmosSensor(0x70, 0x40);
		spin_lock(&hi253_drv_lock);
		/* HI253Status.Brightness = 0x80; */
		spin_unlock(&hi253_drv_lock);
		break;
	}


	return;
}



void HI253set_contrast(UINT16 para)
{
	SENSORDB("[HI253]CONTROLFLOW HI253set_contrast Para:%d;\n", para);
	HI253SetPage(0x10);
	spin_lock(&hi253_drv_lock);
	HI253Status.ISPCTL4 |= 0x02;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x13, HI253Status.ISPCTL4);	/* make sure the Yoffset control is opened. */

	switch (para) {
	case ISP_CONTRAST_LOW:
		/* Low */
		HI253WriteCmosSensor(0x03, 0x10);
		/* HI253WriteCmosSensor(0x48, 0x70); */
		HI253WriteCmosSensor(0x48, 0x60);
		break;
	case ISP_CONTRAST_HIGH:
		/* Hig */
		HI253WriteCmosSensor(0x03, 0x10);
		/* HI253WriteCmosSensor(0x48, 0x90); */
		HI253WriteCmosSensor(0x48, 0xa0);
		break;
	case ISP_CONTRAST_MIDDLE:
	default:
		/* Med */
		HI253WriteCmosSensor(0x03, 0x10);
		HI253WriteCmosSensor(0x48, 0x80);
		break;
	}

	return;
}



void HI253set_iso(UINT16 para)
{
#if 0

#else


	SENSORDB("[HI253]CONTROLFLOW HI253set_iso Para:%d;\n", para);

	switch (para) {
	case AE_ISO_100:
		/* ISO100 */
		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x1c);
		/* HI253WriteCmosSensor(0xb0, 0x1d);//AG */
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */

		HI253WriteCmosSensor(0xb1, 0x1a);	/* AGMIN */
		HI253WriteCmosSensor(0xb2, 0x80);	/* AGMAX */
		HI253WriteCmosSensor(0xb4, 0x14);	/* AGTH1 */
		HI253WriteCmosSensor(0xb5, 0x50);	/* AGTH2 */
		HI253WriteCmosSensor(0xb6, 0x40);	/* AGBTH1 */
		HI253WriteCmosSensor(0xb7, 0x38);	/* AGBTH2 */
		HI253WriteCmosSensor(0xb8, 0x28);	/* AGBTH3 */
		HI253WriteCmosSensor(0xb9, 0x22);	/* AGBTH4 */
		HI253WriteCmosSensor(0xba, 0x20);	/* AGBTH5 */
		HI253WriteCmosSensor(0xbb, 0x1e);	/* AGBTH6 */
		HI253WriteCmosSensor(0xbc, 0x1d);	/* AGBTH7 */
		HI253WriteCmosSensor(0xbd, 0x19);	/* AGBTH8 */
		HI253WriteCmosSensor(0x10, 0x9c);

		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;
	case AE_ISO_200:
		/* ISO200 */
		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x1c);
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		/* HI253WriteCmosSensor(0xb0, 0x1d);//AG */
		HI253WriteCmosSensor(0xb1, 0x1a);	/* AGMIN */
		HI253WriteCmosSensor(0xb2, 0xb0);	/* AGMAX */
		HI253WriteCmosSensor(0xb4, 0x1a);	/* AGTH1 */
		HI253WriteCmosSensor(0xb5, 0x44);	/* AGTH2 */
		HI253WriteCmosSensor(0xb6, 0x2f);	/* AGBTH1 */
		HI253WriteCmosSensor(0xb7, 0x28);	/* AGBTH2 */
		HI253WriteCmosSensor(0xb8, 0x25);	/* AGBTH3 */
		HI253WriteCmosSensor(0xb9, 0x22);	/* AGBTH4 */
		HI253WriteCmosSensor(0xba, 0x21);	/* AGBTH5 */
		HI253WriteCmosSensor(0xbb, 0x20);	/* AGBTH6 */
		HI253WriteCmosSensor(0xbc, 0x1f);	/* AGBTH7 */
		HI253WriteCmosSensor(0xbd, 0x1f);	/* AGBTH8 */
		HI253WriteCmosSensor(0x10, 0x9c);

		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;
	case AE_ISO_400:
		/* ISO400 */
		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x1c);
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		/* HI253WriteCmosSensor(0xb0, 0x1d);//AG */
		HI253WriteCmosSensor(0xb1, 0x1a);	/* AGMIN */
		HI253WriteCmosSensor(0xb2, 0xf0);	/* AGMAX */
		HI253WriteCmosSensor(0xb4, 0x14);	/* AGTH1 */
		HI253WriteCmosSensor(0xb5, 0x30);	/* AGTH2 */
		HI253WriteCmosSensor(0xb6, 0x24);	/* AGBTH1 */
		HI253WriteCmosSensor(0xb7, 0x20);	/* AGBTH2 */
		HI253WriteCmosSensor(0xb8, 0x1c);	/* AGBTH3 */
		HI253WriteCmosSensor(0xb9, 0x1a);	/* AGBTH4 */
		HI253WriteCmosSensor(0xba, 0x19);	/* AGBTH5 */
		HI253WriteCmosSensor(0xbb, 0x18);	/* AGBTH6 */
		HI253WriteCmosSensor(0xbc, 0x18);	/* AGBTH7 */
		HI253WriteCmosSensor(0xbd, 0x16);	/* AGBTH8 */
		HI253WriteCmosSensor(0x10, 0x9c);

		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		break;
	default:
	case AE_ISO_AUTO:	/*  */
		/* Auto */
		HI253WriteCmosSensor(0x03, 0x20);
		HI253WriteCmosSensor(0x10, 0x1c);
		HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
		/* HI253WriteCmosSensor(0xb0, 0x1d);//AG */
		HI253WriteCmosSensor(0xb1, 0x1a);	/* AGMIN */
		HI253WriteCmosSensor(0xb2, 0x80);	/* AGMAX */
		HI253WriteCmosSensor(0xb4, 0x1a);	/* AGTH1 */
		HI253WriteCmosSensor(0xb5, 0x44);	/* AGTH2 */
		HI253WriteCmosSensor(0xb6, 0x2f);	/* AGBTH1 */
		HI253WriteCmosSensor(0xb7, 0x28);	/* AGBTH2 */
		HI253WriteCmosSensor(0xb8, 0x25);	/* AGBTH3 */
		HI253WriteCmosSensor(0xb9, 0x22);	/* AGBTH4 */
		HI253WriteCmosSensor(0xba, 0x21);	/* AGBTH5 */
		HI253WriteCmosSensor(0xbb, 0x20);	/* AGBTH6 */
		HI253WriteCmosSensor(0xbc, 0x1f);	/* AGBTH7 */
		HI253WriteCmosSensor(0xbd, 0x1f);	/* AGBTH8 */
		HI253WriteCmosSensor(0x10, 0x9c);

		HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */
		/*      */
		break;
	}
	return;

#endif
}


void HI253set_saturation(UINT16 para)
{

	SENSORDB("[HI253]CONTROLFLOW HI253set_saturation Para:%d;\n", para);

	HI253SetPage(0x10);
	spin_lock(&hi253_drv_lock);
	HI253Status.SATCTL |= 0x02;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x60, HI253Status.SATCTL);	/* make sure the Yoffset control is opened. */


	switch (para) {
	case ISP_SAT_HIGH:
		/* Hig */
		HI253WriteCmosSensor(0x03, 0x10);
		HI253WriteCmosSensor(0x61, 0xb0);	/* Sat B */
		HI253WriteCmosSensor(0x62, 0xb0);	/* Sat R */
		break;
	case ISP_SAT_LOW:
		/* Low */
		HI253WriteCmosSensor(0x03, 0x10);
		/*HI253WriteCmosSensor(0x61, 0x90); //Sat B
		   HI253WriteCmosSensor(0x62, 0x90); //Sat R */
		HI253WriteCmosSensor(0x61, 0x50);	/* Sat B */
		HI253WriteCmosSensor(0x62, 0x50);	/* Sat R */
		break;
	case ISP_SAT_MIDDLE:
	default:
		/* Med */
		/*HI253WriteCmosSensor(0x03, 0x10);
		   HI253WriteCmosSensor(0x61, 0xa0); //Sat B
		   HI253WriteCmosSensor(0x62, 0xa0); //Sat R */
		HI253WriteCmosSensor(0x03, 0x10);
		HI253WriteCmosSensor(0x61, 0x80);	/* Sat B */
		HI253WriteCmosSensor(0x62, 0x80);	/* Sat R */
		break;
	}
	return;
}



UINT32 HI253YUVSensorSetting(FEATURE_ID Cmd, UINT32 Para)
{
	switch (Cmd) {
	case FID_SCENE_MODE:

		SENSORDB("[HI253]CONTROLFLOW FID_SCENE_MODE: %d;\n", Para);

		HI253set_scene_mode(Para);

		/*
		   if (Para == SCENE_MODE_OFF)
		   {
		   HI253NightMode(KAL_FALSE);
		   }
		   else if (Para == SCENE_MODE_NIGHTSCENE)
		   {
		   HI253NightMode(KAL_TRUE);
		   }  */
		break;
	case FID_AWB_MODE:
		HI253SetWb(Para);
		break;
	case FID_COLOR_EFFECT:
		HI253SetEffect(Para);
		break;
	case FID_AE_EV:
		HI253SetExposure(Para);
		break;
	case FID_AE_FLICKER:
		HI253SetBanding(Para);
		break;
	case FID_AE_SCENE_MODE:
		if (Para == AE_MODE_OFF) {
			HI253SetAeMode(KAL_FALSE);
		} else {
			HI253SetAeMode(KAL_TRUE);
		}
		break;
	case FID_ZOOM_FACTOR:
		SENSORDB("[HI253]ZoomFactor :%d;\n", Para);
		spin_lock(&hi253_drv_lock);
		HI253Status.ZoomFactor = Para;
		spin_unlock(&hi253_drv_lock);
		break;
	case FID_ISP_CONTRAST:
		SENSORDB("[HI253]FID_ISP_CONTRAST:%d\n", Para);
		HI253set_contrast(Para);
		break;
	case FID_ISP_BRIGHT:
		SENSORDB("[HI253]FID_ISP_BRIGHT:%d\n", Para);
		HI253set_brightness(Para);
		break;
	case FID_ISP_SAT:
		SENSORDB("[HI253]FID_ISP_SAT:%d\n", Para);
		HI253set_saturation(Para);
		break;
	case FID_AE_ISO:
		SENSORDB("[HI253]FID_AE_ISO:%d\n", Para);
		HI253set_iso(Para);
		break;
	default:
		break;
	}
	return TRUE;
}				/* HI253YUVSensorSetting */

UINT32 HI253YUVSetVideoMode(UINT16 FrameRate)
{
	if ((FrameRate != 30) && (FrameRate != 15)) {
		if (HI253Status.NightMode == KAL_TRUE)
			FrameRate = 15;
		else
			FrameRate = 30; }
	if (FrameRate * HI253_FRAME_RATE_UNIT > HI253_MAX_FPS)
		return -1;
	spin_lock(&hi253_drv_lock);
	HI253_MPEG4_encode_mode = KAL_TRUE;
	spin_unlock(&hi253_drv_lock);

	SENSORDB("[HI253]CONTROLFLOW HI253YUVSetVideoMode FrameRate:%d;\n", FrameRate);
	HI253WriteCmosSensor(0x03, 0x00);	/* Page 0 */
	HI253WriteCmosSensor(0x40, 0x01);	/* Hblank 316 */
	HI253WriteCmosSensor(0x41, 0x3c);
	HI253WriteCmosSensor(0x42, 0x00);	/* Vblank 20 */
	HI253WriteCmosSensor(0x43, 0x14);
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01, 0xf9);	/* Sleep ON */
	spin_lock(&hi253_drv_lock);
	HI253Status.VDOCTL2 |= 0x04;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x11, HI253Status.VDOCTL2);	/* Fixed frame rate ON */

	HI253SetPage(0x20);
	spin_lock(&hi253_drv_lock);
	HI253Status.AECTL1 &= (~0x80);
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x10, HI253Status.AECTL1);	/* AE ON BIT 7 */
	HI253WriteCmosSensor(0x18, 0x38);	/* AE Reset ON */
	if (FrameRate == 30) {
		/* AntiBand Unlock */
		HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
		HI253WriteCmosSensor(0x2b, 0x34);
		HI253WriteCmosSensor(0x30, 0x78);
		HI253WriteCmosSensor(0x86, 0x01);	/* EXPMin 11403.51 fps */
		HI253WriteCmosSensor(0x87, 0x1d);
		HI253WriteCmosSensor(0x8B, 0x7e);	/* EXP100 */
		HI253WriteCmosSensor(0x8C, 0xea);
		HI253WriteCmosSensor(0x8D, 0x69);	/* EXP120 */
		HI253WriteCmosSensor(0x8E, 0xc3);
		HI253WriteCmosSensor(0x9c, 0x07);	/* EXP Limit 1629.07 fps */
		HI253WriteCmosSensor(0x9d, 0xcb);
		HI253WriteCmosSensor(0x9e, 0x01);	/* EXP Unit */
		HI253WriteCmosSensor(0x9f, 0x1d);
		/* BLC */
		HI253WriteCmosSensor(0x03, 0x00);	/* PAGE 0 */
		HI253WriteCmosSensor(0x90, 0x03);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x03);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */
		/* DCDC */
		HI253WriteCmosSensor(0x03, 0x02);	/* PAGE 2 */
		HI253WriteCmosSensor(0xd4, 0x03);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x03);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */
		switch (HI253Status.Banding) {
		case AE_FLICKER_MODE_50HZ:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
			/* HI253WriteCmosSensor(0x84, 0x7c); */
			/* HI253WriteCmosSensor(0x85, 0xbe); */
			HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 33.33 fps */
			HI253WriteCmosSensor(0x89, 0x7c);
			HI253WriteCmosSensor(0x8a, 0xbe);
			HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
			HI253WriteCmosSensor(0x92, 0xa7);
			HI253WriteCmosSensor(0x93, 0x0c);
			break;
		case AE_FLICKER_MODE_60HZ:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
			/* HI253WriteCmosSensor(0x84, 0xa7); */
			/* HI253WriteCmosSensor(0x85, 0x0c); */

			HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 40.00 fps */
			HI253WriteCmosSensor(0x89, 0x3d);
			HI253WriteCmosSensor(0x8a, 0x49);
			HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
			HI253WriteCmosSensor(0x92, 0xa7);
			HI253WriteCmosSensor(0x93, 0x0c);
			break;
		case AE_FLICKER_MODE_AUTO:
		case AE_FLICKER_MODE_OFF:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
			/* HI253WriteCmosSensor(0x84, 0x7c); */
			/* HI253WriteCmosSensor(0x85, 0xbe); */
			HI253WriteCmosSensor(0x88, 0x01);	/* EXP Max 33.33 fps */
			HI253WriteCmosSensor(0x89, 0x7c);
			HI253WriteCmosSensor(0x8a, 0xbe);
			HI253WriteCmosSensor(0x91, 0x01);	/* EXP Fix 30.01 fps */
			HI253WriteCmosSensor(0x92, 0xa7);
			HI253WriteCmosSensor(0x93, 0x0c);

			HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
			HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
			HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
			break;
		default:
			break;
		}
	} else {
		/* AntiBand Unlock */
		HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
		HI253WriteCmosSensor(0x2b, 0x34);
		HI253WriteCmosSensor(0x30, 0x78);
		HI253WriteCmosSensor(0x86, 0x01);	/* EXPMin 11403.51 fps */
		HI253WriteCmosSensor(0x87, 0x1d);
		HI253WriteCmosSensor(0x8B, 0x7e);	/* EXP100 */
		HI253WriteCmosSensor(0x8C, 0xea);
		HI253WriteCmosSensor(0x8D, 0x69);	/* EXP120 */
		HI253WriteCmosSensor(0x8E, 0xc3);
		HI253WriteCmosSensor(0x9c, 0x07);	/* EXP Limit 1629.07 fps */
		HI253WriteCmosSensor(0x9d, 0xcb);
		HI253WriteCmosSensor(0x9e, 0x01);	/* EXP Unit */
		HI253WriteCmosSensor(0x9f, 0x1d);
		/* BLC */
		HI253WriteCmosSensor(0x03, 0x00);	/* PAGE 0 */
		HI253WriteCmosSensor(0x90, 0x06);	/* BLC_TIME_TH_ON */
		HI253WriteCmosSensor(0x91, 0x06);	/* BLC_TIME_TH_OFF */
		HI253WriteCmosSensor(0x92, 0x78);	/* BLC_AG_TH_ON */
		HI253WriteCmosSensor(0x93, 0x70);	/* BLC_AG_TH_OFF */
		/* DCDC */
		HI253WriteCmosSensor(0x03, 0x02);	/* PAGE 2 */
		HI253WriteCmosSensor(0xd4, 0x06);	/* DCDC_TIME_TH_ON */
		HI253WriteCmosSensor(0xd5, 0x06);	/* DCDC_TIME_TH_OFF */
		HI253WriteCmosSensor(0xd6, 0x78);	/* DCDC_AG_TH_ON */
		HI253WriteCmosSensor(0xd7, 0x70);	/* DCDC_AG_TH_OFF */
		switch (HI253Status.Banding) {
		case AE_FLICKER_MODE_50HZ:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
			/* HI253WriteCmosSensor(0x84, 0x7c); */
			/* HI253WriteCmosSensor(0x85, 0xbe); */

			HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 16.67 fps */
			HI253WriteCmosSensor(0x89, 0xf9);
			HI253WriteCmosSensor(0x8a, 0x7c);

			HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
			HI253WriteCmosSensor(0x92, 0x4e);
			HI253WriteCmosSensor(0x93, 0x18);

			break;
		case AE_FLICKER_MODE_60HZ:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 30.00 fps */
			/* HI253WriteCmosSensor(0x84, 0xa7); */
			/* HI253WriteCmosSensor(0x85, 0x0c); */

			HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 17.14 fps */
			HI253WriteCmosSensor(0x89, 0xe4);
			HI253WriteCmosSensor(0x8a, 0x55);
			HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
			HI253WriteCmosSensor(0x92, 0x4e);
			HI253WriteCmosSensor(0x93, 0x18);
			break;
		case AE_FLICKER_MODE_AUTO:
		case AE_FLICKER_MODE_OFF:
			HI253WriteCmosSensor(0x03, 0x20);	/* Page 20 */
			/* HI253WriteCmosSensor(0x83, 0x01); //EXP Normal 33.33 fps */
			/* HI253WriteCmosSensor(0x84, 0x7c); */
			/* HI253WriteCmosSensor(0x85, 0xbe); */
			HI253WriteCmosSensor(0x88, 0x02);	/* EXP Max 16.67 fps */
			HI253WriteCmosSensor(0x89, 0xf9);
			HI253WriteCmosSensor(0x8a, 0x7c);
			HI253WriteCmosSensor(0x91, 0x03);	/* EXP Fix 15.00 fps */
			HI253WriteCmosSensor(0x92, 0x4e);
			HI253WriteCmosSensor(0x93, 0x18);
			HI253WriteCmosSensor(0x03, 0x17);	/* Page 17 */
			HI253WriteCmosSensor(0xC4, 0x72);	/* FLK200 */
			HI253WriteCmosSensor(0xC5, 0x5f);	/* FLK240 */
			break;
		default:
			break;
		}
	}
	HI253WriteCmosSensor(0x01, 0xf8);	/* Sleep OFF */
	HI253SetPage(0x20);
	spin_lock(&hi253_drv_lock);
	HI253Status.AECTL1 |= 0x80;
	spin_unlock(&hi253_drv_lock);
	HI253WriteCmosSensor(0x10, HI253Status.AECTL1);	/* AE ON BIT 7 */
	HI253WriteCmosSensor(0x18, 0x30);	/* AE Reset OFF */

	return TRUE;
}



void HI253GetExifInfo(UINT32 exifAddr)
{
	SENSOR_EXIF_INFO_STRUCT *pExifInfo = (SENSOR_EXIF_INFO_STRUCT *) exifAddr;
	pExifInfo->FNumber = 28;
	pExifInfo->AEISOSpeed = AE_ISO_100;
	pExifInfo->AWBMode = HI253Status.wb;
	pExifInfo->CapExposureTime = 0;
	pExifInfo->FlashLightTimeus = 0;
	pExifInfo->RealISOValue = AE_ISO_100;
}




void HI253GetDelayInfo(UINT32 delayAddr)
{
	SENSOR_DELAY_INFO_STRUCT *pDelayInfo = (SENSOR_DELAY_INFO_STRUCT *) delayAddr;
	pDelayInfo->InitDelay = 0;
	pDelayInfo->EffectDelay = 2;
	pDelayInfo->AwbDelay = 3;
/* pDelayInfo->AFSwitchDelayFrame = 50; */
}

UINT32 HI253SetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	/* kal_uint32 pclk; */
	/* kal_int16 dummyLine; */
	/* kal_uint16 lineLength,frameHeight; */
#if 0
	SENSORDB("HI253SetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n", scenarioId,
		 frameRate);
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = HI253sensor_pclk;
		lineLength = HI253_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI253_PV_PERIOD_LINE_NUMS;
		HI253.sensorMode = SENSOR_MODE_PREVIEW;
		HI253_SetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = HI253sensor_pclk;
		lineLength = HI253_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI253_PV_PERIOD_LINE_NUMS;
		HI253.sensorMode = SENSOR_MODE_PREVIEW;
		HI253_SetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		pclk = HI253sensor_pclk;
		lineLength = HI253_FULL_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk) / frameRate / lineLength;
		dummyLine = frameHeight - HI253_FULL_PERIOD_LINE_NUMS;
		HI253.sensorMode = SENSOR_MODE_CAPTURE;
		HI253_SetDummy(0, dummyLine);
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		break;
	default:
		break;
	}
#endif
	return ERROR_NONE;
}


UINT32 HI253GetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate)
{
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*pframeRate = 300;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		*pframeRate = 100;
		break;
	case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:	/* added */
	case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
	case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:	/* added */
		*pframeRate = 100;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

typedef enum {
	PRV_W = 640,
	PRV_H = 480,
	ZSD_PRV_W = 1600,
	ZSD_PRV_H = 1200
} PREVIEW_VIEW_SIZE;

static void HI253_FOCUS_Get_AE_Max_Num_Metering_Areas(UINT32 *pFeatureReturnPara32)
{
	SENSORDB("[HI253MIPI]enter HI253_FOCUS_Get_AE_Max_Num_Metering_Areas function:\n ");
	*pFeatureReturnPara32 = 1;
	SENSORDB(" *pFeatureReturnPara32 = %d\n", *pFeatureReturnPara32);
	SENSORDB("[HI253MIPI]exit HI253_FOCUS_Get_AE_Max_Num_Metering_Areas function:\n ");
}

void HI253_mapMiddlewaresizePointToPreviewsizePoint(UINT32 mx,
						    UINT32 my,
						    UINT32 mw,
						    UINT32 mh,
						    UINT32 *pvx,
						    UINT32 *pvy, UINT32 pvw, UINT32 pvh)
{

	*pvx = pvw * mx / mw;
	*pvy = pvh * my / mh;
	SENSORDB("mapping middlware x[%d],y[%d], [%d X %d]\n\t\tto x[%d],y[%d],[%d X %d]\n ",
		 mx, my, mw, mh, *pvx, *pvy, pvw, pvh);
}


void HI253_FOCUS_Set_AE_Window(UINT32 zone_addr)
{				/* update global zone */

	UINT32 FD_XS;
	UINT32 FD_YS;
	UINT32 x0, y0, x1, y1;
	UINT32 pvx0, pvy0, pvx1, pvy1;
	/* UINT32 linenum, rownum; */
	UINT32 central_p_x, central_p_y;
	UINT32 *zone = (UINT32 *) zone_addr;
	UINT8 modify_reg1, modify_reg2;
	UINT8 modify_bit;	/* 1:high 4 bit, 0:low 4 bit */

	SENSORDB("[HI253]enter HI253_FOCUS_Set_AE_Window function:\n ");

	x0 = *zone;
	y0 = *(zone + 1);
	x1 = *(zone + 2);
	y1 = *(zone + 3);
	FD_XS = *(zone + 4);
	FD_YS = *(zone + 5);

	SENSORDB("Set_AE_Window x0=%d,y0=%d,x1=%d,y1=%d,FD_XS=%d,FD_YS=%d\n",
		 x0, y0, x1, y1, FD_XS, FD_YS);

	/*  if(CurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD )
	   {
	   HI253_mapMiddlewaresizePointToPreviewsizePoint(x0,y0,FD_XS,FD_YS,&pvx0, &pvy0, ZSD_PRV_W, ZSD_PRV_H);
	   HI253_mapMiddlewaresizePointToPreviewsizePoint(x1,y1,FD_XS,FD_YS,&pvx1, &pvy1, ZSD_PRV_W, ZSD_PRV_H);
	   }
	   else */

	/* AE window 4X4 */
	{
		HI253_mapMiddlewaresizePointToPreviewsizePoint(x0, y0, FD_XS, FD_YS, &pvx0, &pvy0,
							       4, 4);
		HI253_mapMiddlewaresizePointToPreviewsizePoint(x1, y1, FD_XS, FD_YS, &pvx1, &pvy1,
							       4, 4);
	}
	central_p_x = (pvx0 + pvx1) / 2;
	central_p_y = (pvy0 + pvy1) / 2;

	/*  */
	if ((central_p_x == last_central_p_x) && (central_p_y == last_central_p_y))
		return;
	else {
		spin_lock(&hi253_drv_lock);
		last_central_p_x = central_p_x;
		last_central_p_y = central_p_y;
		spin_unlock(&hi253_drv_lock);
	}
	if ((x0 == x1) && (y0 == y1)) {	/* (160,120)  (160,120) position */
		/* restore AE weight */
		HI253SetPage(0x20);
		HI253WriteCmosSensor(0x60, 0x55);	/* AEWGT1 */
		HI253WriteCmosSensor(0x61, 0x55);	/* AEWGT2 */
		HI253WriteCmosSensor(0x62, 0x6a);	/* AEWGT3 */
		HI253WriteCmosSensor(0x63, 0xa9);	/* AEWGT4 */
		HI253WriteCmosSensor(0x64, 0x6a);	/* AEWGT5 */
		HI253WriteCmosSensor(0x65, 0xa9);	/* AEWGT6 */
		HI253WriteCmosSensor(0x66, 0x6a);	/* AEWGT7 */
		HI253WriteCmosSensor(0x67, 0xa9);	/* AEWGT8 */
		HI253WriteCmosSensor(0x68, 0x6b);	/* AEWGT9 */
		HI253WriteCmosSensor(0x69, 0xe9);	/* AEWGT10 */
		HI253WriteCmosSensor(0x6a, 0x6a);	/* AEWGT11 */
		HI253WriteCmosSensor(0x6b, 0xa9);	/* AEWGT12 */
		HI253WriteCmosSensor(0x6c, 0x6a);	/* AEWGT13 */
		HI253WriteCmosSensor(0x6d, 0xa9);	/* AEWGT14 */
		HI253WriteCmosSensor(0x6e, 0x55);	/* AEWGT15 */
		HI253WriteCmosSensor(0x6f, 0x55);	/* AEWGT16 */

	} else {
		HI253SetPage(0x20);
		HI253WriteCmosSensor(0x60, 0x00);	/* AEWGT1 */
		HI253WriteCmosSensor(0x61, 0x00);	/* AEWGT2 */
		HI253WriteCmosSensor(0x62, 0x00);	/* AEWGT3 */
		HI253WriteCmosSensor(0x63, 0x00);	/* AEWGT4 */
		HI253WriteCmosSensor(0x64, 0x00);	/* AEWGT5 */
		HI253WriteCmosSensor(0x65, 0x00);	/* AEWGT6 */
		HI253WriteCmosSensor(0x66, 0x00);	/* AEWGT7 */
		HI253WriteCmosSensor(0x67, 0x00);	/* AEWGT8 */
		HI253WriteCmosSensor(0x68, 0x00);	/* AEWGT9 */
		HI253WriteCmosSensor(0x69, 0x00);	/* AEWGT10 */
		HI253WriteCmosSensor(0x6a, 0x00);	/* AEWGT11 */
		HI253WriteCmosSensor(0x6b, 0x00);	/* AEWGT12 */
		HI253WriteCmosSensor(0x6c, 0x00);	/* AEWGT13 */
		HI253WriteCmosSensor(0x6d, 0x00);	/* AEWGT14 */
		HI253WriteCmosSensor(0x6e, 0x00);	/* AEWGT15 */
		HI253WriteCmosSensor(0x6f, 0x00);	/* AEWGT16 */

		modify_reg1 = 0x60 + last_central_p_y * 4 + last_central_p_x / 2;
		modify_reg2 = modify_reg1 + 2;
		modify_bit = (last_central_p_x + 1) & 0x01;	/* 1: high 4 bit ,0: low 4 bit */

		SENSORDB("last_central_p_x=%d,last_central_p_y=%d\n", last_central_p_x,
			 last_central_p_y);

		SENSORDB("modify_reg1=%x,modify_reg2=%x\n", modify_reg1, modify_reg2);
		SENSORDB("modify_bit=%d\n", modify_bit);


		if (modify_bit == 1) {
			/* HI253WriteCmosSensor(modify_reg1, (HI253ReadCmosSensor(modify_reg1))|(0xf0)); */
			/* HI253WriteCmosSensor(modify_reg2, (HI253ReadCmosSensor(modify_reg2))|(0xf0)); */
			HI253WriteCmosSensor(modify_reg1, 0xf0);
			HI253WriteCmosSensor(modify_reg2, 0xf0);
		} else {
			HI253WriteCmosSensor(modify_reg1, 0x0f);
			HI253WriteCmosSensor(modify_reg2, 0x0f);
		}
	}

}

UINT32 HI253FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
			   UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16 = (UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16 = (UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32 = (UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32 = (UINT32 *) pFeaturePara;
	/* MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara; */
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData = (MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	switch (FeatureId) {
	case SENSOR_FEATURE_GET_RESOLUTION:
		*pFeatureReturnPara16++ = HI253_FULL_WIDTH;
		*pFeatureReturnPara16 = HI253_FULL_HEIGHT;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++ = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
		*pFeatureReturnPara16 = HI253_PV_PERIOD_LINE_NUMS + HI253Status.PvDummyLines;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*pFeatureReturnPara32 = HI253Status.PvOpClk * 2;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		HI253NightMode((BOOL) *pFeatureData16);
		break;
	case SENSOR_FEATURE_SET_GAIN:
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		HI253WriteCmosSensor(0x03, pSensorRegData->RegAddr >> 8);
		HI253WriteCmosSensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		HI253WriteCmosSensor(0x03, pSensorRegData->RegAddr >> 8);
		pSensorRegData->RegData = HI253ReadCmosSensor(pSensorRegData->RegAddr & 0xff);
		break;
	case SENSOR_FEATURE_GET_CONFIG_PARA:
		*pFeatureParaLen = sizeof(MSDK_SENSOR_CONFIG_STRUCT);
		break;
	case SENSOR_FEATURE_SET_CCT_REGISTER:
	case SENSOR_FEATURE_GET_CCT_REGISTER:
	case SENSOR_FEATURE_SET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_ENG_REGISTER:
	case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
	case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
	case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
	case SENSOR_FEATURE_GET_GROUP_INFO:
	case SENSOR_FEATURE_GET_ITEM_INFO:
	case SENSOR_FEATURE_SET_ITEM_INFO:
	case SENSOR_FEATURE_GET_ENG_INFO:
		break;
	case SENSOR_FEATURE_GET_GROUP_COUNT:
		*pFeatureReturnPara32++ = 0;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*pFeatureReturnPara32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_YUV_CMD:
		HI253YUVSensorSetting((FEATURE_ID) *pFeatureData32, *(pFeatureData32 + 1));
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		HI253YUVSetVideoMode(*pFeatureData16);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		HI253GetSensorID(pFeatureReturnPara32);
		break;
/* /add feature */

	case SENSOR_FEATURE_GET_EV_AWB_REF:
		/* SENSORDB("[HI253] F_GET_EV_AWB_REF\n"); */
		HI253GetEvAwbRef(*pFeatureData32);
		break;

	case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
		/* SENSORDB("[HI253] F_GET_SHUTTER_GAIN_AWB_GAIN\n"); */
		HI253GetCurAeAwbInfo(*pFeatureData32);
		break;

#ifdef MIPI_INTERFACE
	case SENSOR_FEATURE_GET_EXIF_INFO:
		/* SENSORDB("[HI253] F_GET_EXIF_INFO\n"); */
		HI253GetExifInfo(*pFeatureData32);
		break;
#endif

	case SENSOR_FEATURE_GET_DELAY_INFO:
		/* SENSORDB("[HI253] F_GET_DELAY_INFO\n"); */
		HI253GetDelayInfo(*pFeatureData32);
		break;

	case SENSOR_FEATURE_SET_SLAVE_I2C_ID:
		/* SENSORDB("[HI253] F_SET_SLAVE_I2C_ID:[%d]\n",*pFeatureData32); */
		HI253sensor_socket = *pFeatureData32;
		break;

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		/* 1 TODO */
		/* SENSORDB("[HI253] F_SET_TEST_PATTERN: FAIL: NOT Support\n"); */
		HI253SetTestPatternMode((BOOL) *pFeatureData16);
		break;

	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		/* 1 TODO */
		/* SENSORDB("[HI253] F_SET_MAX_FRAME_RATE_BY_SCENARIO: FAIL: NOT Support\n"); */
		HI253SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM) *pFeatureData32,
					       *(pFeatureData32 + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		/* SENSORDB("[HI253] F_GET_DEFAULT_FRAME_RATE_BY_SCENARIO\n"); */
		HI253GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM) *pFeatureData32,
						   (MUINT32 *) (*(pFeatureData32 + 1)));
		break;
	case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
		/* SENSORDB("[HI253] F_GET_AE_AWB_LOCK_INFO\n"); */
		HI253get_AEAWB_lock((UINT32 *) (*pFeatureData32),
				    (UINT32 *) (*(pFeatureData32 + 1)));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*pFeatureReturnPara32 = HI253_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
		HI253_FOCUS_Get_AE_Max_Num_Metering_Areas(pFeatureReturnPara32);
		*pFeatureParaLen = 4;
		break;
	case SENSOR_FEATURE_SET_AE_WINDOW:
		SENSORDB("AE zone addr = 0x%x\n", *pFeatureData32);
		HI253_FOCUS_Set_AE_Window(*pFeatureData32);
		break;




/* // */
	default:
		break;
	}
	return ERROR_NONE;
}				/* HI253FeatureControl() */



UINT32 HI253_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	static SENSOR_FUNCTION_STRUCT SensorFuncHI253 = {
		HI253Open,
		HI253GetInfo,
		HI253GetResolution,
		HI253FeatureControl,
		HI253Control,
		HI253Close
	};

	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncHI253;

	return ERROR_NONE;
}				/* SensorInit() */
