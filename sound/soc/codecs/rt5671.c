/*
 * rt5671.c  --  RT5671 ALSA SoC audio codec driver
 *
 * Copyright 2012 Realtek Semiconductor Corp.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*
#define DEBUG
*/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif
#define REALTEK_USE_AMIC	0
#define RTK_IOCTL
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
#include "rt_codec_ioctl.h"
#include "rt5671_ioctl.h"
#endif
#endif

#include "rt5671.h"
#include "rt5671-dsp.h"

static int pmu_depop_time = 0;
module_param(pmu_depop_time, int, 0644);

static int pmd_depop_time = 20;
module_param(pmd_depop_time, int, 0644);

static int hp_amp_time = 45;
module_param(hp_amp_time, int, 0644);

static int hp_amp_power_on = 1;
module_param(hp_amp_power_on, int, 0644);

static int hp_amp_power_off = 0;
module_param(hp_amp_power_off, int, 0644);

static bool hp_amp_delay_for_hp = true;
module_param(hp_amp_delay_for_hp, bool, 0644);

static bool hp_amp_delay_for_lout = true;
module_param(hp_amp_delay_for_lout, bool, 0644);

static bool dsp_depop_in_dsp_bypass = true;
module_param(dsp_depop_in_dsp_bypass, bool, 0644);

static int dsp_depop_delay_in_dsp_bypass = 150;
module_param(dsp_depop_delay_in_dsp_bypass, int, 0644);

static bool headphone_depop = true;
module_param(headphone_depop, bool, 0644);

static int headphone_depop_delay = 0; //0 is not effect at all, 100ms will be effect.
module_param(headphone_depop_delay, int, 0644);

bool hp_amp_power_up = false;
bool lout_amp_power_up = false;

static int depop_delay_test = 1;
module_param(depop_delay_test, int, 0644);

#define RT5671_DET_EXT_MIC 0
/*#define USE_INT_CLK*/
/*#define ALC_DRC_FUNC*/
#define USE_TDM // RealTek Patch : 0416
#define JD1_FUNC


/* remove this define when merged to SOC
 * #define VAD_CAPTURE_TEST
 */

/* use this define when switching 16khz fs for vad */
/*
#define SWITCH_FS_FOR_VAD
*/
#define VERSION "0.0.4 LG alsa 1.0.25"

struct rt5671_init_reg {
	u8 reg;
	u16 val;
};

static int rt5671_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level);

static struct rt5671_init_reg init_list[] = {
	{RT5671_IN2_CTRL	, 0x0648},	/* Oder 20130321 mic gain increase to 44 dB */
	{RT5671_LOUT1		, 0xc888},	/* For LOUT output (130318 Oder) */
	{RT5671_GEN_CTRL1	, 0xc011},	/* fa[0]=1, fa[3]=0'b disabled MCLK det, fa[15:14]=11'b for pdm */
	{RT5671_ADDA_CLK1	, 0x0000},	/* 73[2] = 1'b */
	{RT5671_PRIV_INDEX	, 0x0014},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x9a8a},
	{RT5671_PRIV_INDEX	, 0x003d},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x3640},
	/*playback*/
/*	{RT5671_STO_DAC_MIXER	, 0x1616},	Dig inf 1 -> Sto DAC mixer -> DACL */
	{RT5671_OUT_L1_MIXER	, 0x0072},	/* DACL1 -> OUTMIXL */
	{RT5671_OUT_R1_MIXER	, 0x00d2},	/* DACR1 -> OUTMIXR */
	{RT5671_LOUT_MIXER	, 0xc000},
	{RT5671_HP_VOL		, 0x8888},	/* OUTMIX -> HPVOL */
	{RT5671_HPO_MIXER	, 0xf00a},	/* Oder 20130321 a00a -> e00a change the path */
/*	{RT5671_HPO_MIXER	, 0xa000},	DAC1 -> HPOLMIX */
	{RT5671_CHARGE_PUMP	, 0x0c00},
/*	{RT5671_I2S1_SDP	, 0xD000},	change IIS1 and IIS2 */
	/*record*/
/*	{RT5671_IN2_CTRL	, 0x5080},	IN1 boost 40db and differential mode */
/*	{RT5671_IN3_IN4_CTRL	, 0x0500},	IN2 boost 40db and signal ended mode */
/*	{RT5671_REC_L2_MIXER	, 0x0077},	Mic3 -> RECMIXL */
/*	{RT5671_REC_R2_MIXER	, 0x0077},	Mic3 -> RECMIXR */
	{RT5671_REC_L2_MIXER	, 0x007d},	/* Mic1 -> RECMIXL */
	{RT5671_REC_R2_MIXER	, 0x007d},	/* Mic1 -> RECMIXR */
	{RT5671_STO1_ADC_MIXER	, 0x3920},	/* ADC -> Sto ADC mixer */
	{RT5671_STO1_ADC_DIG_VOL, 0xafaf},	/* Mute STO1 ADC for depop */
	{RT5671_MONO_ADC_DIG_VOL, 0xafaf},	/* Mute Mono ADC for depop */
	{RT5671_STO2_ADC_DIG_VOL, 0xafaf},	/* Mute STO2 ADC for depop */
/*	{RT5671_MONO_DAC_MIXER	, 0x1414}, */
	{RT5671_PDM_OUT_CTRL	, 0xff00},
#ifdef JD1_FUNC
#if REALTEK_USE_AMIC
	{RT5671_GPIO_CTRL2	, 0x0024}, /* Oder 130618 */
	{RT5671_GPIO_CTRL1	, 0x8000}, /* Oder 130806 */
	{RT5671_IRQ_CTRL1	, 0x0280},
	{RT5671_JD_CTRL3	, 0x0088},
#else
	{RT5671_GPIO_CTRL3	, 0x0100}, /* Oder 130619 set GPIO8 OUTPUT */
	{RT5671_GPIO_CTRL2	, 0x8004},
	{RT5671_GPIO_CTRL1	, 0x8000},
	{RT5671_IRQ_CTRL1	, 0x0000}, /* Disable JD to IRQ function */
/*
               
         
                                                                  
                  
*/
	{RT5671_JD_CTRL3	, 0x0008},
/*              */
#endif
#endif
	{RT5671_ASRC_8		, 0x0123},
	{RT5671_IL_CMD3		, 0x001b},
	{RT5671_GEN_CTRL3	, 0x0084},
	{RT5671_IN1_CTRL1	, 0x0021}, /* Enable Manual mode */
	{RT5671_IN1_CTRL2	, 0x08a7}, /* Enable Manual mode */
	{RT5671_GEN_CTRL2	, 0x0031}, /* Enable Manual mode */ // RealTek Patch : 0416
	{RT5671_TDM_CTRL_3	, 0x0101}, /* Add for IF1 DAC TDM selection */ /* oder 140514 */
	{RT5671_PWR_DIG2	, 0x0601}, /* Add for preventing pop noise from I2S4 playback */
};
#define RT5671_INIT_REG_LEN ARRAY_SIZE(init_list)

static struct rt5671_init_reg init_list_RevI[] = {
	{RT5671_IN2_CTRL	, 0x0648},	/* Oder 20130321 mic gain increase to 44 dB */
	{RT5671_LOUT1		, 0xc888},	/* For LOUT output (130318 Oder) */
	{RT5671_GEN_CTRL1	, 0xc011},	/* fa[0]=1, fa[3]=0'b disabled MCLK det, fa[15:14]=11'b for pdm */
	{RT5671_ADDA_CLK1	, 0x0000},	/* 73[2] = 1'b */
	{RT5671_PRIV_INDEX	, 0x0014},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x9a8a},
	{RT5671_PRIV_INDEX	, 0x003d},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x3640},
	/*playback*/
/*	{RT5671_STO_DAC_MIXER	, 0x1616},	Dig inf 1 -> Sto DAC mixer -> DACL */
	{RT5671_OUT_L1_MIXER	, 0x0072},	/* DACL1 -> OUTMIXL */
	{RT5671_OUT_R1_MIXER	, 0x00d2},	/* DACR1 -> OUTMIXR */
	{RT5671_LOUT_MIXER	, 0xc000},
	{RT5671_HP_VOL		, 0x8888},	/* OUTMIX -> HPVOL */
	{RT5671_HPO_MIXER	, 0xf00a},	/* Oder 20130321 a00a -> e00a change the path */
/*	{RT5671_HPO_MIXER	, 0xa000},	DAC1 -> HPOLMIX */
	{RT5671_CHARGE_PUMP	, 0x0c00},
/*	{RT5671_I2S1_SDP	, 0xD000},	change IIS1 and IIS2 */
	/*record*/
/*	{RT5671_IN2_CTRL	, 0x5080},	IN1 boost 40db and differential mode */
/*	{RT5671_IN3_IN4_CTRL	, 0x0500},	IN2 boost 40db and signal ended mode */
/*	{RT5671_REC_L2_MIXER	, 0x0077},	Mic3 -> RECMIXL */
/*	{RT5671_REC_R2_MIXER	, 0x0077},	Mic3 -> RECMIXR */
	{RT5671_REC_L2_MIXER	, 0x007d},	/* Mic1 -> RECMIXL */
	{RT5671_REC_R2_MIXER	, 0x007d},	/* Mic1 -> RECMIXR */
	{RT5671_STO1_ADC_MIXER	, 0x3920},	/* ADC -> Sto ADC mixer */
	{RT5671_STO1_ADC_DIG_VOL, 0xafaf},	/* Mute STO1 ADC for depop */
/*	{RT5671_MONO_DAC_MIXER	, 0x1414}, */
	{RT5671_PDM_OUT_CTRL	, 0xff00},
#ifdef JD1_FUNC
#if REALTEK_USE_AMIC
	{RT5671_GPIO_CTRL2	, 0x0024}, /* Oder 130618 */
	{RT5671_GPIO_CTRL1	, 0x8000}, /* Oder 130806 */
	{RT5671_IRQ_CTRL1	, 0x0280},
	{RT5671_JD_CTRL3	, 0x0088},
#else
	{RT5671_GPIO_CTRL3	, 0x0100}, /* Oder 130619 set GPIO8 OUTPUT */
	{RT5671_GPIO_CTRL2	, 0x8004},
	{RT5671_GPIO_CTRL1	, 0x8000},
	{RT5671_IRQ_CTRL1	, 0x0000}, /* Disable JD to IRQ function */
	{RT5671_JD_CTRL3	, 0x0088},
#endif
#endif
	{RT5671_ASRC_8		, 0x0123},
	{RT5671_IL_CMD3		, 0x001b},
	{RT5671_GEN_CTRL3	, 0x0084},
	{RT5671_IN1_CTRL1	, 0x0021}, /* Enable Manual mode */
	{RT5671_IN1_CTRL2	, 0x08a7}, /* Enable Manual mode */
	{RT5671_GEN_CTRL2	, 0x0031}, /* Enable Manual mode */
};
#define RT5671_INIT_REG_LEN_RevI ARRAY_SIZE(init_list_RevI)


static struct rt5671_init_reg init_list_RevG[] = {
	{RT5671_IN2_CTRL	, 0x0648},	/* Oder 20130321 mic gain increase to 44 dB */
	{RT5671_LOUT1		, 0xc888},	/* For LOUT output (130318 Oder) */
	{RT5671_GEN_CTRL1	, 0xc011},	/* fa[0]=1, fa[3]=0'b disabled MCLK det, fa[15:14]=11'b for pdm */
	{RT5671_ADDA_CLK1	, 0x0000},	/* 73[2] = 1'b */
	{RT5671_PRIV_INDEX	, 0x0014},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x9a8a},
	{RT5671_PRIV_INDEX	, 0x003d},	/* PR3d[12] = 0'b; PR3d[9] = 1'b */
	{RT5671_PRIV_DATA	, 0x3640},
	/*playback*/
/*	{RT5671_STO_DAC_MIXER	, 0x1616},	Dig inf 1 -> Sto DAC mixer -> DACL */
	{RT5671_OUT_L1_MIXER	, 0x0072},	/* DACL1 -> OUTMIXL */
	{RT5671_OUT_R1_MIXER	, 0x00d2},	/* DACR1 -> OUTMIXR */
	{RT5671_LOUT_MIXER	, 0xc000},
	{RT5671_HP_VOL		, 0x8888},	/* OUTMIX -> HPVOL */
	{RT5671_HPO_MIXER	, 0xf00a},	/* Oder 20130321 a00a -> e00a change the path */
/*	{RT5671_HPO_MIXER	, 0xa000},	DAC1 -> HPOLMIX */
	{RT5671_CHARGE_PUMP	, 0x0c00},
/*	{RT5671_I2S1_SDP	, 0xD000},	change IIS1 and IIS2 */
	/*record*/
/*	{RT5671_IN2_CTRL	, 0x5080},	IN1 boost 40db and differential mode */
/*	{RT5671_IN3_IN4_CTRL	, 0x0500},	IN2 boost 40db and signal ended mode */
/*	{RT5671_REC_L2_MIXER	, 0x0077},	Mic3 -> RECMIXL */
/*	{RT5671_REC_R2_MIXER	, 0x0077},	Mic3 -> RECMIXR */
	{RT5671_REC_L2_MIXER	, 0x007d},	/* Mic1 -> RECMIXL */
	{RT5671_REC_R2_MIXER	, 0x007d},	/* Mic1 -> RECMIXR */
	{RT5671_STO1_ADC_MIXER	, 0x3920},	/* ADC -> Sto ADC mixer */
	{RT5671_STO1_ADC_DIG_VOL, 0xafaf},	/* Mute STO1 ADC for depop */
/*	{RT5671_MONO_DAC_MIXER	, 0x1414}, */
	{RT5671_PDM_OUT_CTRL	, 0xff00},

#ifdef JD1_FUNC
#if REALTEK_USE_AMIC
	{RT5671_GPIO_CTRL2	, 0x0024}, /* Oder 130618 */
	{RT5671_GPIO_CTRL1	, 0x8000}, /* Oder 130806 */
	{RT5671_IRQ_CTRL1	, 0x0280},
	{RT5671_JD_CTRL3	, 0x0088},
#else
	{RT5671_GPIO_CTRL3	, 0x0100}, /* Oder 130619 set GPIO8 OUTPUT */
	{RT5671_GPIO_CTRL2	, 0x8004},
	{RT5671_GPIO_CTRL1	, 0x8000},
	{RT5671_IRQ_CTRL1	, 0x0280}, /* Enable JD to IRQ function */
	{RT5671_JD_CTRL3	, 0x0088},
#endif
#endif
	{RT5671_ASRC_8		, 0x0123},
	{RT5671_IL_CMD3		, 0x001b},
	{RT5671_GEN_CTRL3	, 0x0084},
};
#define RT5671_INIT_REG_LEN_RevG ARRAY_SIZE(init_list_RevG)

#ifdef ALC_DRC_FUNC
static struct rt5671_init_reg alc_drc_list[] = {
	{ RT5671_ALC_DRC_CTRL1	, 0x0000 },
	{ RT5671_ALC_DRC_CTRL2	, 0x0000 },
	{ RT5671_ALC_CTRL_2	, 0x0000 },
	{ RT5671_ALC_CTRL_3	, 0x0000 },
	{ RT5671_ALC_CTRL_4	, 0x0000 },
	{ RT5671_ALC_CTRL_1	, 0x0000 },
};
#define RT5671_ALC_DRC_REG_LEN ARRAY_SIZE(alc_drc_list)
#endif

//XXX Younghyun Jo added start
static bool rt5671_is_revision_g(struct snd_soc_codec *codec)
{
	struct device_node *np = codec->dev->of_node;
	const char *revision = NULL;

	if (of_property_read_string(codec->dev->of_node, "revision", &revision) < 0) {
		pr_err("of_property_read_string failed. propname:revision\n");
		return false;
	}
	else if (strcmp(revision, "RevG") == 0)
		return true;
	else
		return false;
}
//XXX Younghyun Jo added end

static int rt5671_reg_init(struct snd_soc_codec *codec)
{
	int i;
	struct device_node *np = codec->dev->of_node;
	const char *temp_string = np->name;
	int rc, len;

	rc = of_property_read_string(np, "revision", &temp_string);
	pr_debug("revision : %s\n", temp_string);

	if(!strcmp(temp_string, "RevJ")){
		for (i = 0; i < RT5671_INIT_REG_LEN; i++)
			snd_soc_write(codec, init_list[i].reg, init_list[i].val);
	} else if(!strcmp(temp_string, "RevI")){
		for (i = 0; i < RT5671_INIT_REG_LEN_RevI; i++)
			snd_soc_write(codec, init_list_RevI[i].reg, init_list_RevI[i].val);
	} else {
		for (i = 0; i < RT5671_INIT_REG_LEN_RevG; i++)
			snd_soc_write(codec, init_list_RevG[i].reg, init_list_RevG[i].val);
	}
#ifdef ALC_DRC_FUNC
	for (i = 0; i < RT5671_ALC_DRC_REG_LEN; i++)
		snd_soc_write(codec, alc_drc_list[i].reg, alc_drc_list[i].val);
#endif

	return 0;
}

static int rt5671_index_sync(struct snd_soc_codec *codec)
{
	int i;
	struct device_node *np = codec->dev->of_node;
	const char *temp_string = np->name;
	int rc, len;

	rc = of_property_read_string(np, "revision", &temp_string);
	pr_debug("revision : %s\n", temp_string);

	if(!strcmp(temp_string, "RevJ")){
	for (i = 0; i < RT5671_INIT_REG_LEN; i++)
		if (RT5671_PRIV_INDEX == init_list[i].reg ||
			RT5671_PRIV_DATA == init_list[i].reg)
			snd_soc_write(codec, init_list[i].reg,
					init_list[i].val);
	} else if(!strcmp(temp_string, "RevI")){
	for (i = 0; i < RT5671_INIT_REG_LEN_RevI; i++)
		if (RT5671_PRIV_INDEX == init_list_RevI[i].reg ||
			RT5671_PRIV_DATA == init_list_RevI[i].reg)
			snd_soc_write(codec, init_list_RevI[i].reg,
					init_list_RevI[i].val);
	} else {
	for (i = 0; i < RT5671_INIT_REG_LEN_RevG; i++)
		if (RT5671_PRIV_INDEX == init_list_RevG[i].reg ||
			RT5671_PRIV_DATA == init_list_RevG[i].reg)
			snd_soc_write(codec, init_list_RevG[i].reg,
					init_list_RevG[i].val);
	}

	return 0;
}

static const u16 rt5671_reg[RT5671_VENDOR_ID2 + 1] = {
	[RT5671_HP_VOL] = 0x8888,
	[RT5671_LOUT1] = 0x8888,
	[RT5671_MONO_OUT] = 0x8800,
	[RT5671_IN1_CTRL1] = 0x0001,
	[RT5671_IN1_CTRL2] = 0x0827,
	[RT5671_IN2_CTRL] = 0x0008,
	[RT5671_INL1_INR1_VOL] = 0x0808,
	[RT5671_SIDETONE_CTRL] = 0x018b,
	[RT5671_DAC1_DIG_VOL] = 0xafaf,
	[RT5671_DAC2_DIG_VOL] = 0xafaf,
	[RT5671_DAC_CTRL] = 0x0011,
	[RT5671_STO1_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_MONO_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_STO2_ADC_DIG_VOL] = 0x2f2f,
	[RT5671_STO2_ADC_MIXER] = 0x7860,
	[RT5671_STO1_ADC_MIXER] = 0x7860,
	[RT5671_MONO_ADC_MIXER] = 0x7871,
	[RT5671_AD_DA_MIXER] = 0x8080,
	[RT5671_STO_DAC_MIXER] = 0x5656,
	[RT5671_MONO_DAC_MIXER] = 0x5454,
	[RT5671_DIG_MIXER] = 0xaaa0,
	[RT5671_DSP_VOL_CTRL] = 0x2f2f,
	[RT5671_DIG_INF1_DATA] = 0x1002,
	[RT5671_PDM_OUT_CTRL] = 0x5f00,
	[RT5671_REC_L2_MIXER] = 0x007f,
	[RT5671_REC_R2_MIXER] = 0x007f,
	[RT5671_REC_MONO2_MIXER] = 0x001f,
	[RT5671_HPO_MIXER] = 0xe00f,
	[RT5671_MONO_MIXER] = 0x5380,
	[RT5671_OUT_L1_MIXER] = 0x0073,
	[RT5671_OUT_R1_MIXER] = 0x00d3,
	[RT5671_LOUT_MIXER] = 0xf0f0,
	[RT5671_PWR_DIG2] = 0x0001,
	[RT5671_PWR_ANLG1] = 0x00c3,
	[RT5671_I2S4_SDP] = 0x8000,
	[RT5671_I2S1_SDP] = 0x8000,
	[RT5671_I2S2_SDP] = 0x8000,
	[RT5671_I2S3_SDP] = 0x8000,
	[RT5671_ADDA_CLK1] = 0x7770,
	[RT5671_ADDA_HPF] = 0x0e00,
	[RT5671_DMIC_CTRL1] = 0x1505,
	[RT5671_DMIC_CTRL2] = 0x0015,
	[RT5671_TDM_CTRL_1] = 0x0c00,
	[RT5671_TDM_CTRL_2] = 0x4000,
	[RT5671_TDM_CTRL_3] = 0x0123,
	[RT5671_DSP_CLK] = 0x1100,
	[RT5671_ASRC_4] = 0x0008,
	[RT5671_ASRC_10] = 0x0003,
	[RT5671_DEPOP_M1] = 0x0004,
	[RT5671_DEPOP_M2] = 0x1100,
	[RT5671_DEPOP_M3] = 0x0646,
	[RT5671_CHARGE_PUMP] = 0x0c06,
	[RT5671_VAD_CTRL1] = 0x2184,
	[RT5671_VAD_CTRL2] = 0x010a,
	[RT5671_VAD_CTRL3] = 0x0aea,
	[RT5671_VAD_CTRL4] = 0x000c,
	[RT5671_VAD_CTRL5] = 0x0400,
	[RT5671_ADC_EQ_CTRL1] = 0x7000,
	[RT5671_EQ_CTRL1] = 0x7000,
	[RT5671_ALC_DRC_CTRL2] = 0x001f,
	[RT5671_ALC_CTRL_1] = 0x220c,
	[RT5671_ALC_CTRL_2] = 0x1f00,
	[RT5671_BASE_BACK] = 0x1813,
	[RT5671_MP3_PLUS1] = 0x0690,
	[RT5671_MP3_PLUS2] = 0x1c17,
	[RT5671_ADC_STO1_HP_CTRL1] = 0xa220,
	[RT5671_HP_CALIB_AMP_DET] = 0x0400,
	[RT5671_SV_ZCD1] = 0x0809,
	[RT5671_IL_CMD1] = 0x0001,
	[RT5671_IL_CMD2] = 0x0049,
	[RT5671_IL_CMD3] = 0x0024,
	[RT5671_DRC_HL_CTRL1] = 0x8000,
	[RT5671_ADC_MONO_HP_CTRL1] = 0xa200,
	[RT5671_ADC_STO2_HP_CTRL1] = 0xa200,
	[RT5671_GEN_CTRL1] = 0x8010,
	[RT5671_GEN_CTRL2] = 0x0033,
	[RT5671_GEN_CTRL3] = 0x0080,
};

