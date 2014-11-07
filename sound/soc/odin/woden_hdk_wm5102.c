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
#include <linux/mfd/arizona/registers.h>

#include "../codecs/wm5102.h"

#define DRV_NAME "woden-wm5102"

#define WM5102_MCLK1_FREQ 22579200

static int woden_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	int masterclock = 0;
	/* woden_i2s_set_sysclk */
	masterclock = snd_soc_dai_set_sysclk(cpu_dai,0,params_rate(params),0);
	if ( masterclock < 0 )
		return masterclock;

	if (!masterclock)
		return -EINVAL;

	/* Set the Codec DAI configuration */
	/* arizona_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	/* woden_i2s_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	dev_dbg(dev,"set MCLK(%d) to codec\n",masterclock);

	/* arizona_set_sysclk */
	return snd_soc_codec_set_sysclk(codec,
		ARIZONA_CLK_SYSCLK, ARIZONA_CLK_SRC_MCLK1, masterclock, 0);
}

static const unsigned int woden_i2s_channel_dai0[] = { 2 };
static struct snd_pcm_hw_constraint_list woden_i2s_dai0_channel_constraint = {
	.count	= ARRAY_SIZE(woden_i2s_channel_dai0),
	.list	= woden_i2s_channel_dai0,
};

static int woden_wm5102_startup(struct snd_pcm_substream *substream)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	switch(cpu_dai->id) {
	case 0:
		dev_dbg(dev,"cpu_dai %d added 2 channel constraint\n",cpu_dai->id);
		snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_CHANNELS, &woden_i2s_dai0_channel_constraint);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	default:
		break;
	}

	return 0;
}

static int woden_wm5102_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_write(codec, ARIZONA_NOISE_GATE_SELECT_1L, 0);
	snd_soc_write(codec, ARIZONA_NOISE_GATE_SELECT_1R, 0);

	snd_soc_dapm_enable_pin(dapm, "OnboardMic1");
	snd_soc_dapm_enable_pin(dapm, "OnboardMic2");
	snd_soc_dapm_enable_pin(dapm, "OnboardMic3");
	snd_soc_dapm_enable_pin(dapm, "Headset Mic");

	return 0;
}

static struct snd_soc_ops woden_ops = {
	.startup = woden_wm5102_startup,
	.hw_params = woden_hw_params,
};

static const struct snd_soc_dapm_widget woden_wm5102_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("OnboardMic1", NULL),
	SND_SOC_DAPM_MIC("OnboardMic2", NULL),
	SND_SOC_DAPM_MIC("OnboardMic3", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
};

static const struct snd_soc_dapm_route woden_wm5102_audio_map[] = {
	{"IN1L", NULL, "OnboardMic1"},
	{"IN1R", NULL, "OnboardMic2"},
	{"IN2L", NULL, "OnboardMic3"},
	{"IN3L", NULL, "Headset Mic"},

	{"OnboardMic1", NULL, "MICBIAS1"},
	{"OnboardMic2", NULL, "MICBIAS2"},
	{"OnboardMic3", NULL, "MICBIAS1"},
	{"Headset Mic", NULL, "MICBIAS3"},
};

