/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "AudDrv_Afe.h"
#include "mt_soc_digital_type.h"


/**
* here define conenction table for input and output
*/
static const char mConnectionTable[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output]
	= {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 1, -1, -1},	/* I00 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, 1, 3, -1, -1},	/* I01 */
	{1, 1, 1, 1, 1, 1, -1, 1, -1, -1, -1, 1, -1, -1, -1, 1, 1, 1, -1},	/* I02 */
	{1, 1, 1, 1, 1, 1, -1, 1, -1, 1, -1, -1, -1, -1, -1, 1, 1, 1, -1},	/* I03 */
	{1, 1, 1, 1, 1, -1, 1, -1, 1, -1, 1, -1, -1, -1, -1, 1, 1, -1, 1},	/* I04 */
	{3, 3, 3, 3, 3, 1, -1, 0, -1, 0, -1, 0, -1, 1, 1, -1, -1, 0, -1},	/* I05 */
	{3, 3, 3, 3, 3, -1, 1, -1, 0, -1, 0, -1, 0, 1, 1, -1, -1, -1, 0},	/* I06 */
	{3, 3, 3, 3, 3, 1, -1, 0, -1, 0, -1, 0, -1, 1, 1, -1, -1, 0, -1},	/* I07 */
	{3, 3, 3, 3, 3, -1, 1, -1, 0, -1, 0, -1, 0, 1, 1, -1, -1, -1, 0},	/* I08 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1},	/* I09 */
	{3, -1, 3, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, -1},	/* I10 */
	{-1, 3, 3, -1, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1},	/* I11 */
	{3, -1, 3, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, -1},	/* I12 */
	{-1, 3, 3, -1, 3, -1, 0, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1},	/* I13 */
	{1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1},	/* I14 */
	{3, 3, 3, 3, 3, 3, -1, -1, -1, 1, -1, -1, -1, -1, -1, 3, 1, -1, -1},	/* I15 */
	{3, 3, 3, 3, 3, -1, 3, -1, -1, -1, 1, -1, -1, -1, -1, 1, 3, -1, -1},	/* I16 */
};

/**
* connection bits of certain bits
*/
static const char mConnectionbits[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18 */
	{0, 16, 0, 16, 0, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16, 22, -1, -1},	/* I00 */
	{1, 17, 1, 17, 1, -1, 22, -1, -1, -1, -1, -1, -1, -1, -1, 17, 23, -1, -1},	/* I01 */
	{2, 18, 2, 18, 2, 17, -1, 21, -1, -1, -1, 6, -1, -1, -1, 18, 24, 22, -1},	/* I02 */
	{3, 19, 3, 19, 3, 18, -1, 26, -1, 0, -1, -1, -1, -1, -1, 19, 25, 13, -1},	/* I03 */
	{4, 20, 4, 20, 4, -1, 23, -1, 29, -1, 3, -1, -1, -1, -1, 20, 26, -1, 16},	/* I04 */
	{5, 21, 5, 21, 5, 19, -1, 27, -1, 1, -1, 7, -1, 16, 20, -1, -1, 14, -1},	/* I05 */
	{6, 22, 6, 22, 6, -1, 24, -1, 30, -1, 4, -1, 9, 17, 21, -1, -1, -1, 17},	/* I06 */
	{7, 23, 7, 23, 7, 20, -1, 28, -1, 2, -1, 8, -1, 18, 22, -1, -1, 15, -1},	/* I07 */
	{8, 24, 8, 24, 8, -1, 25, -1, 31, -1, 5, -1, 10, 19, 23, -1, -1, -1, 18},	/* I08 */
	{9, 25, 9, 25, 9, 21, 27, -1, -1, -1, -1, -1, 11, -1, -1, 21, 27, -1, -1},	/* I09 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1},	/* I10 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25},	/* I11 */
	{0, -1, 4, 8, -1, 12, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6, -1},	/* I12 */
	{-1, 2, 6, -1, 10, -1, 13, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 7},	/* I13 */
	{12, 17, 22, 27, 0, 5, -1, -1, -1, -1, -1, -1, 12, -1, -1, 28, 1, -1, -1},	/* I14 */
	{13, 18, 23, 28, 1, 6, -1, -1, -1, 10, -1, -1, -1, -1, -1, 29, 2, -1, -1},	/* I15 */
	{14, 19, 24, 29, 2, -1, 8, -1, -1, -1, 11, -1, -1, -1, -1, 30, 3, -1, -1},	/* I16 */
};


