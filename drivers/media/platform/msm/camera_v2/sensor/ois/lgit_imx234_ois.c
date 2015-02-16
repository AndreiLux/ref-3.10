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
#include "lgit_imx234_ois.h"



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
uint8_t spigyrocheck=0x00;


#define	OIS_FW_POLLING_PASS		0
#define	OIS_FW_POLLING_FAIL		-1
#define	OIS_FW_POLLING_VERSION_FAIL		-2
#define	CLRGYR_POLLING_LIMIT_A	6
#define	ACCWIT_POLLING_LIMIT_A	6
#define	INIDGY_POLLING_LIMIT_A	12
#define INIDGY_POLLING_LIMIT_B	12

#define FILREGTAB	7
#define FILRAMTAB	200

//#define	CATCHMODE

#ifdef	CATCHMODE

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/11/28 15:27:49*/
/*FC filename : LC898122_FIL_G4_MTM_V0014_catch*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C011[FILREGTAB]	= {

	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x0F},		/*0F,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C011[FILRAMTAB]	= {

	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3D1E56C0},		/*3D1E56C0,1004,LPF,300Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3D1E56C0},		/*3D1E56C0,1005,LPF,300Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F6C3540},		/*3F6C3540,1006,LPF,300Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x00000000},		/*00000000,1007,Cutoff,invert=0*/
	{0x1008,	0x3F800000},		/*3F800000,1008,0dB,invert=0*/
	{0x1009,	0xBF800000},		/*BF800000,1009,0dB,invert=1*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2AD80},		/*3DA2AD80,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFE00},		/*3F7FFE00,1012,Free,fs/1,invert=0*/
	{0x1013,	0x402C6400},		/*402C6400,1013,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1014,	0xC02A0140},		/*C02A0140,1014,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1015,	0x3F65F240},		/*3F65F240,1015,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1016,	0x3F5C2A00},		/*3F5C2A00,1016,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1017,	0xBF5C1B00},		/*BF5C1B00,1017,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF100},		/*3F7FF100,1018,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1019,	0x3F800000},		/*3F800000,1019,Through,0dB,fs/1,invert=0*/
	{0x101A,	0x00000000},		/*00000000,101A,Through,0dB,fs/1,invert=0*/
	{0x101B,	0x00000000},		/*00000000,101B,Through,0dB,fs/1,invert=0*/
	{0x101C,	0x3F800000},		/*3F800000,101C,0dB,invert=0*/
	{0x101D,	0x00000000},		/*00000000,101D,Cutoff,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x40100000},
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x00000000},		/*00000000,1030,Cutoff,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Cutoff,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Cutoff,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x4F7FFFC0},		/*4F7FFFC0,1036,Free,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Free,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Free,fs/1,invert=0*/
	{0x1039,	0x00000000},		/*00000000,1039,Free,fs/1,invert=0*/
	{0x103A,	0x30800000},		/*30800000,103A,Free,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Free,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3BDA2580},		/*3BDA2580,10A1,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3BDA2580},		/*3BDA2580,10A2,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F7C9780},		/*3F7C9780,10A3,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x00000000},		/*00000000,10A4,Free,fs/1,invert=0*/
	{0x10A5,	0x3A031240},		/*3A031240,10A5,Free,fs/1,invert=0*/
	{0x10A6,	0x3F800000},		/*3F800000,10A6,Free,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0xBF800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1CFA80},		/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA,	0x3C1CFA80},		/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FF040},		/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC,	0x3E300080},		/*3E300080,10CC,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CD,	0xBE2EC780},		/*BE2EC780,10CD,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CE,	0x3F7FA840},		/*3F7FA840,10CE,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41B31900},		/*41B31900,10D7,Through,27dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,27dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,27dB,fs/1,invert=0*/
	{0x10DA,	0x3F6A6D80},		/*3F6A6D80,10DA,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFDF0380},		/*BFDF0380,10DB,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FDF0380},		/*3FDF0380,10DC,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F624D40},		/*3F624D40,10DD,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF4CBAC0},		/*BF4CBAC0,10DE,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10E0,	0x3DF21080},		/*3DF21080,10E0,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DF21080},		/*3DF21080,10E1,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F437BC0},		/*3F437BC0,10E2,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
#ifndef	XY_SIMU_SET
#endif	//XY_SIMU_SET
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x41487EC0},		/*41487EC0,1204,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1205,	0xC1461740},		/*C1461740,1205,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1207,	0x3E0DE280},		/*3E0DE280,1207,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3E0DE280},		/*3E0DE280,1208,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F390EC0},		/*3F390EC0,1209,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3E224B00},		/*3E224B00,120D,DI,-16dB,fs/16,invert=0*/
	{0x120E,	0x00000000},		/*00000000,120E,DI,-16dB,fs/16,invert=0*/
	{0x120F,	0x3F800000},		/*3F800000,120F,DI,-16dB,fs/16,invert=0*/
	{0x1210,	0x3EC662C0},		/*3EC662C0,1210,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1211,	0xBEC4BE80},		/*BEC4BE80,1211,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1212,	0x3F7F96C0},		/*3F7F96C0,1212,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x407ECA00},		/*407ECA00,1215,12dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F7BE9C0},		/*3F7BE9C0,1218,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFF6B800},		/*BFF6B800,1219,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FF6B800},		/*3FF6B800,121A,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F73B380},		/*3F73B380,121B,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF6F9D40},		/*BF6F9D40,121C,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/12/2 13:05:07*/
/*FC filename : LC898122_FIL_G4_MTM_V000F_catch*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C010[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x00},		/*00,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C010[FILRAMTAB]	= {

	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3D04CE00},		/*3D04CE00,1004,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3D04CE00},		/*3D04CE00,1005,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F6F6640},		/*3F6F6640,1006,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x00000000},		/*00000000,1007,Cutoff,invert=0*/
	{0x1008,	0x3F800000},		/*3F800000,1008,0dB,invert=0*/
	{0x1009,	0xBF800000},		/*BF800000,1009,0dB,invert=1*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2AD80},		/*3DA2AD80,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFE00},		/*3F7FFE00,1012,Free,fs/1,invert=0*/
	{0x1013,	0x402C6400},		/*402C6400,1013,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1014,	0xC02A0140},		/*C02A0140,1014,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1015,	0x3F65F240},		/*3F65F240,1015,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1016,	0x3F5C2A00},		/*3F5C2A00,1016,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1017,	0xBF5C1B00},		/*BF5C1B00,1017,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF100},		/*3F7FF100,1018,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1019,	0x3F800000},		/*3F800000,1019,Through,0dB,fs/1,invert=0*/
	{0x101A,	0x00000000},		/*00000000,101A,Through,0dB,fs/1,invert=0*/
	{0x101B,	0x00000000},		/*00000000,101B,Through,0dB,fs/1,invert=0*/
	{0x101C,	0x3F800000},		/*3F800000,101C,0dB,invert=0*/
	{0x101D,	0x00000000},		/*00000000,101D,Cutoff,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x40100000},
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x00000000},		/*00000000,1030,Cutoff,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Cutoff,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Cutoff,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x4F7FFFC0},		/*4F7FFFC0,1036,Free,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Free,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Free,fs/1,invert=0*/
	{0x1039,	0x00000000},		/*00000000,1039,Free,fs/1,invert=0*/
	{0x103A,	0x30800000},		/*30800000,103A,Free,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Free,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3BDA2580},		/*3BDA2580,10A1,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3BDA2580},		/*3BDA2580,10A2,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F7C9780},		/*3F7C9780,10A3,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x00000000},		/*00000000,10A4,Free,fs/1,invert=0*/
	{0x10A5,	0x3A031240},		/*3A031240,10A5,Free,fs/1,invert=0*/
	{0x10A6,	0x3F800000},		/*3F800000,10A6,Free,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0xBF800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1CFA80},		/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA,	0x3C1CFA80},		/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FF040},		/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC,	0x3E0BCD80},		/*3E0BCD80,10CC,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10CD,	0xBE0AD500},		/*BE0AD500,10CD,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10CE,	0x3F7FA840},		/*3F7FA840,10CE,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41B31900},		/*41B31900,10D7,Through,27dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,27dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,27dB,fs/1,invert=0*/
	{0x10DA,	0x3F64D080},		/*3F64D080,10DA,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFCCC8C0},		/*BFCCC8C0,10DB,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FCCC8C0},		/*3FCCC8C0,10DC,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F574300},		/*3F574300,10DD,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF3C1380},		/*BF3C1380,10DE,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10E0,	0x3DF21080},		/*3DF21080,10E0,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DF21080},		/*3DF21080,10E1,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F437BC0},		/*3F437BC0,10E2,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
#ifndef	XY_SIMU_SET
#endif	//XY_SIMU_SET
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x4148A100},		/*4148A100,1204,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1205,	0xC145F500},		/*C145F500,1205,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1207,	0x3E0DE280},		/*3E0DE280,1207,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3E0DE280},		/*3E0DE280,1208,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F390EC0},		/*3F390EC0,1209,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3BF94A80},		/*3BF94A80,120D,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x120E,	0x3BF94A80},		/*3BF94A80,120E,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x120F,	0x3F7FE400},		/*3F7FE400,120F,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x1210,	0x3EB4F440},		/*3EB4F440,1210,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1211,	0xBEB35000},		/*BEB35000,1211,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1212,	0x3F7F96C0},		/*3F7F96C0,1212,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x407ECA00},		/*407ECA00,1215,12dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F7A4100},		/*3F7A4100,1218,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFF24B00},		/*BFF24B00,1219,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FF24B00},		/*3FF24B00,121A,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F6EB4C0},		/*3F6EB4C0,121B,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF68F580},		/*BF68F580,121C,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/12/9 19:22:17*/
/*FC filename : LC898122_FIL_G4_V0020_4th_catch*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C002[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x0F},		/*0F,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C002[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3D04CE00},		/*3D04CE00,1004,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3D04CE00},		/*3D04CE00,1005,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F6F6640},		/*3F6F6640,1006,LPF,250Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x00000000},		/*00000000,1007,Cutoff,invert=0*/
	{0x1008,	0x3F800000},		/*3F800000,1008,0dB,invert=0*/
	{0x1009,	0xBF800000},		/*BF800000,1009,0dB,invert=1*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2AD80},		/*3DA2AD80,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFE00},		/*3F7FFE00,1012,Free,fs/1,invert=0*/
	{0x1013,	0x402C6400},		/*402C6400,1013,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1014,	0xC02A0140},		/*C02A0140,1014,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1015,	0x3F65F240},		/*3F65F240,1015,HBF,52Hz,400Hz,9dB,fs/1,invert=0*/
	{0x1016,	0x3F5C2A00},		/*3F5C2A00,1016,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1017,	0xBF5C1B00},		/*BF5C1B00,1017,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF100},		/*3F7FF100,1018,LBF,0.86Hz,1Hz,0dB,fs/1,invert=0*/
	{0x1019,	0x3F800000},		/*3F800000,1019,Through,0dB,fs/1,invert=0*/
	{0x101A,	0x00000000},		/*00000000,101A,Through,0dB,fs/1,invert=0*/
	{0x101B,	0x00000000},		/*00000000,101B,Through,0dB,fs/1,invert=0*/
	{0x101C,	0x3F800000},		/*3F800000,101C,0dB,invert=0*/
	{0x101D,	0x00000000},		/*00000000,101D,Cutoff,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x40100000},
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x00000000},		/*00000000,1030,Cutoff,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Cutoff,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Cutoff,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x4F7FFFC0},		/*4F7FFFC0,1036,Free,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Free,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Free,fs/1,invert=0*/
	{0x1039,	0x00000000},		/*00000000,1039,Free,fs/1,invert=0*/
	{0x103A,	0x30800000},		/*30800000,103A,Free,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Free,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3BDA2580},		/*3BDA2580,10A1,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3BDA2580},		/*3BDA2580,10A2,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F7C9780},		/*3F7C9780,10A3,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x00000000},		/*00000000,10A4,Free,fs/1,invert=0*/
	{0x10A5,	0x3A031240},		/*3A031240,10A5,Free,fs/1,invert=0*/
	{0x10A6,	0x3F800000},		/*3F800000,10A6,Free,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C765F40},		/*3C765F40,10C9,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CA,	0x3C765F40},		/*3C765F40,10CA,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FEE80},		/*3F7FEE80,10CB,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CC,	0x3DDA0F40},		/*3DDA0F40,10CC,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10CD,	0xBDD85040},		/*BDD85040,10CD,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10CE,	0x3F7F7380},		/*3F7F7380,10CE,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41FCFB80},		/*41FCFB80,10D7,Through,30dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,30dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,30dB,fs/1,invert=0*/
	{0x10DA,	0x3F71CBC0},		/*3F71CBC0,10DA,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DB,	0xBFDC4380},		/*BFDC4380,10DB,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DC,	0x3FDC4380},		/*3FDC4380,10DC,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DD,	0x3F554080},		/*3F554080,10DD,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DE,	0xBF470C40},		/*BF470C40,10DE,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10E0,	0x3DC65740},		/*3DC65740,10E0,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DC65740},		/*3DC65740,10E1,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F4E6A40},		/*3F4E6A40,10E2,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
#ifndef	XY_SIMU_SET
#endif	//XY_SIMU_SET
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x410E08C0},		/*410E08C0,1204,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1205,	0xC10C24C0},		/*C10C24C0,1205,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1207,	0x3DF21080},		/*3DF21080,1207,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3DF21080},		/*3DF21080,1208,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F437BC0},		/*3F437BC0,1209,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3E653EC0},		/*3E653EC0,120D,DI,-13dB,fs/16,invert=0*/
	{0x120E,	0x00000000},		/*00000000,120E,DI,-13dB,fs/16,invert=0*/
	{0x120F,	0x3F800000},		/*3F800000,120F,DI,-13dB,fs/16,invert=0*/
	{0x1210,	0x3EE2AC00},		/*3EE2AC00,1210,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1211,	0xBEE0BC40},		/*BEE0BC40,1211,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1212,	0x3F7F5080},		/*3F7F5080,1212,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x40346080},		/*40346080,1215,9dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F769A40},		/*3F769A40,1218,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFE71540},		/*BFE71540,1219,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FE71540},		/*3FE71540,121A,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F63B800},		/*3F63B800,121B,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF5A5280},		/*BF5A5280,121C,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

#else	//CATCHMODE
/*Filter Calculator Version 4.02*/
/*the time and date : 2014/11/28 11:48:54*/
/*FC filename : LC898122_FIL_G4_MTM_V0013*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C011[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x0F},		/*0F,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C011[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x37A8A800},		/*37A8A800,1004,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x37A8A800},		/*37A8A800,1005,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F7FFD40},		/*3F7FFD40,1006,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x3F800000},		/*3F800000,1007,0dB,invert=0*/
	{0x1008,	0xBF800000},		/*BF800000,1008,0dB,invert=1*/
	{0x1009,	0x3F800000},		/*3F800000,1009,0dB,invert=0*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2ADC0},		/*3DA2ADC0,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFD80},		/*3F7FFD80,1012,Free,fs/1,invert=0*/
	{0x1013,	0x3F772680},		/*3F772680,1013,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1014,	0xBF71E800},		/*BF71E800,1014,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1015,	0x3F690E80},		/*3F690E80,1015,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1016,	0x40FE2AC0},		/*40FE2AC0,1016,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1017,	0xC0FE2AC0},		/*C0FE2AC0,1017,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF680},		/*3F7FF680,1018,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1019,	0x3ED1D880},		/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101A,	0xBED1CFC0},		/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101B,	0x3F7FFB80},		/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101C,	0x00000000},		/*00000000,101C,Cutoff,invert=0*/
	{0x101D,	0x3F800000},		/*3F800000,101D,0dB,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x3F800000},		/*3F800000,1021,0dB,invert=0*/
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x3F800000},		/*3F800000,1030,Through,0dB,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Through,0dB,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Through,0dB,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x3F800000},		/*3F800000,1036,Through,0dB,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Through,0dB,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Through,0dB,fs/1,invert=0*/
	{0x1039,	0x3F800000},		/*3F800000,1039,Through,0dB,fs/1,invert=0*/
	{0x103A,	0x00000000},		/*00000000,103A,Through,0dB,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Through,0dB,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3C58B440},		/*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3C58B440},		/*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F793A40},		/*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x3C58B440},		/*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A5,	0x3C58B440},		/*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A6,	0x3F793A40},		/*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1CFA80},		/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA,	0x3C1CFA80},		/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FF040},		/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC,	0x3E300080},		/*3E300080,10CC,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CD,	0xBE2EC780},		/*BE2EC780,10CD,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10CE,	0x3F7FA840},		/*3F7FA840,10CE,LBF,5Hz,26Hz,-1dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41B31900},		/*41B31900,10D7,Through,27dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,27dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,27dB,fs/1,invert=0*/
	{0x10DA,	0x3F6A6D80},		/*3F6A6D80,10DA,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFDF0380},		/*BFDF0380,10DB,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FDF0380},		/*3FDF0380,10DC,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F624D40},		/*3F624D40,10DD,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF4CBAC0},		/*BF4CBAC0,10DE,PKF,950Hz,-16dB,5,fs/1,invert=0*/
	{0x10E0,	0x3DF21080},		/*3DF21080,10E0,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DF21080},		/*3DF21080,10E1,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F437BC0},		/*3F437BC0,10E2,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x41487EC0},		/*41487EC0,1204,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1205,	0xC1461740},		/*C1461740,1205,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,45Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1207,	0x3E0DE280},		/*3E0DE280,1207,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3E0DE280},		/*3E0DE280,1208,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F390EC0},		/*3F390EC0,1209,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3E224B00},		/*3E224B00,120D,DI,-16dB,fs/16,invert=0*/
	{0x120E,	0x00000000},		/*00000000,120E,DI,-16dB,fs/16,invert=0*/
	{0x120F,	0x3F800000},		/*3F800000,120F,DI,-16dB,fs/16,invert=0*/
	{0x1210,	0x3EC662C0},		/*3EC662C0,1210,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1211,	0xBEC4BE80},		/*BEC4BE80,1211,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1212,	0x3F7F96C0},		/*3F7F96C0,1212,LBF,6Hz,31Hz,6dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x407ECA00},		/*407ECA00,1215,12dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F7BE9C0},		/*3F7BE9C0,1218,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFF6B800},		/*BFF6B800,1219,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FF6B800},		/*3FF6B800,121A,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F73B380},		/*3F73B380,121B,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF6F9D40},		/*BF6F9D40,121C,PKF,350Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/11/6 19:27:18*/
/*FC filename : LC898122_FIL_G4_MTM_V000F*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C010[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x00},		/*00,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C010[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3760E040},		/*3760E040,1004,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3760E040},		/*3760E040,1005,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F7FFE40},		/*3F7FFE40,1006,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x3F800000},		/*3F800000,1007,0dB,invert=0*/
	{0x1008,	0xBF800000},		/*BF800000,1008,0dB,invert=1*/
	{0x1009,	0x3F800000},		/*3F800000,1009,0dB,invert=0*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2ADC0},		/*3DA2ADC0,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFD80},		/*3F7FFD80,1012,Free,fs/1,invert=0*/
	{0x1013,	0x3F772680},		/*3F772680,1013,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1014,	0xBF71E800},		/*BF71E800,1014,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1015,	0x3F690E80},		/*3F690E80,1015,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1016,	0x40FE2AC0},		/*40FE2AC0,1016,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1017,	0xC0FE2AC0},		/*C0FE2AC0,1017,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF680},		/*3F7FF680,1018,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1019,	0x3ED1D880},		/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101A,	0xBED1CFC0},		/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101B,	0x3F7FFB80},		/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101C,	0x00000000},		/*00000000,101C,Cutoff,invert=0*/
	{0x101D,	0x3F800000},		/*3F800000,101D,0dB,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x3F800000},		/*3F800000,1021,0dB,invert=0*/
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x3F800000},		/*3F800000,1030,Through,0dB,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Through,0dB,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Through,0dB,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x3F800000},		/*3F800000,1036,Through,0dB,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Through,0dB,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Through,0dB,fs/1,invert=0*/
	{0x1039,	0x3F800000},		/*3F800000,1039,Through,0dB,fs/1,invert=0*/
	{0x103A,	0x00000000},		/*00000000,103A,Through,0dB,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Through,0dB,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3C58B440},		/*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3C58B440},		/*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F793A40},		/*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x3C58B440},		/*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A5,	0x3C58B440},		/*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A6,	0x3F793A40},		/*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1CFA80},		/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA,	0x3C1CFA80},		/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FF040},		/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC,	0x3E0BCD80},		/*3E0BCD80,10CC,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10CD,	0xBE0AD500},		/*BE0AD500,10CD,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10CE,	0x3F7FA840},		/*3F7FA840,10CE,LBF,5Hz,26Hz,-3dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41B31900},		/*41B31900,10D7,Through,27dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,27dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,27dB,fs/1,invert=0*/
	{0x10DA,	0x3F64D080},		/*3F64D080,10DA,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFCCC8C0},		/*BFCCC8C0,10DB,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FCCC8C0},		/*3FCCC8C0,10DC,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F574300},		/*3F574300,10DD,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF3C1380},		/*BF3C1380,10DE,PKF,1500Hz,-14dB,5,fs/1,invert=0*/
	{0x10E0,	0x3DF21080},		/*3DF21080,10E0,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DF21080},		/*3DF21080,10E1,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F437BC0},		/*3F437BC0,10E2,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x4148A100},		/*4148A100,1204,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1205,	0xC145F500},		/*C145F500,1205,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,23dB,fs/1,invert=0*/
	{0x1207,	0x3E0DE280},		/*3E0DE280,1207,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3E0DE280},		/*3E0DE280,1208,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F390EC0},		/*3F390EC0,1209,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3BF94A80},		/*3BF94A80,120D,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x120E,	0x3BF94A80},		/*3BF94A80,120E,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x120F,	0x3F7FE400},		/*3F7FE400,120F,LPF,1.6Hz,31dB,fs/1,invert=0*/
	{0x1210,	0x3EB4F440},		/*3EB4F440,1210,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1211,	0xBEB35000},		/*BEB35000,1211,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1212,	0x3F7F96C0},		/*3F7F96C0,1212,LBF,6Hz,34Hz,6dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x407ECA00},		/*407ECA00,1215,12dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F7A4100},		/*3F7A4100,1218,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFF24B00},		/*BFF24B00,1219,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FF24B00},		/*3FF24B00,121A,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F6EB4C0},		/*3F6EB4C0,121B,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF68F580},		/*BF68F580,121C,PKF,500Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/12/8 14:49:38*/