static const struct snd_kcontrol_new woden_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("OnboardMic1"),
	SOC_DAPM_PIN_SWITCH("OnboardMic2"),
	SOC_DAPM_PIN_SWITCH("OnboardMic3"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static struct snd_soc_dai_driver voice_dai[] = {
	{
		.name = "odin-voice-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	},
	{
		.name = "odin-bt-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
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

static int woden_voice_call_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out = 24000000;
	int ret = 0;

	if (params_rate(params) != 16000)
		return -EINVAL;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the codec FLL */
	snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
				  WM5102_MCLK1_FREQ, pll_out);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	snd_soc_codec_set_sysclk(codec,
		ARIZONA_CLK_SYSCLK, ARIZONA_CLK_SRC_FLL1, params_rate(params) * 256, 0);

	return 0;
}

static struct snd_soc_ops woden_voice_call_ops = {
	.hw_params = woden_voice_call_hw_params,
};

#include <asm/io.h>			/**< For ioremap_nocache */
static int woden_lpa_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	int masterclock = 12288000;

	/* woden_i2s_set_sysclk */
	masterclock = snd_soc_dai_set_sysclk(cpu_dai,0,params_rate(params),0);
	if ( masterclock < 0 )
		return masterclock;

	if (!masterclock)
		return -EINVAL;

	/* Set the Codec DAI configuration */
	/* arizona_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	dev_dbg(dev,"set MCLK(%d) to codec\n",masterclock);

	/* arizona_set_sysclk */
	return snd_soc_codec_set_sysclk(codec,
		ARIZONA_CLK_SYSCLK, ARIZONA_CLK_SRC_MCLK1, masterclock, 0);
}

static struct snd_soc_ops woden_lpa_ops = {
	.hw_params = woden_lpa_hw_params,
};

enum {
	HIFI_DAI = 0,
	CP_DAI = 1,
	BT_DAI = 2,
	LPA_DAI = 3,
};

static struct snd_soc_dai_link woden_dai[] = {
	[HIFI_DAI] = { /* Primary Playback i/f */
		.name = "HiFi dai",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "34611000.i2s",/*"woden-i2s.0",*/
		.codec_dai_name = "wm5102-aif1",
		.platform_name = "34611000.i2s", /*"odin-pcm-audio"*/
		.codec_name = "wm5102-codec",
		.init = woden_wm5102_init,
		.ops = &woden_ops,
	},
	[CP_DAI] = {
		.name = "CP dai",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "odin-voice-dai",
		.codec_dai_name = "wm5102-aif2",
		.codec_name = "wm5102-codec",
		.ops = &woden_voice_call_ops,
	},
	[BT_DAI] = {
		.name = "BT dai",
		.stream_name = "Third_Dai",
		.cpu_dai_name = "odin-bt-dai",
		.codec_dai_name = "wm5102-aif3",
		.codec_name = "wm5102-codec",
		//.ops = &woden_voice_call_bt_ops,
	},
	[LPA_DAI] = {
		.name = "LPA dai",
		.stream_name = "LPA_Dai",
		.cpu_dai_name = "34611000.i2s",
		.codec_dai_name = "wm5102-aif1",
		.codec_name = "wm5102-codec",
		.ops = &woden_lpa_ops,
	},
};

static struct snd_soc_card snd_soc_woden_wm5102 = {
	.name = "WODEN_HDK_WM5102",
	.owner = THIS_MODULE,
	.dai_link = woden_dai,
	.num_links = ARRAY_SIZE(woden_dai),

	.controls = woden_wm5102_controls,
	.num_controls = ARRAY_SIZE(woden_wm5102_controls),
	.dapm_widgets = woden_wm5102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(woden_wm5102_dapm_widgets),
	.dapm_routes = woden_wm5102_audio_map,
	.num_dapm_routes = ARRAY_SIZE(woden_wm5102_audio_map),
};

static int woden_wm5102_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_woden_wm5102;
	int ret;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_register_dais(&pdev->dev,
				voice_dai, ARRAY_SIZE(voice_dai));
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_dais failed (%d)\n",
			ret);
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	return 0;

err_reg_card:
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(voice_dai));
err:
	return ret;
}

static int woden_wm5102_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id woden_wm5102_of_match[] = {
	{ .compatible = "lge,woden-audio-wm5102", },
	{},
};

static struct platform_driver woden_wm5102_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = woden_wm5102_of_match,
	},
	.probe = woden_wm5102_probe,
	.remove = woden_wm5102_remove,
};
module_platform_driver(woden_wm5102_driver);


MODULE_AUTHOR("Hyunhee Jeon, hyunhee.jeon@lge.com");
MODULE_DESCRIPTION("ALSA SoC WODEN FPGA WM5102");
MODULE_LICENSE("GPL");
