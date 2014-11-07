//==============================================================================
// ois+init.c Code START
//==============================================================================
//**************************
//	Include Header File
//**************************
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

#include "../../odin-css.h"

#define		OISINI
#include "lgit_ois_old.h"

#define LAST_UPDATE  "13-08-01, 0x0204B_Q2_3"

#include	"odin-ois-i2c.h"
//#include	"ois.h"

//**************************
//	Global Variable
//**************************
unsigned char spigyrocheck=0x00;

#define	OIS_FW_POLLING_PASS		OIS_SUCCESS
#define	OIS_FW_POLLING_FAIL		OIS_INIT_TIMEOUT
#define	CLRGYR_POLLING_LIMIT_A	6
#define	ACCWIT_POLLING_LIMIT_A	6
#define	INIDGY_POLLING_LIMIT_A	12
#define INIDGY_POLLING_LIMIT_B	12
#define BSYWIT_POLLING_LIMIT_A  6

#define HALREGTAB	32
#define HALFILTAB	138
#define	GYRFILTAB	125

/*Filter Calculator Version 4.02*/
/*the time and date : 2013/3/15 16:46:01*/
/*the time and date : 2013/3/28 22:25:01*/
/*fs,23438Hz*/
/*LSI No.,LC898111_AS*/
/*Comment,*/

/* 8bit */
OISINI__ const struct STHALREG	CsHalReg[][HALREGTAB]	= {
	{	//VER 04 /*FC filename : Mitsumi_130501*/
		{ 0x000E, 0x00},	/*00,000E*/
		{ 0x000F, 0x00},	/*00,000F,0dB*/
		{ 0x0086, 0x00},	/*00,0086,0dB*/
		{ 0x0087, 0x00},	/*00,0087,0dB*/
		{ 0x0088, 0x15},	/*15,0088,30dB*/
		{ 0x0089, 0x00},	/*00,0089*/
		{ 0x008A, 0x40},	/*40,008A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x008B, 0x54},	/*54,008B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x008C, 0x00},	/*00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0*/
		{ 0x0090, 0x00},	/*00,0090,0dB*/
		{ 0x0091, 0x00},	/*00,0091,0dB*/
		{ 0x0092, 0x15},	/*15,0092,30dB*/
		{ 0x0093, 0x00},	/*00,0093*/
		{ 0x0094, 0x40},	/*40,0094,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x0095, 0x54},	/*54,0095,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x0096, 0x00},	/*00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0*/
		{ 0x00A0, 0x00},	/*00,00A0,0dB*/
		{ 0x00A1, 0x00},	/*00,00A1*/
		{ 0x00B4, 0x00},	/*00,00B4*/
		{ 0x00B5, 0x00},	/*00,00B5,0dB*/
		{ 0x00B6, 0x00},	/*00,00B6,Through,0dB,fs/1,invert=0*/
		{ 0x00BB, 0x00},	/*00,00BB*/
		{ 0x00BC, 0x00},	/*00,00BC,0dB*/
		{ 0x00BD, 0x00},	/*00,00BD,Through,0dB,fs/1,invert=0*/
		{ 0x00C1, 0x00},	/*00,00C1,Through,0dB,fs/1,invert=0*/
		{ 0x00C5, 0x00},	/*00,00C5,Through,0dB,fs/1,invert=0*/
		{ 0x00C8, 0x00},	/*00,00C8*/
		{ 0x0110, 0x01},	/*01,0110*/
		{ 0x0111, 0x00},	/*00,0111*/
		{ 0x0112, 0x00},	/*00,0112*/
		{ 0x017D, 0x01},	/*01,017D*/
		{ 0xFFFF , 0xFF }
	}
} ;

/* 16bit */
OISINI__ const struct STHALFIL	CsHalFil[][HALFILTAB]	= {
	{	//VER 04 /*FC filename : Mitsumi_130501*/
		{ 0x1128, 0x0000},	/*0000,1128,Cutoff,invert=0*/
		{ 0x1168, 0x0000},	/*0000,1168,Cutoff,invert=0*/
		{ 0x11E0, 0x7FFF},	/*7FFF,11E0,0dB,invert=0*/
		{ 0x11E1, 0x7FFF},	/*7FFF,11E1,0dB,invert=0*/
		{ 0x11E2, 0x7FFF},	/*7FFF,11E2,0dB,invert=0*/
		{ 0x11E3, 0x7FFF},	/*7FFF,11E3,0dB,invert=0*/
		{ 0x11E4, 0x7FFF},	/*7FFF,11E4,0dB,invert=0*/
		{ 0x11E5, 0x7FFF},	/*7FFF,11E5,0dB,invert=0*/
		{ 0x12E0, 0x7FFF},	/*7FFF,12E0,Through,0dB,fs/1,invert=0*/
		{ 0x12E1, 0x0000},	/*0000,12E1,Through,0dB,fs/1,invert=0*/
		{ 0x12E2, 0x0000},	/*0000,12E2,Through,0dB,fs/1,invert=0*/
		{ 0x12E3, 0x7FFF},	/*7FFF,12E3,Through,0dB,fs/1,invert=0*/
		{ 0x12E4, 0x0000},	/*0000,12E4,Through,0dB,fs/1,invert=0*/
		{ 0x12E5, 0x0000},	/*0000,12E5,Through,0dB,fs/1,invert=0*/
		{ 0x12E6, 0x7FFF},	/*7FFF,12E6,0dB,invert=0*/
		{ 0x1301, 0x7FFF},	/*7FFF,1301,0dB,invert=0*/
		{ 0x1302, 0x7FFF},	/*7FFF,1302,0dB,invert=0*/
		{ 0x1305, 0x7FFF},	/*7FFF,1305,Through,0dB,fs/1,invert=0*/
		{ 0x1306, 0x0000},	/*0000,1306,Through,0dB,fs/1,invert=0*/
		{ 0x1307, 0x0000},	/*0000,1307,Through,0dB,fs/1,invert=0*/
		{ 0x1308, 0x0000},	/*0000,1308,Cutoff,invert=0*/
		{ 0x1309, 0x5A9D},	/*5A9D,1309,-3dB,invert=0*/
		{ 0x130A, 0x0000},	/*0000,130A,Cutoff,invert=0*/
		{ 0x130B, 0x0000},	/*0000,130B,Cutoff,invert=0*/
		{ 0x130C, 0x7FFF},	/*7FFF,130C,0dB,invert=0*/
		{ 0x130D, 0x46EB},	/*46EB,130D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x130E, 0xBA1D},	/*BA1D,130E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x130F, 0x3679},	/*3679,130F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1310, 0x0000},	/*0000,1310,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1311, 0x0000},	/*0000,1311,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1312, 0x43B3},	/*43B3,1312,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1313, 0xBCD7},	/*BCD7,1313,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1314, 0x3F51},	/*3F51,1314,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1315, 0x0000},	/*0000,1315,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1316, 0x0000},	/*0000,1316,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1317, 0x7FFF},	/*7FFF,1317,0dB,invert=0*/
		{ 0x1318, 0x0000},	/*0000,1318,Cutoff,invert=0*/
		{ 0x1319, 0x0000},	/*0000,1319,Cutoff,invert=0*/
		{ 0x131A, 0x002B},	/*002B,131A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131B, 0x0055},	/*0055,131B,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131C, 0x72F9},	/*72F9,131C,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131D, 0x002B},	/*002B,131D,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131E, 0xCC5D},	/*CC5D,131E,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x131F, 0x020D},	/*020D,131F,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1320, 0x020D},	/*020D,1320,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1321, 0x7FCB},	/*7FCB,1321,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1322, 0x0000},	/*0000,1322,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1323, 0x0000},	/*0000,1323,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1324, 0x334D},	/*334D,1324,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1325, 0xCD0A},	/*CD0A,1325,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1326, 0x7FA9},	/*7FA9,1326,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1327, 0x0000},	/*0000,1327,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1328, 0x0000},	/*0000,1328,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1329, 0x7FFF},	/*7FFF,1329,0dB,invert=0*/
		{ 0x132A, 0x7FFF},	/*7FFF,132A,0dB,invert=0*/
		{ 0x132B, 0x0000},	/*0000,132B,Cutoff,invert=0*/
		{ 0x132C, 0x0000},	/*0000,132C,Cutoff,invert=0*/
		{ 0x132D, 0x0000},	/*0000,132D,Cutoff,invert=0*/
		{ 0x132E, 0x0000},	/*0000,132E,Cutoff,invert=0*/
		{ 0x132F, 0x7485},	/*7485,132F,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1330, 0x8EDF},	/*8EDF,1330,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1331, 0x7C9D},	/*7C9D,1331,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1332, 0x3853},	/*3853,1332,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1333, 0x9CAB},	/*9CAB,1333,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1334, 0x6355},	/*6355,1334,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1335, 0x313B},	/*313B,1335,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1336, 0xD672},	/*D672,1336,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1337, 0x0D17},	/*0D17,1337,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1338, 0x0D17},	/*0D17,1338,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1339, 0x65D1},	/*65D1,1339,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133A, 0x0000},	/*0000,133A,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133B, 0x0000},	/*0000,133B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x133C, 0x7FFF},	/*7FFF,133C,0dB,invert=0*/
		{ 0x133D, 0x0000},	/*0000,133D,Cutoff,invert=0*/
		{ 0x133E, 0x0000},	/*0000,133E,Cutoff,invert=0*/
		{ 0x133F, 0x7FFF},	/*7FFF,133F,0dB,invert=0*/
		{ 0x1341, 0x7FFF},	/*7FFF,1341,0dB,invert=0*/
		{ 0x1342, 0x7FFF},	/*7FFF,1342,0dB,invert=0*/
		{ 0x1345, 0x7FFF},	/*7FFF,1345,Through,0dB,fs/1,invert=0*/
		{ 0x1346, 0x0000},	/*0000,1346,Through,0dB,fs/1,invert=0*/
		{ 0x1347, 0x0000},	/*0000,1347,Through,0dB,fs/1,invert=0*/
		{ 0x1348, 0x0000},	/*0000,1348,Cutoff,invert=0*/
		{ 0x1349, 0x5A9D},	/*5A9D,1349,-3dB,invert=0*/
		{ 0x134A, 0x0000},	/*0000,134A,Cutoff,invert=0*/
		{ 0x134B, 0x0000},	/*0000,134B,Cutoff,invert=0*/
		{ 0x134C, 0x7FFF},	/*7FFF,134C,0dB,invert=0*/
		{ 0x134D, 0x46EB},	/*46EB,134D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x134E, 0xBA1D},	/*BA1D,134E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x134F, 0x3679},	/*3679,134F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1350, 0x0000},	/*0000,1350,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1351, 0x0000},	/*0000,1351,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0*/
		{ 0x1352, 0x43B3},	/*43B3,1352,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1353, 0xBCD7},	/*BCD7,1353,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1354, 0x3F51},	/*3F51,1354,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1355, 0x0000},	/*0000,1355,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1356, 0x0000},	/*0000,1356,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0*/
		{ 0x1357, 0x7FFF},	/*7FFF,1357,0dB,invert=0*/
		{ 0x1358, 0x0000},	/*0000,1358,Cutoff,invert=0*/
		{ 0x1359, 0x0000},	/*0000,1359,Cutoff,invert=0*/
		{ 0x135A, 0x002B},	/*002B,135A,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135B, 0x0055},	/*0055,135B,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135C, 0x72F9},	/*72F9,135C,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135D, 0x002B},	/*002B,135D,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135E, 0xCC5D},	/*CC5D,135E,LPF,400Hz,0dB,0,fs/1,invert=0*/
		{ 0x135F, 0x020D},	/*020D,135F,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1360, 0x020D},	/*020D,1360,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1361, 0x7FCB},	/*7FCB,1361,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1362, 0x0000},	/*0000,1362,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1363, 0x0000},	/*0000,1363,LPF,1.5Hz,26dB,fs/4,invert=0*/
		{ 0x1364, 0x334D},	/*334D,1364,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1365, 0xCD0A},	/*CD0A,1365,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1366, 0x7FA9},	/*7FA9,1366,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1367, 0x0000},	/*0000,1367,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1368, 0x0000},	/*0000,1368,LBF,10Hz,25Hz,0dB,fs/1,invert=0*/
		{ 0x1369, 0x7FFF},	/*7FFF,1369,0dB,invert=0*/
		{ 0x136A, 0x7FFF},	/*7FFF,136A,0dB,invert=0*/
		{ 0x136B, 0x0000},	/*0000,136B,Cutoff,invert=0*/
		{ 0x136C, 0x0000},	/*0000,136C,Cutoff,invert=0*/
		{ 0x136D, 0x0000},	/*0000,136D,Cutoff,invert=0*/
		{ 0x136E, 0x0000},	/*0000,136E,Cutoff,invert=0*/
		{ 0x136F, 0x7485},	/*7485,136F,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1370, 0x8EDF},	/*8EDF,1370,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1371, 0x7C9D},	/*7C9D,1371,LBF,100Hz,110Hz,0dB,fs/1,invert=0*/
		{ 0x1372, 0x3853},	/*3853,1372,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1373, 0x9CAB},	/*9CAB,1373,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1374, 0x6355},	/*6355,1374,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1375, 0x313B},	/*313B,1375,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1376, 0xD672},	/*D672,1376,PKF,1300Hz,-10dB,3,fs/1,invert=0*/
		{ 0x1377, 0x0D17},	/*0D17,1377,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1378, 0x0D17},	/*0D17,1378,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x1379, 0x65D1},	/*65D1,1379,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137A, 0x0000},	/*0000,137A,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137B, 0x0000},	/*0000,137B,LPF,850Hz,0dB,fs/1,invert=0*/
		{ 0x137C, 0x7FFF},	/*7FFF,137C,0dB,invert=0*/
		{ 0x137D, 0x0000},	/*0000,137D,Cutoff,invert=0*/
		{ 0x137E, 0x0000},	/*0000,137E,Cutoff,invert=0*/
		{ 0x137F, 0x7FFF},	/*7FFF,137F,0dB,invert=0*/
		{ 0xFFFF , 0xFFFF }
	}
} ;
/* 32bit */
OISINI__ const struct STGYRFIL	CsGyrFil[][GYRFILTAB]	= {
	{	//VER 04
		{ 0x1800, 0x3F800000},	/*3F800000,1800,0dB,invert=0*/
		{ 0x1801, 0x3C3F00A9},	/*3C3F00A9,1801,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1802, 0x3F7A07FB},	/*3F7A07FB,1802,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1803, 0x38A8A554},	/*38A8A554,1803,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1804, 0x38A8A554},	/*38A8A554,1804,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1805, 0x3F7FF576},	/*3F7FF576,1805,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1806, 0x3C3F00A9},	/*3C3F00A9,1806,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1807, 0x00000000},	/*00000000,1807,Cutoff,invert=0*/
		{ 0x180A, 0x38A8A554},	/*38A8A554,180A,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180B, 0x38A8A554},	/*38A8A554,180B,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180C, 0x3F7FF576},	/*3F7FF576,180C,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x180D, 0x3F800000},	/*3F800000,180D,0dB,invert=0*/
		{ 0x180E, 0xBF800000},	/*BF800000,180E,0dB,invert=1*/
		{ 0x180F, 0x3FFF64C1},	/*3FFF64C1,180F,6dB,invert=0*/
		{ 0x1810, 0x3F800000},	/*3F800000,1810,0dB,invert=0*/
		{ 0x1811, 0x3F800000},	/*3F800000,1811,0dB,invert=0*/
		{ 0x1812, 0x3B02C2F2},	/*3B02C2F2,1812,Free,fs/2,invert=0*/
		{ 0x1813, 0x00000000},	/*00000000,1813,Free,fs/2,invert=0*/
		{ 0x1814, 0x3F7FFD80},	/*3F7FFD80,1814,Free,fs/2,invert=0*/
		{ 0x1815, 0x428C7352},	/*428C7352,1815,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1816, 0xC28AA79E},	/*C28AA79E,1816,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1817, 0x3DDE3847},	/*3DDE3847,1817,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1818, 0x3F231C22},	/*3F231C22,1818,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x1819, 0xBF1ECB8E},	/*BF1ECB8E,1819,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x181A, 0x3F7A916B},	/*3F7A916B,181A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x181B, 0x00000000},	/*00000000,181B,Cutoff,invert=0*/
		{ 0x181C, 0x3F800000},	/*3F800000,181C,0dB,invert=0*/
		{ 0x181D, 0x3F800000},	/*3F800000,181D,Through,0dB,fs/2,invert=0*/
		{ 0x181E, 0x00000000},	/*00000000,181E,Through,0dB,fs/2,invert=0*/
		{ 0x181F, 0x00000000},	/*00000000,181F,Through,0dB,fs/2,invert=0*/
		{ 0x1820, 0x3F75C43F},	/*3F75C43F,1820,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1821, 0xBF756FF8},	/*BF756FF8,1821,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1822, 0x3F7FABB9},	/*3F7FABB9,1822,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1823, 0x3F800000},	/*3F800000,1823,Through,0dB,fs/2,invert=0*/
		{ 0x1824, 0x00000000},	/*00000000,1824,Through,0dB,fs/2,invert=0*/
		{ 0x1825, 0x00000000},	/*00000000,1825,Through,0dB,fs/2,invert=0*/
		{ 0x1826, 0x00000000},	/*00000000,1826,Cutoff,invert=0*/
		{ 0x1827, 0x3F800000},	/*3F800000,1827,0dB,invert=0*/
		{ 0x1828, 0x3F800000},	/*3F800000,1828,0dB,invert=0*/
		{ 0x1829, 0x41400000},	/*41400000,1829,21.5836dB,invert=0*/
		{ 0x182A, 0x3F800000},	/*3F800000,182A,0dB,invert=0*/
		{ 0x182B, 0x3F800000},	/*3F800000,182B,0dB,invert=0*/
		{ 0x182C, 0x00000000},	/*00000000,182C,Cutoff,invert=0*/
		{ 0x1830, 0x3D1E56E0},	/*3D1E56E0,1830,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1831, 0x3D1E56E0},	/*3D1E56E0,1831,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1832, 0x3F6C3524},	/*3F6C3524,1832,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1833, 0x00000000},	/*00000000,1833,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1834, 0x00000000},	/*00000000,1834,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1835, 0x00000000},	/*00000000,1835,Cutoff,invert=0*/
		{ 0x1838, 0x3F800000},	/*3F800000,1838,0dB,invert=0*/
		{ 0x1839, 0x3C58B55D},	/*3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183A, 0x3C58B55D},	/*3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183B, 0x3F793A55},	/*3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183C, 0x3C58B55D},	/*3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183D, 0x3C58B55D},	/*3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x183E, 0x3F793A55},	/*3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x1840, 0x38A8A554},	/*38A8A554,1840,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1841, 0x38A8A554},	/*38A8A554,1841,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1842, 0x3F7FF576},	/*3F7FF576,1842,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1850, 0x38A8A554},	/*38A8A554,1850,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1851, 0x38A8A554},	/*38A8A554,1851,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1852, 0x3F7FF576},	/*3F7FF576,1852,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1900, 0x3F800000},	/*3F800000,1900,0dB,invert=0*/
		{ 0x1901, 0x3C3F00A9},	/*3C3F00A9,1901,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1902, 0x3F7A07FB},	/*3F7A07FB,1902,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1903, 0x38A8A554},	/*38A8A554,1903,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1904, 0x38A8A554},	/*38A8A554,1904,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1905, 0x3F7FF576},	/*3F7FF576,1905,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1906, 0x3C3F00A9},	/*3C3F00A9,1906,LPF,44Hz,0dB,fs/2,invert=0*/
		{ 0x1907, 0x00000000},	/*00000000,1907,Cutoff,invert=0*/
		{ 0x190A, 0x38A8A554},	/*38A8A554,190A,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190B, 0x38A8A554},	/*38A8A554,190B,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190C, 0x3F7FF576},	/*3F7FF576,190C,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x190D, 0x3F800000},	/*3F800000,190D,0dB,invert=0*/
		{ 0x190E, 0xBF800000},	/*BF800000,190E,0dB,invert=1*/
		{ 0x190F, 0x3FFF64C1},	/*3FFF64C1,190F,6dB,invert=0*/
		{ 0x1910, 0x3F800000},	/*3F800000,1910,0dB,invert=0*/
		{ 0x1911, 0x3F800000},	/*3F800000,1911,0dB,invert=0*/
		{ 0x1912, 0x3B02C2F2},	/*3B02C2F2,1912,Free,fs/2,invert=0*/
		{ 0x1913, 0x00000000},	/*00000000,1913,Free,fs/2,invert=0*/
		{ 0x1914, 0x3F7FFD80},	/*3F7FFD80,1914,Free,fs/2,invert=0*/
		{ 0x1915, 0x428C7352},	/*428C7352,1915,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1916, 0xC28AA79E},	/*C28AA79E,1916,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1917, 0x3DDE3847},	/*3DDE3847,1917,HBF,24Hz,3000Hz,42dB,fs/2,invert=0*/
		{ 0x1918, 0x3F231C22},	/*3F231C22,1918,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x1919, 0xBF1ECB8E},	/*BF1ECB8E,1919,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x191A, 0x3F7A916B},	/*3F7A916B,191A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0*/
		{ 0x191B, 0x00000000},	/*00000000,191B,Cutoff,invert=0*/
		{ 0x191C, 0x3F800000},	/*3F800000,191C,0dB,invert=0*/
		{ 0x191D, 0x3F800000},	/*3F800000,191D,Through,0dB,fs/2,invert=0*/
		{ 0x191E, 0x00000000},	/*00000000,191E,Through,0dB,fs/2,invert=0*/
		{ 0x191F, 0x00000000},	/*00000000,191F,Through,0dB,fs/2,invert=0*/
		{ 0x1920, 0x3F75C43F},	/*3F75C43F,1920,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1921, 0xBF756FF8},	/*BF756FF8,1921,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1922, 0x3F7FABB9},	/*3F7FABB9,1922,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0*/
		{ 0x1923, 0x3F800000},	/*3F800000,1923,Through,0dB,fs/2,invert=0*/
		{ 0x1924, 0x00000000},	/*00000000,1924,Through,0dB,fs/2,invert=0*/
		{ 0x1925, 0x00000000},	/*00000000,1925,Through,0dB,fs/2,invert=0*/
		{ 0x1926, 0x00000000},	/*00000000,1926,Cutoff,invert=0*/
		{ 0x1927, 0x3F800000},	/*3F800000,1927,0dB,invert=0*/
		{ 0x1928, 0x3F800000},	/*3F800000,1928,0dB,invert=0*/
		{ 0x1929, 0x41400000},	/*41400000,1929,21.5836dB,invert=0*/
		{ 0x192A, 0x3F800000},	/*3F800000,192A,0dB,invert=0*/
		{ 0x192B, 0x3F800000},	/*3F800000,192B,0dB,invert=0*/
		{ 0x192C, 0x00000000},	/*00000000,192C,Cutoff,invert=0*/
		{ 0x1930, 0x3D1E56E0},	/*3D1E56E0,1930,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1931, 0x3D1E56E0},	/*3D1E56E0,1931,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1932, 0x3F6C3524},	/*3F6C3524,1932,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1933, 0x00000000},	/*00000000,1933,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1934, 0x00000000},	/*00000000,1934,LPF,300Hz,0dB,fs/1,invert=0*/
		{ 0x1935, 0x00000000},	/*00000000,1935,Cutoff,invert=0*/
		{ 0x1938, 0x3F800000},	/*3F800000,1938,0dB,invert=0*/
		{ 0x1939, 0x3C58B55D},	/*3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193A, 0x3C58B55D},	/*3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193B, 0x3F793A55},	/*3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193C, 0x3C58B55D},	/*3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193D, 0x3C58B55D},	/*3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x193E, 0x3F793A55},	/*3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0*/
		{ 0x1940, 0x38A8A554},	/*38A8A554,1940,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1941, 0x38A8A554},	/*38A8A554,1941,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1942, 0x3F7FF576},	/*3F7FF576,1942,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1950, 0x38A8A554},	/*38A8A554,1950,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1951, 0x38A8A554},	/*38A8A554,1951,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0x1952, 0x3F7FF576},	/*3F7FF576,1952,LPF,0.3Hz,0dB,fs/2,invert=0*/
		{ 0xFFFF , 0xFFFFFFFF }
	}
} ;
static struct odin_ois_fn lgit_ois_func_tbl;