static int rt5671_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5671_RESET, 0);
}

/**
 * rt5671_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * rt5671_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int rt5671_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, RT5671_PRIV_DATA);
}

/**
 * rt5671_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int rt5671_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	int change, ret;

	ret = rt5671_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	if (change) {
		ret = rt5671_index_write(codec, reg, new);
		if (ret < 0) {
			dev_err(codec->dev,
				"Failed to write private reg: %d\n", ret);
			goto err;
		}
	}
	return change;

err:
	return ret;
}

static unsigned rt5671_pdm1_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PDM1_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x0200);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5671_PDM_DATA_CTRL1);
	} while (ret & 0x0100);
	return snd_soc_read(codec, RT5671_PDM1_DATA_CTRL4);
err:
	return ret;
}

static int rt5671_pdm1_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PDM1_DATA_CTRL3, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM1_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x0600);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x3600);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5671_PDM_DATA_CTRL1);
	} while (ret & 0x0100);
	return 0;

err:
	return ret;
}

static unsigned rt5671_pdm2_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PDM2_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x0002);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5671_PDM_DATA_CTRL1);
	} while (ret & 0x0001);
	return snd_soc_read(codec, RT5671_PDM2_DATA_CTRL4);
err:
	return ret;
}

static int rt5671_pdm2_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5671_PDM2_DATA_CTRL3, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM2_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x0006);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_PDM_DATA_CTRL1, 0x0036);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5671_PDM_DATA_CTRL1);
	} while (ret & 0x0001);

	return 0;

err:
	return ret;
}


static int rt5671_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5671_RESET:
	case RT5671_PDM_DATA_CTRL1:
	case RT5671_PDM1_DATA_CTRL4:
	case RT5671_PDM2_DATA_CTRL4:
	case RT5671_PRIV_DATA:
	case RT5671_IN1_CTRL1:
	case RT5671_IN1_CTRL2:
	case RT5671_IN1_CTRL3:
	case RT5671_A_JD_CTRL1:
	case RT5671_A_JD_CTRL2:
	case RT5671_VAD_CTRL5:
	case RT5671_ADC_EQ_CTRL1:
	case RT5671_EQ_CTRL1:
	case RT5671_ALC_CTRL_1:
	case RT5671_IRQ_CTRL1:
	case RT5671_IRQ_CTRL2:
	case RT5671_IRQ_CTRL3:
	case RT5671_IL_CMD1:
	case RT5671_DSP_CTRL1:
	case RT5671_DSP_CTRL2:
	case RT5671_DSP_CTRL3:
	case RT5671_DSP_CTRL4:
	case RT5671_DSP_CTRL5:
	case RT5671_VENDOR_ID:
	case RT5671_VENDOR_ID1:
	case RT5671_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

static int rt5671_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5671_RESET:
	case RT5671_HP_VOL:
	case RT5671_LOUT1:
	case RT5671_MONO_OUT:
	case RT5671_IN1_CTRL1:
	case RT5671_IN1_CTRL2:
	case RT5671_IN1_CTRL3:
	case RT5671_IN2_CTRL:
	case RT5671_IN3_IN4_CTRL:
	case RT5671_INL1_INR1_VOL:
	case RT5671_SIDETONE_CTRL:
	case RT5671_DAC1_DIG_VOL:
	case RT5671_DAC2_DIG_VOL:
	case RT5671_DAC_CTRL:
	case RT5671_STO1_ADC_DIG_VOL:
	case RT5671_MONO_ADC_DIG_VOL:
	case RT5671_STO2_ADC_DIG_VOL:
	case RT5671_ADC_BST_VOL1:
	case RT5671_ADC_BST_VOL2:
	case RT5671_STO2_ADC_MIXER:
	case RT5671_STO1_ADC_MIXER:
	case RT5671_MONO_ADC_MIXER:
	case RT5671_AD_DA_MIXER:
	case RT5671_STO_DAC_MIXER:
	case RT5671_MONO_DAC_MIXER:
	case RT5671_DIG_MIXER:
	case RT5671_DSP_PATH1:
	case RT5671_DSP_VOL_CTRL:
	case RT5671_DIG_INF1_DATA:
	case RT5671_DIG_INF2_DATA:
	case RT5671_PDM_OUT_CTRL:
	case RT5671_PDM_DATA_CTRL1:
	case RT5671_PDM1_DATA_CTRL2:
	case RT5671_PDM1_DATA_CTRL3:
	case RT5671_PDM1_DATA_CTRL4:
	case RT5671_PDM2_DATA_CTRL2:
	case RT5671_PDM2_DATA_CTRL3:
	case RT5671_PDM2_DATA_CTRL4:
	case RT5671_REC_L1_MIXER:
	case RT5671_REC_L2_MIXER:
	case RT5671_REC_R1_MIXER:
	case RT5671_REC_R2_MIXER:
	case RT5671_REC_MONO1_MIXER:
	case RT5671_REC_MONO2_MIXER:
	case RT5671_HPO_MIXER:
	case RT5671_MONO_MIXER:
	case RT5671_OUT_L1_MIXER:
	case RT5671_OUT_R1_MIXER:
	case RT5671_LOUT_MIXER:
	case RT5671_PWR_DIG1:
	case RT5671_PWR_DIG2:
	case RT5671_PWR_ANLG1:
	case RT5671_PWR_ANLG2:
	case RT5671_PWR_MIXER:
	case RT5671_PWR_VOL:
	case RT5671_PRIV_INDEX:
	case RT5671_PRIV_DATA:
	case RT5671_I2S4_SDP:
	case RT5671_I2S1_SDP:
	case RT5671_I2S2_SDP:
	case RT5671_I2S3_SDP:
	case RT5671_ADDA_CLK1:
	case RT5671_ADDA_HPF:
	case RT5671_DMIC_CTRL1:
	case RT5671_DMIC_CTRL2:
	case RT5671_TDM_CTRL_1:
	case RT5671_TDM_CTRL_2:
	case RT5671_TDM_CTRL_3:
	case RT5671_DSP_CLK:
	case RT5671_GLB_CLK:
	case RT5671_PLL_CTRL1:
	case RT5671_PLL_CTRL2:
	case RT5671_ASRC_1:
	case RT5671_ASRC_2:
	case RT5671_ASRC_3:
	case RT5671_ASRC_4:
	case RT5671_ASRC_5:
	case RT5671_ASRC_7:
	case RT5671_ASRC_8:
	case RT5671_ASRC_9:
	case RT5671_ASRC_10:
	case RT5671_ASRC_11:
	case RT5671_ASRC_12:
	case RT5671_ASRC_13:
	case RT5671_ASRC_14:
	case RT5671_DEPOP_M1:
	case RT5671_DEPOP_M2:
	case RT5671_DEPOP_M3:
	case RT5671_CHARGE_PUMP:
	case RT5671_MICBIAS:
	case RT5671_A_JD_CTRL1:
	case RT5671_A_JD_CTRL2:
	case RT5671_VAD_CTRL1:
	case RT5671_VAD_CTRL2:
	case RT5671_VAD_CTRL3:
	case RT5671_VAD_CTRL4:
	case RT5671_VAD_CTRL5:
	case RT5671_ADC_EQ_CTRL1:
	case RT5671_ADC_EQ_CTRL2:
	case RT5671_EQ_CTRL1:
	case RT5671_EQ_CTRL2:
	case RT5671_ALC_DRC_CTRL1:
	case RT5671_ALC_DRC_CTRL2:
	case RT5671_ALC_CTRL_1:
	case RT5671_ALC_CTRL_2:
	case RT5671_ALC_CTRL_3:
	case RT5671_JD_CTRL1:
	case RT5671_JD_CTRL2:
	case RT5671_IRQ_CTRL1:
	case RT5671_IRQ_CTRL2:
	case RT5671_IRQ_CTRL3:
	case RT5671_GPIO_CTRL1:
	case RT5671_GPIO_CTRL2:
	case RT5671_GPIO_CTRL3:
	case RT5671_SCRABBLE_FUN:
	case RT5671_SCRABBLE_CTRL:
	case RT5671_BASE_BACK:
	case RT5671_MP3_PLUS1:
	case RT5671_MP3_PLUS2:
	case RT5671_ADC_STO1_HP_CTRL1:
	case RT5671_ADC_STO1_HP_CTRL2:
	case RT5671_HP_CALIB_AMP_DET:
	case RT5671_SV_ZCD1:
	case RT5671_SV_ZCD2:
	case RT5671_IL_CMD1:
	case RT5671_IL_CMD2:
	case RT5671_IL_CMD3:
	case RT5671_DRC_HL_CTRL1:
	case RT5671_DRC_HL_CTRL2:
	case RT5671_ADC_MONO_HP_CTRL1:
	case RT5671_ADC_MONO_HP_CTRL2:
	case RT5671_ADC_STO2_HP_CTRL1:
	case RT5671_ADC_STO2_HP_CTRL2:
	case RT5671_JD_CTRL3:
	case RT5671_JD_CTRL4:
	case RT5671_GEN_CTRL1:
	case RT5671_DSP_CTRL1:
	case RT5671_DSP_CTRL2:
	case RT5671_DSP_CTRL3:
	case RT5671_DSP_CTRL4:
	case RT5671_DSP_CTRL5:
	case RT5671_GEN_CTRL2:
	case RT5671_GEN_CTRL3:
	case RT5671_VENDOR_ID:
	case RT5671_VENDOR_ID1:
	case RT5671_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

/**
 * rt5671_headset_detect - Detect headset.
 * @codec: SoC audio codec device.
 * @jack_insert: Jack insert or not.
 *
 * Detect whether is headset or not when jack inserted.
 *
 * Returns detect status.
 */

int rt5671_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
	int jack_type, val, reg61, reg63, reg64, regfa, reg80;

	if(jack_insert) {
		snd_soc_update_bits(codec, RT5671_GEN_CTRL3, 0x4, 0x0);
		snd_soc_update_bits(codec, RT5671_IN1_CTRL2,
			RT5671_CBJ_DET_MODE | RT5671_CBJ_MN_JD,
			RT5671_CBJ_MN_JD);
		reg63 = snd_soc_read(codec, RT5671_PWR_ANLG1);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
			RT5671_PWR_MB | RT5671_LDO_SEL_MASK,
			RT5671_PWR_MB | 0x3);
		reg64 = snd_soc_read(codec, RT5671_PWR_ANLG2);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_JD1, RT5671_PWR_JD1);
		snd_soc_update_bits(codec, RT5671_PWR_VOL,
			RT5671_PWR_MIC_DET, RT5671_PWR_MIC_DET);
		regfa = snd_soc_read(codec, RT5671_GEN_CTRL1);
		snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x1);
/*
		snd_soc_write(codec, RT5671_GPIO_CTRL2, 0x0004);
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP1_PIN_MASK, RT5671_GP1_PIN_IRQ);
		reg61 = snd_soc_read(codec, RT5671_PWR_DIG1);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_DAC_L1 | RT5671_PWR_DAC_R1,
			RT5671_PWR_DAC_L1 | RT5671_PWR_DAC_R1);
*/
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL1, 0x80, 0x80);
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level) {
			reg80 = snd_soc_read(codec, RT5671_GLB_CLK);
//XXX Younghyun Jo added start
#if 0
			snd_soc_update_bits(codec, RT5671_GLB_CLK,
				RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_RCCLK);
#endif
//XXX Younghyun Jo added end
		}
		snd_soc_update_bits(codec, RT5671_IN1_CTRL1,
			RT5671_CBJ_BST1_EN, RT5671_CBJ_BST1_EN);
		snd_soc_write(codec, RT5671_JD_CTRL3, 0x00f0);
		snd_soc_update_bits(codec, RT5671_IN1_CTRL2,
			RT5671_CBJ_MN_JD, RT5671_CBJ_MN_JD);
		snd_soc_update_bits(codec, RT5671_IN1_CTRL2,
			RT5671_CBJ_MN_JD, 0);
//XXX Younghyun Jo added start
		msleep(300);
//XXX Younghyun Jo added end
		val = snd_soc_read(codec, RT5671_IN1_CTRL3) & 0x7;
		pr_debug("val=%d\n", val);
		if (val == 0x1 || val == 0x2) {
			jack_type = SND_JACK_HEADSET;
			/* for push button */
			reg63 &= 0xfffc;
			reg63 |= (RT5671_PWR_MB | 0x3);
			reg64 |= RT5671_PWR_JD1 | RT5671_PWR_MB2;
			snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x8, 0x8);
			snd_soc_update_bits(codec, RT5671_IL_CMD1, 0x40, 0x40);
			snd_soc_read(codec, RT5671_IL_CMD1);
		} else {
			jack_type = SND_JACK_HEADPHONE;
			snd_soc_update_bits(codec, RT5671_PWR_VOL,
				RT5671_PWR_MIC_DET, 0);
		}

		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level)
			snd_soc_write(codec, RT5671_GLB_CLK, reg80);

		snd_soc_write(codec, RT5671_GEN_CTRL1, regfa);
		snd_soc_write(codec, RT5671_PWR_ANLG2, reg64);
		snd_soc_write(codec, RT5671_PWR_ANLG1, reg63);
		snd_soc_update_bits(codec, RT5671_GEN_CTRL3, 0x4, 0x4);
	} else {
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x8, 0x0);
		snd_soc_update_bits(codec, RT5671_IN1_CTRL2, RT5671_CBJ_DET_MODE,
			RT5671_CBJ_DET_MODE);
		jack_type = 0;
	}

	pr_debug("jack_type=%d\n", jack_type);
	return jack_type;
}
EXPORT_SYMBOL(rt5671_headset_detect);

int rt5671_button_detect(struct snd_soc_codec *codec)
{
	int btn_type, val;

	snd_soc_update_bits(codec, RT5671_IL_CMD1, 0x40, 0x40);

	val = snd_soc_read(codec, RT5671_IL_CMD1);
	btn_type = val & 0xff80;
	pr_debug("btn_type = 0x%x\n",btn_type);
	snd_soc_write(codec, RT5671_IL_CMD1, val);
	return btn_type;
}
EXPORT_SYMBOL(rt5671_button_detect);

int rt5671_check_interrupt_event(struct snd_soc_codec *codec, int *data)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	int val, jack_type, event_type;

	if (snd_soc_read(codec, 0xbe) & 0x0080)
		return RT5671_VAD_EVENT;

	val = snd_soc_read(codec, RT5671_A_JD_CTRL1) & 0x0070;
	pr_debug("val = 0x%x rt5671->jack_type = 0x%x\n", val, rt5671->jack_type);
	*data = 0;
	switch (val) {
	case 0x30:
	case 0x0:
		/* jack insert */
		if (rt5671->jack_type == 0) {
			rt5671->jack_type = rt5671_headset_detect(codec, 1);
			/* change jd polarity */
			snd_soc_update_bits(codec, RT5671_IRQ_CTRL1,
				0x1 << 7, 0);
			*data = rt5671->jack_type;
			return RT5671_J_IN_EVENT;
		}
		event_type = 0;
		if (snd_soc_read(codec, RT5671_IRQ_CTRL3) & 0x4) {
			/* button event */
			event_type = RT5671_BTN_EVENT;
			*data = rt5671_button_detect(codec);
		}
		if (*data == 0) {
			pr_debug("button release\n");
			event_type = RT5671_BR_EVENT;
		}
		if (*data & 0x2480)
			snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x1, 0x1);
		else
			snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x1, 0x0);
		return (event_type == 0 ? RT5671_UN_EVENT : event_type);
	case 0x70:
	case 0x10:
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL3, 0x1, 0x0);
		/* change jd polarity */
		snd_soc_update_bits(codec, RT5671_IRQ_CTRL1,
			0x1 << 7, 0x1 << 7);

		rt5671->jack_type = rt5671_headset_detect(codec, 0);
		/*
		snd_soc_jack_report(rt5671->pdata.combo_jack,
			rt5671->pdata.report, 0);
		*/
		return RT5671_J_OUT_EVENT;
	default:
		printk("invalid jd type\n");
		return RT5671_UN_EVENT;
	}

	return RT5671_UN_EVENT;

}
EXPORT_SYMBOL(rt5671_check_interrupt_event);

#if REALTEK_USE_AMIC
/* Oder 130618 start */
int rt5671_gpio2_control(struct snd_soc_codec *codec, int on)
{
	printk(KERN_INFO "[%s] spk is %d\n", __func__, on);

	if (on)
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL2, RT5671_GP2_OUT_MASK, RT5671_GP2_OUT_HI);
	else
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL2, RT5671_GP2_OUT_MASK, RT5671_GP2_OUT_LO);

	return 0;
}
EXPORT_SYMBOL(rt5671_gpio2_control);
/* Oder 130618 end */
#else
/* Oder 130619 start */
int rt5671_gpio8_control(struct snd_soc_codec *codec, int on)
{
	if (on)
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL3, 0x1 << 7, 0x1 << 7);
	else
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL3, 0x1 << 7, 0x0 << 7);

	return 0;
}
EXPORT_SYMBOL(rt5671_gpio8_control);
/* Oder 130619 end */
#endif
int rt5671_sel_gpio78910_i2s_pin(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, RT5671_GPIO_CTRL2, 0x1 << 15, 0x1 << 15);

	return 0;
}
EXPORT_SYMBOL(rt5671_sel_gpio78910_i2s_pin);

static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -65625, 375, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_bst_tlv, 0, 1200, 0);
//                                                                  
static const DECLARE_TLV_DB_SCALE(rec_vol_tlv, -18000, 3000, 0);
//             

/* {0, +20, +24, +30, +35, +40, +44, +50, +52} dB */
static unsigned int bst_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(2400, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(3000, 500, 0),
	6, 6, TLV_DB_SCALE_ITEM(4400, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(5000, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(5200, 0, 0),
};

/* IN1/IN2 Input Type */
static const char * const rt5671_input_mode[] = {
	"Single ended", "Differential"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_in2_mode_enum, RT5671_IN2_CTRL,
	RT5671_IN_SFT2, rt5671_input_mode);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_in3_mode_enum, RT5671_IN3_IN4_CTRL,
	RT5671_IN_SFT1, rt5671_input_mode);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_in4_mode_enum, RT5671_IN3_IN4_CTRL,
	RT5671_IN_SFT2, rt5671_input_mode);

/* Interface data select */
static const char * const rt5671_data_select[] = {
	"Normal", "Swap", "left copy to right", "right copy to left"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_if1_adc1_enum, RT5671_TDM_CTRL_1,
				6, rt5671_data_select);
static const SOC_ENUM_SINGLE_DECL(rt5671_if1_adc2_enum, RT5671_TDM_CTRL_1,
				4, rt5671_data_select);
static const SOC_ENUM_SINGLE_DECL(rt5671_if1_adc3_enum, RT5671_TDM_CTRL_1,
				2, rt5671_data_select);
static const SOC_ENUM_SINGLE_DECL(rt5671_if1_adc4_enum, RT5671_TDM_CTRL_1,
				0, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if2_dac_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF2_DAC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if2_adc_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF2_ADC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if3_dac_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF3_DAC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if3_adc_enum, RT5671_DIG_INF1_DATA,
				RT5671_IF3_ADC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if4_dac_enum, RT5671_DIG_INF2_DATA,
				RT5671_IF4_DAC_SEL_SFT, rt5671_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5671_if4_adc_enum, RT5671_DIG_INF2_DATA,
				RT5671_IF4_ADC_SEL_SFT, rt5671_data_select);

static const char * const rt5671_asrc_clk_source[] = {
	"clk_sysy_div_out", "clk_i2s1_track", "clk_i2s2_track",
	"clk_i2s3_track", "clk_i2s4_track", "clk_sys2", "clk_sys3",
	"clk_sys4", "clk_sys5"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_da_sto_asrc_enum, RT5671_ASRC_2,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_da_monol_asrc_enum, RT5671_ASRC_2,
				8, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_da_monor_asrc_enum, RT5671_ASRC_2,
				4, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_sto1_asrc_enum, RT5671_ASRC_2,
				0, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_up_filter_asrc_enum, RT5671_ASRC_3,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_down_filter_asrc_enum, RT5671_ASRC_3,
				8, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_monol_asrc_enum, RT5671_ASRC_3,
				4, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_monor_asrc_enum, RT5671_ASRC_3,
				0, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_ad_sto2_asrc_enum, RT5671_ASRC_10,
				12, rt5671_asrc_clk_source);

static const SOC_ENUM_SINGLE_DECL(rt5671_dsp_asrc_enum, RT5671_DSP_CLK,
				0, rt5671_asrc_clk_source);

static const char * const rt5671_tdm_select[] = {
	"1_2_3_4", "3_4_1_2", "2_1_4_3", "4_3_2_1"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_if1_tdm_enum, RT5671_TDM_CTRL_1,
				8, rt5671_tdm_select);

static const char * const rt5671_adc_hp_coarse_select[] = {
	"8k_12k_16k", "24k_32k", "44.1k_48k", "88.2k_96k", "176.4k_192k"
};

static const SOC_ENUM_DOUBLE_DECL(rt5671_sto1_adc_hp_coarse_select,
	RT5671_ADC_STO1_HP_CTRL1, 12, 8, rt5671_adc_hp_coarse_select);

