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

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include <mach/pmic_mt6397_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>
#include <linux/module.h>
#include <sound/soc.h>

#define USING_CLASSD_AMP

static struct mt6397_Codec_Data_Priv *mCodec_data;

static uint32_t mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = { 44100, 32000 };

static DEFINE_MUTEX(Ana_Ctrl_Mutex);

/* Function implementation */
static void Speaker_Amp_Change(bool enable);
static void Audio_Amp_Change(int channels, bool enable);
static void Headset_Speaker_Amp_Change(bool enable);
static bool TurnOnADcPower(int ADCType, bool enable);
static void TurnOnDmicPower(bool enable);

uint32_t GetULFrequency(uint32_t frequency)
{
	uint32_t Reg_value = 0;
	pr_debug("AudCodec GetULFrequency ApplyDLFrequency = %d", frequency);

	switch (frequency) {
	case 8000:
		Reg_value = 0x0 << 1;
		break;
	case 16000:
		Reg_value = 0x5 << 1;
		break;
	case 32000:
		Reg_value = 0xa << 1;
		break;
	case 48000:
		Reg_value = 0xf << 1;
		break;
	default:
		pr_warn("AudCodec GetULFrequency with unsupported frequency = %d",
				frequency);
	}
	pr_debug("AudCodec GetULFrequency Reg_value = %d", Reg_value);
	return Reg_value;
}

static int mt6397_codec_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pr_debug("%s SNDRV_PCM_STREAM_CAPTURE\n", __func__);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("%s SNDRV_PCM_STREAM_PLAYBACK\n", __func__);

	return 0;
}

static int mt6397_codec_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("AudCodec %s set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
			__func__, substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("AudCodec %s set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
			__func__, substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	return 0;
}

static int mt6397_codec_trigger(struct snd_pcm_substream *substream, int command,
				struct snd_soc_dai *Daiport)
{
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	pr_debug("%s command = %d\n ", __func__, command);
	return 0;
}

static const struct snd_soc_dai_ops mt6397_aif1_dai_ops = {
	.startup = mt6397_codec_startup,
	.prepare = mt6397_codec_prepare,
	.trigger = mt6397_codec_trigger,
};

static struct snd_soc_dai_driver mtk_codec_dai_drvs[] = {
	{
		.name = MT_SOC_CODEC_TXDAI_NAME,
		.ops = &mt6397_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_DL1_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = MT_SOC_CODEC_RXDAI_NAME,
		.ops = &mt6397_aif1_dai_ops,
		.capture = {
			.stream_name = MT_SOC_UL1_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

uint32_t GetDLNewIFFrequency(unsigned int frequency)
{
	uint32_t Reg_value = 0;
	pr_debug("AudCodec %s frequency = %d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
		Reg_value = 0;
		break;
	case 11025:
		Reg_value = 1;
		break;
	case 12000:
		Reg_value = 2;
		break;
	case 16000:
		Reg_value = 3;
		break;
	case 22050:
		Reg_value = 4;
		break;
	case 24000:
		Reg_value = 5;
		break;
	case 32000:
		Reg_value = 6;
		break;
	case 44100:
		Reg_value = 7;
		break;
	case 48000:
		Reg_value = 8;
		break;
	default:
		pr_warn("AudCodec %s unexpected frequency = %d\n", __func__, frequency);
	}
	return Reg_value;
}

static bool GetDLStatus(void)
{
	int i = 0;
	for (i = 0; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

static bool GetAdcStatus(void)
{
	int i = 0;
	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_IN_ADC3; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

static bool GetULinkStatus(void)
{
	int i = 0;
	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}


static void AnalogSetMux(enum AUDIO_ANALOG_DEVICE_TYPE DeviceType, enum AUDIO_ANALOG_MUX_TYPE MuxType)
{
	uint32_t Reg_Value = 0;
	switch (DeviceType) {
	case AUDIO_ANALOG_DEVICE_OUT_EARPIECEL:
	case AUDIO_ANALOG_DEVICE_OUT_EARPIECER:
		{
		if (MuxType == AUDIO_ANALOG_MUX_OPEN) {
			Reg_Value = 0;
		} else if (MuxType == AUDIO_ANALOG_MUX_MUTE) {
			Reg_Value = 1 << 3;
		} else if (MuxType == AUDIO_ANALOG_MUX_VOICE) {
			Reg_Value = 2 << 3;
		} else {
			Reg_Value = 2 << 3;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDBUF_CFG0, Reg_Value, 0x000000018);	/* bits 3,4 */
		break;
	}
	case AUDIO_ANALOG_DEVICE_OUT_HEADSETL:
		{
		if (MuxType == AUDIO_ANALOG_MUX_OPEN) {
			Reg_Value = 0;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_L) {
			Reg_Value = 1 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_R) {
			Reg_Value = 2 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_STEREO) {
			Reg_Value = 3 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_AUDIO) {
			Reg_Value = 4 << 5;
			} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_AUDIO_MONO) {
			Reg_Value = 5 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_IV_BUFFER) {
			Reg_Value = 8 << 5;
		} else {
			Reg_Value = 4 << 5;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDBUF_CFG0, Reg_Value, 0x000001e0);
		break;
	}
	case AUDIO_ANALOG_DEVICE_OUT_HEADSETR:
		{
		if (MuxType == AUDIO_ANALOG_MUX_OPEN) {
			Reg_Value = 0;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_L) {
			Reg_Value = 1 << 9;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_R) {
			Reg_Value = 2 << 9;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_STEREO) {
			Reg_Value = 3 << 9;
		} else if (MuxType == AUDIO_ANALOG_MUX_AUDIO) {
			Reg_Value = 4 << 9;
			} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_AUDIO_MONO) {
			Reg_Value = 5 << 9;
		} else if (MuxType == AUDIO_ANALOG_MUX_IV_BUFFER) {
			Reg_Value = 8 << 9;
		} else {
			Reg_Value = 4 << 9;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDBUF_CFG0, Reg_Value, 0x00001e00);
		break;
	}
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKERR:
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKERL:
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R:
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_L:
		{
		if (MuxType == AUDIO_ANALOG_MUX_OPEN) {
			Reg_Value = 0;
		} else if ((MuxType == AUDIO_ANALOG_MUX_LINEIN_L)
				   || (MuxType == AUDIO_ANALOG_MUX_LINEIN_R)) {
			Reg_Value = 1 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_STEREO) {
			Reg_Value = 2 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_OPEN) {
			Reg_Value = 3 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_AUDIO) {
			Reg_Value = 4 << 2;
			} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_AUDIO_MONO) {
			Reg_Value = 5 << 2;
			} else if (MuxType == AUDIO_ANALOG_MUX_LINEIN_AUDIO_STEREO) {
			Reg_Value = 6 << 2;
		} else {
			Reg_Value = 4 << 2;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		/* pr_notice("AudCodec AnalogSetMux Get AUD_IV_CFG0 = 0x%x\n",Ana_Get_Reg(AUD_IV_CFG0)); */
		/* pr_notice("AudCodec AnalogSetMux MuxType = 0x%x\n",MuxType); */
		/* pr_notice("AudCodec AnalogSetMux Set AUD_IV_CFG0 = 0x%x\n",Reg_Value | (Reg_Value << 8)); */
			Ana_Set_Reg(AUD_IV_CFG0, Reg_Value | (Reg_Value << 8), 0x00001c1c);
		/* pr_notice("AudCodec AnalogSetMux AUD_IV_CFG0 = 0x%x\n",Ana_Get_Reg(AUD_IV_CFG0)); */
		break;
	}
	case AUDIO_ANALOG_DEVICE_IN_PREAMP_L:
		{
		if (MuxType == AUDIO_ANALOG_MUX_IN_MIC1) {
			Reg_Value = 1 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_MIC2) {
			Reg_Value = 2 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_MIC3) {
			Reg_Value = 3 << 2;
		} else {
			Reg_Value = 1 << 2;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDPREAMP_CON0, Reg_Value, 0x0000001c);
		break;
	}
	case AUDIO_ANALOG_DEVICE_IN_PREAMP_R:
		{
		if (MuxType == AUDIO_ANALOG_MUX_IN_MIC1) {
			Reg_Value = 1 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_MIC2) {
			Reg_Value = 2 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_MIC3) {
			Reg_Value = 3 << 5;
		} else {
			Reg_Value = 1 << 5;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDPREAMP_CON0, Reg_Value, 0x000000e0);
		break;
	}
	case AUDIO_ANALOG_DEVICE_IN_ADC1:
		{
		if (MuxType == AUDIO_ANALOG_MUX_IN_MIC1) {
			Reg_Value = 1 << 2;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_PREAMP_1) {
			Reg_Value = 4 << 2;
			} else if (MuxType == AUDIO_ANALOG_MUX_IN_LEVEL_SHIFT_BUFFER) {
			Reg_Value = 5 << 2;
		} else {
			Reg_Value = 1 << 2;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDADC_CON0, Reg_Value, 0x0000001c);
		break;
	}
	case AUDIO_ANALOG_DEVICE_IN_ADC2:
		{
		if (MuxType == AUDIO_ANALOG_MUX_IN_MIC1) {
			Reg_Value = 1 << 5;
		} else if (MuxType == AUDIO_ANALOG_MUX_IN_PREAMP_2) {
			Reg_Value = 4 << 5;
			} else if (MuxType == AUDIO_ANALOG_MUX_IN_LEVEL_SHIFT_BUFFER) {
			Reg_Value = 5 << 5;
		} else {
			Reg_Value = 1 << 5;
			pr_warn("AudCodec AnalogSetMux: %d %d\n", DeviceType, MuxType);
		}
		Ana_Set_Reg(AUDADC_CON0, Reg_Value, 0x000000e0);
		break;
	}
	default:
		break;
	}
}

/* iru:AudioPlatformDevice::AnalogOpen */
static void TurnOnDacPower(void)
{
	/* pr_notice("AudCodec TurnOnDacPower\n"); */
	/* Ana_Clk_Enable(0x0003, true); //Luke */
	/* pr_notice("AudCodec TurnOnDacPower mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = %d\n",
	   mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]); */
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
		    GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12,
				0xf000);
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffff);
	Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffff);
	Ana_Set_Reg(AFE_DL_SRC2_CON0_H,
		    0x0300 | GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) <<
		    12, 0x0ffff);
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x007f, 0xffff);
	Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x1801, 0xffff);
	/* Ana_Set_Reg(AFE_UL_SRC_CON1_H , 0x00e1, 0xffff); //Luke */
	/* Afe_Set_Reg(AFE_ADDA_UL_SRC_CON1, 0x00e10000, 0xffff0000);//Luke */
	/* Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0xf000);//Luke */
}