//**************************
//	Global Variable
//**************************
int OnsemiI2CCheck(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char UcLsiVer;
	RegRead8(client, CVER, &UcLsiVer); // 0x27E
	return (UcLsiVer == 0x43) ? 1 : 0;
}

int	IniSet(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	WitTim(5);
	RegWrite8(client, SOFRES1	, 0x00);	// Software Reset
	WitTim(5);
	RegWrite8(client, SOFRES1	, 0x11);

	// Read calibration data from E2PROM
	E2pDat() ;
	// Version Check
	VerInf() ;
	// Clock Setting
	IniClk() ;
	// I/O Port Initial Setting
	IniIop() ;
	// Monitor & Other Initial Setting
	IniMon() ;
	// Servo Initial Setting
	IniSrv() ;
	// Gyro Filter Initial Setting
	IniGyr() ; //Pan OFF

	//POLLING EXCEPTION Setting
	// Hall Filter Initial Setting
	if (IniHfl() != OIS_FW_POLLING_PASS) return OIS_FW_POLLING_FAIL ;
	// Gyro Filter Initial Setting
	if (IniGfl() != OIS_FW_POLLING_PASS) return OIS_FW_POLLING_FAIL ;
	// DigitalGyro Initial Setting
	if (IniDgy() != OIS_FW_POLLING_PASS) return OIS_FW_POLLING_FAIL ;

	// Adjust Fix Value Setting
	IniAdj() ; //Pan ON

	return OIS_FW_POLLING_PASS ;
}

//********************************************************************************
// Function Name 	: E2pDat
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: E2PROM Calibration Data Read Function
// History			: First edition 						2013.06.21 Y.Kim
//********************************************************************************
void	E2pDat(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	MemClr((unsigned char *)&StCalDat, sizeof(stCalDat)) ;

	E2pRed(client, (unsigned short)ADJ_COMP_FLAG, 2,		(unsigned char *)&StCalDat.UsAdjCompF) ;

	E2pRed(client, (unsigned short)HALL_OFFSET_X, 2,		(unsigned char *)&StCalDat.StHalAdj.UsHlxOff) ;
	E2pRed(client, (unsigned short)HALL_BIAS_X, 2,			(unsigned char *)&StCalDat.StHalAdj.UsHlxGan) ;
	E2pRed(client, (unsigned short)HALL_AD_OFFSET_X, 2,	(unsigned char *)&StCalDat.StHalAdj.UsAdxOff) ;

	E2pRed(client, (unsigned short)HALL_OFFSET_Y, 2,		(unsigned char *)&StCalDat.StHalAdj.UsHlyOff) ;
	E2pRed(client, (unsigned short)HALL_BIAS_Y, 2,			(unsigned char *)&StCalDat.StHalAdj.UsHlyGan) ;
	E2pRed(client, (unsigned short)HALL_AD_OFFSET_Y, 2,	(unsigned char *)&StCalDat.StHalAdj.UsAdyOff) ;

	E2pRed(client, (unsigned short)LOOP_GAIN_X, 2,			(unsigned char *)&StCalDat.StLopGan.UsLxgVal) ;
	E2pRed(client, (unsigned short)LOOP_GAIN_Y, 2,			(unsigned char *)&StCalDat.StLopGan.UsLygVal) ;

	E2pRed(client, (unsigned short)LENS_CENTER_FINAL_X, 2,	(unsigned char *)&StCalDat.StLenCen.UsLsxVal) ;
	E2pRed(client, (unsigned short)LENS_CENTER_FINAL_Y, 2,	(unsigned char *)&StCalDat.StLenCen.UsLsyVal) ;

	E2pRed(client, (unsigned short)GYRO_AD_OFFSET_X, 2,	(unsigned char *)&StCalDat.StGvcOff.UsGxoVal) ;
	E2pRed(client, (unsigned short)GYRO_AD_OFFSET_Y, 2,	(unsigned char *)&StCalDat.StGvcOff.UsGyoVal) ;

	E2pRed(client, (unsigned short)GYRO_GAIN_X, 4 ,		(unsigned char *)&StCalDat.UlGxgVal) ;
	E2pRed(client, (unsigned short)GYRO_GAIN_Y, 4 ,		(unsigned char *)&StCalDat.UlGygVal) ;

	E2pRed(client, (unsigned short)OSC_CLK_VAL, 2 ,		(unsigned char *)&StCalDat.UsOscVal) ;

	E2pRed(client, (unsigned short)SYSTEM_VERSION_INFO, 2,	(unsigned char *)&StCalDat.UsVerDat) ;

	return;
}

//********************************************************************************
// Function Name 	: VerInf
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: F/W Version Check
// History			: First edition 						2013.03.21 Y.Kim
//********************************************************************************
void	VerInf(void)
{
	UcVerHig = (unsigned char)(StCalDat.UsVerDat >> 8) ;		// System Version
	UcVerLow = (unsigned char)(StCalDat.UsVerDat)  ;			// Filter Version

	printk("%s : %x, %x \n",__func__, UcVerHig, UcVerLow);

	if (UcVerHig == 0xFF){						// If System Version Info not exist
		UcVerHig = 0x00 ;
	}else if (UcVerHig <= (unsigned char)0x02){		// Not use old Version
		UcVerHig = 0x00 ;
	};

	if (UcVerLow == 0xFF){							// If System Version Info not exist
		UcVerLow = 0x00 ;
	}else if (UcVerLow <= (unsigned char)0x04){		// Not use old Version
		UcVerLow = 0x00 ;
	};

	return;
}

//********************************************************************************
// Function Name 	: IniClk
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Clock Setting
// History			: First edition 						2009.07.30 Y.Tashita
//					  LC898111 changes						2011.04.08 d.yamagata
//********************************************************************************
void	IniClk(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	UcOscAdjFlg	= 0 ;					// Osc adj flag

	RegWrite8(client, CLKON,	0x13) ;		// 0x020B	 [ - | - | CmCalClkOn  | CMGifClkOn  | CmPezClkOn  | CmEepClkOn  | CmSrvClkOn  | CmPwmClkOn  ]

	return;
}

//********************************************************************************
// Function Name 	: IniIop
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: I/O Port Initial Setting
// History			: First edition 						2009.07.30 Y.Tashita
//					  LC898111 changes						2011.04.08 d.yamagata
//********************************************************************************
void	IniIop(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	/*select IOP signal*/
	RegWrite8(client, IOP0SEL, 0x00); 	// 0x0230	[1:0] 00: DGMOSI, 01: HPS_CTL0, 1x: IOP0
	RegWrite8(client, IOP3SEL, 0x01); 	// 0x0233	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: DGINT, 01: MON, 1x: IOP3
	RegWrite8(client, IOP4SEL, 0x00); 	// 0x0234	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: BUSY1/EPSIIF, 01: MON, 1x: IOP4
	RegWrite8(client, IOP5SEL, 0x21); 	// 0x0235	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: BUSY2, 01: MON, 1x: IOP5
	RegWrite8(client, IOP6SEL, 0x11); 	// 0x0236	[5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND
	//			[1:0] 00: EPSIIF, 01: MON, 1x: IOP6
	RegWrite8(client, SRMODE, 0x02);		// 0x0251	[1]    0: SRAM DL ON, 1: OFF
	//			[0]    0: USE SRAM OFF, 1: ON

	return;
}

