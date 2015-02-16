/*********************************************************************
	Include Header File
*********************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_ois_i2c.h"
#include "lgit_imx214_ois.h"
/*
#ifdef MSM_OIS_DEBUG
#undef CDBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif
*/
/*********************************************************************
	Global Variable
*********************************************************************/
uint8_t spigyrocheck = 0x00;

#define	OIS_FW_POLLING_PASS		0
#define	OIS_FW_POLLING_FAIL		-1
#define	OIS_FW_POLLING_VERSION_FAIL	-2
#define	CLRGYR_POLLING_LIMIT_A	6
#define	ACCWIT_POLLING_LIMIT_A	6
#define	INIDGY_POLLING_LIMIT_A	12
#define INIDGY_POLLING_LIMIT_B	12

#define FILREGTAB	7
#define	XY_SIMU_SET
#ifdef	XY_SIMU_SET
#define FILRAMTAB	200
#else	/* XY_SIMU_SET */
#define FILRAMTAB	363
#endif	/* XY_SIMU_SET */

/* Filter Calculator Version 4.02 */
/* the time and date : 2014/2/20 14:27:13 */
/* FC filename : LC898122_FIL_V0013 */
/* fs,23438Hz */
/* LSI No.,LC898122 */
/* Comment */

/* 8bit */
const struct STFILREG CsFilReg[][FILREGTAB]	= {
	{
		{0x0111, 0x00},	/* 00,0111 */
		{0x0113, 0x00},	/* 00,0113 */
		{0x0114, 0x00},	/* 00,0114 */
		{0x0172, 0x00},	/* 00,0172 */
		{0x01E3, 0x00},	/* 00,01E3 */
		{0x01E4, 0x00},	/* 00,01E4 */
		{0xFFFF, 0xFF}
	}
};

