/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This file is based on code in sound/soc/dwc/designware_i2s.c
 * and include/sound/designware_i2s.h
 *
 * Copyright (C) 2010 ST Microelectronics
 * Rajeev Kumar <rajeev-dlh.kumar@st.com>
 *
 * designware_i2s.c and designware_i2s.h is distributed under GPL 2.0
 * See designware_i2s.c and designware_i2s.h for further copyright information.
 *
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/dmaengine.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/odin_clk.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "odin_mi2s.h"
#include "odin_pcm.h"
#include "odin_aud_powclk.h"

#define MAX_MI2S_CHANNEL_REG 4

struct odin_mi2s {
	struct device *dev;
	int active;
	void __iomem *mi2s_base;
	int mi2s_clk;
	struct snd_soc_dai_driver *dai_drv;
	struct odin_pcm_dma_params playback_dma_data;
	struct odin_pcm_dma_params capture_dma_data;
	struct regmap *regmap; /* TBD */
};

struct suitable_nco {
	unsigned int mclk;
	unsigned int smaplerate;
	unsigned int nco_value;
};

static unsigned int calculted_nco_value(unsigned int mclk, int samplerate)
{
	/* Fixed 256 fs */
	struct suitable_nco nco_value_list[] = {
		{ 8192000, 32000 , 262144},
		{11289600, 44100 , 262144},
		{12288000, 48000 , 262144},
		{16384000, 64000 , 262144},
		{22579200, 88200 , 262144},
		{24576000, 96000 , 262144},
		{45158400, 176400, 262144},
		{49152000, 192000, 262144},
	};

	int i;

	for (i=0 ; i < ARRAY_SIZE(nco_value_list) ; i++) {
		if (nco_value_list[i].mclk == mclk
		    && nco_value_list[i].smaplerate == samplerate)
			return nco_value_list[i].nco_value;
	}

	return 0;
}

static void i2s_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static u32 i2s_read_reg(void __iomem *io_base, int reg)
{
	return readl(io_base + reg);
}

static void i2s_fifo_all_flush(struct odin_mi2s *i2s, u32 stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(i2s->mi2s_base, TXFFR, 1);
	else
		i2s_write_reg(i2s->mi2s_base, RXFFR, 1);
}

static void i2s_dma_tlevel_set(struct odin_mi2s *i2s, u32 stream, u32 lv)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(i2s->mi2s_base, DMATXLR, lv);
	else
		i2s_write_reg(i2s->mi2s_base, DMARXLR, lv);
}

static void i2s_dma_seq_reset(struct odin_mi2s *i2s, u32 stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(i2s->mi2s_base, RTXDMA, 1);
	else
		i2s_write_reg(i2s->mi2s_base, RRXDMA, 1);
}

static void i2s_dma_disable(struct odin_mi2s *i2s)
{
	i2s_write_reg(i2s->mi2s_base, DMAER, 0);
}

static void i2s_dma_enable(struct odin_mi2s *i2s)
{
	i2s_write_reg(i2s->mi2s_base, DMAER, 1);
}

static void i2s_disable_channels(struct odin_mi2s *i2s, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < 4; i++)
			i2s_write_reg(i2s->mi2s_base, TER(i), 0);
	} else {
		for (i = 0; i < 4; i++)
			i2s_write_reg(i2s->mi2s_base, RER(i), 0);
	}
}

static void i2s_clear_irqs(struct odin_mi2s *i2s, u32 stream)
{
	u32 i = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < 4; i++)
			i2s_write_reg(i2s->mi2s_base, TOR(i), 0);
	} else {
		for (i = 0; i < 4; i++)
			i2s_write_reg(i2s->mi2s_base, ROR(i), 0);
	}
}

static void i2s_start(struct odin_mi2s *i2s,
		      struct snd_pcm_substream *substream)
{
	i2s_dma_seq_reset(i2s,substream->stream);

	if (!i2s->active)
		i2s_write_reg(i2s->mi2s_base, IER, 1);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(i2s->mi2s_base, ITER, 1);
	else
		i2s_write_reg(i2s->mi2s_base, IRER, 1);