int	IniDgy(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char	UcRegBak ;
	unsigned char	UcRamIni0, UcRamIni1, UcCntPla, UcCntPlb ;

	UcCntPla = 0 ;
	UcCntPlb = 0 ;

	/*Gyro Filter Setting*/
	RegWrite8(client, GGADON	, 0x01);		// 0x011C		[ - | - | - | CmSht2PanOff ][ - | - | CmGadOn(1:0) ]
	RegWrite8(client, GGADSMPT , 0x0E);		// 0x011F
	/*Set to Command Mode*/
	RegWrite8(client, GRSEL	, 0x01);							// 0x0380	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	/*Digital Gyro Read settings*/
	RegWrite8(client, GRINI	, 0x80);							// 0x0381	[ PARA_REG | AXIS7EN | AXIS4EN | - ][ LSBF | SLOWMODE | I2CMODE | - ]

	// IDG-2021 Register Write Max1MHz
	RegRead8(client, GIFDIV, &UcRegBak) ;
	RegWrite8(client, GIFDIV, 0x04) ;				// 48MHz / 4 = 12MHz
	RegWrite8(client, GRINI, 0x84) ;				// SPI Clock = Slow Mode 12MHz / 4 / 4 = 750KHz

	// Gyro Clock Setting
	RegWrite8(client, GRADR0	, 0x6B) ;					// 0x0383	Set USER CONTROL
	RegWrite8(client, GSETDT	, 0x01) ;					// 0x038A	Set Write Data
	RegWrite8(client, GRACC	, 0x10);					// 0x0382	[ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ]

	if (AccWit(0x10) == OIS_FW_POLLING_FAIL){ return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/

	UcRamIni0 = 0x02 ;
	do {
		RegWrite8(client, GRACC,	0x01) ;				/* 0x0382	Set 1Byte Read Trigger ON			*/
		if (AccWit(0x01) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }	/* Digital Gyro busy wait 				*/
		RegRead8(client, GRADT0H, &UcRamIni0) ;			/* 0x0390	*/
		UcCntPla++ ;
	} while ((UcRamIni0 & 0x02) && (UcCntPla < INIDGY_POLLING_LIMIT_A));
	if (UcCntPla == INIDGY_POLLING_LIMIT_A) { return OIS_FW_POLLING_FAIL; }

	WitTim(2);
	spigyrocheck = UcRamIni0;

	RegWrite8(client, GRADR0,	0x1B) ;					// 0x0383	Set GYRO_CONFIG
	RegWrite8(client, GSETDT,	(FS_SEL << 3)) ;			// 0x038A	Set Write Data //FS_SEL 2 ; 0x10 :
	RegWrite8(client, GRACC,	0x10) ;					/* 0x0382	Set Trigger ON				*/
	if (AccWit(0x10) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/
	/* SIG_COND_RESET ---< */

	/* SIG_COND_RESET ---> */
	RegWrite8(client, GRADR0	, 0x6A) ;					// 0x0383	Set USER CONTROL
	RegWrite8(client, GSETDT	, 0x11) ;					// 0x038A	Set Write Data
	RegWrite8(client, GRACC	, 0x10);					// 0x0382	[ ADRPLUS(1:0) | - | WR1B ][ - | RD4B | RD2B | RD1B ]


	if (AccWit(0x10) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }			/* Digital Gyro busy wait 				*/

	UcRamIni1 = 0x01 ;
	do {
		RegWrite8(client, GRACC,	0x01) ;				/* 0x0382	Set 1Byte Read Trigger ON			*/
		if (AccWit(0x01) == OIS_FW_POLLING_FAIL) { return OIS_FW_POLLING_FAIL; }		/* Digital Gyro busy wait 				*/
		RegRead8(client, GRADT0H, &UcRamIni1) ;			/* 0x0390	*/
		UcCntPlb++ ;
	} while ((UcRamIni1 & 0x01) && (UcCntPlb < INIDGY_POLLING_LIMIT_B)) ;
	if (UcCntPlb == INIDGY_POLLING_LIMIT_B) { return OIS_FW_POLLING_FAIL; }

	RegWrite8(client, GIFDIV, UcRegBak) ;			// 48MHz / 3 = 16MHz
	RegWrite8(client, GRINI, 0x80) ;				// SPI Clock = 16MHz / 4 = 4MHz

	// Gyro Signal Output Select
	RegWrite8(client, GRADR0,	0x43) ;			// 0x0383	Set Gyro XOUT H~L
	RegWrite8(client, GRADR1,	0x45) ;			// 0x0384	Set Gyro YOUT H~L

	/*Start OIS Reading*/
	RegWrite8(client, GRSEL	, 0x02);							// 0x0380	[ - | - | - | - ][ - | SRDMOE | OISMODE | COMMODE ]

	return OIS_FW_POLLING_PASS;
}

void	IniMon(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	RegWrite8(client, PWMMONFC, 0x00) ;				// 0x00F4
	RegWrite8(client, DAMONFC, 0x81) ;				// 0x00F5

	RegWrite8(client, MONSELA, 0x57) ;				// 0x0270
	RegWrite8(client, MONSELB, 0x58) ;				// 0x0271
	RegWrite8(client, MONSELC, 0x56) ;				// 0x0272
	RegWrite8(client, MONSELD, 0x63) ;				// 0x0273

	return;
}

//********************************************************************************
// Function Name 	: IniSrv
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Servo Initial Setting
// History			: First edition 						2009.07.30 Y.Tashita
//********************************************************************************
void	IniSrv(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	RegWrite8(client, LXEQEN 	, 0x00);				// 0x0084
	RegWrite8(client, LYEQEN 	, 0x00);				// 0x008E

	RamWrite16(client, ADHXOFF,   0x0000) ;			// 0x1102
	RamWrite16(client, ADSAD4OFF, 0x0000) ;			// 0x110E
	RamWrite16(client, HXINOD,    0x0000) ;			// 0x1127
	RamWrite16(client, HXDCIN,    0x0000) ;			// 0x1126
	RamWrite16(client, HXSEPT1,    0x0000) ;			// 0x1123
	RamWrite16(client, HXSEPT2,    0x0000) ;			// 0x1124
	RamWrite16(client, HXSEPT3,    0x0000) ;			// 0x1125
	RamWrite16(client, LXDOBZ,     0x0000) ;			// 0x114A
	RamWrite16(client, LXFZF,      0x0000) ;			// 0x114B
	RamWrite16(client, LXFZB,      0x0000) ;			// 0x1156
	RamWrite16(client, LXDX,      0x0000) ;			// 0x1148
	RamWrite16(client, LXLMT,     0x7FFF) ;			// 0x1157
	RamWrite16(client, LXLMT2,    0x7FFF) ;			// 0x1158
	RamWrite16(client, LXLMTSD,   0x0000) ;			// 0x1159
	RamWrite16(client, PLXOFF,    0x0000) ;			// 0x115B
	RamWrite16(client, LXDODAT,   0x0000) ;			// 0x115A

	RamWrite16(client, ADHYOFF,   0x0000) ;			// 0x1105
	RamWrite16(client, HYINOD,    0x0000) ;			// 0x1167
	RamWrite16(client, HYDCIN,    0x0000) ;			// 0x1166
	RamWrite16(client, HYSEPT1,    0x0000) ;			// 0x1163
	RamWrite16(client, HYSEPT2,    0x0000) ;			// 0x1164
	RamWrite16(client, HYSEPT3,    0x0000) ;			// 0x1165
	RamWrite16(client, LYDOBZ,     0x0000) ;			// 0x118A
	RamWrite16(client, LYFZF,      0x0000) ;			// 0x118B
	RamWrite16(client, LYFZB,      0x0000) ;			// 0x1196
	RamWrite16(client, LYDX,      0x0000) ;			// 0x1188
	RamWrite16(client, LYLMT,     0x7FFF) ;			// 0x1197
	RamWrite16(client, LYLMT2,    0x7FFF) ;			// 0x1198
	RamWrite16(client, LYLMTSD,   0x0000) ;			// 0x1199
	RamWrite16(client, PLYOFF,    0x0000) ;			// 0x119B
	RamWrite16(client, LYDODAT,   0x0000) ;			// 0x119A

	RamWrite16(client, LXOLMT,		0x2C00);			// 0x121A		// 165mA Limiter for CVL Mode
	RamWrite16(client, LYOLMT,		0x2C00);			// 0x121B		// 165mA Limiter for CVL Mode
	RegWrite8(client, LXEQFC2 , 0x09) ;				// 0x0083		Linear Mode ON
	RegWrite8(client, LYEQFC2 , 0x09) ;				// 0x008D

	/* Calculation flow   X : Y1->X1    Y : X2->Y2 */
	RegWrite8(client, LCXFC, (unsigned char)0x00) ;			// 0x0001	High-order function X function setting
	RegWrite8(client, LCYFC, (unsigned char)0x00) ;			// 0x0006	High-order function Y function setting

	RegWrite8(client, LCY1INADD, (unsigned char)LXDOIN) ;		// 0x0007	High-order function Y1 input selection
	RegWrite8(client, LCY1OUTADD, (unsigned char)DLY00) ;		// 0x0008	High-order function Y1 output selection
	RegWrite8(client, LCX1INADD, (unsigned char)DLY00) ;		// 0x0002	High-order function X1 input selection
	RegWrite8(client, LCX1OUTADD, (unsigned char)LXADOIN) ;	// 0x0003	High-order function X1 output selection

	RegWrite8(client, LCX2INADD, (unsigned char)LYDOIN) ;		// 0x0004	High-order function X2 input selection
	RegWrite8(client, LCX2OUTADD, (unsigned char)DLY01) ;		// 0x0005	High-order function X2 output selection
	RegWrite8(client, LCY2INADD, (unsigned char)DLY01) ;		// 0x0009	High-order function Y2 input selection
	RegWrite8(client, LCY2OUTADD, (unsigned char)LYADOIN) ;	// 0x000A	High-order function Y2 output selection

	/* (0.5468917X^3+0.3750114X)*(0.5468917X^3+0.3750114X) 6.5ohm*/
	RamWrite16(client, LCY1A0, 0x0000) ;			// 0x12F2	0
	RamWrite16(client, LCY1A1, 0x4600) ;			// 0x12F3	1
	RamWrite16(client, LCY1A2, 0x0000) ;			// 0x12F4	2
	RamWrite16(client, LCY1A3, 0x3000) ;			// 0x12F5	3
	RamWrite16(client, LCY1A4, 0x0000) ;			// 0x12F6	4
	RamWrite16(client, LCY1A5, 0x0000) ;			// 0x12F7	5
	RamWrite16(client, LCY1A6, 0x0000) ;			// 0x12F8	6

	RamWrite16(client, LCX1A0, 0x0000) ;			// 0x12D2	0
	RamWrite16(client, LCX1A1, 0x4600) ;			// 0x12D3	1
	RamWrite16(client, LCX1A2, 0x0000) ;			// 0x12D4	2
	RamWrite16(client, LCX1A3, 0x3000) ;			// 0x12D5	3
	RamWrite16(client, LCX1A4, 0x0000) ;			// 0x12D6	4
	RamWrite16(client, LCX1A5, 0x0000) ;			// 0x12D7	5
	RamWrite16(client, LCX1A6, 0x0000) ;			// 0x12D8	6

	RamWrite16(client, LCX2A0, 0x0000) ;			// 0x12D9	0
	RamWrite16(client, LCX2A1, 0x4600) ;			// 0x12DA	1
	RamWrite16(client, LCX2A2, 0x0000) ;			// 0x12DB	2
	RamWrite16(client, LCX2A3, 0x3000) ;			// 0x12DC	3
	RamWrite16(client, LCX2A4, 0x0000) ;			// 0x12DD	4
	RamWrite16(client, LCX2A5, 0x0000) ;			// 0x12DE	5
	RamWrite16(client, LCX2A6, 0x0000) ;			// 0x12DF	6

	RamWrite16(client, LCY2A0, 0x0000) ;			// 0x12F9	0
	RamWrite16(client, LCY2A1, 0x4600) ;			// 0x12FA	1
	RamWrite16(client, LCY2A2, 0x0000) ;			// 0x12FB	2
	RamWrite16(client, LCY2A3, 0x3000) ;			// 0x12FC	3
	RamWrite16(client, LCY2A4, 0x0000) ;			// 0x12FD	4
	RamWrite16(client, LCY2A5, 0x0000) ;			// 0x12FE	5
	RamWrite16(client, LCY2A6, 0x0000) ;			// 0x12FF	6

	RegWrite8(client, GDPX1INADD,  0x00) ;		// 0x00A5	Default Setting
	RegWrite8(client, GDPX1OUTADD, 0x00) ;		// 0x00A6	General Data Pass Output Cut Off
	RegWrite8(client, GDPX2INADD,  0x00) ;		// 0x00A7	Default Setting
	RegWrite8(client, GDPX2OUTADD, 0x00) ;		// 0x00A8	General Data Pass Output Cut Off

	// Gyro Filter Interface
	RegWrite8(client, GYINFC, 0x00) ;				// 0x00DA		LXGZB,LYGZB Input Cut Off, 0 Sampling Delay, Down Sampling 1/1

	// Sin Wave Generater
	RegWrite8(client, SWEN, 0x08) ;				// 0x00DB		Sin Wave Generate OFF, Sin Wave Setting
	RegWrite8(client, SWFC2, 0x08) ;				// 0x00DE		SWC = 0

	// Delay RAM Monitor
	RegWrite8(client, MDLY1ADD, 0x10) ;			// 0x00E5		Delay Monitor1
	RegWrite8(client, MDLY2ADD, 0x11) ;			// 0x00E6		Delay Monitor2

	// Delay RAM Clear
	//BsyWit(DLYCLR, 0xFF) ;				// 0x00EE	Delay RAM All Clear
	//BsyWit(DLYCLR2, 0xEC) ;				// 0x00EF	Delay RAM All Clear
	RegWrite8(client, DLYCLR, 0xFF) ;				// 0x00EE	Delay RAM All Clear
	WitTim(1);
	RegWrite8(client, DLYCLR2, 0xEC) ;				// 0x00EF	Delay RAM All Clear
	WitTim(1);
	RegWrite8(client, DLYCLR	, 0x00);			// 0x00EE	CLR disable

	// Hall Amp...
	RegWrite8(client, RTXADD, 0x00) ;				// 0x00CE	Cal OFF
	RegWrite8(client, RTYADD, 0x00) ;				// 0x00E8	Cal OFF

	// PWM Signal Generate
	DrvSw(OFF) ;							/* 0x0070	Driver Block Ena=0 */
	RegWrite8(client, DRVFC2	, 0x40);			// 0x0068	PriDriver:Slope, Driver:DeadTime 12.5ns

	RegWrite8(client, PWMFC,   0x5C) ;			// 0x0075	VREF, PWMCLK/64, MODE1, 12Bit Accuracy
	RegWrite8(client, PWMPERIODX, 0x00);			// 0x0062
	RegWrite8(client, PWMPERIODY, 0x00);			// 0x0063

	RegWrite8(client, PWMA,    0x00) ;			// 0x0074	PWM X/Y standby
	RegWrite8(client, PWMDLY1,  0x03) ;			// 0x0076	X Phase Delay Setting
	RegWrite8(client, PWMDLY2,  0x03) ;			// 0x0077	Y Phase Delay Setting

	RegWrite8(client, LNA		, 0xC0);			// 0x0078	Low Noise mode enable
	RegWrite8(client, LNFC 	, 0x02);			// 0x0079
	RegWrite8(client, LNSMTHX	, 0x80);			// 0x007A
	RegWrite8(client, LNSMTHY	, 0x80);			// 0x007B

	// Flag Monitor
	RegWrite8(client, FLGM, 0xCC) ;				// 0x00F8	BUSY2 Output ON
	RegWrite8(client, FLGIST, 0xCC) ;				// 0x00F9	Interrupt Clear
	RegWrite8(client, FLGIM2, 0xF8) ;				// 0x00FA	BUSY2 Output ON
	RegWrite8(client, FLGIST2, 0xF8) ;			// 0x00FB	Interrupt Clear

	// Function Setting
	RegWrite8(client, FCSW, 0x00) ;				// 0x00F6	2Axis Input, PWM Mode, X,Y Axis Reverse OFF
	RegWrite8(client, FCSW2, 0x00) ;				// 0x00F7	X,Y Axis Invert OFF, PWM Synchronous, A/D Over Sampling ON

	/* Srv Smooth start */
	RamWrite16(client, HXSMSTP   , 0x0400) ;					/* 0x1120	*/
	RamWrite16(client, HYSMSTP   , 0x0400) ;					/* 0x1160	*/

	RegWrite8(client, SSSFC1, 0x43) ;				// 0x0098	0.68ms * 8times = 5.46ms
	RegWrite8(client, SSSFC2, 0x03) ;				// 0x0099	1.36ms * 3 = 4.08ms
	RegWrite8(client, SSSFC3, 0x50) ;				// 0x009A	1.36ms

	RegWrite8(client, STBB, 0x00) ;				// 0x0260	All standby

	return;
}

void	IniGyr(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	RegWrite8(client, GEQON	, 0x00);				// 0x0100		[ - | - | - | - ][ - | - | - | CmEqOn ]

	/*Initialize Gyro RAM*/
	ClrGyr(0x00 , 0x03);

	/*Gyro Filter Setting*/
	RegWrite8(client, GGADON	, 0x01);		// 0x011C		[ - | - | - | CmSht2PanOff ][ - | - | CmGadOn(1:0) ]
	RegWrite8(client, GGADSMPT , 0x0E);		// 0x011F

	// Limiter
	RamWrite32(client, gxlmt1L, 0x00000000) ;	// 0x18B0
	RamWrite32(client, gxlmt1H, 0x3F800000) ;	// 0x18B1
	RamWrite32(client, gxlmt2L, 0x3B16BB99) ;	// 0x18B2	0.0023
	RamWrite32(client, gxlmt2H, 0x3F800000) ;	// 0x18B3

	RamWrite32(client, gylmt1L, 0x00000000) ;	// 0x19B0
	RamWrite32(client, gylmt1H, 0x3F800000) ;	// 0x19B1
	RamWrite32(client, gylmt2L, 0x3B16BB99) ;	// 0x19B2	0.0023
	RamWrite32(client, gylmt2H, 0x3F800000) ;	// 0x19B3

	// Limiter3
	RamWrite32(client, gxlmt3H0, 0x3E800000) ;	// 0x18B4
	RamWrite32(client, gylmt3H0, 0x3E800000) ;	// 0x19B4	Limiter = 0.25, 0.7deg
	RamWrite32(client, gxlmt3H1, 0x3E800000) ;	// 0x18B5
	RamWrite32(client, gylmt3H1, 0x3E800000) ;	// 0x19B5

	RamWrite32(client, gxlmt4H0, GYRLMT_S1) ;	//0x1808	X Axis Limiter4 High Threshold 0
	RamWrite32(client, gylmt4H0, GYRLMT_S1) ;	//0x1908	Y Axis Limiter4 High Threshold 0

	RamWrite32(client, gxlmt4H1, GYRLMT_S2) ;	//0x1809	X Axis Limiter4 High Threshold 1
	RamWrite32(client, gylmt4H1, GYRLMT_S2) ;	//0x1909	Y Axis Limiter4 High Threshold 1

	// Monitor Circuit
	RegWrite8(client, GDLYMON10, 0xF5) ;			// 0x0184
	RegWrite8(client, GDLYMON11, 0x01) ;			// 0x0185
	RegWrite8(client, GDLYMON20, 0xF5) ;			// 0x0186
	RegWrite8(client, GDLYMON21, 0x00) ;			// 0x0187
	RamWrite32(client, gdm1g, 0x3F800000) ;		// 0x18AC
	RamWrite32(client, gdm2g, 0x3F800000) ;		// 0x19AC

	/* Pan/Tilt parameter */
	RegWrite8(client, GPANADDA, 0x14) ;			// 0x0130	GXH1Z2/GYH1Z2
	RegWrite8(client, GPANADDB, 0x0E) ;			// 0x0131	GXI3Z/GYI3Z

	//Threshold
	RamWrite32(client, SttxHis, 	0x00000000);			// 0x183F
	RamWrite32(client, SttyHis, 	0x00000000);			// 0x193F
	RamWrite32(client, SttxaL, 	0x00000000);			// 0x18AE
	RamWrite32(client, SttxbL, 	0x00000000);			// 0x18BE
	RamWrite32(client, Sttx12aM, 	GYRA12_MID_DEG);		// 0x184F
	RamWrite32(client, Sttx12aH, 	GYRA12_HGH_DEG);		// 0x185F
	RamWrite32(client, Sttx12bM, 	GYRB12_MID);			// 0x186F
	RamWrite32(client, Sttx12bH, 	GYRB12_HGH);			// 0x187F
	RamWrite32(client, Sttx34aM, 	GYRA34_MID_DEG);		// 0x188F
	RamWrite32(client, Sttx34aH, 	GYRA34_HGH_DEG);		// 0x189F
	RamWrite32(client, Sttx34bM, 	GYRB34_MID);			// 0x18AF
	RamWrite32(client, Sttx34bH, 	GYRB34_HGH);			// 0x18BF
	RamWrite32(client, SttyaL, 	0x00000000);			// 0x19AE
	RamWrite32(client, SttybL, 	0x00000000);			// 0x19BE
	RamWrite32(client, Stty12aM, 	GYRA12_MID_DEG);		// 0x194F
	RamWrite32(client, Stty12aH, 	GYRA12_HGH_DEG);		// 0x195F
	RamWrite32(client, Stty12bM, 	GYRB12_MID);			// 0x196F
	RamWrite32(client, Stty12bH, 	GYRB12_HGH);			// 0x197F
	RamWrite32(client, Stty34aM, 	GYRA34_MID_DEG);		// 0x198F
	RamWrite32(client, Stty34aH, 	GYRA34_HGH_DEG);		// 0x199F
	RamWrite32(client, Stty34bM, 	GYRB34_MID);			// 0x19AF
	RamWrite32(client, Stty34bH, 	GYRB34_HGH);			// 0x19BF

	// Phase Transition Setting
	RegWrite8(client, GPANSTT31JUG0, 	0x01);		// 0x0142
	RegWrite8(client, GPANSTT31JUG1, 	0x00);		// 0x0143
	RegWrite8(client, GPANSTT13JUG0, 	0x00);		// 0x0148
	RegWrite8(client, GPANSTT13JUG1, 	0x07);		// 0x0149

	// State Timer
	//RegWrite8(client, GPANSTT1LEVTMR, 	0x01);		// 0x0160
	RegWrite8(client, GPANSTT1LEVTMR, 	0x00);		// 0x0160

	// Control filter
	RegWrite8(client, GPANTRSON0, 		0x01);		// 0x0132		// gxgain/gygain, gxistp/gyistp
	RegWrite8(client, GPANTRSON1, 		0x1C);		// 0x0133		// I Filter Control

	// State Setting
	RegWrite8(client, GPANSTTSETGAIN, 	0x10);		// 0x0155
	RegWrite8(client, GPANSTTSETISTP, 	0x10);		// 0x0156
	RegWrite8(client, GPANSTTSETI1FTR,	0x10);		// 0x0157
	RegWrite8(client, GPANSTTSETI2FTR,	0x10);		// 0x0158

	// State2,4 Step Time Setting
	RegWrite8(client, GPANSTT2TMR0,	0xEA);		// 0x013C
	RegWrite8(client, GPANSTT2TMR1,	0x00);		// 0x013D
	RegWrite8(client, GPANSTT4TMR0,	0x92);		// 0x013E
	RegWrite8(client, GPANSTT4TMR1,	0x04);		// 0x013F

	RegWrite8(client, GPANSTTXXXTH,	0x0F);		// 0x015D

	RegWrite8(client, GPANSTTSETILHLD,	0x00);		// 0x0168

	RamWrite32(client, gxlevmid, TRI_LEVEL);					// 0x182D	Low Th
	RamWrite32(client, gxlevhgh, TRI_HIGH);					// 0x182E	Hgh Th
	RamWrite32(client, gylevmid, TRI_LEVEL);					// 0x192D	Low Th
	RamWrite32(client, gylevhgh, TRI_HIGH);					// 0x192E	Hgh Th
	RamWrite32(client, gxadjmin, XMINGAIN);					// 0x18BA	Low gain
	RamWrite32(client, gxadjmax, XMAXGAIN);					// 0x18BB	Hgh gain
	RamWrite32(client, gxadjdn, XSTEPDN);					// 0x18BC	-step
	RamWrite32(client, gxadjup, XSTEPUP);					// 0x18BD	+step
	RamWrite32(client, gyadjmin, YMINGAIN);					// 0x19BA	Low gain
	RamWrite32(client, gyadjmax, YMAXGAIN);					// 0x19BB	Hgh gain
	RamWrite32(client, gyadjdn, YSTEPDN);					// 0x19BC	-step
	RamWrite32(client, gyadjup, YSTEPUP);					// 0x19BD	+step

	RegWrite8(client, GLEVGXADD, (unsigned char)XMONADR);		// 0x0120	Input signal
	RegWrite8(client, GLEVGYADD, (unsigned char)YMONADR);		// 0x0124	Input signal
	RegWrite8(client, GLEVTMR, 		TIMEBSE);				// 0x0128	Base Time
	RegWrite8(client, GLEVTMRLOWGX, 	TIMELOW);				// 0x0121	X Low Time
	RegWrite8(client, GLEVTMRMIDGX, 	TIMEMID);				// 0x0122	X Mid Time
	RegWrite8(client, GLEVTMRHGHGX, 	TIMEHGH);				// 0x0123	X Hgh Time
	RegWrite8(client, GLEVTMRLOWGY, 	TIMELOW);				// 0x0125	Y Low Time
	RegWrite8(client, GLEVTMRMIDGY, 	TIMEMID);				// 0x0126	Y Mid Time
	RegWrite8(client, GLEVTMRHGHGY, 	TIMEHGH);				// 0x0127	Y Hgh Time
	RegWrite8(client, GADJGANADD, (unsigned char)GANADR);		// 0x012A	control address
	RegWrite8(client, GADJGANGO, 		0x00);					// 0x0108	manual off

	SetPanTiltMode(OFF) ;								/* Pan/Tilt OFF */

	return;
}

int	IniHfl(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned short	UsAryIda, UsAryIdb ;

	// Hall&Gyro Register Parameter Setting
	UsAryIda	= 0 ;
	while (CsHalReg[UcVerLow][ UsAryIda].UsRegAdd != 0xFFFF)
	{
		RegWrite8(client, CsHalReg[UcVerLow][ UsAryIda].UsRegAdd, CsHalReg[UcVerLow][ UsAryIda].UcRegDat) ;
		UsAryIda++ ;
		if (UsAryIda > HALREGTAB){ return OIS_FW_POLLING_FAIL ; }
	}

	// Hall Filter Parameter Setting
	UsAryIdb	= 0 ;
	while (CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd != 0xFFFF)
	{
		if (CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd < gag) {
			RamWrite16(client, CsHalFil[UcVerLow][ UsAryIdb].UsRamAdd, CsHalFil[UcVerLow][ UsAryIdb].UsRamDat) ;
		}
		UsAryIdb++ ;
		if (UsAryIdb > HALFILTAB){ return OIS_FW_POLLING_FAIL ; }
	}

	if ((unsigned char)(StCalDat.UsVerDat) <= (unsigned char)0x01){		// X Reverse
		RamWrite16(client, plxg, 0x8001);
	}

	return OIS_FW_POLLING_PASS ;
}

int	IniGfl(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned short	UsAryIda ;

	// Gyro Filter Parameter Setting
	UsAryIda	= 0 ;

	while (CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd != 0xFFFF)
	{
		if ((CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd & 0xFEFF) < gxi2a_a) {
			RamWrite32(client, CsGyrFil[UcVerLow][ UsAryIda].UsRamAdd, CsGyrFil[UcVerLow][ UsAryIda].UlRamDat) ;
		}
		UsAryIda++ ;
		if (UsAryIda > GYRFILTAB){ return OIS_FW_POLLING_FAIL ; }
	}

	return OIS_FW_POLLING_PASS ;
}

void	IniAdj(void)
{
//	unsigned long	UlLpfCof ;
	struct i2c_client *client = lgit_ois_func_tbl.client;

	RegWrite8(client, CMSDAC, BIAS_CUR) ;				// 0x0261	Hall Dac Current
	RegWrite8(client, OPGSEL, AMP_GAIN) ;				// 0x0262	Hall Amp Gain

	/* Hall Xaxis Bias,Offset */
	if ((StCalDat.UsAdjCompF == 0x0000) || (StCalDat.UsAdjCompF == 0xFFFF) || (StCalDat.UsAdjCompF & (EXE_HXADJ - EXE_END))){
		RamWrite16(client, DAHLXO, DAHLXO_INI) ;	// 0x1114
		RamWrite16(client, DAHLXB, DAHLXB_INI) ;	// 0x1115
		RamWrite16(client, ADHXOFF, 0x0000) ;		// 0x1102
	}else{
		RamWrite16(client, DAHLXO, StCalDat.StHalAdj.UsHlxOff) ;	// 0x1114
		RamWrite16(client, DAHLXB, StCalDat.StHalAdj.UsHlxGan) ;	// 0x1115
		RamWrite16(client, ADHXOFF, StCalDat.StHalAdj.UsAdxOff) ;	// 0x1102
	}

	/* Hall Yaxis Bias,Offset */
	if ((StCalDat.UsAdjCompF == 0x0000) || (StCalDat.UsAdjCompF == 0xFFFF) || (StCalDat.UsAdjCompF & (EXE_HYADJ - EXE_END))){
		RamWrite16(client, DAHLYO, DAHLYO_INI) ;	// 0x1116
		RamWrite16(client, DAHLYB, DAHLYB_INI) ;	// 0x1117
		RamWrite16(client, ADHYOFF, 0x0000) ;		// 0x1105
	}else{
		RamWrite16(client, DAHLYO, StCalDat.StHalAdj.UsHlyOff) ;	// 0x1116
		RamWrite16(client, DAHLYB, StCalDat.StHalAdj.UsHlyGan) ;	// 0x1117
		RamWrite16(client, ADHYOFF, StCalDat.StHalAdj.UsAdyOff) ;	// 0x1105
	}

	/* Hall Xaxis Loop Gain */
	if ((StCalDat.UsAdjCompF == 0x0000) || (StCalDat.UsAdjCompF == 0xFFFF) || (StCalDat.UsAdjCompF & (EXE_LXADJ - EXE_END))){
		RamWrite16(client, lxgain, LXGAIN_INI) ;	// 0x132A
	}else{
		RamWrite16(client, lxgain, StCalDat.StLopGan.UsLxgVal) ;	// 0x132A
	}

	/* Hall Yaxis Loop Gain */
	if ((StCalDat.UsAdjCompF == 0x0000) || (StCalDat.UsAdjCompF == 0xFFFF) || (StCalDat.UsAdjCompF & (EXE_LYADJ - EXE_END))){
		RamWrite16(client, lygain, LYGAIN_INI) ;	// 0x136A
	}else{
		RamWrite16(client, lygain, StCalDat.StLopGan.UsLygVal) ;	// 0x136A
	}

	/* Lens Center */
	// X axis Lens Center Offset Read & Setting
	if ((StCalDat.StLenCen.UsLsxVal != 0x0000) && (StCalDat.StLenCen.UsLsxVal != 0xffff)){
		UsCntXof = StCalDat.StLenCen.UsLsxVal ;					/* Set Lens center X value */
	} else {
		UsCntXof = MECCEN_X ;						/* Clear Lens center X value */
	}
	RamWrite16(client, HXINOD, UsCntXof) ;				// 0x1127

	// Y axis Lens Center Offset Read & Setting
	if ((StCalDat.StLenCen.UsLsyVal != 0x0000) && (StCalDat.StLenCen.UsLsyVal != 0xffff)){
		UsCntYof = StCalDat.StLenCen.UsLsyVal ;					/* Set Lens center Y value */
	} else {
		UsCntYof = MECCEN_Y ;						/* Clear Lens center Y value */
	}
	RamWrite16(client, HYINOD, UsCntYof) ;				// 0x1167

	/* Gyro Xaxis Offset */
	if ((StCalDat.StGvcOff.UsGxoVal == 0x0000) || (StCalDat.StGvcOff.UsGxoVal == 0xffff)){
		RamWrite16(client, ADGXOFF, 0x0000) ;							// 0x1108
		RegWrite8(client, IZAH, DGYRO_OFST_XH) ;						// 0x03A0		Set Offset High byte
		RegWrite8(client, IZAL, DGYRO_OFST_XL) ;						// 0x03A1		Set Offset Low byte
	}else{
		RamWrite16(client, ADGXOFF, 0x0000) ;							// 0x1108
		RegWrite8(client, IZAH, (unsigned char)(StCalDat.StGvcOff.UsGxoVal >> 8)) ;	// 0x03A0		Set Offset High byte
		RegWrite8(client, IZAL, (unsigned char)(StCalDat.StGvcOff.UsGxoVal)) ;			// 0x03A1		Set Offset Low byte
	}

	/* Gyro Yaxis Offset */
	if ((StCalDat.StGvcOff.UsGyoVal == 0x0000) || (StCalDat.StGvcOff.UsGyoVal == 0xffff)){
		RamWrite16(client, ADGYOFF, 0x0000) ;							// 0x110B
		RegWrite8(client, IZBH, DGYRO_OFST_YH) ;						// 0x03A2		Set Offset High byte
		RegWrite8(client, IZBL, DGYRO_OFST_YL) ;						// 0x03A3		Set Offset Low byte
	}else{
		RamWrite16(client, ADGYOFF, 0x0000) ;							// 0x110B
		RegWrite8(client, IZBH, (unsigned char)(StCalDat.StGvcOff.UsGyoVal >> 8)) ;	// 0x03A2		Set Offset High byte
		RegWrite8(client, IZBL, (unsigned char)(StCalDat.StGvcOff.UsGyoVal)) ;			// 0x03A3		Set Offset Low byte
	}

	/* Gyro Xaxis Gain */
	if ((StCalDat.UlGxgVal != 0x00000000) && (StCalDat.UlGxgVal != 0xffffffff)){
		RamWrite32(client, gxzoom, StCalDat.UlGxgVal) ;		// 0x1828 Gyro X axis Gain adjusted value
	}else{
		RamWrite32(client, gxzoom, GXGAIN_INI) ;		// 0x1828 Gyro X axis Gain adjusted initial value
	}

	/* Gyro Yaxis Gain */
	if ((StCalDat.UlGygVal != 0x00000000) && (StCalDat.UlGygVal != 0xffffffff)){
		RamWrite32(client, gyzoom, StCalDat.UlGygVal) ;		// 0x1928 Gyro Y axis Gain adjusted value
	}else{
		RamWrite32(client, gyzoom, GYGAIN_INI) ;		// 0x1928 Gyro Y axis Gain adjusted initial value
	}

	/* OSC Clock value */
	if (((unsigned char)StCalDat.UsOscVal != 0x00) && ((unsigned char)StCalDat.UsOscVal != 0xff)){
		RegWrite8(client, OSCSET, (unsigned char)((unsigned char)StCalDat.UsOscVal | 0x01)) ;		// 0x0264
	}else{
		RegWrite8(client, OSCSET, OSC_INI) ;						// 0x0264
	}

	RamWrite16(client, hxinog, 0x8001) ;			// 0x1128	back up initial value
	RamWrite16(client, hyinog, 0x8001) ;			// 0x1168	back up initial value

	RegWrite8(client, STBB 	, 0x0F);							// 0x0260 	[ - | - | - | - ][ STBOPAY | STBOPAX | STBDAC | STBADC ]

	RegWrite8(client, LXEQEN 	, 0x45);			// 0x0084
	RegWrite8(client, LYEQEN 	, 0x45);			// 0x008E

	RamWrite32(client, gxistp_1, 0x00000000);
	RamWrite32(client, gyistp_1, 0x00000000);


	// I Filter X							// 1s
	RamWrite32(client, gxi1a_1, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gxi1b_1, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gxi1c_1, 0x33E6A3C6) ;		// Up

	RamWrite32(client, gxi1a_a, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gxi1b_a, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gxi1c_a, 0x33E6A3C6) ;		// Up

	RamWrite32(client, gxi1a_b, 0x3AAF73A1) ;		// 5Hz
	RamWrite32(client, gxi1b_b, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gxi1c_b, 0x3F800000) ;		// Up

	RamWrite32(client, gxi1a_c, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gxi1b_c, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gxi1c_c, 0x33E6A3C6) ;		// Up

	// I Filter Y
	RamWrite32(client, gyi1a_1, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gyi1b_1, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gyi1c_1, 0x33E6A3C6) ;		// Up

	RamWrite32(client, gyi1a_a, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gyi1b_a, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gyi1c_a, 0x33E6A3C6) ;		// Up

	RamWrite32(client, gyi1a_b, 0x3AAF73A1) ;		// 5Hz
	RamWrite32(client, gyi1b_b, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gyi1c_b, 0x3F800000) ;		// Up

	RamWrite32(client, gyi1a_c, 0x38A8A554) ;		// 0.3Hz
	RamWrite32(client, gyi1b_c, 0xB3E6A3C6) ;		// Down
	RamWrite32(client, gyi1c_c, 0x33E6A3C6) ;		// Up

	// gxgain								// 0.4s
	RamWrite32(client, gxl4a_1, 0x3F800000) ;		// 1
	RamWrite32(client, gxl4b_1, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gxl4c_1, 0x39DFB23C) ;		// Up
	RamWrite32(client, gxl4a_a, 0x3F800000) ;		// 1
	RamWrite32(client, gxl4b_a, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gxl4c_a, 0x39DFB23C) ;		// Up
	RamWrite32(client, gxl4a_b, 0x00000000) ;		// Cut Off
	RamWrite32(client, gxl4b_b, 0xBF800000) ;		// Down
	RamWrite32(client, gxl4c_b, 0x39DFB23C) ;		// Up
	RamWrite32(client, gxl4a_c, 0x3F800000) ;		// 1
	RamWrite32(client, gxl4b_c, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gxl4c_c, 0x39DFB23C) ;		// Up

	// gygain
	RamWrite32(client, gyl4a_1, 0x3F800000) ;		// 1
	RamWrite32(client, gyl4b_1, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gyl4c_1, 0x39DFB23C) ;		// Up
	RamWrite32(client, gyl4a_a, 0x3F800000) ;		// 1
	RamWrite32(client, gyl4b_a, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gyl4c_a, 0x39DFB23C) ;		// Up
	RamWrite32(client, gyl4a_b, 0x00000000) ;		// Cut Off
	RamWrite32(client, gyl4b_b, 0xBF800000) ;		// Down
	RamWrite32(client, gyl4c_b, 0x39DFB23C) ;		// Up
	RamWrite32(client, gyl4a_c, 0x3F800000) ;		// 1
	RamWrite32(client, gyl4b_c, 0xB9DFB23C) ;		// Down
	RamWrite32(client, gyl4c_c, 0x39DFB23C) ;		// Up

	// gxistp								// 0.2s
	RamWrite32(client, gxgyro_1, 0x00000000) ;		// Cut Off
	RamWrite32(client, gxgain_1, 0xBF800000) ;		// Down
	RamWrite32(client, gxistp_1, 0x3F800000) ;		// Up
	RamWrite32(client, gxgyro_a, 0x00000000) ;		// Cut Off
	RamWrite32(client, gxgain_a, 0xBF800000) ;		// Down
	RamWrite32(client, gxistp_a, 0x3F800000) ;		// Up
	RamWrite32(client, gxgyro_b, 0x37EC6C50) ;		// -91dB
	RamWrite32(client, gxgain_b, 0xBF800000) ;		// Down
	RamWrite32(client, gxistp_b, 0x3F800000) ;		// Up
	RamWrite32(client, gxgyro_c, 0x00000000) ;		// Cut Off
	RamWrite32(client, gxgain_c, 0xBF800000) ;		// Down
	RamWrite32(client, gxistp_c, 0x3F800000) ;		// Up

	// gyistp
	RamWrite32(client, gygyro_1, 0x00000000) ;		// Cut Off
	RamWrite32(client, gygain_1, 0xBF800000) ;		// Down
	RamWrite32(client, gyistp_1, 0x3F800000) ;		// Up
	RamWrite32(client, gygyro_a, 0x00000000) ;		// Cut Off
	RamWrite32(client, gygain_a, 0xBF800000) ;		// Down
	RamWrite32(client, gyistp_a, 0x3F800000) ;		// Up
	RamWrite32(client, gygyro_b, 0x37EC6C50) ;		// -91dB
	RamWrite32(client, gygain_b, 0xBF800000) ;		// Down
	RamWrite32(client, gyistp_b, 0x3F800000) ;		// Up
	RamWrite32(client, gygyro_c, 0x00000000) ;		// Cut Off
	RamWrite32(client, gygain_c, 0xBF800000) ;		// Down
	RamWrite32(client, gyistp_c, 0x3F800000) ;		// Up

	/* exe function */
	AutoGainControlSw(OFF) ;					/* Auto Gain Control Mode OFF */

	DrvSw(ON) ;								/* 0x0070		Driver Mode setting */

//	RegWrite8(client, G2NDCEFON0, 0x03) ;				// 0x0106
	RegWrite8(client, GEQON	, 0x01);				// 0x0100		[ - | - | - | - ][ - | - | - | CmEqOn ]
	RegWrite8(client, GPANFILMOD, 0x01) ;				// 0x0167
	SetPanTiltMode(ON) ;						/* Pan/Tilt ON */
	RegWrite8(client, GPANSTTFRCE, 0x44) ;			// 0x010A
	RegWrite8(client, GLSEL, 0x04) ;					// 0x017A

	RegWrite8(client, GHCHR, 0x11) ;					// 0x017B

	RamWrite32(client, gxl2a_2, 0x00000000) ;	// 0x1860	0
	RamWrite32(client, gxl2b_2, 0x3D829952) ;	// 0x1861	0.063769
	RamWrite32(client, gyl2a_2, 0x00000000) ;	// 0x1960	0
	RamWrite32(client, gyl2b_2, 0x3D829952) ;	// 0x1961	0.063769

	RamWrite32(client, gxl2c_2, 0xBA9CD414) ;	// 0x1862	-0.001196506	3F7FF800
	RamWrite32(client, gyl2c_2, 0xBA9CD414) ;	// 0x1962	-0.001196506	3F7FF800

	//RamWrite32(client, gxh1c_2, 0x3F7FFD00) ;	// 0x18A2
	//RamWrite32(client, gyh1c_2, 0x3F7FFD00) ;	// 0x19A2
	RamWrite32(client, gxh1c_2, 0x3F7FFD80) ;	// 0x18A2
	RamWrite32(client, gyh1c_2, 0x3F7FFD80) ;	// 0x19A2

	RegWrite8(client, GPANSTTFRCE, 0x11) ;									// 0x010A

	return;
}

void	MemClr(unsigned char	*NcTgtPtr, unsigned short	UsClrSiz)
{
	unsigned short	UsClrIdx ;

	for (UsClrIdx = 0 ; UsClrIdx < UsClrSiz ; UsClrIdx++)
	{
		*NcTgtPtr	= 0 ;
		NcTgtPtr++ ;
	}

	return;
}

int	AccWit(unsigned char UcTrgDat)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char	UcFlgVal ;
	unsigned char	UcCntPla ;
	UcFlgVal	= 1 ;
	UcCntPla	= 0 ;

	do {
		RegRead8(client, GRACC, &UcFlgVal) ;
		UcFlgVal	&= UcTrgDat ;
		UcCntPla++ ;
	} while (UcFlgVal && (UcCntPla < ACCWIT_POLLING_LIMIT_A)) ;
	if (UcCntPla == ACCWIT_POLLING_LIMIT_A) { return OIS_FW_POLLING_FAIL; }

	return OIS_FW_POLLING_PASS ;
}

void	AutoGainControlSw(unsigned char UcModeSw)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	if (UcModeSw == OFF)
	{
		RegWrite8(client, GADJGANGXMOD, 0x00) ;				// 0x0108	Gain Down
		RegWrite8(client, GADJGANGYMOD, 0x00) ;				// 0x0108	Gain Down
		RegWrite8(client, GADJGANGO,	 0x11) ;				// 0x0108	Gain Down
	}
	else
	{
		RegWrite8(client, GADJGANGO,		0x00) ;				// 0x0108	Gain Up
		RegWrite8(client, GADJGANGXMOD, 0xA7) ;				// 0x0108	Gain Down
		RegWrite8(client, GADJGANGYMOD, 0xA7) ;				// 0x0108	Gain Down
	}

	return;
}