/* 32bit */
const struct STFILRAM CsFilRam[][FILRAMTAB] = {
	{
		{0x1000, 0x3F800000},	/* 3F800000,1000,0dB,invert=0 */
		{0x1001, 0x3F800000},	/* 3F800000,1001,0dB,invert=0 */
		{0x1002, 0x00000000},	/* 00000000,1002,Cutoff,invert=0 */
		{0x1003, 0x3EBFED40},		/*3EBFED40,1003,-8.5227dB,invert=0*/
		{0x1004, 0x3860DE00},	/* 3860DE00,1004,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1005, 0x3860DE00},	/* 3860DE00,1005,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1006, 0x3F7FF900},	/* 3F7FF900,1006,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1007, 0x3F800000},	/* 3F800000,1007,0dB,invert=0 */
		{0x1008, 0xBF800000},	/* BF800000,1008,0dB,invert=1 */
		{0x1009, 0x3F800000},	/* 3F800000,1009,0dB,invert=0 */
		{0x100A, 0x3F800000},	/* 3F800000,100A,0dB,invert=0 */
		{0x100B, 0x3F800000},	/* 3F800000,100B,0dB,invert=0 */
		{0x100C, 0x3F800000},	/* 3F800000,100C,0dB,invert=0 */
		{0x100E, 0x3F800000},	/* 3F800000,100E,0dB,invert=0 */
		{0x1010, 0x3DA2ADC0},	/* 3DA2ADC0,1010 */
		{0x1011, 0x00000000},	/* 00000000,1011,Free,fs/1,invert=0 */
		{0x1012, 0x3F7FFD80},	/* 3F7FFD80,1012,Free,fs/1,invert=0 */
		{0x1013, 0x3F76D280},	/*3F76D280,1013,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1014, 0xBF723C00},	/*BF723C00,1014,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1015, 0x3F690E80},	/*3F690E80,1015,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1016, 0x40FE2B00},	/*40FE2B00,1016,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1017, 0xC0FE2B00},	/*C0FE2B00,1017,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1018, 0x3F7FF740},	/*3F7FF740,1018,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1019, 0x3ED1D880},	/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x101A, 0xBED1CFC0},	/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x101B, 0x3F7FFB80},	/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x101C, 0x00000000},	/* 00000000,101C,Cutoff,invert=0*/
		{0x101D, 0x3F800000},	/* 3F800000,101D,0dB,invert=0 */
		{0x101E, 0x3F800000},	/* 3F800000,101E,0dB,invert=0 */
		{0x1020, 0x3F800000},	/* 3F800000,1020,0dB,invert=0 */
		{0x1021, 0x3F800000},	/* 3F800000,1021,0dB,invert=0 */
		{0x1022, 0x3F800000},	/* 3F800000,1022,0dB,invert=0 */
		{0x1023, 0x3F800000},	/* 3F800000,1023,Through,0dB,fs/1,invert=0 */
		{0x1024, 0x00000000},	/* 00000000,1024,Through,0dB,fs/1,invert=0 */
		{0x1025, 0x00000000},	/* 00000000,1025,Through,0dB,fs/1,invert=0 */
		{0x1026, 0x00000000},	/* 00000000,1026,Through,0dB,fs/1,invert=0 */
		{0x1027, 0x00000000},	/* 00000000,1027,Through,0dB,fs/1,invert=0 */
		{0x1030, 0x3F800000},	/* 3F800000,1030,Through,0dB,fs/1,invert=0 */
		{0x1031, 0x00000000},	/* 00000000,1031,Through,0dB,fs/1,invert=0 */
		{0x1032, 0x00000000},	/* 00000000,1032,Through,0dB,fs/1,invert=0 */
		{0x1033, 0x3F800000},	/* 3F800000,1033,Through,0dB,fs/1,invert=0 */
		{0x1034, 0x00000000},	/* 00000000,1034,Through,0dB,fs/1,invert=0 */
		{0x1035, 0x00000000},	/* 00000000,1035,Through,0dB,fs/1,invert=0 */
		{0x1036, 0x3F800000},	/* 3F800000,1036,Through,0dB,fs/1,invert=0 */
		{0x1037, 0x00000000},	/* 00000000,1037,Through,0dB,fs/1,invert=0 */
		{0x1038, 0x00000000},	/* 00000000,1038,Through,0dB,fs/1,invert=0 */
		{0x1039, 0x3F800000},	/* 3F800000,1039,Through,0dB,fs/1,invert=0 */
		{0x103A, 0x00000000},	/* 00000000,103A,Through,0dB,fs/1,invert=0 */
		{0x103B, 0x00000000},	/* 00000000,103B,Through,0dB,fs/1,invert=0 */
		{0x103C, 0x3F800000},	/* 3F800000,103C,Through,0dB,fs/1,invert=0 */
		{0x103D, 0x00000000},	/* 00000000,103D,Through,0dB,fs/1,invert=0 */
		{0x103E, 0x00000000},	/* 00000000,103E,Through,0dB,fs/1,invert=0 */
		{0x1043, 0x39D2BD40},	/* 39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1044, 0x39D2BD40},	/* 39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1045, 0x3F7FCB40},	/* 3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1046, 0x388C8A40},	/* 388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1047, 0x388C8A40},	/* 388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1048, 0x3F7FF740},	/* 3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1049, 0x390C87C0},	/* 390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x104A, 0x390C87C0},	/* 390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x104B, 0x3F7FEE80},	/* 3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x104C, 0x398C8300},	/* 398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x104D, 0x398C8300},	/* 398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x104E, 0x3F7FDCC0},	/* 3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x1053, 0x3F800000},	/* 3F800000,1053,Through,0dB,fs/1,invert=0 */
		{0x1054, 0x00000000},	/* 00000000,1054,Through,0dB,fs/1,invert=0 */
		{0x1055, 0x00000000},	/* 00000000,1055,Through,0dB,fs/1,invert=0 */
		{0x1056, 0x3F800000},	/* 3F800000,1056,Through,0dB,fs/1,invert=0 */
		{0x1057, 0x00000000},	/* 00000000,1057,Through,0dB,fs/1,invert=0 */
		{0x1058, 0x00000000},	/* 00000000,1058,Through,0dB,fs/1,invert=0 */
		{0x1059, 0x3F800000},	/* 3F800000,1059,Through,0dB,fs/1,invert=0 */
		{0x105A, 0x00000000},	/* 00000000,105A,Through,0dB,fs/1,invert=0 */
		{0x105B, 0x00000000},	/* 00000000,105B,Through,0dB,fs/1,invert=0 */
		{0x105C, 0x3F800000},	/* 3F800000,105C,Through,0dB,fs/1,invert=0 */
		{0x105D, 0x00000000},	/* 00000000,105D,Through,0dB,fs/1,invert=0 */
		{0x105E, 0x00000000},	/* 00000000,105E,Through,0dB,fs/1,invert=0 */
		{0x1063, 0x3F800000},	/* 3F800000,1063,0dB,invert=0 */
		{0x1066, 0x3F800000},	/* 3F800000,1066,0dB,invert=0 */
		{0x1069, 0x3F800000},	/* 3F800000,1069,0dB,invert=0 */
		{0x106C, 0x3F800000},	/* 3F800000,106C,0dB,invert=0 */
		{0x1073, 0x00000000},	/* 00000000,1073,Cutoff,invert=0 */
		{0x1076, 0x3F800000},	/* 3F800000,1076,0dB,invert=0 */
		{0x1079, 0x3F800000},	/* 3F800000,1079,0dB,invert=0 */
		{0x107C, 0x3F800000},	/* 3F800000,107C,0dB,invert=0 */
		{0x1083, 0x38D1B700},	/* 38D1B700,1083,-80dB,invert=0 */
		{0x1086, 0x00000000},	/* 00000000,1086,Cutoff,invert=0 */
		{0x1089, 0x00000000},	/* 00000000,1089,Cutoff,invert=0 */
		{0x108C, 0x00000000},	/* 00000000,108C,Cutoff,invert=0 */
		{0x1093, 0x00000000},	/* 00000000,1093,Cutoff,invert=0 */
		{0x1098, 0x3F800000},	/* 3F800000,1098,0dB,invert=0 */
		{0x1099, 0x3F800000},	/* 3F800000,1099,0dB,invert=0 */
		{0x109A, 0x3F800000},	/* 3F800000,109A,0dB,invert=0 */
		{0x10A1, 0x3C58B440},	/* 3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A2, 0x3C58B440},	/* 3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A3, 0x3F793A40},	/* 3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A4, 0x3C58B440},	/* 3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A5, 0x3C58B440},	/* 3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A6, 0x3F793A40},	/* 3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x10A7, 0x3F800000},	/* 3F800000,10A7,Through,0dB,fs/1,invert=0 */
		{0x10A8, 0x00000000},	/* 00000000,10A8,Through,0dB,fs/1,invert=0 */
		{0x10A9, 0x00000000},	/* 00000000,10A9,Through,0dB,fs/1,invert=0 */
		{0x10AA, 0x00000000},	/* 00000000,10AA,Cutoff,invert=0 */
		{0x10AB, 0x3BDA2580},	/* 3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x10AC, 0x3BDA2580},	/* 3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x10AD, 0x3F7C9780},	/* 3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x10B0, 0x3F800000},	/* 3F800000,10B0,Through,0dB,fs/1,invert=0 */
		{0x10B1, 0x00000000},	/* 00000000,10B1,Through,0dB,fs/1,invert=0 */
		{0x10B2, 0x00000000},	/* 00000000,10B2,Through,0dB,fs/1,invert=0 */
		{0x10B3, 0x3F800000},	/* 3F800000,10B3,0dB,invert=0 */
		{0x10B4, 0x00000000},	/* 00000000,10B4,Cutoff,invert=0 */
		{0x10B5, 0x00000000},	/* 00000000,10B5,Cutoff,invert=0 */
		{0x10B6, 0x3F353C00},	/* 3F353C00,10B6,-3dB,invert=0 */
		{0x10B8, 0x3F800000},	/* 3F800000,10B8,0dB,invert=0 */
		{0x10B9, 0x00000000},	/* 00000000,10B9,Cutoff,invert=0*/
		{0x10C0, 0x3FCC34C0},	/* 3FCC34C0,10C0,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x10C1, 0xBFCA0700},	/* BFCA0700,10C1,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x10C2, 0x3F48E240},	/* 3F48E240,10C2,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x10C3, 0x3F800000},	/* 3F800000,10C3,Through,0dB,fs/1,invert=0 */
		{0x10C4, 0x00000000},	/* 00000000,10C4,Through,0dB,fs/1,invert=0 */
		{0x10C5, 0x00000000},	/* 00000000,10C5,Through,0dB,fs/1,invert=0 */
		{0x10C6, 0x3D506F00},	/* 3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x10C7, 0x3D506F00},	/* 3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x10C8, 0x3F65F240},	/* 3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x10C9, 0x3C5E3CC0},	/* 3C5E3CC0,10C9,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x10CA, 0x3C5E3CC0},	/* 3C5E3CC0,10CA,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x10CB, 0x3F7FE080},	/* 3F7FE080,10CB,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x10CC, 0x3E1D2100},	/* 3E1D2100,10CC,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x10CD, 0xBE1C0980},	/* BE1C0980,10CD,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x10CE, 0x3F7F5080},	/* 3F7F5080,10CE,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x10D0, 0x3FFF64C0},	/* 3FFF64C0,10D0,6dB,invert=0 */
		{0x10D1, 0x00000000},	/* 00000000,10D1,Cutoff,invert=0 */
		{0x10D2, 0x3F800000},	/* 3F800000,10D2,0dB,invert=0 */
		{0x10D3, 0x3F800000},	/* 3F800000,10D3,0dB,invert=0 */
		{0x10D4, 0x3F800000},	/* 3F800000,10D4,0dB,invert=0 */
		{0x10D5, 0x3F800000},	/* 3F800000,10D5,0dB,invert=0 */
		{0x10D7, 0x41FCFB80},	/* 41FCFB80,10D7,Through,30dB,fs/1,invert=0 */
		{0x10D8, 0x00000000},	/* 00000000,10D8,Through,30dB,fs/1,invert=0 */
		{0x10D9, 0x00000000},	/* 00000000,10D9,Through,30dB,fs/1,invert=0 */
		{0x10DA, 0x3F6FE180},	/* 3F6FE180,10DA,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x10DB, 0xBFDF9DC0},	/* BFDF9DC0,10DB,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x10DC, 0x3FDF9DC0},	/* 3FDF9DC0,10DC,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x10DD, 0x3F633A80},	/* 3F633A80,10DD,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x10DE, 0xBF531C00},	/* BF531C00,10DE,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x10E0, 0x3DD17800},	/* 3DD17800,10E0,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x10E1, 0x3DD17800},	/* 3DD17800,10E1,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x10E2, 0x3F4BA200},	/* 3F4BA200,10E2,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x10E3, 0x00000000},	/* 00000000,10E3,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x10E4, 0x00000000},	/* 00000000,10E4,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x10E5, 0x3F800000},	/* 3F800000,10E5,0dB,invert=0 */
		{0x10E8, 0x3F800000},	/* 3F800000,10E8,0dB,invert=0 */
		{0x10E9, 0x00000000},	/* 00000000,10E9,Cutoff,invert=0 */
		{0x10EA, 0x00000000},	/* 00000000,10EA,Cutoff,invert=0 */
		{0x10EB, 0x00000000},	/* 00000000,10EB,Cutoff,invert=0 */
		{0x10F0, 0x3F800000},	/* 3F800000,10F0,Through,0dB,fs/1,invert=0 */
		{0x10F1, 0x00000000},	/* 00000000,10F1,Through,0dB,fs/1,invert=0 */
		{0x10F2, 0x00000000},	/* 00000000,10F2,Through,0dB,fs/1,invert=0 */
		{0x10F3, 0x00000000},	/* 00000000,10F3,Through,0dB,fs/1,invert=0 */
		{0x10F4, 0x00000000},	/* 00000000,10F4,Through,0dB,fs/1,invert=0 */
		{0x10F5, 0x3F800000},	/* 3F800000,10F5,Through,0dB,fs/1,invert=0 */
		{0x10F6, 0x00000000},	/* 00000000,10F6,Through,0dB,fs/1,invert=0 */
		{0x10F7, 0x00000000},	/* 00000000,10F7,Through,0dB,fs/1,invert=0 */
		{0x10F8, 0x00000000},	/* 00000000,10F8,Through,0dB,fs/1,invert=0 */
		{0x10F9, 0x00000000},	/* 00000000,10F9,Through,0dB,fs/1,invert=0 */
#ifndef	XY_SIMU_SET
		{0x1100, 0x3F800000},	/* 3F800000,1100,0dB,invert=0 */
		{0x1101, 0x3F800000},	/* 3F800000,1101,0dB,invert=0 */
		{0x1102, 0x00000000},	/* 00000000,1102,Cutoff,invert=0 */
		{0x1103, 0x3EBFED40},	/*3EBFED40,1103,-8.5227dB,invert=0*/
		{0x1104, 0x3860DE00},	/* 3860DE00,1104,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1105, 0x3860DE00},	/* 3860DE00,1105,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1106, 0x3F7FF900},	/* 3F7FF900,1106,LPF,0.4Hz,0dB,fs/1,invert=0 */
		{0x1107, 0x3F800000},	/* 3F800000,1107,0dB,invert=0 */
		{0x1108, 0xBF800000},	/* BF800000,1108,0dB,invert=1 */
		{0x1109, 0x3F800000},	/* 3F800000,1109,0dB,invert=0 */
		{0x110A, 0x3F800000},	/* 3F800000,110A,0dB,invert=0 */
		{0x110B, 0x3F800000},	/* 3F800000,110B,0dB,invert=0 */
		{0x110C, 0x3F800000},	/* 3F800000,110C,0dB,invert=0 */
		{0x110E, 0x3F800000},	/* 3F800000,110E,0dB,invert=0 */
		{0x1110, 0x3DA2ADC0},	/* 3DA2ADC0,1110 */
		{0x1111, 0x00000000},	/* 00000000,1111,Free,fs/1,invert=0 */
		{0x1112, 0x3F7FFD80},	/* 3F7FFD80,1112,Free,fs/1,invert=0 */
		{0x1113, 0x3F76D280},	/*3F76D280,1113,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1114, 0xBF723C00},	/*BF723C00,1114,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1115, 0x3F690E80},	/*3F690E80,1115,HBF,70Hz,350Hz,0dB,fs/1,invert=0*/
		{0x1116, 0x40FE2B00},	/*40FE2B00,1116,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1117, 0xC0FE2B00},	/*C0FE2B00,1117,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1118, 0x3F7FF740},	/*3F7FF740,1118,HPF,0.5Hz,18dB,fs/1,invert=0*/
		{0x1119, 0x3ED1D880},	/*3ED1D880,1119,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x111A, 0xBED1CFC0},	/*BED1CFC0,111A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x111B, 0x3F7FFB80},	/*3F7FFB80,111B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
		{0x111C, 0x00000000},	/* 00000000,111C,Cutoff,invert=0*/
		{0x111D, 0x3F800000},	/* 3F800000,111D,0dB,invert=0 */
		{0x111E, 0x3F800000},	/* 3F800000,111E,0dB,invert=0 */
		{0x1120, 0x3F800000},	/* 3F800000,1120,0dB,invert=0 */
		{0x1121, 0x3F800000},	/* 3F800000,1121,0dB,invert=0 */
		{0x1122, 0x3F800000},	/* 3F800000,1122,0dB,invert=0 */
		{0x1123, 0x3F800000},	/* 3F800000,1123,Through,0dB,fs/1,invert=0 */
		{0x1124, 0x00000000},	/* 00000000,1124,Through,0dB,fs/1,invert=0 */
		{0x1125, 0x00000000},	/* 00000000,1125,Through,0dB,fs/1,invert=0 */
		{0x1126, 0x00000000},	/* 00000000,1126,Through,0dB,fs/1,invert=0 */
		{0x1127, 0x00000000},	/* 00000000,1127,Through,0dB,fs/1,invert=0 */
		{0x1130, 0x3F800000},	/* 3F800000,1130,Through,0dB,fs/1,invert=0 */
		{0x1131, 0x00000000},	/* 00000000,1131,Through,0dB,fs/1,invert=0 */
		{0x1132, 0x00000000},	/* 00000000,1132,Through,0dB,fs/1,invert=0 */
		{0x1133, 0x3F800000},	/* 3F800000,1133,Through,0dB,fs/1,invert=0 */
		{0x1134, 0x00000000},	/* 00000000,1134,Through,0dB,fs/1,invert=0 */
		{0x1135, 0x00000000},	/* 00000000,1135,Through,0dB,fs/1,invert=0 */
		{0x1136, 0x3F800000},	/* 3F800000,1136,Through,0dB,fs/1,invert=0 */
		{0x1137, 0x00000000},	/* 00000000,1137,Through,0dB,fs/1,invert=0 */
		{0x1138, 0x00000000},	/* 00000000,1138,Through,0dB,fs/1,invert=0 */
		{0x1139, 0x3F800000},	/* 3F800000,1139,Through,0dB,fs/1,invert=0 */
		{0x113A, 0x00000000},	/* 00000000,113A,Through,0dB,fs/1,invert=0 */
		{0x113B, 0x00000000},	/* 00000000,113B,Through,0dB,fs/1,invert=0 */
		{0x113C, 0x3F800000},	/* 3F800000,113C,Through,0dB,fs/1,invert=0 */
		{0x113D, 0x00000000},	/* 00000000,113D,Through,0dB,fs/1,invert=0 */
		{0x113E, 0x00000000},	/* 00000000,113E,Through,0dB,fs/1,invert=0 */
		{0x1143, 0x39D2BD40},	/* 39D2BD40,1143,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1144, 0x39D2BD40},	/* 39D2BD40,1144,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1145, 0x3F7FCB40},	/* 3F7FCB40,1145,LPF,3Hz,0dB,fs/1,invert=0 */
		{0x1146, 0x388C8A40},	/* 388C8A40,1146,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1147, 0x388C8A40},	/* 388C8A40,1147,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1148, 0x3F7FF740},	/* 3F7FF740,1148,LPF,0.5Hz,0dB,fs/1,invert=0 */
		{0x1149, 0x390C87C0},	/* 390C87C0,1149,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x114A, 0x390C87C0},	/* 390C87C0,114A,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x114B, 0x3F7FEE80},	/* 3F7FEE80,114B,LPF,1Hz,0dB,fs/1,invert=0 */
		{0x114C, 0x398C8300},	/* 398C8300,114C,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x114D, 0x398C8300},	/* 398C8300,114D,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x114E, 0x3F7FDCC0},	/* 3F7FDCC0,114E,LPF,2Hz,0dB,fs/1,invert=0 */
		{0x1153, 0x3F800000},	/* 3F800000,1153,Through,0dB,fs/1,invert=0 */
		{0x1154, 0x00000000},	/* 00000000,1154,Through,0dB,fs/1,invert=0 */
		{0x1155, 0x00000000},	/* 00000000,1155,Through,0dB,fs/1,invert=0 */
		{0x1156, 0x3F800000},	/* 3F800000,1156,Through,0dB,fs/1,invert=0 */
		{0x1157, 0x00000000},	/* 00000000,1157,Through,0dB,fs/1,invert=0 */
		{0x1158, 0x00000000},	/* 00000000,1158,Through,0dB,fs/1,invert=0 */
		{0x1159, 0x3F800000},	/* 3F800000,1159,Through,0dB,fs/1,invert=0 */
		{0x115A, 0x00000000},	/* 00000000,115A,Through,0dB,fs/1,invert=0 */
		{0x115B, 0x00000000},	/* 00000000,115B,Through,0dB,fs/1,invert=0 */
		{0x115C, 0x3F800000},	/* 3F800000,115C,Through,0dB,fs/1,invert=0 */
		{0x115D, 0x00000000},	/* 00000000,115D,Through,0dB,fs/1,invert=0 */
		{0x115E, 0x00000000},	/* 00000000,115E,Through,0dB,fs/1,invert=0 */
		{0x1163, 0x3F800000},	/* 3F800000,1163,0dB,invert=0 */
		{0x1166, 0x3F800000},	/* 3F800000,1166,0dB,invert=0 */
		{0x1169, 0x3F800000},	/* 3F800000,1169,0dB,invert=0 */
		{0x116C, 0x3F800000},	/* 3F800000,116C,0dB,invert=0 */
		{0x1173, 0x00000000},	/* 00000000,1173,Cutoff,invert=0 */
		{0x1176, 0x3F800000},	/* 3F800000,1176,0dB,invert=0 */
		{0x1179, 0x3F800000},	/* 3F800000,1179,0dB,invert=0 */
		{0x117C, 0x3F800000},	/* 3F800000,117C,0dB,invert=0 */
		{0x1183, 0x38D1B700},	/* 38D1B700,1183,-80dB,invert=0 */
		{0x1186, 0x00000000},	/* 00000000,1186,Cutoff,invert=0 */
		{0x1189, 0x00000000},	/* 00000000,1189,Cutoff,invert=0 */
		{0x118C, 0x00000000},	/* 00000000,118C,Cutoff,invert=0 */
		{0x1193, 0x00000000},	/* 00000000,1193,Cutoff,invert=0 */
		{0x1198, 0x3F800000},	/* 3F800000,1198,0dB,invert=0 */
		{0x1199, 0x3F800000},	/* 3F800000,1199,0dB,invert=0 */
		{0x119A, 0x3F800000},	/* 3F800000,119A,0dB,invert=0 */
		{0x11A1, 0x3C58B440},	/* 3C58B440,11A1,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A2, 0x3C58B440},	/* 3C58B440,11A2,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A3, 0x3F793A40},	/* 3F793A40,11A3,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A4, 0x3C58B440},	/* 3C58B440,11A4,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A5, 0x3C58B440},	/* 3C58B440,11A5,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A6, 0x3F793A40},	/* 3F793A40,11A6,LPF,100Hz,0dB,fs/1,invert=0 */
		{0x11A7, 0x3F800000},	/* 3F800000,11A7,Through,0dB,fs/1,invert=0 */
		{0x11A8, 0x00000000},	/* 00000000,11A8,Through,0dB,fs/1,invert=0 */
		{0x11A9, 0x00000000},	/* 00000000,11A9,Through,0dB,fs/1,invert=0 */
		{0x11AA, 0x00000000},	/* 00000000,11AA,Cutoff,invert=0 */
		{0x11AB, 0x3BDA2580},	/* 3BDA2580,11AB,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x11AC, 0x3BDA2580},	/* 3BDA2580,11AC,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x11AD, 0x3F7C9780},	/* 3F7C9780,11AD,LPF,50Hz,0dB,fs/1,invert=0 */
		{0x11B0, 0x3F800000},	/* 3F800000,11B0,Through,0dB,fs/1,invert=0 */
		{0x11B1, 0x00000000},	/* 00000000,11B1,Through,0dB,fs/1,invert=0 */
		{0x11B2, 0x00000000},	/* 00000000,11B2,Through,0dB,fs/1,invert=0 */
		{0x11B3, 0x3F800000},	/* 3F800000,11B3,0dB,invert=0 */
		{0x11B4, 0x00000000},	/* 00000000,11B4,Cutoff,invert=0 */
		{0x11B5, 0x00000000},	/* 00000000,11B5,Cutoff,invert=0 */
		{0x11B6, 0x3F353C00},	/* 3F353C00,11B6,-3dB,invert=0 */
		{0x11B8, 0x3F800000},	/* 3F800000,11B8,0dB,invert=0 */
		{0x11B9, 0x00000000},	/* 00000000,11B9,Cutoff,invert=0*/
		{0x11C0, 0x3FCC34C0},	/* 3FCC34C0,11C0,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x11C1, 0xBFCA0700},	/* BFCA0700,11C1,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x11C2, 0x3F48E240},	/* 3F48E240,11C2,HBF,40Hz,900Hz,5dB,fs/1,invert=0 */
		{0x11C3, 0x3F800000},	/* 3F800000,11C3,Through,0dB,fs/1,invert=0 */
		{0x11C4, 0x00000000},	/* 00000000,11C4,Through,0dB,fs/1,invert=0 */
		{0x11C5, 0x00000000},	/* 00000000,11C5,Through,0dB,fs/1,invert=0 */
		{0x11C6, 0x3D506F00},	/* 3D506F00,11C6,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x11C7, 0x3D506F00},	/* 3D506F00,11C7,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x11C8, 0x3F65F240},	/* 3F65F240,11C8,LPF,400Hz,0dB,fs/1,invert=0 */
		{0x11C9, 0x3C5E3CC0},	/* 3C5E3CC0,11C9,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x11CA, 0x3C5E3CC0},	/* 3C5E3CC0,11CA,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x11CB, 0x3F7FE080},	/* 3F7FE080,11CB,LPF,1.8Hz,35dB,fs/1,invert=0 */
		{0x11CC, 0x3E1D2100},	/* 3E1D2100,11CC,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x11CD, 0xBE1C0980},	/* BE1C0980,11CD,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x11CE, 0x3F7F5080},	/* 3F7F5080,11CE,LBF,10Hz,26Hz,-8dB,fs/1,invert=0 */
		{0x11D0, 0x3FFF64C0},	/* 3FFF64C0,11D0,6dB,invert=0 */
		{0x11D1, 0x00000000},	/* 00000000,11D1,Cutoff,invert=0 */
		{0x11D2, 0x3F800000},	/* 3F800000,11D2,0dB,invert=0 */
		{0x11D3, 0x3F800000},	/* 3F800000,11D3,0dB,invert=0 */
		{0x11D4, 0x3F800000},	/* 3F800000,11D4,0dB,invert=0 */
		{0x11D5, 0x3F800000},	/* 3F800000,11D5,0dB,invert=0 */
		{0x11D7, 0x41FCFB80},	/* 41FCFB80,11D7,Through,30dB,fs/1,invert=0 */
		{0x11D8, 0x00000000},	/* 00000000,11D8,Through,30dB,fs/1,invert=0 */
		{0x11D9, 0x00000000},	/* 00000000,11D9,Through,30dB,fs/1,invert=0 */
		{0x11DA, 0x3F6FE180},	/* 3F6FE180,11DA,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x11DB, 0xBFDF9DC0},	/* BFDF9DC0,11DB,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x11DC, 0x3FDF9DC0},	/* 3FDF9DC0,11DC,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x11DD, 0x3F633A80},	/* 3F633A80,11DD,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x11DE, 0xBF531C00},	/* BF531C00,11DE,PKF,1100Hz,-11dB,5,fs/1,invert=0 */
		{0x11E0, 0x3DD17800},	/* 3DD17800,11E0,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x11E1, 0x3DD17800},	/* 3DD17800,11E1,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x11E2, 0x3F4BA200},	/* 3F4BA200,11E2,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x11E3, 0x00000000},	/* 00000000,11E3,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x11E4, 0x00000000},	/* 00000000,11E4,LPF,850Hz,0dB,fs/1,invert=0 */
		{0x11E5, 0x3F800000},	/* 3F800000,11E5,0dB,invert=0 */
		{0x11E8, 0x3F800000},	/* 3F800000,11E8,0dB,invert=0 */
		{0x11E9, 0x00000000},	/* 00000000,11E9,Cutoff,invert=0 */
		{0x11EA, 0x00000000},	/* 00000000,11EA,Cutoff,invert=0 */
		{0x11EB, 0x00000000},	/* 00000000,11EB,Cutoff,invert=0 */
		{0x11F0, 0x3F800000},	/* 3F800000,11F0,Through,0dB,fs/1,invert=0 */
		{0x11F1, 0x00000000},	/* 00000000,11F1,Through,0dB,fs/1,invert=0 */
		{0x11F2, 0x00000000},	/* 00000000,11F2,Through,0dB,fs/1,invert=0 */
		{0x11F3, 0x00000000},	/* 00000000,11F3,Through,0dB,fs/1,invert=0 */
		{0x11F4, 0x00000000},	/* 00000000,11F4,Through,0dB,fs/1,invert=0 */
		{0x11F5, 0x3F800000},	/* 3F800000,11F5,Through,0dB,fs/1,invert=0 */
		{0x11F6, 0x00000000},	/* 00000000,11F6,Through,0dB,fs/1,invert=0 */
		{0x11F7, 0x00000000},	/* 00000000,11F7,Through,0dB,fs/1,invert=0 */
		{0x11F8, 0x00000000},	/* 00000000,11F8,Through,0dB,fs/1,invert=0 */
		{0x11F9, 0x00000000},	/* 00000000,11F9,Through,0dB,fs/1,invert=0 */
#endif	/* XY_SIMU_SET */
		{0x1200, 0x00000000},	/* 00000000,1200,Cutoff,invert=0 */
		{0x1201, 0x3F800000},	/* 3F800000,1201,0dB,invert=0 */
		{0x1202, 0x3F800000},	/* 3F800000,1202,0dB,invert=0 */
		{0x1203, 0x3F800000},	/* 3F800000,1203,0dB,invert=0 */
		{0x1204, 0x3F800000},	/* 3F800000,1204,Through,0dB,fs/1,invert=0 */
		{0x1205, 0x00000000},	/* 00000000,1205,Through,0dB,fs/1,invert=0 */
		{0x1206, 0x00000000},	/* 00000000,1206,Through,0dB,fs/1,invert=0 */
		{0x1207, 0x3F800000},	/* 3F800000,1207,Through,0dB,fs/1,invert=0 */
		{0x1208, 0x00000000},	/* 00000000,1208,Through,0dB,fs/1,invert=0 */
		{0x1209, 0x00000000},	/* 00000000,1209,Through,0dB,fs/1,invert=0 */
		{0x120A, 0x3F800000},	/* 3F800000,120A,Through,0dB,fs/1,invert=0 */
		{0x120B, 0x00000000},	/* 00000000,120B,Through,0dB,fs/1,invert=0 */
		{0x120C, 0x00000000},	/* 00000000,120C,Through,0dB,fs/1,invert=0 */
		{0x120D, 0x3F800000},	/* 3F800000,120D,Through,0dB,fs/1,invert=0 */
		{0x120E, 0x00000000},	/* 00000000,120E,Through,0dB,fs/1,invert=0 */
		{0x120F, 0x00000000},	/* 00000000,120F,Through,0dB,fs/1,invert=0 */
		{0x1210, 0x3F800000},	/* 3F800000,1210,Through,0dB,fs/1,invert=0 */
		{0x1211, 0x00000000},	/* 00000000,1211,Through,0dB,fs/1,invert=0 */
		{0x1212, 0x00000000},	/* 00000000,1212,Through,0dB,fs/1,invert=0 */
		{0x1213, 0x3F800000},	/* 3F800000,1213,0dB,invert=0 */
		{0x1214, 0x3F800000},	/* 3F800000,1214,0dB,invert=0 */
		{0x1215, 0x3F800000},	/* 3F800000,1215,0dB,invert=0 */
		{0x1216, 0x3F800000},	/* 3F800000,1216,0dB,invert=0 */
		{0x1217, 0x3F800000},	/* 3F800000,1217,0dB,invert=0 */
		{0x1218, 0x00000000},	/* 00000000,1218,Cutoff,fs/1,invert=0 */
		{0x1219, 0x00000000},	/* 00000000,1219,Cutoff,fs/1,invert=0 */
		{0x121A, 0x00000000},	/* 00000000,121A,Cutoff,fs/1,invert=0 */
		{0x121B, 0x00000000},	/* 00000000,121B,Cutoff,fs/1,invert=0 */
		{0x121C, 0x00000000},	/* 00000000,121C,Cutoff,fs/1,invert=0 */
		{0x121D, 0x3F800000},	/* 3F800000,121D,0dB,invert=0 */
		{0x121E, 0x3F800000},	/* 3F800000,121E,0dB,invert=0 */
		{0x121F, 0x3F800000},	/* 3F800000,121F,0dB,invert=0 */
		{0x1235, 0x3F800000},	/* 3F800000,1235,0dB,invert=0 */
		{0x1236, 0x3F800000},	/* 3F800000,1236,0dB,invert=0 */
		{0x1237, 0x3F800000},	/* 3F800000,1237,0dB,invert=0 */
		{0x1238, 0x3F800000},	/* 3F800000,1238,0dB,invert=0 */
		{0xFFFF, 0xFFFFFFFF}
	}
};

/* DI Coefficient Setting Value */
#define	 COEFTBL	7
const uint32_t ClDiCof[COEFTBL] = {
	0x3F7FFD80,	/* 0 */
	0x3F7FFD80,	/* 1 */
	0x3F7FFD80,	/* 2 */
	0x3F7FFD80,	/* 3 */
	0x3F7FFD80,	/* 4 */
	0x3F7FFD80,	/* 5 */
	0x3F7FFD80	/* 6 */
};

/*********************************************************************
	Global Variable
*********************************************************************/
int32_t lc898122a_i2c_check(void)
{
	uint8_t UcLsiVer;
	RegReadA(CVER, &UcLsiVer);	/* 0x27E */
	return (UcLsiVer == 0xA1) ? 1 : 0;	/*In the case of using LC898122A */
}

void lc898122a_wait_time(uint16_t Uslc898122a_wait_time)
{
	usleep(Uslc898122a_wait_time * 1000);
}

