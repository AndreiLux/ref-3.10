/*
 * tegra_rt5677.c - Tegra machine ASoC driver for boards using ALC5645 codec.
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#include <asm/mach-types.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/cpufreq.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/gpio-tegra.h>
#include <linux/sysedp.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/rt5677.h"
#include "../codecs/rt5506.h"
#include "../codecs/tfa9895.h"

#include "tegra_pcm.h"
#include "tegra30_ahub.h"
#include "tegra30_i2s.h"
#include "tegra30_dam.h"
#include "tegra_rt5677.h"

#define DRV_NAME "tegra-snd-rt5677"

#define DAI_LINK_HIFI			0
#define DAI_LINK_SPEAKER		1
#define DAI_LINK_BTSCO			2
#define DAI_LINK_MI2S_DUMMY		3
#define DAI_LINK_PCM_OFFLOAD_FE	4
#define DAI_LINK_COMPR_OFFLOAD_FE	5
#define DAI_LINK_I2S_OFFLOAD_BE	6
#define DAI_LINK_I2S_OFFLOAD_SPEAKER_BE	7
#define DAI_LINK_PCM_OFFLOAD_CAPTURE_FE	8
#define DAI_LINK_FAST_FE	9
#define NUM_DAI_LINKS		10

#define HOTWORD_CPU_FREQ_BOOST_MIN 2000000
#define HOTWORD_CPU_FREQ_BOOST_DURATION_MS 400

const char *tegra_rt5677_i2s_dai_name[TEGRA30_NR_I2S_IFC] = {
	"tegra30-i2s.0",
	"tegra30-i2s.1",
	"tegra30-i2s.2",
	"tegra30-i2s.3",
	"tegra30-i2s.4",
};

struct regulator *rt5677_reg;
static struct sysedp_consumer *sysedpc;

void __set_rt5677_power(struct tegra_rt5677 *machine, bool enable, bool hp_depop);
void set_rt5677_power_locked(struct tegra_rt5677 *machine, bool enable, bool hp_depop);

static int hotword_cpufreq_notifier(struct notifier_block* nb,
				    unsigned long event, void* data)
{
	struct cpufreq_policy *policy = data;

	if (policy == NULL || event != CPUFREQ_ADJUST)
		return 0;

	pr_debug("%s: adjusting cpu%d min freq to %d for hotword (currently "
		 "at %d)\n", __func__, policy->cpu, HOTWORD_CPU_FREQ_BOOST_MIN,
		 policy->cur);

	/* Make sure that the policy makes sense overall. */
	cpufreq_verify_within_limits(policy, HOTWORD_CPU_FREQ_BOOST_MIN,
				     policy->cpuinfo.max_freq);
	return 0;
}

static struct notifier_block hotword_cpufreq_notifier_block = {
	.notifier_call = hotword_cpufreq_notifier
};

static void tegra_do_hotword_work(struct work_struct *work)
{
	struct tegra_rt5677 *machine = container_of(work, struct tegra_rt5677, hotword_work);
	char *hot_event[] = { "ACTION=HOTWORD", NULL };
	int ret, i;

	/* Register a CPU policy change listener that will bump up the min
	 * frequency while we're processing the hotword. */
	ret = cpufreq_register_notifier(&hotword_cpufreq_notifier_block,
					CPUFREQ_POLICY_NOTIFIER);
	if (!ret) {
		for_each_online_cpu(i)
			cpufreq_update_policy(i);
	}

	kobject_uevent_env(&machine->pcard->dev->kobj, KOBJ_CHANGE, hot_event);

	/* If we registered the notifier, we can wait the specified duration
	 * before reseting the CPUs back to what they were. */
	if (!ret) {
		msleep(HOTWORD_CPU_FREQ_BOOST_DURATION_MS);
		cpufreq_unregister_notifier(&hotword_cpufreq_notifier_block,
					    CPUFREQ_POLICY_NOTIFIER);
		for_each_online_cpu(i)
			cpufreq_update_policy(i);
	}

	return;
}

static irqreturn_t detect_rt5677_irq_handler(int irq, void *dev_id)
{
	int value;
	struct tegra_rt5677 *machine = dev_id;

	value = gpio_get_value(machine->pdata->gpio_irq1);

	pr_info("RT5677 IRQ is triggered = 0x%x\n", value);
	if (value == 1) {
		schedule_work(&machine->hotword_work);
		wake_lock_timeout(&machine->vad_wake, msecs_to_jiffies(1500));
	}
	return IRQ_HANDLED;
}

static void tegra_rt5677_set_cif(int cif, unsigned int channels,
	unsigned int sample_size)
{
	tegra30_ahub_set_tx_cif_channels(cif, channels, channels);

	switch (sample_size) {
	case 8:
		tegra30_ahub_set_tx_cif_bits(cif,
		  TEGRA30_AUDIOCIF_BITS_8, TEGRA30_AUDIOCIF_BITS_8);
		tegra30_ahub_set_tx_fifo_pack_mode(cif,
		  TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_8_4);
		break;

	case 16:
		tegra30_ahub_set_tx_cif_bits(cif,
		  TEGRA30_AUDIOCIF_BITS_16, TEGRA30_AUDIOCIF_BITS_16);
		tegra30_ahub_set_tx_fifo_pack_mode(cif,
		  TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_16);
		break;

	case 24:
		tegra30_ahub_set_tx_cif_bits(cif,
		  TEGRA30_AUDIOCIF_BITS_24, TEGRA30_AUDIOCIF_BITS_24);
		tegra30_ahub_set_tx_fifo_pack_mode(cif, 0);
		break;

	case 32:
		tegra30_ahub_set_tx_cif_bits(cif,
		  TEGRA30_AUDIOCIF_BITS_32, TEGRA30_AUDIOCIF_BITS_32);
		tegra30_ahub_set_tx_fifo_pack_mode(cif, 0);
		break;

	default:
		pr_err("Error in sample_size\n");
		break;
	}
}


static int tegra_rt5677_fe_pcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra30_ahub_enable_clocks();
	tegra30_ahub_allocate_tx_fifo(
		&machine->playback_fifo_cif,
		&machine->playback_dma_data.addr,
		&machine->playback_dma_data.req_sel);

	machine->playback_dma_data.wrap = 4;
	machine->playback_dma_data.width = 32;
	cpu_dai->playback_dma_data = &machine->playback_dma_data;

	tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
		(machine->dam_ifc * 2), machine->playback_fifo_cif);

	return 0;
}

static void tegra_rt5677_fe_pcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
		(machine->dam_ifc * 2));
	tegra30_ahub_free_tx_fifo(machine->playback_fifo_cif);
	machine->playback_fifo_cif = -1;
	tegra30_ahub_disable_clocks();
}

static int tegra_rt5677_fe_pcm_trigger(struct snd_pcm_substream *substream,
			int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_ENABLE,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_ahub_enable_tx_fifo(machine->playback_fifo_cif);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		tegra30_ahub_disable_tx_fifo(machine->playback_fifo_cif);
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_DISABLE,
			TEGRA30_DAM_CHIN0_SRC);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_rt5677_fe_compr_ops_startup(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *fe = cstream->private_data;
	struct snd_soc_dai *cpu_dai = fe->cpu_dai;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(fe->card);

	tegra30_ahub_enable_clocks();
	tegra30_ahub_allocate_tx_fifo(
		&machine->playback_fifo_cif,
		&machine->playback_dma_data.addr,
		&machine->playback_dma_data.req_sel);

	machine->playback_dma_data.wrap = 4;
	machine->playback_dma_data.width = 32;
	cpu_dai->playback_dma_data = &machine->playback_dma_data;

	tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
		(machine->dam_ifc * 2), machine->playback_fifo_cif);

	return 0;
}

static void tegra_rt5677_fe_compr_ops_shutdown(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *fe = cstream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(fe->card);

	tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
		(machine->dam_ifc * 2));
	tegra30_ahub_free_tx_fifo(machine->playback_fifo_cif);
	machine->playback_fifo_cif = -1;
	tegra30_ahub_disable_clocks();
}

