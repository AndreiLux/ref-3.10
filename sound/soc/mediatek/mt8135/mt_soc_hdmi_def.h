/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT_SOC_HDMI_DEF_H__
#define __MT_SOC_HDMI_DEF_H__

#define HDMI_BUFFER_SIZE_MAX    (512*1024)
#define HDMI_PERIOD_SIZE_MIN    64
#define HDMI_PERIOD_SIZE_MAX    HDMI_BUFFER_SIZE_MAX
#define HDMI_FORMATS            (SNDRV_PCM_FMTBIT_S16_LE)
#define HDMI_RATES              (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000| \
				 SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
				 SNDRV_PCM_RATE_192000)
#define HDMI_RATE_MIN           32000
#define HDMI_RATE_MAX           192000
#define HDMI_CHANNELS_MIN       2
#define HDMI_CHANNELS_MAX       8
#define HDMI_PERIODS_MIN        1
#define HDMI_PERIODS_MAX        1024

void Auddrv_HDMI_Interrupt_Handler(void);

#endif