int	ClrGyr(unsigned char UcClrFil , unsigned char UcClrMod)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char	UcRamClr;
	unsigned char	UcCntPla;

	UcCntPla = 0 ;

	/*Select Filter to clear*/
	RegWrite8(client, GRAMDLYMOD	, UcClrFil) ;	// 0x011B	[ - | - | - | P ][ T | L | H | I ]

	/*Enable Clear*/
	RegWrite8(client, GRAMINITON	, UcClrMod) ;	// 0x0103	[ - | - | - | - ][ - | - | ’x‰„Clr | ŒW”Clr ]

	/*Check RAM Clear complete*/
	do {
		RegRead8(client, GRAMINITON, &UcRamClr);
		UcRamClr &= 0x03;
		UcCntPla++;
	} while ((UcRamClr != 0x00) && (UcCntPla < CLRGYR_POLLING_LIMIT_A));
	if (UcCntPla == CLRGYR_POLLING_LIMIT_A) { return OIS_FW_POLLING_FAIL; }

	return OIS_FW_POLLING_PASS ;
}

void	DrvSw(unsigned char UcDrvSw)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	if (UcDrvSw == ON)	{
		RegWrite8(client, DRVFC	, 0xE3);					// 0x0070	MODE=2,Drvier Block Ena=1,FullMode=1
	} else {
		RegWrite8(client, DRVFC	, 0x00);					// 0x0070	Drvier Block Ena=0
	}

	return;
}
//==============================================================================
// OisIni.c Code END (2012.11.20)
//==============================================================================