static int tegra_rt5677_fe_compr_ops_trigger(struct snd_compr_stream *cstream,
			int cmd)
{
	struct snd_soc_pcm_runtime *fe = cstream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(fe->card);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_ENABLE,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_ahub_enable_tx_fifo(machine->playback_fifo_cif);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		tegra30_ahub_disable_tx_fifo(machine->playback_fifo_cif);
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_DISABLE,
			TEGRA30_DAM_CHIN0_SRC);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_rt5677_fe_fast_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra30_ahub_enable_clocks();
	tegra30_ahub_allocate_tx_fifo(
		&machine->playback_fast_fifo_cif,
		&machine->playback_fast_dma_data.addr,
		&machine->playback_fast_dma_data.req_sel);

	machine->playback_fast_dma_data.width = 32;
	machine->playback_fast_dma_data.wrap = 4;
	cpu_dai->playback_dma_data = &machine->playback_fast_dma_data;

	tegra30_ahub_set_rx_cif_source(
			TEGRA30_AHUB_RXCIF_DAM0_RX1 + (machine->dam_ifc * 2),
			machine->playback_fast_fifo_cif);

	return 0;
}

static void tegra_rt5677_fe_fast_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX1 +
			(machine->dam_ifc * 2));
	tegra30_ahub_free_tx_fifo(machine->playback_fast_fifo_cif);
	machine->playback_fast_fifo_cif = -1;
	tegra30_ahub_disable_clocks();
}

static int tegra_rt5677_fe_fast_trigger(struct snd_pcm_substream *substream,
			int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		tegra30_dam_ch0_set_datasync(machine->dam_ifc, 2);
		tegra30_dam_ch1_set_datasync(machine->dam_ifc, 0);
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_ENABLE,
				TEGRA30_DAM_CHIN1);
		tegra30_ahub_enable_tx_fifo(machine->playback_fast_fifo_cif);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		tegra30_ahub_disable_tx_fifo(machine->playback_fast_fifo_cif);
		tegra30_dam_enable(machine->dam_ifc, TEGRA30_DAM_DISABLE,
				TEGRA30_DAM_CHIN1);
		tegra30_dam_ch0_set_datasync(machine->dam_ifc, 1);
		tegra30_dam_ch1_set_datasync(machine->dam_ifc, 0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_rt5677_spk_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_card *card = rtd->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	int dam_ifc = machine->dam_ifc;
	pr_info("%s:mi2s amp on\n",__func__);

	tegra_asoc_utils_tristate_pd_dap(i2s->id, false);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rtd->dai_link->be_id == DAI_LINK_I2S_OFFLOAD_SPEAKER_BE) {
			mutex_lock(&machine->dam_mutex);
			if (machine->dam_ref_cnt == 0) {
				tegra30_dam_soft_reset(dam_ifc);
				tegra30_dam_allocate_channel(dam_ifc,
						TEGRA30_DAM_CHIN0_SRC);
				tegra30_dam_allocate_channel(dam_ifc,
						TEGRA30_DAM_CHIN1);
				tegra30_dam_enable_clock(dam_ifc);

				tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHOUT,
					48000);
				tegra30_dam_set_samplerate(dam_ifc,
					TEGRA30_DAM_CHIN0_SRC, 48000);
				tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
					0x1000);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
					2, 16, 2, 32);
				tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN1,
					0x1000);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN1,
					2, 16, 2, 32);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHOUT,
					2, 16, 2, 32);
				tegra30_dam_enable_stereo_mixing(machine->dam_ifc, 1);
				tegra30_dam_ch0_set_datasync(dam_ifc, 0);
				tegra30_dam_ch1_set_datasync(dam_ifc, 0);
			}
			tegra30_ahub_set_rx_cif_source(i2s->playback_i2s_cif,
					TEGRA30_AHUB_TXCIF_DAM0_TX0 + machine->dam_ifc);
			machine->dam_ref_cnt++;
			mutex_unlock(&machine->dam_mutex);
		} else {
			tegra30_ahub_allocate_tx_fifo(
					&i2s->playback_fifo_cif,
					&i2s->playback_dma_data.addr,
					&i2s->playback_dma_data.req_sel);
			i2s->playback_dma_data.wrap = 4;
			i2s->playback_dma_data.width = 32;
			cpu_dai->playback_dma_data = &i2s->playback_dma_data;
			tegra30_ahub_set_rx_cif_source(
				i2s->playback_i2s_cif,
				i2s->playback_fifo_cif);
			}
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mutex_lock(&machine->spk_amp_lock);
		set_tfa9895_spkamp(1, 0);
		set_tfa9895l_spkamp(1, 0);
		mutex_unlock(&machine->spk_amp_lock);
	}

	return 0;
}

static void tegra_rt5677_spk_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_soc_card *card = rtd->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	pr_info("%s:mi2s amp off\n",__func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rtd->dai_link->be_id == DAI_LINK_I2S_OFFLOAD_SPEAKER_BE) {
			mutex_lock(&machine->dam_mutex);
			tegra30_ahub_unset_rx_cif_source(i2s->playback_i2s_cif);
			machine->dam_ref_cnt--;
			if (machine->dam_ref_cnt == 0)
				tegra30_dam_disable_clock(machine->dam_ifc);
			mutex_unlock(&machine->dam_mutex);
		} else {
			tegra30_ahub_unset_rx_cif_source(i2s->playback_i2s_cif);
			tegra30_ahub_free_tx_fifo(i2s->playback_fifo_cif);
			i2s->playback_fifo_cif = -1;
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mutex_lock(&machine->spk_amp_lock);
		set_tfa9895_spkamp(0, 0);
		set_tfa9895l_spkamp(0, 0);
		mutex_unlock(&machine->spk_amp_lock);
	}
	tegra_asoc_utils_tristate_pd_dap(i2s->id, true);
}

static int tegra_rt5677_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	struct snd_soc_card *card = rtd->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int dam_ifc = machine->dam_ifc;

	pr_debug("%s i2s->id=%d %d\n", __func__, i2s->id,
			pdata->i2s_param[HIFI_CODEC].audio_port_id);
	tegra_asoc_utils_tristate_pd_dap(i2s->id, false);
	if (i2s->id == pdata->i2s_param[HIFI_CODEC].audio_port_id) {
		cancel_delayed_work_sync(&machine->power_work);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			set_rt5677_power_locked(machine, true, true);
		else
			set_rt5677_power_locked(machine, true, false);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rtd->dai_link->be_id == DAI_LINK_I2S_OFFLOAD_BE) {
			mutex_lock(&machine->dam_mutex);
			if (machine->dam_ref_cnt == 0) {
				tegra30_dam_soft_reset(dam_ifc);
				tegra30_dam_allocate_channel(dam_ifc,
						TEGRA30_DAM_CHIN0_SRC);
				tegra30_dam_allocate_channel(dam_ifc,
						TEGRA30_DAM_CHIN1);
				tegra30_dam_enable_clock(dam_ifc);

				tegra30_dam_set_samplerate(dam_ifc, TEGRA30_DAM_CHOUT,
					48000);
				tegra30_dam_set_samplerate(dam_ifc,
					TEGRA30_DAM_CHIN0_SRC, 48000);
				tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
					0x1000);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN0_SRC,
					2, 16, 2, 32);
				tegra30_dam_set_gain(dam_ifc, TEGRA30_DAM_CHIN1,
					0x1000);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHIN1,
					2, 16, 2, 32);
				tegra30_dam_set_acif(dam_ifc, TEGRA30_DAM_CHOUT,
					2, 16, 2, 32);
				tegra30_dam_enable_stereo_mixing(machine->dam_ifc, 1);
				tegra30_dam_ch0_set_datasync(dam_ifc, 0);
				tegra30_dam_ch1_set_datasync(dam_ifc, 0);
			}
			tegra30_ahub_set_rx_cif_source(i2s->playback_i2s_cif,
					TEGRA30_AHUB_TXCIF_DAM0_TX0 + machine->dam_ifc);
			machine->dam_ref_cnt++;
			mutex_unlock(&machine->dam_mutex);
		} else {
			tegra30_ahub_allocate_tx_fifo(
					&i2s->playback_fifo_cif,
					&i2s->playback_dma_data.addr,
					&i2s->playback_dma_data.req_sel);
			i2s->playback_dma_data.wrap = 4;
			i2s->playback_dma_data.width = 32;
			cpu_dai->playback_dma_data = &i2s->playback_dma_data;
			tegra30_ahub_set_rx_cif_source(
				i2s->playback_i2s_cif,
				i2s->playback_fifo_cif);
		}
	}
	return 0;
}

