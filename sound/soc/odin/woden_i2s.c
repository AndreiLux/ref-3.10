/*
 * woden_i2s.c - Woden I2S driver
 *
 * Author: Hyunhee Jeon <hyunhee.jeon@lge.com>
 * Copyright (c) 2012, LGE Corporation.
 *
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "woden_i2s.h"

#define DRV_NAME "woden-i2s"

enum adma_ch {
	ADMA_CH_MULTICHANNEL_I2S_RX = 0,
	ADMA_CH_I2S0_RX,
	ADMA_CH_I2S1_RX,
	ADMA_CH_I2S2_RX,
	ADMA_CH_I2S3_RX,
	ADMA_CH_I2S4_RX,
	ADMA_CH_SPDIF_RX,
	ADMA_CH_SPI_RX,
	ADMA_CH_MULTICHANNEL_I2S_TX,
	ADMA_CH_I2S0_TX,
	ADMA_CH_I2S1_TX,
	ADMA_CH_I2S2_TX,
	ADMA_CH_I2S3_TX,
	ADMA_CH_I2S4_TX,
	ADMA_CH_SPDIF_TX,
	ADMA_CH_SPI_TX,
	ADMA_CH_MAX		/* the end */
};

static struct woden_i2s  i2scont[WODEN_NR_I2S_IFC];

volatile I2S_REGISTERS *pI2SbaseAddress;

struct suitable_nco {
	unsigned int mclk;
	unsigned int smaplerate;
	unsigned int nco_value;
};

static unsigned int calculted_nco_value(int mclk, int samplerate)
{
	struct suitable_nco nco_value_list[] = {
		{24576000, 8000  ,21845 },
		{22579200, 11025 ,32768 },
		{24576000, 16000 ,43691 },
		{22579200, 22050 ,65536 },
		{24576000, 32000 ,87381 },
		{22579200, 44100 ,131072},
		{24576000, 48000 ,131072},
		{22579200, 88200 ,262144},
		{24576000, 96000 ,262144},
		{45158400, 176400,262144},
		{49152000, 192000,262144},
	};

	int i;

	for( i=0 ; i < ARRAY_SIZE(nco_value_list) ; i++ ) {
		if(nco_value_list[i].mclk == mclk
		&& nco_value_list[i].smaplerate == samplerate)
			return nco_value_list[i].nco_value;
	}

	return 0;
}

static inline void woden_i2s_write(struct woden_i2s *i2s, u32 reg, u32 val)
{
#if 0
#ifdef CONFIG_PM
	i2s->reg_cache[reg >> 2] = val;
#endif
#endif
	__raw_writel(val, i2s->base + reg);
}

static inline u32 woden_i2s_read(struct woden_i2s *i2s, u32 reg)
{
	return __raw_readl(i2s->base + reg);
}

static void woden_i2s_enable_clocks(struct woden_i2s *i2s)
{
	//clk_enable(i2s->clk_i2s);
}

static void woden_i2s_disable_clocks(struct woden_i2s *i2s)
{
	//clk_disable(i2s->clk_i2s);
}

#ifdef CONFIG_DEBUG_FS
static int woden_i2s_show(struct seq_file *s, void *unused)
{
#define REG(r) { r, #r }
	static const struct {
		int offset;
		const char *name;
	} regs[] = {
		REG(IER),
		REG(IRER),
		REG(ITER),
		REG(CER	),
		REG(CCR	),
		REG(RXFFR),
		REG(TXFFR),
	};
#undef REG

	struct woden_i2s *i2s = s->private;
	int i;

	woden_i2s_enable_clocks(i2s);

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u32 val = woden_i2s_read(i2s, regs[i].offset);
		seq_printf(s, "%s = %08x\n", regs[i].name, val);
	}

	woden_i2s_disable_clocks(i2s);

	return 0;
}

static int woden_i2s_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, woden_i2s_show, inode->i_private);
}

