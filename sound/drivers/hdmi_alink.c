/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * Some code from patch_hdmi.c
 *  Copyright (c) 2008-2010 Intel Corporation. All rights reserved.
 *  Copyright (c) 2006 ATI Technologies Inc.
 *  Copyright (c) 2008 NVIDIA Corp.  All rights reserved.
 *  Copyright (c) 2008 Wei Ni <wni@nvidia.com>
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

#include <linux/device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm_params.h>
#include <sound/hdmi_alink.h>

#define SAD_DESC_ONE_BLOCK_CNT	 3

#define EDID_SAD_MAX_CHANNEL_CNT 4
#define EDID_SAD_MAX_SAMPLE_CNT  7
#define EDID_SAD_MAX_BIT_CNT     3

enum {
	HDMI_AUDIO_FORMAT_CODE_RESERVED,
	HDMI_AUDIO_FORMAT_CODE_LPCM,
	HDMI_AUDIO_FORMAT_CODE_AC3,
	HDMI_AUDIO_FORMAT_CODE_MPEG1,
	HDMI_AUDIO_FORMAT_CODE_MP3,
	HDMI_AUDIO_FORMAT_CODE_MPEG2,
	HDMI_AUDIO_FORMAT_CODE_AAC,
	HDMI_AUDIO_FORMAT_CODE_DTS,
	HDMI_AUDIO_FORMAT_CODE_ATRAC,
	HDMI_AUDIO_FORMAT_CODE_SACD,
	HDMI_AUDIO_FORMAT_CODE_DDPLUS,
	HDMI_AUDIO_FORMAT_CODE_DTS_HD,
	HDMI_AUDIO_FORMAT_CODE_MPL_DOLBY,
	HDMI_AUDIO_FORMAT_CODE_DST_AUDIO,
	HDMI_AUDIO_FORMAT_CODE_MS_WMA_PRO,
};

struct hdmi_alink {
	struct device *hdmi_dev;
	unsigned int format;
	unsigned int mclk;
	struct hdmi_audio_driver *driver;
	struct hdmi_audio_data_conf dataconf;
};

struct hdmi_alink *halink = NULL;

static unsigned int hdmi_supported_rates[EDID_SAD_MAX_SAMPLE_CNT] = {
	32000,
	44100,
	48000,
	88000,
	96000,
	176000,
	192000
};

static unsigned int hdmi_supported_bits[EDID_SAD_MAX_BIT_CNT] = {
	/* Does not guarantee */
	16, /* 16 bit => 16 bit of 16bit storage */
	24, /* 20 bit => 20 bit of 24bit storage */
	32  /* 24 bit => 24 bit of 32bit storage */
};

static unsigned int channel_capacity[EDID_SAD_MAX_CHANNEL_CNT] = { 0,};
static unsigned int rates_capacity[EDID_SAD_MAX_SAMPLE_CNT] = { 0,};
static unsigned int bits_capacity[EDID_SAD_MAX_BIT_CNT] = { 0,};

static struct snd_pcm_hw_constraint_list hdmi_chn_const;
static struct snd_pcm_hw_constraint_list hdmi_rates_const;
static struct snd_pcm_hw_constraint_list hdmi_bits_const;

enum cea_speaker_placement {
	FL  = (1 <<  0),	/* Front Left           */
	FC  = (1 <<  1),	/* Front Center         */
	FR  = (1 <<  2),	/* Front Right          */
	FLC = (1 <<  3),	/* Front Left Center    */
	FRC = (1 <<  4),	/* Front Right Center   */
	RL  = (1 <<  5),	/* Rear Left            */
	RC  = (1 <<  6),	/* Rear Center          */
	RR  = (1 <<  7),	/* Rear Right           */
	RLC = (1 <<  8),	/* Rear Left Center     */
	RRC = (1 <<  9),	/* Rear Right Center    */
	LFE = (1 << 10),	/* Low Frequency Effect */
	FLW = (1 << 11),	/* Front Left Wide      */
	FRW = (1 << 12),	/* Front Right Wide     */
	FLH = (1 << 13),	/* Front Left High      */
	FCH = (1 << 14),	/* Front Center High    */
	FRH = (1 << 15),	/* Front Right High     */
	TC  = (1 << 16),	/* Top Center           */
};

