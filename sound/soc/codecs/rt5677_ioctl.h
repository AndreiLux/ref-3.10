/*
 * rt5677_ioctl.h  --  RT5677 ALSA SoC audio driver IO control
 *
 * Copyright 2014 Google, Inc
 * Author: Dmitry Shmidt <dimitrysh@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT5677_IOCTL_H__
#define __RT5677_IOCTL_H__

#include <sound/hwdep.h>
#include <linux/ioctl.h>

#define RT5677_MIC_BUF_ADDR	0x60030000
#define RT5677_MODEL_ADDR	0x60000000

int rt5677_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg);

#endif /* __RT5677_IOCTL_H__ */
