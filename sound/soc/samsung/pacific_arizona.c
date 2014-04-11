/*
 *  pacific_arizona.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/tlv.h>
#include <linux/input.h>
#include <linux/delay.h>

#include <linux/mfd/arizona/registers.h>
#include <linux/mfd/arizona/core.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <mach/exynos-audio.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/wm5102.h"
#include "../codecs/florida.h"

/* PACIFIC use CLKOUT from AP */
#define PACIFIC_MCLK_FREQ	24000000
#define PACIFIC_AUD_PLL_FREQ	393216018

#define PACIFIC_DEFAULT_MCLK1	24000000
#define PACIFIC_DEFAULT_MCLK2	32768

static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);

enum {
	PACIFIC_PLAYBACK_DAI,
	PACIFIC_VOICECALL_DAI,
	PACIFIC_BT_SCO_DAI,
	PACIFIC_MULTIMEDIA_DAI,
};

struct arizona_machine_priv {
	int mic_bias_gpio;
	int clock_mode;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dai *aif[3];
	struct delayed_work mic_work;
	struct input_dev *input;
	int aif2mode;
	int voice_key_event;
	struct clk *mclk;

	unsigned int hp_impedance_step;

	int sysclk_rate;
	int asyncclk_rate;
};

static struct arizona_machine_priv pacific_wm5102_priv = {
	.sysclk_rate = 4915200,
	.asyncclk_rate = 49152000,
};

static struct arizona_machine_priv pacific_wm5110_priv = {
	.sysclk_rate = 147456000,
	.asyncclk_rate = 49152000,
};

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

const char *voicecontrol_mode_text[] = {
	"Normal", "LPSD"
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static const struct soc_enum voicecontrol_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voicecontrol_mode_text), voicecontrol_mode_text),
};

static const struct snd_soc_component_driver pacific_cmpnt = {
	.name	= "pacific-audio",
};

static struct {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
} hp_gain_table[] = {
	{    0,      42, 0 },
	{   43,     100, 4 },
	{  101,     200, 8 },
	{  201,     450, 12 },
	{  451,    1000, 14 },
	{ 1001, INT_MAX, 0 },
};

static struct snd_soc_codec *the_codec;

void pacific_arizona_hpdet_cb(unsigned int meas)
{
	int i;
	struct arizona_machine_priv *priv;

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->card->drvdata;

	for (i = 0; i < ARRAY_SIZE(hp_gain_table); i++) {
		if (meas < hp_gain_table[i].min || meas > hp_gain_table[i].max)
			continue;

		dev_info(the_codec->dev, "SET GAIN %d step for %d ohms\n",
			 hp_gain_table[i].gain, meas);
		priv->hp_impedance_step = hp_gain_table[i].gain;
	}
}

static int arizona_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv = codec->card->drvdata;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	val += priv->hp_impedance_step;
	dev_info(codec->dev,
			 "SET GAIN %d according to impedance, moved %d step\n",
			 val, priv->hp_impedance_step);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	return err;
}


static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv = codec->card->drvdata;

	ucontrol->value.integer.value[0] = priv->aif2mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv = codec->card->drvdata;

	priv->aif2mode = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "set aif2 mode: %s\n",
					 aif2_mode_text[priv->aif2mode]);
	return  0;
}

static int get_voicecontrol_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv = codec->card->drvdata;

	if (priv->voice_key_event == KEY_VOICE_WAKEUP)
		ucontrol->value.integer.value[0] = 0;
	else
		ucontrol->value.integer.value[0] = 1;

	return 0;
}

static int set_voicecontrol_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct arizona_machine_priv *priv = codec->card->drvdata;
	int voicecontrol_mode;

	voicecontrol_mode = ucontrol->value.integer.value[0];

	if (voicecontrol_mode == 0) {
		priv->voice_key_event = KEY_VOICE_WAKEUP;
	} else if (voicecontrol_mode == 1) {
		priv->voice_key_event = KEY_LPSD_WAKEUP;
	} else {
		dev_err(the_codec->card->dev, "Invalid voice control mode =%d", voicecontrol_mode);
		return 0;
	}

	dev_info(codec->dev, "set voice control mode: %s key_event = %d\n",
					 voicecontrol_mode_text[voicecontrol_mode], priv->voice_key_event);
	return  0;
}