unsigned char	RtnCen(unsigned char	UcCmdPar)
{
	unsigned char	UcCmdSts ;


	UcCmdSts	= EXE_END ;

	if (!UcCmdPar) {										// X,Y Centering
		StbOnn() ;
	} else if (UcCmdPar == 0x01) {							// X Centering Only

		SrvCon(X_DIR, ON) ;								// X only Servo ON
		SrvCon(Y_DIR, OFF) ;
	} else if (UcCmdPar == 0x02) {							// Y Centering Only

		SrvCon(X_DIR, OFF) ;								// Y only Servo ON
		SrvCon(Y_DIR, ON) ;
	}

	return(UcCmdSts) ;
}

void	OisEna(void) //Ois On
{
	// Servo ON
	GyrCon(ON) ;

	return;
}

void	OisOff(void) //Ois Off
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	GyrCon(OFF) ;
	RamWrite16(client, lxggf, 0x0000) ;			// 0x1308
	RamWrite16(client, lyggf, 0x0000) ;			// 0x1348

	return;
}

void	S2cPro(unsigned char uc_mode)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	RamWrite32(client, gxh1c, 0x3F7FFD00) ;										// 0x1814
	RamWrite32(client, gyh1c, 0x3F7FFD00) ;										// 0x1914

	if (uc_mode == 1) {														//Capture Mode
		// HPF Through Setting
//		RegWrite8(client, GSHTON, 0x01) ;												// 0x0104
		RegWrite8(client, G2NDCEFON0, 0x03) ;											// 0x0106	I Filter Control
	} else {																	//Video Mode
		// HPF Setting
//		RegWrite8(client, GSHTON, 0x00) ;												// 0x0104
		RegWrite8(client, G2NDCEFON0, 0x00) ;											// 0x0106
	}

	return;
}

//********************************************************************************
// Function Name 	: StbOnn
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2010.09.15 Y.Shigeoka
//********************************************************************************
void StbOnn(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char	UcRegValx,UcRegValy;					// Registor value

	RegRead8(client, LXEQEN, &UcRegValx) ;				/* 0x0084 */
	RegRead8(client, LYEQEN, &UcRegValy) ;				/* 0x008E */
	if (((UcRegValx & 0x80) != 0x80) && ((UcRegValy & 0x80) != 0x80))
	{
		RegWrite8(client, SSSEN,  0x88) ;				// 0x0097	Smooth Start enable

		SrvCon(X_DIR, ON) ;
		SrvCon(Y_DIR, ON) ;
	}
	else
	{
		SrvCon(X_DIR, ON) ;
		SrvCon(Y_DIR, ON) ;
	}

	return;
}

void	SrvCon(unsigned char	UcDirSel, unsigned char	UcSwcCon)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	if (UcSwcCon) {
		if (!UcDirSel) {									// X Direction
			RegWrite8(client, LXEQEN, 0xC5) ;				// 0x0084	LXSW = ON
		} else {											// Y Direction
			RegWrite8(client, LYEQEN, 0xC5) ;				// 0x008E	LYSW = ON
		}
	} else {
		if (!UcDirSel) {									// X Direction
			RegWrite8(client, LXEQEN, 0x45) ;				// 0x0084	LXSW = OFF
			RamWrite16(client, LXDODAT, 0x0000) ;			// 0x115A
		} else {											// Y Direction
			RegWrite8(client, LYEQEN, 0x45) ;				// 0x008E	LYSW = OFF
			RamWrite16(client, LYDODAT, 0x0000) ;			// 0x119A
		}
	}

	return;
}

