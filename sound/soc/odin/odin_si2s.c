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
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/async.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/odin_clk.h>
#include <linux/odin_pm_domain.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "hw_semaphore.h"
#include "odin_si2s.h"
#include "odin_pcm.h"
#include "odin_aud_powclk.h"

#define SI2S_OSC_CLOCK	24000000

struct odin_si2s {
	struct device *dev;
	int id;
	int mode;
	int mclk;
	int irq;
	int irq_reged;
	void __iomem *si2s_base;
	int si2s_clk;
	struct snd_pcm_substream *cur_substream;
	struct snd_soc_dai_driver *dai_drv;
	struct odin_pcm_dma_params playback_dma_data;
	struct odin_pcm_dma_params capture_dma_data;
	struct regmap *regmap; /* TBD */

	struct hw_sema_uelement *playback_lock;
	struct hw_sema_uelement *capture_lock;
	struct hw_sema_uelement *dsp_playback_lock;
	struct hw_sema_uelement *dsp_capture_lock;

	unsigned int last_status;
	unsigned int last_ff_lv;
};

static u32 odin_si2s_read(void __iomem *reg)
{
	return readl(reg);
}

static void odin_si2s_write(void __iomem *reg, u32 mask, u32 val)
{
	u32 rwval = 0;
	if (!mask)
		return;
	rwval = odin_si2s_read(reg);
	rwval = (rwval & ~mask) | (mask & val);
	writel(rwval, reg);
}

static ssize_t si2s_stat_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char sbuf[512] = { 0, };
	int chcnt = 0;
	unsigned int ctrl_val = 0;
	struct odin_si2s *i2s = NULL;

	i2s = (struct odin_si2s *)dev_get_drvdata(dev);
	if (!i2s)
		return 0;

	ctrl_val = odin_si2s_read(i2s->si2s_base+ODIN_SI2S_CTRL);

	if (ctrl_val & ODIN_SI2S_CTRL_ENABLE_MASK) {
		chcnt += sprintf(sbuf + chcnt, "i2s[%d] is running\n", i2s->id);
		if (ctrl_val & ODIN_SI2S_CTRL_DIR_CFG_MASK)
			chcnt += sprintf(sbuf + chcnt, "playback mode\n");
		else
			chcnt += sprintf(sbuf + chcnt, "receiver mode\n");

		chcnt += sprintf(sbuf + chcnt, "current fifo level %d\n",
			odin_si2s_read(i2s->si2s_base+ODIN_SI2S_FIFO_LEVEL));
	} else {
		chcnt += sprintf(sbuf + chcnt, "i2s[%d] is stopped\n", i2s->id);
	}

	if (i2s->irq_reged)
		chcnt += sprintf(sbuf + chcnt, "i2s irq is enabled\n");

	if (i2s->last_status & ODIN_SI2S_STAT_UNDER_RUN_MASK)
		chcnt += sprintf(sbuf + chcnt, "OCCURRED UNDER RUN\n");

	if (i2s->last_status & ODIN_SI2S_STAT_OVER_RUN_MASK)
		chcnt += sprintf(sbuf + chcnt, "OCCURRED OVER RUN\n");

	if (i2s->last_status)
		chcnt += sprintf(sbuf + chcnt, "FIFO LV %d\n", i2s->last_ff_lv);

	memcpy (buf, sbuf, chcnt);
	return chcnt;
}

static DEVICE_ATTR(si2s_stat, S_IRUGO , si2s_stat_show, NULL);

static void odin_si2s_abort_stream(struct odin_si2s *i2s)
{
	unsigned long flags;
	int ret = -1;

	if (i2s->cur_substream == NULL) {
		dev_err(i2s->dev, "released or not registed substream\n");
		return;
	}

	snd_pcm_stream_lock_irqsave(i2s->cur_substream, flags);

	if (snd_pcm_running(i2s->cur_substream))
		ret = snd_pcm_stop(i2s->cur_substream, SNDRV_PCM_STATE_XRUN);

	snd_pcm_stream_unlock_irqrestore(i2s->cur_substream, flags);

	if (!ret)
		dev_err(i2s->dev, "stream was stoped because I2S XRUN\n");
	else
		dev_err(i2s->dev, "stream stop failed %d\n", ret);
}