static void tegra_rt5677_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rtd->dai_link->be_id == DAI_LINK_I2S_OFFLOAD_BE) {
			mutex_lock(&machine->dam_mutex);
			tegra30_ahub_unset_rx_cif_source(i2s->playback_i2s_cif);
			machine->dam_ref_cnt--;
			if (machine->dam_ref_cnt == 0)
				tegra30_dam_disable_clock(machine->dam_ifc);
			mutex_unlock(&machine->dam_mutex);
		} else {
			tegra30_ahub_unset_rx_cif_source(i2s->playback_i2s_cif);
			tegra30_ahub_free_tx_fifo(i2s->playback_fifo_cif);
			i2s->playback_fifo_cif = -1;
		}
	}
	tegra_asoc_utils_tristate_pd_dap(i2s->id, true);

	if (machine->codec && machine->codec->active)
		return;

	schedule_delayed_work(&machine->power_work,
			msecs_to_jiffies(500));
}

static int tegra_rt5677_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int srate, mclk, i2s_daifmt, codec_daifmt;
	int err, rate, sample_size;
	unsigned int i2sclock;

	srate = params_rate(params);
	mclk = 256 * srate;

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[HIFI_CODEC].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sample_size = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		sample_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_size = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (pdata->i2s_param[HIFI_CODEC].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		if (!(machine->util_data.set_mclk % mclk)) {
			mclk = machine->util_data.set_mclk;
		} else {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 1);

	rate = clk_get_rate(machine->util_data.clk_cdev1);

	if (pdata->i2s_param[HIFI_CODEC].is_i2s_master) {
		err = snd_soc_dai_set_sysclk(codec_dai, RT5677_SCLK_S_MCLK,
				rate, SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "codec_dai clock not set\n");
			return err;
		}
	} else {

		err = snd_soc_dai_set_pll(codec_dai, 0, RT5677_PLL1_S_MCLK,
				rate, 512*srate);
		if (err < 0) {
			dev_err(card->dev, "codec_dai pll not set\n");
			return err;
		}
		err = snd_soc_dai_set_sysclk(codec_dai, RT5677_SCLK_S_PLL1,
				512*srate, SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "codec_dai clock not set\n");
			return err;
		}
	}

	/* Use 64Fs */
	i2sclock = srate * 2 * 32;

	err = snd_soc_dai_set_sysclk(cpu_dai, 0,
			i2sclock, SND_SOC_CLOCK_OUT);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai clock not set\n");
		return err;
	}

	codec_daifmt = i2s_daifmt;

	/*invert the codec bclk polarity when codec is master
	in DSP mode this is done to match with the negative
	edge settings of tegra i2s*/
	if (((i2s_daifmt & SND_SOC_DAIFMT_FORMAT_MASK)
		== SND_SOC_DAIFMT_DSP_A) &&
		((i2s_daifmt & SND_SOC_DAIFMT_MASTER_MASK)
		== SND_SOC_DAIFMT_CBM_CFM)) {
		codec_daifmt &= ~(SND_SOC_DAIFMT_INV_MASK);
		codec_daifmt |= SND_SOC_DAIFMT_IB_NF;
	}

	err = snd_soc_dai_set_fmt(codec_dai, codec_daifmt);
	if (err < 0) {
		dev_err(card->dev, "codec_dai fmt not set\n");
		return err;
	}

	err = snd_soc_dai_set_fmt(cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if ((int)machine->playback_fifo_cif >= 0)
			tegra_rt5677_set_cif(machine->playback_fifo_cif,
				params_channels(params), sample_size);

		if ((int)machine->playback_fast_fifo_cif >= 0)
			tegra_rt5677_set_cif(machine->playback_fast_fifo_cif,
				params_channels(params), sample_size);
	}

	return 0;
}

static int tegra_speaker_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int i2s_daifmt;
	int err, sample_size;

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[SPEAKER].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[SPEAKER].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	err = snd_soc_dai_set_fmt(rtd->cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sample_size = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		sample_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_size = 32;
		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if ((int)machine->playback_fifo_cif >= 0)
			tegra_rt5677_set_cif(machine->playback_fifo_cif,
				params_channels(params), sample_size);

		if ((int)machine->playback_fast_fifo_cif >= 0)
			tegra_rt5677_set_cif(machine->playback_fast_fifo_cif,
				params_channels(params), sample_size);
	}

	return 0;
}

static int tegra_bt_sco_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int i2s_daifmt;
	int err;

	i2s_daifmt = SND_SOC_DAIFMT_NB_NF;
	i2s_daifmt |= pdata->i2s_param[BT_SCO].is_i2s_master ?
			SND_SOC_DAIFMT_CBS_CFS : SND_SOC_DAIFMT_CBM_CFM;

	switch (pdata->i2s_param[BT_SCO].i2s_mode) {
	case TEGRA_DAIFMT_I2S:
		i2s_daifmt |= SND_SOC_DAIFMT_I2S;
		break;
	case TEGRA_DAIFMT_DSP_A:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_A;
		break;
	case TEGRA_DAIFMT_DSP_B:
		i2s_daifmt |= SND_SOC_DAIFMT_DSP_B;
		break;
	case TEGRA_DAIFMT_LEFT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_LEFT_J;
		break;
	case TEGRA_DAIFMT_RIGHT_J:
		i2s_daifmt |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	default:
		dev_err(card->dev, "Can't configure i2s format\n");
		return -EINVAL;
	}

	err = snd_soc_dai_set_fmt(rtd->cpu_dai, i2s_daifmt);
	if (err < 0) {
		dev_err(card->dev, "cpu_dai fmt not set\n");
		return err;
	}

	return 0;
}

static int tegra_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_lock_clk_rate(&machine->util_data, 0);

	return 0;
}

static struct snd_soc_ops tegra_rt5677_ops = {
	.hw_params = tegra_rt5677_hw_params,
	.hw_free = tegra_hw_free,
	.startup = tegra_rt5677_startup,
	.shutdown = tegra_rt5677_shutdown,
};

static struct snd_soc_ops tegra_rt5677_fe_pcm_ops = {
	.startup = tegra_rt5677_fe_pcm_startup,
	.shutdown = tegra_rt5677_fe_pcm_shutdown,
	.trigger = tegra_rt5677_fe_pcm_trigger,
};

static struct snd_soc_compr_ops tegra_rt5677_fe_compr_ops = {
	.startup = tegra_rt5677_fe_compr_ops_startup,
	.shutdown = tegra_rt5677_fe_compr_ops_shutdown,
	.trigger = tegra_rt5677_fe_compr_ops_trigger,
};

static struct snd_soc_ops tegra_rt5677_fe_fast_ops = {
	.startup = tegra_rt5677_fe_fast_startup,
	.shutdown = tegra_rt5677_fe_fast_shutdown,
	.trigger = tegra_rt5677_fe_fast_trigger,
};

static struct snd_soc_ops tegra_rt5677_speaker_ops = {
	.hw_params = tegra_speaker_hw_params,
	.startup = tegra_rt5677_spk_startup,
	.shutdown = tegra_rt5677_spk_shutdown,
};

static struct snd_soc_ops tegra_rt5677_bt_sco_ops = {
	.hw_params = tegra_bt_sco_hw_params,
	.startup = tegra_rt5677_startup,
	.shutdown = tegra_rt5677_shutdown,
};

