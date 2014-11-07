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

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <asm/mach-types.h>

#include "../codecs/wm8580.h"

/*
 * Default CFG switch settings to use this driver:
 *
 *   Set CFG1 1-3 Off, CFG2 1-4 On
 */

/* WODEN FPGA has a 12MHZ crystal attached to WM8580 */
#define WODEN_WM8580_FREQ 12000000

static int woden_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	//struct snd_soc_codec *codec = rtd->codec;
	unsigned int pll_out;
	int bfs, rfs, ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S8:
		bfs = 16;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	/* The Fvco for WM8580 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
		rfs = 512;
		break;
	default:
		return -EINVAL;
	}
	pll_out = params_rate(params) * rfs;

#if 1
	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, 
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8580 to drive MCLK from its PLLA */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_PLLA);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA, 0,
					WODEN_WM8580_FREQ, pll_out);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8580_CLKSRC_PLLA,
				     pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;
#endif

	//wm8580_woden_fpga_setting(codec);	

	return 0;
}

/*
 * WODEN WM8580 DAI operations.
 */
static struct snd_soc_ops woden_ops = {
	.hw_params = woden_hw_params,
};

/* WODEN Playback widgets */
static const struct snd_soc_dapm_widget woden_wm8580_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Front", NULL),
	SND_SOC_DAPM_HP("Center+Sub", NULL),
	SND_SOC_DAPM_HP("Rear", NULL),

	SND_SOC_DAPM_MIC("MicIn", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
};

/* WODEN-PAIFTX connections */
static const struct snd_soc_dapm_route woden_wm8580_audio_map[] = {
	/* MicIn feeds AINL */
	{"AINL", NULL, "MicIn"},

	/* LineIn feeds AINL/R */
	{"AINL", NULL, "LineIn"},
	{"AINR", NULL, "LineIn"},

	/* Front Left/Right are fed VOUT1L/R */
	{"Front", NULL, "VOUT1L"},
	{"Front", NULL, "VOUT1R"},

	/* Center/Sub are fed VOUT2L/R */
	{"Center+Sub", NULL, "VOUT2L"},
	{"Center+Sub", NULL, "VOUT2R"},

	/* Rear Left/Right are fed VOUT3L/R */
	{"Rear", NULL, "VOUT3L"},
	{"Rear", NULL, "VOUT3R"},
};

enum {
	HIFI = 0,
};

static struct snd_soc_dai_link woden_dai[] = {
	[HIFI] = { /* Primary Playback i/f */
		.name = "WM8580",
		.stream_name = "WM8580 HiFi",
		.cpu_dai_name = "woden-i2s.0",
		.codec_dai_name = "wm8580-hifi-playback",
		.platform_name = "odin-pcm-audio",
		.codec_name = "wm8580.0-001b",
		.ops = &woden_ops,
	},
};

static struct snd_soc_card woden = {
	.name = "WODEN_FPGA_WM8580",
	.owner = THIS_MODULE,
	.dai_link = woden_dai,
	.num_links = ARRAY_SIZE(woden_dai),
#if 0
	.dapm_widgets = woden_wm8580_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(woden_wm8580_dapm_widgets),
	.dapm_routes = woden_wm8580_audio_map,
	.num_dapm_routes = ARRAY_SIZE(woden_wm8580_audio_map),
#endif	
};

static struct platform_device *woden_snd_device;

static int __init woden_audio_init(void)
{
	int ret;

	woden_snd_device = platform_device_alloc("soc-audio", -1);
	if (!woden_snd_device)
		return -ENOMEM;

	platform_set_drvdata(woden_snd_device, &woden);
	ret = platform_device_add(woden_snd_device);

	if (ret)
		platform_device_put(woden_snd_device);

	return ret;
}
module_init(woden_audio_init);

static void __exit woden_audio_exit(void)
{
	platform_device_unregister(woden_snd_device);
}
module_exit(woden_audio_exit);

MODULE_AUTHOR("Hyunhee Jeon, hyunhee.jeon@lge.com");
MODULE_DESCRIPTION("ALSA SoC WODEN FPGA WM8580");
MODULE_LICENSE("GPL");