static int pacific_out3_differential(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_soc_codec *codec = priv->codec;
	int err = 0;

	dev_dbg(codec->dev, "%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		err = snd_soc_update_bits_locked(codec, ARIZONA_OUTPUT_PATH_CONFIG_3L,
				ARIZONA_OUT3_MONO_MASK, ARIZONA_OUT3_MONO);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		err = snd_soc_update_bits_locked(codec, ARIZONA_OUTPUT_PATH_CONFIG_3L,
				ARIZONA_OUT3_MONO_MASK, 0x0);
		break;
	}

	return err;
}

static int pacific_ext_mainmicbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct arizona_machine_priv *priv = card->drvdata;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		gpio_set_value(priv->mic_bias_gpio,  1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		gpio_set_value(priv->mic_bias_gpio,  0);
		break;
	}

	dev_err(w->dapm->dev, "Main Mic BIAS: %d\n", event);

	return 0;
}

static const struct snd_kcontrol_new pacific_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1L,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		ARIZONA_DAC_DIGITAL_VOLUME_1R,
		ARIZONA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, arizona_put_impedance_volsw,
		digital_tlv),

	SOC_ENUM_EXT("VoiceControl Mode", voicecontrol_mode_enum[0],
		get_voicecontrol_mode, set_voicecontrol_mode),
};

static const struct snd_kcontrol_new pacific_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("VPS"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Third Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
};

const struct snd_soc_dapm_widget pacific_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HDMIL"),
	SND_SOC_DAPM_OUTPUT("HDMIR"),
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_SPK("RCV", pacific_out3_differential),
	SND_SOC_DAPM_LINE("VPS", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", pacific_ext_mainmicbias),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
	SND_SOC_DAPM_MIC("Third Mic", NULL),
};

const struct snd_soc_dapm_route pacific_dapm_routes[] = {
	{ "HDMIL", NULL, "AIF1RX1" },
	{ "HDMIR", NULL, "AIF1RX2" },
	{ "HDMI", NULL, "HDMIL" },
	{ "HDMI", NULL, "HDMIR" },

	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "VPS", NULL, "HPOUT2L" },
	{ "VPS", NULL, "HPOUT2R" },

#ifdef CONFIG_MFD_FLORIDA
	{ "RCV", NULL, "HPOUT3L" },
	{ "RCV", NULL, "HPOUT3R" },
#else
	{ "RCV", NULL, "EPOUTN" },
	{ "RCV", NULL, "EPOUTP" },
#endif

	/* SEL of main mic is connected to GND */
	{ "IN1R", NULL, "Main Mic" },
	{ "Main Mic", NULL, "MICBIAS2" },

	/* Headset mic is Analog Mic */
	{ "Headset Mic", NULL, "MICBIAS1" },
	{ "IN2R", NULL, "Headset Mic" },

	/* SEL of 2nd mic is connected to MICBIAS2 */
	{ "Sub Mic", NULL, "MICBIAS3" },
	{ "IN3R", NULL, "Sub Mic" },

	/* SEL of 3rd mic is connected to GND */
	{ "Third Mic", NULL, "MICBIAS2" },
	{ "IN4L", NULL, "Third Mic" },
};

int pacific_set_media_clocking(struct arizona_machine_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;
	struct snd_soc_card *card = codec->card;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       priv->sysclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       priv->asyncclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev,
				 "Unable to set ASYNCCLK to FLL2: %d\n", ret);

	/* AIF1 from SYSCLK, AIF2 and 3 from ASYNCCLK */
	ret = snd_soc_dai_set_sysclk(priv->aif[0], ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Can't set AIF1 to SYSCLK: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(priv->aif[1], ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Can't set AIF2 to ASYNCCLK: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(priv->aif[2], ARIZONA_CLK_ASYNCCLK, 0, 0);
	if (ret < 0)
		dev_err(card->dev, "Can't set AIF3 to ASYNCCLK: %d\n", ret);

	return 0;
}

static int pacific_aif_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d startup\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static void pacific_aif_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_info(card->dev, "%s-%d shutdown\n",
			rtd->dai_link->name, substream->stream);
}

static int pacific_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz, %dbytes\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	pacific_set_media_clocking(priv);

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 codec fmt: %d\n", ret);
		return ret;
	}

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set aif1 cpu fmt: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
		return ret;
	}

	return ret;
}

static int pacific_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d\n hw_free",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int pacific_aif1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d prepare\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static int pacific_aif1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s-%d trigger\n",
			rtd->dai_link->name, substream->stream);

	return 0;
}

