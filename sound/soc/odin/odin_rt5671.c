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

#define DEBUG 1
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/gpio.h>
#include <sound/jack.h>
#include <sound/tpa2028d.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/dmaengine_pcm.h>
#include <asm/system_info.h>
#include "odin_asoc_utils.h"

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#include "../codecs/rt5671.h"

#define SPK_ON	1
#define SPK_OFF	0
#define EXT_OSC_ON 1
#define EXT_OSC_OFF 0

static void odin_rt5671_set_aud_osc(int pin, int state);

struct odin_rt5671 {
	int jack_status;
	int aud_ldo_en;
	int aud_irq;
	int ao_mic_off;
	int hph_aud_sw_int;
	int aud_osc_en;
	int ext_osc;
	int pll_control;
	char revision[10];
	struct snd_soc_codec *rt5671_codec;
	bool calling;
};

struct odin_rt5671 *odin_machine_data;
struct class *snd_card_rev_class;

static ssize_t snd_card_rev_show (struct class *class,
			struct class_attribute *attr, char *buf)
{
	int rc = 0;
	u8 board_rev = 0;

	board_rev = (system_rev & 0xff);

	switch (board_rev) {
	case 0x10:
		rc += sprintf(buf, "HDK\n");
		break;
	case 0x12:
		rc += sprintf(buf, "RDK_I\n");
		break;
	case 0x13:
	case 0xa:
		rc += sprintf(buf, "RDK_J\n");
		break;
	default:
		rc += sprintf(buf, "RDK_J\n");
		pr_err("[%s]Not suitable revision value:%x\n", __func__, board_rev);
		break;
	}
	return rc;
}
static CLASS_ATTR(snd_card_rev, S_IWUSR|S_IRUGO, snd_card_rev_show, NULL);

static int odin_rt5671_startup(struct snd_pcm_substream *substream)
{
	struct device *dev = substream->pcm->card->dev;
	dev_info(dev, "[%i] %s startup, devnum %d\n", task_pid_nr(current),
		 substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Ply" : "Cap",
		 substream->pcm->device);
	return 0;
}

static void odin_rt5671_shutdown(struct snd_pcm_substream *substream)
{
	struct device *dev = substream->pcm->card->dev;
	dev_info(dev, "[%i] %s shutdown, devnum %d\n", task_pid_nr(current),
		 substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Ply" : "Cap",
		 substream->pcm->device);
}

static int odin_rt5671_hw_params(struct snd_pcm_substream *substream,
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

	dev_info(dev, "%s devnum %d rate %d ch %d psize %d pcnt %d buf(%d,%d)\n",
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Ply" : "Cap",
		substream->pcm->device,
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

	if (machine->pll_control) {
		/* Use PLL, mclk is source clock of PLL */
		dev_dbg(dev, "Codec PLL1 seted Fin %d, Fout %d\n",
				machine->ext_osc, sysclk);

		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5671_PLL1_S_MCLK,
						machine->ext_osc, sysclk);
		if (ret < 0) {
			dev_err(dev, "Failed to set codec_dai sysclk\n");
			return ret;
		}

		/* Select PLL as system clock */
		ret = snd_soc_dai_set_sysclk(codec_dai, RT5671_SCLK_S_PLL1,
						sysclk, SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set codec pll\n");
			return ret;
		}
	} else {
		/* Use the original MCLK */
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, machine->ext_osc,
						SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set codec pll\n");
			return ret;
		}
	}

	return ret;
}