/* iru:AudioPlatformDevice::AnalogClose */
static void TurnOffDacPower(void)
{
	pr_debug("AudCodec TurnOffDacPower\n");

	Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x1800, 0xffff);
	if (!GetULinkStatus())
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);
}

static void SPKAutoTrimOffset(void)
{
	uint32_t WaitforReady = 0;
	uint32_t reg1 = 0;
	uint32_t chip_version = 0;
	int retyrcount = 50;

	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	Ana_Set_Reg(AUDLDO_CFG0, 0x0D92, 0xffff);	/* enable VA28 , VA 33 VBAT ref , set dc */
	Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000C, 0xffff);	/* set ACC mode  enable NVREF */
	Ana_Set_Reg(AUD_NCP0, 0xE000, 0xE000);	/* enable LDO ; fix me , seperate for UL  DL LDO */
	Ana_Set_Reg(NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
	Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
	udelay(200);
	Ana_Set_Reg(ZCD_CON0, 0x0301, 0xffff);	/* ZCD setting gain step gain and enable */
	Ana_Set_Reg(IBIASDIST_CFG0, 0x0552, 0xffff);	/* audio bias adjustment */
	Ana_Set_Reg(ZCD_CON4, 0x0505, 0xffff);	/* set DUDIV gain ,iv buffer gain */
	Ana_Set_Reg(AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
	udelay(100);
	Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0001, 0x0001);	/* reset docoder */
	Ana_Set_Reg(AUDDAC_CON0, 0x000f, 0xffff);	/* power on DAC */
	udelay(100);
	AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERR, AUDIO_ANALOG_MUX_AUDIO);
	AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERL, AUDIO_ANALOG_MUX_AUDIO);
	Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x0007);	/* set Mux */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);

	Ana_Set_Reg(SPK_CON1, 0, 0x7f00);	/* disable the software register mode */
	Ana_Set_Reg(SPK_CON4, 0, 0x7f00);	/* disable the software register mode */
	Ana_Set_Reg(SPK_CON9, 0x0018, 0xffff);	/* Choose new mode for trim (E2 Trim) */
	Ana_Set_Reg(SPK_CON0, 0x0008, 0xffff);	/* Enable auto trim */
	Ana_Set_Reg(SPK_CON3, 0x0008, 0xffff);	/* Enable auto trim R */
	Ana_Set_Reg(SPK_CON0, 0x3000, 0xf000);	/* set gain */
	Ana_Set_Reg(SPK_CON3, 0x3000, 0xf000);	/* set gain R */
	Ana_Set_Reg(SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
	Ana_Set_Reg(SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */
	Ana_Set_Reg(SPK_CON0, 0x0001, 0x0001);	/* Enable amplifier & auto trim */
	Ana_Set_Reg(SPK_CON3, 0x0001, 0x0001);	/* Enable amplifier & auto trim R */

	/* empirical data shows it usually takes 13ms to be ready */
	msleep(15);

	do {
		WaitforReady = Ana_Get_Reg(SPK_CON1);
		WaitforReady = ((WaitforReady & 0x8000) >> 15);

		if (WaitforReady) {
			WaitforReady = Ana_Get_Reg(SPK_CON4);
			WaitforReady = ((WaitforReady & 0x8000) >> 15);
			if (WaitforReady)
				break;
		}

		pr_debug("SPKAutoTrimOffset sleep\n");
		udelay(100);
	} while (retyrcount--);

	if (likely(WaitforReady))
		pr_debug("SPKAutoTrimOffset done\n");
	else
		pr_warn("SPKAutoTrimOffset fail\n");

	Ana_Set_Reg(SPK_CON9, 0x0, 0xffff);
	Ana_Set_Reg(SPK_CON5, 0, 0x7800);	/* set gain R */
	Ana_Set_Reg(SPK_CON0, 0x0000, 0x0001);
	Ana_Set_Reg(SPK_CON3, 0x0000, 0x0001);

	/* get trim offset result */
	pr_debug("GetSPKAutoTrimOffset ");
	Ana_Set_Reg(TEST_CON0, 0x0805, 0xffff);
	reg1 = Ana_Get_Reg(TEST_OUT_L);
	mCodec_data->mISPKltrim = ((reg1 >> 0) & 0xf);
	Ana_Set_Reg(TEST_CON0, 0x0806, 0xffff);
	reg1 = Ana_Get_Reg(TEST_OUT_L);
	/* mCodec_data->mISPKltrim |= (((reg1 >> 0) & 0x1) << 4); */
	mCodec_data->mISPKltrim |= 0xF;
	mCodec_data->mSPKlpolarity = ((reg1 >> 1) & 0x1);
	Ana_Set_Reg(TEST_CON0, 0x080E, 0xffff);
	reg1 = Ana_Get_Reg(TEST_OUT_L);
	mCodec_data->mISPKrtrim = ((reg1 >> 0) & 0xf);
	Ana_Set_Reg(TEST_CON0, 0x080F, 0xffff);
	reg1 = Ana_Get_Reg(TEST_OUT_L);
	mCodec_data->mISPKrtrim |= (((reg1 >> 0) & 0x1) << 4);
	mCodec_data->mSPKrpolarity = ((reg1 >> 1) & 0x1);

	chip_version = upmu_get_cid();

	if (chip_version == PMIC6397_E1_CID_CODE) {
		pr_debug("PMIC is MT6397 E1, set speaker R trim code to 0\n");
		mCodec_data->mISPKrtrim = 0;
		mCodec_data->mSPKrpolarity = 0;
	}

	pr_debug("mSPKlpolarity = %d mISPKltrim = 0x%x\n",
			  mCodec_data->mSPKlpolarity, mCodec_data->mISPKltrim);
	pr_debug("mSPKrpolarity = %d mISPKrtrim = 0x%x\n",
			  mCodec_data->mSPKrpolarity, mCodec_data->mISPKrtrim);

	/* turn off speaker after trim */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	Ana_Set_Reg(SPK_CON0, 0x0000, 0xffff);
	Ana_Set_Reg(SPK_CON3, 0x0000, 0xffff);
	Ana_Set_Reg(SPK_CON11, 0x0000, 0xffff);

	Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0001);	/* enable LDO ; fix me , seperate for UL  DL LDO */
	Ana_Set_Reg(AUDDAC_CON0, 0x0000, 0xffff);	/* RG DEV ck on */
	Ana_Set_Reg(AUD_IV_CFG0, 0x0000, 0xffff);	/* NCP on */
	Ana_Set_Reg(IBIASDIST_CFG0, 0x1552, 0xffff);	/* Audio headset power on */
	/* Ana_Set_Reg(AUDBUF_CFG1, 0x0000, 0x0100); */

	Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffff);
	Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	Ana_Set_Reg(AUD_NCP0, 0x0000, 0x6000);
	Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
}