static int odin_si2s_irq_enable(struct odin_si2s *i2s, int enable)
{
	unsigned int val = 0;
	unsigned int mask = 0;

	if (enable) {
		/* clear fifo interrupt count */

		/* i2s interrupt enable */
		mask |= ODIN_SI2S_CTRL_INTREQ_M_MASK;
		val |= ODIN_SI2S_CTRL_INTREQ_M(I2S_INTREQ_INDIVIDUAL_MASK);

		/* i2s under/overrun mask disable */
		mask |= ODIN_SI2S_CTRL_SI2S_M_MASK;
		val |= ODIN_SI2S_CTRL_SI2S_M(I2S_UNDER_OVER_RUN_UNMASK);

		/* i2s FIFO full/empty mask enable */
		mask |= ODIN_SI2S_CTRL_FIFO_EMPTY_MASK;
		val |= ODIN_SI2S_CTRL_FIFO_EMPTY(I2S_FIFO_EMPTY_MASK);
		mask |= ODIN_SI2S_CTRL_FIFO_FULL_MASK;
		val |= ODIN_SI2S_CTRL_FIFO_FULL(I2S_FIFO_FULL_MASK);

		/* i2s FIFO almost full/empty mask enable */
		mask |= ODIN_SI2S_CTRL_FIFO_AEMPTY_MASK;
		val |= ODIN_SI2S_CTRL_FIFO_AEMPTY(I2S_FIFO_AEMPTY_MASK);
		mask |= ODIN_SI2S_CTRL_FIFO_AFULL_MASK;
		val |= ODIN_SI2S_CTRL_FIFO_AFULL(I2S_FIFO_AFULL_MASK);

	} else {
		/* i2s interrupt disable */
		mask |= ODIN_SI2S_CTRL_INTREQ_M_MASK;
		val |= ODIN_SI2S_CTRL_INTREQ_M(I2S_INTREQ_ALL_MASK);
	}

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);

	return 0;
}

void odin_si2s_reg_dump(struct odin_si2s *i2s)
{
	int i;

	dev_info(i2s->dev, "--------------------\n");
	dev_info(i2s->dev, "I2S-%d Register Dump\n", i2s->id);
	for (i = 0; i < ODIN_SI2S_FIFO; i += 4) {
		dev_info(i2s->dev, "0x%08x: 0x%08x\n",
			 i, odin_si2s_read(i2s->si2s_base+i));
	}
	dev_info(i2s->dev, "--------------------\n");
	dev_info(i2s->dev, "I2S-%d FIFO trace\n", i2s->id);
	for (i = 0; i < 5; i++ ) {
		dev_info(i2s->dev, "0x%08x: 0x%08x\n",
			 ODIN_SI2S_FIFO_LEVEL,
			 odin_si2s_read(i2s->si2s_base+ODIN_SI2S_FIFO_LEVEL));
		msleep(10);
	}
	dev_info(i2s->dev, "--------------------\n");
}

void odin_si2s_async_func(void *data, async_cookie_t cookie)
{
	struct odin_si2s *i2s = (struct odin_si2s *)data;

	if (i2s->last_status & ODIN_SI2S_STAT_UNDER_RUN_MASK) {
		dev_err(i2s->dev, "OCCURRED UNDER RUN FIFO LEVEL %d\n", i2s->last_ff_lv);
		if (i2s->last_ff_lv == 0)
			goto i2s_stream_stop;
	}

	if (i2s->last_status & ODIN_SI2S_STAT_OVER_RUN_MASK) {
		dev_err(i2s->dev, "OCCURRED OVER RUN\n");
		goto i2s_stream_stop;
	}

	return;

i2s_stream_stop:
	odin_si2s_reg_dump(i2s);
	odin_si2s_abort_stream(i2s);
}

static irqreturn_t odin_si2s_irq_handler(int irq, void *data)
{
	struct odin_si2s *i2s = (struct odin_si2s *)data;

	unsigned int stat = odin_si2s_read(i2s->si2s_base+ODIN_SI2S_STAT);
	unsigned int val = 0;
	unsigned int mask = 0;

	i2s->last_status = stat;
	i2s->last_ff_lv = odin_si2s_read(i2s->si2s_base+ODIN_SI2S_FIFO_LEVEL);

	if (stat & ODIN_SI2S_STAT_UNDER_RUN_MASK) {
		mask |= ODIN_SI2S_STAT_UNDER_RUN_MASK;
		val |= ODIN_SI2S_STAT_UNDER_RUN(I2S_UNDER_RUN_CLEAR);
		if (i2s->last_ff_lv == 0)
			odin_si2s_irq_enable(i2s, 0);
	}

	if (stat & ODIN_SI2S_STAT_OVER_RUN_MASK) {
		mask |= ODIN_SI2S_STAT_OVER_RUN_MASK;
		val |= ODIN_SI2S_STAT_OVER_RUN(I2S_OVER_RUN_CLEAR);
		odin_si2s_irq_enable(i2s, 0);
	}

	async_schedule(odin_si2s_async_func, i2s);

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_STAT, mask, val);

	return IRQ_HANDLED;
}

