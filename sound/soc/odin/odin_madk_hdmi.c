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

static const struct snd_kcontrol_new odin_madk_hdmi_controls[] = {
	SOC_DAPM_PIN_SWITCH("HDMI TX")
};

static int odin_hdmi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* Set the Codec DAI configuration */
	/* odin_hdmi_set_fmt */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}

	/* Set the AP DAI configuration */
	/* odin_mi2s_set_fmt */
	ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai format\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, params_rate(params) * 256, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops odin_hdmi_ops = {
	.hw_params = odin_hdmi_hw_params,
};

static struct snd_soc_dai_link odin_hdmi_dai_link[] = {
	{
		.name = "HDMI_link0",
		.stream_name = "HDMI HiFi",
		.cpu_dai_name = "odin-mi2s.0",
		.codec_dai_name = "odin-hdmi-codec-dai",
		.platform_name = "odin-pcm-audio",
		.codec_name = "odin-hdmi-codec",
		.ops = &odin_hdmi_ops,
	},
};

static struct snd_soc_card odin_hdmi_card = {
	.name = "OdinHdmiCard",
	.owner = THIS_MODULE,
	.controls = odin_madk_hdmi_controls,
	.num_controls = ARRAY_SIZE(odin_madk_hdmi_controls),
	.dai_link = odin_hdmi_dai_link,
	.num_links = ARRAY_SIZE(odin_hdmi_dai_link),
};

#ifdef CONFIG_SND_SOC_SI2S_TO_MI2S_IO
static int odin_hdmi_audio_io_pin_change(struct device *dev)
{
	/* audio io number and function structure */
	struct aud_io_nf {
		int aud_gpio_number;
		int io_function;
	};

	struct aud_io_nf change_tb[] = {
		{.aud_gpio_number = AUD_GPIO_16, .io_function = AUD_IO_FUNC3},
		{.aud_gpio_number = AUD_GPIO_17, .io_function = AUD_IO_FUNC3},
		{.aud_gpio_number = AUD_GPIO_21, .io_function = AUD_IO_FUNC3},
		{.aud_gpio_number = AUD_GPIO_22, .io_function = AUD_IO_FUNC3},
		{.aud_gpio_number = AUD_GPIO_23, .io_function = AUD_IO_FUNC3},
		{.aud_gpio_number = AUD_GPIO_24, .io_function = AUD_IO_FUNC3},
	};

	int i;
	int ret;
	int base = 0;

	/* IO functions change the outpin for external device(ex> slimport) */
	ret = odin_of_get_base_bank_num(dev, &base);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(change_tb); i++) {
		ret = odin_set_aud_io_func(base, change_tb[i].aud_gpio_number,
						change_tb[i].io_function, 0);
		if (ret < 0) {
			dev_warn(dev, "Can not change audio[%d] io %d\n",
					change_tb[i].aud_gpio_number, ret);
		}
	}

	return 0;
}
#endif

static int odin_hdmi_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &odin_hdmi_card;
	int ret;

	card->dev = &pdev->dev;

	ret = odin_of_dai_property_get(&pdev->dev, card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get property:%d\n", ret);
		goto err;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register soc_card:%d\n", ret);
		goto err;
	}

#ifdef CONFIG_SND_SOC_SI2S_TO_MI2S_IO
	if (odin_hdmi_audio_io_pin_change(&pdev->dev))
		dev_warn(&pdev->dev, "SI2S I/O funcs can not change to MI2S");
#endif
	dev_info(&pdev->dev, "Probing success! %d", ret);
	return 0;
err:
	return ret;
}

static int odin_hdmi_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	return snd_soc_unregister_card(card);
}

static const struct of_device_id odin_hdmi_audio_match[] = {
	{ .compatible = "lge,odin-hdmi-audio", },
	{},
};

static struct platform_driver odin_hdmi_audio_driver = {
	.driver		= {
		.name	= "odin-hdmi-audio",
		.owner	= THIS_MODULE,
		.pm	= &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(odin_hdmi_audio_match),
	},
	.probe		= odin_hdmi_audio_probe,
	.remove		= odin_hdmi_audio_remove,
};

module_platform_driver(odin_hdmi_audio_driver);

MODULE_AUTHOR("JongHo Kim <m4jongho.kim@lgepartner.com>");
MODULE_DESCRIPTION("ALSA SoC Odin HDMI");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-hdmi-audio");