/*FC filename : LC898122_FIL_G4_V0019_4th*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C002[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x0F},		/*0F,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C002[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x37A8A800},		/*37A8A800,1004,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x37A8A800},		/*37A8A800,1005,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F7FFD40},		/*3F7FFD40,1006,LPF,0.15Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x3F800000},		/*3F800000,1007,0dB,invert=0*/
	{0x1008,	0xBF800000},		/*BF800000,1008,0dB,invert=1*/
	{0x1009,	0x3F800000},		/*3F800000,1009,0dB,invert=0*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2ADC0},		/*3DA2ADC0,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFD80},		/*3F7FFD80,1012,Free,fs/1,invert=0*/
	{0x1013,	0x3F772680},		/*3F772680,1013,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1014,	0xBF71E800},		/*BF71E800,1014,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1015,	0x3F690E80},		/*3F690E80,1015,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1016,	0x40FE2AC0},		/*40FE2AC0,1016,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1017,	0xC0FE2AC0},		/*C0FE2AC0,1017,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF680},		/*3F7FF680,1018,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1019,	0x3ED1D880},		/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101A,	0xBED1CFC0},		/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101B,	0x3F7FFB80},		/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101C,	0x00000000},		/*00000000,101C,Cutoff,invert=0*/
	{0x101D,	0x3F800000},		/*3F800000,101D,0dB,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x3F800000},		/*3F800000,1021,0dB,invert=0*/
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x3F800000},		/*3F800000,1030,Through,0dB,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Through,0dB,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Through,0dB,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x3F800000},		/*3F800000,1036,Through,0dB,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Through,0dB,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Through,0dB,fs/1,invert=0*/
	{0x1039,	0x3F800000},		/*3F800000,1039,Through,0dB,fs/1,invert=0*/
	{0x103A,	0x00000000},		/*00000000,103A,Through,0dB,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Through,0dB,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3C58B440},		/*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3C58B440},		/*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F793A40},		/*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x3C58B440},		/*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A5,	0x3C58B440},		/*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A6,	0x3F793A40},		/*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C765F40},		/*3C765F40,10C9,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CA,	0x3C765F40},		/*3C765F40,10CA,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FEE80},		/*3F7FEE80,10CB,LPF,1Hz,41dB,fs/1,invert=0*/
	{0x10CC,	0x3DDA0F40},		/*3DDA0F40,10CC,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10CD,	0xBDD85040},		/*BDD85040,10CD,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10CE,	0x3F7F7380},		/*3F7F7380,10CE,LBF,8Hz,30Hz,-8dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41FCFB80},		/*41FCFB80,10D7,Through,30dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,30dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,30dB,fs/1,invert=0*/
	{0x10DA,	0x3F71CBC0},		/*3F71CBC0,10DA,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DB,	0xBFDC4380},		/*BFDC4380,10DB,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DC,	0x3FDC4380},		/*3FDC4380,10DC,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DD,	0x3F554080},		/*3F554080,10DD,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10DE,	0xBF470C40},		/*BF470C40,10DE,PKF,950Hz,-6dB,3,fs/1,invert=0*/
	{0x10E0,	0x3DC65740},		/*3DC65740,10E0,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DC65740},		/*3DC65740,10E1,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F4E6A40},		/*3F4E6A40,10E2,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,800Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x410E08C0},		/*410E08C0,1204,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1205,	0xC10C24C0},		/*C10C24C0,1205,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1207,	0x3DF21080},		/*3DF21080,1207,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3DF21080},		/*3DF21080,1208,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F437BC0},		/*3F437BC0,1209,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3E653EC0},		/*3E653EC0,120D,DI,-13dB,fs/16,invert=0*/
	{0x120E,	0x00000000},		/*00000000,120E,DI,-13dB,fs/16,invert=0*/
	{0x120F,	0x3F800000},		/*3F800000,120F,DI,-13dB,fs/16,invert=0*/
	{0x1210,	0x3EE2AC00},		/*3EE2AC00,1210,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1211,	0xBEE0BC40},		/*BEE0BC40,1211,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1212,	0x3F7F5080},		/*3F7F5080,1212,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x40346080},		/*40346080,1215,9dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F769A40},		/*3F769A40,1218,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x1219,	0xBFE71540},		/*BFE71540,1219,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121A,	0x3FE71540},		/*3FE71540,121A,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121B,	0x3F63B800},		/*3F63B800,121B,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121C,	0xBF5A5280},		/*BF5A5280,121C,PKF,850Hz,-6dB,4,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};
#endif	//CATCHMODE

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/11/7 20:52:47*/
/*FC filename : LC898122_FIL_G4_V0017_Third*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C001[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x0F},		/*0F,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

/* 32bit */
const struct STFILRAM	CsFilRam_C001[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3760E040},		/*3760E040,1004,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3760E040},		/*3760E040,1005,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F7FFE40},		/*3F7FFE40,1006,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x3F800000},		/*3F800000,1007,0dB,invert=0*/
	{0x1008,	0xBF800000},		/*BF800000,1008,0dB,invert=1*/
	{0x1009,	0x3F800000},		/*3F800000,1009,0dB,invert=0*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2ADC0},		/*3DA2ADC0,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFD80},		/*3F7FFD80,1012,Free,fs/1,invert=0*/
	{0x1013,	0x3F772680},		/*3F772680,1013,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1014,	0xBF71E800},		/*BF71E800,1014,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1015,	0x3F690E80},		/*3F690E80,1015,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1016,	0x40FE2AC0},		/*40FE2AC0,1016,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1017,	0xC0FE2AC0},		/*C0FE2AC0,1017,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF680},		/*3F7FF680,1018,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1019,	0x3ED1D880},		/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101A,	0xBED1CFC0},		/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101B,	0x3F7FFB80},		/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101C,	0x00000000},		/*00000000,101C,Cutoff,invert=0*/
	{0x101D,	0x3F800000},		/*3F800000,101D,0dB,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x3F800000},		/*3F800000,1021,0dB,invert=0*/
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x3F800000},		/*3F800000,1030,Through,0dB,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Through,0dB,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Through,0dB,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x3F800000},		/*3F800000,1036,Through,0dB,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Through,0dB,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Through,0dB,fs/1,invert=0*/
	{0x1039,	0x3F800000},		/*3F800000,1039,Through,0dB,fs/1,invert=0*/
	{0x103A,	0x00000000},		/*00000000,103A,Through,0dB,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Through,0dB,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3C58B440},		/*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3C58B440},		/*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F793A40},		/*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x3C58B440},		/*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A5,	0x3C58B440},		/*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A6,	0x3F793A40},		/*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FD13600},		/*3FD13600,10C0,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCEFAC0},		/*BFCEFAC0,10C1,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F5414C0},		/*3F5414C0,10C2,HBF,40Hz,700Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1CFA80},		/*3C1CFA80,10C9,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CA,	0x3C1CFA80},		/*3C1CFA80,10CA,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FF040},		/*3F7FF040,10CB,LPF,0.9Hz,38dB,fs/1,invert=0*/
	{0x10CC,	0x3E1D2100},		/*3E1D2100,10CC,LBF,10Hz,26Hz,-8dB,fs/1,invert=0*/
	{0x10CD,	0xBE1C0980},		/*BE1C0980,10CD,LBF,10Hz,26Hz,-8dB,fs/1,invert=0*/
	{0x10CE,	0x3F7F5080},		/*3F7F5080,10CE,LBF,10Hz,26Hz,-8dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x00000000},		/*00000000,10D1,Cutoff,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41FCFB80},		/*41FCFB80,10D7,Through,30dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,30dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,30dB,fs/1,invert=0*/
	{0x10DA,	0x3F793580},		/*3F793580,10DA,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFEC2C40},		/*BFEC2C40,10DB,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FEC2C40},		/*3FEC2C40,10DC,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F6B8FC0},		/*3F6B8FC0,10DD,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF64C540},		/*BF64C540,10DE,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10E0,	0x3E0DE280},		/*3E0DE280,10E0,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3E0DE280},		/*3E0DE280,10E1,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F390EC0},		/*3F390EC0,10E2,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1200Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x410E08C0},		/*410E08C0,1204,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1205,	0xC10C24C0},		/*C10C24C0,1205,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1207,	0x3DF21080},		/*3DF21080,1207,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3DF21080},		/*3DF21080,1208,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F437BC0},		/*3F437BC0,1209,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3E653EC0},		/*3E653EC0,120D,DI,-13dB,fs/16,invert=0*/
	{0x120E,	0x00000000},		/*00000000,120E,DI,-13dB,fs/16,invert=0*/
	{0x120F,	0x3F800000},		/*3F800000,120F,DI,-13dB,fs/16,invert=0*/
	{0x1210,	0x3EE2AC00},		/*3EE2AC00,1210,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1211,	0xBEE0BC40},		/*BEE0BC40,1211,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1212,	0x3F7F5080},		/*3F7F5080,1212,LBF,10Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x40346080},		/*40346080,1215,9dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F7AB6C0},		/*3F7AB6C0,1218,PKF,650Hz,-6dB,5,fs/1,invert=0*/
	{0x1219,	0xBFF1B480},		/*BFF1B480,1219,PKF,650Hz,-6dB,5,fs/1,invert=0*/
	{0x121A,	0x3FF1B480},		/*3FF1B480,121A,PKF,650Hz,-6dB,5,fs/1,invert=0*/
	{0x121B,	0x3F701780},		/*3F701780,121B,PKF,650Hz,-6dB,5,fs/1,invert=0*/
	{0x121C,	0xBF6ACE40},		/*BF6ACE40,121C,PKF,650Hz,-6dB,5,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

/*Filter Calculator Version 4.02*/
/*the time and date : 2014/11/7 20:51:29*/
/*FC filename : LC898122_FIL_G4_V0017_First*/
/*fs,23438Hz*/
/*LSI No.,LC898122*/
/*Comment,*/

/* 8bit */
const struct STFILREG	CsFilReg_C000[FILREGTAB]	= {
	{0x0111,	0x00},		/*00,0111*/
	{0x0113,	0x00},		/*00,0113*/
	{0x0114,	0x00},		/*00,0114*/
	{0x0172,	0x00},		/*00,0172*/
	{0x01E3,	0x00},		/*00,01E3*/
	{0x01E4,	0x00},		/*00,01E4*/
	{0xFFFF,	0xFF}
};

const struct STFILRAM	CsFilRam_C000[FILRAMTAB] = {
	{0x1000,	0x3F800000},		/*3F800000,1000,0dB,invert=0*/
	{0x1001,	0x3F800000},		/*3F800000,1001,0dB,invert=0*/
	{0x1002,	0x00000000},		/*00000000,1002,Cutoff,invert=0*/
	{0x1003,	0x3F800000},		/*3F800000,1003,0dB,invert=0*/
	{0x1004,	0x3760E040},		/*3760E040,1004,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1005,	0x3760E040},		/*3760E040,1005,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1006,	0x3F7FFE40},		/*3F7FFE40,1006,LPF,0.1Hz,0dB,fs/1,invert=0*/
	{0x1007,	0x3F800000},		/*3F800000,1007,0dB,invert=0*/
	{0x1008,	0xBF800000},		/*BF800000,1008,0dB,invert=1*/
	{0x1009,	0x3F800000},		/*3F800000,1009,0dB,invert=0*/
	{0x100A,	0x3F800000},		/*3F800000,100A,0dB,invert=0*/
	{0x100B,	0x3F800000},		/*3F800000,100B,0dB,invert=0*/
	{0x100C,	0x3F800000},		/*3F800000,100C,0dB,invert=0*/
	{0x100E,	0x3F800000},		/*3F800000,100E,0dB,invert=0*/
	{0x1010,	0x3DA2ADC0},		/*3DA2ADC0,1010*/
	{0x1011,	0x00000000},		/*00000000,1011,Free,fs/1,invert=0*/
	{0x1012,	0x3F7FFD80},		/*3F7FFD80,1012,Free,fs/1,invert=0*/
	{0x1013,	0x3F772680},		/*3F772680,1013,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1014,	0xBF71E800},		/*BF71E800,1014,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1015,	0x3F690E80},		/*3F690E80,1015,HBF,80Hz,350Hz,0dB,fs/1,invert=0*/
	{0x1016,	0x40FE2AC0},		/*40FE2AC0,1016,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1017,	0xC0FE2AC0},		/*C0FE2AC0,1017,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1018,	0x3F7FF680},		/*3F7FF680,1018,HPF,0.54Hz,18dB,fs/1,invert=0*/
	{0x1019,	0x3ED1D880},		/*3ED1D880,1019,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101A,	0xBED1CFC0},		/*BED1CFC0,101A,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101B,	0x3F7FFB80},		/*3F7FFB80,101B,LBF,0.25Hz,0.61Hz,0dB,fs/1,invert=0*/
	{0x101C,	0x00000000},		/*00000000,101C,Cutoff,invert=0*/
	{0x101D,	0x3F800000},		/*3F800000,101D,0dB,invert=0*/
	{0x101E,	0x3F800000},		/*3F800000,101E,0dB,invert=0*/
	{0x1020,	0x3F800000},		/*3F800000,1020,0dB,invert=0*/
	{0x1021,	0x3F800000},		/*3F800000,1021,0dB,invert=0*/
	{0x1022,	0x3F800000},		/*3F800000,1022,0dB,invert=0*/
	{0x1023,	0x3F800000},		/*3F800000,1023,Through,0dB,fs/1,invert=0*/
	{0x1024,	0x00000000},		/*00000000,1024,Through,0dB,fs/1,invert=0*/
	{0x1025,	0x00000000},		/*00000000,1025,Through,0dB,fs/1,invert=0*/
	{0x1026,	0x00000000},		/*00000000,1026,Through,0dB,fs/1,invert=0*/
	{0x1027,	0x00000000},		/*00000000,1027,Through,0dB,fs/1,invert=0*/
	{0x1030,	0x3F800000},		/*3F800000,1030,Through,0dB,fs/1,invert=0*/
	{0x1031,	0x00000000},		/*00000000,1031,Through,0dB,fs/1,invert=0*/
	{0x1032,	0x00000000},		/*00000000,1032,Through,0dB,fs/1,invert=0*/
	{0x1033,	0x3F800000},		/*3F800000,1033,Through,0dB,fs/1,invert=0*/
	{0x1034,	0x00000000},		/*00000000,1034,Through,0dB,fs/1,invert=0*/
	{0x1035,	0x00000000},		/*00000000,1035,Through,0dB,fs/1,invert=0*/
	{0x1036,	0x3F800000},		/*3F800000,1036,Through,0dB,fs/1,invert=0*/
	{0x1037,	0x00000000},		/*00000000,1037,Through,0dB,fs/1,invert=0*/
	{0x1038,	0x00000000},		/*00000000,1038,Through,0dB,fs/1,invert=0*/
	{0x1039,	0x3F800000},		/*3F800000,1039,Through,0dB,fs/1,invert=0*/
	{0x103A,	0x00000000},		/*00000000,103A,Through,0dB,fs/1,invert=0*/
	{0x103B,	0x00000000},		/*00000000,103B,Through,0dB,fs/1,invert=0*/
	{0x103C,	0x3F800000},		/*3F800000,103C,Through,0dB,fs/1,invert=0*/
	{0x103D,	0x00000000},		/*00000000,103D,Through,0dB,fs/1,invert=0*/
	{0x103E,	0x00000000},		/*00000000,103E,Through,0dB,fs/1,invert=0*/
	{0x1043,	0x39D2BD40},		/*39D2BD40,1043,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1044,	0x39D2BD40},		/*39D2BD40,1044,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1045,	0x3F7FCB40},		/*3F7FCB40,1045,LPF,3Hz,0dB,fs/1,invert=0*/
	{0x1046,	0x388C8A40},		/*388C8A40,1046,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1047,	0x388C8A40},		/*388C8A40,1047,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1048,	0x3F7FF740},		/*3F7FF740,1048,LPF,0.5Hz,0dB,fs/1,invert=0*/
	{0x1049,	0x390C87C0},		/*390C87C0,1049,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104A,	0x390C87C0},		/*390C87C0,104A,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104B,	0x3F7FEE80},		/*3F7FEE80,104B,LPF,1Hz,0dB,fs/1,invert=0*/
	{0x104C,	0x398C8300},		/*398C8300,104C,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104D,	0x398C8300},		/*398C8300,104D,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x104E,	0x3F7FDCC0},		/*3F7FDCC0,104E,LPF,2Hz,0dB,fs/1,invert=0*/
	{0x1053,	0x3F800000},		/*3F800000,1053,Through,0dB,fs/1,invert=0*/
	{0x1054,	0x00000000},		/*00000000,1054,Through,0dB,fs/1,invert=0*/
	{0x1055,	0x00000000},		/*00000000,1055,Through,0dB,fs/1,invert=0*/
	{0x1056,	0x3F800000},		/*3F800000,1056,Through,0dB,fs/1,invert=0*/
	{0x1057,	0x00000000},		/*00000000,1057,Through,0dB,fs/1,invert=0*/
	{0x1058,	0x00000000},		/*00000000,1058,Through,0dB,fs/1,invert=0*/
	{0x1059,	0x3F800000},		/*3F800000,1059,Through,0dB,fs/1,invert=0*/
	{0x105A,	0x00000000},		/*00000000,105A,Through,0dB,fs/1,invert=0*/
	{0x105B,	0x00000000},		/*00000000,105B,Through,0dB,fs/1,invert=0*/
	{0x105C,	0x3F800000},		/*3F800000,105C,Through,0dB,fs/1,invert=0*/
	{0x105D,	0x00000000},		/*00000000,105D,Through,0dB,fs/1,invert=0*/
	{0x105E,	0x00000000},		/*00000000,105E,Through,0dB,fs/1,invert=0*/
	{0x1063,	0x3F800000},		/*3F800000,1063,0dB,invert=0*/
	{0x1066,	0x3F800000},		/*3F800000,1066,0dB,invert=0*/
	{0x1069,	0x3F800000},		/*3F800000,1069,0dB,invert=0*/
	{0x106C,	0x3F800000},		/*3F800000,106C,0dB,invert=0*/
	{0x1073,	0x00000000},		/*00000000,1073,Cutoff,invert=0*/
	{0x1076,	0x3F800000},		/*3F800000,1076,0dB,invert=0*/
	{0x1079,	0x3F800000},		/*3F800000,1079,0dB,invert=0*/
	{0x107C,	0x3F800000},		/*3F800000,107C,0dB,invert=0*/
	{0x1083,	0x38D1B700},		/*38D1B700,1083,-80dB,invert=0*/
	{0x1086,	0x00000000},		/*00000000,1086,Cutoff,invert=0*/
	{0x1089,	0x00000000},		/*00000000,1089,Cutoff,invert=0*/
	{0x108C,	0x00000000},		/*00000000,108C,Cutoff,invert=0*/
	{0x1093,	0x00000000},		/*00000000,1093,Cutoff,invert=0*/
	{0x1098,	0x3F800000},		/*3F800000,1098,0dB,invert=0*/
	{0x1099,	0x3F800000},		/*3F800000,1099,0dB,invert=0*/
	{0x109A,	0x3F800000},		/*3F800000,109A,0dB,invert=0*/
	{0x10A1,	0x3C58B440},		/*3C58B440,10A1,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A2,	0x3C58B440},		/*3C58B440,10A2,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A3,	0x3F793A40},		/*3F793A40,10A3,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A4,	0x3C58B440},		/*3C58B440,10A4,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A5,	0x3C58B440},		/*3C58B440,10A5,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A6,	0x3F793A40},		/*3F793A40,10A6,LPF,100Hz,0dB,fs/1,invert=0*/
	{0x10A7,	0x3F800000},		/*3F800000,10A7,Through,0dB,fs/1,invert=0*/
	{0x10A8,	0x00000000},		/*00000000,10A8,Through,0dB,fs/1,invert=0*/
	{0x10A9,	0x00000000},		/*00000000,10A9,Through,0dB,fs/1,invert=0*/
	{0x10AA,	0x00000000},		/*00000000,10AA,Cutoff,invert=0*/
	{0x10AB,	0x3BDA2580},		/*3BDA2580,10AB,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AC,	0x3BDA2580},		/*3BDA2580,10AC,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10AD,	0x3F7C9780},		/*3F7C9780,10AD,LPF,50Hz,0dB,fs/1,invert=0*/
	{0x10B0,	0x3F800000},		/*3F800000,10B0,Through,0dB,fs/1,invert=0*/
	{0x10B1,	0x00000000},		/*00000000,10B1,Through,0dB,fs/1,invert=0*/
	{0x10B2,	0x00000000},		/*00000000,10B2,Through,0dB,fs/1,invert=0*/
	{0x10B3,	0x3F800000},		/*3F800000,10B3,0dB,invert=0*/
	{0x10B4,	0x00000000},		/*00000000,10B4,Cutoff,invert=0*/
	{0x10B5,	0x00000000},		/*00000000,10B5,Cutoff,invert=0*/
	{0x10B6,	0x3F353C00},		/*3F353C00,10B6,-3dB,invert=0*/
	{0x10B8,	0x3F800000},		/*3F800000,10B8,0dB,invert=0*/
	{0x10B9,	0x00000000},		/*00000000,10B9,Cutoff,invert=0*/
	{0x10C0,	0x3FCEF400},		/*3FCEF400,10C0,HBF,50Hz,800Hz,5dB,fs/1,invert=0*/
	{0x10C1,	0xBFCC32C0},		/*BFCC32C0,10C1,HBF,50Hz,800Hz,5dB,fs/1,invert=0*/
	{0x10C2,	0x3F4E6A40},		/*3F4E6A40,10C2,HBF,50Hz,800Hz,5dB,fs/1,invert=0*/
	{0x10C3,	0x3F800000},		/*3F800000,10C3,Through,0dB,fs/1,invert=0*/
	{0x10C4,	0x00000000},		/*00000000,10C4,Through,0dB,fs/1,invert=0*/
	{0x10C5,	0x00000000},		/*00000000,10C5,Through,0dB,fs/1,invert=0*/
	{0x10C6,	0x3D506F00},		/*3D506F00,10C6,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C7,	0x3D506F00},		/*3D506F00,10C7,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C8,	0x3F65F240},		/*3F65F240,10C8,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x10C9,	0x3C1A1000},		/*3C1A1000,10C9,LPF,1.4Hz,34dB,fs/1,invert=0*/
	{0x10CA,	0x3C1A1000},		/*3C1A1000,10CA,LPF,1.4Hz,34dB,fs/1,invert=0*/
	{0x10CB,	0x3F7FE780},		/*3F7FE780,10CB,LPF,1.4Hz,34dB,fs/1,invert=0*/
	{0x10CC,	0x3F2C0BC0},		/*3F2C0BC0,10CC,LBF,8Hz,15Hz,2dB,fs/1,invert=0*/
	{0x10CD,	0xBF2B5B00},		/*BF2B5B00,10CD,LBF,8Hz,15Hz,2dB,fs/1,invert=0*/
	{0x10CE,	0x3F7F7380},		/*3F7F7380,10CE,LBF,8Hz,15Hz,2dB,fs/1,invert=0*/
	{0x10D0,	0x3FFF64C0},		/*3FFF64C0,10D0,6dB,invert=0*/
	{0x10D1,	0x3E361880},		/*3E361880,10D1,-15dB,invert=0*/
	{0x10D2,	0x3F800000},		/*3F800000,10D2,0dB,invert=0*/
	{0x10D3,	0x3F004DC0},		/*3F004DC0,10D3,-6dB,invert=0*/
	{0x10D4,	0x3F800000},		/*3F800000,10D4,0dB,invert=0*/
	{0x10D5,	0x3F800000},		/*3F800000,10D5,0dB,invert=0*/
	{0x10D7,	0x41B31900},		/*41B31900,10D7,Through,27dB,fs/1,invert=0*/
	{0x10D8,	0x00000000},		/*00000000,10D8,Through,27dB,fs/1,invert=0*/
	{0x10D9,	0x00000000},		/*00000000,10D9,Through,27dB,fs/1,invert=0*/
	{0x10DA,	0x3F793580},		/*3F793580,10DA,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DB,	0xBFEC2C40},		/*BFEC2C40,10DB,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DC,	0x3FEC2C40},		/*3FEC2C40,10DC,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DD,	0x3F6B8FC0},		/*3F6B8FC0,10DD,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10DE,	0xBF64C540},		/*BF64C540,10DE,PKF,850Hz,-6dB,5,fs/1,invert=0*/
	{0x10E0,	0x3DF21080},		/*3DF21080,10E0,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E1,	0x3DF21080},		/*3DF21080,10E1,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E2,	0x3F437BC0},		/*3F437BC0,10E2,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E3,	0x00000000},		/*00000000,10E3,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E4,	0x00000000},		/*00000000,10E4,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x10E5,	0x3F800000},		/*3F800000,10E5,0dB,invert=0*/
	{0x10E8,	0x3F800000},		/*3F800000,10E8,0dB,invert=0*/
	{0x10E9,	0x00000000},		/*00000000,10E9,Cutoff,invert=0*/
	{0x10EA,	0x00000000},		/*00000000,10EA,Cutoff,invert=0*/
	{0x10EB,	0x00000000},		/*00000000,10EB,Cutoff,invert=0*/
	{0x10F0,	0x3F800000},		/*3F800000,10F0,Through,0dB,fs/1,invert=0*/
	{0x10F1,	0x00000000},		/*00000000,10F1,Through,0dB,fs/1,invert=0*/
	{0x10F2,	0x00000000},		/*00000000,10F2,Through,0dB,fs/1,invert=0*/
	{0x10F3,	0x00000000},		/*00000000,10F3,Through,0dB,fs/1,invert=0*/
	{0x10F4,	0x00000000},		/*00000000,10F4,Through,0dB,fs/1,invert=0*/
	{0x10F5,	0x3F800000},		/*3F800000,10F5,Through,0dB,fs/1,invert=0*/
	{0x10F6,	0x00000000},		/*00000000,10F6,Through,0dB,fs/1,invert=0*/
	{0x10F7,	0x00000000},		/*00000000,10F7,Through,0dB,fs/1,invert=0*/
	{0x10F8,	0x00000000},		/*00000000,10F8,Through,0dB,fs/1,invert=0*/
	{0x10F9,	0x00000000},		/*00000000,10F9,Through,0dB,fs/1,invert=0*/
	{0x1200,	0x00000000},		/*00000000,1200,Cutoff,invert=0*/
	{0x1201,	0x3F800000},		/*3F800000,1201,0dB,invert=0*/
	{0x1202,	0x3F800000},		/*3F800000,1202,0dB,invert=0*/
	{0x1203,	0x3F800000},		/*3F800000,1203,0dB,invert=0*/
	{0x1204,	0x410E08C0},		/*410E08C0,1204,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1205,	0xC10C24C0},		/*C10C24C0,1205,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1206,	0x3F437BC0},		/*3F437BC0,1206,HBF,50Hz,1000Hz,20dB,fs/1,invert=0*/
	{0x1207,	0x3DF21080},		/*3DF21080,1207,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1208,	0x3DF21080},		/*3DF21080,1208,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x1209,	0x3F437BC0},		/*3F437BC0,1209,LPF,1000Hz,0dB,fs/1,invert=0*/
	{0x120A,	0x3D506F00},		/*3D506F00,120A,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120B,	0x3D506F00},		/*3D506F00,120B,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120C,	0x3F65F240},		/*3F65F240,120C,LPF,400Hz,0dB,fs/1,invert=0*/
	{0x120D,	0x3BDE2E40},		/*3BDE2E40,120D,LPF,1.6Hz,30dB,fs/1,invert=0*/
	{0x120E,	0x3BDE2E40},		/*3BDE2E40,120E,LPF,1.6Hz,30dB,fs/1,invert=0*/
	{0x120F,	0x3F7FE400},		/*3F7FE400,120F,LPF,1.6Hz,30dB,fs/1,invert=0*/
	{0x1210,	0x3EB562C0},		/*3EB562C0,1210,LBF,8Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1211,	0xBEB3D640},		/*BEB3D640,1211,LBF,8Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1212,	0x3F7F7380},		/*3F7F7380,1212,LBF,8Hz,32Hz,3dB,fs/1,invert=0*/
	{0x1213,	0x3FFF64C0},		/*3FFF64C0,1213,6dB,invert=0*/
	{0x1214,	0x00000000},		/*00000000,1214,Cutoff,invert=0*/
	{0x1215,	0x40346080},		/*40346080,1215,9dB,invert=0*/
	{0x1216,	0x3F004DC0},		/*3F004DC0,1216,-6dB,invert=0*/
	{0x1217,	0x3F800000},		/*3F800000,1217,0dB,invert=0*/
	{0x1218,	0x3F800000},		/*3F800000,1218,Through,0dB,fs/1,invert=0*/
	{0x1219,	0x00000000},		/*00000000,1219,Through,0dB,fs/1,invert=0*/
	{0x121A,	0x00000000},		/*00000000,121A,Through,0dB,fs/1,invert=0*/
	{0x121B,	0x00000000},		/*00000000,121B,Through,0dB,fs/1,invert=0*/
	{0x121C,	0x00000000},		/*00000000,121C,Through,0dB,fs/1,invert=0*/
	{0x121D,	0x3F800000},		/*3F800000,121D,0dB,invert=0*/
	{0x121E,	0x3F800000},		/*3F800000,121E,0dB,invert=0*/
	{0x121F,	0x3F800000},		/*3F800000,121F,0dB,invert=0*/
	{0x1235,	0x3F800000},		/*3F800000,1235,0dB,invert=0*/
	{0x1236,	0x3F800000},		/*3F800000,1236,0dB,invert=0*/
	{0x1237,	0x3F800000},		/*3F800000,1237,0dB,invert=0*/
	{0x1238,	0x3F800000},		/*3F800000,1238,0dB,invert=0*/
	{0xFFFF,	0xFFFFFFFF}
};