static int odin_hifi_hw_params(struct snd_pcm_substream *substream,
           struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);

	int ret;
	unsigned int cpudai_clk;
	unsigned int codecdai_clk;

	dev_info(dev, "%s devnum %d rate %d ch %d psize %d pcnt %d buf(%d,%d)\n",
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "Ply" : "Cap",
		substream->pcm->device,
		params_rate(params),
		params_channels(params),
		params_period_size(params),
		params_periods(params),
		params_buffer_size(params),
		params_buffer_bytes(params));

	/* Set the Codec DAI configuration */
	/* rt5671_set_dai_fmt */
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

	/* If si2s used the slave mode, clk will be required to set bclk*6 */
	/* (64fs(bclk) * 6 = 384fs) */
	/* We set 512fs because 384fs is not divisor for aud_pll */
	cpudai_clk = (params_rate(params) * 512);
	codecdai_clk = (params_rate(params) * 256);

	dev_dbg(dev, "Set cpudai %d, codec dai %d\n", cpudai_clk, codecdai_clk);
	/* odin_si2s_set_sysclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, cpudai_clk, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(dev, "Failed to set cpu_dai sysclk\n");
		return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5671_PLL1_S_MCLK,
					machine->ext_osc, codecdai_clk);
	if (ret < 0){
		dev_err(dev, "Failed to set codec pll\n");
		return ret;
	}

	/* rt5671_set_dai_sysclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5671_SCLK_S_PLL1,
					codecdai_clk, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		dev_err(dev, "Failed to set codec_dai sysclk\n");
		return ret;
	}

	return ret;
}

static int odin_compr_set_params(struct snd_compr_stream *cstream)
{
	struct device *dev = cstream->device->card->dev;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	struct snd_pcm_substream *substream = NULL;
	struct snd_pcm_hw_params *params;
	int sampling_rate = 48000;
	int ret = 0;
	unsigned int sysclk = 0;

	pr_debug("%s\n",__func__);

	/* odin_i2s_set_sysclk */
	dev_dbg(dev, "CODEC_MASTER sampling rate :%d\n", sampling_rate);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_I2S |
					SND_SOC_DAIFMT_NB_NF |
					SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0){
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}


	if (machine->pll_control) {
		/* Use PLL, mclk is source clock of PLL */
		dev_dbg(dev, "Codec PLL1 seted Fin %d, Fout %d\n",
				machine->ext_osc, sysclk);

		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5671_PLL1_S_MCLK,
				machine->ext_osc, sysclk);
		if (ret < 0) {
			dev_err(dev, "Failed to set codec_dai sysclk\n");
			return ret;
		}

		/* Select PLL as system clock */
		ret = snd_soc_dai_set_sysclk(codec_dai, RT5671_SCLK_S_PLL1,
						sysclk, SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set codec pll\n");
			return ret;
		}
	}
	else {
		/* Use the original MCLK */
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, machine->ext_osc,
						SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set codec pll\n");
			return ret;
		}
	}

	if (codec_dai->driver->ops->hw_params)
	{
		params = kzalloc(sizeof(struct snd_pcm_hw_params), GFP_KERNEL);

		_snd_pcm_hw_params_any(params);
		_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_CHANNELS,
				2, 0);
		_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_FORMAT,
				SNDRV_PCM_FORMAT_S16_LE, 0);
		_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_RATE,
				48000, 0);
		_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_PERIODS,
				8, 0);
		_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
				1024, 0);

		ret = codec_dai->driver->ops->hw_params(substream, params, codec_dai);
		if (ret < 0)
		{
			dev_err(dev, "Failed to set hw params\n");
			kfree(params);
			return ret;
		}

		kfree(params);
	}

	return 0;
}

static void odin_compr_shutdown(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_pcm_substream *substream = NULL;

	if (codec_dai->driver->ops->shutdown)
		codec_dai->driver->ops->shutdown(substream, codec_dai);

}

static struct snd_soc_jack odin_rt5671_hp_jack;

#ifdef CONFIG_SWITCH
static struct switch_dev wired_switch_dev = {
	.name = "h2w",
};

/* These values are copied from WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
};

static int odin_rt5671_jack_notifier(struct notifier_block *self,
			      unsigned long action, void *dev)
{
	struct snd_soc_jack *jack = dev;
	struct snd_soc_codec *codec = jack->codec;
	struct snd_soc_card *card = codec->card;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	enum headset_state state = 0;
	int modified_state = 0;
	int status = 0;
	int irq_event = 0;
	unsigned int vad_status = 0;

	if (jack == &odin_rt5671_hp_jack){
		state = 0;

		dev_dbg(card->dev, "[%s] This is EARJACK IRQ! action = %d\n",
				__func__, action);
			if (action){
				irq_event = rt5671_check_interrupt_event(codec, &status);
				dev_dbg(card->dev, "[%s] irq_event : %d\n", __FUNCTION__, irq_event);

				switch (irq_event) {
				case RT5671_VAD_EVENT:

					dev_dbg(card->dev, "[%s] This is VAD IRQ!\n", __func__);

					/* Assume that the 200ms to be ready for DSP */
					mdelay(200);
					dev_dbg(card->dev,
							"[%s] DSP is ready for processing sensory algorithm..\n", __func__);

					dev_dbg(card->dev, "[%s] OK. Sending Ack to Codec!\n", __func__);
					snd_soc_write(machine->rt5671_codec, 0x9a, 0x277c);

					/* if the command matched by sensory algorithm,
					 * Turn off the VAD using the below I2C command.
					 *
					 * snd_soc_write(machine->rt5671_codec, 0x9a, 0x2784);
					 *
					 * When phone goes into sleep mode,
					 * VAD will be turned on automatically.
					 */
					break;
				case RT5671_J_IN_EVENT:
				case RT5671_J_OUT_EVENT:
					dev_dbg(card->dev, "[%s] status : %d\n", __FUNCTION__, status);
					switch (status) {
					case SND_JACK_HEADSET :
						state = BIT_HEADSET;
						break;
					case SND_JACK_HEADPHONE :
						state = BIT_HEADSET_NO_MIC;
						break;
					default:
						state = BIT_NO_HEADSET;
					}
					switch_set_state(&wired_switch_dev, state);
					dev_dbg(card->dev, "hyunsoo dbg: %s %d action :%d state:%d\n",
					__FUNCTION__, __LINE__,action , state);
					break;
				case RT5671_BTN_EVENT:
					dev_dbg(card->dev, "[%s] status : %d\n", __FUNCTION__, status);
					if (status & 0x4900)
						dev_dbg(card->dev, "Double Click\n");
					else if (status & 0x9200)
						dev_dbg(card->dev, "One Click\n");
					else if (status & 0x2480)
						dev_dbg(card->dev, "Hold Push\n");
					break;
				case RT5671_BR_EVENT:
					dev_dbg(card->dev, "[%s] status : %d\n", __FUNCTION__, status);
					if (status == 0)
						dev_dbg(card->dev, "Hold Release\n");
					break;
				default:
					dev_dbg(card->dev, "Unknow IRQ event\n");
					break;
				}
			}
	}

	return NOTIFY_OK;

}