#define HTC_SOC_ENUM_EXT(xname, xhandler_get, xhandler_put) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_volsw_ext, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = 255 }

static int tegra_rt5677_rt5506_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rt5506_get_gain();
	return 0;
}

static int tegra_rt5677_rt5506_gain_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: 0x%x\n", __func__,
		(unsigned int)ucontrol->value.integer.value[0]);
	rt5506_set_gain((unsigned char)ucontrol->value.integer.value[0]);
	return 0;
}

/* speaker single channel */
enum speaker_state {
	BIT_LR_CH = 0,
	BIT_LEFT_CH = (1 << 0),
	BIT_RIGHT_CH = (1 << 1),
};

static const char * const tegra_rt5677_speaker_test_mode[] = {
	"LR", "Left", "Right",
};

static int speaker_test_mode;

static const SOC_ENUM_SINGLE_DECL(tegra_rt5677_speaker_test_mode_enum, 0, 0,
	tegra_rt5677_speaker_test_mode);

static int tegra_rt5677_speaker_test_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = speaker_test_mode;
	return 0;
}

static int tegra_rt5677_speaker_test_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	enum speaker_state state = BIT_LR_CH;

	if (ucontrol->value.integer.value[0] == 1) {
		state = BIT_LEFT_CH;
		tfa9895_disable(1);
		tfa9895l_disable(0);
	} else if (ucontrol->value.integer.value[0] == 2) {
		state = BIT_RIGHT_CH;
		tfa9895_disable(0);
		tfa9895l_disable(1);
	} else {
		state = BIT_LR_CH;
		tfa9895_disable(0);
		tfa9895l_disable(0);
	}

	speaker_test_mode = state;

	pr_info("%s: tegra_rt5677_speaker_test_dev set to %d done\n",
		__func__, state);

	return 0;
}

/* digital mic bias */
enum dmic_bias_state {
	BIT_DMIC_BIAS_DISABLE = 0,
	BIT_DMIC_BIAS_ENABLE = (1 << 0),
};

static const char * const tegra_rt5677_dmic_mode[] = {
	"disable", "enable"
};

static int dmic_mode;

static const SOC_ENUM_SINGLE_DECL(tegra_rt5677_dmic_mode_enum, 0, 0,
	tegra_rt5677_dmic_mode);

static int tegra_rt5677_dmic_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = dmic_mode;
	return 0;
}

static int tegra_rt5677_dmic_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	enum dmic_bias_state state = BIT_DMIC_BIAS_DISABLE;

	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int ret = 0;

	pr_info("tegra_rt5677_dmic_set, %d\n", pdata->gpio_int_mic_en);

	if (ucontrol->value.integer.value[0] == 0) {
		state = BIT_DMIC_BIAS_DISABLE;
		ret = gpio_direction_output(pdata->gpio_int_mic_en, 0);
		if (ret)
			pr_err("gpio_int_mic_en=0 fail,%d\n", ret);
		else
			pr_info("gpio_int_mic_en=0\n");
	} else {
		state = BIT_DMIC_BIAS_ENABLE;
		ret = gpio_direction_output(pdata->gpio_int_mic_en, 1);
		if (ret)
			pr_err("gpio_int_mic_en=1 fail,%d\n", ret);
		else
			pr_info("gpio_int_mic_en=1\n");
	}

	dmic_mode = state;

	pr_info("%s: tegra_rt5677_dmic_set set to %d done\n",
		__func__, state);
	return 0;
}

/* headset mic bias */
enum amic_bias_state {
	BIT_AMIC_BIAS_DISABLE = 0,
	BIT_AMIC_BIAS_ENABLE = (1 << 0),
};

static const char * const tegra_rt5677_amic_test_mode[] = {
	"disable", "enable"
};

static int amic_test_mode;

static const SOC_ENUM_SINGLE_DECL(tegra_rt5677_amic_test_mode_enum, 0, 0,
	tegra_rt5677_amic_test_mode);

static int tegra_rt5677_amic_test_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = amic_test_mode;
	return 0;
}

static int tegra_rt5677_amic_test_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	enum amic_bias_state state = BIT_AMIC_BIAS_DISABLE;

	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int ret = 0;

	pr_info("tegra_rt5677_amic_test_set, %d\n", pdata->gpio_ext_mic_en);

	if (gpio_is_valid(pdata->gpio_ext_mic_en)) {
		pr_info("gpio_ext_mic_en %d is valid\n",
			pdata->gpio_ext_mic_en);
		ret = gpio_request(pdata->gpio_ext_mic_en, "ext-mic-enable");
		if (ret) {
			pr_err("Fail gpio_request gpio_ext_mic_en, %d\n",
				ret);
			return ret;
		}
	} else {
		pr_err("gpio_ext_mic_en %d is invalid\n",
			pdata->gpio_ext_mic_en);
		return -1;
	}


	if (ucontrol->value.integer.value[0] == 0) {
		state = BIT_AMIC_BIAS_DISABLE;
		ret = gpio_direction_output(pdata->gpio_ext_mic_en, 0);
		if (ret)
			pr_err("gpio_ext_mic_en=0 fail,%d\n", ret);
		else
			pr_info("gpio_ext_mic_en=0\n");
	} else {
		state = BIT_AMIC_BIAS_ENABLE;
		ret = gpio_direction_output(pdata->gpio_ext_mic_en, 1);
		if (ret)
			pr_err("gpio_ext_mic_en=1 fail,%d\n", ret);
		else
			pr_info("gpio_ext_mic_en=1\n");
	}

	gpio_free(pdata->gpio_ext_mic_en);

	amic_test_mode = state;

	pr_info("%s: tegra_rt5677_amic_test_dev set to %d done\n",
		__func__, state);

	return 0;
}

/* rt5506 dump register */
enum rt5506_dump_state {
	BIT_RT5506_DUMP_DISABLE = 0,
	BIT_RT5506_DUMP_ENABLE = (1 << 0),
};

static const char * const tegra_rt5677_rt5506_dump_mode[] = {
	"disable", "enable"
};

static int rt5506_dump_mode;

static const SOC_ENUM_SINGLE_DECL(tegra_rt5677_rt5506_dump_enum, 0, 0,
	tegra_rt5677_rt5506_dump_mode);

static int tegra_rt5677_rt5506_dump_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rt5506_dump_mode;
	return 0;
}

static int tegra_rt5677_rt5506_dump_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	enum rt5506_dump_state state = BIT_RT5506_DUMP_DISABLE;

	int ret = 0;

	if (ucontrol->value.integer.value[0] == 0) {
		state = BIT_RT5506_DUMP_DISABLE;
	} else {
		state = BIT_RT5506_DUMP_ENABLE;
		pr_info("tegra_rt5677_rt5506_dump_set, rt5506_dump_reg()\n");
		rt5506_dump_reg();
	}

	rt5506_dump_mode = state;

	pr_info("%s: tegra_rt5677_rt5506_dump_dev set to %d done\n",
		__func__, state);

	return ret;
}

static int tegra_rt5677_event_headphone_jack(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_dbg(card->dev, "tegra_rt5677_event_headphone_jack (%d)\n",
		event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* set hp_en low and usleep 10 ms for charging */
		set_rt5506_hp_en(0);
		usleep_range(10000,10000);
		dev_dbg(card->dev, "%s: set_rt5506_amp(1,0)\n", __func__);
		/*msleep(900); depop*/
		set_rt5506_amp(1, 0);
	break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(card->dev, "%s: set_rt5506_amp(0,0)\n", __func__);
		set_rt5506_amp(0, 0);
	break;

	default:
	return 0;
	}

	return 0;
}

static int tegra_rt5677_event_mic_jack(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_dbg(card->dev, "tegra_rt5677_event_mic_jack (%d)\n",
		event);

	return 0;
}

static int tegra_rt5677_event_int_mic(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *k, int event)
{
	return 0;
}