// DI Coefficient Setting Value
#define	COEFTBL	7
const uint32_t ClDiCof[COEFTBL] = {
	0x3F7FFD80, /* 0 */
	0x3F7FFD80, /* 1 */
	0x3F7FFD80, /* 2 */
	0x3F7FFD80, /* 3 */
	0x3F7FFD80, /* 4 */
	0x3F7FFD80, /* 5 */
	0x3F7FFD80	/* 6 */
};

/*********************************************************************
	Global Variable
*********************************************************************/
int32_t lc898122a_i2c_check(void)
{
	uint8_t UcLsiVer;
	RegReadA(CVER, &UcLsiVer);		// 0x27E
	return (UcLsiVer == 0xA1) ? 1 : 0;	//In the case of using LC898122A
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

	RegWriteA(SOFTRES2, 0x0F);
	lc898122a_wait_time(5);

	/* Read E2PROM Data */
	lc898122a_e2p_data_read();
	/* Get Version Info. */
	if (lc898122a_version_check() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_VERSION_FAIL;
	/* Clock Setting */
	lc898122a_init_clock();
	// AF Initial Setting

#if 1
	/* I/O Port Initial Setting */
	lc898122a_init_io_port();
#ifdef	INI_SHORT3
#else	//INI_SHORT3
	/* Monitor & Other Initial Setting */
	lc898122a_init_monitor();
#endif	//INI_SHORT3
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
#endif
	// CL-AF Initial Setting
	lc898122a_init_closed_af();

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
	lc898122a_mem_clear((uint8_t *)&StCalDat, sizeof(stCalDat));

	E2pRed((uint16_t)HALL_BIAS_Z, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlzGan); //WitTim(5);
	E2pRed((uint16_t)HALL_OFFSET_Z, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlzOff); //WitTim(5);
	E2pRed((uint16_t)HALL_ADOFFSET_Z, 2, (uint8_t *)&StCalDat.StHalAdj.UsAdzOff); //WitTim(5);
	E2pRed((uint16_t)LOOP_GAIN_Z, 2, (uint8_t *)&StCalDat.StLopGan.UsLzgVal); //WitTim(5);
	E2pRed((uint16_t)DRV_OFFSET_Z, 1, (uint8_t *)&StCalDat.StAfOff.UcAfOff); //WitTim(5);
	E2pRed((uint16_t)ADJ_HALL_Z_FLG, 2, (uint8_t *)&StCalDat.UsAdjHallZF); //WitTim(5);

	E2pRed((uint16_t)HALL_BIAS_X, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlxGan); //WitTim(5);
	E2pRed((uint16_t)HALL_BIAS_Y, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlyGan); //WitTim(5);

	E2pRed((uint16_t)HALL_OFFSET_X, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlxOff); //WitTim(5);
	E2pRed((uint16_t)HALL_OFFSET_Y, 2, (uint8_t *)&StCalDat.StHalAdj.UsHlyOff); //WitTim(5);

	E2pRed((uint16_t)LOOP_GAIN_X, 2, (uint8_t *)&StCalDat.StLopGan.UsLxgVal); //WitTim(5);
	E2pRed((uint16_t)LOOP_GAIN_Y, 2, (uint8_t *)&StCalDat.StLopGan.UsLygVal); //WitTim(5);

	E2pRed((uint16_t)LENS_CENTER_FINAL_X, 2, (uint8_t *)&StCalDat.StLenCen.UsLsxVal); //WitTim(5);
	E2pRed((uint16_t)LENS_CENTER_FINAL_Y, 2, (uint8_t *)&StCalDat.StLenCen.UsLsyVal); //WitTim(5);

	E2pRed((uint16_t)GYRO_AD_OFFSET_X, 2, (uint8_t *)&StCalDat.StGvcOff.UsGxoVal); //WitTim(5);
	E2pRed((uint16_t)GYRO_AD_OFFSET_Y, 2, (uint8_t *)&StCalDat.StGvcOff.UsGyoVal); //WitTim(5);

	E2pRed((uint16_t)OSC_CLK_VAL, 1, (uint8_t *)&StCalDat.UcOscVal); //WitTim(5);

	E2pRed((uint16_t)ADJ_HALL_FLAG, 2, (uint8_t *)&StCalDat.UsAdjHallF); //WitTim(5);
	E2pRed((uint16_t)ADJ_GYRO_FLAG, 2, (uint8_t *)&StCalDat.UsAdjGyroF); //WitTim(5);
	E2pRed((uint16_t)ADJ_LENS_FLAG, 2, (uint8_t *)&StCalDat.UsAdjLensF); //WitTim(5);

	E2pRed((uint16_t)GYRO_GAIN_X, 4, (uint8_t *)&StCalDat.UlGxgVal); //WitTim(5);
	E2pRed((uint16_t)GYRO_GAIN_Y, 4, (uint8_t *)&StCalDat.UlGygVal); //WitTim(5);

	E2pRed((uint16_t)FW_VERSION_INFO, 2, (uint8_t *)&StCalDat.UsVerDat); //WitTim(5);

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
	//CDBG("%s : %x, %x \n",__func__, (uint8_t)(StCalDat.UsVerDat >> 8, (uint8_t)(StCalDat.UsVerDat));
	UcVerHig = (uint8_t)(StCalDat.UsVerDat >> 8);		// System Version
	UcVerLow = (uint8_t)(StCalDat.UsVerDat);			// Filter Version

	if (UcVerHig == 0xC0) {							// 0xC0 1st Sample LGIT Act.
		UcVerHig = 0x00;
	} else if (UcVerHig == 0xC1) {					// 0xC1 PWM
		UcVerHig = 0x01;
	} else {
		return OIS_FW_POLLING_VERSION_FAIL;
	}

	/* Matching Version -> Filter */
	if (UcVerLow == 0x00) {							// 0xC0 1st Sample LGIT Act.
		UcVerLow = 0x00;
	} else if (UcVerLow == 0x01) {
		UcVerLow = 0x01;							// 0x01 3rd LGIT Act.
	} else if (UcVerLow == 0x02) {					// 0x02 4th LGIT Act.
		UcVerLow = 0x02 ;
	} else if (UcVerLow == 0x10) {
		UcVerLow = 0x10;
	} else if (UcVerLow == 0x11) {					// 0x11 2nd MTM Act.
		UcVerLow = 0x11;
	} else {
		return OIS_FW_POLLING_VERSION_FAIL;
	};

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
	RegReadA(CVER, &UcCvrCod);		// 0x027E

	/*Clock Enables*/
	RegWriteA(CLKON, 0x1F);			// 0x020B
}

//********************************************************************************
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
	RegWriteA(IOP1SEL, 0x00);	// 0x0231	IOP1 : DGDATAIN (ATT:0236h[0]=1)
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
	uint8_t	UcGrini;

	/*Set SPI Type*/

	RegWriteA(SPIM, 0x01);					// 0x028F	[ - | - | - | - ][ - | - | - | DGSPI4 ]
	//		DGSPI4	0: 3-wire SPI, 1: 4-wire SPI

	/*Set to Command Mode*/
	RegWriteA(GRSEL, 0x01);					// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	/*Digital Gyro Read settings*/
	RegWriteA(GRINI, 0x80);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegReadA(GRINI, &UcGrini);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA(GRINI, (uint8_t)(UcGrini | SLOWMODE));		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegWriteA(GRADR0, 0x6B);					// 0x0283	Set CLKSEL
	RegWriteA(GSETDT, 0x01);					// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

	RegWriteA(GRADR0, 0x1B);					// 0x0283	Set FS_SEL
	RegWriteA(GSETDT, (FS_SEL << 3));			// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

	RegWriteA(GRADR0, 0x1A);					// 0x0283	Set DLPF_CFG
	RegWriteA(GSETDT, 0x00);					// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;		/* Digital Gyro busy wait */

	RegWriteA(GRADR0, 0x6A);					// 0x0283	Set I2C_IF_DIS, SIG_COND_RESET
	RegWriteA(GSETDT, 0x11);					// 0x028A	Set Write Data
	RegWriteA(GRACC, 0x10);					/* 0x0282	Set Trigger ON				*/
	if (lc898122a_acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;	/* Digital Gyro busy wait */

	RegReadA(GRINI, &UcGrini);					// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]
	RegWriteA(GRINI, (uint8_t)(UcGrini & ~SLOWMODE));		// 0x0281	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ - | SLOWMODE | - | - ]

	RegWriteA(RDSEL, 0x7C);					// 0x028B	RDSEL(Data1 and 2 for continous mode)
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
	RegWriteA(PWMMONA, 0x00);				// 0x0030	0:off
	RegWriteA(MONSELA, 0x5C);				// 0x0270	DLYMON1
	RegWriteA(MONSELB, 0x5D);				// 0x0271	DLYMON2
	RegWriteA(MONSELC, 0x00);				// 0x0272
	RegWriteA(MONSELD, 0x00);				// 0x0273
	// Monitor Circuit
	RegWriteA(WC_PINMON1, 0x00);			// 0x01C0	Filter Monitor
	RegWriteA(WC_PINMON2, 0x00);			// 0x01C1
	RegWriteA(WC_PINMON3, 0x00);			// 0x01C2
	RegWriteA(WC_PINMON4, 0x00);			// 0x01C3
	/* Delay Monitor */
	RegWriteA(WC_DLYMON11, 0x04);			// 0x01C5	DlyMonAdd1[10:8]
	RegWriteA(WC_DLYMON10, 0x40);			// 0x01C4	DlyMonAdd1[ 7:0]
	RegWriteA(WC_DLYMON21, 0x04);			// 0x01C7	DlyMonAdd2[10:8]
	RegWriteA(WC_DLYMON20, 0xC0);			// 0x01C6	DlyMonAdd2[ 7:0]
	RegWriteA(WC_DLYMON31, 0x00);			// 0x01C9	DlyMonAdd3[10:8]
	RegWriteA(WC_DLYMON30, 0x00);			// 0x01C8	DlyMonAdd3[ 7:0]
	RegWriteA(WC_DLYMON41, 0x00);			// 0x01CB	DlyMonAdd4[10:8]
	RegWriteA(WC_DLYMON40, 0x00);			// 0x01CA	DlyMonAdd4[ 7:0]

	/* Monitor */
	RegWriteA(PWMMONA, 0x80);				// 0x0030	1:on
//	RegWriteA(IOP0SEL, 0x01);			// 0x0230	IOP0 : MONA

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
	uint8_t	UcStbb0;

	if (UcVerHig == 0x00) {							// 0x00 CVL
		UcPwmMod = PWMMOD_CVL ;
	} else if (UcVerHig == 0x01) {					// 0x01 PWM
		UcPwmMod = PWMMOD_PWM ;
	}

	RegWriteA(WC_EQON, 0x00);				// 0x0101		Filter Calcu
	RegWriteA(WC_RAMINITON,0x00);				// 0x0102
	lc898122a_gyro_ram_clear(0x0000, CLR_ALL_RAM);	/* All Clear */

	RegWriteA(WH_EQSWX, 0x02);				// 0x0170		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
	RegWriteA(WH_EQSWY, 0x02);				// 0x0171		[ - | - | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]

	lc898122a_ram_mode(OFF);				/* 32bit Float mode */

#ifdef	INI_SHORT3
#else	//INI_SHORT3
	/* Monitor Gain */
	RamWrite32A(dm1g, 0x3F800000);				// 0x109A
	RamWrite32A(dm2g, 0x3F800000);				// 0x109B
	RamWrite32A(dm3g, 0x3F800000);				// 0x119A
	RamWrite32A(dm4g, 0x3F800000);				// 0x119B
#endif	//INI_SHORT3

	/* Hall output limiter */
	RamWrite32A(sxlmta1, 0x3F800000);			// 0x10E6		Hall X output Limit
	RamWrite32A(sylmta1, 0x3F800000);			// 0x11E6		Hall Y output Limit

	/* Emergency Stop */
	RegWriteA(WH_EMGSTPON, 0x00);				// 0x0178		Emergency Stop OFF
	RegWriteA(WH_EMGSTPTMR,0xFF);				// 0x017A		255*(16/23.4375kHz)=174ms

	RamWrite32A(sxemglev, 0x3F800000);			// 0x10EC		Hall X Emergency threshold
	RamWrite32A(syemglev, 0x3F800000);			// 0x11EC		Hall Y Emergency threshold

	/* Hall Servo smoothing */
	RegWriteA(WH_SMTSRVON, 0x00);				// 0x017C		Smooth Servo OFF
	RegWriteA(WH_SMTSRVSMP,0x06);				// 0x017D		2.7ms=2^06/23.4375kHz
	RegWriteA(WH_SMTTMR, 0x01);				// 0x017E		1.3ms=(1+1)*16/23.4375kHz

	RamWrite32A(sxsmtav, 0xBC800000);			// 0x10ED		1/64 X smoothing ave coefficient
	RamWrite32A(sysmtav, 0xBC800000);			// 0x11ED		1/64 Y smoothing ave coefficient
	RamWrite32A(sxsmtstp, 0x3AE90466);			// 0x10EE		0.001778 X smoothing offset
	RamWrite32A(sysmtstp, 0x3AE90466);			// 0x11EE		0.001778 Y smoothing offset

	/* High-dimensional correction  */
	RegWriteA(WH_HOFCON, 0x11);				// 0x0174		OUT 3x3
	if ((UcVerLow == 0x00) || (UcVerLow == 0x01) || (UcVerLow == 0x02)) {
		/* (0.4531388X^3+0.4531388X)*(0.4531388X^3+0.4531388X) 15ohm*/
		/* Front */
		RamWrite32A(sxiexp3, 0x3EE801CF);			// 0x10BA
		RamWrite32A(sxiexp2, 0x00000000);			// 0x10BB
		RamWrite32A(sxiexp1, 0x3EE801CF);			// 0x10BC
		RamWrite32A(sxiexp0, 0x00000000);			// 0x10BD
		RamWrite32A(sxiexp, 0x3F800000);			// 0x10BE

		RamWrite32A(syiexp3, 0x3EE801CF);			// 0x11BA
		RamWrite32A(syiexp2, 0x00000000);			// 0x11BB
		RamWrite32A(syiexp1, 0x3EE801CF);			// 0x11BC
		RamWrite32A(syiexp0, 0x00000000);			// 0x11BD
		RamWrite32A(syiexp, 0x3F800000);			// 0x11BE

		/* Back */
		RamWrite32A(sxoexp3, 0x3EE801CF);			// 0x10FA
		RamWrite32A(sxoexp2, 0x00000000);			// 0x10FB
		RamWrite32A(sxoexp1, 0x3EE801CF);			// 0x10FC
		RamWrite32A(sxoexp0, 0x00000000);			// 0x10FD
		RamWrite32A(sxoexp, 0x3F800000);			// 0x10FE

		RamWrite32A(syoexp3, 0x3EE801CF);			// 0x11FA
		RamWrite32A(syoexp2, 0x00000000);			// 0x11FB
		RamWrite32A(syoexp1, 0x3EE801CF);			// 0x11FC
		RamWrite32A(syoexp0, 0x00000000);			// 0x11FD
		RamWrite32A(syoexp, 0x3F800000);			// 0x11FE
	} else if (UcVerLow == 0x10 || (UcVerLow == 0x11)) {
		/* (0.3750114X^3+0.5937681X)*(0.3750114X^3+0.5937681X) 6.5ohm*/
		/* Front */
		RamWrite32A(sxiexp3, 0x3EC0017F);			// 0x10BA
		RamWrite32A(sxiexp2, 0x00000000);			// 0x10BB
		RamWrite32A(sxiexp1, 0x3F180130);			// 0x10BC
		RamWrite32A(sxiexp0, 0x00000000);			// 0x10BD
		RamWrite32A(sxiexp, 0x3F800000);			// 0x10BE

		RamWrite32A(syiexp3, 0x3EC0017F);			// 0x11BA
		RamWrite32A(syiexp2, 0x00000000);			// 0x11BB
		RamWrite32A(syiexp1, 0x3F180130);			// 0x11BC
		RamWrite32A(syiexp0, 0x00000000);			// 0x11BD
		RamWrite32A(syiexp, 0x3F800000);			// 0x11BE

		/* Back */
		RamWrite32A(sxoexp3, 0x3EC0017F);			// 0x10FA
		RamWrite32A(sxoexp2, 0x00000000);			// 0x10FB
		RamWrite32A(sxoexp1, 0x3F180130);			// 0x10FC
		RamWrite32A(sxoexp0, 0x00000000);			// 0x10FD
		RamWrite32A(sxoexp, 0x3F800000);			// 0x10FE

		RamWrite32A(syoexp3, 0x3EC0017F);			// 0x11FA
		RamWrite32A(syoexp2, 0x00000000);			// 0x11FB
		RamWrite32A(syoexp1, 0x3F180130);			// 0x11FC
		RamWrite32A(syoexp0, 0x00000000);			// 0x11FD
		RamWrite32A(syoexp, 0x3F800000);			// 0x11FE
	}
#ifdef	CATCHMODE
	RegWriteA(WC_DPI1ADD0, 0x2F);				// 0x01B0		Data Pass
	RegWriteA(WC_DPI1ADD1, 0x00);				// 0x01B1		0x142F(PXMBZ2) --> 0x1406(GXI2Z2)
	RegWriteA(WC_DPI2ADD0, 0xAF);				// 0x01B2
	RegWriteA(WC_DPI2ADD1, 0x00);				// 0x01B3		0x14AF(PYMBZ2) --> 0x1486(GYI2Z2)
	RegWriteA(WC_DPI3ADD0, 0x38);				// 0x01B4
	RegWriteA(WC_DPI3ADD1, 0x00);				// 0x01B5		0x1438(GXK1Z2) --> 0x143A(GXK2Z1)
	RegWriteA(WC_DPI4ADD0, 0xB8);				// 0x01B6
	RegWriteA(WC_DPI4ADD1, 0x00);				// 0x01B7		0x14B8(GYK1Z2) --> 0x14BA(GYK2Z1)
	RegWriteA(WC_DPO1ADD0, 0x32);				// 0x01B8		--> 0x1432(GXJ1Z2)	//Tokoro 20141128 CATCHMODE
	RegWriteA(WC_DPO1ADD1, 0x00);				// 0x01B9
	RegWriteA(WC_DPO2ADD0, 0xB2);				// 0x01BA		--> 0x14B2(GYJ1Z2)	//Tokoro 20141128 CATCHMODE
	RegWriteA(WC_DPO2ADD1, 0x00);				// 0x01BB
	RegWriteA(WC_DPO3ADD0, 0x3A);				// 0x01BC
	RegWriteA(WC_DPO3ADD1, 0x00);				// 0x01BD
	RegWriteA(WC_DPO4ADD0, 0xBA);				// 0x01BE
	RegWriteA(WC_DPO4ADD1, 0x00);				// 0x01BF
	RegWriteA(WC_DPON, 0x0F);				// 0x0105		Data pass ON
#endif	//CATCHMODE
	/* Ram Access */
	lc898122a_ram_mode(OFF);	/* 32bit float mode */

	// PWM Signal Generate
	lc898122a_driver_mode(OFF);					/* 0x0070 Driver Block Ena=0 */
	RegWriteA(DRVSELX, 0xFF);					// 0x0003	PWM X drv max current  DRVSELX[7:0]
	RegWriteA(DRVSELY, 0xFF);					// 0x0004	PWM Y drv max current  DRVSELY[7:0]

	RegWriteA(PWMA, 0x00);					// 0x0010	PWM X/Y standby
	RegWriteA(PWMDLYX, 0x04);					// 0x0012	X Phase Delay Setting
	RegWriteA(PWMDLYY, 0x04);					// 0x0013	Y Phase Delay Setting

	if (UcPwmMod == PWMMOD_PWM) {						// PWMMOD_PWM
		PwmCareerSet(PWM_CAREER_MODE_A);
	} else {
		RegWriteA(DRVFC2, 0x90);				// 0x0002	Slope 3, Dead Time = 30ns
#ifdef	PWM_BREAK
		RegWriteA(PWMFC, 0x3D);					// 0x0011	VREF, PWMCLK/128, MODE0B, 12Bit Accuracy
#else
		RegWriteA(PWMFC, 0x21);					// 0x0011	VREF, PWMCLK/256, MODE1, 12Bit Accuracy
#endif
		RegWriteA(PWMPERIODX, 0x00);			// 0x0018		PWM Carrier Freq
		RegWriteA(PWMPERIODX2, 0x00);			// 0x0019		PWM Carrier Freq
		RegWriteA(PWMPERIODY, 0x00);			// 0x001A		PWM Carrier Freq
		RegWriteA(PWMPERIODY2, 0x00);			// 0x001B		PWM Carrier Freq
	}
	/* Linear PWM circuit setting */
	RegWriteA(CVA, 0xC0);					// 0x0020	Linear PWM mode enable
	RegWriteA(CVFC2, 0x80);					// 0x0022

	RegReadA(STBB0, &UcStbb0);				// 0x0250	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 &= 0x80;
	RegWriteA(STBB0, UcStbb0);					// 0x0250	OIS standby

}

/*********************************************************************
 Function Name	: lc898122a_init_gyro_setting
 Return Value		: NON
 Argument Value	: NON
 Explanation		: Gyro Filter Setting Initialize Function
 History			: First edition							2013.01.09 Y.Shigeoka
*********************************************************************/
void lc898122a_init_gyro_setting(void)
{
#ifdef	CATCHMODE
	/* CPU control */
	RegWriteA(WC_CPUOPEON, 0x11);	// 0x0103		CPU control
//	RegWriteA(WC_CPUOPE1ADD , 0x06);	// 0x018A		0x1406(GXI2Z2), 0x1486(GYI2Z2)
	RegWriteA(WC_CPUOPE1ADD, 0x32);	// 0x018A		0x1432(GXJ1Z2), 0x14B2(GYJ1Z2)	//Tokoro 20141128 CATCHMODE
	RegWriteA(WC_CPUOPE2ADD, 0x3A);	// 0x018B		0x143A(GXK2Z1), 0x14BA(GYK2Z1)
	RegWriteA(WG_EQSW, 0x43);		// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
#else	//CATCHMODE
	/*Gyro Filter Setting*/
	RegWriteA(WG_EQSW, 0x03);						// 0x0110		[ - | Sw6 | Sw5 | Sw4 ][ Sw3 | Sw2 | Sw1 | Sw0 ]
#endif	//CATCHMODE

	/*Gyro Filter Down Sampling*/

	RegWriteA(WG_SHTON, 0x10);						// 0x0107		[ - | - | - | CmSht2PanOff ][ - | - | CmShtOpe(1:0) ]
	RegWriteA(WG_SHTMOD, 0x06);						// 0x0116		Shutter Hold mode
	// Limiter
	RamWrite32A(gxlmt1H, GYRLMT1H);					// 0x1028
	RamWrite32A(gylmt1H, GYRLMT1H);					// 0x1128
	RamWrite32A(Sttx12aM, GYRA12_MID);				// 0x104F
	RamWrite32A(Sttx12bM, GYRB12_MID);				// 0x106F
	RamWrite32A(Sttx12bH, GYRB12_HGH);				// 0x107F
	RamWrite32A(Sttx34aM, GYRA34_MID);				// 0x108F
	RamWrite32A(Sttx34aH, GYRA34_HGH);				// 0x109F
	RamWrite32A(Sttx34bM, GYRB34_MID);				// 0x10AF
	RamWrite32A(Sttx34bH, GYRB34_HGH);				// 0x10BF
	RamWrite32A(Stty12aM, GYRA12_MID);				// 0x114F
	RamWrite32A(Stty12bM, GYRB12_MID);				// 0x116F
	RamWrite32A(Stty12bH, GYRB12_HGH);				// 0x117F
	RamWrite32A(Stty34aM, GYRA34_MID);				// 0x118F
	RamWrite32A(Stty34aH, GYRA34_HGH);				// 0x119F
	RamWrite32A(Stty34bM, GYRB34_MID);				// 0x11AF
	RamWrite32A(Stty34bH, GYRB34_HGH);				// 0x11BF

#ifdef	CATCHMODE
#ifdef	CORRECT_1DEG
	SelectPtRange(ON);
#else
	SelectPtRange(OFF);
#endif
	/* Pan/Tilt parameter */
	RegWriteA(WG_PANADDA, 0x12);		// 0x0130	GXH2Z2/GYH2Z2 Select
	RegWriteA(WG_PANADDB, 0x3B);		// 0x0131	GXK2Z2/GYK2Z2 Select
#else	//CATCHMODE
	SelectPtRange(OFF);

	/* Pan/Tilt parameter */
	RegWriteA(WG_PANADDA, 0x12);					// 0x0130	GXH1Z2/GYH1Z2 Select
	RegWriteA(WG_PANADDB, 0x09);					// 0x0131	GXIZ/GYIZ Select
#endif	//CATCHMODE

	//Threshold
	RamWrite32A(SttxHis, 0x00000000);				// 0x1226
	RamWrite32A(SttxaL,	0x00000000);				// 0x109D
	RamWrite32A(SttxbL,	0x00000000);				// 0x109E
	RamWrite32A(SttyaL,	0x00000000);				// 0x119D
	RamWrite32A(SttybL,	0x00000000);				// 0x119E
	// Pan level
	RegWriteA(WG_PANLEVABS, 0x00);				// 0x0133

	// Average parameter are set IniAdj
#ifdef	CATCHMODE
	// Phase Transition Setting
	// State 2 -> 1
	RegWriteA(WG_PANSTT21JUG0, 0x07);		// 0x0140
	RegWriteA(WG_PANSTT21JUG1, 0x00);		// 0x0141
	// State 3 -> 1
	RegWriteA(WG_PANSTT31JUG0, 0x00);		// 0x0142
	RegWriteA(WG_PANSTT31JUG1, 0x00);		// 0x0143
	// State 4 -> 1
	RegWriteA(WG_PANSTT41JUG0, 0x00);		// 0x0144
	RegWriteA(WG_PANSTT41JUG1, 0x00);		// 0x0145
	// State 1 -> 2
	RegWriteA(WG_PANSTT12JUG0, 0x00);		// 0x0146
	RegWriteA(WG_PANSTT12JUG1, 0x07);		// 0x0147
	// State 1 -> 3
	RegWriteA(WG_PANSTT13JUG0, 0x00);		// 0x0148
	RegWriteA(WG_PANSTT13JUG1, 0x00);		// 0x0149
	// State 2 -> 3
	RegWriteA(WG_PANSTT23JUG0, 0x00);		// 0x014A
	RegWriteA(WG_PANSTT23JUG1, 0x00);		// 0x014B
	// State 4 -> 3
	RegWriteA(WG_PANSTT43JUG0, 0x00);		// 0x014C
	RegWriteA(WG_PANSTT43JUG1, 0x00);		// 0x014D
	// State 3 -> 4
	RegWriteA(WG_PANSTT34JUG0, 0x00);		// 0x014E
	RegWriteA(WG_PANSTT34JUG1, 0x00);		// 0x014F
	// State 2 -> 4
	RegWriteA(WG_PANSTT24JUG0, 0x00);		// 0x0150
	RegWriteA(WG_PANSTT24JUG1, 0x00);		// 0x0151
	// State 4 -> 2
	RegWriteA(WG_PANSTT42JUG0, 0x00);		// 0x0152
	RegWriteA(WG_PANSTT42JUG1, 0x00);		// 0x0153

	// State Timer
	RegWriteA(WG_PANSTT1LEVTMR,	0x00);		// 0x015B
	RegWriteA(WG_PANSTT2LEVTMR,	0x00);		// 0x015C
	RegWriteA(WG_PANSTT3LEVTMR,	0x00);		// 0x015D
	RegWriteA(WG_PANSTT4LEVTMR,	0x00);		// 0x015E

	// Control filter
	RegWriteA(WG_PANTRSON0, 0x1B);		// 0x0132	USE iSTP	0001 1011

	// State Setting
	IniPtMovMod(OFF);							// Pan/Tilt setting (Still)

	// Hold
	RegWriteA(WG_PANSTTSETILHLD, 0x00);		// 0x015F


	// State2,4 Step Time Setting
	RegWriteA(WG_PANSTT2TMR0,	0x01);		// 0x013C
	RegWriteA(WG_PANSTT2TMR1,	0x00);		// 0x013D
	RegWriteA(WG_PANSTT4TMR0,	0x01);		// 0x013E
	RegWriteA(WG_PANSTT4TMR1,	0x00);		// 0x013F

	RegWriteA(WG_PANSTTXXXTH,	0x00);		// 0x015A

#if 1
	AutoGainContIni();
	/* exe function */
	AutoGainControlSw(ON);							/* Auto Gain Control Mode ON  */
#else
	StartUpGainContIni();
#endif

#else	//CATCHMODE

	// Phase Transition Setting
	// State 2 -> 1
	RegWriteA(WG_PANSTT21JUG0, 0x00);				// 0x0140
	RegWriteA(WG_PANSTT21JUG1, 0x00);				// 0x0141
	// State 3 -> 1
	RegWriteA(WG_PANSTT31JUG0, 0x01);				// 0x0142
	RegWriteA(WG_PANSTT31JUG1, 0x00);				// 0x0143
	// State 4 -> 1
	RegWriteA(WG_PANSTT41JUG0, 0x00);				// 0x0144
	RegWriteA(WG_PANSTT41JUG1, 0x00);				// 0x0145
	// State 1 -> 2
	RegWriteA(WG_PANSTT12JUG0, 0x00);				// 0x0146
	RegWriteA(WG_PANSTT12JUG1, 0x00);				// 0x0147
	// State 1 -> 3
	RegWriteA(WG_PANSTT13JUG0, 0x00);				// 0x0148
	RegWriteA(WG_PANSTT13JUG1, 0x07);				// 0x0149
	// State 2 -> 3
	RegWriteA(WG_PANSTT23JUG0, 0x00);				// 0x014A
	RegWriteA(WG_PANSTT23JUG1, 0x00);				// 0x014B
	// State 4 -> 3
	RegWriteA(WG_PANSTT43JUG0, 0x00);				// 0x014C
	RegWriteA(WG_PANSTT43JUG1, 0x00);				// 0x014D
	// State 3 -> 4
	RegWriteA(WG_PANSTT34JUG0, 0x00);				// 0x014E
	RegWriteA(WG_PANSTT34JUG1, 0x00);				// 0x014F
	// State 2 -> 4
	RegWriteA(WG_PANSTT24JUG0, 0x00);				// 0x0150
	RegWriteA(WG_PANSTT24JUG1, 0x00);				// 0x0151
	// State 4 -> 2
	RegWriteA(WG_PANSTT42JUG0, 0x00);				// 0x0152
	RegWriteA(WG_PANSTT42JUG1, 0x00);				// 0x0153

#ifdef	INI_SHORT3
#else	//INI_SHORT3
	// State Timer
	RegWriteA(WG_PANSTT1LEVTMR, 0x00);				// 0x015B
	RegWriteA(WG_PANSTT2LEVTMR, 0x00);				// 0x015C
	RegWriteA(WG_PANSTT3LEVTMR, 0x00);				// 0x015D
	RegWriteA(WG_PANSTT4LEVTMR, 0x00);				// 0x015E
#endif	//INI_SHORT3

	// Control filter
	//RegWriteA(WG_PANTRSON0, 0x11);				// 0x0132	USE I12/iSTP/Gain-Filter
	RegWriteA(WG_PANTRSON0, 0x91);				// 0x0132	USE I12/iSTP/Gain-Filter, USE Linear

	RegWriteA(WG_PANSTTSETGYRO, 0x00);				// 0x0154
	RegWriteA(WG_PANSTTSETGAIN, 0x10);				// 0x0155
	RegWriteA(WG_PANSTTSETISTP, 0x10);				// 0x0156
	RegWriteA(WG_PANSTTSETIFTR, 0x10);				// 0x0157
	RegWriteA(WG_PANSTTSETLFTR, 0x00);				// 0x0158

#ifdef	INI_SHORT2
#else	//INI_SHORT2
	// State Setting
	lc898122a_init_pan_mode(OFF);								// Pan/Tilt setting (Still)
#endif	//INI_SHORT2

	// Hold
	RegWriteA(WG_PANSTTSETILHLD, 0x00);				// 0x015F

	// State2,4 Step Time Setting
	RegWriteA(WG_PANSTT2TMR0, 0xEA);					// 0x013C	9.983787013ms
	RegWriteA(WG_PANSTT2TMR1, 0x00);					// 0x013D
	RegWriteA(WG_PANSTT4TMR0, 0x92);					// 0x013E	49.91893506ms
	RegWriteA(WG_PANSTT4TMR1, 0x04);					// 0x013F

	RegWriteA(WG_PANSTTXXXTH, 0x0F);					// 0x015A


#ifdef GAIN_CONT
	AutoGainContIni();
	/* exe function */
	lc898122a_auto_gain_con(ON);						/* Auto Gain Control Mode OFF */
#endif

#endif	//CATCHMODE
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
#ifdef	CATCHMODE
#ifdef	INI_SHORT3
#else	//INI_SHORT3
	RegWriteA(WG_PANSTT1DWNSMP0, 0x00);		// 0x0134
	RegWriteA(WG_PANSTT1DWNSMP1, 0x00);		// 0x0135
	RegWriteA(WG_PANSTT2DWNSMP0, 0x00);		// 0x0136
	RegWriteA(WG_PANSTT2DWNSMP1, 0x00);		// 0x0137
	RegWriteA(WG_PANSTT3DWNSMP0, 0x00);		// 0x0138
	RegWriteA(WG_PANSTT3DWNSMP1, 0x00);		// 0x0139
	RegWriteA(WG_PANSTT4DWNSMP0, 0x00);		// 0x013A
	RegWriteA(WG_PANSTT4DWNSMP1, 0x00);		// 0x013B
#endif	//INI_SHORT3

	RamWrite32A(st1mean, 0x3f800000);			// 0x1235
	RamWrite32A(st2mean, 0x3f800000);			// 0x1236
	RamWrite32A(st3mean, 0x3f800000);			// 0x1237
	RamWrite32A(st4mean, 0x3f800000);			// 0x1238
#else	//CATCHMODE
#ifdef	INI_SHORT3
#else	//INI_SHORT3
	RegWriteA(WG_PANSTT1DWNSMP0, 0x00);		// 0x0134
	RegWriteA(WG_PANSTT1DWNSMP1, 0x00);		// 0x0135
	RegWriteA(WG_PANSTT2DWNSMP0, 0x00);		// 0x0136
	RegWriteA(WG_PANSTT2DWNSMP1, 0x00);		// 0x0137
	RegWriteA(WG_PANSTT3DWNSMP0, 0x00);		// 0x0138
	RegWriteA(WG_PANSTT3DWNSMP1, 0x00);		// 0x0139
	RegWriteA(WG_PANSTT4DWNSMP0, 0x00);		// 0x013A
	RegWriteA(WG_PANSTT4DWNSMP1, 0x00);		// 0x013B
#endif	//INI_SHORT3

	RamWrite32A(st1mean, 0x3f800000);			// 0x1235
	RamWrite32A(st2mean, 0x3f800000);			// 0x1236
	RamWrite32A(st3mean, 0x3f800000);			// 0x1237
	RamWrite32A(st4mean, 0x3f800000);			// 0x1238
#endif	//CATCHMODE
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
#ifdef	CATCHMODE
	switch (UcPtMod ) {
	case OFF:
		RegWriteA(WG_PANSTTSETGYRO,	0x00);		// 0x0154
		RegWriteA(WG_PANSTTSETGAIN,	0x00);		// 0x0155
		RegWriteA(WG_PANSTTSETISTP,	0x04);		// 0x0156
		RegWriteA(WG_PANSTTSETIFTR,	0x00);		// 0x0157
		RegWriteA(WG_PANSTTSETLFTR,	0x00);		// 0x0158

		break;
	case ON:
		RegWriteA(WG_PANSTTSETGYRO,	0x00);		// 0x0154
		RegWriteA(WG_PANSTTSETGAIN,	0x00);		// 0x0155
		RegWriteA(WG_PANSTTSETISTP,	0x00);		// 0x0156
		RegWriteA(WG_PANSTTSETIFTR,	0x00);		// 0x0157
		RegWriteA(WG_PANSTTSETLFTR,	0x00);		// 0x0158
		break ;
	}
#else	//CATCHMODE
	switch (UcPtMod) {
	case OFF:
		// State 3 -> 1
		RegWriteA(WG_PANSTT31JUG0, 0x01);			// 0x0142
		RegWriteA(WG_PANSTT31JUG1, 0x00);			// 0x0143
		// State 4 -> 1
		RegWriteA(WG_PANSTT41JUG0, 0x00);			// 0x0144
		RegWriteA(WG_PANSTT41JUG1, 0x00);			// 0x0145
		// State 1 -> 3
		RegWriteA(WG_PANSTT13JUG0, 0x00);			// 0x0148
		RegWriteA(WG_PANSTT13JUG1, 0x07);			// 0x0149
		// State 4 -> 3
		RegWriteA(WG_PANSTT43JUG0, 0x00);			// 0x014C
		RegWriteA(WG_PANSTT43JUG1, 0x00);			// 0x014D
		// State 3 -> 4
		RegWriteA(WG_PANSTT34JUG0, 0x00);			// 0x014E
		RegWriteA(WG_PANSTT34JUG1, 0x00);			// 0x014F

		RegWriteA(WG_PANSTTXXXTH, 0x0F);				// 0x015A
		RamWrite32A(Sttx34aM, GYRA34_MID);			// 0x108F
		RamWrite32A(Stty34aM, GYRA34_MID);			// 0x118F
		if ((UcVerLow == 0x00) || (UcVerLow == 0x01) || (UcVerLow == 0x10) ) {
			// I Filter X							// 2s
			RamWrite32A(gxia_1, 0x3760E040);		// 0x1043	0.1Hz
			RamWrite32A(gxib_1, 0xB261CF49);		// 0x1044	Down
			RamWrite32A(gxic_1, 0x3261CF49);		// 0x1045	Up

			RamWrite32A(gxia_a, 0x3760E040);		// 0x1046	0.1Hz
			RamWrite32A(gxib_a, 0xB261CF49);		// 0x1047	Down
			RamWrite32A(gxic_a, 0x3261CF49);		// 0x1048	Up

			RamWrite32A(gxia_b, 0x3A2F91C0);		// 0x1049	5Hz
			RamWrite32A(gxib_b, 0xB261CF49);		// 0x104A	Down
			RamWrite32A(gxic_b, 0x3F800000);		// 0x104B	Up

			RamWrite32A(gxia_c, 0x3760E040);		// 0x104C	0.1Hz
			RamWrite32A(gxib_c, 0xB261CF49);		// 0x104D	Down
			RamWrite32A(gxic_c, 0x3261CF49);		// 0x104E	Up

			// I Filter Y
			RamWrite32A(gyia_1, 0x3760E040);		// 0x1143	0.1Hz
			RamWrite32A(gyib_1, 0xB261CF49);		// 0x1144	Down
			RamWrite32A(gyic_1, 0x3261CF49);		// 0x1145	Up

			RamWrite32A(gyia_a, 0x3760E040);		// 0x1146	0.1Hz
			RamWrite32A(gyib_a, 0xB261CF49);		// 0x1147	Down
			RamWrite32A(gyic_a, 0x3261CF49);		// 0x1148	Up

			RamWrite32A(gyia_b, 0x3A2F91C0);		// 0x1149	5Hz
			RamWrite32A(gyib_b, 0xB261CF49);		// 0x114A	Down
			RamWrite32A(gyic_b, 0x3F800000);		// 0x114B	Up

			RamWrite32A(gyia_c, 0x3760E040);		// 0x114C	0.1Hz
			RamWrite32A(gyib_c, 0xB261CF49);		// 0x114D	Down
			RamWrite32A(gyic_c, 0x3261CF49);		// 0x114E	Up
		} else {	// (UcVerLow == 0x02) || (UcVerLow == 0x11)
			// I Filter X							// 2s
			RamWrite32A(gxia_1, 0x37A8A800);		// 0x1043	0.15Hz
			RamWrite32A(gxib_1, 0xB261CF49);		// 0x1044	Down
			RamWrite32A(gxic_1, 0x3261CF49);		// 0x1045	Up

			RamWrite32A(gxia_a, 0x37A8A800);		// 0x1046	0.15Hz
			RamWrite32A(gxib_a, 0xB261CF49);		// 0x1047	Down
			RamWrite32A(gxic_a, 0x3261CF49);		// 0x1048	Up

			RamWrite32A(gxia_b, 0x3A2F91C0);		// 0x1049	5Hz
			RamWrite32A(gxib_b, 0xB261CF49);		// 0x104A	Down
			RamWrite32A(gxic_b, 0x3F800000);		// 0x104B	Up

			RamWrite32A(gxia_c, 0x37A8A800);		// 0x104C	0.15Hz
			RamWrite32A(gxib_c, 0xB261CF49);		// 0x104D	Down
			RamWrite32A(gxic_c, 0x3261CF49);		// 0x104E	Up

			// I Filter Y
			RamWrite32A(gyia_1, 0x37A8A800);		// 0x1143	0.15Hz
			RamWrite32A(gyib_1, 0xB261CF49);		// 0x1144	Down
			RamWrite32A(gyic_1, 0x3261CF49);		// 0x1145	Up

			RamWrite32A(gyia_a, 0x37A8A800);		// 0x1146	0.15Hz
			RamWrite32A(gyib_a, 0xB261CF49);		// 0x1147	Down
			RamWrite32A(gyic_a, 0x3261CF49);		// 0x1148	Up

			RamWrite32A(gyia_b, 0x3A2F91C0);		// 0x1149	5Hz
			RamWrite32A(gyib_b, 0xB261CF49);		// 0x114A	Down
			RamWrite32A(gyic_b, 0x3F800000);		// 0x114B	Up

			RamWrite32A(gyia_c, 0x37A8A800);		// 0x114C	0.15Hz
			RamWrite32A(gyib_c, 0xB261CF49);		// 0x114D	Down
			RamWrite32A(gyic_c, 0x3261CF49);		// 0x114E	Up
		}
		break;
	case ON:
		// State 3 -> 1
		RegWriteA(WG_PANSTT31JUG0, 0x00);			// 0x0142
		RegWriteA(WG_PANSTT31JUG1, 0x00);			// 0x0143
		// State 4 -> 1
		RegWriteA(WG_PANSTT41JUG0, 0x07);			// 0x0144
		RegWriteA(WG_PANSTT41JUG1, 0x00);			// 0x0145
		// State 1 -> 3
		RegWriteA(WG_PANSTT13JUG0, 0x00);			// 0x0148
		RegWriteA(WG_PANSTT13JUG1, 0x07);			// 0x0149
		// State 4 -> 3
		RegWriteA(WG_PANSTT43JUG0, 0x00);			// 0x014C
		RegWriteA(WG_PANSTT43JUG1, 0x07);			// 0x014D
		// State 3 -> 4
		RegWriteA(WG_PANSTT34JUG0, 0x01);			// 0x014E
		RegWriteA(WG_PANSTT34JUG1, 0x00);			// 0x014F

		RegWriteA(WG_PANSTTXXXTH, 0xF0);				// 0x015A
		RamWrite32A(Sttx34aM, GYRA34_MID_M);			// 0x108F
		RamWrite32A(Stty34aM, GYRA34_MID_M);			// 0x118F
		if ((UcVerLow == 0x00) || (UcVerLow == 0x01) || (UcVerLow == 0x10) ) {
			// I Filter X							// 2s
			RamWrite32A(gxia_1, 0x3760E040);		// 0x1043	0.1Hz
			RamWrite32A(gxib_1, 0xB261CF49);		// 0x1044	Down
			RamWrite32A(gxic_1, 0x3261CF49);		// 0x1045	Up

			RamWrite32A(gxia_a, 0x3760E040);		// 0x1046	0.1Hz
			RamWrite32A(gxib_a, 0xB261CF49);		// 0x1047	Down
			RamWrite32A(gxic_a, 0x3261CF49);		// 0x1048	Up

			RamWrite32A(gxia_b, 0x3760E040);		// 0x1049	0.1Hz
			RamWrite32A(gxib_b, 0xB261CF49);		// 0x104A	Down
			RamWrite32A(gxic_b, 0x3261CF49);		// 0x104B	Up

			RamWrite32A(gxia_c, 0x3A2F91C0);		// 0x104C	5Hz
			RamWrite32A(gxib_c, 0xB261CF49);		// 0x104D	Down
			RamWrite32A(gxic_c, 0x3F800000);		// 0x104E	Up

			// I Filter Y
			RamWrite32A(gyia_1, 0x3760E040);		// 0x1143	0.1Hz
			RamWrite32A(gyib_1, 0xB261CF49);		// 0x1144	Down
			RamWrite32A(gyic_1, 0x3261CF49);		// 0x1145	Up

			RamWrite32A(gyia_a, 0x3760E040);		// 0x1146	0.1Hz
			RamWrite32A(gyib_a, 0xB261CF49);		// 0x1147	Down
			RamWrite32A(gyic_a, 0x3261CF49);		// 0x1148	Up

			RamWrite32A(gyia_b, 0x3760E040);		// 0x1149	0.1Hz
			RamWrite32A(gyib_b, 0xB261CF49);		// 0x114A	Down
			RamWrite32A(gyic_b, 0x3261CF49);		// 0x114B	Up

			RamWrite32A(gyia_c, 0x3A2F91C0);		// 0x114C	5Hz
			RamWrite32A(gyib_c, 0xB261CF49);		// 0x114D	Down
			RamWrite32A(gyic_c, 0x3F800000);		// 0x114E	Up
		} else {	// (UcVerLow == 0x02) || (UcVerLow == 0x11)
			// I Filter X							// 2s
			RamWrite32A(gxia_1, 0x37A8A800);		// 0x1043	0.15Hz
			RamWrite32A(gxib_1, 0xB261CF49);		// 0x1044	Down
			RamWrite32A(gxic_1, 0x3261CF49);		// 0x1045	Up

			RamWrite32A(gxia_a, 0x37A8A800);		// 0x1046	0.15Hz
			RamWrite32A(gxib_a, 0xB261CF49);		// 0x1047	Down
			RamWrite32A(gxic_a, 0x3261CF49);		// 0x1048	Up

			RamWrite32A(gxia_b, 0x37A8A800);		// 0x1049	0.15Hz
			RamWrite32A(gxib_b, 0xB261CF49);		// 0x104A	Down
			RamWrite32A(gxic_b, 0x3261CF49);		// 0x104B	Up

			RamWrite32A(gxia_c, 0x3A2F91C0);		// 0x104C	5Hz
			RamWrite32A(gxib_c, 0xB261CF49);		// 0x104D	Down
			RamWrite32A(gxic_c, 0x3F800000);		// 0x104E	Up

			// I Filter Y
			RamWrite32A(gyia_1, 0x37A8A800);		// 0x1143	0.15Hz
			RamWrite32A(gyib_1, 0xB261CF49);		// 0x1144	Down
			RamWrite32A(gyic_1, 0x3261CF49);		// 0x1145	Up

			RamWrite32A(gyia_a, 0x37A8A800);		// 0x1146	0.15Hz
			RamWrite32A(gyib_a, 0xB261CF49);		// 0x1147	Down
			RamWrite32A(gyic_a, 0x3261CF49);		// 0x1148	Up

			RamWrite32A(gyia_b, 0x37A8A800);		// 0x1149	0.15Hz
			RamWrite32A(gyib_b, 0xB261CF49);		// 0x114A	Down
			RamWrite32A(gyic_b, 0x3261CF49);		// 0x114B	Up

			RamWrite32A(gyia_c, 0x3A2F91C0);		// 0x114C	5Hz
			RamWrite32A(gyib_c, 0xB261CF49);		// 0x114D	Down
			RamWrite32A(gyic_c, 0x3F800000);		// 0x114E	Up
		}
		break;
	}
#endif	//CATCHMODE
}
//********************************************************************************
// Function Name	: SelectPtRange
// Retun Value		: NON
// Argment Value	: OFF:Narrow  ON:Wide
// Explanation		: Pan/Tilt parameter Range function
// History			: First edition							2014.04.08 Y.Shigeoka
//********************************************************************************
void	SelectPtRange(unsigned char UcSelRange )
{
	switch (UcSelRange ) {
	case OFF:
		RamWrite32A(gxlmt3HS0, GYRLMT3_S1);		// 0x1029
		RamWrite32A(gylmt3HS0, GYRLMT3_S1);		// 0x1129

		RamWrite32A(gxlmt3HS1, GYRLMT3_S2);		// 0x102A
		RamWrite32A(gylmt3HS1, GYRLMT3_S2);		// 0x112A

		RamWrite32A(gylmt4HS0, GYRLMT4_S1);		//0x112B	Y axis Limiter4 High Threshold0
		RamWrite32A(gxlmt4HS0, GYRLMT4_S1);		//0x102B	X axis Limiter4 High Threshold0

		RamWrite32A(gxlmt4HS1, GYRLMT4_S2);		//0x102C	X axis Limiter4 High Threshold1
		RamWrite32A(gylmt4HS1, GYRLMT4_S2);		//0x112C	Y axis Limiter4 High Threshold1

		RamWrite32A(Sttx12aH,	GYRA12_HGH);		// 0x105F
		RamWrite32A(Stty12aH,	GYRA12_HGH);		// 0x115F

		break;

#ifdef	CATCHMODE
	case ON:
		RamWrite32A(gxlmt3HS0, GYRLMT3_S1_W);		// 0x1029
		RamWrite32A(gylmt3HS0, GYRLMT3_S1_W);		// 0x1129

		RamWrite32A(gxlmt3HS1, GYRLMT3_S2_W);		// 0x102A
		RamWrite32A(gylmt3HS1, GYRLMT3_S2_W);		// 0x112A

		RamWrite32A(gylmt4HS0, GYRLMT4_S1_W);		//0x112B	Y axis Limiter4 High Threshold0
		RamWrite32A(gxlmt4HS0, GYRLMT4_S1_W);		//0x102B	X axis Limiter4 High Threshold0

		RamWrite32A(gxlmt4HS1, GYRLMT4_S2_W);		//0x102C	X axis Limiter4 High Threshold1
		RamWrite32A(gylmt4HS1, GYRLMT4_S2_W);		//0x112C	Y axis Limiter4 High Threshold1

		RamWrite32A(Sttx12aH,	GYRA12_HGH_W);			// 0x105F
		RamWrite32A(Stty12aH,	GYRA12_HGH_W);			// 0x115F

		break;
#endif // CATCHMODE
	}
}

//********************************************************************************
// Function Name	: SelectIstpMod
// Retun Value		: NON
// Argment Value	: OFF:Narrow  ON:Wide
// Explanation		: Pan/Tilt parameter Range function
// History			: First edition							2014.04.08 Y.Shigeoka
//********************************************************************************
void	SelectIstpMod(unsigned char UcSelRange )
{
	switch (UcSelRange ) {
	case OFF:
		RamWrite32A(gxistp_1, GYRISTP);		// 0x1083
		RamWrite32A(gyistp_1, GYRISTP);		// 0x1183
		break ;

#ifdef	CATCHMODE
	case ON:
		RamWrite32A(gxistp_1, GYRISTP_W);	// 0x1083
		RamWrite32A(gyistp_1, GYRISTP_W);	// 0x1183
		break ;
#endif	//CATCHMODE
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
	uint16_t		UsAryId;
	struct STFILREG		*pFilReg;
	struct STFILRAM		*pFilRam;

	// Filter Register Parameter Setting
	UsAryId	= 0;

	if (UcVerLow == 0x00) {
		pFilReg = (struct STFILREG *)CsFilReg_C000;
		pFilRam = (struct STFILRAM *)CsFilRam_C000;
	} else if (UcVerLow == 0x01) {						// UcVerLow == 0x01 // LGIT 3rd Act. 141105
		pFilReg = (struct STFILREG *)CsFilReg_C001;
		pFilRam = (struct STFILRAM *)CsFilRam_C001;
	} else if (UcVerLow == 0x02 ) {						// UcVerLow == 0x02 // LGIT 4th Act. 141208
		pFilReg = (struct STFILREG *)CsFilReg_C002;
		pFilRam = (struct STFILRAM *)CsFilRam_C002;
	} else if (UcVerLow == 0x10) {						// MTM
		pFilReg = (struct STFILREG *)CsFilReg_C010;
		pFilRam = (struct STFILRAM *)CsFilRam_C010;
	} else if (UcVerLow == 0x11 ) {
		pFilReg = (struct STFILREG *)CsFilReg_C011;
		pFilRam = (struct STFILRAM *)CsFilRam_C011;
	} else {
		return OIS_FW_POLLING_FAIL;
	}

	while (pFilReg[UsAryId].UsRegAdd != 0xFFFF) {
		RegWriteA(pFilReg[UsAryId].UsRegAdd, pFilReg[UsAryId].UcRegDat);
		UsAryId++ ;
		if (UsAryId > FILREGTAB) {
			return OIS_FW_POLLING_FAIL ;
		}
	}

	RegWriteA(WC_RAMACCXY, 0x01);			// 0x018D	Simultaneously Setting On

	// Filter Ram Parameter Setting
	UsAryId	= 0;
	while (pFilRam[ UsAryId ].UsRamAdd != 0xFFFF) {
		RamWrite32A(pFilRam[ UsAryId ].UsRamAdd, pFilRam[ UsAryId ].UlRamDat);
		UsAryId++;
		if (UsAryId > FILRAMTAB) {
			return OIS_FW_POLLING_FAIL;
		}
	}
#ifdef	CATCHMODE
#else	//CATCHMODE
	RamWrite32A(gxigain2, 0x00000000);
#endif	//CATCHMODE

	RegWriteA(WC_RAMACCXY, 0x00);			// 0x018D	Simultaneously Setting Off

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
#ifdef	INI_SHORT3
#else	//INI_SHORT3
	RegWriteA(WC_RAMACCXY, 0x00);			// 0x018D	Filter copy off
#endif	//INI_SHORT3
#ifdef	CATCHMODE
#ifdef	CORRECT_1DEG
	SelectIstpMod(ON);
#else
	SelectIstpMod(OFF);
#endif
#else
	//SelectIstpMod(OFF);
#endif // CATCHMODE

	lc898122a_init_pan_average();							/* Average setting */

	/* OIS */
	if ((UcVerLow == 0x00) ) {							// LGIT 1st
		RegWriteA(CMSDAC0, BIAS_CUR_OIS_C000);		// 0x0251	Hall DAC Current
		RegWriteA(OPGSEL0, AMP_GAIN_X_C000);			// 0x0253	Hall amp Gain X
		RegWriteA(OPGSEL1, AMP_GAIN_Y_C000);			// 0x0254	Hall amp Gain Y
	} else if (UcVerLow == 0x01 ) {						// LGIT 3rd
		RegWriteA(CMSDAC0, BIAS_CUR_OIS_C001);		// 0x0251	Hall DAC Current
		RegWriteA(OPGSEL0, AMP_GAIN_X_C001);			// 0x0253	Hall amp Gain X
		RegWriteA(OPGSEL1, AMP_GAIN_Y_C001);			// 0x0254	Hall amp Gain Y
	} else if (UcVerLow == 0x02 ) {						// LGIT 4th
		RegWriteA(CMSDAC0, BIAS_CUR_OIS_C002);		// 0x0251	Hall DAC Current
		RegWriteA(OPGSEL0, AMP_GAIN_X_C002);			// 0x0253	Hall amp Gain X
		RegWriteA(OPGSEL1, AMP_GAIN_Y_C002);			// 0x0254	Hall amp Gain Y
	} else if (UcVerLow == 0x10 ) {						//MTM
		RegWriteA(CMSDAC0, BIAS_CUR_OIS_C010);		// 0x0251	Hall DAC Current
		RegWriteA(OPGSEL0, AMP_GAIN_X_C010);			// 0x0253	Hall amp Gain X
		RegWriteA(OPGSEL1, AMP_GAIN_Y_C010);			// 0x0254	Hall amp Gain Y
	} else if (UcVerLow == 0x11 ) {
		RegWriteA(CMSDAC0, BIAS_CUR_OIS_C011);		// 0x0251	Hall DAC Current
		RegWriteA(OPGSEL0, AMP_GAIN_X_C011);			// 0x0253	Hall amp Gain X
		RegWriteA(OPGSEL1, AMP_GAIN_Y_C011);			// 0x0254	Hall amp Gain Y
	}

	/* AF */
	if (UcVerLow == 0x00) {								// LGIT 2nd Act. 141007
		RegWriteA(CMSDAC1, BIAS_CUR_AF_C000);		// 0x0252	Hall Dac current
		RegWriteA(OPGSEL2, AMP_GAIN_AF_C000);		// 0x0255	Hall amp Gain AF
	} else if (UcVerLow == 0x01 ) {						// LGIT 3rd Act. 141105
		RegWriteA(CMSDAC1, BIAS_CUR_AF_C001);		// 0x0252	Hall Dac current
		RegWriteA(OPGSEL2, AMP_GAIN_AF_C001);		// 0x0255	Hall amp Gain AF
	} else if (UcVerLow == 0x02 ) {						// LGIT 4th Act. 141208
		RegWriteA(CMSDAC1, BIAS_CUR_AF_C002);		// 0x0252	Hall Dac current
		RegWriteA(OPGSEL2, AMP_GAIN_AF_C002);		// 0x0255	Hall amp Gain AF
	} else if (UcVerLow == 0x10) {						// MTM
		RegWriteA(CMSDAC1, BIAS_CUR_AF_C010);		// 0x0252	Hall Dac current
		RegWriteA(OPGSEL2, AMP_GAIN_AF_C010);		// 0x0255	Hall amp Gain AF
	} else if (UcVerLow == 0x11 ) {
		RegWriteA(CMSDAC1, BIAS_CUR_AF_C011);		// 0x0252	Hall Dac current
		RegWriteA(OPGSEL2, AMP_GAIN_AF_C011);		// 0x0255	Hall amp Gain AF
	}
	/* OSC Clock value */
	if (((uint8_t)StCalDat.UcOscVal == 0x00) || ((uint8_t)StCalDat.UcOscVal == 0xFF)) {
		RegWriteA(OSCSET, OSC_INI);				// 0x0257	OSC ini
	} else {
		RegWriteA(OSCSET, StCalDat.UcOscVal);	// 0x0257
	}

	/* adjusted value */
	/* Gyro X axis Offset */
	if ((StCalDat.StGvcOff.UsGxoVal == 0x0000) || (StCalDat.StGvcOff.UsGxoVal == 0xFFFF)) {
		RegWriteA(IZAH, DGYRO_OFST_XH);		// 0x02A0		Set Offset High byte
		RegWriteA(IZAL, DGYRO_OFST_XL);		// 0x02A1		Set Offset Low byte
	} else {
		RegWriteA(IZAH, (uint8_t)(StCalDat.StGvcOff.UsGxoVal >> 8));	// 0x02A0		Set Offset High byte
		RegWriteA(IZAL, (uint8_t)(StCalDat.StGvcOff.UsGxoVal));		// 0x02A1		Set Offset Low byte
	}
	/* Gyro Y axis Offset */
	if ((StCalDat.StGvcOff.UsGyoVal == 0x0000) || (StCalDat.StGvcOff.UsGyoVal == 0xFFFF)) {
		RegWriteA(IZBH, DGYRO_OFST_YH);		// 0x02A2		Set Offset High byte
		RegWriteA(IZBL, DGYRO_OFST_YL);		// 0x02A3		Set Offset Low byte
	} else {
		RegWriteA(IZBH, (uint8_t)(StCalDat.StGvcOff.UsGyoVal >> 8));	// 0x02A2		Set Offset High byte
		RegWriteA(IZBL, (uint8_t)(StCalDat.StGvcOff.UsGyoVal));		// 0x02A3		Set Offset Low byte
	}

	/* Ram Access */
	lc898122a_ram_mode(ON);	/* 16bit Fix mode */

	/* OIS adjusted parameter */
	/* Hall X axis Bias,Offset,Lens center */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_HXADJ - EXE_END))) {
		RamWriteA(DAXHLO, DAHLXO_INI);				// 0x1479
		RamWriteA(DAXHLB, DAHLXB_INI);				// 0x147A
	} else {
		RamWriteA(DAXHLO, StCalDat.StHalAdj.UsHlxOff);	// 0x1479
		RamWriteA(DAXHLB, StCalDat.StHalAdj.UsHlxGan);	// 0x147A
	}

	/* Hall Y axis Bias,Offset,Lens center */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_HYADJ - EXE_END))) {
		RamWriteA(DAYHLO, DAHLYO_INI);				// 0x14F9
		RamWriteA(DAYHLB, DAHLYB_INI);				// 0x14FA
	} else {
		RamWriteA(DAYHLO, StCalDat.StHalAdj.UsHlyOff);	// 0x14F9
		RamWriteA(DAYHLB, StCalDat.StHalAdj.UsHlyGan);	// 0x14FA
	}

	/* Hall X axis Loop Gain */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_LXADJ - EXE_END))) {
		RamWriteA(sxg, SXGAIN_INI);			// 0x10D3
	} else {
		RamWriteA(sxg, StCalDat.StLopGan.UsLxgVal);	// 0x10D3
	}

	/* Hall Y axis Loop Gain */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjHallF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_LYADJ - EXE_END))) {
		RamWriteA(syg, SYGAIN_INI);			// 0x11D3
	} else {
		RamWriteA(syg, StCalDat.StLopGan.UsLygVal);	// 0x11D3
	}

	/* Hall Z AF adjusted parameter */
	if ((StCalDat.UsAdjHallZF == 0x0000) || (StCalDat.UsAdjHallZF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_HZADJ - EXE_END))) {
		RamWriteA(DAZHLO,		DAHLZO_INI);		// 0x1529
		RamWriteA(DAZHLB, DAHLZB_INI);		// 0x152A
		RamWriteA(OFF4Z, AFOFF4Z_INI);		// 0x145F
		RamWriteA(OFF6Z, AFOFF6Z_INI);		// 0x1510
		RegWriteA(DRVFC4AF, (uint8_t)(AFDROF_INI << 3));					// 0x0084
	} else {
		RamWriteA(DAZHLO, StCalDat.StHalAdj.UsHlzOff);		// 0x1529
		RamWriteA(DAZHLB, StCalDat.StHalAdj.UsHlzGan);		// 0x152A
		RamWriteA(OFF4Z, StCalDat.StHalAdj.UsAdzOff);		// 0x145F
		RamWriteA(OFF6Z, AFOFF6Z_INI);					// 0x1510
		RegWriteA(DRVFC4AF, (uint8_t)(StCalDat.StAfOff.UcAfOff << 3));				// 0x0084
	}

	if ((StCalDat.UsAdjHallZF == 0x0000) || (StCalDat.UsAdjHallZF == 0xFFFF) || (StCalDat.UsAdjHallF & (EXE_LZADJ - EXE_END))) {
		RamWriteA(afg, SZGAIN_INI);		// 0x1216
	} else {
		RamWriteA(afg, StCalDat.StLopGan.UsLzgVal);	// 0x1216
	}

	/* Ram Access */
	lc898122a_ram_mode(OFF);	/* 32bit Float mode */

	/* Gyro X axis Gain */
	if ((StCalDat.UlGxgVal == 0x00000000) || (StCalDat.UlGxgVal == 0xFFFFFFFF)) {
		RamWrite32A(gxzoom, GXGAIN_INI);				// 0x1020 Gyro X axis Gain adjusted value
	} else {
		RamWrite32A(gxzoom, StCalDat.UlGxgVal);		// 0x1020 Gyro X axis Gain adjusted value
	}

	/* Gyro Y axis Gain */
	if ((StCalDat.UlGygVal == 0x00000000) || (StCalDat.UlGygVal == 0xFFFFFFFF)) {
		RamWrite32A(gyzoom, GYGAIN_INI);				// 0x1120 Gyro Y axis Gain adjusted value
	} else {
		RamWrite32A(gyzoom, StCalDat.UlGygVal);		// 0x1120 Gyro Y axis Gain adjusted value
	}

	lc898122a_ram_mode(ON);	/* 16bit Fix mode */
	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjLensF == 0xFFFF) || (StCalDat.UsAdjLensF & (EXE_CXADJ - EXE_END))) {
		RamWriteA(OFF0Z, HXOFF0Z_INI);				// 0x1450
	} else {
		RamWriteA(OFF0Z, StCalDat.StLenCen.UsLsxVal);	// 0x1450
	}

	if ((StCalDat.UsAdjHallF == 0x0000) || (StCalDat.UsAdjLensF == 0xFFFF) || (StCalDat.UsAdjLensF & (EXE_CYADJ - EXE_END))) {
		RamWriteA(OFF1Z, HYOFF1Z_INI);				// 0x14D0
	} else {
		RamWriteA(OFF1Z, StCalDat.StLenCen.UsLsyVal);	// 0x14D0
	}
	lc898122a_ram_mode(OFF);		/* 32bit Float mode */

	if ((UcVerLow == 0x00) || (UcVerLow == 0x01) || (UcVerLow == 0x02) ) {
		RamWrite32A(sxq, SXQ_INI_LGIT);			// 0x10E5	X axis output direction initial value
		RamWrite32A(syq, SYQ_INI_LGIT);			// 0x11E5	Y axis output direction initial value
	} else if (UcVerLow == 0x10 || (UcVerLow == 0x11) ) {
		RamWrite32A(sxq, SXQ_INI_MTM);			// 0x10E5	X axis output direction initial value
		RamWrite32A(syq, SYQ_INI_MTM);			// 0x11E5	Y axis output direction initial value
	}
	RamWrite32A(afag, AFAG_INI);				// 0x1203	CL-AF output direction initial value