/*********************************************************************
 (Function Name	: lc898122a_init
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Initial Setting Function
 History			: First edition							2009.07.30 Y.Tashita
*********************************************************************/
int32_t	lc898122a_init(void)
{
	if (lc898122a_i2c_check() == 0)
		return OIS_FW_POLLING_FAIL;

	/* RegWriteA(SOFTRES2, 0x0F); */
	/* lc898122a_wait_time(5); */

	/* Read E2PROM Data */
	lc898122a_e2p_data_read();
	/* Get Version Info. */
	if (lc898122a_version_check() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_VERSION_FAIL;
	/* Clock Setting */
	lc898122a_init_clock();
	/* AF Initial Setting */
	lc898122a_init_af();
	/* I/O Port Initial Setting */
	lc898122a_init_io_port();
	/* Monitor & Other Initial Setting */
	lc898122a_init_monitor();
	/* Servo Initial Setting */
	lc898122a_init_servo();
	/* Gyro Initial Setting */
	lc898122a_init_gyro_setting();
	/* Filter Initial Setting */
	if (lc898122a_init_gyro_filter() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;
	/* DigitalGyro Initial Setting */
	if (lc898122a_init_digital_gyro() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;
	/* Adjust Fix Value Setting */
	lc898122a_init_adjust_value();

	return OIS_FW_POLLING_PASS;
}
/*********************************************************************
 Function Name	: lc898122a_e2p_data_read
 Return Value		: NON
 Argument Value	: NON
 Explanation		: E2PROM Calibration Data Read Function
 History			: First edition							2013.06.21 Y.Kim
*********************************************************************/
void lc898122a_e2p_data_read(void)
{
	uint8_t UcGvcFlg;

	lc898122a_mem_clear((uint8_t *)&StCalDat, sizeof(stCalDat));

	E2pRed((uint16_t)0x093B, 1, (uint8_t *)&UcGvcFlg);	/*GYRO OFFSET Mobile Flag */

	E2pRed((uint16_t)HALL_BIAS_X, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlxGan);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)HALL_BIAS_Y, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlyGan);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)HALL_OFFSET_X, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlxOff);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)HALL_OFFSET_Y, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlyOff);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)LOOP_GAIN_X, 2, (uint8_t *)&StCalDat.StLopGan.UsLxgVal);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)LOOP_GAIN_Y, 2, (uint8_t *)&StCalDat.StLopGan.UsLygVal);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)LENS_CENTER_FINAL_X, 2, (uint8_t *)&StCalDat.StLenCen.UsLsxVal);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)LENS_CENTER_FINAL_Y, 2, (uint8_t *)&StCalDat.StLenCen.UsLsyVal);
	/*	lc898122a_wait_time(5); */

	if (0xE7 == UcGvcFlg) {
		E2pRed((uint16_t)0x0937, 2, (uint8_t *)&StCalDat.StGvcOff.UsGxoVal);
		/*		lc898122a_wait_time(5); */	/*GYRO OFFSET Mobile */
		E2pRed((uint16_t)0x0939, 2, (uint8_t *)&StCalDat.StGvcOff.UsGyoVal);
		/*		lc898122a_wait_time(5); */	/*GYRO OFFSET Mobile */
	} else {
		E2pRed((uint16_t)GYRO_AD_OFFSET_X, 2, (uint8_t *)&StCalDat.StGvcOff.UsGxoVal);
		/*		lc898122a_wait_time(5); */
		E2pRed((uint16_t)GYRO_AD_OFFSET_Y, 2, (uint8_t *)&StCalDat.StGvcOff.UsGyoVal);
		/*		lc898122a_wait_time(5); */
	}

	E2pRed((uint16_t)OSC_CLK_VAL, 1, (uint8_t *)&StCalDat.UcOscVal);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)ADJ_HALL_FLAG, 2, (uint8_t *)&StCalDat.UsAdjHallF);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)ADJ_GYRO_FLAG, 2, (uint8_t *)&StCalDat.UsAdjGyroF);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)ADJ_LENS_FLAG, 2, (uint8_t *)&StCalDat.UsAdjLensF);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)GYRO_GAIN_X, 4, (uint8_t *)&StCalDat.UlGxgVal);
	/*	lc898122a_wait_time(5); */
	E2pRed((uint16_t)GYRO_GAIN_Y, 4, (uint8_t *)&StCalDat.UlGygVal);
	/*	lc898122a_wait_time(5); */

	E2pRed((uint16_t)FW_VERSION_INFO, 2, (uint8_t *)&StCalDat.UsVerDat);
	/*	lc898122a_wait_time(5); */

	return;
}

/**********************************************************************
 Function Name	: lc898122a_version_check
 Return Value		: Vesion check result
 Argument Value	: NON
 Explanation		: F/W Version Check
 History			: First edition							2013.03.21 Y.Kim
*********************************************************************/
int32_t	lc898122a_version_check(void)
{
	UcVerHig = (uint8_t)(StCalDat.UsVerDat >> 8);		/* System Version */
	UcVerLow = (uint8_t)(StCalDat.UsVerDat);			/* Filter Version */

	CDBG("%s:UcVerHig 0x%x, UcVerLow 0x%x\n", __func__, UcVerHig, UcVerLow);

	if (UcVerHig == 0xA1)	/* 0xA1 1st Sample STM Gyro*/
		UcVerHig = 0x00;
	else if (UcVerHig == 0xB1)
		UcVerHig = 0x01;	/* 0xB2 2nd Sample INV Gyro */
	else
		return OIS_FW_POLLING_VERSION_FAIL;

	/* Matching Version -> Filter */
	if (UcVerLow == 0xA1)			/* 0xA1 1st Sample STM Gyro */
		UcVerLow = 0x00;
	else if (UcVerLow == 0xB1)
		UcVerLow = 0x00;			/* 0xB2 2nd Sample INV Gyro */
	else
		return OIS_FW_POLLING_VERSION_FAIL;

	return OIS_FW_POLLING_PASS;
}

/*********************************************************************
 Function Name	: lc898122a_init_clock
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Clock Setting
 History			: First edition							2013.01.08 Y.Shigeoka
*********************************************************************/
void lc898122a_init_clock(void)
{
	RegReadA(CVER, &UcCvrCod);		/* 0x027E */
	/*Clock Enables*/
	RegWriteA(CLKON, 0x1F);			/* 0x020B */
}

/*********************************************************************
 Function Name	: lc898122a_init_af
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Open AF Initial Setting
 History			: First edition							2013.09.12 Y.Shigeoka
*********************************************************************/
void lc898122a_init_af(void)
{
	uint8_t UcStbb0;

	lc898122a_af_driver_mode(OFF);					/* AF Drvier Block Ena=0 */
	RegWriteA(DRVFCAF, 0x20);		/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-2 */
	RegWriteA(DRVFC3AF, 0x00);		/* 0x0083 DGAINDAF	Gain 0 */
	RegWriteA(DRVFC4AF, 0x80);		/* 0x0084 DOFSTDAF */
	RegWriteA(PWMAAF, 0x00);		/* 0x0090 AF PWM standby */
	RegWriteA(AFFC, 0x80);			/* 0x0088 OpenAF/-/- */
	RegWriteA(DRVFC2AF, 0x00);		/* 0x0082 AF slope0 */
	RegWriteA(DRVCH3SEL, 0x00);		/* 0x0085 AF H bridge control */
	RegWriteA(PWMFCAF, 0x01);		/* 0x0091 AF VREF, Carrier, MODE1 */
	RegWriteA(PWMPERIODAF, 0x20);	/* 0x0099 AF none-synchronism */
	RegWriteA(CCFCAF, 0x40);		/* 0x00A1 GND/- */

	RegReadA(STBB0, &UcStbb0);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
	UcStbb0 &= 0x7F;
	RegWriteA(STBB0, UcStbb0);		/* 0x0250 OIS standby */
	RegWriteA(STBB1, 0x00);			/* 0x0264 All standby */

	/* AF Initial setting */
	RegWriteA(FSTMODE, FSTMODE_AF);		/* 0x0302 */
	RamWriteA(RWEXD1_L,	RWEXD1_L_AF);	/* 0x0396 - 0x0397 (Register continuos write) */
	RamWriteA(RWEXD2_L,	RWEXD2_L_AF);	/* 0x0398 - 0x0399 (Register continuos write) */
	RamWriteA(RWEXD3_L,	RWEXD3_L_AF);	/* 0x039A - 0x039B (Register continuos write) */
	RegWriteA(FSTCTIME,	FSTCTIME_AF);	/* 0x0303 */
	RamWriteA(TCODEH, 0x0000);			/* 0x0304 - 0x0305 (Register continuos write) */
	RamWriteA(LTHDH, LTHDHL_AF);		/* 0x0306 - 0x0307 (Register continuos write) */

	UcStbb0 |= 0x80;
	RegWriteA(STBB0, UcStbb0);		/* 0x0250 */
	RegWriteA(STBB1, 0x05);			/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */

	lc898122a_af_driver_mode(ON);					/* AF Driver Block Ena=1 */
}

/*********************************************************************
 Function Name	: lc898122a_af_driver_mode
 Return Value		: NON
 Argument Value	: 0:OFF  1:ON
 Explanation		: AF Driver Mode setting function
 History			: First edition							2013.09.12 Y.Shigeoka
*********************************************************************/
void lc898122a_af_driver_mode(uint8_t UcDrvSw)
{
	if (UcDrvSw == ON) {
		RegWriteA(DRVFCAF, 0x20);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-2 */
		RegWriteA(CCAAF, 0x80);		/* 0x00A0 [7]=0:OFF 1:ON */
	} else {
		RegWriteA(CCAAF, 0x00);		/* 0x00A0 [7]=0:OFF 1:ON */
	}
}

/*********************************************************************
 Function Name	: lc898122a_af_vcm_code
 Return Value		: NON
 Argument Value	: AF Code
 Explanation		: VCM AF Code setting function
 History			: First edition							2013.12.23 YS.Kim
*********************************************************************/
#define	MAX_ACTCODE 1023
#define SET_AFMODE	0x0400	/* 0x0000 = DIRECT MODE ON (FAST MODE OFF) */
/* 0x0400 = FAST MODE ON */
/* 0x0C00 = FAST MODE ON & AUTO PARAMETER */
void lc898122a_af_vcm_code(uint16_t UsCodVal)
{
	if (UsCodVal < 0)
		UsCodVal = 0;
	else if (UsCodVal > MAX_ACTCODE)
		UsCodVal = MAX_ACTCODE;

	UsCodVal |= SET_AFMODE;
	RamWriteA(TCODEH, UsCodVal);
}

/*********************************************************************
 Function Name	: lc898122a_init_io_port
 Return Value		: NON
 Argument Value	: NON
 Explanation		: I/O Port Initial Setting
 History			: First edition							2013.01.08 Y.Shigeoka
*********************************************************************/
void lc898122a_init_io_port(void)
{
	/*select IOP signal*/
	if (UcVerHig == 0x00)
		RegWriteA(IOP1SEL, 0x02);	/* 0x0231	IOP1 : IOP1 */
	else
		RegWriteA(IOP1SEL, 0x00);	/* 0x0231	IOP1 : DGDATAIN (ATT:0236h[0]=1) */
}

/*********************************************************************
 Function Name	: lc898122a_init_digital_gyro
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Digital Gyro Initial Setting
 History			: First edition							2013.01.08 Y.Shigeoka
*********************************************************************/
int32_t	lc898122a_init_digital_gyro(void)
{
	uint8_t UcGrini;

	/*************
	 For ST gyro
	************/

	/* Set SPI Type */
	if (UcVerHig == 0x00) {
		RegWriteA(SPIM, 0x00);	/* 0x028F	[ - | - | - | - ][ - | - | - | DGSPI4 ] */
		/* DGSPI4	0: 3-wire SPI, 1: 4-wire SPI */
	} else {
		RegWriteA(SPIM, 0x01);	/* 0x028F	[ - | - | - | - ][ - | - | - | DGSPI4 ] */
		/* DGSPI4	0: 3-wire SPI, 1: 4-wire SPI */
	}
	/* Set to Command Mode */
	RegWriteA(GRSEL, 0x01);		/* 0x0280 [ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ] */

	/* Digital Gyro Read settings */
	RegWriteA(GRINI, 0x80);		/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */

	RegReadA(GRINI, &UcGrini);	/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */
	RegWriteA(GRINI, (uint8_t)(UcGrini | SLOWMODE));		/* 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */
	if (UcVerHig == 0x00) {
		RegWriteA(LSBF, 0x03);		/* 0x028D */

		/* Set Digital Gyro Settings */
		RegWriteA(GRADR0, 0x0B);	/* 0x0283 */
		RegWriteA(GSETDT, 0x07);					/* 0x028A	Enable BDU, Enable 3-wire SPI mode */
		RegWriteA(GRACC, 0x10);		/* 0x0282 [ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ] */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */

		/* Set Digital Gyro Settings */
		RegWriteA(GRADR0, 0x1F);	/* 0x0283 */
		RegWriteA(GSETDT, 0x08);	/* 0x028A FS = +/-130dps */
		RegWriteA(GRACC, 0x10);		/* 0x0282 [ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ] */
		lc898122a_acc_wait(0x10);

		/* Set Digital Gyro Settings */
		RegWriteA(GRADR0, 0x0D);		/* 0x0283 */
		RegWriteA(GSETDT, 0x01);	/* 0x028A LPF = 300Hz */
		RegWriteA(GRACC, 0x10);		/* 0x0282 [ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ] */
		lc898122a_acc_wait(0x10);	/* Digital Gyro busy wait */
	} else {
		RegWriteA(GRADR0, 0x6B);	/* 0x0283 Set CLKSEL*/
		RegWriteA(GSETDT, 0x01);	/* 0x028A Set Write Data */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

		RegWriteA(GRADR0, 0x1B);			/* 0x0283 Set FS_SEL */
		RegWriteA(GSETDT, (FS_SEL << 3));	/* 0x028A Set Write Data */
		RegWriteA(GRACC, 0x10);				/* 0x0282 Set Trigger ON */
		if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

		RegWriteA(GRADR0, 0x1A);			/* 0x0283 Set DLPF_CFG */
		RegWriteA(GSETDT, 0x00);			/* 0x028A Set Write Data */
		RegWriteA(GRACC, 0x10);				/* 0x0282 Set Trigger ON */
		if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

		RegWriteA(GRADR0, 0x6A);	/* 0x0283 Set I2C_IF_DIS, SIG_COND_RESET */
		RegWriteA(GSETDT, 0x11);	/* 0x028A Set Write Data */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL;	/* Digital Gyro busy wait */
	}
	RegReadA(GRINI, &UcGrini);	/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */
	RegWriteA(GRINI, (uint8_t)(UcGrini & ~SLOWMODE));		/* 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */

	RegWriteA(RDSEL, 0x7C);		/* 0x028B RDSEL(Data1 and 2 for continuos mode) */
	lc898122a_select_gyro_sig();

	return OIS_FW_POLLING_PASS;
}

/*********************************************************************
 Function Name	: lc898122a_init_monitor
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Monitor & Other Initial Setting
 History			: First edition							2013.01.08 Y.Shigeoka
*********************************************************************/
void lc898122a_init_monitor(void)
{
	RegWriteA(PWMMONA, 0x00);		/* 0x0030 0:off */

	RegWriteA(MONSELA, 0x5C);		/* 0x0270 DLYMON1 */
	RegWriteA(MONSELB, 0x5D);		/* 0x0271 DLYMON2 */
	RegWriteA(MONSELC, 0x00);		/* 0x0272 */
	RegWriteA(MONSELD, 0x00);		/* 0x0273 */

	/* Monitor Circuit */
	RegWriteA(WC_PINMON1, 0x00);	/* 0x01C0	Filter Monitor */
	RegWriteA(WC_PINMON2, 0x00);	/* 0x01C1 */
	RegWriteA(WC_PINMON3, 0x00);	/* 0x01C2 */
	RegWriteA(WC_PINMON4, 0x00);	/* 0x01C3 */
	/* Delay Monitor */
	RegWriteA(WC_DLYMON11, 0x04);	/* 0x01C5 DlyMonAdd1[10:8] */
	RegWriteA(WC_DLYMON10, 0x40);	/* 0x01C4 DlyMonAdd1[ 7:0] */
	RegWriteA(WC_DLYMON21, 0x04);	/* 0x01C7 DlyMonAdd2[10:8] */
	RegWriteA(WC_DLYMON20, 0xC0);	/* 0x01C6 DlyMonAdd2[ 7:0] */
	RegWriteA(WC_DLYMON31, 0x00);	/* 0x01C9 DlyMonAdd3[10:8] */
	RegWriteA(WC_DLYMON30, 0x00);	/* 0x01C8 DlyMonAdd3[ 7:0] */
	RegWriteA(WC_DLYMON41, 0x00);	/* 0x01CB DlyMonAdd4[10:8] */
	RegWriteA(WC_DLYMON40, 0x00);	/* 0x01CA DlyMonAdd4[ 7:0] */

	/* Monitor */
	RegWriteA(PWMMONA, 0x80);		/* 0x0030 1:on */
}