static const struct snd_soc_dapm_widget flounder_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", tegra_rt5677_event_headphone_jack),
	SND_SOC_DAPM_MIC("Mic Jack", tegra_rt5677_event_mic_jack),
	SND_SOC_DAPM_MIC("Int Mic", tegra_rt5677_event_int_mic),
};

static const struct snd_soc_dapm_route flounder_audio_map[] = {
	{"Headphone Jack", NULL, "LOUT1"},
	{"Headphone Jack", NULL, "LOUT2"},
	{"IN2P", NULL, "Mic Jack"},
	{"IN2N", NULL, "Mic Jack"},
	{"DMIC L1", NULL, "Int Mic"},
	{"DMIC R1", NULL, "Int Mic"},
	{"DMIC L2", NULL, "Int Mic"},
	{"DMIC R2", NULL, "Int Mic"},
	/* AHUB BE connections */
	{"DAM VMixer", NULL, "fast-pcm-playback"},
	{"DAM VMixer", NULL, "offload-compr-playback"},
	{"AIF1 Playback", NULL, "I2S1_OUT"},
	{"Playback", NULL, "I2S2_OUT"},
};

static const struct snd_kcontrol_new flounder_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
	SOC_DAPM_PIN_SWITCH("Mic Jack"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),

	HTC_SOC_ENUM_EXT("Headset rt5506 Volume",
		tegra_rt5677_rt5506_gain_get, tegra_rt5677_rt5506_gain_set),

	SOC_ENUM_EXT("Speaker Channel Switch",
		tegra_rt5677_speaker_test_mode_enum,
		tegra_rt5677_speaker_test_get, tegra_rt5677_speaker_test_set),

	SOC_ENUM_EXT("AMIC Test Switch", tegra_rt5677_amic_test_mode_enum,
		tegra_rt5677_amic_test_get, tegra_rt5677_amic_test_set),

	SOC_ENUM_EXT("DMIC BIAS Switch", tegra_rt5677_dmic_mode_enum,
		tegra_rt5677_dmic_get, tegra_rt5677_dmic_set),

	SOC_ENUM_EXT("RT5506 Dump Register", tegra_rt5677_rt5506_dump_enum,
		tegra_rt5677_rt5506_dump_get, tegra_rt5677_rt5506_dump_set),
};

static int tegra_rt5677_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = codec->card;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	int ret;

	ret = tegra_asoc_utils_register_ctls(&machine->util_data);
	if (ret < 0)
		return ret;

	/* FIXME: Calculate automatically based on DAPM routes? */
	snd_soc_dapm_nc_pin(dapm, "LOUT1");
	snd_soc_dapm_nc_pin(dapm, "LOUT2");
	snd_soc_dapm_sync(dapm);
	machine->codec = codec;

	return 0;
}

static int tegra_rt5677_be_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	i2s->allocate_pb_fifo_cif = false;

	return 0;
}

static int tegra_offload_hw_params_be_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *snd_rate = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *snd_channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	snd_rate->min = snd_rate->max = 48000;
	snd_channels->min = snd_channels->max = 2;

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				SNDRV_PCM_HW_PARAM_FIRST_MASK],
				SNDRV_PCM_FORMAT_S16_LE);

	pr_debug("%s::%d %d %d\n", __func__, params_rate(params),
			params_channels(params), params_format(params));
	return 0;
}

static struct snd_soc_dai_link tegra_rt5677_dai[NUM_DAI_LINKS] = {
	[DAI_LINK_HIFI] = {
		.name = "rt5677",
		.stream_name = "rt5677 PCM",
		.codec_name = "rt5677.1-002d",
		.platform_name = "tegra30-i2s.1",
		.cpu_dai_name = "tegra30-i2s.1",
		.codec_dai_name = "rt5677-aif1",
		.init = tegra_rt5677_init,
		.ops = &tegra_rt5677_ops,
	},

	[DAI_LINK_SPEAKER] = {
		.name = "SPEAKER",
		.stream_name = "SPEAKER PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra30-i2s.2",
		.cpu_dai_name = "tegra30-i2s.2",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_rt5677_speaker_ops,
	},

	[DAI_LINK_BTSCO] = {
		.name = "BT-SCO",
		.stream_name = "BT SCO PCM",
		.codec_name = "spdif-dit.1",
		.platform_name = "tegra30-i2s.3",
		.cpu_dai_name = "tegra30-i2s.3",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_rt5677_bt_sco_ops,
	},

	[DAI_LINK_MI2S_DUMMY] = {
		.name = "MI2S DUMMY",
		.stream_name = "MI2S DUMMY PCM",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra30-i2s.2",
		.cpu_dai_name = "tegra30-i2s.2",
		.codec_dai_name = "dit-hifi",
		.ops = &tegra_rt5677_speaker_ops,
	},
	[DAI_LINK_PCM_OFFLOAD_FE] = {
		.name = "fe-offload-pcm",
		.stream_name = "offload-pcm",

		.platform_name = "tegra-offload",
		.cpu_dai_name = "tegra-offload-pcm",

		.codec_dai_name =  "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &tegra_rt5677_fe_pcm_ops,
		.dynamic = 1,
	},
	[DAI_LINK_COMPR_OFFLOAD_FE] = {
		.name = "fe-offload-compr",
		.stream_name = "offload-compr",

		.platform_name = "tegra-offload",
		.cpu_dai_name = "tegra-offload-compr",

		.codec_dai_name =  "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.compr_ops = &tegra_rt5677_fe_compr_ops,
		.dynamic = 1,
	},
	[DAI_LINK_PCM_OFFLOAD_CAPTURE_FE] = {
		.name = "fe-offload-pcm-capture",
		.stream_name = "offload-pcm-capture",

		.platform_name = "tegra-offload",
		.cpu_dai_name = "tegra-offload-pcm",

		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",

	},
	[DAI_LINK_FAST_FE] = {
		.name = "fe-fast-pcm",
		.stream_name = "fast-pcm",
		.platform_name = "tegra-pcm-audio",
		.cpu_dai_name = "tegra-fast-pcm",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ops = &tegra_rt5677_fe_fast_ops,
		.dynamic = 1,
	},
	[DAI_LINK_I2S_OFFLOAD_BE] = {
		.name = "be-offload-audio-codec",
		.stream_name = "offload-audio-pcm",
		.codec_name = "rt5677.1-002d",
		.platform_name = "tegra30-i2s.1",
		.cpu_dai_name = "tegra30-i2s.1",
		.codec_dai_name = "rt5677-aif1",
		.init = tegra_rt5677_be_init,
		.ops = &tegra_rt5677_ops,

		.no_pcm = 1,

		.be_id = DAI_LINK_I2S_OFFLOAD_BE,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = tegra_offload_hw_params_be_fixup,
	},
	[DAI_LINK_I2S_OFFLOAD_SPEAKER_BE] = {
		.name = "be-offload-audio-speaker",
		.stream_name = "offload-audio-pcm-spk",
		.codec_name = "spdif-dit.0",
		.platform_name = "tegra30-i2s.2",
		.cpu_dai_name = "tegra30-i2s.2",
		.codec_dai_name = "dit-hifi",
		.init = tegra_rt5677_be_init,
		.ops = &tegra_rt5677_speaker_ops,

		.no_pcm = 1,

		.be_id = DAI_LINK_I2S_OFFLOAD_SPEAKER_BE,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = tegra_offload_hw_params_be_fixup,
	},
};

void mclk_enable(struct tegra_rt5677 *machine, bool on)
{
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int ret;
	if (on && !machine->clock_enabled) {
		gpio_free(pdata->codec_mclk.id);
		pr_debug("%s: gpio_free for gpio[%d] %s\n",
				__func__, pdata->codec_mclk.id, pdata->codec_mclk.name);
		machine->clock_enabled = 1;
		tegra_asoc_utils_clk_enable(&machine->util_data);
	} else if (!on && machine->clock_enabled){
		machine->clock_enabled = 0;
		tegra_asoc_utils_clk_disable(&machine->util_data);
		ret = gpio_request(pdata->codec_mclk.id,
					 pdata->codec_mclk.name);
		if (ret) {
			pr_err("Fail gpio_request codec_mclk, %d\n",
				ret);
			return;
		}
		gpio_direction_output(pdata->codec_mclk.id, 0);
		pr_debug("%s: gpio_request for gpio[%d] %s, return %d\n",
				__func__, pdata->codec_mclk.id, pdata->codec_mclk.name, ret);
	}
}