static int odin_si2s_calc_srate_reg_value(int mclk, int srate,
						int chns, int chn_width)
{
	return (mclk / srate) / (chns * chn_width);
#if 0
	/* The following algorithms are described in the data sheet. */
	float mclk_per_srate = 0;
	float nfs_mul_of_data = 0;
	float need_data_per_nfs = 0;
	float cacled_srate = 0;
	float is_suitable = 0;
	unsigned int reg_srate = 0;
	unsigned int suit_count = 1;
	unsigned int i = 0;
	mclk_per_srate = (mclk / srate); /* mclk = [N]fs. */
	for (i = 0; i < 2; i++) {
		nfs_mul_of_data = (mclk_per_srate / (chns * chn_width)) + i;
		need_data_per_nfs = nfs_mul_of_data * chns * chn_width;
		cacled_srate = 1.0 / (need_data_per_nfs / mclk);
		is_suitable = (srate - cacled_srate) / srate;
		if (is_suitable < suit_count) {
			reg_srate = nfs_mul_of_data;
			suit_count = is_suitable;
		}
	}

	return reg_srate;
#endif
}

static int odin_si2s_sample_rate_set(struct odin_si2s *i2s,
					int mclk,
					int ch_width,
					struct snd_pcm_hw_params *params)
{
	unsigned int mask = 0;
	unsigned int val = 0;
	unsigned int reg_srate = 0;

	if (!mclk)
		return -EINVAL;

	mask = ODIN_SI2S_SAMPLE_RATE_MASK;

	reg_srate = odin_si2s_calc_srate_reg_value(mclk, params_rate(params),
					params_channels(params), ch_width);

	dev_dbg(i2s->dev, "sample_rate_set mclk %d, reg_srate %d \n",
							mclk, reg_srate);

	val = ODIN_SI2S_SAMPLE_RATE(I2S_SAMPLE_RATE(reg_srate));

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_SRATE, mask, val);

	return 0;
}

static int odin_si2s_sample_resolution_set(struct odin_si2s *i2s,
					struct snd_pcm_hw_params *params)
{
	unsigned int val;
	unsigned int mask = ODIN_SI2S_SAMPLE_RES_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		val = ODIN_SI2S_SAMPLE_RES(I2S_SAMPLE_RESOLUTION(8));
		break;
	case SNDRV_PCM_FORMAT_S16:
		val = ODIN_SI2S_SAMPLE_RES(I2S_SAMPLE_RESOLUTION(16));
		break;
	case SNDRV_PCM_FORMAT_S24:
		val = ODIN_SI2S_SAMPLE_RES(I2S_SAMPLE_RESOLUTION(24));
		break;
	case SNDRV_PCM_FORMAT_S32:
		val = ODIN_SI2S_SAMPLE_RES(I2S_SAMPLE_RESOLUTION(32));
		break;
	default:
		return -EINVAL;
	}

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_SRES, mask, val);

	return 0;
}

static void odin_si2s_fifo_level_set(struct odin_si2s *i2s,
					struct snd_pcm_substream *substream)
{
	unsigned int val = 0;
	unsigned int mask = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mask |= ODIN_SI2S_AEMPTY_THRES_MASK;
		val |= ODIN_SI2S_AEMPTY_THRES(I2S_FIFO_AEMPTY_THRES(47));
		odin_si2s_write(i2s->si2s_base+ODIN_SI2S_FIFO_AEMPTY, mask, val);

		mask |= ODIN_SI2S_AFULL_THRES_MASK;
		val |= ODIN_SI2S_AFULL_THRES(I2S_FIFO_AFULL_THRES(63));
		odin_si2s_write(i2s->si2s_base+ODIN_SI2S_FIFO_AFULL, mask, val);
	} else {
		mask |= ODIN_SI2S_AEMPTY_THRES_MASK;
		val |= ODIN_SI2S_AEMPTY_THRES(I2S_FIFO_AEMPTY_THRES(0));
		odin_si2s_write(i2s->si2s_base+ODIN_SI2S_FIFO_AEMPTY, mask, val);

		mask |= ODIN_SI2S_AFULL_THRES_MASK;
		val |= ODIN_SI2S_AFULL_THRES(I2S_FIFO_AFULL_THRES(7));
		odin_si2s_write(i2s->si2s_base+ODIN_SI2S_FIFO_AFULL, mask, val);
	}
}