void	GyrCon(unsigned char	UcGyrCon)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	// Return HPF Setting
	RegWrite8(client, GSHTON, 0x00) ;												// 0x0104

	if (UcGyrCon) {														// Gyro ON
		RamWrite16(client, lxggf, 0x7fff) ;										// 0x1308
		RamWrite16(client, lyggf, 0x7fff) ;										// 0x1348
		RamWrite32(client, gxi1b_1, 0xBF800000) ;								// Down
		RamWrite32(client, gyi1b_1, 0xBF800000) ;								// Down

//		RegWrite8(client, GPANFILMOD, 0x00) ;										// 0x0167
//		ClrGyr(0x02, 0x02) ;
//		ClrGyr(0x04, 0x02) ;
//		RegWrite8(client, GPANFILMOD, 0x01) ;										// 0x0167
		RamWrite32(client, gxi1b_1, 0xB3E6A3C6) ;								// Down
		RamWrite32(client, gyi1b_1, 0xB3E6A3C6) ;								// Down
		RegWrite8(client, GPANSTTFRCE, 0x00) ;									// 0x010A
		AutoGainControlSw(ON) ;											/* Auto Gain Control Mode ON */
	} else {																// Gyro OFF
		/* Gain3 Register */
		AutoGainControlSw(OFF) ;											/* Auto Gain Control Mode OFF */
	}

	//S2cPro(ON) ; //Go to Cature Mode
	S2cPro(OFF) ; //Go to Video Mode

	return;
}

void	SetPanTiltMode(unsigned char UcPnTmod)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	switch (UcPnTmod) {
		case OFF :
			RegWrite8(client, GPANON, 0x00) ;			// 0x0109	X,Y Pan/Tilt Function OFF
			break ;
		case ON :
			RegWrite8(client, GPANON, 0x11) ;			// 0x0109	X,Y Pan/Tilt Function ON
			break ;
	}

	return;
}

void	VSModeConvert(unsigned char UcVstmod)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	if (UcVstmod) {								// Cam Mode Convert
		RamWrite32(client, gxl2a_2, 0x00000000) ;	// 0x1860	0
		RamWrite32(client, gxl2b_2, 0x3E06FD65) ;	// 0x1861	0.131826
		RamWrite32(client, gyl2a_2, 0x00000000) ;	// 0x1960	0
		RamWrite32(client, gyl2b_2, 0x3E06FD65) ;	// 0x1961	0.131826

		RamWrite32(client, gxl2c_2, 0xBB894028) ;	// 0x1862	-0.004188556	3F7FEC00
		RamWrite32(client, gyl2c_2, 0xBB894028) ;	// 0x1962	-0.004188556	3F7FEC00

		// Phase Transition Setting
		RegWrite8(client, GPANSTT31JUG0, 	0x00);		// 0x0142
		RegWrite8(client, GPANSTT31JUG1, 	0x00);		// 0x0143
		RegWrite8(client, GPANSTT41JUG0, 	0x07);		// 0x0144
		RegWrite8(client, GPANSTT41JUG1, 	0x00);		// 0x0145
		RegWrite8(client, GPANSTT13JUG0, 	0x00);		// 0x0148
		RegWrite8(client, GPANSTT13JUG1, 	0x07);		// 0x0149
		RegWrite8(client, GPANSTT43JUG0, 	0x00);		// 0x014C
		RegWrite8(client, GPANSTT43JUG1, 	0x07);		// 0x014D
		RegWrite8(client, GPANSTT34JUG0, 	0x01);		// 0x014E
		RegWrite8(client, GPANSTT34JUG1, 	0x00);		// 0x014F

		RegWrite8(client, GPANSTTXXXTH,	0xF0);		// 0x015D

		// I Filter X							// 1s
		RamWrite32(client, gxi1a_1, 0x38A8A554) ;	// 0.3Hz
		RamWrite32(client, gxi1b_1, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gxi1c_1, 0x33E6A3C6) ;	// Up
		RamWrite32(client, gxi1a_b, 0x38A8A554) ;	// 0.3Hz
		RamWrite32(client, gxi1b_b, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gxi1c_b, 0x33E6A3C6) ;	// Up
		RamWrite32(client, gxi1a_c, 0x3AAF73A1) ;	// 5Hz
		RamWrite32(client, gxi1b_c, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gxi1c_c, 0x3F800000) ;	// Up

		// I Filter Y
		RamWrite32(client, gyi1a_1, 0x38A8A554) ;	// 0.3Hz
		RamWrite32(client, gyi1b_1, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gyi1c_1, 0x33E6A3C6) ;	// Up
		RamWrite32(client, gyi1a_b, 0x38A8A554) ;	// 0.3Hz
		RamWrite32(client, gyi1b_b, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gyi1c_b, 0x33E6A3C6) ;	// Up
		RamWrite32(client, gyi1a_c, 0x3AAF73A1) ;	// 5Hz
		RamWrite32(client, gyi1b_c, 0xB3E6A3C6) ;	// Down
		RamWrite32(client, gyi1c_c, 0x3F800000) ;	// Up
	} else {										// Still Mode Convert
		RamWrite32(client, gxl2a_2, 0x00000000) ;	// 0x1860	0
		RamWrite32(client, gxl2b_2, 0x3D829952) ;	// 0x1861	0.063769
		RamWrite32(client, gyl2a_2, 0x00000000) ;	// 0x1960	0
		RamWrite32(client, gyl2b_2, 0x3D829952) ;	// 0x1961	0.063769

		RamWrite32(client, gxl2c_2, 0xBA9CD414) ;	// 0x1862	-0.001196506	3F7FF800
		RamWrite32(client, gyl2c_2, 0xBA9CD414) ;	// 0x1962	-0.001196506	3F7FF800

		// Phase Transition Setting
		RegWrite8(client, GPANSTT31JUG0, 	0x01);		// 0x0142
		RegWrite8(client, GPANSTT31JUG1, 	0x00);		// 0x0143
		RegWrite8(client, GPANSTT41JUG0, 	0x00);		// 0x0144
		RegWrite8(client, GPANSTT41JUG1, 	0x00);		// 0x0145
		RegWrite8(client, GPANSTT13JUG0, 	0x00);		// 0x0148
		RegWrite8(client, GPANSTT13JUG1, 	0x07);		// 0x0149
		RegWrite8(client, GPANSTT43JUG0, 	0x00);		// 0x014C
		RegWrite8(client, GPANSTT43JUG1, 	0x00);		// 0x014D
		RegWrite8(client, GPANSTT34JUG0, 	0x00);		// 0x014E
		RegWrite8(client, GPANSTT34JUG1, 	0x00);		// 0x014F
		RegWrite8(client, GPANSTTXXXTH,	0x0F);		// 0x015D

		// I Filter X							// 1s
		RamWrite32(client, gxi1a_1, 0x38A8A554) ;		// 0.3Hz
		RamWrite32(client, gxi1b_1, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gxi1c_1, 0x33E6A3C6) ;		// Up
		RamWrite32(client, gxi1a_b, 0x3AAF73A1) ;		// 5Hz
		RamWrite32(client, gxi1b_b, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gxi1c_b, 0x3F800000) ;		// Up
		RamWrite32(client, gxi1a_c, 0x38A8A554) ;		// 0.3Hz
		RamWrite32(client, gxi1b_c, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gxi1c_c, 0x33E6A3C6) ;		// Up

		// I Filter Y
		RamWrite32(client, gyi1a_1, 0x38A8A554) ;		// 0.3Hz
		RamWrite32(client, gyi1b_1, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gyi1c_1, 0x33E6A3C6) ;		// Up
		RamWrite32(client, gyi1a_b, 0x3AAF73A1) ;		// 5Hz
		RamWrite32(client, gyi1b_b, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gyi1c_b, 0x3F800000) ;		// Up
		RamWrite32(client, gyi1a_c, 0x38A8A554) ;		// 0.3Hz
		RamWrite32(client, gyi1b_c, 0xB3E6A3C6) ;		// Down
		RamWrite32(client, gyi1c_c, 0x33E6A3C6) ;		// Up
	}
	return;
}

void WitTim(unsigned short mWitTim)
{
   msleep(mWitTim);
}

//==============================================================================
// OisCmd.c Code END (2012. 11. 20)
//==============================================================================

#define		HALL_ADJ		0
#define		LOOPGAIN		1
#define		THROUGH			2
#define		NOISE			3

//********************************************************************************
// Function Name 	: TneGvc
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Tunes the Gyro VC offset
// History			: First edition 						2009.07.31  Y.Tashita
//********************************************************************************
#define	INITVAL		0x0000

int BsyWit(unsigned short UsTrgAdr, unsigned char UcTrgDat)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char UcFlgVal;
	unsigned char UcCntPla;

	RegWrite8(client,UsTrgAdr, UcTrgDat);

	UcFlgVal = 1;
	UcCntPla = 0;

	do {
		RegRead8(client,FLGM, &UcFlgVal);
		UcFlgVal &= 0x40;
		UcCntPla ++ ;
	} while (UcFlgVal && (UcCntPla < BSYWIT_POLLING_LIMIT_A));
	if (UcCntPla == BSYWIT_POLLING_LIMIT_A) { return OIS_FW_POLLING_FAIL; }

	return OIS_FW_POLLING_PASS;
}



short	GenMes(unsigned short	UsRamAdd, unsigned char	UcMesMod)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	short	SsMesRlt = 0;

	RegWrite8(client, MS1INADD, (unsigned char)(UsRamAdd & 0x00ff)) ;	// 0x00C2	Input Signal Select

	if (!UcMesMod) {
		RegWrite8(client, MSMPLNSH, 0x03) ;								// 0x00CB
		RegWrite8(client, MSMPLNSL, 0xFF) ;								// 0x00CA	1024 Times Measure
		BsyWit(MSMA, 0x01) ;											// 0x00C9		Average Measure
	} else {
		RegWrite8(client, MSMPLNSH, 0x00) ;								// 0x00CB
		RegWrite8(client, MSMPLNSL, 0x03) ;								// 0x00CA	4 Cycle Measure
		BsyWit(MSMA, 0x22) ;											// 0x00C9		Average Measure
	}

	RegWrite8(client, MSMA, 0x00) ;										// 0x00C9		Measure Stop

	RamRead16(client, MSAV1, (unsigned short *)&SsMesRlt) ;				// 0x1205

	return(SsMesRlt) ;
}

void	MesFil(unsigned char	UcMesMod)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

	if (!UcMesMod) {								// Hall Bias&Offset Adjust
		// Measure Filter1 Setting
		RamWrite16(client, ms1aa, 0x0285) ;		// 0x13AF	LPF150Hz
		RamWrite16(client, ms1ab, 0x0285) ;		// 0x13B0
		RamWrite16(client, ms1ac, 0x7AF5) ;		// 0x13B1
		RamWrite16(client, ms1ad, 0x0000) ;		// 0x13B2
		RamWrite16(client, ms1ae, 0x0000) ;		// 0x13B3
		RamWrite16(client, ms1ba, 0x7FFF) ;		// 0x13B4	Through
		RamWrite16(client, ms1bb, 0x0000) ;		// 0x13B5
		RamWrite16(client, ms1bc, 0x0000) ;		// 0x13B6
		RamWrite16(client, ms1bd, 0x0000) ;		// 0x13B7
		RamWrite16(client, ms1be, 0x0000) ;		// 0x13B8

		RegWrite8(client, MSF1SOF, 0x00) ;		// 0x00C1

		// Measure Filter2 Setting
		RamWrite16(client, ms2aa, 0x0285) ;		// 0x13B9	LPF150Hz
		RamWrite16(client, ms2ab, 0x0285) ;		// 0x13BA
		RamWrite16(client, ms2ac, 0x7AF5) ;		// 0x13BB
		RamWrite16(client, ms2ad, 0x0000) ;		// 0x13BC
		RamWrite16(client, ms2ae, 0x0000) ;		// 0x13BD
		RamWrite16(client, ms2ba, 0x7FFF) ;		// 0x13BE	Through
		RamWrite16(client, ms2bb, 0x0000) ;		// 0x13BF
		RamWrite16(client, ms2bc, 0x0000) ;		// 0x13C0
		RamWrite16(client, ms2bd, 0x0000) ;		// 0x13C1
		RamWrite16(client, ms2be, 0x0000) ;		// 0x13C2

		RegWrite8(client, MSF2SOF, 0x00) ;		// 0x00C5

	} else if (UcMesMod == LOOPGAIN) {				// Loop Gain Adjust
		// Measure Filter1 Setting
		RamWrite16(client, ms1aa, 0x0F21) ;		// 0x13AF	LPF1000Hz
		RamWrite16(client, ms1ab, 0x0F21) ;		// 0x13B0
		RamWrite16(client, ms1ac, 0x61BD) ;		// 0x13B1
		RamWrite16(client, ms1ad, 0x0000) ;		// 0x13B2
		RamWrite16(client, ms1ae, 0x0000) ;		// 0x13B3
		RamWrite16(client, ms1ba, 0x7F7D) ;		// 0x13B4	HPF30Hz
		RamWrite16(client, ms1bb, 0x8083) ;		// 0x13B5
		RamWrite16(client, ms1bc, 0x7EF9) ;		// 0x13B6
		RamWrite16(client, ms1bd, 0x0000) ;		// 0x13B7
		RamWrite16(client, ms1be, 0x0000) ;		// 0x13B8

		RegWrite8(client, MSF1SOF, 0x00) ;		// 0x00C1

		// Measure Filter2 Setting
		RamWrite16(client, ms2aa, 0x0F21) ;		// 0x13B9	LPF1000Hz
		RamWrite16(client, ms2ab, 0x0F21) ;		// 0x13BA
		RamWrite16(client, ms2ac, 0x61BD) ;		// 0x13BB
		RamWrite16(client, ms2ad, 0x0000) ;		// 0x13BC
		RamWrite16(client, ms2ae, 0x0000) ;		// 0x13BD
		RamWrite16(client, ms2ba, 0x7F7D) ;		// 0x13BE	HPF30Hz
		RamWrite16(client, ms2bb, 0x8083) ;		// 0x13BF
		RamWrite16(client, ms2bc, 0x7EF9) ;		// 0x13C0
		RamWrite16(client, ms2bd, 0x0000) ;		// 0x13C1
		RamWrite16(client, ms2be, 0x0000) ;		// 0x13C2

		RegWrite8(client, MSF2SOF, 0x00) ;		// 0x00C5

	} else if (UcMesMod == THROUGH) {				// for Through
		// Measure Filter1 Setting
		RamWrite16(client, ms1aa, 0x7FFF) ;		// 0x13AF	Through
		RamWrite16(client, ms1ab, 0x0000) ;		// 0x13B0
		RamWrite16(client, ms1ac, 0x0000) ;		// 0x13B1
		RamWrite16(client, ms1ad, 0x0000) ;		// 0x13B2
		RamWrite16(client, ms1ae, 0x0000) ;		// 0x13B3
		RamWrite16(client, ms1ba, 0x7FFF) ;		// 0x13B4	Through
		RamWrite16(client, ms1bb, 0x0000) ;		// 0x13B5
		RamWrite16(client, ms1bc, 0x0000) ;		// 0x13B6
		RamWrite16(client, ms1bd, 0x0000) ;		// 0x13B7
		RamWrite16(client, ms1be, 0x0000) ;		// 0x13B8

		RegWrite8(client, MSF1SOF, 0x00) ;		// 0x00C1

		// Measure Filter2 Setting
		RamWrite16(client, ms2aa, 0x7FFF) ;		// 0x13B9	Through
		RamWrite16(client, ms2ab, 0x0000) ;		// 0x13BA
		RamWrite16(client, ms2ac, 0x0000) ;		// 0x13BB
		RamWrite16(client, ms2ad, 0x0000) ;		// 0x13BC
		RamWrite16(client, ms2ae, 0x0000) ;		// 0x13BD
		RamWrite16(client, ms2ba, 0x7FFF) ;		// 0x13BE	Through
		RamWrite16(client, ms2bb, 0x0000) ;		// 0x13BF
		RamWrite16(client, ms2bc, 0x0000) ;		// 0x13C0
		RamWrite16(client, ms2bd, 0x0000) ;		// 0x13C1
		RamWrite16(client, ms2be, 0x0000) ;		// 0x13C2

		RegWrite8(client, MSF2SOF, 0x00) ;		// 0x00C5

	} else if (UcMesMod == NOISE) {				// SINE WAVE TEST for NOISE

		RamWrite16(client, ms1aa, 0x04F3) ;		// 0x13AF	LPF150Hz
		RamWrite16(client, ms1ab, 0x04F3) ;		// 0x13B0
		RamWrite16(client, ms1ac, 0x761B) ;		// 0x13B1
		RamWrite16(client, ms1ad, 0x0000) ;		// 0x13B2
		RamWrite16(client, ms1ae, 0x0000) ;		// 0x13B3
		RamWrite16(client, ms1ba, 0x04F3) ;		// 0x13B4	LPF150Hz
		RamWrite16(client, ms1bb, 0x04F3) ;		// 0x13B5
		RamWrite16(client, ms1bc, 0x761B) ;		// 0x13B6
		RamWrite16(client, ms1bd, 0x0000) ;		// 0x13B7
		RamWrite16(client, ms1be, 0x0000) ;		// 0x13B8

		RegWrite8(client, MSF1SOF, 0x00) ;		// 0x00C1

		RamWrite16(client, ms2aa, 0x04F3) ;		// 0x13B9	LPF150Hz
		RamWrite16(client, ms2ab, 0x04F3) ;		// 0x13BA
		RamWrite16(client, ms2ac, 0x761B) ;		// 0x13BB
		RamWrite16(client, ms2ad, 0x0000) ;		// 0x13BC
		RamWrite16(client, ms2ae, 0x0000) ;		// 0x13BD
		RamWrite16(client, ms2ba, 0x04F3) ;		// 0x13BE	LPF150Hz
		RamWrite16(client, ms2bb, 0x04F3) ;		// 0x13BF
		RamWrite16(client, ms2bc, 0x761B) ;		// 0x13C0
		RamWrite16(client, ms2bd, 0x0000) ;		// 0x13C1
		RamWrite16(client, ms2be, 0x0000) ;		// 0x13C2

		RegWrite8(client, MSF2SOF, 0x00) ;		// 0x00C5
	}
}