static const struct file_operations woden_i2s_debug_fops = {
	.open    = woden_i2s_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void woden_i2s_debug_add(struct woden_i2s *i2s, int id)
{
	char name[] = DRV_NAME ".0";

	snprintf(name, sizeof(name), DRV_NAME".%1d", id);
	i2s->debug = debugfs_create_file(name, S_IRUGO, snd_soc_debugfs_root,
						i2s, &woden_i2s_debug_fops);
}

static void woden_i2s_debug_remove(struct woden_i2s *i2s)
{
	if (i2s->debug)
		debugfs_remove(i2s->debug);
}
#else
static inline void woden_i2s_debug_add(struct woden_i2s *i2s, int id)
{
}

static inline void woden_i2s_debug_remove(struct woden_i2s *i2s)
{
}
#endif
static inline void
i2s_fifo_flush(struct woden_i2s *i2s, u32 stream)
{
    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pI2SbaseAddress->txFlush_0.txFifoFlush = true;
		pI2SbaseAddress->txFlush_1.txFifoFlush = true;
		pI2SbaseAddress->txFlush_2.txFifoFlush = true;
		pI2SbaseAddress->txFlush_3.txFifoFlush = true;
    } else {
		pI2SbaseAddress->rxFlush_0.rxFifoFlush = true;
		pI2SbaseAddress->rxFlush_1.rxFifoFlush = true;
		pI2SbaseAddress->rxFlush_2.rxFifoFlush = true;
		pI2SbaseAddress->rxFlush_3.rxFifoFlush = true;
    }
}

static inline void
i2s_dma_init(struct woden_i2s *i2s, u32 stream)
{
    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pI2SbaseAddress->dmaTxRequest.dmaTxLevel = 0xD;
		pI2SbaseAddress->resetTransmmiterBlockDma = 0x1;
	} else {
		pI2SbaseAddress->dmaRxRequest.dmaRxLevel = 0x1;
		pI2SbaseAddress->resetReceiverBlockDma = 0x1;
	}
}

static inline void
i2s_dma_disable(struct woden_i2s *i2s, u32 stream)
{
	pI2SbaseAddress->dmaEn.dmaEnable = 0x0;
}

static inline void
i2s_dma_enable(struct woden_i2s *i2s, u32 stream)
{
	if(pI2SbaseAddress->dmaEn.dmaEnable == 0x0)
		pI2SbaseAddress->dmaEn.dmaEnable = 0x1;
}

static inline void
i2s_config_channel(struct woden_i2s *i2s, u32 ch, u32 stream, u32 cr)
{
    u32 irq;

    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
        woden_i2s_write(i2s, TCR(ch), i2s->xfer_resolution);
        woden_i2s_write(i2s, TFCR(ch), 0x08);
        //irq = woden_i2s_read(i2s, IMR(ch));
        //woden_i2s_write(i2s, IMR(ch), irq & ~0x30);
        woden_i2s_write(i2s, TER(ch), 1);
    } else {
        woden_i2s_write(i2s, RCR(ch), i2s->xfer_resolution);
        woden_i2s_write(i2s, RFCR(ch), 0x08);
        irq = woden_i2s_read(i2s, IMR(ch));
        woden_i2s_write(i2s, IMR(ch), irq & ~0x03);
        woden_i2s_write(i2s, RER(ch), 1);
    }
}

static inline void
i2s_disable_channels(struct woden_i2s *i2s, u32 stream)
{
    u32 i = 0;

    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
        for (i = 0; i < 4; i++)
            woden_i2s_write(i2s, TER(i), 0);
    } else {
        for (i = 0; i < 4; i++)
            woden_i2s_write(i2s, RER(i), 0);
    }
}

static inline void
i2s_clear_irqs(struct woden_i2s *i2s, u32 stream)
{
    u32 i = 0;

    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
        for (i = 0; i < 4; i++)
            woden_i2s_write(i2s, TOR(i), 0);
    } else {
        for (i = 0; i < 4; i++)
            woden_i2s_write(i2s, ROR(i), 0);
    }
}