static void odin_si2s_start(struct odin_si2s *i2s,
				struct snd_pcm_substream *substream)
{
	unsigned int val = 0;
	unsigned int mask = 0;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
		i2s->dsp_playback_lock) {
		ret = hw_sema_lock_status(i2s->dsp_playback_lock);
		if (ret < 0)
			dev_warn(i2s->dev, "dsp_lock failed to get %d", ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		i2s->dsp_capture_lock) {
		ret = hw_sema_lock_status(i2s->dsp_capture_lock);
		if (ret < 0)
			dev_warn(i2s->dev, "dsp_lock failed to get %d", ret);
	}

	if (ret == 1) { /* dsp is using this i2s */
		dev_dbg(i2s->dev, "i2s over enable!");
	}

	odin_si2s_irq_enable(i2s, 1);

	mask |= ODIN_SI2S_CTRL_ENABLE_MASK;
	val |= ODIN_SI2S_CTRL_ENABLE(I2S_ENABLE);

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);
}

static void odin_si2s_stop(struct odin_si2s *i2s,
					struct snd_pcm_substream *substream)
{
	unsigned int val = 0;
	unsigned int mask = 0;
	int ret = 0;

	odin_si2s_irq_enable(i2s, 0);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
		i2s->dsp_playback_lock) {
		ret = hw_sema_lock_status(i2s->dsp_playback_lock);
		if (ret < 0)
			dev_warn(i2s->dev, "dsp_lock failed to get %d", ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		i2s->dsp_capture_lock) {
		ret = hw_sema_lock_status(i2s->dsp_capture_lock);
		if (ret < 0)
			dev_warn(i2s->dev, "dsp_lock failed to get %d", ret);
	}

	if (ret == 1) { /* dsp is using this i2s */
		dev_dbg(i2s->dev, "i2s disalbe skip");
		return;
	}

	mask = ODIN_SI2S_CTRL_ENABLE_MASK;
	val = ODIN_SI2S_CTRL_ENABLE(I2S_DISABLE);
	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);
}

static int odin_si2s_startup(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int val = 0;
	unsigned int mask = 0;
	int ret;

	struct odin_pcm_dma_params *dma_data = NULL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &i2s->playback_dma_data;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data = &i2s->capture_dma_data;

	snd_soc_dai_set_dma_data(dai, substream, (void *)dma_data);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
		i2s->playback_lock) {
		ret = hw_sema_down(i2s->playback_lock);
		if (ret)
			dev_warn(i2s->dev, "playback lock failed %d\n", ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		i2s->capture_lock) {
		ret = hw_sema_down(i2s->capture_lock);
		if (ret)
			dev_warn(i2s->dev, "capture lock failed %d\n", ret);

	}

	mask |= ODIN_SI2S_CTRL_SFR_RST_MASK;
	val |= ODIN_SI2S_CTRL_SFR_RST(I2S_SFR_CTRL_RESET);

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);

	if (i2s->cur_substream == NULL)
		i2s->cur_substream = substream;
	else
		dev_warn(i2s->dev, "open err: check open/close sequence\n");

	return 0;
}

static int odin_si2s_prepare(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL,
			ODIN_SI2S_CTRL_FIFO_RST_MASK,
			ODIN_SI2S_CTRL_FIFO_RST(I2S_FIFO_RESET));
	return 0;
}

static void odin_si2s_shutdown(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	unsigned int val = 0;
	unsigned int mask = 0;
	int ret;

	odin_si2s_irq_enable(i2s, 0);

	mask |= ODIN_SI2S_CTRL_ENABLE_MASK;
	val |= ODIN_SI2S_CTRL_ENABLE(I2S_DISABLE);
	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
		i2s->playback_lock) {
		ret = hw_sema_up(i2s->playback_lock);
		if (ret)
			dev_warn(i2s->dev, "playback unlock failed %d", ret);
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		i2s->capture_lock) {
		ret = hw_sema_up(i2s->capture_lock);
		if (ret)
			dev_warn(i2s->dev, "capture unlock failed %d", ret);
	}

	if (i2s->cur_substream == substream)
		i2s->cur_substream = NULL;
	else
		dev_warn(i2s->dev, "close err: check open/close sequence\n");
}