static void GetTrimOffset(void)
{
	uint32_t reg1 = 0, reg2 = 0;
	bool trim_enable = 0;

	/* get to check if trim happen */
	reg1 = Ana_Get_Reg(PMIC_TRIM_ADDRESS1);
	reg2 = Ana_Get_Reg(PMIC_TRIM_ADDRESS2);
	pr_debug("AudCodec reg1 = 0x%x reg2 = 0x%x\n", reg1, reg2);

	trim_enable = (reg1 >> 11) & 1;
	if (trim_enable == 0) {
		mCodec_data->mHPLtrim = 2;
		mCodec_data->mHPLfinetrim = 0;
		mCodec_data->mHPRtrim = 2;
		mCodec_data->mHPRfinetrim = 0;
		mCodec_data->mIVHPLtrim = 3;
		mCodec_data->mIVHPLfinetrim = 0;
		mCodec_data->mIVHPRtrim = 3;
		mCodec_data->mIVHPRfinetrim = 0;
	} else {
		mCodec_data->mHPLtrim = ((reg1 >> 3) & 0xf);
		mCodec_data->mHPRtrim = ((reg1 >> 7) & 0xf);
		mCodec_data->mHPLfinetrim = ((reg1 >> 12) & 0x3);
		mCodec_data->mHPRfinetrim = ((reg1 >> 14) & 0x3);
		mCodec_data->mIVHPLtrim = ((reg2 >> 0) & 0xf);
		mCodec_data->mIVHPRtrim = ((reg2 >> 4) & 0xf);
		mCodec_data->mIVHPLfinetrim = ((reg2 >> 8) & 0x3);
		mCodec_data->mIVHPRfinetrim = ((reg2 >> 10) & 0x3);
	}

	pr_debug("AudCodec %s trim_enable = %d reg1 = 0x%x reg2 = 0x%x\n",
			  __func__, trim_enable, reg1, reg2);
	pr_debug
	("AudCodec %s mHPLtrim = 0x%x mHPLfinetrim = 0x%x mHPRtrim = 0x%x mHPRfinetrim = 0x%x\n",
	 __func__, mCodec_data->mHPLtrim, mCodec_data->mHPLfinetrim,
	 mCodec_data->mHPRtrim, mCodec_data->mHPRfinetrim);
	pr_debug
	("AudCodec %s mIVHPLtrim = 0x%x mIVHPLfinetrim = 0x%x mIVHPRtrim = 0x%x mIVHPRfinetrim = 0x%x\n",
	 __func__, mCodec_data->mIVHPLtrim, mCodec_data->mIVHPLfinetrim,
	 mCodec_data->mIVHPRtrim, mCodec_data->mIVHPRfinetrim);
}

static void SetHPTrimOffset(void)
{
	uint32_t AUDBUG_reg = 0;
	pr_debug("AudCodec SetHPTrimOffset");
	AUDBUG_reg |= 1 << 8;	/* enable trim function */
	AUDBUG_reg |= mCodec_data->mHPRfinetrim << 11;
	AUDBUG_reg |= mCodec_data->mHPLfinetrim << 9;
	AUDBUG_reg |= mCodec_data->mHPRtrim << 4;
	AUDBUG_reg |= mCodec_data->mHPLtrim;
	Ana_Set_Reg(AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void SetSPKTrimOffset(void)
{
	/* #ifdef MT8135_AUD_REG */
	uint32_t AUDBUG_reg = 0;
	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= mCodec_data->mSPKlpolarity << 13;	/* polarity */
	AUDBUG_reg |= mCodec_data->mISPKltrim << 8;	/* polarity */
	pr_debug("SetSPKlTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	Ana_Set_Reg(SPK_CON1, AUDBUG_reg, 0x7f00);
	AUDBUG_reg = 0;
	AUDBUG_reg |= 1 << 14;	/* enable trim function */
	AUDBUG_reg |= mCodec_data->mSPKrpolarity << 13;	/* polarity */
	AUDBUG_reg |= mCodec_data->mISPKrtrim << 8;	/* polarity */
	pr_debug("SetSPKrTrimOffset AUDBUG_reg = 0x%x\n", AUDBUG_reg);
	Ana_Set_Reg(SPK_CON4, AUDBUG_reg, 0x7f00);
}

static void SetIVHPTrimOffset(void)
{
	uint32_t AUDBUG_reg = 0;
	AUDBUG_reg |= 1 << 8;	/* enable trim function */

	if ((mCodec_data->mHPRfinetrim == 0) || (mCodec_data->mHPLfinetrim == 0))
		mCodec_data->mIVHPRfinetrim = 0;
	else
		mCodec_data->mIVHPRfinetrim = 2;

	AUDBUG_reg |= mCodec_data->mIVHPRfinetrim << 11;

	if ((mCodec_data->mHPRfinetrim == 0) || (mCodec_data->mHPLfinetrim == 0))
		mCodec_data->mIVHPLfinetrim = 0;
	else
		mCodec_data->mIVHPLfinetrim = 2;

	AUDBUG_reg |= mCodec_data->mIVHPLfinetrim << 9;

	AUDBUG_reg |= mCodec_data->mIVHPRtrim << 4;
	AUDBUG_reg |= mCodec_data->mIVHPLtrim;
	Ana_Set_Reg(AUDBUF_CFG3, AUDBUG_reg, 0x1fff);
}

static void Audio_Amp_Change(int channels, bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on Audio_Amp_Change\n");
		if (!GetDLStatus())
			TurnOnDacPower();

		/* here pmic analog control */
		/* iru : AudioMachineDevice::AnalogOpen */
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == false) {
			Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
			SetHPTrimOffset();
			Ana_Set_Reg(AUDLDO_CFG0, 0x0D92, 0xffff);	/* enable VA28 , VA 33 VBAT ref , set dc */
			Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000C, 0xffff);	/* set ACC mode      enable NVREF */
			Ana_Set_Reg(AUD_NCP0, 0xE000, 0xE000);	/* enable LDO ; fix me , seperate for UL  DL LDO */
			Ana_Set_Reg(NCP_CLKDIV_CON0, 0x102b, 0xffff);	/* RG DEV ck on */
			Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
			/* msleep(1);// temp remove */
			udelay(200);

			Ana_Set_Reg(ZCD_CON0, 0x0101, 0xffff);
			Ana_Set_Reg(AUDACCDEPOP_CFG0, 0x0030, 0xffff);
			Ana_Set_Reg(AUDBUF_CFG0, 0x0008, 0xffff);
			Ana_Set_Reg(IBIASDIST_CFG0, 0x0552, 0xffff);
			Ana_Set_Reg(ZCD_CON2, 0x0c0c, 0xffff);
			Ana_Set_Reg(ZCD_CON3, 0x000F, 0xffff);
			Ana_Set_Reg(AUDBUF_CFG1, 0x0900, 0xffff);
			Ana_Set_Reg(AUDBUF_CFG2, 0x0082, 0xffff);
			/* msleep(1);// temp remove */

			Ana_Set_Reg(AUDBUF_CFG0, 0x0009, 0xffff);
			/* msleep(30); //temp */

			Ana_Set_Reg(AUDBUF_CFG1, 0x0940, 0xffff);
			udelay(200);
			Ana_Set_Reg(AUDBUF_CFG0, 0x000F, 0xffff);
			/* msleep(1);// temp remove */

			Ana_Set_Reg(AUDBUF_CFG1, 0x0100, 0xffff);
			udelay(100);
			Ana_Set_Reg(AUDBUF_CFG2, 0x0022, 0xffff);
			Ana_Set_Reg(ZCD_CON2, 0x00c0c, 0xffff);
			udelay(100);
			/* msleep(1);// temp remove */

			Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0001, 0x0001);
			Ana_Set_Reg(AUDDAC_CON0, 0x000F, 0xffff);
			udelay(100);
			/* msleep(1);// temp remove */
			Ana_Set_Reg(AUDBUF_CFG0, 0x0006, 0x0007);
			AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_HEADSETR, AUDIO_ANALOG_MUX_AUDIO);
			AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_HEADSETL, AUDIO_ANALOG_MUX_AUDIO);

			Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		}
		pr_debug("AudCodec turn on Audio_Amp_Change done\n");
	} else {
		pr_debug("AudCodec turn off Audio_Amp_Change\n");

		/* iru : AudioMachineDevice::AnalogClose */
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTR] == false) {
			Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
			Ana_Set_Reg(ZCD_CON2, 0x0c0c, 0xffff);
			Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x1fe7);
			Ana_Set_Reg(IBIASDIST_CFG0, 0x1552, 0xffff);	/* RG DEV ck off; */
			Ana_Set_Reg(AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
			Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0001);

			if (GetULinkStatus() == false)
				Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffff);	/* need check */

			Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
			Ana_Set_Reg(AUD_NCP0, 0x0000, 0x6000);

			if (GetULinkStatus() == false)
				Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);

			Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
			if (GetDLStatus() == false)
				TurnOffDacPower();
		}
		pr_debug("AudCodec turn off Audio_Amp_Change done\n");

	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("AudCodec %s() gain = %d\n ", __func__,
			  (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] =
			ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		AudDrv_ANA_Clk_Off();
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Speaker_Amp_Change(bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on Speaker_Amp_Change\n");
		if (GetDLStatus() == false)
			TurnOnDacPower();

		/* here pmic analog control */
		Ana_Enable_Clk(0x0604, true);	/* enable SPK related CLK //Luke */
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		SetSPKTrimOffset();
		Ana_Set_Reg(AUDLDO_CFG0, 0x0D92, 0xffff);	/* enable VA28 , VA 33 VBAT ref , set dc */
		Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000C, 0xffff);	/* set ACC mode  enable NVREF */
		Ana_Set_Reg(AUD_NCP0, 0xE000, 0xE000);	/* enable LDO ; fix me , seperate for UL  DL LDO */
		Ana_Set_Reg(NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
		Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
		udelay(200);
		/* msleep(1); // temp remove */
		/* TODO: wait 6397 ECO to enable the SPK-R auto-trim monitor out read */
		/* SPKAutoTrimOffset(); // temp remove */
		Ana_Set_Reg(ZCD_CON0, 0x0301, 0xffff);	/* ZCD setting gain step gain and enable */
		Ana_Set_Reg(IBIASDIST_CFG0, 0x0552, 0xffff);	/* audio bias adjustment */
		Ana_Set_Reg(ZCD_CON4, 0x0505, 0xffff);	/* set DUDIV gain ,iv buffer gain */
		Ana_Set_Reg(AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
		udelay(100);
		Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0001, 0x0001);	/* reset docoder */
		Ana_Set_Reg(AUDDAC_CON0, 0x000f, 0xffff);	/* power on DAC */
		udelay(100);
		/* msleep(1);// temp remove */
		AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERR, AUDIO_ANALOG_MUX_AUDIO);
		AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERL, AUDIO_ANALOG_MUX_AUDIO);
		Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x0007);	/* set Mux */
		/* msleep(1);// temp remove */
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);

		Ana_Set_Reg(SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
		Ana_Set_Reg(SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */

#ifdef USING_CLASSD_AMP
		if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 0) {	/* Stereo */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - Stereo\n"); */
			Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff);
			Ana_Set_Reg(SPK_CON3, 0x3001, 0xffff);
			Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
			Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 1) {	/* MonoLeft */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - MonoLeft\n"); */
			Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff);
			Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 2) {	/* MonoRight */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - MonoRight\n"); */
			Ana_Set_Reg(SPK_CON3, 0x3001, 0xffff);
			Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
		} else {
			AUDIO_ASSERT(true);
		}