static struct snd_soc_ops pacific_aif1_ops = {
	.startup = pacific_aif_startup,
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif1_hw_params,
	.hw_free = pacific_aif1_hw_free,
	.prepare = pacific_aif1_prepare,
	.trigger = pacific_aif1_trigger,
};

static int pacific_aif2_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct arizona_machine_priv *priv = rtd->card->drvdata;
	int ret;
	int prate, bclk;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	prate = params_rate(params);
	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		dev_warn(card->dev,
			"Unsupported LRCLK %d, falling back to 8000Hz\n",
			(int)params_rate(params));
		bclk = 256000;
	}

	/* Set the codec DAI configuration, aif2_mode:0 is slave */

	if (priv->aif2mode == 0)
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	else
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(card->dev,
			"Failed to set audio format in codec: %d\n", ret);
		return ret;
	}

	if (priv->aif2mode  == 0) {
		ret = snd_soc_dai_set_pll(codec_dai, FLORIDA_FLL2_REFCLK,
					  ARIZONA_FLL_SRC_MCLK1,
					  PACIFIC_DEFAULT_MCLK1,
					  priv->asyncclk_rate);
		if (ret != 0) {
			dev_err(card->dev,
					"Failed to start FLL2 REF: %d\n", ret);
			return ret;
		}

		ret = snd_soc_dai_set_pll(codec_dai, FLORIDA_FLL2,
					  ARIZONA_FLL_SRC_AIF2BCLK,
					  bclk,
					  priv->asyncclk_rate);
		if (ret != 0) {
			dev_err(card->dev,
					 "Failed to start FLL2%d\n", ret);
			return ret;
		}
	} else {
		ret = snd_soc_dai_set_pll(codec_dai, FLORIDA_FLL2, 0, 0, 0);
		if (ret != 0)
			dev_err(card->dev,
					"Failed to stop FLL2: %d\n", ret);

		ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL2_REFCLK,
					  ARIZONA_FLL_SRC_NONE, 0, 0);
		if (ret != 0) {
			dev_err(card->dev,
				 "Failed to start FLL2 REF: %d\n", ret);
			return ret;
		}
		ret = snd_soc_dai_set_pll(codec_dai, FLORIDA_FLL2,
					  ARIZONA_CLK_SRC_MCLK1,
					  PACIFIC_DEFAULT_MCLK1,
					  priv->asyncclk_rate);
		if (ret != 0) {
			dev_err(card->dev,
					"Failed to start FLL2: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static struct snd_soc_ops pacific_aif2_ops = {
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif2_hw_params,
};

static int pacific_aif3_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	dev_info(card->dev, "%s-%d %dch, %dHz\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params));

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				| SND_SOC_DAIFMT_NB_NF
				| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set BT mode: %d\n", ret);
		return ret;
	}
	return 0;
}

static struct snd_soc_ops pacific_aif3_ops = {
	.shutdown = pacific_aif_shutdown,
	.hw_params = pacific_aif3_hw_params,
};

