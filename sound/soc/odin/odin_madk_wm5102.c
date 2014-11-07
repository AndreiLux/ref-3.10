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
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <asm/mach-types.h>
#include <linux/mfd/arizona/registers.h>

#include "odin_asoc_utils.h"
#include "../codecs/wm5102.h"

#define MCLK1_FREQ	24000000
#define MCLK2_FREQ	32768

struct odin_wm5102 {
	struct clk *clk;
	unsigned int machine_mclk;
	unsigned int wm5102_sync_clk;
	unsigned int wm5102_async_clk;
	struct snd_soc_jack jack;
};

static const struct snd_kcontrol_new odin_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Earpeice"),
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Recog Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

static const struct snd_soc_dapm_widget odin_wm5102_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_SPK("Earpeice", NULL),
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Recog Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
};

static const struct snd_soc_dapm_route odin_wm5102_audio_map[] = {
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

	{"IN3L", NULL, "MICBIAS3"},
	{"MICBIAS3", NULL, "Headset Mic"},
};

static int odin_wm5102_aif1_hw_params
				(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_wm5102 *machine =
				snd_soc_card_get_drvdata(rtd->codec->card);
	int ret;

	machine->wm5102_sync_clk = params_rate(params) * 512;

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
								machine->wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai sysclk\n");
		return ret;
	}

	/* odin_i2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0,
							machine->machine_mclk, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	/* arizona_set_sysclk */
	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
						ARIZONA_CLK_SRC_FLL1, machine->wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec sysclk\n");
		return ret;
	}

	/* wm5102_set_fll */
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
					machine->machine_mclk, machine->wm5102_sync_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec pll\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops odin_wm5102_aif1_ops = {
	.hw_params = odin_wm5102_aif1_hw_params,
};

static int odin_wm5102_aif2_hw_params(
							struct snd_pcm_substream *substream,
							struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct odin_wm5102 *machine =
				snd_soc_card_get_drvdata(rtd->codec->card);
	int bclk;
	int ret = 0;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_codec_set_sysclk(codec_dai->codec, ARIZONA_CLK_ASYNCCLK,
								ARIZONA_CLK_SRC_FLL2,
								machine->wm5102_async_clk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_ASYNCCLK,
					machine->wm5102_async_clk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the codec FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2, ARIZONA_FLL_SRC_MCLK1,
					machine->machine_mclk, machine->wm5102_async_clk);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops odin_wm5102_aif2_ops = {
	.hw_params = odin_wm5102_aif2_hw_params,
};

#if 0
static int odin_lpa_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	int masterclock = 12288000;

#if 0
	/* odin_i2s_set_sysclk */
	masterclock = snd_soc_dai_set_sysclk(cpu_dai,0,params_rate(params),0);
	if (!masterclock)
		return -EINVAL;
#endif

	/* Set the Codec DAI configuration */
	/* arizona_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	dev_dbg(dev,"set MCLK(%d) to codec\n",masterclock);

	/* arizona_set_sysclk */
	return snd_soc_codec_set_sysclk(codec,
		ARIZONA_CLK_SYSCLK, ARIZONA_CLK_SRC_MCLK1, masterclock, 0);
}
#endif

static int odin_lpa_hw_params(struct snd_pcm_substream *substream,
                     struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_wm5102 *machine =
		snd_soc_card_get_drvdata(rtd->codec->card);
	int ret;

	/* Set the Codec DAI configuration */
	/* arizona_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
			SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* arizona_dai_set_sysclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
			machine->wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai sysclk\n");
		return ret;
	}

	/* arizona_set_sysclk */
	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
			ARIZONA_CLK_SRC_FLL1, machine->wm5102_sync_clk, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec sysclk\n");
		return ret;
	}

	/* wm5102_set_fll */
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
			machine->machine_mclk, machine->wm5102_sync_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec pll\n");
		return ret;
	}

	printk("%s\n", __func__);
	return 0;
}


static struct snd_soc_ops odin_lpa_ops = {
	.hw_params = odin_lpa_hw_params,
};

static struct snd_soc_dai_link odin_dai[] = {
	{
		.name = "HiFi Playback dai",
		.stream_name = "Pri_Dai_Playback",
		.cpu_dai_name = "odin-si2s.0",
		.codec_dai_name = "wm5102-aif1",
		.platform_name = "odin-pcm-audio",
		.codec_name = "wm5102-codec",
		.ops = &odin_wm5102_aif1_ops,
	},
	{
		.name = "HiFi Capture dai",
		.stream_name = "Pri_Dai_Capture",
		.cpu_dai_name = "odin-si2s.1",
		.codec_dai_name = "wm5102-aif1",
		.platform_name = "odin-pcm-audio",
		.codec_name = "wm5102-codec",
		.ops = &odin_wm5102_aif1_ops,
	},
	{
		.name = "CP dai",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "odin-voice-dai",
		.codec_dai_name = "wm5102-aif2",
		.codec_name = "wm5102-codec",
		.ops = &odin_wm5102_aif2_ops,
	},
	{
		.name = "BT dai",
		.stream_name = "Third_Dai",
		.cpu_dai_name = "odin-bt-dai",
		.codec_dai_name = "wm5102-aif3",
		.codec_name = "wm5102-codec",
	},
	{
		.name = "LPA dai",
		.stream_name = "LPA_Dai",
		.cpu_dai_name = "34611000.i2s",
		.codec_dai_name = "wm5102-aif1",
		.codec_name = "wm5102-codec",
		.ops = &odin_lpa_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_driver odin_ext_dai[] = {
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

static int odin_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	int ret;

	/* output volume incresing ramp rate */
	snd_soc_write(codec, 0x409, 0x20);

	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
				MCLK2_FREQ, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Unable to switch to MCLK2\n");

	snd_soc_dapm_ignore_suspend(&card->dapm, "Speaker");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Earpeice");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headphone");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Recog Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");

	return 0;
}

static int odin_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct odin_wm5102 *machine =
				snd_soc_card_get_drvdata(codec->card);
#if 0
	clk_disable(machine->clk);
#endif
	return 0;
}