/**
* connection shift bits of certain bits
*/
static const char mShiftConnectionbits[Soc_Aud_InterConnectionInput_Num_Input]
				[Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0   1   2   3   4   5   6    7  8   9  10  11  12  13  14  15  16  17  18 */
	{10, 26, 10, 26, 10, 19, -1, -1, -1, -1, -1, -1, -1, -1, -1, 31, -1, -1, -1},	/* I00 */
	{11, 27, 11, 27, 11, -1, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, -1, -1},	/* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{12, 28, 12, 28, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I05 */
	{13, 29, 13, 29, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I06 */
	{14, 30, 14, 30, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I07 */
	{15, 31, 15, 31, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{1, -1, 5, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 3, 7, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{15, 20, 25, 30, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1},	/* I15 */
	{16, 21, 26, 31, 4, -1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, 5, -1, -1},	/* I16 */
};

/**
* connection of register
*/
static const short mConnectionReg[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0      1      2      3      4      5      6      7      8      9     10     11    12      13     14     15
	    16     17     18 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x438,
	 0x438, -1, -1}, /* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, 0x438,
	 0x438, -1, -1}, /* I01 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x30, -1, -1, -1, 0x2C, -1, -1, -1, 0x438,
	 0x438, 0x30, -1}, /* I02 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, -1, -1, 0x438,
	 0x438, 0x30, -1}, /* I03 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, -1, -1, -1, 0x438,
	 0x438, -1, 0x30}, /* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, -1, 0x420, 0x420, -1,
	 -1, 0x30, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1,
	 -1, -1, 0x30},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, -1, 0x420, 0x420, -1,
	 -1, 0x30, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x28, -1, 0x28, -1, 0x2C, -1, 0x2C, 0x420, 0x420, -1,
	 -1, -1, 0x30},	/* I08 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x28, -1, -1, -1, -1, -1, -1, 0x2C, -1, -1, 0x438,
	 0x438, -1, -1}, /* I09 */
	{0x420, -1, 0x420, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, 0x420, -1}, /* I10 */
	{-1, 0x420, 0x420, -1, 0x420, -1, 0x420, -1, 0x420, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, 0x420}, /* I11 */
	{0x438, -1, 0x438, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, 0x440, -1}, /* I12 */
	{-1, 0x438, 0x438, -1, 0x438, -1, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, 0x440}, /* I13 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1, 0x30, -1, -1, 0x438,
	 0x440, -1, -1}, /* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, 0x30, -1, -1, -1, -1, -1, 0x438,
	 0x440, -1, -1}, /* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, 0x30, -1, -1, -1, -1, 0x438,
	 0x440, -1, -1}, /* I16 */
};


/**
* shift connection of register
*/
static const short mShiftConnectionReg[Soc_Aud_InterConnectionInput_Num_Input]
				[Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0    1    2    3    4    5    6    7    8    9    10    11    12    13    14    15    16    17    18 */
	{0x20, 0x20, 0x24, 0x24, 0x28, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x438, -1, -1, -1}, /* I00 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x440, -1, -1}, /* I01 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I02 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I03 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I04 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I05 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I06 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I07 */
	{0x20, 0x20, 0x24, 0x24, 0x28, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I08 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I09 */
	{0x420, -1, 0x420, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I10 */
	{0x420, 0x420, 0x420, -1, 0x420, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I11 */
	{0x438, -1, 0x438, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I12 */
	{-1, 0x438, 0x438, -1, 0x438, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I13 */
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* I14 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x440, -1, -1, -1},	/* I15 */
	{0x2C, 0x2C, 0x2C, 0x2C, 0x30, -1, 0x30, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x440, -1, -1},	/* I16 */
};

/**
* connection state of register
*/
static char mConnectionState[Soc_Aud_InterConnectionInput_Num_Input][Soc_Aud_InterConnectionOutput_Num_Output] = {
	/* 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I00 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I01 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I02 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I03 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I04 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I05 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I06 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I07 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I08 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I09 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I10 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I11 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I12 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I13 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I14 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I15 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* I16 */
};

/**
* value of connection register to set for certain input
*/
const uint32_t gHdmiConnectionValue[Soc_Aud_NUM_HDMI_INPUT] = {
	/* I_20 I_21 I_22 I_23 I_24 I_25 I_26 I_27 */
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7
};

/**
* mask of connection register to set for certain output
*/
const uint32_t gHdmiConnectionMask[Soc_Aud_NUM_HDMI_OUTPUT] = {
	/* O_20  O_21  O_22  O_23  O_24  O_25  O_26  O_27  O_28  O_29 */
	0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7
};

/**
* shift bits of connection register to set for certain output
*/
const char gHdmiConnectionShiftBits[Soc_Aud_NUM_HDMI_OUTPUT] = {
	/* O_20 O_21 O_22 O_23 O_24 O_25 O_26 O_27 O_28 O_29 */
	0, 3, 6, 9, 12, 15, 18, 21, 24, 27
};

/**
* connection state of HDMI
*/
char gHdmiConnectionState[Soc_Aud_NUM_HDMI_INPUT][Soc_Aud_NUM_HDMI_OUTPUT] = {
	/* O_20 O_21 O_22 O_23 O_24 O_25 O_26 O_27 O_28 O_29 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_20 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_21 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_22 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_23 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_24 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_25 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	/* I_26 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}	/* I_27 */
};


static bool CheckBitsandReg(short regaddr, char bits)
{
	if (regaddr <= 0 || bits < 0) {
		pr_notice("regaddr = %x bits = %d\n", regaddr, bits);
		return false;
	}
	return true;
}

bool SetHdmiInterConnection(uint32_t ConnectionState, uint32_t Input, uint32_t Output)
{
	uint32_t inputIndex;
	uint32_t outputIndex;

	/* check if connection request is valid */
	if (Input < Soc_Aud_HDMI_CONN_INPUT_BASE ||
	    Input > Soc_Aud_HDMI_CONN_INPUT_MAX ||
	    Output < Soc_Aud_HDMI_CONN_OUTPUT_BASE || Output > Soc_Aud_HDMI_CONN_OUTPUT_MAX) {
		return false;
	}

	inputIndex = Input - Soc_Aud_HDMI_CONN_INPUT_BASE;
	outputIndex = Output - Soc_Aud_HDMI_CONN_OUTPUT_BASE;

	/* do connection */
	switch (ConnectionState) {
	case Soc_Aud_InterCon_Connection:
		{
			uint32_t regValue = gHdmiConnectionValue[inputIndex];
			uint32_t regMask = gHdmiConnectionMask[outputIndex];
			uint32_t regShiftBits = gHdmiConnectionShiftBits[outputIndex];
			Afe_Set_Reg(AFE_HDMI_CONN0, regValue << regShiftBits,
				    regMask << regShiftBits);
			gHdmiConnectionState[inputIndex][outputIndex] = Soc_Aud_InterCon_Connection;
			break;
		}
	case Soc_Aud_InterCon_DisConnect:
		{
			uint32_t regValue = gHdmiConnectionValue[Soc_Aud_InterConnectionInput_I27 -
						 Soc_Aud_HDMI_CONN_INPUT_BASE];
			uint32_t regMask = gHdmiConnectionMask[outputIndex];
			uint32_t regShiftBits = gHdmiConnectionShiftBits[outputIndex];
			Afe_Set_Reg(AFE_HDMI_CONN0, regValue << regShiftBits,
				    regMask << regShiftBits);
			gHdmiConnectionState[inputIndex][outputIndex] = Soc_Aud_InterCon_DisConnect;
			break;
		}
	default:
		break;
	}

	return true;
}


bool SetConnectionState(uint32_t ConnectionState, uint32_t Input, uint32_t Output)
{
	pr_debug("%s ConnectionState = %d Input = %d Output = %d\n",
		__func__, ConnectionState, Input, Output);

	if (Input >= Soc_Aud_HDMI_CONN_INPUT_BASE || Output >= Soc_Aud_HDMI_CONN_OUTPUT_BASE)
		return SetHdmiInterConnection(ConnectionState, Input, Output);

	if (unlikely(Input >= Soc_Aud_InterConnectionInput_Num_Input ||
				Output >= Soc_Aud_InterConnectionOutput_Num_Output)) {
		pr_warn("out of bound connection mpConnectionTable[%d][%d]\n", Input, Output);
		return false;
	}

	if (unlikely(mConnectionTable[Input][Output] < 0 || mConnectionTable[Input][Output] == 0)) {
		pr_warn("No connection mpConnectionTable[%d][%d] = %d\n", Input, Output,
			mConnectionTable[Input][Output]);
		return true;
	}

	if (likely(mConnectionTable[Input][Output])) {
		int connectionBits = 0;
		int connectReg = 0;
		switch (ConnectionState) {
		case Soc_Aud_InterCon_DisConnect:
			{
				if ((mConnectionState[Input][Output] &
				     Soc_Aud_InterCon_Connection) == Soc_Aud_InterCon_Connection) {
					/* here to disconnect connect bits */
					connectionBits = mConnectionbits[Input][Output];
					connectReg = mConnectionReg[Input][Output];
					if (CheckBitsandReg(connectReg, connectionBits)) {
						Afe_Set_Reg(connectReg, 0 << connectionBits,
							    1 << connectionBits);
						mConnectionState[Input][Output] &=
						    ~(Soc_Aud_InterCon_Connection);
					}
				}
				if ((mConnectionState[Input][Output] &
				     Soc_Aud_InterCon_ConnectionShift) ==
				    Soc_Aud_InterCon_ConnectionShift) {
					/* here to disconnect connect shift bits */
					connectionBits = mShiftConnectionbits[Input][Output];
					connectReg = mShiftConnectionReg[Input][Output];
					if (CheckBitsandReg(connectReg, connectionBits)) {
						Afe_Set_Reg(connectReg, 0 << connectionBits,
							    1 << connectionBits);
						mConnectionState[Input][Output] &=
						    ~(Soc_Aud_InterCon_ConnectionShift);
					}
				}
				break;
			}
		case Soc_Aud_InterCon_Connection:
			{
				/* here to disconnect connect shift bits */
				connectionBits = mConnectionbits[Input][Output];
				connectReg = mConnectionReg[Input][Output];
				if (CheckBitsandReg(connectReg, connectionBits)) {
					Afe_Set_Reg(connectReg, 1 << connectionBits,
						    1 << connectionBits);
					mConnectionState[Input][Output] |=
					    Soc_Aud_InterCon_Connection;
				}
				break;
			}
		case Soc_Aud_InterCon_ConnectionShift:
			{
				if ((mConnectionTable[Input][Output] &
				     Soc_Aud_InterCon_ConnectionShift) !=
				    Soc_Aud_InterCon_ConnectionShift) {
					pr_warn("donn't support shift opeartion");
					break;
				}
				connectionBits = mShiftConnectionbits[Input][Output];
				connectReg = mShiftConnectionReg[Input][Output];
				if (CheckBitsandReg(connectReg, connectionBits)) {
					Afe_Set_Reg(connectReg, 1 << connectionBits,
						    1 << connectionBits);
					mConnectionState[Input][Output] |=
					    Soc_Aud_InterCon_ConnectionShift;
				}
				break;
			}
		default:
			pr_warn("%s no this state ConnectionState = %d\n", __func__, ConnectionState);
			break;
		}
	}
	return true;
}