static struct notifier_block odin_rt5671_jack_detect_nb = {
	.notifier_call = odin_rt5671_jack_notifier,
};
#else

static struct snd_soc_jack_pin odin_rt5671_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};
#endif

static struct snd_soc_jack_gpio odin_rt5671_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 0,
	.invert = 0,
};

static int odin_rt5671_event_ext_spk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{

	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	if (SND_SOC_DAPM_EVENT_ON(event)){
		if (!strncmp(w->name, "Ext Spk", 7))
			set_amp_gain(SPK_ON);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n", __func__, w->name);
			return -EINVAL;
		}
	} else {
		if (!strncmp(w->name, "Ext Spk", 7))
			set_amp_gain(SPK_OFF);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n", __func__, w->name);
			return -EINVAL;
		}
	}

	return 0;
}

static int odin_rt5671_event_hp(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
		return 0;
}


static const struct snd_kcontrol_new odin_rt5671_controls[] = {
	SOC_DAPM_PIN_SWITCH("Receiver"),
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("MainMic"),
	SOC_DAPM_PIN_SWITCH("SubMic1"),
	SOC_DAPM_PIN_SWITCH("SubMic2"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("BT PCM OUT"),
	SOC_DAPM_PIN_SWITCH("BT PCM IN"),
};

static const struct snd_soc_dapm_widget odin_rt5671_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Receiver", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", odin_rt5671_event_ext_spk),
	SND_SOC_DAPM_HP("Headphone Jack", odin_rt5671_event_hp),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic1", NULL),
	SND_SOC_DAPM_MIC("SubMic2", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_INPUT("BT PCM OUT"),
	SND_SOC_DAPM_OUTPUT("BT PCM IN"),
};