#ifdef	CATCHMODE
	RamWrite32A(gx45g, G_45G_INI);			// 0x1000
	RamWrite32A(gy45g, G_45G_INI);			// 0x1100
	ClrGyr(0x00FF , CLR_FRAM1);		// Gyro Delay RAM Clear
#endif	//CATCHMODE
	RegWriteA(PWMA, 0xC0);				// 0x0010		PWM enable

	RegWriteA(STBB0, 0xDF);				// 0x0250	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]

	RegWriteA(WC_EQSW, 0x10);				// 0x01E0 The RAM value is copied 1502h to 151Fh by Filter EQON.
	RamWrite32A(afstmg, 0x3F800000);			// 0x1202

	RegWriteA(WC_MESLOOP1, 0x02);			// 0x0193
	RegWriteA(WC_MESLOOP0, 0x00);			// 0x0192
	RegWriteA(WC_AMJLOOP1, 0x02);			// 0x01A3
	RegWriteA(WC_AMJLOOP0, 0x00);			// 0x01A2
#ifdef	CATCHMODE
	RamWrite32A(sxgx, GXQ_INI);	//0xBF800000);		// 0x10B8	INV Gyro signal polarity
	RamWrite32A(sygy, GYQ_INI);	//0xBF800000);		// 0x11B8	INV Gyro signal polarity

	SetPanTiltMode(OFF);					/* Pan/Tilt OFF */

	SetH1cMod(ACTMODE);					/* Lvl Change Active mode */

	DrvSw(ON);							/* 0x0001		Driver Mode setting */

	RegWriteA(WC_EQON, 0x01);			// 0x0101	Filter ON