int16_t lgit_convert_float32(uint32_t float32, uint8_t len);


unsigned char	TneGvc(uint8_t flag)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char	UcGvcSts ;
	unsigned short UsGxoVal, UsGyoVal;
	int16_t oldx, oldy;

	UcGvcSts	= EXE_END ;

	{
		uint32_t gyrox, gyroy = 0;
		RamRead32(client,GXADZ, &gyrox);
		RamRead32(client,GYADZ, &gyroy);
		oldx = abs(lgit_convert_float32(gyrox,18));
		oldy = abs(lgit_convert_float32(gyroy,18));
		printk("%s old gxadz %x, %x \n",__func__,oldx, oldy);

		if (oldx < 262*2 && oldy < 262*2)
		{
			printk("%s no need to adjust \n",__func__);
			return UcGvcSts;
		}
	}


	// for InvenSense Digital Gyro	ex.IDG-2000
	// A/D Offset Clear
	RamWrite16(client, ADGXOFF, 0x0000) ;	// 0x1108
	RamWrite16(client, ADGYOFF, 0x0000) ;	// 0x110B
	RegWrite8(client, IZAH,	(unsigned char)(INITVAL >> 8)) ;	// 0x03A0		Set Offset High byte
	RegWrite8(client, IZAL,	(unsigned char)INITVAL) ;			// 0x03A1		Set Offset Low byte
	RegWrite8(client, IZBH,	(unsigned char)(INITVAL >> 8)) ;	// 0x03A2		Set Offset High byte
	RegWrite8(client, IZBL,	(unsigned char)INITVAL) ;			// 0x03A3		Set Offset Low byte

	RegWrite8(client, GDLYMON10, 0xF5) ;		// 0x0184 <- GXADZ(0x19F5)
	RegWrite8(client, GDLYMON11, 0x01) ;		// 0x0185 <- GXADZ(0x19F5)
	RegWrite8(client, GDLYMON20, 0xF5) ;		// 0x0186 <- GYADZ(0x18F5)
	RegWrite8(client, GDLYMON21, 0x00) ;		// 0x0187 <- GYADZ(0x18F5)
	MesFil(THROUGH) ;
	RegWrite8(client, MSF1EN, 0x01) ;			// 0x00C0		Measure Filter1 Equalizer ON
	//////////
	// X
	//////////
	if (BsyWit(DLYCLR2, 0x80) != OIS_FW_POLLING_PASS) {return OIS_FW_POLLING_FAIL ;}			// 0x00EF	Measure Filter1 Delay RAM Clear
	UsGxoVal = (unsigned short)GenMes(GYRMON1, 0);		// GYRMON1(0x1110) <- GXADZ(0x19F5)
	RegWrite8(client, IZAH, (unsigned char)(UsGxoVal >> 8)) ;	// 0x03A0		Set Offset High byte
	RegWrite8(client, IZAL, (unsigned char)(UsGxoVal)) ;		// 0x03A1		Set Offset Low byte
	//////////
	// Y
	//////////
	if (BsyWit(DLYCLR2, 0x80) != OIS_FW_POLLING_PASS) {return OIS_FW_POLLING_FAIL;}			// 0x00EF	Measure Filter1 Delay RAM Clear
	UsGyoVal = (unsigned short)GenMes(GYRMON2, 0);		// GYRMON2(0x1111) <- GYADZ(0x18F5)

	RegWrite8(client, IZBH, (unsigned char)(UsGyoVal >> 8)) ;	// 0x03A2		Set Offset High byte
	RegWrite8(client, IZBL, (unsigned char)(UsGyoVal)) ;		// 0x03A3		Set Offset Low byte


	RegWrite8(client, MSF1EN, 0x00) ;			// 0x00C0		Measure Filter1 Equalizer OFF

	if (((short)UsGxoVal > (short)GYROFF_HIGH) ||
		((short)UsGxoVal < (short)GYROFF_LOW)) {
		UcGvcSts	= EXE_GYRADJ ;
	}

	if (((short)UsGyoVal > (short)GYROFF_HIGH) ||
		((short)UsGyoVal < (short)GYROFF_LOW)) {
		UcGvcSts	= EXE_GYRADJ ;
	}

	{
	uint32_t gyrox, gyroy = 0;
	int16_t newx, newy;

	RamRead32(client,GXADZ, &gyrox);
	RamRead32(client,GYADZ, &gyroy);
	newx = abs(lgit_convert_float32(gyrox,18));
	newy = abs(lgit_convert_float32(gyroy,18));
	printk("%s new gxadz %x, %x \n",__func__,newx, newy);

	if (newx > 262*2 || newy > 262*2 || newx > oldx || newy > oldy)
	{ UcGvcSts = EXE_GYRADJ; }
	}

	if (UcGvcSts != EXE_GYRADJ)
	{
		printk("%s : gyro original : %x, %x \n",__func__, StCalDat.StGvcOff.UsGxoVal, StCalDat.StGvcOff.UsGyoVal);
		printk("%s : gyro result : %x, %x \n",__func__, UsGxoVal, UsGyoVal);
		if (flag == OIS_VER_CALIBRATION)
		{
			E2PRegWrite16(client,GYRO_AD_OFFSET_X, 0xFFFF & UsGxoVal);//ois_i2c_e2p_write(GYRO_AD_OFFSET_X, 0xFFFF & UsGxoVal, 2);
			msleep(10);
			E2PRegWrite16(client,GYRO_AD_OFFSET_Y, 0xFFFF & UsGyoVal);//ois_i2c_e2p_write(GYRO_AD_OFFSET_Y, 0xFFFF & UsGyoVal, 2);
		}
	}

	return(UcGvcSts) ;
}
//---------------------------------------------------------------------------------//
// 2013-08-10 added for gain-loop test
//---------------------------------------------------------------------------------//
#define     __MEASURE_LOOPGAIN      0x00

//==============================================================================
//  Function    :   SetSineWave()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//  outputs     :   void
//  explanation :   Initializes sine wave settings:
//                      Sine Table, Amplitue, Offset, Frequency
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void SetSineWave(unsigned char UcJikuSel , unsigned char UcMeasMode)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned char   UcSWFC1[]   = { 0x7D/*139Hz*/ , 0x0F/*15Hz*/ } ,            // { Loop Gain setting , Bias/Offset setting}
                    UcSWFC2[]   = { 0x00/*150Hz*/ , 0x00/*20Hz*/ } ;            // { Loop Gain setting , Bias/Offset setting}

    unsigned char   UcSwSel[2][2] = { { 0x80 , 0x40 } ,                         // Loop gain setting
                                      { 0x00 , 0x00 }                           // Bias/Offset Setting
                                    };

    UcMeasMode &= 0x01;
    UcJikuSel  &= 0x01;

    /* Manually Set Offset */
    RamWrite16(client, WAVXO , 0x0000);                                                // 0x11D5
    RamWrite16(client, WAVYO , 0x0000);                                                // 0x11D6

    /* Manually Set Amplitude */
    //RamWrite16(client, wavxg , 0x7FFF);                                                // 0x13C3
    //RamWrite16(client, wavyg , 0x7FFF);                                                // 0x13C4
	RamWrite16(client, wavxg , 0x3000);                                                // 0x13C3
    RamWrite16(client, wavyg , 0x3000);                                                // 0x13C4

    /* Set Frequency */
    //****************************************************
    //  Žü”g” = (fs/96)*(SWB+1)/(SWA+1)*1/(2^SWC)  [ Hz ]
    //****************************************************
    RegWrite8(client, SWFC1 , UcSWFC1[UcMeasMode]);                                   // 0x00DD    [ SWB(7:4) ][ SWA(3:0) ]
    RegWrite8(client, SWFC2 , UcSWFC2[UcMeasMode]);                                   // 0x00DE    [ SWCIR | SWDIR | - | - ][ SWCOM | SWFC(2:0) ]
    RegWrite8(client, SWFC3 , 0x00);                                                  // 0x00DF    [ SWINIP | SWBGNP(6:0) ]
    RegWrite8(client, SWFC4 , 0x00);                                                  // 0x00E0    [ SWFINP | SWENDP(6:0) ]
    RegWrite8(client, SWFC5 , 0x00);                                                  // 0x00E1    [ SWFT(7:0) ]

    /* Set Sine Wave Input RAM */
    RegWrite8(client, SWSEL , UcSwSel[UcMeasMode][UcJikuSel]);                        // 0x00E2    [ SINX | SINY | SING | SIN0END ][ SINGDPX1 | SINGDPX2 | SINGDPY1 | SINGDPY2 ]

    /* Clear Optional Sine wave input address */
    if (!UcMeasMode)       // Loop Gain mode
    {
        RegWrite8(client, SINXADD , 0x00);                                            // 0x00E3  [ SINXADD(7:0) ]
        RegWrite8(client, SINYADD , 0x00);                                            // 0x00E4  [ SINYADD(7:0) ]
    }
    else if (!UcJikuSel)   // Bias/Offset mode X-Axis
    {
        RegWrite8(client, SINXADD , (unsigned char)LXDODAT);                          // 0x00E3  [ SINXADD(7:0) ]
        RegWrite8(client, SINYADD , 0x00);                                            // 0x00E4  [ SINYADD(7:0) ]
    }
    else                    // Bias/Offset mode Y-Axis
    {
        RegWrite8(client, SINXADD , 0x00);                                            // 0x00E3  [ SINXADD(7:0) ]
        RegWrite8(client, SINYADD , (unsigned char)LYDODAT);                          // 0x00E4  [ SINYADD(7:0) ]
    }
}

//==============================================================================
//  Function    :   StartSineWave()
//  inputs      :   none
//  outputs     :   void
//  explanation :   Starts sine wave
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StartSineWave(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
    /* Start Sine Wave */
    RegWrite8(client, SWEN , 0x80);                                                   // 0x00DB     [ SINON | SINRST | - | - ][ SININISET | - | - | - ]
}

//==============================================================================
//  Function    :   StopSineWave()
//  inputs      :   void
//  outputs     :   void
//  explanation :   Stops sine wave
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StopSineWave(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;

    /* Set Sine Wave Input RAM */
    RegWrite8(client, SWSEL   , 0x00);                                                // 0x00E2    [ SINX | SINY | SING | SIN0END ][ SINGDPX1 | SINGDPX2 | SINGDPY1 | SINGDPY2 ]
    RegWrite8(client, SINXADD , 0x00);                                                // 0x00E3  [ SINXADD(7:0) ]
    RegWrite8(client, SINYADD , 0x00);                                                // 0x00E4  [ SINYADD(7:0) ]

    /* Stop Sine Wave */
    RegWrite8(client, SWEN  , 0x00);                                                  // 0x00DB     [ SINON | SINRST | - | - ][ SININISET | - | - | - ]
}


//==============================================================================
//  Function    :   SetMeaseFil_LoopGain()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//                  UcFilSel
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void SetMeasFil(unsigned char UcJikuSel , unsigned char UcMeasMode , unsigned char UcFilSel)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
    unsigned short  UsIn1Add[2][2] = { { LXC1   , LYC1   } ,                    // Loop Gain Setting
                                       { ADHXI0 , ADHYI0 }                      // Bias/Offset Setting
                                     } ,
                    UsIn2Add[2][2] = { { LXC2   , LYC2   } ,                    // Loop Gain Setting
                                       { 0x0000 , 0x0000 }                      // Bias/Offset Setting
                                     } ;

    /* Set Limits on Input Parameters */
    UcJikuSel  &= 0x01;
    UcMeasMode &= 0x01;
    if (UcFilSel > NOISE) UcFilSel = THROUGH;

	MesFil(UcFilSel) ;					/* Set Measure filter */

    RegWrite8(client, MS1INADD , (unsigned char)UsIn1Add[UcMeasMode][UcJikuSel]);     // 0x00C2
    RegWrite8(client, MS1OUTADD, 0x00);                                               // 0x00C3


    RegWrite8(client, MS2INADD , (unsigned char)UsIn2Add[UcMeasMode][UcJikuSel]);     // 0x00C6
    RegWrite8(client, MS2OUTADD, 0x00);                                               // 0x00C7

    /* Set Measure Filter Down Sampling */
    //****************************************************
    //  Measure Filter Down Sampling = Fs/(MSFDS+1)
    //****************************************************
    RegWrite8(client, MSFDS , 0x00);                                                  // 0x00C8
}

//==============================================================================
//  Function    :   StartMeasFil()
//  inputs      :   void
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StartMeasFil(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
    /* Enable Measure Filters */
    RegWrite8(client, MSF1EN , 0x01);                                                 // 0x00C0       [ MSF1SW | - | - | - ][ - | - | - | MSF1EN ]
    RegWrite8(client, MSF2EN , 0x01);                                                 // 0x00C4       [ MSF2SW | - | - | - ][ - | - | - | MSF2EN ]
}

//==============================================================================
//  Function    :   StopMeasFil()
//  inputs      :   void
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StopMeasFil(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
    /* Enable Measure Filters */
    RegWrite8(client, MSF1EN , 0x00);                                                 // 0x00C0       [ MSF1SW | - | - | - ][ - | - | - | MSF1EN ]
    RegWrite8(client, MSF2EN , 0x00);                                                 // 0x00C4       [ MSF2SW | - | - | - ][ - | - | - | MSF2EN ]
}

//********************************************************************************
// Function Name 	: IntegralMes
// Retun Value		: void
// Argment Value	: Sine wave Frequency Index
// Explanation		: Integration Value Measure Setting
// History			: First edition 						2013.07.31 Y.Tashita
//********************************************************************************
void	IntegralMes(unsigned char UcSinFrq)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	RegWrite8(client, MSMA, 0x03) ;							// 0x00C9	Cycle Wait, Sin Wave Measure

	if (UcSinFrq == 0){
		WitTim(200) ;
	}else if (UcSinFrq == 1){
		WitTim(100) ;
	}else{
		WitTim(60) ;
	}

	RegWrite8(client, MSMA, 0x00) ;							// 0x00C9	Measure End

}

//********************************************************************************
// Function Name 	: GetIntegral
// Retun Value		: Measure Result
// Argment Value	: LXG1/LXG2 Select
// Explanation		: Integration Value Measure Result Read
// History			: First edition 						2013.07.31 Y.Tashita
//********************************************************************************
unsigned short	GetIntegral(unsigned char	UcDirSel)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned short	UsAmpVal ;

	if (!UcDirSel) {
		RamRead16(client, MSCAP, &UsAmpVal) ;				// 0x120A
	} else {
		RamRead16(client, MSCAP2, &UsAmpVal) ;				// 0x12AC
	}

	return(UsAmpVal) ;
}

