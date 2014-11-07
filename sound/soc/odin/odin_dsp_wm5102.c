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
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include <linux/mfd/arizona/registers.h>
#include "odin_asoc_utils.h"
#include "../codecs/wm5102.h"

#define CODEC_MASTER_MODE 	1

static int odin_dsp_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	int mclk = 0;
	int codec_sysclk = 0;

#ifdef CODEC_MASTER_MODE
	unsigned int wm5102_sync_clk = 0;
	unsigned int machine_mclk = 24000000;

	wm5102_sync_clk = params_rate(params) * 512;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(dev, "Unable to set codec DAIFMT\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(dev, "Unable to set CPU DAIFMT\n");
		return ret;
	}

	/* arizona_dai_set_sysclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
								wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai sysclk\n");
		return ret;
	}

	/* odin_i2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0,
							machine_mclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	/* arizona_set_sysclk */
	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
						ARIZONA_CLK_SRC_FLL1, wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec sysclk\n");
		return ret;
	}

	/* wm5102_set_fll */
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
					machine_mclk, wm5102_sync_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec pll\n");
		return ret;
	}
#else
	/* Set the Codec DAI configuration */
	/* arizona_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}

	/* Set the AP DAI configuration */
	/* odin_platform_dai_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai format\n");
		return ret;
	}

	if ( params_rate(params) >= 44100 )
		mclk = 512 * params_rate(params);
	else
		mclk = 1024 * params_rate(params);

	/* odin_platform_dai_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	/* arizona_dai_set_sysclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, mclk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai sysclk\n");
		return ret;
	}

	/* arizona_set_sysclk */
	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
					ARIZONA_CLK_SRC_MCLK1, mclk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec sysclk\n");
		return ret;
	}
#endif
	return ret;
}

static int odin_dsp_wm5102_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_write(codec, ARIZONA_NOISE_GATE_SELECT_1L, 0);
	snd_soc_write(codec, ARIZONA_NOISE_GATE_SELECT_1R, 0);

	snd_soc_dapm_enable_pin(dapm, "Main Mic");
	//snd_soc_dapm_enable_pin(dapm, "OnboardMic2");
	//snd_soc_dapm_enable_pin(dapm, "OnboardMic3");
	snd_soc_dapm_enable_pin(dapm, "Headset Mic");

	return 0;
}

static struct snd_soc_ops odin_dsp_ops = {
	.hw_params = odin_dsp_hw_params,
};

static const struct snd_soc_dapm_widget odin_dsp_wm5102_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_SPK("Earpeice", NULL),
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Recog Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
};

static const struct snd_soc_dapm_route odin_dsp_wm5102_audio_map[] = {
	{"Speaker", NULL, "SPKOUTLN"},
	{"Speaker", NULL, "SPKOUTLP"},

	{"Earpeice", NULL, "EPOUTN"},
	{"Earpeice", NULL, "EPOUTP"},

	{"Headphone", NULL, "HPOUT1L"},
	{"Headphone", NULL, "HPOUT1R"},

	{"IN1L", NULL, "MICBIAS1"},
	{"MICBIAS1", NULL, "Main Mic"},
	{"IN1R", NULL, "MICBIAS2"},
	{"MICBIAS2", NULL, "Sub Mic"},

	{"IN2L", NULL, "MICBIAS1"},
	{"MICBIAS1", NULL, "Recog Mic"},
	{"IN2R", NULL, "MICBIAS3"},
	{"MICBIAS3", NULL, "Headset Mic"},
};

static int odin_wm5102_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
};

static const struct snd_kcontrol_new odin_dsp_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Earpeice"),
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Recog Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static struct snd_soc_dai_link odin_dsp_dai_link[] = {
	{
		.name = "WM5102_l0",
		.stream_name = "WM5102 HiFi",
		.cpu_dai_name = "pcm-cpu-dai",
		.codec_dai_name = "wm5102-aif1",
		.platform_name = "odin-platform",
		.codec_name = "wm5102-codec",
		.init = odin_dsp_wm5102_init,
		.ops = &odin_dsp_ops,
	},
};

static struct snd_soc_card odin_dsp_card = {
	//.name = "OdinAudioCard",
	.name = "ODIN_SOUND_CARD",
	.owner = THIS_MODULE,
	.dai_link = odin_dsp_dai_link,
	.num_links = ARRAY_SIZE(odin_dsp_dai_link),

	.controls = odin_dsp_wm5102_controls,
	.num_controls = ARRAY_SIZE(odin_dsp_wm5102_controls),
	.dapm_widgets = odin_dsp_wm5102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(odin_dsp_wm5102_dapm_widgets),
	.dapm_routes = odin_dsp_wm5102_audio_map,
	.num_dapm_routes = ARRAY_SIZE(odin_dsp_wm5102_audio_map),
};

static int odin_dsp_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &odin_dsp_card;
	int ret;

	card->dev = &pdev->dev;

	ret = odin_of_dai_property_get(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get property:%d\n", ret);
		//return -EINVAL;
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register soc_card:%d\n", ret);
		//return -EINVAL;
		goto err;
	}
	dev_info(&pdev->dev, "Success!!\n");

	return 0;
err:
	return ret;
}

static int odin_dsp_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	return snd_soc_unregister_card(card);
}

static const struct of_device_id odin_dsp_audio_match[] = {
	{ .compatible = "lge,odin-dsp-audio", },
	{},
};

static struct platform_driver odin_dsp_audio_driver = {
	.driver		= {
		.name	= "odin-dsp-audio",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_dsp_audio_match),
	},
	.probe		= odin_dsp_audio_probe,
	.remove		= __devexit_p(odin_dsp_audio_remove),
};

module_platform_driver(odin_dsp_audio_driver);
MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("ALSA SoC Odin WM5102");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-dsp-audio");