static const SOC_ENUM_DOUBLE_DECL(rt5671_mono_adc_hp_coarse_select,
	RT5671_ADC_MONO_HP_CTRL1, 12, 8, rt5671_adc_hp_coarse_select);

static const SOC_ENUM_DOUBLE_DECL(rt5671_sto2_adc_hp_coarse_select,
	RT5671_ADC_STO2_HP_CTRL1, 12, 8, rt5671_adc_hp_coarse_select);

/*I2S clock division for tracking mode */
static const char * const rt5671_i2s_track_div[] = {
	"Normal", "Divided by 2", "Divided by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_i2s1_track_div, RT5671_ASRC_8,
	14, rt5671_i2s_track_div);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_i2s2_track_div, RT5671_ASRC_8,
	10, rt5671_i2s_track_div);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_i2s3_track_div, RT5671_ASRC_8,
	6, rt5671_i2s_track_div);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_i2s4_track_div, RT5671_ASRC_8,
	2, rt5671_i2s_track_div);

static int rt5671_vol_rescale_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val = snd_soc_read(codec, mc->reg);

	ucontrol->value.integer.value[0] = RT5671_VOL_RSCL_MAX -
		((val & RT5671_L_VOL_MASK) >> mc->shift);
	ucontrol->value.integer.value[1] = RT5671_VOL_RSCL_MAX -
		(val & RT5671_R_VOL_MASK);

	return 0;
}

static int rt5671_vol_rescale_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val, val2;

	val = RT5671_VOL_RSCL_MAX - ucontrol->value.integer.value[0];
	val2 = RT5671_VOL_RSCL_MAX - ucontrol->value.integer.value[1];
	return snd_soc_update_bits_locked(codec, mc->reg, RT5671_L_VOL_MASK |
			RT5671_R_VOL_MASK, val << mc->shift | val2);
}

static const char *rt5671_vad_mode[] = {
	"Disable", "Enable"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_vad_enum, 0, 0, rt5671_vad_mode);

static int rt5671_vad_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5671->vad_en;

	return 0;
}

static int rt5671_vad_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] == 0) {
		rt5671->vad_en = 0;
		snd_soc_write(codec, RT5671_VAD_CTRL1, 0x2784);
	} else {
		rt5671->vad_en = 1;
	}

	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static const char *rt5671_voice_call_mode[] = {
	"Disable", "Enable"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_voice_call_enum, 0, 0, rt5671_voice_call_mode);

static int voice_call = 0;
static int rt5671_voice_call_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = voice_call;

	return 0;
}

static int rt5671_voice_call_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] == 0) {
		voice_call = 0;
		snd_soc_update_bits(codec, RT5671_ASRC_2, 0x0ff0, 0x0000);
		snd_soc_update_bits(codec, RT5671_ASRC_3, 0x00ff, 0x0000);
	} else {
		voice_call = 1;
		snd_soc_update_bits(codec, RT5671_ASRC_2, 0x0ff0, 0x0550);
		snd_soc_update_bits(codec, RT5671_ASRC_3, 0x00ff, 0x0022);
		snd_soc_update_bits(codec, RT5671_DIG_INF1_DATA, 0x0c00, 0x0800);
	}
	return 0;
}

static const char *rt5671_bt_call_mode[] = {
	"Disable", "Enable"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_bt_call_enum, 0, 0, rt5671_bt_call_mode);

static int bt_call = 0;
static int rt5671_bt_call_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = bt_call;

	return 0;
}

static int rt5671_bt_call_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.integer.value[0] == 0) {
		bt_call = 0;
		snd_soc_update_bits(codec, RT5671_ASRC_2, 0xfff0, 0x0000);
		snd_soc_update_bits(codec, RT5671_ASRC_3, 0x00ff, 0x0000);
		snd_soc_update_bits(codec, RT5671_ASRC_10, 0xf000, 0x0000);
		snd_soc_update_bits(codec, RT5671_TDM_CTRL_1, 0x40c0, 0x4000);
	} else {
		bt_call = 1;
		snd_soc_update_bits(codec, RT5671_ASRC_2, 0xfff0, 0x6550);
		snd_soc_update_bits(codec, RT5671_ASRC_3, 0x00ff, 0x0022);
		snd_soc_update_bits(codec, RT5671_ASRC_10, 0xf000, 0x3000);
		snd_soc_update_bits(codec, RT5671_DIG_INF1_DATA, 0x00c0, 0x0080);
		snd_soc_update_bits(codec, RT5671_TDM_CTRL_1, 0x40c0, 0x40c0);
	}
	return 0;
}

static const char *rt5671_eq_mode[] = {
	"Normal", "Speaker", "Headphone", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_eq_mode_enum, 0, 0, rt5671_eq_mode);

static int rt5671_eq_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5671->eq_mode;

	return 0;
}

static int rt5671_eq_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->eq_mode = ucontrol->value.integer.value[0];

	return 0;
}


#define RT5671_AIF3_SR_NONE	0
#define RT5671_AIF3_SR_8000	1
#define RT5671_AIF3_SR_16000	2

static const char *rt5671_aif3_sr[] = {
	"None", "8K", "16K",
};

static const SOC_ENUM_SINGLE_DECL(rt5671_aif3_sr_enum, 0, 0, rt5671_aif3_sr);


static int rt5671_aif3_sr_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (rt5671->aif3_srate) {
	case 8000:
		ucontrol->value.integer.value[0] = RT5671_AIF3_SR_8000;
		break;
	case 16000:
		ucontrol->value.integer.value[0] = RT5671_AIF3_SR_16000;
		break;
	default:
		ucontrol->value.integer.value[0] = RT5671_AIF3_SR_NONE;
		break;
	}
	return 0;
}

static int rt5671_aif3_sr_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);


	switch (ucontrol->value.integer.value[0]) {
	case 1:
		rt5671->aif3_srate = 8000;
		break;
	case 2:
		rt5671->aif3_srate = 16000;
		break;
	default:
		rt5671->aif3_srate = 0;
		break;
	}
	return 0;
}


static const char *rt5671_adc_eq_mode[] = {
	"Normal", "Type1", "Type2", "Type3"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_adc_eq_mode_enum, 0, 0, rt5671_adc_eq_mode);

static int rt5671_adc_eq_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
		(rt5671->adc_eq_mode == 0) ? rt5671->adc_eq_mode :
		rt5671->adc_eq_mode - HP;

	return 0;
}

static int rt5671_adc_eq_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->adc_eq_mode = (ucontrol->value.integer.value[0] == 0) ?
		ucontrol->value.integer.value[0] :
		ucontrol->value.integer.value[0] + HP;

	return 0;
}

static const char *rt5671_alc_drc_mode[] = {
	"Normal", "Type1", "Type2", "Type3"
};

static const SOC_ENUM_SINGLE_DECL(rt5671_alc_drc_mode_enum, 0, 0,
	rt5671_alc_drc_mode);

static int rt5671_alc_drc_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5671->alc_drc_mode;

	return 0;
}

static int rt5671_alc_drc_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->alc_drc_mode = ucontrol->value.integer.value[0];

	return 0;
}


static int rt5671_mono_adc_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, ">>>>> rt5671->mono_adc_mute : %d TRACE [%s]->(%d) <<<<<\n", rt5671->mono_adc_mute, __FUNCTION__, __LINE__);

	ucontrol->value.integer.value[0] = rt5671->mono_adc_mute;

	return 0;
}

static int rt5671_mono_adc_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->mono_adc_mute = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, ">>>>> rt5671->mono_adc_mute : %d TRACE [%s]->(%d) <<<<<\n", rt5671->mono_adc_mute, __FUNCTION__, __LINE__);

	return 0;
}

static int rt5671_sto2_adc_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, ">>>>> rt5671->sto2_adc_mute : %d TRACE [%s]->(%d) <<<<<\n", rt5671->sto2_adc_mute, __FUNCTION__, __LINE__);

	ucontrol->value.integer.value[0] = rt5671->sto2_adc_mute;

	return 0;
}

static int rt5671_sto2_adc_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->sto2_adc_mute = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, ">>>>> rt5671->sto2_adc_mute : %d TRACE [%s]->(%d) <<<<<\n", rt5671->sto2_adc_mute, __FUNCTION__, __LINE__);

	return 0;
}

static int rt5671_dac2_playback_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.integer.value[0] = rt5671->dac2_playback_mute_l;
    ucontrol->value.integer.value[1] = rt5671->dac2_playback_mute_r;

    dev_dbg(codec->dev, ">>>>> rt5671->dac2_playback_mute_l : %d TRACE [%s]->(%d) <<<<<\n",
	rt5671->dac2_playback_mute_l, __FUNCTION__, __LINE__);
    dev_dbg(codec->dev, ">>>>> rt5671->dac2_playback_mute_r : %d TRACE [%s]->(%d) <<<<<\n",
	rt5671->dac2_playback_mute_r, __FUNCTION__, __LINE__);

	return 0;
}

static int rt5671_dac2_playback_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
    struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

    rt5671->dac2_playback_mute_l = ucontrol->value.integer.value[0];
    rt5671->dac2_playback_mute_r = ucontrol->value.integer.value[1];

    dev_dbg(codec->dev, ">>>>> rt5671->dac2_playback_mute_l : %d TRACE [%s]->(%d) <<<<<\n",
	rt5671->dac2_playback_mute_l, __FUNCTION__, __LINE__);
    dev_dbg(codec->dev, ">>>>> rt5671->dac2_playback_mute_r : %d TRACE [%s]->(%d) <<<<<\n",
    rt5671->dac2_playback_mute_r, __FUNCTION__, __LINE__);

	return 0;
}