static void odin_si2s_hw_dump(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	odin_si2s_reg_dump(i2s);
}

static int odin_si2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);

	unsigned int val = 0;
	unsigned int mask = 0;

	if (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		mask |= ODIN_SI2S_CTRL_WS_MODE_MASK;
		mask |= ODIN_SI2S_CTRL_WS_POLAR_MASK;
		mask |= ODIN_SI2S_CTRL_DATA_WSDEL_MASK;
		mask |= ODIN_SI2S_CTRL_DATA_ALIGN_MASK;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		val |= ODIN_SI2S_CTRL_WS_MODE(I2S_WS_MODE_I2S);
		val |= ODIN_SI2S_CTRL_WS_POLAR(I2S_WS_POLAR_L0R1_IN_I2S);
		val |= ODIN_SI2S_CTRL_DATA_WSDEL(ODIN_SI2S_DATA_DELAY_IN_LINE(1));
		val |= ODIN_SI2S_CTRL_DATA_ALIGN(I2S_DATA_ALIGN_LEFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val |= ODIN_SI2S_CTRL_WS_MODE(I2S_WS_MODE_I2S);
		val |= ODIN_SI2S_CTRL_WS_POLAR(I2S_WS_POLAR_L1R0_IN_I2S);
		val |= ODIN_SI2S_CTRL_DATA_WSDEL(ODIN_SI2S_DATA_DELAY_IN_LINE(0));
		val |= ODIN_SI2S_CTRL_DATA_ALIGN(I2S_DATA_ALIGN_RIGHT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val |= ODIN_SI2S_CTRL_WS_MODE(I2S_WS_MODE_I2S);
		val |= ODIN_SI2S_CTRL_WS_POLAR(I2S_WS_POLAR_L1R0_IN_I2S);
		val |= ODIN_SI2S_CTRL_DATA_WSDEL(ODIN_SI2S_DATA_DELAY_IN_LINE(0));
		val |= ODIN_SI2S_CTRL_DATA_ALIGN(I2S_DATA_ALIGN_LEFT);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		val |= ODIN_SI2S_CTRL_WS_MODE(I2S_WS_MODE_DSP);
		val |= ODIN_SI2S_CTRL_WS_POLAR(I2S_WS_POLAR_L0R1_IN_I2S);
		val |= ODIN_SI2S_CTRL_DATA_WSDEL(ODIN_SI2S_DATA_DELAY_IN_LINE(1));
		val |= ODIN_SI2S_CTRL_DATA_ALIGN(I2S_DATA_ALIGN_LEFT);
		break;
	case SND_SOC_DAIFMT_DSP_B:
		val |= ODIN_SI2S_CTRL_WS_MODE(I2S_WS_MODE_DSP);
		val |= ODIN_SI2S_CTRL_WS_POLAR(I2S_WS_POLAR_L0R1_IN_I2S);
		val |= ODIN_SI2S_CTRL_DATA_WSDEL(ODIN_SI2S_DATA_DELAY_IN_LINE(0));
		val |= ODIN_SI2S_CTRL_DATA_ALIGN(I2S_DATA_ALIGN_LEFT);
		break;
	default:
		return -EINVAL;
	}

	if (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		mask |= ODIN_SI2S_CTRL_MS_CFG_MASK;
		mask |=	ODIN_SI2S_CTRL_SCK_POLAR_MASK;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= ODIN_SI2S_CTRL_MS_CFG(I2S_SLAVE);
		val |= ODIN_SI2S_CTRL_SCK_POLAR(I2S_SDATA_UPDATED_FALLING);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		val |= ODIN_SI2S_CTRL_MS_CFG(I2S_MASTER);
		val |= ODIN_SI2S_CTRL_SCK_POLAR(I2S_SDATA_UPDATED_RISING);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		dev_err(dai->dev, "Unsupported clock master mask\n");
	default:
		return -EINVAL;
	}

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);

	return 0;
}

