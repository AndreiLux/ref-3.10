/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*----------------------------------------------------------
  Audio Clock & Reset Genereter Register 0x34670000
----------------------------------------------------------*/

/*----------------------------------------------------------
  DSP CCLK Register 0x34670010
----------------------------------------------------------*/
/*----------------------------------------------------------
*  0x10 DSP_PLLKSRC_SEL	-	Main clock source selection
*							0 : OSC_CLK, 1 : NPLL 2 : IPLL
----------------------------------------------------------*/
typedef struct
{
	unsigned int clock_source_sel		:1;		/* 0  */
} DSP_PLLKSRC_SEL;

/*----------------------------------------------------------
*  0x14 DSP_CCLKSRC_MASK	-	clk source mask
*								0 : mask, 1 : not mask
----------------------------------------------------------*/
typedef struct
{
	unsigned int clock_source_mask		:1;		/* 0  */
} DSP_CCLKSRC_MASK;

/*----------------------------------------------------------
*  0x18 DSP_CCLK_DIV	-	Dividing value for dsp core clock
----------------------------------------------------------*/
typedef struct
{
	unsigned int cclk_div				:8;		/* 0:7  */
} DSP_CCLK_DIV;

/*----------------------------------------------------------
*  0x1C DSP_CCLK_GATE0	-	Clock gating for dsp core clock
*							0 : gating 1 : clock enable
----------------------------------------------------------*/
typedef struct
{
	unsigned int cclk_gate				:1;		/* 0  */
} DSP_CCLK_GATE0;

typedef struct {
	DSP_PLLKSRC_SEL		dsp_pllksrc_sel;
	DSP_CCLKSRC_MASK	dsp_cclksrc_mask;
	DSP_CCLK_DIV		dsp_cclk_div;
	DSP_CCLK_GATE0		dsp_cclk_gate0;
} AUD_CRG_CCLK_ODIN;