/*********************************************************************
 Function Name	: lc898122a_init_servo
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Servo Initial Setting
 History			: First edition							2013.01.08 Y.Shigeoka
*********************************************************************/
void lc898122a_init_servo(void)
{
	uint8_t UcStbb0;

	UcPwmMod = INIT_PWMMODE;		/* Driver output mode */

	RegWriteA(WC_EQON, 0x00);		/* 0x0101 Filter Calcu */
	RegWriteA(WC_RAMINITON, 0x00);	/* 0x0102 */
	lc898122a_gyro_ram_clear(0x0000, CLR_ALL_RAM);	/* All Clear */

	RegWriteA(WH_EQSWX,	0x02);		/* 0x0170 [ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ] */
	RegWriteA(WH_EQSWY,	0x02);		/* 0x0171 [ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ] */

	lc898122a_ram_mode(OFF);				/* 32bit Float mode */

	/* Monitor Gain */
	RamWrite32A(dm1g, 0x3F800000);	/* 0x109A */
	RamWrite32A(dm2g, 0x3F800000);	/* 0x109B */
	RamWrite32A(dm3g, 0x3F800000);	/* 0x119A */
	RamWrite32A(dm4g, 0x3F800000);	/* 0x119B */

	/* Hall output limiter */
	RamWrite32A(sxlmta1, 0x3F800000);	/* 0x10E6 Hall X output Limit */
	RamWrite32A(sylmta1, 0x3F800000);	/* 0x11E6 Hall Y output Limit */

	/* Emergency Stop */
	RegWriteA(WH_EMGSTPON, 0x00);		/* 0x0178 Emergency Stop OFF */
	RegWriteA(WH_EMGSTPTMR, 0xFF);		/* 0x017A 255*(16/23.4375kHz)=174ms */

	RamWrite32A(sxemglev, 0x3F800000);	/* 0x10EC Hall X Emergency threshold */
	RamWrite32A(syemglev, 0x3F800000);	/* 0x11EC Hall Y Emergency threshold */

	/* Hall Servo smoothing */
	RegWriteA(WH_SMTSRVON, 0x00);		/* 0x017C Smooth Servo OFF */
	RegWriteA(WH_SMTSRVSMP, 0x06);		/* 0x017D 2.7ms=2^06/23.4375kHz */
	RegWriteA(WH_SMTTMR, 0x01);			/* 0x017E 1.3ms=(1+1)*16/23.4375kHz */

	RamWrite32A(sxsmtav, 0xBC800000);	/* 0x10ED 1/64 X smoothing ave coefficient */
	RamWrite32A(sysmtav, 0xBC800000);	/* 0x11ED 1/64 Y smoothing ave coefficient */
	RamWrite32A(sxsmtstp, 0x3AE90466);	/* 0x10EE 0.001778 X smoothing offset */
	RamWrite32A(sysmtstp, 0x3AE90466);	/* 0x11EE 0.001778 Y smoothing offset */

	/* High-dimensional correction  */
	RegWriteA(WH_HOFCON, 0x11);			/* 0x0174 OUT 3x3 */

	/* (0.4531388X^3+0.4531388X)*(0.4531388X^3+0.4531388X) 15ohm*/
	/* Front */
	RamWrite32A(sxiexp3, 0x3EE801CF);	/* 0x10BA */
	RamWrite32A(sxiexp2, 0x00000000);	/* 0x10BB */
	RamWrite32A(sxiexp1, 0x3EE801CF);	/* 0x10BC */
	RamWrite32A(sxiexp0, 0x00000000);	/* 0x10BD */
	RamWrite32A(sxiexp, 0x3F800000);	/* 0x10BE */

	RamWrite32A(syiexp3, 0x3EE801CF);	/* 0x11BA */
	RamWrite32A(syiexp2, 0x00000000);	/* 0x11BB */
	RamWrite32A(syiexp1, 0x3EE801CF);	/* 0x11BC */
	RamWrite32A(syiexp0, 0x00000000);	/* 0x11BD */
	RamWrite32A(syiexp, 0x3F800000);	/* 0x11BE */

	/* Back */
	RamWrite32A(sxoexp3, 0x3EE801CF);	/* 0x10FA */
	RamWrite32A(sxoexp2, 0x00000000);	/* 0x10FB */
	RamWrite32A(sxoexp1, 0x3EE801CF);	/* 0x10FC */
	RamWrite32A(sxoexp0, 0x00000000);	/* 0x10FD */
	RamWrite32A(sxoexp, 0x3F800000);	/* 0x10FE */

	RamWrite32A(syoexp3, 0x3EE801CF);	/* 0x11FA */
	RamWrite32A(syoexp2, 0x00000000);	/* 0x11FB */
	RamWrite32A(syoexp1, 0x3EE801CF);	/* 0x11FC */
	RamWrite32A(syoexp0, 0x00000000);	/* 0x11FD */
	RamWrite32A(syoexp, 0x3F800000);	/* 0x11FE */

	/* Ram Access */
	lc898122a_ram_mode(OFF);	/* 32bit float mode */

	/* PWM Signal Generate */
	lc898122a_driver_mode(OFF);					/* 0x0070 Driver Block Ena=0 */
	RegWriteA(DRVFC2, 0x90);	/* 0x0002 Slope 3, Dead Time = 30ns  */
	RegWriteA(DRVSELX, 0xFF);	/* 0x0003 PWM X drv max current  DRVSELX[7:0] */
	RegWriteA(DRVSELY, 0xFF);	/* 0x0004 PWM Y drv max current  DRVSELY[7:0 ] */

#ifdef PWM_BREAK
	RegWriteA(PWMFC, 0x3D);			/* 0x0011 VREF, PWMCLK/128, MODE0B, 12Bit Accuracy */
#else
	RegWriteA(PWMFC, 0x21);			/* 0x0011 VREF, PWMCLK/256, MODE1, 12Bit Accuracy */
#endif

	RegWriteA(PWMA, 0x00);			/* 0x0010 PWM X/Y standby */
	RegWriteA(PWMDLYX, 0x04);		/* 0x0012 X Phase Delay Setting */
	RegWriteA(PWMDLYY, 0x04);		/* 0x0013 Y Phase Delay Setting */

	RegWriteA(PWMPERIODX, 0x00);	/* 0x0018 PWM Carrier Freq */
	RegWriteA(PWMPERIODX2, 0x00);	/* 0x0019 PWM Carrier Freq */
	RegWriteA(PWMPERIODY, 0x00);	/* 0x001A PWM Carrier Freq */
	RegWriteA(PWMPERIODY2, 0x00);	/* 0x001B PWM Carrier Freq */


	/* Linear PWM circuit setting */
	RegWriteA(CVA, 0xC0);			/* 0x0020	Linear PWM mode enable */
	RegWriteA(CVFC2, 0x80);			/* 0x0022 */

	RegReadA(STBB0, &UcStbb0);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
	UcStbb0 &= 0x80;
	RegWriteA(STBB0, UcStbb0);		/* 0x0250 OIS standby */

}

/*********************************************************************
 Function Name	: lc898122a_init_gyro_setting
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Gyro Filter Setting Initialize Function
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
#ifdef GAIN_CONT
#define	TRI_LEVEL		0x3B23D70A		/* 0.0025 */ /*140314 */
#define	TIMELOW			0x40			/*140314 */
#define	TIMEHGH			0x01
#define	TIMEBSE			0x5D			/* 3.96ms */
#define	MONADR			GXXFZ
#define	GANADR			gxadj
#define	XMINGAIN		0x00000000
#define	XMAXGAIN		0x3F800000
#define	YMINGAIN		0x00000000
#define	YMAXGAIN		0x3F800000
#define	XSTEPUP			0x3F800000		/* 1.0 */
#define	XSTEPDN			0xBF80000D		/* -1.0 */
#define	YSTEPUP			0x3F800000		/* 1.0 */
#define	YSTEPDN			0xBF800000		/* -1.0 */
#endif

void lc898122a_init_gyro_setting(void)
{
#ifdef NEW_PTST
	UnFltVal UnGyrLmt;
#endif
	/*Gyro Filter Setting*/
	RegWriteA(WG_EQSW, 0x03);	/* 0x0110 [ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ] */

	/*Gyro Filter Down Sampling*/

	RegWriteA(WG_SHTON, 0x10);	/* 0x0107 [ - | - | - | CmSht2PanOff ][ - | - | CmShtOpe(1:0) ] */
	/* CmShtOpe[1:0] 00: シャッ??OFF, 01: シャッ??ON, 1x:外部制御 */
	RegWriteA(WG_SHTMOD, 0x06);	/* 0x0116 Shutter Hold mode */

	/* Limiter */
	RamWrite32A(gxlmt1H, GYRLMT1H);			/* 0x1028 */
	RamWrite32A(gylmt1H, GYRLMT1H);			/* 0x1128 */

	RamWrite32A(gxlmt3HS0, GYRLMT3_S1);		/* 0x1029 */
	RamWrite32A(gylmt3HS0, GYRLMT3_S1);		/* 0x1129 */

	RamWrite32A(gxlmt3HS1, GYRLMT3_S2);		/* 0x102A */
	RamWrite32A(gylmt3HS1, GYRLMT3_S2);		/* 0x112A */

	RamWrite32A(gylmt4HS0, GYRLMT4_S1);		/* 0x112B Y軸Limiter4 High?値0 */
	RamWrite32A(gxlmt4HS0, GYRLMT4_S1);		/* 0x102B X軸Limiter4 High?値0 */
	RamWrite32A(gxlmt4HS1, GYRLMT4_S2);		/* 0x102C X軸Limiter4 High?値1 */
	RamWrite32A(gylmt4HS1, GYRLMT4_S2);		/* 0x112C Y軸Limiter4 High?値1 */

	/* Pan/Tilt parameter */
	RegWriteA(WG_PANADDA, 0x12);			/* 0x0130 GXH1Z2/GYH1Z2 Select */
	RegWriteA(WG_PANADDB, 0x09);			/* 0x0131 GXIZ/GYIZ Select */

	/*Threshold */
	RamWrite32A(SttxHis, 0x00000000);		/* 0x1226 */
	RamWrite32A(SttxaL,	0x00000000);		/* 0x109D */
	RamWrite32A(SttxbL,	0x00000000);		/* 0x109E */
	RamWrite32A(Sttx12aM, GYRA12_MID);		/* 0x104F */
	RamWrite32A(Sttx12aH, GYRA12_HGH);		/* 0x105F */
	RamWrite32A(Sttx12bM, GYRB12_MID);		/* 0x106F */
	RamWrite32A(Sttx12bH, GYRB12_HGH);		/* 0x107F */
	RamWrite32A(Sttx34aM, GYRA34_MID);		/* 0x108F */
	RamWrite32A(Sttx34aH, GYRA34_HGH);		/* 0x109F */
	RamWrite32A(Sttx34bM, GYRB34_MID);		/* 0x10AF */
	RamWrite32A(Sttx34bH, GYRB34_HGH);		/* 0x10BF */
	RamWrite32A(SttyaL,	0x00000000);		/* 0x119D */
	RamWrite32A(SttybL,	0x00000000);		/* 0x119E */
	RamWrite32A(Stty12aM, GYRA12_MID);		/* 0x114F */
	RamWrite32A(Stty12aH, GYRA12_HGH);		/* 0x115F */
	RamWrite32A(Stty12bM, GYRB12_MID);		/* 0x116F */
	RamWrite32A(Stty12bH, GYRB12_HGH);		/* 0x117F */
	RamWrite32A(Stty34aM, GYRA34_MID);		/* 0x118F */
	RamWrite32A(Stty34aH, GYRA34_HGH);		/* 0x119F */
	RamWrite32A(Stty34bM, GYRB34_MID);		/* 0x11AF */
	RamWrite32A(Stty34bH, GYRB34_HGH);		/* 0x11BF */

	/* Pan level */
	RegWriteA(WG_PANLEVABS, 0x00);			/* 0x0133 */

	/* Average parameter are set lc898122a_init_adjust_value */

	/* Phase Transition Setting */
	/* State 2 -> 1 */
	RegWriteA(WG_PANSTT21JUG0, 0x00);		/* 0x0140 */
	RegWriteA(WG_PANSTT21JUG1, 0x00);		/* 0x0141 */
	/* State 3 -> 1 */
	RegWriteA(WG_PANSTT31JUG0, 0x01);		/* 0x0142 */
	RegWriteA(WG_PANSTT31JUG1, 0x00);		/* 0x0143 */
	/* State 4 -> 1 */
	RegWriteA(WG_PANSTT41JUG0, 0x00);		/* 0x0144 */
	RegWriteA(WG_PANSTT41JUG1, 0x00);		/* 0x0145 */
	/* State 1 -> 2 */
	RegWriteA(WG_PANSTT12JUG0, 0x00);		/* 0x0146 */
	RegWriteA(WG_PANSTT12JUG1, 0x00);		/* 0x0147 */
	/* State 1 -> 3 */
	RegWriteA(WG_PANSTT13JUG0, 0x00);		/* 0x0148 */
	RegWriteA(WG_PANSTT13JUG1, 0x07);		/* 0x0149 */
	/* State 2 -> 3 */
	RegWriteA(WG_PANSTT23JUG0, 0x00);		/* 0x014A */
	RegWriteA(WG_PANSTT23JUG1, 0x00);		/* 0x014B */
	/* State 4 -> 3 */
	RegWriteA(WG_PANSTT43JUG0, 0x00);		/* 0x014C */
	RegWriteA(WG_PANSTT43JUG1, 0x00);		/* 0x014D */
	/* State 3 -> 4 */
	RegWriteA(WG_PANSTT34JUG0, 0x00);		/* 0x014E */
	RegWriteA(WG_PANSTT34JUG1, 0x00);		/* 0x014F */
	/* State 2 -> 4 */
	RegWriteA(WG_PANSTT24JUG0, 0x00);		/* 0x0150 */
	RegWriteA(WG_PANSTT24JUG1, 0x00);		/* 0x0151 */
	/* State 4 -> 2 */
	RegWriteA(WG_PANSTT42JUG0, 0x00);		/* 0x0152 */
	RegWriteA(WG_PANSTT42JUG1, 0x00);		/* 0x0153 */

	/* State Timer */
	RegWriteA(WG_PANSTT1LEVTMR, 0x00);		/* 0x015B */
	RegWriteA(WG_PANSTT2LEVTMR, 0x00);		/* 0x015C */
	RegWriteA(WG_PANSTT3LEVTMR, 0x00);		/* 0x015D */
	RegWriteA(WG_PANSTT4LEVTMR, 0x00);		/* 0x015E */

	/* Control filter */
	RegWriteA(WG_PANTRSON0, 0x91);			/* 0x0132 USE I12/iSTP/Gain-Filter, USE Linear */

	RegWriteA(WG_PANSTTSETGYRO, 0x00);		/* 0x0154 */
	RegWriteA(WG_PANSTTSETGAIN, 0x10);		/* 0x0155 */
	RegWriteA(WG_PANSTTSETISTP, 0x10);		/* 0x0156 */
	RegWriteA(WG_PANSTTSETIFTR, 0x10);		/* 0x0157 */
	RegWriteA(WG_PANSTTSETLFTR, 0x00);		/* 0x0158 */
	/* State Setting */
	lc898122a_init_pan_mode(OFF);						/* Pan/Tilt setting (Still) */

	/* Hold */
	RegWriteA(WG_PANSTTSETILHLD, 0x00);		/* 0x015F */

	/* State2,4 Step Time Setting */
	RegWriteA(WG_PANSTT2TMR0, 0xEA);		/* 0x013C 9.983787013ms */
	RegWriteA(WG_PANSTT2TMR1, 0x00);		/* 0x013D */
	RegWriteA(WG_PANSTT4TMR0, 0x92);		/* 0x013E 49.91893506ms */
	RegWriteA(WG_PANSTT4TMR1, 0x04);		/* 0x013F */

	RegWriteA(WG_PANSTTXXXTH, 0x0F);		/* 0x015A */

#ifdef NEW_PTST
	UnGyrLmt.SfFltVal = 0.003F;					/* St4 Limiter?1/S↓ */
	RamWrite32A(npxlev8, UnGyrLmt.UlLngVal);	/* 0x109B */
	RamWrite32A(npylev8, UnGyrLmt.UlLngVal);	/* 0x119B */


	/* Fast */
	UnGyrLmt.SfFltVal = 0.02F;					/* St1 */
	RamWrite32A(npxlev1, UnGyrLmt.UlLngVal);	/* 0x100F */
	RamWrite32A(npylev1, UnGyrLmt.UlLngVal);	/* 0x110F */
	RamWrite32A(npxlev1_i, UnGyrLmt.UlLngVal);	/* 0x10CF */
	RamWrite32A(npylev1_i, UnGyrLmt.UlLngVal);	/* 0x11CF */
	UnGyrLmt.SfFltVal	= 0.0005F;				/* St2 Limiter */
	RamWrite32A(npxlev2, UnGyrLmt.UlLngVal);	/* 0x101F */
	RamWrite32A(npylev2, UnGyrLmt.UlLngVal);	/* 0x111F */
	RamWrite32A(npxlev2_i, UnGyrLmt.UlLngVal);	/* 0x10DF */
	RamWrite32A(npylev2_i, UnGyrLmt.UlLngVal);	/* 0x11DF */

	/*Slow */
	UnGyrLmt.SfFltVal = 0.0005F;				/* St3元 */
	RamWrite32A(npxlev3, UnGyrLmt.UlLngVal);	/* 0x102F */
	RamWrite32A(npylev3, UnGyrLmt.UlLngVal);	/* 0x112F */
	UnGyrLmt.SfFltVal	= 0.0025F;				/* St3元 */
	RamWrite32A(npxlev3_i, UnGyrLmt.UlLngVal);	/* 0x10EF */
	RamWrite32A(npylev3_i, UnGyrLmt.UlLngVal);	/* 0x11EF */
	UnGyrLmt.SfFltVal	= 0.0005f;				/* St4 Limiter */
	RamWrite32A(npxlev4, UnGyrLmt.UlLngVal);	/* 0x103F */
	RamWrite32A(npylev4, UnGyrLmt.UlLngVal);	/* 0x113F */
	UnGyrLmt.SfFltVal	= 0.0005F;				/* St4 Limiter */
	RamWrite32A(npxlev4_i, UnGyrLmt.UlLngVal);	/* 0x10FF */
	RamWrite32A(npylev4_i, UnGyrLmt.UlLngVal);	/* 0x11FF */

	/* Pan/Tilt NEW?PanTilt?Setteing */
	RegWriteA(WG_NPANST12BTMR, 0x0F);			/* 0x0167 */
	RegWriteA(WG_NPANST3RTMR, 0x0A);			/* 0x0166 */
	RegWriteA(WG_NPANST12TMRX, 0x00);			/* 0x0168 682u:682μ */
	RegWriteA(WG_NPANST12TMRY, 0x00);			/* 0x0169 682u:682μ */
	RegWriteA(WG_NPANST3TMRX, 0x08);			/* 0x016A 98ms */
	RegWriteA(WG_NPANST3TMRY, 0x08);			/* 0x016B 98ms */
	RegWriteA(WG_NPANST4TMRX, 0x02);			/* 0x016C 21ms */
	RegWriteA(WG_NPANST4TMRY, 0x02);			/* 0x016D 21ms */

	RegWriteA(WG_NPANSTFRC, 0x00);				/* 0x010B State posituon ON */
	RegWriteA(WG_NPANFUN, 0x01);				/* 0x016E Gain Cut,decrease */
	RegWriteA(WG_NPANINITMR, 0x02);				/* 0x016F 21ms '?価時に検討現在は同じ値 */
	RegWriteA(WG_NPANSTOFF, 0xA0);				/* 0x010E State5,7 OFF */

	RegWriteA(WG_NPANTST0, 0x08);				/* 0x0164 Option Setting */

	UnGyrLmt.UlLngVal = 0x3951B717;				/* St1 decrease */
	RamWrite32A(gxistp_1u, UnGyrLmt.UlLngVal);	/* 0x1085 */
	RamWrite32A(gyistp_1u, UnGyrLmt.UlLngVal);	/* 0x1185 */
	UnGyrLmt.UlLngVal = 0x3F7FFE80;
	RamWrite32A(gxistp_2d, UnGyrLmt.UlLngVal);	/* 0x1087 ST1,ST3?Ccof Val */
	RamWrite32A(gyistp_2d, UnGyrLmt.UlLngVal);	/* 0x1187 ST1,ST3?Ccof Val */
	UnGyrLmt.UlLngVal = 0x3F7FFE80;
	RamWrite32A(gyistp_2u, UnGyrLmt.UlLngVal);	/* 0x1188 ST2,ST4?Ccof Val */
	RamWrite32A(gxistp_2u, UnGyrLmt.UlLngVal);	/* 0x1088 ST2,ST4?Ccof Val */

#endif

#ifdef GAIN_CONT
	RamWrite32A(gxlevlow, TRI_LEVEL);	/* 0x10AE Low Th */
	RamWrite32A(gylevlow, TRI_LEVEL);	/* 0x11AE Low Th */
	RamWrite32A(gxadjmin, XMINGAIN);	/* 0x1094 Low gain */
	RamWrite32A(gxadjmax, XMAXGAIN);	/* 0x1095 Hgh gain */
	RamWrite32A(gxadjdn, XSTEPDN);		/* 0x1096 -step */
	RamWrite32A(gxadjup, XSTEPUP);		/* 0x1097 +step */
	RamWrite32A(gyadjmin, YMINGAIN);	/* 0x1194 Low gain */
	RamWrite32A(gyadjmax, YMAXGAIN);	/* 0x1195 Hgh gain */
	RamWrite32A(gyadjdn, YSTEPDN);		/* 0x1196 -step */
	RamWrite32A(gyadjup, YSTEPUP);		/* 0x1197 +step */

	RegWriteA(WG_LEVADD, (uint8_t)MONADR);/* 0x0120 Input signal */
	RegWriteA(WG_LEVTMR, TIMEBSE);				/* 0x0123 Base Time */
	RegWriteA(WG_LEVTMRLOW, TIMELOW);			/* 0x0121 X Low Time */
	RegWriteA(WG_LEVTMRHGH, TIMEHGH);			/* 0x0122 X Hgh Time */
	RegWriteA(WG_ADJGANADD, (uint8_t)GANADR);	/* 0x0128 control address */
	RegWriteA(WG_ADJGANGO, 0x00);				/* 0x0108 manual off */

	/* exe function */
	lc898122a_auto_gain_con(OFF);						/* Auto Gain Control Mode OFF */
#endif

}