void woden_i2s_start(struct woden_i2s *i2s, struct snd_pcm_substream *substream)
{
	u32 irq;

	i2s_fifo_flush(i2s, substream->stream);
	i2s_dma_init(i2s, substream->stream);
	if (!i2s->active) {
		i2s_dma_enable(i2s, substream->stream);
		woden_i2s_write(i2s, IER, 1);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		woden_i2s_write(i2s, ITER, 1);
	} else {
		woden_i2s_write(i2s, IRER, 1);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		irq = woden_i2s_read(i2s, IMR(i2s->id));
		woden_i2s_write(i2s, IMR(i2s->id), irq & ~0x20);
	} else {
		irq = woden_i2s_read(i2s, IMR(i2s->id));
		woden_i2s_write(i2s, IMR(i2s->id), irq & ~0x02);
	}

	if (!i2s->active) {
		woden_i2s_write(i2s, CER, 1);
	}
}

static void
woden_i2s_stop(struct woden_i2s *i2s, struct snd_pcm_substream *substream)
{
	u32 i = 0, irq;

	i2s_clear_irqs(i2s, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		woden_i2s_write(i2s, ITER, 0);

		for (i = 0; i < 4; i++) {
			irq = woden_i2s_read(i2s, IMR(i));
			woden_i2s_write(i2s, IMR(i), irq | 0x30);
		}
	} else {
		woden_i2s_write(i2s, IRER, 0);

		for (i = 0; i < 4; i++) {
			irq = woden_i2s_read(i2s, IMR(i));
			woden_i2s_write(i2s, IMR(i), irq | 0x03);
		}
	}

	if (!i2s->active) {
		i2s_dma_disable(i2s, substream->stream);
		woden_i2s_write(i2s, CER, 0);
		woden_i2s_write(i2s, IER, 0);
	}
}

int woden_i2s_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	woden_i2s_enable_clocks(i2s);

#ifdef WODEN_I2S_RESET
	if( i2s->active++ == 0 ){
		int retry_cnt = 0;
		woden_i2s_write(i2s, woden_I2S_CTRL,
			(i2s->reg_ctrl | woden_I2S_CTRL_SOFT_RESET));
		while((woden_i2s_read(i2s, woden_I2S_CTRL)&woden_I2S_CTRL_SOFT_RESET) && retry_cnt++ < 10000)udelay(100);
	}
#endif
#if 0
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* increment the playback ref count */
		i2s->playback_ref_count++;
		i2s->playback_dma_data.wrap = 4;
		i2s->playback_dma_data.width = 32;

	} else {
		i2s->capture_dma_data.wrap = 4;
		i2s->capture_dma_data.width = 32;
	}
#else
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dai_set_dma_data(dai, substream, &i2s->play_dma_data);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_dai_set_dma_data(dai, substream, &i2s->capture_dma_data);
#endif
	woden_i2s_disable_clocks(i2s);
	return ret;
}

void woden_i2s_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	woden_i2s_enable_clocks(i2s);
#if 0
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s->playback_ref_count == 1)

		/* free the apbif dma channel*/
		woden_ahub_free_tx_fifo(i2s->txcif);

		/* decrement the playback ref count */
		i2s->playback_ref_count--;
	} else {
	}
#endif

#ifdef WODEN_I2S_RESET
	if( --i2s->active == 0 )
	{
		int retry_cnt = 0;
		woden_i2s_write(i2s, woden_I2S_CTRL,
			(i2s->reg_ctrl | woden_I2S_CTRL_SOFT_RESET));
		while((woden_i2s_read(i2s, woden_I2S_CTRL)&woden_I2S_CTRL_SOFT_RESET) && retry_cnt++ < 10000)udelay(100);
	}
#endif
	woden_i2s_disable_clocks(i2s);
}

//master / slave mode
// dsp/i2s/left just mode,,,
static int woden_i2s_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	//struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		pI2SbaseAddress->selectMode.masterSlaveSel = HOST_MASTER;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		pI2SbaseAddress->selectMode.masterSlaveSel = HOST_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		break;
	case SND_SOC_DAIFMT_DSP_B:
		break;
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