static const struct snd_kcontrol_new rt5671_snd_controls[] = {
	/* Headphone Output Volume */
	SOC_DOUBLE("HP Playback Switch", RT5671_HP_VOL,
		RT5671_L_MUTE_SFT, RT5671_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("HP Playback Volume", RT5671_HP_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* OUTPUT Control */
	SOC_DOUBLE("OUT Playback Switch", RT5671_LOUT1,
		RT5671_L_MUTE_SFT, RT5671_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("OUT Playback Volume", RT5671_LOUT1,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* MONO Output Control */
	SOC_SINGLE("Mono Playback Switch", RT5671_MONO_OUT,
		RT5671_L_MUTE_SFT, 1, 1),
	SOC_SINGLE_TLV("Mono Playback Volume", RT5671_MONO_OUT,
		RT5671_L_VOL_SFT, 39, 1, out_vol_tlv),
	/* DAC Digital Mute Status */
	SOC_DOUBLE_EXT("DAC2 Playback Mute", SND_SOC_NOPM,
		0, 1, 1, 0, rt5671_dac2_playback_mute_get, rt5671_dac2_playback_mute_put),
	/* DAC Digital Volume */
	SOC_DOUBLE("DAC2 Playback Switch", RT5671_DAC_CTRL,
		RT5671_M_DAC_L2_VOL_SFT, RT5671_M_DAC_R2_VOL_SFT, 1, 1),
	SOC_DOUBLE_TLV("DAC1 Playback Volume", RT5671_DAC1_DIG_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 175, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("Mono DAC Playback Volume", RT5671_DAC2_DIG_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 175, 0, dac_vol_tlv),
	/* IN1/IN2 Control */
	SOC_SINGLE_TLV("IN1 Boost", RT5671_IN1_CTRL1,
		RT5671_BST_SFT1, 8, 0, bst_tlv),
	SOC_ENUM("IN2 Mode Control", rt5671_in2_mode_enum),
	SOC_SINGLE_TLV("IN2 Boost", RT5671_IN2_CTRL,
		RT5671_BST_SFT2, 8, 0, bst_tlv),
	SOC_ENUM("IN3 Mode Control", rt5671_in3_mode_enum),
	SOC_SINGLE_TLV("IN3 Boost", RT5671_IN3_IN4_CTRL,
		RT5671_BST_SFT1, 8, 0, bst_tlv),
	SOC_ENUM("IN4 Mode Control", rt5671_in4_mode_enum),
	SOC_SINGLE_TLV("IN4 Boost", RT5671_IN3_IN4_CTRL,
		RT5671_BST_SFT2, 8, 0, bst_tlv),
	/* INL/INR Volume Control */
	SOC_DOUBLE_TLV("IN Capture Volume", RT5671_INL1_INR1_VOL,
		RT5671_INL_VOL_SFT, RT5671_INR_VOL_SFT, 31, 1, in_vol_tlv),
	//                                                               
	/* RECMIXL Volume Control */
	SOC_SINGLE_TLV("RECMIXL BST1 Gain", RT5671_REC_L2_MIXER,
		RT5671_G_BST1_RM_L_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXL BST2 Gain", RT5671_REC_L1_MIXER,
		RT5671_G_BST2_RM_L_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXL BST3 Gain", RT5671_REC_L1_MIXER,
		RT5671_G_BST3_RM_L_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXL BST4 Gain", RT5671_REC_L1_MIXER,
		RT5671_G_BST4_RM_L_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXL INL1 Gain", RT5671_REC_L1_MIXER,
		RT5671_G_IN_L_RM_L_SFT, 6, 1, rec_vol_tlv),
	/* RECMIXR Volume Control */
	SOC_SINGLE_TLV("RECMIXR INR1 Gain", RT5671_REC_R1_MIXER,
		RT5671_G_IN_R_RM_R_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXR BST1 Gain", RT5671_REC_R2_MIXER,
		RT5671_G_BST1_RM_R_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXR BST2 Gain", RT5671_REC_R1_MIXER,
		RT5671_G_BST2_RM_R_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXR BST3 Gain", RT5671_REC_R1_MIXER,
		RT5671_G_BST3_RM_R_SFT, 6, 1, rec_vol_tlv),
	SOC_SINGLE_TLV("RECMIXR BST4 Gain", RT5671_REC_R1_MIXER,
		RT5671_G_BST4_RM_R_SFT, 6, 1, rec_vol_tlv),
	/* RECMIXM Volume Control */
	// NOT YET - will not use
	//             

	/* ADC Digital Volume Control */
	SOC_DOUBLE("STO1 ADC Capture Switch", RT5671_STO1_ADC_DIG_VOL,
		RT5671_L_MUTE_SFT, RT5671_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE("Mono ADC Capture Switch", RT5671_MONO_ADC_DIG_VOL,
		RT5671_L_MUTE_SFT, RT5671_R_MUTE_SFT, 1, 1),
	/*
                
                                  
                                    
  */
	SOC_SINGLE_TLV("MonoL ADC Capture Volume", RT5671_MONO_ADC_DIG_VOL,
			RT5671_L_VOL_SFT, 127, 0, adc_vol_tlv),
	SOC_SINGLE_TLV("MonoR ADC Capture Volume", RT5671_MONO_ADC_DIG_VOL,
			RT5671_R_VOL_SFT, 127, 0, adc_vol_tlv),
	/*              */
	SOC_DOUBLE("STO2 ADC Capture Switch", RT5671_STO2_ADC_DIG_VOL,
		RT5671_L_MUTE_SFT, RT5671_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("STO1 ADC Capture Volume", RT5671_STO1_ADC_DIG_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 127, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("Mono ADC Capture Volume", RT5671_MONO_ADC_DIG_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 127, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("STO2 ADC Capture Volume", RT5671_STO2_ADC_DIG_VOL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 127, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("DSP TxDP Volume", RT5671_DSP_VOL_CTRL,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 127, 0, adc_vol_tlv),
	/* ADC Boost Volume Control */
	SOC_DOUBLE_TLV("STO1 ADC Boost Gain", RT5671_ADC_BST_VOL1,
		RT5671_STO1_ADC_L_BST_SFT, RT5671_STO1_ADC_R_BST_SFT, 3, 0,
		adc_bst_tlv),
	SOC_DOUBLE_TLV("Mono ADC Boost Gain", RT5671_ADC_BST_VOL2,
		RT5671_MONO_ADC_L_BST_SFT, RT5671_MONO_ADC_R_BST_SFT, 3, 0,
		adc_bst_tlv),
	SOC_DOUBLE_TLV("STO2 ADC Boost Gain", RT5671_ADC_BST_VOL1,
		RT5671_STO2_ADC_L_BST_SFT, RT5671_STO2_ADC_R_BST_SFT, 3, 0,
		adc_bst_tlv),

	SOC_ENUM("ADC1 IF1 Data Switch", rt5671_if1_adc1_enum),
	SOC_ENUM("ADC2 IF1 Data Switch", rt5671_if1_adc2_enum),
	SOC_ENUM("ADC3 IF1 Data Switch", rt5671_if1_adc3_enum),
	SOC_ENUM("ADC4 IF1 Data Switch", rt5671_if1_adc4_enum),

	SOC_ENUM("ADC IF2 Data Switch", rt5671_if2_adc_enum),
	SOC_ENUM("DAC IF2 Data Switch", rt5671_if2_dac_enum),
	SOC_ENUM("ADC IF3 Data Switch", rt5671_if3_adc_enum),
	SOC_ENUM("DAC IF3 Data Switch", rt5671_if3_dac_enum),
	SOC_ENUM("ADC IF4 Data Switch", rt5671_if4_adc_enum),
	SOC_ENUM("DAC IF4 Data Switch", rt5671_if4_dac_enum),

	SOC_ENUM("DA STO ASRC Switch", rt5671_da_sto_asrc_enum),
	SOC_ENUM("DA MONOL ASRC Switch", rt5671_da_monol_asrc_enum),
	SOC_ENUM("DA MONOR ASRC Switch", rt5671_da_monor_asrc_enum),
	SOC_ENUM("AD STO1 ASRC Switch", rt5671_ad_sto1_asrc_enum),
	SOC_ENUM("AD STO2 ASRC Switch", rt5671_ad_sto2_asrc_enum),
	SOC_ENUM("AD MONOL ASRC Switch", rt5671_ad_monol_asrc_enum),
	SOC_ENUM("AD MONOR ASRC Switch", rt5671_ad_monor_asrc_enum),
	SOC_ENUM("UP ASRC Switch", rt5671_up_filter_asrc_enum),
	SOC_ENUM("DOWN ASRC Switch", rt5671_down_filter_asrc_enum),
	SOC_ENUM("DSP ASRC Switch", rt5671_dsp_asrc_enum),

	SOC_ENUM("IF1 ADC TDM Switch", rt5671_if1_tdm_enum),

	SOC_SINGLE("STO1 ADC Comp Gain", RT5671_ADC_BST_VOL1,
		RT5671_STO1_ADC_COMP_SFT, 3, 0),
	SOC_SINGLE("STO2 ADC Comp Gain", RT5671_ADC_BST_VOL1,
		RT5671_STO2_ADC_COMP_SFT, 3, 0),
	SOC_SINGLE("MONO ADC Comp Gain", RT5671_ADC_BST_VOL2,
		RT5671_MONO_ADC_COMP_SFT, 3, 0),

	SOC_DOUBLE("STO1 ADC HP Filter", RT5671_ADC_STO1_HP_CTRL2,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 63, 0),
	SOC_DOUBLE("STO2 ADC HP Filter", RT5671_ADC_STO2_HP_CTRL2,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 63, 0),
	SOC_DOUBLE("MONO ADC HP Filter", RT5671_ADC_MONO_HP_CTRL2,
		RT5671_L_VOL_SFT, RT5671_R_VOL_SFT, 63, 0),
	SOC_ENUM("STO1 ADC HP Filter Coarse", rt5671_sto1_adc_hp_coarse_select),
	SOC_ENUM("STO2 ADC HP Filter Coarse", rt5671_sto2_adc_hp_coarse_select),
	SOC_ENUM("MONO ADC HP Filter Coarse", rt5671_mono_adc_hp_coarse_select),

	SOC_ENUM_EXT("VAD Switch", rt5671_vad_enum,
		rt5671_vad_get, rt5671_vad_put),
	SOC_ENUM_EXT("Voice Call", rt5671_voice_call_enum,
		rt5671_voice_call_get, rt5671_voice_call_put),
	SOC_ENUM_EXT("BT Call", rt5671_bt_call_enum,
		rt5671_bt_call_get, rt5671_bt_call_put),
	SOC_ENUM_EXT("EQ Mode", rt5671_eq_mode_enum,
		rt5671_eq_mode_get, rt5671_eq_mode_put),
	SOC_ENUM_EXT("ADC EQ Mode", rt5671_adc_eq_mode_enum,
		rt5671_adc_eq_mode_get, rt5671_adc_eq_mode_put),
	SOC_ENUM_EXT("ALC DRC Mode", rt5671_alc_drc_mode_enum,
		rt5671_alc_drc_mode_get, rt5671_alc_drc_mode_put),

	SOC_ENUM("I2S1 Track Division Switch", rt5671_i2s1_track_div),
	SOC_ENUM("I2S2 Track Division Switch", rt5671_i2s2_track_div),
	SOC_ENUM("I2S3 Track Division Switch", rt5671_i2s3_track_div),
	SOC_ENUM("I2S4 Track Division Switch", rt5671_i2s4_track_div),
	SOC_ENUM_EXT("BT Sample Rate", rt5671_aif3_sr_enum,
		rt5671_aif3_sr_get, rt5671_aif3_sr_put),

	//                                                                   
	// do not use this control, this is only for HW tuning Test.
	// leave it as default status (0=-6dB).
	SOC_SINGLE("PDM Gain Select", RT5671_PDM_OUT_CTRL,
		RT5671_PDM_GAIN_SFT, 1, 0),
	//             

	SOC_SINGLE_EXT("Mono ADC Mute", SND_SOC_NOPM, 0, 1, 0,
		rt5671_mono_adc_mute_get, rt5671_mono_adc_mute_put),
	SOC_SINGLE_EXT("Sto2 ADC Mute", SND_SOC_NOPM, 0, 1, 0,
		rt5671_sto2_adc_mute_get, rt5671_sto2_adc_mute_put),
};

/**
 * set_dmic_clk - Set parameter of dmic.
 *
 * @w: DAPM widget.
 * @kcontrol: The kcontrol of this widget.
 * @event: Event id.
 *
 * Choose dmic clock between 1MHz and 3MHz.
 * It is better for clock to approximate 3MHz.
 */
static int set_dmic_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	int div[] = {2, 3, 4, 6, 8, 12}, idx = -EINVAL, i;
	int rate, red, bound, temp;

	rate = rt5671->lrck[rt5671->aif_pu] << 8;
	red = 2000000 * 12;
	for (i = 0; i < ARRAY_SIZE(div); i++) {

#if 0 /* original */
		bound = div[i] * 2000000;
		if (rate > bound)
			continue;
		temp = bound - rate;
		if (temp < red) {
			red = temp;
			idx = i;
		}
#else
		bound = div[i] * 2000000;
		if (rate > bound || div[i]==3)
			continue;

		temp = bound - rate;
		if (temp < red) {
			red = temp;
			idx = i;
		}
#endif
	}
	if (idx < 0)
		dev_err(codec->dev, "Failed to set DMIC clock\n");
	else
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_CLK_MASK, idx << RT5671_DMIC_CLK_SFT);
	return idx;
}

static int check_sysclk1_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5671_GLB_CLK);
	val &= RT5671_SCLK_SRC_MASK;
	if (val == RT5671_SCLK_SRC_PLL1)
		return 1;
	else
		return 0;
}

/* Digital Mixer */
static const struct snd_kcontrol_new rt5671_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO1_ADC_MIXER,
			RT5671_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_STO2_ADC_MIXER,
			RT5671_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5671_MONO_ADC_MIXER,
			RT5671_M_MONO_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5671_AD_DA_MIXER,
			RT5671_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_AD_DA_MIXER,
			RT5671_M_DAC1_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5671_AD_DA_MIXER,
			RT5671_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_AD_DA_MIXER,
			RT5671_M_DAC1_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R1_STO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_ANC_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_R2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1_STO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5671_STO_DAC_MIXER,
			RT5671_M_ANC_DAC_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L1_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5671_SIDETONE_CTRL,
			RT5671_M_ST_DACL2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R1_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5671_SIDETONE_CTRL,
			RT5671_M_ST_DACR2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dig_l_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix L Switch", RT5671_DIG_MIXER,
			RT5671_M_STO_L_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_dig_r_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix R Switch", RT5671_DIG_MIXER,
			RT5671_M_STO_R_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_R_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt5671_rec_l_mix[] = {
	SOC_DAPM_SINGLE("INL Switch", RT5671_REC_L2_MIXER,
			RT5671_M_IN_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST4_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST3_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST2_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_L2_MIXER,
			RT5671_M_BST1_RM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_rec_r_mix[] = {
	SOC_DAPM_SINGLE("INR Switch", RT5671_REC_R2_MIXER,
			RT5671_M_IN_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST4_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST3_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST2_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_R2_MIXER,
			RT5671_M_BST1_RM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_rec_m_mix[] = {
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST4_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST3_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST2_RM_M_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_REC_MONO2_MIXER,
			RT5671_M_BST1_RM_M_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_out_l_mix[] = {
	SOC_DAPM_SINGLE("BST2 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_BST2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_BST1_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_IN_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_DAC_L2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_OUT_L1_MIXER,
			RT5671_M_DAC_L1_OM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_out_r_mix[] = {
	SOC_DAPM_SINGLE("BST3 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_BST3_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_BST4_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_IN_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_DAC_R2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_OUT_R1_MIXER,
			RT5671_M_DAC_R1_OM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_mono_mix[] = {
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_R2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_L2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST4 Switch", RT5671_MONO_MIXER,
			RT5671_M_BST4_MM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpo_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DAC1_HM_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPVOL Switch", RT5671_HPO_MIXER,
			RT5671_M_HPVOL_HM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpvoll_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACL1_HML_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5671_HPO_MIXER,
			RT5671_M_INL1_HML_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_hpvolr_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5671_HPO_MIXER,
			RT5671_M_DACR1_HMR_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5671_HPO_MIXER,
			RT5671_M_INR1_HMR_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_lout_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_LOUT_MIXER,
			RT5671_M_DAC_L1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5671_LOUT_MIXER,
			RT5671_M_DAC_R1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX L Switch", RT5671_LOUT_MIXER,
			RT5671_M_OV_L_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX R Switch", RT5671_LOUT_MIXER,
			RT5671_M_OV_R_LM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5671_monoamp_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5671_MONO_MIXER,
			RT5671_M_DAC_L1_MA_SFT, 1, 1),
	SOC_DAPM_SINGLE("MONOVOL Switch", RT5671_MONO_MIXER,
			RT5671_M_OV_L_MM_SFT, 1, 1),
};

/* DAC1 L/R source */ /* MX-29 [9:8] [11:10] */
static const char * const const rt5671_dac1_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac1l_enum, RT5671_AD_DA_MIXER,
	RT5671_DAC1_L_SEL_SFT, rt5671_dac1_src);

static const struct snd_kcontrol_new rt5671_dac1l_mux =
	SOC_DAPM_ENUM("DAC1 L source", rt5671_dac1l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac1r_enum, RT5671_AD_DA_MIXER,
	RT5671_DAC1_R_SEL_SFT, rt5671_dac1_src);

static const struct snd_kcontrol_new rt5671_dac1r_mux =
	SOC_DAPM_ENUM("DAC1 R source", rt5671_dac1r_enum);

/* DAC2 L/R source */ /* MX-1B [6:4] [2:0] */
static int rt5671_dac12_map_values[] = {
	0, 1, 2, 3, 5, 6,
};
static const char * const const rt5671_dac12_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "TxDC DAC", "VAD_ADC", "IF4 DAC"
};

static const SOC_VALUE_ENUM_SINGLE_DECL(
	rt5671_dac2l_enum, RT5671_DAC_CTRL,
	RT5671_DAC2_L_SEL_SFT, 0x7, rt5671_dac12_src, rt5671_dac12_map_values);

static const struct snd_kcontrol_new rt5671_dac_l2_mux =
	SOC_DAPM_VALUE_ENUM("DAC2 L source", rt5671_dac2l_enum);

static const char * const rt5671_dacr2_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "TxDC DAC", "TxDP ADC", "IF4 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dac2r_enum, RT5671_DAC_CTRL,
	RT5671_DAC2_R_SEL_SFT, rt5671_dacr2_src);

static const struct snd_kcontrol_new rt5671_dac_r2_mux =
	SOC_DAPM_ENUM("DAC2 R source", rt5671_dac2r_enum);

/* RxDP source */ /* MX-2D [15:13] */
static const char * const rt5671_rxdp_src[] = {
	"IF2 DAC", "IF1 DAC2", "STO1 ADC Mixer", "STO2 ADC Mixer",
	"Mono ADC Mixer L", "Mono ADC Mixer R", "DAC1"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_rxdp_enum, RT5671_DSP_PATH1,
	RT5671_RXDP_SEL_SFT, rt5671_rxdp_src);

static const struct snd_kcontrol_new rt5671_rxdp_mux =
	SOC_DAPM_ENUM("DSP RXDP source", rt5671_rxdp_enum);

/* MX-2D [1] [0] */
static const char * const rt5671_dsp_bypass_src[] = {
	"DSP", "Bypass"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dsp_ul_enum, RT5671_DSP_PATH1,
	RT5671_DSP_UL_SFT, rt5671_dsp_bypass_src);

static const struct snd_kcontrol_new rt5671_dsp_ul_mux =
	SOC_DAPM_ENUM("DSP UL source", rt5671_dsp_ul_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_dsp_dl_enum, RT5671_DSP_PATH1,
	RT5671_DSP_DL_SFT, rt5671_dsp_bypass_src);

static const struct snd_kcontrol_new rt5671_dsp_dl_mux =
	SOC_DAPM_ENUM("DSP DL source", rt5671_dsp_dl_enum);


/* INL/R source */
static const char * const rt5671_inl_src[] = {
	"IN2P", "MonoP"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_inl_enum, RT5671_INL1_INR1_VOL,
	RT5671_INL_SEL_SFT, rt5671_inl_src);

static const struct snd_kcontrol_new rt5671_inl_mux =
	SOC_DAPM_ENUM("INL source", rt5671_inl_enum);

static const char * const rt5671_inr_src[] = {
	"IN2N", "MonoN"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_inr_enum, RT5671_INL1_INR1_VOL,
	RT5671_INR_SEL_SFT, rt5671_inr_src);

static const struct snd_kcontrol_new rt5671_inr_mux =
	SOC_DAPM_ENUM("INR source", rt5671_inr_enum);

/* Stereo2 ADC source */
/* MX-26 [15] */
static const char * const rt5671_stereo2_adc_lr_src[] = {
	"L", "LR"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc_lr_enum, RT5671_STO2_ADC_MIXER,
	RT5671_STO2_ADC_SRC_SFT, rt5671_stereo2_adc_lr_src);

static const struct snd_kcontrol_new rt5671_sto2_adc_lr_mux =
	SOC_DAPM_ENUM("Stereo2 ADC LR source", rt5671_stereo2_adc_lr_enum);

/* Stereo1 ADC source */
/* MX-27 MX-26 [12] */
static const char * const rt5671_stereo_adc1_src[] = {
	"DAC MIX", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc1_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_1_SRC_SFT, rt5671_stereo_adc1_src);

static const struct snd_kcontrol_new rt5671_sto_adc1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC1 source", rt5671_stereo1_adc1_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc1_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_1_SRC_SFT, rt5671_stereo_adc1_src);

static const struct snd_kcontrol_new rt5671_sto2_adc1_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1 source", rt5671_stereo2_adc1_enum);

/* MX-27 MX-26 [11] */
static const char * const rt5671_stereo_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc2_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_2_SRC_SFT, rt5671_stereo_adc2_src);

static const struct snd_kcontrol_new rt5671_sto_adc2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC2 source", rt5671_stereo1_adc2_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc2_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_2_SRC_SFT, rt5671_stereo_adc2_src);

static const struct snd_kcontrol_new rt5671_sto2_adc2_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2 source", rt5671_stereo2_adc2_enum);

/* MX-27 MX26 [10] */
static const char * const rt5671_stereo_adc_src[] = {
	"ADC1L ADC2R", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_adc_enum, RT5671_STO1_ADC_MIXER,
	RT5671_ADC_SRC_SFT, rt5671_stereo_adc_src);

static const struct snd_kcontrol_new rt5671_sto_adc_mux =
	SOC_DAPM_ENUM("Stereo1 ADC source", rt5671_stereo1_adc_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_adc_enum, RT5671_STO2_ADC_MIXER,
	RT5671_ADC_SRC_SFT, rt5671_stereo_adc_src);

static const struct snd_kcontrol_new rt5671_sto2_adc_mux =
	SOC_DAPM_ENUM("Stereo2 ADC source", rt5671_stereo2_adc_enum);

/* MX-27 MX-26 [9:8] */
static const char * const rt5671_stereo_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo1_dmic_enum, RT5671_STO1_ADC_MIXER,
	RT5671_DMIC_SRC_SFT, rt5671_stereo_dmic_src);

static const struct snd_kcontrol_new rt5671_sto1_dmic_mux =
	SOC_DAPM_ENUM("Stereo1 DMIC source", rt5671_stereo1_dmic_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo2_dmic_enum, RT5671_STO2_ADC_MIXER,
	RT5671_DMIC_SRC_SFT, rt5671_stereo_dmic_src);

static const struct snd_kcontrol_new rt5671_sto2_dmic_mux =
	SOC_DAPM_ENUM("Stereo2 DMIC source", rt5671_stereo2_dmic_enum);

/* MX-27 [0] */
static const char * const rt5671_stereo_dmic3_src[] = {
	"DMIC3", "PDM ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_stereo_dmic3_enum, RT5671_STO1_ADC_MIXER,
	RT5671_DMIC3_SRC_SFT, rt5671_stereo_dmic3_src);

static const struct snd_kcontrol_new rt5671_sto_dmic3_mux =
	SOC_DAPM_ENUM("Stereo DMIC3 source", rt5671_stereo_dmic3_enum);

/* Mono ADC source */
/* MX-28 [12] */
static const char * const rt5671_mono_adc_l1_src[] = {
	"Mono DAC MIXL", "ADC1 ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l1_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L1_SRC_SFT, rt5671_mono_adc_l1_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l1_mux =
	SOC_DAPM_ENUM("Mono ADC1 left source", rt5671_mono_adc_l1_enum);
/* MX-28 [11] */
static const char * const rt5671_mono_adc_l2_src[] = {
	"Mono DAC MIXL", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l2_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L2_SRC_SFT, rt5671_mono_adc_l2_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l2_mux =
	SOC_DAPM_ENUM("Mono ADC2 left source", rt5671_mono_adc_l2_enum);

/* MX-28 [10] */
static const char * const rt5671_mono_adc_l_src[] = {
	"ADC1", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_l_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_L_SRC_SFT, rt5671_mono_adc_l_src);

static const struct snd_kcontrol_new rt5671_mono_adc_l_mux =
	SOC_DAPM_ENUM("Mono ADC left source", rt5671_mono_adc_l_enum);

/* MX-28 [9:8] */
static const char * const rt5671_mono_dmic_src[] = {
	"DMIC1", "DMIC2", "DMIC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_dmic_l_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_DMIC_L_SRC_SFT, rt5671_mono_dmic_src);

static const struct snd_kcontrol_new rt5671_mono_dmic_l_mux =
	SOC_DAPM_ENUM("Mono DMIC left source", rt5671_mono_dmic_l_enum);
/* MX-28 [1:0] */
static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_dmic_r_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_DMIC_R_SRC_SFT, rt5671_mono_dmic_src);

static const struct snd_kcontrol_new rt5671_mono_dmic_r_mux =
	SOC_DAPM_ENUM("Mono DMIC Right source", rt5671_mono_dmic_r_enum);
/* MX-28 [4] */
static const char * const rt5671_mono_adc_r1_src[] = {
	"Mono DAC MIXR", "ADC2 ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r1_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R1_SRC_SFT, rt5671_mono_adc_r1_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r1_mux =
	SOC_DAPM_ENUM("Mono ADC1 right source", rt5671_mono_adc_r1_enum);
/* MX-28 [3] */
static const char * const rt5671_mono_adc_r2_src[] = {
	"Mono DAC MIXR", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r2_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R2_SRC_SFT, rt5671_mono_adc_r2_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r2_mux =
	SOC_DAPM_ENUM("Mono ADC2 right source", rt5671_mono_adc_r2_enum);

/* MX-28 [2] */
static const char * const rt5671_mono_adc_r_src[] = {
	"ADC2", "ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_mono_adc_r_enum, RT5671_MONO_ADC_MIXER,
	RT5671_MONO_ADC_R_SRC_SFT, rt5671_mono_adc_r_src);

static const struct snd_kcontrol_new rt5671_mono_adc_r_mux =
	SOC_DAPM_ENUM("Mono ADC Right source", rt5671_mono_adc_r_enum);

/* MX-2D [3:2] */
static const char * const rt5671_txdp_slot_src[] = {
	"Slot 0-1", "Slot 2-3", "Slot 4-5", "Slot 6-7"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_txdp_slot_enum, RT5671_DSP_PATH1,
	RT5671_TXDP_SLOT_SEL_SFT, rt5671_txdp_slot_src);

static const struct snd_kcontrol_new rt5671_txdp_slot_mux =
	SOC_DAPM_ENUM("TxDP Slot source", rt5671_txdp_slot_enum);

/* MX-2F [15] */
static const char * const rt5671_if1_adc2_in_src[] = {
	"IF_ADC2", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc2_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF1_ADC2_IN_SFT, rt5671_if1_adc2_in_src);

static const struct snd_kcontrol_new rt5671_if1_adc2_in_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN source", rt5671_if1_adc2_in_enum);

/* MX-2F [14:12] */
static const char * const rt5671_if2_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3", "TxDC_DAC", "TxDP_ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if2_adc_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF2_ADC_IN_SFT, rt5671_if2_adc_in_src);

static const struct snd_kcontrol_new rt5671_if2_adc_in_mux =
	SOC_DAPM_ENUM("IF2 ADC IN source", rt5671_if2_adc_in_enum);

/* MX-2F [2:0] */
static const char * const rt5671_if3_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3", "TxDC_DAC", "TxDP_ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if3_adc_in_enum, RT5671_DIG_INF1_DATA,
	RT5671_IF3_ADC_IN_SFT, rt5671_if3_adc_in_src);

static const struct snd_kcontrol_new rt5671_if3_adc_in_mux =
	SOC_DAPM_ENUM("IF3 ADC IN source", rt5671_if3_adc_in_enum);

/* MX-30 [5:4] */
static const char * const rt5671_if4_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "IF_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if4_adc_in_enum, RT5671_DIG_INF2_DATA,
	RT5671_IF4_ADC_IN_SFT, rt5671_if4_adc_in_src);

static const struct snd_kcontrol_new rt5671_if4_adc_in_mux =
	SOC_DAPM_ENUM("IF4 ADC IN source", rt5671_if4_adc_in_enum);

/* MX-31 [15] [13] [11] [9] */
static const char * const rt5671_pdm_src[] = {
	"Mono DAC", "Stereo DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm1_l_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM1_L_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm1_l_mux =
	SOC_DAPM_ENUM("PDM1 L source", rt5671_pdm1_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm1_r_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM1_R_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm1_r_mux =
	SOC_DAPM_ENUM("PDM1 R source", rt5671_pdm1_r_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm2_l_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM2_L_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm2_l_mux =
	SOC_DAPM_ENUM("PDM2 L source", rt5671_pdm2_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5671_pdm2_r_enum, RT5671_PDM_OUT_CTRL,
	RT5671_PDM2_R_SFT, rt5671_pdm_src);

static const struct snd_kcontrol_new rt5671_pdm2_r_mux =
	SOC_DAPM_ENUM("PDM2 R source", rt5671_pdm2_r_enum);

/* MX-FA [12] */
static const char * const rt5671_if1_adc1_in1_src[] = {
	"IF_ADC1", "IF1_ADC3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc1_in1_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC1_IN1_SFT, rt5671_if1_adc1_in1_src);

static const struct snd_kcontrol_new rt5671_if1_adc1_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN1 source", rt5671_if1_adc1_in1_enum);

/* MX-FA [11] */
static const char * const rt5671_if1_adc1_in2_src[] = {
	"IF1_ADC1_IN1", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc1_in2_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC1_IN2_SFT, rt5671_if1_adc1_in2_src);

static const struct snd_kcontrol_new rt5671_if1_adc1_in2_mux =
	SOC_DAPM_ENUM("IF1 ADC1 IN2 source", rt5671_if1_adc1_in2_enum);

/* MX-FA [10] */
static const char * const rt5671_if1_adc2_in1_src[] = {
	"IF1_ADC2_IN", "IF1_ADC4"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_if1_adc2_in1_enum, RT5671_GEN_CTRL1,
	RT5671_IF1_ADC2_IN1_SFT, rt5671_if1_adc2_in1_src);

static const struct snd_kcontrol_new rt5671_if1_adc2_in1_mux =
	SOC_DAPM_ENUM("IF1 ADC2 IN1 source", rt5671_if1_adc2_in1_enum);

/* MX-18 [11:9] */
static const char * const rt5671_sidetone_src[] = {
	"DMIC L1", "DMIC L2", "DMIC L3", "ADC 1", "ADC 2", "ADC 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_sidetone_enum, RT5671_SIDETONE_CTRL,
	RT5671_ST_SEL_SFT, rt5671_sidetone_src);

static const struct snd_kcontrol_new rt5671_sidetone_mux =
	SOC_DAPM_ENUM("Sidetone source", rt5671_sidetone_enum);

/* MX-18 [6] */
static const char * const rt5671_anc_src[] = {
	"SNC", "Sidetone"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_anc_enum, RT5671_SIDETONE_CTRL,
	RT5671_ST_EN_SFT, rt5671_anc_src);

static const struct snd_kcontrol_new rt5671_anc_mux =
	SOC_DAPM_ENUM("ANC source", rt5671_anc_enum);

/* MX-9D [9:8] */
static const char * const rt5671_vad_adc_src[] = {
	"Sto1 ADC L", "Mono ADC L", "Mono ADC R", "Sto2 ADC L"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_vad_adc_enum, RT5671_VAD_CTRL4,
	RT5671_VAD_SEL_SFT, rt5671_vad_adc_src);

static const struct snd_kcontrol_new rt5671_vad_adc_mux =
	SOC_DAPM_ENUM("VAD ADC source", rt5671_vad_adc_enum);

static int rt5671_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_index_update_bits(codec,
			RT5671_CHOP_DAC_ADC, 0x1000, 0x1000);
		break;

	case SND_SOC_DAPM_POST_PMD:
		rt5671_index_update_bits(codec,
			RT5671_CHOP_DAC_ADC, 0x1000, 0x0000);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto1_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);


	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_update_eqmode(codec, EQ_CH_ADC, rt5671->adc_eq_mode);
		rt5671_update_alcmode(codec, rt5671->alc_drc_mode);
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		rt5671_update_eqmode(codec, EQ_CH_ADC, NORMAL);
		rt5671_update_alcmode(codec, ALC_NORMAL);
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE,
			RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto1_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_R_MUTE,
			RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

#if 1 //remove for unexpected unmute issue [TD:67399]
static int rt5671_mono_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		if (rt5671->dsp_depoping_in_dsp & 0x1000) {
			rt5671->dsp_depoping_in_dsp |= 0x20;
		}
		else {
			if (!rt5671->mono_adc_mute) {
				dev_dbg(codec->dev, ">>>>> TRACE mono adcl un-mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
				snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
						RT5671_L_MUTE, 0);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		dev_dbg(codec->dev, ">>>>> TRACE mono adcl mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_L_MUTE,
			RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_mono_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		if (rt5671->dsp_depoping_in_dsp & 0x1000) {
			rt5671->dsp_depoping_in_dsp |= 0x10;
		}
		else {
			if (!rt5671->mono_adc_mute){
				dev_dbg(codec->dev, ">>>>> TRACE mono adcr un-mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
				snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
						RT5671_R_MUTE, 0);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		dev_dbg(codec->dev, ">>>>> TRACE mono adcr mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_MONO_ADC_DIG_VOL,
			RT5671_R_MUTE,
			RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto2_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (rt5671->dsp_depoping_in_dsp & 0x1000) {
			rt5671->dsp_depoping_in_dsp |= 0x02;
		}
		else {
			if (!rt5671->sto2_adc_mute){
				dev_dbg(codec->dev, ">>>>> TRACE sto2 adcl un-mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
				snd_soc_update_bits(codec, RT5671_STO2_ADC_DIG_VOL,
						RT5671_L_MUTE, 0);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE sto2 adcl mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_STO2_ADC_DIG_VOL,
			RT5671_L_MUTE,
			RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_sto2_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (rt5671->dsp_depoping_in_dsp & 0x1000) {
			rt5671->dsp_depoping_in_dsp |= 0x01;
		}
		else {
			if (!rt5671->sto2_adc_mute) {
				dev_dbg(codec->dev, ">>>>> TRACE sto2 adcr un-mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
				snd_soc_update_bits(codec, RT5671_STO2_ADC_DIG_VOL,
						RT5671_R_MUTE, 0);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE sto2 adcr mute[%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_STO2_ADC_DIG_VOL,
			RT5671_R_MUTE,
			RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}
#endif //remove for unexpected unmute issue [TD:67399]

static void hp_amp_power(struct snd_soc_codec *codec, int on, bool delay)
{
	if (on) {
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_PM_HP_MASK, RT5671_PM_HP_HV);
		snd_soc_update_bits(codec, RT5671_GEN_CTRL2,
			0x0400, 0x0400);
		/* headphone amp power on */
		snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
			RT5671_PWR_HA |	RT5671_PWR_FV1 |
			RT5671_PWR_FV2,	RT5671_PWR_HA |
			RT5671_PWR_FV1 | RT5671_PWR_FV2);
		/* depop parameters */
		snd_soc_write(codec, RT5671_DEPOP_M2, 0x3100);
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x8009);
		if( hp_amp_power_up ){
			pr_debug("HP_DCC_INT1 Setting%d\n");
		rt5671_index_write(codec, RT5671_HP_DCC_INT1, 0x9f00);
		}
		if (delay) {
			pr_debug("hp_amp_time=%d\n",hp_amp_time);
			mdelay(hp_amp_time);
		}
		if( hp_amp_power_up ){
			pr_debug("hp enable%d\n");
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
		}
	} else {
		snd_soc_write(codec, RT5671_DEPOP_M1, 0x0004);
/*
		if (delay)
			mdelay(30);
*/
	}
}

static void rt5671_pmu_depop(struct snd_soc_codec *codec)
{
	bool umute_ad_da = false;
	unsigned int hp_vol;
	/* headphone unmute sequence */

	dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);

	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x805d);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x831d);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL2,
				0x0300, 0x0300);
	snd_soc_update_bits(codec, RT5671_HP_VOL,
		RT5671_L_MUTE | RT5671_R_MUTE, 0);

	pr_debug("pmu_depop_time=%d\n",pmu_depop_time);
	mdelay(pmu_depop_time);

	if (!(snd_soc_read(codec, RT5671_AD_DA_MIXER) &
		(RT5671_M_ADCMIX_L | RT5671_M_ADCMIX_R)) &&
		headphone_depop ) {

		snd_soc_update_bits(codec, RT5671_SV_ZCD1,
			RT5671_HP_SV_MASK | RT5671_SV_DLY_MASK,
			RT5671_HP_SV_EN | 0x5);

		hp_vol = snd_soc_read(codec, RT5671_HP_VOL);
		snd_soc_update_bits(codec, RT5671_HP_VOL,
			RT5671_L_VOL_MASK | RT5671_R_VOL_MASK, 0x2727);

		umute_ad_da = true;
	}
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
	if (umute_ad_da && headphone_depop){
		mdelay(headphone_depop_delay);
		snd_soc_update_bits(codec, RT5671_HP_VOL,
			RT5671_L_VOL_MASK | RT5671_R_VOL_MASK, hp_vol);
	}
	dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
}

static void rt5671_pmd_depop(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, RT5671_SV_ZCD1, RT5671_HP_SV_MASK,
		RT5671_HP_SV_DIS);

	/* headphone mute sequence */
	dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xb400);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0772);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x803d);
	mdelay(10);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x831d);
	mdelay(10);
	snd_soc_update_bits(codec, RT5671_HP_VOL,
		RT5671_L_MUTE | RT5671_R_MUTE, RT5671_L_MUTE | RT5671_R_MUTE);
	mdelay(20);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL2, 0x0300, 0x0);
	mdelay(pmd_depop_time);
	snd_soc_write(codec, RT5671_DEPOP_M1, 0x8019);
	snd_soc_write(codec, RT5671_DEPOP_M3, 0x0707);
	rt5671_index_write(codec, RT5671_MAMP_INT_REG2, 0xfc00);
	dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);

}

static int rt5671_hp_power_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		hp_amp_power_up = true;

		if (hp_amp_power_up != lout_amp_power_up)
			hp_amp_power(codec, hp_amp_power_on, hp_amp_delay_for_hp);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hp_amp_power_up = false;

		if (!hp_amp_power_up && !lout_amp_power_up)
			hp_amp_power(codec, hp_amp_power_off, hp_amp_delay_for_hp);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_lout_power_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		lout_amp_power_up = true;

		if (hp_amp_power_up != lout_amp_power_up)
			hp_amp_power(codec, hp_amp_power_on, hp_amp_delay_for_lout);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		lout_amp_power_up = false;

		if (!hp_amp_power_up && !lout_amp_power_up)
			hp_amp_power(codec, hp_amp_power_off, hp_amp_delay_for_lout);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_pmu_depop(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5671_pmd_depop(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_mono_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_MONO_OUT,
				RT5671_L_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_MONO_OUT,
			RT5671_L_MUTE, RT5671_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_lout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_PM_HP_MASK, RT5671_PM_HP_HV);
		snd_soc_update_bits(codec, RT5671_LOUT1,
			RT5671_L_MUTE | RT5671_R_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_LOUT1,
			RT5671_L_MUTE | RT5671_R_MUTE,
			RT5671_L_MUTE | RT5671_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_1L_LH_MASK | RT5671_DMIC_1R_LH_MASK |
			RT5671_DMIC_1_DP_MASK,
			RT5671_DMIC_1L_LH_FALLING | RT5671_DMIC_1R_LH_RISING |
			RT5671_DMIC_1_DP_IN3P);
		break;

	case SND_SOC_DAPM_POST_PMD:
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK |
			RT5671_DMIC_2_DP_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING |
			RT5671_DMIC_2_DP_IN3N);
		break;

	case SND_SOC_DAPM_POST_PMD:
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_set_dmic3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING);
		break;

	case SND_SOC_DAPM_POST_PMD:
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_GEN_CTRL3, 0x4, 0x0);
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_OSW_L_MASK | RT5671_OSW_R_MASK,
			RT5671_OSW_L_DIS | RT5671_OSW_R_DIS);

		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1_P, RT5671_PWR_BST1_P);
		if (rt5671->combo_jack_en) {
			snd_soc_update_bits(codec, RT5671_PWR_VOL,
				RT5671_PWR_MIC_DET, RT5671_PWR_MIC_DET);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_GEN_CTRL3, 0x4, 0x4);
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST1_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST2_P, RT5671_PWR_BST2_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST2_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST3_P, RT5671_PWR_BST3_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST3_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_bst4_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST4_P, RT5671_PWR_BST4_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_BST4_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm1_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_L, RT5671_M_PDM1_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm1_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM1_R, RT5671_M_PDM1_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm2_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_L, RT5671_M_PDM2_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_pdm2_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5671_PDM_OUT_CTRL,
			RT5671_M_PDM2_R, RT5671_M_PDM2_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_update_eqmode(codec, EQ_CH_DACL, rt5671->eq_mode);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5671_update_eqmode(codec, EQ_CH_DACL, NORMAL);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5671_update_eqmode(codec, EQ_CH_DACR, rt5671->eq_mode);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5671_update_eqmode(codec, EQ_CH_DACR, NORMAL);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac_filter_depop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_sto_dac, reg_mono_dac;
	unsigned int reg_pwr_dig2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
//	case SND_SOC_DAPM_PRE_PMD:
		reg_pwr_dig2 = snd_soc_read(codec, RT5671_PWR_DIG2);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) pwr_dig2[0x%x]=off->on? (%d) <<<<<\n",
			__FUNCTION__, __LINE__, reg_pwr_dig2, (reg_pwr_dig2 & RT5671_PWR_DAC_S1F));
		if (reg_pwr_dig2 & RT5671_PWR_DAC_S1F)
			return 0;
		reg_sto_dac = snd_soc_read(codec, RT5671_STO_DAC_MIXER);
		reg_mono_dac = snd_soc_read(codec, RT5671_MONO_DAC_MIXER);

		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1 | RT5671_M_DAC_R1_STO_L |
			RT5671_M_DAC_R1 | RT5671_M_DAC_L1_STO_R,
			RT5671_M_DAC_L1 | RT5671_M_DAC_R1_STO_L |
			RT5671_M_DAC_R1 | RT5671_M_DAC_L1_STO_R);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L1_MONO_L | RT5671_M_DAC_R1_MONO_R,
			RT5671_M_DAC_L1_MONO_L | RT5671_M_DAC_R1_MONO_R);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
//		break;
		snd_soc_update_bits(codec, RT5671_PWR_DIG2,
			RT5671_PWR_DAC_S1F, RT5671_PWR_DAC_S1F);
//	case SND_SOC_DAPM_POST_PMU:
//	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		mdelay(depop_delay_test);
		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L1 | RT5671_M_DAC_R1_STO_L |
			RT5671_M_DAC_R1 | RT5671_M_DAC_L1_STO_R,
			reg_sto_dac);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L1_MONO_L | RT5671_M_DAC_R1_MONO_R,
			reg_mono_dac);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac_monol_filter_depop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_sto_dac, reg_mono_dac, reg_dig_mix;
	unsigned int reg_pwr_dig2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
//	case SND_SOC_DAPM_PRE_PMD:
		reg_pwr_dig2 = snd_soc_read(codec, RT5671_PWR_DIG2);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) pwr_dig2[0x%x]=off->on? (%d) <<<<<\n",
			__FUNCTION__, __LINE__, reg_pwr_dig2, (reg_pwr_dig2 & RT5671_PWR_DAC_MF_L));
		if (reg_pwr_dig2 & RT5671_PWR_DAC_MF_L)
			return 0;

		reg_sto_dac = snd_soc_read(codec, RT5671_STO_DAC_MIXER);
		reg_mono_dac = snd_soc_read(codec, RT5671_MONO_DAC_MIXER);
		reg_dig_mix = snd_soc_read(codec, RT5671_DIG_MIXER);

		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L2, RT5671_M_DAC_L2);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_L | RT5671_M_DAC_L2_MONO_R,
			RT5671_M_DAC_L2_MONO_L | RT5671_M_DAC_L2_MONO_R);
		snd_soc_update_bits(codec, RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_L | RT5671_M_DAC_L2_DAC_R,
			RT5671_M_DAC_L2_DAC_L | RT5671_M_DAC_L2_DAC_R);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