/*********************************************************************
 Function Name	: lc898122a_init_pan_average
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Pan/Tilt Average parameter setting function
 History			: First edition							2013.09.26 Y.Shigeoka
*********************************************************************/
void lc898122a_init_pan_average(void)
{
	RegWriteA(WG_PANSTT1DWNSMP0, 0x00);		/* 0x0134 */
	RegWriteA(WG_PANSTT1DWNSMP1, 0x00);		/* 0x0135 */
	RegWriteA(WG_PANSTT2DWNSMP0, 0x00);		/* 0x0136 */
	RegWriteA(WG_PANSTT2DWNSMP1, 0x00);		/* 0x0137 */
	RegWriteA(WG_PANSTT3DWNSMP0, 0x00);		/* 0x0138 */
	RegWriteA(WG_PANSTT3DWNSMP1, 0x00);		/* 0x0139 */
	RegWriteA(WG_PANSTT4DWNSMP0, 0x00);		/* 0x013A */
	RegWriteA(WG_PANSTT4DWNSMP1, 0x00);		/* 0x013B */

	RamWrite32A(st1mean, 0x3f800000);		/* 0x1235 */
	RamWrite32A(st2mean, 0x3f800000);		/* 0x1236 */
	RamWrite32A(st3mean, 0x3f800000);		/* 0x1237 */
	RamWrite32A(st4mean, 0x3f800000);		/* 0x1238 */

}

/*********************************************************************
 Function Name	: lc898122a_init_pan_mode
 Return Value		: NON
 Argument Value	: OFF:Still  ON:Movie
 Explanation		: Pan/Tilt parameter setting by mode function
 History			: First edition							2013.09.26 Y.Shigeoka
*********************************************************************/
void lc898122a_init_pan_mode(uint8_t UcPtMod)
{
	switch (UcPtMod) {
	case OFF:
		/* State 3 -> 1 */
		RegWriteA(WG_PANSTT31JUG0, 0x01);	/* 0x0142 */
		RegWriteA(WG_PANSTT31JUG1, 0x00);	/* 0x0143 */
		/* State 4 -> 1 */
		RegWriteA(WG_PANSTT41JUG0, 0x00);	/* 0x0144 */
		RegWriteA(WG_PANSTT41JUG1, 0x00);	/* 0x0145 */
		/* State 1 -> 3 */
		RegWriteA(WG_PANSTT13JUG0, 0x00);	/* 0x0148 */
		RegWriteA(WG_PANSTT13JUG1, 0x07);	/* 0x0149 */
		/* State 4 -> 3 */
		RegWriteA(WG_PANSTT43JUG0, 0x00);	/* 0x014C */
		RegWriteA(WG_PANSTT43JUG1, 0x00);	/* 0x014D */
		/* State 3 -> 4 */
		RegWriteA(WG_PANSTT34JUG0, 0x00);	/* 0x014E */
		RegWriteA(WG_PANSTT34JUG1, 0x00);	/* 0x014F */

		RegWriteA(WG_PANSTTXXXTH, 0x0F);	/* 0x015A */
		RamWrite32A(Sttx34aM, GYRA34_MID);	/* 0x108F */
		RamWrite32A(Stty34aM, GYRA34_MID);	/* 0x118F */

		/* I Filter X */							/* 2s */
		RamWrite32A(gxia_1, 0x3860DE00);	/* 0x1043 0.4Hz */
		RamWrite32A(gxib_1, 0xB261CF49);	/* 0x1044 Down */
		RamWrite32A(gxic_1, 0x3261CF49);	/* 0x1045 Up */

		RamWrite32A(gxia_a, 0x3860DE00);	/* 0x1046 0.4Hz */
		RamWrite32A(gxib_a, 0xB261CF49);	/* 0x1047 Down */
		RamWrite32A(gxic_a, 0x3261CF49);	/* 0x1048 Up */

		RamWrite32A(gxia_b, 0x3A2F91C0);	/* 0x1049 5Hz */
		RamWrite32A(gxib_b, 0xB261CF49);	/* 0x104A Down */
		RamWrite32A(gxic_b, 0x3F800000);	/* 0x104B Up */

		RamWrite32A(gxia_c, 0x3860DE00);	/* 0x104C 0.4Hz */
		RamWrite32A(gxib_c, 0xB261CF49);	/* 0x104D Down */
		RamWrite32A(gxic_c, 0x3261CF49);	/* 0x104E Up */

		/* I Filter Y */
		RamWrite32A(gyia_1, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_1, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_1, 0x3261CF49);	/* Up */

		RamWrite32A(gyia_a, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_a, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_a, 0x3261CF49);	/* Up */

		RamWrite32A(gyia_b, 0x3A2F91C0);	/* 5Hz */
		RamWrite32A(gyib_b, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_b, 0x3F800000);	/* Up */

		RamWrite32A(gyia_c, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_c, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_c, 0x3261CF49);	/* Up */
		break;
	case ON:
		/* State 3 -> 1 */
		RegWriteA(WG_PANSTT31JUG0, 0x00);	/* 0x0142 */
		RegWriteA(WG_PANSTT31JUG1, 0x00);	/* 0x0143 */
		/* State 4 -> 1 */
		RegWriteA(WG_PANSTT41JUG0, 0x07);	/* 0x0144 */
		RegWriteA(WG_PANSTT41JUG1, 0x00);	/* 0x0145 */
		/* State 1 -> 3 */
		RegWriteA(WG_PANSTT13JUG0, 0x00);	/* 0x0148 */
		RegWriteA(WG_PANSTT13JUG1, 0x07);	/* 0x0149 */
		/* State 4 -> 3 */
		RegWriteA(WG_PANSTT43JUG0, 0x00);	/* 0x014C */
		RegWriteA(WG_PANSTT43JUG1, 0x07);	/* 0x014D */
		/* State 3 -> 4 */
		RegWriteA(WG_PANSTT34JUG0, 0x01);	/* 0x014E */
		RegWriteA(WG_PANSTT34JUG1, 0x00);	/* 0x014F */

		RegWriteA(WG_PANSTTXXXTH, 0xF0);	/* 0x015A */
		RamWrite32A(Sttx34aM, GYRA34_MID_M);	/* 0x108F */
		RamWrite32A(Stty34aM, GYRA34_MID_M);	/* 0x118F */

		/* I Filter X */					/* 2s */
		RamWrite32A(gxia_1, 0x3860DE00);	/* 0x1043 0.4Hz */
		RamWrite32A(gxib_1, 0xB261CF49);	/* 0x1044 Down */
		RamWrite32A(gxic_1, 0x3261CF49);	/* 0x1045 Up */

		RamWrite32A(gxia_a, 0x3860DE00);	/* 0x1046 0.4Hz */
		RamWrite32A(gxib_a, 0xB261CF49);	/* 0x1047 Down */
		RamWrite32A(gxic_a, 0x3261CF49);	/* 0x1048 Up */

		RamWrite32A(gxia_b, 0x3860DE00);	/* 0x1049 0.4Hz */
		RamWrite32A(gxib_b, 0xB261CF49);	/* 0x104A Down */
		RamWrite32A(gxic_b, 0x3261CF49);	/* 0x104B Up */

		RamWrite32A(gxia_c, 0x3A2F91C0);	/* 0x104C 5Hz */
		RamWrite32A(gxib_c, 0xB261CF49);	/* 0x104D Down */
		RamWrite32A(gxic_c, 0x3F800000);	/* 0x104E Up */

		/* I Filter Y */
		RamWrite32A(gyia_1, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_1, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_1, 0x3261CF49);	/* Up */

		RamWrite32A(gyia_a, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_a, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_a, 0x3261CF49);	/* Up */

		RamWrite32A(gyia_b, 0x3860DE00);	/* 0.4Hz */
		RamWrite32A(gyib_b, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_b, 0x3261CF49);	/* Up */

		RamWrite32A(gyia_c, 0x3A2F91C0);	/* 5Hz */
		RamWrite32A(gyib_c, 0xB261CF49);	/* Down */
		RamWrite32A(gyic_c, 0x3F800000);	/* Up */
		break;
	}
}

/*********************************************************************
 Function Name	: lc898122a_init_gyro_filter
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Gyro Filter Initial Parameter Setting
 History			: First edition							2009.07.30 Y.Tashita
*********************************************************************/
int32_t	lc898122a_init_gyro_filter(void)
{
	uint16_t UsAryId;

	/* Filter Register Parameter Setting */
	UsAryId	= 0;
	while (CsFilReg[UcVerLow][UsAryId].UsRegAdd != 0xFFFF) {
		RegWriteA(CsFilReg[UcVerLow][UsAryId].UsRegAdd, CsFilReg[UcVerLow][UsAryId].UcRegDat);
		UsAryId++;
		if (UsAryId > FILREGTAB)
			return OIS_FW_POLLING_FAIL;
	}

	RegWriteA(WC_RAMACCXY, 0x01);	/* 0x018D Simultaneously Setting On */

	/* Filter Ram Parameter Setting */
	UsAryId	= 0;
	while (CsFilRam[UcVerLow][UsAryId].UsRamAdd != 0xFFFF) {
		RamWrite32A(CsFilRam[UcVerLow][UsAryId].UsRamAdd, CsFilRam[UcVerLow][UsAryId].UlRamDat);
		UsAryId++;
		if (UsAryId > FILRAMTAB)
			return OIS_FW_POLLING_FAIL;
	}

	RegWriteA(WC_RAMACCXY, 0x00);	/* 0x018D	Simultaneously Setting Off */

	return OIS_FW_POLLING_PASS;
}

/*********************************************************************
 Function Name	: lc898122a_init_adjust_value
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Adjust Value Setting
 History			: First edition							2009.07.30 Y.Tashita
*********************************************************************/
void lc898122a_init_adjust_value(void)
{
	RegWriteA(WC_RAMACCXY, 0x00);		/* 0x018D	Filter copy off */

	lc898122a_init_pan_average();							/* Average setting */

	/* OIS */
	RegWriteA(CMSDAC0, BIAS_CUR_OIS);	/* 0x0251 Hall DAC Current */
	RegWriteA(OPGSEL0, AMP_GAIN_X);		/* 0x0253 Hall amp Gain X */
	RegWriteA(OPGSEL1, AMP_GAIN_Y);		/* 0x0254 Hall amp Gain Y */

	/* AF */
	RegWriteA(CMSDAC1, BIAS_CUR_AF);	/* 0x0252 Hall Dac電流 */
	RegWriteA(OPGSEL2, AMP_GAIN_AF);	/* 0x0255 Hall amp Gain AF */

	/* OSC Clock value */
	if (((uint8_t)StCalDat.UcOscVal == 0x00) || ((uint8_t)StCalDat.UcOscVal == 0xFF)) {
		/* 0x0257	OSC ini */
		RegWriteA(OSCSET, OSC_INI);
	} else {
		/* 0x0257 */
		RegWriteA(OSCSET, StCalDat.UcOscVal);
	}

	/* adjusted value */
	/* Gyro X axis Offset */
	if ((StCalDat.StGvcOff.UsGxoVal == 0x0000) || (StCalDat.StGvcOff.UsGxoVal == 0xFFFF)) {
		RegWriteA(IZAH,	DGYRO_OFST_XH);		/* 0x02A0 Set Offset High byte */
		RegWriteA(IZAL,	DGYRO_OFST_XL);		/* 0x02A1 Set Offset Low byte */
	} else {
		RegWriteA(IZAH, (uint8_t)(StCalDat.StGvcOff.UsGxoVal >> 8));	/* 0x02A0 Set Offset High byte */
		RegWriteA(IZAL, (uint8_t)(StCalDat.StGvcOff.UsGxoVal));		/* 0x02A1 Set Offset Low byte */
	}
	/* Gyro Y axis Offset */
	if ((StCalDat.StGvcOff.UsGyoVal == 0x0000) || (StCalDat.StGvcOff.UsGyoVal == 0xFFFF)) {
		RegWriteA(IZBH,	DGYRO_OFST_YH);		/* 0x02A2 Set Offset High byte */
		RegWriteA(IZBL,	DGYRO_OFST_YL);		/* 0x02A3 Set Offset Low byte */
	} else {
		RegWriteA(IZBH, (uint8_t)(StCalDat.StGvcOff.UsGyoVal >> 8));	/* 0x02A2 Set Offset High byte */
		RegWriteA(IZBL, (uint8_t)(StCalDat.StGvcOff.UsGyoVal));		/* 0x02A3 Set Offset Low byte */
	}

	/* Ram Access */
	lc898122a_ram_mode(ON);	/* 16bit Fix mode */

	/* OIS adjusted parameter */
	/* Hall X axis Bias,Offset,Lens center */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_HXADJ - EXE_END))) {
		RamWriteA(DAXHLO, DAHLXO_INI);	/* 0x1479 */
		RamWriteA(DAXHLB, DAHLXB_INI);	/* 0x147A */
	} else {
		RamWriteA(DAXHLO, StCalDat.StHalAdj.UsHlxOff);	/* 0x1479 */
		RamWriteA(DAXHLB, StCalDat.StHalAdj.UsHlxGan);	/* 0x147A */
	}

	/* Hall Y axis Bias,Offset,Lens center */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_HYADJ - EXE_END))) {
		RamWriteA(DAYHLO, DAHLYO_INI);	/* 0x14F9 */
		RamWriteA(DAYHLB, DAHLYB_INI);	/* 0x14FA */
	} else {
		RamWriteA(DAYHLO, StCalDat.StHalAdj.UsHlyOff);	/* 0x14F9 */
		RamWriteA(DAYHLB, StCalDat.StHalAdj.UsHlyGan);	/* 0x14FA */
	}

	/* Hall X axis Loop Gain */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_LXADJ - EXE_END))) {
		/* 0x10D3 */
		RamWriteA(sxg, SXGAIN_INI);
	} else {
		RamWriteA(sxg, StCalDat.StLopGan.UsLxgVal);	/* 0x10D3 */
	}

	/* Hall Y axis Loop Gain */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_LYADJ - EXE_END))) {
		/* 0x11D3 */
		RamWriteA(syg, SYGAIN_INI);
	} else {
		RamWriteA(syg, StCalDat.StLopGan.UsLygVal);	/* 0x11D3 */
	}

	/* Ram Access */
	lc898122a_ram_mode(OFF);	/* 32bit Float mode */

	/* Gyro X axis Gain */
	if ((StCalDat.UlGxgVal == 0x00000000) || (StCalDat.UlGxgVal == 0xFFFFFFFF) || (StCalDat.UsAdjLensF & (EXE_GXADJ - EXE_END))) {
		/* 0x1020 Gyro X axis Gain adjusted value */
		RamWrite32A(gxzoom, GXGAIN_INI);
	} else {
		RamWrite32A(gxzoom, StCalDat.UlGxgVal);		/* 0x1020 Gyro X axis Gain adjusted value */
	}

	/* Gyro Y axis Gain */
	if ((StCalDat.UlGygVal == 0x00000000) || (StCalDat.UlGygVal == 0xFFFFFFFF) || (StCalDat.UsAdjLensF & (EXE_GYADJ - EXE_END))) {
		/* 0x1120 Gyro Y axis Gain adjusted value */
		RamWrite32A(gyzoom, GYGAIN_INI);
	} else {
		RamWrite32A(gyzoom, StCalDat.UlGygVal);	/* 0x1120 Gyro Y axis Gain adjusted value */
	}

	lc898122a_ram_mode(ON);	/* 16bit Fix mode */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjLensF == 0xFFFF) || (StCalDat.UsAdjLensF & (EXE_CXADJ - EXE_END))) {
		/* 0x1450 */
		RamWriteA(OFF0Z, HXOFF0Z_INI);
	} else {
		RamWriteA(OFF0Z, StCalDat.StLenCen.UsLsxVal);	/* 0x1450 */
	}

	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjLensF == 0xFFFF) || (StCalDat.UsAdjLensF & (EXE_CYADJ - EXE_END))) {
		/* 0x14D0 */
		RamWriteA(OFF1Z, HYOFF1Z_INI);
	} else {
		RamWriteA(OFF1Z, StCalDat.StLenCen.UsLsyVal);	/* 0x14D0 */
	}
	lc898122a_ram_mode(OFF);		/* 32bit Float mode */

	RamWrite32A(sxq, 0xBF800000);		/* 0x10E5 X axis output direction initial value */
	RamWrite32A(syq, 0xBF800000);		/* 0x11E5 Y axis output direction initial value */

	RegWriteA(PWMA, 0xC0);				/* 0x0010 PWM enable */

	RegWriteA(STBB0, 0xDF);				/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
	RegWriteA(WC_EQSW, 0x02);			/* 0x01E0 */
	RegWriteA(WC_MESLOOP1, 0x02);		/* 0x0193 */
	RegWriteA(WC_MESLOOP0, 0x00);		/* 0x0192 */
	RegWriteA(WC_AMJLOOP1, 0x02);		/* 0x01A3 */
	RegWriteA(WC_AMJLOOP0, 0x00);		/* 0x01A2 */

	/* I Filter X */								/*	2s */
	RamWrite32A(gxia_1, 0x3860DE00);	/* 0x1043 0.4Hz */
	RamWrite32A(gxib_1, 0xB261CF49);	/* 0x1044 Down */
	RamWrite32A(gxic_1, 0x3261CF49);	/* 0x1045 Up */

	RamWrite32A(gxia_a, 0x3860DE00);	/* 0x1046 0.4Hz */
	RamWrite32A(gxib_a, 0xB261CF49);	/* 0x1047 Down */
	RamWrite32A(gxic_a, 0x3261CF49);	/* 0x1048 Up */

	RamWrite32A(gxia_b, 0x3A2F91C0);	/* 0x1049 5Hz */
	RamWrite32A(gxib_b, 0xB261CF49);	/* 0x104A Down */
	RamWrite32A(gxic_b, 0x3F800000);	/* 0x104B Up */

	RamWrite32A(gxia_c, 0x3860DE00);	/* 0x104C 0.4Hz */
	RamWrite32A(gxib_c, 0xB261CF49);	/* 0x104D Down */
	RamWrite32A(gxic_c, 0x3261CF49);	/* 0x104E Up */

	/* I Filter Y */
	RamWrite32A(gyia_1, 0x3860DE00);	/* 0x1143 0.4Hz */
	RamWrite32A(gyib_1, 0xB261CF49);	/* 0x1144 Down */
	RamWrite32A(gyic_1, 0x3261CF49);	/* 0x1145 Up */

	RamWrite32A(gyia_a, 0x3860DE00);	/* 0x1146 0.4Hz */
	RamWrite32A(gyib_a, 0xB261CF49);	/* 0x1147 Down */
	RamWrite32A(gyic_a, 0x3261CF49);	/* 0x1148 Up */

	RamWrite32A(gyia_b, 0x3A2F91C0);	/* 0x1149 5Hz */
	RamWrite32A(gyib_b, 0xB261CF49);	/* 0x114A Down */
	RamWrite32A(gyic_b, 0x3F800000);	/* 0x114B Up */

	RamWrite32A(gyia_c, 0x3860DE00);	/* 0x114C 0.4Hz */
	RamWrite32A(gyib_c, 0xB261CF49);	/* 0x114D Down */
	RamWrite32A(gyic_c, 0x3261CF49);	/* 0x114E Up */

	/* gxgain */
	RamWrite32A(gxgain_1, 0x3F800000);		/* 0x1073 1 */
	RamWrite32A(gxgain_1d, 0xB7B2F402);		/* 0x1074 Down */
	RamWrite32A(gxgain_1u, 0x37B2F402);		/* 0x1075 Up */	/*2.0s */	/*140314 */

	RamWrite32A(gxgain_a, 0x3F800000);		/* 0x1076 1 */
	RamWrite32A(gxgain_2d, 0xB8DFB102);		/* 0x1077 Down */
	RamWrite32A(gxgain_2u, 0x38DFB102);		/* 0x1078 Up */

	RamWrite32A(gxgain_b, 0x00000000);		/* 0x1079 Cut Off */
	RamWrite32A(gxgain_3d, 0xBF800000);		/* 0x107A Down */	/*0.0s */
	RamWrite32A(gxgain_3u, 0x38DFB102);		/* 0x107B Up */

	RamWrite32A(gxgain_c, 0x3F800000);		/* 0x107C 1 */
	RamWrite32A(gxgain_4d, 0xB8DFB102);		/* 0x107D Down */	/*0.4s */
	RamWrite32A(gxgain_4u, 0x38DFB102);		/* 0x107E Up */	/*0.4s */

	/* gygain */
	RamWrite32A(gygain_1, 0x3F800000);		/* 1 */
	RamWrite32A(gygain_1d, 0xB7B2F402);		/* Down */
	RamWrite32A(gygain_1u, 0x37B2F402);		/* Up */	/*2.0s */	/*140314 */

	RamWrite32A(gygain_a, 0x3F800000);		/* 1 */
	RamWrite32A(gygain_2d, 0xB8DFB102);		/* Down */
	RamWrite32A(gygain_2u, 0x38DFB102);		/* Up */

	RamWrite32A(gygain_b, 0x00000000);		/* Cut Off */
	RamWrite32A(gygain_3d, 0xBF800000);		/* Down */	/*0.0s */
	RamWrite32A(gygain_3u, 0x38DFB102);		/* Up */

	RamWrite32A(gygain_c, 0x3F800000);		/* 1 */
	RamWrite32A(gygain_4d, 0xB8DFB102);		/* Down */	/*0.4s */
	RamWrite32A(gygain_4u, 0x38DFB102);		/* Up */	/*0.4s */

	/* gxistp */								/* */
	RamWrite32A(gxistp_1, 0x00000000);		/* 0x1083 Cut Off */
	RamWrite32A(gxistp_1d, 0xBF800000);		/* 0x1084 Down */
	RamWrite32A(gxistp_1u, 0x3F800000);		/* 0x1085 Up */

	RamWrite32A(gxistp_a, 0x00000000);		/* 0x1086 Cut Off */
	RamWrite32A(gxistp_2d, 0xBF800000);		/* 0x1087 Down */
	RamWrite32A(gxistp_2u, 0x3F800000);		/* 0x1088 Up */

	RamWrite32A(gxistp_b, 0x38D1B700);		/* 0x1089 -80dB */
	RamWrite32A(gxistp_3d, 0xBF800000);		/* 0x108A Down */
	RamWrite32A(gxistp_3u, 0x3F800000);		/* 0x108B Up */

	RamWrite32A(gxistp_c, 0x00000000);		/* 0x108C Cut Off */
	RamWrite32A(gxistp_4d, 0xBF800000);		/* 0x108D Down */
	RamWrite32A(gxistp_4u, 0x3F800000);		/* 0x108E Up */

	/* gyistp */
	RamWrite32A(gyistp_1, 0x00000000);		/* Cut Off */
	RamWrite32A(gyistp_1d, 0xBF800000);		/* Down */
	RamWrite32A(gyistp_1u, 0x3F800000);		/* Up */

	RamWrite32A(gyistp_a, 0x00000000);		/* Cut Off */
	RamWrite32A(gyistp_2d, 0xBF800000);		/* Down */
	RamWrite32A(gyistp_2u, 0x3F800000);		/* Up */

	RamWrite32A(gyistp_b, 0x38D1B700);		/* -80dB */
	RamWrite32A(gyistp_3d, 0xBF800000);		/* Down */
	RamWrite32A(gyistp_3u, 0x3F800000);		/* Up */

	RamWrite32A(gyistp_c, 0x00000000);		/* Cut Off */
	RamWrite32A(gyistp_4d, 0xBF800000);		/* Down */
	RamWrite32A(gyistp_4u, 0x3F800000);		/* Up */

	if (UcVerHig == 0x00) {
	} else {
		RamWrite32A( sxgx, 0x3F800000);		// INV Gyro signal polarity
		RamWrite32A( sygy, 0xBF800000);		// INV Gyro signal polarity
	}

	lc898122a_pantilt_mode(OFF);	/* Pan/Tilt OFF */

	lc898122a_set_di_filter(0);				/* DI initial value */