	if (!i2s->active) {
		i2s_write_reg(i2s->mi2s_base, CER, 1);
		i2s_dma_enable(i2s);
	}
}

static void i2s_stop(struct odin_mi2s *i2s,
		struct snd_pcm_substream *substream)
{
	u32 i = 0, irq;

	i2s_clear_irqs(i2s, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_write_reg(i2s->mi2s_base, ITER, 0);

		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(i2s->mi2s_base, IMR(i));
			i2s_write_reg(i2s->mi2s_base, IMR(i), irq | 0x30);
		}
	} else {
		i2s_write_reg(i2s->mi2s_base, IRER, 0);

		for (i = 0; i < 4; i++) {
			irq = i2s_read_reg(i2s->mi2s_base, IMR(i));
			i2s_write_reg(i2s->mi2s_base, IMR(i), irq | 0x03);
		}
	}

	if (!i2s->active) {
		i2s_dma_disable(i2s);
		i2s_write_reg(i2s->mi2s_base, CER, 0);
		i2s_write_reg(i2s->mi2s_base, IER, 0);
	}
}

static int odin_mi2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s= snd_soc_dai_get_drvdata(dai);
	struct odin_pcm_dma_params *dma_data = NULL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &i2s->playback_dma_data;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data = &i2s->capture_dma_data;

	snd_soc_dai_set_dma_data(dai, substream, (void *)dma_data);

	return 0;
}

static int odin_mi2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	u32 ccr, xfer_res, i;
	u32 enable_ch[MAX_MI2S_CHANNEL_REG] = { 0, 0, 0, 0 };

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		ccr = 0x00;
		xfer_res = 0x02;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		ccr = 0x08;
		xfer_res = 0x04;
		break;

	case SNDRV_PCM_FORMAT_S32_LE:
		ccr = 0x10;
		xfer_res = 0x05;
		break;

	default:
		dev_err(i2s->dev, "odin-multi-i2s: unsuppted PCM fmt");
		return -EINVAL;
	}

	/* odin only use 64fs */
	ccr = 0x10;
	i2s_write_reg(i2s->mi2s_base, CCR, ccr);

	/* Refer the speaker allocation of CEA-861 */
	switch (params_channels(params)) {
	case EIGHT_CHANNEL_SUPPORT:
		enable_ch[3] = 1;
		enable_ch[2] = 1;
		enable_ch[1] = 1;
		enable_ch[0] = 1;
		break;
	case SIX_CHANNEL_SUPPORT:
		enable_ch[2] = 1;
		enable_ch[1] = 1;
		enable_ch[0] = 1;
		break;
	case FOUR_CHANNEL_SUPPORT:
		enable_ch[2] = 1;
		enable_ch[0] = 1;
		break;
	case TWO_CHANNEL_SUPPORT:
		enable_ch[0] = 1;
		break;
	default:
		dev_err(i2s->dev, "channel not supported\n");
		return -EINVAL;
	}

	i2s_disable_channels(i2s, substream->stream);

	for (i = 0; i < MAX_MI2S_CHANNEL_REG; i++) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (enable_ch[i] == 1) {
				i2s_write_reg(i2s->mi2s_base, TCR(i), xfer_res);
				i2s_write_reg(i2s->mi2s_base, TFCR(i), 0xC);
				i2s_write_reg(i2s->mi2s_base, TER(i), 1);
				i2s_dma_tlevel_set(i2s, substream->stream, 0xC);
			}
		} else {
			if (enable_ch[i] == 1) {
				i2s_write_reg(i2s->mi2s_base, RCR(i), xfer_res);
				i2s_write_reg(i2s->mi2s_base, RFCR(i), 0x07);
				i2s_write_reg(i2s->mi2s_base, RER(i), 1);
				i2s_dma_tlevel_set(i2s, substream->stream, 0x7);
			}
		}
	}

	return 0;
}

static void odin_mi2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	snd_soc_dai_set_dma_data(dai, substream, NULL);
}

