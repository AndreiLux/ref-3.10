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

int rt5677_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct snd_soc_codec *codec = hw->private_data;
	struct rt5677_priv *rt5677 = snd_soc_codec_get_drvdata(codec);
	struct rt_codec_cmd __user *_rt_codec = (struct rt_codec_cmd *)arg;
	struct rt_codec_cmd rt_codec;
	u8 *buf = NULL;
	int ret = -EFAULT;
	u32 addr = RT5677_MIC_BUF_ADDR;

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		struct compat_rt_codec_cmd compat_rt_codec;

		if (copy_from_user(&compat_rt_codec, _rt_codec,
				   sizeof(compat_rt_codec)))
			goto err;
		rt_codec.number = compat_rt_codec.number;
		rt_codec.buf = compat_ptr(compat_rt_codec.buf);
	} else
#endif
	{
		if (copy_from_user(&rt_codec, _rt_codec, sizeof(rt_codec)))
			goto err;
	}

	dev_dbg(codec->dev, "%s: rt_codec.number=%zu, cmd=%u\n",
		__func__, rt_codec.number, cmd);

	switch (cmd) {
	case RT_READ_CODEC_DSP_IOCTL:
		buf = kzalloc(rt_codec.number, GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			goto err;
		}

		/* Read from DSP to buf */
		ret = rt5677_spi_read(addr, buf, rt_codec.number);
		if (ret)
			goto err;
		/* Format: <offset:4 -> buf0> buf1 buf0 */
		addr = le32_to_cpup((u32 *)buf);
		if (addr >= rt_codec.number)
			addr = sizeof(u32);
		if (copy_to_user(rt_codec.buf, buf + addr,
				 rt_codec.number - addr)) {
			ret = -EFAULT;
			goto err;
		}
		if (copy_to_user(rt_codec.buf + rt_codec.number - addr,
				 buf + sizeof(u32), addr - sizeof(u32))) {
			ret = -EFAULT;
			goto err;
		}
		break;

	case RT_WRITE_CODEC_DSP_IOCTL:
		kfree(rt5677->model_buf);
		rt5677->model_len = 0;
		rt5677->model_buf = kzalloc(rt_codec.number, GFP_KERNEL);
		if (!rt5677->model_buf) {
			ret = -ENOMEM;
			goto err;
		}
		if (copy_from_user(rt5677->model_buf, rt_codec.buf,
				   rt_codec.number))
			goto err;
		rt5677->model_len = rt_codec.number;
		break;

	default:
		ret = -ENOTSUPP;
		break;
	}

err:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL_GPL(rt5677_ioctl_common);