#else
		if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 0) {	/* Stereo */
			Ana_Set_Reg(SPK_CON0, 0x3005, 0xffff);
			Ana_Set_Reg(SPK_CON3, 0x3005, 0xffff);
			Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
			Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 1) {	/* MonoLeft */
			Ana_Set_Reg(SPK_CON0, 0x3005, 0xffff);
			Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 2) {	/* MonoRight */
			Ana_Set_Reg(SPK_CON3, 0x3005, 0xffff);
			Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
		} else {
			AUDIO_ASSERT(true);
		}
#endif

		if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 0) {	/* Stereo */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - Stereo\n"); */

			Ana_Set_Reg(SPK_CON9, 0x0800, 0xffff);	/* SPK gain setting */
			Ana_Set_Reg(SPK_CON5, (0x8 << 11), 0x7800);	/* SPK gain setting */
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 1) {	/* MonoLeft */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - MonoLeft\n"); */
			Ana_Set_Reg(SPK_CON9, 0x0800, 0xffff);	/* SPK gain setting */
		} else if (mCodec_data->mAudio_Ana_Spk_Channel_Sel == 2) {	/* MonoRight */
			/* pr_notice("AudCodec turn on Speaker_Amp_Change - MonoRight\n"); */
			Ana_Set_Reg(SPK_CON5, (0x8 << 11), 0x7800);	/* SPK gain setting */
		} else {
			AUDIO_ASSERT(true);
		}
		Ana_Set_Reg(SPK_CON11, 0x0f00, 0xffff);	/* spk output stage enabke and enable */
		usleep_range(4000, 5000);
		pr_debug("AudCodec turn on Speaker_Amp_Change done\n");

	} else {
		pr_debug("AudCodec turn off Speaker_Amp_Change\n");
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		Ana_Set_Reg(SPK_CON0, 0x0000, 0xffff);
		Ana_Set_Reg(SPK_CON3, 0x0000, 0xffff);
		Ana_Set_Reg(SPK_CON11, 0x0000, 0xffff);
		Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0001);	/* enable LDO ; fix me , seperate for UL  DL LDO */
		Ana_Set_Reg(AUDDAC_CON0, 0x0000, 0xffff);	/* RG DEV ck on */
		Ana_Set_Reg(AUD_IV_CFG0, 0x0000, 0xffff);	/* NCP on */
		Ana_Set_Reg(IBIASDIST_CFG0, 0x1552, 0xffff);	/* Audio headset power on */
		/* Ana_Set_Reg(AUDBUF_CFG1, 0x0000, 0x0100); */
		if (GetULinkStatus() == false)
			Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffff);

		Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
		Ana_Set_Reg(AUD_NCP0, 0x0000, 0x6000);
		if (GetULinkStatus() == false)
			Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);

		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		Ana_Enable_Clk(0x0604, false);	/* disable SPK related CLK        //Luke */
		if (GetDLStatus() == false)
			TurnOffDacPower();

		Ana_Set_Reg(ZCD_CON0, 0x0101, 0xffff);	/* temp solution, set ZCD_CON0 to 0x101 for pop noise */
		pr_debug("AudCodec turn off Speaker_Amp_Change done\n");
	}
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()=%d\n", __func__,
			  mCodec_data->
			  mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s() gain = %d\n ", __func__,
			  (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] =
			ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
		AudDrv_ANA_Clk_Off();
	}
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	if (enable) {
		pr_debug("AudCodec turn on Headset_Speaker_Amp_Change\n");
		if (!GetDLStatus())
			TurnOnDacPower();

		/* here pmic analog control */
		Ana_Enable_Clk(0x0604, true);	/* enable SPK related CLK        //Luke */
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		SetHPTrimOffset();
		SetIVHPTrimOffset();
		SetSPKTrimOffset();

		Ana_Set_Reg(AUDLDO_CFG0, 0x0D92, 0xffff);	/* enable VA28 , VA 33 VBAT ref , set dc */
		Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000C, 0xffff);	/* set ACC mode  enable NVREF */
		Ana_Set_Reg(AUD_NCP0, 0xE000, 0xE000);	/* enable LDO ; fix me , seperate for UL  DL LDO */
		Ana_Set_Reg(NCP_CLKDIV_CON0, 0x102B, 0xffff);	/* RG DEV ck on */
		Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0000, 0xffff);	/* NCP on */
		udelay(200);
		/* msleep(30); //temp */

		/* TODO: wait 6397 ECO to enable the SPK-R auto-trim monitor out read */
		/* SPKAutoTrimOffset(); //temp */

		Ana_Set_Reg(ZCD_CON0, 0x0301, 0xffff);	/* ZCD setting gain step gain and enable */
		Ana_Set_Reg(AUDACCDEPOP_CFG0, 0x0030, 0xffff);	/* select charge current ; fix me */
		Ana_Set_Reg(AUDBUF_CFG0, 0x0008, 0xffff);	/* set voice playback with headset */
		Ana_Set_Reg(IBIASDIST_CFG0, 0x0552, 0xffff);	/* audio bias adjustment */
		Ana_Set_Reg(ZCD_CON2, 0x0C0C, 0xffff);	/* HP PGA gain */
		Ana_Set_Reg(ZCD_CON3, 0x000F, 0xffff);	/* HP PGA gain */
		Ana_Set_Reg(AUDBUF_CFG1, 0x0900, 0xffff);	/* HP enhance */
		Ana_Set_Reg(AUDBUF_CFG2, 0x0082, 0xffff);	/* HS enahnce */
		Ana_Set_Reg(AUDBUF_CFG0, 0x0009, 0xffff);
		Ana_Set_Reg(AUDBUF_CFG1, 0x0940, 0xffff);	/* HP vcm short */
		udelay(200);
		Ana_Set_Reg(AUDBUF_CFG0, 0x000F, 0xffff);	/* HP power on */
		/* msleep(1); */
		Ana_Set_Reg(AUDBUF_CFG1, 0x0100, 0xffff);	/* HP vcm not short */
		udelay(100);
		Ana_Set_Reg(AUDBUF_CFG2, 0x0022, 0xffff);	/* HS VCM not short */
		/* msleep(1); */

		Ana_Set_Reg(ZCD_CON2, 0x0808, 0xffff);	/* HP PGA gain */
		udelay(100);
		Ana_Set_Reg(ZCD_CON4, 0x0505, 0xffff);	/* HP PGA gain */

		Ana_Set_Reg(AUD_IV_CFG0, 0x1111, 0xffff);	/* set IV buffer on */
		udelay(100);
		Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0001, 0x0001);	/* reset docoder */
		Ana_Set_Reg(AUDDAC_CON0, 0x000F, 0xffff);	/* power on DAC */
		udelay(100);
		AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERR, AUDIO_ANALOG_MUX_AUDIO);
		AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKERL, AUDIO_ANALOG_MUX_AUDIO);
		Ana_Set_Reg(AUDBUF_CFG0, 0x1106, 0x1106);	/* set headhpone mux */
		/* msleep(1); */

		Ana_Set_Reg(SPK_CON9, 0x0100, 0x0f00);	/* set gain L */
		Ana_Set_Reg(SPK_CON5, (0x1 << 11), 0x7800);	/* set gain R */