static int tegra_rt5677_suspend_post(struct snd_soc_card *card)
{
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	int i, suspend_allowed = 1;

	/*In Voice Call we ignore suspend..so check for that*/
	for (i = 0; i < machine->pcard->num_links; i++) {
		if (machine->pcard->dai_link[i].ignore_suspend) {
			suspend_allowed = 0;
			break;
		}
	}

	if (suspend_allowed) {
		/*This may be required if dapm setbias level is not called in
		some cases, may be due to a wrong dapm map*/
		mutex_lock(&machine->rt5677_lock);
		if (machine->clock_enabled) {
			mclk_enable(machine, 0);
		}
		mutex_unlock(&machine->rt5677_lock);
		/*TODO: Disable Audio Regulators*/
	}

	return 0;
}

static int tegra_rt5677_resume_pre(struct snd_soc_card *card)
{
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	int i, suspend_allowed = 1;
	/*In Voice Call we ignore suspend..so check for that*/
	for (i = 0; i < machine->pcard->num_links; i++) {
		if (machine->pcard->dai_link[i].ignore_suspend) {
			suspend_allowed = 0;
			break;
		}
	}

	if (suspend_allowed) {
		/*This may be required if dapm setbias level is not called in
		some cases, may be due to a wrong dapm map*/
		mutex_lock(&machine->rt5677_lock);
		if (!machine->clock_enabled &&
				machine->bias_level != SND_SOC_BIAS_OFF) {
			mclk_enable(machine, 1);
			tegra_asoc_utils_clk_enable(&machine->util_data);
			__set_rt5677_power(machine, true, true);
		}
		mutex_unlock(&machine->rt5677_lock);
		/*TODO: Enable Audio Regulators*/
	}

	return 0;
}

static int tegra_rt5677_set_bias_level(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);

	cancel_delayed_work_sync(&machine->power_work);
	mutex_lock(&machine->rt5677_lock);
	if (machine->bias_level == SND_SOC_BIAS_OFF &&
		level != SND_SOC_BIAS_OFF && (!machine->clock_enabled)) {
		mclk_enable(machine, 1);
		machine->bias_level = level;
		__set_rt5677_power(machine, true, false);
	}
	mutex_unlock(&machine->rt5677_lock);

	return 0;
}

static int tegra_rt5677_set_bias_level_post(struct snd_soc_card *card,
	struct snd_soc_dapm_context *dapm, enum snd_soc_bias_level level)
{
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_codec *codec = NULL;
	struct rt5677_priv *rt5677 = NULL;
	int i = 0;

	if (machine->codec) {
		codec = machine->codec;
		rt5677 = snd_soc_codec_get_drvdata(codec);
	}

	mutex_lock(&machine->rt5677_lock);

	for (i = 0; i < card->num_rtd; i++) {
		codec = card->rtd[i].codec;
		if (codec && codec->active)
			goto exit;
	}

	machine->bias_level = level;
exit:
	mutex_unlock(&machine->rt5677_lock);

	return 0;
}

static struct snd_soc_card snd_soc_tegra_rt5677 = {
	.name = "tegra-rt5677",
	.owner = THIS_MODULE,
	.dai_link = tegra_rt5677_dai,
	.num_links = ARRAY_SIZE(tegra_rt5677_dai),
	.suspend_post = tegra_rt5677_suspend_post,
	.resume_pre = tegra_rt5677_resume_pre,
	.set_bias_level = tegra_rt5677_set_bias_level,
	.set_bias_level_post = tegra_rt5677_set_bias_level_post,
	.controls = flounder_controls,
	.num_controls = ARRAY_SIZE(flounder_controls),
	.dapm_widgets = flounder_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(flounder_dapm_widgets),
	.dapm_routes = flounder_audio_map,
	.num_dapm_routes = ARRAY_SIZE(flounder_audio_map),
	.fully_routed = true,
};

void __set_rt5677_power(struct tegra_rt5677 *machine, bool enable, bool hp_depop)
{
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	int ret = 0;
	static bool status = false;

	if (enable && status == false) {
		/* set hp_en high to depop for headset path */
		if (hp_depop)
			set_rt5506_hp_en(1);
		pr_info("tegra_rt5677 power_on\n");
		if (!machine->clock_enabled) {
			pr_debug("%s: call mclk_enable(true)\n", __func__);
			mclk_enable(machine, 1);
		}
		/*V_IO_1V8*/
		if (gpio_is_valid(pdata->gpio_ldo1_en)) {
			pr_debug("gpio_ldo1_en %d is valid\n", pdata->gpio_ldo1_en);
			ret = gpio_request(pdata->gpio_ldo1_en, "rt5677-ldo-enable");
			if (ret) {
				pr_err("Fail gpio_request gpio_ldo1_en, %d\n", ret);
			} else {
				ret = gpio_direction_output(pdata->gpio_ldo1_en, 1);
				if (ret) {
					pr_err("gpio_ldo1_en=1 fail,%d\n", ret);
					gpio_free(pdata->gpio_ldo1_en);
				} else
					pr_debug("gpio_ldo1_en=1\n");
			}
		} else {
			pr_err("gpio_ldo1_en %d is invalid\n", pdata->gpio_ldo1_en);
		}

		usleep_range(1000, 2000);

		/*V_AUD_1V2*/
		if (IS_ERR(rt5677_reg))
			pr_err("Fail regulator_get v_ldo2\n");
		else {
			ret = regulator_enable(rt5677_reg);
			if (ret)
				pr_err("Fail regulator_enable v_ldo2, %d\n", ret);
			else
				pr_debug("tegra_rt5677_reg v_ldo2 is enabled\n");
		}

		usleep_range(1000, 2000);

		/*V_AUD_1V8*/
		if (gpio_is_valid(pdata->gpio_int_mic_en)) {
			ret = gpio_direction_output(pdata->gpio_int_mic_en, 1);
			if (ret)
				pr_err("Turn on gpio_int_mic_en fail,%d\n", ret);
			else
				pr_debug("Turn on gpio_int_mic_en\n");
		} else {
			pr_err("gpio_int_mic_en is invalid,%d\n", ret);
		}

		usleep_range(1000, 2000);

		/*AUD_ALC5677_RESET#*/
		if (gpio_is_valid(pdata->gpio_reset)) {
			ret = gpio_direction_output(pdata->gpio_reset, 1);
			if (ret)
				pr_err("Turn on gpio_reset fail,%d\n", ret);
			else
				pr_debug("Turn on gpio_reset\n");
		} else {
			pr_err("gpio_reset is invalid,%d\n", ret);
		}
		status = true;
	} else if (enable == false && status) {
		pr_info("tegra_rt5677 power_off\n");

		/*AUD_ALC5677_RESET#*/
		if (gpio_is_valid(pdata->gpio_reset)) {
			ret = gpio_direction_output(pdata->gpio_reset, 0);
			if (ret)
				pr_err("Turn off gpio_reset fail,%d\n", ret);
			else
				pr_debug("Turn off gpio_reset\n");
		} else {
			pr_err("gpio_reset is invalid,%d\n", ret);
		}

		/*V_AUD_1V8*/
		if (gpio_is_valid(pdata->gpio_int_mic_en)) {
			ret = gpio_direction_output(pdata->gpio_int_mic_en, 0);
			if (ret)
				pr_err("Turn off gpio_int_mic_en fail,%d\n", ret);
			else
				pr_debug("Turn off gpio_int_mic_en\n");
		} else {
			pr_err("gpio_int_mic_en is invalid,%d\n", ret);
		}

		usleep_range(1000, 2000);

		/*V_AUD_1V2*/
		if (IS_ERR(rt5677_reg))
			pr_err("Fail regulator_get v_ldo2\n");
		else {
			ret = regulator_disable(rt5677_reg);
			if (ret)
				pr_err("Fail regulator_disable v_ldo2, %d\n", ret);
			else
				pr_debug("tegra_rt5677_reg v_ldo2 is disabled\n");
		}

		usleep_range(1000, 2000);

		/*V_IO_1V8*/
		if (gpio_is_valid(pdata->gpio_ldo1_en)) {
			pr_debug("gpio_ldo1_en %d is valid\n", pdata->gpio_ldo1_en);
			ret = gpio_direction_output(pdata->gpio_ldo1_en, 0);
			if (ret)
				pr_err("gpio_ldo1_en=0 fail,%d\n", ret);
			else
				pr_debug("gpio_ldo1_en=0\n");

			gpio_free(pdata->gpio_ldo1_en);
		} else {
			pr_err("gpio_ldo1_en %d is invalid\n", pdata->gpio_ldo1_en);
		}

		/* set hp_en low to prevent power leakage */
		set_rt5506_hp_en(0);
		status = false;
		if (machine->clock_enabled)
			mclk_enable(machine, 0);
		machine->bias_level = SND_SOC_BIAS_OFF;
	}

	return;
}