//		break;
		snd_soc_update_bits(codec, RT5671_PWR_DIG2,
			RT5671_PWR_DAC_MF_L, RT5671_PWR_DAC_MF_L);
//	case SND_SOC_DAPM_POST_PMU:
//	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		mdelay(depop_delay_test);
		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			RT5671_M_DAC_L2, reg_sto_dac);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_L2_MONO_L | RT5671_M_DAC_L2_MONO_R,
			reg_mono_dac);
		snd_soc_update_bits(codec, RT5671_DIG_MIXER,
			RT5671_M_DAC_L2_DAC_L | RT5671_M_DAC_L2_DAC_R,
			reg_dig_mix);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac_monor_filter_depop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_sto_dac, reg_mono_dac, reg_dig_mix;
	unsigned int reg_pwr_dig2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
//	case SND_SOC_DAPM_PRE_PMD:
		reg_pwr_dig2 = snd_soc_read(codec, RT5671_PWR_DIG2);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) pwr_dig2[0x%x]=off->on? (%d) <<<<<\n",
			__FUNCTION__, __LINE__, reg_pwr_dig2, (reg_pwr_dig2 & RT5671_PWR_DAC_MF_R));
		if (reg_pwr_dig2 & RT5671_PWR_DAC_MF_R)
			return 0;
		reg_sto_dac = snd_soc_read(codec, RT5671_STO_DAC_MIXER);
		reg_mono_dac = snd_soc_read(codec, RT5671_MONO_DAC_MIXER);
		reg_dig_mix = snd_soc_read(codec, RT5671_DIG_MIXER);

		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			 RT5671_M_DAC_R2, RT5671_M_DAC_R2);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_L | RT5671_M_DAC_R2_MONO_R,
			RT5671_M_DAC_R2_MONO_L | RT5671_M_DAC_R2_MONO_R);
		snd_soc_update_bits(codec, RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_R | RT5671_M_DAC_R2_DAC_L,
			RT5671_M_DAC_R2_DAC_R | RT5671_M_DAC_R2_DAC_L);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
//		break;
		snd_soc_update_bits(codec, RT5671_PWR_DIG2,
			RT5671_PWR_DAC_MF_R, RT5671_PWR_DAC_MF_R);
//	case SND_SOC_DAPM_POST_PMU:
//	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		mdelay(depop_delay_test);
		snd_soc_update_bits(codec, RT5671_STO_DAC_MIXER,
			 RT5671_M_DAC_R2, reg_sto_dac);
		snd_soc_update_bits(codec, RT5671_MONO_DAC_MIXER,
			RT5671_M_DAC_R2_MONO_L | RT5671_M_DAC_R2_MONO_R,
			reg_mono_dac);
		snd_soc_update_bits(codec, RT5671_DIG_MIXER,
			RT5671_M_DAC_R2_DAC_R | RT5671_M_DAC_R2_DAC_L,
			reg_dig_mix);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}
static int rt5671_adc_sto1_filter_depop_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_sto_adc;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		reg_sto_adc = snd_soc_read(codec, RT5671_STO1_ADC_DIG_VOL);

		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE | RT5671_R_MUTE,
			RT5671_L_MUTE | RT5671_R_MUTE);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL,
			RT5671_L_MUTE | RT5671_R_MUTE,
			reg_sto_adc);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5671_dac1_l_powerup_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_outmix, reg_hpmix;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_DAC_L1, RT5671_PWR_DAC_L1);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}
static int rt5671_dac1_r_powerup_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_outmix, reg_hpmix;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_DAC_R1, RT5671_PWR_DAC_R1);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}
static int rt5671_dac2_l_powerup_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_outmix;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_DAC_L2, RT5671_PWR_DAC_L2);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}
static int rt5671_dac2_r_powerup_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	static unsigned int reg_outmix;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_DAC_R2, RT5671_PWR_DAC_R2);
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		break;

	default:
		return 0;
	}

	return 0;
}

#ifdef DELAYED_OFF
static int rt5671_i2s1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_I2S1, RT5671_PWR_I2S1);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_i2s2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_I2S2, RT5671_PWR_I2S2);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_i2s3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_I2S3, RT5671_PWR_I2S3);
		break;
	default:
		return 0;
	}

	return 0;
}

static int rt5671_i2s4_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		snd_soc_update_bits(codec, RT5671_PWR_DIG1,
			RT5671_PWR_I2S4, RT5671_PWR_I2S4);
		break;
	default:
		return 0;
	}

	return 0;
}
#endif /* DELAYED_OFF */

static int rt5671_dsp_depop_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, ">>>>> TRACE [%s]->(%d) <<<<<\n", __FUNCTION__, __LINE__);
		rt5671->dsp_depoping_in_dsp |= 0x1000;
		if (dsp_depop_in_dsp_bypass) {
			cancel_delayed_work_sync(&rt5671->unmute_work);
			snd_soc_update_bits(codec, RT5671_DAC_CTRL,
					RT5671_M_DAC_L2_VOL | RT5671_M_DAC_R2_VOL,
					RT5671_M_DAC_L2_VOL | RT5671_M_DAC_R2_VOL);
			schedule_delayed_work(&rt5671->unmute_work,
					msecs_to_jiffies(dsp_depop_delay_in_dsp_bypass));
		}
		break;

	default:
		return 0;
	}
	return 0;
}

static int rt5671_post_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		break;
	default:
		return 0;
	}
	return 0;
}

static int rt5671_pre_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	default:
		return 0;
	}
	return 0;
}

static const struct snd_soc_dapm_widget rt5671_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("PLL1", RT5671_PWR_ANLG2,
		RT5671_PWR_PLL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S DSP", RT5671_PWR_DIG2,
		RT5671_PWR_I2S_DSP_BIT, 0, NULL, 0),
