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

#include <linux/types.h>
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "mt_soc_hdmi_control.h"
#include "mt_soc_digital_type.h"

const struct Hdmi_Clock_Setting hdmi_clock_settings[] = {
	{32000, APLL_D24, 0},	/* 32k */
	{44100, APLL_D16, 0},	/* 44.1k */
	{48000, APLL_D16, 0},	/* 48k */
	{88200, APLL_D8, 0},	/* 88.2k */
	{96000, APLL_D8, 0},	/* 96k */
	{176400, APLL_D4, 0},	/* 176.4k */
	{192000, APLL_D4, 0}	/* 192k */
};

static unsigned int get_sample_rate_index(unsigned int sample_rate)
{
	switch (sample_rate) {
	case 32000:
		return 0;
	case 44100:
		return 1;
	case 48000:
		return 2;
	case 88200:
		return 3;
	case 96000:
		return 4;
	case 176400:
		return 5;
	case 192000:
		return 6;
	}
	return 0;
}

void set_hdmi_out_control(unsigned int channels)
{
	unsigned int register_value = 0;
	register_value |= (channels << 4);
	register_value |= (SOC_HDMI_INPUT_16BIT << 1);
	Afe_Set_Reg(AFE_HDMI_OUT_CON0, register_value, 0xffffffff);
}

void set_hdmi_out_control_enable(bool enable)
{
	unsigned int register_value = 0;
	if (enable)
		register_value |= 1;

	Afe_Set_Reg(AFE_HDMI_OUT_CON0, register_value, 0x1);
}

void set_hdmi_i2s(void)
{
	unsigned int register_value = 0;
	register_value |= (SOC_HDMI_I2S_32BIT << 4);
	register_value |= (SOC_HDMI_I2S_FIRST_BIT_1T_DELAY << 3);
	register_value |= (SOC_HDMI_I2S_LRCK_NOT_INVERSE << 2);
	register_value |= (SOC_HDMI_I2S_BCLK_INVERSE << 1);
	Afe_Set_Reg(AFE_8CH_I2S_OUT_CON, register_value, 0xffffffff);
}

void set_hdmi_i2s_enable(bool enable)
{
	unsigned int register_value = 0;
	if (enable)
		register_value |= 1;

	Afe_Set_Reg(AFE_8CH_I2S_OUT_CON, register_value, 0x1);
}

void set_hdmi_clock_source(unsigned int sample_rate)
{
	unsigned int sample_rate_index = get_sample_rate_index(sample_rate);
	AudDrv_SetHDMIClkSource(hdmi_clock_settings[sample_rate_index].SAMPLE_RATE,
				hdmi_clock_settings[sample_rate_index].CLK_APLL_SEL);
}

void set_hdmi_bck_div(unsigned int sample_rate)
{
	unsigned int register_value = 0;
	register_value |=
	    (hdmi_clock_settings[get_sample_rate_index(sample_rate)].HDMI_BCK_DIV) << 8;
	Afe_Set_Reg(AUDIO_TOP_CON3, register_value, 0x3F00);
}