#else	//CATCHMODE

	if ((UcVerLow == 0x00) || (UcVerLow == 0x01) || (UcVerLow == 0x10) ) {
		// I Filter X    2014.05.19				// 2s
		RamWrite32A(gxia_1, 0x3760E040);		// 0x1043	0.1Hz
		RamWrite32A(gxib_1, 0xB332EF82);		// 0x1044	Down
		RamWrite32A(gxic_1, 0x3332EF82);		// 0x1045	Up

		RamWrite32A(gxia_a, 0x3760E040);		// 0x1046	0.1Hz
		RamWrite32A(gxib_a, 0xB332EF82);		// 0x1047	Down
		RamWrite32A(gxic_a, 0x3332EF82);		// 0x1048	Up

		RamWrite32A(gxia_b, 0x3B038040);		// 0x1049	15Hz	 2014.05.27
		RamWrite32A(gxib_b, 0xB332EF82);		// 0x104A	Down
		RamWrite32A(gxic_b, 0x3F800000);		// 0x104B	Up

		RamWrite32A(gxia_c, 0x3760E040);		// 0x104C	0.1Hz
		RamWrite32A(gxib_c, 0xB332EF82);		// 0x104D	Down
		RamWrite32A(gxic_c, 0x3332EF82);		// 0x104E	Up

		// I Filter Y    2014.05.19
		RamWrite32A(gyia_1, 0x3760E040);		// 0x1143	0.1Hz
		RamWrite32A(gyib_1, 0xB332EF82);		// 0x1144	Down
		RamWrite32A(gyic_1, 0x3332EF82);		// 0x1145	Up

		RamWrite32A(gyia_a, 0x3760E040);		// 0x1146	0.1Hz
		RamWrite32A(gyib_a, 0xB332EF82);		// 0x1147	Down
		RamWrite32A(gyic_a, 0x3332EF82);		// 0x1148	Up

		RamWrite32A(gyia_b, 0x3B038040);		// 0x1149	15Hz	 2014.05.27
		RamWrite32A(gyib_b, 0xB332EF82);		// 0x114A	Down
		RamWrite32A(gyic_b, 0x3F800000);		// 0x114B	Up

		RamWrite32A(gyia_c, 0x3760E040);		// 0x114C	0.1Hz
		RamWrite32A(gyib_c, 0xB332EF82);		// 0x114D	Down
		RamWrite32A(gyic_c, 0x3332EF82);		// 0x114E	Up
	} else {	// (UcVerLow == 0x02) || (UcVerLow == 0x11)
		// I Filter X    2014.05.19				// 2s
		RamWrite32A(gxia_1, 0x37A8A800);		// 0x1043	0.15Hz
		RamWrite32A(gxib_1, 0xB332EF82);		// 0x1044	Down
		RamWrite32A(gxic_1, 0x3332EF82);		// 0x1045	Up

		RamWrite32A(gxia_a, 0x37A8A800);		// 0x1046	0.15Hz
		RamWrite32A(gxib_a, 0xB332EF82);		// 0x1047	Down
		RamWrite32A(gxic_a, 0x3332EF82);		// 0x1048	Up

		RamWrite32A(gxia_b, 0x3B038040);		// 0x1049	15Hz	 2014.05.27
		RamWrite32A(gxib_b, 0xB332EF82);		// 0x104A	Down
		RamWrite32A(gxic_b, 0x3F800000);		// 0x104B	Up

		RamWrite32A(gxia_c, 0x37A8A800);		// 0x104C	0.15Hz
		RamWrite32A(gxib_c, 0xB332EF82);		// 0x104D	Down
		RamWrite32A(gxic_c, 0x3332EF82);		// 0x104E	Up

		// I Filter Y    2014.05.19
		RamWrite32A(gyia_1, 0x37A8A800);		// 0x1143	0.15Hz
		RamWrite32A(gyib_1, 0xB332EF82);		// 0x1144	Down
		RamWrite32A(gyic_1, 0x3332EF82);		// 0x1145	Up

		RamWrite32A(gyia_a, 0x37A8A800);		// 0x1146	0.15Hz
		RamWrite32A(gyib_a, 0xB332EF82);		// 0x1147	Down
		RamWrite32A(gyic_a, 0x3332EF82);		// 0x1148	Up

		RamWrite32A(gyia_b, 0x3B038040);		// 0x1149	15Hz	 2014.05.27
		RamWrite32A(gyib_b, 0xB332EF82);		// 0x114A	Down
		RamWrite32A(gyic_b, 0x3F800000);		// 0x114B	Up

		RamWrite32A(gyia_c, 0x37A8A800);		// 0x114C	0.15Hz
		RamWrite32A(gyib_c, 0xB332EF82);		// 0x114D	Down
		RamWrite32A(gyic_c, 0x3332EF82);		// 0x114E	Up
	}
	// gxgain
	RamWrite32A(gxgain_1, 0x3F800000);		// 0x1073	1
	RamWrite32A(gxgain_1d, 0xB7B2F402);		// 0x1074	Down
	RamWrite32A(gxgain_1u, 0x37B2F402);		// 0x1075	Up	//2.0s	//140314

	RamWrite32A(gxgain_a, 0x3F800000);		// 0x1076	1
	RamWrite32A(gxgain_2d, 0xB8DFB102);		// 0x1077	Down
	RamWrite32A(gxgain_2u, 0x38DFB102);		// 0x1078	Up

	RamWrite32A(gxgain_b, 0x00000000);		// 0x1079	Cut Off
	RamWrite32A(gxgain_3d, 0xBF800000);		// 0x107A	Down	//0.0s
	RamWrite32A(gxgain_3u, 0x38DFB102);		// 0x107B	Up

	RamWrite32A(gxgain_c, 0x3F800000);		// 0x107C	1
	RamWrite32A(gxgain_4d, 0xB8DFB102);		// 0x107D	Down	//0.4s
	RamWrite32A(gxgain_4u, 0x38DFB102);		// 0x107E	Up	//0.4s

	// gygain
	RamWrite32A(gygain_1, 0x3F800000);		// 0x1173	1
	RamWrite32A(gygain_1d, 0xB7B2F402);		// 0x1174	Down
	RamWrite32A(gygain_1u, 0x37B2F402);		// 0x1175	Up	//2.0s	//140314

	RamWrite32A(gygain_a, 0x3F800000);		// 0x1176	1
	RamWrite32A(gygain_2d, 0xB8DFB102);		// 0x1177	Down
	RamWrite32A(gygain_2u, 0x38DFB102);		// 0x1178	Up

	RamWrite32A(gygain_b, 0x00000000);		// 0x1179	Cut Off
	RamWrite32A(gygain_3d, 0xBF800000);		// 0x117A	Down	//0.0s
	RamWrite32A(gygain_3u, 0x38DFB102);		// 0x117B	Up

	RamWrite32A(gygain_c, 0x3F800000);		// 0x117C	1
	RamWrite32A(gygain_4d, 0xB8DFB102);		// 0x117D	Down	//0.4s
	RamWrite32A(gygain_4u, 0x38DFB102);		// 0x117E	Up	//0.4s

	// gxistp								//
	RamWrite32A(gxistp_1, 0x00000000);		// 0x1083	Cut Off
	RamWrite32A(gxistp_1d, 0xBF800000);		// 0x1084	Down
	RamWrite32A(gxistp_1u, 0x3F800000);		// 0x1085	Up

	RamWrite32A(gxistp_a, 0x00000000);		// 0x1086	Cut Off
	RamWrite32A(gxistp_2d, 0xBF800000);		// 0x1087	Down
	RamWrite32A(gxistp_2u, 0x3F800000);		// 0x1088	Up

	RamWrite32A(gxistp_b, 0x38D1B700);		// 0x1089	-80dB
	RamWrite32A(gxistp_3d, 0xBF800000);		// 0x108A	Down
	RamWrite32A(gxistp_3u, 0x3F800000);		// 0x108B	Up

	RamWrite32A(gxistp_c, 0x00000000);		// 0x108C	Cut Off
	RamWrite32A(gxistp_4d, 0xBF800000);		// 0x108D	Down
	RamWrite32A(gxistp_4u, 0x3F800000);		// 0x108E	Up

	// gyistp
	RamWrite32A(gyistp_1, 0x00000000);		// 0x1183	Cut Off
	RamWrite32A(gyistp_1d, 0xBF800000);		// 0x1184	Down
	RamWrite32A(gyistp_1u, 0x3F800000);		// 0x1185	Up

	RamWrite32A(gyistp_a, 0x00000000);		// 0x1186	Cut Off
	RamWrite32A(gyistp_2d, 0xBF800000);		// 0x1187	Down
	RamWrite32A(gyistp_2u, 0x3F800000);		// 0x1188	Up

	RamWrite32A(gyistp_b, 0x38D1B700);		// 0x1189	-80dB
	RamWrite32A(gyistp_3d, 0xBF800000);		// 0x118A	Down
	RamWrite32A(gyistp_3u, 0x3F800000);		// 0x118B	Up

	RamWrite32A(gyistp_c, 0x00000000);		// 0x118C	Cut Off
	RamWrite32A(gyistp_4d, 0xBF800000);		// 0x118D	Down
	RamWrite32A(gyistp_4u, 0x3F800000);		// 0x118E	Up

	RamWrite32A(sxgx, GXQ_INI);	//0xBF800000);		// 0x10B8	INV Gyro signal polarity
	RamWrite32A(sygy, GYQ_INI);	//0xBF800000);		// 0x11B8	INV Gyro signal polarity