#ifdef USING_CLASSD_AMP
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff);
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON3, 0x3001, 0xffff);
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
		Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
#else
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON0, 0x3005, 0xffff);
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON3, 0x3005, 0xffff);
		/* speaker gain setting , trim enable , spk enable , class AB or D */
		Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff);
		Ana_Set_Reg(SPK_CON5, 0x0014, 0x07ff);
#endif

		Ana_Set_Reg(SPK_CON9, 0x0400, 0xffff);	/* SPK gain setting */
		Ana_Set_Reg(SPK_CON5, (0x4 << 11), 0x7800);	/* SPK-R gain setting */
		Ana_Set_Reg(SPK_CON11, 0x0f00, 0xffff);	/* spk output stage enabke and enableAudioClockPortDST */
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		usleep_range(4000, 5000);

		pr_debug("AudCodec turn on Headset_Speaker_Amp_Change done\n");

	} else {
		pr_debug("AudCodec turn off Headset_Speaker_Amp_Change\n");
		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
		Ana_Set_Reg(SPK_CON0, 0x0000, 0xffff);
		Ana_Set_Reg(SPK_CON3, 0x0000, 0xffff);
		Ana_Set_Reg(SPK_CON11, 0x0000, 0xffff);
		Ana_Set_Reg(ZCD_CON2, 0x0C0C, 0x0f0f);

		Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x0007);
		Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x1fe0);
		Ana_Set_Reg(IBIASDIST_CFG0, 0x1552, 0xffff);
		Ana_Set_Reg(AUDDAC_CON0, 0x0000, 0xffff);
		Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0001);
		Ana_Set_Reg(AUD_IV_CFG0, 0x0010, 0xffff);
		Ana_Set_Reg(AUDBUF_CFG1, 0x0000, 0x0100);
		Ana_Set_Reg(AUDBUF_CFG2, 0x0000, 0x0080);

		if (!GetULinkStatus())
			Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffff);

		Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0001, 0xffff);
		Ana_Set_Reg(AUD_NCP0, 0x0000, 0x6000);
		if (!GetULinkStatus())
			Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);

		Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
		Ana_Enable_Clk(0x0604, false);	/* disable SPK related CLK   //Luke */
		if (!GetDLStatus())
			TurnOffDacPower();
		pr_debug("AudCodec turn off Headset_Speaker_Amp_Change done\n");
		Ana_Set_Reg(ZCD_CON0, 0x0101, 0xffff);	/* ZCD setting gain step gain and enable */
	}
}

static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
								   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()=%d\n", __func__,
			  mCodec_data->
			  mAudio_Ana_DevicePower
			  [AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_L]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_L];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
								   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s() gain = %d\n ", __func__,
			  (int)ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_L] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPEAKER_HEADSET_L] =
			ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
		AudDrv_ANA_Clk_Off();
	}
	return 0;
}

static const char *const amp_function[] = { "Off", "On" };

/* static const char *DAC_SampleRate_function[] = {"8000", "11025", "16000", "24000", "32000", "44100", "48000"}; */
static const char *const DAC_DL_PGA_Headset_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db",
	"-4Db",
	"RES1", "RES2", "-40Db"
};

static const char *const DAC_DL_PGA_Handset_GAIN[] = {
	"-21Db", "-19Db", "-17Db", "-15Db", "-13Db", "-11Db", "-9Db", "-7Db", "-5Db",
	"-3Db", "-1Db", "1Db", "3Db", "5Db", "7Db", "9Db"
};				/* todo: 6397's setting */

static const char *const DAC_DL_PGA_Speaker_GAIN[] = {
	"Mute", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};

static const char *const Voice_Mux_function[] = { "Voice", "Speaker" };
static const char *const Speaker_selection_function[] = { "Stereo", "MonoLeft", "MonoRight" };

static int Speaker_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("AudCodec %s = %d\n", __func__,
			 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL];

	mutex_unlock(&Ana_Ctrl_Mutex);

	return 0;
}

static int Speaker_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index;

	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		mutex_unlock(&Ana_Ctrl_Mutex);
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	/* Allow SPK to mute regardless of the adjuestment */
	if (index > 0)
		index += mCodec_data->spk_pga_adj;

	index = clamp(index, 0, (int)ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1);

	Ana_Set_Reg(SPK_CON9, index << 8, 0x00000f00);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] = index;

	mutex_unlock(&Ana_Ctrl_Mutex);

	return 0;
}

static int Speaker_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("AudCodec %s = %d\n", __func__,
			 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR];

	mutex_unlock(&Ana_Ctrl_Mutex);

	return 0;
}

static int Speaker_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	int index;

	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		mutex_unlock(&Ana_Ctrl_Mutex);
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	/* Allow SPK to mute regardless of the adjuestment */
	if (index > 0)
		index += mCodec_data->spk_pga_adj;

	index = clamp(index, 0, (int)ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1);

	Ana_Set_Reg(SPK_CON5, index << 11, 0x00007800);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] = index;

	mutex_unlock(&Ana_Ctrl_Mutex);

	return 0;
}

static int Speaker_Channel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s()\n", __func__);
	mCodec_data->mAudio_Ana_Spk_Channel_Sel =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Speaker_Channel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s = %d\n", __func__,
			 mCodec_data->mAudio_Ana_Spk_Channel_Sel);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Spk_Channel_Sel;
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
			 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	int index = 0;
	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(ZCD_CON2, index, 0x0000000F);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
			 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	return 0;
}

static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(ZCD_CON2, index << 8, 0x000000F00);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static const struct soc_enum Audio_Amp_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	/* here comes pga gain setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN), DAC_DL_PGA_Handset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
	/* Mux Function */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Voice_Mux_function), Voice_Mux_function),
	/* Configurations */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Speaker_selection_function), Speaker_selection_function),
};

static const struct snd_kcontrol_new mt6397_snd_controls[] = {
	/* SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_Amp_Enum[0], Audio_AmpR_Get, Audio_AmpR_Set), */
	SOC_ENUM_EXT("Audio_Amp_Switch", Audio_Amp_Enum[1], Audio_AmpL_Get, Audio_AmpL_Set),
	/* SOC_ENUM_EXT("Voice_Amp_Switch", Audio_Amp_Enum[2], Voice_Amp_Get, Voice_Amp_Set), */
	SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_Amp_Enum[3], Speaker_Amp_Get, Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_Amp_Enum[4], Headset_Speaker_Amp_Get,
	Headset_Speaker_Amp_Set),

	SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_Amp_Enum[5], Headset_PGAL_Get, Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_Amp_Enum[6], Headset_PGAR_Get, Headset_PGAR_Set),
	/* SOC_ENUM_EXT("Hanset_PGA_GAIN", Audio_Amp_Enum[7], Handset_PGA_Get, Handset_PGA_Set), */
	SOC_ENUM_EXT("Speaker_PGAL_GAIN", Audio_Amp_Enum[8], Speaker_PGAL_Get, Speaker_PGAL_Set),
	SOC_ENUM_EXT("Speaker_PGAR_GAIN", Audio_Amp_Enum[9], Speaker_PGAR_Get, Speaker_PGAR_Set),

	SOC_ENUM_EXT("Speaker_Channel_Select", Audio_Amp_Enum[11], Speaker_Channel_Get,
	Speaker_Channel_Set),

};

/*
static const struct snd_kcontrol_new mt6397_Voice_Switch[] =
{
	SOC_DAPM_ENUM_EXT("Voice Mux", Audio_Amp_Enum[9], Voice_Mux_Get, Voice_Mux_Set),
};
*/

static int Codec_Loopback_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mCodec_data->mCodecLoopbackType;
	return 0;
}

