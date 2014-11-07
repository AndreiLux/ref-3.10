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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include "odin_asoc_utils.h"
//
static int odin_hw_params(struct snd_pcm_substream *substream,
           struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	unsigned int sysclk;

	dev_dbg(dev, "rate %d, ch %d, psize %d, pcount %d, buffer(%d,%d) \n",
						params_rate(params),
						params_channels(params),
						params_period_size(params),
						params_periods(params),
						params_buffer_size(params),
						params_buffer_bytes(params));

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0){
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}

	/* Set the AP DAI configuration */
	/* odin_si2s_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0){
		dev_err(dev, "Failed to set cpu_dai format\n");
		return ret;
	}

	/* sysclk = params_rate(params) * 256; */
	sysclk = 24000000;

	/* odin_si2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, sysclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	/* Use the original MCLK */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 12288000,
					SND_SOC_CLOCK_OUT);
	if (ret < 0){
		dev_err(dev, "Failed to set codec pll\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops odin_ops = {
	.hw_params = odin_hw_params,
};

static struct snd_soc_compr_ops odin_compr_ops = {
	//.set_params = odin_compr_set_params,
};

static int odin_ap_master_hw_params(struct snd_pcm_substream *substream,
           struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	int ret;
	unsigned int sysclk;

	dev_dbg(dev, "rate %d, ch %d, psize %d, pcount %d, buffer(%d,%d) \n",
						params_rate(params),
						params_channels(params),
						params_period_size(params),
						params_periods(params),
						params_buffer_size(params),
						params_buffer_bytes(params));

	/* Set the AP DAI configuration */
	/* odin_si2s_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0){
		dev_err(dev, "Failed to set cpu_dai format\n");
		return ret;
	}

	/* sysclk = params_rate(params) * 256; */
	sysclk = params_rate(params) * 256;

	/* odin_si2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, sysclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops odin_ap_master_ops = {
	.hw_params = odin_ap_master_hw_params,
};

static int odin_ap_slave_hw_params(struct snd_pcm_substream *substream,
           struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	int ret;
	unsigned int sysclk;

	dev_dbg(dev, "rate %d, ch %d, psize %d, pcount %d, buffer(%d,%d) \n",
						params_rate(params),
						params_channels(params),
						params_period_size(params),
						params_periods(params),
						params_buffer_size(params),
						params_buffer_bytes(params));

	/* Set the AP DAI configuration */
	/* odin_si2s_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0){
		dev_err(dev, "Failed to set cpu_dai format\n");
		return ret;
	}

	/* sysclk = params_rate(params) * 256; */
	sysclk = 24000000;

	/* odin_si2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, sysclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops odin_ap_slave_ops = {
	.hw_params = odin_ap_slave_hw_params,
};

static struct snd_soc_dai_driver voice_dai[] = {
	{
		.name = "odin-compress-dsp-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.compress_dai = 1,
	},
	{
		.name = "odin-lpa-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	},
};

static struct snd_soc_dai_link odin_dai_link[] = {

	{
		.name = "Playback Normal",
		.stream_name = "RT5671 Playback",
		.cpu_dai_name = "odin-si2s.0",
		.codec_dai_name = "rt5671-aif1",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_ops,
	},
	{
		.name = "Capture Normal",
		.stream_name = "RT5671 Capture",
		.cpu_dai_name = "odin-si2s.1",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "odin-pcm-audio",
		.codec_name = "snd-soc-dummy",
		.ops = &odin_ap_master_ops,
	},
	{
		.name = "Playback HiFi",
		.stream_name = "RT5671 HiFi Playback",
		.cpu_dai_name = "odin-si2s.2",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "odin-pcm-audio",
		.codec_name = "snd-soc-dummy",
		.ops = &odin_ap_slave_ops,
	},
/*
	{
		.name = "DSP Compress LPA",
		.stream_name = "RT5671 Compress LPA Dai",
		.cpu_dai_name ="odin-compress-dsp-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "odin-compress-dsp",
		.codec_name = "snd-soc-dummy",
		.compr_ops = &odin_compr_ops,
		.dynamic = 1,
		.no_pcm = 1,
	},
	{
		.name = "LPA",
		.stream_name = "RT5671 LPA",
		.cpu_dai_name = "34613000.i2s",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &odin_ap_master_ops,
	},
*/
	{
		.name = "DSP Compress LPA",
		.stream_name = "RT5671 Compress LPA Dai",
		//.cpu_dai_name = "odin-si2s.2",
		//.cpu_dai_name = "odin-voice-dai",
		//.cpu_dai_name = "34613000.i2s",
		.cpu_dai_name ="odin-compress-dsp-dai",
		.codec_dai_name = "rt5671-aif4",
		.platform_name = "odin-compress-dsp",
		.codec_name = "rt5671.4-001c",
		.compr_ops = &odin_compr_ops,
		.dynamic = 1,
		.no_pcm = 1,
	},
};

static struct snd_soc_card odin_slt_card = {
	.name = "OdinSLTCard",
	.owner = THIS_MODULE,
	.dai_link = odin_dai_link,
	.num_links = ARRAY_SIZE(odin_dai_link),
};

static const struct snd_soc_component_driver odin_rt5671_component = {
	.name = "odin_rt5671",
};

static int odin_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &odin_slt_card;
	int ret;

	card->dev = &pdev->dev;

	ret = odin_of_dai_property_get(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get property:%d\n", ret);
		goto err;
	}

	ret = snd_soc_register_component(&pdev->dev, &odin_rt5671_component,
			voice_dai, ARRAY_SIZE(voice_dai));
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_component failed (%d)\n", ret);
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register soc_card:%d\n", ret);
		goto err;
	}

	dev_info(&pdev->dev, "Probing success! %d", ret);
	return 0;
err:
	return ret;
}

static int odin_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	return snd_soc_unregister_card(card);
}

#ifdef CONFIG_OF
static const struct of_device_id odin_audio_of_match[] = {
	{ .compatible = "lge,odin-audio", },
	{},
};
#endif

static struct platform_driver snd_odin_driver = {
	.driver = {
		.name = "odin-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = odin_audio_of_match,
#endif
	},
	.probe = odin_audio_probe,
	.remove = odin_audio_remove,
};

module_platform_driver(snd_odin_driver);