#ifdef	INI_SHORT2
#else	//INI_SHORT2
	lc898122a_pantilt_mode(OFF);	/* Pan/Tilt OFF */
#endif	//INI_SHORT2

#ifdef	INI_SHORT2
#else	//INI_SHORT2
	lc898122a_set_di_filter(0);				/* DI initial value */
#endif	//INI_SHORT2

#ifdef H1COEF_CHANGER
	lc898122a_set_h1c_mode(ACTMODE);		/* Lvl Change Active mode */
//	SetH1cMod(MOVMODE);					/* Lvl Change Active mode */
#endif

	lc898122a_driver_mode(ON);				/* 0x0001 Driver Mode setting */

	RegWriteA(WC_EQON, 0x01);				// 0x0101	Filter ON

	RegWriteA(WG_NPANST12BTMR, 0x01);		// 0x0167
	lc898122a_pantilt_mode(ON);					/* Pan/Tilt */
	RegWriteA(WG_PANSTT6, 0x44);				// 0x010A
	RegWriteA(WG_PANSTT6, 0x11);				// 0x010A
#endif	//CATCHMODE
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
			RegWriteA(DRVFC, 0xF0);			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
		} else {
#ifdef	PWM_BREAK
			RegWriteA(DRVFC, 0x00);			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
#else
			RegWriteA(DRVFC, 0xC0);			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE1
#endif
		}
	} else {
		if (UcPwmMod == PWMMOD_CVL) {
			RegWriteA(DRVFC, 0x30);				// 0x0001	Drvier Block Ena=0
		} else {
#ifdef	PWM_BREAK
			RegWriteA(DRVFC, 0x00);				// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
#else
			RegWriteA(DRVFC, 0x00);				// 0x0001	Drvier Block Ena=0
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
		RegWriteA(WC_RAMACCMOD, 0x00);		// 0x018C		GRAM Access Float32bit
		break;
	case ON:
		RegWriteA(WC_RAMACCMOD, 0x31);		// 0x018C		GRAM Access Fix16bit
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
	uint8_t	UcCmdSts;

	UcCmdSts	= EXE_END;

	lc898122a_gyro_control(OFF);	/* Gyro OFF */

	if (!UcCmdPar) {										// X,Y Centering
		lc898122a_stabilizer_on(); /* X,Y Centering */
		/* Slope Mode */

	} else if (UcCmdPar == 0x01) {							// X Centering Only
		/* X Centering Only */
		lc898122a_servo_command(X_DIR, ON);	/* X only Servo ON */
		lc898122a_servo_command(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) {							// Y Centering Only
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

	if (UcSwcCon) {
		if (!UcDirSel) {								// X Direction
			RegWriteA(WH_EQSWX, 0x03);				// 0x0170
			RamWrite32A(sxggf, 0x00000000);		// 0x10B5
		} else {										// Y Direction
			RegWriteA(WH_EQSWY, 0x03);				// 0x0171
			RamWrite32A(syggf, 0x00000000);		// 0x11B5
		}
	} else {
		if (!UcDirSel) {								// X Direction
			RegWriteA(WH_EQSWX, 0x02);				// 0x0170
			RamWrite32A(SXLMT, 0x00000000);		// 0x1477
		} else {										// Y Direction
			RegWriteA(WH_EQSWY, 0x02);				// 0x0171
			RamWrite32A(SYLMT, 0x00000000);		// 0x14F7
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
	uint8_t	UcRegValx,UcRegValy;					// Register value
	uint8_t	UcRegIni;
	uint8_t	UcCnt;

	RegReadA(WH_EQSWX, &UcRegValx);			// 0x0170
	RegReadA(WH_EQSWY, &UcRegValy);			// 0x0171

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {

		RegWriteA(WH_SMTSRVON, 0x01);				// 0x017C		Smooth Servo ON

		lc898122a_servo_command(X_DIR, ON);
		lc898122a_servo_command(Y_DIR, ON);

		UcCnt = 0;
		UcRegIni = 0x11;
		while ((UcRegIni & 0x77) != 0x66) {
			RegReadA(RH_SMTSRVSTT, &UcRegIni);		// 0x01F8		Smooth Servo phase read
			lc898122a_wait_time(1);
			UcCnt = UcCnt + 1;
			if (UcCnt > 60) {
				break;
			}
		}

		RegWriteA(WH_SMTSRVON, 0x00);				// 0x017C		Smooth Servo OFF

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

	// Return HPF Setting
	RegWriteA(WG_SHTON, 0x00);									// 0x0107

	if (UcGyrCon == ON) {												// Gyro ON

#ifdef GAIN_CONT
		/* Gain3 Register */
		//AutoGainControlSw(ON);											/* Auto Gain Control Mode ON */
#endif
		lc898122a_gyro_ram_clear(0x000E, CLR_FRAM1);	/* Gyro Delay RAM Clear */

		RamWrite32A(sxggf, 0x3F800000);	// 0x10B5
		RamWrite32A(syggf, 0x3F800000);	// 0x11B5

#ifdef	CATCHMODE
//		RamWrite32A(pxmbb, 0x00000000);	// 0x10A5
//		RamWrite32A(pymbb, 0x00000000);	// 0x11A5
#else	//CATCHMODE
		RamWrite32A(gxib_1, 0xBF800000);		// Down
		RamWrite32A(gyib_1, 0xBF800000);		// Down
//		RamWrite32A(gxib_1, 0xB261CF49);		// Down
//		RamWrite32A(gyib_1, 0xB261CF49);		// Down
		lc898122a_pantilt_mode(OFF);						//2014.05.19

		RegWriteA(WG_PANSTT6, 0x00);				// 0x010A
		//SetPanTiltMode(OFF);						// Pantilt OFF
#endif	//CATCHMODE
	} else if (UcGyrCon == SPC) {					// Gyro ON for LINE


#ifdef	GAIN_CONT
		/* Gain3 Register */
		//AutoGainControlSw(ON);											/* Auto Gain Control Mode ON */
#endif

		RamWrite32A(sxggf, 0x3F800000);	// 0x10B5
		RamWrite32A(syggf, 0x3F800000);	// 0x11B5


	} else {															// Gyro OFF

		RamWrite32A(sxggf, 0x00000000);	// 0x10B5
		RamWrite32A(syggf, 0x00000000);	// 0x11B5

#ifdef	GAIN_CONT
		/* Gain3 Register */
		//AutoGainControlSw(OFF);											/* Auto Gain Control Mode OFF */
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
		RegWriteA(WG_PANON, 0x00);			// 0x0109	X,Y Pan/Tilt Function OFF

		break;
	case ON:
		RegWriteA(WG_PANON, 0x01);			// 0x0109	X,Y Pan/Tilt Function ON
		break ;
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
	uint8_t	UcRamClr;

	/*Select Filter to clear*/
	RegWriteA(WC_RAMDLYMOD1, (uint8_t)(UsClrFil >> 8));		// 0x018F		FRAM Initialize Hbyte
	RegWriteA(WC_RAMDLYMOD0, (uint8_t)UsClrFil);				// 0x018E		FRAM Initialize Lbyte

	/*Enable Clear*/
	RegWriteA(WC_RAMINITON, UcClrMod);	// 0x0102	[ - | - | - | - ][ - | - | Ram Clr | Coef Clr ]

	/*Check RAM Clear complete*/
	do {
		RegReadA(WC_RAMINITON, &UcRamClr);
		UcRamClr &= UcClrMod;
	} while (UcRamClr != 0x00);

	return 0;
}

void lc898122a_mem_clear(uint8_t *NcTgtPtr, uint16_t UsClrSiz)
{
	uint16_t	UsClrIdx;

	for (UsClrIdx = 0; UsClrIdx < UsClrSiz; UsClrIdx++) {
		*NcTgtPtr	= 0;
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
	uint8_t	UcFlgVal;
	uint8_t	UcCntPla;
	UcFlgVal	= 1;
	UcCntPla	= 0;

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
//********************************************************************************
// Function Name	: AutoGainContIni
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Gain Control initial function
// History			: First edition							2014.09.16 Y.Shigeoka
//********************************************************************************
#ifdef	CATCHMODE
#define	TRI_LEVEL		0x3A031280		/* 0.0005 */
#define	TIMELOW			0x50			/* */
#define	TIMEHGH			0x05			/* */
#define	TIMEBSE			0x5D			/* 3.96ms */
#define	MONADR			GXXFZ
#define	GANADR			gxadj
#define	XMINGAIN		0x00000000
#define	XMAXGAIN		0x3F800000
#define	YMINGAIN		0x00000000
#define	YMAXGAIN		0x3F800000
#define	XSTEPUP			0x38D1B717		/* 0.0001	 */
#define	XSTEPDN			0xBD4CCCCD		/* -0.05	 */
#define	YSTEPUP			0x38D1B717		/* 0.0001	 */
#define	YSTEPDN			0xBD4CCCCD		/* -0.05	 */
#else	//CATCHMODE
#define	TRI_LEVEL		0x3B23D70A		/* 0.0025 */		//140314
#define	TIMELOW			0x40			/* */				//140314
#define	TIMEHGH			0x01			/* */
#define	TIMEBSE			0x5D			/* 3.96ms */
#define	MONADR			GXXFZ
#define	GANADR			gxadj
#define	XMINGAIN		0x00000000
#define	XMAXGAIN		0x3F800000
#define	YMINGAIN		0x00000000
#define	YMAXGAIN		0x3F800000
#define	XSTEPUP			0x3F800000		/* 1.0	 */
#define	XSTEPDN			0xBF80000D		/* -1.0  */
#define	YSTEPUP			0x3F800000		/* 1.0	 */
#define	YSTEPDN			0xBF800000		/* -1.0  */
#endif	//CATCHMODE
void	AutoGainContIni(void )
{
	RamWrite32A(gxlevlow, TRI_LEVEL);					// 0x10AE	Low Th
	RamWrite32A(gylevlow, TRI_LEVEL);					// 0x11AE	Low Th
	RamWrite32A(gxadjmin, XMINGAIN);					// 0x1094	Low gain
	RamWrite32A(gxadjmax, XMAXGAIN);					// 0x1095	Hgh gain
	RamWrite32A(gxadjdn, XSTEPDN);					// 0x1096	-step
	RamWrite32A(gxadjup, XSTEPUP);					// 0x1097	+step
	RamWrite32A(gyadjmin, YMINGAIN);					// 0x1194	Low gain
	RamWrite32A(gyadjmax, YMAXGAIN);					// 0x1195	Hgh gain
	RamWrite32A(gyadjdn, YSTEPDN);					// 0x1196	-step
	RamWrite32A(gyadjup, YSTEPUP);					// 0x1197	+step

	RegWriteA(WG_LEVADD, (unsigned char)MONADR);		// 0x0120	Input signal
	RegWriteA(WG_LEVTMR,		TIMEBSE);				// 0x0123	Base Time
	RegWriteA(WG_LEVTMRLOW,	TIMELOW);				// 0x0121	X Low Time
	RegWriteA(WG_LEVTMRHGH,	TIMEHGH);				// 0x0122	X Hgh Time
	RegWriteA(WG_ADJGANADD, (unsigned char)GANADR);		// 0x0128	control address
#ifdef	INI_SHORT3
#else	//INI_SHORT3
	RegWriteA(WG_ADJGANGO,			0x00);				// 0x0108	manual off
#endif	//INI_SHORT3
}
/********************************************************************************
 Function Name	: lc898122a_auto_gain_con
 Retun Value		: NON
 Argment Value	: 0 :OFF  1:ON
 Explanation		: Select Gyro Signal Function
 History			: First edition							2010.11.30 Y.Shigeoka
*********************************************************************/
void lc898122a_auto_gain_con(uint8_t UcModeSw)
{

	if (UcModeSw == OFF) {
		RegWriteA(WG_ADJGANGXATO, 0xA0);					// 0x0129	X exe off
		RegWriteA(WG_ADJGANGYATO, 0xA0);					// 0x012A	Y exe off
		RamWrite32A(GANADR		, XMAXGAIN);			// Gain Through
		RamWrite32A(GANADR | 0x0100, YMAXGAIN);			// Gain Through
	} else {
		RegWriteA(WG_ADJGANGXATO, 0xA3);					// 0x0129	X exe on
		RegWriteA(WG_ADJGANGYATO, 0xA3);					// 0x012A	Y exe on
	}
}
#ifdef	CATCHMODE
//********************************************************************************
// Function Name	: StartUpGainContIni
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Start UP Gain Control initial function
// History			: First edition							2014.09.16 Y.Shigeoka
//********************************************************************************
#define	ST_TRI_LEVEL		0x3A031280		/* 0.0005 */
#define	ST_TIMELOW			0x00			/* */
#define	ST_TIMEHGH			0x00			/* */
#define	ST_TIMEBSE			0x00			/* */
#define	ST_MONADR			GXXFZ
#define	ST_GANADR			pxmbb
#define	ST_XMINGAIN		0x3A031240		/* 0.0005 Target gain 0x10A5*/
#define	ST_XMAXGAIN		0x3C031280		/* 0.0080 Initial gain*/
#define	ST_YMINGAIN		0x3A031240
#define	ST_YMAXGAIN		0x3C031280
#define	ST_XSTEPUP			0x3F800000		/* 1	 */
#define	ST_XSTEPDN			0xB3E50F84		/* -0.0000001	 */
#define	ST_YSTEPUP			0x3F800000		/* 1	 */
#define	ST_YSTEPDN			0xB3E50F84		/* -0.05	 */

void	StartUpGainContIni(void )
{
	RamWrite32A(gxlevlow, ST_TRI_LEVEL);					// 0x10AE	Low Th
	RamWrite32A(gylevlow, ST_TRI_LEVEL);					// 0x11AE	Low Th
	RamWrite32A(gxadjmin, ST_XMINGAIN);					// 0x1094	Low gain
	RamWrite32A(gxadjmax, ST_XMAXGAIN);					// 0x1095	Hgh gain
	RamWrite32A(gxadjdn, ST_XSTEPDN);					// 0x1096	-step
	RamWrite32A(gxadjup, ST_XSTEPUP);					// 0x1097	+step
	RamWrite32A(gyadjmin, ST_YMINGAIN);					// 0x1194	Low gain
	RamWrite32A(gyadjmax, ST_YMAXGAIN);					// 0x1195	Hgh gain
	RamWrite32A(gyadjdn, ST_YSTEPDN);					// 0x1196	-step
	RamWrite32A(gyadjup, ST_YSTEPUP);					// 0x1197	+step

	RegWriteA(WG_LEVADD, (unsigned char)ST_MONADR);		// 0x0120	Input signal
	RegWriteA(WG_LEVTMR,		ST_TIMEBSE);				// 0x0123	Base Time
	RegWriteA(WG_LEVTMRLOW,	ST_TIMELOW);				// 0x0121	X Low Time
	RegWriteA(WG_LEVTMRHGH,	ST_TIMEHGH);				// 0x0122	X Hgh Time
	RegWriteA(WG_ADJGANADD, (unsigned char)ST_GANADR);		// 0x0128	control address
	RegWriteA(WG_ADJGANGO, 0x00);					// 0x0108	manual off
}
//********************************************************************************
// Function Name	: InitGainControl
// Retun Value		: NON
// Argment Value	: OFF,  ON
// Explanation		: Gain Control function
// History			: First edition							2014.09.16 Y.Shigeoka
//********************************************************************************
unsigned char	InitGainControl(unsigned char uc_mode )
{
	unsigned char	uc_rtval;

	uc_rtval = 0x00;

	switch (uc_mode) {
	case 0x00:
		RamWrite32A(gx2x4xb, 0x00000000);		// 0x1021
		RamWrite32A(gy2x4xb, 0x00000000);		// 0x1121
		RegWriteA(WG_ADJGANGO, 0x22);					// 0x0108	manual on to go to max
		uc_rtval = 0x22;
		while (uc_rtval) {

			RegReadA(WG_ADJGANGO, &uc_rtval);			// 0x0108	status read
		};
	case 0x01:
		RamWrite32A(gx2x4xb, 0x00000000);		// 0x1021
		RamWrite32A(gy2x4xb, 0x00000000);		// 0x1121
		RegWriteA(WG_ADJGANGO, 0x11);					// 0x0108	manual on to go to min(initial)
		break;
	case 0x02:
		RegReadA(WG_ADJGANGO,	&uc_rtval);			// 0x0108	status read
		break;
	case 0x03:
		lc898122a_gyro_ram_clear(0x000E , CLR_FRAM1);		// Gyro Delay RAM Clear
		RamWrite32A(gx2x4xb, 0x3F800000);		// 0x1021
		RamWrite32A(gy2x4xb, 0x3F800000);		// 0x1121
		break;
	}

	return (uc_rtval);
}
#endif	//CATCHMOD

#endif	//GAIN_CONT


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
		UcSetNum = (COEFTBL - 1);			/* Set Maximum to COEFTBL-1 */

	UlH1Coefval	= ClDiCof[UcSetNum];

	// Zoom Value Setting
	RamWrite32A(gxh1c, UlH1Coefval);		/* 0x1012 */
	RamWrite32A(gyh1c, UlH1Coefval);		/* 0x1112 */

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
#ifdef	CATCHMODE
	case ACTMODE:				// initial
		IniPtMovMod(OFF);							// Pan/Tilt setting (Still)
		/* enable setting */
		UcH1LvlMod = UcSetNum;
#ifdef	CORRECT_1DEG
		RamWrite32A(gxlmt6L, MINLMT_W);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT_W);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6L, MINLMT_W);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT_W);		/* 0x112E H-Limit */
		RamWrite32A(gxmg,		CHGCOEF_W);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg,		CHGCOEF_W);		/* 0x11AA Change coefficient gain */
#else	//CORRECT_1DEG
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg,		CHGCOEF);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg,		CHGCOEF);		/* 0x11AA Change coefficient gain */
#endif	//CORRECT_1DEG
		RamWrite32A(gxhc_tmp,	DIFIL_S2);	/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp,	DIFIL_S2);	/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x10);			// 0x011B	GmHChrOn[1]=0 Sw OFF	Tokoro 2014.11.28
		break;
	case S2MODE:				// cancel lvl change mode
		RegWriteA(WG_HCHR, 0x10);			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break;
	case MOVMODE:			// Movie mode
		IniPtMovMod(ON);						// Pan/Tilt setting (Movie)
		SelectPtRange(OFF);					// Range narrow
		SelectIstpMod(OFF);					// Range narrow
		RamWrite32A(gxlmt6L, MINLMT_MOV);	/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT_MOV);	/* 0x112D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg, CHGCOEF_MOV);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF_MOV);		/* 0x11AA Change coefficient gain */
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;
	case MOVMODE_W:			// Movie mode (wide)
		IniPtMovMod(ON);							// Pan/Tilt setting (Movie)
		SelectPtRange(ON);					// Range wide
		SelectIstpMod(ON);					// Range wide
		RamWrite32A(gxlmt6L, MINLMT_MOV_W);	/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT_MOV_W);	/* 0x112D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT_W);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6H, MAXLMT_W);		/* 0x112E H-Limit */
		RamWrite32A(gxmg, CHGCOEF_MOV_W);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF_MOV_W);		/* 0x11AA Change coefficient gain */
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;
	case STILLMODE:				// Still mode
		IniPtMovMod(OFF);							// Pan/Tilt setting (Still)
		SelectPtRange(OFF);					// Range narrow
		SelectIstpMod(OFF);					// Range narrow
		UcH1LvlMod = UcSetNum ;
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg,	CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg,	CHGCOEF);			/* 0x11AA Change coefficient gain */
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;
	case STILLMODE_W:			// Still mode (Wide)
		IniPtMovMod(OFF);							// Pan/Tilt setting (Still)
		SelectPtRange(ON);					// Range wide
		SelectIstpMod(ON);					// Range wide
		UcH1LvlMod = UcSetNum ;
		RamWrite32A(gxlmt6L, MINLMT_W);		/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT_W);		/* 0x112D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT_W);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6H, MAXLMT_W);		/* 0x112E H-Limit */
		RamWrite32A(gxmg,	CHGCOEF_W);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg,	CHGCOEF_W);			/* 0x11AA Change coefficient gain */
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;

	default:
		IniPtMovMod(OFF);							// Pan/Tilt setting (Still)
		SelectPtRange(OFF);					// Range narrow
		SelectIstpMod(OFF);					// Range narrow
		UcH1LvlMod = UcSetNum ;
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg,	CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg,	CHGCOEF);			/* 0x11AA Change coefficient gain */
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
#else	//CATCHMODE
	case ACTMODE:				// initial