static int Codec_Loopback_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	uint32_t previousLoopbackType = mCodec_data->mCodecLoopbackType;

	pr_debug("%s %ld\n", __func__, ucontrol->value.integer.value[0]);

	if (previousLoopbackType == ucontrol->value.integer.value[0]) {
		pr_debug("%s dummy operation for %u", __func__,
				  mCodec_data->mCodecLoopbackType);
		return 0;
	}

	if (previousLoopbackType != CODEC_LOOPBACK_NONE) {
		/* disable uplink */
		if (previousLoopbackType == CODEC_LOOPBACK_AMIC_TO_SPK ||
			previousLoopbackType == CODEC_LOOPBACK_AMIC_TO_HP ||
			previousLoopbackType == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
			previousLoopbackType == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
			if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] == 1) {
				mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
				    0;
				TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
			}

			if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] == 1) {
				mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
				    0;
				TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
			}
			mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = 0;
			mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] = 0;
		} else if (previousLoopbackType == CODEC_LOOPBACK_DMIC_TO_SPK ||
			   previousLoopbackType == CODEC_LOOPBACK_DMIC_TO_HP) {
			if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC]
				== 1) {
				mCodec_data->mAudio_Ana_DevicePower
				[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC] = 0;
				TurnOnDmicPower(false);
			}
		}
		/* disable downlink */
		if (previousLoopbackType == CODEC_LOOPBACK_AMIC_TO_SPK ||
			previousLoopbackType == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
			previousLoopbackType == CODEC_LOOPBACK_DMIC_TO_SPK) {
			if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] == 1) {
				mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = 0;
				Speaker_Amp_Change(false);
			}
			AudDrv_ANA_Clk_Off();
		} else if (previousLoopbackType == CODEC_LOOPBACK_AMIC_TO_HP ||
				   previousLoopbackType == CODEC_LOOPBACK_DMIC_TO_HP ||
			   previousLoopbackType == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
			if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == 1) {
				mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = 0;
				Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
			}
			AudDrv_ANA_Clk_Off();
		}
	}
	/* enable uplink */
	if (ucontrol->value.integer.value[0] == CODEC_LOOPBACK_AMIC_TO_SPK ||
		ucontrol->value.integer.value[0] == CODEC_LOOPBACK_AMIC_TO_HP ||
	    ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
		AudDrv_ANA_Clk_On();
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 48000;
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = 48000;

		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] == 0) {
			TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] = 1;
		}

		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] == 0) {
			TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] = 1;
		}
		/* mux selection */
		if (ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
		    ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
			AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC2);
			mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = 2;
		} else {
			AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC1);
			mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = 1;
		}
	} else if (ucontrol->value.integer.value[0] == CODEC_LOOPBACK_DMIC_TO_SPK ||
		   ucontrol->value.integer.value[0] == CODEC_LOOPBACK_DMIC_TO_HP) {
		AudDrv_ANA_Clk_On();
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 32000;
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = 32000;

		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC] == 0) {
			TurnOnDmicPower(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC] = 1;
		}
	}
	/* enable downlink */
	if (ucontrol->value.integer.value[0] == CODEC_LOOPBACK_AMIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == CODEC_LOOPBACK_DMIC_TO_SPK) {
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] == 0) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_SPKL] = 1;
		}
	} else if (ucontrol->value.integer.value[0] == CODEC_LOOPBACK_AMIC_TO_HP ||
		   ucontrol->value.integer.value[0] == CODEC_LOOPBACK_DMIC_TO_HP ||
		   ucontrol->value.integer.value[0] == CODEC_LOOPBACK_HEADSET_MIC_TO_HP) {
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] == 0) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_VOLUME_HPOUTL] = 1;
		}
	}

	mCodec_data->mCodecLoopbackType = ucontrol->value.integer.value[0];
	return 0;
}

static const char *const Codec_Loopback_function[] = {
	ENUM_TO_STR(CODEC_LOOPBACK_NONE),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(CODEC_LOOPBACK_HEADSET_MIC_TO_HP),
};

static const struct soc_enum Pmic_Factory_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Codec_Loopback_function), Codec_Loopback_function),
};

static const struct snd_kcontrol_new mt6397_factory_controls[] = {
	SOC_ENUM_EXT("Codec_Loopback_Select", Pmic_Factory_Enum[0], Codec_Loopback_Get,
	Codec_Loopback_Set),
};

static void TurnOnDmicPower(bool enable)
{
	if (enable) {
		/* pmic digital part */
		Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0002);
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
		/* Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x0000, 0x07a3); //Luke */

		Ana_Enable_Clk(0x0003, true);
		Ana_Set_Reg(ANA_AUDIO_TOP_CON0, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_UL_SRC_CON0_H,
			    0x00e0 | GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]),
					0xffff);
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x007f, 0xffff);
		/* Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x0023, 0x07a3); //Luke */
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0023, 0xffff);

		/* AudioMachineDevice */
		Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000c, 0xffff);
		Ana_Set_Reg(AUDDIGMI_CON0, 0x0181, 0xffff);
	} else {
		/* AudioMachineDevice */
		Ana_Set_Reg(AUDDIGMI_CON0, 0x0080, 0xffff);
		/* pmic digital part */
		Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
		if (GetDLStatus() == false)
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);
		/* e_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x00000000, 0xffffffff); Luke */
	}
}

static bool TurnOnADcPower(int ADCType, bool enable)
{
	if (enable) {
		if (!GetAdcStatus()) {
			/* pmic digital part */
			Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0002);
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
			Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0002, 0x0002);
			Ana_Enable_Clk(0x0003, true);
			Ana_Set_Reg(ANA_AUDIO_TOP_CON0, 0x0000, 0xffff);
			Ana_Set_Reg(AFE_UL_SRC_CON0_H,
						0x0000 |
				    GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]),
						0xffff);
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x007f, 0xffff);
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);

			/* pmic analog part */
			Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x000c, 0xffff);
			Ana_Set_Reg(AUDLDO_CFG0, 0x0D92, 0xffff);
			Ana_Set_Reg(AUD_NCP0, 0x9000, 0x9000);

			AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_ADC1, AUDIO_ANALOG_MUX_IN_PREAMP_1);
			AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_ADC2, AUDIO_ANALOG_MUX_IN_PREAMP_2);

			/* AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC1); */
			/* AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_R, AUDIO_ANALOG_MUX_IN_MIC1); */

			/* TODO: Only Single mic is supported, need to add dual-mic path if necessary */
			/* if (mBlockAttribute[DeviceType].mMuxSelect == AudioAnalogType::MUX_IN_LEVEL_SHIFT_BUFFER) */
			/* { */
			/* Ana_Set_Reg(AUDLSBUF_CON1 , 0x0011, 0x003f); // Select LSB MUX as Line-In */
			/* Ana_Set_Reg(AUDLSBUF_CON0 , 0x0003, 0x0003); // Power On LSB */
			/* } */
			/* else   // If not LSB, then must use preAmplifier. So set MUX of preamplifier */
			/* { */
			/* AnalogSetMux(AudioAnalogType::DEVICE_IN_PREAMP_L,
			   (AudioAnalogType::MUX_TYPE)mBlockAttribute[AudioAnalogType::DEVICE_IN_PREAMP_L].mMuxSelect); */
			/* AnalogSetMux(AudioAnalogType::DEVICE_IN_PREAMP_R,
			   (AudioAnalogType::MUX_TYPE)mBlockAttribute[AudioAnalogType::DEVICE_IN_PREAMP_R].mMuxSelect); */
			/* } */
			Ana_Set_Reg(AUDPREAMP_CON0, 0x0003, 0x0003);	/* open power */
			/* msleep(1); */
			/* if (mBlockAttribute[DeviceType].mMuxSelect == AudioAnalogType::MUX_IN_LEVEL_SHIFT_BUFFER) */
			/* { */
			/* Ana_Set_Reg(AUDADC_CON0 , 0x00B7, 0x00ff); // Set ADC input as LSB */
			/* } */
			/* else */
			/* { */
			Ana_Set_Reg(AUDADC_CON0, 0x0093, 0xffff);
			/* } */
			Ana_Set_Reg(NCP_CLKDIV_CON0, 0x102B, 0x102B);
			Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0000, 0xffff);
			Ana_Set_Reg(AUDDIGMI_CON0, 0x0180, 0x0180);
			Ana_Set_Reg(AUDPREAMPGAIN_CON0, 0x0033, 0x0033);
		}
		/* TODO: Please Luke check how to separate AUDIO_ANALOG_DEVICE_IN_ADC1 and
		   AUDIO_ANALOG_DEVICE_IN_ADC2 */

	} else {
		if (!GetAdcStatus()) {
			/* pmic analog part */
			Ana_Set_Reg(AUDPREAMP_CON0, 0x0000, 0x0003);	/* LDO off */
			Ana_Set_Reg(AUDADC_CON0, 0x00B4, 0xffff);	/* RD_CLK off */
			Ana_Set_Reg(AUDDIGMI_CON0, 0x0080, 0xffff);	/* NCP off */
			Ana_Set_Reg(AUD_NCP0, 0x0000, 0x1000);	/* turn iogg LDO */
			Ana_Set_Reg(AUDLSBUF_CON0, 0x0000, 0x0003);	/* Power Off LSB */
			if (GetDLStatus() == false)
				Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0004, 0xffff);

			if (GetDLStatus() == false)
				Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);

			Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0002);
			/* pmic digital part */

			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
			if (!GetDLStatus())
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);
		}
	}
	return true;
}

/* here start uplink power function */
static const char *const ADC_function[] = { "Off", "On" };
static const char *const DMIC_function[] = { "Off", "On" };