static int odin_si2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	int ret;

	unsigned int val = 0;
	unsigned int mask = 0;

	/* ch_width fixed 32bit for 64fs */
	ret = odin_si2s_sample_rate_set(i2s, i2s->mclk, 32, params);
	if (ret < 0) {
		dev_err(dai->dev, "Failed to set sample_rate reg\n");
		return ret;
	}

	/* Fixed 64fs. fs is Frame Sync(=SampleRate) */
	mask |= ODIN_SI2S_CTRL_CHN_WIDTH_MASK;
	val |= ODIN_SI2S_CTRL_CHN_WIDTH(I2S_32SCK_PER_CHANNEL);

	ret = odin_si2s_sample_resolution_set(i2s, params);
	if (ret < 0) {
		dev_err(dai->dev, "Failed to set sample resolution\n");
		return ret;
	}

	odin_si2s_fifo_level_set(i2s, substream);

	mask |= ODIN_SI2S_CTRL_MONO_MODE_MASK;
	val |= ODIN_SI2S_CTRL_MONO_MODE(I2S_MONO_LEFT);

	mask |= ODIN_SI2S_CTRL_AUDIO_MODE_MASK;
	switch (params_channels(params)) {
	case 1:
		val |= ODIN_SI2S_CTRL_AUDIO_MODE(I2S_AUDIO_MONO);
		break;
	case 2:
		val |= ODIN_SI2S_CTRL_AUDIO_MODE(I2S_AUDIO_STEREO);
		break;
	default:
		dev_err(dai->dev, "Unsupported channel\n");
		return -EINVAL;
	}

	mask |= ODIN_SI2S_CTRL_DIR_CFG_MASK;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		val |= ODIN_SI2S_CTRL_DIR_CFG(I2S_TRANSMITTER);
	else
		val |= ODIN_SI2S_CTRL_DIR_CFG(I2S_RECEIVER);

	odin_si2s_write(i2s->si2s_base+ODIN_SI2S_CTRL, mask, val);

	return 0;
}

static int odin_si2s_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		odin_si2s_start(i2s, substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		odin_si2s_stop(i2s, substream);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int odin_si2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
					unsigned int freq, int dir)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(i2s->dev, "set_sysclk freq(mclk) %d, dir %d\n", freq, dir);

	if (freq == SI2S_OSC_CLOCK) {
		ret = odin_aud_powclk_set_clk_parent(i2s->si2s_clk,
						     ODIN_AUD_PARENT_OSC,
						     SI2S_OSC_CLOCK);

	} else {
		ret = odin_aud_powclk_set_clk_parent(i2s->si2s_clk,
						     ODIN_AUD_PARENT_PLL,
						     freq);
	}

	if (ret) {
		dev_err(dai->dev, "Can't set I2S clk rate: %d %d\n", freq, ret);
		return ret;
	}

	i2s->mclk = freq;
	return ret;
}

static struct snd_soc_dai_ops odin_si2s_dai_ops = {
	.set_sysclk	= odin_si2s_set_sysclk,
	.startup	= odin_si2s_startup,
	.shutdown	= odin_si2s_shutdown,
	.set_fmt	= odin_si2s_set_fmt,
	.hw_params	= odin_si2s_hw_params,
	.prepare	= odin_si2s_prepare,
	.trigger	= odin_si2s_trigger,
	.hw_dump	= odin_si2s_hw_dump,
};

static int odin_si2s_daidrv_probe(struct snd_soc_dai *dai)
{
	struct odin_si2s *i2s = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &i2s->capture_dma_data;
	dai->playback_dma_data = &i2s->playback_dma_data;

	return 0;
}

static struct snd_soc_dai_driver odin_si2s_dai_playback_drv = {
	.probe = odin_si2s_daidrv_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &odin_si2s_dai_ops,
};

static struct snd_soc_dai_driver odin_si2s_dai_capture_drv = {
	.probe = odin_si2s_daidrv_probe,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &odin_si2s_dai_ops,
};

static int odin_si2s_runtime_suspend(struct device *dev)
{
	struct odin_si2s *i2s = dev_get_drvdata(dev);
	return odin_aud_powclk_clk_unprepare_disable(i2s->si2s_clk);
}

static int odin_si2s_runtime_resume(struct device *dev)
{
	struct odin_si2s *i2s = dev_get_drvdata(dev);
	return odin_aud_powclk_clk_prepare_enable(i2s->si2s_clk);

}

static const struct snd_soc_component_driver odin_si2s_component = {
	.name = "odin_si2s",
};

