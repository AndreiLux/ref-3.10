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
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/hdmi_alink.h>

#define SND_SOC_DAPM_OUTPUT_EVT(wname, stname, wevent, wflags) \
{	.id = snd_soc_dapm_output, .name = wname, .sname = stname, \
	.kcontrol_news = NULL, .num_kcontrols = 0, .reg = SND_SOC_NOPM, \
	.event = wevent, .event_flags = wflags }

static int odin_hdmi_tx_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	int ret;

	if (SND_SOC_DAPM_POST_PMU & event)
		ret = hdmi_audio_hw_switch(1);

	if (SND_SOC_DAPM_PRE_PMD & event)
		ret = hdmi_audio_hw_switch(0);

	return 0;
}

static const struct snd_soc_dapm_widget odin_hdmi_widgets[] = {
SND_SOC_DAPM_OUTPUT_EVT("HDMI TX", "HDMI Playback", odin_hdmi_tx_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
};

static int odin_hdmi_codec_info_sad(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	int sad_count;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;

	sad_count = hdmi_audio_get_sad_byte_count();

	if (sad_count < 0)
		uinfo->count = 0;
	else
		uinfo->count = sad_count;

	return 0;
}

static int odin_hdmi_codec_get_sad(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int sad_count;
	unsigned char *sadbuffer;

	sad_count = hdmi_audio_get_sad_byte_count();
	if (sad_count <= 0)
		return 0;

	sadbuffer = kzalloc(sad_count, GFP_KERNEL);
	if (!sadbuffer)
		return -ENOMEM;

	hdmi_audio_get_sad_bytes(sadbuffer, sad_count);

	for (i = 0; i< sad_count; i++)
		ucontrol->value.bytes.data[i] = sadbuffer[i];

	kfree(sadbuffer);
	return 0;
}

static struct snd_kcontrol_new odin_hdmi_codec_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "HDMI EDID",
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.info = odin_hdmi_codec_info_sad,
		.get = odin_hdmi_codec_get_sad,
	},
};

static int odin_hdmi_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	return hdmi_audio_set_clock(freq);
}

static int odin_hdmi_startup(struct snd_pcm_substream *substream,
						   struct snd_soc_dai *dai)
{
	return hdmi_audio_open(substream);
}

static int odin_hdmi_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	unsigned int hdmi_fmt = 0;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		hdmi_fmt |= HDMI_AUDIO_FMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		hdmi_fmt |= HDMI_AUDIO_FMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		hdmi_fmt |= HDMI_AUDIO_FMT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		hdmi_fmt |= HDMI_AUDIO_FMT_DSP_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		hdmi_fmt |= HDMI_AUDIO_FMT_DSP_B;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		hdmi_fmt |= HDMI_AUDIO_FMT_NB_NF;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		hdmi_fmt |= HDMI_AUDIO_FMT_NB_IF;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		hdmi_fmt |= HDMI_AUDIO_FMT_IB_NF;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		hdmi_fmt |= HDMI_AUDIO_FMT_IB_IF;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		hdmi_fmt |= HDMI_AUDIO_FMT_CBM_CFM;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		hdmi_fmt |= HDMI_AUDIO_FMT_CBS_CFM;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		hdmi_fmt |= HDMI_AUDIO_FMT_CBM_CFS;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		hdmi_fmt |= HDMI_AUDIO_FMT_CBS_CFS;
		break;
	default:
		return -EINVAL;
	}

	return hdmi_audio_set_fmt(hdmi_fmt);
}

static void odin_hdmi_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	hdmi_audio_close();
}

static int odin_hdmi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	return hdmi_audio_set_params(params);
}

static int odin_hdmi_trigger(struct snd_pcm_substream *substream,
					int cmd, struct snd_soc_dai *dai)
{
	return hdmi_audio_operation(cmd);
}

static struct snd_soc_dai_ops odin_hdmi_dai_ops = {
	.set_sysclk	= odin_hdmi_set_sysclk,
	.startup	= odin_hdmi_startup,
	.shutdown	= odin_hdmi_shutdown,
	.set_fmt	= odin_hdmi_set_fmt,
	.hw_params	= odin_hdmi_hw_params,
	.trigger	= odin_hdmi_trigger,
};

#define ODIN_VIRTUAL_HDMI_RATES \
	(SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_88200 | \
	 SNDRV_PCM_RATE_96000 | \
	 SNDRV_PCM_RATE_176400 | \
	 SNDRV_PCM_RATE_192000)

#define ODIN_VIRTUAL_HDMI_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver odin_hdmi_codec_dai = {
	.name = "odin-hdmi-codec-dai",
	.playback = {
		.stream_name = "HDMI Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = ODIN_VIRTUAL_HDMI_RATES,
		.formats = ODIN_VIRTUAL_HDMI_FORMATS,
	},
	.ops = &odin_hdmi_dai_ops,
};

static struct snd_soc_codec_driver odin_hdmi_codec = {
	.ignore_pmdown_time = true,
	.dapm_widgets = odin_hdmi_widgets,
	.num_dapm_widgets = ARRAY_SIZE(odin_hdmi_widgets),
	.controls = odin_hdmi_codec_controls,
	.num_controls = ARRAY_SIZE(odin_hdmi_codec_controls),
};

static int odin_hdmi_codec_probe(struct platform_device *pdev)
{
	int ret;
	ret = snd_soc_register_codec(&pdev->dev, &odin_hdmi_codec,
			&odin_hdmi_codec_dai, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed register codec\n");
		return ret;
	}

	dev_info(&pdev->dev, "Probing Success\n");

	return 0;
}

static int odin_hdmi_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_hdmi_codec_match[] = {
	{ .compatible = "lge,odin-hdmi-codec", },
	{},
};

static struct platform_driver odin_hdmi_codec_driver = {
	.driver		= {
		.name	= "odin-hdmi-codec",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_hdmi_codec_match),
	},

	.probe		= odin_hdmi_codec_probe,
	.remove		= odin_hdmi_codec_remove,
};

module_platform_driver(odin_hdmi_codec_driver);

MODULE_AUTHOR("JongHo Kim <m4jongho.kim@lgepartner.com>");
MODULE_DESCRIPTION("ASoC ODIN HDMI codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: hdmi-audio-codec");
