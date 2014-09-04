/*
 * rt5677_ioctl.c  --  RT5677 ALSA SoC audio driver IO control
 *
 * Copyright 2014 Google, Inc
 * Author: Dmitry Shmidt <dimitrysh@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spi/spi.h>
#include <sound/soc.h>
#include "rt_codec_ioctl.h"
#include "rt5677_ioctl.h"
#include "rt5677-spi.h"
#include "rt5677.h"

#define RT5677_MIC_BUF_SIZE	0x20000
#define RT5677_MIC_BUF_FIRST_READ_SIZE	0x10000

int rt5677_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct snd_soc_codec *codec = hw->private_data;
	struct rt5677_priv *rt5677 = snd_soc_codec_get_drvdata(codec);
	struct rt_codec_cmd __user *_rt_codec = (struct rt_codec_cmd *)arg;
	struct rt_codec_cmd rt_codec;
	int ret = -EFAULT;
	u32 addr = RT5677_MIC_BUF_ADDR;
	size_t size;
	u32 mic_write_offset;
	size_t bytes_to_user = 0;
	size_t first_chunk_start, first_chunk_len;
	size_t second_chunk_start, second_chunk_len;

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		struct compat_rt_codec_cmd compat_rt_codec;

		if (copy_from_user(&compat_rt_codec, _rt_codec,
				   sizeof(compat_rt_codec)))
			return -EFAULT;
		rt_codec.number = compat_rt_codec.number;
		rt_codec.buf = compat_ptr(compat_rt_codec.buf);
	} else
#endif
	{
		if (copy_from_user(&rt_codec, _rt_codec, sizeof(rt_codec)))
			return -EFAULT;
	}

	dev_dbg(codec->dev, "%s: rt_codec.number=%zu, cmd=%u\n",
		__func__, rt_codec.number, cmd);

	size = sizeof(int) * rt_codec.number;
	switch (cmd) {
	case RT_READ_CODEC_DSP_IOCTL:
	case RT_READ_CODEC_DSP_IOCTL_COMPAT:
		/* Grab the first 4 bytes that holds the write pointer on the
		   dsp, and check to make sure that it points somewhere inside
		   the buffer. */
		ret = rt5677_spi_read(addr, rt5677->mic_buf, 4);
		if (ret)
			return ret;
		mic_write_offset = le32_to_cpup((u32 *)rt5677->mic_buf);

		if (mic_write_offset < sizeof(u32) ||
		    mic_write_offset >= RT5677_MIC_BUF_SIZE) {
			dev_err(codec->dev,
				"Invalid offset in the mic buffer %d\n",
				mic_write_offset);
			return -EFAULT;
		}

		/* If the mic_read_offset is zero, this means it's the first
		   time that we've asked for streaming data. We should start
		   reading from the previous 2 seconds of audio from wherever
		   the mic_write_offset is currently (note that this needs to
		   wraparound the buffer). */
		if (rt5677->mic_read_offset == 0) {
			if (mic_write_offset <
			    RT5677_MIC_BUF_FIRST_READ_SIZE + sizeof(u32)) {
				rt5677->mic_read_offset = (RT5677_MIC_BUF_SIZE -
					(RT5677_MIC_BUF_FIRST_READ_SIZE -
						(mic_write_offset - sizeof(u32))));
			} else {
				rt5677->mic_read_offset = (mic_write_offset -
					RT5677_MIC_BUF_FIRST_READ_SIZE);
			}
		}

		/* If the audio wrapped around, then we need to do the copy in
		   two passes, otherwise, we can do it on one. We should also
		   make sure that we don't read more bytes than we have in the
		   user buffer, or we'll just waste time. */
		if (mic_write_offset < rt5677->mic_read_offset) {
			/* Copy the audio from the last read offset until the
			   end of the buffer, then do the second chunk that
			   starts after the u32. */
			first_chunk_start = rt5677->mic_read_offset;
			first_chunk_len =
				RT5677_MIC_BUF_SIZE - rt5677->mic_read_offset;
			if (first_chunk_len > size) {
				second_chunk_start = 0;
				second_chunk_len = 0;
			} else {
				second_chunk_start = sizeof(u32);
				second_chunk_len =
					mic_write_offset - sizeof(u32);
				if (first_chunk_len + second_chunk_len > size) {
					second_chunk_len =
						size - first_chunk_len;
				}
			}
		} else {
			first_chunk_start = rt5677->mic_read_offset;
			first_chunk_len =
				mic_write_offset - rt5677->mic_read_offset;
			if (first_chunk_len > size)
				first_chunk_len = size;
			second_chunk_start = 0;
			second_chunk_len = 0;
		}

		ret = rt5677_spi_read(addr + first_chunk_start, rt5677->mic_buf,
				      first_chunk_len);
		if (ret)
			return ret;
		bytes_to_user += first_chunk_len;

		if (second_chunk_len) {
			ret = rt5677_spi_read(addr + second_chunk_start,
					      rt5677->mic_buf + first_chunk_len,
					      second_chunk_len);
			if (!ret)
				bytes_to_user += second_chunk_len;
		}

		bytes_to_user -= copy_to_user(rt_codec.buf, rt5677->mic_buf,
					      bytes_to_user);

		rt5677->mic_read_offset += bytes_to_user;
		if (rt5677->mic_read_offset >= RT5677_MIC_BUF_SIZE) {
			rt5677->mic_read_offset -=
				RT5677_MIC_BUF_SIZE - sizeof(u32);
		}
		return bytes_to_user >> 1;

	case RT_WRITE_CODEC_DSP_IOCTL:
	case RT_WRITE_CODEC_DSP_IOCTL_COMPAT:
		kfree(rt5677->model_buf);
		rt5677->model_len = 0;
		rt5677->model_buf = kzalloc(size, GFP_KERNEL);
		if (!rt5677->model_buf) {
			return -ENOMEM;
		}
		if (copy_from_user(rt5677->model_buf, rt_codec.buf, size))
			return -EFAULT;
		rt5677->model_len = size;
		return 0;

	default:
		return -ENOTSUPP;
	}
}
EXPORT_SYMBOL_GPL(rt5677_ioctl_common);