void set_rt5677_power_locked(struct tegra_rt5677 *machine, bool enable, bool hp_depop)
{
	mutex_lock(&machine->rt5677_lock);
	__set_rt5677_power(machine, enable, hp_depop);
	mutex_unlock(&machine->rt5677_lock);
}

void set_rt5677_power_extern(bool enable)
{
	struct snd_soc_card *card = &snd_soc_tegra_rt5677;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);

	set_rt5677_power_locked(machine, enable, false);
}
EXPORT_SYMBOL(set_rt5677_power_extern);

static void trgra_do_power_work(struct work_struct *work)
{
	struct snd_soc_card *card = &snd_soc_tegra_rt5677;
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	mutex_lock(&machine->rt5677_lock);
	__set_rt5677_power(machine, false, false);
	mutex_unlock(&machine->rt5677_lock);
}

static int tegra_rt5677_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_rt5677;
	struct device_node *np = pdev->dev.of_node;
	struct tegra_rt5677 *machine;
	struct tegra_asoc_platform_data *pdata = NULL;
	int ret = 0;
	int codec_id;
	int rt5677_irq = 0;
	u32 val32[7];

	if (!pdev->dev.platform_data && !pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}
	if (pdev->dev.platform_data) {
		pdata = pdev->dev.platform_data;
	} else if (np) {
		pdata = kzalloc(sizeof(struct tegra_asoc_platform_data),
			GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Can't allocate tegra_asoc_platform_data struct\n");
			return -ENOMEM;
		}

		of_property_read_string(np, "nvidia,codec_name",
					&pdata->codec_name);

		of_property_read_string(np, "nvidia,codec_dai_name",
					&pdata->codec_dai_name);

		pdata->gpio_ldo1_en = of_get_named_gpio(np,
						"nvidia,ldo-gpios", 0);
		if (pdata->gpio_ldo1_en < 0)
			dev_warn(&pdev->dev, "Failed to get LDO_EN GPIO\n");

		pdata->gpio_hp_det = of_get_named_gpio(np,
						"nvidia,hp-det-gpios", 0);
		if (pdata->gpio_hp_det < 0)
			dev_warn(&pdev->dev, "Failed to get HP Det GPIO\n");

		pdata->gpio_codec1 = pdata->gpio_codec2 = pdata->gpio_codec3 =
		pdata->gpio_spkr_en = pdata->gpio_hp_mute =
		pdata->gpio_int_mic_en = pdata->gpio_ext_mic_en = -1;

		of_property_read_u32_array(np, "nvidia,i2s-param-hifi", val32,
							   ARRAY_SIZE(val32));
		pdata->i2s_param[HIFI_CODEC].audio_port_id = (int)val32[0];
		pdata->i2s_param[HIFI_CODEC].is_i2s_master = (int)val32[1];
		pdata->i2s_param[HIFI_CODEC].i2s_mode = (int)val32[2];

		of_property_read_u32_array(np, "nvidia,i2s-param-bt", val32,
							   ARRAY_SIZE(val32));
		pdata->i2s_param[BT_SCO].audio_port_id = (int)val32[0];
		pdata->i2s_param[BT_SCO].is_i2s_master = (int)val32[1];
		pdata->i2s_param[BT_SCO].i2s_mode = (int)val32[2];
	}

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		return -EINVAL;
	}

	if (pdata->codec_name)
		card->dai_link->codec_name = pdata->codec_name;

	if (pdata->codec_dai_name)
		card->dai_link->codec_dai_name = pdata->codec_dai_name;

	machine = kzalloc(sizeof(struct tegra_rt5677), GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_rt5677 struct\n");
		if (np)
			kfree(pdata);
		return -ENOMEM;
	}

	machine->pdata = pdata;
	machine->pcard = card;
	machine->bias_level = SND_SOC_BIAS_STANDBY;

	INIT_DELAYED_WORK(&machine->power_work, trgra_do_power_work);

	/*V_IO_1V8*/
	if (gpio_is_valid(pdata->gpio_ldo1_en)) {
		dev_dbg(&pdev->dev, "gpio_ldo1_en %d is valid\n",
			pdata->gpio_ldo1_en);
		ret = gpio_request(pdata->gpio_ldo1_en, "rt5677-ldo-enable");
		if (ret) {
			dev_err(&pdev->dev, "Fail gpio_request gpio_ldo1_en, %d\n",
				ret);
			goto err_free_machine;
		} else {
			ret = gpio_direction_output(pdata->gpio_ldo1_en, 1);
			if (ret) {
				dev_err(&pdev->dev,
					"gpio_ldo1_en=1 fail,%d\n", ret);
				gpio_free(pdata->gpio_ldo1_en);
				goto err_free_machine;
			} else
				dev_dbg(&pdev->dev, "gpio_ldo1_en=1\n");
		}
	} else {
		dev_err(&pdev->dev, "gpio_ldo1_en %d is invalid\n",
			pdata->gpio_ldo1_en);
	}

	usleep_range(1000, 2000);

	INIT_WORK(&machine->hotword_work, tegra_do_hotword_work);

	rt5677_reg = regulator_get(&pdev->dev, "v_ldo2");

	if (gpio_is_valid(pdata->gpio_int_mic_en)) {
		ret = gpio_request(pdata->gpio_int_mic_en, "int-mic-enable");
		if (ret) {
			dev_err(&pdev->dev, "Fail gpio_request gpio_int_mic_en, %d\n", ret);
			goto err_free_machine;
		}
	} else
		dev_err(&pdev->dev, "gpio_int_mic_en %d is invalid\n", pdata->gpio_int_mic_en);

	/*AUD_ALC5677_RESET*/
	if (gpio_is_valid(pdata->gpio_reset)) {
		ret = gpio_request(pdata->gpio_reset, "rt5677-reset");
		if (ret) {
			dev_err(&pdev->dev, "Fail gpio_request gpio_reset, %d\n", ret);
			goto err_free_machine;
		}
	} else
		dev_err(&pdev->dev, "gpio_reset %d is invalid\n", pdata->gpio_reset);

	usleep_range(1000, 2000);
	mutex_init(&machine->rt5677_lock);
	mutex_init(&machine->spk_amp_lock);

	machine->clock_enabled = 1;
	ret = tegra_asoc_utils_init(&machine->util_data, &pdev->dev, card);
	if (ret)
		goto err_free_machine;
	usleep_range(500, 1500);

	set_rt5677_power_locked(machine, true, false);
	usleep_range(500, 1500);

	if (gpio_is_valid(pdata->gpio_irq1)) {
		dev_dbg(&pdev->dev, "gpio_irq1 %d is valid\n",
			pdata->gpio_irq1);
		ret = gpio_request(pdata->gpio_irq1, "rt5677-irq");
		if (ret) {
			dev_err(&pdev->dev, "Fail gpio_request gpio_irq1, %d\n",
				ret);
			goto err_free_machine;
		}

		ret = gpio_direction_input(pdata->gpio_irq1);
		if (ret < 0) {
			dev_err(&pdev->dev, "Fail gpio_direction_input gpio_irq1, %d\n",
			ret);
			goto err_free_machine;
		}

		rt5677_irq = gpio_to_irq(pdata->gpio_irq1);
		if (rt5677_irq < 0) {
			ret = rt5677_irq;
			dev_err(&pdev->dev, "Fail gpio_to_irq gpio_irq1, %d\n",
				ret);
			goto err_free_machine;
		}

		ret = request_irq(rt5677_irq, detect_rt5677_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING ,
			"RT5677_IRQ", machine);
		if (ret) {
			dev_err(&pdev->dev, "request_irq rt5677_irq failed, %d\n",
				ret);
			goto err_free_machine;
		} else {
			dev_dbg(&pdev->dev, "request_irq rt5677_irq ok\n");
			enable_irq_wake(rt5677_irq);
		}
	} else {
		dev_err(&pdev->dev, "gpio_irq1 %d is invalid\n",
			pdata->gpio_irq1);
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	codec_id = pdata->i2s_param[HIFI_CODEC].audio_port_id;
	tegra_rt5677_dai[DAI_LINK_HIFI].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];
	tegra_rt5677_dai[DAI_LINK_HIFI].platform_name =
	tegra_rt5677_i2s_dai_name[codec_id];
	tegra_rt5677_dai[DAI_LINK_I2S_OFFLOAD_BE].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];

	codec_id = pdata->i2s_param[SPEAKER].audio_port_id;
	tegra_rt5677_dai[DAI_LINK_SPEAKER].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];
	tegra_rt5677_dai[DAI_LINK_SPEAKER].platform_name =
	tegra_rt5677_i2s_dai_name[codec_id];

	tegra_rt5677_dai[DAI_LINK_I2S_OFFLOAD_SPEAKER_BE].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];

	tegra_rt5677_dai[DAI_LINK_MI2S_DUMMY].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];
	tegra_rt5677_dai[DAI_LINK_MI2S_DUMMY].platform_name =
	tegra_rt5677_i2s_dai_name[codec_id];

	codec_id = pdata->i2s_param[BT_SCO].audio_port_id;
	tegra_rt5677_dai[DAI_LINK_BTSCO].cpu_dai_name =
	tegra_rt5677_i2s_dai_name[codec_id];
	tegra_rt5677_dai[DAI_LINK_BTSCO].platform_name =
	tegra_rt5677_i2s_dai_name[codec_id];