static struct snd_soc_dai_driver pacific_ext_dai[] = {
	{
		.name = "pacific-ext voice call",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "pacific-ext bluetooth sco",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static struct snd_soc_dai_link pacific_wm5102_dai[] = {
	{ /* playback & recording */
		.name = "playback-pri",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "wm5102-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* voice call */
		.name = "baseband",
		.stream_name = "pacific-ext voice call",
		.cpu_dai_name = "pacific-ext voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm5102-aif2",
		.ops = &pacific_aif2_ops,
		.ignore_suspend = 1,
	},
	{ /* bluetooth sco */
		.name = "bluetooth sco",
		.stream_name = "pacific-ext bluetooth sco",
		.cpu_dai_name = "pacific-ext bluetooth sco",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm5102-aif3",
		.ops = &pacific_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* deep buffer playback */
		.name = "playback-sec",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "wm5102-aif1",
		.ops = &pacific_aif1_ops,
	},
};

static struct snd_soc_dai_link pacific_wm5110_dai[] = {
	{ /* playback & recording */
		.name = "playback-pri",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "florida-aif1",
		.ops = &pacific_aif1_ops,
	},
	{ /* voice call */
		.name = "baseband",
		.stream_name = "pacific-ext voice call",
		.cpu_dai_name = "pacific-ext voice call",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "florida-aif2",
		.ops = &pacific_aif2_ops,
		.ignore_suspend = 1,
	},
	{ /* bluetooth sco */
		.name = "bluetooth sco",
		.stream_name = "pacific-ext bluetooth sco",
		.cpu_dai_name = "pacific-ext bluetooth sco",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "florida-aif3",
		.ops = &pacific_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* deep buffer playback */
		.name = "playback-sec",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "florida-aif1",
		.ops = &pacific_aif1_ops,
	},
	{
		.name = "CPU-DSP Voice Control",
		.stream_name = "CPU-DSP Voice Control",
		.cpu_dai_name = "florida-cpu-voicectrl",
		.platform_name = "florida-codec",
		.codec_dai_name = "florida-dsp-voicectrl",
		.codec_name = "florida-codec",
	},
	{ /* pcm dump interface */
		.name = "CPU-DSP trace",
		.stream_name = "CPU-DSP trace",
		.cpu_dai_name = "florida-cpu-trace",
		.platform_name = "florida-codec",
		.codec_dai_name = "florida-dsp-trace",
		.codec_name = "florida-codec",
	},
};

static int pacific_of_get_pdata(struct snd_soc_card *card)
{
	struct device_node *pdata_np;
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	pdata_np = of_find_node_by_path("/audio_pdata");
	if (!pdata_np) {
		dev_err(card->dev,
			"Property 'samsung,audio-cpu' missing or invalid\n");
		return -EINVAL;
	}

	priv->mic_bias_gpio = of_get_named_gpio(pdata_np, "mic_bias_gpio", 0);

	ret = gpio_request(priv->mic_bias_gpio, "MICBIAS_EN_AP");
	if (ret)
		dev_err(card->dev, "Failed to request gpio: %d\n", ret);

	gpio_direction_output(priv->mic_bias_gpio, 0);

	return 0;
}

static void ez2ctrl_debug_cb(void)
{
	struct arizona_machine_priv *priv;

	pr_info("Voice Control Callback invoked!\n");

	WARN_ON(!the_codec);
	if (!the_codec)
		return;

	priv = the_codec->card->drvdata;
	if (!priv)
		return;

	input_report_key(priv->input, priv->voice_key_event, 1);
	input_sync(priv->input);

	msleep(10);

	input_report_key(priv->input, priv->voice_key_event, 0);
	input_sync(priv->input);

	return;
}

static int pacific_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct arizona_machine_priv *priv = card->drvdata;
	int i, ret;

	priv->codec = codec;
	the_codec = codec;

	pacific_of_get_pdata(card);

	for (i = 0; i < 3; i++)
		priv->aif[i] = card->rtd[i].codec_dai;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->ignore_pmdown_time = true;

	ret = snd_soc_add_codec_controls(codec, pacific_codec_controls,
					ARRAY_SIZE(pacific_codec_controls));
	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	snd_soc_dapm_ignore_suspend(&card->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Third Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VPS");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HP");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "DSP Virtual Output");
	snd_soc_dapm_sync(&codec->dapm);

	ret = snd_soc_codec_set_sysclk(codec,
				       ARIZONA_CLK_SYSCLK,
				       ARIZONA_CLK_SRC_FLL1,
				       48000 * 1024,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set SYSCLK to FLL1: %d\n", ret);

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				       ARIZONA_CLK_SRC_FLL2,
				       priv->asyncclk_rate,
				       SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(card->dev, "Failed to set ASYSCLK to FLL2 %d\n", ret);

	priv->input = devm_input_allocate_device(card->dev);
	if (!priv->input) {
		dev_err(card->dev, "Can't allocate input dev\n");
		ret = -ENOMEM;
		goto err_register;
	}

	priv->input->name = "voicecontrol";
	priv->input->dev.parent = card->dev;

	input_set_capability(priv->input, EV_KEY,
		KEY_VOICE_WAKEUP);
	input_set_capability(priv->input, EV_KEY,
		KEY_LPSD_WAKEUP);

	ret = input_register_device(priv->input);
	if (ret) {
		dev_err(card->dev, "Can't register input device: %d\n", ret);
		goto err_register;
	}
	arizona_set_hpdet_cb(codec, pacific_arizona_hpdet_cb);
	arizona_set_ez2ctrl_cb(codec, ez2ctrl_debug_cb);

	priv->voice_key_event = KEY_VOICE_WAKEUP;

	return 0;

err_register:
	return ret;
}

static int pacific_start_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	struct snd_soc_codec *codec = priv->codec;
	int ret;