#ifdef H1COEF_CHANGER
	lc898122a_set_h1c_mode(ACTMODE);		/* Lvl Change Active mode */
#endif

	lc898122a_driver_mode(ON);				/* 0x0001 Driver Mode setting */

	RegWriteA(WC_EQON, 0x01);	/* 0x0101 Filter ON */

	RegWriteA(WG_NPANST12BTMR, 0x01);	/* 0x0167 */
	lc898122a_pantilt_mode(ON);					/* Pan/Tilt */
	RegWriteA(WG_PANSTT6, 0x44);		/* 0x010A */
	RegWriteA(WG_PANSTT6, 0x11);		/* 0x010A */
}

/*********************************************************************
 Function Name	: lc898122a_driver_mode
 Return Value		: NON
 Argument Value	: 0:OFF  1:ON
 Explanation		: Driver Mode setting function
 History			: First edition							2012.04.25 Y.Shigeoka
*********************************************************************/
void lc898122a_driver_mode(uint8_t UcDrvSw)
{
	if (UcDrvSw == ON) {
		if (UcPwmMod == PWMMOD_CVL) {
			RegWriteA(DRVFC, 0xF0);		/* 0x0001 Drv.MODE=1,Drv.BLK=1,MODE2,LCEN */
		} else {
#ifdef PWM_BREAK
			RegWriteA(DRVFC, 0x00);		/* 0x0001 Drv.MODE=0,Drv.BLK=0,MODE0B */
#else
			RegWriteA(DRVFC, 0xC0);		/* 0x0001 Drv.MODE=1,Drv.BLK=1,MODE1 */
#endif
		}
	} else {
		if (UcPwmMod == PWMMOD_CVL) {
			RegWriteA(DRVFC, 0x30);		/* 0x0001 Drvier Block Ena=0 */
		} else {
#ifdef PWM_BREAK
			RegWriteA(DRVFC, 0x00);		/* 0x0001 Drv.MODE=0,Drv.BLK=0,MODE0B */
#else
			RegWriteA(DRVFC, 0x00);		/* 0x0001 Drvier Block Ena=0 */
#endif
		}
	}
}

/*********************************************************************
 Function Name	: lc898122a_ram_mode
 Return Value		: NON
 Argument Value	: 0:OFF  1:ON
 Explanation		: Ram Access Fix Mode setting function
 History			: First edition							2013.05.21 Y.Shigeoka
*********************************************************************/
void lc898122a_ram_mode(uint8_t UcAccMod)
{
	switch (UcAccMod) {
	case OFF:
		RegWriteA(WC_RAMACCMOD,	0x00);	/* 0x018C GRAM Access Float32bit */
		break;
	case ON:
		RegWriteA(WC_RAMACCMOD,	0x31);	/* 0x018C GRAM Access Fix32bit */
		break;
	}
}

/*********************************************************************
 Function Name	: lc898122a_return_center
 Return Value		: Command Status
 Argument Value	: Command Parameter
 Explanation		: Return to center Command Function
 History			: First edition							2013.01.15 Y.Shigeoka
*********************************************************************/
uint8_t lc898122a_return_center(uint8_t UcCmdPar)
{
	uint8_t UcCmdSts;

	UcCmdSts = EXE_END;

	lc898122a_gyro_control(OFF);	/* Gyro OFF */

	if (!UcCmdPar) {
		lc898122a_stabilizer_on(); /* X,Y Centering */
		/* Slope Mode */
	} else if (UcCmdPar == 0x01) {
		/* X Centering Only */
		lc898122a_servo_command(X_DIR, ON);	/* X only Servo ON */
		lc898122a_servo_command(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) {
		/* Y Centering Only */
		lc898122a_servo_command(X_DIR, OFF);	/* Y only Servo ON */
		lc898122a_servo_command(Y_DIR, ON);
	}

	return UcCmdSts;
}

/*********************************************************************
 Function Name	: lc898122a_servo_command
 Return Value		: NON
 Argument Value	: X or Y Select, Servo ON/OFF
 Explanation		: Servo ON,OFF Function
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
void lc898122a_servo_command(uint8_t UcDirSel, uint8_t UcSwcCon)
{
	uint8_t UcAccMod;

	RegReadA(WC_RAMACCMOD, &UcAccMod);

	if (UcSwcCon) {
		if (!UcDirSel) {
			/* X Direction */
			RegWriteA(WH_EQSWX, 0x03);			/* 0x0170 */
			if (UcAccMod == 0x31) {
				/* 0x10B5 */
				RamWriteA(sxggf, 0x0000);
			} else {
				RamWrite32A(sxggf, 0x00000000);	/* 0x10B5 */
			}
		} else {								/* Y Direction */
			RegWriteA(WH_EQSWY, 0x03);			/* 0x0171 */
			if (UcAccMod == 0x31) {
				/* 0x11B5 */
				RamWriteA(syggf, 0x0000);
			} else {
				RamWrite32A(syggf, 0x00000000);	/* 0x11B5 */
			}
		}
	} else {
		if (!UcDirSel) {
			/* X Direction */
			RegWriteA(WH_EQSWX, 0x02);			/* 0x0170 */
			if (UcAccMod == 0x31) {
				/* 0x1477 */
				RamWriteA(SXLMT, 0x0000);
			} else {
				RamWrite32A(SXLMT, 0x00000000);	/* 0x1477 */
			}
		} else {								/* Y Direction */
			RegWriteA(WH_EQSWY, 0x02);			/* 0x0171 */
			if (UcAccMod == 0x31) {
				/* 0x14F7 */
				RamWriteA(SYLMT, 0x0000);
			} else {
				RamWrite32A(SYLMT, 0x00000000);	/* 0x14F7 */
			}
		}
	}
}

/*********************************************************************
 Function Name	: lc898122a_stabilizer_on
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Stabilizer For Servo On Function
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
void lc898122a_stabilizer_on(void)
{
	uint8_t UcRegValx, UcRegValy;		/* Register value */

	RegReadA(WH_EQSWX, &UcRegValx);			/* 0x0170 */
	RegReadA(WH_EQSWY, &UcRegValy);			/* 0x0171 */

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {

		RegWriteA(WH_SMTSRVON, 0x01);	/* 0x017C Smooth Servo ON */

		lc898122a_servo_command(X_DIR, ON);
		lc898122a_servo_command(Y_DIR, ON);

		lc898122a_wait_time(100);
		/*		lc898122a_wait_time(100); */

		RegWriteA(WH_SMTSRVON, 0x00);	/* 0x017C Smooth Servo OFF */

	} else {
		lc898122a_servo_command(X_DIR, ON);
		lc898122a_servo_command(Y_DIR, ON);
	}
}

/*********************************************************************
 Function Name	: lc898122a_gyro_control
 Return Value		: NON
 Argument Value	: Gyro Filter ON or OFF
 Explanation		: Gyro Filter Control Function
 History			: First edition							2013.01.15 Y.Shigeoka
*********************************************************************/
void lc898122a_gyro_control(uint8_t UcGyrCon)
{
	uint8_t UcAccMod;

	RegReadA(WC_RAMACCMOD, &UcAccMod);

	/* Return HPF Setting */
	RegWriteA(WG_SHTON, 0x00);	/* 0x0107 */

	if (UcGyrCon == ON) {
		/* Gyro ON */
#ifdef GAIN_CONT
		/* Gain3 Register */
		lc898122a_auto_gain_con(ON);		/* Auto Gain Control Mode ON */
#endif
		lc898122a_gyro_ram_clear(0x000E, CLR_FRAM1);	/* Gyro Delay RAM Clear */

		if (UcAccMod == 0x31) {
			RamWriteA(sxggf, 0x7FFF);		/* 0x10B5 */
			RamWriteA(syggf, 0x7FFF);		/* 0x11B5 */

			lc898122a_ram_mode(OFF);
			RamWrite32A(gxib_1, 0xBF800000);	/* Down */
			RamWrite32A(gyib_1, 0xBF800000);	/* Down */
			RamWrite32A(gxib_1, 0xB261CF49);	/* Down */
			RamWrite32A(gyib_1, 0xB261CF49);	/* Down */
			lc898122a_ram_mode(ON);
		} else {
			RamWrite32A(sxggf, 0x3F800000);		/* 0x10B5 */
			RamWrite32A(syggf, 0x3F800000);		/* 0x11B5 */

			RamWrite32A(gxib_1, 0xBF800000);	/* Down */
			RamWrite32A(gyib_1, 0xBF800000);	/* Down */
			RamWrite32A(gxib_1, 0xB261CF49);	/* Down */
			RamWrite32A(gyib_1, 0xB261CF49);	/* Down */
		}
		RegWriteA(WG_PANSTT6, 0x00);	/* 0x010A */
	} else if (UcGyrCon == SPC) {
		/* Gyro ON for LINE */

#ifdef GAIN_CONT
		/* Gain3 Register */
		lc898122a_auto_gain_con(ON);	/* Auto Gain Control Mode ON */
#endif
		RamWrite32A(sxggf, 0x3F800000);	/* 0x10B5 */
		RamWrite32A(syggf, 0x3F800000);	/* 0x11B5 */

	} else {								/* Gyro OFF */
		if (UcAccMod == 0x31) {
			RamWriteA(sxggf, 0x0000);		/* 0x10B5 */
			RamWriteA(syggf, 0x0000);		/* 0x11B5 */
		} else {
			RamWrite32A(sxggf, 0x00000000);	/* 0x10B5 */
			RamWrite32A(syggf, 0x00000000);	/* 0x11B5 */
		}
#ifdef	GAIN_CONT
		/* Gain3 Register */
		lc898122a_auto_gain_con(OFF);			/* Auto Gain Control Mode OFF */
#endif
	}
}

/*********************************************************************
 Function Name	: lc898122a_pantilt_mode
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Pan-Tilt Enable/Disable
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
void lc898122a_pantilt_mode(uint8_t UcPnTmod)
{
	switch (UcPnTmod) {
	case OFF:
		RegWriteA(WG_PANON, 0x00);	/* 0x0109 X,Y Pan/Tilt Function OFF */
		break;
	case ON:
#ifdef NEW_PTST
		RegWriteA(WG_PANON, 0x10);	/* 0x0109 X,Y New Pan/Tilt Function ON */
#else
		RegWriteA(WG_PANON, 0x01);	/* 0x0109 X,Y Pan/Tilt Function ON */
#endif
		break;
	}
}

/*********************************************************************
 Function Name	: lc898122a_ois_enable
 Return Value		: NON
 Argument Value	: Command Parameter
 Explanation		: OIS Enable Control Function
 History			: First edition							2013.01.15 Y.Shigeoka
*********************************************************************/
void lc898122a_ois_enable(void)
{
	lc898122a_gyro_control(ON);
}

/*********************************************************************
 Function Name	: lc898122a_ois_off
 Retun Value		:
 Argment Value	:
 Explanation		:
 History			:
*********************************************************************/
void lc898122a_ois_off(void) /*Ois Off */
{
	lc898122a_gyro_control(OFF);
	return;
}

/*********************************************************************
 Function Name	: lc898122a_gyro_ram_clear
 Retun Value		: NON
 Argment Value	: UsClrFil - Select filter to clear.  If 0x0000, clears entire filter
					  UcClrMod - 0x01: FRAM0 Clear, 0x02: FRAM1, 0x03: All RAM Clear
 Explanation		: Gyro RAM clear function
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
int32_t	lc898122a_gyro_ram_clear(uint16_t UsClrFil, uint8_t UcClrMod)
{
	uint8_t UcRamClr;

	/*Select Filter to clear*/
	RegWriteA(WC_RAMDLYMOD1, (uint8_t)(UsClrFil >> 8));	/* 0x018F FRAM Initialize Hbyte */
	RegWriteA(WC_RAMDLYMOD0, (uint8_t)UsClrFil);			/* 0x018E FRAM Initialize Lbyte */

	/*Enable Clear*/
	RegWriteA(WC_RAMINITON, UcClrMod);	/* 0x0102[ - | - | - | - ][ - | - | 遅延Clr | 係数Clr ] */

	/*Check RAM Clear complete*/
	do {
		RegReadA(WC_RAMINITON, &UcRamClr);
		UcRamClr &= UcClrMod;
	} while (UcRamClr != 0x00);

	return 0;
}