static int odin_si2s_get_hw_lock(struct device *dev, struct odin_si2s *i2s)
{
	struct device_node *np = dev->of_node;
	const char *lock_str = NULL;
	int ret;

	if (!np)
		return -ENOSYS;

	ret = of_property_read_string(np, "playback_lock", &lock_str);
	if (ret)
		dev_warn(dev, "Failed to get lockprop %d\n", ret);

	i2s->playback_lock = hw_sema_get_sema_name(dev, lock_str);
	hw_sema_set_sema_type(dev, i2s->playback_lock, HW_SEMA_TRY_MODE);

	ret = of_property_read_string(np, "dsp_playback_lock", &lock_str);
	if (ret)
		dev_warn(dev, "Failed to get lockprop %d\n", ret);

	i2s->dsp_playback_lock = hw_sema_get_sema_name(dev, lock_str);
	hw_sema_set_sema_type(dev, i2s->dsp_playback_lock, HW_SEMA_TRY_MODE);

	ret = of_property_read_string(np, "capture_lock", &lock_str);
	if (ret)
		dev_warn(dev, "Failed to get lockprop %d\n", ret);

	i2s->capture_lock = hw_sema_get_sema_name(dev, lock_str);
	hw_sema_set_sema_type(dev, i2s->dsp_playback_lock, HW_SEMA_TRY_MODE);

	ret = of_property_read_string(np, "dsp_capture_lock", &lock_str);
	if (ret)
		dev_warn(dev, "Failed to get lockprop %d\n", ret);

	i2s->dsp_capture_lock = hw_sema_get_sema_name(dev, lock_str);
	hw_sema_set_sema_type(dev, i2s->dsp_capture_lock, HW_SEMA_TRY_MODE);

	return 0;
}

static int odin_si2s_platform_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct odin_si2s *i2s = NULL;
	struct resource *res, *memregion;
	phys_addr_t si2s_phys_address;
	phys_addr_t si2s_phys_addr_size;
	u32 dma_tx_chan;
	u32 dma_rx_chan;
	int ret = 0;

	i2s = devm_kzalloc(&pdev->dev, sizeof(struct odin_si2s), GFP_KERNEL);
	if (!i2s) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get mem resource\n");
		ret = -ENODEV;
		goto err_mem_resource;
	}
	si2s_phys_address = res->start;
	si2s_phys_addr_size = resource_size(res);
	memregion = devm_request_mem_region(&pdev->dev, si2s_phys_address,
					    si2s_phys_addr_size, "odin-si2s");
	if (!memregion) {
		dev_err(&pdev->dev, "Failed to set mem_region 0x%x\n",
							si2s_phys_address);
		ret = -EBUSY;
		goto err_mem_resource;
	}

	i2s->si2s_base = devm_ioremap(&pdev->dev, si2s_phys_address,
						si2s_phys_addr_size);
	if (!i2s->si2s_base) {
		dev_err(&pdev->dev, "Failed to set ioremap 0x%x\n",
							si2s_phys_address);
		ret = -ENOMEM;
		goto err_release;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		i2s->irq = res->start;
		ret = devm_request_irq(&pdev->dev, i2s->irq,
					odin_si2s_irq_handler,
					IRQF_ONESHOT, pdev->name, i2s);
		if (ret)
			dev_warn(&pdev->dev, "Failed to request irq(%d) %d\n",
								i2s->irq, ret);
		else
			i2s->irq_reged = 1;
	} else {
		dev_warn(&pdev->dev, "Failed to get irq number\n");
	}

	i2s->si2s_clk = odin_aud_powclk_get_clk_id(si2s_phys_address);

	if (0 > i2s->si2s_clk) {
		dev_err(&pdev->dev, "Failed to get i2s clock\n");
		ret = -EINVAL;
		goto err_unmap;
	}

	if (np) {
		i2s->id = of_alias_get_id(np, "si2s");
		if (i2s->id < 0) {
			dev_err(&pdev->dev, "Failed to get device id\n");
			ret = i2s->id;
			goto err_unmap;
		}

		ret = of_property_read_u32(np, "i2s-mode", &i2s->mode);
		if (ret) {
			dev_err(&pdev->dev, "Failed to get i2s mode\n");
			goto err_unmap;
		}

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
		struct odin_si2s_platform_data *i2s_pdata;

		i2s->id = pdev->id;

		i2s_pdata = (struct odin_si2s_platform_data *)
						pdev->dev.platform_data;
		if (!i2s_pdata) {
			dev_err(&pdev->dev, "Failed to get platform data\n");
			ret = -ENODEV;
			goto err_unmap;
		}
		i2s->mode = i2s_pdata->mode;

		res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
		if (!res) {
			dev_err(&pdev->dev, "Failed to get dma resource\n");
			ret = -ENODEV;
			goto err_unmap;
		}
		dma_tx_chan = res->start;
		dma_rx_chan = res->end;
	}

	if (i2s->mode == ODIN_SI2S_PLAYBACK) {
		i2s->playback_dma_data.addr = si2s_phys_address + ODIN_SI2S_FIFO;
		i2s->playback_dma_data.max_burst = 8;
		i2s->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		i2s->playback_dma_data.chan_num = dma_tx_chan;
		i2s->playback_dma_data.transfer_devtype = ODIN_PCM_FOR_SINGLE;
		i2s->capture_dma_data.addr = 0;
		i2s->capture_dma_data.max_burst = 0;
		i2s->capture_dma_data.addr_width = 0;
		i2s->capture_dma_data.chan_num = 0;
		i2s->capture_dma_data.transfer_devtype = ODIN_PCM_FOR_NONE;
		i2s->dai_drv = &odin_si2s_dai_playback_drv;
	} else if (i2s->mode == ODIN_SI2S_CAPTURE) {
		i2s->playback_dma_data.addr = 0;
		i2s->playback_dma_data.max_burst = 0;
		i2s->playback_dma_data.addr_width = 0;
		i2s->playback_dma_data.chan_num = 0;
		i2s->playback_dma_data.transfer_devtype = ODIN_PCM_FOR_NONE;
		i2s->capture_dma_data.addr = si2s_phys_address + ODIN_SI2S_FIFO;
		i2s->capture_dma_data.max_burst = 8;
		i2s->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		i2s->capture_dma_data.chan_num = dma_rx_chan;
		i2s->capture_dma_data.transfer_devtype = ODIN_PCM_FOR_SINGLE;
		i2s->dai_drv = &odin_si2s_dai_capture_drv;
	} else {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Must set i2s(playback/capture) mode\n");
		goto err_unmap;
	}