static const struct snd_soc_dapm_widget odin_rt5671_dapm_widgets_RevI[] = {
	SND_SOC_DAPM_SPK("Receiver", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", odin_rt5671_event_hp),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic1", NULL),
	SND_SOC_DAPM_MIC("SubMic2", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_INPUT("BT PCM OUT"),
	SND_SOC_DAPM_OUTPUT("BT PCM IN"),
};

static const struct snd_soc_dapm_widget odin_rt5671_dapm_widgets_RevG[] = {
	SND_SOC_DAPM_SPK("Receiver", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", odin_rt5671_event_ext_spk),
	SND_SOC_DAPM_HP("Headphone Jack", odin_rt5671_event_hp),
	SND_SOC_DAPM_MIC("MainMic", NULL),
	SND_SOC_DAPM_MIC("SubMic1", NULL),
	SND_SOC_DAPM_MIC("SubMic2", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_INPUT("BT PCM OUT"),
	SND_SOC_DAPM_OUTPUT("BT PCM IN"),
};

static const struct snd_soc_dapm_route odin_rt5671_audio_map[] = {
	/* receiver SPK */
	{"Receiver", NULL, "MonoP"},
	{"Receiver", NULL, "MonoN"},

	/* spk */
	{"Ext Spk", NULL, "LOUTL"},
	{"Ext Spk", NULL, "LOUTR"},

	/* hp jack */
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},

	/* Analog Main Mic */
	{"IN3P", NULL, "micbias1"},
	{"IN3N", NULL, "micbias1"},
	{"micbias1", NULL, "MainMic"},

	/* Analog SubMic1 */
	{"IN2P", NULL, "micbias2"},
	{"IN2N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic1"},

	/* Analog SubMic2 */
	{"IN4P", NULL, "micbias2"},
	{"IN4N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic2"},

	/* Headset Mic */
	{"IN1P", NULL, "Headset Mic"},
	{"IN1N", NULL, "Headset Mic"},

	{"BT PCM DATA IN", NULL, "BT PCM OUT"},
	{"BT PCM IN", NULL, "BT PCM DATA OUT"},
};

static const struct snd_soc_dapm_route odin_rt5671_audio_map_RevI[] = {
	/* receiver SPK */
	{"Receiver", NULL, "MonoP"},
	{"Receiver", NULL, "MonoN"},

	/* PDM spk for ALC9010 DEVSEL GND */
	{"Ext Spk", NULL, "PDM1L"},

	/* hp jack */
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},

	/* Analog Main Mic */
	{"IN3P", NULL, "micbias1"},
	{"IN3N", NULL, "micbias1"},
	{"micbias1", NULL, "MainMic"},

	/* Analog SubMic1 */
	{"IN2P", NULL, "micbias2"},
	{"IN2N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic1"},

	/* Analog SubMic2 */
	{"IN4P", NULL, "micbias2"},
	{"IN4N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic2"},

	/* Headset Mic */
	{"IN1P", NULL, "Headset Mic"},
	{"IN1N", NULL, "Headset Mic"},

	{"BT PCM DATA IN", NULL, "BT PCM OUT"},
	{"BT PCM IN", NULL, "BT PCM DATA OUT"},
};

static const struct snd_soc_dapm_route odin_rt5671_audio_map_RevG[] = {
	/* receiver SPK */
	{"Receiver", NULL, "MonoP"},
	{"Receiver", NULL, "MonoN"},

	/* spk */
	{"Ext Spk", NULL, "LOUTL"},
	{"Ext Spk", NULL, "LOUTR"},

	/* hp jack */
	{"Headphone Jack", NULL, "HPOL"},
	{"Headphone Jack", NULL, "HPOR"},

	/* Main Mic */
	{"DMIC L2", NULL, "MainMic"},
	{"DMIC R2", NULL, "MainMic"},

	/* SubMic1 */
	{"IN2P", NULL, "micbias2"},
	{"IN2N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic1"},

	/* SubMic2 */
	{"IN4P", NULL, "micbias2"},
	{"IN4N", NULL, "micbias2"},
	{"micbias2", NULL, "SubMic2"},

	/* Headset Mic */
	{"IN1P", NULL, "micbias1"},
	{"IN1N", NULL, "micbias1"},
	{"micbias1", NULL, "Headset Mic"},

	{"BT PCM DATA IN", NULL, "BT PCM OUT"},
	{"BT PCM IN", NULL, "BT PCM DATA OUT"},
};

static int odin_rt5671_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);

	if (machine == NULL)
 		return -EINVAL;

	/* add codec pointer to machine struct */
	machine->rt5671_codec = codec;

	pr_info("%s %d\n", __func__, __LINE__);

	/* enabling audio jack irq */
	if(machine->aud_irq != NULL){
		if (gpio_is_valid(machine->aud_irq)) {
			odin_rt5671_hp_jack_gpio.gpio = machine->aud_irq;
			snd_soc_jack_new(codec, "Headphone Jack",
					SND_JACK_HEADPHONE,
					&odin_rt5671_hp_jack);
#ifndef CONFIG_SWITCH
			snd_soc_jack_add_pins(&odin_rt5671_hp_jack,
					ARRAY_SIZE(odin_rt5671_hp_jack_pins),
					odin_rt5671_hp_jack_pins);
#else
			snd_soc_jack_notifier_register(&odin_rt5671_hp_jack,
					&odin_rt5671_jack_detect_nb);
#endif
			snd_soc_jack_add_gpios(&odin_rt5671_hp_jack, 1,
					&odin_rt5671_hp_jack_gpio);
		}
	}


#if 0
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");
		snd_soc_dapm_enable_pin(dapm, "MainMic");
		snd_soc_dapm_enable_pin(dapm, "SubMic");
		snd_soc_dapm_enable_pin(dapm, "RecogMic");
		snd_soc_dapm_enable_pin(dapm, "Headset Mic");
#endif

	snd_soc_dapm_disable_pin(dapm, "BT PCM OUT");
	snd_soc_dapm_disable_pin(dapm, "BT PCM IN");
	return 0;
}

static struct snd_soc_ops odin_ops = {
	.startup = odin_rt5671_startup,
	.hw_params = odin_rt5671_hw_params,
	.shutdown = odin_rt5671_shutdown,
};

static struct snd_soc_ops odin_hifi_dai_link_ops = {
	.hw_params = odin_hifi_hw_params,
};

static struct snd_soc_compr_ops odin_compr_ops = {
	.set_params = odin_compr_set_params,
	.shutdown = odin_compr_shutdown,
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
			.stream_name = "BT PCM DATA OUT",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.stream_name = "BT PCM DATA IN",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	},
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

static int odin_call_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);

	if (machine->calling) {
		pr_warn("[AUD] Call is already turned on\n");
		return 0;
	}

	machine->calling = true;

	return 0;
}

static int odin_call_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);

	if (machine->calling == false) {
		pr_warn("[AUD] Call is already turned off\n");
		return 0;
	}

	machine->calling = false;

	return 0;
}

