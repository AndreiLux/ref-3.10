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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

#ifndef __HDMI_AUDIO_LINK_H
#define __HDMI_AUDIO_LINK_H

#include <linux/device.h>
#include <linux/errno.h>
#include <sound/pcm.h>

#define HDMI_AUDIO_FMT_I2S      1 /* I2S mode */
#define HDMI_AUDIO_FMT_RIGHT_J  2 /* Right Justified mode */
#define HDMI_AUDIO_FMT_LEFT_J   3 /* Left Justified mode */
#define HDMI_AUDIO_FMT_DSP_A    4 /* L data MSB after FRM LRC */
#define HDMI_AUDIO_FMT_DSP_B    5 /* L data MSB during FRM LRC */
#define HDMI_AUDIO_FMT_AC97     6 /* AC97 */
#define HDMI_AUDIO_FMT_PDM      7 /* Pulse density modulation */

#define HDMI_AUDIO_FMT_NB_NF    (1 << 8) /* normal bit clock + frame */
#define HDMI_AUDIO_FMT_NB_IF    (2 << 8) /* normal BCLK + inv FRM */
#define HDMI_AUDIO_FMT_IB_NF    (3 << 8) /* invert BCLK + nor FRM */
#define HDMI_AUDIO_FMT_IB_IF    (4 << 8) /* invert BCLK + FRM */

#define HDMI_AUDIO_FMT_CBM_CFM  (1 << 12) /* codec clk & FRM master */
#define HDMI_AUDIO_FMT_CBS_CFM  (2 << 12) /* codec clk slave & FRM master */
#define HDMI_AUDIO_FMT_CBM_CFS  (3 << 12) /* codec clk master & frame slave */
#define HDMI_AUDIO_FMT_CBS_CFS  (4 << 12) /* codec clk & FRM slave */

#define HDMI_AUDIO_FMT_FORMAT_MASK  0x000f
#define HDMI_AUDIO_FMT_INV_MASK     0x0f00
#define HDMI_AUDIO_FMT_MASTER_MASK  0xf000

enum {
	HDMI_AUDIO_CMD_NONE,
	HDMI_AUDIO_CMD_START,
	HDMI_AUDIO_CMD_STOP,
};

struct hdmi_audio_data_conf {
	unsigned int sample_rate;
	unsigned int channel;
	unsigned int chn_width;
	unsigned int spk_placement_idx; /* ca index */
};

struct hdmi_audio_driver {
	int (*open)(void);
	int (*set_params)(struct hdmi_audio_data_conf *dataconf);
	int (*set_fmt)(unsigned int fmt);
	int (*set_clock)(unsigned int mclk);
	int (*operation)(int cmd);
	int (*close)(void);
	int (*hw_switch)(int onoff);
	int (*get_sad_byte_cnt)(void);
	int (*get_sad_bytes)(unsigned char *sadbuf, int cnt);
	int (*get_spk_allocation)(unsigned char *spk_alloc);
};

#if defined(CONFIG_SND_HDMI_AUDIO_LINK_DRIVER)
extern int register_hdmi_alink_drv(struct device *dev, struct hdmi_audio_driver *hadrv);
extern int deregister_hdmi_alink_drv(struct device *dev);
extern int hdmi_audio_open(struct snd_pcm_substream *substream);
extern int hdmi_audio_set_params(struct snd_pcm_hw_params *params);
extern int hdmi_audio_operation(int cmd);
extern int hdmi_audio_set_clock(unsigned int mclk);
extern int hdmi_audio_set_fmt(unsigned int fmt);
extern int hdmi_audio_close(void);
extern int hdmi_audio_hw_switch(int onoff);
extern int hdmi_audio_get_sad_byte_count(void);
extern int hdmi_audio_get_sad_bytes(unsigned char *sadbuf, int cnt);
extern int hdmi_audio_get_spk_allocation(unsigned char *spk_alloc);
#else
static inline int register_hdmi_alink_drv(struct device *dev,
						struct hdmi_audio_driver *hadrv)
{
	return -ENOSYS;
}
static inline int deregister_hdmi_alink_drv(struct device *dev)
{
	return -ENOSYS;
}
static inline int hdmi_audio_open(struct snd_pcm_substream *substream)
{
	return -ENOSYS;
}
static inline int hdmi_audio_set_params(struct snd_pcm_hw_params *params)
{
	return -ENOSYS;
}
static inline int hdmi_audio_operation(int cmd)
{
	return -ENOSYS;
}
static inline int hdmi_audio_set_clock(unsigned int mclk)
{
	return -ENOSYS;
}
static inline int hdmi_audio_set_fmt(unsigned int fmt)
{
	return -ENOSYS;
}
static inline int hdmi_audio_close(void)
{
	return -ENOSYS;
}
static inline int hdmi_audio_hw_switch(int onoff)
{
	return -ENOSYS;
}
static inline int hdmi_audio_get_sad_byte_count(void)
{
	return -ENOSYS;
}
static inline int hdmi_audio_get_sad_bytes(unsigned char *sadbuf, int cnt)
{
	return -ENOSYS;
}
static inline int hdmi_audio_get_spk_allocation(unsigned char *spk_alloc)
{
	return -ENOSYS;
}
#endif

#endif /* __HDMI_AUDIO_LINK_H */