//********************************************************************************
// Function Name 	: CalAmpRatio
// Retun Value		: Amplitude Ratio * 100
// Argment Value	: Ratio Calculation Source Value
// Explanation		: Sine Wave Amplitude Ratio Calculation Function
// History			: First edition 						2013.08.01 Y.Tashita
//********************************************************************************
short	CalAmpRatio(unsigned short	UsSource1, unsigned short	UsSource2)
{
	short	SsAmpRatio ;

	SsAmpRatio	= (short)(((unsigned long)UsSource1 * 100) / UsSource2) ;

	return(SsAmpRatio) ;
}

//==============================================================================
//  Function    :   SrvGainMes()
//  inputs      :   UcJikuSel   0: X-Axis, 1: Y-Axis
//  outputs     :   OK/NG Judge Value
//  explanation :
//  revisions   :   First Edition                          2013.07.31 Y.Tashita
//==============================================================================
unsigned char	 SrvGainMes(unsigned char	UcJikuSel)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	unsigned short  UsSineAdd[]	= { lxxg   , lyxg   } ;
	unsigned short  UsSineGan[]	= { 0x2AF5 , 0x2AF5 } ;
	unsigned char	UcSWFC1[]	= { 0x0B, 0x7D, 0x42 } ;	// 20Hz, 139Hz, 406Hz
	unsigned char	UcSWFC2[]	= { 0x00, 0x00, 0x00 } ;	// SWC = 0, 0, 0
	short			SsRatioSh[]	= { 345, 141, 50 } ;
	unsigned char	UcJudgeSts1, UcJudgeSts2, UcJudgeSts3 ;
	unsigned char	UcJudgeSts = SUCCESS ;
	unsigned char	UcSinFrq ;
	unsigned short	UsMSCAP1, UsMSCAP2 ;
	short			SsCalRatio ;

	UcJikuSel	&= 0x01 ;
	UcJudgeSts1	= 0x00 ;
	UcJudgeSts2	= 0x00 ;
	UcJudgeSts3	= 0x00 ;

	/* set sine wave */
	SetSineWave(UcJikuSel, __MEASURE_LOOPGAIN) ;
	RamWrite16(client, wavxg , 0x7FFF);                                                // 0x13C3
    RamWrite16(client, wavyg , 0x7FFF);                                                // 0x13C4

	/* Set Servo Filter */
	RamWrite16(client, UsSineAdd[ UcJikuSel ], UsSineGan[ UcJikuSel ]) ;				// Set Sine Wave input amplitude

	/* set Measure Filter */
	SetMeasFil(UcJikuSel, __MEASURE_LOOPGAIN, THROUGH) ;

	/* Start Measure Filters */
	StartMeasFil() ;

	/* Start Sine Wave */
	StartSineWave() ;
	WitTim(100) ;
	for (UcSinFrq = 0 ; UcSinFrq < 3 ; UcSinFrq++)
	{
		RegWrite8(client, SWFC1, UcSWFC1[ UcSinFrq ]) ;								// 0x00DD	[ SWB(7:4) ][ SWA(3:0) ]
		RegWrite8(client, SWFC2, UcSWFC2[ UcSinFrq ]) ;								// 0x00DE

		RegWrite8(client, DLYCLR2, 0xC0) ;
		WitTim(50) ;
		IntegralMes(UcSinFrq) ;
		UsMSCAP1	= GetIntegral(0) ;
		UsMSCAP2	= GetIntegral(1) ;
		SsCalRatio	= CalAmpRatio(UsMSCAP1, UsMSCAP2) ;
		printk("%s, %d=> %x, %x, ratio=%x (%x) \n",
			__func__, UcSinFrq, UsMSCAP1, UsMSCAP2, SsCalRatio, SsRatioSh[ UcSinFrq ]);

		if (!UcSinFrq) {
			if (SsCalRatio < SsRatioSh[ UcSinFrq ]) {
				if (UcJikuSel == X_DIR){
					UcJudgeSts1	= EXE_XFRQ1 ;
				}else if (UcJikuSel == Y_DIR){
					UcJudgeSts1	= EXE_YFRQ1 ;
				}
			}
		} else if (UcSinFrq == 1) {
			SsCalRatio	&= 0x7FFF ;
			if (SsCalRatio > SsRatioSh[ UcSinFrq ]) {
				if (UcJikuSel == X_DIR){
					UcJudgeSts2	= EXE_XFRQ2 ;
				}else if (UcJikuSel == Y_DIR){
					UcJudgeSts2	= EXE_YFRQ2 ;
				}
			}
		} else {
			if (SsCalRatio > SsRatioSh[ UcSinFrq ]) {
				if (UcJikuSel == X_DIR){
					UcJudgeSts3	= EXE_XFRQ3 ;
				}else if (UcJikuSel == Y_DIR){
					UcJudgeSts3	= EXE_YFRQ3 ;
				}
			}
		}
	}

	/* Cut Sine input */
	RamWrite16(client, UsSineAdd[UcJikuSel] , 0x0000);                                 // Set Sine Wave input amplitude

	/* Stop Sine Wave */
	StopSineWave();

	/* Stop Measure Filter */
	StopMeasFil();

	UcJudgeSts	= UcJudgeSts1 | UcJudgeSts2 | UcJudgeSts3 ;

	return(UcJudgeSts) ;
}

//-------------------------------------------------------------------//

void EnsureWrite(uint16_t addr, int16_t data, int8_t trial)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	int i = 0;
	int16_t written;
	do
	{
		RamWrite16(client,addr, data & 0xFFFF);
		RamRead16(client,addr, &written);
	} while ((data !=  written) && (i < trial));
}

int32_t	lgit_ois_on(enum ois_ver_t ver)
{
	int32_t rc = OIS_SUCCESS;
	printk("%s, enter %s\n", __func__,LAST_UPDATE);
	switch(ver)
	{
		case OIS_VER_RELEASE:
		{
			msleep(20);
			rc = IniSet();
			if (rc < 0) { return rc;}
			msleep(5);
			RtnCen(0);
			msleep(100);

		}
		break;
		case OIS_VER_DEBUG:
		case OIS_VER_CALIBRATION:
		{
			int i=0;
			msleep(20);
			rc = IniSet();
			if (rc < 0) { return rc;}
			msleep(5);
			rc = OIS_INIT_GYRO_ADJ_FAIL;
			do
			{
				if (TneGvc(ver) == EXE_END)
				{
					rc = OIS_SUCCESS;
					break;
				}
			} while (i++ < 5);

			msleep(5);
			RtnCen(0);

			if ((SrvGainMes(X_DIR) | SrvGainMes(Y_DIR)) != 0)
			{
				rc |= OIS_INIT_SRV_GAIN_FAIL;
			}

			OisOff();

			EnsureWrite(HXSEPT1, 0x0000, 5);
			EnsureWrite(HYSEPT1, 0x0000, 5);

			msleep(50);
		}
		break;
	}
	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	if (StCalDat.UsVerDat != 0x0204)
	{
		printk("%s, old module %x \n", __func__,StCalDat.UsVerDat);
		rc |= OIS_INIT_OLD_MODULE;
	}

	printk("%s, exit \n", __func__);
	return rc;
}

int32_t	lgit_ois_off(void)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	int16_t i;
	int16_t hallx,hally;

	printk("%s\n", __func__);

	OisOff();
	msleep(1);

	RamWrite16(client, LXOLMT, 0x1200);
	msleep(2);
	RamRead16(client,HXTMP, &hallx);
	RamWrite16(client,HXSEPT1, -hallx);

	RamWrite16(client, LYOLMT, 0x1200);
	msleep(2);
	RamRead16(client,HYTMP, &hally);
	RamWrite16(client,HYSEPT1, -hally);

	for (i=0x1100; i>=0x0000;  i-=0x100)
	{
		RamWrite16(client, LXOLMT,	i);
		RamWrite16(client, LYOLMT,	i);
		msleep(2);
	}

	SrvCon(X_DIR, OFF) ;
	SrvCon(Y_DIR, OFF) ;

	printk("%s, exit\n", __func__);

	return OIS_SUCCESS;
}

int32_t lgit_ois_mode(enum ois_mode_t data)
{
	int cur_mode = lgit_ois_func_tbl.ois_cur_mode;
	printk("%s:%d\n", __func__,data);

	if (cur_mode == data)
	{
		return OIS_SUCCESS;
	}

	switch(cur_mode)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_CAPTURE :
		case OIS_MODE_VIDEO :
			OisOff();
			break;
		case OIS_MODE_CENTERING_ONLY :
			break;
		case OIS_MODE_CENTERING_OFF:
			RtnCen(0);
			break;
	}

	switch(data)
	{
		case OIS_MODE_PREVIEW_CAPTURE :
		case OIS_MODE_CAPTURE :
			VSModeConvert(OFF);
			OisEna() ;
			break;
		case OIS_MODE_VIDEO :
			VSModeConvert(ON);
			OisEna() ;
			break;
		case OIS_MODE_CENTERING_ONLY :
			break;
		case OIS_MODE_CENTERING_OFF:
			SrvCon(X_DIR, OFF) ;
    		SrvCon(Y_DIR, OFF) ;
			break;
	}

	lgit_ois_func_tbl.ois_cur_mode = data;

	return OIS_SUCCESS;
}

int16_t lgit_convert_float32(uint32_t float32, uint8_t len)
{
	uint8_t sgn = float32 >> 31;
	uint16_t exp = 0xFF & (float32 >> 23);
	uint32_t frc = (0x7FFFFF & (float32)) | 0x800000;

	if (exp > 127) {
		frc = frc << (exp-127);
	} else {
		frc = frc >> (127-exp);
	}

	frc = frc >> (24-len);
	if (frc > 0x007FFF) { frc = 0x7FFF; }
	if (sgn) { frc = (~frc) + 1; }

	return 0xFFFF & frc;
}

int16_t lgit_convert_int32(int32_t in)
{
	if (in > 32767) return 32767;
	if (in < -32767) return -32767;
	return 0xFFFF & in;
}

/*
	[Integration note]
	(1) gyro scale match (output unit -> dps*256)
	from gyro IC & driver IC spec sheet,
		gyro[dps] = gyro[int16] / 65.5
		gyro[int16] = gyro[float32]*0x7FFF

    ->	gyro[dps*256] = gyro[float32] * 0x7FFF * 4 * {256 / 262}
    (attention : lgit_convert_float32(gyro,18) returns gyro[flaot32]*0x7FFF*4)

	(2) hall scale match (output unit -> um*256)
	from measurement, pixel-code relation is averagely given by,
		hall[pixel] : hall[code] = 43 pixel : 8471

	 -> hall[um*256] = hall[code]*256*1.12[um/pixels]*43[pixel]/8471
					 = hall[code]*256/176
*/

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 176
#define STABLE_THRESHOLD  600 // 3.04[pixel]*1.12[um/pixel]*HALL_SCALE_FACTOR

int32_t lgit_ois_stat(struct odin_ois_info *ois_stat)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	uint32_t gyro = 0;
	int16_t hall = 0;
	int16_t target = 0;

	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "LGIT_ONSEMI");

	RamRead32(client,GXADZ, &gyro);
	RamRead16(client,LXGZF, &target);
	RamRead16(client,HXIN, &hall);

	ois_stat->gyro[0] =  lgit_convert_int32((int32_t)(lgit_convert_float32(gyro,18))*256/GYRO_SCALE_FACTOR);
	ois_stat->target[0] = lgit_convert_int32(-1*(((int32_t)target)*256/HALL_SCALE_FACTOR));
	ois_stat->hall[0] = lgit_convert_int32(((int32_t)hall)*256/HALL_SCALE_FACTOR);

	printk("%s gyrox %x, %x | targetx %x, %x | hallx  %x, %x \n",__func__,
		gyro, ois_stat->gyro[0], target, ois_stat->target[0], hall, ois_stat->hall[0]);

	RamRead32(client,GYADZ, &gyro);
	RamRead16(client,LYGZF, &target);
	RamRead16(client,HYIN, &hall);

	ois_stat->gyro[1] =  lgit_convert_int32((int32_t)(lgit_convert_float32(gyro,18))*256/GYRO_SCALE_FACTOR);
	ois_stat->target[1] = lgit_convert_int32(-1*(((int32_t)target)*256/HALL_SCALE_FACTOR));
	ois_stat->hall[1] = lgit_convert_int32(((int32_t)hall)*256/HALL_SCALE_FACTOR);

	printk("%s gyroy %x, %x | targety %x, %x | hally  %x, %x \n",__func__,
		gyro, ois_stat->gyro[1], target, ois_stat->target[1], hall, ois_stat->hall[1]);

	ois_stat->is_stable = 1;

	// if all value = 0, set is_stable = 0
	if (ois_stat->hall[0] == 0 && ois_stat->hall[1] == 0 && ois_stat->gyro[0] == 0 && ois_stat->gyro[1] == 0)
	{
		ois_stat->is_stable = 0;
	}

	// if target-hall difference is bigger than STABLE_THRESHOLD
	RamRead16(client,LXCFIN, &target);
	if (abs(target) > STABLE_THRESHOLD)
	{
		ois_stat->is_stable = 0;
	}

	RamRead16(client,LYCFIN, &hall);
	if (abs(hall) > STABLE_THRESHOLD)
	{
		ois_stat->is_stable = 0;
	}
	printk("%s stable %x, %x, %d", __func__, target,hall, ois_stat->is_stable);

	return OIS_SUCCESS;
}

#define LENS_MOVE_POLLING_LIMIT (10)
#define LENS_MOVE_THRESHOLD     (5) // um

int32_t lgit_ois_move_lens(int16_t target_x, int16_t target_y)
{
	struct i2c_client *client = lgit_ois_func_tbl.client;
	int32_t rc = OIS_SUCCESS;
	int16_t offset_x, offset_y;
	int16_t oldx, oldy;
	int16_t hallx1, hally1;
	int16_t hallx2, hally2;

	//1. convert um*256 -> code
    // hall[code] = hall[pixel*256]*176/256
	offset_x = -1 * target_x * HALL_SCALE_FACTOR / 256;
	offset_y = -1 * target_y * HALL_SCALE_FACTOR / 256;

    //2. read previous condition.
	RamRead16(client,HXSEPT1, &oldx);
	RamRead16(client,HYSEPT1, &oldy);

	printk("%s offset %d,%d -> %d, %d \n",__func__, oldx, oldy, offset_x, offset_y);

	RamRead16(client,HXIN, &hallx1);
	RamRead16(client,HYIN, &hally1);

	hallx1 -= oldx;
	hally1 -= oldy;

	printk("%s : hall %d, %d \n",__func__, hallx1, hally1);

	//3. perform move (ensure HXSEPT1 is set correctly)
	EnsureWrite(HXSEPT1, offset_x, 5);
	EnsureWrite(HYSEPT1, offset_y, 5);

	msleep(30);

	//4. read new lens position
	RamRead16(client,HXIN, &hallx2);
	RamRead16(client,HYIN, &hally2);

	hallx2 -= offset_x;
	hally2 -= offset_y;

	printk("%s : hall %d, %d \n",__func__, hallx2, hally2);

    //5. make decision.
	rc = OIS_FAIL;
	if (abs(hallx2+offset_x) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR ||
		abs(hally2+offset_y) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR)
	{
		rc = OIS_SUCCESS;
	}
#if 1
	else if
	(((abs(hallx1-hallx2) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR) && (oldx != offset_x))
	||((abs(hally1-hally2) < LENS_MOVE_THRESHOLD*HALL_SCALE_FACTOR) && (oldy != offset_y)))
	{
		rc = OIS_FAIL;
		printk("%s : fail \n",__func__);
	}
	else
	{
		rc = OIS_SUCCESS;
	}
#endif

	return rc;

}

void lgit_ois_init(struct i2c_client *client, struct odin_ois_ctrl *ois_ctrl)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;

	lgit_ois_func_tbl.ois_on = lgit_ois_on;
	lgit_ois_func_tbl.ois_off = lgit_ois_off;
	lgit_ois_func_tbl.ois_mode = lgit_ois_mode;
	lgit_ois_func_tbl.ois_stat = lgit_ois_stat;
	lgit_ois_func_tbl.ois_move_lens = lgit_ois_move_lens;
	lgit_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	lgit_ois_func_tbl.client = client;

	pdata->sensor_sid = client->addr;
	pdata->ois_sid = 0x48;
	pdata->eeprom_sid = 0xA0;

	//ois_ctrl->sid_ois = 0x48 >> 1;
	ois_ctrl->ois_func_tbl = &lgit_ois_func_tbl;
}