/*
 * bit 7: Reserved (0)
 * bit 6: Rear Left Center / Rear Right Center present for 1, absent for 0
 * bit 5: Front Left Center / Front Right Center present for 1, absent for 0
 * bit 4: Rear Center present for 1, absent for 0
 * bit 3: Rear Left / Rear Right present for 1, absent for 0
 * bit 2: Front Center present for 1, absent for 0
 * bit 1: LFE present for 1, absent for 0
 * bit 0: Front Left / Front Right present for 1, absent for 0
 */

static int edid_speaker_allocation_bits[] = {
	[0] = FL | FR,
	[1] = LFE,
	[2] = FC,
	[3] = RL | RR,
	[4] = RC,
	[5] = FLC | FRC,
	[6] = RLC | RRC,
	[7] = FLW | FRW,
	[8] = FLH | FRH,
	[9] = TC,
	[10] = FCH,
};

struct cea_channel_speaker_allocation {
	int ca_index;
	int speakers[8];

	/* derived values, just for convenience */
	int channels;
	int spk_mask;
};

static struct cea_channel_speaker_allocation channel_allocations[] = {
/*			  channel:   7     6    5    4    3     2    1    0  */
{ .ca_index = 0x00,  .speakers = {   0,    0,   0,   0,   0,    0,  FR,  FL } },
				 /* 2.1 */
{ .ca_index = 0x01,  .speakers = {   0,    0,   0,   0,   0,  LFE,  FR,  FL } },
				 /* Dolby Surround */
{ .ca_index = 0x02,  .speakers = {   0,    0,   0,   0,  FC,    0,  FR,  FL } },
				 /* surround40 */
{ .ca_index = 0x08,  .speakers = {   0,    0,  RR,  RL,   0,    0,  FR,  FL } },
				 /* surround41 */
{ .ca_index = 0x09,  .speakers = {   0,    0,  RR,  RL,   0,  LFE,  FR,  FL } },
				 /* surround50 */
{ .ca_index = 0x0a,  .speakers = {   0,    0,  RR,  RL,  FC,    0,  FR,  FL } },
				 /* surround51 */
{ .ca_index = 0x0b,  .speakers = {   0,    0,  RR,  RL,  FC,  LFE,  FR,  FL } },
				 /* 6.1 */
{ .ca_index = 0x0f,  .speakers = {   0,   RC,  RR,  RL,  FC,  LFE,  FR,  FL } },
				 /* surround71 */
{ .ca_index = 0x13,  .speakers = { RRC,  RLC,  RR,  RL,  FC,  LFE,  FR,  FL } },

{ .ca_index = 0x03,  .speakers = {   0,    0,   0,   0,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x04,  .speakers = {   0,    0,   0,  RC,   0,    0,  FR,  FL } },
{ .ca_index = 0x05,  .speakers = {   0,    0,   0,  RC,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x06,  .speakers = {   0,    0,   0,  RC,  FC,    0,  FR,  FL } },
{ .ca_index = 0x07,  .speakers = {   0,    0,   0,  RC,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x0c,  .speakers = {   0,   RC,  RR,  RL,   0,    0,  FR,  FL } },
{ .ca_index = 0x0d,  .speakers = {   0,   RC,  RR,  RL,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x0e,  .speakers = {   0,   RC,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x10,  .speakers = { RRC,  RLC,  RR,  RL,   0,    0,  FR,  FL } },
{ .ca_index = 0x11,  .speakers = { RRC,  RLC,  RR,  RL,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x12,  .speakers = { RRC,  RLC,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x14,  .speakers = { FRC,  FLC,   0,   0,   0,    0,  FR,  FL } },
{ .ca_index = 0x15,  .speakers = { FRC,  FLC,   0,   0,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x16,  .speakers = { FRC,  FLC,   0,   0,  FC,    0,  FR,  FL } },
{ .ca_index = 0x17,  .speakers = { FRC,  FLC,   0,   0,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x18,  .speakers = { FRC,  FLC,   0,  RC,   0,    0,  FR,  FL } },
{ .ca_index = 0x19,  .speakers = { FRC,  FLC,   0,  RC,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x1a,  .speakers = { FRC,  FLC,   0,  RC,  FC,    0,  FR,  FL } },
{ .ca_index = 0x1b,  .speakers = { FRC,  FLC,   0,  RC,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x1c,  .speakers = { FRC,  FLC,  RR,  RL,   0,    0,  FR,  FL } },
{ .ca_index = 0x1d,  .speakers = { FRC,  FLC,  RR,  RL,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x1e,  .speakers = { FRC,  FLC,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x1f,  .speakers = { FRC,  FLC,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x20,  .speakers = {   0,  FCH,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x21,  .speakers = {   0,  FCH,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x22,  .speakers = {  TC,    0,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x23,  .speakers = {  TC,    0,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x24,  .speakers = { FRH,  FLH,  RR,  RL,   0,    0,  FR,  FL } },
{ .ca_index = 0x25,  .speakers = { FRH,  FLH,  RR,  RL,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x26,  .speakers = { FRW,  FLW,  RR,  RL,   0,    0,  FR,  FL } },
{ .ca_index = 0x27,  .speakers = { FRW,  FLW,  RR,  RL,   0,  LFE,  FR,  FL } },
{ .ca_index = 0x28,  .speakers = {  TC,   RC,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x29,  .speakers = {  TC,   RC,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x2a,  .speakers = { FCH,   RC,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x2b,  .speakers = { FCH,   RC,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x2c,  .speakers = {  TC,  FCH,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x2d,  .speakers = {  TC,  FCH,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x2e,  .speakers = { FRH,  FLH,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x2f,  .speakers = { FRH,  FLH,  RR,  RL,  FC,  LFE,  FR,  FL } },
{ .ca_index = 0x30,  .speakers = { FRW,  FLW,  RR,  RL,  FC,    0,  FR,  FL } },
{ .ca_index = 0x31,  .speakers = { FRW,  FLW,  RR,  RL,  FC,  LFE,  FR,  FL } },
};

static void init_channel_allocations(void)
{
	int i, j;
	struct cea_channel_speaker_allocation *p;

	for (i = 0; i < ARRAY_SIZE(channel_allocations); i++) {
		p = channel_allocations + i;
		p->channels = 0;
		p->spk_mask = 0;
		for (j = 0; j < ARRAY_SIZE(p->speakers); j++)
			if (p->speakers[j]) {
				p->channels++;
				p->spk_mask |= p->speakers[j];
			}
	}
}

static int hdmi_channel_allocation(int channels)
{
	int i;
	int ca = 0;
	int spk_mask = 0;
	char spk_allocation = 0;

	if (hdmi_audio_get_spk_allocation(&spk_allocation))
		return 0;

	pr_debug("spaker allocation = 0x%x", spk_allocation);

	/*
	 * CA defaults to 0 for basic stereo audio
	 */
	if (channels <= 2)
		return 0;

	/*
	 * expand EDID's speaker allocation mask
	 *
	 * EDID tells the speaker mask in a compact(paired) form,
	 * expand EDID's notions to match the ones used by Audio InfoFrame.
	 */
	for (i = 0; i < ARRAY_SIZE(edid_speaker_allocation_bits); i++) {
		if (spk_allocation & (1 << i))
			spk_mask |= edid_speaker_allocation_bits[i];
	}

	/* search for the first working match in the CA table */
	for (i = 0; i < ARRAY_SIZE(channel_allocations); i++) {
		if (channels == channel_allocations[i].channels &&
		    (spk_mask & channel_allocations[i].spk_mask) ==
				channel_allocations[i].spk_mask) {
			ca = channel_allocations[i].ca_index;
			break;
		}
	}

	return ca;
}

static int set_hw_constraint_alink(struct snd_pcm_runtime *runtime)
{
	int i, ret, sadcnt;
	int ch_max = 2;                   /* default 2 channel */
	int ch_cnt = 0;
	int sample_cnt = 0;
	int bit_cnt = 0;
	unsigned char sample_rates = 0x7; /* default 32Khz, 44Khz, 48Khz */
	unsigned char bitrate = 0x1;      /* default 16bit */
	unsigned char *sadbuf = NULL;
	unsigned char *psad = NULL;

	sadcnt = hdmi_audio_get_sad_byte_count();
	if (sadcnt <= 0) {
		pr_debug("failed to get sad count %d\n", sadcnt);
		goto constraint_set;
	}

	if (sadcnt < SAD_DESC_ONE_BLOCK_CNT) {
		pr_debug("sad count minimum value is 3 but %d\n", sadcnt);
		goto constraint_set;
	}

	if ((sadcnt % SAD_DESC_ONE_BLOCK_CNT) != 0)
		pr_debug("sad count is bad count %d\n", sadcnt);

	sadbuf = kzalloc(sadcnt, GFP_KERNEL);
	if (!sadbuf) {
		pr_err("failed to memory alloc for sadbuf\n");
		goto constraint_set;
	}

	psad = sadbuf;
	ret = hdmi_audio_get_sad_bytes(sadbuf, sadcnt);
	if (ret < 0) {
		pr_debug("failed to get sad bytes %d\n", ret);
		goto constraint_set;
	}

	for (i = 0; i < sadcnt/SAD_DESC_ONE_BLOCK_CNT; i++) {
		if ((psad[0] >> 3) == HDMI_AUDIO_FORMAT_CODE_LPCM) {
			ch_max = (psad[0] & 0x7) + 1;
			sample_rates = psad[1];
			bitrate = psad[2];
		}
		psad += SAD_DESC_ONE_BLOCK_CNT;
	}

	if (ch_max == 0 || sample_rates == 0 || bitrate == 0) {
		pr_debug("LPCM sad is wrong %d,0x%x,0x%x\n",
				ch_max, sample_rates, bitrate);
		ch_max = 2;         /* default 2 channel */
		sample_rates = 0x7; /* default 32Khz, 44Khz, 48Khz */
		bitrate = 0x1;      /* default 16bit */
	}

constraint_set:
	ret = 0;
	for (i = 0; i < ch_max; i++) {
		if (((i + 1) % 2) == 0)
			channel_capacity[ch_cnt++] = i+1;
	}

	for (i = 0; i < EDID_SAD_MAX_SAMPLE_CNT; i++) {
		if (((sample_rates >> i) & 0x1) == 0x1 )
			rates_capacity[sample_cnt++] = hdmi_supported_rates[i];
	}

	for (i = 0; i < EDID_SAD_MAX_BIT_CNT; i++) {
		if (((bitrate >> i) & 0x1) == 0x1 )
			bits_capacity[bit_cnt++] = hdmi_supported_bits[i];
	}

	for (i = 0; i < ch_cnt; i++)
		pr_debug("channel_capacity[%d] = %d\n", i, channel_capacity[i]);

	for (i = 0; i < sample_cnt; i++)
		pr_debug("rates_capacity[%d] = %d\n", i, rates_capacity[i]);

	for (i = 0; i < bit_cnt; i++)
		pr_debug("bits_capacity[%d] = %d\n", i,	bits_capacity[i]);

	if (ch_cnt > 0) {
		hdmi_chn_const.list = channel_capacity;
		hdmi_chn_const.count = ch_cnt;
		hdmi_chn_const.mask = 0;

		ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_CHANNELS, &hdmi_chn_const);
		if (ret < 0) {
			pr_err("channel constraint error %d\n", ret);
			goto constraint_kfree;
		}
	}

	if (sample_cnt > 0) {
		hdmi_rates_const.list = rates_capacity;
		hdmi_rates_const.count = sample_cnt;
		hdmi_rates_const.mask = 0;

		ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &hdmi_rates_const);
		if (ret < 0) {
			pr_err("sample rate constraint error %d\n", ret);
			goto constraint_kfree;
		}
	}

	if (bit_cnt > 0) {
		hdmi_bits_const.list = bits_capacity;
		hdmi_bits_const.count = bit_cnt;
		hdmi_bits_const.mask = 0;

		ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_SAMPLE_BITS, &hdmi_bits_const);
		if (ret < 0) {
			pr_err("channel bit count constraint error %d\n", ret);
			goto constraint_kfree;
		}
	}
constraint_kfree:
	if (sadbuf) {
		kfree(sadbuf);
		sadbuf = NULL;
	}
	return ret;
}

int register_hdmi_alink_drv(struct device *dev, struct hdmi_audio_driver *hadrv)
{
	if (!dev)
		return -ENODEV;

	if (halink) {
		dev_err(dev, "already hdmi link driver was registered\n");
		return -EEXIST;
	}

	halink = devm_kzalloc(dev, sizeof(struct hdmi_alink), GFP_KERNEL);
	if (!halink) {
		dev_err(dev, "Failed to alloc memory\n");
		return -ENOMEM;
	}

	halink->hdmi_dev = dev;
	halink->driver = hadrv;
	init_channel_allocations();

	return 0;
}
EXPORT_SYMBOL_GPL(register_hdmi_alink_drv);

int deregister_hdmi_alink_drv(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (halink)
		devm_kfree(dev, halink);

	halink = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(deregister_hdmi_alink_drv);

int hdmi_audio_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct device *dev;
	int ret;

	dev = substream->pcm->card->dev;

	if (!halink)
		return -ENXIO;

	if (halink->driver->open) {
		ret = halink->driver->open();
		if (ret < 0) {
			dev_err(dev ,"Failed to open halink %d\n", ret);
			return ret;
		}
	}

	ret = set_hw_constraint_alink(runtime);
	if (ret < 0) {
		dev_err(dev ,"Failed to set hw const %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_open);

int hdmi_audio_set_params(struct snd_pcm_hw_params *params)
{
	if (!halink)
		return -ENXIO;

	halink->dataconf.sample_rate = params_rate(params);
	halink->dataconf.channel = params_channels(params);
	halink->dataconf.chn_width =
			snd_pcm_format_width(params_format(params));
	halink->dataconf.spk_placement_idx =
			hdmi_channel_allocation(params_channels(params));

	if (halink->driver->set_params)
		return halink->driver->set_params(&(halink->dataconf));

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_set_params);

int hdmi_audio_operation(int cmd)
{
	if (!halink)
		return -ENXIO;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	if (halink->driver->operation)
		return halink->driver->operation(HDMI_AUDIO_CMD_START);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	if (halink->driver->operation)
		return halink->driver->operation(HDMI_AUDIO_CMD_STOP);
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_operation);

int hdmi_audio_set_clock(unsigned int mclk)
{
	if (!halink)
		return -ENXIO;

	halink->mclk = mclk;

	if (halink->driver->set_clock)
		return halink->driver->set_clock(mclk);

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_set_clock);

int hdmi_audio_set_fmt(unsigned int fmt)
{
	if (!halink)
		return -ENXIO;

	halink->format = fmt;

	if (halink->driver->set_fmt)
		return halink->driver->set_fmt(halink->format);

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_set_fmt);

int hdmi_audio_close(void)
{
	if (!halink)
		return -ENXIO;

	if (halink->driver->close)
		return halink->driver->close();

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_close);

int hdmi_audio_hw_switch(int onoff)
{
	if (!halink)
		return -ENXIO;

	if (halink->driver->hw_switch)
		return halink->driver->hw_switch(onoff);

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_hw_switch);

int hdmi_audio_get_sad_byte_count(void)
{
	if (!halink)
		return -ENXIO;

	if (halink->driver->get_sad_byte_cnt)
		return halink->driver->get_sad_byte_cnt();

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_get_sad_byte_count);

int hdmi_audio_get_sad_bytes(unsigned char *sadbuf, int cnt)
{
	if (!halink)
		return -ENXIO;

	if (!sadbuf || cnt == 0)
		return -EINVAL;

	if (halink->driver->get_sad_bytes)
		return halink->driver->get_sad_bytes(sadbuf, cnt);

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_get_sad_bytes);

int hdmi_audio_get_spk_allocation(unsigned char *spk_alloc)
{
	if (!halink)
		return -ENXIO;

	if (!spk_alloc)
		return -EINVAL;

	if (halink->driver->get_spk_allocation)
		return halink->driver->get_spk_allocation(spk_alloc);

	return 0;
}
EXPORT_SYMBOL_GPL(hdmi_audio_get_spk_allocation);