	if (priv->mclk) {
		clk_enable(priv->mclk);
		dev_info(card->dev, "mclk enabled\n");
	} else
		exynos5_audio_set_mclk(true, 0);

#ifdef CONFIG_MFD_FLORIDA
	ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec, FLORIDA_FLL1, ARIZONA_CLK_SRC_MCLK1,
				    PACIFIC_DEFAULT_MCLK1,
				    priv->sysclk_rate);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#else
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1_REFCLK,
				    ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
				    PACIFIC_DEFAULT_MCLK1,
				    priv->sysclk_rate);
	if (ret != 0) {
		dev_err(card->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#endif

	return ret;
}

static int pacific_stop_sysclk(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;
	int ret;

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL1, 0, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(priv->codec, FLORIDA_FLL2, 0, 0, 0);
	if (ret != 0) {
		dev_err(card->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}

	if (priv->mclk) {
		clk_disable(priv->mclk);
		dev_info(card->dev, "mclk disbled\n");
	} else
		exynos5_audio_set_mclk(false, 0);

	return ret;
}

static int pacific_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct arizona_machine_priv *priv = card->drvdata;

	if (!priv->codec || dapm != &priv->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF)
			pacific_start_sysclk(card);
		break;
	case SND_SOC_BIAS_OFF:
		pacific_stop_sysclk(card);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	default:
	break;
	}

	card->dapm.bias_level = level;
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int pacific_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int pacific_check_clock_conditions(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	int ear_state = 0;
	int ez2ctrl_state = 0;
	char bias[10];

	/* check status of the headset micbias*/
	switch(arizona->pdata.micd_configs->bias) {
	case 1:
		sprintf(bias, "MICBIAS1");
		break;
	case 2:
		sprintf(bias, "MICBIAS2");
		break;
	case 3:
		sprintf(bias, "MICBIAS3");
		break;
	default:
		sprintf(bias, "MICVDD");
		break;
	}
	ear_state = snd_soc_dapm_get_pin_status(&codec->dapm, bias);

#ifdef CONFIG_MFD_FLORIDA
	/* Check status of the Main Mic for ez2control
	 * Because when the phone enters suspend mode,
	 * Enabling case of Main mic is only ez2control mode */
	ez2ctrl_state = snd_soc_dapm_get_pin_status(&card->dapm, "Main Mic");
#endif
	dev_info(card->dev, "%s: ear_state:%d, ez2ctrl_state:%d\n", __func__,
			ear_state, ez2ctrl_state);

	return (!codec->active && ear_state && !ez2ctrl_state);
}

static int pacific_suspend_post(struct snd_soc_card *card)
{
	struct arizona_machine_priv *priv = card->drvdata;

	/* When the card enters suspend state, If codec is not active, the micbias of headset
	 * is enable and the ez2control is not running, The MCLK and the FLL1 should be disable
	 * to reduce the sleep current. In the other cases, these should keep previous status */
	if (pacific_check_clock_conditions(card)) {
		pacific_stop_sysclk(card);
		dev_info(card->dev, "%s\n", __func__);
	}
	if (!priv->codec->active) {
		dev_info(card->dev, "%s : set AIF1 port slave\n", __func__);
		snd_soc_update_bits_locked(priv->codec, ARIZONA_AIF1_BCLK_CTRL,
				ARIZONA_AIF1_BCLK_MSTR_MASK, 0);
		snd_soc_update_bits_locked(priv->codec, ARIZONA_AIF1_TX_PIN_CTRL,
				ARIZONA_AIF1TX_LRCLK_MSTR_MASK, 0);
		snd_soc_update_bits_locked(priv->codec, ARIZONA_AIF1_RX_PIN_CTRL,
				ARIZONA_AIF1RX_LRCLK_MSTR_MASK, 0);
	}
	return 0;
}

static int pacific_resume_pre(struct snd_soc_card *card)
{
	/* When the card enters resume state, If codec is not active, the micbias of headset
	 * is enable and the ez2control is not running, The MCLK and the FLL1 should be enable.
	 * In the other cases, these should keep previous status */
	if (pacific_check_clock_conditions(card)) {
		pacific_start_sysclk(card);
		dev_info(card->dev, "%s\n", __func__);
	}
	return 0;
}

static struct snd_soc_card pacific_cards[] = {
	{
		.name = "Pacific WM5102 Sound",
		.owner = THIS_MODULE,

		.dai_link = pacific_wm5102_dai,
		.num_links = ARRAY_SIZE(pacific_wm5102_dai),

		.controls = pacific_controls,
		.num_controls = ARRAY_SIZE(pacific_controls),
		.dapm_widgets = pacific_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pacific_dapm_widgets),
		.dapm_routes = pacific_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(pacific_dapm_routes),

		.late_probe = pacific_late_probe,

		.suspend_post = pacific_suspend_post,
		.resume_pre = pacific_resume_pre,

		.set_bias_level = pacific_set_bias_level,
		.set_bias_level_post = pacific_set_bias_level_post,

		.drvdata = &pacific_wm5102_priv,
	},
	{
		.name = "Pacific WM5110 Sound",
		.owner = THIS_MODULE,

		.dai_link = pacific_wm5110_dai,
		.num_links = ARRAY_SIZE(pacific_wm5110_dai),

		.controls = pacific_controls,
		.num_controls = ARRAY_SIZE(pacific_controls),
		.dapm_widgets = pacific_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pacific_dapm_widgets),
		.dapm_routes = pacific_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(pacific_dapm_routes),

		.late_probe = pacific_late_probe,

		.suspend_post = pacific_suspend_post,
		.resume_pre = pacific_resume_pre,

		.set_bias_level = pacific_set_bias_level,
		.set_bias_level_post = pacific_set_bias_level_post,

		.drvdata = &pacific_wm5110_priv,
	},
};