//format(16, 24, 32),  number of channel, sampling rate
static int woden_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct device *dev = substream->pcm->card->dev;
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	struct i2s_clk_config_data *config = &i2s->config;
	u32 ch_reg, irq;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		config->data_width = 16;
		i2s->ccr = 0x00;
		i2s->xfer_resolution = 0x02;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		config->data_width = 24;
		i2s->ccr = 0x08;
		i2s->xfer_resolution = 0x04;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		config->data_width = 32;
		i2s->ccr = 0x10;
		i2s->xfer_resolution = 0x05;
		break;

	default:
		dev_err(dev, "woden-i2s: unsuppted PCM fmt");
		return -EINVAL;
	}

	switch (params_channels(params)) {
	case EIGHT_CHANNEL_SUPPORT:
		ch_reg = 3;
		break;
	case SIX_CHANNEL_SUPPORT:
		ch_reg = 2;
		break;
	case FOUR_CHANNEL_SUPPORT:
		ch_reg = 1;
		break;
	case TWO_CHANNEL_SUPPORT:
		ch_reg = 0;
		break;
	default:
		dev_err(dev, "channel not supported\n");
	}

	i2s_disable_channels(i2s, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		woden_i2s_write(i2s, TCR(ch_reg), i2s->xfer_resolution);
		woden_i2s_write(i2s, TFCR(ch_reg), 0x00);
		woden_i2s_write(i2s, TER(ch_reg), 1);
	} else {
		woden_i2s_write(i2s, RCR(ch_reg), i2s->xfer_resolution);
		woden_i2s_write(i2s, RFCR(ch_reg), 0x08);
		woden_i2s_write(i2s, RER(ch_reg), 1);
	}

	//woden_i2s_write(i2s, CCR, i2s->ccr);
	if (!i2s->active) {
		woden_i2s_write(i2s, CCR, 0x10);
	}

	config->sample_rate = params_rate(params);
	i2s->max_channel = config->chan_nr = params_channels(params);

	return 0;
}

static int woden_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		woden_i2s_enable_clocks(i2s);
		woden_i2s_start(i2s, substream);
		i2s->active++;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		i2s->active--;
		//woden_i2s_stop(i2s, substream);
		//woden_i2s_disable_clocks(i2s);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int woden_i2s_probe(struct snd_soc_dai *dai)
{
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);
#ifdef CONFIG_PM
	//int i;
#endif

	//dai->capture_dma_data = &i2s->capture_dma_data;
	//dai->playback_dma_data = &i2s->playback_dma_data;
#ifdef CONFIG_PM
	woden_i2s_enable_clocks(i2s);

#if 0
	/*cache the POR values of i2s regs*/
	for (i = 0; i < WODEN_NR_REG_CACHE; i++)
		i2s->reg_cache[i] = woden_i2s_read(i2s, i<<2);
#endif

	woden_i2s_disable_clocks(i2s);
#endif

	return 0;
}

#ifdef CONFIG_PM
int woden_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	int i, ret = 0;

	woden_i2s_enable_clocks(i2s);

	/*restore the i2s regs*/
	for (i = 0; i < WODEN_NR_REG_CACHE; i++)
		woden_i2s_write(i2s, i<<2, i2s->reg_cache[i]);

	woden_i2s_disable_clocks(i2s);

	return ret;
}
#else
#define woden_i2s_resume NULL
#endif
int woden_i2s_set_sysclk(struct snd_soc_dai *dai,int clk_id, unsigned int freq, int dir)
{
	struct device *dev = dai->dev;
	struct woden_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int mclk_rate = 0;
	volatile unsigned long nco_value = 0;
	int ret = 0;

	/* float_nco = (2097152*((float)lrclk *32))/mclk; */
	switch (freq) {
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 96000:
		mclk_rate = 24576000;
		break;
	case 192000:
		mclk_rate = 49152000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk_rate = 22579200;
		break;
	case 176400:
		mclk_rate = 45158400;
		break;
	default:
		dev_err(dev, "unsuppted sample rate %d\n",freq);
		break;
	}

	nco_value = calculted_nco_value(mclk_rate,freq);
	if (!nco_value) {
		dev_err(dev, "Can't find nco_value\n");
		return -EINVAL;
	}

	dev_dbg(dev,"aud_pll set %d, nco value %ld\n",mclk_rate,nco_value);

	ret = clk_set_rate(i2s->clk_i2s, mclk_rate);
	if (ret) {
		dev_err(dev, "Can't set I2S clock rate: %d\n", ret);
		return ret;
	}

	if (!i2s->active) {
		pI2SbaseAddress->ncoParam.ncoValue = nco_value;
		pI2SbaseAddress->ncoSelect.ncoEnable = 1;
	}

	return mclk_rate;

}