/* static const char *PreAmp_Mux_function[] = {"OPEN", "IN_ADC1", "IN_ADC2", "IN_ADC3"}; */
static const char *const PreAmp_Mux_function[] = { "OPEN", "AIN1", "AIN2", "AIN3" };

/* static const char *ADC_SampleRate_function[] = {"8000", "16000", "32000", "48000"}; */
/* const int PreAmpGain[] = {2, 8, 14, 20, 26, 32}; */
static const char *const ADC_UL_PGA_GAIN[] = { "2Db", "8Db", "14Db", "20Db", "26Db", "32Db" };


static const struct soc_enum Audio_UL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ADC_function),
	SOC_ENUM_SINGLE_EXT(2, ADC_function),
	SOC_ENUM_SINGLE_EXT(4, PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(4, PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(6, ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(6, ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(2, DMIC_function),
};


static int Audio_DMIC_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec Audio_DMIC_Get = %d\n",
			  mCodec_data->
			  mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC];
	return 0;
}


static int Audio_DMIC_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		TurnOnDmicPower(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC] =
			ucontrol->value.integer.value[0];
		TurnOnDmicPower(false);
		AudDrv_ANA_Clk_Off();
	}
	return 0;
}


static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
			  mCodec_data->
			  mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
			ucontrol->value.integer.value[0];
		TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		AudDrv_ANA_Clk_Off();
	}
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
			  mCodec_data->
			  mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		AudDrv_ANA_Clk_On();
		TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
			ucontrol->value.integer.value[0];
		TurnOnADcPower(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		AudDrv_ANA_Clk_Off();
	}
	return 0;
}

static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug
	("AudCodec %s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = %d\n",
	 __func__,
	 mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0) {
		/* Ana_Set_Reg(AUDTOP_CON0, 0x0003, 0x0000000f); */
		/* AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, ); */
	} else if (ucontrol->value.integer.value[0] == 1) {
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC1);
		/* Ana_Set_Reg(AUDTOP_CON0, 0, 0x0000000f); */
	} else if (ucontrol->value.integer.value[0] == 2) {
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC2);
		/* Ana_Set_Reg(AUDTOP_CON0, 1, 0x0000000f); */
	} else if (ucontrol->value.integer.value[0] == 3) {
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_L, AUDIO_ANALOG_MUX_IN_MIC3);
		/* Ana_Set_Reg(AUDTOP_CON3, 0x0100, 0x00000100); */
		/* Ana_Set_Reg(AUDTOP_CON0, 0x4, 0x0000000f); */
	} else {
		/* Ana_Set_Reg(AUDTOP_CON0, 0, 0x0000000f); */
		pr_warn("AudCodec AnalogSetMux warning");
	}

	pr_debug("AudCodec %s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	if (ucontrol->value.integer.value[0] == 0) {
		/* Ana_Set_Reg(AUDTOP_CON1, 0x0020, 0x000000f0); */
	} else if (ucontrol->value.integer.value[0] == 1) {
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_R, AUDIO_ANALOG_MUX_IN_MIC1);
		/* Ana_Set_Reg(AUDTOP_CON3, 0x0200, 0x00000200); */
		/* Ana_Set_Reg(AUDTOP_CON1, 0x0040, 0x000000f0); */
	} else if (ucontrol->value.integer.value[0] == 2) {
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_R, AUDIO_ANALOG_MUX_IN_MIC2);

		/* Ana_Set_Reg(AUDTOP_CON1, 0x0010, 0x000000f0); */
	} else if (ucontrol->value.integer.value[0] == 3) {
		/* Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0); */
		AnalogSetMux(AUDIO_ANALOG_DEVICE_IN_PREAMP_R, AUDIO_ANALOG_MUX_IN_MIC3);
	} else {
		/* Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0); */
	}
	pr_debug("AudCodec %s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
			  mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPL];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	pr_debug("AudCodec %s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	/* Ana_Set_Reg(AUDTOP_CON0, index << 4, 0x00000070); */
	Ana_Set_Reg(AUDPREAMPGAIN_CON0, index << 0, 0x00000007);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("AudCodec %s() = %d\n", __func__,
		  mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPR]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPR];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	pr_debug("AudCodec %s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("AudCodec return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	/* Ana_Set_Reg(AUDTOP_CON1, index << 8, 0x00000700); */
	Ana_Set_Reg(AUDPREAMPGAIN_CON0, index << 4, 0x00000070);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMPR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static const struct snd_kcontrol_new mt6397_UL_Codec_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get, Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get, Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[2], Audio_PreAmp1_Get,
	Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[3], Audio_PreAmp2_Get,
	Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[4], Audio_PGA1_Get, Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[5], Audio_PGA2_Get, Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_Digital_Mic_Switch", Audio_UL_Enum[6], Audio_DMIC_Get, Audio_DMIC_Set),
};

/*
static int hpl_dac_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
    struct snd_soc_codec *codec = w->codec;
    pr_notice("AudCodec hpl_dac_event event = %d\n", event);

    pr_notice(codec->dev, "%s %s %d\n", __func__, w->name, event);

    switch (event)
    {
	case SND_SOC_DAPM_PRE_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMU", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMU", __func__);
	    break;
	case SND_SOC_DAPM_PRE_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMD", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMD", __func__);
	    break;
	case SND_SOC_DAPM_PRE_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_REG", __func__);
	    break;
	case SND_SOC_DAPM_POST_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_REG", __func__);
	    break;
    }
    return 0;
}

static int hpr_dac_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
    struct snd_soc_codec *codec = w->codec;
    pr_notice("AudCodec hpr_dac_event event = %d\n", event);

    pr_notice(codec->dev, "%s %s %d\n", __func__, w->name, event);

    switch (event)
    {
	case SND_SOC_DAPM_PRE_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMU", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMU", __func__);
	    break;
	case SND_SOC_DAPM_PRE_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMD", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMD", __func__);
	    break;
	case SND_SOC_DAPM_PRE_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_REG", __func__);
	    break;
	case SND_SOC_DAPM_POST_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_REG", __func__);
	    break;
    }
    return 0;
}

static int spk_amp_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
    struct snd_soc_codec *codec = w->codec;
    pr_notice("AudCodec spk_amp_event event = %d\n", event);
    switch (event)
    {
	case SND_SOC_DAPM_PRE_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMU", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMU", __func__);
	    break;
	case SND_SOC_DAPM_PRE_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMD", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMD", __func__);
	    break;
    }
    return 0;
}

static void speaker_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
    //struct snd_soc_codec *codec = w->codec;
    pr_notice("AudCodec speaker_event = %d\n", event);
    switch (event)
    {
	case SND_SOC_DAPM_PRE_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMU", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMU", __func__);
	    break;
	case SND_SOC_DAPM_PRE_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMD", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMD", __func__);
	case SND_SOC_DAPM_PRE_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_REG", __func__);
	case SND_SOC_DAPM_POST_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_REG", __func__);
	    break;
    }
}

// The register address is the same as other codec so it can use resmgr
static int codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
    //struct snd_soc_codec *codec = w->codec;
    pr_notice("AudCodec codec_enable_rx_bias = %d\n", event);
    switch (event)
    {
	case SND_SOC_DAPM_PRE_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMU", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMU:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMU", __func__);
	    break;
	case SND_SOC_DAPM_PRE_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_PMD", __func__);
	    break;
	case SND_SOC_DAPM_POST_PMD:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_PMD", __func__);
	case SND_SOC_DAPM_PRE_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_PRE_REG", __func__);
	case SND_SOC_DAPM_POST_REG:
	    pr_notice("AudCodec %s SND_SOC_DAPM_POST_REG", __func__);
	    break;
    }
    return 0;
}
*/

#if 0
static const struct snd_soc_dapm_widget mt6397_dapm_widgets[] = {

	/* Outputs */
	/*
	   SND_SOC_DAPM_OUTPUT("EARPIECE"),
	   SND_SOC_DAPM_OUTPUT("HEADSET"),
	   SND_SOC_DAPM_OUTPUT("SPEAKER"),

	   SND_SOC_DAPM_PGA_E("SPEAKER PGA", SND_SOC_NOPM,
	   0, 0, NULL, 0, speaker_event,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),

	   SND_SOC_DAPM_DAC_E("RX_BIAS", NULL, SND_SOC_NOPM, 0, 0,
	   codec_enable_rx_bias,     SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD
	   SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),

	   SND_SOC_DAPM_MUX_E("VOICE_Mux_E", SND_SOC_NOPM, 0, 0,
	   &mt6397_Voice_Switch, codec_enable_rx_bias,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	 */

	/*
	   SND_SOC_DAPM_MIXER_E("HPL DAC", 0, 0, 0, mt6397_snd_controls,  ARRAY_SIZE(mt6397_snd_controls),
	   hpl_dac_event,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	   SND_SOC_DAPM_MIXER_E("HPR DAC", SND_SOC_NOPM, 0, 0, NULL, 0,
	   hpr_dac_event,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	   SND_SOC_DAPM_MIXER("HPL_DAC_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),
	   SND_SOC_DAPM_MIXER("HPR_DAC_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),
	   SND_SOC_DAPM_MIXER("HPR_VOICE_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),
	   SND_SOC_DAPM_MIXER("HPR_SPEAKER_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),

	   SND_SOC_DAPM_MIXER_E("HPL_PA_MIXER", 4, 0, 0, NULL, 0, hpl_dac_event,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	   SND_SOC_DAPM_MIXER("HPR_PA_MIXER", SND_SOC_NOPM, 0, 0, NULL, 0),

	   SND_SOC_DAPM_DAC_E("DAC1R", MT_SOC_DL1_STREAM_NAME , 0, 0, 0, hpl_dac_event,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
	   SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	 */

};