static int odin_call_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	unsigned int sysclk;
	int ret;

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0){
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}

	if (machine->pll_control) {
		/* sysclk = 16000 * 512 = 8192000 */
		sysclk = params_rate(params) * 512;

		ret = snd_soc_dai_set_sysclk(codec_dai, RT5671_SCLK_S_PLL1,
						sysclk, SND_SOC_CLOCK_OUT);

		if (ret < 0){
			dev_err(dev, "Failed to set sysclk\n");
			return ret;
		}

		/* Use PLL, mclk is source clock of PLL */
		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5671_PLL1_S_MCLK,
			machine->ext_osc, sysclk);

		if (ret < 0){
			dev_err(dev, "Failed to set pll\n");
			return ret;
		}
	}
	else {
		/* Use the original MCLK */
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, machine->ext_osc,
						SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set sysclk\n");
			return ret;
		}
	}

	return ret;
}

static int odin_bt_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = substream->pcm->card->dev;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct odin_rt5671 *machine = snd_soc_card_get_drvdata(codec->card);
	unsigned int sysclk;
	int ret;

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_DSP_A |
		SND_SOC_DAIFMT_IB_NF |
		SND_SOC_DAIFMT_CBM_CFM);

	if (ret < 0){
		dev_err(dev, "Failed to set codec_dai format\n");
		return ret;
	}

	if (machine->pll_control) {
		/* sysclk = 16000 * 512 = 8192000 */
		sysclk = params_rate(params) * 512;

		ret = snd_soc_dai_set_sysclk(codec_dai, RT5671_SCLK_S_PLL1,
						sysclk, SND_SOC_CLOCK_OUT);

		if (ret < 0){
			dev_err(dev, "Failed to set sysclk\n");
			return ret;
		}

		/* Use PLL, mclk is source clock of PLL */
		ret = snd_soc_dai_set_pll(codec_dai, 0, RT5671_PLL1_S_MCLK,
			machine->ext_osc, sysclk);

		if (ret < 0){
			dev_err(dev, "Failed to set pll\n");
			return ret;
		}
	}
	else {
		/* Use the original MCLK */
		ret = snd_soc_dai_set_sysclk(codec_dai, 0, machine->ext_osc,
						SND_SOC_CLOCK_OUT);
		if (ret < 0){
			dev_err(dev, "Failed to set sysclk\n");
			return ret;
		}
	}

	return ret;
}


static struct snd_soc_ops odin_call_ops = {
	.startup = odin_call_startup,
	.shutdown = odin_call_shutdown,
	.hw_params = odin_call_hw_params,
};

static struct snd_soc_ops odin_bt_ops = {
	.hw_params = odin_bt_hw_params,
};

static const struct snd_soc_pcm_stream bt_stream = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 0,
	.rate_max = 0,
	.channels_min = 1,
	.channels_max = 2,
};

static struct snd_soc_dai_link odin_dai_link[] = {
	{
		.name = "Playback Deep",
		.stream_name = "RT5671 Deep Playback",
		.cpu_dai_name = "odin-si2s.0",
		.codec_dai_name = "rt5671-aif1",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.init = odin_rt5671_init,
		.ops = &odin_ops,
	},
	{
		.name = "Capture Normal",
		.stream_name = "RT5671 Capture",
		.cpu_dai_name = "odin-si2s.1",
		.codec_dai_name = "rt5671-aif1",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_ops,
	},
	{	/* Not use */
		.name = "Playback HiFi",
		.stream_name = "RT5671 HiFi Playback",
		.cpu_dai_name = "odin-si2s.2",
		.codec_dai_name = "rt5671-aif4",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_hifi_dai_link_ops,
		.no_pcm = 1,	/* To avoid pcm buffer allocation */
	},
	{	/* Not use */
		.name = "Capture HiFi",
		.stream_name = "RT5671 HiFi Capture",
		.cpu_dai_name = "odin-si2s.3",
		.codec_dai_name = "rt5671-aif4",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_ops,
	},
	{
		.name = "VoiceCall",
		.stream_name = "RT5671 VoiceCall",
		.cpu_dai_name = "odin-voice-dai",
		.codec_dai_name = "rt5671-aif2",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_call_ops,
#ifdef IGNORE_SUSPEND
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
#endif
	},
	{
		.name = "BlueTooth",
		.stream_name = "RT5671 bt-sco",
		.cpu_dai_name = "odin-bt-dai",
		.codec_dai_name = "rt5671-aif3",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_bt_ops,
#ifdef IGNORE_SUSPEND
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
#endif
	},
	{
		.name = "DSP Compress LPA",
		.stream_name = "RT5671 Compress LPA Dai",
		.cpu_dai_name ="odin-compress-dsp-dai",
		.codec_dai_name = "rt5671-compress",
#if CONFIG_SND_SOC_ODIN_COMPR
		.platform_name = "odin-compress-dsp",
#else
		.platform_name = "snd-soc-dummy",
#endif
		.codec_name = "rt5671.4-001c",
		.compr_ops = &odin_compr_ops,
		.no_pcm = 1,
	},
	{
		.name = "Playback Low",
		.stream_name = "RT5671 Low Playback",
		.cpu_dai_name = "odin-si2s.2",
		.codec_dai_name = "rt5671-aif4",
		.platform_name = "odin-pcm-audio",
		.codec_name = "rt5671.4-001c",
		.ops = &odin_ops,
	},
	{
		.name = "Bluetooth SCO",
		.stream_name = "RT5671 Bluetooth SCO",
		.cpu_dai_name = "odin-bt-dai",
		.codec_dai_name = "rt5671-aif3",
		.codec_name = "rt5671.4-001c",
		.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_IB_NF
		            | SND_SOC_DAIFMT_CBM_CFM,
		.params = &bt_stream,
#ifdef IGNORE_SUSPEND
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
#endif
	},
};