#endif

	card->dapm.idle_bias_off = 1;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_unregister_switch;
	}

	if (!card->instantiated) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "sound card not instantiated (%d)\n",
			ret);
		goto err_unregister_card;
	}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	ret = tegra_asoc_utils_set_parent(&machine->util_data,
				pdata->i2s_param[HIFI_CODEC].is_i2s_master);
	if (ret) {
		dev_err(&pdev->dev, "tegra_asoc_utils_set_parent failed (%d)\n",
			ret);
		goto err_unregister_card;
	}
#endif

	if (machine->clock_enabled == 1) {
		pr_info("%s to close MCLK\n", __func__);
		mclk_enable(machine, 0);
	}

	machine->bias_level = SND_SOC_BIAS_OFF;

	sysedpc = sysedp_create_consumer("speaker", "speaker");

	wake_lock_init(&machine->vad_wake, WAKE_LOCK_SUSPEND, "rt5677_wake");

	machine->playback_fifo_cif = -1;
	machine->playback_fast_fifo_cif = -1;

	machine->dam_ifc = tegra30_dam_allocate_controller();
	if (machine->dam_ifc < 0) {
		dev_err(&pdev->dev, "DAM allocation failed\n");
		goto err_unregister_card;
	}
	mutex_init(&machine->dam_mutex);

	return 0;

err_unregister_card:
	snd_soc_unregister_card(card);
err_unregister_switch:
	tegra_asoc_utils_fini(&machine->util_data);
	disable_irq_wake(rt5677_irq);
	free_irq(rt5677_irq, 0);
err_free_machine:
	if (np)
		kfree(machine->pdata);

	kfree(machine);
	if (machine->clock_enabled == 1) {
		pr_info("%s to close MCLK\n", __func__);
		mclk_enable(machine, 0);
	}
	return ret;
}

static int tegra_rt5677_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_rt5677 *machine = snd_soc_card_get_drvdata(card);
	struct tegra_asoc_platform_data *pdata = machine->pdata;
	struct device_node *np = pdev->dev.of_node;

	int ret;
	int rt5677_irq;

	if (gpio_is_valid(pdata->gpio_irq1)) {
		dev_dbg(&pdev->dev, "gpio_irq1 %d is valid\n",
			pdata->gpio_irq1);
		rt5677_irq = gpio_to_irq(pdata->gpio_irq1);
		if (rt5677_irq < 0) {
			ret = rt5677_irq;
			dev_err(&pdev->dev, "Fail gpio_to_irq gpio_irq1, %d\n",
				ret);
		} else {
			disable_irq_wake(rt5677_irq);
			free_irq(rt5677_irq, 0);
		}
		cancel_work_sync(&machine->hotword_work);
	} else {
		dev_err(&pdev->dev, "gpio_irq1 %d is invalid\n",
			pdata->gpio_irq1);
	}

	set_rt5677_power_locked(machine, false, false);

	if (gpio_is_valid(pdata->gpio_int_mic_en))
		gpio_free(pdata->gpio_int_mic_en);
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
	if (rt5677_reg)
		regulator_put(rt5677_reg);

	usleep_range(1000, 2000);

	if (gpio_is_valid(pdata->gpio_ldo1_en)) {
		dev_dbg(&pdev->dev, "gpio_ldo1_en %d is valid\n",
			pdata->gpio_ldo1_en);
		ret = gpio_direction_output(pdata->gpio_ldo1_en, 0);
		if (ret)
			dev_err(&pdev->dev,
				"gpio_ldo1_en=0 fail,%d\n", ret);
		else
			dev_dbg(&pdev->dev, "gpio_ldo1_en=0\n");

		gpio_free(pdata->gpio_ldo1_en);
	} else {
		dev_err(&pdev->dev, "gpio_ldo1_en %d is invalid\n",
			pdata->gpio_ldo1_en);
	}

	if (machine->dam_ifc >= 0)
		tegra30_dam_free_controller(machine->dam_ifc);

	snd_soc_unregister_card(card);

	tegra_asoc_utils_fini(&machine->util_data);

	sysedp_free_consumer(sysedpc);

	wake_lock_destroy(&machine->vad_wake);

	if (np)
		kfree(machine->pdata);

	kfree(machine);

	return 0;
}

static const struct of_device_id tegra_rt5677_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-rt5677", },
	{},
};

static struct platform_driver tegra_rt5677_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_rt5677_of_match,
	},
	.probe = tegra_rt5677_driver_probe,
	.remove = tegra_rt5677_driver_remove,
};

static int __init tegra_rt5677_modinit(void)
{
	return platform_driver_register(&tegra_rt5677_driver);
}
module_init(tegra_rt5677_modinit);

static void __exit tegra_rt5677_modexit(void)
{
	platform_driver_unregister(&tegra_rt5677_driver);
}
module_exit(tegra_rt5677_modexit);
MODULE_AUTHOR("Ravindra Lokhande <rlokhande@nvidia.com>");
MODULE_AUTHOR("Manoj Gangwal <mgangwal@nvidia.com>");
MODULE_AUTHOR("Nikesh Oswal <noswal@nvidia.com>");
MODULE_DESCRIPTION("Tegra+rt5677 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