void lc898122a_mem_clear(uint8_t *NcTgtPtr, uint16_t UsClrSiz)
{
	uint16_t UsClrIdx;

	for (UsClrIdx = 0; UsClrIdx < UsClrSiz; UsClrIdx++) {
		*NcTgtPtr = 0;
		NcTgtPtr++;
	}

	return;
}

/*********************************************************************
 Function Name	: lc898122a_acc_wait
 Return Value		: NON
 Argument Value	: Trigger Register Data
 Explanation		: Acc Wait Function
 History			: First edition							2010.12.27 Y.Shigeoka
*********************************************************************/
int32_t	lc898122a_acc_wait(uint8_t UcTrgDat)
{
	uint8_t UcFlgVal;
	uint8_t UcCntPla;
	UcFlgVal = 1;
	UcCntPla = 0;

	do {
		RegReadA(GRACC, &UcFlgVal);
		UcFlgVal &= UcTrgDat;
		UcCntPla++;
	} while (UcFlgVal && (UcCntPla < ACCWIT_POLLING_LIMIT_A));
	if (UcCntPla == ACCWIT_POLLING_LIMIT_A)
		return OIS_FW_POLLING_FAIL;

	return OIS_FW_POLLING_PASS;
}

#ifdef GAIN_CONT
/*********************************************************************
 Function Name	: lc898122a_auto_gain_con
 Retun Value		: NON
 Argment Value	: 0 :OFF  1:ON
 Explanation		: Select Gyro Signal Function
 History			: First edition							2010.11.30 Y.Shigeoka
*********************************************************************/
void lc898122a_auto_gain_con(uint8_t UcModeSw)
{
	if (UcModeSw == OFF) {
		RegWriteA(WG_ADJGANGXATO, 0xA0);	/* 0x0129	X exe off */
		RegWriteA(WG_ADJGANGYATO, 0xA0);	/* 0x012A	Y exe off */
		RamWrite32A(GANADR, XMAXGAIN);			/* Gain Through */
		RamWrite32A(GANADR | 0x0100, YMAXGAIN);	/* Gain Through */
	} else {
		RegWriteA(WG_ADJGANGXATO, 0xA3);	/* 0x0129	X exe on */
		RegWriteA(WG_ADJGANGYATO, 0xA3);	/* 0x012A	Y exe on */
	}
}
#endif


/*********************************************************************
 Function Name	: S2cPro
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: S2 Command Function
 History			: First edition							2013.01.15 Y.Shigeoka
*********************************************************************/
void S2cPro(uint8_t uc_mode)
{
	if (uc_mode == 1) {
#ifdef H1COEF_CHANGER
		lc898122a_set_h1c_mode(S2MODE);					/* cancel Lvl change */
#endif
		/* HPF→Through Setting */
		RegWriteA(WG_SHTON, 0x11);			/* 0x0107 */
		RamWrite32A(gxh1c, DIFIL_S2);		/* 0x1012 */
		RamWrite32A(gyh1c, DIFIL_S2);		/* 0x1112 */
	} else {
		RamWrite32A(gxh1c, UlH1Coefval);	/* 0x1012 */
		RamWrite32A(gyh1c, UlH1Coefval);	/* 0x1112 */
		/* HPF→Through Setting */
		RegWriteA(WG_SHTON, 0x00);			/* 0x0107 */

#ifdef H1COEF_CHANGER
		lc898122a_set_h1c_mode(UcH1LvlMod);				/* Re-setting */
#endif
	}

}

/*********************************************************************
 Function Name	: lc898122a_set_di_filter
 Retun Value		: NON
 Argment Value	: Command Parameter
 Explanation		: Set DI filter coefficient Function
 History			: First edition							2013.03.22 Y.Shigeoka
*********************************************************************/
void lc898122a_set_di_filter(uint8_t UcSetNum)
{
	/* Zoom Step */
	if (UcSetNum > (COEFTBL - 1))
		UcSetNum = (COEFTBL - 1);		/* 上限をCOEFTBL-1に設定する */

	UlH1Coefval	= ClDiCof[UcSetNum];

	/* Zoom Value Setting */
	RamWrite32A(gxh1c, UlH1Coefval);	/* 0x1012 */
	RamWrite32A(gyh1c, UlH1Coefval);	/* 0x1112 */

#ifdef H1COEF_CHANGER
	lc898122a_set_h1c_mode(UcSetNum);			/* Re-setting */
#endif

}

#ifdef H1COEF_CHANGER
/*********************************************************************
 Function Name	: lc898122a_set_h1c_mode
 Retun Value		: NON
 Argment Value	: Command Parameter
 Explanation		: Set H1C coefficient Level chang Function
 History			: First edition							2013.04.18 Y.Shigeoka
*********************************************************************/
void lc898122a_set_h1c_mode(uint8_t UcSetNum)
{
	switch (UcSetNum) {
	case (ACTMODE):			/* initial */
		lc898122a_init_pan_mode(OFF);	/* Pan/Tilt setting (Still) */

		/* enable setting */
		/* Zoom Step */
		UlH1Coefval	= ClDiCof[0];

		UcH1LvlMod = 0;

		/* Limit value Value Setting */
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */

		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */

		RamWrite32A(gxmg, CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF);			/* 0x11AA Change coefficient gain */

		RamWrite32A(gxhc_tmp, UlH1Coefval);	/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);	/* 0x110E Base Coef */

		RegWriteA(WG_HCHR, 0x12);			/* 0x011B GmHChrOn[1]=1 Sw ON */
		break;

	case (S2MODE):	/* cancel lvl change mode */
		RegWriteA(WG_HCHR, 0x10);	/* 0x011B GmHChrOn[1]=0 Sw OFF */
		break;

	case (MOVMODE):		/* Movie mode */
		lc898122a_init_pan_mode(ON);					/* Pan/Tilt setting (Movie) */

		RamWrite32A(gxlmt6L, MINLMT_MOV);	/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT_MOV);	/* 0x102E H-Limit */

		RamWrite32A(gylmt6L, MINLMT_MOV);	/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT_MOV);	/* 0x112E H-Limit */

		RamWrite32A(gxmg, CHGCOEF_MOV);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF_MOV);		/* 0x11AA Change coefficient gain */

		RamWrite32A(gxhc_tmp, UlH1Coefval);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);		/* 0x110E Base Coef */

		RegWriteA(WG_HCHR, 0x12);			/* 0x011B GmHChrOn[1]=1 Sw ON */
		break;

	default:
		lc898122a_init_pan_mode(OFF);					/* Pan/Tilt setting (Still) */

		UcH1LvlMod = UcSetNum;

		/* Limit value Value Setting */
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */

		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */

		RamWrite32A(gxmg, CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF);			/* 0x11AA Change coefficient gain */

		RamWrite32A(gxhc_tmp, UlH1Coefval);	/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);	/* 0x110E Base Coef */

		RegWriteA(WG_HCHR, 0x12);			/* 0x011B GmHChrOn[1]=1 Sw ON */
		break;

	}
}
#endif