static const struct snd_soc_dapm_route mtk_audio_map[] = {
	/* {"HPL DAC", "Audio_Amp_L_Switch", "HPL_PA_MIXER"}, */
	/* {"HPR DAC", "Audio_Amp_R_Switch", "HPR_PA_MIXER"}, */
	/* {"HPL DAC", "Audio_Amp_L_Switch", "HPL_PA_MIXER"}, */
	/* {"SPEAKER PGA", NULL ,"SPEAKER"}, */
	/* {"SPEAKER", NULL , "SPEAKER PGA"}, */
	/*
	   {"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
	 */
};
#endif

static void mt6397_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_debug("AudCodec mt6397_codec_init_reg\n");

	/* power_init */
	upmu_set_rg_clksq_en(0);
	/* [11]: RG_VA28REFGEN_EN_VA28 */
	/* Ana_Set_Reg(AUDLDO_CFG0, 0x0192 , 0xffffffff); */
	/* [2]: RG_ACC_DCC_SEL_AUDGLB_VA28, [1]: RG_AUDGLB_PWRDN_VA28 */
	/* Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffffffff); */
	/* [15]: RG_NCP_REMOTE_SENS_VA18, [12]: DA_LCLDO_ENC_EN_VA28 */
	/* Ana_Set_Reg(AUD_NCP0, 0x8000, 0xffffffff); */

	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0080, 0x0080);
	Ana_Set_Reg(ZCD_CON2, 0x0c0c, 0xffff);
	Ana_Set_Reg(AUDBUF_CFG0, 0x0000, 0x1fe7);
	Ana_Set_Reg(IBIASDIST_CFG0, 0x1552, 0xffff);	/* RG DEV ck off; */
	Ana_Set_Reg(AUDDAC_CON0, 0x0000, 0xffff);	/* NCP off */
	Ana_Set_Reg(AUDCLKGEN_CFG0, 0x0000, 0x0001);
	Ana_Set_Reg(AUDNVREGGLB_CFG0, 0x0006, 0xffff);	/* need check */
	Ana_Set_Reg(NCP_CLKDIV_CON1, 0x0001, 0xffff);	/* fix me */
	Ana_Set_Reg(AUD_NCP0, 0x0000, 0x6000);
	Ana_Set_Reg(AUDLDO_CFG0, 0x0192, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0x0080);
	Ana_Set_Reg(ZCD_CON0, 0x0101, 0xffff);	/* ZCD setting gain step gain and enable */
}

static ssize_t show_spk_pga_adj(struct device *d,
				   struct device_attribute *attr,
				   char *buf)
{
	ssize_t res;
	mutex_lock(&Ana_Ctrl_Mutex);
	res = snprintf(buf, PAGE_SIZE, "%d\n", mCodec_data->spk_pga_adj);
	mutex_unlock(&Ana_Ctrl_Mutex);

	return res;
}

static ssize_t store_spk_pga_adj(struct device *d,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	ssize_t res = len;
	int pga_adj;

	mutex_lock(&Ana_Ctrl_Mutex);

	if (kstrtoint(buf, 10, &pga_adj))
		res = -EINVAL;
	else {
		mCodec_data->spk_pga_adj += pga_adj;

		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] =
			clamp(mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] + pga_adj, 0,
				(int)ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1);

		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] =
			clamp(mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] + pga_adj, 0,
				(int)ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1);

		Ana_Set_Reg(SPK_CON5, mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] << 11, 0x00007800);
		Ana_Set_Reg(SPK_CON9, mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] << 8, 0x00000f00);
	}

	mutex_unlock(&Ana_Ctrl_Mutex);

	return res;
}

static DEVICE_ATTR(spk_pga_adj, S_IRWXU,
		show_spk_pga_adj, store_spk_pga_adj);

static struct attribute *spk_pga_adj_attrs[] = {
	&dev_attr_spk_pga_adj.attr,
	NULL
};

static struct attribute_group spk_pga_adj_attr_group = {
	.name	= "mt6397",
	.attrs	= spk_pga_adj_attrs,
};

static int mt6397_codec_probe(struct snd_soc_codec *codec)
{
	pr_info("%s()\n", __func__);
	mt6397_codec_init_reg(codec);

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6397_snd_controls, ARRAY_SIZE(mt6397_snd_controls));
	snd_soc_add_codec_controls(codec, mt6397_UL_Codec_controls,
							   ARRAY_SIZE(mt6397_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6397_factory_controls,
							   ARRAY_SIZE(mt6397_factory_controls));
	/* snd_soc_add_codec_controls(codec, mt6397_Voice_Switch, */
	/* ARRAY_SIZE(mt6397_Voice_Switch)); */

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(struct mt6397_Codec_Data_Priv), GFP_KERNEL);
	if (unlikely(!mCodec_data)) {
		pr_err("Failed to allocate private data\n");
		return -ENOMEM;
	}

	snd_soc_codec_set_drvdata(codec, mCodec_data);

	/* TRIM FUNCTION */
	/* AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R, AUDIO_ANALOG_MUX_AUDIO); */
	/* AnalogSetMux(AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_L, AUDIO_ANALOG_MUX_AUDIO); */
	/* Get HP Trim Offset */
	GetTrimOffset();
	SPKAutoTrimOffset();

	if (unlikely(sysfs_create_group(&codec->dev->kobj, &spk_pga_adj_attr_group))) {
		pr_err("Failed to sysfs_create_group\n");
		return -ENOMEM;
	}

	/*
	   Ana_Set_Reg(0x70a  , 0x2000, 0xffff7000);
	   Ana_Set_Reg(0x70a  , 0x200 , 0xffff0700);
	   Ana_Set_Reg(0x64   , 0xa00  , 0xffff0f00);
	   Ana_Set_Reg(0x64   , 0xa00  , 0xffff0f00);
	 */
	return 0;
}

static int mt6397_codec_remove(struct snd_soc_codec *codec)
{
	pr_info("%s()\n", __func__);
	return 0;
}

static unsigned int mt6397_read(struct snd_soc_codec *codec, unsigned int reg)
{
	pr_debug("AudCodec mt6397_read reg = 0x%x", reg);
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6397_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	pr_debug("AudCodec mt6397_write reg = 0x%x  value= 0x%x\n", reg, value);
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static int mt6397_Readable_registers(struct snd_soc_codec *codec, unsigned int reg)
{
	return 1;
}

static int mt6397_volatile_registers(struct snd_soc_codec *codec, unsigned int reg)
{
	return 1;
}

static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6397_codec_probe,
	.remove = mt6397_codec_remove,

	.read = mt6397_read,
	.write = mt6397_write,

	.readable_register = mt6397_Readable_registers,
	.volatile_register = mt6397_volatile_registers,

	/* .controls = mt6397_snd_controls, */
	/* .num_controls = ARRAY_SIZE(mt6397_snd_controls), */
	/*.dapm_widgets = mt6397_dapm_widgets,*/
	/*.num_dapm_widgets = ARRAY_SIZE(mt6397_dapm_widgets),*/
	/*.dapm_routes = mtk_audio_map,*/
	/*.num_dapm_routes = ARRAY_SIZE(mtk_audio_map),*/

};

static int mtk_mt6397_codec_dev_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s.%d", MT_SOC_CODEC_NAME, 1);

	pr_notice("%s dev name %s\n", __func__, dev_name(&pdev->dev));

	return snd_soc_register_codec(&pdev->dev, &soc_mtk_codec,
			mtk_codec_dai_drvs, ARRAY_SIZE(mtk_codec_dai_drvs));
}

static int mtk_mt6397_codec_dev_remove(struct platform_device *pdev)
{
	pr_debug("AudCodec %s:\n", __func__);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;

}

static struct platform_driver mtk_pmic_codec_driver = {
	.driver = {
		.name = MT_SOC_CODEC_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mtk_mt6397_codec_dev_probe,
	.remove = mtk_mt6397_codec_dev_remove,
};

module_platform_driver(mtk_pmic_codec_driver);

/* Module information */
MODULE_DESCRIPTION("MTK 6397 codec driver");
MODULE_LICENSE("GPL v2");