static struct snd_soc_dai_ops woden_i2s_dai_ops = {
	.set_sysclk	= woden_i2s_set_sysclk,
	.startup	= woden_i2s_startup,
	.shutdown	= woden_i2s_shutdown,
	.set_fmt	= woden_i2s_set_fmt,
	.hw_params	= woden_i2s_hw_params,
	.trigger	= woden_i2s_trigger,
};

#define WODEN_SUPPORT_PLAYBACK_RATE	(SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025 \
					|SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050 \
					|SNDRV_PCM_RATE_32000|SNDRV_PCM_RATE_44100 \
					|SNDRV_PCM_RATE_48000|SNDRV_PCM_RATE_88200 \
					|SNDRV_PCM_RATE_96000|SNDRV_PCM_RATE_176400 \
					|SNDRV_PCM_RATE_192000)
#define WODEN_SUPPORT_CAPTURE_RATE	(SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025 \
					|SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_22050 \
					|SNDRV_PCM_RATE_32000|SNDRV_PCM_RATE_44100 \
					|SNDRV_PCM_RATE_48000|SNDRV_PCM_RATE_88200 \
					|SNDRV_PCM_RATE_96000|SNDRV_PCM_RATE_176400 \
					|SNDRV_PCM_RATE_192000)
#define woden_I2S_DAI(id) \
	{ \
		.name = DRV_NAME "." #id, \
		.probe = woden_i2s_probe, \
		.playback = { \
			.channels_min = 1, \
			.channels_max = 2, \
			.rates = WODEN_SUPPORT_PLAYBACK_RATE, \
			.formats = SNDRV_PCM_FMTBIT_S16_LE, \
		}, \
		.capture = { \
			.channels_min = 1, \
			.channels_max = 2, \
			.rates = WODEN_SUPPORT_CAPTURE_RATE, \
			.formats = SNDRV_PCM_FMTBIT_S16_LE, \
		}, \
		.ops = &woden_i2s_dai_ops, \
	}

struct snd_soc_dai_driver woden_i2s_dai[] = {
	woden_I2S_DAI(0),
	woden_I2S_DAI(1),
	woden_I2S_DAI(2),
	woden_I2S_DAI(3),
	woden_I2S_DAI(4),
};

static int woden_i2s_runtime_suspend(struct device *dev)
{
	struct woden_i2s *i2s = dev_get_drvdata(dev);

	clk_disable_unprepare(i2s->clk_i2s);

	return 0;
}