void odin_mi2s_reg_dump(struct odin_mi2s *i2s)
{
	int i = 0;

	dev_info(i2s->dev, "--------------------\n");
	dev_info(i2s->dev, "Multi I2S Register Dump\n");

	for (i=0; i<RXFFR; i+=4)
		dev_info(i2s->dev, "0x%08x: 0x%08x\n",
				i, i2s_read_reg(i2s->mi2s_base, i));
	dev_info(i2s->dev, "TX ENABLE CH0  0x%08x: 0x%08x\n",
			0x2c, i2s_read_reg(i2s->mi2s_base, 0x2c));
	dev_info(i2s->dev, "TX ENABLE CH1 0x%08x: 0x%08x\n",
			0x6c, i2s_read_reg(i2s->mi2s_base, 0x2c));
	dev_info(i2s->dev, "TX ENABLE CH2 0x%08x: 0x%08x\n",
			0xac, i2s_read_reg(i2s->mi2s_base, 0x2c));
	dev_info(i2s->dev, "TX ENABLE CH3 0x%08x: 0x%08x\n",
			0xec, i2s_read_reg(i2s->mi2s_base, 0x2c));
	dev_info(i2s->dev, "--------------------\n");
	dev_info(i2s->dev, "Multi I2S FIFO trace\n");

	for (i = 0; i < 5; i++ ) {
		dev_info(i2s->dev, "Left CH0 0x%08x: 0x%08x\n",
				0x20, i2s_read_reg(i2s->mi2s_base, 0x20));
		dev_info(i2s->dev, "Left CH1 0x%08x: 0x%08x\n",
				0x60, i2s_read_reg(i2s->mi2s_base, 0x60));
		dev_info(i2s->dev, "Left CH2 0x%08x: 0x%08x\n",
				0xa0, i2s_read_reg(i2s->mi2s_base, 0xa0));
		dev_info(i2s->dev, "Left CH3 0x%08x: 0x%08x\n",
				0xe0, i2s_read_reg(i2s->mi2s_base, 0xe0));
		dev_info(i2s->dev, "Rignt CH0 0x%08x: 0x%08x\n",
				0x24, i2s_read_reg(i2s->mi2s_base, 0x24));
		dev_info(i2s->dev, "Rignt CH1 0x%08x: 0x%08x\n",
				0x64, i2s_read_reg(i2s->mi2s_base, 0x64));
		dev_info(i2s->dev, "Rignt CH2 0x%08x: 0x%08x\n",
				0xa4, i2s_read_reg(i2s->mi2s_base, 0xa4));
		dev_info(i2s->dev, "Rignt CH3 0x%08x: 0x%08x\n",
				0xe4, i2s_read_reg(i2s->mi2s_base, 0xe4));
		dev_info(i2s->dev, "==============\n");
		msleep(10);
	}
	dev_info(i2s->dev, "--------------------\n");
}

static void odin_mi2s_hw_dump(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	odin_mi2s_reg_dump(i2s);
}

static int odin_mi2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		i2s_start(i2s, substream);
		i2s->active++;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		i2s->active--;
		i2s_stop(i2s, substream);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int odin_mi2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
	case SND_SOC_DAIFMT_IB_NF:
	case SND_SOC_DAIFMT_IB_IF:
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s_write_reg(i2s->mi2s_base, MSSETR, 1);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		i2s_write_reg(i2s->mi2s_base, MSSETR, 0);
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
	default:
		return -EINVAL;
	}

	return 0;
}

int odin_mi2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
					unsigned int freq, int dir)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	volatile unsigned long nco_value = 0;
	int ret = 0;

	switch (freq) {
	case 8192000:
	case 11289600:
	case 12288000:
	case 16384000:
	case 22579200:
	case 24576000:
	case 45158400:
	case 49152000:
		break;
	default:
		dev_err(dai->dev, "unsuppted sample rate %d\n", freq);
		return -EINVAL;
	}

	/* mclk(freq) is fixed of 256fs */
	/* bclk is 64fs(32*2) */
	/* float_nco = (2097152 * ((float)lrclk * 32)) / mclk; */
	nco_value = calculted_nco_value(freq, (freq/256));

	dev_dbg(dai->dev, "Set mclk(%d) and nco(%ld)\n", freq, nco_value);

	ret = odin_aud_powclk_set_clk_parent(i2s->mi2s_clk,
					     ODIN_AUD_PARENT_PLL, freq);
	if (ret) {
		dev_err(dai->dev, "Can't set I2S clock rate: %d\n", ret);
		return ret;
	}

	if (!i2s->active) {
		i2s_write_reg(i2s->mi2s_base, NCOPR, nco_value);
		i2s_write_reg(i2s->mi2s_base, NCOER, 1);
	}

	return ret;
}