static int odin_rt5671_susepnd_post(struct snd_soc_card *card)
{
	struct odin_rt5671 *rt5671_machine = snd_soc_card_get_drvdata(card);

	if (!strncmp(rt5671_machine->revision, "RevG", 4) ||
			!strncmp(rt5671_machine->revision, "RevI", 4))
		return 0;

	if (rt5671_machine->aud_osc_en != NULL &&
			gpio_is_valid(rt5671_machine->aud_osc_en) &&
			rt5671_machine->calling == false) {
		odin_rt5671_set_aud_osc(rt5671_machine->aud_osc_en,
				EXT_OSC_OFF);
	}

	return 0;
}

static int odin_rt5671_resume_pre(struct snd_soc_card *card)
{
	struct odin_rt5671 *rt5671_machine = snd_soc_card_get_drvdata(card);

	if (!strncmp(rt5671_machine->revision, "RevG", 4) ||
			!strncmp(rt5671_machine->revision, "RevI", 4))
		return 0;

	if (rt5671_machine->aud_osc_en != NULL &&
			gpio_is_valid(rt5671_machine->aud_osc_en) ) {
		odin_rt5671_set_aud_osc(rt5671_machine->aud_osc_en,
				EXT_OSC_ON);
	}

	return 0;
}

static void odin_rt5671_set_aud_osc(int pin, int state)
{
	gpio_set_value(pin, state);
	printk("%s: aud_osc_en is set to %s\n", __func__,
			state ? "high" : "low");
}

#ifdef IGNORE_SUSPEND
static int odin_rt5671_late_probe(struct snd_soc_card *card)
{
    struct snd_soc_dapm_context *dapm = &card->dapm;

    /* set endpoints to not connected */
    snd_soc_dapm_nc_pin(dapm, "DMIC L1");
    snd_soc_dapm_nc_pin(dapm, "DMIC R1");
    snd_soc_dapm_nc_pin(dapm, "DMIC L2");
    snd_soc_dapm_nc_pin(dapm, "DMIC R2");
    snd_soc_dapm_nc_pin(dapm, "DMIC L3");
    snd_soc_dapm_nc_pin(dapm, "DMIC R3");

    snd_soc_dapm_nc_pin(dapm, "PDM1L");
    snd_soc_dapm_nc_pin(dapm, "PDM1R");
    snd_soc_dapm_nc_pin(dapm, "PDM2L");
    snd_soc_dapm_nc_pin(dapm, "PDM2R");

    snd_soc_dapm_ignore_suspend(dapm, "Receiver");
    snd_soc_dapm_ignore_suspend(dapm, "Ext Spk");
    snd_soc_dapm_ignore_suspend(dapm, "Headphone Jack");
    snd_soc_dapm_ignore_suspend(dapm, "MainMic");
    snd_soc_dapm_ignore_suspend(dapm, "SubMic1");
    snd_soc_dapm_ignore_suspend(dapm, "SubMic2");
    snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");

    snd_soc_dapm_ignore_suspend(dapm, "BT PCM IN");
    snd_soc_dapm_ignore_suspend(dapm, "BT PCM OUT");

    snd_soc_dapm_sync(dapm);

    return 0;
}
#endif

static struct snd_soc_card snd_soc_odin_rt5671 = {
	.name = "OdinAudioCard",
	.owner = THIS_MODULE,
	.dai_link = odin_dai_link,
	.num_links = ARRAY_SIZE(odin_dai_link),

	.controls = odin_rt5671_controls,
	.num_controls = ARRAY_SIZE(odin_rt5671_controls),
	.dapm_widgets = odin_rt5671_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(odin_rt5671_dapm_widgets),
	.dapm_routes = odin_rt5671_audio_map,
	.num_dapm_routes = ARRAY_SIZE(odin_rt5671_audio_map),
	.fully_routed = true,
	.suspend_post = odin_rt5671_susepnd_post,
	.resume_pre = odin_rt5671_resume_pre,
#ifdef IGNORE_SUSPEND
	.late_probe = odin_rt5671_late_probe,
#endif
};

static const struct snd_soc_component_driver odin_rt5671_component = {
	.name = "odin_rt5671",
};
#ifdef CONFIG_OF
static int odin_of_gpio_property_get(struct snd_soc_card *card,
		struct odin_rt5671 *rt5671_machine)
{