#ifdef STANDBY_MODE
/*********************************************************************
 Function Name	: lc898122a_set_stand_by
 Retun Value		: NON
 Argment Value	: 0:Standby ON 1:Standby OFF 2:Standby2 ON 3:Standby2 OFF
					: 4:Standby3 ON 5:Standby3 OFF
 Explanation		: Set Standby
 History			: First edition							2013.01.15 Y.Shigeoka
*********************************************************************/
void lc898122a_set_stand_by(uint8_t UcContMode)
{
	uint8_t UcStbb0, UcClkon;
	switch (UcContMode) {
	case STB1_ON:
		RegWriteA(DRVFCAF, 0x00);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
		RegWriteA(STBB0, 0x00);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(STBB1, 0x00);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(PWMA, 0x00);		/* 0x0010 PWM Standby */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(CVA, 0x00);		/* 0x0020 LINEAR PWM mode standby	*/
		lc898122a_driver_mode(OFF);					/* Driver OFF */
		lc898122a_af_driver_mode(OFF);				/* AF Driver OFF */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		lc898122a_select_gyro_mode(ON);			/* Gyro Sleep */
		break;
	case STB1_OFF:
		lc898122a_select_gyro_mode(OFF);			/* Gyro Wake Up */
		RegWriteA(PWMMONA, 0x80);	/* 0x0030 Monitor Active	*/
		lc898122a_driver_mode(ON);					/* Driver Mode setting */
		lc898122a_af_driver_mode(ON);				/* AF Driver Mode setting */
		RegWriteA(CVA, 0xC0);		/* 0x0020 Linear PWM mode enable */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(PWMA, 0xC0);		/* 0x0010 PWM enable */
		RegWriteA(STBB1, 0x05);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(STBB0, 0xDF);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;
	case STB2_ON:
		RegWriteA(DRVFCAF, 0x00);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
		RegWriteA(STBB0, 0x00);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(STBB1, 0x00);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(PWMA, 0x00);		/* 0x0010 PWM Standby */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(CVA, 0x00);		/* 0x0020 LINEAR PWM mode standby	*/
		lc898122a_driver_mode(OFF);					/* Drvier Block Ena=0 */
		lc898122a_af_driver_mode(OFF);				/* AF Drvier Block Ena=0 */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		lc898122a_select_gyro_mode(ON);			/* Gyro Sleep */
		RegWriteA(CLKON, 0x00);		/* 0x020B Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		break;
	case STB2_OFF:
		RegWriteA(CLKON, 0x1F);		/* 0x020B [ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ] */
		lc898122a_select_gyro_mode(OFF);			/* Gyro Wake Up */
		RegWriteA(PWMMONA, 0x80);	/* 0x0030 Monitor Active	*/
		lc898122a_driver_mode(ON);					/* Driver Mode setting */
		lc898122a_af_driver_mode(ON);				/* AF Driver Mode setting */
		RegWriteA(CVA, 0xC0);		/* 0x0020 Linear PWM mode enable */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(PWMA,	0xC0);		/* 0x0010 PWM enable */
		RegWriteA(STBB1, 0x05);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(STBB0, 0xDF);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;
	case STB3_ON:
		RegWriteA(DRVFCAF, 0x00);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
		RegWriteA(STBB0, 0x00);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(STBB1, 0x00);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(PWMA, 0x00);		/* 0x0010 PWM Standby */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(CVA, 0x00);		/* 0x0020 LINEAR PWM mode standby	*/
		lc898122a_driver_mode(OFF);					/* Drvier Block Ena=0 */
		lc898122a_af_driver_mode(OFF);				/* AF Drvier Block Ena=0 */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		lc898122a_select_gyro_mode(ON);			/* Gyro Sleep */
		RegWriteA(CLKON, 0x00);		/* 0x020B Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		RegWriteA(I2CSEL, 0x01);	/* 0x0248 I2C Noise Cancel circuit OFF	*/
		RegWriteA(OSCSTOP, 0x02);	/* 0x0256 Source Clock Input OFF */
		break;
	case STB3_OFF:
		RegWriteA(OSCSTOP, 0x00);	/* 0x0256 Source Clock Input ON */
		RegWriteA(I2CSEL, 0x00);	/* 0x0248 I2C Noise Cancel circuit ON	*/
		RegWriteA(CLKON, 0x1F);		/* 0x020B [ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ] */
		lc898122a_select_gyro_mode(OFF);			/* Gyro Wake Up */
		RegWriteA(PWMMONA, 0x80);	/* 0x0030 Monitor Active	*/
		lc898122a_driver_mode(ON);					/* Driver Mode setting */
		lc898122a_af_driver_mode(ON);				/* AF Driver Mode setting */
		RegWriteA(CVA, 0xC0);		/* 0x0020 Linear PWM mode enable */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(PWMA,	0xC0);		/* 0x0010 PWM enable */
		RegWriteA(STBB1, 0x05);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(STBB0, 0xDF);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;

	case STB4_ON:
		RegWriteA(DRVFCAF, 0x00);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
		RegWriteA(STBB0, 0x00);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(STBB1, 0x00);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(PWMA, 0x00);		/* 0x0010 PWM Standby */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(CVA, 0x00);		/* 0x0020 LINEAR PWM mode standby	*/
		lc898122a_driver_mode(OFF);					/* Drvier Block Ena=0 */
		lc898122a_af_driver_mode(OFF);				/* AF Drvier Block Ena=0 */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		lc898122a_sel_gyro_sig_cont();			/* Gyro Continuos mode */
		RegWriteA(CLKON, 0x04);		/* 0x020B Servo & PWM Clock OFF + D-Gyro I/F ON	*/
		break;
	case STB4_OFF:
		RegWriteA(CLKON, 0x1F);		/* 0x020B [ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ] */
		lc898122a_select_gyro_mode(OFF);			/* Gyro OIS mode */
		RegWriteA(PWMMONA, 0x80);	/* 0x0030 Monitor Active	*/
		lc898122a_driver_mode(ON);					/* Driver Mode setting */
		lc898122a_af_driver_mode(ON);				/* AF Driver Mode setting */
		RegWriteA(CVA, 0xC0);		/* 0x0020 Linear PWM mode enable */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(PWMA,	0xC0);		/* 0x0010 PWM enable */
		RegWriteA(STBB1, 0x05);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(STBB0, 0xDF);		/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;

	/************** special mode ************/
	case STB2_OISON:
		RegReadA(STBB0, &UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		UcStbb0 &= 0x80;
		RegWriteA(STBB0, UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(PWMA, 0x00);		/* 0x0010 PWM Standby */
		RegWriteA(CVA, 0x00);		/* 0x0020 LINEAR PWM mode standby	*/
		lc898122a_driver_mode(OFF);					/* Drvier Block Ena=0 */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		lc898122a_select_gyro_mode(ON);			/* Gyro Sleep */
		RegReadA(CLKON, &UcClkon);	/* 0x020B PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		UcClkon &= 0x1A;
		RegWriteA(CLKON, UcClkon);	/* 0x020B PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		break;
	case STB2_OISOFF:
		RegReadA(CLKON, &UcClkon);	/* 0x020B PWM Clock OFF + D-Gyro I/F ON  */
		UcClkon |= 0x05;
		RegWriteA(CLKON, UcClkon);	/* 0x020B [ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ] */
		lc898122a_select_gyro_mode(OFF);			/* Gyro Wake Up */
		RegWriteA(PWMMONA, 0x80);	/* 0x0030 Monitor Active	*/
		lc898122a_driver_mode(ON);					/* Driver Mode setting */
		RegWriteA(CVA, 0xC0);		/* 0x0020 Linear PWM mode enable */
		RegWriteA(PWMA,	0xC0);		/* 0x0010 PWM enable */
		RegReadA(STBB0, &UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		UcStbb0 |= 0x5F;
		RegWriteA(STBB0, UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;

	case STB2_AFON:
		RegWriteA(DRVFCAF, 0x00);	/* 0x0081 Drv.MODEAF=0,Drv.ENAAF=0,MODE-0 */
		RegReadA(STBB0, &UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		UcStbb0 &= 0x7F;
		RegWriteA(STBB0, UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		RegWriteA(STBB1, 0x00);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		lc898122a_af_driver_mode(OFF);				/* AF Drvier Block Ena=0 */
		RegWriteA(PWMMONA, 0x00);	/* 0x0030 Monitor Standby */
		RegReadA(CLKON, &UcClkon);	/* 0x020B OPAF Clock OFF + AFPWM OFF SRVCLK can't OFF	*/
		UcClkon &= 0x07;
		RegWriteA(CLKON, UcClkon);	/* 0x020B OPAF Clock OFF + AFPWM OFF	SRVCLK can't OFF	*/
		break;
	case STB2_AFOFF:
		RegReadA(CLKON, &UcClkon);	/* 0x020B OPAF Clock ON + AFPWM ON  */
		UcClkon |= 0x18;
		RegWriteA(CLKON, UcClkon);	/* 0x020B [ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ] */
		lc898122a_af_driver_mode(ON);					/* AF Driver Mode setting */
		RegWriteA(PWMAAF, 0x00);	/* 0x0090 AF PWM Standby */
		RegWriteA(STBB1, 0x05);		/* 0x0264 [ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ] */
		RegReadA(STBB0, &UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		UcStbb0 |= 0x80;
		RegWriteA(STBB0, UcStbb0);	/* 0x0250 [ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ] */
		break;
	}
}

/*********************************************************************
 Function Name	: lc898122a_select_gyro_mode
 Retun Value		: NON
 Argment Value	: mode
 Explanation		: Select Gyro mode Function
 History			: First edition							2010.12.27 Y.Shigeoka
*********************************************************************/
void lc898122a_select_gyro_mode(uint8_t UcSelMode)
{
#ifdef USE_INVENSENSE
	uint8_t	UcRamIni;
	uint8_t	UcGrini;

	if (UcSelMode == ON) {
		RegWriteA(WC_EQON, 0x00);	/* 0x0101 Equalizer OFF */
		RegWriteA(GRSEL, 0x01);		/* 0x0280 Set Command Mode */

		RegReadA(GRINI, &UcGrini);	/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */
		RegWriteA(GRINI, (UcGrini | SLOWMODE));		/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ] */

		RegWriteA(GRADR0, 0x6B);	/* 0x0283 Set Write Command	*/
		RegWriteA(GRACC, 0x01);		/* 0x0282 Set Read Trigger ON */
		lc898122a_acc_wait(0x01);				/* Digital Gyro busy wait */
		RegReadA(GRDAT0H, &UcRamIni);	/* 0x0290 */

		UcRamIni |= 0x40;			/* Set Sleep bit */
#ifdef GYROSTBY
		UcRamIni &= ~0x01;			/* Clear PLL bit(internal oscillator */
#endif

		RegWriteA(GRADR0, 0x6B);	/* 0x0283 Set Write Command */
		RegWriteA(GSETDT, UcRamIni);/* 0x028A Set Write Data(Sleep ON) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */

#ifdef GYROSTBY
		RegWriteA(GRADR0, 0x6C);	/* 0x0283 Set Write Command */
		RegWriteA(GSETDT, 0x07);	/* 0x028A Set Write Data(STBY ON) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */
#endif
	} else {
#ifdef GYROSTBY
		RegWriteA(GRADR0, 0x6C);	/* 0x0283 Set Write Command */
		RegWriteA(GSETDT, 0x00);	/* 0x028A Set Write Data(STBY OFF) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */
#endif
		RegWriteA(GRADR0, 0x6B);	/* 0x0283 Set Write Command */
		RegWriteA(GRACC, 0x01);		/* 0x0282 Set Read Trigger ON */
		lc898122a_acc_wait(0x01);				/* Digital Gyro busy wait */
		RegReadA(GRDAT0H, &UcRamIni);/* 0x0290 */

		UcRamIni &= ~0x40;			/* Clear Sleep bit */
#ifdef GYROSTBY
		UcRamIni |=  0x01;			/* Set PLL bit */
#endif

		RegWriteA(GSETDT, UcRamIni);/* 0x028A Set Write Data(Sleep OFF) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */

		RegReadA(GRINI, &UcGrini);	/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ] */
		RegWriteA(GRINI, (UcGrini & ~SLOWMODE));/* 0x0281 [ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ] */

		lc898122a_select_gyro_sig();	/* Select Gyro output signal */
		lc898122a_wait_time(50);		/* 50ms wait */

		RegWriteA(WC_EQON, 0x01);	/* 0x0101 GYRO Equalizer ON */

		lc898122a_gyro_ram_clear(0x007F, CLR_FRAM1);	/* Gyro Delay RAM Clear */
	}
#else									/* Panasonic */

	if (UcSelMode == ON) {
		RegWriteA(WC_EQON, 0x00);	/* 0x0101 GYRO Equalizer OFF */
		RegWriteA(GRSEL, 0x01);		/* 0x0280 Set Command Mode */
		RegWriteA(GRADR0, 0x4C);	/* 0x0283 Set Write Command */
		RegWriteA(GSETDT, 0x02);	/* 0x028A Set Write Data(Sleep ON) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);				/* Digital Gyro busy wait */
	} else {
		RegWriteA(GRADR0, 0x4C);	/* 0x0283 Set Write Command */
		RegWriteA(GSETDT, 0x00);	/* 0x028A Set Write Data(Sleep OFF) */
		RegWriteA(GRACC, 0x10);		/* 0x0282 Set Trigger ON */
		lc898122a_acc_wait(0x10);	/* Digital Gyro busy wait */
		lc898122a_select_gyro_sig();/* Select Gyro output signal */

		lc898122a_wait_time(50);	/* 50ms wait */

		RegWriteA(WC_EQON, 0x01);	/* 0x0101 GYRO Equalizer ON */
		lc898122a_gyro_ram_clear(0x007F, CLR_FRAM1);	/* Gyro Delay RAM Clear */
	}
#endif
}

/*********************************************************************
 Function Name	: lc898122a_select_gyro_sig
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: Select Gyro Signal Function
 History			: First edition							2010.12.27 Y.Shigeoka
*********************************************************************/
void lc898122a_select_gyro_sig(void)
{

	if (UcVerHig == 0x00) {
		RegWriteA(GRADR0, GYROX_INI);			/* 0x0283	Set Gyro XOUT H~L */
		RegWriteA(GRADR1, GYROY_INI);			/* 0x0284	Set Gyro YOUT H~L */
	} else {
		RegWriteA(GRADR0, GYROY_INI);			/* 0x0283	Set Gyro XOUT H~L */
		RegWriteA(GRADR1, GYROX_INI);			/* 0x0284	Set Gyro YOUT H~L */
	}

	/*Start OIS Reading*/
	RegWriteA(GRSEL, 0x02);		/* 0x0280[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ] */

}

/*********************************************************************
 Function Name	: lc898122a_sel_gyro_sig_cont
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: Select Gyro Continuosl Function
 History			: First edition							2013.06.06 Y.Shigeoka
*********************************************************************/
void lc898122a_sel_gyro_sig_cont(void)
{
	/*Start OIS Reading*/
	RegWriteA(GRSEL, 0x04);		/* 0x0280[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ] */
}

#endif	/*STANDBY_MODE */

#define	GYROFF_HIGH_MOBILE	0x1482 /*30 DPS */
#define	GYROFF_LOW_MOBILE	0xEB7E /*-30 DPS */
/*********************************************************************
 Function Name	: lc898122a_gen_measure
 Retun Value		: A/D Convert Result
 Argment Value	: Measure Filter Input Signal Ram Address
 Explanation		: General Measure Function
 History			: First edition							2013.01.10 Y.Shigeoka
*********************************************************************/
int16_t lc898122a_gen_measure(uint16_t UsRamAdd, uint8_t UcMesMod)
{
	int16_t	SsMesRlt;
	uint8_t	UcMesFin;

	RegWriteA(WC_MES1ADD0, (uint8_t)UsRamAdd);	/* 0x0194 */
	RegWriteA(WC_MES1ADD1, (uint8_t)((UsRamAdd >> 8) & 0x0001));	/* 0x0195 */
	RamWrite32A(MSABS1AV, 0x00000000);		/* 0x1041	Clear */

	if (!UcMesMod) {
		RegWriteA(WC_MESLOOP1, 0x04);		/* 0x0193 */
		RegWriteA(WC_MESLOOP0, 0x00);		/* 0x0192	1024 Times Measure */
		RamWrite32A(msmean, 0x3A7FFFF7);	/* 0x1230	1/CmMesLoop[15:0] */
	} else {
		RegWriteA(WC_MESLOOP1, 0x01);		/* 0x0193 */
		RegWriteA(WC_MESLOOP0, 0x00);		/* 0x0192	1 Times Measure */
		RamWrite32A(msmean, 0x3F800000);	/* 0x1230	1/CmMesLoop[15:0] */
	}

	RegWriteA(WC_MESABS, 0x00);				/* 0x0198	none ABS */
	RegWriteA(WC_MESMODE, 0x01);			/* 0x0190	normal Measure */
	lc898122a_wait_time(100);							/* Wait 1024 Times Measure Time */
	RegReadA(WC_MESMODE, &UcMesFin);		/* 0x0190	normal Measure */

	if (0x00 == UcMesFin)
		lc898122a_wait_time(100);

	lc898122a_ram_mode(ON);	/* Fix mode */

	RamReadA(MSABS1AV, (uint16_t *)&SsMesRlt);	/* 0x1041 */

	lc898122a_ram_mode(OFF);	/* Float mode */

	return SsMesRlt;
}

/*********************************************************************
 Function Name	: lc898122a_gyro_vc_offset
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: Tunes the Gyro VC offset
 History			: First edition							2013.01.15  Y.Shigeoka
*********************************************************************/
int32_t	lc898122a_gyro_vc_offset(void)
{
	int  SiRsltSts;
	uint16_t	UsGxoVal, UsGyoVal;
	uint8_t	UcGvcFlg = 0xFF;

	/* A/D Offset Clear */
	RegWriteA(IZAH,	0x00);	/* 0x02A0 Set Offset High byte */
	RegWriteA(IZAL,	0x00);	/* 0x02A1 Set Offset Low byte */
	RegWriteA(IZBH,	0x00);	/* 0x02A2 Set Offset High byte */
	RegWriteA(IZBL,	0x00);	/* 0x02A3 Set Offset Low byte */

	/* Measure Filter1 Setting */
	RamWrite32A(mes1aa, 0x3F800000);	/* 0x10F0	Through */
	RamWrite32A(mes1ab, 0x00000000);	/* 0x10F1 */
	RamWrite32A(mes1ac, 0x00000000);	/* 0x10F2 */
	RamWrite32A(mes1ad, 0x00000000);	/* 0x10F3 */
	RamWrite32A(mes1ae, 0x00000000);	/* 0x10F4 */
	RamWrite32A(mes1ba, 0x3F800000);	/* 0x10F5 Through */
	RamWrite32A(mes1bb, 0x00000000);	/* 0x10F6 */
	RamWrite32A(mes1bc, 0x00000000);	/* 0x10F7 */
	RamWrite32A(mes1bd, 0x00000000);	/* 0x10F8 */
	RamWrite32A(mes1be, 0x00000000);	/* 0x10F9 */

	/* Measure Filter2 Setting */
	RamWrite32A(mes2aa, 0x3F800000);	/* 0x11F0 Through */
	RamWrite32A(mes2ab, 0x00000000);	/* 0x11F1 */
	RamWrite32A(mes2ac, 0x00000000);	/* 0x11F2 */
	RamWrite32A(mes2ad, 0x00000000);	/* 0x11F3 */
	RamWrite32A(mes2ae, 0x00000000);	/* 0x11F4 */
	RamWrite32A(mes2ba, 0x3F800000);	/* 0x11F5 Through */
	RamWrite32A(mes2bb, 0x00000000);	/* 0x11F6 */
	RamWrite32A(mes2bc, 0x00000000);	/* 0x11F7 */
	RamWrite32A(mes2bd, 0x00000000);	/* 0x11F8 */
	RamWrite32A(mes2be, 0x00000000);	/* 0x11F9 */

	/*
	 X
	 */
	RegWriteA(WC_MES1ADD0, 0x00);		/* 0x0194 */
	RegWriteA(WC_MES1ADD1, 0x00);		/* 0x0195 */
	lc898122a_gyro_ram_clear(0x1000, CLR_FRAM1);			/* Measure Filter RAM Clear */
	UsGxoVal = (uint16_t)lc898122a_gen_measure(AD2Z, 0);	/* GYRMON1(0x1110) <- GXADZ(0x144A) */
	RegWriteA(IZAH, (uint8_t)(UsGxoVal >> 8));	/* 0x02A0 Set Offset High byte */
	RegWriteA(IZAL, (uint8_t)(UsGxoVal));		/* 0x02A1 Set Offset Low byte */

	/*
	  Y
	 */
	RegWriteA(WC_MES1ADD0, 0x00);		/* 0x0194 */
	RegWriteA(WC_MES1ADD1, 0x00);		/* 0x0195 */
	lc898122a_gyro_ram_clear(0x1000, CLR_FRAM1);			/* Measure Filter RAM Clear */
	UsGyoVal = (uint16_t)lc898122a_gen_measure(AD3Z, 0);	/* GYRMON2(0x1111) <- GYADZ(0x14CA) */
	RegWriteA(IZBH, (uint8_t)(UsGyoVal >> 8));	/* 0x02A2 Set Offset High byte */
	RegWriteA(IZBL, (uint8_t)(UsGyoVal));		/* 0x02A3 Set Offset Low byte */

	SiRsltSts = EXE_END;				/* Clear Status */

	if (((signed short)UsGxoVal < (signed short)GYROFF_LOW_MOBILE) ||
		((signed short)UsGxoVal > (signed short)GYROFF_HIGH_MOBILE)) {
		SiRsltSts |= EXE_GXADJ;
	}

	if (((signed short)UsGyoVal < (signed short)GYROFF_LOW_MOBILE) ||
		((signed short)UsGyoVal > (signed short)GYROFF_HIGH_MOBILE)) {
		SiRsltSts |= EXE_GYADJ;
	}

	E2pWrt((uint16_t)0x0937, 2, (uint8_t *)&UsGxoVal);
	lc898122a_wait_time(10);		/*GYRO OFFSET Mobile X */
	E2pWrt((uint16_t)0x0939, 2, (uint8_t *)&UsGyoVal);
	lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Y */

	if (EXE_END == SiRsltSts) {
		UcGvcFlg = 0xE7;
		E2pWrt((uint16_t)0x093B, 1, (uint8_t *)&UcGvcFlg);
		lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Flag */
		SiRsltSts = 0;	/*Success */
	} else {
		UcGvcFlg = 0xF0;
		E2pWrt((uint16_t)0x093B, 1, (uint8_t *)&UcGvcFlg);
		lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Flag */
		SiRsltSts = -1;	/*Fail */
	}

	return SiRsltSts;
}

static struct msm_ois_func_tbl lgit_ois_func_tbl;

int32_t lgit_init_set_ois(struct msm_ois_ctrl_t *o_ctrl,
						  struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	enum ois_ver_t ver;

	CDBG("%s Enter\n", __func__);

	if (copy_from_user(&ver, (void *)set_info->setting, sizeof(enum ois_ver_t)))
		ver = OIS_VER_RELEASE;

	CDBG("%s: ois_on! ver:%d\n", __func__, ver);

	rc = lc898122a_init();
	if (rc < 0) {
		CDBG("lc898122a_init fail %d\n", rc);
		return rc;
	}

	lc898122a_return_center(0);

	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	CDBG("%s End\n", __func__);

	return rc;
}

int32_t	lgit_ois_on(struct msm_ois_ctrl_t *o_ctrl,
					struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	CDBG("%s Enter\n", __func__);
	lc898122a_ois_enable();
	CDBG("%s End\n", __func__);
	return rc;
}

int32_t	lgit_ois_off(struct msm_ois_ctrl_t *o_ctrl,
					 struct msm_ois_set_info_t *set_info)
{
	CDBG("%s Enter\n", __func__);
	lc898122a_ois_off();
	usleep(1000);
	CDBG("%s End\n", __func__);

	return OIS_SUCCESS;
}

int32_t lgit_ois_mode(struct msm_ois_ctrl_t *o_ctrl,
					  struct msm_ois_set_info_t *set_info)
{
	int cur_mode = lgit_ois_func_tbl.ois_cur_mode;
	uint8_t mode = *(uint8_t *)set_info->setting;

	CDBG("%s Enter\n", __func__);
	switch (mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
		CDBG("%s:OIS_MODE_PREVIEW_CAPTURE %d\n", __func__, mode);
		break;
	case OIS_MODE_CAPTURE:
		CDBG("%s:OIS_MODE_CAPTURE %d\n", __func__, mode);
		break;
	case OIS_MODE_VIDEO:
		CDBG("%s:OIS_MODE_VIDEO %d\n", __func__, mode);
		break;
	case OIS_MODE_CENTERING_ONLY:
		CDBG("%s:OIS_MODE_CENTERING_ONLY %d\n", __func__, mode);
		break;
	case OIS_MODE_CENTERING_OFF:
		CDBG("%s:OIS_MODE_CENTERING_OFF %d\n", __func__, mode);
		break;
	default:
		CDBG("%s:not support %d\n", __func__, mode);
		break;
	}

	if (cur_mode == mode)
		return OIS_SUCCESS;

	switch (mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
		lc898122a_set_h1c_mode(ACTMODE);
		break;
	case OIS_MODE_VIDEO:
		lc898122a_set_h1c_mode(MOVMODE);
		break;
	case OIS_MODE_CENTERING_ONLY:
		lc898122a_return_center(0);
		break;
	case OIS_MODE_CENTERING_OFF:
		lc898122a_servo_command(X_DIR, OFF);
		lc898122a_servo_command(Y_DIR, OFF);
		break;
	}

	lgit_ois_func_tbl.ois_cur_mode = mode;

	CDBG("%s End\n", __func__);

	return OIS_SUCCESS;
}

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 176
#define STABLE_THRESHOLD  600 /* 3.04[pixel]*1.12[um/pixel]*HALL_SCALE_FACTOR */

int32_t lgit_ois_stat(struct msm_ois_ctrl_t *o_ctrl,
					  struct msm_ois_set_info_t *set_info)
{
	struct msm_ois_info_t ois_stat;
	uint16_t hall_x, hall_y  = 0;
	uint16_t gyro_x, gyro_y = 0;
	uint8_t *ptr_dest = NULL;

	CDBG("%s Enter\n", __func__);
	memset(&ois_stat, 0, sizeof(ois_stat));
	lc898122a_ram_mode(ON);
	RamReadA(AD2Z, &gyro_x);
	RamReadA(AD3Z, &gyro_y);
	RamReadA(AD0OFFZ, &hall_x);
	RamReadA(AD1OFFZ, &hall_y);
	lc898122a_ram_mode(OFF);

#if 0
	CDBG("%s gyro_x 0x%x | hall_x 0x%x\n", __func__,
		 (int16_t)gyro_x, (int16_t)hall_x);

	CDBG("%s gyro_y 0x%x | hall_y 0x%x\n", __func__,
		 (int16_t)gyro_y, (int16_t)hall_y);
#endif

	snprintf(ois_stat.ois_provider, ARRAY_SIZE(ois_stat.ois_provider), "LGIT_ROHM");
	ois_stat.gyro[0] = (int16_t)gyro_x;
	ois_stat.gyro[1] = (int16_t)gyro_y;
	ois_stat.hall[0] = (int16_t)hall_x;
	ois_stat.hall[1] = (int16_t)hall_y;
	ois_stat.is_stable = 1;

	ptr_dest = (uint8_t *)set_info->ois_info;
	if (copy_to_user(ptr_dest, &ois_stat, sizeof(ois_stat))) {
		CDBG("%s: failed copy_to_user result\n", __func__);
	}

	CDBG("%s End\n", __func__);
	return OIS_SUCCESS;
}

#define LENS_MOVE_POLLING_LIMIT (10)
#define LENS_MOVE_THRESHOLD     (5) /* um */
#define HALL_LIMIT 8602
int32_t lgit_ois_move_lens(struct msm_ois_ctrl_t *o_ctrl,
						   struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	int16_t offset[2] = {0,};
	uint16_t hall_x, hall_y  = 0;
	int16_t hall_signed_x, hall_signed_y  = 0;
	int16_t target_signed_x, target_signed_y  = 0;
	CDBG("%s Enter\n", __func__);

	if (copy_from_user(&offset[0], set_info->setting, sizeof(int16_t) * 2)) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		rc = -EFAULT;
		return rc;
	}

	CDBG("%s: 0x%x, 0x%x\n", __func__, offset[0], offset[1]);

	lc898122a_ram_mode(ON);
	RamWriteA(0x1450, offset[0]); /* target x position input */
	RamWriteA(0x14D0, offset[1]); /* target x position input */
	usleep(100000);
	RamReadA(0x1440, &hall_x);
	RamReadA(0x14c0, &hall_y);

	hall_signed_x = (int16_t)hall_x;
	hall_signed_y = (int16_t)hall_y;
	target_signed_x = (int16_t)offset[0];
	target_signed_y = (int16_t)offset[1];
#if 0
	CDBG("%s 1 hall_signed_x 0x%x | hall_signed_y 0x%x\n", __func__,
		 hall_signed_x, hall_signed_y);

	CDBG("%s 1 target_signed_x 0x%x | target_signed_y 0x%x\n", __func__,
		 target_signed_x, target_signed_y);
#endif
	lc898122a_ram_mode(OFF);

	rc = OIS_FAIL;
	if (abs(hall_signed_x + target_signed_x) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR ||
		abs(hall_signed_y + target_signed_y) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR)
		rc = OIS_SUCCESS;
	else {
		printk("%s move fail\n", __func__);
		printk("%s LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR: 0x%x\n", __func__,
			   LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR);
		printk("%s hall_signed_x - target_signed_x: 0x%x - 0x%x = 0x%x\n", __func__,
			   hall_signed_x, target_signed_x, hall_signed_x - target_signed_x);
		printk("%s hall_signed_y - target_signed_y: 0x%x - 0x%x = 0x%x\n", __func__,
			   hall_signed_y, target_signed_y, hall_signed_y - target_signed_y);
	}

	CDBG("%s End\n", __func__);
	return rc;
}

void lgit_imx214_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	lgit_ois_func_tbl.ini_set_ois = lgit_init_set_ois;
	lgit_ois_func_tbl.enable_ois = lgit_ois_on;
	lgit_ois_func_tbl.disable_ois = lgit_ois_off;
	lgit_ois_func_tbl.ois_mode = lgit_ois_mode;
	lgit_ois_func_tbl.ois_stat = lgit_ois_stat;
	lgit_ois_func_tbl.ois_move_lens = lgit_ois_move_lens;
	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	msm_ois_t->sid_ois = 0x48 >> 1;
	msm_ois_t->func_tbl = &lgit_ois_func_tbl;
}