const char *card_ids[] = {
	"wm5102", "wm5110"
};

static int pacific_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai_link;
	struct arizona_machine_priv *priv;

	for (n = 0; n < ARRAY_SIZE(card_ids); n++) {
		if (NULL != of_find_node_by_name(NULL, card_ids[n])) {
			card = &pacific_cards[n];
			dai_link = card->dai_link;
			break;
		}
	}

	if (n == ARRAY_SIZE(card_ids)) {
		dev_err(&pdev->dev, "couldn't find card\n");
		ret = -EINVAL;
		goto out;
	}

	card->dev = &pdev->dev;
	priv = card->drvdata;

	priv->mclk = devm_clk_get(card->dev, "mclk");
	if (IS_ERR(priv->mclk)) {
		dev_dbg(card->dev, "Device tree node not found for mclk");
		priv->mclk = NULL;
	} else
		clk_prepare(priv->mclk);

	ret = snd_soc_register_component(card->dev, &pacific_cmpnt,
				pacific_ext_dai, ARRAY_SIZE(pacific_ext_dai));
	if (ret != 0)
		dev_err(&pdev->dev, "Failed to register component: %d\n", ret);

	if (np) {
		for (n = 0; n < card->num_links; n++) {

			/* Skip parsing DT for fully formed dai links */
			if (dai_link[n].platform_name &&
			    dai_link[n].codec_name) {
				dev_dbg(card->dev, "Skipping dt for populated"
					"dai link %s\n", dai_link[n].name);
				continue;
			}

			cpu_np = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (!cpu_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-cpu'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			codec_np = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!codec_np) {
				dev_err(&pdev->dev,
					"Property 'samsung,audio-codec'"
					" missing or invalid\n");
				ret = -EINVAL;
				goto out;
			}

			if (!dai_link[n].cpu_dai_name)
				dai_link[n].cpu_of_node = cpu_np;

			if (!dai_link[n].platform_name)
				dai_link[n].platform_of_node = cpu_np;

			dai_link[n].codec_of_node = codec_np;
		}
	} else
		dev_err(&pdev->dev, "Failed to get device node\n");

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card:%d\n", ret);
		goto out;
	}

	return ret;

out:
	return ret;
}

static int pacific_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct arizona_machine_priv *priv = card->drvdata;

	snd_soc_unregister_card(card);

	if (priv->mclk) {
		clk_unprepare(priv->mclk);
		devm_clk_put(card->dev, priv->mclk);
	}

	return 0;
}

static const struct of_device_id pacific_arizona_of_match[] = {
	{ .compatible = "samsung,pacific-arizona", },
	{ },
};
MODULE_DEVICE_TABLE(of, pacific_arizona_of_match);

static struct platform_driver pacific_audio_driver = {
	.driver	= {
		.name	= "pacific-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(pacific_arizona_of_match),
	},
	.probe	= pacific_audio_probe,
	.remove	= pacific_audio_remove,
};

module_platform_driver(pacific_audio_driver);

MODULE_DESCRIPTION("ALSA SoC PACIFIC ARIZONA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pacific-audio");