#ifdef CONFIG_AUD_RPM
	switch (si2s_phys_address) {
	case 0x34611000:
		ret = odin_pd_register_dev(&pdev->dev, &odin_pd_aud2_si2s0);
		break;
	case 0x34612000:
		ret = odin_pd_register_dev(&pdev->dev, &odin_pd_aud2_si2s1);
		break;
	case 0x34613000:
		ret = odin_pd_register_dev(&pdev->dev, &odin_pd_aud2_si2s2);
		break;
	case 0x34614000:
		ret = odin_pd_register_dev(&pdev->dev, &odin_pd_aud2_si2s3);
		break;
	default:
		goto err_unmap;
	}
#endif

	i2s->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, i2s);

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = snd_soc_register_component(&pdev->dev, &odin_si2s_component,
							i2s->dai_drv, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to register DAI: %d\n", ret);
		ret = -ENOMEM;
		goto err_suspend;
	}

	odin_si2s_get_hw_lock(&pdev->dev, i2s);

	device_create_file(&pdev->dev, &dev_attr_si2s_stat);

	dev_info(&pdev->dev, "Probing success!");

	return 0;

err_suspend:
	pm_runtime_disable(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);
err_unmap:
	if (i2s->irq_reged)
		devm_free_irq(&pdev->dev, i2s->irq, i2s);
	devm_ioremap_release(&pdev->dev, i2s->si2s_base);
err_release:
	devm_release_mem_region(&pdev->dev, si2s_phys_address,
							si2s_phys_addr_size);
err_mem_resource:
	devm_kfree(&pdev->dev, i2s);
exit:
	return ret;
}

static int odin_si2s_platform_remove(struct platform_device *pdev)
{
	struct odin_si2s *i2s = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	if (!pm_runtime_suspended(&pdev->dev))
		pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	devm_ioremap_release(&pdev->dev,i2s->si2s_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	devm_release_mem_region(&pdev->dev, res->start, resource_size(res));
	if (i2s->irq_reged)
		devm_free_irq(&pdev->dev, i2s->irq, i2s);
	dev_set_drvdata(&pdev->dev, NULL);
	devm_kfree(&pdev->dev, i2s);
	return 0;
}

static const struct dev_pm_ops odin_si2s_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_si2s_runtime_suspend,
			   odin_si2s_runtime_resume, NULL)
};

static const struct of_device_id odin_si2s_match[] = {
	{ .compatible = "lge,odin-single-i2s", },
	{},
};

static struct platform_driver odin_si2s_driver = {
	.driver = {
		.name = "odin-single-i2s",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_si2s_match),
		.pm = &odin_si2s_pm_ops,
	},
	.probe = odin_si2s_platform_probe,
	.remove = odin_si2s_platform_remove,
};

module_platform_driver(odin_si2s_driver);

MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("ODIN SINGLE I2S ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-single-i2s");