	struct device_node *np = card->dev->of_node;
	const char *temp_string = np->name;
	int rc;
	int len;

	printk(KERN_DEBUG"%s\n", __func__);

	rc = of_property_read_u32(np, "aud_ldo_en", &rt5671_machine->aud_ldo_en);
	rc = of_property_read_u32(np, "aud_irq", &rt5671_machine->aud_irq);
	rc = of_property_read_u32(np, "ao_mic_off", &rt5671_machine->ao_mic_off);
	rc = of_property_read_u32(np, "hph_aud_sw_int",
					&rt5671_machine->hph_aud_sw_int);
	rc = of_property_read_u32(np, "aud_osc_en", &rt5671_machine->aud_osc_en);
	rc = of_property_read_u32(np, "ext_osc", &rt5671_machine->ext_osc);
	rc = of_property_read_u32(np, "pll_control",
					&rt5671_machine->pll_control);
	rc = of_property_read_string(np, "revision", &temp_string);
	len = strlen(temp_string);
	memcpy(rt5671_machine->revision, temp_string, len);

	printk(KERN_DEBUG "aud_ldo_en : %d\n", rt5671_machine->aud_ldo_en);
	printk(KERN_DEBUG "aud_irq : %d\n", rt5671_machine->aud_irq);
	printk(KERN_DEBUG "ao_mic_off : %d\n", rt5671_machine->ao_mic_off);
	printk(KERN_DEBUG "hph_aud_sw_int : %d\n", rt5671_machine->hph_aud_sw_int);
	printk(KERN_DEBUG "aud_osc_en : %d\n", rt5671_machine->aud_osc_en);
	printk(KERN_DEBUG "ext_osc : %d\n", rt5671_machine->ext_osc);
	printk(KERN_DEBUG "pll_control : %d\n", rt5671_machine->pll_control);
	printk(KERN_DEBUG "revision : %s\n", &rt5671_machine->revision);

	return rc;
}
#endif

static void odin_rt5671_card_hwfixup(struct odin_rt5671 *rt5671_machine)
{
	if(!strncmp(rt5671_machine->revision, "RevG", 4)) {
		snd_soc_odin_rt5671.dapm_routes = odin_rt5671_audio_map_RevG;
		snd_soc_odin_rt5671.num_dapm_routes = ARRAY_SIZE(odin_rt5671_audio_map_RevG);
		snd_soc_odin_rt5671.dapm_widgets = odin_rt5671_dapm_widgets_RevG;
		snd_soc_odin_rt5671.num_dapm_widgets = ARRAY_SIZE(odin_rt5671_dapm_widgets_RevG);
	} else if (!strncmp(rt5671_machine->revision, "RevI", 4)) {
		snd_soc_odin_rt5671.dapm_routes = odin_rt5671_audio_map_RevI;
		snd_soc_odin_rt5671.num_dapm_routes = ARRAY_SIZE(odin_rt5671_audio_map_RevI);
		snd_soc_odin_rt5671.dapm_widgets = odin_rt5671_dapm_widgets_RevI;
		snd_soc_odin_rt5671.num_dapm_widgets = ARRAY_SIZE(odin_rt5671_dapm_widgets_RevI);

	}
}