static struct snd_soc_dai_ops odin_mi2s_dai_ops = {
	.set_sysclk	= odin_mi2s_set_sysclk,
	.startup	= odin_mi2s_startup,
	.shutdown	= odin_mi2s_shutdown,
	.set_fmt	= odin_mi2s_set_fmt,
	.hw_params	= odin_mi2s_hw_params,
	.trigger	= odin_mi2s_trigger,
	.hw_dump	= odin_mi2s_hw_dump,
};

#ifdef CONFIG_PM
static int odin_mi2s_suspend(struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	return odin_aud_powclk_clk_unprepare_disable(i2s->mi2s_clk);
}

static int odin_mi2s_resume(struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);
	return odin_aud_powclk_clk_prepare_enable(i2s->mi2s_clk);
}
#endif

static int odin_mi2s_daidrv_probe(struct snd_soc_dai *dai)
{
	struct odin_mi2s *i2s = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &i2s->capture_dma_data;
	dai->playback_dma_data = &i2s->playback_dma_data;

	return 0;
}

#define ODIN_MI2S_I2S_RATES \
	(SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_64000 | \
	 SNDRV_PCM_RATE_88200 | \
	 SNDRV_PCM_RATE_96000 | \
	 SNDRV_PCM_RATE_176400 | \
	 SNDRV_PCM_RATE_192000)

#define ODIN_MI2S_I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S24_LE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver odin_mi2s_dai_drv = {
	.probe = odin_mi2s_daidrv_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = MIN_CHANNEL_NUM,
		.channels_max = MAX_CHANNEL_NUM,
		.rates = ODIN_MI2S_I2S_RATES,
		.formats = ODIN_MI2S_I2S_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = MIN_CHANNEL_NUM,
		.channels_max = MAX_CHANNEL_NUM,
		.rates = ODIN_MI2S_I2S_RATES,
		.formats = ODIN_MI2S_I2S_FORMATS,
	},
#ifdef CONFIG_PM
/* Disable suspend/resume because runtime PM was added */
/*	.suspend = odin_mi2s_suspend,                */
/*	.resume = odin_mi2s_resume,                  */
#endif
	.ops = &odin_mi2s_dai_ops,
};

static const struct snd_soc_component_driver odin_mi2s_component = {
	.name		= "odin-multi-i2s",
};

static int odin_mi2s_runtime_suspend(struct device *dev)
{
	struct odin_mi2s *i2s = dev_get_drvdata(dev);

	dev_dbg(dev, "odin_mi2s_runtime_suspend\n");
	odin_aud_powclk_clk_unprepare_disable(i2s->mi2s_clk);

	return 0;
}

