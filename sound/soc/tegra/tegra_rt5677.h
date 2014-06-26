/*
 * tegra_rt5677.h - Tegra machine ASoC driver for boards using ALC5645 codec.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/wakelock.h>
#include "tegra_asoc_utils.h"

struct tegra_rt5677 {
	struct tegra_asoc_utils_data util_data;
	struct tegra_asoc_platform_data *pdata;
	struct snd_soc_codec *codec;
	int gpio_requested;
	enum snd_soc_bias_level bias_level;
	int clock_enabled;
	struct regulator *codec_reg;
	struct regulator *digital_reg;
	struct regulator *analog_reg;
	struct regulator *spk_reg;
	struct regulator *mic_reg;
	struct regulator *dmic_reg;
	struct snd_soc_card *pcard;
	struct delayed_work power_work;
	struct work_struct hotword_work;
	struct mutex rt5677_lock;
	struct mutex spk_amp_lock;
	struct wake_lock vad_wake;
};