static int odin_rt5671_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_odin_rt5671;
	struct odin_rt5671 *rt5671_machine;
	int ret = 0;

	rt5671_machine = devm_kzalloc(&pdev->dev,
		sizeof(struct odin_rt5671), GFP_KERNEL);
	if (!rt5671_machine) {
		dev_err(&pdev->dev, "Can't allocate odin_rt5671\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;

#ifdef CONFIG_OF
	odin_of_gpio_property_get(card, rt5671_machine);
#endif
	odin_rt5671_card_hwfixup(rt5671_machine);
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, rt5671_machine);

	/* If Main board revision is not RevF or RevG or RDK0
	 * enabling micbias ldo for codec 3.3V_VDD_MIC
	 * (aud_ldo_en is set to 1.8V VDD in RDK0)
	 * */
	if(rt5671_machine->aud_ldo_en != NULL){
		if (gpio_is_valid(rt5671_machine->aud_ldo_en)) {
			ret = gpio_request_one(rt5671_machine->aud_ldo_en,
				GPIOF_DIR_OUT | GPIOF_INIT_LOW,
				"micbias ldo enable");
			if (ret)
				return ret;

			gpio_set_value_cansleep(rt5671_machine->aud_ldo_en, 1);
				dev_dbg(&pdev->dev, "mic bias ldo enabled");
	        }
	}

	/* enabling micbias for 1.8V_Main Mic & Always-On Mic.
	 * No need to use ao_mic_off if board is RDK0.
	 * (external jack IC will use this for EAR_KEY in RDK0)
	 */
	if(rt5671_machine->ao_mic_off != NULL){
		if (gpio_is_valid(rt5671_machine->ao_mic_off)){
			ret = gpio_request_one(rt5671_machine->ao_mic_off,
				GPIOF_DIR_OUT | GPIOF_INIT_LOW, "always-on mic bias");
			if (ret)
				return ret;

			gpio_set_value_cansleep(rt5671_machine->ao_mic_off, 1);
			dev_dbg(&pdev->dev, "always-on mic bias enabled");
		}
	}

	/* if revision is not RevF or RevG or RDK0, Run the below code
	 * (hph_aud_sw_int is not AP GPIO in RDK0)
	 */
	if(rt5671_machine->hph_aud_sw_int != NULL){
		/* HPH_AUD_SW_INT */
		if(gpio_is_valid(rt5671_machine->hph_aud_sw_int)){
			ret = gpio_request(rt5671_machine->hph_aud_sw_int,
					"earjeck debugger switch int");
			if (ret)
				return ret;

			gpio_direction_input(rt5671_machine->hph_aud_sw_int);
			dev_dbg(&pdev->dev, "earjack debugger s/w enabled");
		}
	}

	if(rt5671_machine->aud_osc_en != NULL){
		if (gpio_is_valid(rt5671_machine->aud_osc_en)) {
			ret = gpio_request_one(rt5671_machine->aud_osc_en,
				GPIOF_DIR_OUT | GPIOF_INIT_LOW,
				"external osc enable");
			if (ret < 0) {
				pr_err("gpio_request_one failed. gpio:%d err:%d\n", rt5671_machine->aud_osc_en, ret);
				return ret;
			}

			odin_rt5671_set_aud_osc(rt5671_machine->aud_osc_en, EXT_OSC_ON);
		}
	}

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

#ifdef CONFIG_SWITCH
	if(rt5671_machine->aud_irq != NULL){
	/* Add h2w switch class support */
	ret = switch_dev_register(&wired_switch_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "not able to register switch device\n");
		goto switch_err;
		}
	}
#endif

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register soc_card:%d\n", ret);
		return ret;
	}

	snd_card_rev_class = class_create(THIS_MODULE, "sound_card");
	if (IS_ERR(snd_card_rev_class))
		dev_err(&pdev->dev, "Failed to create class(sound_card)\n");

	ret = class_create_file(snd_card_rev_class, &class_attr_snd_card_rev);
	if (ret)
		dev_err(&pdev->dev, "failed to create file for snd_card_rev\n");

	if (snd_dmaengine_pcm_report_pools_init())
		dev_warn(&pdev->dev, "failed to init dmaengine report pools\n");
	dev_info(&pdev->dev, "Audio Machine Probing Success!");

	return 0;

switch_err:
#ifdef CONFIG_SWITCH
	if(rt5671_machine->aud_irq != NULL){
	switch_dev_unregister(&wired_switch_dev);
	}
#endif

err:
	return ret;
}

static int odin_rt5671_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	class_remove_file(snd_card_rev_class,
			&class_attr_snd_card_rev);
	snd_dmaengine_pcm_report_pools_all_destory();
	snd_soc_unregister_card(card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id odin_rt5671_of_match[] = {
	{ .compatible = "lge,odin-audio", },
	{},
};
#endif

static void odin_rt5671_drv_shutdown(struct device *dev)
{
	struct snd_soc_card *soc_card = dev_get_drvdata(dev);
	struct snd_card *card = soc_card->snd_card;
	struct snd_pcm_substream *substream;
	struct snd_device *sdev;
	struct snd_pcm *pcm;
	int i;
	int ret;

	list_for_each_entry(sdev, &card->devices, list) {
		if (sdev->type == SNDRV_DEV_PCM) {
			pcm = (struct snd_pcm *)sdev->device_data;
			mutex_lock(&pcm->open_mutex);
			for (i = 0; i < 2; ++i) {
				substream = pcm->streams[i].substream;
				if (substream && SUBSTREAM_BUSY(substream))
					snd_pcm_release_substream(substream);
			}
			mutex_unlock(&pcm->open_mutex);
		}
	}

	ret = snd_card_disconnect(card);
	if (ret)
		dev_err(dev, "snd_card_disconnect fail %d\n", ret);

	dev_info(dev, "%s %s\n", __func__, soc_card->name);
	return;
}

static struct platform_driver snd_odin_driver = {
	.driver = {
		.name = "odin-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.shutdown = odin_rt5671_drv_shutdown,
#ifdef CONFIG_OF
		.of_match_table = odin_rt5671_of_match,
#endif
	},
	.probe = odin_rt5671_probe,
	.remove = odin_rt5671_remove,
};

module_platform_driver(snd_odin_driver);

MODULE_AUTHOR("Hyunsoo Yoon, hyunsoo.yoon@lge.com");
MODULE_DESCRIPTION("ALSA SoC ODIN RDK0(RevH(P3)/RevI(P2)) RT5671");
MODULE_LICENSE("GPL");