static int odin_mi2s_runtime_resume(struct device *dev)
{
	struct odin_mi2s *i2s = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "odin_mi2s_runtime_resume\n");
	ret = odin_aud_powclk_clk_prepare_enable(i2s->mi2s_clk);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int odin_mi2s_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct odin_mi2s *i2s;
	struct resource *res, *memregion;
	phys_addr_t mi2s_phys_address;
	phys_addr_t mi2s_phys_addr_size;
	u32 dma_tx_chan;
	u32 dma_rx_chan;
	int ret;

	i2s = devm_kzalloc(&pdev->dev, sizeof(*i2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "kzalloc fail\n");
		ret = -ENOMEM;
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get mem resource\n");
		ret = -ENODEV;
		goto err_mem_resource;
	}
	mi2s_phys_address = res->start;
	mi2s_phys_addr_size = resource_size(res);
	memregion = devm_request_mem_region(&pdev->dev, mi2s_phys_address,
					    mi2s_phys_addr_size, "odin-mi2s");

	if (!memregion) {
		dev_err(&pdev->dev, "Failed to set mem_region 0x%x\n",
					(unsigned int)mi2s_phys_address);
		ret = -EBUSY;
		goto err_mem_resource;
	}

	i2s->mi2s_base = devm_ioremap(&pdev->dev, mi2s_phys_address,
						mi2s_phys_addr_size);
	if (!i2s->mi2s_base) {
		dev_err(&pdev->dev, "Failed to set ioremap 0x%x\n",
					(unsigned int)mi2s_phys_address);
		ret = -ENOMEM;
		goto err_release;
	}

	i2s->mi2s_clk = odin_aud_powclk_get_clk_id(mi2s_phys_address);
	if (0 > i2s->mi2s_clk ) {
		dev_err(&pdev->dev, "Failed to get i2s clock\n");
		ret = -EINVAL;
		goto err_unmap;
	}

	if (np) {
		ret = of_property_read_u32(np, "tx-dma-channel", &dma_tx_chan);
		if (ret) {
			dev_err(&pdev->dev, "Failed to get tx dma channel\n");
			goto err_unmap;
		}

		ret = of_property_read_u32(np, "rx-dma-channel", &dma_rx_chan);
		if (ret) {
			dev_err(&pdev->dev, "Failed to get rx dma channel\n");
			goto err_unmap;
		}
	} else {
		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (!res) {
			dev_err(&pdev->dev, "Failed to get dma resource\n");
			ret = -ENODEV;
			goto err_unmap;
		}
		dma_tx_chan = res->start;
		dma_rx_chan = res->end;
	}

	/* Set DMA slaves info */
	i2s->playback_dma_data.addr = res->start + TXDMA;
	i2s->playback_dma_data.max_burst = 4;
	i2s->playback_dma_data.chan_num = dma_tx_chan;
	i2s->playback_dma_data.transfer_devtype = ODIN_PCM_FOR_MULTI;

	i2s->capture_dma_data.addr = res->start + RXDMA;
	i2s->capture_dma_data.max_burst = 4;
	i2s->capture_dma_data.chan_num = dma_rx_chan;
	i2s->capture_dma_data.transfer_devtype = ODIN_PCM_FOR_MULTI;

	i2s->dai_drv = &odin_mi2s_dai_drv;
	i2s->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, i2s);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = odin_mi2s_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_component(&pdev->dev, &odin_mi2s_component,
					i2s->dai_drv, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register component\n");
		goto err_reg_comp;
	}

	dev_info(&pdev->dev, "Probing success!");

	return 0;
err_reg_comp:
	if (!pm_runtime_status_suspended(&pdev->dev))
		odin_mi2s_runtime_suspend(&pdev->dev);
err_pm_disable:
	dev_set_drvdata(&pdev->dev, NULL);
	pm_runtime_disable(&pdev->dev);
err_unmap:
	devm_ioremap_release(&pdev->dev, i2s->mi2s_base);
err_release:
	devm_release_mem_region(&pdev->dev, mi2s_phys_address,
							mi2s_phys_addr_size);
err_mem_resource:
	devm_kfree(&pdev->dev, i2s);
exit:
	return ret;
}

static int odin_mi2s_remove(struct platform_device *pdev)
{
	struct odin_mi2s *i2s = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		odin_mi2s_runtime_suspend(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);
	devm_ioremap_release(&pdev->dev,i2s->mi2s_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	devm_release_mem_region(&pdev->dev, res->start, resource_size(res));
	devm_kfree(&pdev->dev, i2s);
	return 0;
}

static const struct dev_pm_ops odin_mi2s_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_mi2s_runtime_suspend,
			   odin_mi2s_runtime_resume, NULL)
};

static const struct of_device_id odin_mi2s_match[] = {
	{ .compatible = "lge,odin-multi-i2s", },
	{},
};

static struct platform_driver odin_mi2s_driver = {
	.driver = {
		.name = "odin-multi-i2s",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_mi2s_match),
		.pm = &odin_mi2s_pm_ops,
	},
	.probe = odin_mi2s_probe,
	.remove = odin_mi2s_remove,
};

module_platform_driver(odin_mi2s_driver);

MODULE_AUTHOR("JongHo Kim <m4jongho.kim@lgepartner.com>");
MODULE_DESCRIPTION("ODIN MULTI I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-multi-i2s");