#ifdef	INI_SHORT2
#else	//INI_SHORT2
		lc898122a_init_pan_mode(OFF);	/* Pan/Tilt setting (Still) */
		/* enable setting */
		/* Zoom Step */
		UlH1Coefval	= ClDiCof[0];
		UcH1LvlMod = 0;
#endif	//INI_SHORT2
		// Limit value Value Setting
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg, CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF);			/* 0x11AA Change coefficient gain */
#ifdef	INI_SHORT2
		RamWrite32A(gxhc_tmp, DIFIL_S2);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, DIFIL_S2);		/* 0x110E Base Coef */
		RamWrite32A(gxh1c, DIFIL_S2);		/* 0x1012 Base Coef */
		RamWrite32A(gyh1c, DIFIL_S2);		/* 0x1112 Base Coef */
#else	//INI_SHORT2
		RamWrite32A(gxhc_tmp, UlH1Coefval);	/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);	/* 0x110E Base Coef */
#endif	//INI_SHORT2
		RegWriteA(WG_HCHR, 0x10);			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break ;
	case S2MODE:				// cancel lvl change mode
		break;
	case MOVMODE:		/* Movie mode */
		lc898122a_init_pan_mode(ON);					/* Pan/Tilt setting (Movie) */
		RamWrite32A(gxlmt6L, MINLMT_MOV);	/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT_MOV);	/* 0x102E H-Limit */
		RamWrite32A(gylmt6L, MINLMT_MOV);	/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT_MOV);	/* 0x112E H-Limit */
		RamWrite32A(gxmg, CHGCOEF_MOV);		/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF_MOV);		/* 0x11AA Change coefficient gain */
#ifdef	INI_SHORT2
#else	//INI_SHORT2
		RamWrite32A(gxhc_tmp, UlH1Coefval);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);		/* 0x110E Base Coef */
#endif	//INI_SHORT2
		RegWriteA(WG_HCHR, 0x12);			// 0x011B	GmHChrOn[1]=1 Sw ON
		break;
	default:
		lc898122a_init_pan_mode(OFF);					/* Pan/Tilt setting (Still) */
		UcH1LvlMod = UcSetNum;
		// Limit value Value Setting
		RamWrite32A(gxlmt6L, MINLMT);		/* 0x102D L-Limit */
		RamWrite32A(gxlmt6H, MAXLMT);		/* 0x102E H-Limit */
		RamWrite32A(gylmt6L, MINLMT);		/* 0x112D L-Limit */
		RamWrite32A(gylmt6H, MAXLMT);		/* 0x112E H-Limit */
		RamWrite32A(gxmg, CHGCOEF);			/* 0x10AA Change coefficient gain */
		RamWrite32A(gymg, CHGCOEF);			/* 0x11AA Change coefficient gain */
#ifdef	INI_SHORT2
#else	//INI_SHORT2
		RamWrite32A(gxhc_tmp, UlH1Coefval);		/* 0x100E Base Coef */
		RamWrite32A(gyhc_tmp, UlH1Coefval);		/* 0x110E Base Coef */
#endif	//INI_SHORT2
		RegWriteA(WG_HCHR, 0x10);			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break;
#endif	//CATCHMODE

	}
}
#endif

/*********************************************************************
 Function Name	: lc898122a_select_gyro_sig
 Retun Value		: NON
 Argment Value	: NON
 Explanation		: Select Gyro Signal Function
 History			: First edition							2010.12.27 Y.Shigeoka
*********************************************************************/
void lc898122a_select_gyro_sig(void)
{

	RegWriteA(GRADR0, GYROX_INI);			// 0x0283	Set Gyro XOUT H~L
	RegWriteA(GRADR1, GYROY_INI);			// 0x0284	Set Gyro YOUT H~L
	/*Start OIS Reading*/
	RegWriteA(GRSEL, 0x02);				// 0x0280	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]
}