static int woden_i2s_runtime_resume(struct device *dev)
{
	struct woden_i2s *i2s = dev_get_drvdata(dev);
	int ret;

#if 1
{
	void __iomem	*aud_crg_base;

	aud_crg_base = ioremap(0x34670000, 0x100);
	if (!aud_crg_base) {
		dev_err(dev, "audio_crg ioremap failed\n");
		return -ENOMEM;
	}
	#if 1
	__raw_writel(0x1, aud_crg_base + 0x10);	/* dmc clodk select = 360Mhz*/
	__raw_writel(0x1, aud_crg_base + 0x10);	/* dmc clodk select = 360Mhz*/
	__raw_writel(0x1, aud_crg_base + 0x18);	/* div_CCLK 1 = 180Mhz */
	__raw_writel(0x1, aud_crg_base + 0x18);	/* div_CCLK 1 = 180Mhz */
	__raw_writel(0x1, aud_crg_base + 0x1C);	/* DSP enable */
	__raw_writel(0x1, aud_crg_base + 0x1C);	/* DSP enable */
	__raw_writel(0x1, aud_crg_base + 0x20);	/* div_ACLK 1 = 90Mhz*/
	__raw_writel(0x1, aud_crg_base + 0x20);	/* div_ACLK 1 = 90Mhz*/
	__raw_writel(0x3, aud_crg_base + 0x28);	/* div_PCLK 2 = 45Mhz*/
	__raw_writel(0x3, aud_crg_base + 0x28);	/* div_PCLK 2 = 45Mhz*/
	#else
	__raw_writel(0x1, aud_crg_base + 0x10);	/* dmc clodk select = 360Mhz*/
	__raw_writel(0x1, aud_crg_base + 0x10);	/* dmc clodk select = 360Mhz*/
	__raw_writel(0xB, aud_crg_base + 0x18);	/* Divider > dspClkDiv (360 MHz / (11+1) = 30 MHz) */
	__raw_writel(0xB, aud_crg_base + 0x18);	/* Divider > dspClkDiv (360 MHz / (11+1) = 30 MHz) */
	__raw_writel(0x1, aud_crg_base + 0x1C);	/* DSP enable */
	__raw_writel(0x1, aud_crg_base + 0x1C);	/* DSP enable */
	__raw_writel(0x1, aud_crg_base + 0x20);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz) */
	__raw_writel(0x1, aud_crg_base + 0x20);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/
	__raw_writel(0x1, aud_crg_base + 0x28);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/
	__raw_writel(0x1, aud_crg_base + 0x28);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/
	#endif
	/* Audio Async PCLK setting */
	__raw_writel(0x0, aud_crg_base + 0x30);	   /* Main clock source= OSC_CLK */
	__raw_writel(0x0, aud_crg_base + 0x30);	   /* Main clock source= OSC_CLK */
	__raw_writel(0x0, aud_crg_base + 0x34);	   /* div= 0 */
	__raw_writel(0x0, aud_crg_base + 0x34);	   /* div= 0 */
	__raw_writel(0x1, aud_crg_base + 0x38);	   /* gate = clock enable */
	__raw_writel(0x1, aud_crg_base + 0x38);	   /* gate = clock enable */

	__raw_writel(0x01, aud_crg_base + 0x3c);		/* I2S0 AUD PLL base*/
	__raw_writel(0x01, aud_crg_base + 0x48);		/* I2S1 AUD PLL base*/
	__raw_writel(0x01, aud_crg_base + 0x54);		/* I2S2 AUD PLL base*/
	__raw_writel(0x01, aud_crg_base + 0x60);		/* I2S3 AUD PLL base*/
	__raw_writel(0x01, aud_crg_base + 0x6c);		/* I2S4 AUD PLL base*/
	__raw_writel(0x01, aud_crg_base + 0x78);		/* I2S5 AUD PLL base*/

	iounmap(aud_crg_base);
}
#endif

	//printk("woden_i2s_runtime_resume\n");
	ret = clk_prepare_enable(i2s->clk_i2s);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}
	clk_set_rate(i2s->clk_i2s, 11289600);
	clk_set_rate(i2s->clk_i2s, 22579200);

	return 0;
}

static int woden_i2s_platform_probe(struct platform_device *pdev)
{
	struct woden_i2s *i2s;
	struct resource *res, *memregion;
	u32 dma_tx_chan, dma_rx_chan;
	/* ioremap'ed address of I2S registers. */
	void __iomem	*base;
	//void __iomem	*qos_base;
	//int i = 0;
	int ret, val;
	int irq_number;

#if CONFIG_OF
	of_property_read_u32(pdev->dev.of_node, "id", &val);
	pdev->id = val;
#endif

	if ((pdev->id < 0) ||
		(pdev->id >= ARRAY_SIZE(woden_i2s_dai))) {
		dev_err(&pdev->dev, "ID %d out of range\n", pdev->id);
		return -EINVAL;
	}

	i2s = &i2scont[pdev->id];

	dev_set_drvdata(&pdev->dev, i2s);
	i2s->id = pdev->id;

	i2s->clk_i2s = clk_get(&pdev->dev, "i2s0_aud");
	if (IS_ERR(i2s->clk_i2s)) {
		dev_err(&pdev->dev, "Can't retrieve i2s clock\n");
		ret = PTR_ERR(i2s->clk_i2s);
		goto exit;
	}
#ifdef CONFIG_OF
	res = kmalloc(sizeof(struct resource), GFP_KERNEL);

	dma_tx_chan = ADMA_CH_I2S0_TX;
	dma_rx_chan = ADMA_CH_I2S0_RX;

	if(of_address_to_resource(pdev->dev.of_node, 0, res))
	{
		dev_err(&pdev->dev, "No memory resource\n");
		goto err_clk_put;
	}

	base = (unsigned long)of_iomap(pdev->dev.of_node, 0);
	if (!base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release;
	}
#else
	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get I2S-TX dma resource\n");
		goto err_clk_put;
	}
	dma_tx_chan = res->start;

	res = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get I2S-RX dma resource\n");
		goto err_clk_put;
	}
	dma_rx_chan = res->start;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory resource\n");
		goto err_clk_put;
	}

	memregion = request_mem_region(res->start, resource_size(res),
					DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release;
	}
