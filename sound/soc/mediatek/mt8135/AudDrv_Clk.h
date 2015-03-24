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

#ifndef _AUDDRV_CLK_H_
#define _AUDDRV_CLK_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
extern int Aud_Core_Clk_cntr;
extern int Aud_AFE_Clk_cntr;
extern int Aud_ADC_Clk_cntr;
extern int Aud_I2S_Clk_cntr;
extern int Aud_ANA_Clk_cntr;
extern int Aud_LineIn_Clk_cntr;
extern int Aud_HDMI_Clk_cntr;
extern int Afe_Mem_Pwr_on;
extern int Aud_APLL_Tuner_Clk_cntr;
extern int Aud_SPDIF_Clk_cntr;
extern int Aud_HDMI_DVT_Clk_cntr;
extern int Aud_TOP_APLL_Clk_cntr;

/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                 FUNCTION       D E F I N I T I O N
 *****************************************************************************/

void AudDrv_Clk_On(void);
void AudDrv_Clk_Off(void);

void AudDrv_ANA_Clk_On(void);
void AudDrv_ANA_Clk_Off(void);

void AudDrv_I2S_Clk_On(void);
void AudDrv_I2S_Clk_Off(void);

void AudDrv_Core_Clk_On(void);
void AudDrv_Core_Clk_Off(void);

void AudDrv_ADC_Clk_On(void);
void AudDrv_ADC_Clk_Off(void);

void AudDrv_HDMI_Clk_On(void);
void AudDrv_HDMI_Clk_Off(void);

void AudDrv_Suspend_Clk_On(void);
void AudDrv_Suspend_Clk_Off(void);

void AudDrv_APLL_TUNER_Clk_On(void);
void AudDrv_APLL_TUNER_Clk_Off(void);

void AudDrv_SPDIF_Clk_On(void);
void AudDrv_SPDIF_Clk_Off(void);

void AudDrv_SetApllTunerClk(int);

void AudDrv_SetHDMIClkSource(UINT32, int);

void AudDrv_TOP_Apll_Clk_On(void);
void AudDrv_TOP_Apll_Clk_Off(void);

#endif