static int odin_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct odin_wm5102 *machine =
				snd_soc_card_get_drvdata(codec->card);
#if 0
	clk_enable(machine->clk);
#endif
	return 0;
}


static struct snd_soc_card snd_soc_odin_wm5102 = {
	.name = "ODIN_SOUND_CARD",
	.owner = THIS_MODULE,
	.dai_link = odin_dai,
	.num_links = ARRAY_SIZE(odin_dai),

	.controls = odin_wm5102_controls,
	.num_controls = ARRAY_SIZE(odin_wm5102_controls),
	.dapm_widgets = odin_wm5102_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(odin_wm5102_dapm_widgets),
	.dapm_routes = odin_wm5102_audio_map,
	.num_dapm_routes = ARRAY_SIZE(odin_wm5102_audio_map),

	.late_probe = odin_late_probe,

	.suspend_post = odin_card_suspend_post,
	.resume_pre = odin_card_resume_pre,
};

static int odin_wm5102_probe(struct platform_device *pdev)
{
	struct odin_wm5102 *machine;
	int ret;

	ret = odin_of_dai_property_get(&pdev->dev, &snd_soc_odin_wm5102);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get property:%d\n", ret);
		goto err;
	}

	machine = kzalloc(sizeof(*machine), GFP_KERNEL);
	if (!machine) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err;
	}

	machine->machine_mclk = MCLK1_FREQ;
	machine->wm5102_sync_clk = 44100 * 1024; /* default sample rate */
	machine->wm5102_async_clk = 48000* 1024; /* default sample rate */

#if 0
	machine->clk = clk_get(NULL, "mclk_i2s0");
	if (IS_ERR(machine->clk)) {
		pr_err("failed to get system_clk\n");
		ret = PTR_ERR(machine->clk);
		goto err_clk_get;
	}

	ret = clk_prepare_enable(machine->clk);
	if (ret) {
		pr_err("clk_enable failed: %d\n", ret);
		return ret;
	}
	clk_set_rate(machine->clk, machine->machine_mclk);

	/* Start the reference clock for the codec's FLL */
	clk_enable(machine->clk);
#endif

	/*
	ret = snd_soc_register_dais(&pdev->dev,
				odin_ext_dai, ARRAY_SIZE(odin_ext_dai));
	*/
	ret = snd_soc_register_component(&pdev->dev, &odin_wm5102_component,
				odin_ext_dai,ARRAY_SIZE(odin_ext_dai));
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_component failed (%d)\n",
			ret);
		goto err_register_dai;
	}

	snd_soc_card_set_drvdata(&snd_soc_odin_wm5102, machine);

	snd_soc_odin_wm5102.dev = &pdev->dev;
	ret = snd_soc_register_card(&snd_soc_odin_wm5102);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto err_register_card;
	}

	return 0;

err_register_card:
	snd_soc_unregister_card(&snd_soc_odin_wm5102);
err_register_dai:
#if 0
	clk_put(machine->clk);
#endif
err_clk_get:
	kfree(machine);
err:
	return ret;
}

static int odin_wm5102_remove(struct platform_device *pdev)
{
	struct odin_wm5102 *machine = snd_soc_card_get_drvdata(&snd_soc_odin_wm5102);

	snd_soc_unregister_card(&snd_soc_odin_wm5102);
	snd_soc_unregister_component(&pdev->dev);
#if 0
	clk_disable(machine->clk);
	clk_put(machine->clk);
#endif
	kfree(machine);

	return 0;
}

static const struct of_device_id odin_wm5102_of_match[] = {
	{ .compatible = "lge,odin-audio", },
	{},
};

static struct platform_driver odin_wm5102_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "odin-wm5102",
		.pm = &snd_soc_pm_ops,
		.of_match_table = odin_wm5102_of_match,
	},
	.probe = odin_wm5102_probe,
	.remove = odin_wm5102_remove,
};
module_platform_driver(odin_wm5102_driver);

MODULE_AUTHOR("Hyunhee Jeon, hyunhee.jeon@lge.com");
MODULE_DESCRIPTION("ALSA SoC ODIN HDK WM5102");
MODULE_LICENSE("GPL");