#endif
	i2s->base = (I2S_REGISTERS __iomem *)base;
	pI2SbaseAddress = i2s->base;

	/* below dma addr should be modified to virtual address when IOMMU enableed */
	i2s->play_dma_data.addr = res->start + 0x1C8;
	i2s->capture_dma_data.addr = res->start + 0x1C0;
	i2s->play_dma_data.max_burst = 32;
	i2s->capture_dma_data.max_burst = 32;
	i2s->play_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	i2s->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	i2s->play_dma_data.chan_num = dma_tx_chan;
	i2s->capture_dma_data.chan_num = dma_rx_chan;

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = woden_i2s_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
		else
			clk_set_rate(i2s->clk_i2s, 22579200);
	}

	ret = snd_soc_register_dai(&pdev->dev, &woden_i2s_dai[pdev->id]);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI: %d\n", ret);
		ret = -ENOMEM;
		goto err_suspend;
	}

	ret = woden_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		goto err_unregister_dai;
	}

	woden_i2s_debug_add(i2s, pdev->id);

#ifdef CONFIG_OF
	kfree(res);
#endif

	return 0;

err_unregister_dai:
	snd_soc_unregister_dai(&pdev->dev);

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		woden_i2s_runtime_suspend(&pdev->dev);

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_unmap:
	iounmap(base);
err_release:
	release_mem_region(res->start, resource_size(res));
err_clk_put:
	clk_put(i2s->clk_i2s);
exit:
	kfree(i2s);
	return ret;
}

static int woden_i2s_platform_remove(struct platform_device *pdev)
{
	struct woden_i2s *i2s = dev_get_drvdata(&pdev->dev);
	struct resource *res;
	/* ioremap'ed address of I2S registers. */
	void __iomem	*base = (void *)i2s->base;

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		woden_i2s_runtime_suspend(&pdev->dev);

	snd_soc_unregister_dai(&pdev->dev);

	woden_i2s_debug_remove(i2s);

	iounmap(base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	clk_put(i2s->clk_i2s);

	kfree(i2s);

	return 0;
}

static const struct dev_pm_ops woden_i2s_pm_ops = {
	.suspend = woden_i2s_runtime_suspend,
	.resume = woden_i2s_runtime_resume,
	SET_RUNTIME_PM_OPS(woden_i2s_runtime_suspend,
			   woden_i2s_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static struct of_device_id woden_i2s_match[] = {
	{ .compatible = "snps,woden-i2s", },
	{ }
};
MODULE_DEVICE_TABLE(of, woden_i2s_match);
#endif

static struct platform_driver woden_i2s_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = woden_i2s_match,
#endif
#ifndef CLOCK_TEST
		.pm = &woden_i2s_pm_ops,
#endif
	},
	.probe = woden_i2s_platform_probe,
	.remove = woden_i2s_platform_remove,
};

static int __init snd_woden_i2s_init(void)
{
	return platform_driver_register(&woden_i2s_driver);
}
module_init(snd_woden_i2s_init);

static void __exit snd_woden_i2s_exit(void)
{
	platform_driver_unregister(&woden_i2s_driver);
}
module_exit(snd_woden_i2s_exit);

MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("Woden I2S ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);