#ifdef JD1_FUNC
	SND_SOC_DAPM_SUPPLY("JD Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
#else
	SND_SOC_DAPM_SUPPLY("JD Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
#endif

	/* ASRC */
	SND_SOC_DAPM_SUPPLY_S("I2S1 ASRC", 1, RT5671_ASRC_1,
		11, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2S2 ASRC", 1, RT5671_ASRC_1,
		12, 0, rt5671_dac_monol_filter_depop_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("I2S3 ASRC", 1, RT5671_ASRC_1,
		13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("I2S4 ASRC", 1, RT5671_ASRC_1,
		14, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC STO ASRC", 1, RT5671_ASRC_1,
		10, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC MONO L ASRC", 1, RT5671_ASRC_1,
		9, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC MONO R ASRC", 1, RT5671_ASRC_1,
		8, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DMIC STO1 ASRC", 1, RT5671_ASRC_1,
		7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DMIC STO2 ASRC", 1, RT5671_ASRC_1,
		6, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DMIC MONO L ASRC", 1, RT5671_ASRC_1,
		5, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DMIC MONO R ASRC", 1, RT5671_ASRC_1,
		4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC STO1 ASRC", 1, RT5671_ASRC_1,
		3, 0, rt5671_adc_sto1_filter_depop_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("ADC STO2 ASRC", 1, RT5671_ASRC_1,
		2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC MONO L ASRC", 1, RT5671_ASRC_1,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC MONO R ASRC", 1, RT5671_ASRC_1,
		0, 0, NULL, 0),

	/* Input Side */
	/* micbias */
	SND_SOC_DAPM_MICBIAS("micbias1", RT5671_PWR_ANLG2,
		RT5671_PWR_MB1_BIT, 0),
	SND_SOC_DAPM_MICBIAS("micbias2", RT5671_PWR_ANLG2,
		RT5671_PWR_MB2_BIT, 0),
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC L1"),
	SND_SOC_DAPM_INPUT("DMIC R1"),
	SND_SOC_DAPM_INPUT("DMIC L2"),
	SND_SOC_DAPM_INPUT("DMIC R2"),
	SND_SOC_DAPM_INPUT("DMIC L3"),
	SND_SOC_DAPM_INPUT("DMIC R3"),

	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN1N"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_INPUT("IN3P"),
	SND_SOC_DAPM_INPUT("IN3N"),
	SND_SOC_DAPM_INPUT("IN4P"),
	SND_SOC_DAPM_INPUT("IN4N"),

	SND_SOC_DAPM_PGA_E("DMIC1", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC3", SND_SOC_NOPM,
		0, 0, NULL, 0, NULL,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC1 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_1_EN_SFT, 0, rt5671_set_dmic1_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DMIC2 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_2_EN_SFT, 0, rt5671_set_dmic2_event,
		SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC3 Power", RT5671_DMIC_CTRL1,
		RT5671_DMIC_3_EN_SFT, 0, rt5671_set_dmic3_event,
		SND_SOC_DAPM_PRE_PMU),

	/* Boost */
	SND_SOC_DAPM_PGA_E("BST1", RT5671_PWR_ANLG2,
		RT5671_PWR_BST1_BIT, 0, NULL, 0, rt5671_bst1_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST2", RT5671_PWR_ANLG2,
		RT5671_PWR_BST2_BIT, 0, NULL, 0, rt5671_bst2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST3", RT5671_PWR_ANLG2,
		RT5671_PWR_BST3_BIT, 0, NULL, 0, rt5671_bst3_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST4", RT5671_PWR_ANLG2,
		RT5671_PWR_BST4_BIT, 0, NULL, 0, rt5671_bst4_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Input Volume */
	SND_SOC_DAPM_PGA("INL VOL", RT5671_PWR_VOL,
		RT5671_PWR_IN_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR VOL", RT5671_PWR_VOL,
		RT5671_PWR_IN_R_BIT, 0, NULL, 0),

	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIXL", RT5671_PWR_MIXER, RT5671_PWR_RM_L_BIT, 0,
		rt5671_rec_l_mix, ARRAY_SIZE(rt5671_rec_l_mix)),
	SND_SOC_DAPM_MIXER("RECMIXR", RT5671_PWR_MIXER, RT5671_PWR_RM_R_BIT, 0,
		rt5671_rec_r_mix, ARRAY_SIZE(rt5671_rec_r_mix)),
	SND_SOC_DAPM_MIXER("RECMIXM", RT5671_PWR_MIXER, RT5671_PWR_RM_M_BIT, 0,
		rt5671_rec_m_mix, ARRAY_SIZE(rt5671_rec_m_mix)),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC 1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC 2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC 3", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("ADC 1_2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("ADC 1 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC 2 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC 3 power", RT5671_PWR_DIG1,
		RT5671_PWR_ADC_3_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC clock", SND_SOC_NOPM, 0, 0,
		rt5671_adc_clk_event, SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),
	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc_mux),
	SND_SOC_DAPM_MUX("Stereo1 DMIC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto1_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto_adc1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc_mux),
	SND_SOC_DAPM_MUX("Stereo2 DMIC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc2_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc1_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC LR Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sto2_adc_lr_mux),
	SND_SOC_DAPM_MUX("Mono ADC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l_mux),
	SND_SOC_DAPM_MUX("Mono ADC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r_mux),
	SND_SOC_DAPM_MUX("Mono DMIC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_dmic_l_mux),
	SND_SOC_DAPM_MUX("Mono DMIC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_dmic_r_mux),
	SND_SOC_DAPM_MUX("Mono ADC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l2_mux),
	SND_SOC_DAPM_MUX("Mono ADC L1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_l1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_mono_adc_r2_mux),
	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY("adc stereo1 filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("adc stereo2 filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_S2F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Sto1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adc_l_mix, ARRAY_SIZE(rt5671_sto1_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adc_r_mix, ARRAY_SIZE(rt5671_sto1_adc_r_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adc_l_mix, ARRAY_SIZE(rt5671_sto2_adc_l_mix)),
	SND_SOC_DAPM_MIXER("Sto2 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adc_r_mix, ARRAY_SIZE(rt5671_sto2_adc_r_mix)),
	SND_SOC_DAPM_SUPPLY("adc mono left filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Mono ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_mono_adc_l_mix, ARRAY_SIZE(rt5671_mono_adc_l_mix)),
	SND_SOC_DAPM_SUPPLY("adc mono right filter", RT5671_PWR_DIG2,
		RT5671_PWR_ADC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Mono ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_mono_adc_r_mix, ARRAY_SIZE(rt5671_mono_adc_r_mix)),

	/* ADC PGA */
	SND_SOC_DAPM_PGA_S("Stereo1 ADC MIXL", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Stereo1 ADC MIXR", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto1_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#if 1 //remove for unexpected unmute issue [TD:67399]
	SND_SOC_DAPM_PGA_S("Stereo2 ADC MIXL", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Stereo2 ADC MIXR", 1, SND_SOC_NOPM, 0, 0,
		rt5671_sto2_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_PGA_S("Stereo2 ADC MIXL", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Stereo2 ADC MIXR", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
#endif //remove for unexpected unmute issue [TD:67399]

#if 1 //remove for unexpected unmute issue [TD:67399]
	SND_SOC_DAPM_PGA_S("Mono ADC MIXL VOL", 1, SND_SOC_NOPM, 0, 0,
		rt5671_mono_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Mono ADC MIXR VOL", 1, SND_SOC_NOPM, 0, 0,
		rt5671_mono_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_PGA_S("Mono ADC MIXL VOL", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Mono ADC MIXR VOL", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
#endif //remove for unexpected unmute issue [TD:67399]

	SND_SOC_DAPM_PGA("Sto2 ADC LR MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo1 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VAD_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC4", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DSP */
	SND_SOC_DAPM_PGA("TxDP_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDP_ADC_R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("TxDC_DAC", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("TDM Data Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_txdp_slot_mux),

	SND_SOC_DAPM_MUX("DSP UL Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dsp_ul_mux),
	SND_SOC_DAPM_MUX_E("DSP DL Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dsp_dl_mux, rt5671_dsp_depop_event,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX("RxDP Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_rxdp_mux),

	/* IF2 3 4 Mux */
	SND_SOC_DAPM_MUX("IF2 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if2_adc_in_mux),
	SND_SOC_DAPM_MUX("IF3 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if3_adc_in_mux),
	SND_SOC_DAPM_MUX("IF4 ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if4_adc_in_mux),

	/* Digital Interface */
#ifdef DELAYED_OFF
	SND_SOC_DAPM_SUPPLY("I2S1", SND_SOC_NOPM, 0, 0, rt5671_i2s1_event,
		SND_SOC_DAPM_PRE_PMU),
#else
	SND_SOC_DAPM_SUPPLY("I2S1", RT5671_PWR_DIG1,
		RT5671_PWR_I2S1_BIT, 0, NULL, 0),
#endif
	SND_SOC_DAPM_PGA("IF1 DAC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
#ifdef DELAYED_OFF
	SND_SOC_DAPM_SUPPLY("I2S2", SND_SOC_NOPM, 0, 0, rt5671_i2s2_event,
		SND_SOC_DAPM_PRE_PMU),
#else
	SND_SOC_DAPM_SUPPLY("I2S2", RT5671_PWR_DIG1,
		RT5671_PWR_I2S2_BIT, 0, NULL, 0),
#endif
	SND_SOC_DAPM_PGA("IF2 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
#ifdef DELAYED_OFF
	SND_SOC_DAPM_SUPPLY("I2S3", SND_SOC_NOPM, 0, 0, rt5671_i2s3_event,
		SND_SOC_DAPM_PRE_PMU),
#else
	SND_SOC_DAPM_SUPPLY("I2S3", RT5671_PWR_DIG1,
		RT5671_PWR_I2S3_BIT, 0, NULL, 0),
#endif
	SND_SOC_DAPM_PGA("IF3 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
#ifdef DELAYED_OFF
	SND_SOC_DAPM_SUPPLY("I2S4", SND_SOC_NOPM, 0, 0, rt5671_i2s4_event,
		SND_SOC_DAPM_PRE_PMU),
#else
	SND_SOC_DAPM_SUPPLY("I2S4", RT5671_PWR_DIG1,
		RT5671_PWR_I2S4_BIT, 0, NULL, 0),
#endif
	SND_SOC_DAPM_PGA("IF4 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF4 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MUX("IF1 ADC1 IN1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc1_in1_mux),
	SND_SOC_DAPM_MUX("IF1 ADC1 IN2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc1_in2_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc2_in_mux),
	SND_SOC_DAPM_MUX("IF1 ADC2 IN1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_if1_adc2_in1_mux),
	SND_SOC_DAPM_MUX("VAD ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_vad_adc_mux),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF3RX", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF3TX", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF4RX", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF4COMP", "AIF4 Compress", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF4TX", "AIF4 Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Audio DSP */
	SND_SOC_DAPM_PGA("Audio DSP", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Side */
	/* DAC mixer before sound effect */
	SND_SOC_DAPM_MIXER_E("DAC1 MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_dac_l_mix, ARRAY_SIZE(rt5671_dac_l_mix),
		rt5671_dac_l_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("DAC1 MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_dac_r_mix, ARRAY_SIZE(rt5671_dac_r_mix),
		rt5671_dac_r_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DAC1 MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DAC2 channel Mux */
	SND_SOC_DAPM_MUX("DAC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac_l2_mux),
	SND_SOC_DAPM_MUX("DAC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac_r2_mux),

	SND_SOC_DAPM_MUX("DAC1 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac1l_mux),
	SND_SOC_DAPM_MUX("DAC1 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_dac1r_mux),

	/* Sidetone */
	SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_sidetone_mux),
	SND_SOC_DAPM_MUX("ANC Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_anc_mux),
	SND_SOC_DAPM_PGA("SNC", SND_SOC_NOPM,
		0, 0, NULL, 0),
	/* DAC Mixer */
	SND_SOC_DAPM_SUPPLY("dac stereo1 filter", /*RT5671_PWR_DIG2*/SND_SOC_NOPM,
		/*RT5671_PWR_DAC_S1F_BIT*/0, 0, rt5671_dac_filter_depop_event,
		SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("dac mono left filter", /*RT5671_PWR_DIG2*/SND_SOC_NOPM,
		/*RT5671_PWR_DAC_MF_L_BIT*/0, 0, rt5671_dac_monol_filter_depop_event,
		SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("dac mono right filter", /*RT5671_PWR_DIG2*/SND_SOC_NOPM,
		/*RT5671_PWR_DAC_MF_R_BIT*/0, 0, rt5671_dac_monor_filter_depop_event,
		SND_SOC_DAPM_PRE_PMU),

    SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_sto_dac_l_mix, ARRAY_SIZE(rt5671_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_sto_dac_r_mix, ARRAY_SIZE(rt5671_sto_dac_r_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_mono_dac_l_mix, ARRAY_SIZE(rt5671_mono_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_mono_dac_r_mix, ARRAY_SIZE(rt5671_mono_dac_r_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5671_dig_l_mix, ARRAY_SIZE(rt5671_dig_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5671_dig_r_mix, ARRAY_SIZE(rt5671_dig_r_mix)),
	SND_SOC_DAPM_PGA("DAC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DACs */
	SND_SOC_DAPM_SUPPLY("DAC L1 Power", SND_SOC_NOPM, 0, 0,
		rt5671_dac1_l_powerup_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DAC R1 Power", SND_SOC_NOPM, 0, 0,
		rt5671_dac1_r_powerup_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_DAC("DAC L1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC R1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC L2", NULL, SND_SOC_NOPM, 0, 0,
		rt5671_dac2_l_powerup_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_DAC_E("DAC R2", NULL, SND_SOC_NOPM, 0, 0,
		rt5671_dac2_r_powerup_event, SND_SOC_DAPM_PRE_PMU),

	/* OUT Mixer */
	SND_SOC_DAPM_MIXER("OUT MIXL", RT5671_PWR_MIXER, RT5671_PWR_OM_L_BIT,
		0, rt5671_out_l_mix, ARRAY_SIZE(rt5671_out_l_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXR", RT5671_PWR_MIXER, RT5671_PWR_OM_R_BIT,
		0, rt5671_out_r_mix, ARRAY_SIZE(rt5671_out_r_mix)),
	/* Ouput Volume */
	SND_SOC_DAPM_MIXER("HPOVOL MIXL", RT5671_PWR_VOL, RT5671_PWR_HV_L_BIT,
		0, rt5671_hpvoll_mix, ARRAY_SIZE(rt5671_hpvoll_mix)),
	SND_SOC_DAPM_MIXER("HPOVOL MIXR", RT5671_PWR_VOL, RT5671_PWR_HV_R_BIT,
		0, rt5671_hpvolr_mix, ARRAY_SIZE(rt5671_hpvolr_mix)),
	SND_SOC_DAPM_PGA("DAC 1", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC 2", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL", SND_SOC_NOPM,
		0, 0, NULL, 0),

	/* HPO/LOUT/Mono Mixer */
	SND_SOC_DAPM_MIXER("HPO MIX", SND_SOC_NOPM, 0, 0,
		rt5671_hpo_mix, ARRAY_SIZE(rt5671_hpo_mix)),
	SND_SOC_DAPM_MIXER("LOUT MIX", RT5671_PWR_ANLG1, RT5671_PWR_LM_BIT,
		0, rt5671_lout_mix, ARRAY_SIZE(rt5671_lout_mix)),
	SND_SOC_DAPM_MIXER("MONOVOL MIX", RT5671_PWR_ANLG1, RT5671_PWR_MM_BIT,
		0, rt5671_mono_mix, ARRAY_SIZE(rt5671_mono_mix)),
	SND_SOC_DAPM_MIXER("MONOAmp MIX", SND_SOC_NOPM, 0,
		0, rt5671_monoamp_mix, ARRAY_SIZE(rt5671_monoamp_mix)),

	SND_SOC_DAPM_SUPPLY_S("Improve HP Amp Drv", 1, SND_SOC_NOPM,
		0, 0, rt5671_hp_power_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("HP L Amp", RT5671_PWR_ANLG1,
		RT5671_PWR_HP_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("HP R Amp", RT5671_PWR_ANLG1,
		RT5671_PWR_HP_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("HP Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5671_hp_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("Improve LOUT Amp Drv", 1, SND_SOC_NOPM,
		0, 0, rt5671_lout_power_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_S("LOUT Amp", 1, SND_SOC_NOPM, 0, 0,
		rt5671_lout_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Mono Amp", 1, RT5671_PWR_ANLG1,
		RT5671_PWR_MA_BIT, 0, rt5671_mono_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* PDM */
	SND_SOC_DAPM_SUPPLY("PDM1 Power", RT5671_PWR_DIG2,
		RT5671_PWR_PDM1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PDM2 Power", RT5671_PWR_DIG2,
		RT5671_PWR_PDM2_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("PDM1 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm1_l_mux, rt5671_pdm1_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM1 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm1_r_mux, rt5671_pdm1_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 L Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm2_l_mux, rt5671_pdm2_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM2 R Mux", SND_SOC_NOPM, 0, 0,
		&rt5671_pdm2_r_mux, rt5671_pdm2_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),
	SND_SOC_DAPM_OUTPUT("MonoP"),
	SND_SOC_DAPM_OUTPUT("MonoN"),
	SND_SOC_DAPM_OUTPUT("PDM1L"),
	SND_SOC_DAPM_OUTPUT("PDM1R"),
	SND_SOC_DAPM_OUTPUT("PDM2L"),
	SND_SOC_DAPM_OUTPUT("PDM2R"),

	SND_SOC_DAPM_POST("DAPM_POST", rt5671_post_event),
	SND_SOC_DAPM_PRE("DAPM_PRE", rt5671_pre_event),
};

static const struct snd_soc_dapm_route rt5671_dapm_routes[] = {
	{ "Stereo1 DMIC Mux", NULL, "DMIC STO1 ASRC" },
	{ "Stereo2 DMIC Mux", NULL, "DMIC STO2 ASRC" },
	{ "Mono DMIC L Mux", NULL, "DMIC MONO L ASRC" },
	{ "Mono DMIC R Mux", NULL, "DMIC MONO R ASRC" },
	{ "adc stereo1 filter", NULL, "ADC STO1 ASRC" },
	{ "adc stereo2 filter", NULL, "ADC STO2 ASRC" },
	{ "adc mono left filter", NULL, "ADC MONO L ASRC" },
	{ "adc mono right filter", NULL, "ADC MONO R ASRC" },
	{ "dac mono left filter", NULL, "DAC MONO L ASRC" },
	{ "dac mono right filter", NULL, "DAC MONO R ASRC" },
	{ "dac stereo1 filter", NULL, "DAC STO ASRC" },

	{ "I2S1", NULL, "I2S1 ASRC" },
	{ "I2S2", NULL, "I2S2 ASRC" },
	{ "I2S3", NULL, "I2S3 ASRC" },
	{ "I2S4", NULL, "I2S4 ASRC" },

	{ "DMIC1", NULL, "DMIC L1" },
	{ "DMIC1", NULL, "DMIC R1" },
	{ "DMIC2", NULL, "DMIC L2" },
	{ "DMIC2", NULL, "DMIC R2" },
	{ "DMIC3", NULL, "DMIC L3" },
	{ "DMIC3", NULL, "DMIC R3" },

	{ "BST1", NULL, "IN1P" },
	{ "BST1", NULL, "IN1N" },
	{ "BST1", NULL, "JD Power" },
	{ "BST1", NULL, "Mic Det Power" },
	{ "BST2", NULL, "IN2P" },
	{ "BST2", NULL, "IN2N" },
	{ "BST3", NULL, "IN3P" },
	{ "BST3", NULL, "IN3N" },
	{ "BST4", NULL, "IN4P" },
	{ "BST4", NULL, "IN4N" },

	{ "INL VOL", NULL, "IN3P" },
	{ "INR VOL", NULL, "IN3N" },

	{ "RECMIXL", "INL Switch", "INL VOL" },
	{ "RECMIXL", "BST4 Switch", "BST4" },
	{ "RECMIXL", "BST3 Switch", "BST3" },
	{ "RECMIXL", "BST2 Switch", "BST2" },
	{ "RECMIXL", "BST1 Switch", "BST1" },

	{ "RECMIXR", "INR Switch", "INR VOL" },
	{ "RECMIXR", "BST4 Switch", "BST4" },
	{ "RECMIXR", "BST3 Switch", "BST3" },
	{ "RECMIXR", "BST2 Switch", "BST2" },
	{ "RECMIXR", "BST1 Switch", "BST1" },

	{ "RECMIXM", "BST4 Switch", "BST4" },
	{ "RECMIXM", "BST3 Switch", "BST3" },
	{ "RECMIXM", "BST2 Switch", "BST2" },
	{ "RECMIXM", "BST1 Switch", "BST1" },

	{ "ADC 1", NULL, "RECMIXL" },
	{ "ADC 1", NULL, "ADC 1 power" },
	{ "ADC 1", NULL, "ADC clock" },
	{ "ADC 2", NULL, "RECMIXR" },
	{ "ADC 2", NULL, "ADC 2 power" },
	{ "ADC 2", NULL, "ADC clock" },
	{ "ADC 3", NULL, "RECMIXM" },
	{ "ADC 3", NULL, "ADC 3 power" },
	{ "ADC 3", NULL, "ADC clock" },

	{ "DMIC L1", NULL, "DMIC CLK" },
	{ "DMIC R1", NULL, "DMIC CLK" },
	{ "DMIC L2", NULL, "DMIC CLK" },
	{ "DMIC R2", NULL, "DMIC CLK" },
	{ "DMIC L3", NULL, "DMIC CLK" },
	{ "DMIC R3", NULL, "DMIC CLK" },

	{ "DMIC L1", NULL, "DMIC1 Power" },
	{ "DMIC R1", NULL, "DMIC1 Power" },
	{ "DMIC L2", NULL, "DMIC2 Power" },
	{ "DMIC R2", NULL, "DMIC2 Power" },
	{ "DMIC L3", NULL, "DMIC3 Power" },
	{ "DMIC R3", NULL, "DMIC3 Power" },

	{ "Stereo1 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo1 DMIC Mux", "DMIC2", "DMIC2" },
	{ "Stereo1 DMIC Mux", "DMIC3", "DMIC3" },

	{ "Stereo2 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo2 DMIC Mux", "DMIC2", "DMIC2" },
	{ "Stereo2 DMIC Mux", "DMIC3", "DMIC3" },

	{ "Mono DMIC L Mux", "DMIC1", "DMIC L1" },
	{ "Mono DMIC L Mux", "DMIC2", "DMIC L2" },
	{ "Mono DMIC L Mux", "DMIC3", "DMIC L3" },

	{ "Mono DMIC R Mux", "DMIC1", "DMIC R1" },
	{ "Mono DMIC R Mux", "DMIC2", "DMIC R2" },
	{ "Mono DMIC R Mux", "DMIC3", "DMIC R3" },

	{ "ADC 1_2", NULL, "ADC 1" },
	{ "ADC 1_2", NULL, "ADC 2" },

	{ "Stereo1 ADC Mux", "ADC1L ADC2R", "ADC 1_2" },
	{ "Stereo1 ADC Mux", "ADC3", "ADC 3" },

	{ "DAC MIX", NULL, "DAC MIXL" },
	{ "DAC MIX", NULL, "DAC MIXR" },

	{ "Stereo1 ADC2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC2 Mux", "DAC MIX", "DAC MIX" },
	{ "Stereo1 ADC1 Mux", "ADC", "Stereo1 ADC Mux" },
	{ "Stereo1 ADC1 Mux", "DAC MIX", "DAC MIX" },

	{ "Mono ADC L Mux", "ADC1", "ADC 1" },
	{ "Mono ADC L Mux", "ADC3", "ADC 3" },

	{ "Mono ADC R Mux", "ADC2", "ADC 2" },
	{ "Mono ADC R Mux", "ADC3", "ADC 3" },

	{ "Mono ADC L2 Mux", "DMIC", "Mono DMIC L Mux" },
	{ "Mono ADC L2 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "ADC1 ADC3", "Mono ADC L Mux" },

	{ "Mono ADC R1 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },
	{ "Mono ADC R1 Mux", "ADC2 ADC3", "Mono ADC R Mux" },
	{ "Mono ADC R2 Mux", "DMIC", "Mono DMIC R Mux" },
	{ "Mono ADC R2 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },

	{ "Sto1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC1 Mux" },
	{ "Sto1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC2 Mux" },
	{ "Sto1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC1 Mux" },
	{ "Sto1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC2 Mux" },

	{ "Stereo1 ADC MIXL", NULL, "Sto1 ADC MIXL" },
	{ "Stereo1 ADC MIXL", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Stereo1 ADC MIXR", NULL, "Sto1 ADC MIXR" },
	{ "Stereo1 ADC MIXR", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXL", "ADC1 Switch", "Mono ADC L1 Mux" },
	{ "Mono ADC MIXL", "ADC2 Switch", "Mono ADC L2 Mux" },
	{ "Mono ADC MIXL", NULL, "adc mono left filter" },
	{ "adc mono left filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXR", "ADC1 Switch", "Mono ADC R1 Mux" },
	{ "Mono ADC MIXR", "ADC2 Switch", "Mono ADC R2 Mux" },
	{ "Mono ADC MIXR", NULL, "adc mono right filter" },
	{ "adc mono right filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXL VOL", NULL, "Mono ADC MIXL" },
	{ "Mono ADC MIXR VOL", NULL, "Mono ADC MIXR" },

	{ "Stereo2 ADC Mux", "ADC1L ADC2R", "ADC 1_2" },
	{ "Stereo2 ADC Mux", "ADC3", "ADC 3" },

	{ "Stereo2 ADC2 Mux", "DMIC", "Stereo2 DMIC Mux" },
	{ "Stereo2 ADC2 Mux", "DAC MIX", "DAC MIX" },
	{ "Stereo2 ADC1 Mux", "ADC", "Stereo2 ADC Mux" },
	{ "Stereo2 ADC1 Mux", "DAC MIX", "DAC MIX" },

	{ "Sto2 ADC MIXL", "ADC1 Switch", "Stereo2 ADC1 Mux" },
	{ "Sto2 ADC MIXL", "ADC2 Switch", "Stereo2 ADC2 Mux" },
	{ "Sto2 ADC MIXR", "ADC1 Switch", "Stereo2 ADC1 Mux" },
	{ "Sto2 ADC MIXR", "ADC2 Switch", "Stereo2 ADC2 Mux" },

	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXL" },
	{ "Sto2 ADC LR MIX", NULL, "Sto2 ADC MIXR" },

	{ "Stereo2 ADC LR Mux", "L", "Sto2 ADC MIXL" },
	{ "Stereo2 ADC LR Mux", "LR", "Sto2 ADC LR MIX" },

	{ "Stereo2 ADC MIXL", NULL, "Stereo2 ADC LR Mux" },
	{ "Stereo2 ADC MIXL", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Stereo2 ADC MIXR", NULL, "Sto2 ADC MIXR" },
	{ "Stereo2 ADC MIXR", NULL, "adc stereo2 filter" },
	{ "adc stereo2 filter", NULL, "PLL1", check_sysclk1_source },

	{ "VAD ADC Mux", "Sto1 ADC L", "Stereo1 ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC L", "Mono ADC MIXL VOL" },
	{ "VAD ADC Mux", "Mono ADC R", "Mono ADC MIXR VOL" },
	{ "VAD ADC Mux", "Sto2 ADC L", "Stereo2 ADC MIXL" },

	{ "VAD_ADC", NULL, "VAD ADC Mux" },

	{ "IF_ADC1", NULL, "Stereo1 ADC MIXL" },
	{ "IF_ADC1", NULL, "Stereo1 ADC MIXR" },
	{ "IF_ADC2", NULL, "Mono ADC MIXL VOL" },
	{ "IF_ADC2", NULL, "Mono ADC MIXR VOL" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXL" },
	{ "IF_ADC3", NULL, "Stereo2 ADC MIXR" },

	{ "IF1 ADC1 IN1 Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF1 ADC1 IN1 Mux", "IF1_ADC3", "IF_ADC3" },

	{ "IF1 ADC1 IN2 Mux", "IF1_ADC1_IN1", "IF1 ADC1 IN1 Mux" },
	{ "IF1 ADC1 IN2 Mux", "IF1_ADC4", "TxDP_ADC" },

	{ "IF1 ADC2 IN Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF1 ADC2 IN Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF1 ADC2 IN1 Mux", "IF1_ADC2_IN", "IF1 ADC2 IN Mux" },
	{ "IF1 ADC2 IN1 Mux", "IF1_ADC4", "TxDP_ADC" },

	{ "IF1_ADC1" , NULL, "IF1 ADC1 IN2 Mux" },
	{ "IF1_ADC2" , NULL, "IF1 ADC2 IN1 Mux" },

	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXL" },
	{ "Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXR" },
	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXL" },
	{ "Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXR" },
	{ "Mono ADC MIX", NULL, "Mono ADC MIXL VOL" },
	{ "Mono ADC MIX", NULL, "Mono ADC MIXR VOL" },

	{ "RxDP Mux", "IF2 DAC", "IF2 DAC" },
	{ "RxDP Mux", "IF1 DAC2", "IF1 DAC2" },
	{ "RxDP Mux", "STO1 ADC Mixer", "Stereo1 ADC MIX" },
	{ "RxDP Mux", "STO2 ADC Mixer", "Stereo2 ADC MIX" },
	{ "RxDP Mux", "Mono ADC Mixer L", "Mono ADC MIXL VOL" },
	{ "RxDP Mux", "Mono ADC Mixer R", "Mono ADC MIXR VOL" },
	{ "RxDP Mux", "DAC1", "DAC1 MIX" },

	{ "TDM Data Mux", "Slot 0-1", "Stereo1 ADC MIX" },
	{ "TDM Data Mux", "Slot 2-3", "Mono ADC MIX" },
	{ "TDM Data Mux", "Slot 4-5", "Stereo2 ADC MIX" },
	{ "TDM Data Mux", "Slot 6-7", "IF2 DAC" },

	{ "DSP UL Mux", "Bypass", "TDM Data Mux" },
	{ "DSP UL Mux", NULL, "I2S DSP" },
	{ "DSP DL Mux", "Bypass", "RxDP Mux" },
	{ "DSP DL Mux", NULL, "I2S DSP" },

	{ "TxDP_ADC_L", NULL, "DSP UL Mux" },
	{ "TxDP_ADC_R", NULL, "DSP UL Mux" },
	{ "TxDC_DAC", NULL, "DSP DL Mux" },

	{ "TxDP_ADC", NULL, "TxDP_ADC_L" },
	{ "TxDP_ADC", NULL, "TxDP_ADC_R" },

	{ "IF1 ADC", NULL, "I2S1" },
	{ "IF1 ADC", NULL, "IF1_ADC1" },
#ifdef USE_TDM
	{ "IF1 ADC", NULL, "IF1_ADC2" },
	{ "IF1 ADC", NULL, "IF_ADC3" },
	{ "IF1 ADC", NULL, "TxDP_ADC" },
#endif
	{ "IF2 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF2 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF2 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF2 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF2 ADC Mux", "TxDP_ADC", "TxDP_ADC" },
	{ "IF2 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF3 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF3 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF3 ADC Mux", "IF_ADC3", "IF_ADC3" },
	{ "IF3 ADC Mux", "TxDC_DAC", "TxDC_DAC" },
	{ "IF3 ADC Mux", "TxDP_ADC", "TxDP_ADC" },
	{ "IF3 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF4 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF4 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF4 ADC Mux", "IF_ADC3", "IF_ADC3" },

	{ "IF2 ADC L", NULL, "IF2 ADC Mux" },
	{ "IF2 ADC R", NULL, "IF2 ADC Mux" },
	{ "IF3 ADC L", NULL, "IF3 ADC Mux" },
	{ "IF3 ADC R", NULL, "IF3 ADC Mux" },
	{ "IF4 ADC L", NULL, "IF4 ADC Mux" },
	{ "IF4 ADC R", NULL, "IF4 ADC Mux" },

	{ "IF2 ADC", NULL, "I2S2" },
	{ "IF2 ADC", NULL, "IF2 ADC L" },
	{ "IF2 ADC", NULL, "IF2 ADC R" },
	{ "IF3 ADC", NULL, "I2S3" },
	{ "IF3 ADC", NULL, "IF3 ADC L" },
	{ "IF3 ADC", NULL, "IF3 ADC R" },
	{ "IF4 ADC", NULL, "I2S4" },
	{ "IF4 ADC", NULL, "IF4 ADC L" },
	{ "IF4 ADC", NULL, "IF4 ADC R" },

	{ "AIF1TX", NULL, "IF1 ADC" },
	{ "AIF2TX", NULL, "IF2 ADC" },
	{ "AIF3TX", NULL, "IF3 ADC" },
	{ "AIF4TX", NULL, "IF4 ADC" },

	{ "IF1 DAC1", NULL, "AIF1RX" },
	{ "IF1 DAC2", NULL, "AIF1RX" },
	{ "IF2 DAC", NULL, "AIF2RX" },
	{ "IF3 DAC", NULL, "AIF3RX" },
	{ "IF4 DAC", NULL, "AIF4RX" },
	{ "IF4 DAC", NULL, "AIF4COMP" },

	{ "IF1 DAC1", NULL, "I2S1" },
	{ "IF1 DAC2", NULL, "I2S1" },
	{ "IF2 DAC", NULL, "I2S2" },
	{ "IF3 DAC", NULL, "I2S3" },
	{ "IF4 DAC", NULL, "I2S4" },

	{ "IF1 DAC2 L", NULL, "IF1 DAC2" },
	{ "IF1 DAC2 R", NULL, "IF1 DAC2" },
	{ "IF1 DAC1 L", NULL, "IF1 DAC1" },
	{ "IF1 DAC1 R", NULL, "IF1 DAC1" },
	{ "IF2 DAC L", NULL, "IF2 DAC" },
	{ "IF2 DAC R", NULL, "IF2 DAC" },
	{ "IF3 DAC L", NULL, "IF3 DAC" },
	{ "IF3 DAC R", NULL, "IF3 DAC" },
	{ "IF4 DAC L", NULL, "IF4 DAC" },
	{ "IF4 DAC R", NULL, "IF4 DAC" },

	{ "Sidetone Mux", "DMIC L1", "DMIC L1" },
	{ "Sidetone Mux", "DMIC L2", "DMIC L2" },
	{ "Sidetone Mux", "DMIC L3", "DMIC L3" },
	{ "Sidetone Mux", "ADC 1", "ADC 1" },
	{ "Sidetone Mux", "ADC 2", "ADC 2" },
	{ "Sidetone Mux", "ADC 3", "ADC 3" },

	{ "DAC1 L Mux", "IF1 DAC", "IF1 DAC1 L" },
	{ "DAC1 L Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC1 L Mux", "IF3 DAC", "IF3 DAC L" },
	{ "DAC1 L Mux", "IF4 DAC", "IF4 DAC L" },

	{ "DAC1 R Mux", "IF1 DAC", "IF1 DAC1 R" },
	{ "DAC1 R Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC1 R Mux", "IF3 DAC", "IF3 DAC R" },
	{ "DAC1 R Mux", "IF4 DAC", "IF4 DAC R" },

	{ "DAC1 MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL" },
	{ "DAC1 MIXL", "DAC1 Switch", "DAC1 L Mux" },
	{ "DAC1 MIXL", NULL, "dac stereo1 filter" },
	{ "DAC1 MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR" },
	{ "DAC1 MIXR", "DAC1 Switch", "DAC1 R Mux" },
	{ "DAC1 MIXR", NULL, "dac stereo1 filter" },

	{ "DAC1 MIX", NULL, "DAC1 MIXL" },
	{ "DAC1 MIX", NULL, "DAC1 MIXR" },

	{ "Audio DSP", NULL, "DAC1 MIXL" },
	{ "Audio DSP", NULL, "DAC1 MIXR" },

	{ "DAC L2 Mux", "IF1 DAC", "IF1 DAC2 L" },
	{ "DAC L2 Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC L2 Mux", "IF3 DAC", "IF3 DAC L" },
	{ "DAC L2 Mux", "IF4 DAC", "IF4 DAC L" },
	{ "DAC L2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC L2 Mux", "VAD_ADC", "VAD_ADC" },
	{ "DAC L2 Mux", NULL, "dac mono left filter" },

	{ "DAC R2 Mux", "IF1 DAC", "IF1 DAC2 R" },
	{ "DAC R2 Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC R2 Mux", "IF3 DAC", "IF3 DAC R" },
	{ "DAC R2 Mux", "IF4 DAC", "IF4 DAC R" },
	{ "DAC R2 Mux", "TxDC DAC", "TxDC_DAC" },
	{ "DAC R2 Mux", "TxDP ADC", "TxDP_ADC" },
	{ "DAC R2 Mux", NULL, "dac mono right filter" },

	{ "SNC", NULL, "ADC 1" },
	{ "SNC", NULL, "ADC 2" },

	{ "ANC Mux", "SNC", "SNC" },
	{ "ANC Mux", "Sidetone", "Sidetone Mux" },

	{ "Stereo DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXL", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXL", "DAC L2 Switch", "DAC L2 Mux" },
	{ "Stereo DAC MIXL", "ANC Switch", "ANC Mux" },

//	{ "Stereo DAC MIXL", NULL, "dac stereo1 filter" },
	{ "Stereo DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXR", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXR", "DAC R2 Switch", "DAC R2 Mux" },
	{ "Stereo DAC MIXR", "ANC Switch", "ANC Mux" },
//	{ "Stereo DAC MIXR", NULL, "dac stereo1 filter" },

	{ "Mono DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Mono DAC MIXL", "DAC L2 Switch", "DAC L2 Mux" },
	{ "Mono DAC MIXL", "DAC R2 Switch", "DAC R2 Mux" },
	{ "Mono DAC MIXL", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXL", NULL, "dac mono left filter" },
	{ "Mono DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Mono DAC MIXR", "DAC R2 Switch", "DAC R2 Mux" },
	{ "Mono DAC MIXR", "DAC L2 Switch", "DAC L2 Mux" },
	{ "Mono DAC MIXR", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXR", NULL, "dac mono right filter" },

	{ "DAC MIXL", "Sto DAC Mix L Switch", "Stereo DAC MIXL" },
	{ "DAC MIXL", "DAC L2 Switch", "DAC L2 Mux" },
	{ "DAC MIXL", "DAC R2 Switch", "DAC R2 Mux" },
	{ "DAC MIXR", "Sto DAC Mix R Switch", "Stereo DAC MIXR" },
	{ "DAC MIXR", "DAC R2 Switch", "DAC R2 Mux" },
	{ "DAC MIXR", "DAC L2 Switch", "DAC L2 Mux" },

	{ "DAC L1", NULL, "DAC L1 Power" },
	{ "DAC L1", NULL, "Stereo DAC MIXL" },
	{ "DAC L1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R1", NULL, "DAC R1 Power" },
	{ "DAC R1", NULL, "Stereo DAC MIXR" },
	{ "DAC R1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC L2", NULL, "Mono DAC MIXL" },
	{ "DAC L2", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R2", NULL, "Mono DAC MIXR" },
	{ "DAC R2", NULL, "PLL1", check_sysclk1_source },

	{ "OUT MIXL", "BST2 Switch", "BST2" },
	{ "OUT MIXL", "BST1 Switch", "BST1" },
	{ "OUT MIXL", "INL Switch", "INL VOL" },
	{ "OUT MIXL", "DAC L2 Switch", "DAC L2" },
	{ "OUT MIXL", "DAC L1 Switch", "DAC L1" },

	{ "OUT MIXR", "BST4 Switch", "BST4" },
	{ "OUT MIXR", "BST3 Switch", "BST3" },
	{ "OUT MIXR", "INR Switch", "INR VOL" },
	{ "OUT MIXR", "DAC R2 Switch", "DAC R2" },
	{ "OUT MIXR", "DAC R1 Switch", "DAC R1" },

	{ "HPOVOL MIXL", "DAC1 Switch", "DAC L1" },
	{ "HPOVOL MIXL", "INL Switch", "INL VOL" },
	{ "HPOVOL MIXR", "DAC1 Switch", "DAC R1" },
	{ "HPOVOL MIXR", "INR Switch", "INR VOL" },

	{ "DAC 2", NULL, "DAC L2" },
	{ "DAC 2", NULL, "DAC R2" },
	{ "DAC 1", NULL, "DAC L1" },
	{ "DAC 1", NULL, "DAC R1" },
	{ "HPOVOL", NULL, "HPOVOL MIXL" },
	{ "HPOVOL", NULL, "HPOVOL MIXR" },
	{ "HPO MIX", "DAC1 Switch", "DAC 1" },
	{ "HPO MIX", "HPVOL Switch", "HPOVOL" },

	{ "LOUT MIX", "DAC L1 Switch", "DAC L1" },
	{ "LOUT MIX", "DAC R1 Switch", "DAC R1" },
	{ "LOUT MIX", "OUTMIX L Switch", "OUT MIXL" },
	{ "LOUT MIX", "OUTMIX R Switch", "OUT MIXR" },

	{ "MONOVOL MIX", "DAC R2 Switch", "DAC R2" },
	{ "MONOVOL MIX", "DAC L2 Switch", "DAC L2" },
	{ "MONOVOL MIX", "BST4 Switch", "BST4" },

	{ "MONOAmp MIX", "DAC L1 Switch", "DAC L1" },
	{ "MONOAmp MIX", "MONOVOL Switch", "MONOVOL MIX" },

	{ "PDM1 L Mux", "Stereo DAC", "Stereo DAC MIXL" },
	{ "PDM1 L Mux", "Mono DAC", "Mono DAC MIXL" },
	{ "PDM1 L Mux", NULL, "PDM1 Power" },
	{ "PDM1 R Mux", "Stereo DAC", "Stereo DAC MIXR" },
	{ "PDM1 R Mux", "Mono DAC", "Mono DAC MIXR" },
	{ "PDM1 R Mux", NULL, "PDM1 Power" },
	{ "PDM2 L Mux", "Stereo DAC", "Stereo DAC MIXL" },
	{ "PDM2 L Mux", "Mono DAC", "Mono DAC MIXL" },
	{ "PDM2 L Mux", NULL, "PDM2 Power" },
	{ "PDM2 R Mux", "Stereo DAC", "Stereo DAC MIXR" },
	{ "PDM2 R Mux", "Mono DAC", "Mono DAC MIXR" },
	{ "PDM2 R Mux", NULL, "PDM2 Power" },

	{ "HP Amp", NULL, "HPO MIX" },
	{ "HP Amp", NULL, "JD Power" },
	{ "HP Amp", NULL, "Mic Det Power" },
	{ "HPOL", NULL, "HP Amp" },
	{ "HPOL", NULL, "HP L Amp" },
	{ "HPOL", NULL, "Improve HP Amp Drv" },
	{ "HPOR", NULL, "HP Amp" },
	{ "HPOR", NULL, "HP R Amp" },
	{ "HPOR", NULL, "Improve HP Amp Drv" },

	{ "LOUT Amp", NULL, "LOUT MIX" },
	{ "LOUTL", NULL, "LOUT Amp" },
	{ "LOUTR", NULL, "LOUT Amp" },
	{ "LOUTL", NULL, "Improve LOUT Amp Drv" },
	{ "LOUTR", NULL, "Improve LOUT Amp Drv" },

	{ "Mono Amp", NULL, "MONOAmp MIX" },
	{ "MonoP", NULL, "Mono Amp" },
	{ "MonoN", NULL, "Mono Amp" },

	{ "PDM1L", NULL, "PDM1 L Mux" },
	{ "PDM1R", NULL, "PDM1 R Mux" },
	{ "PDM2L", NULL, "PDM2 L Mux" },
	{ "PDM2R", NULL, "PDM2 R Mux" },
};

static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt5671_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int val_len = 0, val_clk, mask_clk;
	int pre_div, bclk_ms, frame_size;

	rt5671->lrck[dai->id] = params_rate(params);

	if (dai->id == RT5671_AIF3)
		rt5671->lrck[dai->id] = rt5671->aif3_srate;

	pre_div = get_clk_info(rt5671->sysclk, rt5671->lrck[dai->id]);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}
	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}
	bclk_ms = frame_size > 32 ? 1 : 0;
	rt5671->bclk[dai->id] = rt5671->lrck[dai->id] * (32 << bclk_ms);
	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5671->bclk[dai->id], rt5671->lrck[dai->id]);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val_len |= RT5671_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val_len |= RT5671_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val_len |= RT5671_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}

	switch (dai->id) {
	case RT5671_AIF1:
		bclk_ms = 1;
        mask_clk = RT5671_I2S_PD1_MASK;
		val_clk = pre_div << RT5671_I2S_PD1_SFT;
		snd_soc_update_bits(codec, RT5671_I2S1_SDP,
			RT5671_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		if (bclk_ms == 1)
			snd_soc_update_bits(codec, RT5671_TDM_CTRL_1, 0x4c00, 0x4c00);
		else
			snd_soc_update_bits(codec, RT5671_TDM_CTRL_1, 0x4c00, 0x4000);
		break;
	case RT5671_AIF2:
/*
               
                                                                            
                                             
                                
*/
		//bclk_ms = 1;
/*              */
		mask_clk = RT5671_I2S_BCLK_MS2_MASK | RT5671_I2S_PD2_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS2_SFT |
			pre_div << RT5671_I2S_PD2_SFT;
		snd_soc_update_bits(codec, RT5671_I2S2_SDP,
			RT5671_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		break;
	case RT5671_AIF3:
		pr_debug("%s master:%d 0x%02x = 0x%04x\n", __func__, rt5671->master[RT5671_AIF3],
				RT5671_I2S3_SDP, snd_soc_read(codec, RT5671_I2S3_SDP));

		snd_soc_update_bits(codec, RT5671_I2S3_SDP,
					RT5671_I2S_MS_MASK | RT5671_I2S_BP_MASK |
					RT5671_I2S_DF_MASK, (RT5671_I2S_DF_PCM_A | RT5671_I2S_BP_INV));

		mask_clk = RT5671_I2S_BCLK_MS3_MASK | RT5671_I2S_PD3_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS3_SFT |
			pre_div << RT5671_I2S_PD3_SFT;
		snd_soc_update_bits(codec, RT5671_I2S3_SDP,
			RT5671_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5671_ADDA_CLK1, mask_clk, val_clk);
		break;
	case RT5671_AIF4:
	case RT5671_COMP:
		bclk_ms = 1;
		mask_clk = RT5671_I2S_BCLK_MS4_MASK | RT5671_I2S_PD4_MASK;
		val_clk = bclk_ms << RT5671_I2S_BCLK_MS4_SFT |
			pre_div << RT5671_I2S_PD4_SFT;
		snd_soc_update_bits(codec, RT5671_I2S4_SDP,
			RT5671_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5671_DSP_CLK, mask_clk, val_clk);
		break;
	}

	return 0;
}

static int rt5671_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->aif_pu = dai->id;
	rt5671->stream = substream->stream;

	return 0;
}

static int rt5671_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

    pr_debug("%s id:%d fmt:0x%x master:%d\n", __func__,
            dai->id, fmt, (fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM );

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5671->master[dai->id] = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg_val |= RT5671_I2S_MS_S;
		rt5671->master[dai->id] = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5671_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5671_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5671_I2S_DF_PCM_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5671_I2S_DF_PCM_B;
		break;
	default:
		return -EINVAL;
	}

	switch (dai->id) {
	case RT5671_AIF1:
		snd_soc_update_bits(codec, RT5671_I2S1_SDP,
			RT5671_I2S_MS_MASK | RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF2:
		snd_soc_update_bits(codec, RT5671_I2S2_SDP,
			RT5671_I2S_MS_MASK | RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF3:
		snd_soc_update_bits(codec, RT5671_I2S3_SDP,
			RT5671_I2S_MS_MASK | RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	case RT5671_AIF4:
	case RT5671_COMP:
		snd_soc_update_bits(codec, RT5671_I2S4_SDP,
			RT5671_I2S_MS_MASK | RT5671_I2S_BP_MASK |
			RT5671_I2S_DF_MASK, reg_val);
		break;
	}

	return 0;
}

static int rt5671_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

/*
	if (freq == rt5671->sysclk && clk_id == rt5671->sysclk_src)
		return 0;
*/
	switch (clk_id) {
	case RT5671_SCLK_S_MCLK:
		reg_val |= RT5671_SCLK_SRC_MCLK;
		break;
	case RT5671_SCLK_S_PLL1:
		reg_val |= RT5671_SCLK_SRC_PLL1;
		break;
	case RT5671_SCLK_S_RCCLK:
		reg_val |= RT5671_SCLK_SRC_RCCLK;
		break;
	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}
	snd_soc_update_bits(codec, RT5671_GLB_CLK,
		RT5671_SCLK_SRC_MASK, reg_val);
	rt5671->sysclk = freq;
	rt5671->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt5671_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K and bypass flag.
 *
 * Calcualte M/N/K code to configure PLL for codec. And K is assigned to 2
 * which make calculation more efficiently.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5671_pll_code *pll_code)
{
	int max_n = RT5671_PLL_N_MAX, max_m = RT5671_PLL_M_MAX;
	int k, n = 0, m = 0, red, n_t, m_t, pll_out, in_t;
	int out_t, red_t = abs(freq_out - freq_in);
	bool bypass = false;

	if (RT5671_PLL_INP_MAX < freq_in || RT5671_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5671_PLL_K_MAX)
		k = RT5671_PLL_K_MAX;
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k + 2);
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	pr_debug("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;
	return 0;
}

static int rt5671_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	struct rt5671_pll_code pll_code;
	int ret;
/*
	if (source == rt5671->pll_src && freq_in == rt5671->pll_in &&
		freq_out == rt5671->pll_out)
		return 0;
*/
	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5671->pll_in = 0;
		rt5671->pll_out = 0;
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_MCLK);
		return 0;
	}

	switch (source) {
	case RT5671_PLL1_S_MCLK:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_MCLK);
		break;
	case RT5671_PLL1_S_BCLK1:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK1);
		break;
	case RT5671_PLL1_S_BCLK2:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK2);
		break;
	case RT5671_PLL1_S_BCLK3:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK3);
		break;
	case RT5671_PLL1_S_BCLK4:
		snd_soc_update_bits(codec, RT5671_GLB_CLK,
			RT5671_PLL1_SRC_MASK, RT5671_PLL1_SRC_BCLK4);
		break;
	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5671_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "bypass=%d m=%d n=%d k=%d\n",
		pll_code.m_bp, (pll_code.m_bp ? 0 : pll_code.m_code),
		pll_code.n_code, pll_code.k_code);

	snd_soc_write(codec, RT5671_PLL_CTRL1,
		pll_code.n_code << RT5671_PLL_N_SFT | pll_code.k_code);
	snd_soc_write(codec, RT5671_PLL_CTRL2,
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5671_PLL_M_SFT |
		pll_code.m_bp << RT5671_PLL_M_BP_SFT);

	rt5671->pll_in = freq_in;
	rt5671->pll_out = freq_out;
	rt5671->pll_src = source;

	return 0;
}

/**
 * rt5671_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5671_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i < 0xff; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5671_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN,
				"%04x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5671_index_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val = 0, addr = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf+i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}

	for (i = i + 1; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf+i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i)-'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5671_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
			rt5671_index_read(codec, addr));
	else
		rt5671_index_write(codec, addr, val);


	return count;
}
static DEVICE_ATTR(index_reg, 0644, rt5671_index_show, rt5671_index_store);

static ssize_t rt5671_codec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i <= RT5671_VENDOR_ID2; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;

		if (rt5671_readable_register(codec, i)) {
			val = snd_soc_read(codec, i);

			cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN,
					"%04x: %04x\n", i, val);
		}
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5671_codec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val = 0, addr = 0;
	int i;

	pr_debug("register \"%s\" count=%d\n", buf, count);
	for (i = 0; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}

	for (i = i+1; i < count; i++) {
		if (*(buf+i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf+i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a')+0xa);
		else if (*(buf+i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A')+0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (addr > RT5671_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_debug("0x%02x = 0x%04x\n", addr,
			snd_soc_read(codec, addr));
	else
		snd_soc_write(codec, addr, val);

	return count;
}

static DEVICE_ATTR(codec_reg, 0644, rt5671_codec_show, rt5671_codec_store);
static ssize_t rt5671_codec_adb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val;
	int cnt = 0, i;

	for (i = 0; i < rt5671->adb_reg_num; i++) {
		if (cnt + RT5671_REG_DISP_LEN >= PAGE_SIZE)
			break;

		switch (rt5671->adb_reg_addr[i] & 0x30000) {
		case 0x10000:
			val = rt5671_index_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
			break;
		case 0x20000:
			val = rt5671_dsp_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
			break;
		default:
			val = snd_soc_read(codec, rt5671->adb_reg_addr[i] & 0xffff);
		}

		cnt += snprintf(buf + cnt, RT5671_REG_DISP_LEN, "%05x: %04x\n",
			rt5671->adb_reg_addr[i], val);
	}

	return cnt;
}

static ssize_t rt5671_codec_adb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int value = 0;
	int i = 2, j = 0;

	if (buf[0] == 'R' || buf[0] == 'r') {
		while (j < 0x100 && i < count) {
			rt5671->adb_reg_addr[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;

			rt5671->adb_reg_addr[j] = value;
			j++;
		}
		rt5671->adb_reg_num = j;
	} else if (buf[0] == 'W' || buf[0] == 'w') {
		while (j < 0x100 && i < count) {
			/* Get address */
			rt5671->adb_reg_addr[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;
			rt5671->adb_reg_addr[j] = value;

			/* Get value */
			rt5671->adb_reg_value[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a')+0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A')+0xa);
				else
					break;
			}
			i++;
			rt5671->adb_reg_value[j] = value;

			j++;
		}

		rt5671->adb_reg_num = j;

		for (i = 0; i < rt5671->adb_reg_num; i++) {
			switch (rt5671->adb_reg_addr[i] & 0x30000) {
			case 0x10000:
				rt5671_index_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
				break;
			case 0x20000:
				rt5671_dsp_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
				break;
			default:
				snd_soc_write(codec,
					rt5671->adb_reg_addr[i] & 0xffff,
					rt5671->adb_reg_value[i]);
			}
		}

	}

	return count;
}
static DEVICE_ATTR(codec_reg_adb, 0644, rt5671_codec_adb_show, rt5671_codec_adb_store);

static int rt5671_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
#ifdef DELAYED_OFF
    static bool booted = false;
#endif
	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(codec->dev, "%s: SND_SOC_BIAS_ON \n", __FUNCTION__);
		if (rt5671->vad_en) {
#ifdef VAD_CAPTURE_TEST /* when merged to soc(dsp), this code should be removed. */
			if (rt5671->stream == SNDRV_PCM_STREAM_CAPTURE){
				snd_soc_write(codec, RT5671_VAD_CTRL1, 0x277c);
				printk("[%s] Sending ACK to Codec...!\n", __func__);
			}
			else
				snd_soc_write(codec, RT5671_VAD_CTRL1, 0x2784);
#endif
		}
		break;

	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "%s: SND_SOC_BIAS_PREPARE \n", __FUNCTION__);
		snd_soc_update_bits(codec, RT5671_CHARGE_PUMP,
			RT5671_OSW_L_MASK | RT5671_OSW_R_MASK,
			RT5671_OSW_L_DIS | RT5671_OSW_R_DIS);
		break;

	case SND_SOC_BIAS_STANDBY:
		dev_dbg(codec->dev, "%s: SND_SOC_BIAS_STANDBY \n", __FUNCTION__);
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level) {
#ifdef DELAYED_OFF
			if(cancel_delayed_work(&rt5671->delayed_off_work) == true)
                dev_dbg(codec->dev, "%s: delayed_off_work was pending and canceled\n", __func__);
            else
                dev_dbg(codec->dev, "%s: delayed_off_work wasn't pending", __func__);
#endif
			snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_LDO_SEL_MASK, 0x3);
			rt5671_index_update_bits(codec, 0x15, 0x7, 0x7);
			snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x1);
			snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_PWR_VREF1 | RT5671_PWR_MB |
				RT5671_PWR_BG | RT5671_PWR_VREF2,
				RT5671_PWR_VREF1 | RT5671_PWR_MB |
				RT5671_PWR_BG | RT5671_PWR_VREF2);
			mdelay(10);
			snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
				RT5671_PWR_JD1, RT5671_PWR_JD1);
			snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
				RT5671_PWR_FV1 | RT5671_PWR_FV2,
				RT5671_PWR_FV1 | RT5671_PWR_FV2);
			codec->cache_only = false;
			codec->cache_sync = 1;
			snd_soc_cache_sync(codec);
			rt5671_index_sync(codec);
		}
		break;

	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "%s: SND_SOC_BIAS_OFF \n", __FUNCTION__);
/* Oder 130620 start */
		if (rt5671->vad_en) {
			snd_soc_update_bits(codec, RT5671_GLB_CLK,
				RT5671_SCLK_SRC_MASK, RT5671_SCLK_SRC_MCLK);
			snd_soc_update_bits(codec, RT5671_I2S1_SDP,
				RT5671_I2S_MS_MASK, RT5671_I2S_MS_M);
#if REALTEK_USE_AMIC
			snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL, RT5671_L_MUTE, RT5671_L_MUTE);
			snd_soc_write(codec, RT5671_PWR_ANLG1, 0xe81b);
			snd_soc_write(codec, RT5671_PWR_DIG2, 0x8001);
			snd_soc_write(codec, RT5671_PWR_DIG1, 0x8008);
			snd_soc_write(codec, RT5671_PWR_ANLG2, 0x4224);
			snd_soc_write(codec, RT5671_PWR_MIXER, 0x0201);
			rt5671_index_write(codec, 0x39, 0x3640);
			snd_soc_write(codec, RT5671_STO1_ADC_MIXER, 0x3c20);
			snd_soc_write(codec, RT5671_REC_MONO2_MIXER, 0x001b);
			snd_soc_write(codec, RT5671_DIG_INF1_DATA, 0x9002);
			/* snd_soc_write(codec, RT5671_TDM_CTRL_1, 0x4200); */
			snd_soc_write(codec, RT5671_TDM_CTRL_1, 0x0e00);
			/* snd_soc_write(codec, RT5671_IN1_CTRL2, 0x08a7); */
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x2784);
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x279c);
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x273c);
			snd_soc_write(codec, RT5671_IRQ_CTRL2, 0x0040);
			snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL, RT5671_L_MUTE, 0);
#else
			/* Oder 130701 update for DMIC1 start 19.2M to 16k */
#ifdef SWITCH_FS_FOR_VAD
			snd_soc_write(codec, RT5671_ADDA_CLK1, 0xc000);/* 48KHZ --> 16KHZ */
			snd_soc_write(codec, RT5671_DMIC_CTRL1, 0x9106);
			snd_soc_write(codec, RT5671_PLL_CTRL1, 0x2a82);/* 19.2M --> 48k*512 */
			snd_soc_write(codec, RT5671_PLL_CTRL2, 0xf000);/* 19.2M --> 48K*512 */
#endif
			snd_soc_update_bits(codec, RT5671_GPIO_CTRL1, 0x4000, 0x4000);
			snd_soc_write(codec, RT5671_PWR_DIG2, 0x8001);
			snd_soc_write(codec, RT5671_PWR_DIG1, 0x8000);
#ifndef SWITCH_FS_FOR_VAD
			snd_soc_write(codec, RT5671_ADDA_CLK1, 0x2770);
			snd_soc_write(codec, RT5671_DMIC_CTRL1, 0x5545);
#endif
			snd_soc_write(codec, RT5671_STO1_ADC_MIXER, 0x5960);
			snd_soc_write(codec, RT5671_DIG_INF1_DATA, 0x9002);
			/* snd_soc_write(codec, RT5671_TDM_CTRL_1, 0x4200); */
			snd_soc_write(codec, RT5671_TDM_CTRL_1, 0x0e00);
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x2784);
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x279c);
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x273c);
			snd_soc_write(codec, RT5671_IRQ_CTRL2, 0x0040);
			snd_soc_update_bits(codec, RT5671_STO1_ADC_DIG_VOL, RT5671_L_MUTE, 0);
			/* Oder 130701 update end */
#endif
		} else {
			if (rt5671->jack_type == SND_JACK_HEADSET) {
				snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
				snd_soc_write(codec, RT5671_PWR_DIG2, 0x0001);
				snd_soc_write(codec, RT5671_PWR_VOL, 0x0020);
				snd_soc_write(codec, RT5671_PWR_MIXER, 0x0001);
				snd_soc_write(codec, RT5671_PWR_ANLG1, 0x2003);
				snd_soc_write(codec, RT5671_PWR_ANLG2, 0x0404);
			} else {
#ifndef DELAYED_OFF
				snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
				snd_soc_write(codec, RT5671_PWR_DIG2, 0x0001);
#endif
				snd_soc_write(codec, RT5671_PWR_VOL, 0x0000);
				snd_soc_write(codec, RT5671_PWR_MIXER, 0x0001);
				//XXX Younghyun Jo added start
				if (!rt5671_is_revision_g(codec)) {
					snd_soc_write(codec, RT5671_PWR_ANLG1, 0x0001);
					snd_soc_write(codec, RT5671_PWR_ANLG2, 0x0000);
				}
				//XXX Younghyun Jo added end
#ifndef DELAYED_OFF
				snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x0);
#endif
				rt5671_index_update_bits(codec, 0x15, 0x7, 0x0);// 0416
#ifdef DELAYED_OFF
				if (!booted)
					booted = true;
				else{
					//Add this to prevent i2c xfer after i2c chip is in suspend mode
					if(rt5671->suspend == 0){
						dev_dbg(codec->dev, "%s: Queueing delayed_off_work... \n", __FUNCTION__);
						schedule_delayed_work(&rt5671->delayed_off_work, msecs_to_jiffies(2000));
					}
					else{
						dev_dbg(codec->dev,
						"%s: Ignoring queing delayed_off_work in suspend mode\n", __FUNCTION__);
					}
				}
#endif
			}
			snd_soc_write(codec, RT5671_VAD_CTRL1, 0x2784);
		}
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

#ifdef DELAYED_OFF
static void rt5671_do_delayed_off_work(struct work_struct *work)
{
	struct rt5671_priv *rt5671 =
		container_of(work, struct rt5671_priv, delayed_off_work.work);
	struct snd_soc_codec *codec = rt5671->codec;

	dev_dbg(codec->dev, "%s:enter \n", __FUNCTION__);

	snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
	snd_soc_write(codec, RT5671_PWR_DIG2, 0x0001);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x0);
}
#endif

static int rt5671_probe(struct snd_soc_codec *codec)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
#endif
#endif
	int ret;

	pr_info("Codec driver version %s\n", VERSION);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	rt5671_reset(codec);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_HP_L | RT5671_PWR_HP_R |
		RT5671_PWR_VREF2, RT5671_PWR_VREF2);
	msleep(100);

	rt5671_reset(codec);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_VREF1 | RT5671_PWR_MB |
		RT5671_PWR_BG | RT5671_PWR_VREF2,
		RT5671_PWR_VREF1 | RT5671_PWR_MB |
		RT5671_PWR_BG | RT5671_PWR_VREF2);
	mdelay(10);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
		RT5671_PWR_FV1 | RT5671_PWR_FV2,
		RT5671_PWR_FV1 | RT5671_PWR_FV2);
	/* DMIC */
	if (rt5671->dmic_en == RT5671_DMIC1) {
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_1L_LH_MASK | RT5671_DMIC_1R_LH_MASK,
			RT5671_DMIC_1L_LH_FALLING | RT5671_DMIC_1R_LH_RISING);
	} else if (rt5671->dmic_en == RT5671_DMIC2) {
		snd_soc_update_bits(codec, RT5671_GPIO_CTRL1,
			RT5671_GP2_PIN_MASK, RT5671_GP2_PIN_DMIC1_SCL);
		snd_soc_update_bits(codec, RT5671_DMIC_CTRL1,
			RT5671_DMIC_2L_LH_MASK | RT5671_DMIC_2R_LH_MASK,
			RT5671_DMIC_2L_LH_FALLING | RT5671_DMIC_2R_LH_RISING);
	}

	rt5671_reg_init(codec);
#ifdef JD1_FUNC
	snd_soc_update_bits(codec, RT5671_PWR_ANLG1,
			RT5671_PWR_MB | RT5671_PWR_BG,
			RT5671_PWR_MB | RT5671_PWR_BG);
	snd_soc_update_bits(codec, RT5671_PWR_ANLG2,
			RT5671_PWR_JD1,
			RT5671_PWR_JD1);
#endif

	snd_soc_update_bits(codec, RT5671_PWR_ANLG1, RT5671_LDO_SEL_MASK, 0x3);//0416 LDO Power to 1.2V

	codec->dapm.bias_level = SND_SOC_BIAS_OFF;
	rt5671->codec = codec;
	rt5671->combo_jack_en = true; /* enable combo jack */

	snd_soc_add_codec_controls(codec, rt5671_snd_controls,
			ARRAY_SIZE(rt5671_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5671_dapm_widgets,
			ARRAY_SIZE(rt5671_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5671_dapm_routes,
			ARRAY_SIZE(rt5671_dapm_routes));
	rt5671->dsp_sw = RT5671_DSP_HANDSET_WB;
	rt5671_dsp_probe(codec);

#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	ioctl_ops->index_write = rt5671_index_write;
	ioctl_ops->index_read = rt5671_index_read;
	ioctl_ops->index_update_bits = rt5671_index_update_bits;
	ioctl_ops->ioctl_common = rt5671_ioctl_common;
	realtek_ce_init_hwdep(codec);
#endif
#endif

#ifdef DELAYED_OFF
	INIT_DELAYED_WORK(&rt5671->delayed_off_work, rt5671_do_delayed_off_work);
#endif

	ret = device_create_file(codec->dev, &dev_attr_index_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}

	ret = device_create_file(codec->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codec_reg sysfs files: %d\n", ret);
		return ret;
	}

	ret = device_create_file(codec->dev, &dev_attr_codec_reg_adb);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codec_reg_adb sysfs files: %d\n", ret);
		return ret;
	}

	rt5671->jack_type = 0;
	rt5671->dsp_depoping_in_dsp = 0;

#ifdef IGNORE_SUSPEND
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
#endif

	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int rt5671_remove(struct snd_soc_codec *codec)
{
	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int rt5671_suspend(struct snd_soc_codec *codec)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s: enter\n", __func__);
#ifdef DELAYED_OFF
	if(cancel_delayed_work(&rt5671->delayed_off_work) == true)
		dev_dbg(codec->dev, "%s: delayed_off_work was pending and canceled\n", __func__);
	else
		dev_dbg(codec->dev, "%s: delayed_off_work wasn't pending", __func__);
#endif
	rt5671_dsp_suspend(codec);
	/* Move to VAD mode */
	rt5671->suspend = 1;
	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
#ifdef DELAYED_OFF
	snd_soc_write(codec, RT5671_PWR_DIG1, 0x0000);
	snd_soc_write(codec, RT5671_PWR_DIG2, 0x0001);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL1, 0x1, 0x0);
#endif

	snd_soc_update_bits(codec, RT5671_I2S1_SDP, RT5671_I2S_MS_MASK,
		RT5671_I2S_MS_S);
	snd_soc_update_bits(codec, RT5671_I2S2_SDP, RT5671_I2S_MS_MASK,
		RT5671_I2S_MS_S);
	snd_soc_update_bits(codec, RT5671_I2S3_SDP, RT5671_I2S_MS_MASK,
		RT5671_I2S_MS_S);
	snd_soc_update_bits(codec, RT5671_I2S4_SDP, RT5671_I2S_MS_MASK,
		RT5671_I2S_MS_S);
	return 0;
}

static int rt5671_resume(struct snd_soc_codec *codec)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "%s: enter\n", __func__);
	/* Disable VAD mode start */
	rt5671->vad_en = 0;
	rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
	rt5671->suspend = 0;
	/* Disable VAD mode end */
	rt5671_dsp_resume(codec);

	return 0;
}
#else
#define rt5671_suspend NULL
#define rt5671_resume NULL
#endif

static void rt5671_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	pr_debug("enter %s\n", __func__);
}

static void rt5671_hw_dump(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int id = substream->pcm->device;

	unsigned int val;
	int cnt = 0, i;

	dev_info(codec->dev, "%i:RT5671 register\n", id);
	for (i = 0; i <= RT5671_VENDOR_ID2; i++) {
		if (rt5671_readable_register(codec, i)) {
			val = snd_soc_read(codec, i);
			dev_info(codec->dev, "%i:%02x: %04x\n", id, i, val);
		}
	}

	dev_info(codec->dev, "%i:RT5671 index register\n", id);
	for (i = 0; i < 0xff; i++) {
		val = rt5671_index_read(codec, i);
		if (!val)
			continue;
		dev_info(codec->dev, "%i:%02x: %04x\n", id, i, val);
	}

}

#define RT5671_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define RT5671_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5671_aif_dai_ops = {
	.hw_params = rt5671_hw_params,
	.prepare = rt5671_prepare,
	.set_fmt = rt5671_set_dai_fmt,
	.set_sysclk = rt5671_set_dai_sysclk,
	.set_pll = rt5671_set_dai_pll,
	.shutdown = rt5671_shutdown,
	.hw_dump = rt5671_hw_dump,
};

struct snd_soc_dai_driver rt5671_dai[] = {
	{
		.name = "rt5671-aif1",
		.id = RT5671_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif2",
		.id = RT5671_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif3",
		.id = RT5671_AIF3,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-aif4",
		.id = RT5671_AIF4,
		.playback = {
			.stream_name = "AIF4 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.capture = {
			.stream_name = "AIF4 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "rt5671-compress",
		.id = RT5671_COMP,
		.playback = {
			.stream_name = "AIF4 Compress",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5671_STEREO_RATES,
			.formats = RT5671_FORMATS,
		},
		.ops = &rt5671_aif_dai_ops,
		.symmetric_rates = 1,
	},


};

static struct snd_soc_codec_driver soc_codec_dev_rt5671 = {
	.probe = rt5671_probe,
	.remove = rt5671_remove,
	.suspend = rt5671_suspend,
	.resume = rt5671_resume,
	.set_bias_level = rt5671_set_bias_level,
	.idle_bias_off = true,
	.reg_cache_size = RT5671_VENDOR_ID2 + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5671_reg,
	.volatile_register = rt5671_volatile_register,
	.readable_register = rt5671_readable_register,
	.reg_cache_step = 1,
};

static const struct i2c_device_id rt5671_i2c_id[] = {
	{ "rt5671", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5671_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id rt5671_of_match[] = {
	{ .compatible = "realtek,rt5671", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt5671_of_match);
#endif

static int rt5671_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct rt5671_priv *rt5671;
	int ret;

	printk("%s %d\n",__FUNCTION__, __LINE__);

	rt5671 = kzalloc(sizeof(struct rt5671_priv), GFP_KERNEL);
	if (NULL == rt5671)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5671);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5671,
			rt5671_dai, ARRAY_SIZE(rt5671_dai));
	if (ret < 0)
		kfree(rt5671);

	return ret;
}

static int rt5671_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

void rt5671_i2c_shutdown(struct i2c_client *client)
{
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;

	pr_debug("enter %s\n", __func__);
	if (codec != NULL)
		rt5671_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

static unsigned short normal_i2c[] = { I2C_CLIENT_END };

struct i2c_driver rt5671_i2c_driver = {
	.driver = {
		.name = "rt5671",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rt5671_of_match,
#endif
	},
	.probe = rt5671_i2c_probe,
	.remove = rt5671_i2c_remove,
	.shutdown = rt5671_i2c_shutdown,
	.id_table = rt5671_i2c_id,
};

module_i2c_driver(rt5671_i2c_driver);

MODULE_DESCRIPTION("ASoC RT5671 driver");
MODULE_AUTHOR("Johnny Hsu <johnnyhsu@realtek.com>");
MODULE_LICENSE("GPL");
/*              */