#if 1
#define		GYROFF_HIGH_MOBILE 0x1482 //30 DPS
#define		GYROFF_LOW_MOBILE 0xEB7E //-30 DPS
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

	RegWriteA(WC_MES1ADD0, (uint8_t)UsRamAdd);							// 0x0194
	RegWriteA(WC_MES1ADD1, (uint8_t)((UsRamAdd >> 8) & 0x0001));	// 0x0195
	RamWrite32A(MSABS1AV, 0x00000000);				// 0x1041	Clear

	if (!UcMesMod) {
		RegWriteA(WC_MESLOOP1, 0x04);				// 0x0193
		RegWriteA(WC_MESLOOP0, 0x00);				// 0x0192	1024 Times Measure
		RamWrite32A(msmean, 0x3A7FFFF7);				// 0x1230	1/CmMesLoop[15:0]
	} else {
		RegWriteA(WC_MESLOOP1, 0x01);				// 0x0193
		RegWriteA(WC_MESLOOP0, 0x00);				// 0x0192	1 Times Measure
		RamWrite32A(msmean, 0x3F800000);				// 0x1230	1/CmMesLoop[15:0]
	}

	RegWriteA(WC_MESABS, 0x00);						// 0x0198	none ABS
	//BsyWit(WC_MESMODE, 0x01);						// 0x0190	normal Measure
	RegWriteA(WC_MESMODE, 0x01);						// 0x0190	normal Measure
	lc898122a_wait_time(100);							/* Wait 1024 Times Measure Time */
	RegReadA(WC_MESMODE, &UcMesFin);					// 0x0190	normal Measure
	if (0x00 == UcMesFin)
		lc898122a_wait_time(100);

	lc898122a_ram_mode(ON);	/* Fix mode */

	RamReadA(MSABS1AV, (uint16_t *)&SsMesRlt);	// 0x1041

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

	// A/D Offset Clear
	RegWriteA(IZAH, 0x00);	// 0x02A0		Set Offset High byte
	RegWriteA(IZAL, 0x00);	// 0x02A1		Set Offset Low byte
	RegWriteA(IZBH, 0x00);	// 0x02A2		Set Offset High byte
	RegWriteA(IZBL, 0x00);	// 0x02A3		Set Offset Low byte

	//MesFil(THROUGH);				// Set Measure filter
	// Measure Filter1 Setting
	RamWrite32A(mes1aa, 0x3F800000);		// 0x10F0	Through
	RamWrite32A(mes1ab, 0x00000000);		// 0x10F1
	RamWrite32A(mes1ac, 0x00000000);		// 0x10F2
	RamWrite32A(mes1ad, 0x00000000);		// 0x10F3
	RamWrite32A(mes1ae, 0x00000000);		// 0x10F4
	RamWrite32A(mes1ba, 0x3F800000);		// 0x10F5	Through
	RamWrite32A(mes1bb, 0x00000000);		// 0x10F6
	RamWrite32A(mes1bc, 0x00000000);		// 0x10F7
	RamWrite32A(mes1bd, 0x00000000);		// 0x10F8
	RamWrite32A(mes1be, 0x00000000);		// 0x10F9

	// Measure Filter2 Setting
	RamWrite32A(mes2aa, 0x3F800000);		// 0x11F0	Through
	RamWrite32A(mes2ab, 0x00000000);		// 0x11F1
	RamWrite32A(mes2ac, 0x00000000);		// 0x11F2
	RamWrite32A(mes2ad, 0x00000000);		// 0x11F3
	RamWrite32A(mes2ae, 0x00000000);		// 0x11F4
	RamWrite32A(mes2ba, 0x3F800000);		// 0x11F5	Through
	RamWrite32A(mes2bb, 0x00000000);		// 0x11F6
	RamWrite32A(mes2bc, 0x00000000);		// 0x11F7
	RamWrite32A(mes2bd, 0x00000000);		// 0x11F8
	RamWrite32A(mes2be, 0x00000000);		// 0x11F9

	//////////
	// X
	//////////
	RegWriteA(WC_MES1ADD0, 0x00);		// 0x0194
	RegWriteA(WC_MES1ADD1, 0x00);		// 0x0195
	lc898122a_gyro_ram_clear(0x1000, CLR_FRAM1);			/* Measure Filter RAM Clear */
	UsGxoVal = (uint16_t)lc898122a_gen_measure(AD2Z, 0);	/* GYRMON1(0x1110) <- GXADZ(0x144A) */
	RegWriteA(IZAH, (uint8_t)(UsGxoVal >> 8));	/* 0x02A0 Set Offset High byte */
	RegWriteA(IZAL, (uint8_t)(UsGxoVal));		/* 0x02A1 Set Offset Low byte */

	//////////
	// Y
	//////////
	RegWriteA(WC_MES1ADD0, 0x00);		// 0x0194
	RegWriteA(WC_MES1ADD1, 0x00);		// 0x0195
	lc898122a_gyro_ram_clear(0x1000, CLR_FRAM1);			/* Measure Filter RAM Clear */
	UsGyoVal = (uint16_t)lc898122a_gen_measure(AD3Z, 0);	/* GYRMON2(0x1111) <- GYADZ(0x14CA) */
	RegWriteA(IZBH, (uint8_t)(UsGyoVal >> 8));	// 0x02A2		Set Offset High byte
	RegWriteA(IZBL, (uint8_t)(UsGyoVal));		// 0x02A3		Set Offset Low byte

	SiRsltSts = EXE_END;						/* Clear Status */

	if (((int16_t)UsGxoVal < (int16_t)GYROFF_LOW_MOBILE) ||
		((int16_t)UsGxoVal > (int16_t)GYROFF_HIGH_MOBILE)) {
		SiRsltSts |= EXE_GXADJ;
	}

	if (((int16_t)UsGyoVal < (int16_t)GYROFF_LOW_MOBILE) ||
		((int16_t)UsGyoVal > (int16_t)GYROFF_HIGH_MOBILE)) {
		SiRsltSts |= EXE_GYADJ;
	}

	E2pWrt((uint16_t)0x0937, 2, (uint8_t *)&UsGxoVal);
	lc898122a_wait_time(10);		/*GYRO OFFSET Mobile X */
	E2pWrt((uint16_t)0x0939, 2, (uint8_t *)&UsGyoVal);
	lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Y */

	if (EXE_END == SiRsltSts) {
		UcGvcFlg  = 0xE7;
		E2pWrt((uint16_t)0x093B, 1, (uint8_t *)&UcGvcFlg);
		lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Flag */
		SiRsltSts = 0;																		//Success
	} else {
		UcGvcFlg = 0xF0;
		E2pWrt((uint16_t)0x093B, 1, (uint8_t *)&UcGvcFlg);
		lc898122a_wait_time(10);		/*GYRO OFFSET Mobile Flag */
		SiRsltSts = -1;																		//Fail
	}

	return SiRsltSts;
}


#endif

//********************************************************************************
// Function Name	: AfVcmCod
// Return Value		: NON
// Argument Value	: AF Code
// Explanation		: VCM AF Code setting function
// History			: First edition							2013.12.23 YS.Kim
//********************************************************************************
void	AfVcmCod(int16_t SsCodVal)
{
	lc898122a_ram_mode(ON);
	RamWriteA(AFSTMTGT, SsCodVal);
	lc898122a_ram_mode(OFF);
	RegWriteA(WC_STPMV, 0x01);
}

//********************************************************************************
// Function Name	: AfVcmCodCnv
// Return Value		: NON
// Argument Value	: AF Code Converting and setting
// Explanation		: VCM AF Code convert unsigned 10bit range to signed 16bit
// History			: First edition							2014.10.31 YS.Kim
//********************************************************************************
void lc898122a_af_vcm_code(int16_t UsVcmCod)
{
	AfVcmCod(UsVcmCod);
}

EXPORT_SYMBOL(lc898122a_af_vcm_code);

void lc898122a_claf_ramdom_test(void)
{
	CDBG("%s Enter\n", __func__);

	SrvConAF(0);

	RamWrite32A(0x151E, 0x3F800000);
	lc898122a_wait_time(100);
	RamWrite32A(0x151E, 0xBF800000);
	lc898122a_wait_time(100);
	RamWrite32A(0x151E, 0x3F800000);
	lc898122a_wait_time(100);
	RamWrite32A(0x151E, 0xBF800000);
	lc898122a_wait_time(100);
	RamWrite32A(0x151E, 0x00000000);

	CDBG("%s End\n", __func__);

}

EXPORT_SYMBOL(lc898122a_claf_ramdom_test);

//********************************************************************************
// Function Name	: lc898122a_init_closed_af
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Closed Loop AF Initial Setting
// History			: First edition							2014.06.16 K.Nakamura
//********************************************************************************
void lc898122a_init_closed_af(void)
{
	uint8_t	UcStbb0;

	RegWriteA(AFFC, 0x10);				// 0x0088	Closed Af, Constant Current select
	RegWriteA(DRVFCAF, 0x10);			// 0x0081	Drv.MODEAF=0, Drv.ENAAF=0, Constant Current MODE
	RegWriteA(DRVFC2AF, 0x00);			// 0x0082
	RegWriteA(DRVFC3AF, 0x40);			// 0x0083	[bit7-5] Constant DAC Gain, [bit2-0] Offset low order 3bit
	//RegWriteA(DRVFC4AF, 0x80);			// 0x0084	[bit7-6] Offset high order 2bit
	RegWriteA(PWMAAF, 0x00);				// 0x0090	AF PWM Disable

	RegWriteA(CCFCAF, 0x08);				// 0x00A1	[ - | CCDTSEL | - | - ][ CCDTSEL | - | - ]

	RegReadA(STBB0, &UcStbb0);			// 0x0250	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
	UcStbb0 |= 0xA0;
	RegWriteA(STBB0, UcStbb0);			// 0x0250	AF ON
	RegWriteA(STBB1, 0x05);			// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]

	RegWriteA(CCAAF, 0x80);			// 0x00A0	[7]=0:OFF 1:ON

	lc898122a_setp_move_set_cl_af();
}

//********************************************************************************
// Function Name	: lc898122a_setp_move_set_cl_af
// Retun Value		: NON
// Argment Value	: CLAF Stmv Set
// Explanation		: Step move setting Function
// History			: First edition							2014.09.19 T.Tokoro
//********************************************************************************
void lc898122a_setp_move_set_cl_af(void)
{
	//RegWriteA(WC_EQSW, 0x13);			// 0x01E0	AmEqSw[4][1][0]=ON
	RegWriteA(WC_EQSW, 0x10);			// 0x01E0	AmEqSw[4][1][0]=OFF
	//RegWriteA(WC_DWNSMP1, 0x00);		// 0x01E3	down sampling I-Filter
	RegWriteA(WC_DWNSMP2, 0x00);			// 0x01E4	Step move Fs=1/1

	lc898122a_ram_mode(ON);					// Fix mode

	RamWriteA(AFSTMSTP, 0x0040);		// 0x1514	Step¡¯l?Y¡¯e[ 1.5um, 0165h]
	RamWriteA(afssmv1, 0x6000);			// 0x1222	Gain Change 1st timing
	RamWriteA(afssmv2, 0x7000);			// 0x1223	Gain Change 2nd timing
	RamWriteA(afstma, 0x7fff);			// 0x121D	Gain 1st
	RamWriteA(afstmb, 0x2000);			// 0x121E	Gain 2nd
	RamWriteA(afstmc, 0x0a00);			// 0x121FGain 3rd

	lc898122a_ram_mode(OFF);					// Float mode

	RegWriteA(WC_STPMVMOD, 0x00);		// 0x01E2	¡¯C?]¡±¡í¡¯e:¡¯E?iMode, ¡¯Z?u¡®¨Ï:¡¯E?iMode?B
}

//********************************************************************************
// Function Name	: SrvConAF
// Retun Value		: NON
// Argment Value	: AF Servo ON/OFF
// Explanation		: Servo ON,OFF Function
// History			: First edition							2014.06.18 K.Nakamura
//********************************************************************************
void	SrvConAF(uint8_t	UcSwcCon)
{
	if (UcSwcCon) {							// Z Servo ON
		//TRACE("SrvConAF ** AF Servo ON\n");
		RegWriteA(WC_EQSW, 0x13);			// 0x01E0 AmEqSw[4][1][0]=ON
	} else {									// Z Servo OFF
		//TRACE("SrvConAF ** AF Servo OFF\n");
		RegWriteA(WC_EQSW, 0x12);			// 0x01E0 AmEqSw[4][1]=ON, AmEqSw[0]=OFF
		RamWrite32A(AF2PWM, 0x00000000);		// 0x151E
	}
}

//********************************************************************************
// Function Name	: StmvExeClAf
// Retun Value		: NON
// Argment Value	: CLAF Stmv
// Explanation		: Step move Function
// History			: First edition							2014.09.19 T.Tokoro
//********************************************************************************
void	StmvExeClAf(uint16_t usRamDat)
{
	lc898122a_ram_mode(ON);					// Fix mode
	RamWriteA(AFSTMTGT, usRamDat);		// 0x1513
	lc898122a_ram_mode(OFF);					// Float mode

	RegWriteA(WC_STPMV, 0x01);			// 0x01E1

	StmvFinClAf();
}

//********************************************************************************
// Function Name	: StmvClAf
// Retun Value		: NON
// Argment Value	: CLAF Stmv
// Explanation		: Step move Function
// History			: First edition							2014.09.19 T.Tokoro
//********************************************************************************
void StmvFinClAf(void)
{
	uint8_t	UcRegDat;

	RegReadA(WC_STPMV, &UcRegDat);

	while (UcRegDat & 0x01) {
		RegReadA(WC_STPMV, &UcRegDat);	// 0x01E1
	}
}
//********************************************************************************
// Function Name	: PwmCareerSet
// Retun Value		: NON
// Argment Value	: Mode
// Explanation		: Pwm Career Set
// History			: First edition							2014.12.04 T.Tokoro
//********************************************************************************
void PwmCareerSet(unsigned char ucMode )
{
	//IOP0 = Stlove(HSYNC)
	//IOP2 = ExClk
	// PWM Clock Frequency = 24 MHz

	switch (ucMode) {
	case PWM_CAREER_MODE_A:
	case PWM_CAREER_MODE_F:
	case PWM_CAREER_MODE_G:
		// PWM Career Frequency = 500 kHz
		RegWriteA(DRVFC2, 0x90);	// 0x0002
		RegWriteA(PWMFC, 0x3C);	// 0x0011
		RegWriteA(PWMPERIODX,	0xE4);	// 0x0018
		RegWriteA(PWMPERIODX2,	0x00);	// 0x0019
		RegWriteA(PWMPERIODY,	0xE4);	// 0x001A
		RegWriteA(PWMPERIODY2,	0x00);	// 0x001B
		RegWriteA(STROBEFC,	0xC0);	// 0x001C
		RegWriteA(STROBEDLYX,	0x00);	// 0x001D
		RegWriteA(STROBEDLYY,	0x00);	// 0x001E
		RegWriteA(CLKSEL,	0x01);	// 0x020C
		break;
	case PWM_CAREER_MODE_B:
		// PWM Career Frequency = 250 kHz
		RegWriteA(DRVFC2,	0x90);	// 0x0002
		RegWriteA(PWMFC, 0x3C);	// 0x0011
		RegWriteA(PWMPERIODX,	0xE3);	// 0x0018
		RegWriteA(PWMPERIODX2, 0x00);	// 0x0019
		RegWriteA(PWMPERIODY,	0xE3);	// 0x001A
		RegWriteA(PWMPERIODY2, 0x00);	// 0x001B
		RegWriteA(STROBEFC,	0xC0);	// 0x001C
		RegWriteA(STROBEDLYX,	0x00);	// 0x001D
		RegWriteA(STROBEDLYY,	0x00);	// 0x001E
		RegWriteA(CLKSEL,	0x01);	// 0x020C
		break;
	case PWM_CAREER_MODE_C:
	case PWM_CAREER_MODE_E:
		// PWM Career Frequency = 500 kHz
		RegWriteA(DRVFC2,	0x90);	// 0x0002
		RegWriteA(PWMFC, 0x4C);	// 0x0011
		RegWriteA(PWMPERIODX,	0xE3);	// 0x0018
		RegWriteA(PWMPERIODX2, 0x00);	// 0x0019
		RegWriteA(PWMPERIODY,	0xE3);	// 0x001A
		RegWriteA(PWMPERIODY2, 0x00);	// 0x001B
		RegWriteA(STROBEFC,	0xC0);	// 0x001C
		RegWriteA(STROBEDLYX,	0x00);	// 0x001D
		RegWriteA(STROBEDLYY,	0x00);	// 0x001E
		RegWriteA(CLKSEL,		0x01);	// 0x020C
		break;
	case PWM_CAREER_MODE_D:
		// PWM Career Frequency = 375 kHz
		RegWriteA(DRVFC2, 0x90);	// 0x0002
		RegWriteA(PWMFC, 0x4C);	// 0x0011
		RegWriteA(PWMPERIODX,	0xE4);	// 0x0018
		RegWriteA(PWMPERIODX2, 0x00);	// 0x0019
		RegWriteA(PWMPERIODY,	0xE4);	// 0x001A
		RegWriteA(PWMPERIODY2, 0x00);	// 0x001B
		RegWriteA(STROBEFC,	0xC0);	// 0x001C
		RegWriteA(STROBEDLYX,	0x00);	// 0x001D
		RegWriteA(STROBEDLYY,	0x00);	// 0x001E
		RegWriteA(CLKSEL, 0x01);	// 0x020C
		break;
	}
}

static struct msm_ois_func_tbl lgit_ois_func_tbl;

int32_t lgit_init_set_ois(struct msm_ois_ctrl_t *o_ctrl,
						  struct msm_ois_set_info_t *set_info)
{
	int32_t rc = OIS_SUCCESS;
	enum ois_ver_t ver;
	//int8_t regA, regB;
	//int16_t regC, regD, regE, regF, regG, regH, regI, regJ, regK, regL, regM;

	CDBG("%s Enter\n", __func__);

	if (copy_from_user(&ver, (void *)set_info->setting, sizeof(enum ois_ver_t)))
		ver = OIS_VER_RELEASE;

	CDBG("%s: ois_on! ver:%d\n", __func__, ver);

	rc = lc898122a_init();
	if (rc < 0) {
		CDBG("lc898122a_init fail %d\n", rc);
		return rc;
	}

	SrvConAF(1);

	lc898122a_return_center(0);
#if 0
	/* For test: KSY1_start */
	lc898122a_ois_off();

	RegReadA(0x01E0, &regA);
	RegReadA(0x01E1, &regB);
	CDBG("%s 0x01E0: %d, 0x01E1: %d", __func__, regA, regB);
	RegWriteA(0x018c, 0x31);

	RamReadA(0x144F, &regC);
	RamReadA(0x145F, &regD);
	RamReadA(0x1510, &regE);
	RamReadA(0x1502, &regF);
	RamReadA(0x1503, &regG);
	RamReadA(0x1504, &regH);
	RamReadA(0x1513, &regI);
	RamReadA(0x1514, &regJ);
	RamReadA(0x151F, &regM);
	RamReadA(0x151D, &regK);
	RamReadA(0x151E, &regL);
	CDBG("%s 0x144F: %d, 0x145F: %d, 0x1510: %d, 0x1502: %d, 0x1503: %d\n", __func__, regC, regD, regE, regF, regG);
	CDBG("%s 0x1504: %d, 0x1513: %d, 0x1514: %d, 0x151F: %d, 0x151D: %d, 0x151E: %d", __func__, regH, regI, regJ, regM, regK, regL);

	RegWriteA(0x018c, 0x00);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x0000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);
	/* For test: KSY1_end */


	/* For test: KSY2_start */

	RegReadA(0x01E0, &regA);
	RegReadA(0x01E1, &regB);
	CDBG("%s 0x01E0: %d, 0x01E1: %d", __func__, regA, regB);
	RegWriteA(0x018c, 0x31);

	RamReadA(0x144F, &regC);
	RamReadA(0x145F, &regD);
	RamReadA(0x1510, &regE);
	RamReadA(0x1502, &regF);
	RamReadA(0x1503, &regG);
	RamReadA(0x1504, &regH);
	RamReadA(0x1513, &regI);
	RamReadA(0x1514, &regJ);
	RamReadA(0x151F, &regM);
	RamReadA(0x151D, &regK);
	RamReadA(0x151E, &regL);
	CDBG("%s 0x144F: %d, 0x145F: %d, 0x1510: %d, 0x1502: %d, 0x1503: %d\n", __func__, regC, regD, regE, regF, regG);
	CDBG("%s 0x1504: %d, 0x1513: %d, 0x1514: %d, 0x151F: %d, 0x151D: %d, 0x151E: %d", __func__, regH, regI, regJ, regM, regK, regL);

	RegWriteA(0x018c, 0x00);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x4000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);
	/* For test: KSY2_end */


	/* For test: KSY3_start */

	RegReadA(0x01E0, &regA);
	RegReadA(0x01E1, &regB);
	CDBG("%s 0x01E0: %d, 0x01E1: %d", __func__, regA, regB);
	RegWriteA(0x018c, 0x31);

	RamReadA(0x144F, &regC);
	RamReadA(0x145F, &regD);
	RamReadA(0x1510, &regE);
	RamReadA(0x1502, &regF);
	RamReadA(0x1503, &regG);
	RamReadA(0x1504, &regH);
	RamReadA(0x1513, &regI);
	RamReadA(0x1514, &regJ);
	RamReadA(0x151F, &regM);
	RamReadA(0x151D, &regK);
	RamReadA(0x151E, &regL);
	CDBG("%s 0x144F: %d, 0x145F: %d, 0x1510: %d, 0x1502: %d, 0x1503: %d\n", __func__, regC, regD, regE, regF, regG);
	CDBG("%s 0x1504: %d, 0x1513: %d, 0x1514: %d, 0x151F: %d, 0x151D: %d, 0x151E: %d", __func__, regH, regI, regJ, regM, regK, regL);

	RegWriteA(0x018c, 0x00);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0xC000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);
	/* For test: KSY3_end */

	/* For test: KSY4_start */

	RegReadA(0x01E0, &regA);
	RegReadA(0x01E1, &regB);
	CDBG("%s 0x01E0: %d, 0x01E1: %d", __func__, regA, regB);
	RegWriteA(0x018c, 0x31);

	RamReadA(0x144F, &regC);
	RamReadA(0x145F, &regD);
	RamReadA(0x1510, &regE);
	RamReadA(0x1502, &regF);
	RamReadA(0x1503, &regG);
	RamReadA(0x1504, &regH);
	RamReadA(0x1513, &regI);
	RamReadA(0x1514, &regJ);
	RamReadA(0x151F, &regM);
	RamReadA(0x151D, &regK);
	RamReadA(0x151E, &regL);
	CDBG("%s 0x144F: %d, 0x145F: %d, 0x1510: %d, 0x1502: %d, 0x1503: %d\n", __func__, regC, regD, regE, regF, regG);
	CDBG("%s 0x1504: %d, 0x1513: %d, 0x1514: %d, 0x151F: %d, 0x151D: %d, 0x151E: %d", __func__, regH, regI, regJ, regM, regK, regL);

	RegWriteA(0x018c, 0x00);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x0000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);
	/* For test: KSY4_end */

	/* For test: KSY5_end */

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0xC000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x0000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x4000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0xC000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x0000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x4000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	lc898122a_wait_time(100);


	/* For test: KSY5_end */

	/* For test: KSY */

	RegWriteA(0x018c, 0x31);
	RamWriteA(0x1513, 0x0000);
	RegWriteA(0x018c, 0x00);
	RegWriteA(0x01E1, 0x01);
	RegWriteA(0x018c, 0x31);
	/* For test: KSY */
#endif

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
		lc898122a_ois_enable();
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

	snprintf(ois_stat.ois_provider, ARRAY_SIZE(ois_stat.ois_provider), "LGIT_ONSEMI");
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

void lgit_imx234_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
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
